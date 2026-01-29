module;

#include <vector>
#include <iostream>
#include <string>
#include <memory>
#include <unordered_map>
#include <filesystem>
#include <variant>
#ifndef _NODISCARD
#define _NODISCARD [[nodiscard]]
#endif

export module Tungsten.parser;
import Tungsten.token;
import Tungsten.ast;
import Tungsten.utils;

namespace tungsten {
   export enum class SymbolType {
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
       // {TokenType::Ternary, 16}, // ?:
       {TokenType::PlusPlus, 2}, // ++
       {TokenType::MinusMinus, 2}, // --

       // {TokenType::Dot, 1}, // class.member
       // {TokenType::Arrow, 1} // class->member
   };

   export struct Externs {
      std::vector<std::unique_ptr<FunctionPrototypeAST>> functions;
      std::vector<std::unique_ptr<ExpressionAST>> variables;
   };

   export class Parser {
   public:
      Parser(const std::filesystem::path& file, const std::vector<Token>& tokens, const std::string& raw)
          : _filePath{file}, _tokens{tokens}, _raw{raw} {
         initLLVM(file.filename().stem().string(), file.filename().string());
         _externs = std::make_unique<Externs>();
      }
      Parser() = delete;
      Parser(const Parser&) = delete;
      Parser& operator=(const Parser&) = delete;

      void parse();
      void writeIR() { dumpIR(); }
      _NODISCARD std::vector<std::unique_ptr<FunctionAST>>& functions() { return _functions; }
      _NODISCARD std::vector<std::unique_ptr<ClassAST>>& classes() { return _classes; }
      _NODISCARD std::vector<std::unique_ptr<ExpressionAST>>& globalVariables() { return _globalVariables; }
      _NODISCARD std::unique_ptr<Externs>& externs() { return _externs; }

   private:
      _NODISCARD Token _peek(size_t offset = 0) const;
      _NODISCARD Token _peekBack(size_t offset = 0) const;
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
      std::shared_ptr<Type> _parseType();
      // std::unique_ptr<ExpressionAST> _parseAlias();
      std::unique_ptr<ExpressionAST> _parseNumberExpression();
      std::unique_ptr<ExpressionAST> _parseIdentifierExpression();
      std::unique_ptr<ExpressionAST> _parseStringExpression();
      std::unique_ptr<ExpressionAST> _parseFunctionCall();
      std::unique_ptr<FunctionPrototypeAST> _parseExternFunctionStatement();
      // std::unique_ptr<ExpressionAST> _parseExternVariableStatement();
      std::unique_ptr<ExpressionAST> _parseReturnStatement();
      std::unique_ptr<ExpressionAST> _parseExitStatement();
      std::unique_ptr<ExpressionAST> _parseExpression();
      std::unique_ptr<ExpressionAST> _parseBinaryOperationRHS(std::unique_ptr<ExpressionAST> lhs);
      std::unique_ptr<ExpressionAST> _parsePrimaryExpression();
      std::unique_ptr<ExpressionAST> _parseUnaryExpression();
      std::unique_ptr<ExpressionAST> _parseBuiltinFunction();
      std::unique_ptr<ExpressionAST> _parseBuiltinColumn();
      std::unique_ptr<ExpressionAST> _parseBuiltinLine();
      std::unique_ptr<ExpressionAST> _parseBuiltinFile();
      std::unique_ptr<ExpressionAST> _parseTypeof();
      std::unique_ptr<ExpressionAST> _parseNameof();
      std::unique_ptr<ExpressionAST> _parseSizeof();
      std::unique_ptr<ExpressionAST> _parseStaticCast();
      std::unique_ptr<ExpressionAST> _parseConstCast();
      std::unique_ptr<ExpressionAST> _parseVariableDeclaration();
      std::unique_ptr<FunctionAST> _parseFunctionDeclaration();
      std::unique_ptr<ExpressionAST> _parseArgument();
      std::unique_ptr<BlockStatementAST> _parseBlock();
      std::unique_ptr<FunctionPrototypeAST> _parseFunctionPrototype();
      std::unique_ptr<ExpressionAST> _parseIfStatement();
      std::unique_ptr<ExpressionAST> _parseWhileStatement();
      std::unique_ptr<ExpressionAST> _parseDoWhileStatement();
      std::unique_ptr<ExpressionAST> _parseForStatement();
      std::unique_ptr<ExpressionAST> _parseImport();
      // std::unique_ptr<FunctionAST> _parseConstructorOrDestructor(const std::string& className);
      // std::unique_ptr<ClassAST> _parseClass();
      // std::unique_ptr<ExpressionAST> _parseNamespace();

