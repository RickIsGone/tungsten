#include <iostream>
#include <filesystem>
#include <cstdlib>

#include "utils.hpp"
namespace fs = std::filesystem;
import Tungsten.tokenizer;
import Tungsten.parser;

int main(int argc, char** argv) {
   Tokenizer tokenizer;
   Parser parser;
   for (int i = 1; i < argc; ++i) {
      std::cout << "argv[" << i << "] = " << argv[i] << '\n';

      if (fs::exists(argv[i])) {
         std::vector<Token> tokens = tokenizer.tokenize(argv[i]);
         // parser.parse(tokens);
      } else
         utils::error("no such file or directory: '{}'", argv[i]);
   }

   return EXIT_SUCCESS;
}
