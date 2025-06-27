module;

#include <string>
#include <memory>
#include <variant>
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
   using Number = std::variant<int, int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t, float, double>;
   class NumberExpressionAST : public ExpressionAST {
   public:
      NumberExpressionAST(Number value) : _value{value} {}

   private:
      Number _value{};
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

   class VariableDeclarationAST : public ExpressionAST {
   public:
      VariableDeclarationAST(const std::string& type, const std::string& name/*, std::unique_ptr<ExpressionAST> init*/)
         : _type{type}, _name{name}/*, _init{std::move(init)}*/ {}

   private:
      std::string _type;
      std::string _name;
      //std::unique_ptr<ExpressionAST> _init;
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
   class IfStatementAST : public ExpressionAST {
   public:
      IfStatementAST(std::unique_ptr<ExpressionAST> condition, std::unique_ptr<ExpressionAST> thenBranch, std::unique_ptr<ExpressionAST> elseBranch = nullptr)
          : _condition{std::move(condition)}, _thenBranch{std::move(thenBranch)}, _elseBranch{std::move(elseBranch)} {}

   private:
      std::unique_ptr<ExpressionAST> _condition;
      std::unique_ptr<ExpressionAST> _thenBranch;
      std::unique_ptr<ExpressionAST> _elseBranch;
   };

   class WhileStatementAST : public ExpressionAST {
   public:
      WhileStatementAST(std::unique_ptr<ExpressionAST> condition, std::unique_ptr<ExpressionAST> body)
            : _condition{std::move(condition)}, _body{std::move(body)} {}

   private:
      std::unique_ptr<ExpressionAST> _condition;
      std::unique_ptr<ExpressionAST> _body;
   };
   class DoWhileStatementAST : public ExpressionAST {
   public:
      DoWhileStatementAST(std::unique_ptr<ExpressionAST> condition, std::unique_ptr<ExpressionAST> body)
            : _condition{std::move(condition)}, _body{std::move(body)} {}
   private:
      std::unique_ptr<ExpressionAST> _condition;
      std::unique_ptr<ExpressionAST> _body;
   };

   class ForStatementAST : public ExpressionAST {
   public:
      ForStatementAST(std::unique_ptr<ExpressionAST> init, std::unique_ptr<ExpressionAST> condition, std::unique_ptr<ExpressionAST> increment, std::unique_ptr<ExpressionAST> body)
            : _init{std::move(init)}, _condition{std::move(condition)}, _increment{std::move(increment)}, _body{std::move(body)} {}

   private:
      std::unique_ptr<ExpressionAST> _init;
      std::unique_ptr<ExpressionAST> _condition;
      std::unique_ptr<ExpressionAST> _increment;
      std::unique_ptr<ExpressionAST> _body;
   };

   class BlockStatementAST : public ExpressionAST {
   public:
      BlockStatementAST(std::vector<std::unique_ptr<ExpressionAST>> statements)
            : _statements{std::move(statements)} {}

   private:
      std::vector<std::unique_ptr<ExpressionAST>> _statements;
   };

   class ReturnStatementAST : public ExpressionAST {
   public:
      ReturnStatementAST(std::unique_ptr<ExpressionAST> value) : _value{std::move(value)} {}
   private:
      std::unique_ptr<ExpressionAST> _value;
   };

   class ExitStatement : public ExpressionAST {
   public:
      ExitStatement(std::unique_ptr<ExpressionAST> value) : _value{std::move(value)} {}
   private:
      std::unique_ptr<ExpressionAST> _value;
   };

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
      FunctionAST(std::unique_ptr<FunctionPrototypeAST> prototype, std::unique_ptr<ExpressionAST> body)
          : _prototype{std::move(prototype)}, _body{std::move(body)} {}

   private:
      std::unique_ptr<FunctionPrototypeAST> _prototype;
      std::unique_ptr<ExpressionAST> _body;
   };
} // namespace tungsten