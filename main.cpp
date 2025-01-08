#include <iostream>
#include <filesystem>
#include <cstdlib>

#include "utils.hpp"
namespace fs = std::filesystem;
import Tungsten.lexer;
import Tungsten.parser;

int main(int argc, char** argv) {
   if (argc < 2) utils::error("no input files");

   tungsten::Lexer lexer;
   tungsten::Parser parser;
   for (int i = 1; i < argc; ++i) {
      std::cout << "argv[" << i << "] = " << argv[i] << '\n';
      if (fs::exists(argv[i])) {
         lexer.setTargetFile(argv[i]);
         std::vector<tungsten::Token> tokens = lexer.tokenize();
         // parser.parse(tokens);
      } else
         utils::error("no such file or directory: '{}'", argv[i]);
   }

   return EXIT_SUCCESS;
}
