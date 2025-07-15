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
      using namespace std;
      using namespace std::chrono;

      constexpr int nameWidth = 40;
      constexpr int barWidth = 52;

      float ratio = pack.size > 0 ? static_cast<float>(currentBytes) / pack.size : 1.0f;
      ratio = clamp(ratio, 0.0f, 1.0f);

      auto formatSize = [](size_t bytes) -> std::string {
         static const std::string units[] = {"B", "KiB", "MiB", "GiB"};
         double size = static_cast<double>(bytes);
         size_t unitIndex = 0;
         while (size >= 1024.0 && unitIndex < 3) {
            size /= 1024.0;
            ++unitIndex;
         }
         ostringstream ss;
         ss << fixed << setprecision(1) << size << " " << units[unitIndex];
         return ss.str();
      };

      ostringstream out;
      out << left << setw(nameWidth) << pack.name;
      out << right << setw(8) << formatSize(pack.size);
      out << "  " << setw(8) << formatSize(static_cast<size_t>(speedBytesPerSec)) << "/s";
      out << " " << timer.eta(currentBytes, pack.size);

      int filled = static_cast<int>(barWidth * ratio);
      out << " [" << string(filled, '#') << string(barWidth - filled, '-') << "]";
      out << " " << setw(3) << static_cast<int>(ratio * 100) << "%";

      cout << '\r' << out.str() << flush;
   }


} // namespace TPKG
