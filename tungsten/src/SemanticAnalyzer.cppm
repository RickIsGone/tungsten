module;

#include <format>
#include <memory>
#include <vector>
#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>
#ifndef _NODISCARD
#define _NODISCARD [[nodiscard]]
#endif

export module Tungsten.semanticAnalyzer;
import Tungsten.token;
import Tungsten.ast;
import Tungsten.parser;

namespace tungsten {
   using Scope = std::unordered_map<std::string, std::shared_ptr<Type>>;
   struct Overload {
      std::shared_ptr<Type> type;
      std::vector<std::pair<std::string, std::shared_ptr<Type>>> args;
   };
   constexpr size_t GlobalScope = 0;

   export class SemanticAnalyzer : public ASTVisitor {
   public:
      SemanticAnalyzer(std::vector<std::unique_ptr<FunctionAST>>& functions,
                       std::vector<std::unique_ptr<ClassAST>>& classes, std::vector<std::unique_ptr<ExpressionAST>>& globVars)
          : _functions{functions}, _classes{classes}, _globalVariables{globVars} { _scopes.push_back({}); }
      SemanticAnalyzer operator=(SemanticAnalyzer&) = delete;
      SemanticAnalyzer(SemanticAnalyzer&) = delete;

      bool analyze();

      void visit(NumberExpressionAST&) override;
      void visit(VariableExpressionAST&) override;
      void visit(StringExpression&) override {}
      void visit(UnaryExpressionAST&) override;
      void visit(BinaryExpressionAST&) override;
      void visit(VariableDeclarationAST&) override;
      void visit(CallExpressionAST&) override;
      void visit(TypeOfStatementAST&) override {}
      void visit(NameOfStatementAST&) override {}
      void visit(SizeOfStatementAST&) override {}
      void visit(__BuiltinFunctionAST&) override {}
      void visit(__BuiltinLineAST&) override {}
      void visit(__BuiltinColumnAST&) override {}
      void visit(__BuiltinFileAST&) override {}
      void visit(StaticCastAST&) override {}
      void visit(ConstCastAST&) override {}
      void visit(IfStatementAST&) override;
      void visit(WhileStatementAST&) override;
      void visit(DoWhileStatementAST&) override;
      void visit(ForStatementAST&) override;
      void visit(BlockStatementAST&) override;
      void visit(ReturnStatementAST&) override;
      void visit(ExitStatement&) override;
      void visit(ExternStatementAST&) override {}
      void visit(NamespaceAST&) override {}
      void visit(ImportStatementAST&) override {}

      void visit(FunctionPrototypeAST&) override;
      void visit(FunctionAST&) override;

      void visit(ClassMethodAST&) override {}
      void visit(ClassVariableAST&) override {}
      void visit(ClassConstructorAST&) override {}
      void visit(ClassDestructorAST&) override {}
      void visit(ClassAST&) override {}

   private:
      template <typename... Ty>
      void _logError(const std::string& err, Ty&&... args) {
         std::cerr << "error: " << std::vformat(err, std::make_format_args(args...)) << "\n";
         _hasErrors = true;
      }
      template <typename... Ty>
      void _logWarn(const std::string& warn, Ty&&... args) { std::cerr << "warning: " << std::vformat(warn, std::make_format_args(args...)) << "\n"; }

      bool _isSignedType(const std::string& type);
      bool _isUnsignedType(const std::string& type);
      bool _isFloatingPointType(const std::string& type);
      bool _isNumberType(const std::string& type);
      bool _isBaseType(const std::string& type);
      bool _isClass(const std::string& cls);
      bool _isFunction(const std::string& fn);
      bool _checkNumericConversionLoss(const std::string& fromType, const std::string& toType);
      bool _doesVariableExist(const std::string& var);
      std::shared_ptr<Type> _variableType(const std::string& var);

      std::vector<std::unique_ptr<FunctionAST>>& _functions;
      std::vector<std::unique_ptr<ClassAST>>& _classes;
      std::vector<std::unique_ptr<ExpressionAST>>& _globalVariables;
      std::vector<Scope> _scopes{};
      std::unordered_map<std::string, std::vector<Overload>> _declaredFunctions{};
      size_t _currentScope{GlobalScope};
      bool _hasErrors{false};
   };

   //  ========================================== implementation ==========================================

