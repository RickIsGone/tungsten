module;

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <llvm/IR/DIBuilder.h>

export module Tungsten.debugInfo;

export namespace tungsten {
   std::unique_ptr<llvm::DIBuilder> DBuilder;

   struct DebugInfo {
      llvm::DICompileUnit* TheCU{nullptr};
      llvm::DIFile* TheFile{nullptr};
      llvm::DISubprogram* CurrentSubprogram{nullptr};

      static llvm::DIType* boolTy() { return DBuilder->createBasicType("bool", 1, llvm::dwarf::DW_ATE_boolean); }
      static llvm::DIType* int8Ty() { return DBuilder->createBasicType("i8", 8, llvm::dwarf::DW_ATE_signed); }
      static llvm::DIType* uint8Ty() { return DBuilder->createBasicType("u8", 8, llvm::dwarf::DW_ATE_unsigned); }
      static llvm::DIType* int16Ty() { return DBuilder->createBasicType("i16", 16, llvm::dwarf::DW_ATE_signed); }
      static llvm::DIType* uint16Ty() { return DBuilder->createBasicType("u16", 16, llvm::dwarf::DW_ATE_unsigned); }
      static llvm::DIType* int32Ty() { return DBuilder->createBasicType("i32", 32, llvm::dwarf::DW_ATE_signed); }
      static llvm::DIType* uint32Ty() { return DBuilder->createBasicType("u32", 32, llvm::dwarf::DW_ATE_unsigned); }
      static llvm::DIType* int64Ty() { return DBuilder->createBasicType("i64", 64, llvm::dwarf::DW_ATE_signed); }
      static llvm::DIType* uint64Ty() { return DBuilder->createBasicType("u64", 64, llvm::dwarf::DW_ATE_unsigned); }
      static llvm::DIType* int128Ty() { return DBuilder->createBasicType("i128", 128, llvm::dwarf::DW_ATE_signed); }
      static llvm::DIType* uint128Ty() { return DBuilder->createBasicType("u128", 128, llvm::dwarf::DW_ATE_unsigned); }
      static llvm::DIType* floatTy() { return DBuilder->createBasicType("float", 32, llvm::dwarf::DW_ATE_float); }
      static llvm::DIType* doubleTy() { return DBuilder->createBasicType("double", 64, llvm::dwarf::DW_ATE_float); }
      static llvm::DIType* charTy() { return DBuilder->createBasicType("char", 8, llvm::dwarf::DW_ATE_signed); }
      static llvm::DIType* pointerTy(llvm::DIType* pointee, uint64_t sizeBits, uint32_t alignBits) {
         return DBuilder->createPointerType(pointee, sizeBits, alignBits);
      }
      static llvm::DIType* arrayTy(llvm::DIType* elemTy, uint64_t count, uint64_t sizeBits, uint32_t alignBits) {
         auto* subrange = DBuilder->getOrCreateSubrange(0, static_cast<int64_t>(count));
         auto subs = DBuilder->getOrCreateArray({subrange});
         return DBuilder->createArrayType(sizeBits, alignBits, elemTy, subs);
      }

      llvm::DIScope* currentScope() const {
         if (!LexicalBlocks.empty() && LexicalBlocks.back())
            return LexicalBlocks.back();
         if (CurrentSubprogram)
            return CurrentSubprogram;
         if (TheFile)
            return TheFile;
         return TheCU;
      }

      void pushLexicalBlock(uint32_t line, uint32_t column) {
         llvm::DIScope* parent = currentScope();
         if (!parent || !TheFile)
            return;
         LexicalBlocks.push_back(DBuilder->createLexicalBlock(parent, TheFile, line, column));
      }

      void popLexicalBlock() {
         if (!LexicalBlocks.empty())
            LexicalBlocks.pop_back();
      }

      void pushVariableScope() {
         LocalVariableScopes.emplace_back();
      }

      void popVariableScope() {
         if (!LocalVariableScopes.empty())
            LocalVariableScopes.pop_back();
      }

      void registerLocalVariable(const std::string& name, llvm::DILocalVariable* variable) {
         if (name.empty() || !variable)
            return;
         if (LocalVariableScopes.empty())
            pushVariableScope();
         LocalVariableScopes.back()[name] = variable;
      }

      llvm::DILocalVariable* lookupLocalVariable(const std::string& name) const {
         for (auto it = LocalVariableScopes.rbegin(); it != LocalVariableScopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end())
               return found->second;
         }
         return nullptr;
      }

      std::vector<llvm::DIScope*> LexicalBlocks;
      std::vector<std::unordered_map<std::string, llvm::DILocalVariable*>> LocalVariableScopes;
   } KSDbgInfo;
   //  ========================================== implementation ==========================================

} // namespace tungsten