module;

#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <map>
#include <iostream>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#ifndef _NODISCARD
#define _NODISCARD [[nodiscard]]
#endif

export module Tungsten.ast;

namespace tungsten {
   // llvm stuff (separate namespace block to avoid exporting to other modules)
   std::unique_ptr<llvm::LLVMContext> TheContext{};
   std::unique_ptr<llvm::IRBuilder<>> Builder{};
   std::unique_ptr<llvm::Module> TheModule{};
   std::map<std::string, llvm::Value*> NamedValues{};
   llvm::Value* LogErrorV(const std::string& Str) {
      std::cerr << "error: " << Str << "\n";
      return nullptr;
   }
   llvm::Function* LogErrorF(const std::string& Str) {
      std::cerr << "error: " << Str << "\n";
      return nullptr;
   }

   llvm::Type* LLVMType(const std::string& type) {
      using namespace llvm;
      if (type == "Void")
         return Type::getVoidTy(*TheContext);
      if (type == "Bool")
         return Type::getInt1Ty(*TheContext);
      if (type == "Char")
         return Type::getInt8Ty(*TheContext);
      if (type == "String")
         return Type::getInt8Ty(*TheContext)->getPointerTo();
      if (type == "Int" || type == "Int32" || type == "Uint" || type == "Uint32")
         return Type::getInt32Ty(*TheContext);
      if (type == "Int8" || type == "Uint8")
         return Type::getInt8Ty(*TheContext);
      if (type == "Int16" || type == "Uint16")
         return Type::getInt16Ty(*TheContext);
      if (type == "Int64" || type == "Uint64")
         return Type::getInt64Ty(*TheContext);
      if (type == "Int128" || type == "Uint128")
         return Type::getInt128Ty(*TheContext);
      if (type == "Float")
         return Type::getFloatTy(*TheContext);
      if (type == "Double")
         return Type::getDoubleTy(*TheContext);
      return nullptr;
   }
   std::string mapLLVMTypeToCustomType(llvm::Type* type) {
      if (type->isIntegerTy()) {
         unsigned bits = type->getIntegerBitWidth();
         switch (bits) {
            case 1:
               return "Bool";
            case 8:
               return "Int8";
            case 16:
               return "Int16";
            case 32:
               return "Int32";
            case 64:
               return "Int64";
            case 128:
               return "Int128";
            default:
               return "Int" + std::to_string(bits); // fallback
         }
      }

      if (type->isFloatingPointTy()) {
         if (type->isFloatTy()) return "Float";
         if (type->isDoubleTy()) return "Double";
         return "FloatN";
      }

      if (type->isPointerTy()) { // TODO: fix infinite recursive calling
         llvm::Type* pointee = type->getPointerTo();
         if (pointee->isIntegerTy(8)) {
            return "String";
         }

         if (pointee->isIntegerTy(8)) {
            return "Char";
         }

         if (pointee->isStructTy()) { // classes
            llvm::StructType* structTy = llvm::cast<llvm::StructType>(pointee);
            if (structTy->hasName()) {
               llvm::StringRef name = structTy->getName();
               if (name.starts_with("class.")) {
                  return name.substr(6).str();
               }
               return name.str();
            }
            return "AnonymousClass";
         }

         return "Ptr<" + mapLLVMTypeToCustomType(pointee) + ">";
      }

      if (type->isVoidTy()) {
         return "Void";
      }

      if (type->isStructTy()) {
         llvm::StructType* structTy = llvm::cast<llvm::StructType>(type);
         if (structTy->hasName()) {
            llvm::StringRef name = structTy->getName();
            if (name.starts_with("class.")) {
               return name.substr(6).str();
            }
            return name.str();
         }
         return "AnonymousStruct";
      }

      return "UnknownType";
   }

   std::string getTypeString(llvm::Value* val) {
      return mapLLVMTypeToCustomType(val->getType());
   }
} // namespace tungsten

export namespace tungsten {
   void initLLVM() {
      TheContext = std::make_unique<llvm::LLVMContext>();
      Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
      TheModule = std::make_unique<llvm::Module>("tungsten", *TheContext);
   }
   void dumpIR() {
#ifdef TUNGSTEN_DEBUG

      std::error_code EC;
      llvm::raw_fd_ostream outFile("tungsten.ll", EC, llvm::sys::fs::OF_None);

      if (EC) {
         std::cerr << "error opening the file: " << EC.message() << std::endl;
      } else {
         TheModule->print(outFile, nullptr);
      }
#endif
   }

