module;

#include <fstream>
#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>
#ifndef _NODISCARD
#define _NODISCARD [[nodiscard]]
#endif

export module Tungsten.lexer;
import Tungsten.token;
namespace fs = std::filesystem;
namespace tungsten {

   export class Lexer {
   public:
      explicit Lexer(const fs::path& path) : _path(path) {}
      Lexer() = default;
      ~Lexer() = default;
      Lexer(const Lexer&) = delete;
      Lexer operator=(const Lexer&) = delete;

      _NODISCARD std::vector<Token> tokenize();
      _NODISCARD const std::string& raw() const { return _raw; }
      void setFileTarget(const fs::path& path);

   private:
      _NODISCARD std::optional<char> _peek(size_t offset = 0) const;
      _NODISCARD std::optional<TokenType> _determineFixedSizeTokenType(std::string_view token) const;
      void _consume(const size_t amount = 1) { _index += amount; }

      size_t _index{0};
      fs::path _path{};
      std::string _raw{};
   };

   /*  ========================================== implementation ==========================================  */

   std::vector<Token> Lexer::tokenize() {
      std::ifstream inputFile{_path};
      std::stringstream ss{};
      std::vector<Token> tokens{};
      ss << inputFile.rdbuf();
      _raw = ss.str();

      while (_peek().has_value()) {
         size_t startingIndex = _index;
         std::string buffer;
         if (std::isspace(static_cast<unsigned char>(_peek().value()))) {
            do {
               _consume();
               if (!_peek().has_value()) break;
            } while (std::isspace(static_cast<unsigned char>(_peek().value())));

         } else if (std::isalpha(static_cast<unsigned char>(_peek().value())) || _peek().value() == '_') {
            // KEYWORDS, TYPES AND IDENTIFIERS
            do {
               buffer.push_back(_peek().value());
               _consume();
            } while (std::isalnum(static_cast<unsigned char>(_peek().value())) || _peek().value() == '_');

            if (tokensMap.contains(buffer)) // looking for known keywords/types
               tokens.push_back({tokensMap.at(buffer), startingIndex, buffer.length()});

            else
               tokens.push_back({TokenType::IDENTIFIER, startingIndex, buffer.length()});

            buffer.clear();

         } else if (std::isdigit(static_cast<unsigned char>(_peek().value()))) {
            // INT LITERALS
            do {
               buffer.push_back(_peek().value());
               _consume();
            } while (std::isdigit(static_cast<unsigned char>(_peek().value())));
            tokens.push_back({TokenType::INT_LITERAL, startingIndex, buffer.length()});
            buffer.clear();

         } else {
            switch (_peek().value()) {
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
                        tokens.push_back({TokenType::STRING_LITERAL, startingIndex, 2});
                        _consume(2);
                     } else {
                        _consume();
                        while (_peek().has_value() && _peek().value() != '"' && _peek().value() != '\n') {
                           buffer.push_back(_peek().value());
                           _consume();
                        }
                        if (_peek().has_value() && _peek().value() == '"') {
                           tokens.push_back({TokenType::STRING_LITERAL, startingIndex, buffer.length() + 2});
                           _consume();
                           buffer.clear();
                        }
                     }
                  } else
                     _consume();
                  break;

               default:
                  TokenType lastValidType = TokenType::INVALID;
                  for (size_t i = 0; _peek(i).has_value() && !std::isspace(static_cast<unsigned char>(_peek(i).value())) && _peek(i).value() != '\n' && !std::isalnum(static_cast<unsigned char>(_peek(i).value())) && _peek(i).value() != '_'; ++i) {
                     buffer.push_back(_peek(i).value());
                     if (_determineFixedSizeTokenType(buffer).has_value() && _determineFixedSizeTokenType(buffer).value() != TokenType::INVALID) {
                        lastValidType = _determineFixedSizeTokenType(buffer).value();
                     } else {
                        if (buffer.length() == 1) {
                           break;
                        }
                        buffer.pop_back();
                        break;
                     }
                  }

                  _consume(buffer.length());
                  tokens.push_back({lastValidType, startingIndex, buffer.length()});

                  break;
            }
         }
      }
      tokens.push_back({TokenType::END_OF_FILE});
      return tokens;
   }

   std::optional<TokenType> Lexer::_determineFixedSizeTokenType(std::string_view token) const {
      using enum TokenType;

      if (token.length() > 2 && token[0] != '<' && token[0] != '>')
         return std::nullopt;

      switch (token[0]) {
         case ';':
            return token.length() == 1 ? SEMICOLON : INVALID;
         case '.':
            return token.length() == 1 ? DOT : INVALID;
         case ',':
            return token.length() == 1 ? COMMA : INVALID;
         case ':':
            return token.length() == 1 ? COLON : INVALID;

         case '(':
            return token.length() == 1 ? OPEN_PAREN : INVALID;
         case ')':
            return token.length() == 1 ? CLOSE_PAREN : INVALID;
         case '[':
            return token.length() == 1 ? OPEN_BRACKET : INVALID;
         case ']':
            return token.length() == 1 ? CLOSE_BRACKET : INVALID;
         case '{':
            return token.length() == 1 ? OPEN_BRACE : INVALID;
         case '}':
            return token.length() == 1 ? CLOSE_BRACE : INVALID;
         case '?':
            return token.length() == 1 ? TERNARY : INVALID;

         case '>':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return GREATER_EQUAL;
               if (token[1] == '>') {
                  if (token.size() == 3 && token[2] == '=')
                     return SHIFT_RIGHT_EQUAL;
                  if (token.size() == 2)
                     return SHIFT_RIGHT;
               }
               return INVALID;
            }
            return GREATER;
         case '<':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return LESS_EQUAL;
               if (token[1] == '>') {
                  if (token.size() == 3 && token[2] == '=')
                     return SHIFT_LEFT_EQUAL;
                  if (token.size() == 2)
                     return SHIFT_LEFT;
               }
               return INVALID;
            }
            return LESS;
         case '+':
            if (token.length() > 1) {
               if (token[1] == '+')
                  return PLUS_PLUS;
               if (token[1] == '=')
                  return PLUS_EQUAL;
               return INVALID;
            }
            return PLUS;
         case '-':
            if (token.length() > 1) {
               if (token[1] == '-')
                  return MINUS_MINUS;
               if (token[1] == '=')
                  return MINUS_EQUAL;
               return INVALID;
            }
            return MINUS;
         case '*':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return MULTIPLY_EQUAL;
               return INVALID;
            }
            return MULTIPLY;
         case '/':
            // already handled comments elsewhere
            if (token.length() > 1) {
               if (token[1] == '=')
                  return DIVIDE_EQUAL;
               return INVALID;
            }
            return DIVIDE;
         case '=':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return EQUAL_EQUAL;
               return INVALID;
            }
            return EQUAL;
         case '^':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return XOR_EQUAL;
               return INVALID;
            }
            return BITWISE_XOR;
         case '|':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return OR_EQUAL;
               if (token[1] == '|')
                  return LOGICAL_OR;
               return INVALID;
            }
            return BITWISE_OR;
         case '&':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return AND_EQUAL;
               if (token[1] == '&')
                  return LOGICAL_AND;
               return INVALID;
            }
            return BITWISE_AND;
         case '!':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return NOT_EQUAL;
               return INVALID;
            }
            return LOGICAL_NOT;
         case '%':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return MODULE_EQUAL;
               return INVALID;
            }
            return MODULE_OPERATOR;
         default:
            return std::nullopt;
      }
   }

   std::optional<char> Lexer::_peek(const size_t offset) const {
      if (_index + offset + 1 <= _raw.size())
         return _raw.at(_index + offset);

      return std::nullopt;
   }

   void Lexer::setFileTarget(const fs::path& path) {
      _path = path;
      _index = 0;
   }

} // namespace tungsten
