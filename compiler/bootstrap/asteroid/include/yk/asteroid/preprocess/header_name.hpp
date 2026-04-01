#ifndef YK_ASTEROID_PREPROCESS_HEADER_NAME_HPP
#define YK_ASTEROID_PREPROCESS_HEADER_NAME_HPP

#include <yk/asteroid/core/parser/common.hpp>

#include <string_view>

namespace yk::asteroid::preprocess {

struct header_name_parser {
  static constexpr parser_result<std::string_view> operator()(std::string_view sv) noexcept
  {
    if (sv.size() < 3) return parse_failure;

    auto const close = [&]() -> std::optional<char> {
      if (sv[0] == '<') return '>';
      if (sv[0] == '"') return '"';
      return std::nullopt;
    }();
    if (!close) return parse_failure;

    for (std::size_t i = 1; i < sv.size(); ++i) {
      if (sv[i] == '\n') return parse_failure;
      if (sv[i] == *close) {
        auto const piece = sv.substr(0, i + 1);
        return {piece, piece.end()};
      }
    }

    return parse_failure;
  }
};

}  // namespace yk::asteroid::preprocess

#endif  // YK_ASTEROID_PREPROCESS_HEADER_NAME_HPP
