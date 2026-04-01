#ifndef YK_ASTEROID_PREPROCESS_IDENTIFIER_HPP
#define YK_ASTEROID_PREPROCESS_IDENTIFIER_HPP

#include <yk/asteroid/core/parser/common.hpp>

#include <string_view>

namespace yk::asteroid::preprocess {

namespace detail {

inline constexpr std::string_view identifier_start_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";

inline constexpr std::string_view identifier_continue_chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";

constexpr bool is_identifier_start(char c) noexcept { return identifier_start_chars.contains(c); }

constexpr bool is_identifier_continue(char c) noexcept { return identifier_continue_chars.contains(c); }

}  // namespace detail

struct identifier_parser {
  static constexpr parser_result<std::string_view> operator()(std::string_view sv) noexcept
  {
    if (sv.empty() || !detail::is_identifier_start(sv[0])) return parse_failure;
    std::size_t const pos = sv.find_first_not_of(detail::identifier_continue_chars, 1);
    auto const end = sv.begin() + (pos != std::string_view::npos ? pos : sv.size());
    return {std::string_view(sv.begin(), end), end};
  }
};

}  // namespace yk::asteroid::preprocess

#endif  // YK_ASTEROID_PREPROCESS_IDENTIFIER_HPP
