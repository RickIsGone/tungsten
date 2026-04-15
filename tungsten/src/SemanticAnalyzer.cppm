module;

#include <format>
#include <algorithm>
#include <memory>
#include <vector>
#include <iostream>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <sstream>
#include <cstdint>
#include <cmath>
#include <unordered_set>
#include <variant>
#ifndef _NODISCARD
#define _NODISCARD [[nodiscard]]
#endif


export module Tungsten.semanticAnalyzer;
import Tungsten.token;
import Tungsten.ast;
import Tungsten.parser;
import Tungsten.utils;

using namespace std::literals;
namespace fs = std::filesystem;

namespace tungsten {
   using Scope = std::unordered_map<std::string, std::pair<bool, std::shared_ptr<Type>>>; // name, {isConst, type}
   struct Overload {
      std::shared_ptr<Type> type;
      std::vector<std::shared_ptr<Type>> args;
   };
   constexpr size_t GlobalScope = 0;

   export class SemanticAnalyzer : public ASTVisitor {
   public:
      SemanticAnalyzer(std::vector<std::unique_ptr<FunctionAST>>& functions,
                       std::vector<std::unique_ptr<ClassAST>>& classes,
                       std::vector<std::unique_ptr<ExpressionAST>>& globVars,
                       std::unique_ptr<Externs>& externs,
                       const fs::path& filePath,
                       const std::string& raw)
          : _functions{functions}, _classes{classes}, _globalVariables{globVars}, _externs{externs}, _filePath{filePath}, _raw{raw} { _scopes.push_back({}); }
      SemanticAnalyzer(const SemanticAnalyzer&) = delete;
      SemanticAnalyzer& operator=(const SemanticAnalyzer&) = delete;

      bool analyze();

      void visit(NumberExpressionAST&) override;
      void visit(VariableExpressionAST&) override;
      void visit(IndexAccessAST&) override;
      void visit(StringExpression&) override {}
      void visit(UnaryExpressionAST&) override;
      void visit(BinaryExpressionAST&) override;
      void visit(VariableDeclarationAST&) override;
      void visit(CallExpressionAST&) override;
      void visit(TypeOfStatementAST&) override;
      void visit(NameOfStatementAST&) override;
      void visit(SizeOfStatementAST&) override;
      void visit(__BuiltinFunctionAST&) override {}
      void visit(__BuiltinLineAST&) override {}
      void visit(__BuiltinColumnAST&) override {}
      void visit(__BuiltinFileAST&) override {}
      void visit(StaticCastAST&) override;
      void visit(ConstCastAST&) override {}
      void visit(NullPtrAST&) override {}
      void visit(IfStatementAST&) override;
      void visit(WhileStatementAST&) override;
      void visit(DoWhileStatementAST&) override;
      void visit(ForStatementAST&) override;
      void visit(BlockStatementAST&) override;
      void visit(ReturnStatementAST&) override;
      void visit(ExitStatement&) override;
      void visit(BreakStatementAST&) override;
      void visit(ContinueStatementAST&) override;
      void visit(ExternVariableStatementAST&) override {}
      void visit(ImportStatementAST&) override {}
      void visit(NewStatementAST&) override;
      void visit(FreeStatementAST&) override;
      void visit(FunctionPrototypeAST&) override;
      void visit(FunctionAST&) override;

      void visit(ClassMethodAST&) override;
      void visit(ClassVariableAST&) override;
      void visit(ClassConstructorAST&) override;
      void visit(ClassDestructorAST&) override;
      void visit(ClassAST&) override;

   private:
      template <typename... Ty>
      void _logError(const ExpressionAST* node, const std::string& err, Ty&&... args);
      template <typename... Ty>
      void _logError(const FunctionPrototypeAST* node, const std::string& err, Ty&&... args);
      template <typename... Ty>
      void _logError(const std::string& err, Ty&&... args) {
         _logError(static_cast<const ExpressionAST*>(nullptr), err, std::forward<Ty>(args)...);
      }
      template <typename... Ty>
      void _logWarn(const std::string& warn, Ty&&... args) {
         _logWarn(static_cast<const ExpressionAST*>(nullptr), warn, std::forward<Ty>(args)...);
      }
      template <typename... Ty>
      void _logWarn(const ExpressionAST* node, const std::string& warn, Ty&&... args);

      _NODISCARD size_t _line(size_t position) const;
      _NODISCARD size_t _column(size_t position) const;

      bool _isSignedType(const std::string& type);
      bool _isUnsignedType(const std::string& type);
      bool _isFloatingPointType(const std::string& type);
      bool _isNumberType(const std::string& type);
      bool _isIntegerType(const std::string& type);
      bool _isBaseType(const std::string& type);
      bool _isClass(const std::string& cls);
      bool _isFunction(const std::string& fn);
      bool _isNamespace(const std::string& ns);
      bool _isQualifiedName(const std::string& name) const;
      bool _hasValidQualifiedPrefix(const std::string& name, std::string& prefixKind, std::string& failingPrefix, bool& missingScope);
      std::string _symbolKindForName(const std::string& name);
      bool _checkNumericConversionLoss(const std::string& fromType, const std::string& toType);
      bool _doesVariableExist(const std::string& var);
      bool _isConstVariable(const std::string& var) const;
      std::optional<std::string> _constWriteTargetName(ExpressionAST* expr) const;
      bool _isConstexpr(const std::unique_ptr<ExpressionAST>& expr);
      _NODISCARD bool _statementTerminates(ExpressionAST* expr) const;
      bool _canCast(std::shared_ptr<Type> from, std::shared_ptr<Type> to);
      std::unique_ptr<ExpressionAST> _evaluateConstexpr(const std::unique_ptr<ExpressionAST>& expr);
      _NODISCARD std::shared_ptr<Type> _variableType(const std::string& var) const;

      std::vector<std::unique_ptr<FunctionAST>>& _functions;
      std::vector<std::unique_ptr<ClassAST>>& _classes;
      std::vector<std::unique_ptr<ExpressionAST>>& _globalVariables;
      std::unique_ptr<Externs>& _externs;
      const fs::path& _filePath;
      const std::string& _raw;

      std::vector<Scope> _scopes{};
      std::unordered_map<std::string, std::vector<Overload>> _declaredFunctions{};
      std::unordered_set<std::string> _declaredClasses{};
      size_t _currentScope{GlobalScope};
      size_t _loopDepth{0};
      bool _hasErrors{false};
      bool _allowUninitializedReferenceDecl{false};
   };

   //  ========================================== implementation ==========================================

   bool SemanticAnalyzer::analyze() {
      _declaredFunctions["shell"] = {{makeInt32(), std::vector{makeString()}}};
      _declaredFunctions["print"] = {{makeVoid(), std::vector{makeString(), makeArgPack()}}};
      _declaredFunctions["input"] = {{makeVoid(), std::vector{makeString(), makeArgPack()}}};
      for (auto& var : _externs->variables) {
         if (var)
            var->accept(*this);
         else
            _hasErrors = true;
      }
      for (auto& fun : _externs->functions) {
         _scopes.push_back({}); // creating a new scope for the argument declarations
         ++_currentScope;
         if (fun)
            fun->accept(*this);
         else
            _hasErrors = true;
         _scopes.pop_back();
         --_currentScope;
      }
      for (auto& var : _globalVariables) {
         if (var)
            var->accept(*this);
         else
            _hasErrors = true;
      }
      for (auto& cls : _classes) {
         if (cls)
            cls->accept(*this);
         else
            _hasErrors = true;
      }
      for (auto& fun : _functions) {
         if (fun)
            fun->accept(*this);
         else
            _hasErrors = true;
      }

      return !_hasErrors;
   }