   bool SemanticAnalyzer::analyze() {
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
      if (_isFunction(var.name()) || _isClass(var.name()))
         return _logError("{} with name '{}' already exists", _isFunction(var.name()) ? "function" : "class", var.name());
      if (_doesVariableExist(var.name()))
         return _logError("variable '{}' already exists", var.name());

      switch (var.type()->kind()) {
         case TypeKind::Array: {
            auto array = static_cast<ArrayTy*>(var.type().get());
            if (!_isBaseType(baseType(array->arrayType())) && !_isClass(baseType(array->arrayType())))
               return _logError("unknown type '{}' in variable declaration '{} {}'", baseType(array->arrayType()), fullTypeString(var.type()), var.name());
         } break;
         case TypeKind::Pointer: {
            auto ptr = static_cast<PointerTy*>(var.type().get());
            if (!_isBaseType(baseType(ptr->pointee())) && !_isClass(baseType(ptr->pointee())))
               return _logError("unknown type '{}' in variable declaration '{} {}'", baseType(ptr->pointee()), fullTypeString(var.type()), var.name());
         } break;
         default:
            if (!_isBaseType(var.type()->string()) && !_isClass(var.type()->string()))
               return _logError("unknown type '{}' in variable declaration '{} {}'", var.type()->string(), fullTypeString(var.type()), var.name());
            break;
      }

      if (var.initializer()) {
         var.initializer()->accept(*this);
         if (!_isNumberType(var.type()->string()) || !_isNumberType(var.initializer()->type()->string())) {
            if (fullTypeString(var.type()) != fullTypeString(var.initializer()->type()))
               return _logError("type mismatch: cannot assign '{}' to variable of type '{}'", fullTypeString(var.initializer()->type()), fullTypeString(var.type()));

         } /*else if (_checkNumericConversionLoss(baseType(var.initializer()->type()), baseType(var.type()))) {
            _logWarn("possible data loss converting from '{}' to '{}' in variable ''", baseType(var.initializer()->type()), baseType(var.type()), var.name());
         }*/
      }

      _scopes.at(_currentScope)[var.name()] = var.type();
   }

   void SemanticAnalyzer::visit(NumberExpressionAST& num) {
      if (!num.type())
         num.setType(makeDouble()); // only for now, will be changed later
   }

   void SemanticAnalyzer::visit(UnaryExpressionAST& op) {
      op.operand()->accept(*this);

      if (op.op() == "--" || op.op() == "++") {
         if (op.operand()->astType() != ASTType::VariableExpression)
            return _logError("cannot use operator '{}' on non-variable expression", op.op());
      }
      if (op.op() == "-") {
         if (op.operand()->astType() != ASTType::NumberExpression)
            return _logError("cannot use operator '-' on non-numeric expression");
      }
      // if (op.operand()->astType() !=)
   }

   void SemanticAnalyzer::visit(BinaryExpressionAST& binop) {
      binop.LHS()->accept(*this);
      binop.RHS()->accept(*this);


      if (binop.op() == "==" || binop.op() == "!=" || binop.op() == "<" || binop.op() == ">" || binop.op() == "<=" || binop.op() == ">=") {
         if (!_isNumberType(fullTypeString(binop.LHS()->type())) || !_isNumberType(fullTypeString(binop.RHS()->type()))) {
            if (fullTypeString(binop.LHS()->type()) != fullTypeString(binop.RHS()->type()))
               return _logError("type mismatch: cannot compare '{}' and '{}' with operator '{}'", fullTypeString(binop.LHS()->type()), fullTypeString(binop.RHS()->type()), binop.op());
            if (fullTypeString(binop.LHS()->type()) == "String")
               return _logError("cannot do comparison between strings with operator '{}'", binop.op());
         }
         binop.setType(makeBool());
         return;
      }
      if (binop.op() == "&&" || binop.op() == "||") {
         if (binop.LHS()->type()->kind() != TypeKind::Bool || binop.RHS()->type()->kind() != TypeKind::Bool)
            return _logError("type mismatch: cannot compare '{}' and '{}' with operator '{}'", fullTypeString(binop.LHS()->type()), fullTypeString(binop.RHS()->type()), binop.op());
         binop.setType(makeBool());
         return;
      }

      if (binop.op() == "=" || binop.op() == "+=" || binop.op() == "-=" || binop.op() == "*=" || binop.op() == "/=" || binop.op() == "%=" || binop.op() == "|=" || binop.op() == "&=") {
         if (!binop.LHS()->isLValue())
            return _logError("left operand of assignment must be a variable");
      }

      // addition, multiplication, division, ecc.
      if (!_isNumberType(fullTypeString(binop.LHS()->type())) || !_isNumberType(fullTypeString(binop.RHS()->type()))) {
         if (fullTypeString(binop.LHS()->type()) != fullTypeString(binop.RHS()->type()))
            return _logError("type mismatch: cannot operate '{}' and '{}' with operator '{}'", fullTypeString(binop.LHS()->type()), fullTypeString(binop.RHS()->type()), binop.op());

         if (fullTypeString(binop.LHS()->type()) == "String" && binop.op() != "=")
            return _logError("strings can only be assigned, not operated on with '{}'", binop.op());
      }

      binop.setType(binop.LHS()->type());
   }

