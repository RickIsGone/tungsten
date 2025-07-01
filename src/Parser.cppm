module;

#include <vector>
#include <iostream>
#include <string>
#include <memory>
#include <unordered_map>
#include <filesystem>

// #include "llvm/ADT/APFloat.h"
// #include "llvm/ADT/STLExtras.h"
// #include "llvm/IR/BasicBlock.h"
// #include "llvm/IR/Constants.h"
// #include "llvm/IR/DerivedTypes.h"
// #include "llvm/IR/Function.h"
// #include "llvm/IR/IRBuilder.h"
// #include "llvm/IR/LLVMContext.h"
// #include "llvm/IR/Module.h"
// #include "llvm/IR/Type.h"
// #include "llvm/IR/Verifier.h"
#ifndef _NODISCARD
#define _NODISCARD [[nodiscard]]
#endif

export module Tungsten.parser;
import Tungsten.token;
import Tungsten.ast;
import Tungsten.utils;

namespace tungsten {
   enum class SymbolType {
      Function,
      Variable,
      Class,
      NoType
   };
   export class Parser {
   public:
      Parser(const std::filesystem::path& file, const std::vector<Token>& tokens, const std::string& raw) : _filePath{file}, _tokens{tokens}, _raw{raw} {}
      Parser() = delete;
      ~Parser() = default;
      Parser(const Parser&) = delete;
      Parser operator=(const Parser&) = delete;

      void parse();

   private:
      _NODISCARD Token _peek(size_t offset = 0) const;
      void _consume(const size_t amount = 1) { _index += amount; }
      std::string _lexeme(const Token& token) const { return _raw.substr(token.position, token.length); }
      std::string _location(const Token& token);
      template <typename Ty = ExpressionAST>
      std::unique_ptr<Ty> _logError(const std::string& str);
      // llvm::Value* _logErrorV(const std::string& Str);
      std::string _parseType();
      std::unique_ptr<ExpressionAST> _parseNumberExpression();
      std::unique_ptr<ExpressionAST> _parseIdentifierExpression();
      std::unique_ptr<ExpressionAST> _parseStringExpression();
      std::unique_ptr<ExpressionAST> _parseFunctionCall();
      std::unique_ptr<FunctionPrototypeAST> _parseExternStatement();
      std::unique_ptr<ExpressionAST> _parseReturnStatement();
      std::unique_ptr<ExpressionAST> _parseExitStatement();
      std::unique_ptr<ExpressionAST> _parsePrimaryExpression();
      std::unique_ptr<ExpressionAST> _parseVariableDeclaration();
      std::unique_ptr<ExpressionAST> _parseArgument();
      std::unique_ptr<BlockStatementAST> _parseBlock();
      std::unique_ptr<FunctionPrototypeAST> _parseFunctionPrototype();
      std::unique_ptr<FunctionAST> _parseFunctionDeclaration();
      std::unique_ptr<ExpressionAST> _parseImport();

      std::unordered_map<std::string, SymbolType> _symbolTable{};
      size_t _index{0};
      const std::string& _raw;
      const std::vector<Token> _tokens{};
      const std::filesystem::path& _filePath{};

      // llvm stuff
      // std::unique_ptr<llvm::LLVMContext> TheContext{};
      // std::unique_ptr<llvm::IRBuilder<>> Builder{};
      // std::unique_ptr<llvm::Module> TheModule{};
      // std::map<std::string, llvm::Value*> NamedValues{};
   };

   //  ========================================== implementation ==========================================
   template <typename Ty = ExpressionAST>
   std::unique_ptr<Ty> Parser::_logError(const std::string& str) {
      std::cerr << _location(_peek()) << " error: " << str << "\n";
      return nullptr;
   }
   // llvm::Value* Parser::_logErrorV(const std::string& Str) {
   //    _logError(Str);
   //    return nullptr;
   // }

   std::string Parser::_location(const Token& token) {
      size_t line = 1, column = 1;
      for (size_t i = 0; i < token.position; ++i) {
         if (_raw[i] == '\n') {
            ++line;
            column = 1;
         } else {
            ++column;
         }
      }
      return std::filesystem::absolute(_filePath).string() + ":" + std::to_string(line) + ":" + std::to_string(column) + ":";
   }

