module;

#include <fstream>
#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>

export module Tungsten.tokenizer;
namespace fs = std::filesystem;

export enum class Tokens {
   ENTRY_POINT,

   RETURN,

   PRIMITIVE_TYPE,
   CLASS_TYPE,

   INT_LITERAL,

   SEMICOLON,
};
export struct Token {
   Tokens token;
   std::optional<int> value;
};

export class Tokenizer {
public:
   Tokenizer() = default;
   ~Tokenizer() = default;

   std::vector<Token> tokenize(const fs::path& path);

private:
   std::optional<char> _Peek(size_t offset);
   void _Consume() { ++_Index; }

   size_t _Index{0};
   std::string _FileContents;
};

/*  ========================================== implementation ==========================================  */

std::vector<Token> Tokenizer::tokenize(const fs::path& path) {
   std::ifstream inputFile{path};
   std::stringstream ss;
   std::vector<Token> tokens;
   ss << inputFile.rdbuf();
   _FileContents = ss.str();

   std::cout << _FileContents << '\n';

   return tokens;
}

std::optional<char> Tokenizer::_Peek(size_t offset) {
   if (_FileContents.size() <= _Index + offset)
      return _FileContents[_Index + offset];

   return std::nullopt;
}
