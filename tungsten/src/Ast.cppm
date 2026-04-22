module;

#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <filesystem>
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
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
#ifndef _NODISCARD
#define _NODISCARD [[nodiscard]]
#endif

export module Tungsten.ast;
import Tungsten.compileOptions;
import Tungsten.debugInfo;
import Tungsten.utils;

using namespace std::literals;
namespace fs = std::filesystem;

namespace tungsten {
   // llvm stuff (separate namespace block to avoid exporting to other modules)
   std::unique_ptr<llvm::LLVMContext> TheContext{};
   std::unique_ptr<llvm::IRBuilder<>> Builder{};
   std::unique_ptr<llvm::Module> TheModule{};

   std::unordered_map<std::string, llvm::Value*> NamedValues{};
   std::unordered_map<std::string, llvm::Type*> VariableTypes{};
   struct ClassScopeCleanupEntry {
      std::string className;
      llvm::Value* storage;
   };
   std::vector<std::vector<ClassScopeCleanupEntry>> ClassScopeCleanups{};
   struct LoopTargetEntry {
      llvm::BasicBlock* breakBB{};
      llvm::BasicBlock* continueBB{};
      size_t breakCleanupDepth{};
      size_t continueCleanupDepth{};
   };
   std::vector<LoopTargetEntry> LoopTargets{};
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
   bool isNumberType(const std::string& type) {
      return isSignedType(type) || isUnsignedType(type) || isFloatingPointType(type);
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

   std::unique_ptr<llvm::TargetMachine> createTargetMachine() {
      llvm::InitializeAllTargetInfos();
      llvm::InitializeAllTargets();
      llvm::InitializeAllTargetMCs();
      llvm::InitializeAllAsmParsers();
      llvm::InitializeAllAsmPrinters();

      auto triple = llvm::sys::getProcessTriple();
      TheModule->setTargetTriple(triple);

      std::string Error;
      auto Target = llvm::TargetRegistry::lookupTarget(triple, Error);
      if (!Target) {
         llvm::errs() << "error: target triple not supported: " << triple << '\n';
         return nullptr;
      }

      llvm::TargetOptions options{};
      auto rm = std::optional<llvm::Reloc::Model>{};
      auto CPU = llvm::sys::getHostCPUName();
      auto tm = std::unique_ptr<llvm::TargetMachine>(Target->createTargetMachine(triple, CPU, "", options, rm));
      if (!tm) {
         llvm::errs() << "error: target machine not supported: " << triple << '\n';
         return nullptr;
      }
      TheModule->setDataLayout(tm->createDataLayout());
      return tm;
   }

   bool emitObject(const CompileOptions& CO) {
      auto tm = createTargetMachine();
      if (!tm) return false;

      std::error_code ec;
      auto fileName = CO.outputFile.empty() ? TheModule->getModuleIdentifier() + ".o" : CO.outputFile;
      llvm::raw_fd_ostream outFile(fileName, ec, llvm::sys::fs::OF_None);
      if (ec) {
         std::cerr << "error opening output file '" << fileName << "': " << ec.message() << "\n";
         return false;
      }

      llvm::legacy::PassManager pm;
      if (tm->addPassesToEmitFile(pm, outFile, nullptr, llvm::CodeGenFileType::ObjectFile)) {
         std::cerr << "error: target does not support this file type\n";
         return false;
      }

      pm.run(*TheModule);
      return true;
   }

   bool emitAssembly(const CompileOptions& CO) {
      auto tm = createTargetMachine();
      if (!tm) return false;

      std::error_code ec;
      auto fileName = CO.outputFile.empty() ? TheModule->getModuleIdentifier() + ".asm" : CO.outputFile;
      llvm::raw_fd_ostream outFile(fileName, ec, llvm::sys::fs::OF_None);
      if (ec) {
         std::cerr << "error opening output file '" << fileName << "': " << ec.message() << "\n";
         return false;
      }

      llvm::legacy::PassManager pm;
      if (tm->addPassesToEmitFile(pm, outFile, nullptr, llvm::CodeGenFileType::AssemblyFile)) {
         std::cerr << "error: target does not support this file type\n";
         return false;
      }

      pm.run(*TheModule);
      return true;
   }

   bool emitLLVMIR(const CompileOptions& CO) {
      auto tm = createTargetMachine();
      if (!tm) return false;

      std::error_code EC;
      auto fileName = CO.outputFile.empty() ? TheModule->getModuleIdentifier() + ".ll" : CO.outputFile;
      llvm::raw_fd_ostream outFile(fileName, EC, llvm::sys::fs::OF_None);

      if (EC) {
         std::cerr << "error opening output file '" << fileName << "': " << EC.message() << "\n";
         return false;
      }
      TheModule->print(outFile, nullptr);
      return true;
   }
} // namespace tungsten

export namespace tungsten {
   void addCoreLibFunctions();
   void initLLVM(const std::string& moduleName, const std::string& fileName, const CompileOptions& CO) {
      TheContext = std::make_unique<llvm::LLVMContext>();
      Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
      TheModule = std::make_unique<llvm::Module>(moduleName, *TheContext);
      TheModule->setSourceFileName(fileName);

      // debug info
      DBuilder = std::make_unique<llvm::DIBuilder>(*TheModule);
      const fs::path sourcePath{fileName};
      const std::string sourceDir = sourcePath.has_parent_path() ? sourcePath.parent_path().string() : ".";
      const std::string sourceFile = sourcePath.has_filename() ? sourcePath.filename().string() : fileName;
      KSDbgInfo.TheFile = DBuilder->createFile(sourceFile, sourceDir);
      bool isDebug = std::find_if(CO.flags.begin(), CO.flags.end(), [](const std::string& flag) { return flag == "--strip"sv; }) == CO.flags.end();
      KSDbgInfo.TheCU = DBuilder->createCompileUnit(llvm::dwarf::DW_LANG_C_plus_plus,
                                                    KSDbgInfo.TheFile,
                                                    "Tungsten Compiler",
                                                    CO.optimizationLevel != OptimizationLevel::O0,
                                                    "", 0, "", isDebug ? llvm::DICompileUnit::FullDebug : llvm::DICompileUnit::NoDebug);
      KSDbgInfo.CurrentSubprogram = nullptr;
      KSDbgInfo.LexicalBlocks.clear();
      KSDbgInfo.LocalVariableScopes.clear();

      TheModule->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
#ifdef _WIN32
      TheModule->addModuleFlag(llvm::Module::Warning, "CodeView", 1);
#endif

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
      DBuilder->finalize();
      switch (CO.outputKind) {
         case OutputKind::Object:
            if (!emitObject(CO))
               std::cerr << "error: failed to emit object file\n";
            break;
         case OutputKind::Assembly:
            if (!emitAssembly(CO))
               std::cerr << "error: failed to emit assembly file\n";
            break;
         case OutputKind::LLVMIR:
         default:
            if (!emitLLVMIR(CO))
               std::cerr << "error: failed to emit LLVM IR\n";
            break;
      }

      // #endif
   }
   struct ASTVisitor {
      virtual ~ASTVisitor() = default;
      virtual void visit(class NumberExpressionAST&) = 0;
      virtual void visit(class VariableExpressionAST&) = 0;
      virtual void visit(class IndexAccessAST&) = 0;
      virtual void visit(class MemberAccessAST&) = 0;
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
      virtual void visit(class BreakStatementAST&) = 0;
      virtual void visit(class ContinueStatementAST&) = 0;
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
      MemberAccessExpression,
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
      BreakStatement,
      ContinueStatement,
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
      void setSource(size_t position, size_t length, size_t line, size_t column) {
         _sourcePosition = position;
         _sourceLength = length;
         _sourceLine = line;
         _sourceColumn = column;
         _hasSource = true;
      }
      _NODISCARD bool hasSource() const noexcept { return _hasSource; }
      _NODISCARD size_t sourcePosition() const noexcept { return _sourcePosition; }
      _NODISCARD size_t sourceLength() const noexcept { return _sourceLength; }
      _NODISCARD size_t sourceLine() const noexcept { return _sourceLine; }
      _NODISCARD size_t sourceColumn() const noexcept { return _sourceColumn; }

   protected:
      std::shared_ptr<Type> _Type;
      size_t _sourcePosition{0};
      size_t _sourceLength{0};
      size_t _sourceLine{1};
      size_t _sourceColumn{1};
      bool _hasSource{false};
   };

   // types declaration
   class Void : public Type {
   public:
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
   public:
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
   public:
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
   public:
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
   public:
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
   public:
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
   public:
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
   public:
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
   public:
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
   public:
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
   public:
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
   public:
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
   public:
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
   public:
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
   public:
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
   public:
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
   public:
      ClassTy(const std::string& name) : _name{name} {}
      _NODISCARD TypeKind kind() const noexcept override {
         return TypeKind::Class;
      }
      _NODISCARD const std::string string() const override {
         return _name;
      }
      _NODISCARD llvm::Type* llvmType() const override {
         return llvm::StructType::getTypeByName(*TheContext, _name);
      }

   private:
      std::string _name;
   };

