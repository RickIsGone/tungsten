module;

#include <memory>
#include <vector>
#include <iostream>
#include <string>
#include <map>
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
      void visit(VariableExpressionAST&) override {}
      void visit(StringExpression&) override {}
      void visit(UnaryExpressionAST&) override {}
      void visit(BinaryExpressionAST&) override {}
      void visit(VariableDeclarationAST&) override {}
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
      void visit(ReturnStatementAST&) override {}
      void visit(ExitStatement&) override {}
      void visit(ExternStatementAST&) override {}
      void visit(NamespaceAST&) override {}

      void visit(FunctionPrototypeAST&) override {}
      void visit(FunctionAST&) override {}

      void visit(ClassMethodAST&) override {}
      void visit(ClassVariableAST&) override {}
      void visit(ClassConstructorAST&) override {}
      void visit(ClassDestructorAST&) override {}
      void visit(ClassAST&) override {}

   private:
      void _analyzeVariableDeclaration(VariableDeclarationAST* var);
      void _analyzeClass(ClassAST* cls);
      void _analyzeFunction(FunctionAST* fun);

      bool _isSignedType(const std::string& type);
      bool _isUnsignedType(const std::string& type);
      bool _isFloatingPointType(const std::string& type);
      bool _isNumberType(const std::string& type);

      std::vector<std::unique_ptr<FunctionAST>> _functions;
      std::vector<std::unique_ptr<ClassAST>> _classes;
      std::vector<std::unique_ptr<ExpressionAST>> _globalVariables;
      std::map<std::string, ElementType> _symbolTable;
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
      return !_hasErrors;
   }

   void SemanticAnalyzer::_analyzeClass(ClassAST* cls) {
   }

   void SemanticAnalyzer::_analyzeFunction(FunctionAST* fun) {
      for (auto& expr : static_cast<BlockStatementAST*>(fun->body().get())->statements()) {
         if (auto var = dynamic_cast<VariableDeclarationAST*>(expr.get())) {
            _analyzeVariableDeclaration(var);
         } else if (auto binop = dynamic_cast<BinaryExpressionAST*>(expr.get())) {
         } else if (auto ifStatement = dynamic_cast<IfStatementAST*>(expr.get())) {
         } else if (auto whileStatement = dynamic_cast<WhileStatementAST*>(expr.get())) {
         } else if (auto doWhileStatement = dynamic_cast<DoWhileStatementAST*>(expr.get())) {
         } else if (auto forStatement = dynamic_cast<ForStatementAST*>(expr.get())) {
         } else if (auto funcCall = dynamic_cast<CallExpressionAST*>(expr.get())) {
         } else if (auto blockStatement = dynamic_cast<BlockStatementAST*>(expr.get())) {
         } else if (auto returnStatement = dynamic_cast<ReturnStatementAST*>(expr.get())) {
         } else if (auto exitStatement = dynamic_cast<ExitStatement*>(expr.get())) {
         } else if (auto unaryop = dynamic_cast<UnaryExpressionAST*>(expr.get())) {
         } else
            _hasErrors = true;
      }
   }

   void SemanticAnalyzer::_analyzeVariableDeclaration(VariableDeclarationAST* var) {
      if (var->initializer().get()) {
         if (auto num = dynamic_cast<NumberExpressionAST*>(var->initializer().get())) {
            if (_isNumberType(var->type()))
               num->setType(var->type());
            else
               _hasErrors = true;
         } else if (auto str = dynamic_cast<StringExpression*>(var->initializer().get())) {
            if (var->type() != "String")
               _hasErrors = true;
         } else if (auto binop = dynamic_cast<BinaryExpressionAST*>(var->initializer().get())) {
            binop->setType(var->type());
         } else if (auto staticCast = dynamic_cast<StaticCastAST*>(var->initializer().get())) {
         } else if (auto constCast = dynamic_cast<ConstCastAST*>(var->initializer().get())) {
         } else if (auto call = dynamic_cast<CallExpressionAST*>(var->initializer().get())) {
         } else if (auto other = dynamic_cast<VariableExpressionAST*>(var->initializer().get())) {
            if (_symbolTable.contains(other->name())) {
               if (!_isNumberType(var->type()) || !_isNumberType(_symbolTable[other->name()].type)) {
                  _hasErrors = true;
               } else if (var->type() != _symbolTable[other->name()].type) { // strings and classes
                  _hasErrors = true;
               }
            } else {
               _hasErrors = true;
            }
         }
      }

      _symbolTable[var->name()] = {SymbolType::Variable, var->type()};
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

} // namespace tungsten