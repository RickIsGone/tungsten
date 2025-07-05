module;

#include <chrono>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

export module TPKG.progressBar;

namespace TPKG {

   export class ProgressTimer {
   public:
      ProgressTimer() {
         _startTime = std::chrono::steady_clock::now();
      }

      std::string eta(size_t current, size_t total) const {
         using namespace std::chrono;
         if (current == 0) return "--:--";
         auto now = steady_clock::now();
         auto elapsed = duration_cast<seconds>(now - _startTime).count();
         long eta = static_cast<long>((elapsed * total) / current);
         long remaining = eta - elapsed;

         std::ostringstream out;
         out << std::setw(2) << std::setfill('0') << (remaining / 60)
             << ":" << std::setw(2) << std::setfill('0') << (remaining % 60);
         return out.str();
      }

   private:
      std::chrono::steady_clock::time_point _startTime;
   };

   //  ========================================== implementation ==========================================

   int shellWidth() {
#if defined(_WIN32)
      CONSOLE_SCREEN_BUFFER_INFO csbi;
      if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
         return csbi.srWindow.Right - csbi.srWindow.Left + 1;
      return 80;
#else
      winsize w{};
      if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0)
         return w.ws_col;
      return 80;
#endif
   }

   export void printProgressBar(size_t current, size_t total, const ProgressTimer& timer) {
      int width = shellWidth() * 0.7f;
      float ratio = (total > 0) ? static_cast<float>(current) / total : 1.0f;
      std::cout << "\r" << std::string(width, ' ') << "\r"; // Clear line

      std::string eta = " ETA: " + timer.eta(current, total);
      std::string percent = " " + std::to_string(static_cast<int>(ratio * 100)) + "%";

      int barWidth = width - static_cast<int>(percent.size() + eta.size() + 4); // 4 for " [] "

      int filled = static_cast<int>(barWidth * ratio);
      std::string bar = "\r[";
      bar += std::string(filled, '#');
      bar += std::string(barWidth - filled, '-');
      bar += "]";

      std::cout << "\r" << bar << percent << eta << std::flush;
   }
} // namespace TPKG