   // base for all nodes in the AST
   class ExpressionAST {
   public:
      virtual ~ExpressionAST() = default;
      virtual llvm::Value* codegen() = 0;
      virtual bool isLValue() { return true; }
   };

   // expression for numbers
   using Number = std::variant<int, int8_t, int16_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t, float, double>;
   class NumberExpressionAST : public ExpressionAST {
   public:
      NumberExpressionAST(Number value) : _value{value} {}
      llvm::Value* codegen() override;
      bool isLValue() override { return false; }

   private:
      Number _value{};
   };

   // expression for variables
   class VariableExpressionAST : public ExpressionAST {
   public:
      VariableExpressionAST(const std::string& name) : _name{name} {}
      llvm::Value* codegen() override;

   private:
      std::string _name{};
   };

   class StringExpression : public ExpressionAST {
   public:
      StringExpression(const std::string& value) : _value{value} {}
      llvm::Value* codegen() override;
      bool isLValue() override { return false; }

   private:
      std::string _value{};
   };

   // expression for binary operations
   class BinaryExpressionAST : public ExpressionAST {
   public:
      BinaryExpressionAST(const std::string& op, std::unique_ptr<ExpressionAST> LHS, std::unique_ptr<ExpressionAST> RHS)
          : _op{op}, _LHS{std::move(LHS)}, _RHS{std::move(RHS)} {}
      llvm::Value* codegen() override;

   private:
      std::string _op;
      std::unique_ptr<ExpressionAST> _LHS;
      std::unique_ptr<ExpressionAST> _RHS;
   };

   class VariableDeclarationAST : public ExpressionAST {
   public:
      VariableDeclarationAST(const std::string& type, const std::string& name, std::unique_ptr<ExpressionAST> init)
          : _type{type}, _name{name}, _init{std::move(init)} {}
      llvm::Value* codegen() override;

      _NODISCARD const std::string& name() const { return _name; }
      _NODISCARD const std::string& type() const { return _type; }

   private:
      std::string _type;
      std::string _name;
      std::unique_ptr<ExpressionAST> _init;
   };

   // expression for function calls
   class CallExpressionAST : public ExpressionAST {
   public:
      CallExpressionAST(const std::string& callee, std::vector<std::unique_ptr<ExpressionAST>> args)
          : _callee{callee}, _args{std::move(args)} {}
      llvm::Value* codegen() override;

   private:
      std::string _callee;
      std::vector<std::unique_ptr<ExpressionAST>> _args;
   };

   class TypeOfStatementAST : public ExpressionAST {
   public:
      TypeOfStatementAST(const std::string& variable) : _variable{variable} {}
      llvm::Value* codegen() override;

   private:
      std::string _variable;
   };

   class NameOfStatementAST : public ExpressionAST {
   public:
      NameOfStatementAST(const std::string& variable) : _name{variable} {}
      llvm::Value* codegen() override;

   private:
      std::string _name;
   };

   class SizeOfStatementAST : public ExpressionAST {
   public:
      SizeOfStatementAST(const std::string& variable) : _variable{variable} {}
      llvm::Value* codegen() override;

   private:
      std::string _variable;
   };

   class __BuiltinFunctionAST : public ExpressionAST {
   public:
      __BuiltinFunctionAST(const std::string& function) : _function{function} {}
      llvm::Value* codegen() override;

   private:
      std::string _function;
   };
   class __BuiltinLineAST : public ExpressionAST {
   public:
      __BuiltinLineAST(size_t line) : _line{line} {}
      llvm::Value* codegen() override;

   private:
      size_t _line;
   };
   class __BuiltinColumnAST : public ExpressionAST {
   public:
      __BuiltinColumnAST(size_t column) : _column{column} {}
      llvm::Value* codegen() override;

   private:
      size_t _column;
   };
   class __BuiltinFileAST : public ExpressionAST {
   public:
      __BuiltinFileAST(const std::string& file) : _file{file} {}
      llvm::Value* codegen() override;

   private:
      std::string _file;
   };

