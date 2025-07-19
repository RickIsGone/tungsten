module;

#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>
#include <thread>
#include <algorithm>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

export module TPKG.progressBar;
import TPKG.package;
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

   // ========================================== implementation ==========================================

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

   export void printProgressBar(const Package& pack, size_t currentBytes, double speedBytesPerSec, const ProgressTimer& timer) {
      const int terminalWidth = shellWidth();

      const int nameWidth = pack.name.length() > 20 ? pack.name.length() : 20;
      const int sizeWidth = 8;
      const int speedWidth = 8;
      const int etaWidth = 8;
      const int fixedSpacing = nameWidth + sizeWidth + speedWidth + etaWidth + 10;
      int barWidth = terminalWidth - fixedSpacing;

      if (barWidth < 10) barWidth = 10;

      float ratio = pack.size > 0 ? static_cast<float>(currentBytes) / pack.size : 1.0f;
      ratio = std::clamp(ratio, 0.0f, 1.0f);
      if (ratio == 1.f)
         --barWidth;

      auto formatSize = [](size_t bytes) -> std::string {
         static const std::string units[] = {"B", "KiB", "MiB", "GiB"};
         double size = static_cast<double>(bytes);
         size_t unitIndex = 0;
         while (size >= 1024.0 && unitIndex < 3) {
            size /= 1024.0;
            ++unitIndex;
         }
         std::ostringstream ss;
         ss << std::fixed << std::setprecision(1) << size << " " << units[unitIndex];
         return ss.str();
      };

      std::ostringstream out;
      out << std::left << std::setw(nameWidth) << pack.name;
      out << std::right << std::setw(sizeWidth) << formatSize(pack.size);
      out << "  " << std::setw(speedWidth) << formatSize(static_cast<size_t>(speedBytesPerSec)) << "/s";
      out << " " << timer.eta(currentBytes, pack.size);

      int filled = static_cast<int>(barWidth * ratio);
      out << " [" << std::string(filled, '#') << std::string(barWidth - filled, '-') << "]";
      out << " " << static_cast<int>(ratio * 100) << "%";

      std::cout << '\r' << out.str() << std::flush;
   }


} // namespace TPKG