      std::unordered_map<std::string, SymbolType> _symbolTable{};
      size_t _index{0};
      const std::filesystem::path& _filePath{};
      const std::vector<Token> _tokens{};
      const std::string& _raw;

      std::unique_ptr<Externs> _externs;
      std::vector<std::unique_ptr<FunctionAST>> _functions;
      std::vector<std::unique_ptr<ClassAST>> _classes;
      std::vector<std::unique_ptr<ExpressionAST>> _globalVariables;
      std::shared_ptr<Type> _currentFunctionReturnType;
      std::string _currentFunctionName;
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
      return _filePath.string() + ":" + std::to_string(line) + ":" + std::to_string(column) + ":"; // std::filesystem::absolute(_filePath).string()
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
      return _currentFunctionName;
   }

   int Parser::_getPrecedence(TokenType type) {
      if (operatorPrecedence.contains(type))
         return operatorPrecedence.at(type);
      return -1;
   }

   Token Parser::_peek(const size_t offset) const {
      if (_index + offset <= _tokens.size() - 1)
         return _tokens.at(_index + offset);
      return _tokens.back();
   }

   _NODISCARD Token Parser::_peekBack(size_t offset) const {
      if (_index - offset >= 0)
         return _tokens.at(_index - offset);
      return _tokens.at(0);
   }


   void Parser::parse() {
      std::unique_ptr<ExpressionAST> expr;
      std::unique_ptr<FunctionAST> func;
      while (_peek().type != TokenType::EndOFFile) {
         switch (_peek().type) {
            case TokenType::Semicolon:
               _consume();
               break;
            // case TokenType::Module:
            //    _parseModule();
            //    break;
            case TokenType::Import:
               expr = _parseImport();
               if (_peek().type != TokenType::Semicolon)
                  _logError("expected ';' after import statement");
               _consume(); // consume ';'
               expr->codegen();
               break;
            case TokenType::Extern:
               // std::unique_ptr<FunctionPrototypeAST> proto;
               // if (_peek(3).type == TokenType::OpenParen)
               //    _externs->variables.push_back(_parseExternVariableStatement());
               // else
               _externs->functions.push_back(_parseExternFunctionStatement());

               if (_peek().type != TokenType::Semicolon)
                  _logError("expected ';' after extern statement");
               _consume(); // consume ';'
               break;

            case TokenType::Fun:
               _functions.push_back(_parseFunctionDeclaration());
               break;

            case TokenType::Num:
            case TokenType::Int:
            case TokenType::Int8:
            case TokenType::Int16:
            case TokenType::Int32:
            case TokenType::Int64:
            case TokenType::Int128:
            case TokenType::Uint:
            case TokenType::Uint8:
            case TokenType::Uint16:
            case TokenType::Uint32:
            case TokenType::Uint64:
            case TokenType::Uint128:
            case TokenType::Float:
            case TokenType::Double:
            case TokenType::Bool:
            case TokenType::Char:
            case TokenType::String:
            case TokenType::Void:
            case TokenType::Identifier: {
               auto expr = _parseVariableDeclaration();
               if (_peek().type != TokenType::Semicolon)
                  _logError("expected ';' after variable declaration");
               _consume(); // consume ';'
               _globalVariables.push_back(std::move(expr));
               break;
            }
               // case TokenType::Namespace:
               //    _parseNamespace();
               //    break;
               // case TokenType::Class:
               //    _classes.push_back(_parseClass());
               //    // cls->codegen();
               //    break;

            default:
               _logError("Invalid token: '" + _lexeme(_peek()) + "'");
               _consume();
               break;
         }
      }
   }

   // std::unique_ptr<ExpressionAST> Parser::_parseNamespace() {
   //    _consume(); // consume namespace
   //    if (_peek().type != TokenType::Identifier)
   //       return _logError("expected a namespace name");
   //    std::string name = _lexeme(_peek());
   //    _consume(); // consume identifier
   //
   //    if (_peek().type != TokenType::OpenBrace)
   //       return _logError("expected '{' after namespace name");
   //    _consume();
   //
   //    std::vector<std::unique_ptr<ExpressionAST>> statements;
   //    std::vector<std::unique_ptr<FunctionAST>> functions;
   //    std::vector<std::unique_ptr<ClassAST>> classes;
   //    while (_peek().type != TokenType::CloseBrace && _peek().type != TokenType::EndOFFile) {
   //       if (_peek().type == TokenType::Namespace)
   //          statements.push_back(_parseNamespace());
   //       else if (_peek().type == TokenType::Class)
   //          classes.push_back(_parseClass());
   //       else if (_peek().type == TokenType::Extern) {
   //          _consume(); // consume extern
   //          if (_peek(2).type == TokenType::OpenParen)
   //             functions.push_back(_parseExternFunctionStatement());
   //          else
   //             statements.push_back(_parseExternVariableStatement());
   //       }
   //    }
   //    if (_peek().type == TokenType::EndOFFile)
   //       return _logError("expected '}' after namespace");
   //    _consume(); // consume }
   //    return std::make_unique<NamespaceAST>(name, std::move(statements), std::move(functions), std::move(classes));
   // }