   void SemanticAnalyzer::visit(VariableDeclarationAST& var) {
      if (_isQualifiedName(var.name())) {
         std::string prefixKind;
         std::string failingPrefix;
         bool missingScope = false;
         if (!_hasValidQualifiedPrefix(var.name(), prefixKind, failingPrefix, missingScope)) {
            if (missingScope)
               return _logError(&var, "unknown scope '{}' in declaration of '{}'", failingPrefix, var.name());
            return _logError(&var, "'{}' is a {}, not a namespace/class scope for declaration '{}'", failingPrefix, prefixKind, var.name());
         }
      }

      const std::string existingKind = _symbolKindForName(var.name());
      if (!existingKind.empty())
         return _logError(&var, "{} with name '{}' already exists", existingKind, var.name());

      _scopes.at(_currentScope)[var.name()] = {var.isConst(), var.type()}; // pushing the variable, so even if there are errors, the variable is already declared to avoid cascade errors like "unknown variable 'x'" every fucking time is mentioned cause im tired of that bs

      switch (var.type()->kind()) {
         case TypeKind::Array: {
            auto array = static_cast<ArrayTy*>(var.type().get());
            if (!_isBaseType(baseType(array->arrayType())) && !_isClass(baseType(array->arrayType())))
               return _logError(&var, "unknown type '{}' in variable declaration '{} {}'", baseType(array->arrayType()), fullTypeString(var.type()), var.name());

            // Validate/fold every dimension: arr[a][b][c]
            std::shared_ptr<Type> current = var.type();
            while (current && current->kind() == TypeKind::Array) {
               auto* dim = static_cast<ArrayTy*>(current.get());
               if (!dim->sizeExpr())
                  return _logError(&var, "array size must be specified in variable declaration '{} {}'", fullTypeString(var.type()), var.name());
               dim->sizeExpr()->accept(*this);
               if (!dim->sizeExpr()->type())
                  return _logError(&var, "array size must be an integer type in variable declaration '{} {}'", fullTypeString(var.type()), var.name());
               if (!_isConstexpr(dim->sizeExpr()))
                  return _logError(&var, "array size must be a compile-time constant in variable declaration '{} {}'", fullTypeString(var.type()), var.name());

               auto foldedSizeExpr = _evaluateConstexpr(dim->sizeExpr());
               if (!foldedSizeExpr) {
                  _hasErrors = true;
                  return; // error already logged
               }

               if (foldedSizeExpr->astType() != ASTType::NumberExpression)
                  return _logError(&var, "array size must be a numeric value in variable declaration '{} {}'", fullTypeString(var.type()), var.name());

               auto* foldedNum = static_cast<NumberExpressionAST*>(foldedSizeExpr.get());
               const Number foldedSize = foldedNum->value();
               const double sizeAsDouble = std::holds_alternative<double>(foldedSize)
                   ? std::get<double>(foldedSize)
                   : static_cast<double>(std::get<uint64_t>(foldedSize));
               if (!std::isfinite(sizeAsDouble) || std::floor(sizeAsDouble) != sizeAsDouble || sizeAsDouble < 1.0)
                  return _logError(&var, "array size must be a positive integer in variable declaration '{} {}'", fullTypeString(var.type()), var.name());

               dim->setSize(std::move(foldedSizeExpr));
               static_cast<NumberExpressionAST*>(dim->sizeExpr().get())->setType(makeUint32());
               current = dim->arrayType();
            }
         } break;
         case TypeKind::Pointer: {
            auto ptr = static_cast<PointerTy*>(var.type().get());
            if (!_isBaseType(baseType(ptr->pointee())) && !_isClass(baseType(ptr->pointee())))
               return _logError(&var, "unknown type '{}' in variable declaration '{} {}'", baseType(ptr->pointee()), fullTypeString(var.type()), var.name());
         } break;
         case TypeKind::Reference: {
            auto ref = static_cast<ReferenceTy*>(var.type().get());
            if (!_isBaseType(baseType(ref->reference())) && !_isClass(baseType(ref->reference())))
               return _logError(&var, "unknown type '{}' in variable declaration '{} {}'", baseType(ref->reference()), fullTypeString(var.type()), var.name());
         } break;
         default:
            if (!_isBaseType(var.type()->string()) && !_isClass(var.type()->string()))
               return _logError(&var, "unknown type '{}' in variable declaration '{} {}'", var.type()->string(), fullTypeString(var.type()), var.name());
            break;
      }

      if (var.initializer()) {
         var.initializer()->accept(*this);

         if (var.initializer()->astType() == ASTType::NumberExpression && _isNumberType(var.type()->string()))
            static_cast<NumberExpressionAST*>(var.initializer().get())->setType(var.type());

         if (var.type()->kind() == TypeKind::Reference) {
            auto* refType = static_cast<ReferenceTy*>(var.type().get());
            auto* initType = var.initializer()->type().get();
            const std::string expected = fullTypeString(refType->reference());

            bool compatible = false;
            if (initType && initType->kind() == TypeKind::Pointer) {
               auto* initPtr = static_cast<PointerTy*>(initType);
               compatible = fullTypeString(initPtr->pointee()) == expected;
            } else if (initType && initType->kind() == TypeKind::Reference) {
               auto* initRef = static_cast<ReferenceTy*>(initType);
               compatible = fullTypeString(initRef->reference()) == expected;
            }

            if (!compatible)
               return _logError(&var, "type mismatch: cannot bind '{}' with non-reference initializer of type '{}'", fullTypeString(var.type()), fullTypeString(var.initializer()->type()));
         } else if (!_isNumberType(var.type()->string()) || !_isNumberType(var.initializer()->type()->string())) {
            if (fullTypeString(var.type()) != fullTypeString(var.initializer()->type())) {
               if (var.initializer()->astType() == ASTType::NullPtr && var.type()->kind() == TypeKind::Pointer)
                  return;

               if (var.type()->kind() == TypeKind::Pointer) {
                  if (var.initializer()->type()->kind() != TypeKind::Pointer)
                     return _logError(&var, "type mismatch: cannot assign '{}' to variable of type '{}'", fullTypeString(var.initializer()->type()), fullTypeString(var.type()));

                  auto* lhsPtr = static_cast<PointerTy*>(var.type().get());
                  auto* rhsPtr = static_cast<PointerTy*>(var.initializer()->type().get());

                  // array decay: int[3]* → int* is allowed
                  if (rhsPtr->pointee()->kind() == TypeKind::Array) {
                     auto* rhsArray = static_cast<ArrayTy*>(rhsPtr->pointee().get());
                     if (fullTypeString(lhsPtr->pointee()) == fullTypeString(rhsArray->arrayType()))
                        return;
                  }

                  // multidimensional array decay: int[3][6]* → int** is allowed
                  if (rhsPtr->pointee()->kind() == TypeKind::Array) {
                     std::shared_ptr<Type> rhsBase = rhsPtr->pointee();
                     int arrayDepth = 0;
                     while (rhsBase && rhsBase->kind() == TypeKind::Array) {
                        auto* arrTy = static_cast<ArrayTy*>(rhsBase.get());
                        rhsBase = arrTy->arrayType();
                        arrayDepth++;
                     }

                     // count pointer levels on LHS
                     std::shared_ptr<Type> lhsBase = lhsPtr->pointee();
                     int ptrDepth = 1; // already one level from outer pointer
                     while (lhsBase && lhsBase->kind() == TypeKind::Pointer) {
                        auto* ptrTy = static_cast<PointerTy*>(lhsBase.get());
                        lhsBase = ptrTy->pointee();
                        ptrDepth++;
                     }

                     // if LHS has enough pointer levels and base types match, allow decay
                     if (ptrDepth == arrayDepth && fullTypeString(lhsBase) == fullTypeString(rhsBase))
                        return;
                  }

                  if (fullTypeString(lhsPtr->pointee()) != fullTypeString(rhsPtr->pointee()))
                     return _logError(&var, "type mismatch: cannot assign '{}' to variable of type '{}'", fullTypeString(var.initializer()->type()), fullTypeString(var.type()));
                  return;
               }

               if (var.initializer()->astType() != ASTType::NullPtr)
                  return _logError(&var, "type mismatch: cannot assign '{}' to variable of type '{}'", fullTypeString(var.initializer()->type()), fullTypeString(var.type()));
            }
         }
      } else if (var.type()->kind() == TypeKind::Reference && !_allowUninitializedReferenceDecl) {
         return _logError(&var, "reference variable '{}' must be initialized", var.name());
      }
   }

   void SemanticAnalyzer::visit(NameOfStatementAST& var) {
      var.statement()->accept(*this);

      switch (var.statement()->astType()) {
         case ASTType::VariableExpression:
         case ASTType::IndexAccessExpression:
         case ASTType::CallExpression:
            break;
         case ASTType::UnaryExpression:
            if (static_cast<UnaryExpressionAST*>(var.statement().get())->op() == "*"sv)
               break;
            [[fallthrough]];
         case ASTType::StaticCast:
         case ASTType::ConstCast:
            break;
         default:
            return _logError(&var, "cannot use 'nameof' on expression without a name");
      }
   }
   void SemanticAnalyzer::visit(SizeOfStatementAST& var) {
      if (var.hasExplicitType()) {
         if (!var.explicitType())
            return _logError(&var, "cannot use 'sizeof' on invalid type");
         var.setType(makeUint64());
         return;
      }

      if (!var.statement())
         return _logError(&var, "cannot use 'sizeof' without a valid argument");

      if (var.statement()->astType() == ASTType::VariableExpression) {
         auto* variable = static_cast<VariableExpressionAST*>(var.statement());
         if (_isClass(variable->name())) {
            variable->setType(makeClass(variable->name()));
            var.setType(makeUint64());
            return;
         }
      }

      var.statement()->accept(*this);
      if (!var.statement()->type())
         return _logError(&var, "cannot use 'sizeof' on expression without a type");
      var.setType(makeUint64());
   }
   void SemanticAnalyzer::visit(StaticCastAST& cast) {
      cast.value()->accept(*this);
      if (!cast.value()->type())
         return; // error already logged
      if (!_canCast(cast.value()->type(), cast.type()))
         return _logError(&cast, "cannot cast from type '{}' to '{}'", fullTypeString(cast.value()->type()), fullTypeString(cast.type()));
   }
   void SemanticAnalyzer::visit(TypeOfStatementAST& var) {
      if (!var.statement())
         return _logError(&var, "cannot use 'typeof' without a valid argument");

      if (var.statement()->astType() == ASTType::VariableExpression) {
         auto* variable = static_cast<VariableExpressionAST*>(var.statement().get());
         if (_isClass(variable->name())) {
            variable->setType(makeClass(variable->name()));
            return;
         }
      }

      var.statement()->accept(*this);
      if (!var.statement()->type())
         return _logError(&var, "cannot use 'typeof' on expression without a type");
   }

   void SemanticAnalyzer::visit(NumberExpressionAST& num) {
      if (!num.type()) {
         if (std::holds_alternative<uint64_t>(num.value()))
            num.setType(makeInt64());
         else
            num.setType(makeDouble());
      }
   }

   void SemanticAnalyzer::visit(UnaryExpressionAST& op) {
      op.operand()->accept(*this);

      if (op.op() == "--"sv || op.op() == "++"sv) {
         bool validIncrementTarget = op.operand()->astType() == ASTType::VariableExpression ||
             op.operand()->astType() == ASTType::IndexAccessExpression ||
             (op.operand()->astType() == ASTType::UnaryExpression && static_cast<UnaryExpressionAST*>(op.operand().get())->op() == "*"sv);
         if (!validIncrementTarget)
            return _logError(&op, "cannot use operator '{}' on non-variable expression", op.op());
         if (auto constName = _constWriteTargetName(op.operand().get()); constName.has_value())
            return _logError(&op, "cannot modify const variable '{}'", *constName);
         op.setType(op.operand()->type());
      }
      if (op.op() == "-"sv) {
         if (op.operand()->astType() != ASTType::NumberExpression)
            return _logError(&op, "cannot use operator '-' on non-numeric expression");
         op.setType(op.operand()->type());
      }
      if (op.op() == "!"sv) {
         op.setType(makeBool());
      }
      if (op.op() == "&"sv) {
         if (!op.operand()->isLValue())
            return _logError(&op, "cannot use operator '{}' on non-lvalue expression", op.op());
         op.setType(makePointer(op.operand()->type()));
      }
      if (op.op() == "*"sv) {
         if (op.operand()->type()->kind() != TypeKind::Pointer)
            return _logError(&op, "cannot dereference non-pointer type '{}'", fullTypeString(op.operand()->type()));
         op.setType(static_cast<PointerTy*>(op.operand()->type().get())->pointee());
      }
   }

