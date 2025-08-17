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
      llvm::verifyModule(*TheModule, &llvm::outs());
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
   struct ASTVisitor {
      virtual ~ASTVisitor() = default;
      virtual void visit(class NumberExpressionAST&) = 0;
      virtual void visit(class VariableExpressionAST&) = 0;
      virtual void visit(class StringExpression&) = 0;
      virtual void visit(class UnaryExpressionAST&) = 0;
      virtual void visit(class BinaryExpressionAST&) = 0;
      virtual void visit(class VariableDeclarationAST&) = 0;
      virtual void visit(class CallExpressionAST&) = 0;
      virtual void visit(class TypeOfStatementAST&) = 0;
      virtual void visit(class NameOfStatementAST&) = 0;
      virtual void visit(class SizeOfStatementAST&) = 0;
      virtual void visit(class __BuiltinFunctionAST&) = 0;
      virtual void visit(class __BuiltinLineAST&) = 0;
      virtual void visit(class __BuiltinColumnAST&) = 0;
      virtual void visit(class __BuiltinFileAST&) = 0;
      virtual void visit(class StaticCastAST&) = 0;
      virtual void visit(class ConstCastAST&) = 0;
      virtual void visit(class IfStatementAST&) = 0;
      virtual void visit(class WhileStatementAST&) = 0;
      virtual void visit(class DoWhileStatementAST&) = 0;
      virtual void visit(class ForStatementAST&) = 0;
      virtual void visit(class BlockStatementAST&) = 0;
      virtual void visit(class ReturnStatementAST&) = 0;
      virtual void visit(class ExitStatement&) = 0;
      virtual void visit(class ExternStatementAST&) = 0;
      virtual void visit(class NamespaceAST&) = 0;
      virtual void visit(class ImportStatementAST&) = 0;

      virtual void visit(class FunctionPrototypeAST&) = 0;
      virtual void visit(class FunctionAST&) = 0;

      virtual void visit(class ClassMethodAST&) = 0;
      virtual void visit(class ClassVariableAST&) = 0;
      virtual void visit(class ClassConstructorAST&) = 0;
      virtual void visit(class ClassDestructorAST&) = 0;
      virtual void visit(class ClassAST&) = 0;
   };

   enum class TypeKind {
      Void,
      Char,
      Bool,
      String,
      Int8,
      Int16,
      Int32,
      Int64,
      Int128,
      Uint8,
      Uint16,
      Uint32,
      Uint64,
      Uint128,
      Float,
      Double,
      Pointer,
      Array,
      Class,

      Null
   };

   class Type { // base class for all types
   public:
      virtual ~Type() = default;
      _NODISCARD virtual TypeKind kind() const noexcept = 0;
      _NODISCARD virtual const std::string type() const = 0;

      _NODISCARD bool operator==(Type& other) {
         return kind() == other.kind();
      }
      operator std::string();
   };

   // base for all nodes in the AST
   class ExpressionAST {
   public:
      ExpressionAST(std::unique_ptr<Type> ty) { _Type = std::move(ty); }
      ExpressionAST() = default;
      virtual ~ExpressionAST() = default;
      virtual llvm::Value* codegen() = 0;
      virtual bool isLValue() { return true; }
      virtual void accept(ASTVisitor& v) = 0;

      _NODISCARD virtual std::shared_ptr<Type>& type() { return _Type; }

   protected:
      std::shared_ptr<Type> _Type;
   };

   // types declaration
   class Void : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Void;
      }
      _NODISCARD const std::string type() const override {
         return "Void";
      }
   };

   class Char : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Char;
      }
      _NODISCARD const std::string type() const override {
         return "Char";
      }
   };

   class Bool : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Bool;
      }
      _NODISCARD const std::string type() const override {
         return "Bool";
      }
   };

   class String : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::String;
      }
      _NODISCARD const std::string type() const override {
         return "String";
      }
   };

   class Int8 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Int8;
      }
      _NODISCARD const std::string type() const override {
         return "Int8";
      }
   };

   class Int16 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Int16;
      }
      _NODISCARD const std::string type() const override {
         return "Int16";
      }
   };

   class Int32 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Int32;
      }
      _NODISCARD const std::string type() const override {
         return "Int32";
      }
   };

   class Int64 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Int64;
      }
      _NODISCARD const std::string type() const override {
         return "Int64";
      }
   };

   class Int128 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Int128;
      }
      _NODISCARD const std::string type() const override {
         return "Int128";
      }
   };

   class Uint8 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Uint8;
      }
      _NODISCARD const std::string type() const override {
         return "Uint8";
      }
   };

   class Uint16 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Uint16;
      }
      _NODISCARD const std::string type() const override {
         return "Uint16";
      }
   };

   class Uint32 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Uint32;
      }
      _NODISCARD const std::string type() const override {
         return "Uint32";
      }
   };

   class Uint64 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Uint64;
      }
      _NODISCARD const std::string type() const override {
         return "Uint64";
      }
   };

   class Uint128 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Uint128;
      }
      _NODISCARD const std::string type() const override {
         return "Uint128";
      }
   };

   class Float : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Float;
      }
      _NODISCARD const std::string type() const override {
         return "Float";
      }
   };

   class Double : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Double;
      }
      _NODISCARD const std::string type() const override {
         return "Double";
      }
   };

   class PointerTy : public Type {
   public:
      PointerTy(std::shared_ptr<Type> pointee) : _pointee{pointee} {}
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Pointer;
      }
      _NODISCARD const std::string type() const override {
         return "Pointer";
      }
      _NODISCARD std::shared_ptr<Type>& pointee() { return _pointee; }

   private:
      std::shared_ptr<Type> _pointee;
   };

   class ArrayTy : public Type {
   public:
      ArrayTy(std::shared_ptr<Type> type, std::unique_ptr<ExpressionAST> size) : _arrayType{type}, _arraySize{std::move(size)} {}

      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Array;
      }
      _NODISCARD const std::string type() const override {
         return "Array";
      }
      _NODISCARD std::shared_ptr<Type>& arrayType() { return _arrayType; }
      _NODISCARD std::unique_ptr<ExpressionAST>& size() { return _arraySize; }

   private:
      std::shared_ptr<Type> _arrayType;
      std::unique_ptr<ExpressionAST> _arraySize;
   };

   class ClassTy : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Class;
      }
      _NODISCARD const std::string type() const override {
         return "Class";
      }
   };

   class NullTy : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Null;
      }
      _NODISCARD const std::string type() const override {
         return "NullTy";
      }
   };

   inline std::shared_ptr<Type> makeVoid() { return std::make_shared<Void>(); }
   inline std::shared_ptr<Type> makeBool() { return std::make_shared<Bool>(); }
   inline std::shared_ptr<Type> makeChar() { return std::make_shared<Char>(); }
   inline std::shared_ptr<Type> makeString() { return std::make_shared<String>(); }
   inline std::shared_ptr<Type> makeInt8() { return std::make_shared<Int8>(); }
   inline std::shared_ptr<Type> makeInt16() { return std::make_shared<Int16>(); }
   inline std::shared_ptr<Type> makeInt32() { return std::make_shared<Int32>(); }
   inline std::shared_ptr<Type> makeInt64() { return std::make_shared<Int64>(); }
   inline std::shared_ptr<Type> makeInt128() { return std::make_shared<Int128>(); }
   inline std::shared_ptr<Type> makeUint8() { return std::make_shared<Uint8>(); }
   inline std::shared_ptr<Type> makeUint16() { return std::make_shared<Uint16>(); }
   inline std::shared_ptr<Type> makeUint32() { return std::make_shared<Uint32>(); }
   inline std::shared_ptr<Type> makeUint64() { return std::make_shared<Uint64>(); }
   inline std::shared_ptr<Type> makeUint128() { return std::make_shared<Uint128>(); }
   inline std::shared_ptr<Type> makeFloat() { return std::make_shared<Float>(); }
   inline std::shared_ptr<Type> makeDouble() { return std::make_shared<Double>(); }
   inline std::shared_ptr<Type> makeClass() { return std::make_shared<ClassTy>(); }
   inline std::shared_ptr<Type> makePointer(std::shared_ptr<Type> pt) { return std::make_shared<PointerTy>(std::move(pt)); }
   inline std::shared_ptr<Type> makeArray(std::shared_ptr<Type> pt, std::unique_ptr<ExpressionAST> size) { return std::make_shared<ArrayTy>(std::move(pt), std::move(size)); }
   inline std::shared_ptr<Type> makeNullType() { return std::make_shared<NullTy>(); }

   // expression for numbers
   using Number = std::variant<int, int8_t, int16_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t, float, double>;
   class NumberExpressionAST : public ExpressionAST {
   public:
      NumberExpressionAST(Number value) : _value{value} {}
      llvm::Value* codegen() override;

      void setType(std::shared_ptr<Type> type) { _Type = type; }
      bool isLValue() override { return false; }
      void accept(ASTVisitor& v) override { v.visit(*this); }

   private:
      Number _value;
   };

   // expression for variables
   class VariableExpressionAST : public ExpressionAST {
   public:
      VariableExpressionAST(const std::string& name) : _name{name} {}
      llvm::Value* codegen() override;

      _NODISCARD const std::string& name() const { return _name; }
      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }

   private:
      std::string _name{};
   };

   class StringExpression : public ExpressionAST {
   public:
      StringExpression(const std::string& value) : _value{value} { _Type = makeString(); }
      llvm::Value* codegen() override;

      bool isLValue() override { return false; }
      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }

   private:
      std::string _value{};
   };

   class UnaryExpressionAST : public ExpressionAST {
   public:
      UnaryExpressionAST(const std::string& op, std::unique_ptr<ExpressionAST> operand)
          : _op{op}, _operand{std::move(operand)} {}
      llvm::Value* codegen() override;

      _NODISCARD const std::string& op() const { return _op; }
      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }

   private:
      std::string _op;
      std::unique_ptr<ExpressionAST> _operand;
   };

   // expression for binary operations
   class BinaryExpressionAST : public ExpressionAST {
   public:
      BinaryExpressionAST(const std::string& op, std::unique_ptr<ExpressionAST> LHS, std::unique_ptr<ExpressionAST> RHS)
          : _op{op}, _LHS{std::move(LHS)}, _RHS{std::move(RHS)} {}
      llvm::Value* codegen() override;
      void setType(std::shared_ptr<Type> type) { _Type = type; }
      void accept(ASTVisitor& v) override { v.visit(*this); }

   private:
      std::string _op;
      std::unique_ptr<ExpressionAST> _LHS;
      std::unique_ptr<ExpressionAST> _RHS;
   };

   class VariableDeclarationAST : public ExpressionAST {
   public:
      VariableDeclarationAST(std::shared_ptr<Type> type, const std::string& name, std::unique_ptr<ExpressionAST> init)
          : _name{name}, _init{std::move(init)} { _Type = type; }
      llvm::Value* codegen() override;

      _NODISCARD const std::string& name() const { return _name; }
      _NODISCARD std::unique_ptr<ExpressionAST>& initializer() { return _init; }
      void accept(ASTVisitor& v) override { v.visit(*this); }

   private:
      std::string _name;
      std::unique_ptr<ExpressionAST> _init;
   };

   // expression for function calls
   class CallExpressionAST : public ExpressionAST {
   public:
      CallExpressionAST(const std::string& callee, std::vector<std::unique_ptr<ExpressionAST>> args)
          : _callee{callee}, _args{std::move(args)} {}
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }

   private:
      std::string _callee;
      std::vector<std::unique_ptr<ExpressionAST>> _args;
   };

   class TypeOfStatementAST : public ExpressionAST {
   public:
      TypeOfStatementAST(const std::string& variable) : _variable{variable} { _Type = makeString(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }

   private:
      std::string _variable;
   };

   class NameOfStatementAST : public ExpressionAST {
   public:
      NameOfStatementAST(const std::string& variable) : _name{variable} { _Type = makeString(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }

   private:
      std::string _name;
   };

   class SizeOfStatementAST : public ExpressionAST {
   public:
      SizeOfStatementAST(const std::string& variable) : _variable{variable} {}
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }

   private:
      std::string _variable;
   };

   class __BuiltinFunctionAST : public ExpressionAST {
   public:
      __BuiltinFunctionAST(const std::string& function) : _function{function} { _Type = makeString(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }

   private:
      std::string _function;
   };
   class __BuiltinLineAST : public ExpressionAST {
   public:
      __BuiltinLineAST(size_t line) : _line{line} {}
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }

   private:
      size_t _line;
   };
   class __BuiltinColumnAST : public ExpressionAST {
   public:
      __BuiltinColumnAST(size_t column) : _column{column} {}
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }

   private:
      size_t _column;
   };
   class __BuiltinFileAST : public ExpressionAST {
   public:
      __BuiltinFileAST(const std::string& file) : _file{file} { _Type = makeString(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }

   private:
      std::string _file;
   };

   // cast expressions
   class StaticCastAST : public ExpressionAST {
   public:
      StaticCastAST(std::shared_ptr<Type> type, std::unique_ptr<ExpressionAST> value)
          : _value{std::move(value)} { _Type = type; }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }

   private:
      std::unique_ptr<ExpressionAST> _value;
   };
   class ConstCastAST : public ExpressionAST {
   public:
      ConstCastAST(std::shared_ptr<Type>& type, std::unique_ptr<ExpressionAST> value)
          : _value{std::move(value)} { _Type = type; }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }

   private:
      std::unique_ptr<ExpressionAST> _value;
   };

   // expression for if statements
   class IfStatementAST : public ExpressionAST {
   public:
      IfStatementAST(std::unique_ptr<ExpressionAST> condition, std::unique_ptr<ExpressionAST> thenBranch, std::unique_ptr<ExpressionAST> elseBranch = nullptr)
          : _condition{std::move(condition)}, _thenBranch{std::move(thenBranch)}, _elseBranch{std::move(elseBranch)} { _Type = makeNullType(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }

   private:
      std::unique_ptr<ExpressionAST> _condition;
      std::unique_ptr<ExpressionAST> _thenBranch;
      std::unique_ptr<ExpressionAST> _elseBranch;
   };

   class WhileStatementAST : public ExpressionAST {
   public:
      WhileStatementAST(std::unique_ptr<ExpressionAST> condition, std::unique_ptr<ExpressionAST> body)
          : _condition{std::move(condition)}, _body{std::move(body)} { _Type = makeNullType(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }

   private:
      std::unique_ptr<ExpressionAST> _condition;
      std::unique_ptr<ExpressionAST> _body;
   };
   class DoWhileStatementAST : public ExpressionAST {
   public:
      DoWhileStatementAST(std::unique_ptr<ExpressionAST> condition, std::unique_ptr<ExpressionAST> body)
          : _condition{std::move(condition)}, _body{std::move(body)} { _Type = makeNullType(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }

   private:
      std::unique_ptr<ExpressionAST> _condition;
      std::unique_ptr<ExpressionAST> _body;
   };
   class ForStatementAST : public ExpressionAST {
   public:
      ForStatementAST(std::unique_ptr<ExpressionAST> init, std::unique_ptr<ExpressionAST> condition, std::unique_ptr<ExpressionAST> increment, std::unique_ptr<ExpressionAST> body)
          : _init{std::move(init)}, _condition{std::move(condition)}, _increment{std::move(increment)}, _body{std::move(body)} { _Type = makeNullType(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }

   private:
      std::unique_ptr<ExpressionAST> _init;
      std::unique_ptr<ExpressionAST> _condition;
      std::unique_ptr<ExpressionAST> _increment;
      std::unique_ptr<ExpressionAST> _body;
   };

   class BlockStatementAST : public ExpressionAST {
   public:
      BlockStatementAST(std::vector<std::unique_ptr<ExpressionAST>> statements)
          : _statements{std::move(statements)} { _Type = makeNullType(); }
      llvm::Value* codegen() override;

      _NODISCARD std::vector<std::unique_ptr<ExpressionAST>>& statements() { return _statements; }
      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }

   private:
      std::vector<std::unique_ptr<ExpressionAST>> _statements;
   };

   class ReturnStatementAST : public ExpressionAST {
   public:
      ReturnStatementAST(std::unique_ptr<ExpressionAST> value) : _value{std::move(value)} {}
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }
      _NODISCARD std::unique_ptr<ExpressionAST>& value() { return _value; }

   private:
      std::unique_ptr<ExpressionAST> _value;
   };

   class ExitStatement : public ExpressionAST {
   public:
      ExitStatement(std::unique_ptr<ExpressionAST> value) : _value{std::move(value)} { _Type = makeInt32(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }
      _NODISCARD std::unique_ptr<ExpressionAST>& value() { return _value; }

   private:
      std::unique_ptr<ExpressionAST> _value;
   };

   //  prototype for function declarations
   class FunctionPrototypeAST {
   public:
      FunctionPrototypeAST(std::shared_ptr<Type> type, const std::string& name, std::vector<std::unique_ptr<ExpressionAST>> args)
          : _type{type}, _name{name}, _args{std::move(args)} {}
      virtual ~FunctionPrototypeAST() = default;
      llvm::Function* codegen();

      _NODISCARD const std::string& name() const { return _name; }
      _NODISCARD std::shared_ptr<Type>& type() { return _type; }
      _NODISCARD const std::vector<std::unique_ptr<ExpressionAST>>& args() const { return _args; }
      void accept(ASTVisitor& v) { v.visit(*this); }

   private:
      std::shared_ptr<Type> _type;
      std::string _name;
      std::vector<std::unique_ptr<ExpressionAST>> _args;
   };

   // expression for external function declarations
   class ExternStatementAST : public ExpressionAST {
   public:
      ExternStatementAST(std::unique_ptr<FunctionPrototypeAST> value) : _value{std::move(value)} {}
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }

   private:
      std::unique_ptr<FunctionPrototypeAST> _value;
   };

   // function definition itself
   class FunctionAST {
   public:
      FunctionAST(std::unique_ptr<FunctionPrototypeAST> prototype, std::unique_ptr<ExpressionAST> body)
          : _prototype{std::move(prototype)}, _body{std::move(body)} {}
      virtual ~FunctionAST() = default;
      llvm::Function* codegen();

      _NODISCARD const std::string& name() const { return _prototype->name(); }
      _NODISCARD std::shared_ptr<Type>& type() const { return _prototype->type(); }
      _NODISCARD std::unique_ptr<ExpressionAST>& body() { return _body; }
      void accept(ASTVisitor& v) { v.visit(*this); }

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
      ClassMethodAST(Visibility visibility, const std::string& name, std::unique_ptr<Type>& type, bool isStatic, std::unique_ptr<FunctionAST> method)
          : _visibility{visibility}, _name{name}, _type{type}, _isStatic{isStatic}, _method{std::move(method)} {}
      llvm::Value* codegen();
      void accept(ASTVisitor& v) { v.visit(*this); }

   private:
      Visibility _visibility;
      std::string _name;
      std::unique_ptr<Type>& _type;
      bool _isStatic;
      std::unique_ptr<FunctionAST> _method;
   };

   class ClassVariableAST {
   public:
      ClassVariableAST(Visibility visibility, const std::string& name, std::unique_ptr<Type>& type, bool isStatic, std::unique_ptr<ExpressionAST> variable)
          : _visibility{visibility}, _name{name}, _type{type}, _isStatic{isStatic}, _variable{std::move(variable)} {}
      llvm::Value* codegen();
      void accept(ASTVisitor& v) { v.visit(*this); }

   private:
      Visibility _visibility;
      std::string _name;
      std::unique_ptr<Type>& _type;
      bool _isStatic;
      std::unique_ptr<ExpressionAST> _variable;
   };

   class ClassConstructorAST {
   public:
      ClassConstructorAST(Visibility visibility, std::unique_ptr<FunctionAST> constructor)
          : _visibility{visibility}, _constructor{std::move(constructor)} {}
      llvm::Value* codegen();
      void accept(ASTVisitor& v) { v.visit(*this); }

   private:
      Visibility _visibility;
      std::unique_ptr<FunctionAST> _constructor;
   };
   class ClassDestructorAST {
   public:
      ClassDestructorAST(std::unique_ptr<FunctionAST> destructor)
          : _destructor{std::move(destructor)} {}
      llvm::Value* codegen();
      void accept(ASTVisitor& v) { v.visit(*this); }

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
      void accept(ASTVisitor& v) { v.visit(*this); }

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
      void accept(ASTVisitor& v) override { v.visit(*this); }

   private:
      std::string _name;
      std::vector<std::unique_ptr<ExpressionAST>> _variables;
      std::vector<std::unique_ptr<FunctionAST>> _functions;
      std::vector<std::unique_ptr<ClassAST>> _classes;
   };

   class ImportStatementAST : public ExpressionAST {
   public:
      ImportStatementAST(const std::string& module) : _module{module} { _Type = makeNullType(); }
      llvm::Value* codegen() override;
      void accept(ASTVisitor& v) override { v.visit(*this); }

      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }

   private:
      std::string _module;
   };
} // namespace tungsten

