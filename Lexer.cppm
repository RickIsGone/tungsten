module;

#include <fstream>
#include <filesystem>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <vector>
#ifndef _NODISCARD
#define _NODISCARD [[nodiscard]]
#endif
export module Tungsten.lexer;
namespace fs = std::filesystem;

namespace tungsten {

   export enum class TokenType {
      INVALID = -1,

      // keywords
      RETURN = -2,
      EXIT = -3,
      NEW = -4,
      FREE = -5,
      EXTERN = -6,

      // operators
      PLUS = -7,
      PLUS_EQUAL = -8,
      PLUS_PLUS = -9,
      MINUS = -10,
      MINUS_EQUAL = -11,
      MINUS_MINUS = -12,
      EQUAL = -13,
      EQUAL_EQUAL = -14,
      DIVIDE = -15,
      DIVIDE_EQUAL = -16,

      // types
      INT = -17,
      FLOAT = -18,
      DOUBLE = -19,
      BOOL = -20,
      CHAR = -21,
      STRING = -22,
      VOID = -23,
      UINT = -24,
      UINT8 = -25,
      UINT16 = -26,
      UINT32 = -27,
      UINT64 = -28,
      INT8 = -29,
      INT16 = -30,
      INT32 = -31,
      INT64 = -32,

      // literals
      INT_LITERAL = -33,
      STRING_LITERAL = -34,
      IDENTIFIER = -35,

      OPEN_PAREN = -36,
      CLOSE_PAREN = -37,
      OPEN_BRACE = -38,
      CLOSE_BRACE = -39,
      OPEN_BRACKET = -40,
      CLOSE_BRACKET = -41,

      SEMICOLON = -42,

      END_OF_FILE = -43
   };

   export inline std::unordered_map<std::string, TokenType> tokensMap = {
       // keywords
       {"return", TokenType::RETURN},
       {"exit", TokenType::EXIT},
       {"new", TokenType::NEW},
       {"free", TokenType::FREE},
       {"extern", TokenType::EXTERN},

       // types
       {"Int", TokenType::INT},
       {"Float", TokenType::FLOAT},
       {"Double", TokenType::DOUBLE},
       {"Bool", TokenType::BOOL},
       {"Char", TokenType::CHAR},
       {"String", TokenType::STRING},
       {"Void", TokenType::VOID},
       {"Uint", TokenType::UINT},
       {"Uint8", TokenType::UINT8},
       {"Uint16", TokenType::UINT16},
       {"Uint32", TokenType::UINT32},
       {"Uint64", TokenType::UINT64},
       {"Int8", TokenType::INT8},
       {"Int16", TokenType::INT16},
       {"Int32", TokenType::INT32},
       {"Int64", TokenType::INT64}};

   export struct Token {
      TokenType type{TokenType::INVALID};
      std::optional<std::string> value{};
   };

   export class Lexer {
   public:
      explicit Lexer(const fs::path& path) : _path(path) {}
      Lexer() = default;
      ~Lexer() = default;
      Lexer(const Lexer&) = delete;
      Lexer operator=(const Lexer&) = delete;

      _NODISCARD std::vector<Token> tokenize();
      void setFileTarget(const fs::path& path);

   private:
      _NODISCARD std::optional<char> _peek(size_t offset = 0) const;
      void _consume(const size_t amount = 1) { _index += amount; }

      size_t _index{0};
      fs::path _path{};
      std::string _fileContents{};
   };

   /*  ========================================== implementation ==========================================  */

   /**
    * bool isPrimitiveType(const std::string& type) {
    *    return type == "Int" || type == "Float" || type == "Double" || type == "Bool" || type == "Char" || type == "String" || type == "Void" || type == "Uint" || type == "Uint8" || type == "Uint16" || type == "Uint32" || type == "Uint64" || type == "Int8" || type == "Int16" || type == "Int64";
    * }
    * bool isKeyword(const std::string& keyword) {
    *    return keyword == "return" || keyword == "exit" || keyword == "new" || keyword == "free" || keyword == "extern";
    * }
    */