   void SemanticAnalyzer::visit(BinaryExpressionAST& binop) {
      binop.LHS()->accept(*this);
      binop.RHS()->accept(*this);
      if (binop.op() == "=="sv || binop.op() == "!="sv || binop.op() == "<"sv || binop.op() == ">"sv || binop.op() == "<="sv || binop.op() == ">="sv) {
         const bool lhsIsPointer = binop.LHS()->type()->kind() == TypeKind::Pointer;
         const bool rhsIsPointer = binop.RHS()->type()->kind() == TypeKind::Pointer;
         const bool lhsIsNullPtr = binop.LHS()->astType() == ASTType::NullPtr || binop.LHS()->type()->kind() == TypeKind::Null;
         const bool rhsIsNullPtr = binop.RHS()->astType() == ASTType::NullPtr || binop.RHS()->type()->kind() == TypeKind::Null;

         if ((lhsIsPointer && rhsIsNullPtr) || (rhsIsPointer && lhsIsNullPtr)) {
            binop.setType(makeBool());
            return;
         }

         if (!_isNumberType(fullTypeString(binop.LHS()->type())) || !_isNumberType(fullTypeString(binop.RHS()->type()))) {
            if (fullTypeString(binop.LHS()->type()) != fullTypeString(binop.RHS()->type()))
               return _logError(&binop, "type mismatch: cannot compare '{}' and '{}' with operator '{}'", fullTypeString(binop.LHS()->type()), fullTypeString(binop.RHS()->type()), binop.op());
            if (fullTypeString(binop.LHS()->type()) == "char*"sv)
               return _logError(&binop, "cannot do comparison between strings with operator '{}'", binop.op());
         }
         binop.setType(makeBool());
         return;
      }

      if (binop.op() == "&&"sv || binop.op() == "||"sv) {
         if (binop.LHS()->type()->kind() != TypeKind::Bool || binop.RHS()->type()->kind() != TypeKind::Bool)
            return _logError(&binop, "type mismatch: cannot compare '{}' and '{}' with operator '{}'", fullTypeString(binop.LHS()->type()), fullTypeString(binop.RHS()->type()), binop.op());
         binop.setType(makeBool());
         return;
      }

      if (binop.op() == "="sv || binop.op() == "+="sv || binop.op() == "-="sv || binop.op() == "*="sv || binop.op() == "/="sv || binop.op() == "%="sv || binop.op() == "|="sv || binop.op() == "&="sv || binop.op() == "^="sv || binop.op() == "<<="sv || binop.op() == ">>="sv) {
         if (!binop.LHS()->isLValue())
            return _logError(&binop, "left side of assignment must be a variable");
         if (auto constName = _constWriteTargetName(binop.LHS().get()); constName.has_value())
            return _logError(&binop, "cannot modify const variable '{}'", *constName);
      }

      // addition, multiplication, division, ecc.
      if (!_isNumberType(fullTypeString(binop.LHS()->type())) || !_isNumberType(fullTypeString(binop.RHS()->type()))) {
         if (fullTypeString(binop.LHS()->type()) != fullTypeString(binop.RHS()->type())) {
            if (binop.op() == "="sv) {
               if (binop.LHS()->type()->kind() == TypeKind::Pointer &&
                   binop.RHS()->type()->kind() == TypeKind::Pointer) {
                  auto* lhsPtr = static_cast<PointerTy*>(binop.LHS()->type().get());
                  auto* rhsPtr = static_cast<PointerTy*>(binop.RHS()->type().get());

                  // Allow array-to-pointer decay for assignments like: char* p = new char[20];
                  if (rhsPtr->pointee()->kind() == TypeKind::Array) {
                     auto* rhsArr = static_cast<ArrayTy*>(rhsPtr->pointee().get());
                     if (fullTypeString(lhsPtr->pointee()) == fullTypeString(rhsArr->arrayType())) {
                        binop.setType(binop.LHS()->type());
                        return;
                     }
                  }
               } else if (binop.LHS()->type()->kind() == TypeKind::Array &&
                          binop.RHS()->type()->kind() == TypeKind::Pointer) {
                  auto* lhsArr = static_cast<ArrayTy*>(binop.LHS()->type().get());
                  auto* rhsPtr = static_cast<PointerTy*>(binop.RHS()->type().get());
                  if (fullTypeString(rhsPtr->pointee()) == fullTypeString(binop.LHS()->type()) ||
                      fullTypeString(rhsPtr->pointee()) == fullTypeString(lhsArr->arrayType())) {
                     binop.setType(binop.LHS()->type());
                     return;
                  }
               }
            }

            return _logError(&binop, "type mismatch: cannot operate '{}' and '{}' with operator '{}'", fullTypeString(binop.LHS()->type()), fullTypeString(binop.RHS()->type()), binop.op());
         }

         if (fullTypeString(binop.LHS()->type()) == "char*"sv && binop.op() != "=")
            return _logError(&binop, "strings can only be assigned, not operated on with '{}'", binop.op());
      }

      binop.setType(binop.LHS()->type());
   }

   void SemanticAnalyzer::visit(VariableExpressionAST& var) {
      if (!_doesVariableExist(var.name())) {
         if (_isFunction(var.name()))
            return _logError(&var, "'{}' is a function, not a variable", var.name());
         if (_isClass(var.name()))
            return _logError(&var, "'{}' is a class, not a variable", var.name());
         if (_isNamespace(var.name()))
            return _logError(&var, "'{}' is a namespace, not a variable", var.name());

         if (var.name().find("::"sv) != std::string::npos) {
            std::string prefixKind;
            std::string failingPrefix;
            bool missingScope = false;
            const auto splitPos = var.name().find_last_of(':');
            const std::string prefix = splitPos == std::string::npos ? "" : var.name().substr(0, splitPos - 1);
            const std::string leaf = splitPos == std::string::npos ? var.name() : var.name().substr(splitPos + 1);

            if (!_hasValidQualifiedPrefix(var.name(), prefixKind, failingPrefix, missingScope)) {
               if (missingScope)
                  return _logError(&var, "unknown scope '{}'", failingPrefix);
               return _logError(&var, "'{}' is a {}, not a namespace/class scope", failingPrefix, prefixKind);
            }

            const std::string fullKind = _symbolKindForName(var.name());
            if (!fullKind.empty() && fullKind != "variable")
               return _logError(&var, "'{}' is a {}, not a variable", var.name(), fullKind);

            return _logError(&var, "no variable '{}' found in {} '{}'", leaf, prefixKind.empty() ? "scope" : prefixKind, prefix);
         }

         return _logError(&var, "unknown variable '{}'", var.name());
      }

      var.setType(_variableType(var.name()));
   }

   void SemanticAnalyzer::visit(IndexAccessAST& access) {
      access.array()->accept(*this);
      access.index()->accept(*this);

      if (!access.array()->type())
         return _logError(&access, "cannot index expression with unknown type");
      if (access.array()->type()->kind() != TypeKind::Array && access.array()->type()->kind() != TypeKind::Pointer)
         return _logError(&access, "cannot index non-array/non-pointer type '{}'", fullTypeString(access.array()->type()));

      if (_isConstexpr(access.index())) {
         auto foldedIndexExpr = _evaluateConstexpr(access.index());
         if (!foldedIndexExpr) {
            _hasErrors = true;
            return; // error already logged
         }
         access.index() = std::move(foldedIndexExpr);
      }

      // Numeric literals are currently parsed as `double`; accept them as indices
      // only when they are finite, non-negative integer values.
      if (access.index()->astType() == ASTType::NumberExpression) {
         const Number indexNumber = static_cast<NumberExpressionAST*>(access.index().get())->value();
         const double indexAsDouble = std::holds_alternative<double>(indexNumber)
             ? std::get<double>(indexNumber)
             : static_cast<double>(std::get<uint64_t>(indexNumber));

         if (!std::isfinite(indexAsDouble) || std::floor(indexAsDouble) != indexAsDouble)
            return _logError(access.index().get(), "array index must be an integer");
         if (indexAsDouble < 0.0)
            return _logError(access.index().get(), "array index cannot be negative");

         static_cast<NumberExpressionAST*>(access.index().get())->setType(makeUint32());
      }

      if (!access.index()->type())
         return _logError(&access, "cannot index expression with unknown type");
      if (!_isNumberType(access.index()->type()->string()))
         return _logError(&access, "cannot index non-numeric type '{}'", fullTypeString(access.index()->type()));

      if (access.array()->type()->kind() == TypeKind::Array) {
         auto* arrayTy = static_cast<ArrayTy*>(access.array()->type().get());
         if (arrayTy->sizeExpr() && _isConstexpr(arrayTy->sizeExpr()) && _isConstexpr(access.index())) {
            auto foldedSizeExpr = _evaluateConstexpr(arrayTy->sizeExpr());
            if (!foldedSizeExpr) {
               _hasErrors = true;
               return; // error already logged
            }
            arrayTy->setSize(std::move(foldedSizeExpr));
            static_cast<NumberExpressionAST*>(arrayTy->sizeExpr().get())->setType(makeUint32());

            if (access.index()->astType() != ASTType::NumberExpression)
               return _logError(access.index().get(), "constexpr array index must fold to a number");

            const Number sizeNumber = static_cast<NumberExpressionAST*>(arrayTy->sizeExpr().get())->value();
            const Number indexNumber = static_cast<NumberExpressionAST*>(access.index().get())->value();
            const double sizeAsDouble = std::holds_alternative<double>(sizeNumber)
                ? std::get<double>(sizeNumber)
                : static_cast<double>(std::get<uint64_t>(sizeNumber));
            const double indexAsDouble = std::holds_alternative<double>(indexNumber)
                ? std::get<double>(indexNumber)
                : static_cast<double>(std::get<uint64_t>(indexNumber));
            if (!std::isfinite(indexAsDouble) || std::floor(indexAsDouble) != indexAsDouble)
               return _logError(access.index().get(), "array index must be an integer");
            if (indexAsDouble < 0.0)
               return _logError(access.index().get(), "array index cannot be negative");
            if (!std::isfinite(sizeAsDouble) || std::floor(sizeAsDouble) != sizeAsDouble || sizeAsDouble < 1.0)
               return _logError(arrayTy->sizeExpr().get(), "array size must be a positive integer");
            if (indexAsDouble >= sizeAsDouble)
               return _logError(access.index().get(), "array index out of bounds: index {} but size is {}", indexAsDouble, sizeAsDouble);
         }

         access.setType(arrayTy->arrayType());
         return;
      }

      auto* pointerTy = static_cast<PointerTy*>(access.array()->type().get());
      access.setType(pointerTy->pointee());
   }

