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
      _NODISCARD Token _Peek(size_t offset = 0) const;
      void _Consume(const size_t amount = 1) { _Index += amount; }

      size_t _Index{0};
      std::vector<Token> _Tokens{};
   };

   //  ========================================== implementation ==========================================

   export inline std::unordered_map<TokenType, std::string> tokenTypeNames = {
       {TokenType::INVALID, "INVALID"},
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
       {TokenType::INT64, "INT64"},

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

       {TokenType::END_OF_FILE, "END_OF_FILE"}};

   std::ostream& operator<<(std::ostream& out, const std::vector<Token>& tokens) {
      out << "Tokens: {";

      for (int i = 1; const Token& token : tokens) {
         out << "{" << tokenTypeNames.at(token.type)
             << (token.value.has_value() ? ", " + token.value.value() : "")
             << (i++ < tokens.size() ? "},\n\t " : "}");
      }
      out << "}\n";
      return out;
   }

   void Parser::setTarget(const std::vector<Token>& tokens) {
      _Tokens = tokens;
      _Index = 0;
   }

   Token Parser::_Peek(const size_t offset) const {
      return _Tokens[_Index + offset];
   }

   void Parser::parse() {
      std::cout << _Tokens << '\n';

      while (_Peek().type != TokenType::END_OF_FILE) {
         // TODO
         _Consume();
      }
   }

} // namespace tungsten
