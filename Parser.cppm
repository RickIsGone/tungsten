module;

#include <vector>
#include <optional>
#include <ostream>
#include <iostream>
#ifndef _NODISCARD
#define _NODISCARD [[nodiscard]]
#endif

export module Tungsten.parser;
import Tungsten.token;
import Tungsten.ast;

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