   void SemanticAnalyzer::visit(BlockStatementAST& block) {
      _scopes.push_back({});
      ++_currentScope;
      bool reachedTerminator = false;
      for (auto& expr : block.statements()) {
         if (!expr) {
            _hasErrors = true;
            continue;
         }

         if (reachedTerminator) {
            _logWarn(expr.get(), "unreachable code");
            break;
         }

         expr->accept(*this);
         if (expr->astType() == ASTType::VariableExpression) {
            auto varExpr = static_cast<VariableExpressionAST*>(expr.get());
            _logWarn(expr.get(), "unused result of expression '{}'", varExpr->name());
         }

         if (_statementTerminates(expr.get()))
            reachedTerminator = true;
      }
      _scopes.pop_back();
      --_currentScope;
   }

   // void SemanticAnalyzer::visit(ExternVariableStatementAST& fun) {
   // }

   void SemanticAnalyzer::visit(FunctionPrototypeAST& proto) {
      if (_isQualifiedName(proto.name())) {
         std::string prefixKind;
         std::string failingPrefix;
         bool missingScope = false;
         if (!_hasValidQualifiedPrefix(proto.name(), prefixKind, failingPrefix, missingScope)) {
            if (missingScope)
               return _logError(&proto, "unknown scope '{}' in declaration of function '{}'", failingPrefix, proto.name());
            return _logError(&proto, "'{}' is a {}, not a namespace/class scope for function '{}'", failingPrefix, prefixKind, proto.name());
         }
      }

      if (_scopes.at(GlobalScope).contains(proto.name()) || _isClass(proto.name()) || _isNamespace(proto.name())) {
         const std::string kind = _symbolKindForName(proto.name());
         return _logError(&proto, "{} with name '{}' already exists", kind.empty() ? "symbol" : kind, proto.name());
      }

      if (_declaredFunctions.contains("main") && proto.name() == "main"sv)
         return _logError(&proto, "redefinition of main function");

      Overload over;
      over.type = proto.type();
      _allowUninitializedReferenceDecl = true;
      for (auto& arg : proto.args()) {
         auto cast = static_cast<VariableDeclarationAST*>(arg.get());
         cast->accept(*this);
         over.args.push_back(cast->type());
      }
      _allowUninitializedReferenceDecl = false;
      auto& overloads = _declaredFunctions[proto.name()];

      std::string signature = proto.name();
      signature += "(";
      for (size_t i = 0; i < over.args.size(); ++i) {
         signature += fullTypeString(over.args[i]);
         if (i + 1 < over.args.size())
            signature += ", ";
      }
      signature += ")";

      auto sameSignature = [&](const Overload& o) {
         bool oVariadic = !o.args.empty() && o.args.back()->kind() == TypeKind::ArgPack;
         bool overVariadic = !over.args.empty() && over.args.back()->kind() == TypeKind::ArgPack;

         if (!oVariadic && !overVariadic && o.args.size() != over.args.size()) return false;
         if (oVariadic && overVariadic) {
            if (o.args.size() != over.args.size()) return false;
         }
         if ((oVariadic && !overVariadic) || (!oVariadic && overVariadic)) {
            if (o.args.size() - 1 > over.args.size() - 1) return false;
         }

         size_t minArgs = std::min(o.args.size(), over.args.size());
         for (size_t i = 0; i < minArgs; i++) {
            if (fullTypeString(o.args[i]) != fullTypeString(over.args[i])) return false;
         }

         return true;
      };
      auto sameParams = [&](const Overload& o) {
         if (o.args.size() != over.args.size()) return false;
         for (size_t i = 0; i < o.args.size(); i++) {
            if (fullTypeString(o.args[i]) != fullTypeString(over.args[i]))
               return false;
         }
         return true;
      };

      for (auto& existing : overloads) {
         if (sameSignature(existing)) {
            return _logError(&proto, "function '{}' with the same signature already exists", proto.name());
         }
         if (sameParams(existing) && fullTypeString(existing.type) != fullTypeString(over.type)) {
            return _logError(&proto, "function '{}' with same parameters but different return type already exists", proto.name());
         }
      }

      if (proto.name() == "main"sv) {
         if (proto.type()->kind() != TypeKind::Int32)
            return _logError(&proto, "main function must have return type 'int' or 'i32'");
      }
      std::string args;
      for (const auto& arg : proto.args()) {
         auto cast = static_cast<VariableDeclarationAST*>(arg.get());
         args += fullTypeString(cast->type()) + (arg == proto.args().back() ? "" : ", ");
      }
      utils::debugLog("declared function: fun {}({}) -> {}", proto.name(), args, fullTypeString(proto.type()));
      overloads.push_back(over);
   }

   void SemanticAnalyzer::visit(FunctionAST& fun) {
      _scopes.push_back({}); // scope already created inside visit(BlockStatementAST&) but I need to make one for the function arguments
      ++_currentScope;
      fun.prototype()->accept(*this);
      // Main has a fixed ABI-like signature: int/i32 main([int/i32 argc, String*|char** argv])
      if (fun.name() == "main"sv) {
         if (fun.prototype()) {
            if (!fun.prototype()->args().empty()) {
               if (fun.prototype()->args().size() != 2 && fun.prototype()->args().size() != 0)
                  return _logError("main function must have either 0 or 2 arguments, not {}", fun.prototype()->args().size());
               if (fun.prototype()->args().size() == 2) {
                  if (fun.prototype()->args()[0]->type()->kind() != TypeKind::Int32)
                     return _logError("main function must have first argument of type 'int' or 'i32'");
                  if (fullTypeString(fun.prototype()->args()[1]->type()) != "String*"sv && fullTypeString(fun.prototype()->args()[1]->type()) != "char**"sv)
                     return _logError("main function must have second argument of type 'String*' or 'char**'");
               }
            }
         }
         if (fun.body()) {
            for (auto& expr : static_cast<BlockStatementAST*>(fun.body().get())->statements()) {
               if (expr && expr->astType() == ASTType::ReturnStatement)
                  static_cast<ReturnStatementAST*>(expr.get())->setType(makeInt32());
            }
         }
      }
      if (fun.name().find("-") != std::string::npos) {
         _scopes.at(_currentScope)["this"] = {false, makeClass(fun.name().substr(0, fun.name().find("-")))}; // add implicit 'this' variable to class methods
      }
      if (fun.body())
         fun.body()->accept(*this);
      else
         _hasErrors = true;
      _scopes.pop_back();
      --_currentScope;
   }

