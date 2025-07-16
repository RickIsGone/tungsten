#include <iostream>
// import TPKG.curl;
#include <filesystem>
#include <thread>
using namespace std::string_view_literals;
namespace fs = std::filesystem;
import TPKG.progressBar;
import TPKG.package;
constexpr const char* Version = "0.1.0";
namespace specialChars {
   constexpr const char* ColorRed = "\x1B[91m";
   constexpr const char* ColorReset = "\x1B[0m";
   constexpr const char* HideCursor = "\033[?25l";
   constexpr const char* ShowCursor = "\033[?25h";
} // namespace specialChars

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


int main(int argc, char** argv) {
   switch (argc) {
      case 1:
         helpMessage();
         break;

      case 2:
         if (argv[1] == "--version"sv) {
            std::cout << "tpkg version " << Version << "\n";
            break;
         }
         if (argv[1] == "-h"sv || argv[1] == "--help"sv) {
            helpMessage();
            break;
         }
         if (argv[1] == "list"sv) {
            std::cout << "no packages installed\n";
            break;
         }
         if (argv[1] == "update"sv) {
            std::cout << "no packages to update\n";
            break;
         }
         if (argv[1] == "upgrade"sv) {
            std::cout << "no packages to upgrade\n";
            break;
         }

         if (argv[1] == "install"sv || argv[1] == "search"sv || argv[1] == "remove"sv) {
            std::cerr << "tpkg:" << specialChars::ColorRed << " error:" << specialChars::ColorReset << " '" << argv[1] << "' no package specified\n";
            return EXIT_FAILURE;
         }
         std::cerr << "tpkg:" << specialChars::ColorRed << " error:" << specialChars::ColorReset << " unknown command: '" << argv[1] << "'\n";
         return EXIT_FAILURE;

      default:
         if (argv[1] == "install"sv) {
            fs::path downloads = downloadsDir(argv[0]);
            TPKG::ProgressTimer timer;
            TPKG::Package package{argv[2], 292864};
            double speed = 10 * 1024; // 10KiB
            std::cout << specialChars::HideCursor;
            for (size_t downloaded = 0; downloaded <= package.size; downloaded += speed) {
               std::this_thread::sleep_for(std::chrono::milliseconds(200));
               TPKG::printProgressBar(package, downloaded, speed, timer);
            }
            TPKG::printProgressBar(package, package.size, speed, timer);
            std::cout << specialChars::ShowCursor;
            break;
         }
         if (argv[1] == "remove"sv) {
            std::cout << "removing package '" << argv[2] << "'\n";
            break;
         }
         if (argv[1] == "search"sv) {
            std::cout << "no package named '" << argv[2] << "'\n";
            break;
         }

         std::cerr << "tpkg:" << specialChars::ColorRed << " error:" << specialChars::ColorReset << " unknown command: '" << argv[1] << "'\n";
         return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}