   class NullTy : public Type {
   public:
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
   inline std::shared_ptr<Type> makeClass(const std::string& name) { return std::make_shared<ClassTy>(name); }
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
   TypeKind baseTypeKind(const std::shared_ptr<Type>& ty) {
      switch (ty->kind()) {
         case TypeKind::Pointer: {
            auto ptrTy = std::static_pointer_cast<PointerTy>(ty);
            return baseTypeKind(ptrTy->pointee());
         }
         case TypeKind::Array: {
            auto arrTy = std::static_pointer_cast<ArrayTy>(ty);
            return baseTypeKind(arrTy->arrayType());
         }
         case TypeKind::Reference: {
            auto refTy = std::static_pointer_cast<ReferenceTy>(ty);
            return baseTypeKind(refTy->reference());
         }
         default:
            return ty->kind();
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

   class MemberAccessAST : public ExpressionAST {
   public:
      MemberAccessAST(std::unique_ptr<ExpressionAST> base, std::unique_ptr<ExpressionAST> member, bool isArrow)
          : _base{std::move(base)}, _member{std::move(member)}, _isArrow{isArrow} {}

      llvm::Value* codegen() override;
      void accept(ASTVisitor& v) override { v.visit(*this); }
      void setType(std::shared_ptr<Type> type) { _Type = type; }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::MemberAccessExpression; }
      _NODISCARD std::unique_ptr<ExpressionAST>& base() { return _base; }
      _NODISCARD std::unique_ptr<ExpressionAST>& member() { return _member; }
      _NODISCARD bool isArrow() const { return _isArrow; }

   private:
      std::unique_ptr<ExpressionAST> _base;
      std::unique_ptr<ExpressionAST> _member;
      bool _isArrow;
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
      void setCallee(const std::string& callee) { _callee = callee; }

   private:
      std::string _callee;
      std::vector<std::unique_ptr<ExpressionAST>> _args{};
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

   class BreakStatementAST : public ExpressionAST {
   public:
      BreakStatementAST() { _Type = makeNullType(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::BreakStatement; }
   };

   class ContinueStatementAST : public ExpressionAST {
   public:
      ContinueStatementAST() { _Type = makeNullType(); }
      llvm::Value* codegen() override;

      void accept(ASTVisitor& v) override { v.visit(*this); }
      _NODISCARD std::shared_ptr<Type>& type() override { return _Type; }
      _NODISCARD ASTType astType() const noexcept override { return ASTType::ContinueStatement; }
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
      void setSource(size_t position, size_t length) {
         _sourcePosition = position;
         _sourceLength = length;
         _hasSource = true;
      }
      void setSource(size_t position, size_t length, size_t line, size_t column) {
         _sourcePosition = position;
         _sourceLength = length;
         _sourceLine = line;
         _sourceColumn = column;
         _hasSource = true;
      }
      _NODISCARD bool hasSource() const noexcept { return _hasSource; }
      _NODISCARD size_t sourcePosition() const noexcept { return _sourcePosition; }
      _NODISCARD size_t sourceLength() const noexcept { return _sourceLength; }
      _NODISCARD size_t sourceLine() const noexcept { return _sourceLine; }
      _NODISCARD size_t sourceColumn() const noexcept { return _sourceColumn; }
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
         utils::debugLog("Mangled name for function fun {}({}) -> {}: {}\n", _name, args, fullTypeString(_type), result);
         return result;
      }
      void setExternC(bool c) { _isExternC = c; }
      void setName(const std::string& name) { _name = name; }

   private:
      std::shared_ptr<Type> _type;
      std::string _name;
      bool _isExternC{false};
      std::vector<std::unique_ptr<ExpressionAST>> _args;
      size_t _sourcePosition{0};
      size_t _sourceLength{0};
      size_t _sourceLine{1};
      size_t _sourceColumn{1};
      bool _hasSource{false};
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

   class ClassAST;

   class ClassMethodAST {
   public:
      ClassMethodAST(Visibility visibility, bool isStatic, std::unique_ptr<FunctionAST> method)
          : _visibility{visibility}, _isStatic{isStatic}, _method{std::move(method)}, _parent(nullptr) {}
      llvm::Value* codegen();
      void accept(ASTVisitor& v) { v.visit(*this); }
      _NODISCARD Visibility visibility() const { return _visibility; }
      _NODISCARD bool isStatic() const { return _isStatic; }
      _NODISCARD std::unique_ptr<FunctionAST>& method() { return _method; }
      void setParent(ClassAST* parent) { _parent = parent; }
      ClassAST* parent() const { return _parent; }

   private:
      Visibility _visibility;
      bool _isStatic;
      std::unique_ptr<FunctionAST> _method;
      ClassAST* _parent;
   };

   class ClassVariableAST {
   public:
      ClassVariableAST(Visibility visibility, bool isStatic, std::unique_ptr<ExpressionAST> variable)
          : _visibility{visibility}, _isStatic{isStatic}, _variable{std::move(variable)} {}
      llvm::Value* codegen();
      void accept(ASTVisitor& v) { v.visit(*this); }
      _NODISCARD Visibility visibility() const { return _visibility; }
      _NODISCARD bool isStatic() const { return _isStatic; }
      _NODISCARD std::unique_ptr<ExpressionAST>& variable() { return _variable; }

   private:
      Visibility _visibility;
      bool _isStatic;
      std::unique_ptr<ExpressionAST> _variable;
   };

   class ClassConstructorAST {
   public:
      ClassConstructorAST(Visibility visibility, std::unique_ptr<FunctionAST> constructor)
          : _visibility{visibility}, _constructor{std::move(constructor)}, _parent(nullptr) {}
      llvm::Value* codegen();
      void accept(ASTVisitor& v) { v.visit(*this); }
      _NODISCARD Visibility visibility() const { return _visibility; }
      _NODISCARD std::unique_ptr<FunctionAST>& constructor() { return _constructor; }
      void setParent(ClassAST* parent) { _parent = parent; }
      ClassAST* parent() const { return _parent; }

   private:
      Visibility _visibility;
      std::unique_ptr<FunctionAST> _constructor;
      ClassAST* _parent;
   };
   class ClassDestructorAST {
   public:
      ClassDestructorAST(Visibility visibility, std::unique_ptr<FunctionAST> destructor)
          : _visibility{visibility}, _destructor{std::move(destructor)}, _parent{nullptr} {}
      llvm::Value* codegen();
      void accept(ASTVisitor& v) { v.visit(*this); }
      _NODISCARD Visibility visibility() const { return _visibility; }
      _NODISCARD std::unique_ptr<FunctionAST>& destructor() { return _destructor; }
      void setParent(ClassAST* parent) { _parent = parent; }
      ClassAST* parent() const { return _parent; }

   private:
      Visibility _visibility;
      std::unique_ptr<FunctionAST> _destructor;
      ClassAST* _parent;
   };

   class ClassAST {
   public:
      ClassAST(const std::string& name, std::vector<std::unique_ptr<ClassVariableAST>> variables, std::vector<std::unique_ptr<ClassMethodAST>> methods,
               std::vector<std::unique_ptr<ClassConstructorAST>> constructors, std::unique_ptr<ClassDestructorAST> destructor, bool hasErrors)
          : _name{name}, _members{std::move(variables)}, _methods{std::move(methods)}, _constructors{std::move(constructors)}, _destructor{std::move(destructor)}, _hasErrors{hasErrors} {}
      llvm::Value* codegen();

      _NODISCARD const std::string& name() const { return _name; }
      _NODISCARD const std::vector<std::unique_ptr<ClassVariableAST>>& members() const { return _members; }
      _NODISCARD const std::vector<std::unique_ptr<ClassMethodAST>>& methods() const { return _methods; }
      _NODISCARD const std::vector<std::unique_ptr<ClassConstructorAST>>& constructors() const { return _constructors; }
      _NODISCARD const std::unique_ptr<ClassDestructorAST>& destructor() const { return _destructor; }
      bool hasErrors() const { return _hasErrors; }

      void accept(ASTVisitor& v) { v.visit(*this); }
      void setSource(size_t position, size_t length) {
         _sourcePosition = position;
         _sourceLength = length;
         _hasSource = true;
      }
      void setSource(size_t position, size_t length, size_t line, size_t column) {
         _sourcePosition = position;
         _sourceLength = length;
         _sourceLine = line;
         _sourceColumn = column;
         _hasSource = true;
      }
      static std::unique_ptr<ClassDestructorAST> makeDefaultDestructor(const std::string& classname) {
         std::vector<std::unique_ptr<ExpressionAST>> args{};
         args.push_back(std::make_unique<VariableDeclarationAST>(makePointer(makeClass(classname)), "this", nullptr, false));
         auto proto = std::make_unique<FunctionPrototypeAST>(makeVoid(), "destructor", std::move(args));
         auto dtor = std::make_unique<FunctionAST>(std::move(proto), std::make_unique<BlockStatementAST>(std::vector<std::unique_ptr<ExpressionAST>>{}));
         return std::make_unique<ClassDestructorAST>(Visibility::Public, std::move(dtor));
      }
      static std::unique_ptr<ClassConstructorAST> makeDefaultConstructor(const std::string& classname) {
         std::vector<std::unique_ptr<ExpressionAST>> args{};
         args.push_back(std::make_unique<VariableDeclarationAST>(makePointer(makeClass(classname)), "this", nullptr, false));
         auto proto = std::make_unique<FunctionPrototypeAST>(makeVoid(), "constructor", std::move(args));
         auto ctor = std::make_unique<FunctionAST>(std::move(proto), std::make_unique<BlockStatementAST>(std::vector<std::unique_ptr<ExpressionAST>>{}));
         return std::make_unique<ClassConstructorAST>(Visibility::Public, std::move(ctor));
      }
      static std::unique_ptr<ClassConstructorAST> makeDefaultCopyConstructor(const std::string& classname) {
         std::vector<std::unique_ptr<ExpressionAST>> args{};
         args.push_back(std::make_unique<VariableDeclarationAST>(makePointer(makeClass(classname)), "this", nullptr, false));
         args.push_back(std::make_unique<VariableDeclarationAST>(makeReference(makeClass(classname)), "other", nullptr, true));
         auto proto = std::make_unique<FunctionPrototypeAST>(makeVoid(), "constructor", std::move(args));
         auto var1 = std::make_unique<UnaryExpressionAST>("*", std::make_unique<VariableExpressionAST>("this"));
         auto var2 = std::make_unique<VariableExpressionAST>("other");
         auto assign = std::make_unique<BinaryExpressionAST>("=", std::move(var1), std::move(var2));
         auto statements = std::vector<std::unique_ptr<ExpressionAST>>{};
         statements.push_back(std::move(assign));
         auto ctor = std::make_unique<FunctionAST>(std::move(proto), std::make_unique<BlockStatementAST>(std::move(statements)));
         return std::make_unique<ClassConstructorAST>(Visibility::Public, std::move(ctor));
      }
      static bool hasDefaultConstructor(const std::vector<std::unique_ptr<ClassConstructorAST>>& ctors, const std::string& className) {
         for (const auto& ctor : ctors) {
            const auto& args = ctor->constructor()->args();
            if (args.size() == 1) {
               auto* varDecl = static_cast<VariableDeclarationAST*>(args[0].get());
               if (varDecl && fullTypeString(varDecl->type()) == className + "*")
                  return true;
            }
         }
         return false;
      }

      static bool hasCopyConstructor(const std::vector<std::unique_ptr<ClassConstructorAST>>& ctors, const std::string& className) {
         for (const auto& ctor : ctors) {
            const auto& args = ctor->constructor()->args();
            if (args.size() == 2) {
               auto* varDecl1 = static_cast<VariableDeclarationAST*>(args[0].get());
               auto* varDecl2 = static_cast<VariableDeclarationAST*>(args[1].get());
               if (varDecl1 && varDecl2 &&
                   fullTypeString(varDecl1->type()) == className + "*" &&
                   fullTypeString(varDecl2->type()) == className + "&")
                  return true;
            }
         }
         return false;
      }

   private:
      std::string _name;
      std::vector<std::unique_ptr<ClassVariableAST>> _members;
      std::vector<std::unique_ptr<ClassMethodAST>> _methods;
      std::vector<std::unique_ptr<ClassConstructorAST>> _constructors;
      std::unique_ptr<ClassDestructorAST> _destructor;
      size_t _sourcePosition{0};
      size_t _sourceLength{0};
      size_t _sourceLine{1};
      size_t _sourceColumn{1};
      bool _hasSource{false};
      bool _hasErrors{false};
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
   static uint64_t debugTypeSizeBits(llvm::Type* ty) {
      if (!ty || !TheModule || !ty->isSized())
         return 0;
      return TheModule->getDataLayout().getTypeAllocSizeInBits(ty);
   }

   static uint32_t debugTypeAlignBits(llvm::Type* ty) {
      if (!ty || !TheModule || !ty->isSized())
         return 0;
      return static_cast<uint32_t>(TheModule->getDataLayout().getABITypeAlign(ty).value() * 8);
   }

   static llvm::DIType* debugTypeFor(const std::shared_ptr<Type>& ty) {
      if (!ty)
         return DBuilder->createUnspecifiedType("unknown");

      switch (ty->kind()) {
         case TypeKind::Bool:
            return KSDbgInfo.boolTy();
         case TypeKind::Char:
            return KSDbgInfo.charTy();
         case TypeKind::Int8:
            return KSDbgInfo.int8Ty();
         case TypeKind::Uint8:
            return KSDbgInfo.uint8Ty();
         case TypeKind::Int16:
            return KSDbgInfo.int16Ty();
         case TypeKind::Uint16:
            return KSDbgInfo.uint16Ty();
         case TypeKind::Int32:
            return KSDbgInfo.int32Ty();
         case TypeKind::Uint32:
            return KSDbgInfo.uint32Ty();
         case TypeKind::Int64:
            return KSDbgInfo.int64Ty();
         case TypeKind::Uint64:
            return KSDbgInfo.uint64Ty();
         case TypeKind::Int128:
            return KSDbgInfo.int128Ty();
         case TypeKind::Uint128:
            return KSDbgInfo.uint128Ty();
         case TypeKind::Float:
            return KSDbgInfo.floatTy();
         case TypeKind::Double:
            return KSDbgInfo.doubleTy();
         case TypeKind::Void:
            return DBuilder->createUnspecifiedType("void");
         case TypeKind::Pointer: {
            auto* ptrTy = static_cast<PointerTy*>(ty.get());
            const uint64_t ptrBits = TheModule ? TheModule->getDataLayout().getPointerSizeInBits(0) : 64;
            return KSDbgInfo.pointerTy(debugTypeFor(ptrTy->pointee()), ptrBits, static_cast<uint32_t>(ptrBits));
         }
         case TypeKind::Reference: {
            auto* refTy = static_cast<ReferenceTy*>(ty.get());
            const uint64_t ptrBits = TheModule ? TheModule->getDataLayout().getPointerSizeInBits(0) : 64;
            return KSDbgInfo.pointerTy(debugTypeFor(refTy->reference()), ptrBits, static_cast<uint32_t>(ptrBits));
         }
         case TypeKind::Array: {
            auto* arrTy = static_cast<ArrayTy*>(ty.get());
            uint64_t count = 0;
            if (arrTy->sizeExpr() && arrTy->sizeExpr()->astType() == ASTType::NumberExpression) {
               const Number rawSize = static_cast<NumberExpressionAST*>(arrTy->sizeExpr().get())->value();
               count = std::holds_alternative<double>(rawSize)
                   ? static_cast<uint64_t>(std::get<double>(rawSize))
                   : std::get<uint64_t>(rawSize);
            }
            llvm::Type* arrLlvmTy = ty->llvmType();
            return KSDbgInfo.arrayTy(debugTypeFor(arrTy->arrayType()), count, debugTypeSizeBits(arrLlvmTy), debugTypeAlignBits(arrLlvmTy));
         }
         default:
            return DBuilder->createUnspecifiedType(ty->string());
      }
   }

   static uint32_t debugLineFor(ExpressionAST* expr) {
      if (!expr || !expr->hasSource())
         return 1;
      return static_cast<uint32_t>(expr->sourceLine());
   }

   static uint32_t debugColumnFor(ExpressionAST* expr) {
      if (!expr || !expr->hasSource())
         return 1;
      return static_cast<uint32_t>(expr->sourceColumn());
   }

   static void setDebugLocationFor(ExpressionAST* expr) {
      if (!Builder || !TheContext)
         return;
      llvm::DIScope* scope = KSDbgInfo.currentScope();
      if (!scope)
         return;
      Builder->SetCurrentDebugLocation(llvm::DILocation::get(*TheContext, debugLineFor(expr), debugColumnFor(expr), scope));
   }

   static void emitDbgValueForVariableName(const std::string& variableName, llvm::Value* value, ExpressionAST* locationExpr) {
      if (!DBuilder || !Builder || !TheContext || variableName.empty() || !value)
         return;

      llvm::DILocalVariable* localVar = KSDbgInfo.lookupLocalVariable(variableName);
      if (!localVar)
         return;

      llvm::BasicBlock* insertBlock = Builder->GetInsertBlock();
      llvm::DIScope* scope = KSDbgInfo.currentScope();
      if (!insertBlock || !scope)
         return;

      llvm::DILocation* location = llvm::DILocation::get(*TheContext, debugLineFor(locationExpr), debugColumnFor(locationExpr), scope);
      DBuilder->insertDbgValueIntrinsic(value,
                                        localVar,
                                        DBuilder->createExpression(),
                                        location,
                                        insertBlock);
   }

   static uint32_t functionDebugLine(FunctionPrototypeAST* prototype, ExpressionAST* body) {
      if (body && body->hasSource())
         return static_cast<uint32_t>(body->sourceLine());
      if (prototype) {
         for (const auto& arg : prototype->args()) {
            if (arg && arg->hasSource())
               return static_cast<uint32_t>(arg->sourceLine());
         }
      }
      return 1;
   }

   static uint32_t functionDebugColumn(FunctionPrototypeAST* prototype, ExpressionAST* body) {
      if (body && body->hasSource())
         return static_cast<uint32_t>(body->sourceColumn());
      if (prototype) {
         for (const auto& arg : prototype->args()) {
            if (arg && arg->hasSource())
               return static_cast<uint32_t>(arg->sourceColumn());
         }
      }
      return 1;
   }

   static llvm::DISubprogram* createFunctionDebugInfo(llvm::Function* function, FunctionPrototypeAST* prototype, ExpressionAST* body) {
      if (!function || !prototype || !DBuilder || !KSDbgInfo.TheFile)
         return nullptr;

      std::vector<llvm::Metadata*> diTypes;
      diTypes.push_back(debugTypeFor(prototype->type()));
      for (const auto& arg : prototype->args())
         diTypes.push_back(debugTypeFor(arg->type()));

      auto* subroutineType = DBuilder->createSubroutineType(DBuilder->getOrCreateTypeArray(diTypes));
      const uint32_t line = functionDebugLine(prototype, body);
      auto* subprogram = DBuilder->createFunction(
          KSDbgInfo.TheFile,
          prototype->name(),
          function->getName(),
          KSDbgInfo.TheFile,
          line,
          subroutineType,
          line,
          llvm::DINode::FlagPrototyped,
          llvm::DISubprogram::SPFlagDefinition);

      function->setSubprogram(subprogram);
      KSDbgInfo.CurrentSubprogram = subprogram;
      return subprogram;
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
      if (!NamedValues.contains(_name) || !NamedValues[_name]) {
         return LogErrorV("unknown variable: '" + _name + "'");
      }

      return NamedValues[_name];
   }

   llvm::Value* StringExpression::codegen() {
      setDebugLocationFor(this);
      return Builder->CreateGlobalStringPtr(_value, "strtmp");
   }

   llvm::Value* UnaryExpressionAST::codegen() {
      setDebugLocationFor(this);
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
            if (_operand->astType() == ASTType::VariableExpression)
               emitDbgValueForVariableName(static_cast<VariableExpressionAST*>(_operand.get())->name(), result, this);
            return result;
         }

         if (operandType->isIntegerTy())
            result = Builder->CreateSub(loadedOperand, llvm::ConstantInt::get(operandType, 1), "decrement");
         else if (operandType->isFloatingPointTy())
            result = Builder->CreateFSub(loadedOperand, llvm::ConstantFP::get(operandType, 1.0), "decrement");
         else
            return LogErrorV("unsupported type for decrement operation");
         auto* stored = Builder->CreateStore(result, targetAddr);
         if (_operand->astType() == ASTType::VariableExpression)
            emitDbgValueForVariableName(static_cast<VariableExpressionAST*>(_operand.get())->name(), result, this);
         return stored;
      }

      llvm::Type* operandType = operandValue->getType(); // TODO: make it work with both L and R values
      if (_op == "!"sv) {
         if (operandType->isIntegerTy())
            return Builder->CreateICmpEQ(operandValue, llvm::ConstantInt::get(operandType, 0), "nottmp");

         if (operandType->isFloatingPointTy())
            return Builder->CreateFCmpOEQ(operandValue, llvm::ConstantFP::get(operandType, 0.0), "nottmp");

         if (operandType->isPointerTy())
            return Builder->CreateICmpEQ(operandValue, llvm::Constant::getNullValue(operandType), "nottmp");

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
   void emitClassScopeDestructors(bool allScopes) {
      if (ClassScopeCleanups.empty())
         return;

      auto emitForScope = [](std::vector<ClassScopeCleanupEntry>& entries) {
         for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
            if (!it->storage)
               continue;

            const std::string dtorName = it->className + "-destructor$" + it->className + "*";
            llvm::Function* dtor = TheModule->getFunction(dtorName);
            if (!dtor) {
               utils::debugLog("no destructor found for class '{}' when looking for destructor with this signature: {}", it->className, dtorName);
               continue;
            }

            Builder->CreateCall(dtor, {it->storage});
         }
      };

      if (allScopes) {
         for (auto it = ClassScopeCleanups.rbegin(); it != ClassScopeCleanups.rend(); ++it)
            emitForScope(*it);
      } else {
         emitForScope(ClassScopeCleanups.back());
      }
   }
   void emitClassScopeDestructorsToDepth(size_t targetDepth) {
      if (ClassScopeCleanups.size() <= targetDepth)
         return;

      auto emitForScope = [](std::vector<ClassScopeCleanupEntry>& entries) {
         for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
            if (!it->storage)
               continue;

            const std::string dtorName = it->className + "-destructor$";
            llvm::Function* dtor = TheModule->getFunction(dtorName);
            if (!dtor)
               continue;

            Builder->CreateCall(dtor, {it->storage});
         }
      };

      for (size_t depth = ClassScopeCleanups.size(); depth > targetDepth; --depth)
         emitForScope(ClassScopeCleanups[depth - 1]);
   }
   llvm::Value* branchToLoopTarget(llvm::BasicBlock* target, size_t cleanupDepth) {
      if (!target)
         return LogErrorV("invalid loop target");

      emitClassScopeDestructorsToDepth(cleanupDepth);
      Builder->CreateBr(target);
      return llvm::Constant::getNullValue(llvm::Type::getInt32Ty(*TheContext));
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
      setDebugLocationFor(this);

      llvm::Value* LHS = _LHS->codegen();
      llvm::Value* RHS = _RHS->codegen();

      if (!LHS || !RHS)
         return LogErrorV("invalid operands in binary expression '" + _op + "'");

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
         if (_LHS->type()->kind() == TypeKind::Reference) {
            auto* refTy = static_cast<ReferenceTy*>(_LHS->type().get());
            targetAddr = LHS;
            lhsValueType = refTy->reference()->llvmType();
         } else if (_LHS->astType() == ASTType::UnaryExpression && static_cast<UnaryExpressionAST*>(_LHS.get())->op() == "*"sv) {
            auto& derefOperand = static_cast<UnaryExpressionAST*>(_LHS.get())->operand();
            if (!derefOperand)
               return LogErrorV("invalid dereference target");

            llvm::Type* pointerValueType = derefOperand->type()->llvmType();
            targetAddr = Builder->CreateLoad(pointerValueType, LHS, "deref.addr");
         }

         if (_LHS->type()->kind() == TypeKind::Pointer && _LHS->astType() != ASTType::UnaryExpression) {
            Builder->CreateStore(RHS, LHS);
            if (_LHS->astType() == ASTType::VariableExpression)
               emitDbgValueForVariableName(static_cast<VariableExpressionAST*>(_LHS.get())->name(), RHS, this);
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
         if (_LHS->astType() == ASTType::VariableExpression)
            emitDbgValueForVariableName(static_cast<VariableExpressionAST*>(_LHS.get())->name(), result, this);
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

      if (_LHS->astType() == ASTType::NullPtr)
         loadedL = LHS;
      if (_RHS->astType() == ASTType::NullPtr)
         loadedR = RHS;

      loadedR = castToCommonType(loadedR, lhsValueType);

      if (_op == "&&"sv) {
         LHS = Builder->CreateICmpNE(loadedL, loadedR, "lcond");
         RHS = Builder->CreateICmpNE(loadedR, loadedR, "rcond");
         return Builder->CreateAnd(LHS, RHS);
      }
      if (_op == "||"sv) {
         LHS = Builder->CreateICmpNE(loadedL, loadedR, "lcond");
         RHS = Builder->CreateICmpNE(loadedR, loadedR, "rcond");
         return Builder->CreateOr(LHS, RHS);
      }

      if ((_op == "=="sv || _op == "!="sv || _op == "<"sv || _op == "<="sv || _op == ">"sv || _op == ">="sv) &&
          (loadedL->getType()->isPointerTy() || loadedR->getType()->isPointerTy())) {
         if (_op == "=="sv)
            return Builder->CreateICmpEQ(loadedL, loadedR, "ptrcmp");
         if (_op == "!="sv)
            return Builder->CreateICmpNE(loadedL, loadedR, "ptrcmp");
         if (_op == "<"sv)
            return Builder->CreateICmpULT(loadedL, loadedR, "ptrcmp");
         if (_op == "<="sv)
            return Builder->CreateICmpULE(loadedL, loadedR, "ptrcmp");
         if (_op == ">"sv)
            return Builder->CreateICmpUGT(loadedL, loadedR, "ptrcmp");
         return Builder->CreateICmpUGE(loadedL, loadedR, "ptrcmp");
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
      setDebugLocationFor(this);

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
         auto isPrimitiveArray = [](const std::shared_ptr<Type>& t) {
            std::shared_ptr<Type> current = t;
            while (current && current->kind() == TypeKind::Array) {
               current = static_cast<ArrayTy*>(current.get())->arrayType();
            }
            return current && current->kind() != TypeKind::Class;
         };
         bool primitiveArray = false;
         if (_Type->kind() == TypeKind::Array) {
            primitiveArray = isPrimitiveArray(std::static_pointer_cast<ArrayTy>(_Type)->arrayType());
         }
         if (isNullPtrArray || (initVal && (_Type->kind() != TypeKind::Array || primitiveArray))) {
            llvm::Value* storeVal = isNullPtrArray ? llvm::Constant::getNullValue(type) : initVal;
            Builder->CreateStore(storeVal, allocInstance);
         }
      } else if (_Type->kind() == TypeKind::Pointer) {
         Builder->CreateStore(llvm::ConstantPointerNull::get(Builder->getPtrTy()), allocInstance);

      } else if (_Type->kind() == TypeKind::Array) {
         auto arrTy = static_cast<ArrayTy*>(_Type.get());
         auto elemType = arrTy->arrayType();
         if (elemType->kind() == TypeKind::Class) {
            const std::string className = elemType->string();
            const std::string ctorName = className + "-constructor$";
            llvm::Function* ctor = TheModule->getFunction(ctorName);
            if (ctor && arrTy->sizeExpr() && arrTy->sizeExpr()->astType() == ASTType::NumberExpression) {
               auto* num = static_cast<NumberExpressionAST*>(arrTy->sizeExpr().get());
               const Number rawSize = num->value();
               uint64_t count = std::holds_alternative<double>(rawSize)
                   ? static_cast<uint64_t>(std::get<double>(rawSize))
                   : std::get<uint64_t>(rawSize);
               // Call constructor for each element
               for (uint64_t i = 0; i < count; ++i) {
                  Builder->CreateCall(ctor, {allocInstance});
               }
               // Register for destructor call at scope exit
               if (!ClassScopeCleanups.empty())
                  for (uint64_t i = 0; i < count; ++i)
                     ClassScopeCleanups.back().push_back({className, Builder->CreateGEP(elemType->llvmType(), allocInstance, Builder->getInt64(i))});
            }
         } else if (isNumberType(elemType->string())) {
            llvm::Constant* zero = llvm::ConstantAggregateZero::get(type);
            Builder->CreateStore(zero, allocInstance);
         }

      } else if (isNumberType(_Type->string()) && !_init)
         Builder->CreateStore(llvm::Constant::getNullValue(type), allocInstance);

      NamedValues[_name] = allocInstance;
      VariableTypes[_name] = type;

      llvm::DIScope* scope = KSDbgInfo.currentScope();
      if (scope && DBuilder && KSDbgInfo.TheFile) {
         const uint32_t line = debugLineFor(this);
         const uint32_t column = debugColumnFor(this);
         auto* localVar = DBuilder->createAutoVariable(scope, _name, KSDbgInfo.TheFile, line, debugTypeFor(_Type), true);
         KSDbgInfo.registerLocalVariable(_name, localVar);
         DBuilder->insertDeclare(allocInstance,
                                 localVar,
                                 DBuilder->createExpression(),
                                 llvm::DILocation::get(*TheContext, line, column, scope),
                                 Builder->GetInsertBlock());
      }

      return allocInstance;
   }

   llvm::Value* IndexAccessAST::codegen() {
      setDebugLocationFor(this);
      llvm::Value* arrayVal = _array->codegen();
      llvm::Value* indexVal = _index->codegen();

      if (!arrayVal || !indexVal)
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

      // allocating the array if passed by value instead of by pointer
      if (baseTy->isArrayTy() && !arrayVal->getType()->isPointerTy()) {
         llvm::Function* function = Builder->GetInsertBlock()->getParent();
         llvm::IRBuilder<> tmpBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());
         llvm::AllocaInst* allocInstance = tmpBuilder.CreateAlloca(baseTy, nullptr, "tmp.arrval");
         Builder->CreateStore(arrayVal, allocInstance);
         arrayVal = allocInstance;
      }

      if (baseTy->isArrayTy()) {
         llvm::Value* zero = Builder->getInt64(0);
         return Builder->CreateGEP(baseTy, arrayVal, {zero, indexVal}, "elementptr");
      }

      if (_array->type()->kind() == TypeKind::Pointer) {
         auto* ptrTy = static_cast<PointerTy*>(_array->type().get());

         if (_array->astType() == ASTType::VariableExpression && arrayVal->getType()->isPointerTy() && !llvm::isa<llvm::Argument>(arrayVal))
            arrayVal = Builder->CreateLoad(arrayVal->getType(), arrayVal, "arrptr.load");

         llvm::Type* gepType = ptrTy->pointee()->llvmType();
         llvm::Value* elementPtr = Builder->CreateGEP(gepType, arrayVal, indexVal, "elementptr");

         if (ptrTy->pointee()->kind() == TypeKind::Pointer)
            elementPtr = Builder->CreateLoad(gepType, elementPtr, "element");

         return elementPtr;
      }

      return LogErrorV("cannot index non-array, non-pointer value");
   }
   llvm::Value* MemberAccessAST::codegen() {
      return nullptr;
   }
   // llvm::Value* MemberAccessAST::codegen() {
   //    setDebugLocationFor(this);
   //    llvm::Value* baseValue = _base->codegen();
   //    if (!baseValue)
   //       return LogErrorV("invalid base expression for member access");
   //
   //    std::shared_ptr<Type> baseType = _base->type();
   //    if (!baseType)
   //       return LogErrorV("invalid type for member access");
   //
   //    // Gestione del puntatore se '->'
   //    if (_isArrow) {
   //       if (baseType->kind() != TypeKind::Pointer)
   //          return LogErrorV("operator '->' requires a pointer type");
   //       baseType = static_cast<PointerTy*>(baseType.get())->pointee();
   //       baseValue = Builder->CreateLoad(baseValue->getType(), baseValue, "deref");
   //    }
   //
   //    if (baseType->kind() != TypeKind::Class)
   //       return LogErrorV("member access requires a class type");
   //
   //    // Ricava il nome del membro
   //    auto* memberVar = dynamic_cast<VariableExpressionAST*>(_member.get());
   //    if (!memberVar)
   //       return LogErrorV("member expression is not a variable");
   //    const std::string& memberName = memberVar->name();
   //
   //    // Cerca la definizione della classe
   //    auto it = std::find_if(allClasses.begin(), allClasses.end(), [&](const auto& c) { return c->name() == baseType->string(); });
   //    if (it == allClasses.end())
   //       return LogErrorV("class '" + baseType->string() + "' not found for member access");
   //
   //    // Cerca membro variabile
   //    const auto& members = (*it)->members();
   //    for (size_t i = 0; i < members.size(); ++i) {
   //       const auto& member = members[i];
   //       if (member->variable()->astType() == ASTType::VariableDeclaration) {
   //          auto* varDecl = static_cast<VariableDeclarationAST*>(member->variable().get());
   //          if (varDecl->name() == memberName) {
   //             llvm::Value* indices[] = {Builder->getInt32(0), Builder->getInt32(static_cast<uint32_t>(i))};
   //             llvm::Value* ptr = Builder->CreateGEP(baseValue->getType(), baseValue, indices, "memberptr");
   //             if (_Type->kind() == TypeKind::Reference)
   //                return ptr;
   //             return Builder->CreateLoad(expressionValueTypeForCodegen(this), ptr, memberName);
   //          }
   //       }
   //    }
   //    // Cerca metodo
   //    const auto& methods = (*it)->methods();
   //    for (const auto& method : methods) {
   //       if (method && method->method() && method->method()->prototype() && method->method()->prototype()->name() == memberName) {
   //          llvm::Function* fn = TheModule->getFunction(method->method()->prototype()->mangledName());
   //          if (!fn)
   //             return LogErrorV("method '" + memberName + "' not found in module");
   //          std::vector<llvm::Value*> args;
   //          args.push_back(baseValue);
   //          return Builder->CreateCall(fn, args, memberName);
   //       }
   //    }
   //    return LogErrorV("member '" + memberName + "' not found in class '" + baseType->string() + "'");
   // }

   llvm::Value* CallExpressionAST::codegen() {
      setDebugLocationFor(this);

      std::string name = _callee + "$";
      for (auto& arg : _args) {
         if (arg != _args.front())
            name += "$";
         if (arg->astType() == ASTType::UnaryExpression && static_cast<UnaryExpressionAST*>(arg.get())->op() == "&"sv) {
            if (arg->type()->kind() == TypeKind::Reference)
               name += fullTypeString(static_cast<UnaryExpressionAST*>(arg.get())->operand()->type()) + "&";
            else
               name += fullTypeString(static_cast<UnaryExpressionAST*>(arg.get())->operand()->type()) + "*";
         } else
            name += fullTypeString(arg->type());
      }
      utils::debugLog("looking for function with mangled name: {}", name);
      llvm::Function* callee = TheModule->getFunction(name);

      if (!callee) {
         utils::debugLog("{} not found: looking for {}", name, _callee);
         callee = TheModule->getFunction(_callee); // for extern functions ex. shell$String doesn't exist -> shell exists
         if (!callee)
            return LogErrorV("function '" + _callee + "' not found");
         utils::debugLog("found function: {}", _callee);
      }
      utils::debugLog("llvm function arguments size: {}", callee->arg_size());

      std::vector<llvm::Value*> args;
      std::string argsTypes;
      utils::debugLog("arguments size: {}", _args.size());
      for (size_t i = 0; i < _args.size(); ++i) {
         auto& arg = _args[i];
         argsTypes += fullTypeString(arg->type());
         llvm::Value* argVal = arg->codegen();
         if (!argVal) {
            utils::debugLog("arg[{}] codegen returned nullptr", i);
            return nullptr;
         }

         if (arg->astType() == ASTType::UnaryExpression && static_cast<UnaryExpressionAST*>(arg.get())->op() == "*"sv) {
            auto& derefOperand = static_cast<UnaryExpressionAST*>(arg.get())->operand();
            if (!derefOperand)
               return LogErrorV("invalid dereference argument");

            llvm::Type* pointerValueType = derefOperand->type()->llvmType();
            argVal = Builder->CreateLoad(pointerValueType, argVal, "arg.deref.addr");
         }
         if (arg->astType() == ASTType::VariableExpression && argVal->getType()->isPointerTy() && arg->type()->kind() != TypeKind::Array) {
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
            if (loadedType && argVal->getType()->isPointerTy() && arg->type()->kind() != TypeKind::Pointer && arg->type()->kind() != TypeKind::Reference && !indexAccessAlreadyValue)
               argVal = Builder->CreateLoad(loadedType, argVal, "arg.idxload");
         }
         llvm::Type* expectedParamType = nullptr;
         if (callee && i < callee->arg_size())
            expectedParamType = callee->getFunctionType()->getParamType(static_cast<unsigned>(i));
         if (expectedParamType) {
            if (!expectedParamType->isPointerTy() && argVal->getType()->isPointerTy()) {
               argVal = Builder->CreateLoad(expectedParamType, argVal, "arg.load");
            }
            argVal = castToCommonType(argVal, expectedParamType);
         } else {
            if (argVal->getType()->isPointerTy() && arg->type()->kind() != TypeKind::Pointer && arg->type()->kind() != TypeKind::Reference && arg->type()->kind() != TypeKind::Array && !indexAccessAlreadyValue) {
               argVal = Builder->CreateLoad(expressionValueTypeForCodegen(arg.get()), argVal, "arg.load");
            }
         }
         args.push_back(argVal);
      }
      setDebugLocationFor(this);
      utils::debugLog("calling function with arguments: {}\n", argsTypes);
      llvm::Value* callResult = Builder->CreateCall(callee, args);

      // Register class instance for cleanup if this is a constructor call
      if (!ClassScopeCleanups.empty() && _callee.find("-constructor") != std::string::npos) {
         // first argument is 'this' so it's the storage pointer
         if (!args.empty()) {
            std::string className = _callee.substr(0, _callee.find("-"));
            ClassScopeCleanups.back().push_back({className, args[0]});
         }
      }
      return callResult;
   }

   llvm::Value* TypeOfStatementAST::codegen() {
      setDebugLocationFor(this);
      return Builder->CreateGlobalStringPtr(fullTypeString(_statement->type()));
   }
   llvm::Value* NameOfStatementAST::codegen() {
      setDebugLocationFor(this);
      std::string name;
      if (!tryBuildNameOfExpression(_statement.get(), name))
         return LogErrorV("cannot use 'nameof' on expression without a stable name");
      return Builder->CreateGlobalStringPtr(name);
   }
   llvm::Value* SizeOfStatementAST::codegen() {
      if (hasExplicitType()) {
         auto type = explicitType();
         if (!type)
            return LogErrorV("cannot use 'sizeof' on invalid type");
         return Builder->getInt64(type->size());
      }

      auto* expr = statement();
      if (!expr || !expr->type())
         return LogErrorV("cannot use 'sizeof' on expression without a type");

      return Builder->getInt64(expr->type()->size());
   }

   llvm::Value* __BuiltinFunctionAST::codegen() {
      setDebugLocationFor(this);
      return Builder->CreateGlobalStringPtr(_function, "strtmp");
   }
   llvm::Value* __BuiltinLineAST::codegen() {
      return Builder->getInt64(_line);
   }
   llvm::Value* __BuiltinColumnAST::codegen() {
      return Builder->getInt64(_column);
   }
   llvm::Value* __BuiltinFileAST::codegen() {
      setDebugLocationFor(this);
      return Builder->CreateGlobalStringPtr(_file, "strtmp");
   }

   llvm::Value* StaticCastAST::codegen() {
      setDebugLocationFor(this);

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
      setDebugLocationFor(this);
      KSDbgInfo.pushLexicalBlock(debugLineFor(this), debugColumnFor(this));
      KSDbgInfo.pushVariableScope();
      ClassScopeCleanups.push_back({});

      llvm::Value* last = llvm::Constant::getNullValue(llvm::Type::getInt8Ty(*TheContext)); // to avoid empty functions from getting recognized as wrong
      for (const auto& statement : _statements) {
         if (Builder->GetInsertBlock() && Builder->GetInsertBlock()->getTerminator())
            break;

         last = statement->codegen();
         if (!last) {
            ClassScopeCleanups.pop_back();
            KSDbgInfo.popVariableScope();
            KSDbgInfo.popLexicalBlock();
            return nullptr;
         }

         if (Builder->GetInsertBlock() && Builder->GetInsertBlock()->getTerminator())
            break;
      }

      if (Builder->GetInsertBlock() && !Builder->GetInsertBlock()->getTerminator())
         emitClassScopeDestructors(false);

      ClassScopeCleanups.pop_back();
      KSDbgInfo.popVariableScope();
      KSDbgInfo.popLexicalBlock();
      return last;
   }

   llvm::Value* ExternVariableStatementAST::codegen() {
      return nullptr;
   }

   llvm::Value* ReturnStatementAST::codegen() {
      setDebugLocationFor(this);

      auto moveToDeadUnreachableBlock = [&]() {
         llvm::Function* function = Builder->GetInsertBlock()->getParent();
         llvm::BasicBlock* deadBB = llvm::BasicBlock::Create(*TheContext, "return.unreachable", function);
         Builder->SetInsertPoint(deadBB);
         Builder->CreateUnreachable();
      };

      if (_value == nullptr) {
         emitClassScopeDestructors(true);
         llvm::Value* ret = Builder->CreateRetVoid();
         moveToDeadUnreachableBlock();
         return ret;
      }

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

      if (!returnsReference && _Type->kind() == TypeKind::Array) {
         llvm::Type* retType = valueTypeForCodegen(_Type);
         if (returnValue->getType()->isPointerTy() && retType->isArrayTy()) {
            returnValue = Builder->CreateLoad(retType, returnValue, "arrval");
         }
      }

      if (returnsReference) {
         if (!returnValue->getType()->isPointerTy())
            return LogErrorV("reference return requires an lvalue/address expression");
         emitClassScopeDestructors(true);
         llvm::Value* ret = Builder->CreateRet(returnValue);
         moveToDeadUnreachableBlock();
         return ret;
      }

      returnValue = castToCommonType(returnValue, valueTypeForCodegen(_Type));
      emitClassScopeDestructors(true);
      llvm::Value* ret = Builder->CreateRet(returnValue);
      moveToDeadUnreachableBlock();
      return ret;
   }
   llvm::Value* ExitStatement::codegen() {
      setDebugLocationFor(this);

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

      setDebugLocationFor(this);
      llvm::Value* call = Builder->CreateCall(exitFunc, exitValue);

      Builder->CreateUnreachable();

      return call;
   }

   llvm::Value* BreakStatementAST::codegen() {
      setDebugLocationFor(this);
      if (LoopTargets.empty())
         return LogErrorV("'break' used outside of a loop");

      const auto& target = LoopTargets.back();
      return branchToLoopTarget(target.breakBB, target.breakCleanupDepth);
   }

   llvm::Value* ContinueStatementAST::codegen() {
      setDebugLocationFor(this);
      if (LoopTargets.empty())
         return LogErrorV("'continue' used outside of a loop");

      const auto& target = LoopTargets.back();
      return branchToLoopTarget(target.continueBB, target.continueCleanupDepth);
   }

   llvm::Value* IfStatementAST::codegen() {
      setDebugLocationFor(this);

      llvm::Value* CondV = _condition->codegen();
      if (!CondV)
         return nullptr;

      CondV = Builder->CreateICmpNE(CondV, llvm::ConstantInt::get(CondV->getType(), 0), "if.cond");

      llvm::Function* function = Builder->GetInsertBlock()->getParent();

      llvm::BasicBlock* ThenBB = llvm::BasicBlock::Create(*TheContext, "if.then", function);
      llvm::BasicBlock* ElseBB = llvm::BasicBlock::Create(*TheContext, "if.else");
      llvm::BasicBlock* MergeBB = llvm::BasicBlock::Create(*TheContext, "if.end");

      setDebugLocationFor(this);
      Builder->CreateCondBr(CondV, ThenBB, ElseBB);

      // then
      Builder->SetInsertPoint(ThenBB);
      bool thenTerminated = false;
      if (_thenBranch)
         _thenBranch->codegen();
      thenTerminated = Builder->GetInsertBlock() && Builder->GetInsertBlock()->getTerminator();
      if (!thenTerminated)
         Builder->CreateBr(MergeBB);

      // else
      function->insert(function->end(), ElseBB);
      Builder->SetInsertPoint(ElseBB);
      bool elseTerminated = false;
      if (_elseBranch)
         _elseBranch->codegen();
      elseTerminated = Builder->GetInsertBlock() && Builder->GetInsertBlock()->getTerminator();
      if (!elseTerminated)
         Builder->CreateBr(MergeBB);

      // end of if
      function->insert(function->end(), MergeBB);
      Builder->SetInsertPoint(MergeBB);

      if (thenTerminated && elseTerminated) {
         setDebugLocationFor(this);
         Builder->CreateUnreachable();
      }

      return llvm::Constant::getNullValue(llvm::Type::getInt32Ty(*TheContext));
   }


   llvm::Value* WhileStatementAST::codegen() {
      setDebugLocationFor(this);

      llvm::Function* function = Builder->GetInsertBlock()->getParent();

      llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*TheContext, "while.cond", function);
      llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*TheContext, "while.body");
      llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*TheContext, "while.end");