   void SemanticAnalyzer::visit(VariableExpressionAST& var) {
      if (!_doesVariableExist(var.name()))
         return _logError("unknown variable '{}'", var.name());

      var.setType(_variableType(var.name()));
   }

   void SemanticAnalyzer::visit(BlockStatementAST& block) {
      _scopes.push_back({});
      ++_currentScope;
      for (auto& expr : block.statements()) {
         expr->accept(*this);
      }
      _scopes.pop_back();
      --_currentScope;
   }

   void SemanticAnalyzer::visit(FunctionPrototypeAST& proto) {
      if (_scopes.at(GlobalScope).contains(proto.name()) || _isClass(proto.name()))
         return _logError("{} with name {}' already exists", _isClass(proto.name()) ? "class" : "variable", proto.name());

      if (_declaredFunctions.contains("main") && proto.name() == "main")
         return _logError("redefinition of main function");

      Overload over;
      over.type = proto.type();
      for (auto& arg : proto.args()) {
         auto cast = static_cast<VariableDeclarationAST*>(arg.get());
         cast->accept(*this);
         over.args.push_back({cast->name(), cast->type()});
      }
      auto& overloads = _declaredFunctions[proto.name()];

      auto sameSignature = [&](const Overload& o) {
         if (o.args.size() != over.args.size()) return false;
         for (size_t i = 0; i < o.args.size(); i++) {
            if (fullTypeString(o.args[i].second) != fullTypeString(over.args[i].second))
               return false;
         }
         return true;
      };
      auto sameParams = [&](const Overload& o) {
         if (o.args.size() != over.args.size()) return false;
         for (size_t i = 0; i < o.args.size(); i++) {
            if (fullTypeString(o.args[i].second) != fullTypeString(over.args[i].second))
               return false;
         }
         return true;
      };

      for (auto& existing : overloads) {
         if (sameSignature(existing)) {
            return _logError("function '{}' with the same signature already exists", proto.name());
         }
         if (sameParams(existing) && fullTypeString(existing.type) != fullTypeString(over.type)) {
            return _logError("function '{}' with same parameters but different return type already exists", proto.name());
         }
      }

      if (proto.name() == "main") {
         if (fullTypeString(proto.type()) != "Double")
            return _logError("main function must have return type 'Num'"); // will replace with Int32 after reintroducing numeric types
         proto.setType(makeInt32());
      }

      overloads.push_back(over);
   }

   void SemanticAnalyzer::visit(FunctionAST& fun) {
      _scopes.push_back({}); // scope already created inside visit(BlockStatementAST&) but I need to make one for the function arguments
      ++_currentScope;
      fun.prototype()->accept(*this);
      // temporary fix because of the single numeric type being a double
      if (fun.name() == "main") {
         if (fun.body()) {
            for (auto& expr : static_cast<BlockStatementAST*>(fun.body().get())->statements()) {
               if (expr->astType() == ASTType::ReturnStatement)
                  static_cast<ReturnStatementAST*>(expr.get())->setType(makeInt32());
            }
         }
      }
      fun.body()->accept(*this);
      _scopes.pop_back();
      --_currentScope;
   }