   // std::unique_ptr<FunctionAST> Parser::_parseConstructorOrDestructor(const std::string& className) {
   //    std::string name = _lexeme(_peek());
   //    _consume();
   //    // utils::debugLog(name[0] == '~' ? "definition of class '{}' destructor" : "definition of class '{}' constructor", className);
   //    std::vector<std::unique_ptr<ExpressionAST>> args;
   //    // check already handled in _parseClass()
   //    _consume(); // consumes (
   //    while (_peek().type != TokenType::CloseParen && _peek().type != TokenType::EndOFFile) {
   //       args.push_back(_parseArgument());
   //       // utils::debugLog("argument {}: {} {}", args.size(), _lexeme(_peek(-2)), _lexeme(_peekBack()));
   //       if (_peek().type == TokenType::Comma)
   //          _consume(); // consume ,
   //    }
   //    if (_peek().type == TokenType::EndOFFile)
   //       return _logError<FunctionAST>("expected ')'");
   //    _consume(); // consumes )
   //    auto proto = std::make_unique<FunctionPrototypeAST>(makeClass(), className, name, std::move(args));
   //    // _symbolTable.insert({proto->name(), SymbolType::Function});
   //    if (_peek().type != TokenType::OpenBrace)
   //       return _logError<FunctionAST>("expected '{'");

   //    std::unique_ptr<BlockStatementAST> body = _parseBlock();
   //    return std::make_unique<FunctionAST>(std::move(proto), std::move(body));
   // }

   // std::unique_ptr<ClassAST> Parser::_parseClass() { // TODO: fix because of type rework
   //    _consume(); // consume class
   //    if (_peek().type != TokenType::Identifier)
   //       return _logError<ClassAST>("expected a class name");
   //    std::string name = _lexeme(_peek());
   //    _consume(); // consume identifier
   //    _symbolTable.insert({name, SymbolType::Class});
   //    // TODO: add inheritance
   //    if (_peek().type != TokenType::OpenBrace)
   //       return _logError<ClassAST>("expected '{' after class name");
   //    _consume(); // consume '{'
   //    std::vector<std::unique_ptr<ClassVariableAST>> variables;
   //    std::vector<std::unique_ptr<ClassMethodAST>> methods;
   //    std::vector<std::unique_ptr<ClassConstructorAST>> constructors;
   //    std::unique_ptr<ClassDestructorAST> destructor;
   //    Visibility visibility = Visibility::Private;
   //    bool hasDestructor = false;
   //    while (_peek().type != TokenType::CloseBrace && _peek().type != TokenType::EndOFFile) {
   //       if (_peek().type == TokenType::Public || _peek().type == TokenType::Private || _peek().type == TokenType::Protected) {
   //          if (_peek(1).type == TokenType::Colon) {
   //             switch (_peek().type) {
   //                case TokenType::Public:
   //                   visibility = Visibility::Public;
   //                   break;
   //                case TokenType::Private:
   //                   visibility = Visibility::Private;
   //                   break;
   //                case TokenType::Protected:
   //                   visibility = Visibility::Protected;
   //                   break;
   //                default:
   //                   break;
   //             }
   //             _consume(2); // consume 'modifier:'
   //          } else {
   //             return _logError<ClassAST>("expected ':' after '" + _lexeme(_peek()) + "'");
   //          }
   //       }
   //       bool isStatic = false;
   //       if (_peek().type == TokenType::Static) {
   //          isStatic = true;
   //          _consume(); // consume static
   //       }
   //       if (_peek(1).type != TokenType::Identifier && (_lexeme(_peek()) == name || _lexeme(_peek()) == "~" + name)) {
   //          auto method = _parseConstructorOrDestructor(name);
   //          if (method->name() == name)
   //             constructors.push_back(std::make_unique<ClassConstructorAST>(visibility, std::move(method)));
   //          else if (method->name() == "~" + name) {
   //             if (!hasDestructor) {
   //                hasDestructor = true;
   //                destructor = std::make_unique<ClassDestructorAST>(std::move(method));
   //             } else
   //                return _logError<ClassAST>("class '" + name + "' already has a destructor");
   //          }
   //       } else if (_peek(2).type == TokenType::OpenParen) {
   //          auto method = _parseFunctionDeclaration();
   //          if (!method)
   //             return nullptr; // error already logged

