#ifndef YK_ASTEROID_PREPROCESS_CHAR_LITERAL_HPP
#define YK_ASTEROID_PREPROCESS_CHAR_LITERAL_HPP

#include <yk/asteroid/core/parser/common.hpp>

#include <string_view>

namespace yk::asteroid::preprocess {

struct char_literal_parser {
  static constexpr parser_result<std::string_view> operator()(std::string_view sv) noexcept
  {
    std::size_t pos = 0;

    if (sv.starts_with("u8'")) {
      pos = 2;
    } else if (sv.starts_with("u'") || sv.starts_with("U'") || sv.starts_with("L'")) {
      pos = 1;
    } else if (sv.starts_with("'")) {
      pos = 0;
    } else {
      return parse_failure;
    }

    ++pos;  // skip opening '

    while (pos < sv.size()) {
      if (sv[pos] == '\'') {
        auto const piece = sv.substr(0, pos + 1);
        return {piece, piece.end()};
      }
      if (sv[pos] == '\n') return parse_failure;
      if (sv[pos] == '\\' && pos + 1 < sv.size()) ++pos;
      ++pos;
    }

    return parse_failure;
  }
};

}  // namespace yk::asteroid::preprocess

#endif  // YK_ASTEROID_PREPROCESS_CHAR_LITERAL_HPP