   // // expression for if statements
   class IfStatementAST : public ExpressionAST {
   public:
      IfStatementAST(std::unique_ptr<ExpressionAST> condition, std::unique_ptr<ExpressionAST> thenBranch, std::unique_ptr<ExpressionAST> elseBranch = nullptr)
          : _condition{std::move(condition)}, _thenBranch{std::move(thenBranch)}, _elseBranch{std::move(elseBranch)} {}
      llvm::Value* codegen() override;

   private:
      std::unique_ptr<ExpressionAST> _condition;
      std::unique_ptr<ExpressionAST> _thenBranch;
      std::unique_ptr<ExpressionAST> _elseBranch;
   };

   class WhileStatementAST : public ExpressionAST {
   public:
      WhileStatementAST(std::unique_ptr<ExpressionAST> condition, std::unique_ptr<ExpressionAST> body)
          : _condition{std::move(condition)}, _body{std::move(body)} {}
      llvm::Value* codegen() override;

   private:
      std::unique_ptr<ExpressionAST> _condition;
      std::unique_ptr<ExpressionAST> _body;
   };
   class DoWhileStatementAST : public ExpressionAST {
   public:
      DoWhileStatementAST(std::unique_ptr<ExpressionAST> condition, std::unique_ptr<ExpressionAST> body)
          : _condition{std::move(condition)}, _body{std::move(body)} {}
      llvm::Value* codegen() override;

   private:
      std::unique_ptr<ExpressionAST> _condition;
      std::unique_ptr<ExpressionAST> _body;
   };
   class ForStatementAST : public ExpressionAST {
   public:
      ForStatementAST(std::unique_ptr<ExpressionAST> init, std::unique_ptr<ExpressionAST> condition, std::unique_ptr<ExpressionAST> increment, std::unique_ptr<ExpressionAST> body)
          : _init{std::move(init)}, _condition{std::move(condition)}, _increment{std::move(increment)}, _body{std::move(body)} {}
      llvm::Value* codegen() override;

   private:
      std::unique_ptr<ExpressionAST> _init;
      std::unique_ptr<ExpressionAST> _condition;
      std::unique_ptr<ExpressionAST> _increment;
      std::unique_ptr<ExpressionAST> _body;
   };

   class BlockStatementAST : public ExpressionAST {
   public:
      BlockStatementAST(std::vector<std::unique_ptr<ExpressionAST>> statements)
          : _statements{std::move(statements)} {}
      llvm::Value* codegen() override;

   private:
      std::vector<std::unique_ptr<ExpressionAST>> _statements;
   };

   class ReturnStatementAST : public ExpressionAST {
   public:
      ReturnStatementAST(std::unique_ptr<ExpressionAST> value) : _value{std::move(value)} {}
      llvm::Value* codegen() override;

   private:
      std::unique_ptr<ExpressionAST> _value;
   };

   class ExitStatement : public ExpressionAST {
   public:
      ExitStatement(std::unique_ptr<ExpressionAST> value) : _value{std::move(value)} {}
      llvm::Value* codegen() override;

   private:
      std::unique_ptr<ExpressionAST> _value;
   };

   //  prototype for function declarations
   class FunctionPrototypeAST {
   public:
      FunctionPrototypeAST(const std::string& type, const std::string& name, std::vector<std::unique_ptr<ExpressionAST>> args)
          : _type{type}, _name{name}, _args{std::move(args)} {}
      llvm::Function* codegen();

      _NODISCARD const std::string& name() const { return _name; }
      _NODISCARD const std::string& type() const { return _type; }

   private:
      std::string _type;
      std::string _name;
      std::vector<std::unique_ptr<ExpressionAST>> _args;
   };

   // expression for external function declarations
   class ExternStatementAST : public ExpressionAST {
   public:
      ExternStatementAST(std::unique_ptr<FunctionPrototypeAST> value) : _value{std::move(value)} {}
      llvm::Value* codegen() override;

   private:
      std::unique_ptr<FunctionPrototypeAST> _value;
   };

   // function definition itself
   class FunctionAST {
   public:
      FunctionAST(std::unique_ptr<FunctionPrototypeAST> prototype, std::unique_ptr<ExpressionAST> body)
          : _prototype{std::move(prototype)}, _body{std::move(body)} {}
      llvm::Function* codegen();

