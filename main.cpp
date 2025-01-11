#include <filesystem>
#include <cstdlib>
#include <vector>
namespace fs = std::filesystem;
import Tungsten.lexer;
import Tungsten.parser;
import Tungsten.utils;

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

         } else if (fs::is_directory(argv[i]))
            tungsten::utils::pushError("'{}' is a directory", argv[i]);
         else
            tungsten::utils::pushError("no such file: '{}'", argv[i]);
      }
   }
   if (fileProcessed == 0) tungsten::utils::pushError("no input files");
   tungsten::utils::printErrors();

   return EXIT_SUCCESS;
}
