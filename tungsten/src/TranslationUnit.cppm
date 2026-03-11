module;

#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#endif
export module Tungsten.translationUnit;
import Tungsten.lexer;
import Tungsten.parser;
import Tungsten.semanticAnalyzer;
import Tungsten.compileOptions;
namespace fs = std::filesystem;

namespace tungsten {
   std::string getExecutablePath() {
#if defined(_WIN32)
      char path[MAX_PATH];
      DWORD size = GetModuleFileNameA(NULL, path, MAX_PATH);
      if (size == 0) return "";
      return std::string(path);
#elif defined(__linux__)
      char path[PATH_MAX];
      ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
      if (len == -1) return "";
      path[len] = '\0';
      return std::string(path);
#elif defined(__APPLE__)
      char path[PATH_MAX];
      uint32_t size = sizeof(path);
      if (_NSGetExecutablePath(path, &size) != 0) return "";
      return std::string(path);
#else
      return "";
#endif
   }
   export class TranslationUnit {
   public:
      explicit TranslationUnit(const CompileOptions& options) : _compileOptions{options} {}
      TranslationUnit() = default;
      ~TranslationUnit() = default;
      TranslationUnit(const TranslationUnit&) = delete;
      TranslationUnit& operator=(const TranslationUnit&) = delete;

      void compile(const fs::path& path);

   private:
      CompileOptions _compileOptions{};
      std::string _raw{};
      fs::path _filePath{};
   };

   //  ========================================== implementation ==========================================

   void TranslationUnit::compile(const fs::path& path) {
      Lexer lexer{path, _raw};
      Parser parser{path, lexer.tokenize(), _raw};
      parser.parse();
      SemanticAnalyzer analyzer{parser.functions(), parser.classes(), parser.globalVariables(), parser.externs(), path, _raw};
      if (analyzer.analyze()) {
         for (auto& var : parser.externs()->variables) {
            var->codegen();
         }
         for (auto& fun : parser.externs()->functions) {
            fun->codegen();
         }
         for (auto& fun : parser.functions()) {
            fun->prototype()->codegen(); // forward declaration
         }
         for (auto& cls : parser.classes()) {
            cls->codegen();
         }
         for (auto& var : parser.globalVariables()) {
            var->codegen();
         }
         for (auto& fun : parser.functions()) {
            fun->codegen();
         }
         parser.writeIR();

         fs::path currentDir = fs::path(getExecutablePath()).parent_path();
         fs::path parentDir = currentDir.parent_path();
         fs::path libDir = parentDir / "lib";
#ifdef WIN32
         std::string libs = libDir.string() + "/core.lib" + " " + libDir.string() + "/stdlib.lib";
         system(("clang++ " + path.stem().string() + ".ll " + libs + " -o " + path.stem().string() + ".exe -O3").c_str());
#else
         std::string libs = libDir.string() + "/libcore.a" + " " + libDir.string() + "/libstdlib.a";
         system(("clang++ " + path.stem().string() + ".ll " + libs + " -o " + path.stem().string() + " -O3").c_str());
#endif
         fs::remove(path.stem().string() + ".ll");
      }
   }

} // namespace tungsten