   //          methods.push_back(std::make_unique<ClassMethodAST>(visibility, method->name(), method->type(), isStatic, std::move(method)));
   //       } else if (!_parseType()) { // checks if there is a variable declaration  // TODO: fix because of rework of _parseType()
   //          std::unique_ptr<VariableDeclarationAST> variable{static_cast<VariableDeclarationAST*>(_parseVariableDeclaration().release())};
   //          if (!variable)
   //             return nullptr; // error already logged
   //          variables.push_back(std::make_unique<ClassVariableAST>(visibility, variable->name(), variable->type(), isStatic, std::move(variable)));
   //          if (_peek().type != TokenType::Semicolon)
   //             return _logError<ClassAST>("expected ';' after variable declaration");
   //          _consume(); // consume ';'
   //       }
   //    }
   //    if (_peek().type != TokenType::CloseBrace)
   //       return _logError<ClassAST>("expected '}' after class");
   //    _consume();
   //    if (_peek().type != TokenType::Semicolon)
   //       return _logError<ClassAST>("expected ';' after class declaration");
   //    _consume();
   //    return std::make_unique<ClassAST>(name, std::move(variables), std::move(methods), std::move(constructors), std::move(destructor));
   // }
   std::unique_ptr<ExpressionAST> Parser::_parseExpression() {
      auto lhs = _parsePrimaryExpression();
      if (!lhs)
         return nullptr;
      return _parseBinaryOperationRHS(std::move(lhs));
   }

   std::unique_ptr<ExpressionAST> Parser::_parseBinaryOperationRHS(std::unique_ptr<ExpressionAST> lhs) {
      while (true) {
         int tokPrecedence = _getPrecedence(_peek().type);

         if (tokPrecedence == -1)
            return std::move(lhs);

         Token opToken = _peek();
         _consume(); // consume operator

         auto rhs = _parsePrimaryExpression();
         if (!rhs)
            return nullptr;

         TokenType nextOpType = _peek().type;
         int nextPrecedence = _getPrecedence(nextOpType);

         if (tokPrecedence > nextPrecedence) {
            rhs = _parseBinaryOperationRHS(std::move(rhs));
            if (!rhs)
               return nullptr;
         }

         lhs = std::make_unique<BinaryExpressionAST>(_lexeme(opToken), std::move(lhs), std::move(rhs));
      }
   }

   std::unique_ptr<ExpressionAST> Parser::_parseNumberExpression() {
      auto num = _lexeme(_peek());
      // auto type = _peek().type;
      _consume();
      // TODO: go back to uint64 and double after first ship
      // if (type == TokenType::IntLiteral)
      //    return std::make_unique<NumberExpressionAST>(std::stoull(num), makeInt32());

      return std::make_unique<NumberExpressionAST>(std::stod(num), makeDouble());
   }

   std::unique_ptr<ExpressionAST> Parser::_parseIdentifierExpression() {
      std::string identifier = _lexeme(_peek());
      _consume();

      return std::make_unique<VariableExpressionAST>(identifier);
   }

   std::unique_ptr<ExpressionAST> Parser::_parseStringExpression() {
      std::string str = _lexeme(_peek());
      str = str.substr(1, str.size() - 2); // remove quotes
      _consume();

      std::string result;
      result.reserve(str.size());

      for (size_t i = 0; i < str.size(); ++i) {
         if (str[i] == '\\' && i + 1 < str.size()) {
            ++i;
            switch (str[i]) {
               case 'n':
                  result.push_back('\n');
                  break;
               case 't':
                  result.push_back('\t');
                  break;
               case 'r':
                  result.push_back('\r');
                  break;
               case '"':
                  result.push_back('\"');
                  break;
               case '\'':
                  result.push_back('\'');
                  break;
               case '\\':
                  result.push_back('\\');
                  break;
               case '0':
                  result.push_back('\0');
                  break;

               case 'x': // hex \xHH
                  if (i + 2 < str.size()) {
                     std::string hex = str.substr(i + 1, 2);
                     char c = static_cast<char>(std::stoi(hex, nullptr, 16));
                     result.push_back(c);
                     i += 2;
                  } else
                     return _logError<StringExpression>("Invalid hex escape sequence");
                  break;

               case 'u': // unicode \uHHHH
                  if (i + 4 < str.size()) {
                     std::string hex = str.substr(i + 1, 4);
                     unsigned int codepoint = std::stoi(hex, nullptr, 16);
                     if (codepoint <= 0x7F)
                        result.push_back(static_cast<char>(codepoint));
                     else if (codepoint <= 0x7FF) {
                        result.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
                        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                     } else if (codepoint <= 0xFFFF) {
                        result.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
                        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                     } else if (codepoint <= 0x10FFFF) {
                        result.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
                        result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
                        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                     } else
                        return _logError<StringExpression>("Unicode codepoint out of range");
                     i += 4;
                  } else
                     return _logError<StringExpression>("Invalid unicode escape sequence");
                  break;

               default:
                  result.push_back('\\');
                  result.push_back(str[i]);
                  break;
            }
         } else
            result.push_back(str[i]);
      }

      return std::make_unique<StringExpression>(result);
   }

