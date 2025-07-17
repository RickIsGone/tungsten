module;
#include <curl/curl.h>
#include <stdexcept>
export module TPKG.curl;
import TPKG.progressBar;

namespace TPKG {
   export class Curl {
   public:
      Curl();
      ~Curl();
      Curl(const Curl& other) = delete;
      Curl operator=(const Curl& other) = delete;

      void download(const std::string& url);

   private:
      CURL* _curl;
   };

   //  ========================================== implementation ==========================================

   Curl::Curl() {
      _curl = curl_easy_init();
      if (!_curl)
         throw std::runtime_error("failed to init curl");
   }
   Curl::~Curl() {
      curl_easy_cleanup(_curl);
   }

   void Curl::download(const std::string& url) {
   }
} // namespace TPKG