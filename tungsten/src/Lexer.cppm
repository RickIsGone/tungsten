module;

#include <fstream>
#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>
#include <sstream>
#ifndef _NODISCARD
#define _NODISCARD [[nodiscard]]
#endif

export module Tungsten.lexer;
import Tungsten.token;
namespace fs = std::filesystem;
namespace tungsten {

   export class Lexer {
   public:
      Lexer(const fs::path& path, std::string& raw) : _path(path), _raw{raw} {}
      Lexer() = delete;
      ~Lexer() = default;
      Lexer(const Lexer&) = delete;
      Lexer operator=(const Lexer&) = delete;

      _NODISCARD std::vector<Token> tokenize();
      _NODISCARD const std::string& raw() const { return _raw; }

   private:
      _NODISCARD std::optional<char> _peek(size_t offset = 0) const;
      _NODISCARD std::optional<char> _peekBack(size_t offset = 0) const;
      _NODISCARD std::optional<TokenType> _determineFixedSizeTokenType(std::string_view token) const;
      void _consume(const size_t amount = 1) { _index += amount; }

      size_t _index{0};
      const fs::path& _path{};
      std::string& _raw;
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
            } while (_peek().has_value() && std::isspace(static_cast<unsigned char>(_peek().value())));

         } else if (std::isalpha(static_cast<unsigned char>(_peek().value())) || _peek().value() == '_' || _peek().value() == '~') {
            // KEYWORDS, TYPES AND IDENTIFIERS
            do {
               buffer.push_back(_peek().value());
               _consume();
            } while (_peek().has_value() && (std::isalnum(static_cast<unsigned char>(_peek().value())) || _peek().value() == '_'));

            if (tokensMap.contains(buffer)) // looking for known keywords/types
               tokens.push_back({tokensMap.at(buffer), startingIndex, buffer.length()});

            else
               tokens.push_back({TokenType::Identifier, startingIndex, buffer.length()});

            buffer.clear();

         } else if (std::isdigit(static_cast<unsigned char>(_peek().value()))) {
            // INT LITERALS
            do {
               buffer.push_back(_peek().value());
               _consume();
            } while (_peek().has_value() && (std::isdigit(static_cast<unsigned char>(_peek().value())) || _peek().value() == '.'));
            tokens.push_back({buffer.find(".") == std::string::npos ? TokenType::IntLiteral : TokenType::FloatLiteral, startingIndex, buffer.length()});
            buffer.clear();

         } else {
            switch (_peek().value()) {
               case '"':
                  // STRING LITERALS
                  _consume();
                  while (_peek().has_value() && _peek().value() != '\n') {
                     if (_peek().value() == '"') {
                        size_t offset = 0;
                        size_t backslashes = 0;
                        while (_peekBack(offset).has_value() && _peekBack(offset).value() == '\\') {
                           ++offset;
                           ++backslashes;
                        }
                        if (backslashes % 2 == 0)
                           break;
                     }
                     buffer.push_back(_peek().value());
                     _consume();
                  }
                  if (_peek().has_value() && _peek().value() == '"') {
                     tokens.push_back({TokenType::StringLiteral, startingIndex, buffer.length() + 2});
                     _consume();
                     buffer.clear();
                  }
                  break;

               case '/':
                  // COMMENTS
                  if (_peek(1).has_value()) {
                     if (_peek(1).value() == '/') {
                        while (_peek().has_value() && _peek().value() != '\n') {
                           _consume();
                        }
                        break;
                     }
                     if (_peek(1).value() == '*') {
                        while (_peek().has_value()) {
                           if (_peek().value() == '*' && _peek(1).has_value() && _peek(1).value() == '/') {
                              _consume(2);
                              break;
                           }
                           _consume();
                        }
                        break;
                     }
                  }
                  // no break because the remaining tokens with `/` are operators
                  [[fallthrough]];
               default:
                  TokenType lastValidType = TokenType::Invalid;
                  for (size_t i = 0; _peek(i).has_value() && !std::isspace(static_cast<unsigned char>(_peek(i).value())) && _peek(i).value() != '\n' && !std::isalnum(static_cast<unsigned char>(_peek(i).value())) && _peek(i).value() != '_'; ++i) {
                     buffer.push_back(_peek(i).value());
                     if (_determineFixedSizeTokenType(buffer).has_value() && _determineFixedSizeTokenType(buffer).value() != TokenType::Invalid)
                        lastValidType = _determineFixedSizeTokenType(buffer).value();
                     else {
                        if (buffer.length() == 1)
                           break;

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
      tokens.push_back({TokenType::EndOFFile});
      return tokens;
   }

   std::optional<TokenType> Lexer::_determineFixedSizeTokenType(std::string_view token) const {
      using enum TokenType;

      if (token.length() > 2 && token[0] != '<' && token[0] != '>')
         return std::nullopt;

      switch (token[0]) {
         case ';':
            return token.length() == 1 ? Semicolon : Invalid;
         case '.':
            return token.length() == 1 ? Dot : Invalid;
         case ',':
            return token.length() == 1 ? Comma : Invalid;
         case ':':
            return token.length() == 1 ? Colon : Invalid;

         case '(':
            return token.length() == 1 ? OpenParen : Invalid;
         case ')':
            return token.length() == 1 ? CloseParen : Invalid;
         case '[':
            return token.length() == 1 ? OpenBracket : Invalid;
         case ']':
            return token.length() == 1 ? CloseBracket : Invalid;
         case '{':
            return token.length() == 1 ? OpenBrace : Invalid;
         case '}':
            return token.length() == 1 ? CloseBrace : Invalid;
         case '?':
            return token.length() == 1 ? Ternary : Invalid;

         case '>':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return GreaterEqual;
               if (token[1] == '>') {
                  if (token.size() == 3 && token[2] == '=')
                     return ShiftRightEqual;
                  if (token.size() == 2)
                     return ShiftRight;
               }
               return Invalid;
            }
            return Greater;
         case '<':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return LessEqual;
               if (token[1] == '<') {
                  if (token.size() == 3 && token[2] == '=')
                     return ShiftLeftEqual;
                  if (token.size() == 2)
                     return ShiftLeft;
               }
               return Invalid;
            }
            return Less;
         case '+':
            if (token.length() > 1) {
               if (token[1] == '+')
                  return PlusPlus;
               if (token[1] == '=')
                  return PlusEqual;
               return Invalid;
            }
            return Plus;
         case '-':
            if (token.length() > 1) {
               if (token[1] == '-')
                  return MinusMinus;
               if (token[1] == '=')
                  return MinusEqual;
               if (token[1] == '>')
                  return Arrow;
               return Invalid;
            }
            return Minus;
         case '*':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return MultiplyEqual;
               return Invalid;
            }
            return Multiply;
         case '/':
            // already handled comments elsewhere
            if (token.length() > 1) {
               if (token[1] == '=')
                  return DivideEqual;
               return Invalid;
            }
            return Divide;
         case '=':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return EqualEqual;
               return Invalid;
            }
            return Equal;
         case '^':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return XorEqual;
               return Invalid;
            }
            return BitwiseXor;
         case '|':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return OrEqual;
               if (token[1] == '|')
                  return LogicalOr;
               return Invalid;
            }
            return BitwiseOr;
         case '&':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return AndEqual;
               if (token[1] == '&')
                  return LogicalAnd;
               return Invalid;
            }
            return BitwiseAnd;
         case '!':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return NotEqual;
               return Invalid;
            }
            return LogicalNot;
         case '%':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return ModuleEqual;
               return Invalid;
            }
            return ModuleOperator;
         default:
            return std::nullopt;
      }
   }

   std::optional<char> Lexer::_peek(const size_t offset) const {
      if (_index + offset <= _raw.size() - 1)
         return _raw.at(_index + offset);

      return std::nullopt;
   }

   std::optional<char> Lexer::_peekBack(const size_t offset) const {
      if (_index < offset + 1)
         return std::nullopt;
      return _raw.at(_index - offset - 1);
   }


} // namespace tungsten
