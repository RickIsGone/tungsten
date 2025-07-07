module;

#include <vector>
#include <iostream>
#include <string>
#include <memory>
#include <unordered_map>
#include <filesystem>
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
   std::unordered_map<TokenType, int> operatorPrecedence = {
       {TokenType::LogicalOr, 15}, // ||
       {TokenType::LogicalAnd, 14}, // &&
       {TokenType::BitwiseOr, 13}, // |
       {TokenType::BitwiseXor, 12}, // ^
       {TokenType::BitwiseAnd, 11}, // &
       {TokenType::EqualEqual, 10}, // ==
       {TokenType::NotEqual, 10}, // !=
       {TokenType::Less, 9}, // <
       {TokenType::LessEqual, 9}, // <=
       {TokenType::Greater, 9}, // >
       {TokenType::GreaterEqual, 9}, // >=
       {TokenType::ShiftLeft, 7}, // <<
       {TokenType::ShiftRight, 7}, // >>
       {TokenType::Plus, 6}, // +
       {TokenType::Minus, 6}, // -
       {TokenType::Multiply, 5}, // *
       {TokenType::Divide, 5}, // /
       {TokenType::ModuleOperator, 5}, // %
       {TokenType::LogicalNot, 3}, // ! (unary)
       {TokenType::Equal, 16}, // =   (assignment)
       {TokenType::PlusEqual, 16}, // +=
       {TokenType::MinusEqual, 16}, // -=
       {TokenType::MultiplyEqual, 16}, // *=
       {TokenType::DivideEqual, 16}, // /=
       {TokenType::ModuleEqual, 16}, // %=
       {TokenType::AndEqual, 16}, // &=
       {TokenType::OrEqual, 16}, // |=
       {TokenType::XorEqual, 16}, // ^=
       {TokenType::ShiftLeftEqual, 16}, // <<=
       {TokenType::ShiftRightEqual, 16}, // >>=
       {TokenType::Ternary, 16}, // ?:
       {TokenType::PlusPlus, 2}, // ++
       {TokenType::MinusMinus, 2}, // --
   };
   export class Parser {
   public:
      Parser(const std::filesystem::path& file, const std::vector<Token>& tokens, const std::string& raw)
          : _filePath{file}, _tokens{tokens}, _raw{raw} { initLLVM(); }
      Parser() = delete;
      ~Parser() { dumpIR(); }
      Parser(const Parser&) = delete;
      Parser operator=(const Parser&) = delete;

      void parse();

   private:
      _NODISCARD Token _peek(size_t offset = 0) const;
      void _consume(const size_t amount = 1) { _index += amount; }
      _NODISCARD std::string _lexeme(const Token& token) const { return _raw.substr(token.position, token.length); }
      std::string _location(const Token& token);
      size_t _column(const Token& token);
      size_t _line(const Token& token);
      std::string _file();
      std::string _function();
      int _getPrecedence(TokenType type);
      template <typename Ty = ExpressionAST>
      std::unique_ptr<Ty> _logError(const std::string& str);
      std::string _parseType();
      std::unique_ptr<ExpressionAST> _parseNumberExpression();
      std::unique_ptr<ExpressionAST> _parseIdentifierExpression();
      std::unique_ptr<ExpressionAST> _parseStringExpression();
      std::unique_ptr<ExpressionAST> _parseFunctionCall();
      std::unique_ptr<FunctionPrototypeAST> _parseExternFunctionStatement();
      std::unique_ptr<ExpressionAST> _parseExternVariableStatement();
      std::unique_ptr<ExpressionAST> _parseReturnStatement();
      std::unique_ptr<ExpressionAST> _parseExitStatement();
      std::unique_ptr<ExpressionAST> _parseExpression();
      std::unique_ptr<ExpressionAST> _parseBinaryOperationRHS(int exprPrecedence, std::unique_ptr<ExpressionAST> lhs);
      std::unique_ptr<ExpressionAST> _parsePrimaryExpression();
      std::unique_ptr<ExpressionAST> _parseBuiltinFunction();
      std::unique_ptr<ExpressionAST> _parseBuiltinColumn();
      std::unique_ptr<ExpressionAST> _parseBuiltinLine();
      std::unique_ptr<ExpressionAST> _parseBuiltinFile();
      std::unique_ptr<ExpressionAST> _parseTypeof();
      std::unique_ptr<ExpressionAST> _parseNameof();
      std::unique_ptr<ExpressionAST> _parseSizeof();
      std::unique_ptr<ExpressionAST> _parseVariableDeclaration();
      std::unique_ptr<ExpressionAST> _parseArgument();
      std::unique_ptr<BlockStatementAST> _parseBlock();
      std::unique_ptr<FunctionPrototypeAST> _parseFunctionPrototype();
      std::unique_ptr<FunctionAST> _parseFunctionDeclaration();
      std::unique_ptr<ExpressionAST> _parseIfStatement();
      std::unique_ptr<ExpressionAST> _parseWhileStatement();
      std::unique_ptr<ExpressionAST> _parseDoWhileStatement();
      std::unique_ptr<ExpressionAST> _parseForStatement();
      std::unique_ptr<ExpressionAST> _parseImport();

      std::unordered_map<std::string, SymbolType> _symbolTable{};
      size_t _index{0};
      const std::filesystem::path& _filePath{};
      const std::vector<Token> _tokens{};
      const std::string& _raw;
   };

   //  ========================================== implementation ==========================================

   template <typename Ty = ExpressionAST>
   std::unique_ptr<Ty> Parser::_logError(const std::string& str) {
      std::cerr << _location(_peek()) << " error: " << str << "\n";
      return nullptr;
   }

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
   size_t Parser::_column(const Token& token) {
      size_t column = 1;
      for (size_t i = 0; i < token.position; ++i) {
         if (_raw[i] == '\n') {
            column = 1;
         } else {
            ++column;
         }
      }
      return column;
   }
   size_t Parser::_line(const Token& token) {
      size_t line = 1;
      for (size_t i = 0; i < token.position; ++i) {
         if (_raw[i] == '\n') ++line;
      }
      return line;
   }
   std::string Parser::_file() {
      return std::filesystem::absolute(_filePath).string();
   }
   std::string Parser::_function() {
      return ""; // TODO: fix
   }

   int Parser::_getPrecedence(TokenType type) {
      if (operatorPrecedence.contains(type))
         return operatorPrecedence.at(type);
      return -1;
   }

   Token Parser::_peek(const size_t offset) const {
      if (_index + offset <= _raw.size() - 1)
         return _tokens.at(_index + offset);
      return _tokens.back();
   }

   void Parser::parse() {
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
               if (_peek(3).type == TokenType::OpenParen)
                  _parseExternFunctionStatement();
               else
                  _parseExternVariableStatement();

               if (_peek().type != TokenType::Semicolon)
                  _logError("expected ';' after extern statement");
               _consume(); // consume ';'
               break;
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
                        _logError("expected ';' after variable declaration");
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
   std::unique_ptr<ExpressionAST> Parser::_parseExpression() {
      auto lhs = _parsePrimaryExpression();
      if (!lhs)
         return nullptr;
      return _parseBinaryOperationRHS(0, std::move(lhs));
   }

   std::unique_ptr<ExpressionAST> Parser::_parseBinaryOperationRHS(int exprPrecedence, std::unique_ptr<ExpressionAST> lhs) {
      while (true) {
         int tokPrecedence = _getPrecedence(_peek().type);

         if (tokPrecedence < exprPrecedence)
            return lhs;

         Token opToken = _peek();
         _consume(); // consume operator

         auto rhs = _parsePrimaryExpression();
         if (!rhs)
            return nullptr;

         TokenType nextOpType = _peek().type;
         int nextPrecedence = _getPrecedence(nextOpType);

         if (tokPrecedence < nextPrecedence) {
            rhs = _parseBinaryOperationRHS(tokPrecedence + 1, std::move(rhs));
            if (!rhs)
               return nullptr;
         }

         lhs = std::make_unique<BinaryExpressionAST>(_lexeme(opToken), std::move(lhs), std::move(rhs));
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
      _consume(); // consume identifier
      _consume(); // consume (
      std::vector<std::unique_ptr<ExpressionAST>> args;
      while (_peek().type != TokenType::CloseParen && _peek().type != TokenType::EndOFFile) {
         if (!_parseExpression())
            return nullptr; // error already logged

         if (_peek().type == TokenType::Comma)
            _consume(); // consume ,
      }

      if (_peek().type == TokenType::EndOFFile)
         return _logError("expected ')' after function call");
      _consume(); // consume )

      utils::debugLog("function call: '{}', args number: {}", identifier, args.size());
      if (!_symbolTable.contains(identifier))
         return _logError("unknown function: '" + identifier + "'");

      return std::make_unique<CallExpressionAST>(identifier, std::move(args));
   }

   std::unique_ptr<FunctionPrototypeAST> Parser::_parseExternFunctionStatement() {
      _consume(); // consume extern
      return _parseFunctionPrototype();
   }
   std::unique_ptr<ExpressionAST> Parser::_parseExternVariableStatement() {
      _consume(); // consume extern
      return _parseVariableDeclaration();
   }

   std::unique_ptr<ExpressionAST> Parser::_parseReturnStatement() {
      _consume(); // consume return
      if (_peek().type == TokenType::Semicolon)
         return std::make_unique<ReturnStatementAST>(nullptr);

      return std::make_unique<ReturnStatementAST>(_parseExpression());
   }
   std::unique_ptr<ExpressionAST> Parser::_parseExitStatement() {
      _consume(); // consume exit
      if (_peek().type == TokenType::Semicolon)
         return _logError("expected an expression after 'exit'");

      return std::make_unique<ExitStatement>(_parseExpression());
   }

   std::unique_ptr<ExpressionAST> Parser::_parsePrimaryExpression() {
      std::unique_ptr<ExpressionAST> expr;
      switch (_peek().type) {
         case TokenType::Semicolon:
            _consume();
            return nullptr;
         case TokenType::True:
            _consume();
            return std::make_unique<NumberExpressionAST>(1);
         case TokenType::False:
            _consume();
            return std::make_unique<NumberExpressionAST>(0);
         case TokenType::CodeSuccess:
            _consume();
            return std::make_unique<NumberExpressionAST>(0);
         case TokenType::CodeFailure:
            _consume();
            return std::make_unique<NumberExpressionAST>(1);

         case TokenType::OpenBrace:
            return _parseBlock();

         case TokenType::OpenParen:
            _consume(); // consume (
            expr = _parseExpression();
            if (_peek().type != TokenType::CloseParen)
               return _logError("expected ')' after expression");
            _consume(); // consume )
            return std::move(expr);

         case TokenType::Identifier:
            if (_symbolTable.contains(_lexeme(_peek())) && _symbolTable.at(_lexeme(_peek())) == SymbolType::Class)
               return _parseVariableDeclaration();
            if (_peek(1).type == TokenType::OpenParen)
               return _parseFunctionCall();
            return _parseIdentifierExpression();

         case TokenType::IntLiteral:
         case TokenType::FloatLiteral:
            return _parseNumberExpression();
         case TokenType::StringLiteral:
            return _parseStringExpression();

         case TokenType::Return:
            expr = _parseReturnStatement();
            expr->codegen();
            return std::move(expr);

         case TokenType::Exit:
            expr = _parseExitStatement();
            expr->codegen();
            return std::move(expr);
         case TokenType::If:
            return _parseIfStatement();

         case TokenType::While:
            return _parseWhileStatement();

         case TokenType::Do:
            return _parseDoWhileStatement();

         case TokenType::For:
            return _parseForStatement();

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
            return _parseVariableDeclaration();

         case TokenType::__BuiltinFunction:
            return _parseBuiltinFunction();
         case TokenType::__BuiltinColumn:
            return _parseBuiltinColumn();
         case TokenType::__BuiltinLine:
            return _parseBuiltinLine();
         case TokenType::__BuiltinFile:
            return _parseBuiltinFile();
         case TokenType::Nameof:
            return _parseNameof();
         case TokenType::Typeof:
            return _parseTypeof();
         case TokenType::Sizeof:
            return _parseSizeof();

         default:
            _consume();
            return _logError("unknown token when expecting an expression");
      }
   }

   std::unique_ptr<ExpressionAST> Parser::_parseBuiltinFunction() {
      if (_peek(1).type != TokenType::OpenParen) {
         _consume(2);
         return _logError("expected '(' after '__builtinFunction'");
      }
      if (_peek(2).type != TokenType::CloseParen) {
         _consume(3);
         return _logError("expected ')' after '('");
      }

      auto expr = std::make_unique<__BuiltinFunctionAST>(_function());
      _consume(3); // consume '__builtinFunction()'
      return std::move(expr);
   }
   std::unique_ptr<ExpressionAST> Parser::_parseBuiltinColumn() {
      if (_peek(1).type != TokenType::OpenParen) {
         _consume(2);
         return _logError("expected '(' after '__builtinColumn'");
      }
      if (_peek(2).type != TokenType::CloseParen) {
         _consume(3);
         return _logError("expected ')' after '('");
      }

      auto expr = std::make_unique<__BuiltinColumnAST>(_column(_peek()));
      _consume(3); // consume '__builtinColumn()'
      return std::move(expr);
   }
   std::unique_ptr<ExpressionAST> Parser::_parseBuiltinLine() {
      if (_peek(1).type != TokenType::OpenParen) {
         _consume(2);
         return _logError("expected '(' after '__builtinLine'");
      }
      if (_peek(2).type != TokenType::CloseParen) {
         _consume(3);
         return _logError("expected ')' after '('");
      }

      auto expr = std::make_unique<__BuiltinColumnAST>(_column(_peek()));
      _consume(3); // consume '__builtinLine()'
      return std::move(expr);
   }
   std::unique_ptr<ExpressionAST> Parser::_parseBuiltinFile() {
      if (_peek(1).type != TokenType::OpenParen) {
         _consume(2);
         return _logError("expected '(' after '__builtinFile'");
      }
      if (_peek(2).type != TokenType::CloseParen) {
         _consume(3);
         return _logError("expected ')' after '('");
      }

      auto expr = std::make_unique<__BuiltinFileAST>(_file());
      _consume(3); // consume '__builtinFile()'
      return std::move(expr);
   }
   std::unique_ptr<ExpressionAST> Parser::_parseTypeof() {
      return std::make_unique<TypeOfStatementAST>(nullptr);
   }
   std::unique_ptr<ExpressionAST> Parser::_parseNameof() {
      return std::make_unique<NameOfStatementAST>(nullptr);
   }
   std::unique_ptr<ExpressionAST> Parser::_parseSizeof() {
      return std::make_unique<SizeOfStatementAST>(nullptr);
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
      _consume(); // consume type

      if (_peek().type != TokenType::Identifier)
         return _logError("expected an identifier after type declaration but got: '" + _lexeme(_peek()) + "'");

      std::string name = _lexeme(_peek());
      _consume(); // consume identifier

      if (_symbolTable.contains(name))
         return _logError("redefinition of '" + name + "'");

      _symbolTable.insert({name, SymbolType::Variable});

      std::unique_ptr<ExpressionAST> initExpr = nullptr;

      if (_peek().type == TokenType::Equal) {
         _consume(); // consume '='
         initExpr = _parseExpression();
      }

      utils::debugLog("definition of variable '{}' with type '{}' and initialization", name, type);

      return std::make_unique<VariableDeclarationAST>(type, name, std::move(initExpr));
   }

   std::unique_ptr<ExpressionAST> Parser::_parseArgument() {
      std::string type = _parseType();
      _consume(); // consume type
      std::string name = _lexeme(_peek());
      _consume(); // consume identifier

      if (_symbolTable.contains(name))
         return _logError("redefinition of '" + name + "'");
      _symbolTable.insert({name, SymbolType::Variable});

      std::unique_ptr<ExpressionAST> initExpr = nullptr;

      if (_peek().type == TokenType::Equal) {
         _consume(); // consume '=' / '{'
         initExpr = _parseExpression();
      } else if (_peek().type == TokenType::OpenBrace) {
         _consume(); // consume '{'
         initExpr = _parseExpression();
         _consume(); // consume '}'
      }
      return std::make_unique<VariableDeclarationAST>(type, name, std::move(initExpr));
   }

   std::unique_ptr<BlockStatementAST> Parser::_parseBlock() {
      _consume(); // consume {
      std::vector<std::unique_ptr<ExpressionAST>> statements;
      while (_peek().type != TokenType::CloseBrace && _peek().type != TokenType::EndOFFile) {
         _parseExpression();
         if (_peek().type != TokenType::Semicolon && _peek(-1).type != TokenType::CloseBrace)
            return _logError<BlockStatementAST>("expected ';' before '" + _lexeme(_peek()) + "'");
         if (_peek(-1).type != TokenType::CloseBrace)
            _consume(); // consume ;
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
      // check already handled in _parsePrimaryExpression()
      _consume(); // consumes (
      while (_peek().type != TokenType::CloseParen && _peek().type != TokenType::EndOFFile) {
         args.push_back(_parseArgument());
         utils::debugLog("argument {}: {} {}", args.size(), _lexeme(_peek(-2)), _lexeme(_peek(-1)));
         if (_peek().type == TokenType::Comma)
            _consume(); // consume ,
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

      std::unique_ptr<BlockStatementAST> body = _parseBlock();
      return std::make_unique<FunctionAST>(std::move(proto), std::move(body));
   }

   std::unique_ptr<ExpressionAST> Parser::_parseIfStatement() {
      _consume(); // consume if
      if (_peek().type != TokenType::OpenParen)
         return _logError("expected '(' after 'if'");
      _consume(); // consume (
      std::unique_ptr<ExpressionAST> condition = _parseExpression();
      if (_peek().type != TokenType::CloseParen)
         return _logError("expected ')' after 'if' condition");
      _consume(); // consume )
      std::unique_ptr<ExpressionAST> thenBranch;
      std::unique_ptr<ExpressionAST> elseBranch = nullptr;
      if (_peek().type == TokenType::OpenBrace)
         thenBranch = _parseBlock();
      else {
         thenBranch = _parseExpression();
         if (_peek().type != TokenType::Semicolon)
            return _logError("expected ';' after expression");
         // not consuming because if the if doesn't have an else the semicolon is automatically consumed in the _parseBlock()
      }
      if (_peek(1).type == TokenType::Else) {
         _consume();
         _consume();
         if (_peek().type == TokenType::OpenBrace)
            elseBranch = _parseBlock();
         else {
            elseBranch = _parseExpression();
            if (_peek().type != TokenType::Semicolon)
               return _logError("expected ';' after expression");
            _consume(); // consume ;
         }
      }
      return std::make_unique<IfStatementAST>(std::move(condition), std::move(thenBranch), std::move(elseBranch));
   }

   std::unique_ptr<ExpressionAST> Parser::_parseWhileStatement() {
      _consume(); // consume while
      if (_peek().type != TokenType::OpenParen)
         return _logError("expected '(' after 'while'");
      _consume(); // consume (
      std::unique_ptr<ExpressionAST> condition = _parseExpression();
      if (_peek().type != TokenType::CloseParen)
         return _logError("expected ')' after 'while' condition");
      _consume(); // consume )
      if (_peek().type != TokenType::OpenBrace)
         _logError("expected '{' after 'while'");

      std::unique_ptr<ExpressionAST> body = _parseBlock();

      return std::make_unique<WhileStatementAST>(std::move(condition), std::move(body));
   }

   std::unique_ptr<ExpressionAST> Parser::_parseDoWhileStatement() {
      _consume(); // consume do
      if (_peek().type != TokenType::OpenBrace)
         return _logError("expected '{' after 'do'");
      std::unique_ptr<ExpressionAST> body = _parseBlock();
      if (_peek().type != TokenType::While)
         return _logError("expected 'while' after 'do' block");
      _consume(); // consume while
      if (_peek().type != TokenType::OpenParen)
         return _logError("expected '(' after 'while'");
      _consume(); // consume (
      std::unique_ptr<ExpressionAST> condition = _parseExpression();
      if (_peek().type != TokenType::CloseParen)
         return _logError("expected ')' after 'while' condition");
      _consume(); // consume )
      return std::make_unique<DoWhileStatementAST>(std::move(condition), std::move(body));
   }

   std::unique_ptr<ExpressionAST> Parser::_parseForStatement() {
      _consume(); // consume for
      if (_peek().type != TokenType::OpenParen)
         return _logError("expected '(' after 'for'");
      _consume(); // consume (
      std::unique_ptr<ExpressionAST> init = _parseExpression();
      if (_peek().type != TokenType::Semicolon)
         return _logError("expected ';' after 'for' initialization");
      _consume(); // consume ;
      std::unique_ptr<ExpressionAST> condition = _parseExpression();
      if (_peek().type != TokenType::Semicolon)
         return _logError("expected ';' after 'for' condition");
      _consume(); // consume ;
      std::unique_ptr<ExpressionAST> increment = _parseExpression();
      if (_peek().type != TokenType::CloseParen)
         return _logError("expected ')' after 'for' condition");
      _consume(); // consume )
      std::unique_ptr<ExpressionAST> body;
      if (_peek().type == TokenType::OpenBrace)
         body = _parseBlock();
      else
         body = _parsePrimaryExpression();

      return std::make_unique<ForStatementAST>(std::move(init), std::move(condition), std::move(increment), std::move(body));
   }

   std::unique_ptr<ExpressionAST> Parser::_parseImport() {
      _consume(); // consume import
      if (_peek().type != TokenType::Identifier)
         return _logError("expected an identifier after 'import'");
      std::string module = _lexeme(_peek());
      _consume(); // consume identifier
      // TODO: Implement module import logic
      utils::debugLog("Importing module: {}", module);

      return nullptr;
   }
} // namespace tungsten
