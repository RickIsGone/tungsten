module;

#include <fstream>
#include <filesystem>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <vector>

export module Tungsten.lexer;
namespace fs = std::filesystem;

export namespace tungsten {

   enum class TokenType {
      INVALID,

      ENTRY_POINT,

      KEYWORD,

      PRIMITIVE_TYPE,
      CLASS_TYPE,

      INT_LITERAL,

      COMMENT,

      SEMICOLON,
   };

   std::unordered_map<TokenType, std::string> tokenTypeNames = {
       {TokenType::INVALID, "INVALID"},
       {TokenType::ENTRY_POINT, "ENTRY_POINT"},
       {TokenType::KEYWORD, "KEYWORD"},
       {TokenType::PRIMITIVE_TYPE, "PRIMITIVE_TYPE"},
       {TokenType::CLASS_TYPE, "CLASS_TYPE"},
       {TokenType::INT_LITERAL, "INT_LITERAL"},
       {TokenType::COMMENT, "COMMENT"},
       {TokenType::SEMICOLON, "SEMICOLON"},
   };

   struct Token {
      TokenType type{TokenType::INVALID};
      std::optional<std::string> value{};
   };

   class Lexer {
   public:
      Lexer() = default;
      ~Lexer() = default;

      std::vector<Token> tokenize();
      void setFileTarget(const fs::path& path);

   private:
      std::optional<char> _Peek(size_t offset = 1);
      void _Consume() { ++_Index; }

      size_t _Index{0};
      fs::path _Path;
      std::string _FileContents;
   };

   /*  ========================================== implementation ==========================================  */


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

   std::vector<Token> Lexer::tokenize() {
      std::ifstream inputFile{_Path};
      std::stringstream ss{};
      std::vector<Token> tokens{};
      ss << inputFile.rdbuf();
      _FileContents = ss.str();

      std::cout << _FileContents << '\n';
      while (_Peek().has_value()) {
         std::string buffer;
         if (std::isspace(_Peek().value())) {
           do {
               _Consume();
              if (!_Peek().has_value()) break;
           } while(std::isspace(_Peek().value()));

         } else if (std::isalpha(_Peek().value())) {
            do {
               buffer.push_back(_Peek().value());
               _Consume();
            } while (std::isalpha(_Peek().value()));
            if (buffer == "return") tokens.push_back(Token{TokenType::KEYWORD, buffer});
            else {
                 tokens.push_back(Token{TokenType::INVALID, buffer});
               }
            buffer.clear();

         } else if (std::isdigit(_Peek().value())) {
            do {
               buffer.push_back(_Peek().value());
               _Consume();
            } while (std::isdigit(_Peek().value()));
            tokens.push_back(Token{TokenType::INT_LITERAL,buffer});
            buffer.clear();

         } else if (_Peek().value() == ';') {
            _Consume();
            tokens.push_back(Token{TokenType::SEMICOLON});
         } else {
            if (_Peek().value() == '/' && _Peek(2).has_value()) {
               if ( _Peek(2).value() == '/')
                  while (_Peek().has_value() && _Peek().value() != '\n')
                     _Consume();
               else if (_Peek(2).value() == '*') {
                  while (_Peek().has_value()) {
                     if (_Peek().value() == '*' && _Peek(2).has_value() && _Peek(2).value() == '/') {
                        _Consume();
                        _Consume();
                        break;
                     }
                     _Consume();
                  }
               }
            } else
               _Consume();
         }

      }

      return tokens;
   }

   std::optional<char> Lexer::_Peek(size_t offset) {
      if (_Index + offset <= _FileContents.size())
         return _FileContents.at(_Index + offset -1);

      return std::nullopt;
   }

   void Lexer::setFileTarget(const fs::path& path) {
      _Path = path;
      _Index = 0;
   }
} // namespace tungsten
