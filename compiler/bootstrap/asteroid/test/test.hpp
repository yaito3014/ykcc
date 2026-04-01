#ifndef YK_ASTEROID_TEST_TEST_HPP
#define YK_ASTEROID_TEST_TEST_HPP

#include <catch2/catch_test_macros.hpp>

#include <yk/asteroid/core/parser/common.hpp>

#include <charconv>

template<class T>
struct numeric_parser {
  constexpr yk::asteroid::parser_result<T> operator()(std::string_view sv) const
  {
    char const* it = std::to_address(sv.cbegin());
    char const* se = std::to_address(sv.cend());
    T result;
    if (auto [ptr, ec] = std::from_chars(it, se, result); ec == std::errc{}) {
      return {result, sv.begin() + (ptr - it)};
    } else {
      return yk::asteroid::parse_failure;
    }
  };
};

struct alphabet_parser {
  constexpr yk::asteroid::parser_result<std::string> operator()(std::string_view sv) const
  {
    std::size_t const pos = sv.find_first_not_of("abcdefghijklmnopqrstuvwxyz");
    if (pos == 0) {
      return yk::asteroid::parse_failure;
    } else {
      return {std::string(sv.substr(0, pos)), sv.begin() + (pos == std::string_view::npos ? sv.size() : pos)};
    }
  };
};

#endif  // YK_ASTEROID_TEST_TEST_HPP
