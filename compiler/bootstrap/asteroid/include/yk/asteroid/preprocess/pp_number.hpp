#ifndef YK_ASTEROID_PREPROCESS_PP_NUMBER_HPP
#define YK_ASTEROID_PREPROCESS_PP_NUMBER_HPP

#include <yk/asteroid/core/parser/common.hpp>

#include <string_view>

namespace yk::asteroid::preprocess {

namespace detail {

inline constexpr std::string_view digit_chars = "0123456789";

inline constexpr std::string_view nondigit_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";

constexpr bool is_digit(char c) noexcept { return digit_chars.contains(c); }

constexpr bool is_nondigit(char c) noexcept { return nondigit_chars.contains(c); }

constexpr bool is_sign(char c) noexcept { return c == '+' || c == '-'; }

constexpr bool is_exponent(char c) noexcept { return c == 'e' || c == 'E' || c == 'p' || c == 'P'; }

}  // namespace detail

struct pp_number_parser {
  static constexpr parser_result<std::string_view> operator()(std::string_view sv) noexcept
  {
    if (sv.empty()) return parse_failure;

    std::size_t pos = 0;

    if (detail::is_digit(sv[0])) {
      pos = 1;
    } else if (sv[0] == '.' && sv.size() > 1 && detail::is_digit(sv[1])) {
      pos = 2;
    } else {
      return parse_failure;
    }

    while (pos < sv.size()) {
      char const c = sv[pos];
      if (detail::is_digit(c) || detail::is_nondigit(c) || c == '.') {
        ++pos;
      } else if (detail::is_exponent(sv[pos - 1]) && detail::is_sign(c)) {
        ++pos;
      } else if (c == '\'' && pos + 1 < sv.size() && (detail::is_digit(sv[pos + 1]) || detail::is_nondigit(sv[pos + 1]))) {
        pos += 2;
      } else {
        break;
      }
    }

    return {sv.substr(0, pos), sv.begin() + pos};
  }
};

}  // namespace yk::asteroid::preprocess

#endif  // YK_ASTEROID_PREPROCESS_PP_NUMBER_HPP
