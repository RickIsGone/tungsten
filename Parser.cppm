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
       {TokenType::MODULE, "MODULE"},
       {TokenType::EXPORT, "EXPORT"},
       {TokenType::IMPORT, "IMPORT"},

       {TokenType::PLUS, "PLUS"},
       {TokenType::PLUS_EQUAL, "PLUS_EQUAL"},
       {TokenType::PLUS_PLUS, "PLUS_PLUS"},
       {TokenType::MINUS, "MINUS"},
       {TokenType::MINUS_EQUAL, "MINUS_EQUAL"},
       {TokenType::MINUS_MINUS, "MINUS_MINUS"},
       {TokenType::EQUAL, "EQUAL"},
       {TokenType::EQUAL_EQUAL, "EQUAL_EQUAL"},
       {TokenType::MULTIPLY, "MULTIPLY"},
       {TokenType::MULTIPLY_EQUAL, "MULTIPLY_EQUAL"},
       {TokenType::DIVIDE, "DIVIDE"},
       {TokenType::DIVIDE_EQUAL, "DIVIDE_EQUAL"},
       {TokenType::MODULE, "MODULE"},
       {TokenType::MODULE_EQUAL, "MODULE_EQUAL"},
       {TokenType::LOGICAL_AND, "LOGICAL_AND"},
       {TokenType::BITWISE_AND, "BITWISE_AND"},
       {TokenType::AND_EQUAL, "AND_EQUAL"},
       {TokenType::LOGICAL_OR, "LOGICAL_OR"},
       {TokenType::BITWISE_OR, "BITWISE_OR"},
       {TokenType::OR_EQUAL, "OR_EQUAL"},
       {TokenType::BITWISE_XOR, "BITWISE_XOR"},
       {TokenType::XOR_EQUAL, "XOR_EQUAL"},
       {TokenType::LOGICAL_NOT, "LOGICAL_NOT"},
       {TokenType::NOT_EQUAL, "NOT_EQUAL"},

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
       {TokenType::COLON, "COLON"},

       {TokenType::END_OF_FILE, "END_OF_FILE"}};

   std::ostream& operator<<(std::ostream& out, const std::vector<Token>& tokens) {
      out << "Tokens: {";

      for (int i = 1; const Token& token : tokens) {
         out << "{" << tokenTypeNames.at(token.type);
         if (token.type == TokenType::INT_LITERAL || token.type == TokenType::STRING_LITERAL || token.type == TokenType::IDENTIFIER || token.type == TokenType::INVALID) {
            std::string buffer;
            for (size_t j = token.position; j < token.position + token.length; ++j)
               buffer.push_back(Lexer::_fileContents[j]);
            out << ", " << buffer;
         }
         out << (i++ < tokens.size() ? "},\n\t " : "}");
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
