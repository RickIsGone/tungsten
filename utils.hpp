#pragma once

#include <format>
#include <string_view>
#include <iostream>

inline constexpr const char* COLOR_RED = "\x1B[91m";
inline constexpr const char* COLOR_WHITE = "\x1B[97m";
inline constexpr const char* COLOR_RESET = "\x1B[0m";

namespace utils {

   template <typename... Args>
   void error(std::string_view message, Args&&... args) {
      std::cerr << "tungsten: " << COLOR_RED << "error: " << COLOR_WHITE << std::vformat(message, std::make_format_args(args...)) << COLOR_RESET << '\n';
      exit(EXIT_FAILURE);
   }

} // namespace utils