   Token Parser::_peek(const size_t offset) const {
      if (_index + offset + 1 <= _raw.size())
         return _tokens.at(_index + offset);
      return _tokens.back();
   }

   void Parser::parse() {
      // std::cout << "Tokens: {";
      // for (int i = 1; const Token& token : _tokens) {
      //    std::cout << "{" << tokenTypeNames.at(token.type);
      //    if (token.type == TokenType::IntLiteral || token.type == TokenType::StringLiteral || token.type == TokenType::Identifier || token.type == TokenType::Invalid)
      //       std::cout << ", " << _lexeme(token);
      //
      //    std::cout << (i++ < _tokens.size() ? "},\n\t " : "}");
      // }
      // std::cout << "}\n";

      while (_peek().type != TokenType::EndOFFile) {
         switch (_peek().type) {
            case TokenType::Semicolon:
               _consume();
               break;
            // case TokenType::Module:
            //    _parseModule();
            //    break;
            case TokenType::Import:
               _parseImport();
               if (_peek().type != TokenType::Semicolon)
                  _logError("expected ';' after import statement");
               _consume(); // consume ';'
               break;
            case TokenType::Extern:
               _parseExternStatement();
               if (_peek().type != TokenType::Semicolon)
                  _logError("expected ';' after extern statement");
               _consume(); // consume ';'
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
               if (_peek(2).type == TokenType::OpenParen) {
                  _parseFunctionDeclaration();
               } else {
                  _parseVariableDeclaration();
                  if (_peek().type != TokenType::Semicolon)
                     _logError("expected ';' after variable declaration");
                  _consume(); // consume ';'
               }
               break;
            case TokenType::Identifier:
               if (_symbolTable.contains(_lexeme(_peek())) && _symbolTable.at(_lexeme(_peek())) == SymbolType::Class) {
                  if (_peek(2).type == TokenType::OpenParen)
                     _parseFunctionDeclaration();
                  else {
                     _parseVariableDeclaration();
                     if (_peek().type != TokenType::Semicolon)
                        _logError("expected ';' after import statement");
                     _consume(); // consume ';'
                  }
               } else {
                  _logError("'" + _lexeme(_peek()) + "' is not a known type or class");
                  _consume();
               }
               break;
            default:
               _logError("Invalid token: '" + _lexeme(_peek()) + "'");
               _consume();
               break;
         }
      }
      if (!_symbolTable.contains("main") || (_symbolTable.contains("main") && _symbolTable.at("main") != SymbolType::Function)) {
         _logError("no entry point found");
      } else {
         utils::debugLog("found entry point");
      }
   }

   std::unique_ptr<ExpressionAST> Parser::_parseNumberExpression() {
      auto number = _peek().type == TokenType::IntLiteral ? std::stoi(_lexeme(_peek())) : std::stof(_lexeme(_peek()));
      _consume();
      return std::make_unique<NumberExpressionAST>(number);
   }

   std::unique_ptr<ExpressionAST> Parser::_parseIdentifierExpression() {
      std::string identifier = _lexeme(_peek());
      _consume();
      if (!_symbolTable.contains(identifier))
         return _logError("unknown identifier: '" + identifier + "'");

      return std::make_unique<VariableExpressionAST>(identifier);
   }
   std::unique_ptr<ExpressionAST> Parser::_parseStringExpression() {
      std::string string = _lexeme(_peek());
      _consume();
      return std::make_unique<StringExpression>(string);
   }

