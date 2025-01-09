#include <filesystem>
#include <cstdlib>

#include "utils.hpp"
namespace fs = std::filesystem;
import Tungsten.lexer;
import Tungsten.parser;

int main(int argc, char** argv) {
   size_t fileProcessed{0};
   if (argc > 1) {
      tungsten::Lexer lexer;
      tungsten::Parser parser;
      for (int i = 1; i < argc; ++i) {
         if (fs::exists(argv[i]) && !fs::is_directory(argv[i])) {
               lexer.setFileTarget(argv[i]);
               std::vector<tungsten::Token> tokens = lexer.tokenize();
               parser.parse(tokens);
               ++fileProcessed;

         } else
            if (fs::is_directory(argv[i]))
               utils::pushError("'{}' is a directory", argv[i]);
            else
               utils::pushError("no such file: '{}'", argv[i]);
      }
   }
   if (fileProcessed == 0) utils::pushError("no input files");
   utils::printErrors();

   return EXIT_SUCCESS;
}