   void SemanticAnalyzer::visit(CallExpressionAST& call) {
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
               over.args.push_back({cast->name(), cast->type()});
            }
            overloads.push_back(over);
         }
      }
      if (overloads.empty()) {
         call.setType(makeNullType()); // temporary fix (if I don't fucking forget to fix it *again*)
         return _logError("unknown function '{}'", call.callee());
      }

      for (auto& arg : call.args()) {
         arg->accept(*this);
      }

      bool valid = false;
      for (auto& overload : overloads) {
         if (overload.args.size() != call.args().size())
            continue;
         bool match = true;
         for (size_t i = 0; i < call.args().size(); ++i) {
            if (fullTypeString(call.args()[i]->type()) != fullTypeString(overload.args[i].second)) {
               match = false;
               break;
            }
         }
         if (match) {
            valid = true;
            call.setType(overload.type);
            break;
         }
      }

      if (!valid) {
         std::string args;
         for (size_t i = 0; i < call.args().size(); ++i) {
            args += fullTypeString(call.args().at(i)->type()) + (i == call.args().size() - 1 ? "" : ", ");
         }
         call.setType(makeNullType());
         if (args.empty())
            return _logError("no overload of function '{}' with no arguments", call.callee());
         return _logError("no overload of function '{}' with arguments '{}'", call.callee(), args);
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
            return _logError("if statement condition must be a numeric or boolean expression");
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
            return _logError("while statement condition must be a numeric or boolean expression");
      }

      whileStmnt.body()->accept(*this);
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
            return _logError("while statement condition must be a numeric or boolean expression");
      }

      doWhile.body()->accept(*this);
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
            return _logError("for statement initialization must be a variable declaration or a binary expression");
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
               return _logError("while statement condition must be a numeric or boolean expression");
            break;
         default:
            return _logError("for statement condition must be a numeric or boolean expression");
      }
      forStmnt.increment()->accept(*this);
      switch (forStmnt.increment()->astType()) {
         case ASTType::BinaryExpression:
         case ASTType::UnaryExpression:
            break;
         default:
            return _logError("for statement increment must be a binary expression");
      }
      forStmnt.body()->accept(*this);
      _scopes.pop_back();
      --_currentScope;
   }

   void SemanticAnalyzer::visit(ExitStatement& ext) {
      ext.value()->accept(*this);
      if (!ext.value()->type()) {
         if (ext.value()->astType() == ASTType::NumberExpression)
            static_cast<NumberExpressionAST*>(ext.value().get())->setType(makeInt32());
      }
      if (!_isNumberType(ext.value()->type()->string()))
         return _logError("exit statement expects a numeric value");
   }

   void SemanticAnalyzer::visit(ReturnStatementAST& ret) {
      if (ret.type()->kind() == TypeKind::Void) {
         if (ret.value() != nullptr) {
            ret.value()->accept(*this);
            return _logError("'Void' function cannot return '{}'", fullTypeString(ret.value()->type()));
         }
         return;
      }

      ret.value()->accept(*this);
      if (!ret.value()->type()) {
         if (ret.value()->astType() == ASTType::NumberExpression) {
            if (_isNumberType(ret.type()->string()))
               static_cast<NumberExpressionAST*>(ret.value().get())->setType(ret.type());
            else
               static_cast<NumberExpressionAST*>(ret.value().get())->setType(makeInt32());
            return;
         }
      }
      if (!_isNumberType(ret.type()->string()) || !_isNumberType(ret.value()->type()->string())) {
         if (ret.type()->string() != ret.value()->type()->string())
            return _logError("'{}' function cannot return '{}'", fullTypeString(ret.type()), fullTypeString(ret.value()->type()));
      } else {
         if (_checkNumericConversionLoss(baseType(ret.value()->type()), baseType(ret.type())))
            _logWarn("possible data loss converting from '{}' to '{}' in return statement", baseType(ret.value()->type()), baseType(ret.type()));
      }
   }

   bool SemanticAnalyzer::_isSignedType(const std::string& type) {
      return type == "Int" || type == "Int8" || type == "Int16" || type == "Int32" || type == "Int64" || type == "Int128";
   }
   bool SemanticAnalyzer::_isUnsignedType(const std::string& type) {
      return type == "Uint" || type == "Uint8" || type == "Uint16" || type == "Uint32" || type == "Uint64" || type == "Uint128";
   }
   bool SemanticAnalyzer::_isFloatingPointType(const std::string& type) {
      return type == "Float" || type == "Double";
   }
   bool SemanticAnalyzer::_isNumberType(const std::string& type) {
      return _isSignedType(type) || _isUnsignedType(type) || _isFloatingPointType(type);
   }
   bool SemanticAnalyzer::_isBaseType(const std::string& type) {
      return _isNumberType(type) || type == "String" || type == "Char" || type == "Void" || type == "Bool";
   }
   bool SemanticAnalyzer::_isClass(const std::string& cls) {
      for (auto& clss : _classes) {
         if (clss->name() == cls)
            return true;
      }
      return false;
   }
   bool SemanticAnalyzer::_isFunction(const std::string& fn) {
      for (auto& fun : _functions) {
         if (fun->name() == fn)
            return true;
      }
      return false;
   }
   bool SemanticAnalyzer::_checkNumericConversionLoss(const std::string& fromType, const std::string& toType) {
      if (fromType == toType) return false;

      if (_isNumberType(fromType) && _isNumberType(toType)) {
         // Float/Double → non floating point
         if (_isFloatingPointType(fromType) && !_isFloatingPointType(toType))
            return true;

         // Double → Float
         if (fromType == "Double" && toType == "Float")
            return true;

         auto bitSize = [](const std::string& t) -> int {
            if (t == "Int8" || t == "Uint8") return 8;
            if (t == "Int16" || t == "Uint16") return 16;
            if (t == "Int32" || t == "Uint32" || t == "Int" || t == "Uint") return 32;
            if (t == "Int64" || t == "Uint64") return 64;
            if (t == "Int128" || t == "Uint128") return 128;
            if (t == "Float") return 32;
            if (t == "Double") return 64;
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
      for (auto scope : _scopes) {
         if (scope.contains(var))
            return true;
      }
      return false;
   }
   std::shared_ptr<Type> SemanticAnalyzer::_variableType(const std::string& var) {
      for (auto& scope : _scopes) {
         if (scope.contains(var))
            return scope.at(var);
      }
      return nullptr;
   }

} // namespace tungsten