   void SemanticAnalyzer::visit(CallExpressionAST& call) {
      if (call.callee() == "main"sv)
         return _logError(&call, "cannot call main function");

      auto overloadsIt = _declaredFunctions.find(call.callee());
      std::vector<Overload> overloads;
      if (overloadsIt != _declaredFunctions.end()) {
         overloads = overloadsIt->second;
      }
      for (auto& fun : _functions) {
         if (fun->name() == call.callee()) {
            Overload over;
            over.type = fun->type();
            for (auto& arg : fun->args()) {
               auto cast = static_cast<VariableDeclarationAST*>(arg.get());
               over.args.push_back(cast->type());
            }
            overloads.push_back(over);
         }
      }

      auto sameOverload = [](const Overload& lhs, const Overload& rhs) {
         if (fullTypeString(lhs.type) != fullTypeString(rhs.type))
            return false;
         if (lhs.args.size() != rhs.args.size())
            return false;
         for (size_t i = 0; i < lhs.args.size(); ++i) {
            if (fullTypeString(lhs.args[i]) != fullTypeString(rhs.args[i]))
               return false;
         }
         return true;
      };

      std::vector<Overload> uniqueOverloads;
      uniqueOverloads.reserve(overloads.size());
      for (const auto& over : overloads) {
         bool alreadyPresent = false;
         for (const auto& existing : uniqueOverloads) {
            if (sameOverload(existing, over)) {
               alreadyPresent = true;
               break;
            }
         }
         if (!alreadyPresent)
            uniqueOverloads.push_back(over);
      }
      overloads = std::move(uniqueOverloads);

      if (overloads.empty()) {
         call.setType(makeNullType()); // temporary fix (if I don't fucking forget to fix it *again*)
         if (call.callee().find("::"sv) != std::string::npos)
            return _logError(&call, "no function '{}' found in namespace '{}'", call.callee().substr(call.callee().find_last_of(":") + 1), call.callee().substr(0, call.callee().find_last_of("::") - 1));
         return _logError(&call, "unknown function '{}'", call.callee());
      }

      for (auto& arg : call.args()) {
         arg->accept(*this);
      }

      struct OverloadCandidate {
         size_t index{};
         int score{};
         bool valid{};
         size_t fixedArgs{};
         bool variadic{};
         std::vector<std::shared_ptr<Type>> castTargets;
         std::vector<bool> applyDecay;
      };

      auto scoreArgument = [&](const std::shared_ptr<Type>& expected,
                               const std::shared_ptr<Type>& actual,
                               std::shared_ptr<Type>& castTarget,
                               bool& decay) -> int {
         castTarget = nullptr;
         decay = false;

         if (fullTypeString(expected) == fullTypeString(actual))
            return 0;

         if (expected->kind() == TypeKind::Pointer && actual->kind() == TypeKind::Array) {
            auto* expectedPtr = static_cast<PointerTy*>(expected.get());
            auto* actualArray = static_cast<ArrayTy*>(actual.get());
            if (fullTypeString(expectedPtr->pointee()) == fullTypeString(actualArray->arrayType())) {
               decay = true;
               return 1;
            }
         }


         if (expected->kind() == TypeKind::Reference) {
            auto* expectedRef = static_cast<ReferenceTy*>(expected.get());
            const std::string expectedUnderlying = fullTypeString(expectedRef->reference());

            if (actual->kind() == TypeKind::Pointer) {
               auto* actualPtr = static_cast<PointerTy*>(actual.get());
               if (fullTypeString(actualPtr->pointee()) == expectedUnderlying)
                  return 0;
            }

            if (actual->kind() == TypeKind::Reference) {
               auto* actualRef = static_cast<ReferenceTy*>(actual.get());
               if (fullTypeString(actualRef->reference()) == expectedUnderlying)
                  return 0;
            }
         }

         if (_isNumberType(expected->string()) && _isNumberType(actual->string())) {
            castTarget = expected;
            int score = 10;
            if (_checkNumericConversionLoss(actual->string(), expected->string()))
               score += 5;
            return score;
         }

         return -1;
      };

      auto evaluateOverload = [&](size_t index, const Overload& overload) -> OverloadCandidate {
         OverloadCandidate candidate;
         candidate.index = index;
         candidate.valid = false;
         candidate.score = 0;
         candidate.castTargets.resize(call.args().size());
         candidate.applyDecay.resize(call.args().size(), false);

         candidate.variadic = !overload.args.empty() && overload.args.back()->kind() == TypeKind::ArgPack;
         candidate.fixedArgs = candidate.variadic ? overload.args.size() - 1 : overload.args.size();

         if (candidate.variadic) {
            if (call.args().size() < candidate.fixedArgs)
               return candidate;
            candidate.score += 2;
         } else {
            if (call.args().size() != candidate.fixedArgs)
               return candidate;
         }

         for (size_t i = 0; i < candidate.fixedArgs; ++i) {
            auto expected = overload.args[i];
            auto actual = call.args()[i]->type();
            if (!expected || !actual)
               return candidate;

            std::shared_ptr<Type> castTarget;
            bool decay = false;
            int argScore = scoreArgument(expected, actual, castTarget, decay);
            if (argScore < 0)
               return candidate;

            candidate.score += argScore;
            candidate.castTargets[i] = castTarget;
            candidate.applyDecay[i] = decay;
         }

         candidate.valid = true;
         return candidate;
      };

      bool valid = false;
      bool ambiguous = false;
      OverloadCandidate best;
      int bestScore = std::numeric_limits<int>::max();

      for (size_t i = 0; i < overloads.size(); ++i) {
         auto candidate = evaluateOverload(i, overloads[i]);
         if (!candidate.valid)
            continue;

         if (candidate.score < bestScore) {
            bestScore = candidate.score;
            best = std::move(candidate);
            valid = true;
            ambiguous = false;
         } else if (candidate.score == bestScore) {
            ambiguous = true;
         }
      }

      if (valid && ambiguous) {
         call.setType(makeNullType());
         return _logError(&call, "ambiguous call to function '{}'", call.callee());
      }

      if (valid) {
         auto& chosen = overloads[best.index];

         for (size_t i = 0; i < best.fixedArgs; ++i) {
            if (best.applyDecay[i]) {
               auto actual = call.args()[i]->type();
               auto* actualArray = static_cast<ArrayTy*>(actual.get());
               call.args()[i]->type() = makePointer(actualArray->arrayType());
            }
            if (chosen.args[i]->kind() == TypeKind::Reference && call.args()[i]->type()->kind() == TypeKind::Pointer)
               call.args()[i]->type() = chosen.args[i]; // automatic detection of references
            if (best.castTargets[i]) {
               call.args()[i] = std::make_unique<StaticCastAST>(best.castTargets[i], std::move(call.args()[i]));
            }
         }

         call.setType(chosen.type);
         return;
      }

      if (!valid) {
         std::string args;
         for (size_t i = 0; i < call.args().size(); ++i) {
            args += fullTypeString(call.args().at(i)->type()) + (i == call.args().size() - 1 ? "" : ", ");
         }
         call.setType(makeNullType());
         if (args.empty())
            return _logError(&call, "no overload of function '{}' with no arguments", call.callee());
         return _logError(&call, "no overload of function '{}' with arguments '{}'", call.callee(), args);
      }
   }

   void SemanticAnalyzer::visit(IfStatementAST& ifStmnt) {
      ifStmnt.condition()->accept(*this);
      switch (ifStmnt.condition()->astType()) {
         case ASTType::NumberExpression:
         case ASTType::VariableExpression:
         case ASTType::CallExpression:
         case ASTType::UnaryExpression:
         case ASTType::BinaryExpression:
         case ASTType::VariableDeclaration:
            break;

         default:
            return _logError(&ifStmnt, "if statement condition must be a numeric or boolean expression");
      }
      if (ifStmnt.thenBranch()->astType() != ASTType::BlockStatement) {
         _scopes.push_back({});
         ++_currentScope;
         ifStmnt.thenBranch()->accept(*this);
         _scopes.pop_back();
         --_currentScope;
      } else
         ifStmnt.thenBranch()->accept(*this);

      if (ifStmnt.elseBranch()) {
         if (ifStmnt.elseBranch()->astType() != ASTType::BlockStatement) {
            _scopes.push_back({});
            ++_currentScope;
            ifStmnt.elseBranch()->accept(*this);
            _scopes.pop_back();
            --_currentScope;
         } else
            ifStmnt.elseBranch()->accept(*this);
      }
   }
   void SemanticAnalyzer::visit(WhileStatementAST& whileStmnt) {
      whileStmnt.condition()->accept(*this);
      switch (whileStmnt.condition()->astType()) {
         case ASTType::NumberExpression:
         case ASTType::VariableExpression:
         case ASTType::CallExpression:
         case ASTType::UnaryExpression:
         case ASTType::BinaryExpression:
         case ASTType::VariableDeclaration:
            break;

         default:
            return _logError(&whileStmnt, "while statement condition must be a numeric or boolean expression");
      }

      ++_loopDepth;
      whileStmnt.body()->accept(*this);
      --_loopDepth;
   }
   void SemanticAnalyzer::visit(DoWhileStatementAST& doWhile) {
      doWhile.condition()->accept(*this);
      switch (doWhile.condition()->astType()) {
         case ASTType::NumberExpression:
         case ASTType::VariableExpression:
         case ASTType::CallExpression:
         case ASTType::UnaryExpression:
         case ASTType::BinaryExpression:
         case ASTType::VariableDeclaration:
            break;

         default:
            return _logError(&doWhile, "while statement condition must be a numeric or boolean expression");
      }

      ++_loopDepth;
      doWhile.body()->accept(*this);
      --_loopDepth;
   }
   void SemanticAnalyzer::visit(ForStatementAST& forStmnt) {
      _scopes.push_back({});
      ++_currentScope;
      forStmnt.init()->accept(*this);
      switch (forStmnt.init()->astType()) {
         case ASTType::VariableDeclaration:
         case ASTType::BinaryExpression:
            break;
         default:
            return _logError(&forStmnt, "for statement initialization must be a variable declaration or a binary expression");
      }
      forStmnt.condition()->accept(*this);
      switch (forStmnt.condition()->astType()) {
         case ASTType::NumberExpression:
         case ASTType::VariableExpression:
         case ASTType::CallExpression:
         case ASTType::UnaryExpression:
         case ASTType::BinaryExpression:
         case ASTType::VariableDeclaration:
            if (forStmnt.condition()->type()->kind() != TypeKind::Bool && !_isNumberType(fullTypeString(forStmnt.condition()->type())))
               return _logError(&forStmnt, "while statement condition must be a numeric or boolean expression");
            break;
         default:
            return _logError(&forStmnt, "for statement condition must be a numeric or boolean expression");
      }
      forStmnt.increment()->accept(*this);
      switch (forStmnt.increment()->astType()) {
         case ASTType::BinaryExpression:
         case ASTType::UnaryExpression:
            break;
         default:
            return _logError(&forStmnt, "for statement increment must be a binary expression");
      }
      ++_loopDepth;
      forStmnt.body()->accept(*this);
      --_loopDepth;
      _scopes.pop_back();
      --_currentScope;
   }

   void SemanticAnalyzer::visit(BreakStatementAST& brk) {
      if (_loopDepth == 0)
         return _logError(&brk, "'break' can only be used inside a loop");
   }

   void SemanticAnalyzer::visit(ContinueStatementAST& cont) {
      if (_loopDepth == 0)
         return _logError(&cont, "'continue' can only be used inside a loop");
   }

   void SemanticAnalyzer::visit(ExitStatement& ext) {
      ext.value()->accept(*this);
      if (!ext.value()->type()) {
         if (ext.value()->astType() == ASTType::NumberExpression)
            static_cast<NumberExpressionAST*>(ext.value().get())->setType(makeInt64());
      }
      if (!_isNumberType(ext.value()->type()->string()))
         return _logError(&ext, "exit statement expects a numeric value");
   }

   void SemanticAnalyzer::visit(ReturnStatementAST& ret) {
      if (ret.type()->kind() == TypeKind::Void) {
         if (ret.value() != nullptr) {
            ret.value()->accept(*this);
            return _logError(&ret, "'void' function cannot return '{}'", fullTypeString(ret.value()->type()));
         }
         return;
      }

      ret.value()->accept(*this);
      if (!ret.value()->type()) {
         if (ret.value()->astType() == ASTType::NumberExpression) {
            static_cast<NumberExpressionAST*>(ret.value().get())->setType(ret.type());
            return;
         }
      }
      if (!_isNumberType(ret.type()->string()) || !_isNumberType(ret.value()->type()->string())) {
         if (ret.type()->string() != ret.value()->type()->string())
            return _logError(&ret, "'{}' function cannot return '{}'", fullTypeString(ret.type()), fullTypeString(ret.value()->type()));
      } /*else {
         if (_checkNumericConversionLoss(baseType(ret.value()->type()), baseType(ret.type())))
            _logWarn("possible data loss converting from '{}' to '{}' in return statement", baseType(ret.value()->type()), baseType(ret.type()));
      }*/
   }

   bool SemanticAnalyzer::_statementTerminates(ExpressionAST* expr) const {
      if (!expr)
         return false;

      switch (expr->astType()) {
         case ASTType::ReturnStatement:
         case ASTType::ExitStatement:
         case ASTType::BreakStatement:
         case ASTType::ContinueStatement:
            return true;

         case ASTType::IfStatement: {
            auto* ifStmnt = static_cast<IfStatementAST*>(expr);
            if (!ifStmnt->elseBranch())
               return false;
            return _statementTerminates(ifStmnt->thenBranch().get()) && _statementTerminates(ifStmnt->elseBranch().get());
         }

         case ASTType::BlockStatement: {
            auto* block = static_cast<BlockStatementAST*>(expr);
            for (const auto& statement : block->statements()) {
               if (!statement)
                  continue;
               if (_statementTerminates(statement.get()))
                  return true;
            }
            return false;
         }

         default:
            return false;
      }
   }

