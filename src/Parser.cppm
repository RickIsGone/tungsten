module;

#include <vector>
#include <ostream>
#include <iostream>
#include <string>
#include <memory>
#include <unordered_map>
#ifndef _NODISCARD
#define _NODISCARD [[nodiscard]]
#endif

export module Tungsten.parser;
import Tungsten.token;
import Tungsten.ast;

namespace tungsten {
   enum class SymbolType {
      Function,
      Variable,
      Class,
      NoType
   };
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
      std::string _lexme(Token token) { return _fileContents.substr(token.position, token.length); }
      //std::unique_ptr<ExpressionAST> _parseNumberExpression();
      std::unique_ptr<ExpressionAST> _parseIdentifierExpression();
      std::unique_ptr<ExpressionAST> _parsePrimaryExpression();
      std::string _parseType();
      std::unique_ptr<ExpressionAST> _parseVariableDeclaration();
      std::unique_ptr<FunctionPrototypeAST> _parseFunctionPrototype();
      std::unique_ptr<FunctionAST> _parseFunctionDeclaration();

      std::unordered_map<std::string, SymbolType> _symbolTable{};
      size_t _index{0};
      std::vector<Token> _tokens{};
      std::string _fileContents{};
   };

   //  ========================================== implementation ==========================================
   std::unique_ptr<ExpressionAST> LogError(const std::string& str) {
      std::cerr << "Error: " << str << "\n";
      return nullptr;
   }
   std::unique_ptr<FunctionPrototypeAST> LogErrorP(const std::string& str) {
      LogError(str);
      return nullptr;
   }

   void Parser::setTarget(const std::vector<Token>& tokens) {
      _tokens = tokens;
      _index = 0;
   }

   Token Parser::_peek(const size_t offset) const {
      return _tokens.at(_index + offset);
   }

   void Parser::parse() {
      std::cout << "Tokens: {";
      for (int i = 1; const Token& token : _tokens) {
         std::cout << "{" << tokenTypeNames.at(token.type);
         if (token.type == TokenType::IntLiteral || token.type == TokenType::StringLiteral || token.type == TokenType::Identifier || token.type == TokenType::Invalid)
            std::cout << ", " << _lexme(token);

         std::cout << (i++ < _tokens.size() ? "},\n\t " : "}");
      }
      std::cout << "}\n";

      while (_peek().type != TokenType::EndOFFile) {
         switch (_peek().type) {
            //case TokenType::Module:
            //   _parseModule();
            //   break;
            //case TokenType::Import:
            //   _parseImport();
            //   break;
            case TokenType::Int:
            case TokenType::Int8:
            case TokenType::Int16:
            case TokenType::Int32:
            case TokenType::Int64:
            case TokenType::Uint:
            case TokenType::Uint8:
            case TokenType::Uint16:
            case TokenType::Uint32:
            case TokenType::Uint64:
            case TokenType::Float:
            case TokenType::Double:
            case TokenType::Bool:
            case TokenType::Char:
            case TokenType::String:
            case TokenType::Void:
               if (_peek(2).type == TokenType::OpenParen)
                  _parseFunctionDeclaration();
               else _parseVariableDeclaration();
               break;
            case TokenType::Identifier:
               if (_symbolTable.contains(_lexme(_peek()))) {
                  if (_peek(2).type == TokenType::OpenParen)
                     _parseFunctionDeclaration();
                  else _parseVariableDeclaration();
               } else {
                  LogError(_lexme(_peek()) + " is not a known type or class");
                  _consume();
               }
               break;
            default:
               LogError("Invalid token: " + _lexme(_peek()));
               _consume();
               break;
         }
      }
   }

   //std::unique_ptr<ExpressionAST> Parser::_parseNumberExpression() {
   //   Number number = _lexme(_peek()); // TODO: fix
   //   auto expr = std::make_unique<NumberExpressionAST>(number);
   //   _consume();
   //   return std::move(expr);
   //}

   std::unique_ptr<ExpressionAST> Parser::_parseIdentifierExpression() {
      std::string identifier = _lexme(_peek());
      _consume();
      return std::make_unique<VariableExpressionAST>(identifier);
   }

   std::unique_ptr<ExpressionAST> Parser::_parsePrimaryExpression() {
      switch (_peek().type) {
         case TokenType::Identifier:
            return _parseIdentifierExpression();
         case TokenType::IntLiteral:
            //return _parseNumberExpression();
         default:
            return LogError("unknown token when expecting an expression");
      }
   }

   std::string Parser::_parseType() {
      std::string type = _lexme(_peek());
      if ((_peek().type >= TokenType::Auto && _peek().type <= TokenType::Int64) || _symbolTable.contains(_lexme(_peek())))
         return type;
      LogError("expected a type, got '" + type);
      return "";
   }

   std::unique_ptr<ExpressionAST> Parser::_parseVariableDeclaration() {
      std::string type = _parseType();
      _consume();
      std::string name = _lexme(_peek());
      _consume();
      if (_peek().type != TokenType::Semicolon)
         return LogError("Expected a semicolon");
      _consume();
      if (_symbolTable.contains(name))
         return LogError("redefinition of '" + name + "'");
      _symbolTable.insert({name, SymbolType::Variable});

      return std::make_unique<VariableDeclarationAST>(type, name);
   }

   std::unique_ptr<FunctionPrototypeAST> Parser::_parseFunctionPrototype() {
      std::string type = _parseType();
      _consume();
      std::string name = _lexme(_peek());
      _consume();
      if (_symbolTable.contains(name))
         return LogErrorP("Function '" + name + "' already exists");

      std::vector<std::unique_ptr<ExpressionAST>> args;
      _consume(); // consumes (
      while (_peek().type != TokenType::CloseParen) {
         args.push_back(_parseVariableDeclaration());
      }
      _consume(); // consumes )
      return std::make_unique<FunctionPrototypeAST>(type, name, std::move(args));
   }

   std::unique_ptr<FunctionAST> Parser::_parseFunctionDeclaration() {
      std::unique_ptr<FunctionPrototypeAST> proto = _parseFunctionPrototype();

      _symbolTable.insert({proto->name(), SymbolType::Function});
      std::unique_ptr<BlockStatementAST> body = nullptr;

      return std::make_unique<FunctionAST>(std::move(proto), std::move(body));
   }

} // namespace tungsten
