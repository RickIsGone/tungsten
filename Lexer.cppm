module;

#include <fstream>
#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>

export module Tungsten.lexer;
namespace fs = std::filesystem;

namespace tungsten {

   export enum class TokenType {
      INVALID,

      ENTRY_POINT,

      KEYWORD,

      OPERATOR,

      PRIMITIVE_TYPE,
      CLASS_TYPE,

      INT_LITERAL,
      STRING_LITERAL,

      IDENTIFIER,

      OPEN_PAREN,
      CLOSE_PAREN,
      OPEN_BRACE,
      CLOSE_BRACE,
      OPEN_BRACKET,
      CLOSE_BRACKET,

      SEMICOLON,
   };


   export struct Token {
      const TokenType type{TokenType::INVALID};
      const std::optional<std::string> value{};
   };

   export class Lexer {
   public:
      Lexer() = default;
      ~Lexer() = default;

      std::vector<Token> tokenize();
      void setFileTarget(const fs::path& path);

   private:
      std::optional<char> _Peek(size_t offset = 0);
      void _Consume(const size_t amount = 1) { _Index += amount; }

      size_t _Index{0};
      fs::path _Path;
      std::string _FileContents;
   };

   /*  ========================================== implementation ==========================================  */

   bool isPrimitiveType(const std::string& type) {
      return type == "Int" || type == "Float" || type == "Double" || type == "Bool" || type == "Char" || type == "String" || type == "Void" || type == "Uint" || type == "Uint8" || type == "Uint16" || type == "Uint32" || type == "Uint64";
   }
   bool isKeyword(const std::string& keyword) {
      return keyword == "return" || keyword == "exit" || keyword == "new" || keyword == "free";
   }

   std::vector<Token> Lexer::tokenize() {
      std::ifstream inputFile{_Path};
      std::stringstream ss{};
      std::vector<Token> tokens{};
      ss << inputFile.rdbuf();
      _FileContents = ss.str();

      std::cout << _FileContents << '\n';
      while (_Peek().has_value()) {
         std::string buffer;
         if (std::isspace(_Peek().value())) {
            do {
               _Consume();
               if (!_Peek().has_value()) break;
            } while (std::isspace(_Peek().value()));

         } else if (std::isalpha(_Peek().value())) {
            // KEYWORDS, TYPES AND IDENTIFIERS
            do {
               buffer.push_back(_Peek().value());
               _Consume();
            } while (std::isalnum(_Peek().value()) || _Peek().value() == '_');
            if (isKeyword(buffer))
               tokens.push_back({TokenType::KEYWORD, buffer});

            else if (isPrimitiveType(buffer))
               tokens.push_back({TokenType::PRIMITIVE_TYPE, buffer});

            else {
               tokens.push_back({TokenType::IDENTIFIER, buffer});
            }
            buffer.clear();

         } else if (std::isdigit(_Peek().value())) {
            // INT LITERALS
            do {
               buffer.push_back(_Peek().value());
               _Consume();
            } while (std::isdigit(_Peek().value()));
            tokens.push_back({TokenType::INT_LITERAL, buffer});
            buffer.clear();

         } else {
            switch (_Peek().value()) {
               case ';':
                  _Consume();
                  tokens.push_back({TokenType::SEMICOLON});
                  break;

               // OPERATORS
               case '=':
                  if (_Peek(1).has_value()) {
                     if (_Peek(1).value() == '=') {
                        _Consume(2);
                        tokens.push_back({TokenType::OPERATOR, "=="});
                     } else {
                        _Consume();
                        tokens.push_back({TokenType::OPERATOR, "="});
                     }
                  } else {
                     tokens.push_back({TokenType::OPERATOR, "="});
                     _Consume();
                  }
                  break;

               case '+':
                  if (_Peek(1).has_value()) {
                     if (_Peek(1).value() == '+') {
                        _Consume(2);
                        tokens.push_back({TokenType::OPERATOR, "++"});
                     } else if (_Peek(1).value() == '=') {
                        _Consume(2);
                        tokens.push_back({TokenType::OPERATOR, "+="});
                     } else {
                        _Consume();
                        tokens.push_back({TokenType::OPERATOR, "+"});
                     }
                  } else {
                     tokens.push_back({TokenType::OPERATOR, "+"});
                     _Consume();
                  }
                  break;

               case '-':
                  if (_Peek(1).has_value()) {
                     if (_Peek(1).value() == '-') {
                        _Consume(2);
                        tokens.push_back({TokenType::OPERATOR, "--"});

                     } else if (_Peek(1).value() == '=') {
                        _Consume(2);
                        tokens.push_back({TokenType::OPERATOR, "-="});
                     } else {
                        _Consume();
                        tokens.push_back({TokenType::OPERATOR, "-"});
                     }
                  } else {
                     tokens.push_back({TokenType::OPERATOR, "-"});
                     _Consume();
                  }
                  break;


               case '/':
                  // COMMENTS
                  if (_Peek(1).has_value()) {
                     if (_Peek(1).value() == '/')
                        while (_Peek().has_value() && _Peek().value() != '\n')
                           _Consume();
                     else if (_Peek(1).value() == '*') {
                        while (_Peek().has_value()) {
                           if (_Peek().value() == '*' && _Peek(1).has_value() && _Peek(1).value() == '/') {
                              _Consume(2);
                              break;
                           }
                           _Consume();
                        }
                        // OPERATORS
                     } else if (_Peek(1).value() == '=') {
                        _Consume(2);
                        tokens.push_back({TokenType::OPERATOR, "/="});
                     } else {
                        _Consume();
                        tokens.push_back({TokenType::OPERATOR, "/"});
                     }
                  } else {
                     _Consume();
                     tokens.push_back({TokenType::OPERATOR, "/"});
                  }
                  break;

               case '"':
                  // STRING LITERALS
                  if (_Peek(1).has_value()) {
                     if (_Peek(1).value() == '"') {
                        _Consume(2);
                        tokens.push_back({TokenType::STRING_LITERAL, ""});
                     } else {
                        _Consume();
                        while (_Peek().has_value() && _Peek().value() != '"' && _Peek().value() != '\n') {
                           buffer.push_back(_Peek().value());
                           _Consume();
                        }
                        _Consume();
                        if (_Peek().has_value() && _Peek().value() == '"') {
                           _Consume();
                           tokens.push_back({TokenType::STRING_LITERAL, buffer});
                           buffer.clear();
                        }
                     }
                  } else
                     _Consume();

               case '(':
                  tokens.push_back({TokenType::OPEN_PAREN});
                  _Consume();
                  break;

               case ')':
                  tokens.push_back({TokenType::CLOSE_PAREN});
                  _Consume();
                  break;

               case '{':
                  tokens.push_back({TokenType::OPEN_BRACE});
                  _Consume();
                  break;

               case '}':
                  tokens.push_back({TokenType::CLOSE_BRACE});
                  _Consume();
                  break;

               case '[':
                  tokens.push_back({TokenType::OPEN_BRACKET});
                  _Consume();
                  break;

               case ']':
                  tokens.push_back({TokenType::CLOSE_BRACKET});
                  _Consume();
                  break;

               default:
                  _Consume();
            }
         }
      }

      return tokens;
   }

   std::optional<char> Lexer::_Peek(const size_t offset) {
      if (_Index + offset + 1 <= _FileContents.size())
         return _FileContents.at(_Index + offset);

      return std::nullopt;
   }

   void Lexer::setFileTarget(const fs::path& path) {
      _Path = path;
      _Index = 0;
   }

} // namespace tungsten
