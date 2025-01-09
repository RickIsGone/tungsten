module;

#include <vector>
#include <iostream>

export module Tungsten.parser;
import Tungsten.lexer;

namespace tungsten {
   export class Parser {
   public:
      void parse(const std::vector<Token>& tokens);
   };

   //  ========================================== implementation ==========================================

   std::ostream& operator<<(std::ostream& out, const std::vector<Token>& tokens) {
      out << "Tokens: {";
      for (int i = 1; const Token& token : tokens) {
         out << "{" << tokenTypeNames.at(token.type) << ", "
             << (token.value.has_value() ? token.value.value() : "std::nullopt")
             << (i < tokens.size() ? "}, " : "}");
         ++i;
      }
      out << "}\n";
      return out;
   }

   void Parser::parse(const std::vector<Token>& tokens) {
      std::cout << tokens << '\n';
   }

} // namespace tungsten
