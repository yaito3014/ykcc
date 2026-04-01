#ifndef YK_ASTEROID_PREPROCESS_STRING_LITERAL_HPP
#define YK_ASTEROID_PREPROCESS_STRING_LITERAL_HPP

#include <yk/asteroid/core/parser/common.hpp>

#include <string_view>

namespace yk::asteroid::preprocess {

struct string_literal_parser {
  static constexpr parser_result<std::string_view> operator()(std::string_view sv) noexcept
  {
    std::size_t pos = 0;

    // encoding-prefix(opt) followed by " or R"
    if (sv.starts_with("u8R\"") || sv.starts_with("uR\"") || sv.starts_with("UR\"") || sv.starts_with("LR\"")) {
      return parse_raw(sv);
    }
    if (sv.starts_with("R\"")) {
      return parse_raw(sv);
    }

    if (sv.starts_with("u8\"")) {
      pos = 2;
    } else if (sv.starts_with("u\"") || sv.starts_with("U\"") || sv.starts_with("L\"")) {
      pos = 1;
    } else if (sv.starts_with("\"")) {
      pos = 0;
    } else {
      return parse_failure;
    }

    ++pos;  // skip opening "

    while (pos < sv.size()) {
      if (sv[pos] == '"') {
        auto const piece = sv.substr(0, pos + 1);
        return {piece, piece.end()};
      }
      if (sv[pos] == '\n') return parse_failure;
      if (sv[pos] == '\\' && pos + 1 < sv.size()) ++pos;
      ++pos;
    }

    return parse_failure;
  }

private:
  static constexpr parser_result<std::string_view> parse_raw(std::string_view sv) noexcept
  {
    // find the opening R"
    auto const r_pos = sv.find("R\"");
    if (r_pos == std::string_view::npos) return parse_failure;

    auto const delim_start = r_pos + 2;
    auto const paren_pos = sv.find('(', delim_start);
    if (paren_pos == std::string_view::npos) return parse_failure;
    if (paren_pos - delim_start > 16) return parse_failure;  // delimiter max 16 chars

    auto const delim = sv.substr(delim_start, paren_pos - delim_start);

    // search for )delim"
    auto const body_start = paren_pos + 1;
    for (std::size_t i = body_start; i < sv.size(); ++i) {
      if (sv[i] == ')') {
        auto const remaining = sv.substr(i + 1);
        if (remaining.starts_with(delim) && remaining.size() > delim.size() && remaining[delim.size()] == '"') {
          auto const end = i + 1 + delim.size() + 1;
          auto const piece = sv.substr(0, end);
          return {piece, piece.end()};
        }
      }
    }

    return parse_failure;
  }
};

}  // namespace yk::asteroid::preprocess

#endif  // YK_ASTEROID_PREPROCESS_STRING_LITERAL_HPP
