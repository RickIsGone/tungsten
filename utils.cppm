module;

#include <format>
#include <string_view>
#include <iostream>
#include <sstream>

export module Tungsten.utils;

namespace Colors {
   inline constexpr const char* Red = "\x1B[91m";
   inline constexpr const char* White = "\x1B[97m";
   inline constexpr const char* Reset = "\x1B[0m";
} // namespace Colors

export inline std::stringstream errors{};

export namespace tungsten::utils {

   template <typename... Args>
   void pushError(std::string_view message, Args&&... args) {
      errors << "tungsten: " << Colors::Red << "error: " << Colors::White << std::vformat(message, std::make_format_args(args...)) << Colors::Reset << '\n';
   }
   void printErrors() {
      std::cerr << errors.str();
   }

} // namespace tungsten::utils
