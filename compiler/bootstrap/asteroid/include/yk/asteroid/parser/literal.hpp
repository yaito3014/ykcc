#ifndef YK_ASTEROID_CORE_PARSER_LITERAL_HPP
#define YK_ASTEROID_CORE_PARSER_LITERAL_HPP

#include <yk/asteroid/parser/common.hpp>

#include <string_view>

namespace yk::asteroid {

struct any_char_parser {
  constexpr parser_result<char> operator()(std::string_view sv) const noexcept
  {
    if (!sv.empty()) {
      return {sv[0], sv.begin() + 1};
    }
    return parse_failure;
  }
};

struct literal_string_parser {
  std::string_view str;

  constexpr parser_result<std::string_view> operator()(std::string_view sv) const noexcept
  {
    if (sv.starts_with(str)) {
      auto const piece = sv.substr(0, str.size());
      return {piece, piece.end()};
    } else {
      return parse_failure;
    }
  }
};

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_CORE_PARSER_LITERAL_HPP
