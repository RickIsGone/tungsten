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

export namespace tungsten {
   enum class SymbolType {
      Variable,
      Function,
      Class
   };
   struct ElementType {
      SymbolType symbolType;
      std::string type;
   };

   class SemanticAnalyzer {
   public:
      SemanticAnalyzer(std::vector<std::unique_ptr<FunctionAST>> functions,
                       std::vector<std::unique_ptr<ClassAST>> classes, std::vector<std::unique_ptr<ExpressionAST>> globVars)
          : _functions{std::move(functions)}, _classes{std::move(classes)}, _globalVariables{std::move(globVars)} {}
      ~SemanticAnalyzer() = default;
      SemanticAnalyzer operator=(SemanticAnalyzer&) = delete;
      SemanticAnalyzer(SemanticAnalyzer&) = delete;

      bool analyze();

   private:
      bool _analyzeVariableDeclaration(VariableDeclarationAST* var);
      bool _analyzeClass(ClassAST* cls);
      bool _analyzeFunction(FunctionAST* fun);

      bool _isSignedType(const std::string& type);
      bool _isUnsignedType(const std::string& type);
      bool _isFloatingPointType(const std::string& type);
      bool _isNumberType(const std::string& type);

      std::vector<std::unique_ptr<FunctionAST>> _functions;
      std::vector<std::unique_ptr<ClassAST>> _classes;
      std::vector<std::unique_ptr<ExpressionAST>> _globalVariables;
      std::map<std::string, ElementType> _symbolTable;
   };

   //  ========================================== implementation ==========================================

   bool SemanticAnalyzer::analyze() {
      bool isOk = true;

      for (auto& var : _globalVariables) {
         auto castVar = static_cast<VariableDeclarationAST*>(var.get());
         std::cout << castVar->type() << " " << castVar->name() << "\n";
         if (!_analyzeVariableDeclaration(castVar))
            isOk = false;
      }
      std::cout << "\n";
      for (auto& cls : _classes) {
         std::cout << "class " << cls->name() << "\n";
         if (!_analyzeClass(cls.get()))
            isOk = false;
      }
      std::cout << "\n";
      for (auto& fun : _functions) {
         std::cout << fun->type() << " " << fun->name() << "()\n";
         if (!_analyzeFunction(fun.get()))
            isOk = false;
      }
      return isOk;
   }

   bool SemanticAnalyzer::_analyzeClass(ClassAST* cls) {
      return true;
   }

   bool SemanticAnalyzer::_analyzeFunction(FunctionAST* fun) {
      for (auto& expr : static_cast<BlockStatementAST*>(fun->body().get())->statements()) {
         if (auto var = dynamic_cast<VariableDeclarationAST*>(expr.get())) {
            if (!_analyzeVariableDeclaration(var))
               return false;
         } else if (auto binop = dynamic_cast<BinaryExpressionAST*>(expr.get())) {
         } else if (auto ifStatement = dynamic_cast<IfStatementAST*>(expr.get())) {
         } else if (auto whileStatement = dynamic_cast<WhileStatementAST*>(expr.get())) {
         } else if (auto doWhileStatement = dynamic_cast<DoWhileStatementAST*>(expr.get())) {
         } else if (auto forStatement = dynamic_cast<ForStatementAST*>(expr.get())) {
         } else if (auto funcCall = dynamic_cast<CallExpressionAST*>(expr.get())) {
         } else if (auto blockStatement = dynamic_cast<BlockStatementAST*>(expr.get())) {
         } else if (auto returnStatement = dynamic_cast<ReturnStatementAST*>(expr.get())) {
         } else if (auto exitStatement = dynamic_cast<ExitStatement*>(expr.get())) {
         } else
            return false;
      }
      return true;
   }

   bool SemanticAnalyzer::_analyzeVariableDeclaration(VariableDeclarationAST* var) {
      if (var->initializer().get()) {
         if (auto num = dynamic_cast<NumberExpressionAST*>(var->initializer().get())) {
            if (_isNumberType(var->type()))
               num->setType(var->type());
            else
               return false;
         } else if (auto str = dynamic_cast<StringExpression*>(var->initializer().get())) {
            if (var->type() != "String")
               return false;
         } else if (auto binop = dynamic_cast<BinaryExpressionAST*>(var->initializer().get())) {
            binop->setType(var->type());
         } else if (auto staticCast = dynamic_cast<StaticCastAST*>(var->initializer().get())) {
         } else if (auto constCast = dynamic_cast<ConstCastAST*>(var->initializer().get())) {
         } else if (auto call = dynamic_cast<CallExpressionAST*>(var->initializer().get())) {
         } else if (auto other = dynamic_cast<VariableExpressionAST*>(var->initializer().get())) {
            if (_symbolTable.contains(other->name())) {
               if (!_isNumberType(var->type()) || !_isNumberType(_symbolTable[other->name()].type)) {
                  return false;
               } else if (var->type() != _symbolTable[other->name()].type) { // strings and classes
                  return false;
               }
            } else {
               return false;
            }
         }
      }

      _symbolTable[var->name()] = {SymbolType::Variable, var->type()};
      return true;
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