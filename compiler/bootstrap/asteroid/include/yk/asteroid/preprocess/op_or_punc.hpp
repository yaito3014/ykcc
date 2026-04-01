#ifndef YK_ASTEROID_PREPROCESS_OP_OR_PUNC_HPP
#define YK_ASTEROID_PREPROCESS_OP_OR_PUNC_HPP

#include <yk/asteroid/core/parser/common.hpp>

#include <algorithm>
#include <array>
#include <string_view>

namespace yk::asteroid::preprocess {

namespace detail {

using namespace std::string_view_literals;

inline constexpr std::array op_or_punc_4 = {"%:%:"sv};

inline constexpr std::array op_or_punc_3 = {"<=>"sv, ">>="sv, "<<="sv, "->*"sv, "..."sv};

inline constexpr std::array op_or_punc_2 = {
    "##"sv, "::"sv, "->"sv, ".*"sv, "+="sv, "-="sv, "*="sv, "/="sv, "%="sv, "^="sv, "&="sv, "|="sv, "=="sv, "!="sv, "<="sv,
    ">="sv, "&&"sv, "||"sv, "<<"sv, ">>"sv, "++"sv, "--"sv, "<%"sv, "%>"sv, "<:"sv, ":>"sv, "%:"sv, "[:"sv, ":]"sv, "^^"sv,
};

inline constexpr std::string_view op_or_punc_1 = "{}[]();:?~!+-*/%^&|=<>,.#";

inline constexpr std::array alternative_tokens = {
    "and"sv, "and_eq"sv, "bitand"sv, "bitor"sv, "compl"sv, "not"sv, "not_eq"sv, "or"sv, "or_eq"sv, "xor"sv, "xor_eq"sv,
};

}  // namespace detail

struct op_or_punc_parser {
  static constexpr parser_result<std::string_view> operator()(std::string_view sv) noexcept
  {
    if (sv.size() >= 4) {
      auto const prefix = sv.substr(0, 4);
      if (std::ranges::contains(detail::op_or_punc_4, prefix)) {
        return {prefix, sv.begin() + 4};
      }
    }
    if (sv.size() >= 3) {
      auto const prefix = sv.substr(0, 3);
      if (std::ranges::contains(detail::op_or_punc_3, prefix)) {
        return {prefix, sv.begin() + 3};
      }
    }
    if (sv.size() >= 2) {
      auto const prefix = sv.substr(0, 2);
      // [lex.pptoken]/3: <:: not followed by : or > is treated as < ::
      if (prefix == "<:" && sv.size() >= 3 && sv[2] == ':' && (sv.size() < 4 || (sv[3] != ':' && sv[3] != '>'))) {
        return {sv.substr(0, 1), sv.begin() + 1};
      }
      // [lex.pptoken]/3: [:: not followed by : is treated as [ ::, [:> is treated as [ :>
      if (prefix == "[:" && sv.size() >= 3 && (sv[2] == '>' || (sv[2] == ':' && (sv.size() < 4 || sv[3] != ':')))) {
        return {sv.substr(0, 1), sv.begin() + 1};
      }
      if (std::ranges::contains(detail::op_or_punc_2, prefix)) {
        return {prefix, sv.begin() + 2};
      }
    }
    if (!sv.empty() && detail::op_or_punc_1.contains(sv[0])) {
      return {sv.substr(0, 1), sv.begin() + 1};
    }
    return parse_failure;
  }
};

constexpr bool is_alternative_token(std::string_view sv) noexcept { return std::ranges::contains(detail::alternative_tokens, sv); }

}  // namespace yk::asteroid::preprocess

#endif  // YK_ASTEROID_PREPROCESS_OP_OR_PUNC_HPP