//  ========================================== implementation ==========================================

namespace tungsten {
   llvm::Type* LLVMType(std::shared_ptr<Type>& type) {
      if (type->type() == "Void")
         return Builder->getVoidTy();
      if (type->type() == "Bool")
         return Builder->getInt1Ty();
      if (type->type() == "Char")
         return Builder->getInt8Ty();
      if (type->type() == "String")
         return Builder->getInt8Ty()->getPointerTo();
      if (type->type() == "Int" || type->type() == "Int32" || type->type() == "Uint" || type->type() == "Uint32")
         return Builder->getInt32Ty();
      if (type->type() == "Int8" || type->type() == "Uint8")
         return Builder->getInt8Ty();
      if (type->type() == "Int16" || type->type() == "Uint16")
         return Builder->getInt16Ty();
      if (type->type() == "Int64" || type->type() == "Uint64")
         return Builder->getInt64Ty();
      if (type->type() == "Int128" || type->type() == "Uint128")
         return Builder->getInt128Ty();
      if (type->type() == "Float")
         return Builder->getFloatTy();
      if (type->type() == "Double")
         return Builder->getDoubleTy();
      return nullptr;
   }
}; // namespace tungsten

export namespace tungsten {
   Type::operator std::string() {
      std::string str{};
      Type* contained = this;
      while (kind() == TypeKind::Pointer || kind() == TypeKind::Array) {
         if (kind() == TypeKind::Pointer)
            str += "*";
         else
            str += "[]";
         if (auto tmp = dynamic_cast<ArrayTy*>(contained))
            contained = tmp->arrayType().get();
         else if (auto tmp = dynamic_cast<PointerTy*>(contained))
            contained = tmp->pointee().get();
      }
      return contained->type() + str;
   }

