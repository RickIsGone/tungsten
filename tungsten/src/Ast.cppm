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
   std::map<std::string, llvm::Type*> VariableTypes{};
   llvm::Value* LogErrorV(const std::string& Str) {
      std::cerr << "error: " << Str << "\n";
      return nullptr;
   }
   llvm::Function* LogErrorF(const std::string& Str) {
      std::cerr << "error: " << Str << "\n";
      return nullptr;
   }

   bool isSignedType(const std::string& type) {
      return type == "Int" || type == "Int8" || type == "Int16" || type == "Int32" || type == "Int64" || type == "Int128";
   }
   bool isUnsignedType(const std::string& type) {
      return type == "Uint" || type == "Uint8" || type == "Uint16" || type == "Uint32" || type == "Uint64" || type == "Uint128";
   }
   bool isFloatingPointType(const std::string& type) {
      return type == "Float" || type == "Double";
   }

   llvm::Type* LLVMType(const std::string& type) {
      using namespace llvm;
      if (type == "Void")
         return Builder->getVoidTy();
      if (type == "Bool")
         return Builder->getInt1Ty();
      if (type == "Char")
         return Builder->getInt8Ty();
      if (type == "String")
         return Builder->getInt8Ty()->getPointerTo();
      if (type == "Int" || type == "Int32" || type == "Uint" || type == "Uint32")
         return Builder->getInt32Ty();
      if (type == "Int8" || type == "Uint8")
         return Builder->getInt8Ty();
      if (type == "Int16" || type == "Uint16")
         return Builder->getInt16Ty();
      if (type == "Int64" || type == "Uint64")
         return Builder->getInt64Ty();
      if (type == "Int128" || type == "Uint128")
         return Builder->getInt128Ty();
      if (type == "Float")
         return Builder->getFloatTy();
      if (type == "Double")
         return Builder->getDoubleTy();
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

      if (type->isVoidTy()) {
         return "Void";
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
   void initLLVM(const std::string& moduleName, const std::string& fileName) {
      TheContext = std::make_unique<llvm::LLVMContext>();
      Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
      TheModule = std::make_unique<llvm::Module>(moduleName, *TheContext);
      TheModule->setSourceFileName(fileName);
   }
   void dumpIR() {
#ifdef TUNGSTEN_DEBUG

      std::error_code EC;
      llvm::raw_fd_ostream outFile(TheModule->getModuleIdentifier() + ".ll", EC, llvm::sys::fs::OF_None);

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
      _NODISCARD const std::string& name() const { return _name; }
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

   // cast expressions
   class StaticCastAST : public ExpressionAST {
   public:
      StaticCastAST(const std::string& type, std::unique_ptr<ExpressionAST> value)
          : _type{type}, _value{std::move(value)} {}

      _NODISCARD const std::string& type() const { return _type; }
      llvm::Value* codegen() override;

   private:
      std::string _type;
      std::unique_ptr<ExpressionAST> _value;
   };
   class ConstCastAST : public ExpressionAST {
   public:
      ConstCastAST(const std::string& type, std::unique_ptr<ExpressionAST> value)
          : _type{type}, _value{std::move(value)} {}

      _NODISCARD const std::string& type() const { return _type; }
      llvm::Value* codegen() override;

   private:
      std::string _type;
      std::unique_ptr<ExpressionAST> _value;
   };

   // expression for if statements
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
      _NODISCARD const std::vector<std::unique_ptr<ExpressionAST>>& args() const { return _args; }

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
            return Builder->getIntN(32, val);
         } else if constexpr (std::is_same_v<T, int8_t>) {
            return Builder->getIntN(8, val);
         } else if constexpr (std::is_same_v<T, int16_t>) {
            return Builder->getIntN(16, val);
         } else if constexpr (std::is_same_v<T, int64_t>) {
            return Builder->getIntN(64, val);
         } else if constexpr (std::is_same_v<T, uint8_t>) {
            return Builder->getIntN(8, val);
         } else if constexpr (std::is_same_v<T, uint16_t>) {
            return Builder->getIntN(16, val);
         } else if constexpr (std::is_same_v<T, uint32_t>) {
            return Builder->getIntN(32, val);
         } else if constexpr (std::is_same_v<T, uint64_t>) {
            return Builder->getIntN(64, val);
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
      return Builder->CreateLoad(VariableTypes[_name], V, "varval");
   }

   llvm::Value* StringExpression::codegen() {
      return Builder->CreateGlobalStringPtr(_value, "strtmp");
   }

   llvm::Value* BinaryExpressionAST::codegen() { // TODO: fix
      llvm::Value* L = _LHS->codegen();
      llvm::Value* R = _RHS->codegen();
      if (!L || !R)
         return nullptr;

      auto castRToLType = [](llvm::Value*& L, llvm::Value*& R) -> bool {
         llvm::Type* typeL = L->getType();
         llvm::Type* typeR = R->getType();

         if (typeL == typeR)
            return true;

         if (typeL->isIntegerTy() && typeR->isIntegerTy()) {
            R = Builder->CreateIntCast(R, typeL, false, "castR");
            return true;
         }

         if (typeL->isIntegerTy() && typeR->isFloatingPointTy()) {
            R = Builder->CreateFPToUI(R, typeL, "castR");
            return true;
         }

         if (typeL->isDoubleTy() && typeR->isIntegerTy()) {
            R = Builder->CreateUIToFP(R, llvm::Type::getDoubleTy(*TheContext), "castR");
            return true;
         }

         if (typeL->isFloatingPointTy() && typeR->isIntegerTy()) {
            R = Builder->CreateUIToFP(R, typeL, "castR");
            return true;
         }

         if (typeL->isDoubleTy() && typeR->isFloatTy()) {
            R = Builder->CreateFPExt(R, llvm::Type::getDoubleTy(*TheContext), "castR");
            return true;
         }
         if (typeL->isFloatTy() && typeR->isDoubleTy()) {
            R = Builder->CreateFPTrunc(R, llvm::Type::getFloatTy(*TheContext), "castR");
            return true;
         }

         if (typeL->isFloatingPointTy() && typeR->isFloatingPointTy()) {
            unsigned bitsL = typeL->getPrimitiveSizeInBits();
            unsigned bitsR = typeR->getPrimitiveSizeInBits();
            if (bitsR < bitsL) {
               R = Builder->CreateFPExt(R, typeL, "castR");
            } else if (bitsR > bitsL) {
               R = Builder->CreateFPTrunc(R, typeL, "castR");
            }
            return true;
         }

         LogErrorV("unsupported or unsafe type cast between operands");
         return false;
      };

      if (_op == "=" || _op == "+=" || _op == "-=" || _op == "*=" || _op == "/=" || _op == "%=" || _op == "|=" || _op == "&=") {
         if (/*!L->getType()->isPointerTy() || */ !_LHS->isLValue())
            return LogErrorV("left side of assignment must be a variable or assignable expression");

         llvm::Value* loadedL = L;
         if (_op != "=" && _op != "+=" && _op != "-=" && _op != "*=" && _op != "/=" && _op != "%=" && _op != "|=" && _op != "&=") {
            if (auto* varExpr = dynamic_cast<VariableExpressionAST*>(_LHS.get())) {
               llvm::Type* ltype = VariableTypes[varExpr->name()];
               // loadedL = Builder->CreateLoad(ltype, L, "lval");
            }
         }

         if (R->getType()->isPointerTy() && !_RHS->isLValue()) {
            if (auto* varExpr = dynamic_cast<VariableExpressionAST*>(_RHS.get())) {
               llvm::Type* rtype = VariableTypes[varExpr->name()];
               // R = Builder->CreateLoad(rtype, R, "rval");
            }
         }

         if (!castRToLType(loadedL, R))
            return nullptr;

         llvm::Value* result = nullptr;
         if (_op == "=") {
            result = R;
         } else if (_op == "+=") {
            result = loadedL->getType()->isFloatingPointTy() ? Builder->CreateFAdd(loadedL, R, "addtmp") : Builder->CreateAdd(loadedL, R, "addtmp");
         } else if (_op == "-=") {
            result = loadedL->getType()->isFloatingPointTy() ? Builder->CreateFSub(loadedL, R, "subtmp") : Builder->CreateSub(loadedL, R, "subtmp");
         } else if (_op == "*=") {
            result = loadedL->getType()->isFloatingPointTy() ? Builder->CreateFMul(loadedL, R, "multmp") : Builder->CreateMul(loadedL, R, "multmp");
         } else if (_op == "/=") {
            result = loadedL->getType()->isFloatingPointTy() ? Builder->CreateFDiv(loadedL, R, "divtmp") : Builder->CreateSDiv(loadedL, R, "divtmp");
         } else if (_op == "%=") {
            result = loadedL->getType()->isFloatingPointTy() ? Builder->CreateFRem(loadedL, R, "modtmp") : Builder->CreateSRem(loadedL, R, "modtmp");
         } else if (_op == "|=") {
            result = Builder->CreateOr(loadedL, R, "ortmp");
         } else if (_op == "&=") {
            result = Builder->CreateAnd(loadedL, R, "andtmp");
         }
         return Builder->CreateStore(result, L);
      }

      if (L->getType()->isPointerTy() && !_LHS->isLValue()) {
         if (auto* varExpr = dynamic_cast<VariableExpressionAST*>(_LHS.get())) {
            llvm::Type* ltype = VariableTypes[varExpr->name()];
            // L = Builder->CreateLoad(ltype, L, "lval");
         }
      }
      if (R->getType()->isPointerTy() && !_RHS->isLValue()) {
         if (auto* varExpr = dynamic_cast<VariableExpressionAST*>(_RHS.get())) {
            llvm::Type* rtype = VariableTypes[varExpr->name()];
            // R = Builder->CreateLoad(rtype, R, "rval");
         }
      }

      if (!castRToLType(L, R))
         return nullptr;

      if (_op == "&&") {
         L = Builder->CreateICmpNE(L, llvm::ConstantInt::get(L->getType(), 0), "lcond");
         R = Builder->CreateICmpNE(R, llvm::ConstantInt::get(R->getType(), 0), "rcond");
         return Builder->CreateAnd(L, R, "andtmp");
      }
      if (_op == "||") {
         L = Builder->CreateICmpNE(L, llvm::ConstantInt::get(L->getType(), 0), "lcond");
         R = Builder->CreateICmpNE(R, llvm::ConstantInt::get(R->getType(), 0), "rcond");
         return Builder->CreateOr(L, R, "ortmp");
      }

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
         if (_op == "==")
            return Builder->CreateFCmpOEQ(L, R, "eqtmp");
         if (_op == "!=")
            return Builder->CreateFCmpONE(L, R, "netmp");
         if (_op == "<")
            return Builder->CreateFCmpOLT(L, R, "lttmp");
         if (_op == "<=")
            return Builder->CreateFCmpOLE(L, R, "letmp");
         if (_op == ">")
            return Builder->CreateFCmpOGT(L, R, "gttmp");
         if (_op == ">=")
            return Builder->CreateFCmpOGE(L, R, "getmp");
      }

      return nullptr;
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
      VariableTypes[_name] = type;

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
      if (!NamedValues.contains(_variable) || !VariableTypes.contains(_variable))
         return LogErrorV("unknown variable '" + _variable + "'");

      llvm::Type* type = VariableTypes[_variable];
      return nullptr; // Builder->CreateGlobalStringPtr() TODO: fix
   }
   llvm::Value* NameOfStatementAST::codegen() {
      if (!NamedValues.contains(_name))
         return LogErrorV("unknown variable '" + _name + "'");

      return Builder->CreateGlobalStringPtr(_name, "strtmp");
   }
   llvm::Value* SizeOfStatementAST::codegen() {
      if (!NamedValues.contains(_variable) || !VariableTypes.contains(_variable)) {
         llvm::Type* type = LLVMType(_variable);
         if (type)
            return Builder->getInt64(TheModule->getDataLayout().getTypeAllocSize(type));
         return LogErrorV("unknown variable or type '" + _variable + "'");
      }

      uint64_t size = TheModule->getDataLayout().getTypeAllocSize(VariableTypes[_variable]);

      return Builder->getInt64(size);
   }

   llvm::Value* __BuiltinFunctionAST::codegen() {
      return Builder->CreateGlobalStringPtr(_function, "strtmp");
   }
   llvm::Value* __BuiltinLineAST::codegen() {
      return Builder->getInt64(_line);
   }
   llvm::Value* __BuiltinColumnAST::codegen() {
      return Builder->getInt64(_column);
   }
   llvm::Value* __BuiltinFileAST::codegen() {
      return Builder->CreateGlobalStringPtr(_file, "strtmp");
   }

   llvm::Value* StaticCastAST::codegen() {
      llvm::Type* type = LLVMType(_type);
      if (!type)
         return LogErrorV("unknown type '" + _type + "'");

      if (!_value)
         return nullptr;
      llvm::Value* castedValue = _value->codegen();
      if (castedValue == nullptr)
         return nullptr;

      if (castedValue->getType()->isPointerTy()) // didn't add pointers yet so this check is ok
         castedValue = Builder->CreateLoad(VariableTypes[static_cast<VariableExpressionAST*>(_value.get())->name()], castedValue, "lval");

      if (isSignedType(_type)) {
         return Builder->CreateIntCast(castedValue, type, true, "staticCast");
      }
      if (isUnsignedType(_type)) {
         return Builder->CreateIntCast(castedValue, type, false, "staticCast");
      }
      if (isFloatingPointType(_type)) {
         return Builder->CreateFPCast(castedValue, type, "staticCast");
      }

      return nullptr;
   }
   llvm::Value* ConstCastAST::codegen() {
      return nullptr;
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
      llvm::Function* function = Builder->GetInsertBlock()->getParent();

      llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*TheContext, "CondBB", function);
      llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*TheContext, "BodyBB");
      llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*TheContext, "whilecont");

      Builder->CreateBr(condBB);

      // condtion
      Builder->SetInsertPoint(condBB);
      llvm::Value* CondV = _condition->codegen();
      if (!CondV)
         return nullptr;
      CondV = Builder->CreateICmpNE(CondV, llvm::ConstantInt::get(CondV->getType(), 0), "whilecond");
      Builder->CreateCondBr(CondV, bodyBB, endBB);

      // body
      function->insert(function->end(), bodyBB);
      Builder->SetInsertPoint(bodyBB);
      if (_body)
         _body->codegen();
      Builder->CreateBr(condBB);

      // end of while
      function->insert(function->end(), endBB);
      Builder->SetInsertPoint(endBB);

      return llvm::Constant::getNullValue(llvm::Type::getInt32Ty(*TheContext));
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

      size_t idx = 0;
      for (auto& arg : function->args()) {
         auto* varDecl = static_cast<VariableDeclarationAST*>(_prototype->args()[idx].get());
         llvm::IRBuilder<> tmpBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());
         llvm::AllocaInst* alloca = tmpBuilder.CreateAlloca(arg.getType(), nullptr, varDecl->name());
         Builder->CreateStore(&arg, alloca);
         NamedValues[varDecl->name()] = alloca;
         VariableTypes[varDecl->name()] = arg.getType();
         ++idx;
      }

      if (!_body || !_body->codegen()) {
         function->eraseFromParent();
         return LogErrorF("error in function: '" + name() + "'");
      }

      if (Builder->GetInsertBlock()->getTerminator() == nullptr) {
         if (_prototype->type() == "Void")
            Builder->CreateRetVoid();
         else
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