      ClassScopeCleanups.push_back({});
      LoopTargets.push_back({endBB, condBB, ClassScopeCleanups.size() - 1, ClassScopeCleanups.size()});

      Builder->CreateBr(condBB);

      // condition
      Builder->SetInsertPoint(condBB);
      llvm::Value* CondV = _condition->codegen();
      if (!CondV) {
         LoopTargets.pop_back();
         ClassScopeCleanups.pop_back();
         return nullptr;
      }
      CondV = Builder->CreateICmpNE(CondV, llvm::ConstantInt::get(CondV->getType(), 0), "whilecond");
      setDebugLocationFor(this);
      Builder->CreateCondBr(CondV, bodyBB, endBB);

      // body
      function->insert(function->end(), bodyBB);
      Builder->SetInsertPoint(bodyBB);
      if (_body && !_body->codegen()) {
         LoopTargets.pop_back();
         ClassScopeCleanups.pop_back();
         return nullptr;
      }
      if (!Builder->GetInsertBlock()->getTerminator())
         Builder->CreateBr(condBB);

      // end of while
      function->insert(function->end(), endBB);
      Builder->SetInsertPoint(endBB);

      LoopTargets.pop_back();
      ClassScopeCleanups.pop_back();

      return llvm::Constant::getNullValue(llvm::Type::getInt32Ty(*TheContext));
   }

