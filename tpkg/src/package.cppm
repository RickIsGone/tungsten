module;
#include <string>
export module TPKG.package;

namespace TPKG {
   export struct Package {
      const std::string name;
      const size_t size{};
   };
   //  ========================================== implementation ==========================================

} // namespace TPKG