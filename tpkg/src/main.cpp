#include <iostream>
// import TPKG.curl;
#include <filesystem>
#include <thread>
using namespace std::string_view_literals;
constexpr const char* Version = "0.1.0";
namespace fs = std::filesystem;
import TPKG.progressBar;

fs::path downloadsDir(const std::string& exePath) {
   fs::path downloadDir = exePath.substr(0, exePath.find_last_of("/\\")) / fs::path{"downloads"};
   return downloadDir;
}
void helpMessage() {
   std::cout << "usage: tpkg <command> [options]\n\n"
             << "Package Installation:\n"
             << "  install                Installs a package\n"
             << "  remove                 Uninstalls a package\n"
             << "  upgrade                Upgrades all outdate packages\n\n"
             << "Package Discovery:\n"
             << "  list                   Lists all installed packages\n"
             << "  search                 Searches for available packages\n"
             << "  update                 Lists package that can be upgraded\n\n";
}
constexpr const char* HideCursor = "\033[?25l";
constexpr const char* ShowCursor = "\033[?25h";

int main(int argc, char** argv) {
   if (argc == 1) {
      helpMessage();
      return EXIT_SUCCESS;
   }

   if (argc == 2) {
      if (argv[1] == "--version"sv)
         std::cout << "tpkg version " << Version << "\n";
      else if (argv[1] == "list"sv)
         std::cout << "no packages installed\n";
   }
   fs::path downloads = downloadsDir(argv[0]);
   // TPKG::Curl curl{};

   TPKG::ProgressTimer timer;
   std::cout << HideCursor;
   for (size_t i = 0; i <= 100; ++i) {
      printProgressBar(i, 100, timer);
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   }
   std::cout << ShowCursor;
   return EXIT_SUCCESS;
}