   std::unique_ptr<ExpressionAST> Parser::_parseFunctionCall() {
      std::string identifier = _lexeme(_peek());
      _consume();
      _consume(); // consume (
      std::vector<std::unique_ptr<ExpressionAST>> args;
      do {
         if (_peek().type == TokenType::Identifier) {
            if (!_symbolTable.contains(_lexeme(_peek())))
               _logError("unknown identifier: '" + _lexeme(_peek()) + "'");

            if (_peek(1).type == TokenType::OpenParen)
               args.push_back(_parseFunctionCall());
            else
               args.push_back(_parseIdentifierExpression());
         } else if (_peek().type == TokenType::IntLiteral || _peek().type == TokenType::FloatLiteral)
            args.push_back(_parseNumberExpression());

         else if (_peek().type == TokenType::StringLiteral)
            args.push_back(_parseStringExpression());

         else {
            _consume();
            return _logError("expected an argument in function call, got: '" + _lexeme(_peek()) + "'");
         }
         if (_peek().type == TokenType::Comma)
            _consume(); // consume ,
      } while (_peek().type != TokenType::CloseParen && _peek().type != TokenType::EndOFFile);

      if (_peek().type == TokenType::EndOFFile)
         return _logError("expected ')' after function call");
      _consume(); // consume )

      utils::debugLog("function call: '{}', args number: {}", identifier, args.size());
      if (!_symbolTable.contains(identifier))
         return _logError("unknown function: '" + identifier + "'");

      return std::make_unique<CallExpressionAST>(identifier, std::move(args));
   }
   std::unique_ptr<FunctionPrototypeAST> Parser::_parseExternStatement() {
      _consume(); // consume extern
      if (_peek(1).type != TokenType::OpenParen && _peek().type != TokenType::Identifier)
         return _logError<FunctionPrototypeAST>("expected an function after 'extern'");
      return _parseFunctionPrototype();
   }

   std::unique_ptr<ExpressionAST> Parser::_parseReturnStatement() {
      _consume(); // consume return
      if (_peek().type == TokenType::Semicolon) {
         return std::make_unique<ReturnStatementAST>(nullptr);
      }

      auto value = _parsePrimaryExpression();
      utils::debugLog("returning value: {}", _lexeme(_peek(-1)));
      return std::make_unique<ReturnStatementAST>(std::move(value));
   }
   std::unique_ptr<ExpressionAST> Parser::_parseExitStatement() {
      _consume(); // consume exit
      if (_peek().type == TokenType::Semicolon)
         return _logError("expected an expression after 'exit'");

      auto value = _parsePrimaryExpression();

      utils::debugLog("exiting with value: {}", _lexeme(_peek(-1)));
      return std::make_unique<ExitStatement>(std::move(value));
   }

   std::unique_ptr<ExpressionAST> Parser::_parsePrimaryExpression() {
      std::unique_ptr<ExpressionAST> expr;
      switch (_peek().type) {
         case TokenType::Semicolon:
            _consume();
            return nullptr;
         case TokenType::CodeSuccess:
            _consume();
            return std::make_unique<NumberExpressionAST>(0);

         case TokenType::CodeFailure:
            _consume();
            return std::make_unique<NumberExpressionAST>(1);

         case TokenType::Identifier:
            if (_symbolTable.contains(_lexeme(_peek())) && _symbolTable.at(_lexeme(_peek())) == SymbolType::Class) {
               expr = _parseVariableDeclaration();
               if (_peek().type != TokenType::Semicolon)
                  return _logError("expected ';' after variable declaration");
               _consume(); // consume ;
               return std::move(expr);
            }
            if (_peek(1).type == TokenType::OpenParen) {
               expr = _parseFunctionCall();
               if (_peek().type != TokenType::Semicolon)
                  return _logError("expected ';' after function call");
               _consume(); // consume ;
               return std::move(expr);
            }
            expr = _parseIdentifierExpression();
            return std::move(expr);

         case TokenType::IntLiteral:
         case TokenType::FloatLiteral:
            return _parseNumberExpression();

         case TokenType::Return:
            expr = _parseReturnStatement();
            if (_peek().type != TokenType::Semicolon)
               return _logError("expected ';' after return statement");
            _consume(); // consume ;
            return std::move(expr);

         case TokenType::Exit:
            expr = _parseExitStatement();
            if (_peek().type != TokenType::Semicolon)
               return _logError("expected ';' after exit statement");
            _consume(); // consume ;
            return std::move(expr);

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
            expr = _parseVariableDeclaration();
            if (_peek().type != TokenType::Semicolon)
               return _logError("expected ';' variable declaration");
            _consume(); // consume ;
            return std::move(expr);

         default:
            _consume();
            return _logError("unknown token when expecting an expression");
      }
   }

