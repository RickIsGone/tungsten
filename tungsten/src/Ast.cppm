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
#include <llvm/Passes/PassBuilder.h>
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#ifndef _NODISCARD
#define _NODISCARD [[nodiscard]]
#endif

export module Tungsten.ast;
import Tungsten.compileOptions;
import Tungsten.utils;

using namespace std::literals;

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
      return type == "int"sv || type == "i8"sv || type == "i16"sv || type == "i32"sv || type == "i64"sv || type == "i128"sv;
   }
   bool isUnsignedType(const std::string& type) {
      return type == "uint"sv || type == "u8"sv || type == "u16"sv || type == "u32"sv || type == "u64"sv || type == "u128"sv;
   }
   bool isFloatingPointType(const std::string& type) {
      return type == "float"sv || type == "double"sv;
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
   llvm::Value* castValueToType(llvm::Value* val, llvm::Type* targetType) {
      if (!val || !targetType)
         return val;

      llvm::Type* srcType = val->getType();
      if (srcType == targetType)
         return val;

      if (auto* constant = llvm::dyn_cast<llvm::Constant>(val)) {
         if (srcType->isIntegerTy() && targetType->isFloatingPointTy())
            return llvm::ConstantExpr::getCast(llvm::Instruction::SIToFP, constant, targetType);
         if (srcType->isFloatingPointTy() && targetType->isIntegerTy())
            return llvm::ConstantExpr::getCast(llvm::Instruction::FPToSI, constant, targetType);
         if (srcType->isIntegerTy() && targetType->isIntegerTy()) {
            const auto srcBits = srcType->getIntegerBitWidth();
            const auto dstBits = targetType->getIntegerBitWidth();
            if (srcBits == dstBits)
               return constant;
            const auto op = dstBits > srcBits ? llvm::Instruction::SExt : llvm::Instruction::Trunc;
            return llvm::ConstantExpr::getCast(op, constant, targetType);
         }
         if (srcType->isFloatingPointTy() && targetType->isFloatingPointTy()) {
            const auto srcBits = srcType->getPrimitiveSizeInBits();
            const auto dstBits = targetType->getPrimitiveSizeInBits();
            if (srcBits == dstBits)
               return constant;
            const auto op = dstBits > srcBits ? llvm::Instruction::FPExt : llvm::Instruction::FPTrunc;
            return llvm::ConstantExpr::getCast(op, constant, targetType);
         }
         return val;
      }

      return castToCommonType(val, targetType);
   }
   llvm::Type* commonNumericType(llvm::Type* leftType, llvm::Type* rightType) {
      if (!leftType || !rightType)
         return leftType ? leftType : rightType;

      if (leftType->isFloatingPointTy() || rightType->isFloatingPointTy()) {
         if (leftType->isDoubleTy() || rightType->isDoubleTy())
            return llvm::Type::getDoubleTy(*TheContext);
         return llvm::Type::getFloatTy(*TheContext);
      }

      if (leftType->isIntegerTy() && rightType->isIntegerTy()) {
         unsigned width = std::max(leftType->getIntegerBitWidth(), rightType->getIntegerBitWidth());
         return llvm::Type::getIntNTy(*TheContext, width);
      }

      return leftType;
   }
   llvm::Value* createNumericArithmeticOp(std::string_view op, llvm::Value* left, llvm::Value* right) {
      if (!left || !right)
         return nullptr;

      llvm::Type* targetType = commonNumericType(left->getType(), right->getType());

      left = castToCommonType(left, targetType);
      right = castToCommonType(right, targetType);

      llvm::Type* ty = left->getType();
      if (ty->isFloatingPointTy()) {
         if (op == "+"sv) return Builder->CreateFAdd(left, right);
         if (op == "-"sv) return Builder->CreateFSub(left, right);
         if (op == "*"sv) return Builder->CreateFMul(left, right);
         if (op == "/"sv) return Builder->CreateFDiv(left, right);
         if (op == "%"sv) return Builder->CreateFRem(left, right);
      } else if (ty->isIntegerTy()) {
         if (op == "+"sv) return Builder->CreateAdd(left, right);
         if (op == "-"sv) return Builder->CreateSub(left, right);
         if (op == "*"sv) return Builder->CreateMul(left, right);
         if (op == "/"sv) return Builder->CreateSDiv(left, right);
         if (op == "%"sv) return Builder->CreateSRem(left, right);
      }

      return nullptr;
   }
   llvm::Value* createNumericComparisonOp(std::string_view op, llvm::Value* left, llvm::Value* right) {
      if (!left || !right)
         return nullptr;

      llvm::Type* targetType = commonNumericType(left->getType(), right->getType());

      left = castToCommonType(left, targetType);
      right = castToCommonType(right, targetType);

      llvm::Type* ty = left->getType();
      if (ty->isFloatingPointTy()) {
         if (op == "=="sv) return Builder->CreateFCmpOEQ(left, right);
         if (op == "!="sv) return Builder->CreateFCmpONE(left, right);
         if (op == "<"sv) return Builder->CreateFCmpOLT(left, right);
         if (op == "<="sv) return Builder->CreateFCmpOLE(left, right);
         if (op == ">"sv) return Builder->CreateFCmpOGT(left, right);
         if (op == ">="sv) return Builder->CreateFCmpOGE(left, right);
      } else if (ty->isIntegerTy()) {
         if (op == "=="sv) return Builder->CreateICmpEQ(left, right);
         if (op == "!="sv) return Builder->CreateICmpNE(left, right);
         if (op == "<"sv) return Builder->CreateICmpSLT(left, right);
         if (op == "<="sv) return Builder->CreateICmpSLE(left, right);
         if (op == ">"sv) return Builder->CreateICmpSGT(left, right);
         if (op == ">="sv) return Builder->CreateICmpSGE(left, right);
      }

      return nullptr;
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
   void dumpIR(const CompileOptions& CO) {
      llvm::verifyModule(*TheModule, &llvm::outs());

      // code optimization following llvm guide at https://llvm.org/docs/NewPassManager.html#status-of-the-new-and-legacy-pass-managers
      llvm::LoopAnalysisManager LAM{};
      llvm::FunctionAnalysisManager FAM{};
      llvm::CGSCCAnalysisManager CGAM{};
      llvm::ModuleAnalysisManager MAM{};

      llvm::PassBuilder PB;

      PB.registerModuleAnalyses(MAM);
      PB.registerCGSCCAnalyses(CGAM);
      PB.registerFunctionAnalyses(FAM);
      PB.registerLoopAnalyses(LAM);
      PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

      llvm::OptimizationLevel OL;
      switch (CO.optimizationLevel) {
         case OptimizationLevel::O0:
            OL = llvm::OptimizationLevel::O0;
            break;
         case OptimizationLevel::O1:
            OL = llvm::OptimizationLevel::O1;
            break;
         case OptimizationLevel::O2:
            OL = llvm::OptimizationLevel::O2;
            break;
         case OptimizationLevel::O3:
            OL = llvm::OptimizationLevel::O3;
            break;
         default:
            OL = llvm::OptimizationLevel::O0;
      }
      llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(OL);
      MPM.run(*TheModule, MAM);

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
      virtual void visit(class IndexAccessAST&) = 0;
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
      virtual void visit(class NullPtrAST&) = 0;
      virtual void visit(class IfStatementAST&) = 0;
      virtual void visit(class WhileStatementAST&) = 0;
      virtual void visit(class DoWhileStatementAST&) = 0;
      virtual void visit(class ForStatementAST&) = 0;
      virtual void visit(class BlockStatementAST&) = 0;
      virtual void visit(class ReturnStatementAST&) = 0;
      virtual void visit(class ExitStatement&) = 0;
      virtual void visit(class ExternVariableStatementAST&) = 0;
      virtual void visit(class ImportStatementAST&) = 0;
      virtual void visit(class NewStatementAST&) = 0;
      virtual void visit(class FreeStatementAST&) = 0;
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
      Reference,
      Array,
      Class,

      ArgPack,
      NullPtr,
      Null
   };

   enum class ASTType {
      Expression,
      NumberExpression,
      VariableExpression,
      IndexAccessExpression,
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
      NullPtr,
      IfStatement,
      WhileStatement,
      DoWhileStatement,
      ForStatement,
      BlockStatement,
      ReturnStatement,
      ExitStatement,
      ExternVariableStatement,
      Namespace,
      ImportStatement,
      NewStatement,
      FreeStatement,
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
      _NODISCARD virtual uint64_t size() const noexcept {
         llvm::Type* ty = llvmType();
         if (!ty)
            return 0;
         if (TheModule) {
            const llvm::DataLayout& DL = TheModule->getDataLayout();
            if (ty->isSized())
               return DL.getTypeAllocSize(ty);
         }
         uint64_t bits = ty->getPrimitiveSizeInBits();
         return bits > 0 ? bits / 8 : 0;
      }
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
      void setSource(size_t position, size_t length) {
         _sourcePosition = position;
         _sourceLength = length;
         _hasSource = true;
      }
      _NODISCARD bool hasSource() const noexcept { return _hasSource; }
      _NODISCARD size_t sourcePosition() const noexcept { return _sourcePosition; }
      _NODISCARD size_t sourceLength() const noexcept { return _sourceLength; }

   protected:
      std::shared_ptr<Type> _Type;
      size_t _sourcePosition{0};
      size_t _sourceLength{0};
      bool _hasSource{false};
   };

   // types declaration
   class Void : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Void;
      }
      _NODISCARD const std::string string() const override {
         return "void";
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
         return "char";
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
         return "bool";
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getInt1Ty();
      }
   };

   class Int8 : public Type {
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Int8;
      }
      _NODISCARD const std::string string() const override {
         return "i8";
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
         return "i16";
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
         return "i32";
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
         return "i64";
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
         return "i128";
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
         return "u8";
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
         return "u16";
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
         return "u32";
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
         return "u64";
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
         return "u128";
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
         return "float";
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
         return "double";
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
         return "pointer";
      }
      _NODISCARD std::shared_ptr<Type>& pointee() { return _pointee; }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getPtrTy();
      }

   private:
      std::shared_ptr<Type> _pointee;
   };

   class ReferenceTy : public Type {
   public:
      ReferenceTy(std::shared_ptr<Type> ref) : _ref{ref} {}
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Reference;
      }
      _NODISCARD const std::string string() const override {
         return "reference";
      }
      std::shared_ptr<Type>& reference() { return _ref; }
      _NODISCARD llvm::Type* llvmType() const override {
         return Builder->getPtrTy();
      }

   private:
      std::shared_ptr<Type> _ref;
   };

   class ArrayTy : public Type {
   public:
      ArrayTy(std::shared_ptr<Type> type, std::unique_ptr<ExpressionAST> size) : _arrayType{type}, _arraySize{std::move(size)} {}

      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Array;
      }
      _NODISCARD const std::string string() const override {
         return "array";
      }
      _NODISCARD std::shared_ptr<Type>& arrayType() { return _arrayType; }
      _NODISCARD std::unique_ptr<ExpressionAST>& sizeExpr() { return _arraySize; }

      _NODISCARD llvm::Type* llvmType() const override; // line 1880 cause of NumberExpressionAST not being declared yet

      void setSize(std::unique_ptr<ExpressionAST> size) {
         _arraySize = std::move(size);
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
         return Builder->getPtrTy();
      }
   };

   inline std::shared_ptr<Type> makeVoid() { return std::make_shared<Void>(); }
   inline std::shared_ptr<Type> makeBool() { return std::make_shared<Bool>(); }
   inline std::shared_ptr<Type> makeChar() { return std::make_shared<Char>(); }
   inline std::shared_ptr<Type> makeString() { return std::make_shared<PointerTy>(makeChar()); }
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
   inline std::shared_ptr<Type> makeReference(std::shared_ptr<Type> ref) { return std::make_shared<ReferenceTy>(std::move(ref)); }
   inline std::shared_ptr<Type> makeArray(std::shared_ptr<Type> pt, std::unique_ptr<ExpressionAST> size) { return std::make_shared<ArrayTy>(std::move(pt), std::move(size)); }
   inline std::shared_ptr<Type> makeArgPack() { return std::make_shared<ArgPackTy>(); }
   inline std::shared_ptr<Type> makeNullType() { return std::make_shared<NullTy>(); }

   std::string fullTypeString(const std::shared_ptr<Type>& ty);
   std::vector<uint64_t> getArrayDimensions(const std::shared_ptr<Type>& ty);

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
         case TypeKind::Reference: {
            auto refTy = std::static_pointer_cast<ReferenceTy>(ty);
            return baseType(refTy->reference());
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

      _NODISCARD Number value() const { return _value; }

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
      VariableDeclarationAST(std::shared_ptr<Type> type, const std::string& name, std::unique_ptr<ExpressionAST> init, bool isConst, bool isGlobal = false)
          : ExpressionAST{type}, _name{name}, _init{std::move(init)}, _isConst{isConst}, _isGlobal{isGlobal} {}
      llvm::Value* codegen() override;

      _NODISCARD const std::string& name() const { return _name; }
      _NODISCARD std::unique_ptr<ExpressionAST>& initializer() { return _init; }
      _NODISCARD bool isConst() const { return _isConst; }
      _NODISCARD bool isGlobal() const { return _isGlobal; }
      void accept(ASTVisitor& v) override { v.visit(*this); }

      _NODISCARD ASTType astType() const noexcept override { return ASTType::VariableDeclaration; }

      void setType(std::shared_ptr<Type> type) { _Type = type; }

   private:
      std::string _name;
      std::unique_ptr<ExpressionAST> _init;
      bool _isConst;
      bool _isGlobal;
   };

   // expression for index access array[index]
   class IndexAccessAST : public ExpressionAST {
   public:
      IndexAccessAST(std::unique_ptr<ExpressionAST> array, std::unique_ptr<ExpressionAST> index)
          : _array{std::move(array)}, _index{std::move(index)} {}
      llvm::Value* codegen() override;

      _NODISCARD std::unique_ptr<ExpressionAST>& array() { return _array; }
      _NODISCARD std::unique_ptr<ExpressionAST>& index() { return _index; }
      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::IndexAccessExpression; }

   private:
      std::unique_ptr<ExpressionAST> _array;
      std::unique_ptr<ExpressionAST> _index;
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
      TypeOfStatementAST(std::unique_ptr<ExpressionAST> statement) : _statement{std::move(statement)} { _Type = makeString(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::TypeOfStatement; }

      _NODISCARD const std::unique_ptr<ExpressionAST>& statement() const { return _statement; }

   private:
      std::unique_ptr<ExpressionAST> _statement;
   };

   class NameOfStatementAST : public ExpressionAST {
   public:
      NameOfStatementAST(std::unique_ptr<ExpressionAST> statement) : _statement{std::move(statement)} { _Type = makeString(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::NameOfStatement; }

      _NODISCARD const std::unique_ptr<ExpressionAST>& statement() const { return _statement; }

   private:
      std::unique_ptr<ExpressionAST> _statement;
   };

   class SizeOfStatementAST : public ExpressionAST {
   public:
      SizeOfStatementAST(std::unique_ptr<ExpressionAST> statement)
          : _argument{std::move(statement)} {}
      SizeOfStatementAST(std::shared_ptr<Type> targetType)
          : _argument{std::move(targetType)} {}
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::SizeOfStatement; }

      _NODISCARD ExpressionAST* statement() {
         if (!std::holds_alternative<std::unique_ptr<ExpressionAST>>(_argument))
            return nullptr;
         return std::get<std::unique_ptr<ExpressionAST>>(_argument).get();
      }
      _NODISCARD const ExpressionAST* statement() const {
         if (!std::holds_alternative<std::unique_ptr<ExpressionAST>>(_argument))
            return nullptr;
         return std::get<std::unique_ptr<ExpressionAST>>(_argument).get();
      }
      _NODISCARD bool hasExplicitType() const {
         return std::holds_alternative<std::shared_ptr<Type>>(_argument) && static_cast<bool>(std::get<std::shared_ptr<Type>>(_argument));
      }
      _NODISCARD std::shared_ptr<Type> explicitType() const {
         if (!std::holds_alternative<std::shared_ptr<Type>>(_argument))
            return nullptr;
         return std::get<std::shared_ptr<Type>>(_argument);
      }

   private:
      std::variant<std::unique_ptr<ExpressionAST>, std::shared_ptr<Type>> _argument;
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
      _NODISCARD const std::unique_ptr<ExpressionAST>& value() const { return _value; }

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
      _NODISCARD const std::unique_ptr<ExpressionAST>& value() const { return _value; }

   private:
      std::unique_ptr<ExpressionAST> _value;
   };
   class NullPtrAST : public ExpressionAST {
   public:
      NullPtrAST() : ExpressionAST{makeNullType()} {}
      llvm::Value* codegen() override;

      _NODISCARD ASTType astType() const noexcept override { return ASTType::NullPtr; }
      void accept(ASTVisitor& v) override { v.visit(*this); }
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
         if (_args.empty())
            return _name + "$";

         std::string result = _name;
         std::string args;
         for (const auto& arg : _args) {
            result += "$" + fullTypeString(arg->type());
            args += fullTypeString(arg->type());
            if (arg != _args.back())
               args += ", ";
         }
         utils::debugLog("Mangled name for function fun {}({}) -> {}: {}", _name, args, fullTypeString(_type), result);
         return result;
      }
      void setExternC(bool c) { _isExternC = c; }

   private:
      std::shared_ptr<Type> _type;
      std::string _name;
      bool _isExternC{false};
      std::vector<std::unique_ptr<ExpressionAST>> _args;
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

   class NewStatementAST : public ExpressionAST {
   public:
      NewStatementAST(std::shared_ptr<Type> type) : ExpressionAST{type} {}
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::NewStatement; }
      void setType(std::shared_ptr<Type> type) { _Type = type; }
   };

   class FreeStatementAST : public ExpressionAST {
   public:
      FreeStatementAST(std::unique_ptr<ExpressionAST> variable) : _variable{std::move(variable)} { _Type = makeNullType(); }
      llvm::Value* codegen() override;
      void accept(ASTVisitor& v) override { v.visit(*this); }

      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::FreeStatement; }
      _NODISCARD std::unique_ptr<ExpressionAST>& variable() { return _variable; }

   private:
      std::unique_ptr<ExpressionAST> _variable{};
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
          CoreFun{llvm::Type::getDoubleTy(*TheContext), "shell", {Builder->getPtrTy()}},
          CoreFun{Builder->getVoidTy(), "input", {Builder->getPtrTy()}},
          CoreFun(Builder->getVoidTy(), "print", {Builder->getPtrTy()})};

      for (auto& fun : coreFunctions) {
         std::vector<llvm::Type*> paramTypes;
         for (const auto& arg : fun.args) {
            paramTypes.push_back(arg);
         }
         llvm::FunctionType* functionType;
         if (fun.name == "print"sv || fun.name == "input"sv)
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
      const double floatValue = std::holds_alternative<double>(_value) ? std::get<double>(_value) : static_cast<double>(std::get<uint64_t>(_value));
      const uint64_t intValue = std::holds_alternative<uint64_t>(_value) ? std::get<uint64_t>(_value) : static_cast<uint64_t>(std::get<double>(_value));

      switch (_Type->kind()) {
         case TypeKind::Double:
            return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*TheContext), floatValue);
         case TypeKind::Float:
            return llvm::ConstantFP::get(llvm::Type::getFloatTy(*TheContext), floatValue);
         case TypeKind::Bool:
            return llvm::ConstantInt::getBool(*TheContext, intValue != 0);
         default:
            return Builder->getIntN(_Type->llvmType()->getIntegerBitWidth(), intValue);
      }
   }

   llvm::Value* VariableExpressionAST::codegen() {
      llvm::Value* V = NamedValues[_name];
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
      if (_op == "++"sv || _op == "--"sv) {
         llvm::Value* targetAddr = operandValue;
         llvm::Type* loadType = _operand->type()->llvmType();
         if (_operand->type()->kind() == TypeKind::Reference)
            loadType = static_cast<ReferenceTy*>(_operand->type().get())->reference()->llvmType();
         else if (_operand->astType() == ASTType::VariableExpression)
            loadType = VariableTypes[static_cast<VariableExpressionAST*>(_operand.get())->name()];

         if (_operand->astType() == ASTType::UnaryExpression && static_cast<UnaryExpressionAST*>(_operand.get())->op() == "*"sv) {
            auto& derefOperand = static_cast<UnaryExpressionAST*>(_operand.get())->operand();
            if (derefOperand && (derefOperand->astType() == ASTType::VariableExpression || derefOperand->type()->kind() == TypeKind::Reference)) {
               llvm::Type* pointerValueType = derefOperand->type()->llvmType();
               targetAddr = Builder->CreateLoad(pointerValueType, targetAddr, "deref.addr");
            }
         }

         llvm::Value* loadedOperand = Builder->CreateLoad(loadType, targetAddr, "loadedoperand");
         llvm::Type* operandType = loadedOperand->getType();
         if (_op == "++"sv) {
            if (operandType->isIntegerTy())
               result = Builder->CreateAdd(loadedOperand, llvm::ConstantInt::get(operandType, 1), "increment");
            else if (operandType->isFloatingPointTy())
               result = Builder->CreateFAdd(loadedOperand, llvm::ConstantFP::get(operandType, 1.0), "increment");
            else
               return LogErrorV("unsupported type for increment operation");
            Builder->CreateStore(result, targetAddr);
            return result;
         }

         if (operandType->isIntegerTy())
            result = Builder->CreateSub(loadedOperand, llvm::ConstantInt::get(operandType, 1), "decrement");
         else if (operandType->isFloatingPointTy())
            result = Builder->CreateFSub(loadedOperand, llvm::ConstantFP::get(operandType, 1.0), "decrement");
         else
            return LogErrorV("unsupported type for decrement operation");
         return Builder->CreateStore(result, targetAddr);
      }

      llvm::Type* operandType = operandValue->getType(); // TODO: make it work with both L and R values
      if (_op == "!"sv) {
         if (operandType->isIntegerTy())
            return Builder->CreateICmpEQ(operandValue, llvm::ConstantInt::get(operandType, 0), "nottmp");

         if (operandType->isFloatingPointTy())
            return Builder->CreateFCmpOEQ(operandValue, llvm::ConstantFP::get(operandType, 0.0), "nottmp");

         return LogErrorV("unsupported type for logical not operation");
      }
      if (_op == "&"sv || _op == "*"sv)
         return operandValue;

      // _op == '-'
      if (operandType->isIntegerTy())
         return Builder->CreateNeg(operandValue, "negtmp");
      if (operandType->isFloatingPointTy())
         return Builder->CreateFNeg(operandValue, "negtmp");

      return LogErrorV("unsupported type for unary operation");
   }

   llvm::Value* loadIfPointer(llvm::Value* val, llvm::Type* ty, const std::string& name) {
      if (val->getType()->isPointerTy())
         return Builder->CreateLoad(ty, val, name);
      return val;
   }
   llvm::Value* loadIfPointer(llvm::Value* val, llvm::Type* ty) {
      if (val->getType()->isPointerTy())
         return Builder->CreateLoad(ty, val);
      return val;
   }
   llvm::Value* loadDereferenceAddressIfUnaryStar(ExpressionAST* expr, llvm::Value* exprValue, const std::string& name) {
      if (!expr || expr->astType() != ASTType::UnaryExpression)
         return exprValue;

      auto* unary = static_cast<UnaryExpressionAST*>(expr);
      if (unary->op() != "*"sv)
         return exprValue;

      auto& derefOperand = unary->operand();
      if (!derefOperand)
         return nullptr;

      llvm::Type* pointerValueType = derefOperand->type()->llvmType();
      return Builder->CreateLoad(pointerValueType, exprValue, name);
   }
   llvm::Type* valueTypeForCodegen(const std::shared_ptr<Type>& ty) {
      if (!ty)
         return nullptr;
      if (ty->kind() == TypeKind::Reference)
         return static_cast<ReferenceTy*>(ty.get())->reference()->llvmType();
      return ty->llvmType();
   }
   llvm::Type* expressionValueTypeForCodegen(ExpressionAST* expr) {
      if (!expr || !expr->type())
         return nullptr;

      if (expr->astType() == ASTType::VariableExpression) {
         const auto& varName = static_cast<VariableExpressionAST*>(expr)->name();
         if (VariableTypes.contains(varName))
            return VariableTypes[varName];
      }

      return valueTypeForCodegen(expr->type());
   }
   bool tryBuildNameOfExpression(ExpressionAST* expr, std::string& outName) {
      if (!expr)
         return false;

      switch (expr->astType()) {
         case ASTType::VariableExpression:
            outName = static_cast<VariableExpressionAST*>(expr)->name();
            return true;
         case ASTType::NumberExpression: {
            auto value = static_cast<NumberExpressionAST*>(expr)->value();
            if (std::holds_alternative<uint64_t>(value)) {
               outName = std::to_string(std::get<uint64_t>(value));
               return true;
            }
            outName = std::to_string(std::get<double>(value));
            return true;
         }
         case ASTType::StringExpression:
            outName = "\"" + static_cast<StringExpression*>(expr)->value() + "\"";
            return true;
         case ASTType::NullPtr:
            outName = "nullptr";
            return true;
         case ASTType::CallExpression: {
            auto* call = static_cast<CallExpressionAST*>(expr);
            if (call->callee().empty())
               return false;

            outName = call->callee() + "(";
            auto& args = call->args();
            for (size_t i = 0; i < args.size(); ++i) {
               std::string argName;
               if (!tryBuildNameOfExpression(args[i].get(), argName))
                  return false;
               if (i != 0)
                  outName += ",";
               outName += argName;
            }
            outName += ")";
            return true;
         }
         case ASTType::IndexAccessExpression: {
            auto* index = static_cast<IndexAccessAST*>(expr);
            std::string baseName;
            if (!tryBuildNameOfExpression(index->array().get(), baseName))
               return false;
            std::string indexName;
            if (!tryBuildNameOfExpression(index->index().get(), indexName))
               return false;
            outName = baseName + "[" + indexName + "]";
            return true;
         }
         case ASTType::UnaryExpression: {
            auto* unary = static_cast<UnaryExpressionAST*>(expr);
            std::string operandName;
            if (!tryBuildNameOfExpression(unary->operand().get(), operandName))
               return false;
            outName = unary->op() + operandName;
            return true;
         }
         case ASTType::BinaryExpression: {
            auto* binary = static_cast<BinaryExpressionAST*>(expr);
            std::string lhsName;
            std::string rhsName;
            if (!tryBuildNameOfExpression(binary->LHS().get(), lhsName) || !tryBuildNameOfExpression(binary->RHS().get(), rhsName))
               return false;
            outName = lhsName + binary->op() + rhsName;
            return true;
         }
         case ASTType::StaticCast:
            return tryBuildNameOfExpression(static_cast<StaticCastAST*>(expr)->value().get(), outName);
         case ASTType::ConstCast:
            return tryBuildNameOfExpression(static_cast<ConstCastAST*>(expr)->value().get(), outName);
         default:
            return false;
      }
   }

   llvm::Value* BinaryExpressionAST::codegen() {
      llvm::Value* LHS = _LHS->codegen();
      llvm::Value* RHS = _RHS->codegen();

      llvm::Type* lhsValueType = expressionValueTypeForCodegen(_LHS.get());
      llvm::Type* rhsValueType = expressionValueTypeForCodegen(_RHS.get());

      if (_RHS->astType() == ASTType::UnaryExpression && static_cast<UnaryExpressionAST*>(_RHS.get())->op() == "*"sv) {
         RHS = loadDereferenceAddressIfUnaryStar(_RHS.get(), RHS, "derefr.addr");
         if (!RHS)
            return LogErrorV("invalid dereference operand");
         RHS = Builder->CreateLoad(rhsValueType, RHS, "derefr");
      }

      const bool isAssignmentOp = (_op == "="sv || _op == "+="sv || _op == "-="sv || _op == "*="sv || _op == "/="sv || _op == "%="sv || _op == "|="sv || _op == "&="sv || _op == "^="sv || _op == "<<="sv || _op == ">>="sv);

      if (isAssignmentOp) {
         llvm::Value* targetAddr = LHS;
         if (_LHS->astType() == ASTType::UnaryExpression && static_cast<UnaryExpressionAST*>(_LHS.get())->op() == "*"sv) {
            auto& derefOperand = static_cast<UnaryExpressionAST*>(_LHS.get())->operand();
            if (!derefOperand)
               return LogErrorV("invalid dereference target");

            llvm::Type* pointerValueType = derefOperand->type()->llvmType();
            targetAddr = Builder->CreateLoad(pointerValueType, LHS, "deref.addr");
         }

         if (_LHS->type()->kind() == TypeKind::Pointer && _LHS->astType() != ASTType::UnaryExpression) {
            Builder->CreateStore(RHS, LHS);
            return RHS;
         }

         if (_LHS->astType() == ASTType::IndexAccessExpression) {
            auto* indexExpr = static_cast<IndexAccessAST*>(_LHS.get());
            llvm::Value* arrayPtr = indexExpr->array()->codegen();
            llvm::Value* indexVal = indexExpr->index()->codegen();

            if (!arrayPtr || !indexVal)
               return nullptr;

            if (indexVal->getType()->isPointerTy())
               indexVal = Builder->CreateLoad(expressionValueTypeForCodegen(indexExpr->index().get()), indexVal, "idx.load");

            if (indexVal->getType()->isFloatingPointTy())
               indexVal = Builder->CreateFPToUI(indexVal, Builder->getInt64Ty(), "idx.fptoui");
            else if (!indexVal->getType()->isIntegerTy())
               return LogErrorV("array index must be an integer value");
            else if (!indexVal->getType()->isIntegerTy(64))
               indexVal = Builder->CreateIntCast(indexVal, Builder->getInt64Ty(), false, "idx.cast");

            llvm::Type* baseTy = indexExpr->array()->type()->llvmType();
            if (!baseTy)
               return LogErrorV("cannot index expression with unknown LLVM type");

            if (baseTy->isArrayTy()) {
               llvm::Value* zero = Builder->getInt64(0);
               targetAddr = Builder->CreateGEP(baseTy, arrayPtr, {zero, indexVal}, "elementptr");
            } else if (indexExpr->array()->type()->kind() == TypeKind::Pointer) {
               auto* ptrTy = static_cast<PointerTy*>(indexExpr->array()->type().get());

               if (indexExpr->array()->astType() == ASTType::VariableExpression && arrayPtr->getType()->isPointerTy() && !llvm::isa<llvm::Argument>(arrayPtr))
                  arrayPtr = Builder->CreateLoad(arrayPtr->getType(), arrayPtr, "arrptr.load");

               llvm::Type* gepType = ptrTy->pointee()->llvmType();
               targetAddr = Builder->CreateGEP(gepType, arrayPtr, indexVal, "elementptr");
            } else {
               return LogErrorV("cannot index non-array, non-pointer value");
            }
         }

         llvm::Value* loadedL = nullptr;
         llvm::Value* loadedR = nullptr;

         if (_op == "="sv) {
            loadedR = RHS;
            if (_RHS->astType() != ASTType::IndexAccessExpression && loadedR->getType()->isPointerTy())
               loadedR = Builder->CreateLoad(rhsValueType, loadedR, "rval");
            loadedR = castToCommonType(loadedR, lhsValueType);
         } else {
            loadedL = Builder->CreateLoad(lhsValueType, targetAddr, "lval");

            loadedR = loadIfPointer(RHS, rhsValueType, "rval");
            if (_RHS->astType() == ASTType::IndexAccessExpression &&
                _RHS->type()->kind() == TypeKind::Pointer)
               loadedR = RHS;

            loadedR = castToCommonType(loadedR, lhsValueType);
         }

         llvm::Value* result = nullptr;
         if (_op == "="sv) result = loadedR;
         else if (_op == "+="sv)
            result = createNumericArithmeticOp("+"sv, loadedL, loadedR);
         else if (_op == "-="sv)
            result = createNumericArithmeticOp("-"sv, loadedL, loadedR);
         else if (_op == "*="sv)
            result = createNumericArithmeticOp("*"sv, loadedL, loadedR);
         else if (_op == "/="sv)
            result = createNumericArithmeticOp("/"sv, loadedL, loadedR);
         else if (_op == "%="sv)
            result = createNumericArithmeticOp("%"sv, loadedL, loadedR);
         else if (_op == "|="sv)
            result = Builder->CreateOr(loadedL, loadedR);
         else if (_op == "&="sv)
            result = Builder->CreateAnd(loadedL, loadedR);
         else if (_op == "^="sv)
            result = Builder->CreateXor(loadedL, loadedR);
         else if (_op == "<<="sv)
            result = Builder->CreateShl(loadedL, loadedR);
         else if (_op == ">>="sv)
            result = Builder->CreateLShr(loadedL, loadedR);

         if (!result)
            return LogErrorV("unsupported types for compound assignment operator '" + _op + "'");

         Builder->CreateStore(result, targetAddr);
         return result;
      }

      llvm::Value* loadedL = loadIfPointer(LHS, lhsValueType, "lval");
      llvm::Value* loadedR = loadIfPointer(RHS, rhsValueType, "rval");

      if (_LHS->astType() == ASTType::UnaryExpression && static_cast<UnaryExpressionAST*>(_LHS.get())->op() == "*"sv) {
         llvm::Value* derefAddr = loadDereferenceAddressIfUnaryStar(_LHS.get(), LHS, "lval.deref.addr");
         if (!derefAddr)
            return LogErrorV("invalid dereference operand");
         loadedL = Builder->CreateLoad(lhsValueType, derefAddr, "lval.deref");
      }

      if (_RHS->astType() == ASTType::IndexAccessExpression &&
          _RHS->type()->kind() == TypeKind::Pointer)
         loadedR = RHS;

      loadedR = castToCommonType(loadedR, lhsValueType);

      if (_op == "&&"sv) {
         LHS = Builder->CreateICmpNE(loadedL, Builder->getInt1(0), "lcond");
         RHS = Builder->CreateICmpNE(loadedR, Builder->getInt1(0), "rcond");
         return Builder->CreateAnd(LHS, RHS);
      }
      if (_op == "||"sv) {
         LHS = Builder->CreateICmpNE(loadedL, Builder->getInt1(0), "lcond");
         RHS = Builder->CreateICmpNE(loadedR, Builder->getInt1(0), "rcond");
         return Builder->CreateOr(LHS, RHS);
      }

      if (_op == "+"sv)
         return createNumericArithmeticOp("+"sv, loadedL, loadedR);
      if (_op == "-"sv)
         return createNumericArithmeticOp("-"sv, loadedL, loadedR);
      if (_op == "*"sv)
         return createNumericArithmeticOp("*"sv, loadedL, loadedR);
      if (_op == "/"sv)
         return createNumericArithmeticOp("/"sv, loadedL, loadedR);
      if (_op == "%"sv)
         return createNumericArithmeticOp("%"sv, loadedL, loadedR);
      if (_op == "=="sv)
         return createNumericComparisonOp("=="sv, loadedL, loadedR);
      if (_op == "!="sv)
         return createNumericComparisonOp("!="sv, loadedL, loadedR);
      if (_op == "<"sv)
         return createNumericComparisonOp("<"sv, loadedL, loadedR);
      if (_op == "<="sv)
         return createNumericComparisonOp("<="sv, loadedL, loadedR);
      if (_op == ">"sv)
         return createNumericComparisonOp(">"sv, loadedL, loadedR);
      if (_op == ">="sv)
         return createNumericComparisonOp(">="sv, loadedL, loadedR);
      if (_op == "^"sv)
         return Builder->CreateXor(loadedL, loadedR);
      if (_op == "|"sv)
         return Builder->CreateOr(loadedL, loadedR);
      if (_op == "&"sv)
         return Builder->CreateAnd(loadedL, loadedR);
      if (_op == "<<"sv)
         return Builder->CreateShl(loadedL, loadedR);
      if (_op == ">>"sv)
         return Builder->CreateLShr(loadedL, loadedR);

      return LogErrorV("error during binop codegen with operator '" + _op + "'");
   }


   llvm::Value* VariableDeclarationAST::codegen() {
      if (_Type->kind() == TypeKind::Reference) {
         if (!_init && _isGlobal)
            return LogErrorV("global reference requires an initializer");

         llvm::Value* initVal = _init->codegen();
         if (!initVal)
            return nullptr;

         if (_init->astType() == ASTType::UnaryExpression && static_cast<UnaryExpressionAST*>(_init.get())->op() == "*"sv) {
            initVal = loadDereferenceAddressIfUnaryStar(_init.get(), initVal, "ref.bind");
            if (!initVal)
               return LogErrorV("invalid dereference initializer");
         }

         if (!initVal->getType()->isPointerTy())
            return LogErrorV("reference initializer must be an lvalue/address expression");

         auto* refType = static_cast<ReferenceTy*>(_Type.get());
         NamedValues[_name] = initVal;
         VariableTypes[_name] = refType->reference()->llvmType();
         return initVal;
      }

      llvm::Type* type = _Type->llvmType();
      llvm::Value* initVal = nullptr;
      bool isNullPtrArray = false;

      if (_init) {
         initVal = _init->codegen();
         if (!initVal)
            return nullptr;

         if (_init->astType() == ASTType::NullPtr && type->isArrayTy()) {
            isNullPtrArray = true;
         } else {
            if (_init->astType() == ASTType::UnaryExpression && static_cast<UnaryExpressionAST*>(_init.get())->op() == "*"sv) {
               initVal = loadDereferenceAddressIfUnaryStar(_init.get(), initVal, "init.deref.addr");
               if (!initVal)
                  return LogErrorV("invalid dereference initializer");
               initVal = Builder->CreateLoad(type, initVal, "derefl");
            }

            if (initVal->getType()->isPointerTy() && !type->isPointerTy()) {
               llvm::Type* initValueType = expressionValueTypeForCodegen(_init.get());
               if (!initValueType)
                  initValueType = type;
               initVal = Builder->CreateLoad(initValueType, initVal, "init.load");
            }

            initVal = castValueToType(initVal, type);
            if (initVal->getType() != type)
               return LogErrorV("variable initializer type does not match variable type");
         }
      }

      if (_isGlobal) {
         llvm::Constant* initConstant = llvm::Constant::getNullValue(type);

         if (isNullPtrArray) {
            initConstant = llvm::Constant::getNullValue(type);
         } else if (initVal) {
            if (auto* constInit = llvm::dyn_cast<llvm::Constant>(initVal))
               initConstant = constInit;
            else
               return LogErrorV("global variable initializer must be a constant expression");
         } else if (_Type->kind() == TypeKind::Pointer) {
            initConstant = llvm::ConstantPointerNull::get(Builder->getPtrTy());
         }

         auto* globalVar = new llvm::GlobalVariable(
             *TheModule,
             type,
             _isConst,
             llvm::GlobalValue::ExternalLinkage,
             initConstant,
             _name);

         NamedValues[_name] = globalVar;
         VariableTypes[_name] = type;
         return globalVar;
      }

      llvm::Function* function = Builder->GetInsertBlock()->getParent();
      llvm::IRBuilder tmpBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());
      llvm::AllocaInst* allocInstance = tmpBuilder.CreateAlloca(type, nullptr, _name);

      if (isNullPtrArray || initVal) {
         llvm::Value* storeVal = isNullPtrArray ? llvm::Constant::getNullValue(type) : initVal;
         Builder->CreateStore(storeVal, allocInstance);
      } else if (_Type->kind() == TypeKind::Pointer) {
         Builder->CreateStore(llvm::ConstantPointerNull::get(Builder->getPtrTy()), allocInstance);
      }

      NamedValues[_name] = allocInstance;
      VariableTypes[_name] = type;

      return allocInstance;
   }

   llvm::Value* IndexAccessAST::codegen() {
      llvm::Value* arrayPtr = _array->codegen();
      llvm::Value* indexVal = _index->codegen();

      if (!arrayPtr || !indexVal)
         return nullptr;

      if (indexVal->getType()->isPointerTy())
         indexVal = Builder->CreateLoad(_index->type()->llvmType(), indexVal, "idx.load");

      if (indexVal->getType()->isFloatingPointTy())
         indexVal = Builder->CreateFPToUI(indexVal, Builder->getInt64Ty(), "idx.fptoui");
      else if (!indexVal->getType()->isIntegerTy())
         return LogErrorV("array index must be an integer value");
      else if (!indexVal->getType()->isIntegerTy(64))
         indexVal = Builder->CreateIntCast(indexVal, Builder->getInt64Ty(), false, "idx.cast");

      llvm::Type* baseTy = _array->type()->llvmType();
      if (!baseTy)
         return LogErrorV("cannot index expression with unknown LLVM type");

      if (baseTy->isArrayTy()) {
         llvm::Value* zero = Builder->getInt64(0);
         return Builder->CreateGEP(baseTy, arrayPtr, {zero, indexVal}, "elementptr");
      }

      if (_array->type()->kind() == TypeKind::Pointer) {
         auto* ptrTy = static_cast<PointerTy*>(_array->type().get());

         if (_array->astType() == ASTType::VariableExpression && arrayPtr->getType()->isPointerTy() && !llvm::isa<llvm::Argument>(arrayPtr))
            arrayPtr = Builder->CreateLoad(arrayPtr->getType(), arrayPtr, "arrptr.load");

         llvm::Type* gepType = ptrTy->pointee()->llvmType();
         llvm::Value* elementPtr = Builder->CreateGEP(gepType, arrayPtr, indexVal, "elementptr");

         if (ptrTy->pointee()->kind() == TypeKind::Pointer)
            elementPtr = Builder->CreateLoad(gepType, elementPtr, "element");

         return elementPtr;
      }

      return LogErrorV("cannot index non-array, non-pointer value");
   }

   llvm::Value* CallExpressionAST::codegen() {
      std::string name = _callee + "$";
      for (auto& arg : _args) {
         if (arg != _args.front())
            name += "$";
         if (arg->astType() == ASTType::UnaryExpression && static_cast<UnaryExpressionAST*>(arg.get())->op() == "&"sv)
            name += fullTypeString(static_cast<UnaryExpressionAST*>(arg.get())->operand()->type()) + "&";
         else
            name += fullTypeString(arg->type());
      }
      utils::debugLog("looking for function with mangled name: {}", name);
      llvm::Function* callee = TheModule->getFunction(name);

      if (!callee)
         callee = TheModule->getFunction(_callee); // for extern functions ex. shell$String doesn't exist -> shell exists

      std::vector<llvm::Value*> args;
      std::string argsTypes;
      for (size_t i = 0; i < _args.size(); ++i) {
         auto& arg = _args[i];
         llvm::Value* argVal = arg->codegen();
         if (!argVal)
            return nullptr;

         if (arg->astType() == ASTType::UnaryExpression && static_cast<UnaryExpressionAST*>(arg.get())->op() == "*"sv) {
            auto& derefOperand = static_cast<UnaryExpressionAST*>(arg.get())->operand();
            if (!derefOperand)
               return LogErrorV("invalid dereference argument");

            llvm::Type* pointerValueType = derefOperand->type()->llvmType();
            argVal = Builder->CreateLoad(pointerValueType, argVal, "arg.deref.addr");
         }

         if (arg->astType() == ASTType::VariableExpression &&
             argVal->getType()->isPointerTy() &&
             arg->type()->kind() != TypeKind::Array) {
            llvm::Type* loadedType = expressionValueTypeForCodegen(arg.get());
            if (loadedType)
               argVal = Builder->CreateLoad(loadedType, argVal, "arg.varload");
         }

         bool indexAccessAlreadyValue = false;
         if (arg->astType() == ASTType::IndexAccessExpression) {
            llvm::Type* loadedType = expressionValueTypeForCodegen(arg.get());
            if (!loadedType && arg->type())
               loadedType = arg->type()->llvmType();

            if (loadedType && loadedType->isPointerTy())
               indexAccessAlreadyValue = true;

            if (loadedType && argVal->getType()->isPointerTy() &&
                arg->type()->kind() != TypeKind::Pointer &&
                arg->type()->kind() != TypeKind::Reference &&
                !indexAccessAlreadyValue)
               argVal = Builder->CreateLoad(loadedType, argVal, "arg.idxload");
         }

         llvm::Type* expectedParamType = nullptr;
         if (callee && i < callee->arg_size())
            expectedParamType = callee->getFunctionType()->getParamType(static_cast<unsigned>(i));

         if (argVal->getType()->isPointerTy() && expectedParamType && !expectedParamType->isPointerTy() && !indexAccessAlreadyValue) {
            argVal = Builder->CreateLoad(expressionValueTypeForCodegen(arg.get()), argVal, "arg.load");
         } else if (argVal->getType()->isPointerTy() && !expectedParamType &&
                    arg->type()->kind() != TypeKind::Pointer &&
                    arg->type()->kind() != TypeKind::Reference &&
                    arg->type()->kind() != TypeKind::Array &&
                    !indexAccessAlreadyValue) {
            argVal = Builder->CreateLoad(expressionValueTypeForCodegen(arg.get()), argVal, "arg.load");
         }

         if (expectedParamType && !expectedParamType->isPointerTy())
            argVal = castToCommonType(argVal, expectedParamType);

         args.push_back(argVal);
      }
      return Builder->CreateCall(callee, args);
   }

   llvm::Value* TypeOfStatementAST::codegen() {
      return Builder->CreateGlobalStringPtr(fullTypeString(_statement->type()));
   }
   llvm::Value* NameOfStatementAST::codegen() {
      std::string name;
      if (!tryBuildNameOfExpression(_statement.get(), name))
         return LogErrorV("cannot use 'nameof' on expression without a stable name");
      return Builder->CreateGlobalStringPtr(name);
   }
   llvm::Value* SizeOfStatementAST::codegen() {
      if (hasExplicitType())
         return Builder->getInt64(explicitType()->size());
      return Builder->getInt64(statement()->type()->size());
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
      const auto& fromType = _value->type();
      const auto& toType = _Type;
      llvm::Value* castedValue = _value->codegen();
      if (castedValue == nullptr)
         return nullptr;

      if (castedValue->getType()->isPointerTy()) {
         const bool isAddressLikeExpr =
             _value->astType() == ASTType::VariableExpression ||
             _value->astType() == ASTType::IndexAccessExpression ||
             (_value->astType() == ASTType::UnaryExpression && static_cast<UnaryExpressionAST*>(_value.get())->op() == "*"sv);

         if (isAddressLikeExpr) {
            llvm::Type* sourceValueType = expressionValueTypeForCodegen(_value.get());
            castedValue = Builder->CreateLoad(sourceValueType, castedValue, "lval");
         }
      }

      const bool sameType = fullTypeString(fromType) == fullTypeString(toType);
      const bool numericCast = isSignedType(fromType->string()) || isUnsignedType(fromType->string()) || isFloatingPointType(fromType->string());
      const bool numericTarget = isSignedType(toType->string()) || isUnsignedType(toType->string()) || isFloatingPointType(toType->string());
      const bool pointerCast = fromType->kind() == TypeKind::Pointer && toType->kind() == TypeKind::Pointer;
      const bool pointerIntegerCast =
          (fromType->kind() == TypeKind::Pointer && (isSignedType(toType->string()) || isUnsignedType(toType->string()))) ||
          (toType->kind() == TypeKind::Pointer && (isSignedType(fromType->string()) || isUnsignedType(fromType->string())));

      if (!sameType && !(numericCast && numericTarget) && !pointerCast && !pointerIntegerCast)
         return LogErrorV("cannot cast from type '" + fullTypeString(fromType) + "' to '" + fullTypeString(toType) + "'");

      if (pointerCast) {
         if (!castedValue->getType()->isPointerTy())
            return LogErrorV("internal error: expected pointer value during pointer cast");
         return Builder->CreatePointerCast(castedValue, type, "staticCast");
      }

      if (toType->kind() == TypeKind::Pointer) {
         if (castedValue->getType()->isPointerTy())
            return Builder->CreatePointerCast(castedValue, type, "staticCast");
         if (castedValue->getType()->isIntegerTy())
            return Builder->CreateIntToPtr(castedValue, type, "staticCast");
         return LogErrorV("unsupported cast to pointer type");
      }

      if (fromType->kind() == TypeKind::Pointer) {
         if (type->isIntegerTy())
            return Builder->CreatePtrToInt(castedValue, type, "staticCast");
         return LogErrorV("unsupported cast from pointer type");
      }

      if (sameType)
         return castedValue;

      if (isSignedType(_Type->string())) {
         if (castedValue->getType()->isFloatingPointTy())
            return Builder->CreateFPToSI(castedValue, type, "staticCast");
         return Builder->CreateIntCast(castedValue, type, true, "staticCast");
      }
      if (isUnsignedType(_Type->string())) {
         if (castedValue->getType()->isFloatingPointTy())
            return Builder->CreateFPToUI(castedValue, type, "staticCast");
         return Builder->CreateIntCast(castedValue, type, false, "staticCast");
      }
      if (isFloatingPointType(_Type->string())) {
         if (castedValue->getType()->isIntegerTy()) {
            if (isUnsignedType(fromType->string()))
               return Builder->CreateUIToFP(castedValue, type, "staticCast");
            return Builder->CreateSIToFP(castedValue, type, "staticCast");
         }
         return Builder->CreateFPCast(castedValue, type, "staticCast");
      }

      return nullptr;
   }
   llvm::Value* ConstCastAST::codegen() {
      return nullptr;
   }
   llvm::Value* NullPtrAST::codegen() {
      return llvm::Constant::getNullValue(Builder->getPtrTy());
   }

   llvm::Value* BlockStatementAST::codegen() {
      llvm::Value* last = llvm::Constant::getNullValue(llvm::Type::getInt8Ty(*TheContext)); // to avoid empty functions from getting recognized as wrong
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

   llvm::Value* ReturnStatementAST::codegen() {
      if (_value == nullptr)
         return Builder->CreateRetVoid();

      const bool returnsReference = _Type->kind() == TypeKind::Reference;
      llvm::Value* returnValue = _value->codegen();

      if (!returnsReference && _value->astType() == ASTType::VariableExpression)
         returnValue = loadIfPointer(returnValue, expressionValueTypeForCodegen(_value.get()), "rval");

      if (!returnsReference && _value->astType() == ASTType::UnaryExpression && static_cast<UnaryExpressionAST*>(_value.get())->op() == "*"sv) {
         returnValue = loadDereferenceAddressIfUnaryStar(_value.get(), returnValue, "ret.deref.addr");
         if (!returnValue)
            return LogErrorV("invalid dereference return value");
         returnValue = Builder->CreateLoad(valueTypeForCodegen(_value->type()), returnValue, "derefl");
      }

      if (returnsReference) {
         if (!returnValue->getType()->isPointerTy())
            return LogErrorV("reference return requires an lvalue/address expression");
         return Builder->CreateRet(returnValue);
      }

      returnValue = castToCommonType(returnValue, valueTypeForCodegen(_Type));
      return Builder->CreateRet(returnValue);
   }
   llvm::Value* ExitStatement::codegen() {
      llvm::Value* exitValue = _value->codegen();
      if (_value->astType() == ASTType::UnaryExpression && static_cast<UnaryExpressionAST*>(_value.get())->op() == "*"sv) {
         exitValue = loadDereferenceAddressIfUnaryStar(_value.get(), exitValue, "exit.deref.addr");
         if (!exitValue)
            return LogErrorV("invalid dereference exit value");
         exitValue = Builder->CreateLoad(_value->type()->llvmType(), exitValue, "derefl");
      }

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
         exitValue = loadIfPointer(exitValue, VariableTypes[static_cast<VariableExpressionAST*>(_value.get())->name()], "lval");
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
      if (_name == "main"sv || _isExternC)
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
      llvm::Function* function = TheModule->getFunction(_prototype->name() == "main"sv ? "main" : _prototype->mangledName());
      if (!function)
         function = _prototype->codegen();

      llvm::BasicBlock* BB = llvm::BasicBlock::Create(*TheContext, "entry", function);
      Builder->SetInsertPoint(BB);
      llvm::IRBuilder tmpBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());

      size_t idx = 0;
      for (auto& arg : function->args()) {
         auto* varDecl = static_cast<VariableDeclarationAST*>(_prototype->args()[idx].get());
         if (varDecl->type()->kind() == TypeKind::Reference) {
            NamedValues[varDecl->name()] = &arg;
            VariableTypes[varDecl->name()] = static_cast<ReferenceTy*>(varDecl->type().get())->reference()->llvmType();
         } else {
            llvm::AllocaInst* paramAddr = tmpBuilder.CreateAlloca(arg.getType(), nullptr, varDecl->name());
            tmpBuilder.CreateStore(&arg, paramAddr);
            NamedValues[varDecl->name()] = paramAddr;
            VariableTypes[varDecl->name()] = arg.getType();
         }
         ++idx;
      }

      if (!_body || !_body->codegen()) {
         function->eraseFromParent();
         // return LogErrorF("error in function: '" + name() + "'");
         return nullptr;
      }

      if (Builder->GetInsertBlock()->getTerminator() == nullptr) {
         if (_prototype->type()->string() == "void"sv)
            Builder->CreateRetVoid();
         else if (_prototype->name() == "main"sv)
            Builder->CreateRet(Builder->getInt32(0)); // returning 0 by default in main function
         else
            return LogErrorF("missing return statement in function: '" + name() + "'");
      }

      return function;
   }

   llvm::Value* ImportStatementAST::codegen() {
      return nullptr;
   }

   llvm::Value* NewStatementAST::codegen() {
      llvm::Function* mallocFun = TheModule->getFunction("malloc");
      if (!mallocFun) {
         llvm::FunctionType* mallocType = llvm::FunctionType::get(
             Builder->getPtrTy(),
             {Builder->getInt64Ty()},
             false);
         mallocFun = llvm::Function::Create(mallocType, llvm::Function::ExternalLinkage, "malloc", TheModule.get());
      }

      auto size = Builder->getInt64(static_cast<PointerTy*>(_Type.get())->pointee()->size());

      return Builder->CreateCall(mallocFun, {size});
   }
   llvm::Value* FreeStatementAST::codegen() {
      llvm::Function* freeFun = TheModule->getFunction("free");
      if (!freeFun) {
         llvm::FunctionType* freeType = llvm::FunctionType::get(
             Builder->getVoidTy(),
             {Builder->getPtrTy()},
             false);
         freeFun = llvm::Function::Create(freeType, llvm::Function::ExternalLinkage, "free", TheModule.get());
      }

      if (!_variable || !_variable->type())
         return LogErrorV("free statement expects a valid expression");

      auto ptr = _variable->codegen();
      if (!ptr)
         return nullptr;

      llvm::Value* pointerToFree = ptr;
      llvm::Value* nullStoreTarget = nullptr;

      if (_variable->astType() == ASTType::UnaryExpression && static_cast<UnaryExpressionAST*>(_variable.get())->op() == "*"sv) {
         nullStoreTarget = ptr;
         pointerToFree = loadDereferenceAddressIfUnaryStar(_variable.get(), ptr, "free.deref.addr");
         if (!pointerToFree)
            return LogErrorV("invalid dereference free operand");
      } else if (_variable->astType() == ASTType::VariableExpression || _variable->astType() == ASTType::IndexAccessExpression) {
         nullStoreTarget = ptr;
         llvm::Type* pointeeType = expressionValueTypeForCodegen(_variable.get());
         if (!pointeeType)
            pointeeType = _variable->type()->llvmType();
         if (pointeeType && ptr->getType()->isPointerTy())
            pointerToFree = Builder->CreateLoad(pointeeType, ptr, "free.ptr");
      }

      auto call = Builder->CreateCall(freeFun, {pointerToFree});

      // Ensure later loads observe null in the same lvalue used by `free`.
      if (nullStoreTarget && nullStoreTarget->getType()->isPointerTy())
         Builder->CreateStore(llvm::ConstantPointerNull::get(Builder->getPtrTy()), nullStoreTarget);

      return call;
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

   llvm::Type* ArrayTy::llvmType() const {
      if (!_arraySize || _arraySize->astType() != ASTType::NumberExpression)
         return nullptr;

      auto* num = static_cast<NumberExpressionAST*>(_arraySize.get());
      const Number rawSize = num->value();
      uint64_t size = std::holds_alternative<double>(rawSize)
          ? static_cast<uint64_t>(std::get<double>(rawSize))
          : std::get<uint64_t>(rawSize);
      return llvm::ArrayType::get(_arrayType->llvmType(), size);
   }

   std::vector<uint64_t> getArrayDimensions(const std::shared_ptr<Type>& ty) {
      std::vector<uint64_t> dimensions;
      auto current = ty;

      while (current && current->kind() == TypeKind::Array) {
         auto* arrTy = static_cast<ArrayTy*>(current.get());
         if (arrTy->sizeExpr() && arrTy->sizeExpr()->astType() == ASTType::NumberExpression) {
            auto* num = static_cast<NumberExpressionAST*>(arrTy->sizeExpr().get());
            const Number rawSize = num->value();
            const uint64_t size = std::holds_alternative<double>(rawSize)
                ? static_cast<uint64_t>(std::get<double>(rawSize))
                : std::get<uint64_t>(rawSize);
            dimensions.push_back(size);
         } else {
            dimensions.push_back(0); // unknown size
         }
         current = arrTy->arrayType();
      }

      return dimensions;
   }
   std::string fullTypeString(const std::shared_ptr<Type>& ty) {
      if (!ty)
         return "";

      switch (ty->kind()) {
         case TypeKind::Pointer: {
            auto ptrTy = std::static_pointer_cast<PointerTy>(ty);
            return fullTypeString(ptrTy->pointee()) + "*";
         }
         case TypeKind::Array: {
            auto dimensions = getArrayDimensions(ty);

            auto current = ty;
            while (current && current->kind() == TypeKind::Array)
               current = static_cast<ArrayTy*>(current.get())->arrayType();

            std::string result = fullTypeString(current);
            for (uint64_t dim : dimensions) {
               if (dim == 0)
                  result += "[]";
               else
                  result += "[" + std::to_string(dim) + "]";
            }
            return result;
         }
         case TypeKind::Reference: {
            auto ref = std::static_pointer_cast<ReferenceTy>(ty);
            return fullTypeString(ref->reference()) + "&";
         }
         default:
            return ty->string();
      }
   }

} // namespace tungsten