   std::unique_ptr<ExpressionAST> Parser::_parseFunctionCall() {
      std::string identifier = _lexeme(_peek());
      _consume(); // consume identifier
      _consume(); // consume (
      std::vector<std::unique_ptr<ExpressionAST>> args;
      while (_peek().type != TokenType::CloseParen && _peek().type != TokenType::EndOFFile) {
         args.push_back(_parseExpression());
         if (!args.back())
            return nullptr; // error already logged

         if (_peek().type == TokenType::Comma)
            _consume(); // consume ,
      }

      if (_peek().type == TokenType::EndOFFile)
         return _logError("expected ')' after function call");
      _consume(); // consume )

      // utils::debugLog("function call: '{}', args number: {}", identifier, args.size());
      // if (!_symbolTable.contains(identifier))
      //    return _logError("unknown function: '" + identifier + "'");

      return std::make_unique<CallExpressionAST>(identifier, std::move(args));
   }

   std::unique_ptr<FunctionPrototypeAST> Parser::_parseExternFunctionStatement() {
      _consume(); // consume extern
      bool C = false;
      if (_peek().type == TokenType::StringLiteral) {
         auto str = _parseStringExpression();
         if (!str)
            return nullptr;
         if (static_cast<StringExpression*>(str.get())->value() != "C")
            return _logError<FunctionPrototypeAST>("invalid extern function type");
         C = true;
      }
      auto proto = _parseFunctionPrototype();
      proto->setExternC(C);
      return std::move(proto);
   }
   // std::unique_ptr<ExpressionAST> Parser::_parseExternVariableStatement() {
   //    _consume(); // consume extern
   //    auto ext = _parseVariableDeclaration();
   //    if (ext)
   //       return std::make_unique<ExternVariableStatementAST>(std::move(ext));
   //    // TODO: set _isGlobal to true
   //    return std::move(ext);
   // }

   std::unique_ptr<ExpressionAST> Parser::_parseReturnStatement() {
      _consume(); // consume return
      if (_peek().type == TokenType::Semicolon)
         return std::make_unique<ReturnStatementAST>(nullptr, makeNullType());

      return std::make_unique<ReturnStatementAST>(_parseExpression(), _currentFunctionReturnType);
   }
   std::unique_ptr<ExpressionAST> Parser::_parseExitStatement() {
      _consume(); // consume exit
      if (_peek().type == TokenType::Semicolon)
         return _logError("expected an expression after 'exit'");

      return std::make_unique<ExitStatement>(_parseExpression());
   }