   std::vector<Token> Lexer::tokenize() {
      std::ifstream inputFile{_path};
      std::stringstream ss{};
      std::vector<Token> tokens{};
      ss << inputFile.rdbuf();
      _fileContents = ss.str();

      while (_peek().has_value()) {
         std::string buffer;
         if (std::isspace(static_cast<unsigned char>(_peek().value()))) {
            do {
               _consume();
               if (!_peek().has_value()) break;
            } while (std::isspace(static_cast<unsigned char>(_peek().value())));

         } else if (std::isalpha(static_cast<unsigned char>(_peek().value()))) {
            // KEYWORDS, TYPES AND IDENTIFIERS
            do {
               buffer.push_back(_peek().value());
               _consume();
            } while (std::isalnum(static_cast<unsigned char>(_peek().value())) || _peek().value() == '_');

            if (tokensMap.contains(buffer)) // looking for known keywords/types
               tokens.push_back({tokensMap.at(buffer)});

            else
               tokens.push_back({TokenType::IDENTIFIER, buffer});

            buffer.clear();

         } else if (std::isdigit(static_cast<unsigned char>(_peek().value()))) {
            // INT LITERALS
            do {
               buffer.push_back(_peek().value());
               _consume();
            } while (std::isdigit(static_cast<unsigned char>(_peek().value())));
            tokens.push_back({TokenType::INT_LITERAL, buffer});
            buffer.clear();

         } else {
            switch (_peek().value()) {
               case ';':
                  tokens.push_back({TokenType::SEMICOLON});
                  _consume();
                  break;

               // OPERATORS
               case '=':
                  if (_peek(1).has_value()) {
                     if (_peek(1).value() == '=') {
                        tokens.push_back({TokenType::EQUAL_EQUAL});
                        _consume(2);
                     } else {
                        tokens.push_back({TokenType::EQUAL});
                        _consume();
                     }
                  } else {
                     tokens.push_back({TokenType::EQUAL});
                     _consume();
                  }
                  break;

               case '+':
                  if (_peek(1).has_value()) {
                     if (_peek(1).value() == '+') {
                        tokens.push_back({TokenType::PLUS_PLUS});
                        _consume(2);
                     } else if (_peek(1).value() == '=') {
                        tokens.push_back({TokenType::PLUS_EQUAL});
                        _consume(2);
                     } else {
                        tokens.push_back({TokenType::PLUS});
                        _consume();
                     }
                  } else {
                     tokens.push_back({TokenType::PLUS});
                     _consume();
                  }
                  break;

               case '-':
                  if (_peek(1).has_value()) {
                     if (_peek(1).value() == '-') {
                        tokens.push_back({TokenType::MINUS_MINUS});
                        _consume(2);

                     } else if (_peek(1).value() == '=') {
                        tokens.push_back({TokenType::MINUS_EQUAL});
                        _consume(2);
                     } else {
                        tokens.push_back({TokenType::MINUS});
                        _consume();
                     }
                  } else {
                     tokens.push_back({TokenType::MINUS});
                     _consume();
                  }
                  break;


               case '/':
                  // COMMENTS
                  if (_peek(1).has_value()) {
                     if (_peek(1).value() == '/')
                        while (_peek().has_value() && _peek().value() != '\n')
                           _consume();
                     else if (_peek(1).value() == '*') {
                        while (_peek().has_value()) {
                           if (_peek().value() == '*' && _peek(1).has_value() && _peek(1).value() == '/') {
                              _consume(2);
                              break;
                           }
                           _consume();
                        }
                        // OPERATORS
                     } else if (_peek(1).value() == '=') {
                        tokens.push_back({TokenType::DIVIDE_EQUAL});
                        _consume(2);
                     } else {
                        tokens.push_back({TokenType::DIVIDE});
                        _consume();
                     }
                  } else {
                     tokens.push_back({TokenType::DIVIDE});
                     _consume();
                  }
                  break;

               case '"':
                  // STRING LITERALS
                  if (_peek(1).has_value()) {
                     if (_peek(1).value() == '"') {
                        tokens.push_back({TokenType::STRING_LITERAL, ""});
                        _consume(2);
                     } else {
                        _consume();
                        while (_peek().has_value() && _peek().value() != '"' && _peek().value() != '\n') {
                           buffer.push_back(_peek().value());
                           _consume();
                        }
                        if (_peek().has_value() && _peek().value() == '"') {
                           tokens.push_back({TokenType::STRING_LITERAL, buffer});
                           _consume();
                           buffer.clear();
                        }
                     }
                  } else
                     _consume();
                  break;

               case '(':
                  tokens.push_back({TokenType::OPEN_PAREN});
                  _consume();
                  break;

               case ')':
                  tokens.push_back({TokenType::CLOSE_PAREN});
                  _consume();
                  break;

               case '{':
                  tokens.push_back({TokenType::OPEN_BRACE});
                  _consume();
                  break;

               case '}':
                  tokens.push_back({TokenType::CLOSE_BRACE});
                  _consume();
                  break;

               case '[':
                  tokens.push_back({TokenType::OPEN_BRACKET});
                  _consume();
                  break;

               case ']':
                  tokens.push_back({TokenType::CLOSE_BRACKET});
                  _consume();
                  break;

               default:
                  tokens.push_back({TokenType::INVALID});
                  _consume();
                  break;
            }
         }
      }
      tokens.push_back({TokenType::END_OF_FILE});
      return tokens;
   }

   std::optional<char> Lexer::_peek(const size_t offset) const {
      if (_index + offset + 1 <= _fileContents.size())
         return _fileContents.at(_index + offset);

      return std::nullopt;
   }

   void Lexer::setFileTarget(const fs::path& path) {
      _path = path;
      _index = 0;
   }

} // namespace tungsten
