#pragma once

#include <format>
#include <string_view>
#include <iostream>
#include <string>

namespace Colors {
   inline constexpr const char* Red = "\x1B[91m";
   inline constexpr const char* White = "\x1B[97m";
   inline constexpr const char* Reset = "\x1B[0m";
} // namespace Colors

namespace utils {
   inline std::stringstream errors{};
   template <typename... Args>
   void pushError(std::string_view message, Args&&... args) {
      errors << "tungsten: " << Colors::Red << "error: " << Colors::White << std::vformat(message, std::make_format_args(args...)) << Colors::Reset << '\n';
   }
   inline void printErrors() {
      std::cerr << errors.str();
   }

} // namespace utils