   void SemanticAnalyzer::visit(NewStatementAST& var) {
      if (!var.type())
         return _logError(&var, "new statement expects a valid type");
      if (var.type()->kind() == TypeKind::Reference)
         return _logError(&var, "cannot use 'new' with reference type '{}'", fullTypeString(var.type()));

      // keep `new` on pointer types stable (avoid creating an extra pointer layer).
      if (var.type()->kind() == TypeKind::Pointer)
         return;

      var.setType(makePointer(var.type()));
   }
   void SemanticAnalyzer::visit(FreeStatementAST& var) {
      var.variable()->accept(*this);
      if (var.variable()->type()->kind() != TypeKind::Pointer && var.variable()->type()->kind() != TypeKind::Array)
         return _logError(&var, "free statement expects a pointer type");
   }

   void SemanticAnalyzer::visit(ClassMethodAST& method) {
      if (!method.method()) {
         _hasErrors = true;
         return;
      }

      _scopes.push_back({});
      ++_currentScope;

      _allowUninitializedReferenceDecl = true;
      for (auto& arg : method.method()->args()) {
         if (!arg) {
            _hasErrors = true;
            continue;
         }
         auto* cast = static_cast<VariableDeclarationAST*>(arg.get());
         cast->accept(*this);
      }
      _allowUninitializedReferenceDecl = false;

      if (method.method()->body())
         method.method()->body()->accept(*this);
      else
         _hasErrors = true;

      _scopes.pop_back();
      --_currentScope;
   }
   void SemanticAnalyzer::visit(ClassVariableAST& var) {
      if (!var.variable()) {
         _hasErrors = true;
         return;
      }

      if (var.variable()->astType() != ASTType::VariableDeclaration)
         return _logError("class member must be a variable declaration");

      var.variable()->accept(*this);
   }
   void SemanticAnalyzer::visit(ClassConstructorAST& ctor) {
      if (!ctor.constructor()) {
         _hasErrors = true;
         return;
      }

      _scopes.push_back({});
      ++_currentScope;

      _allowUninitializedReferenceDecl = true;
      for (auto& arg : ctor.constructor()->args()) {
         if (!arg) {
            _hasErrors = true;
            continue;
         }
         auto* cast = static_cast<VariableDeclarationAST*>(arg.get());
         cast->accept(*this);
      }
      _allowUninitializedReferenceDecl = false;

      if (ctor.constructor()->body())
         ctor.constructor()->body()->accept(*this);
      else
         _hasErrors = true;

      _scopes.pop_back();
      --_currentScope;
   }
   void SemanticAnalyzer::visit(ClassDestructorAST& dtor) {
      if (!dtor.destructor()) {
         _hasErrors = true;
         return;
      }

      if (dtor.destructor()->args().size() > 1)
         _logError(dtor.destructor()->args().at(0).get(), "destructors cannot have arguments");

      _scopes.push_back({});
      ++_currentScope;

      if (dtor.destructor()->body())
         dtor.destructor()->body()->accept(*this);
      else
         _hasErrors = true;

      if (dtor.visibility() != Visibility::Public)
         _logError(dtor.destructor()->prototype().get(), "destructor must be public");

      _scopes.pop_back();
      --_currentScope;
   }
   void SemanticAnalyzer::visit(ClassAST& cls) {
      const std::string existingKind = _symbolKindForName(cls.name());
      if (!existingKind.empty() && existingKind != "class")
         _logError("{} with name '{}' already exists", existingKind, cls.name());
      else if (_declaredClasses.contains(cls.name()))
         if (cls.hasErrors())
            _hasErrors = true;
      _scopes.push_back({});
      ++_currentScope;

      for (auto& var : cls.members()) {
         if (var)
            var->accept(*this);
         else
            _hasErrors = true;
      }

      for (auto& ctor : cls.constructors()) {
         if (!ctor || !ctor->constructor() || !ctor->constructor()->prototype()) {
            _hasErrors = true;
            continue;
         }
         std::string ctorName = cls.name() + "-constructor";
         Overload over;
         over.type = makeVoid();
         for (const auto& arg : ctor->constructor()->args()) {
            over.args.push_back(arg->type());
         }
         // check for duplicate and overloads
         auto& overloads = _declaredFunctions[ctorName];
         auto sameSignature = [&](const Overload& o) {
            bool oVariadic = !o.args.empty() && o.args.back()->kind() == TypeKind::ArgPack;
            bool overVariadic = !over.args.empty() && over.args.back()->kind() == TypeKind::ArgPack;
            if (!oVariadic && !overVariadic && o.args.size() != over.args.size()) return false;
            if (oVariadic && overVariadic) {
               if (o.args.size() != over.args.size()) return false;
            }
            if ((oVariadic && !overVariadic) || (!oVariadic && overVariadic)) {
               if (o.args.size() - 1 > over.args.size() - 1) return false;
            }
            size_t minArgs = std::min(o.args.size(), over.args.size());
            for (size_t i = 0; i < minArgs; i++) {
               if (fullTypeString(o.args[i]) != fullTypeString(over.args[i])) return false;
            }
            return true;
         };
         auto sameParams = [&](const Overload& o) {
            if (o.args.size() != over.args.size()) return false;
            for (size_t i = 0; i < o.args.size(); i++) {
               if (fullTypeString(o.args[i]) != fullTypeString(over.args[i]))
                  return false;
            }
            return true;
         };
         bool duplicate = false;
         for (auto& existing : overloads) {
            if (sameSignature(existing)) {
               _logError(ctor->constructor()->prototype().get(), "constructor with signature already exists in class '{}'", cls.name());
               _hasErrors = true;
               duplicate = true;
               break;
            }
            if (sameParams(existing) && fullTypeString(existing.type) != fullTypeString(over.type)) {
               _logError(ctor->constructor()->prototype().get(), "constructor with same parameters but different return type already exists in class '{}'", cls.name());
               _hasErrors = true;
               duplicate = true;
               break;
            }
         }
         if (duplicate)
            continue;
         overloads.push_back(over);
         ctor->accept(*this);
      }

      if (cls.destructor())
         cls.destructor()->accept(*this);
      else
         _hasErrors = true;

      for (auto& fun : cls.methods()) {
         if (!fun || !fun->method() || !fun->method()->prototype()) {
            _hasErrors = true;
            continue;
         }
         std::string methodName = cls.name() + "-" + fun->method()->prototype()->name() + "$";
         Overload over;
         over.type = fun->method()->prototype()->type();
         _allowUninitializedReferenceDecl = true;
         for (auto& arg : fun->method()->prototype()->args()) {
            if (!arg) {
               _hasErrors = true;
               continue;
            }
            auto* cast = static_cast<VariableDeclarationAST*>(arg.get());
            cast->accept(*this);
            over.args.push_back(cast->type());
         }
         _allowUninitializedReferenceDecl = false;
         auto& overloads = _declaredFunctions[methodName];
         auto sameSignature = [&](const Overload& o) {
            bool oVariadic = !o.args.empty() && o.args.back()->kind() == TypeKind::ArgPack;
            bool overVariadic = !over.args.empty() && over.args.back()->kind() == TypeKind::ArgPack;
            if (!oVariadic && !overVariadic && o.args.size() != over.args.size()) return false;
            if (oVariadic && overVariadic) {
               if (o.args.size() != over.args.size()) return false;
            }
            if ((oVariadic && !overVariadic) || (!oVariadic && overVariadic)) {
               if (o.args.size() - 1 > over.args.size() - 1) return false;
            }
            size_t minArgs = std::min(o.args.size(), over.args.size());
            for (size_t i = 0; i < minArgs; i++) {
               if (fullTypeString(o.args[i]) != fullTypeString(over.args[i])) return false;
            }
            return true;
         };
         auto sameParams = [&](const Overload& o) {
            if (o.args.size() != over.args.size()) return false;
            for (size_t i = 0; i < o.args.size(); i++) {
               if (fullTypeString(o.args[i]) != fullTypeString(over.args[i]))
                  return false;
            }
            return true;
         };
         bool duplicate = false;
         for (auto& existing : overloads) {
            if (sameSignature(existing)) {
               _logError(fun->method()->prototype().get(), "method '{}' with the same signature already exists", fun->method()->prototype()->name());
               _hasErrors = true;
               duplicate = true;
               break;
            }
            if (sameParams(existing) && fullTypeString(existing.type) != fullTypeString(over.type)) {
               _logError(fun->method()->prototype().get(), "method '{}' with same parameters but different return type already exists", fun->method()->prototype()->name());
               _hasErrors = true;
               duplicate = true;
               break;
            }
         }
         if (duplicate)
            continue;
         overloads.push_back(over);
         fun->accept(*this);
      }

      _scopes.pop_back();
      --_currentScope;

      std::string types{};
      for (const auto& ty : cls.members()) {
         types += fullTypeString(ty->variable()->type()) + (ty == cls.members().back() ? "" : ", ");
      }
      utils::debugLog("declared class: class {}, with members of type: {{{}}}", cls.name(), types);
   }

