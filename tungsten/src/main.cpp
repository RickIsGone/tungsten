#include <cstdlib>
#include <filesystem>
#include <memory>
import Tungsten.translationUnit;
import Tungsten.compileOptions;
import Tungsten.utils;

int main(int argc, char** argv) {
   const tungsten::CompileOptions options = tungsten::parseArguments(argc, argv);

   if (tungsten::utils::hasErrors()) {
      tungsten::utils::printErrors();
      return EXIT_FAILURE;
   }
   if (options.newProject) {
      tungsten::utils::createProject(options.files[0].string());
      return EXIT_SUCCESS;
   }
   if (options.buildSystem) {
      tungsten::utils::pushError("build-tgs is not implemented yet");
      tungsten::utils::printErrors();
      return EXIT_FAILURE;
   }

   std::unique_ptr<tungsten::TranslationUnit> tu = std::make_unique<tungsten::TranslationUnit>(options);
   tu->compile(options.files[0]); // temporary for testing

   return EXIT_SUCCESS;
}
