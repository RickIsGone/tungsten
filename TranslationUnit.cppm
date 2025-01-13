module;

#include <filesystem>

export module Tungsten.translationUnit;
import Tungsten.lexer;
import Tungsten.parser;
namespace fs = std::filesystem;

export namespace tungsten {
   class TranslationUnit {
   public:
      explicit TranslationUnit(const fs::path& path) : _Lexer{path} {}
      TranslationUnit() = default;
      ~TranslationUnit() = default;
      TranslationUnit(const TranslationUnit&) = delete;
      TranslationUnit operator=(const TranslationUnit&) = delete;

      void compile(const fs::path& path);

   private:
      Lexer _Lexer{};
      Parser _Parser{};
   };

   //  ========================================== implementation ==========================================

   void TranslationUnit::compile(const fs::path& path) {
      _Lexer.setFileTarget(path);
      _Parser.setTarget(_Lexer.tokenize());
      _Parser.parse();
   }


} // namespace tungsten
