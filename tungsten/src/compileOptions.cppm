module;

#include <vector>
#include <string>
#include <string_view>
#include <filesystem>
#include <iostream>
#include <unordered_set>

export module Tungsten.compileOptions;
import Tungsten.utils;

using namespace std::string_view_literals;
namespace fs = std::filesystem;

namespace tungsten {
   export enum class OutputKind {
      Executable,
      Object,
      Assembly,
      LLVMIR
   };

   const std::unordered_set validFlags{
       "--help"sv, "-h"sv,
       "-o"sv,
       "-O1"sv, "-O2"sv, "-O3"sv,
       "-C"sv,
       "-S"sv,
       "-emit-llvm"sv};

   export struct CompileOptions {
      std::vector<fs::path> files;
      std::vector<std::string> flags;
      bool newProject{false};
      bool buildSystem{false};
      OutputKind outputKind{OutputKind::Executable};
      int optimizationLevel{0};
      std::string outputFile{"a.out"};
   };

   void checkBuildSystemOptions(const CompileOptions& options) {
      if (options.buildSystem && !options.files.empty())
         utils::pushError("build-tgs does not take any input files");
   }

   void checkInputFiles(const CompileOptions& options) {
      if (!options.buildSystem && !options.newProject) {
         if (options.files.empty())
            utils::pushError("no input files");


         for (const auto& file : options.files) {
            if (!fs::exists(file))
               utils::pushError("no such file: '{}'", file.string());
            else if (fs::is_directory(file))
               utils::pushError("'{}' is a directory", file.string());
         }
      }
   }

   void showHelpAndExit() {
      std::cout << "Usage: tungsten [options] <files>\n"
                   "Options:\n"
                   "  -h, --help             Show this help message\n"
                   "  new <project_name>     Create a new project\n"
                   "  build-tgs [options]    Build a build.tgs file\n\n"
                   "Available flags:\n"
                   "  -o <name>              Set the output file name\n"
                   "  -O<1,2,3>              Enable optimizations\n"
                   "  -C                     Emit object files\n"
                   "  -S                     Emit assembly files\n"
                   "  -emit-llvm             Emit LLVM IR\n";
      exit(EXIT_SUCCESS);
   }

   void checkHelpFlag(const CompileOptions& options) {
      for (const auto& flag : options.flags) {
         if (flag == "--help"sv || flag == "-h"sv) {
            if (options.files.empty())
               showHelpAndExit();
            else
               utils::pushError("the --help flag cannot be used with input files");
         }
      }
   }

   void checkOutputFlags(CompileOptions& options) {
      int count = 0;
      for (const auto& flag : options.flags) {
         if (flag == "-C"sv || flag == "-S"sv || flag == "-emit-llvm"sv) {
            if (flag == "-C"sv)
               options.outputKind = OutputKind::Object;
            else if (flag == "-S"sv)
               options.outputKind = OutputKind::Assembly;
            else
               options.outputKind = OutputKind::LLVMIR;
            ++count;
         }
      }
      if (count > 1)
         utils::pushError("only one of -C, -S, or -emit-llvm can be used at a time");
   }

   void checkOptimizationFlags(CompileOptions& options) {
      for (const auto& flag : options.flags) {
         if (flag == "-O1"sv)
            options.optimizationLevel = 1;
         else if (flag == "-O2"sv)
            options.optimizationLevel = 2;
         else if (flag == "-O3"sv)
            options.optimizationLevel = 3;
      }
   }

   void checkFlags(const CompileOptions& options) {
      for (const auto& flag : options.flags) {
         if (flag != "--help"sv && flag != "-h"sv) {
            if (validFlags.find(flag) == validFlags.end())
               utils::pushError("unknown flag: '{}'", flag);
         }
      }
   }

   void analyzeOptions(CompileOptions& options) {
      checkBuildSystemOptions(options);
      checkInputFiles(options);
      checkHelpFlag(options);
      checkOutputFlags(options);
      checkOptimizationFlags(options);
      checkFlags(options);
   }

   void parseNewProjectCommand(CompileOptions& options, int argc, char** argv) {
      if (argc != 3)
         utils::pushError("usage: tungsten new <project_name>");
      else {
         options.files.push_back(argv[2]);
         options.newProject = true;
      }
   }

   void parseBuildSystemCommand(CompileOptions& options, int argc, char** argv) {
      options.buildSystem = true;
      for (int i = 2; i < argc; ++i) {
         std::string_view arg = argv[i];
         if (arg[0] == '-')
            options.flags.push_back(arg.data());
         else
            options.files.push_back(arg.data());
      }
   }

   void parseStandardArguments(CompileOptions& options, int argc, char** argv) {
      for (int i = 1; i < argc; ++i) {
         std::string_view arg = argv[i];
         if (arg[0] == '-') {
            if (arg == "-o"sv) {
               if (i + 1 < argc)
                  options.outputFile = argv[++i];
               else
                  utils::pushError("the -o flag requires an argument");
            }
            options.flags.push_back(arg.data());
         } else
            options.files.push_back(arg.data());
      }
   }

   export CompileOptions parseArguments(int argc, char** argv) {
      CompileOptions options;
      if (argc < 2) {
         utils::pushError("no input files");
         return options;
      }
      if (argv[1] == "new"sv || argv[1] == "build-tgs"sv) {
         if (argv[1] == "new"sv)
            parseNewProjectCommand(options, argc, argv);
         else
            parseBuildSystemCommand(options, argc, argv);
      } else
         parseStandardArguments(options, argc, argv);

      analyzeOptions(options);

      return options;
   }
} // namespace tungsten