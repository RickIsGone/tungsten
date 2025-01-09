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

      RETURN,
      EXIT,

      PRIMITIVE_TYPE,
      CLASS_TYPE,

      INT_LITERAL,

      COMMENT,

      SEMICOLON,
   };

   std::unordered_map<TokenType, std::string> tokenTypeNames = {
       {TokenType::INVALID, "INVALID"},
       {TokenType::ENTRY_POINT, "ENTRY_POINT"},
       {TokenType::RETURN, "RETURN"},
       {TokenType::EXIT, "EXIT"},
       {TokenType::PRIMITIVE_TYPE, "PRIMITIVE_TYPE"},
       {TokenType::CLASS_TYPE, "CLASS_TYPE"},
       {TokenType::INT_LITERAL, "INT_LITERAL"},
       {TokenType::COMMENT, "COMMENT"},
       {TokenType::SEMICOLON, "SEMICOLON"},
   };

   struct Token {
      TokenType type;
      std::optional<int> value;
   };

   class Lexer {
   public:
      Lexer() = default;
      ~Lexer() = default;

      std::vector<Token> tokenize();
      void setTargetFile(const fs::path& path);

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
         out << "{"
             << tokenTypeNames.at(token.type)
             << ", "
             << (token.value.has_value() ? std::to_string(token.value.value()) : "std::nullopt")
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
           } while(std::isspace(_Peek().value()));

         } else if (std::isalpha(_Peek().value())) {
            do {
               buffer.push_back(_Peek().value());
               _Consume();
            } while (std::isalpha(_Peek().value()));
            if (buffer == "return") tokens.push_back({TokenType::RETURN});
            else {
                 std::cout << "invalid token: [" << buffer << "]\n";
                 tokens.push_back({TokenType::INVALID});
               }
            buffer.clear();

         } else if (std::isdigit(_Peek().value())) {
            do {
               buffer.push_back(_Peek().value());
               _Consume();
            } while (std::isdigit(_Peek().value()));
            tokens.push_back({TokenType::INT_LITERAL, std::stoi(buffer)});
            buffer.clear();

         } else if (_Peek().value() == ';') {
            _Consume();
            tokens.push_back({TokenType::SEMICOLON});
         }
      }

      return tokens;
   }

   std::optional<char> Lexer::_Peek(size_t offset) {
      if (_Index + offset < _FileContents.size())
         return _FileContents[_Index + offset];

      return std::nullopt;
   }

   void Lexer::setTargetFile(const fs::path& path) {
      _Path = path;
      _Index = 0;
   }
} // namespace tungsten
