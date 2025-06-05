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
      MODULE = -7,
      EXPORT = -8,
      IMPORT = -9,

      // operators
      PLUS = -10,
      PLUS_EQUAL = -11,
      PLUS_PLUS = -12,
      MINUS = -13,
      MINUS_EQUAL = -14,
      MINUS_MINUS = -15,
      EQUAL = -16,
      EQUAL_EQUAL = -17,
      MULTIPLY = -18,
      MULTIPLY_EQUAL = -19,
      DIVIDE = -20,
      DIVIDE_EQUAL = -21,
      MODULE_OPERATOR = -22,
      MODULE_EQUAL = -23,
      LOGICAL_AND = -24,
      BITWISE_AND = -25,
      AND_EQUAL = -26,
      LOGICAL_OR = -27,
      BITWISE_OR = -28,
      OR_EQUAL = -29,
      BITWISE_XOR = -30,
      XOR_EQUAL = -31,
      LOGICAL_NOT = -32,
      NOT_EQUAL = -33,

      // types
      INT = -34,
      FLOAT = -35,
      DOUBLE = -36,
      BOOL = -37,
      CHAR = -38,
      STRING = -39,
      VOID = -40,
      UINT = -41,
      UINT8 = -42,
      UINT16 = -43,
      UINT32 = -44,
      UINT64 = -45,
      INT8 = -46,
      INT16 = -47,
      INT32 = -48,
      INT64 = -49,

      // literals
      INT_LITERAL = -50,
      STRING_LITERAL = -51,
      IDENTIFIER = -52,

      OPEN_PAREN = -53,
      CLOSE_PAREN = -54,
      OPEN_BRACE = -55,
      CLOSE_BRACE = -56,
      OPEN_BRACKET = -57,
      CLOSE_BRACKET = -58,

      SEMICOLON = -59,
      DOT = -60,
      COMMA = -61,
      COLON = -62,

      END_OF_FILE = -63
   };
   export inline std::unordered_map<std::string, TokenType> tokensMap = {
       // keywords
       {"return", TokenType::RETURN},
       {"exit", TokenType::EXIT},
       {"new", TokenType::NEW},
       {"free", TokenType::FREE},
       {"extern", TokenType::EXTERN},
       {"module", TokenType::MODULE},
       {"export", TokenType::EXPORT},
       {"import", TokenType::IMPORT},

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
      size_t position{};
      size_t length{1};
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
      static std::string _fileContents;

   private:
      _NODISCARD std::optional<char> _peek(size_t offset = 0) const;
      _NODISCARD std::optional<TokenType> _determineFixedSizeTokenType(std::string_view token) const;
      void _consume(const size_t amount = 1) { _index += amount; }

      size_t _index{0};
      fs::path _path{};
   };

   /*  ========================================== implementation ==========================================  */
   std::string Lexer::_fileContents{};
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
      Lexer::_fileContents = ss.str();

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
                  for (size_t i = 0; _peek(i).has_value() && !std::isspace(static_cast<unsigned char>(_peek(i).value())) && _peek(i).value() != '\n' && !std::isalnum(static_cast<unsigned char>(_peek(i).value())); ++i) {
                     buffer.push_back(_peek(i).value());
                     if (_determineFixedSizeTokenType(buffer).has_value() && _determineFixedSizeTokenType(buffer).value() != TokenType::INVALID) {
                        lastValidType = _determineFixedSizeTokenType(buffer).value();
                     } else {
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

      if (token.length() > 2)
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

         case '+':
            if (token.length() > 1) {
               if (token[1] == '+')
                  return PLUS_PLUS;
               if (token[1] == '=')
                  return PLUS_EQUAL;
            }
            return PLUS;
         case '-':
            if (token.length() > 1) {
               if (token[1] == '-')
                  return MINUS_MINUS;
               if (token[1] == '=')
                  return MINUS_EQUAL;
            }
            return MINUS;
         case '*':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return MULTIPLY_EQUAL;
            }
            return MULTIPLY;
         case '/':
            // already handled comments elsewhere
            if (token.length() > 1)
               if (token[1] == '=')
                  return DIVIDE_EQUAL;
            return DIVIDE;
         case '=':
            if (token.length() > 1)
               if (token[1] == '=')
                  return EQUAL_EQUAL;
            return EQUAL;
         case '^':
            if (token.length() > 1)
               if (token[1] == '=')
                  return XOR_EQUAL;
            return BITWISE_XOR;
         case '|':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return OR_EQUAL;
               if (token[1] == '|')
                  return LOGICAL_OR;
            }
            return BITWISE_OR;
         case '&':
            if (token.length() > 1) {
               if (token[1] == '=')
                  return AND_EQUAL;
               if (token[1] == '&')
                  return LOGICAL_AND;
            }
            return BITWISE_AND;
         case '!':
            if (token.length() > 1)
               if (token[1] == '=')
                  return NOT_EQUAL;
            return LOGICAL_NOT;

         default:
            return std::nullopt;
      }
   }

   std::optional<char> Lexer::_peek(const size_t offset) const {
      if (_index + offset + 1 <= Lexer::_fileContents.size())
         return Lexer::_fileContents.at(_index + offset);

      return std::nullopt;
   }

   void Lexer::setFileTarget(const fs::path& path) {
      _path = path;
      _index = 0;
   }

} // namespace tungsten
