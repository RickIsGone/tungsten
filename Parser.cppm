module;

#include <vector>
#include <unordered_map>
#include <string>
#include <optional>
#include <ostream>
#include <iostream>
#ifndef _NODISCARD
#define _NODISCARD [[nodiscard]]
#endif
export module Tungsten.parser;
import Tungsten.lexer;

namespace tungsten {
   export class Parser {
   public:
      Parser() = default;
      ~Parser() = default;
      Parser(const Parser&) = delete;
      Parser operator=(const Parser&) = delete;

      void parse();
      void setTarget(const std::vector<Token>& tokens);

   private:
      _NODISCARD Token _peek(size_t offset = 0) const;
      void _consume(const size_t amount = 1) { _index += amount; }

      size_t _index{0};
      std::vector<Token> _tokens{};
   };

   //  ========================================== implementation ==========================================

   // will be removed when the llvm backend is implemented
   export inline std::unordered_map<TokenType, std::string> tokenTypeNames = {
       {TokenType::INVALID, "INVALID"},

       {TokenType::RETURN, "RETURN"},
       {TokenType::EXIT, "EXIT"},
       {TokenType::NEW, "NEW"},
       {TokenType::FREE, "FREE"},
       {TokenType::EXTERN, "EXTERN"},

       {TokenType::PLUS, "PLUS"},
       {TokenType::PLUS_EQUAL, "PLUS_EQUAL"},
       {TokenType::PLUS_PLUS, "PLUS_PLUS"},
       {TokenType::MINUS, "MINUS"},
       {TokenType::MINUS_EQUAL, "MINUS_EQUAL"},
       {TokenType::MINUS_MINUS, "MINUS_MINUS"},
       {TokenType::EQUAL, "EQUAL"},
       {TokenType::EQUAL_EQUAL, "EQUAL_EQUAL"},
       {TokenType::DIVIDE, "DIVIDE"},
       {TokenType::DIVIDE_EQUAL, "DIVIDE_EQUAL"},

       {TokenType::INT, "INT"},
       {TokenType::FLOAT, "FLOAT"},
       {TokenType::DOUBLE, "DOUBLE"},
       {TokenType::BOOL, "BOOL"},
       {TokenType::CHAR, "CHAR"},
       {TokenType::STRING, "STRING"},
       {TokenType::VOID, "VOID"},
       {TokenType::UINT, "UINT"},
       {TokenType::UINT8, "UINT8"},
       {TokenType::UINT16, "UINT16"},
       {TokenType::UINT32, "UINT32"},
       {TokenType::UINT64, "UINT64"},
       {TokenType::INT8, "INT8"},
       {TokenType::INT16, "INT16"},
       {TokenType::INT32, "INT32"},
       {TokenType::INT64, "INT64"},

       {TokenType::INT_LITERAL, "INT_LITERAL"},
       {TokenType::STRING_LITERAL, "STRING_LITERAL"},
       {TokenType::IDENTIFIER, "IDENTIFIER"},

       {TokenType::OPEN_PAREN, "OPEN_PAREN"},
       {TokenType::CLOSE_PAREN, "CLOSE_PAREN"},
       {TokenType::OPEN_BRACE, "OPEN_BRACE"},
       {TokenType::CLOSE_BRACE, "CLOSE_BRACE"},
       {TokenType::OPEN_BRACKET, "OPEN_BRACKET"},
       {TokenType::CLOSE_BRACKET, "CLOSE_BRACKET"},

       {TokenType::SEMICOLON, "SEMICOLON"},
       {TokenType::DOT, "DOT"},
       {TokenType::COMMA, "COMMA"},


       {TokenType::END_OF_FILE, "END_OF_FILE"}};

   std::ostream& operator<<(std::ostream& out, const std::vector<Token>& tokens) {
      out << "Tokens: {";

      for (int i = 1; const Token& token : tokens) {
         out << "{" << tokenTypeNames.at(token.type)
             << (token.value.has_value() ? ", " + (token.type == TokenType::STRING_LITERAL ? '"' + token.value.value() + '"' : token.value.value()) : "")
             << (i++ < tokens.size() ? "},\n\t " : "}");
      }
      out << "}\n";
      return out;
   }

   void Parser::setTarget(const std::vector<Token>& tokens) {
      _tokens = tokens;
      _index = 0;
   }

   Token Parser::_peek(const size_t offset) const {
      return _tokens[_index + offset];
   }

   void Parser::parse() {
      std::cout << _tokens << '\n';

      while (_peek().type != TokenType::END_OF_FILE) {
         // TODO
         _consume();
      }
   }

} // namespace tungsten
