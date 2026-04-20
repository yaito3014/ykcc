#ifndef YK_ASTEROID_CORE_PARSER_ONE_OF_HPP
#define YK_ASTEROID_CORE_PARSER_ONE_OF_HPP

#include <yk/asteroid/parser/common.hpp>

#include <string_view>

namespace yk::asteroid {

struct one_of_parser {
  std::string_view chars;

  constexpr parser_result<char> operator()(std::string_view sv) const noexcept
  {
    if (!sv.empty() && chars.contains(sv[0])) {
      return {sv[0], sv.begin() + 1};
    }
    return parse_failure;
  }
};

struct none_of_parser {
  std::string_view chars;

  constexpr parser_result<char> operator()(std::string_view sv) const noexcept
  {
    if (!sv.empty() && !chars.contains(sv[0])) {
      return {sv[0], sv.begin() + 1};
    }
    return parse_failure;
  }
};

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_CORE_PARSER_ONE_OF_HPP
