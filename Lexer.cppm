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
      std::optional<char> _Peek(size_t offset);
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
      std::stringstream ss;
      std::vector<Token> tokens{{TokenType::RETURN}, {TokenType::INT_LITERAL, 0}};
      ss << inputFile.rdbuf();
      _FileContents = ss.str();

      std::cout << _FileContents << '\n';
      std::cout << tokens << '\n';
      return tokens;
   }

   std::optional<char> Lexer::_Peek(size_t offset) {
      if (_FileContents.size() <= _Index + offset)
         return _FileContents[_Index + offset];

      return std::nullopt;
   }

   void Lexer::setTargetFile(const fs::path& path) {
      _Path = path;
      _Index = 0;
   }
} // namespace tungsten
