module;

#include <filesystem>
#include <iostream>

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
      SemanticAnalyzer analyzer{parser.functions(), parser.classes(), parser.globalVariables(), parser.externs()};
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
         std::cout << "passed semantic analysis\n";
      }
   }

} // namespace tungsten
