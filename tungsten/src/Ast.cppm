module;

#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>
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
   std::unordered_map<std::string, llvm::Value*> NamedValues{};
   std::unordered_map<std::string, llvm::Type*> VariableTypes{};
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
   llvm::Value* castToCommonType(llvm::Value* val, llvm::Type* targetType) {
      llvm::Type* srcType = val->getType();
      if (srcType == targetType) return val;

      if (srcType->isIntegerTy() && targetType->isFloatingPointTy())
         return Builder->CreateSIToFP(val, targetType, "sitofp");
      if (srcType->isFloatingPointTy() && targetType->isIntegerTy())
         return Builder->CreateFPToSI(val, targetType, "fptosi");
      if (srcType->isIntegerTy() && targetType->isIntegerTy())
         return Builder->CreateIntCast(val, targetType, true, "intcast");
      if (srcType->isFloatingPointTy() && targetType->isFloatingPointTy())
         return Builder->CreateFPCast(val, targetType, "fpcast");

      return val;
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
   void addCoreLibFunctions();
   void initLLVM(const std::string& moduleName, const std::string& fileName) {
      TheContext = std::make_unique<llvm::LLVMContext>();
      Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
      TheModule = std::make_unique<llvm::Module>(moduleName, *TheContext);
      TheModule->setSourceFileName(fileName);
      addCoreLibFunctions();
   }
   void dumpIR() {
      llvm::verifyModule(*TheModule, &llvm::outs());
      // #ifdef TUNGSTEN_DEBUG

      std::error_code EC;
      llvm::raw_fd_ostream outFile(TheModule->getModuleIdentifier() + ".ll", EC, llvm::sys::fs::OF_None);

      if (EC) {
         std::cerr << "error opening the file: " << EC.message() << std::endl;
      } else {
         TheModule->print(outFile, nullptr);
      }
      // #endif
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
      virtual void visit(class ExternFunctionStatementAST&) = 0;
      virtual void visit(class ExternVariableStatementAST&) = 0;
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

      ArgPack,

      Null
   };

   enum class ASTType {
      Expression,
      NumberExpression,
      VariableExpression,
      StringExpression,
      UnaryExpression,
      BinaryExpression,
      VariableDeclaration,
      CallExpression,
      TypeOfStatement,
      NameOfStatement,
      SizeOfStatement,
      BuiltinFunction,
      BuiltinLine,
      BuiltinColumn,
      BuiltinFile,
      StaticCast,
      ConstCast,
      IfStatement,
      WhileStatement,
      DoWhileStatement,
      ForStatement,
      BlockStatement,
      ReturnStatement,
      ExitStatement,
      ExternFunctionStatement,
      ExternVariableStatement,
      Namespace,
      ImportStatement,
      FunctionPrototype,
      Function,
      ClassMethod,
      ClassVariable,
      ClassConstructor,
      ClassDestructor,
      Class
   };

   class Type { // base class for all types
   public:
      virtual ~Type() = default;
      _NODISCARD virtual TypeKind kind() const noexcept = 0;
      _NODISCARD virtual const std::string string() const = 0;
      _NODISCARD virtual llvm::Type* llvmType() const = 0;
   };

   // base for all nodes in the AST
   class ExpressionAST {
   public:
      ExpressionAST(std::shared_ptr<Type> ty) : _Type{ty} {}
      ExpressionAST() = default;
      virtual ~ExpressionAST() = default;
      virtual llvm::Value* codegen() = 0;
      virtual bool isLValue() { return true; }
      virtual void accept(ASTVisitor& v) = 0;

      _NODISCARD virtual ASTType astType() const noexcept { return ASTType::Expression; }
      _NODISCARD virtual std::shared_ptr<Type>& type() { return _Type; }

   protected:
      std::shared_ptr<Type> _Type;
   };

   // types declaration
   class Void : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Void;
      }
      _NODISCARD const std::string string() const override {
         return "Void";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getVoidTy();
      }
   };

   class Char : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Char;
      }
      _NODISCARD const std::string string() const override {
         return "Char";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getInt8Ty();
      }
   };

   class Bool : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Bool;
      }
      _NODISCARD const std::string string() const override {
         return "Bool";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getInt1Ty();
      }
   };

   class String : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::String;
      }
      _NODISCARD const std::string string() const override {
         return "String";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getPtrTy();
      }
   };

   class Int8 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Int8;
      }
      _NODISCARD const std::string string() const override {
         return "Int8";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getInt8Ty();
      }
   };

   class Int16 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Int16;
      }
      _NODISCARD const std::string string() const override {
         return "Int16";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getInt16Ty();
      }
   };

   class Int32 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Int32;
      }
      _NODISCARD const std::string string() const override {
         return "Int32";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getInt32Ty();
      }
   };

   class Int64 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Int64;
      }
      _NODISCARD const std::string string() const override {
         return "Int64";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getInt64Ty();
      }
   };

   class Int128 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Int128;
      }
      _NODISCARD const std::string string() const override {
         return "Int128";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getInt128Ty();
      }
   };

   class Uint8 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Uint8;
      }
      _NODISCARD const std::string string() const override {
         return "Uint8";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getInt8Ty();
      }
   };

   class Uint16 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Uint16;
      }
      _NODISCARD const std::string string() const override {
         return "Uint16";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getInt16Ty();
      }
   };

   class Uint32 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Uint32;
      }
      _NODISCARD const std::string string() const override {
         return "Uint32";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getInt32Ty();
      }
   };

   class Uint64 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Uint64;
      }
      _NODISCARD const std::string string() const override {
         return "Uint64";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getInt64Ty();
      }
   };

   class Uint128 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Uint128;
      }
      _NODISCARD const std::string string() const override {
         return "Uint128";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getInt128Ty();
      }
   };

   class Float : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Float;
      }
      _NODISCARD const std::string string() const override {
         return "Float";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getFloatTy();
      }
   };

   class Double : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Double;
      }
      _NODISCARD const std::string string() const override {
         return "Double";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getDoubleTy();
      }
   };

   class PointerTy : public Type {
   public:
      PointerTy(std::shared_ptr<Type> pointee) : _pointee{pointee} {}
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Pointer;
      }
      _NODISCARD const std::string string() const override {
         return "Pointer";
      }
      _NODISCARD std::shared_ptr<Type>& pointee() { return _pointee; }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getPtrTy();
      }

   private:
      std::shared_ptr<Type> _pointee;
   };

   class ArrayTy : public Type {
   public:
      ArrayTy(std::shared_ptr<Type> type, std::unique_ptr<ExpressionAST> size) : _arrayType{type}, _arraySize{std::move(size)} {}

      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Array;
      }
      _NODISCARD const std::string string() const override {
         return "Array";
      }
      _NODISCARD std::shared_ptr<Type>& arrayType() { return _arrayType; }
      _NODISCARD std::unique_ptr<ExpressionAST>& size() { return _arraySize; }

      _NODISCARD llvm::Type* llvmType() const override {
         return nullptr; // llvm::ArrayType::get();
      }

   private:
      std::shared_ptr<Type> _arrayType;
      std::unique_ptr<ExpressionAST> _arraySize;
   };

   class ArgPackTy : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::ArgPack;
      }
      _NODISCARD const std::string string() const override {
         return "ArgPack";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return nullptr;
      }
   };

   class ClassTy : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Class;
      }
      _NODISCARD const std::string string() const override {
         return "Class";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return nullptr; // llvm::StructType::create(*TheContext, elements, "MyStruct");
      }
   };

   class NullTy : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Null;
      }
      _NODISCARD const std::string string() const override {
         return "NullTy";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return nullptr;
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
   inline std::shared_ptr<Type> makeArgPack() { return std::make_shared<ArgPackTy>(); }
   inline std::shared_ptr<Type> makeNullType() { return std::make_shared<NullTy>(); }

   std::string fullTypeString(const std::shared_ptr<Type>& ty) {
      if (!ty)
         return "";

      switch (ty->kind()) {
         case TypeKind::Pointer: {
            auto ptrTy = std::static_pointer_cast<PointerTy>(ty);
            return fullTypeString(ptrTy->pointee()) + "*";
         }
         case TypeKind::Array: {
            auto arrTy = std::static_pointer_cast<ArrayTy>(ty);
            return fullTypeString(arrTy->arrayType()) + "[]";
         }
         default:
            return ty->string();
      }
   }
   std::string baseType(const std::shared_ptr<Type>& ty) {
      switch (ty->kind()) {
         case TypeKind::Pointer: {
            auto ptrTy = std::static_pointer_cast<PointerTy>(ty);
            return baseType(ptrTy->pointee());
         }
         case TypeKind::Array: {
            auto arrTy = std::static_pointer_cast<ArrayTy>(ty);
            return baseType(arrTy->arrayType());
         }
         default:
            return ty->string();
      }
   }
   // expression for numbers
   using Number = std::variant<double, uint64_t>;
   class NumberExpressionAST : public ExpressionAST {
   public:
      NumberExpressionAST(Number value, std::shared_ptr<Type> type) : _value{value}, ExpressionAST{type} {}
      llvm::Value* codegen() override;

      void setType(std::shared_ptr<Type> type) { _Type = type; }
      bool isLValue() override { return false; }
      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::NumberExpression; }

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
      _NODISCARD ASTType astType() const noexcept override { return ASTType::VariableExpression; }

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
      _NODISCARD ASTType astType() const noexcept override { return ASTType::StringExpression; }
      _NODISCARD const std::string& value() const { return _value; }

   private:
      std::string _value{};
   };

   class UnaryExpressionAST : public ExpressionAST {
   public:
      UnaryExpressionAST(const std::string& op, std::unique_ptr<ExpressionAST> operand)
          : _op{op}, _operand{std::move(operand)} {}
      llvm::Value* codegen() override;

      _NODISCARD const std::string& op() const { return _op; }
      _NODISCARD std::unique_ptr<ExpressionAST>& operand() { return _operand; }
      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::UnaryExpression; }

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
      _NODISCARD ASTType astType() const noexcept override { return ASTType::BinaryExpression; }
      _NODISCARD const std::string& op() const { return _op; }
      _NODISCARD std::unique_ptr<ExpressionAST>& LHS() { return _LHS; }
      _NODISCARD std::unique_ptr<ExpressionAST>& RHS() { return _RHS; }
      bool isLValue() override { return false; }

   private:
      std::string _op;
      std::unique_ptr<ExpressionAST> _LHS;
      std::unique_ptr<ExpressionAST> _RHS;
   };

   class VariableDeclarationAST : public ExpressionAST {
   public:
      VariableDeclarationAST(std::shared_ptr<Type> type, const std::string& name, std::unique_ptr<ExpressionAST> init, bool isGlobal = false)
          : _name{name}, _init{std::move(init)}, _isGlobal{isGlobal}, ExpressionAST{type} {}
      llvm::Value* codegen() override;

      _NODISCARD const std::string& name() const { return _name; }
      _NODISCARD std::unique_ptr<ExpressionAST>& initializer() { return _init; }
      void accept(ASTVisitor& v) override { v.visit(*this); }

      _NODISCARD ASTType astType() const noexcept override { return ASTType::VariableDeclaration; }

   private:
      std::string _name;
      bool _isGlobal;
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
      _NODISCARD const std::string& callee() const { return _callee; }
      _NODISCARD std::vector<std::unique_ptr<ExpressionAST>>& args() { return _args; }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::CallExpression; }

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
      _NODISCARD ASTType astType() const noexcept override { return ASTType::TypeOfStatement; }

   private:
      std::string _variable;
   };

   class NameOfStatementAST : public ExpressionAST {
   public:
      NameOfStatementAST(const std::string& variable) : _name{variable} { _Type = makeString(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::NameOfStatement; }

   private:
      std::string _name;
   };

   class SizeOfStatementAST : public ExpressionAST {
   public:
      SizeOfStatementAST(const std::string& variable) : _variable{variable} {}
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::SizeOfStatement; }

   private:
      std::string _variable;
   };

   class __BuiltinFunctionAST : public ExpressionAST {
   public:
      __BuiltinFunctionAST(const std::string& function) : _function{function} { _Type = makeString(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::BuiltinFunction; }

   private:
      std::string _function;
   };
   class __BuiltinLineAST : public ExpressionAST {
   public:
      __BuiltinLineAST(size_t line) : _line{line}, ExpressionAST{makeInt64()} {}
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::BuiltinLine; }

   private:
      size_t _line;
   };
   class __BuiltinColumnAST : public ExpressionAST {
   public:
      __BuiltinColumnAST(size_t column) : _column{column}, ExpressionAST{makeInt64()} {}
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::BuiltinColumn; }

   private:
      size_t _column;
   };
   class __BuiltinFileAST : public ExpressionAST {
   public:
      __BuiltinFileAST(const std::string& file) : _file{file} { _Type = makeString(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::BuiltinFile; }

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
      _NODISCARD ASTType astType() const noexcept override { return ASTType::StaticCast; }

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
      _NODISCARD ASTType astType() const noexcept override { return ASTType::ConstCast; }

   private:
      std::unique_ptr<ExpressionAST> _value;
   };

   // class AliasAST : public ExpressionAST {
   // public:
   //    AliasAST(std::shared_ptr<Type> alias, std::shared_ptr<Type> type) : _alias{alias}, ExpressionAST{type} {}
   //
   // private:
   //    std::shared_ptr<Type> _alias;
   // };

   // expression for if statements
   class IfStatementAST : public ExpressionAST {
   public:
      IfStatementAST(std::unique_ptr<ExpressionAST> condition, std::unique_ptr<ExpressionAST> thenBranch, std::unique_ptr<ExpressionAST> elseBranch = nullptr)
          : _condition{std::move(condition)}, _thenBranch{std::move(thenBranch)}, _elseBranch{std::move(elseBranch)} { _Type = makeNullType(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::IfStatement; }
      _NODISCARD std::unique_ptr<ExpressionAST>& condition() { return _condition; }
      _NODISCARD std::unique_ptr<ExpressionAST>& thenBranch() { return _thenBranch; }
      _NODISCARD std::unique_ptr<ExpressionAST>& elseBranch() { return _elseBranch; }

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
      _NODISCARD ASTType astType() const noexcept override { return ASTType::WhileStatement; }
      _NODISCARD std::unique_ptr<ExpressionAST>& condition() { return _condition; }
      _NODISCARD std::unique_ptr<ExpressionAST>& body() { return _body; }

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
      _NODISCARD ASTType astType() const noexcept override { return ASTType::DoWhileStatement; }
      _NODISCARD std::unique_ptr<ExpressionAST>& condition() { return _condition; }
      _NODISCARD std::unique_ptr<ExpressionAST>& body() { return _body; }

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
      _NODISCARD ASTType astType() const noexcept override { return ASTType::ForStatement; }
      _NODISCARD std::unique_ptr<ExpressionAST>& init() { return _init; }
      _NODISCARD std::unique_ptr<ExpressionAST>& condition() { return _condition; }
      _NODISCARD std::unique_ptr<ExpressionAST>& increment() { return _increment; }
      _NODISCARD std::unique_ptr<ExpressionAST>& body() { return _body; }

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
      _NODISCARD ASTType astType() const noexcept override { return ASTType::BlockStatement; }

   private:
      std::vector<std::unique_ptr<ExpressionAST>> _statements;
   };

   class ReturnStatementAST : public ExpressionAST {
   public:
      ReturnStatementAST(std::unique_ptr<ExpressionAST> value, std::shared_ptr<Type> type) : _value{std::move(value)}, ExpressionAST{type} {}
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }
      _NODISCARD std::unique_ptr<ExpressionAST>& value() { return _value; }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::ReturnStatement; }

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
      _NODISCARD ASTType astType() const noexcept override { return ASTType::ExitStatement; }

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
      void setType(std::shared_ptr<Type> ty) { _type = ty; }
      _NODISCARD const std::vector<std::unique_ptr<ExpressionAST>>& args() const { return _args; }
      void accept(ASTVisitor& v) { v.visit(*this); }
      _NODISCARD ASTType astType() const noexcept { return ASTType::FunctionPrototype; }
      _NODISCARD std::string mangledName() const {
         std::string result = _name;
         for (auto& arg : _args) {
            result += "$" + fullTypeString(arg->type());
         }
         return result;
      }
      void setExternC(bool c) { _isExternC = c; }

   private:
      std::shared_ptr<Type> _type;
      std::string _name;
      bool _isExternC;
      std::vector<std::unique_ptr<ExpressionAST>> _args;
   };

   // expression for external function declarations
   class ExternFunctionStatementAST : public ExpressionAST {
   public:
      ExternFunctionStatementAST(std::unique_ptr<FunctionPrototypeAST> fun) : _fun{std::move(fun)} {}
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::ExternFunctionStatement; }

   private:
      std::unique_ptr<FunctionPrototypeAST> _fun;
   };
   class ExternVariableStatementAST : public ExpressionAST {
   public:
      ExternVariableStatementAST(std::unique_ptr<ExpressionAST> var) : _var{std::move(var)} {}
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::ExternVariableStatement; }

   private:
      std::unique_ptr<ExpressionAST> _var;
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
      _NODISCARD const std::vector<std::unique_ptr<ExpressionAST>>& args() const { return _prototype->args(); }
      _NODISCARD std::unique_ptr<FunctionPrototypeAST>& prototype() { return _prototype; }
      _NODISCARD std::unique_ptr<ExpressionAST>& body() { return _body; }
      _NODISCARD ASTType astType() const noexcept { return ASTType::Function; }
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
      _NODISCARD ASTType astType() const noexcept override { return ASTType::ImportStatement; }

   private:
      std::string _module;
   };
} // namespace tungsten