   std::unique_ptr<ExpressionAST> Parser::_parsePrimaryExpression() {
      switch (_peek().type) {
         case TokenType::Semicolon:
            _consume();
            return nullptr;
         case TokenType::True:
            _consume();
            return std::make_unique<NumberExpressionAST>((uint64_t)1, makeBool());
         case TokenType::False:
            _consume();
            return std::make_unique<NumberExpressionAST>((uint64_t)0, makeBool());
         case TokenType::CodeSuccess:
            _consume();
            // TODO: go back to Int32 after first ship
            return std::make_unique<NumberExpressionAST>(double(0), makeDouble()); // makeInt32()
         case TokenType::CodeFailure:
            _consume();
            return std::make_unique<NumberExpressionAST>(double(1), makeDouble()); // makeInt32()
         case TokenType::Null:
            _consume();
            return std::make_unique<NumberExpressionAST>(double(0), makeDouble()); // makeInt32()

         case TokenType::OpenBrace:
            return _parseBlock();

         case TokenType::OpenParen: {
            _consume(); // consume (
            auto expr = _parseExpression();
            if (_peek().type != TokenType::CloseParen)
               return _logError("expected ')' after expression");
            _consume(); // consume )
            return std::move(expr);
         }

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

         case TokenType::Ret:
            return _parseReturnStatement();
         case TokenType::Exit:
            return _parseExitStatement();

         case TokenType::StaticCast:
            return _parseStaticCast();
         case TokenType::ConstCast:
            return _parseConstCast();

         case TokenType::If:
            return _parseIfStatement();
         case TokenType::While:
            return _parseWhileStatement();
         case TokenType::Do:
            return _parseDoWhileStatement();
         case TokenType::For:
            return _parseForStatement();

         case TokenType::Num:
         case TokenType::Int:
         case TokenType::Int8:
         case TokenType::Int16:
         case TokenType::Int32:
         case TokenType::Int64:
         case TokenType::Int128:
         case TokenType::Uint:
         case TokenType::Uint8:
         case TokenType::Uint16:
         case TokenType::Uint32:
         case TokenType::Uint64:
         case TokenType::Uint128:
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

         case TokenType::Minus:
         case TokenType::MinusMinus:
         case TokenType::PlusPlus:
         case TokenType::LogicalNot:
         case TokenType::BitwiseAnd:
         case TokenType::Multiply:
            return _parseUnaryExpression();

         default:
            _consume();
            return _logError("unexpected token '" + _lexeme(_peek()) + "' when expecting an expression");
      }
   }
   std::unique_ptr<ExpressionAST> Parser::_parseUnaryExpression() {
      std::string op = _lexeme(_peek());
      _consume(); // consume operator

      std::unique_ptr<ExpressionAST> operand = _parsePrimaryExpression();
      if (!operand)
         return nullptr;

      return std::make_unique<UnaryExpressionAST>(op, std::move(operand));
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

      auto expr = std::make_unique<__BuiltinLineAST>(_line(_peek()));
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
      _consume(); // consume typeof
      if (_peek().type != TokenType::OpenParen)
         return _logError("expected '(' after 'typeof'");
      _consume(); // consume '('
      if (_peek().type != TokenType::Identifier)
         return _logError("expected an identifier after '(' in 'typeof'");
      std::string identifier = _lexeme(_peek());
      _consume(); // consume identifier
      if (_peek().type != TokenType::CloseParen)
         return _logError("expected ')' after identifier in 'typeof'");
      _consume(); // consume ')'
      return std::make_unique<TypeOfStatementAST>(identifier);
   }
   std::unique_ptr<ExpressionAST> Parser::_parseNameof() {
      _consume(); // consume nameof
      if (_peek().type != TokenType::OpenParen)
         return _logError("expected '(' after 'nameof'");
      _consume(); // consume '('
      if (_peek().type != TokenType::Identifier)
         return _logError("expected an identifier after '(' in 'nameof'");
      std::string identifier = _lexeme(_peek());
      _consume(); // consume identifier
      if (_peek().type != TokenType::CloseParen)
         return _logError("expected ')' after identifier in 'nameof'");
      _consume(); // consume ')'
      return std::make_unique<NameOfStatementAST>(identifier);
   }
   std::unique_ptr<ExpressionAST> Parser::_parseSizeof() {
      _consume(); // consume sizeof
      if (_peek().type != TokenType::OpenParen)
         return _logError("expected '(' after 'nameof'");
      _consume(); // consume '('
      if (_peek().type != TokenType::Identifier)
         return _logError("expected an identifier after '(' in 'nameof'");
      std::string identifier = _lexeme(_peek());
      _consume(); // consume identifier
      if (_peek().type != TokenType::CloseParen)
         return _logError("expected ')' after identifier in 'nameof'");
      _consume(); // consume ')'
      return std::make_unique<SizeOfStatementAST>(identifier);
   }
   std::unique_ptr<ExpressionAST> Parser::_parseStaticCast() {
      _consume(); // consume staticCast
      if (_peek().type != TokenType::Less)
         return _logError("expected '<' after 'staticCast'");
      _consume(); // consume '<'
      std::shared_ptr<Type> type = _parseType();
      if (!type)
         return _logError("expected a type after '<' in 'staticCast'");
      if (_peek().type != TokenType::Greater)
         return _logError("expected '>' after type in 'staticCast'");
      _consume(); // consume '>'
      if (_peek().type != TokenType::OpenParen)
         return _logError("expected '(' after '>' in 'staticCast'");
      _consume(); // consume '('
      std::unique_ptr<ExpressionAST> value = _parseExpression();
      if (!value)
         return nullptr; // error already logged
      if (_peek().type != TokenType::CloseParen)
         return _logError("expected ')' after expression in 'staticCast'");
      _consume(); // consume ')'
      return std::make_unique<StaticCastAST>(type, std::move(value));
   }
   std::unique_ptr<ExpressionAST> Parser::_parseConstCast() {
      _consume(); // consume constCast
      if (_peek().type != TokenType::Less)
         return _logError("expected '<' after 'constCast'");
      _consume(); // consume '<'
      std::shared_ptr<Type> type = _parseType();
      if (!type)
         return _logError("expected a type after '<' in 'constCast'");
      if (_peek().type != TokenType::Greater)
         return _logError("expected '>' after type in 'constCast'");
      _consume(); // consume '>'
      if (_peek().type != TokenType::OpenParen)
         return _logError("expected '(' after '>' in 'constCast'");
      _consume(); // consume '('
      std::unique_ptr<ExpressionAST> value = _parseExpression();
      if (!value)
         return nullptr; // error already logged
      if (_peek().type != TokenType::CloseParen)
         return _logError("expected ')' after expression in 'constCast'");
      _consume(); // consume ')'
      return std::make_unique<ConstCastAST>(type, std::move(value));
   }

