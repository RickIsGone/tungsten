module;

#include <string>
#include <memory>
#include <type_traits>
#include <vector>
#ifndef _NODISCARD
#define _NODISCARD [[nodiscard]]
#endif

export module Tungsten.ast;

export namespace tungsten {
   // base for all nodes in the AST
   class ExpressionAST {
   public:
      virtual ~ExpressionAST() = default;
   };

   // expression for numbers
   class NumberExpressionAST : public ExpressionAST {
   public:
      NumberExpressionAST(double value) : _value{value} {}

   private:
      double _value{};
   };

   // expression for variables
   class VariableExpressionAST : public ExpressionAST {
   public:
      VariableExpressionAST(const std::string& name) : _name{name} {}

   private:
      std::string _name{};
   };

   // expression for binary operations
   class BinaryExpressionAST : public ExpressionAST {
   public:
      BinaryExpressionAST(char op, std::unique_ptr<ExpressionAST> LHS, std::unique_ptr<ExpressionAST> RHS)
          : _op{op}, _LHS{std::move(LHS)}, _RHS{std::move(RHS)} {}

   private:
      char _op;
      std::unique_ptr<ExpressionAST> _LHS;
      std::unique_ptr<ExpressionAST> _RHS;
   };

   // expression for function calls
   class CallExpressionAST : public ExpressionAST {
   public:
      CallExpressionAST(const std::string& callee, std::vector<std::unique_ptr<ExpressionAST>> args)
          : _callee{callee}, _args{std::move(args)} {}

   private:
      std::string _callee;
      std::vector<std::unique_ptr<ExpressionAST>> _args;
   };

   // expression for if statements
   class IfExpressionAST : public ExpressionAST {
   public:
      IfExpressionAST(std::unique_ptr<ExpressionAST> condition, std::unique_ptr<ExpressionAST> thenBranch, std::unique_ptr<ExpressionAST> elseBranch = nullptr)
          : _condition{std::move(condition)}, _thenBranch{std::move(thenBranch)}, _elseBranch{std::move(elseBranch)} {}

   private:
      std::unique_ptr<ExpressionAST> _condition;
      std::unique_ptr<ExpressionAST> _thenBranch;
      std::unique_ptr<ExpressionAST> _elseBranch;
   };
   // TODO: add other conditional expressions

   //  prototype for function declarations
   class FunctionPrototypeAST {
   public:
      FunctionPrototypeAST(const std::string& type, const std::string& name, std::vector<std::unique_ptr<ExpressionAST>> args)
          : _type{type}, _name{name}, _args{std::move(args)} {}

      _NODISCARD const std::string& name() const { return _name; }

   private:
      std::string _type;
      std::string _name;
      std::vector<std::unique_ptr<ExpressionAST>> _args;
   };

   // function definition itself
   class FunctionAST {
   public:
      FunctionAST(std::unique_ptr<FunctionPrototypeAST> function, std::unique_ptr<ExpressionAST> body)
          : _function{std::move(function)}, _body{std::move(body)} {}

   private:
      std::unique_ptr<FunctionPrototypeAST> _function;
      std::unique_ptr<ExpressionAST> _body;
   };
} // namespace tungsten