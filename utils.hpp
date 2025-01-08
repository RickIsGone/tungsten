#pragma once

#include <format>
#include <string_view>
#include <iostream>

namespace Colors {
   inline constexpr const char* Red = "\x1B[91m";
   inline constexpr const char* White = "\x1B[97m";
   inline constexpr const char* Reset = "\x1B[0m";
} // namespace Colors

namespace utils {

   template <typename... Args>
   void error(std::string_view message, Args&&... args) {
      std::cerr << "tungsten: " << Colors::Red << "error: " << Colors::White << std::vformat(message, std::make_format_args(args...)) << Colors::Reset << '\n';
      exit(EXIT_FAILURE);
   }

} // namespace utils