   bool SemanticAnalyzer::_isSignedType(const std::string& type) {
      return type == "int"sv || type == "i8"sv || type == "i16"sv || type == "i32"sv || type == "i64"sv || type == "i128"sv;
   }
   size_t SemanticAnalyzer::_line(size_t position) const {
      size_t line = 1;
      const size_t safePosition = std::min(position, _raw.size());
      for (size_t i = 0; i < safePosition; ++i) {
         if (_raw[i] == '\n')
            ++line;
      }
      return line;
   }
   size_t SemanticAnalyzer::_column(size_t position) const {
      size_t column = 1;
      const size_t safePosition = std::min(position, _raw.size());
      for (size_t i = 0; i < safePosition; ++i) {
         if (_raw[i] == '\n')
            column = 1;
         else
            ++column;
      }
      return column;
   }
   bool SemanticAnalyzer::_isUnsignedType(const std::string& type) {
      return type == "uint"sv || type == "u8"sv || type == "u16"sv || type == "u32"sv || type == "u64"sv || type == "u128"sv;
   }
   bool SemanticAnalyzer::_isFloatingPointType(const std::string& type) {
      return type == "float"sv || type == "double"sv;
   }
   bool SemanticAnalyzer::_isNumberType(const std::string& type) {
      return _isSignedType(type) || _isUnsignedType(type) || _isFloatingPointType(type);
   }
   bool SemanticAnalyzer::_isIntegerType(const std::string& type) {
      return _isSignedType(type) || _isUnsignedType(type);
   }
   bool SemanticAnalyzer::_isBaseType(const std::string& type) {
      return _isNumberType(type) || type == "char*"sv || type == "char"sv || type == "void"sv || type == "bool"sv || type == "ArgPack"sv;
   }
   bool SemanticAnalyzer::_isClass(const std::string& cls) {
      for (auto& clss : _classes) {
         if (clss->name() == cls)
            return true;
      }
      return false;
   }
   bool SemanticAnalyzer::_isFunction(const std::string& fn) {
      if (_declaredFunctions.contains(fn))
         return true;
      for (auto& fun : _functions) {
         if (fun->name() == fn)
            return true;
      }
      return false;
   }
   bool SemanticAnalyzer::_isNamespace(const std::string& ns) {
      const std::string prefix = ns + "::";

      for (const auto& [name, _] : _declaredFunctions) {
         if (name.rfind(prefix, 0) == 0)
            return true;
      }
      for (const auto& scope : _scopes) {
         for (const auto& [name, _] : scope) {
            if (name.rfind(prefix, 0) == 0)
               return true;
         }
      }
      for (const auto& cls : _classes) {
         if (!cls)
            continue;
         if (cls->name().rfind(prefix, 0) == 0)
            return true;
      }

      return false;
   }
   bool SemanticAnalyzer::_isQualifiedName(const std::string& name) const {
      return name.find("::"sv) != std::string::npos;
   }
   bool SemanticAnalyzer::_hasValidQualifiedPrefix(const std::string& name, std::string& prefixKind, std::string& failingPrefix, bool& missingScope) {
      prefixKind.clear();
      failingPrefix.clear();
      missingScope = false;

      if (!_isQualifiedName(name))
         return true;

      std::string prefix;
      size_t partStart = 0;
      while (true) {
         const size_t sep = name.find("::", partStart);
         if (sep == std::string::npos)
            break;

         if (!prefix.empty())
            prefix += "::";
         prefix += name.substr(partStart, sep - partStart);

         if (_isClass(prefix)) {
            prefixKind = "class";
            partStart = sep + 2;
            continue;
         }
         if (_isNamespace(prefix)) {
            prefixKind = "namespace";
            partStart = sep + 2;
            continue;
         }

         const std::string kind = _symbolKindForName(prefix);
         failingPrefix = prefix;
         if (!kind.empty()) {
            prefixKind = kind;
            return false;
         }

         // Prefix not found: can be acceptable for inline namespace declarations.
         missingScope = true;
         prefixKind = "namespace";
         return false;
      }

      return true;
   }
   std::string SemanticAnalyzer::_symbolKindForName(const std::string& name) {
      // Must scan all currently active scopes: function scopes are popped on exit,
      // so this reflects exactly what is visible at the analysis point.
      if (_doesVariableExist(name))
         return "variable";
      if (_isClass(name))
         return "class";
      if (_isNamespace(name))
         return "namespace";
      if (_isFunction(name))
         return "function";
      return "";
   }
   bool SemanticAnalyzer::_checkNumericConversionLoss(const std::string& fromType, const std::string& toType) {
      if (fromType == toType) return false;

      if (_isNumberType(fromType) && _isNumberType(toType)) {
         // Float/Double → non floating point
         if (_isFloatingPointType(fromType) && !_isFloatingPointType(toType))
            return true;

         // Double → Float
         if (fromType == "double"sv && toType == "float"sv)
            return true;

         auto bitSize = [](const std::string& t) -> int {
            if (t == "i8"sv || t == "u8"sv) return 8;
            if (t == "i16"sv || t == "u16"sv) return 16;
            if (t == "i32"sv || t == "u32"sv || t == "int"sv || t == "uint"sv) return 32;
            if (t == "i64"sv || t == "u64"sv) return 64;
            if (t == "i128"sv || t == "u128"sv) return 128;
            if (t == "float"sv) return 32;
            if (t == "double"sv) return 64;
            return 0; // unknown
         };

         int fromBits = bitSize(fromType);
         int toBits = bitSize(toType);

         if (fromBits > toBits) return true;

         // Signed → Unsigned
         if (_isSignedType(fromType) && _isUnsignedType(toType))
            return true;
      }

      return false;
   }
   bool SemanticAnalyzer::_doesVariableExist(const std::string& var) {
      for (const auto& scope : _scopes) {
         if (scope.contains(var))
            return true;
      }
      return false;
   }
   bool SemanticAnalyzer::_isConstVariable(const std::string& var) const {
      for (const auto& scope : _scopes) {
         if (scope.contains(var) && scope.at(var).first)
            return true;
      }
      return false;
   }
   std::optional<std::string> SemanticAnalyzer::_constWriteTargetName(ExpressionAST* expr) const {
      if (!expr)
         return std::nullopt;

      switch (expr->astType()) {
         case ASTType::VariableExpression: {
            const auto* variable = static_cast<VariableExpressionAST*>(expr);
            if (_isConstVariable(variable->name()))
               return variable->name();
            return std::nullopt;
         }
         case ASTType::IndexAccessExpression: {
            auto* access = static_cast<IndexAccessAST*>(expr);
            return _constWriteTargetName(access->array().get());
         }
         default:
            return std::nullopt;
      }
   }
   std::shared_ptr<Type> SemanticAnalyzer::_variableType(const std::string& var) const {
      for (const auto& scope : _scopes) {
         if (scope.contains(var))
            return scope.at(var).second;
      }
      return nullptr;
   }

   bool SemanticAnalyzer::_isConstexpr(const std::unique_ptr<ExpressionAST>& expr) {
      if (!expr)
         return false;

      switch (expr->astType()) {
         case ASTType::NumberExpression:
         case ASTType::StringExpression:
         case ASTType::BuiltinLine:
         case ASTType::BuiltinColumn:
         case ASTType::BuiltinFile:
         case ASTType::BuiltinFunction:
         case ASTType::NullPtr:
            return true;

         case ASTType::UnaryExpression: {
            auto* unaryExpr = static_cast<UnaryExpressionAST*>(expr.get());
            const auto& op = unaryExpr->op();
            if (op != "+"sv && op != "-"sv && op != "!"sv)
               return false;
            return _isConstexpr(unaryExpr->operand());
         }

         case ASTType::BinaryExpression: {
            auto* binaryExpr = static_cast<BinaryExpressionAST*>(expr.get());
            const auto& op = binaryExpr->op();

            if (op != "+"sv && op != "-"sv && op != "*"sv && op != "/"sv && op != "%"sv &&
                op != "=="sv && op != "!="sv && op != "<"sv && op != "<="sv && op != ">"sv && op != ">="sv &&
                op != "&&"sv && op != "||"sv &&
                op != "&"sv && op != "|"sv && op != "^"sv && op != "<<"sv && op != ">>"sv)
               return false;


            return _isConstexpr(binaryExpr->LHS()) && _isConstexpr(binaryExpr->RHS());
         }

         default:
            return false;
      }
   }

