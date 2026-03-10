module;

#include <format>
#include <string_view>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

export module Tungsten.utils;

using namespace std::string_view_literals;
namespace fs = std::filesystem;

export namespace tungsten::Colors {
   constexpr const char* Red = "\x1B[91m";
   constexpr const char* Green = "\x1B[92m";
   constexpr const char* Yellow = "\x1B[93m";
   constexpr const char* White = "\x1B[97m";
   constexpr const char* Reset = "\x1B[0m";
} // namespace tungsten::Colors

export inline std::stringstream errors{};

namespace tungsten::utils {
   static bool runGitInitSilently() {
#ifdef _WIN32
      std::wstring cmd = L"git init";
      STARTUPINFOW si{};
      PROCESS_INFORMATION pi{};
      si.cb = sizeof(si);
      si.dwFlags = STARTF_USESTDHANDLES;

      HANDLE devNull = CreateFileW(L"NUL", GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
      if (devNull == INVALID_HANDLE_VALUE) {
         return false;
      }

      si.hStdOutput = devNull;
      si.hStdError = devNull;
      si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

      BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                               CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
      CloseHandle(devNull);
      if (!ok) {
         return false;
      }
      WaitForSingleObject(pi.hProcess, INFINITE);
      DWORD exitCode = 1;
      GetExitCodeProcess(pi.hProcess, &exitCode);
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
      return exitCode == 0;
#else
      pid_t pid = fork();
      if (pid == 0) {
         int devNull = open("/dev/null", O_WRONLY);
         if (devNull >= 0) {
            dup2(devNull, STDOUT_FILENO);
            dup2(devNull, STDERR_FILENO);
            close(devNull);
         }
         execlp("git", "git", "init", (char*)nullptr);
         _exit(127);
      }
      if (pid < 0) {
         return false;
      }
      int status = 0;
      if (waitpid(pid, &status, 0) < 0) {
         return false;
      }
      return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
   }
} // namespace tungsten::utils
export std::string operator*(std::string_view value, size_t amount) {
   std::string result;
   for (size_t i = 0; i < amount; ++i)
      result += value;
   return result;
}

export namespace tungsten::utils {


   template <typename... Args>
   void pushError(std::string_view message, Args&&... args) {
      errors << "tungsten: " << Colors::Red << "error: " << Colors::White << std::vformat(message, std::make_format_args(args...)) << Colors::Reset << '\n';
   }
   bool hasErrors() {
      return !errors.str().empty();
   }
   void printErrors() {
      std::cerr << errors.str();
   }

   void createProject(std::string_view name) {
      if (fs::exists(name)) {
         pushError("a file or directory with the name '{}' already exists", name);
         printErrors();
         return;
      }
      fs::create_directory(name);
      fs::current_path(name);
      fs::create_directory("src");

      std::ofstream gitIgnoreFile(".gitignore");
      std::ofstream readmeFile("README.md");
      std::ofstream mainFile("src/main.tgs");
      std::ofstream buildFile("build.tgs");

      mainFile << "fun main() -> num {\n"
               << "    print(\"Hello, World!\");\n"
               << "    ret CodeSuccess;\n"
               << "}\n";

      buildFile << "import build;\n"
                << "Project " << name << "{Executable, " << name << "};\n\n"
                << "fun main() -> num {\n"
                << "    " << name << ".addSource(\"main.tgs\");\n"
                << "    " << name << ".build();\n"
                << "}\n";

      gitIgnoreFile << "build/\n";

      readmeFile << std::format("# {}  \n", name);

      if (!runGitInitSilently()) {
         pushError("failed to initialize git repo");
         printErrors();
      }
   }

#ifdef TUNGSTEN_DEBUG
   template <typename... Args>
   void debugLog(std::string_view message, Args&&... args) {
      std::cout << "[DEBUG] " << std::vformat(message, std::make_format_args(args...)) << '\n';
   }
#else
   template <typename... Args>
   void debugLog(Args&&... args) {}
#endif
} // namespace tungsten::utils
