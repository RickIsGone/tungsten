#include <cstdlib>
#include <filesystem>
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

   for (const auto& file : options.files) {
      tungsten::TranslationUnit tu{};
      tu.compile(file);
   }

   return EXIT_SUCCESS;
}
