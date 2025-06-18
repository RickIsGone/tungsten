module;

#include <vector>
#include <optional>
#include <ostream>
#include <iostream>
#include <string>
#ifndef _NODISCARD
#define _NODISCARD [[nodiscard]]
#endif

export module Tungsten.parser;
import Tungsten.token;
import Tungsten.ast;

namespace tungsten {
   export class Parser {
   public:
      Parser() = default;
      ~Parser() = default;
      Parser(const Parser&) = delete;
      Parser operator=(const Parser&) = delete;

      void parse();
      void setTarget(const std::vector<Token>& tokens);
      void setRaw(const std::string& raw) { _fileContents = raw; }

   private:
      _NODISCARD Token _peek(size_t offset = 0) const;
      void _consume(const size_t amount = 1) { _index += amount; }

      std::unique_ptr<ExpressionAST> _parseNumberExpression();
      std::unique_ptr<ExpressionAST> _parseIdentifierExpression();
      std::unique_ptr<ExpressionAST> _parsePrimaryExpression();

      size_t _index{0};
      std::vector<Token> _tokens{};
      std::string _fileContents{};
   };

   //  ========================================== implementation ==========================================
   std::unique_ptr<ExpressionAST> LogError(const char* Str) {
      fprintf(stderr, "Error: %s\n", Str);
      return nullptr;
   }
   std::unique_ptr<FunctionPrototypeAST> LogErrorP(const char* Str) {
      LogError(Str);
      return nullptr;
   }

   void Parser::setTarget(const std::vector<Token>& tokens) {
      _tokens = tokens;
      _index = 0;
   }

   Token Parser::_peek(const size_t offset) const {
      return _tokens[_index + offset];
   }

   void Parser::parse() {
      std::cout << "Tokens: {";
      for (int i = 1; const Token& token : _tokens) {
         std::cout << "{" << tokenTypeNames.at(token.type);
         if (token.type == TokenType::INT_LITERAL || token.type == TokenType::STRING_LITERAL || token.type == TokenType::IDENTIFIER || token.type == TokenType::INVALID)
            std::cout << ", " << _fileContents.substr(token.position, token.length);

         std::cout << (i++ < _tokens.size() ? "},\n\t " : "}");
      }
      std::cout << "}\n";

      while (_peek().type != TokenType::END_OF_FILE) {
         // TODO
         _consume();
      }
   }

   std::unique_ptr<ExpressionAST> Parser::_parseNumberExpression() {
      double number = stod(_fileContents.substr(_peek().position, _peek().length));
      auto expr = std::make_unique<NumberExpressionAST>(number);
      _consume();
      return std::move(expr);
   }
   std::unique_ptr<ExpressionAST> Parser::_parseIdentifierExpression() {
      std::string identifier = _fileContents.substr(_peek().position, _peek().length);
      auto expr = std::make_unique<VariableExpressionAST>(identifier);
      _consume();
      return std::move(expr);
   }
   std::unique_ptr<ExpressionAST> Parser::_parsePrimaryExpression() {
      switch (_peek().type) {
         case TokenType::IDENTIFIER:
            return _parseIdentifierExpression();
         case TokenType::INT_LITERAL:
            return _parseNumberExpression();
         default:
            return LogError("unknown token when expecting an expression");
      }
   }
} // namespace tungsten
