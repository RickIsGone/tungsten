module;

#include <vector>
#include <iostream>
#include <unordered_map>
#include <string>
#include <optional>
#include <ostream>

export module Tungsten.parser;
import Tungsten.lexer;

namespace tungsten {
   export class Parser {
   public:
      void parse(const std::vector<Token>& tokens);
   };

   //  ========================================== implementation ==========================================

   inline std::unordered_map<TokenType, std::string> tokenTypeNames = {
       {TokenType::INVALID, "INVALID"},
       {TokenType::ENTRY_POINT, "ENTRY_POINT"},
       {TokenType::KEYWORD, "KEYWORD"},
       {TokenType::OPERATOR, "OPERATOR"},
       {TokenType::PRIMITIVE_TYPE, "PRIMITIVE_TYPE"},
       {TokenType::CLASS_TYPE, "CLASS_TYPE"},
       {TokenType::INT_LITERAL, "INT_LITERAL"},
       {TokenType::STRING_LITERAL, "STRING_LITERAL"},
       {TokenType::SEMICOLON, "SEMICOLON"},
   };

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
   void Parser::parse(const std::vector<Token>& tokens) {
      std::cout << tokens << '\n';
   }

} // namespace tungsten
