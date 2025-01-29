module;

#include <filesystem>

export module Tungsten.translationUnit;
import Tungsten.lexer;
import Tungsten.parser;
namespace fs = std::filesystem;

export namespace tungsten {
   class TranslationUnit {
   public:
      explicit TranslationUnit(const fs::path& path) : _lexer{path} {}
      TranslationUnit() = default;
      ~TranslationUnit() = default;
      TranslationUnit(const TranslationUnit&) = delete;
      TranslationUnit operator=(const TranslationUnit&) = delete;

      void compile(const fs::path& path);

   private:
      Lexer _lexer{};
      Parser _parser{};
   };

   //  ========================================== implementation ==========================================

   void TranslationUnit::compile(const fs::path& path) {
      _lexer.setFileTarget(path);
      _parser.setTarget(_lexer.tokenize());
      _parser.parse();
   }


} // namespace tungsten
