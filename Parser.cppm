module;

#include <vector>
#include <iostream>

export module Tungsten.parser;
import Tungsten.lexer;

export namespace tungsten {
   class Parser {
   public:
      void parse(const std::vector<Token>& tokens);
   };

   //  ========================================== implementation ==========================================

   void Parser::parse(const std::vector<Token>& tokens) {
      std::cout << tokens << '\n';
   }

} // namespace tungsten