   std::string Parser::_parseType() {
      std::string type = _lexeme(_peek());
      if ((_peek().type >= TokenType::Auto && _peek().type <= TokenType::Int64) || _symbolTable.contains(_lexeme(_peek())))
         return type;
      _logError<std::string>("expected a type, got '" + type + "'");
      return "";
   }

   std::unique_ptr<ExpressionAST> Parser::_parseVariableDeclaration() {
      std::string type = _parseType();
      _consume();
      if (_peek().type != TokenType::Identifier)
         return _logError("expected an identifier after type declaration but got: '" + _lexeme(_peek()) + "'");
      std::string name = _lexeme(_peek());
      _consume();

      if (_symbolTable.contains(name))
         return _logError("redefinition of '" + name + "'");
      _symbolTable.insert({name, SymbolType::Variable});

      utils::debugLog("definition of variable '{}' with type '{}'", name, type);
      return std::make_unique<VariableDeclarationAST>(type, name);
   }

   std::unique_ptr<ExpressionAST> Parser::_parseArgument() {
      std::string type = _parseType();
      _consume();
      std::string name = _lexeme(_peek());
      _consume();

      if (_symbolTable.contains(name))
         return _logError("redefinition of '" + name + "'");
      _symbolTable.insert({name, SymbolType::Variable});

      return std::make_unique<VariableDeclarationAST>(type, name);
   }

   std::unique_ptr<BlockStatementAST> Parser::_parseBlock() {
      std::vector<std::unique_ptr<ExpressionAST>> statements;
      while (_peek().type != TokenType::CloseBrace && _peek().type != TokenType::EndOFFile) {
         _parsePrimaryExpression();
         // _consume();
      }
      if (_peek().type == TokenType::EndOFFile)
         return _logError<BlockStatementAST>("expected '}'");

      _consume(); // consume }
      return std::make_unique<BlockStatementAST>(std::move(statements));
   }

   std::unique_ptr<FunctionPrototypeAST> Parser::_parseFunctionPrototype() {
      std::string type = _parseType();
      _consume();
      std::string name = _lexeme(_peek());
      _consume();
      if (_symbolTable.contains(name))
         return _logError<FunctionPrototypeAST>("redefinition of '" + name + "'");

      utils::debugLog("definition of function prototype '{}' with type '{}'", name, type);
      std::vector<std::unique_ptr<ExpressionAST>> args;
      _consume(); // consumes (
      while (_peek().type != TokenType::CloseParen && _peek().type != TokenType::EndOFFile) {
         args.push_back(_parseArgument());
         utils::debugLog("argument {}: {} {}", args.size(), _lexeme(_peek(-2)), _lexeme(_peek(-1)));
      }
      if (_peek().type == TokenType::EndOFFile)
         return _logError<FunctionPrototypeAST>("expected ')'");
      _consume(); // consumes )
      return std::make_unique<FunctionPrototypeAST>(type, name, std::move(args));
   }

   std::unique_ptr<FunctionAST> Parser::_parseFunctionDeclaration() {
      std::unique_ptr<FunctionPrototypeAST> proto = _parseFunctionPrototype();

      _symbolTable.insert({proto->name(), SymbolType::Function});
      if (_peek().type != TokenType::OpenBrace)
         return _logError<FunctionAST>("expected '{'");

      _consume(); // consume {
      std::unique_ptr<BlockStatementAST> body = _parseBlock();
      return std::make_unique<FunctionAST>(std::move(proto), std::move(body));
   }

   std::unique_ptr<ExpressionAST> Parser::_parseImport() {
      _consume(); // consume import
      if (_peek().type != TokenType::Identifier)
         return _logError("expected an identifier after 'import'");
      std::string module = _lexeme(_peek());
      _consume(); // consume identifier

      utils::debugLog("Importing module: {}", module);

      return nullptr;
   }
} // namespace tungsten