   std::unique_ptr<ExpressionAST> SemanticAnalyzer::_evaluateConstexpr(const std::unique_ptr<ExpressionAST>& expr) {
      if (!expr) {
         _logError("invalid constexpr expression: null expression");
         return nullptr;
      }

      auto makeFolded = [&](double value) -> std::unique_ptr<ExpressionAST> {
         auto folded = std::make_unique<NumberExpressionAST>(value, makeDouble());
         if (expr->hasSource())
            folded->setSource(expr->sourcePosition(), expr->sourceLength());
         return folded;
      };

      auto extractNumber = [&](const ExpressionAST* node, double& out) -> bool {
         if (!node || node->astType() != ASTType::NumberExpression)
            return false;

         const Number number = static_cast<const NumberExpressionAST*>(node)->value();
         out = std::holds_alternative<double>(number)
             ? std::get<double>(number)
             : static_cast<double>(std::get<uint64_t>(number));
         return true;
      };

      switch (expr->astType()) {
         case ASTType::NumberExpression: {
            double value = 0.0;
            if (!extractNumber(expr.get(), value)) {
               _logError(expr.get(), "invalid numeric literal in constant expression");
               return nullptr;
            }
            return makeFolded(value);
         }

         case ASTType::UnaryExpression: {
            auto* unaryExpr = static_cast<UnaryExpressionAST*>(expr.get());
            auto foldedOperand = _evaluateConstexpr(unaryExpr->operand());
            if (!foldedOperand)
               return nullptr;

            double operandValue = 0.0;
            if (!extractNumber(foldedOperand.get(), operandValue)) {
               _logError(unaryExpr, "invalid operand in constant unary expression");
               return nullptr;
            }

            if (unaryExpr->op() == "+"sv)
               return makeFolded(operandValue);
            if (unaryExpr->op() == "-"sv)
               return makeFolded(-operandValue);
            if (unaryExpr->op() == "!"sv)
               return makeFolded(operandValue == 0.0 ? 1.0 : 0.0);

            _logError(unaryExpr, "unsupported unary operator '{}' in constant expression", unaryExpr->op());
            return nullptr;
         }

         case ASTType::BinaryExpression: {
            auto* binaryExpr = static_cast<BinaryExpressionAST*>(expr.get());

            auto foldedLHS = _evaluateConstexpr(binaryExpr->LHS());
            if (!foldedLHS)
               return nullptr;

            auto foldedRHS = _evaluateConstexpr(binaryExpr->RHS());
            if (!foldedRHS)
               return nullptr;

            double lhsValue = 0.0;
            double rhsValue = 0.0;
            if (!extractNumber(foldedLHS.get(), lhsValue) || !extractNumber(foldedRHS.get(), rhsValue)) {
               _logError(binaryExpr, "invalid operands in constant binary expression");
               return nullptr;
            }

            if (binaryExpr->op() == "+"sv)
               return makeFolded(lhsValue + rhsValue);
            if (binaryExpr->op() == "-"sv)
               return makeFolded(lhsValue - rhsValue);
            if (binaryExpr->op() == "*"sv)
               return makeFolded(lhsValue * rhsValue);
            if (binaryExpr->op() == "/"sv) {
               if (rhsValue == 0.0) {
                  _logError(binaryExpr, "division by zero in constant expression");
                  return nullptr;
               }
               return makeFolded(lhsValue / rhsValue);
            }
            if (binaryExpr->op() == "%"sv) {
               if (rhsValue == 0.0) {
                  _logError(binaryExpr, "modulo by zero in constant expression");
                  return nullptr;
               }

               if (lhsValue < 0.0 || rhsValue < 0.0) {
                  _logError(binaryExpr, "modulo in constant expression requires non-negative integer operands");
                  return nullptr;
               }

               const auto lhsInt = static_cast<uint64_t>(lhsValue);
               const auto rhsInt = static_cast<uint64_t>(rhsValue);
               if (static_cast<double>(lhsInt) != lhsValue || static_cast<double>(rhsInt) != rhsValue) {
                  _logError(binaryExpr, "modulo in constant expression requires integer operands");
                  return nullptr;
               }
               if (rhsInt == 0) {
                  _logError(binaryExpr, "modulo by zero in constant expression");
                  return nullptr;
               }

               return makeFolded(static_cast<double>(lhsInt % rhsInt));
            }

            if (binaryExpr->op() == "=="sv)
               return makeFolded(lhsValue == rhsValue ? 1.0 : 0.0);
            if (binaryExpr->op() == "!="sv)
               return makeFolded(lhsValue != rhsValue ? 1.0 : 0.0);
            if (binaryExpr->op() == "<"sv)
               return makeFolded(lhsValue < rhsValue ? 1.0 : 0.0);
            if (binaryExpr->op() == "<="sv)
               return makeFolded(lhsValue <= rhsValue ? 1.0 : 0.0);
            if (binaryExpr->op() == ">"sv)
               return makeFolded(lhsValue > rhsValue ? 1.0 : 0.0);
            if (binaryExpr->op() == ">="sv)
               return makeFolded(lhsValue >= rhsValue ? 1.0 : 0.0);

            if (binaryExpr->op() == "&&"sv)
               return makeFolded((lhsValue != 0.0 && rhsValue != 0.0) ? 1.0 : 0.0);
            if (binaryExpr->op() == "||"sv)
               return makeFolded((lhsValue != 0.0 || rhsValue != 0.0) ? 1.0 : 0.0);

            if (binaryExpr->op() == "&"sv || binaryExpr->op() == "|"sv || binaryExpr->op() == "^"sv ||
                binaryExpr->op() == "<<"sv || binaryExpr->op() == ">>"sv) {
               if (lhsValue < 0.0 || rhsValue < 0.0) {
                  _logError(binaryExpr, "bitwise operators in constant expression require non-negative integer operands");
                  return nullptr;
               }

               const auto lhsInt = static_cast<uint64_t>(lhsValue);
               const auto rhsInt = static_cast<uint64_t>(rhsValue);
               if (static_cast<double>(lhsInt) != lhsValue || static_cast<double>(rhsInt) != rhsValue) {
                  _logError(binaryExpr, "bitwise operators in constant expression require integer operands");
                  return nullptr;
               }

               if (binaryExpr->op() == "&"sv)
                  return makeFolded(static_cast<double>(lhsInt & rhsInt));
               if (binaryExpr->op() == "|"sv)
                  return makeFolded(static_cast<double>(lhsInt | rhsInt));
               if (binaryExpr->op() == "^"sv)
                  return makeFolded(static_cast<double>(lhsInt ^ rhsInt));
               if (binaryExpr->op() == "<<"sv) {
                  return makeFolded(static_cast<double>(lhsInt << rhsInt));
               }
               if (binaryExpr->op() == ">>"sv) {
                  return makeFolded(static_cast<double>(lhsInt >> rhsInt));
               }
            }

            _logError(binaryExpr, "unsupported binary operator '{}' in constant expression", binaryExpr->op());
            return nullptr;
         }

         default:
            _logError(expr.get(), "expression is not a valid constant expression");
            return nullptr;
      }
   }

   bool SemanticAnalyzer::_canCast(std::shared_ptr<Type> from, std::shared_ptr<Type> to) {
      if (fullTypeString(from) == fullTypeString(to))
         return true;
      if (_isNumberType(from->string()) && _isNumberType(to->string()))
         return true;
      if (from->kind() == TypeKind::Pointer && to->kind() == TypeKind::Pointer)
         return true; // probably will change
      if (from->kind() == TypeKind::Pointer && _isIntegerType(to->string()))
         return true;
      if (to->kind() == TypeKind::Pointer && _isIntegerType(from->string()))
         return true;
      return false;
   }

   template <typename... Ty>
   void SemanticAnalyzer::_logError(const ExpressionAST* node, const std::string& err, Ty&&... args) {
      const std::string message = std::vformat(err, std::make_format_args(args...));
      if (node && node->hasSource() && !_raw.empty()) {
         const bool isSemicolonError = message.find("expected ';'") != std::string::npos;

         const size_t linePosition = node->sourcePosition();
         const size_t errorPosition = isSemicolonError && node->sourceLength() > 0
             ? node->sourcePosition() + node->sourceLength()
             : node->sourcePosition();

         const size_t start = _raw.rfind('\n', linePosition) == std::string::npos ? 0 : _raw.rfind('\n', linePosition) + 1;
         const size_t end = _raw.find('\n', linePosition) == std::string::npos ? _raw.size() : _raw.find('\n', linePosition);

         std::string line = _raw.substr(start, end - start);
         size_t indentation = 0;
         for (char c : line) {
            if (c == ' ' || c == '\t')
               ++indentation;
            else
               break;
         }

         size_t column = _column(errorPosition) > indentation ? _column(errorPosition) - indentation : 0;
         size_t lineNum = _line(errorPosition);
         size_t lineLength = std::to_string(lineNum).length();

         std::cerr << _filePath.string() << ":" << lineNum << ":" << _column(errorPosition) << Colors::Red << " error: " << Colors::White << message << "\n";
         std::cerr << lineNum << " | " << line.substr(indentation) << "\n";
         std::cerr << " "sv * lineLength << " | " << " "sv * (column > 1 ? column - 1 : 0) << Colors::Green << "^" << Colors::Reset << "\n";
         if (isSemicolonError)
            std::cerr << " "sv * lineLength << " | " << " "sv * (column > 1 ? column - 1 : 0) << Colors::Green << ";" << Colors::Reset << "\n";
         std::cerr << "\n";
      } else {
         std::cerr << "error: " << message << "\n";
      }
      _hasErrors = true;
   }

   template <typename... Ty>
   void SemanticAnalyzer::_logError(const FunctionPrototypeAST* node, const std::string& err, Ty&&... args) {
      const std::string message = std::vformat(err, std::make_format_args(args...));
      if (node && node->hasSource() && !_raw.empty()) {
         const bool isSemicolonError = message.find("expected ';'") != std::string::npos;

         const size_t linePosition = node->sourcePosition();
         const size_t errorPosition = isSemicolonError && node->sourceLength() > 0
             ? node->sourcePosition() + node->sourceLength()
             : node->sourcePosition();

         const size_t start = _raw.rfind('\n', linePosition) == std::string::npos ? 0 : _raw.rfind('\n', linePosition) + 1;
         const size_t end = _raw.find('\n', linePosition) == std::string::npos ? _raw.size() : _raw.find('\n', linePosition);

         std::string line = _raw.substr(start, end - start);
         size_t indentation = 0;
         for (char c : line) {
            if (c == ' ' || c == '\t')
               ++indentation;
            else
               break;
         }

         size_t column = _column(errorPosition) > indentation ? _column(errorPosition) - indentation : 0;
         size_t lineNum = _line(errorPosition);
         size_t lineLength = std::to_string(lineNum).length();

         std::cerr << _filePath.string() << ":" << lineNum << ":" << _column(errorPosition) << Colors::Red << " error: " << Colors::White << message << "\n";
         std::cerr << lineNum << " | " << line.substr(indentation) << "\n";
         std::cerr << " "sv * lineLength << " | " << " "sv * (column > 1 ? column - 1 : 0) << Colors::Green << "^" << Colors::Reset << "\n";
         if (isSemicolonError)
            std::cerr << " "sv * lineLength << " | " << " "sv * (column > 1 ? column - 1 : 0) << Colors::Green << ";" << Colors::Reset << "\n";
         std::cerr << "\n";
      } else {
         std::cerr << "error: " << message << "\n";
      }
      _hasErrors = true;
   }

   template <typename... Ty>
   void SemanticAnalyzer::_logWarn(const ExpressionAST* node, const std::string& warn, Ty&&... args) {
      const std::string message = std::vformat(warn, std::make_format_args(args...));
      if (node && node->hasSource() && !_raw.empty()) {
         const size_t linePosition = node->sourcePosition();
         const size_t errorPosition = node->sourcePosition();

         const size_t start = _raw.rfind('\n', linePosition) == std::string::npos ? 0 : _raw.rfind('\n', linePosition) + 1;
         const size_t end = _raw.find('\n', linePosition) == std::string::npos ? _raw.size() : _raw.find('\n', linePosition);

         std::string line = _raw.substr(start, end - start);
         size_t indentation = 0;
         for (char c : line) {
            if (c == ' ' || c == '\t')
               ++indentation;
            else
               break;
         }

         size_t column = _column(errorPosition) > indentation ? _column(errorPosition) - indentation : 0;
         size_t lineNum = _line(errorPosition);
         size_t lineLength = std::to_string(lineNum).length();

         std::cerr << _filePath.string() << ":" << lineNum << ":" << _column(errorPosition) << Colors::Yellow << " warning: " << Colors::White << message << "\n";
         std::cerr << lineNum << " | " << line.substr(indentation) << "\n";
         std::cerr << " "sv * lineLength << " | " << " "sv * (column > 1 ? column - 1 : 0) << Colors::Green << "^" << Colors::Reset << "\n";
         std::cerr << "\n";
      } else {
         std::cerr << "warning: " << message << "\n";
      }
   }
} // namespace tungsten