   std::shared_ptr<Type> Parser::_parseType() {
      Token baseType = _peek();
      std::shared_ptr<Type> type;
      switch (baseType.type) {
         using enum TokenType;
         case ArgPack:
            type = makeArgPack();
            break;
         case Num:
            type = makeDouble();
            break;
         case Void:
            type = makeVoid();
            break;
         case Char:
            type = makeChar();
            break;
         case Bool:
            type = makeBool();
            break;
         case String:
            type = makeString();
            break;
         case Int8:
            type = makeInt8();
            break;
         case Int16:
            type = makeInt16();
            break;
         case Int32:
         case Int:
            type = makeInt32();
            break;
         case Int64:
            type = makeInt64();
            break;
         case Int128:
            type = makeInt128();
            break;
         case Uint8:
            type = makeUint8();
            break;
         case Uint16:
            type = makeUint16();
            break;
         case Uint32:
         case Uint:
            type = makeUint32();
            break;
         case Uint64:
            type = makeUint64();
            break;
         case Uint128:
            type = makeUint128();
            break;
         case Float:
            type = makeFloat();
            break;
         case Double:
            type = makeDouble();
            break;

         case Identifier:
            type = makeClass();
            break;

         default:
            return nullptr;
      };
      _consume(); // consume type
      if (type->kind() == TypeKind::ArgPack)
         return type;
      while (_peek().type == TokenType::Multiply || _peek().type == TokenType::OpenBracket) {
         switch (_peek().type) {
            case TokenType::Multiply:
               type = makePointer(type);
               _consume(); // consume '*'
               break;
            case TokenType::OpenBracket:
               _consume(); // consume '['
               if (std::unique_ptr<ExpressionAST> expr = _parseExpression()) {
                  if (_peek().type == TokenType::CloseBracket)
                     _consume(); // consume ']'
                  type = makeArray(type, std::move(expr));
               }
               break;
            default:
               break;
         }
      }
      return type;
   }

   // std::unique_ptr<ExpressionAST> Parser::_parseAlias() {
   // }

   std::unique_ptr<ExpressionAST> Parser::_parseVariableDeclaration() {
      std::shared_ptr<Type> type = _parseType();

      if (_peek().type != TokenType::Identifier)
         return _logError("expected an identifier after type in variable declaration but got: '" + _lexeme(_peek()) + "'");

      std::string name = _lexeme(_peek());
      _consume(); // consume identifier

      std::unique_ptr<ExpressionAST> initExpr = nullptr;

      if (_peek().type == TokenType::Equal || _peek().type == TokenType::OpenBrace) {
         _consume(); // consume '=' / '{'
         initExpr = _parseExpression();
         if (_peek().type == TokenType::CloseBrace)
            _consume(); // consume '}'

         // if (initExpr) {
         //    if (auto* expr = dynamic_cast<NumberExpressionAST*>(initExpr.get())) {
         //       expr->setType(type);
         //    } else if (auto* expr = dynamic_cast<BinaryExpressionAST*>(initExpr.get())) {
         //       expr->setType(type);
         //    }
         // }
      }

      // utils::debugLog("definition of variable '{}' with type '{}'", name, type);

      return std::make_unique<VariableDeclarationAST>(type, name, std::move(initExpr));
   }

   std::unique_ptr<FunctionAST> Parser::_parseFunctionDeclaration() {
      auto proto = _parseFunctionPrototype();

      if (_peek().type != TokenType::OpenBrace)
         return _logError<FunctionAST>("expected '{'");

      _currentFunctionReturnType = proto->type();
      _currentFunctionName = proto->name();
      std::unique_ptr<BlockStatementAST> body = _parseBlock();
      return std::make_unique<FunctionAST>(std::move(proto), std::move(body));
   }

   std::unique_ptr<ExpressionAST> Parser::_parseArgument() {
      std::shared_ptr<Type> type = _parseType();
      if (type->kind() == TypeKind::ArgPack)
         return std::make_unique<VariableDeclarationAST>(type, "", nullptr);

      std::string name = _lexeme(_peek());
      _consume(); // consume identifier

      // if (_symbolTable.contains(name))
      //    return _logError("redefinition of '" + name + "'");
      // _symbolTable.insert({name, SymbolType::Variable});

      std::unique_ptr<ExpressionAST> initExpr = nullptr;
      if (_peek().type == TokenType::Equal) {
         _consume(); // consume '=' / '{'
         initExpr = _parseExpression();
      } else if (_peek().type == TokenType::OpenBrace) {
         _consume(); // consume '{'
         initExpr = _parseExpression();
         _consume(); // consume '}'
      }
      if (initExpr) // TODO: allow default values (still have to figure out how to make them initialize at the location where the function is called instead of where it's defined)
         return _logError<VariableDeclarationAST>("function arguments cannot have an initializer");

      return std::make_unique<VariableDeclarationAST>(type, name, std::move(initExpr));
   }