namespace tungsten {
   struct CoreFun {
      llvm::Type* type;
      std::string name;
      std::vector<llvm::Type*> args;
   };

   void addCoreLibFunctions() {
      const std::array coreFunctions = {
          CoreFun{llvm::Type::getDoubleTy(*TheContext), "shell", {llvm::Type::getInt8Ty(*TheContext)->getPointerTo()}},
          CoreFun{Builder->getVoidTy(), "input", {llvm::Type::getInt8Ty(*TheContext)->getPointerTo()}},
          CoreFun(Builder->getVoidTy(), "print", {llvm::Type::getInt8Ty(*TheContext)->getPointerTo()})};

      for (auto& fun : coreFunctions) {
         std::vector<llvm::Type*> paramTypes;
         for (const auto& arg : fun.args) {
            paramTypes.push_back(arg);
         }
         llvm::FunctionType* functionType;
         if (fun.name == "print" || fun.name == "input")
            functionType = llvm::FunctionType::get(fun.type, paramTypes, true);
         else
            functionType = llvm::FunctionType::get(fun.type, paramTypes, false);

         llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, fun.name, TheModule.get());
      }
   }
} // namespace tungsten

//  ========================================== implementation ==========================================
export namespace tungsten {
   llvm::Value* NumberExpressionAST::codegen() { // TODO: fix
      switch (_Type->kind()) {
         case TypeKind::Double:
         case TypeKind::Float:
            return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*TheContext), std::get<double>(_value));
         default:
            return Builder->getIntN(_Type->llvmType()->getIntegerBitWidth(), std::get<double>(_value)); // std::get<uint64_t>
      }
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
      if (_op == "&") {
         return operandValue;
      }
      if (operandType->isIntegerTy())
         return Builder->CreateNeg(operandValue, "negtmp");
      if (operandType->isFloatingPointTy())
         return Builder->CreateFNeg(operandValue, "negtmp");

      return LogErrorV("unsupported type for unary operation");
   }

   llvm::Value* BinaryExpressionAST::codegen() {
      llvm::Value* LHS = _LHS->codegen();
      llvm::Value* RHS = _RHS->codegen();

      llvm::Value* loadedL = LHS;
      if (_LHS->astType() == ASTType::VariableExpression)
         loadedL = Builder->CreateLoad(_LHS->type()->llvmType(), LHS, "lval");
      if (_RHS->astType() == ASTType::VariableExpression)
         RHS = Builder->CreateLoad(_RHS->type()->llvmType(), RHS, "rval");
      castToCommonType(RHS, _LHS->type()->llvmType());

      if (_op == "=" || _op == "+=" || _op == "-=" || _op == "*=" || _op == "/=" || _op == "%=" || _op == "|=" || _op == "&=" || _op == "^=" || _op == "<<=" || _op == ">>=") {
         if (_LHS->type()->kind() == TypeKind::String) {
            Builder->CreateStore(RHS, LHS);
            return RHS;
         }

         llvm::Value* result = nullptr;
         if (_op == "=") {
            result = RHS;
         } else if (_op == "+=") {
            result = Builder->CreateFAdd(loadedL, RHS);
         } else if (_op == "-=") {
            result = Builder->CreateFSub(loadedL, RHS);
         } else if (_op == "*=") {
            result = Builder->CreateFMul(loadedL, RHS);
         } else if (_op == "/=") {
            result = Builder->CreateFDiv(loadedL, RHS);
         } else if (_op == "%=") {
            result = Builder->CreateFRem(loadedL, RHS);
         } else if (_op == "|=") {
            result = Builder->CreateOr(loadedL, RHS);
         } else if (_op == "&=") {
            result = Builder->CreateAnd(loadedL, RHS);
         } else if (_op == "^=") {
            result = Builder->CreateXor(loadedL, RHS);
         } else if (_op == "<<=") {
            result = Builder->CreateShl(loadedL, RHS);
         } else if (_op == ">>=") {
            result = Builder->CreateLShr(loadedL, RHS);
         }
         return Builder->CreateStore(result, LHS);
      }

      if (_op == "&&") {
         LHS = Builder->CreateICmpNE(loadedL, Builder->getInt1(0), "lcond");
         RHS = Builder->CreateICmpNE(RHS, Builder->getInt1(0), "rcond");
         return Builder->CreateAnd(LHS, RHS);
      }
      if (_op == "||") {
         LHS = Builder->CreateICmpNE(loadedL, Builder->getInt1(0), "lcond");
         RHS = Builder->CreateICmpNE(RHS, Builder->getInt1(0), "rcond");
         return Builder->CreateOr(LHS, RHS);
      }

      if (_op == "+")
         return Builder->CreateFAdd(loadedL, RHS);
      if (_op == "-")
         return Builder->CreateFSub(loadedL, RHS);
      if (_op == "*")
         return Builder->CreateFMul(loadedL, RHS);
      if (_op == "/")
         return Builder->CreateFDiv(loadedL, RHS);
      if (_op == "%")
         return Builder->CreateFRem(loadedL, RHS);
      if (_op == "==")
         return Builder->CreateFCmpOEQ(loadedL, RHS);
      if (_op == "!=")
         return Builder->CreateFCmpONE(loadedL, RHS);
      if (_op == "<")
         return Builder->CreateFCmpOLT(loadedL, RHS);
      if (_op == "<=")
         return Builder->CreateFCmpOLE(loadedL, RHS);
      if (_op == ">")
         return Builder->CreateFCmpOGT(loadedL, RHS);
      if (_op == ">=")
         return Builder->CreateFCmpOGE(loadedL, RHS);
      if (_op == "^")
         return Builder->CreateXor(loadedL, RHS);
      if (_op == "|")
         return Builder->CreateOr(loadedL, RHS);
      if (_op == "&")
         return Builder->CreateAnd(loadedL, RHS);
      if (_op == "<<")
         return Builder->CreateShl(loadedL, RHS);
      if (_op == ">>")
         return Builder->CreateLShr(loadedL, RHS);
   }

   llvm::Value* VariableDeclarationAST::codegen() {
      llvm::Type* type = _Type->llvmType();
      if (_isGlobal) {
         return nullptr;
      }
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
      std::string name = _callee;
      for (auto& arg : _args) {
         name += "$" + fullTypeString(arg->type());
      }
      llvm::Function* callee = TheModule->getFunction(name);

      if (!callee)
         callee = TheModule->getFunction(_callee); // for extern functions ex. shell$String doesn't exist -> shell exists

      std::vector<llvm::Value*> args;
      std::string argsTypes;
      for (auto& arg : _args) {
         llvm::Value* argVal = arg->codegen();
         if (!argVal)
            return nullptr;

         if (arg->astType() == ASTType::VariableExpression)
            argVal = Builder->CreateLoad(arg->type()->llvmType(), argVal);

         args.push_back(argVal);
      }
      return Builder->CreateCall(callee, args);
   }

   llvm::Value* TypeOfStatementAST::codegen() {
      if (!NamedValues.contains(_variable) || !VariableTypes.contains(_variable))
         return LogErrorV("unknown variable '" + _variable + "'");

      llvm::Type* type = VariableTypes[_variable];
      return nullptr; // Builder->CreateGlobalStringPtr() TODO: fix
   }
   llvm::Value* NameOfStatementAST::codegen() {
      return Builder->CreateGlobalStringPtr(_name, "varname");
   }
   llvm::Value* SizeOfStatementAST::codegen() { // TODO: fix because of type rework
      // if (!NamedValues.contains(_variable) || !VariableTypes.contains(_variable)) {
      //    llvm::Type* type = _variable->type()->llvmType();
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
      return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*TheContext), _line); // return Builder->getInt64(_line);
   }
   llvm::Value* __BuiltinColumnAST::codegen() {
      return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*TheContext), _column); // Builder->getInt64(_column)
   }
   llvm::Value* __BuiltinFileAST::codegen() {
      return Builder->CreateGlobalStringPtr(_file, "strtmp");
   }

   llvm::Value* StaticCastAST::codegen() {
      llvm::Type* type = _Type->llvmType();

      if (!_value)
         return nullptr;
      llvm::Value* castedValue = _value->codegen();
      if (castedValue == nullptr)
         return nullptr;

      if (castedValue->getType()->isPointerTy()) // didn't add pointers yet so this check is ok
         castedValue = Builder->CreateLoad(VariableTypes[static_cast<VariableExpressionAST*>(_value.get())->name()], castedValue, "lval");

      if (isSignedType(_Type->string())) {
         return Builder->CreateIntCast(castedValue, type, true, "staticCast");
      }
      if (isUnsignedType(_Type->string())) {
         return Builder->CreateIntCast(castedValue, type, false, "staticCast");
      }
      if (isFloatingPointType(_Type->string())) {
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

   llvm::Value* ExternVariableStatementAST::codegen() {
      return nullptr;
   }
   llvm::Value* ExternFunctionStatementAST::codegen() {
      if (auto* existing = TheModule->getFunction(_fun->name()))
         return existing;

      std::vector<llvm::Type*> paramTypes;
      for (const auto& arg : _fun->args()) {
         paramTypes.push_back(arg->type()->llvmType());
      }

      llvm::FunctionType* functionType = llvm::FunctionType::get(_fun->type()->llvmType(), paramTypes, false);

      llvm::Function* function = llvm::Function::Create(
          functionType,
          llvm::Function::ExternalLinkage,
          _fun->name(),
          TheModule.get());

      return function;
   }

   llvm::Value* ReturnStatementAST::codegen() {
      if (_value == nullptr)
         return Builder->CreateRetVoid();

      llvm::Value* returnValue = _value->codegen();
      if (_value->astType() == ASTType::VariableExpression)
         returnValue = Builder->CreateLoad(_value->type()->llvmType(), returnValue, "rval");
      returnValue = castToCommonType(returnValue, _Type->llvmType());
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

      if (_value->astType() == ASTType::VariableExpression) {
         exitValue = Builder->CreateLoad(VariableTypes[static_cast<VariableExpressionAST*>(_value.get())->name()], exitValue, "lval");
         exitValue = Builder->CreateFPToSI(exitValue, Builder->getInt32Ty(), "cast");

      } else if (exitValue->getType() != llvm::Type::getInt32Ty(*TheContext)) {
         exitValue = Builder->CreateIntCast(exitValue, Builder->getInt32Ty(), true);
      }

      llvm::Value* call = Builder->CreateCall(exitFunc, exitValue);

      Builder->CreateUnreachable();

      return call;
   }

   llvm::Value* IfStatementAST::codegen() {
      llvm::Value* CondV = _condition->codegen();
      if (!CondV)
         return nullptr;

      CondV = Builder->CreateICmpNE(CondV, llvm::ConstantInt::get(CondV->getType(), 0), "if.cond");

      llvm::Function* function = Builder->GetInsertBlock()->getParent();

      llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(*TheContext, "if.then", function);
      llvm::BasicBlock* ElseBB = llvm::BasicBlock::Create(*TheContext, "if.else");
      llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(*TheContext, "if.end");

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

      llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*TheContext, "while.cond", function);
      llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*TheContext, "while.body");
      llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*TheContext, "while.end");

      Builder->CreateBr(condBB);

      // condition
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
      llvm::Function* function = Builder->GetInsertBlock()->getParent();

      llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*TheContext, "dowhile.body", function);
      llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*TheContext, "dowhile.cond");
      llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*TheContext, "dowhile.end");

      Builder->CreateBr(bodyBB);

      // body
      Builder->SetInsertPoint(bodyBB);
      if (_body)
         _body->codegen();
      Builder->CreateBr(condBB);

      // condition
      function->insert(function->end(), condBB);
      Builder->SetInsertPoint(condBB);
      llvm::Value* CondV = _condition->codegen();
      if (!CondV)
         return nullptr;
      CondV = Builder->CreateICmpNE(
          CondV,
          llvm::ConstantInt::get(CondV->getType(), 0),
          "dowhilecond");
      Builder->CreateCondBr(CondV, bodyBB, endBB);

      // end
      function->insert(function->end(), endBB);
      Builder->SetInsertPoint(endBB);

      return llvm::Constant::getNullValue(llvm::Type::getInt32Ty(*TheContext));
   }

   llvm::Value* ForStatementAST::codegen() {
      llvm::Function* function = Builder->GetInsertBlock()->getParent();

      llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*TheContext, "for.cond");
      llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*TheContext, "for.body");
      llvm::BasicBlock* incBB = llvm::BasicBlock::Create(*TheContext, "for.inc");
      llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*TheContext, "for.end");

      // init
      if (_init)
         _init->codegen();

      Builder->CreateBr(condBB);

      // condition
      function->insert(function->end(), condBB);
      Builder->SetInsertPoint(condBB);
      llvm::Value* CondV = _condition ? _condition->codegen() : nullptr;
      if (_condition) {
         if (!CondV) return nullptr;
         CondV = Builder->CreateICmpNE(
             CondV,
             llvm::ConstantInt::get(CondV->getType(), 0),
             "forcond");
         Builder->CreateCondBr(CondV, bodyBB, endBB);
      } else
         Builder->CreateBr(bodyBB);


      // body
      function->insert(function->end(), bodyBB);
      Builder->SetInsertPoint(bodyBB);
      if (_body)
         _body->codegen();
      Builder->CreateBr(incBB);

      // increment
      function->insert(function->end(), incBB);
      Builder->SetInsertPoint(incBB);
      if (_increment)
         _increment->codegen();
      Builder->CreateBr(condBB);

      // end
      function->insert(function->end(), endBB);
      Builder->SetInsertPoint(endBB);

      return llvm::Constant::getNullValue(llvm::Type::getInt32Ty(*TheContext));
   }

   llvm::Function* FunctionPrototypeAST::codegen() {
      std::string name;
      if (_name == "main" || _isExternC)
         name = _name;
      else
         name = mangledName();

      if (auto* existing = TheModule->getFunction(name))
         return existing;

      std::vector<llvm::Type*> paramTypes;
      bool isVarArgs = false;
      for (const auto& arg : _args) {
         if (arg->type()->kind() == TypeKind::ArgPack)
            isVarArgs = true;
         else
            paramTypes.push_back(arg->type()->llvmType());
      }

      llvm::FunctionType* functionType = llvm::FunctionType::get(_type->llvmType(), paramTypes, isVarArgs);

      llvm::Function* function = llvm::Function::Create(
          functionType,
          llvm::Function::ExternalLinkage,
          name,
          TheModule.get());

      return function;
   }

   llvm::Function* FunctionAST::codegen() {
      llvm::Function* function = TheModule->getFunction(_prototype->name() == "main" ? "main" : _prototype->mangledName());
      if (!function)
         function = _prototype->codegen();

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
         if (_prototype->type()->string() == "Void")
            Builder->CreateRetVoid();
         else if (_prototype->name() == "main")
            Builder->CreateRet(Builder->getInt32(0)); // returning 0 by default in main function
         else
            return LogErrorF("missing return statement in function: '" + name() + "'");
      }

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
