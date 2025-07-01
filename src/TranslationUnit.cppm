module;

#include <filesystem>

export module Tungsten.translationUnit;
import Tungsten.lexer;
import Tungsten.parser;
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
      Lexer _lexer{path, _raw};
      Parser _parser{path, _lexer.tokenize(), _raw};
      _parser.parse();
   }

} // namespace tungsten