   std::unique_ptr<BlockStatementAST> Parser::_parseBlock() {
      _consume(); // consume {
      std::vector<std::unique_ptr<ExpressionAST>> statements{};
      while (_peek().type != TokenType::CloseBrace && _peek().type != TokenType::EndOFFile) {
         if (auto expr = _parseExpression())
            statements.push_back(std::move(expr));

         switch (statements.back()->astType()) {
            case ASTType::BlockStatement:
            case ASTType::ForStatement:
            case ASTType::WhileStatement:
            case ASTType::IfStatement:
               break;
            // case ASTType::IfStatement:
            //    if (auto* ifStmnt = static_cast<IfStatementAST*>(statements.back().get())) {
            //       if (!ifStmnt->elseBranch())
            //    }
            default:
               if (_peek().type != TokenType::Semicolon)
                  return _logError<BlockStatementAST>("expected ';' before '" + _lexeme(_peek()) + "'");
               _consume(); // consume ';'
         }
      }
      if (_peek().type != TokenType::CloseBrace)
         return _logError<BlockStatementAST>("expected '}'");
      _consume(); // consume }

      return std::make_unique<BlockStatementAST>(std::move(statements));
   }

   std::unique_ptr<FunctionPrototypeAST> Parser::_parseFunctionPrototype() {
      _consume(); // consume 'fun'

      if (_peek().type != TokenType::Identifier)
         return _logError<FunctionPrototypeAST>("expected an identifier after fun in function declaration but got: '" + _lexeme(_peek()) + "'");
      std::string name = _lexeme(_peek());
      _consume(); // consume identifier

      std::vector<std::unique_ptr<ExpressionAST>> args;

      if (_peek().type != TokenType::OpenParen)
         return _logError<FunctionPrototypeAST>("expected '('");
      _consume(); // consume '('

      while (_peek().type != TokenType::CloseParen && _peek().type != TokenType::EndOFFile) {
         args.push_back(_parseArgument());
         // utils::debugLog("argument {}: {}", args.size(), _lexeme(tok));
         if (_peek().type == TokenType::Comma)
            _consume(); // consume ,
      }
      if (_peek().type == TokenType::EndOFFile)
         return _logError<FunctionPrototypeAST>("expected ')'");
      _consume(); // consumes )

      if (_peek().type != TokenType::Arrow)
         return _logError<FunctionPrototypeAST>("expected '->'");
      _consume(); // consume '->'

      std::shared_ptr<Type> type = _parseType();
      if (type->kind() == TypeKind::ArgPack)
         return _logError<FunctionPrototypeAST>("variadic arguments are not allowed as type");

      return std::make_unique<FunctionPrototypeAST>(type, name, std::move(args));
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

      thenBranch = _parseExpression();
      if (!thenBranch)
         return nullptr; // error already logged
      switch (thenBranch->astType()) {
         case ASTType::BlockStatement:
         case ASTType::ForStatement:
         case ASTType::WhileStatement:
         case ASTType::IfStatement:
            break;
         default:
            if (_peek().type != TokenType::Semicolon)
               return _logError("expected ';' before '" + _lexeme(_peek()) + "'");
            _consume(); // consume ';'
      }

      if (_peek().type == TokenType::Else) {
         _consume(); // consume else
         elseBranch = _parseExpression();
         if (!elseBranch)
            return nullptr; // error already logged
         switch (elseBranch->astType()) {
            case ASTType::BlockStatement:
            case ASTType::ForStatement:
            case ASTType::WhileStatement:
            case ASTType::IfStatement:
               break;
            default:
               if (_peek().type != TokenType::Semicolon)
                  return _logError("expected ';' before '" + _lexeme(_peek()) + "'");
               _consume(); // consume ';'
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
         return _logError("expected '{}' after for statement");

      return std::make_unique<ForStatementAST>(std::move(init), std::move(condition), std::move(increment), std::move(body));
   }

   std::unique_ptr<ExpressionAST> Parser::_parseImport() {
      _consume(); // consume import
      if (_peek().type != TokenType::Identifier)
         return _logError("expected an identifier after 'import'");
      std::string module = _lexeme(_peek());
      _consume(); // consume identifier
      // TODO: Implement module import logic
      // utils::debugLog("Importing module: {}", module);

      return std::make_unique<ImportStatementAST>(module);
   }
} // namespace tungsten
