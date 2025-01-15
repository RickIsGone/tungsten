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
      INT64 = -31,

      // literals
      INT_LITERAL = -32,
      STRING_LITERAL = -33,
      IDENTIFIER = -34,

      OPEN_PAREN = -35,
      CLOSE_PAREN = -36,
      OPEN_BRACE = -37,
      CLOSE_BRACE = -38,
      OPEN_BRACKET = -39,
      CLOSE_BRACKET = -40,

      SEMICOLON = -41,

      END_OF_FILE = -42
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
       {"Int64", TokenType::INT64}};

   export struct Token {
      TokenType type{TokenType::INVALID};
      std::optional<std::string> value{};
   };

   export class Lexer {
   public:
      explicit Lexer(const fs::path& path) : _Path(path) {}
      Lexer() = default;
      ~Lexer() = default;
      Lexer(const Lexer&) = delete;
      Lexer operator=(const Lexer&) = delete;

      _NODISCARD std::vector<Token> tokenize();
      void setFileTarget(const fs::path& path);

   private:
      _NODISCARD std::optional<char> _Peek(size_t offset = 0) const;
      void _Consume(const size_t amount = 1) { _Index += amount; }

      size_t _Index{0};
      fs::path _Path{};
      std::string _FileContents{};
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
      std::ifstream inputFile{_Path};
      std::stringstream ss{};
      std::vector<Token> tokens{};
      ss << inputFile.rdbuf();
      _FileContents = ss.str();

      while (_Peek().has_value()) {
         std::string buffer;
         if (std::isspace(static_cast<unsigned char>(_Peek().value()))) {
            do {
               _Consume();
               if (!_Peek().has_value()) break;
            } while (std::isspace(static_cast<unsigned char>(_Peek().value())));

         } else if (std::isalpha(static_cast<unsigned char>(_Peek().value()))) {
            // KEYWORDS, TYPES AND IDENTIFIERS
            do {
               buffer.push_back(_Peek().value());
               _Consume();
            } while (std::isalnum(static_cast<unsigned char>(_Peek().value())) || _Peek().value() == '_');

            if (tokensMap.contains(buffer))
               tokens.push_back({tokensMap.at(buffer)});

            else
               tokens.push_back({TokenType::IDENTIFIER, buffer});

            buffer.clear();

         } else if (std::isdigit(static_cast<unsigned char>(_Peek().value()))) {
            // INT LITERALS
            do {
               buffer.push_back(_Peek().value());
               _Consume();
            } while (std::isdigit(static_cast<unsigned char>(_Peek().value())));
            tokens.push_back({TokenType::INT_LITERAL, buffer});
            buffer.clear();

         } else {
            switch (_Peek().value()) {
               case ';':
                  tokens.push_back({TokenType::SEMICOLON});
                  _Consume();
                  break;

               // OPERATORS
               case '=':
                  if (_Peek(1).has_value()) {
                     if (_Peek(1).value() == '=') {
                        tokens.push_back({TokenType::EQUAL_EQUAL});
                        _Consume(2);
                     } else {
                        tokens.push_back({TokenType::EQUAL});
                        _Consume();
                     }
                  } else {
                     tokens.push_back({TokenType::EQUAL});
                     _Consume();
                  }
                  break;

               case '+':
                  if (_Peek(1).has_value()) {
                     if (_Peek(1).value() == '+') {
                        tokens.push_back({TokenType::PLUS_PLUS});
                        _Consume(2);
                     } else if (_Peek(1).value() == '=') {
                        tokens.push_back({TokenType::PLUS_EQUAL});
                        _Consume(2);
                     } else {
                        tokens.push_back({TokenType::PLUS});
                        _Consume();
                     }
                  } else {
                     tokens.push_back({TokenType::PLUS});
                     _Consume();
                  }
                  break;

               case '-':
                  if (_Peek(1).has_value()) {
                     if (_Peek(1).value() == '-') {
                        tokens.push_back({TokenType::MINUS_MINUS});
                        _Consume(2);

                     } else if (_Peek(1).value() == '=') {
                        tokens.push_back({TokenType::MINUS_EQUAL});
                        _Consume(2);
                     } else {
                        tokens.push_back({TokenType::MINUS});
                        _Consume();
                     }
                  } else {
                     tokens.push_back({TokenType::MINUS});
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
                        tokens.push_back({TokenType::DIVIDE_EQUAL});
                        _Consume(2);
                     } else {
                        tokens.push_back({TokenType::DIVIDE});
                        _Consume();
                     }
                  } else {
                     tokens.push_back({TokenType::DIVIDE});
                     _Consume();
                  }
                  break;

               case '"':
                  // STRING LITERALS
                  if (_Peek(1).has_value()) {
                     if (_Peek(1).value() == '"') {
                        tokens.push_back({TokenType::STRING_LITERAL, ""});
                        _Consume(2);
                     } else {
                        _Consume();
                        while (_Peek().has_value() && _Peek().value() != '"' && _Peek().value() != '\n') {
                           buffer.push_back(_Peek().value());
                           _Consume();
                        }
                        _Consume();
                        if (_Peek().has_value() && _Peek().value() == '"') {
                           tokens.push_back({TokenType::STRING_LITERAL, buffer});
                           _Consume();
                           buffer.clear();
                        }
                     }
                  } else
                     _Consume();
                  break;

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
                  tokens.push_back({TokenType::INVALID});
                  _Consume();
                  break;
            }
         }
      }
      tokens.push_back({TokenType::END_OF_FILE});
      return tokens;
   }

   std::optional<char> Lexer::_Peek(const size_t offset) const {
      if (_Index + offset + 1 <= _FileContents.size())
         return _FileContents.at(_Index + offset);

      return std::nullopt;
   }

   void Lexer::setFileTarget(const fs::path& path) {
      _Path = path;
      _Index = 0;
   }

} // namespace tungsten