   llvm::Value* NumberExpressionAST::codegen() { // TODO: fix
      if (_Type->type() == "Int")
         return Builder->getInt32(std::get<uint64_t>(_value));
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
      //    if (_type == "Int" || _type == "Int32")
      //       return Builder->getInt32(std::get<uint64_t>(_value));
      //    if (_type == "Uint" || _type == "Uint32")
      //       return Builder->getInt32(std::get<uint>(_value));
      //    if (_type == "Int8" || _type == "Uint8")
      //       return Builder->getInt8(std::get<int8_t>(_value));
      //    if (_type == "Int16" || _type == "Uint16")
      //       return Builder->getInt16(std::get<int16_t>(_value));
      //    if (_type == "Int64" || _type == "Uint64")
      //       return Builder->getInt64(std::get<uint64_t>(_value));
      //    if (_type == "Int128" || _type == "Uint128")
      //       return Builder->getIntN(128, std::get<int64_t>(_value));
      //    if (_type == "Float")
      //       return llvm::ConstantFP::get(llvm::Type::getFloatTy(*TheContext), std::get<float>(_value));
      //    if (_type == "Double")
      //       return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*TheContext), std::get<double>(_value));

      //    return Builder->getInt64(std::get<uint64_t>(_value)); // fallback to uint64_t
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

   llvm::Value* UnaryExpressionAST::codegen() {
      llvm::Value* operandValue = _operand->codegen();
      if (!operandValue)
         return nullptr;

      llvm::Value* result;
      if (_op == "++" || _op == "--") {
         llvm::Value* loadedOperand = Builder->CreateLoad(VariableTypes[static_cast<VariableExpressionAST*>(_operand.get())->name()], operandValue, "loadedoperand");
         llvm::Type* operandType = loadedOperand->getType();
         if (_op == "++") {
            if (operandType->isIntegerTy())
               result = Builder->CreateAdd(loadedOperand, llvm::ConstantInt::get(operandType, 1), "increment");
            else if (operandType->isFloatingPointTy())
               result = Builder->CreateFAdd(loadedOperand, llvm::ConstantFP::get(operandType, 1.0), "increment");
            else
               return LogErrorV("unsupported type for increment operation");
            Builder->CreateStore(result, operandValue);
            return result;
         }

         if (operandType->isIntegerTy())
            result = Builder->CreateSub(loadedOperand, llvm::ConstantInt::get(operandType, 1), "decrement");
         else if (operandType->isFloatingPointTy())
            result = Builder->CreateFSub(loadedOperand, llvm::ConstantFP::get(operandType, 1.0), "decrement");
         else
            return LogErrorV("unsupported type for decrement operation");
         return Builder->CreateStore(result, operandValue);
      }

      llvm::Type* operandType = operandValue->getType(); // TODO: make it work with both L and R values
      if (_op == "!") {
         if (operandType->isIntegerTy())
            return Builder->CreateICmpEQ(operandValue, llvm::ConstantInt::get(operandType, 0), "nottmp");

         if (operandType->isFloatingPointTy())
            return Builder->CreateFCmpOEQ(operandValue, llvm::ConstantFP::get(operandType, 0.0), "nottmp");

         return LogErrorV("unsupported type for logical not operation");
      }

      if (operandType->isIntegerTy())
         return Builder->CreateNeg(operandValue, "negtmp");
      if (operandType->isFloatingPointTy())
         return Builder->CreateFNeg(operandValue, "negtmp");

      return LogErrorV("unsupported type for unary operation");
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
         if (!L->getType()->isPointerTy() || !_LHS->isLValue())
            return LogErrorV("left side of assignment must be a variable or assignable expression");

         llvm::Value* loadedL = L;
         if (_op != "=" && _op != "+=" && _op != "-=" && _op != "*=" && _op != "/=" && _op != "%=" && _op != "|=" && _op != "&=") {
            if (auto* varExpr = dynamic_cast<VariableExpressionAST*>(_LHS.get())) {
               llvm::Type* ltype = VariableTypes[varExpr->name()];
               loadedL = Builder->CreateLoad(ltype, L, "lval");
            }
         }

         if (R->getType()->isPointerTy() && !_RHS->isLValue()) {
            if (auto* varExpr = dynamic_cast<VariableExpressionAST*>(_RHS.get())) {
               llvm::Type* rtype = VariableTypes[varExpr->name()];
               R = Builder->CreateLoad(rtype, R, "rval");
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
            L = Builder->CreateLoad(ltype, L, "lval");
         }
      }
      if (R->getType()->isPointerTy() && !_RHS->isLValue()) {
         if (auto* varExpr = dynamic_cast<VariableExpressionAST*>(_RHS.get())) {
            llvm::Type* rtype = VariableTypes[varExpr->name()];
            R = Builder->CreateLoad(rtype, R, "rval");
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
      llvm::Type* type = LLVMType(_Type);

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
            return LogErrorV("no instance of function " + _callee + " with zero args");

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
   llvm::Value* SizeOfStatementAST::codegen() { // TODO: fix because of type rework
      // if (!NamedValues.contains(_variable) || !VariableTypes.contains(_variable)) {
      //    llvm::Type* type = LLVMType(_variable);
      //    if (type)
      //       return Builder->getInt64(TheModule->getDataLayout().getTypeAllocSize(type));
      //    return LogErrorV("unknown type or variable '" + _variable + "'");
      // }

      // uint64_t size = TheModule->getDataLayout().getTypeAllocSize(VariableTypes[_variable]);

      // return Builder->getInt64(size);
      return nullptr;
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
      llvm::Type* type = LLVMType(_Type);

      if (!_value)
         return nullptr;
      llvm::Value* castedValue = _value->codegen();
      if (castedValue == nullptr)
         return nullptr;

      if (castedValue->getType()->isPointerTy()) // didn't add pointers yet so this check is ok
         castedValue = Builder->CreateLoad(VariableTypes[static_cast<VariableExpressionAST*>(_value.get())->name()], castedValue, "lval");

      if (isSignedType(_Type->type())) {
         return Builder->CreateIntCast(castedValue, type, true, "staticCast");
      }
      if (isUnsignedType(_Type->type())) {
         return Builder->CreateIntCast(castedValue, type, false, "staticCast");
      }
      if (isFloatingPointType(_Type->type())) {
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
      if (_value == nullptr)
         return Builder->CreateRetVoid();

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
         paramTypes.push_back(type);
      }
      llvm::Type* returnType = LLVMType(_type);

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
         if (_prototype->type()->type() == "Void")
            Builder->CreateRetVoid();
         else if (_prototype->name() == "main")
            Builder->CreateRet(Builder->getInt32(0)); // returning 0 by default in main function
         else
            return LogErrorF("missing return statement in function: '" + name() + "'");
      }

      llvm::verifyFunction(*function, &llvm::outs());
      return function;
   }

   llvm::Value* NamespaceAST::codegen() {
      return nullptr;
   }
   llvm::Value* ImportStatementAST::codegen() {
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