      _NODISCARD const std::string& name() const { return _prototype->name(); }
      _NODISCARD const std::string& type() const { return _prototype->type(); }

   private:
      std::unique_ptr<FunctionPrototypeAST> _prototype;
      std::unique_ptr<ExpressionAST> _body;
   };

   // classes
   enum class Visibility {
      Private,
      Public,
      Protected
   };
   enum class MemberType {
      Variable,
      Method,
      Constructor,
      Destructor
   };

   class ClassMethodAST {
   public:
      ClassMethodAST(Visibility visibility, const std::string& name, const std::string& type, bool isStatic, std::unique_ptr<FunctionAST> method)
          : _visibility{visibility}, _name{name}, _type{type}, _isStatic{isStatic}, _method{std::move(method)} {}
      llvm::Value* codegen();

   private:
      Visibility _visibility;
      std::string _name;
      std::string _type;
      bool _isStatic;
      std::unique_ptr<FunctionAST> _method;
   };

   class ClassVariableAST {
   public:
      ClassVariableAST(Visibility visibility, const std::string& name, const std::string& type, bool isStatic, std::unique_ptr<ExpressionAST> variable)
          : _visibility{visibility}, _name{name}, _type{type}, _isStatic{isStatic}, _variable{std::move(variable)} {}
      llvm::Value* codegen();

   private:
      Visibility _visibility;
      std::string _name;
      std::string _type;
      bool _isStatic;
      std::unique_ptr<ExpressionAST> _variable;
   };
   class ClassConstructorAST {
   public:
      ClassConstructorAST(Visibility visibility, std::unique_ptr<FunctionAST> constructor)
          : _visibility{visibility}, _constructor{std::move(constructor)} {}
      llvm::Value* codegen();

   private:
      Visibility _visibility;
      std::unique_ptr<FunctionAST> _constructor;
   };
   class ClassDestructorAST {
   public:
      ClassDestructorAST(std::unique_ptr<FunctionAST> destructor)
          : _destructor{std::move(destructor)} {}
      llvm::Value* codegen();

   private:
      std::unique_ptr<FunctionAST> _destructor;
   };

   class ClassAST {
   public:
      ClassAST(const std::string& name, std::vector<std::unique_ptr<ClassVariableAST>> variables, std::vector<std::unique_ptr<ClassMethodAST>> methods,
               std::vector<std::unique_ptr<ClassConstructorAST>> constructors, std::unique_ptr<ClassDestructorAST> destructor)
          : _name{name}, _variables{std::move(variables)}, _methods{std::move(methods)}, _constructors{std::move(constructors)}, _destructor{std::move(destructor)} {}
      llvm::Value* codegen();

      _NODISCARD const std::string& name() const { return _name; }

   private:
      std::string _name;
      std::vector<std::unique_ptr<ClassVariableAST>> _variables;
      std::vector<std::unique_ptr<ClassMethodAST>> _methods;
      std::vector<std::unique_ptr<ClassConstructorAST>> _constructors;
      std::unique_ptr<ClassDestructorAST> _destructor;
   };

   class NamespaceAST : public ExpressionAST {
   public:
      NamespaceAST(const std::string& name, std::vector<std::unique_ptr<ExpressionAST>> variables, std::vector<std::unique_ptr<FunctionAST>> functions, std::vector<std::unique_ptr<ClassAST>> classes)
          : _name{name}, _variables{std::move(variables)}, _functions{std::move(functions)}, _classes{std::move(classes)} {}
      llvm::Value* codegen() override;

   private:
      std::string _name;
      std::vector<std::unique_ptr<ExpressionAST>> _variables;
      std::vector<std::unique_ptr<FunctionAST>> _functions;
      std::vector<std::unique_ptr<ClassAST>> _classes;
   };
   //  ========================================== implementation ==========================================

