module;

#include <filesystem>

export module Tungsten.translationUnit;
import Tungsten.lexer;
import Tungsten.parser;
import Tungsten.semanticAnalyzer;
namespace fs = std::filesystem;

export namespace tungsten {
   class TranslationUnit {
   public:
      explicit TranslationUnit(const fs::path& path) : _filePath{path} {}
      TranslationUnit() = default;
      ~TranslationUnit() = default;
      TranslationUnit(const TranslationUnit&) = delete;
      TranslationUnit operator=(const TranslationUnit&) = delete;

      void compile(const fs::path& path);

   private:
      std::string _raw{};
      fs::path _filePath{};
   };

   //  ========================================== implementation ==========================================

   void TranslationUnit::compile(const fs::path& path) {
      Lexer lexer{path, _raw};
      Parser parser{path, lexer.tokenize(), _raw};
      parser.parse();
      SemanticAnalyzer analyzer{parser.functions(), parser.classes(), parser.globalVariables()};
      if (analyzer.analyze()) {
         // codegen();
      }
   }

} // namespace tungsten
