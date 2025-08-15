module;

#include <memory>
#include <vector>
#include <iostream>
#include <string>
#include <map>
#include <sstream>
#ifndef _NODISCARD
#define _NODISCARD [[nodiscard]]
#endif

export module Tungsten.semanticAnalyzer;
import Tungsten.token;
import Tungsten.ast;
import Tungsten.parser;

namespace tungsten {
   struct ElementType {
      SymbolType symbolType;
      std::string type;
   };

   export class SemanticAnalyzer : public ASTVisitor {
   public:
      SemanticAnalyzer(std::vector<std::unique_ptr<FunctionAST>> functions,
                       std::vector<std::unique_ptr<ClassAST>> classes, std::vector<std::unique_ptr<ExpressionAST>> globVars)
          : _functions{std::move(functions)}, _classes{std::move(classes)}, _globalVariables{std::move(globVars)} {}
      ~SemanticAnalyzer() = default;
      SemanticAnalyzer operator=(SemanticAnalyzer&) = delete;
      SemanticAnalyzer(SemanticAnalyzer&) = delete;

      bool analyze();

      void visit(NumberExpressionAST&) override {}
      void visit(VariableExpressionAST&) override;
      void visit(StringExpression&) override {}
      void visit(UnaryExpressionAST&) override {}
      void visit(BinaryExpressionAST&) override {}
      void visit(VariableDeclarationAST&) override;
      void visit(CallExpressionAST&) override {}
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
      void visit(BlockStatementAST&) override {}
      void visit(ReturnStatementAST&) override;
      void visit(ExitStatement&) override;
      void visit(ExternStatementAST&) override {}
      void visit(NamespaceAST&) override {}
      void visit(ImportStatementAST&) override {}

      void visit(FunctionPrototypeAST&) override {}
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
      void _print() { std::cerr << _errors.str() << _warnings.str(); }

      bool _isSignedType(const std::string& type);
      bool _isUnsignedType(const std::string& type);
      bool _isFloatingPointType(const std::string& type);
      bool _isNumberType(const std::string& type);
      bool _isBaseType(const std::string& type);
      bool _isClass(const std::string& cls);
      bool _checkNumericConversionLoss(const std::string& fromType, const std::string& toType);

      std::vector<std::unique_ptr<FunctionAST>> _functions;
      std::vector<std::unique_ptr<ClassAST>> _classes;
      std::vector<std::unique_ptr<ExpressionAST>> _globalVariables;
      std::map<std::string, ElementType> _symbolTable;
      std::stringstream _errors;
      std::stringstream _warnings;
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
      if (!_isBaseType(var.type()) && !_isClass(var.type()))
         return _logError("unknown type '" + var.type() + "' in variable declaration '" + var.type() + " " + var.name() + "'");

      if (var.initializer()) {
         var.initializer()->accept(*this);
         if (!_isNumberType(var.type()) || !_isNumberType(var.initializer()->type())) {
            if (var.type() != var.initializer()->type())
               return _logError("type mismatch: cannot assign '" + var.initializer()->type() + "' to variable of type '" + var.type() + "'");

         } else if (_checkNumericConversionLoss(var.initializer()->type(), var.type())) {
            _logWarn("possible data loss converting from '" + var.initializer()->type() + "' to '" + var.type() + "' in variable '" + var.name() + "'");
         }
      }

      _symbolTable[var.name()] = {SymbolType::Variable, var.type()};
   }

   void SemanticAnalyzer::visit(VariableExpressionAST& var) {
      if (!_symbolTable.contains(var.name()))
         return _logError("unkown variable '" + var.name() + "'");

      var.setType(_symbolTable[var.name()].type);
   }

   void SemanticAnalyzer::visit(FunctionAST& fun) {
      if (_symbolTable.contains(fun.name()) && _symbolTable[fun.name()].symbolType != SymbolType::Function)
         return _logError(std::string(_symbolTable[fun.name()].symbolType == SymbolType::Variable ? "Variable" : "Class") + " with name " + fun.name() + "' already exists");

      for (auto& expr : static_cast<BlockStatementAST*>(fun.body().get())->statements()) {
         expr->accept(*this);
      }

      _symbolTable[fun.name()] = {SymbolType::Function, fun.type()};
   }

   void SemanticAnalyzer::visit(ExitStatement& ext) {
      ext.value()->accept(*this);
      if (!_isSignedType(ext.value()->type()) && !_isUnsignedType(ext.value()->type()))
         return _logError("exit statement expects a numeric value");
   }

   void SemanticAnalyzer::visit(ReturnStatementAST& ret) {
      ret.value()->accept(*this);
      if (ret.type() == "Void") {
         if (ret.value() != nullptr)
            return _logError("'Void' function cannot return '" + ret.value()->type() + "'");
         return;
      }

      if (!_isNumberType(ret.type()) || !_isNumberType(ret.value()->type())) {
         if (ret.type() != ret.value()->type())
            return _logError("'" + ret.type() + "' function cannot return '" + ret.value()->type() + "'");
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
      return _isNumberType(type) || type == "String" || type == "Char" || type == "Void";
   }
   bool SemanticAnalyzer::_isClass(const std::string& cls) {
      if (!_symbolTable.contains(cls))
         return false;
      if (_symbolTable[cls].symbolType != SymbolType::Class) {
         for (auto& clss : _classes) {
            if (clss->name() == cls)
               return true;
         }
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

} // namespace tungsten