   llvm::Value* NumberExpressionAST::codegen() {
      return std::visit([](auto&& val) -> llvm::Value* {
         using T = std::decay_t<decltype(val)>;

         if constexpr (std::is_same_v<T, int>) {
            return llvm::ConstantInt::get(*TheContext, llvm::APInt(32, val, true));
         } else if constexpr (std::is_same_v<T, int8_t>) {
            return llvm::ConstantInt::get(*TheContext, llvm::APInt(8, val, true));
         } else if constexpr (std::is_same_v<T, int16_t>) {
            return llvm::ConstantInt::get(*TheContext, llvm::APInt(16, val, true));
         } else if constexpr (std::is_same_v<T, int64_t>) {
            return llvm::ConstantInt::get(*TheContext, llvm::APInt(64, val, true));
         } else if constexpr (std::is_same_v<T, uint8_t>) {
            return llvm::ConstantInt::get(*TheContext, llvm::APInt(8, val, false));
         } else if constexpr (std::is_same_v<T, uint16_t>) {
            return llvm::ConstantInt::get(*TheContext, llvm::APInt(16, val, false));
         } else if constexpr (std::is_same_v<T, uint32_t>) {
            return llvm::ConstantInt::get(*TheContext, llvm::APInt(32, val, false));
         } else if constexpr (std::is_same_v<T, float>) {
            return llvm::ConstantFP::get(llvm::Type::getFloatTy(*TheContext), val);
         } else if constexpr (std::is_same_v<T, double>) {
            return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*TheContext), val);
         } else {
            return nullptr;
         }
      },
                        _value);
   }

   llvm::Value* VariableExpressionAST::codegen() {
      llvm::Value* V = NamedValues[_name];
      if (!V)
         return LogErrorV("unknown variable '" + _name + "'");
      return V;
   }

   llvm::Value* StringExpression::codegen() {
      return Builder->CreateGlobalStringPtr(_value, "strtmp");
   }

   llvm::Value* BinaryExpressionAST::codegen() {
      llvm::Value* L = _LHS->codegen();
      llvm::Value* R = _RHS->codegen();
      if (!L || !R)
         return nullptr;

      if (!L->getType()->isFloatingPointTy() && !R->getType()->isFloatingPointTy()) {
         if (_op == "+")
            return Builder->CreateAdd(L, R, "addtmp");
         if (_op == "-")
            return Builder->CreateSub(L, R, "subtmp");
         if (_op == "*")
            return Builder->CreateMul(L, R, "multmp");
         if (_op == "/")
            return Builder->CreateSDiv(L, R, "divtmp");

         if (_op == "%")
            return Builder->CreateSRem(L, R, "modtmp");
         if (_op == "&")
            return Builder->CreateAnd(L, R, "andtmp");
         if (_op == "|")
            return Builder->CreateOr(L, R, "ortmp");
         if (_op == "^")
            return Builder->CreateXor(L, R, "xortmp");
         if (_op == "==")
            return Builder->CreateICmpEQ(L, R, "eqtmp");

         if (_op == "=") {
            if (!L->getType()->isPointerTy() || !_LHS->isLValue()) {
               return LogErrorV("left side of assignment must be a variable or assignable expression");
            }
            return Builder->CreateStore(R, L);
         }
         if (_op == "!=")
            return Builder->CreateICmpNE(L, R, "netmp");
         if (_op == "<")
            return Builder->CreateICmpSLT(L, R, "lttmp");
         if (_op == "<=")
            return Builder->CreateICmpSLE(L, R, "letmp");
         if (_op == ">")
            return Builder->CreateICmpSGT(L, R, "gttmp");
         if (_op == ">=")
            return Builder->CreateICmpSGE(L, R, "getmp");

      } else {
         if (_op == "+")
            return Builder->CreateFAdd(L, R, "addtmp");
         if (_op == "-")
            return Builder->CreateFSub(L, R, "subtmp");
         if (_op == "*")
            return Builder->CreateFMul(L, R, "multmp");
         if (_op == "/")
            return Builder->CreateFDiv(L, R, "divtmp");

         if (_op == "%")
            return Builder->CreateFRem(L, R, "modtmp");
         if (_op == "&")
            return Builder->CreateAnd(L, R, "andtmp");
         if (_op == "|")
            return Builder->CreateOr(L, R, "ortmp");
         if (_op == "^")
            return Builder->CreateXor(L, R, "xortmp");
         if (_op == "==")
            return Builder->CreateFCmpOEQ(L, R, "eqtmp");
         if (_op == "=") {
            if (!L->getType()->isPointerTy() || !_LHS->isLValue()) {
               return LogErrorV("left side of assignment must be a variable or assignable expression");
            }
            return Builder->CreateStore(R, L);
         }
         if (_op == "!=")
            return Builder->CreateFCmpONE(L, R, "netmp");
         if (_op == "<")
            return Builder->CreateFCmpOLT(L, R, "netmp");
         if (_op == "<=")
            return Builder->CreateFCmpOLE(L, R, "netmp");
         if (_op == ">")
            return Builder->CreateFCmpOGT(L, R, "gttmp");
         if (_op == ">=")
            return Builder->CreateFCmpOGE(L, R, "getmp");
      }


      // if (_op == "&&")
      //    return
      // TODO: add other operators
   }

   llvm::Value* VariableDeclarationAST::codegen() {
      llvm::Type* type = LLVMType(_type);
      if (!type)
         return LogErrorV("unknown type '" + _type + "'");

      llvm::Function* function = Builder->GetInsertBlock()->getParent();
      llvm::IRBuilder tmpBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());
      llvm::AllocaInst* allocInstance = tmpBuilder.CreateAlloca(type, nullptr, _name);

      if (_init) {
         llvm::Value* initVal = _init->codegen();
         if (!initVal)
            return nullptr;

         Builder->CreateStore(initVal, allocInstance);
      }

      NamedValues[_name] = allocInstance;

      return allocInstance;
   }

   llvm::Value* CallExpressionAST::codegen() {
      llvm::Function* callee = TheModule->getFunction(_callee);
      if (!callee)
         return LogErrorV("unknown function '" + _callee + "'");

      bool areParamsOk = true;

      if (callee->arg_size() != _args.size())
         areParamsOk = false;

      std::vector<llvm::Value*> args;
      std::string argsTypes;
      for (unsigned i = 0; i < _args.size(); ++i) {
         llvm::Value* argVal = _args[i]->codegen();
         if (!argVal)
            return nullptr;

         llvm::Type* expectedType = callee->getFunctionType()->getParamType(i);
         argsTypes += i == _args.size() - 1 ? getTypeString(argVal) + ", " : getTypeString(argVal);
         if (argVal->getType() != expectedType) {
            areParamsOk = false;
         }

         args.push_back(argVal);
      }
      if (!areParamsOk) {
         if (callee->arg_size() == 0)
            return LogErrorV("no instance of function " + _callee + " with no args");

         return LogErrorV("no instance of function " + _callee + " with args '" + argsTypes + "'");
      }

      return Builder->CreateCall(callee->getFunctionType(), callee, args, "calltmp");
   }

   llvm::Value* TypeOfStatementAST::codegen() {
      return nullptr;
   }
   llvm::Value* NameOfStatementAST::codegen() {
      return nullptr;
   }
   llvm::Value* SizeOfStatementAST::codegen() {
      return nullptr;
   }

   llvm::Value* __BuiltinFunctionAST::codegen() {
      return Builder->CreateGlobalStringPtr(_function, "strtmp");
   }
   llvm::Value* __BuiltinLineAST::codegen() {
      return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*TheContext), _line, false);
   }
   llvm::Value* __BuiltinColumnAST::codegen() {
      return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*TheContext), _column, false);
   }
   llvm::Value* __BuiltinFileAST::codegen() {
      return Builder->CreateGlobalStringPtr(_file, "strtmp");
   }

   llvm::Value* BlockStatementAST::codegen() {
      llvm::Value* last = nullptr;
      for (const auto& statement : _statements) {
         last = statement->codegen();
         if (!last) {
            return nullptr;
         }
      }
      return last;
   }

   llvm::Value* ExternStatementAST::codegen() {
      return nullptr;
   }
   llvm::Value* ReturnStatementAST::codegen() {
      llvm::Value* returnValue = _value->codegen();
      return Builder->CreateRet(returnValue);
   }
   llvm::Value* ExitStatement::codegen() {
      llvm::Value* exitValue = _value->codegen();
      if (!exitValue)
         return LogErrorV("exit statement requires a value");

      llvm::Function* exitFunc = TheModule->getFunction("exit");
      if (!exitFunc) {
         llvm::FunctionType* exitType = llvm::FunctionType::get(
             llvm::Type::getVoidTy(*TheContext),
             {llvm::Type::getInt32Ty(*TheContext)},
             false);
         exitFunc = llvm::Function::Create(
             exitType, llvm::Function::ExternalLinkage, "exit", TheModule.get());
      }

      if (exitValue->getType() != llvm::Type::getInt32Ty(*TheContext)) {
         exitValue = Builder->CreateIntCast(exitValue, llvm::Type::getInt32Ty(*TheContext), false);
      }

      llvm::Value* call = Builder->CreateCall(exitFunc, exitValue);

      Builder->CreateUnreachable();

      return call;
   }

   llvm::Value* IfStatementAST::codegen() {
      llvm::Value* CondV = _condition->codegen();
      if (!CondV)
         return nullptr;

      CondV = Builder->CreateICmpNE(CondV, llvm::ConstantInt::get(CondV->getType(), 0), "ifcond");

      llvm::Function* function = Builder->GetInsertBlock()->getParent();

      llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(*TheContext, "then", function);
      llvm::BasicBlock* ElseBB = llvm::BasicBlock::Create(*TheContext, "else");
      llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(*TheContext, "ifcont");

      Builder->CreateCondBr(CondV, ThenBB, ElseBB);

      // then
      Builder->SetInsertPoint(ThenBB);
      if (_thenBranch)
         _thenBranch->codegen();
      if (!Builder->GetInsertBlock()->getTerminator())
         Builder->CreateBr(MergeBB);

      // else
      function->insert(function->end(), ElseBB);
      Builder->SetInsertPoint(ElseBB);
      if (_elseBranch)
         _elseBranch->codegen();
      if (!Builder->GetInsertBlock()->getTerminator())
         Builder->CreateBr(MergeBB);

      // end of if
      function->insert(function->end(), MergeBB);
      Builder->SetInsertPoint(MergeBB);

      return llvm::Constant::getNullValue(llvm::Type::getInt32Ty(*TheContext));
   }


   llvm::Value* WhileStatementAST::codegen() {
      return nullptr;
   }

   llvm::Value* DoWhileStatementAST::codegen() {
      return nullptr;
   }

   llvm::Value* ForStatementAST::codegen() {
      return nullptr;
   }

   llvm::Function* FunctionPrototypeAST::codegen() {
      std::vector<llvm::Type*> paramTypes;
      for (const auto& arg : _args) {
         llvm::Type* type = LLVMType(static_cast<VariableDeclarationAST*>(arg.get())->type());
         if (!type)
            return LogErrorF("unknown type: '" + static_cast<VariableDeclarationAST*>(arg.get())->type() + "' in argument declaration");
         paramTypes.push_back(type);
      }
      llvm::Type* returnType = LLVMType(_type);
      if (!returnType)
         return LogErrorF("unknown type: '" + _type + "' in function declaration");

      llvm::FunctionType* functionType = llvm::FunctionType::get(returnType, paramTypes, false);
      llvm::Function* function = llvm::Function::Create(
          functionType,
          llvm::Function::ExternalLinkage,
          _name,
          TheModule.get());

      return function;
   }

   llvm::Function* FunctionAST::codegen() {
      llvm::Function* function = _prototype->codegen();
      if (!function)
         return nullptr;

      llvm::BasicBlock* BB = llvm::BasicBlock::Create(*TheContext, "entry", function);
      Builder->SetInsertPoint(BB);

      if (!_body || !_body->codegen()) {
         function->eraseFromParent();
         return LogErrorF("error in function: '" + name() + "'");
      }

      if (Builder->GetInsertBlock()->getTerminator() == nullptr) {
         return LogErrorF("missing return statement in function: '" + name() + "'");
      }

      llvm::verifyFunction(*function, &llvm::errs());
      return function;
   }

   llvm::Value* NamespaceAST::codegen() {
      return nullptr;
   }

   llvm::Value* ClassMethodAST::codegen() {
      return nullptr;
   }
   llvm::Value* ClassVariableAST::codegen() {
      return nullptr;
   }
   llvm::Value* ClassConstructorAST::codegen() {
      return nullptr;
   }
   llvm::Value* ClassDestructorAST::codegen() {
      return nullptr;
   }
   llvm::Value* ClassAST::codegen() {
      return nullptr;
   }

} // namespace tungsten
