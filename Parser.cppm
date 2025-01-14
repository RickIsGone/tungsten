module;

#include <vector>
#include <iostream>
#include <unordered_map>
#include <string>
#include <optional>
#include <ostream>
#include <cstdlib>

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
      std::vector<Token> _Tokens;
   };

   //  ========================================== implementation ==========================================

   export inline std::unordered_map<TokenType, std::string> tokenTypeNames = {
       {TokenType::INVALID, "INVALID"},
       {TokenType::KEYWORD, "KEYWORD"},
       {TokenType::OPERATOR, "OPERATOR"},
       {TokenType::PRIMITIVE_TYPE, "PRIMITIVE_TYPE"},
       {TokenType::CLASS_TYPE, "CLASS_TYPE"},
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
         out << "{" << tokenTypeNames.at(token.type) << ", "
             << (token.value.has_value() ? token.value.value() : "std::nullopt")
             << (i++ < tokens.size() ? "}, " : "}");
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

   void generateExit(const int8_t exitCode) {
      std::cout << "   mov rax, 60\n"
                << "   mov rdi, " << static_cast<int>(exitCode) << "\n"
      std::cout << "\n   mov rax, 60\n"
                << (exitCode == 0 ? "   xor rdi, rdi" : "   mov rdi, " + static_cast<int>(exitCode)) << "\n"
                << "   syscall\n";
   }

   // rdi -> argc
   // rsi -> argv
   void generateEntryPoint() {
      std::cout << "section .text\n"
                << "   global _start:\n\n"
                << "_start:\n";
   }

   void Parser::parse() {
      std::cout << _Tokens << '\n';

      while (_Peek().type != TokenType::END_OF_FILE) {
         // TODO
         _Consume();
      }
      // generateExit(EXIT_SUCCESS);
      std::cout << "assembly file:\n"
                << "--------------------------------------\n";
      generateEntryPoint();
      generateExit(EXIT_SUCCESS);
   }

} // namespace tungsten
