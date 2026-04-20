#ifndef YK_ASTEROID_DIAGNOSTICS_SOURCE_LOCATION_HPP
#define YK_ASTEROID_DIAGNOSTICS_SOURCE_LOCATION_HPP

#include <compare>
#include <cstddef>
#include <string_view>

namespace yk::asteroid {

struct source_location {
  std::string_view file;
  std::size_t offset = 0;
  std::size_t line = 0;
  std::size_t column = 0;

  friend constexpr bool operator==(source_location const&, source_location const&) = default;
};

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_DIAGNOSTICS_SOURCE_LOCATION_HPP