   llvm::Value* DoWhileStatementAST::codegen() {
      setDebugLocationFor(this);

      llvm::Function* function = Builder->GetInsertBlock()->getParent();

      llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*TheContext, "dowhile.body", function);
      llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*TheContext, "dowhile.cond");
      llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*TheContext, "dowhile.end");

      ClassScopeCleanups.push_back({});
      LoopTargets.push_back({endBB, condBB, ClassScopeCleanups.size() - 1, ClassScopeCleanups.size()});

      Builder->CreateBr(bodyBB);

      // body
      Builder->SetInsertPoint(bodyBB);
      if (_body && !_body->codegen()) {
         LoopTargets.pop_back();
         ClassScopeCleanups.pop_back();
         return nullptr;
      }
      if (!Builder->GetInsertBlock()->getTerminator())
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
      setDebugLocationFor(this);
      Builder->CreateCondBr(CondV, bodyBB, endBB);

      // end
      function->insert(function->end(), endBB);
      Builder->SetInsertPoint(endBB);

      LoopTargets.pop_back();
      ClassScopeCleanups.pop_back();

      return llvm::Constant::getNullValue(llvm::Type::getInt32Ty(*TheContext));
   }

   llvm::Value* ForStatementAST::codegen() {
      setDebugLocationFor(this);

      llvm::Function* function = Builder->GetInsertBlock()->getParent();

      llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*TheContext, "for.cond");
      llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*TheContext, "for.body");
      llvm::BasicBlock* incBB = llvm::BasicBlock::Create(*TheContext, "for.inc");
      llvm::BasicBlock* endBB = llvm::BasicBlock::Create(*TheContext, "for.end");

      ClassScopeCleanups.push_back({});
      LoopTargets.push_back({endBB, incBB, ClassScopeCleanups.size() - 1, ClassScopeCleanups.size()});

      // init
      if (_init && !_init->codegen()) {
         LoopTargets.pop_back();
         ClassScopeCleanups.pop_back();
         return nullptr;
      }

      Builder->CreateBr(condBB);

      // condition
      function->insert(function->end(), condBB);
      Builder->SetInsertPoint(condBB);
      llvm::Value* CondV = _condition ? _condition->codegen() : nullptr;
      if (_condition) {
         if (!CondV) {
            LoopTargets.pop_back();
            ClassScopeCleanups.pop_back();
            return nullptr;
         }
         CondV = Builder->CreateICmpNE(
             CondV,
             llvm::ConstantInt::get(CondV->getType(), 0),
             "forcond");
         setDebugLocationFor(this);
         Builder->CreateCondBr(CondV, bodyBB, endBB);
      } else
         Builder->CreateBr(bodyBB);


      // body
      function->insert(function->end(), bodyBB);
      Builder->SetInsertPoint(bodyBB);
      if (_body && !_body->codegen()) {
         LoopTargets.pop_back();
         ClassScopeCleanups.pop_back();
         return nullptr;
      }
      if (!Builder->GetInsertBlock()->getTerminator())
         Builder->CreateBr(incBB);

      // increment
      function->insert(function->end(), incBB);
      Builder->SetInsertPoint(incBB);
      if (_increment && !_increment->codegen()) {
         LoopTargets.pop_back();
         ClassScopeCleanups.pop_back();
         return nullptr;
      }
      if (!Builder->GetInsertBlock()->getTerminator())
         Builder->CreateBr(condBB);

      // end
      function->insert(function->end(), endBB);
      Builder->SetInsertPoint(endBB);

      LoopTargets.pop_back();
      ClassScopeCleanups.pop_back();

      return llvm::Constant::getNullValue(llvm::Type::getInt32Ty(*TheContext));
   }

   llvm::Function* FunctionPrototypeAST::codegen() {
      std::string name{};
      if (_name == "main"sv || _isExternC)
         name = _name;
      else
         name = mangledName();

      if (auto* existing = TheModule->getFunction(name))
         return existing;

      std::vector<llvm::Type*> paramTypes{};
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

      if (!function)
         return nullptr;

      if (!function->getSubprogram())
         createFunctionDebugInfo(function, _prototype.get(), _body.get());
      else
         KSDbgInfo.CurrentSubprogram = function->getSubprogram();

      llvm::BasicBlock* BB = llvm::BasicBlock::Create(*TheContext, "entry", function);
      Builder->SetInsertPoint(BB);
      if (KSDbgInfo.CurrentSubprogram)
         Builder->SetCurrentDebugLocation(llvm::DILocation::get(*TheContext,
                                                                functionDebugLine(_prototype.get(), _body.get()),
                                                                functionDebugColumn(_prototype.get(), _body.get()),
                                                                KSDbgInfo.CurrentSubprogram));

      KSDbgInfo.pushVariableScope();

      llvm::IRBuilder tmpBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());

      size_t idx = 0;
      bool addedThis{false};
      for (auto& arg : function->args()) {
         if (!addedThis && function->getName().find("-") != std::string::npos && !function->getName().ends_with("-constructor") && !function->getName().ends_with("-destructor")) { // if class method
            NamedValues["this"] = &function->args().begin()[0];
            VariableTypes["this"] = Builder->getPtrTy();
            addedThis = true;
            // continue;
         }
         auto* varDecl = static_cast<VariableDeclarationAST*>(_prototype->args()[idx].get());
         if (varDecl->type()->kind() == TypeKind::Reference) {
            NamedValues[varDecl->name()] = &arg;
            VariableTypes[varDecl->name()] = static_cast<ReferenceTy*>(varDecl->type().get())->reference()->llvmType();
         } else if (varDecl->type()->kind() == TypeKind::Class) {
            llvm::AllocaInst* paramAddr = tmpBuilder.CreateAlloca(arg.getType(), nullptr, varDecl->name());
            std::string className = varDecl->type()->string();
            std::string copyCtorName = className + "-constructor$" + className + "*$" + className + "&";
            llvm::Function* copyCtor = TheModule->getFunction(copyCtorName);
            if (!copyCtor)
               return LogErrorF("copy constructor not found for class '" + className + "'");
            llvm::AllocaInst* incomingPtr = tmpBuilder.CreateAlloca(arg.getType());
            tmpBuilder.CreateStore(&arg, incomingPtr);
            Builder->CreateCall(copyCtor, {paramAddr, incomingPtr});
            NamedValues[varDecl->name()] = paramAddr;
            VariableTypes[varDecl->name()] = arg.getType();
            if (DBuilder && KSDbgInfo.CurrentSubprogram && KSDbgInfo.TheFile) {
               const uint32_t paramLine = debugLineFor(varDecl);
               const uint32_t paramColumn = debugColumnFor(varDecl);
               auto* paramVar = DBuilder->createParameterVariable(
                   KSDbgInfo.CurrentSubprogram,
                   varDecl->name(),
                   static_cast<unsigned>(idx + 1),
                   KSDbgInfo.TheFile,
                   paramLine,
                   debugTypeFor(varDecl->type()),
                   true);
               KSDbgInfo.registerLocalVariable(varDecl->name(), paramVar);
               DBuilder->insertDeclare(paramAddr,
                                       paramVar,
                                       DBuilder->createExpression(),
                                       llvm::DILocation::get(*TheContext, paramLine, paramColumn, KSDbgInfo.CurrentSubprogram),
                                       BB);
            }
         } else {
            llvm::AllocaInst* paramAddr = tmpBuilder.CreateAlloca(arg.getType(), nullptr, varDecl->name());
            tmpBuilder.CreateStore(&arg, paramAddr);
            NamedValues[varDecl->name()] = paramAddr;
            VariableTypes[varDecl->name()] = arg.getType();
            if (DBuilder && KSDbgInfo.CurrentSubprogram && KSDbgInfo.TheFile) {
               const uint32_t paramLine = debugLineFor(varDecl);
               const uint32_t paramColumn = debugColumnFor(varDecl);
               auto* paramVar = DBuilder->createParameterVariable(
                   KSDbgInfo.CurrentSubprogram,
                   varDecl->name(),
                   static_cast<unsigned>(idx + 1),
                   KSDbgInfo.TheFile,
                   paramLine,
                   debugTypeFor(varDecl->type()),
                   true);
               KSDbgInfo.registerLocalVariable(varDecl->name(), paramVar);
               DBuilder->insertDeclare(paramAddr,
                                       paramVar,
                                       DBuilder->createExpression(),
                                       llvm::DILocation::get(*TheContext, paramLine, paramColumn, KSDbgInfo.CurrentSubprogram),
                                       BB);
            }
         }
         ++idx;
      }

      if (!_body || !_body->codegen()) {
         KSDbgInfo.popVariableScope();
         function->eraseFromParent();
         // return LogErrorF("error in function: '" + name() + "'");
         return nullptr;
      }

      if (Builder->GetInsertBlock()->getTerminator() == nullptr) {
         if (_prototype->type()->string() == "void"sv)
            Builder->CreateRetVoid();
         else if (_prototype->name() == "main"sv)
            Builder->CreateRet(Builder->getInt32(0)); // returning 0 by default in main function
         else {
            const std::string funcName = name();
            const bool isConstructor = funcName.ends_with("-constructor");
            const bool isDestructor = funcName.ends_with("-destructor");

            if (isConstructor || isDestructor) {
               Builder->CreateRet(llvm::Constant::getNullValue(Builder->getVoidTy()));
            } else {
               KSDbgInfo.popVariableScope();
               return LogErrorF("missing return statement in function: '" + name() + "'");
            }
         }
      }

      KSDbgInfo.CurrentSubprogram = nullptr;
      KSDbgInfo.popVariableScope();

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

      // determine if it's an array or a single class instance
      auto allocType = static_cast<PointerTy*>(_Type.get())->pointee();
      bool isArray = allocType->kind() == TypeKind::Array;
      llvm::Value* ptr = nullptr;
      uint64_t count = 1;
      if (isArray) {
         auto arrTy = static_cast<ArrayTy*>(allocType.get());
         if (arrTy->sizeExpr() && arrTy->sizeExpr()->astType() == ASTType::NumberExpression) {
            auto* num = static_cast<NumberExpressionAST*>(arrTy->sizeExpr().get());
            const Number rawSize = num->value();
            count = std::holds_alternative<double>(rawSize)
                ? static_cast<uint64_t>(std::get<double>(rawSize))
                : std::get<uint64_t>(rawSize);
         }
         auto elemSize = arrTy->arrayType()->size();
         auto size = Builder->getInt64(elemSize * count);
         setDebugLocationFor(this);
         ptr = Builder->CreateCall(mallocFun, {size});
      } else {
         auto size = Builder->getInt64(allocType->size());
         setDebugLocationFor(this);
         ptr = Builder->CreateCall(mallocFun, {size});
      }
      return ptr;
   }
   llvm::Value* FreeStatementAST::codegen() {
      setDebugLocationFor(this);

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

      // determine type
      auto type = _variable->type();
      bool isArray = false;
      std::shared_ptr<Type> elemType;
      uint64_t count = 1;
      if (type->kind() == TypeKind::Pointer) {
         auto pt = static_cast<PointerTy*>(type.get())->pointee();
         if (pt->kind() == TypeKind::Array) {
            isArray = true;
            auto arrTy = static_cast<ArrayTy*>(pt.get());
            elemType = arrTy->arrayType();
            if (arrTy->sizeExpr() && arrTy->sizeExpr()->astType() == ASTType::NumberExpression) {
               auto* num = static_cast<NumberExpressionAST*>(arrTy->sizeExpr().get());
               const Number rawSize = num->value();
               count = std::holds_alternative<double>(rawSize)
                   ? static_cast<uint64_t>(std::get<double>(rawSize))
                   : std::get<uint64_t>(rawSize);
            }
         } else {
            elemType = pt;
         }
      } else {
         elemType = type;
      }

      // call destructor
      if (elemType && elemType->kind() == TypeKind::Class) {
         const std::string dtorName = static_cast<ClassTy*>(elemType.get())->string() + "-destructor$";
         llvm::Function* dtor = TheModule->getFunction(dtorName);
         if (dtor) {
            if (isArray) {
               // destructors in reverse order
               for (int64_t i = static_cast<int64_t>(count) - 1; i >= 0; --i) {
                  llvm::Value* gep = Builder->CreateGEP(
                      elemType->llvmType(), pointerToFree, Builder->getInt64(i));
                  Builder->CreateCall(dtor, {gep});
               }
            } else {
               Builder->CreateCall(dtor, {pointerToFree});
            }
         }
      }

      setDebugLocationFor(this);
      auto call = Builder->CreateCall(freeFun, {pointerToFree});

      // Ensure later loads observe null in the same lvalue used by `free`.
      if (nullStoreTarget && nullStoreTarget->getType()->isPointerTy()) {
         setDebugLocationFor(this);
         Builder->CreateStore(llvm::ConstantPointerNull::get(Builder->getPtrTy()), nullStoreTarget);
      }

      return call;
   }

   llvm::Value* ClassMethodAST::codegen() {
      return _method->codegen();
   }
   llvm::Value* ClassVariableAST::codegen() {
      return _variable->codegen();
   }
   llvm::Value* ClassConstructorAST::codegen() {
      return _constructor->codegen();
   }
   llvm::Value* ClassDestructorAST::codegen() {
      return _destructor->codegen();
   }
   llvm::Value* ClassAST::codegen() {
      std::vector<llvm::Type*> paramTypes;
      std::vector<llvm::Metadata*> debugMembers;
      for (const auto& var : _members) {
         if (!var || !var->variable() || var->variable()->astType() != ASTType::VariableDeclaration)
            continue;

         auto* varDecl = static_cast<VariableDeclarationAST*>(var->variable().get());
         paramTypes.push_back(varDecl->type()->llvmType());
      }
      llvm::StructType* str = llvm::StructType::getTypeByName(*TheContext, _name);
      str->setBody(paramTypes);

      const llvm::StructLayout* layout = TheModule->getDataLayout().getStructLayout(str);
      size_t fieldIndex = 0;
      for (const auto& var : _members) {
         if (!var || !var->variable() || var->variable()->astType() != ASTType::VariableDeclaration)
            continue;

         auto flags = llvm::DINode::FlagZero;
         switch (var->visibility()) {
            case Visibility::Private:
               flags = llvm::DINode::FlagPrivate;
               break;
            case Visibility::Public:
               flags = llvm::DINode::FlagPublic;
               break;
            case Visibility::Protected:
               flags = llvm::DINode::FlagProtected;
               break;
         }

         auto* varDecl = static_cast<VariableDeclarationAST*>(var->variable().get());
         llvm::Type* fieldType = varDecl->type()->llvmType();
         const uint64_t fieldOffsetBits = layout->getElementOffsetInBits(static_cast<unsigned>(fieldIndex));
         debugMembers.push_back(DBuilder->createMemberType(KSDbgInfo.currentScope(),
                                                           varDecl->name(),
                                                           KSDbgInfo.TheFile,
                                                           static_cast<unsigned>(varDecl->sourceLine()),
                                                           debugTypeSizeBits(fieldType),
                                                           debugTypeAlignBits(fieldType),
                                                           fieldOffsetBits,
                                                           flags,
                                                           debugTypeFor(varDecl->type())));
         ++fieldIndex;
      }

      auto* classType = DBuilder->createClassType(KSDbgInfo.currentScope(),
                                                  _name,
                                                  KSDbgInfo.TheFile,
                                                  _sourceLine,
                                                  debugTypeSizeBits(str),
                                                  debugTypeAlignBits(str),
                                                  0,
                                                  llvm::DINode::FlagZero,
                                                  nullptr,
                                                  DBuilder->getOrCreateArray(debugMembers));
      DBuilder->retainType(classType);

      for (auto& method : _methods) {
         if (!method)
            return LogErrorV("invalid method in class '" + _name + "'");
         method->codegen();
      }

      for (auto& ctor : _constructors) {
         if (!ctor)
            return LogErrorV("invalid constructor in class '" + _name + "'");
         ctor->codegen();
      }
      if (!_destructor)
         return LogErrorV("missing destructor in class '" + _name + "'");
      _destructor->codegen();

      return nullptr;
   }
   llvm::StructType* forwardDeclareClass(std::unique_ptr<ClassAST>& cls) {
      return llvm::StructType::create(*TheContext, cls->name());
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
