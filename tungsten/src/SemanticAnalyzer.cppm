module;

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

      void visit(NumberExpressionAST&) override {}
      void visit(VariableExpressionAST&) override;
      void visit(StringExpression&) override {}
      void visit(UnaryExpressionAST&) override {}
      void visit(BinaryExpressionAST&) override {}
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
      void visit(IfStatementAST&) override {}
      void visit(WhileStatementAST&) override {}
      void visit(DoWhileStatementAST&) override {}
      void visit(ForStatementAST&) override {}
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
      void _logError(const std::string& err) {
         _errors << "error: " << err << "\n";
         _hasErrors = true;
      }
      void _logWarn(const std::string& warn) { _warnings << "warning: " << warn << "\n"; }
      void _print() const { std::cerr << _errors.str() << _warnings.str(); }

      bool _isSignedType(const std::string& type);
      bool _isUnsignedType(const std::string& type);
      bool _isFloatingPointType(const std::string& type);
      bool _isNumberType(const std::string& type);
      bool _isBaseType(const std::string& type);
      bool _isClass(const std::string& cls);
      bool _isFunction(const std::string& fn);
      bool _checkNumericConversionLoss(const std::string& fromType, const std::string& toType);
      bool _doesVariableExist(const std::string& var);
      std::string _fullTypeString(const std::shared_ptr<Type>& ty);
      std::string _baseType(const std::shared_ptr<Type>& ty);

      std::vector<std::unique_ptr<FunctionAST>>& _functions;
      std::vector<std::unique_ptr<ClassAST>>& _classes;
      std::vector<std::unique_ptr<ExpressionAST>>& _globalVariables;
      std::vector<Scope> _scopes{};
      std::unordered_map<std::string, std::vector<Overload>> _declaredFunctions{};
      std::stringstream _errors{};
      std::stringstream _warnings{};
      size_t _currentScope{GlobalScope};
      bool _hasErrors{false};
   };

   //  ========================================== implementation ==========================================

   bool SemanticAnalyzer::analyze() {
      for (auto& var : _globalVariables) {
         var->accept(*this);
      }
      for (auto& cls : _classes) {
         cls->accept(*this);
      }
      for (auto& fun : _functions) {
         fun->accept(*this);
      }
      _print();

      return !_hasErrors;
   }

   void SemanticAnalyzer::visit(VariableDeclarationAST& var) {
      if (_doesVariableExist(var.name()))
         return _logError("variable '" + var.name() + "' already exists");

      switch (var.type()->kind()) {
         case TypeKind::Array: {
            auto array = static_cast<ArrayTy*>(var.type().get());
            if (!_isBaseType(_baseType(array->arrayType())) && !_isClass(_baseType(array->arrayType())))
               return _logError("unknown type '" + _baseType(array->arrayType()) + "' in variable declaration '" + _fullTypeString(var.type()) + " " + var.name() + "'");
         } break;
         case TypeKind::Pointer: {
            auto ptr = static_cast<PointerTy*>(var.type().get());
            if (!_isBaseType(_baseType(ptr->pointee())) && !_isClass(_baseType(ptr->pointee())))
               return _logError("unknown type '" + _baseType(ptr->pointee()) + "' in variable declaration '" + _fullTypeString(var.type()) + " " + var.name() + "'");
         } break;
         default:
            if (!_isBaseType(var.type()->string()) && !_isClass(var.type()->string()))
               return _logError("unknown type '" + var.type()->string() + "' in variable declaration '" + var.type()->string() + " " + var.name() + "'");
            break;
      }

      if (var.initializer()) {
         var.initializer()->accept(*this);
         if (!_isNumberType(var.type()->string()) || !_isNumberType(var.initializer()->type()->string())) {
            if (_fullTypeString(var.type()) != _fullTypeString(var.initializer()->type()))
               return _logError("type mismatch: cannot assign '" + _fullTypeString(var.initializer()->type()) + "' to variable of type '" + _fullTypeString(var.type()) + "'");

         } else if (_checkNumericConversionLoss(_baseType(var.initializer()->type()), _baseType(var.type()))) {
            _logWarn("possible data loss converting from '" + _baseType(var.initializer()->type()) + "' to '" + _baseType(var.type()) + "' in variable '" + var.name() + "'");
         }
      }

      _scopes.at(_currentScope)[var.name()] = var.type();
   }

   void SemanticAnalyzer::visit(VariableExpressionAST& var) {
      if (!_doesVariableExist(var.name()))
         return _logError("unknown variable '" + var.name() + "'");

      var.setType(_scopes.at(_currentScope).at(var.name()));
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
         return _logError(std::string(_isClass(proto.name()) ? "class" : "variable") + " with name " + proto.name() + "' already exists");

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
            if (_fullTypeString(o.args[i].second) != _fullTypeString(over.args[i].second))
               return false;
         }
         return true;
      };

      for (auto& existing : overloads) {
         if (sameSignature(existing)) {
            return _logError("function '" + proto.name() + "' with the same signature already exists");
         }
      }
      overloads.push_back(over);
   }

   void SemanticAnalyzer::visit(FunctionAST& fun) {
      _scopes.push_back({}); // scope already created inside visit(BlockStatementAST&) but i need to make one for the function arguments
      ++_currentScope;
      fun.prototype()->accept(*this);
      fun.body()->accept(*this);
      _scopes.pop_back();
      --_currentScope;
   }

   void SemanticAnalyzer::visit(CallExpressionAST& call) {
      auto overloadsIt = _declaredFunctions.find(call.callee());
      std::vector<Overload> overloads;
      if (overloadsIt != _declaredFunctions.end()) {
         overloads = overloadsIt->second;
      } else {
         for (auto& fun : _functions) {
            if (fun->name() == call.callee()) {
               Overload over;
               over.type = fun->prototype()->type();
               for (auto& arg : fun->prototype()->args()) {
                  auto cast = static_cast<VariableDeclarationAST*>(arg.get());
                  over.args.push_back({cast->name(), cast->type()});
               }
               overloads.push_back(over);
            }
         }
         if (overloads.empty())
            return _logError("unknown function '" + call.callee() + "'");
      }

      bool valid = false;
      for (auto& overload : overloads) {
         if (overload.args.size() != call.args().size())
            continue;
         bool match = true;
         for (size_t i = 0; i < call.args().size(); ++i) {
            if (_fullTypeString(call.args()[i]->type()) != _fullTypeString(overload.args[i].second)) {
               match = false;
               break;
            }
         }
         if (match) {
            valid = true;
            break;
         }
      }

      if (!valid) {
         std::string args;
         for (size_t i = 0; i < call.args().size(); ++i) {
            args += _fullTypeString(call.args().at(i)->type()) + (i == call.args().size() - 1 ? "" : ", ");
         }
         if (args.empty())
            return _logError("no overload of function '" + call.callee() + "' with no arguments");
         return _logError("no overload of function '" + call.callee() + "' with arguments '" + args + "'");
      }
   }

   void SemanticAnalyzer::visit(ExitStatement& ext) {
      ext.value()->accept(*this);
      if (!ext.value()->type()) {
         if (auto num = dynamic_cast<NumberExpressionAST*>(ext.value().get())) {
            num->setType(makeInt32());
         }
      }
      if (!_isSignedType(ext.value()->type()->string()) && !_isUnsignedType(ext.value()->type()->string()))
         return _logError("exit statement expects a numeric value");
   }

   void SemanticAnalyzer::visit(ReturnStatementAST& ret) {
      if (ret.type()->kind() == TypeKind::Void) {
         if (ret.value() != nullptr) {
            ret.value()->accept(*this);
            return _logError("'Void' function cannot return '" + _fullTypeString(ret.value()->type()) + "'");
         }
         return;
      }

      ret.value()->accept(*this);
      if (!ret.value()->type()) {
         if (auto num = dynamic_cast<NumberExpressionAST*>(ret.value().get())) {
            if (_isNumberType(ret.type()->string()))
               num->setType(ret.type());
            else
               num->setType(makeInt32());
         }
      }
      if (!_isNumberType(ret.type()->string()) || !_isNumberType(ret.value()->type()->string())) {
         if (ret.type()->string() != ret.value()->type()->string())
            return _logError("'" + _fullTypeString(ret.type()) + "' function cannot return '" + _fullTypeString(ret.value()->type()) + "'");
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
   std::string SemanticAnalyzer::_fullTypeString(const std::shared_ptr<Type>& ty) {
      switch (ty->kind()) {
         case TypeKind::Pointer: {
            auto ptrTy = std::static_pointer_cast<PointerTy>(ty);
            return _fullTypeString(ptrTy->pointee()) + "*";
         }
         case TypeKind::Array: {
            auto arrTy = std::static_pointer_cast<ArrayTy>(ty);
            return _fullTypeString(arrTy->arrayType()) + "[]";
         }
         default:
            return ty->string();
      }
   }
   std::string SemanticAnalyzer::_baseType(const std::shared_ptr<Type>& ty) {
      switch (ty->kind()) {
         case TypeKind::Pointer: {
            auto ptrTy = std::static_pointer_cast<PointerTy>(ty);
            return _baseType(ptrTy->pointee());
         }
         case TypeKind::Array: {
            auto arrTy = std::static_pointer_cast<ArrayTy>(ty);
            return _baseType(arrTy->arrayType());
         }
         default:
            return ty->string();
      }
   }
} // namespace tungsten