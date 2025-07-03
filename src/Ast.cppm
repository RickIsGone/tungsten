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

   llvm::Type* LLVMType(const std::string& type) {
      using namespace llvm;
      if (type == "Void")
         return Type::getVoidTy(*TheContext);
      if (type == "Bool")
         return Type::getInt1Ty(*TheContext);
      if (type == "Char")
         return Type::getInt8Ty(*TheContext);
      if (type == "String")
         return PointerType::getInt8Ty(*TheContext);
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
} // namespace tungsten

export namespace tungsten {
   void initLLVM() {
      TheContext = std::make_unique<llvm::LLVMContext>();
      Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
      TheModule = std::make_unique<llvm::Module>("tungsten", *TheContext);
   }
   // base for all nodes in the AST
   class ExpressionAST {
   public:
      virtual ~ExpressionAST() = default;
      virtual llvm::Value* codegen() = 0;
   };

   // expression for numbers
   using Number = std::variant<int, int8_t, int16_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t, float, double>;
   class NumberExpressionAST : public ExpressionAST {
   public:
      NumberExpressionAST(Number value) : _value{value} {}
      llvm::Value* codegen() override;

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

   private:
      std::unique_ptr<FunctionPrototypeAST> _prototype;
      std::unique_ptr<ExpressionAST> _body;
   };

   //  ========================================== implementation ==========================================

   llvm::Value* NumberExpressionAST::codegen() {
      return nullptr;
   }

   llvm::Value* VariableExpressionAST::codegen() {
      return nullptr;
   }

   llvm::Value* StringExpression::codegen() {
      return Builder->CreateGlobalString(_value);
   }

   llvm::Value* BinaryExpressionAST::codegen() {
      return nullptr;
   }

   llvm::Value* VariableDeclarationAST::codegen() {
      llvm::Type* type = LLVMType(_type);
      if (!type)
         return LogErrorV("unknown type '" + _type + "'");

      return nullptr;
   }

   llvm::Value* CallExpressionAST::codegen() {
      return nullptr;
   }

   llvm::Value* BlockStatementAST::codegen() {
      return nullptr;
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
      return nullptr;
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
      return nullptr;
   }

   llvm::Function* FunctionAST::codegen() {
      return nullptr;
   }

} // namespace tungsten
