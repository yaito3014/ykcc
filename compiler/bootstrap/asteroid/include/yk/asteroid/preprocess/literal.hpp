#ifndef YK_ASTEROID_PREPROCESS_LITERAL_HPP
#define YK_ASTEROID_PREPROCESS_LITERAL_HPP

#include <yk/asteroid/core/parser/alternative.hpp>
#include <yk/asteroid/core/parser/as_span.hpp>
#include <yk/asteroid/core/parser/common.hpp>
#include <yk/asteroid/core/parser/kleene.hpp>
#include <yk/asteroid/core/parser/literal.hpp>
#include <yk/asteroid/core/parser/negation.hpp>
#include <yk/asteroid/core/parser/one_of.hpp>
#include <yk/asteroid/core/parser/optional.hpp>
#include <yk/asteroid/core/parser/plus.hpp>
#include <yk/asteroid/core/parser/sequence.hpp>
#include <yk/asteroid/core/parser/surrounded_by.hpp>

#include <string_view>

namespace yk::asteroid::preprocess {

namespace detail {

inline constexpr auto encoding_prefix = literal_string_parser{"u8"} | literal_string_parser{"u"} | literal_string_parser{"U"} | literal_string_parser{"L"};

inline constexpr auto backslash = literal_string_parser{"\\"};
inline constexpr auto simple_escape_char = one_of_parser{R"('"?\abfnrtv)"};
inline constexpr auto octal_digit = one_of_parser{"01234567"};
inline constexpr auto hex_digit = one_of_parser{"0123456789abcdefABCDEF"};
inline constexpr auto hex4 = hex_digit >> hex_digit >> hex_digit >> hex_digit;

inline constexpr auto simple_escape = backslash >> simple_escape_char;
inline constexpr auto octal_escape = backslash >> octal_digit >> -octal_digit >> -octal_digit;
inline constexpr auto hex_escape = literal_string_parser{"\\x"} >> +hex_digit;
inline constexpr auto universal_escape = (literal_string_parser{"\\U"} >> hex4 >> hex4) | (literal_string_parser{"\\u"} >> hex4);
inline constexpr auto escape_sequence = simple_escape | octal_escape | hex_escape | universal_escape;

}  // namespace detail

struct char_literal_parser {
  static constexpr parser_result<std::string_view> operator()(std::string_view sv) noexcept
  {
    constexpr auto apostrophe = literal_string_parser{"'"};
    constexpr auto basic_c_char = none_of_parser{"'\\\n"};
    constexpr auto c_char = basic_c_char | detail::escape_sequence;

    constexpr auto body = surrounded_by_parser{apostrophe, +c_char, apostrophe};
    constexpr auto parser = as_span_parser{-detail::encoding_prefix >> body};
    return parser(sv);
  }
};

struct raw_string_literal_parser {
  static constexpr parser_result<std::string_view> operator()(std::string_view sv) noexcept
  {
    constexpr auto prefix = as_span_parser{-detail::encoding_prefix >> literal_string_parser{"R\""}};

    auto const prefix_result = prefix(sv);
    if (!prefix_result) return parse_failure;

    auto const rest = std::string_view(prefix_result.parsed_point(), sv.end());

    constexpr std::size_t max_delimiter_length = 16;  // [lex.string]/2
    auto const paren_pos = rest.find('(');
    if (paren_pos == std::string_view::npos || paren_pos > max_delimiter_length) return parse_failure;

    auto const delim = rest.substr(0, paren_pos);
    auto const closing = literal_string_parser{")"} >> literal_string_parser{delim} >> literal_string_parser{"\""};
    auto const not_closing = !closing >> any_char_parser{};
    auto const body = literal_string_parser{"("} >> *not_closing >> closing;

    if (auto const body_result = body(rest.substr(paren_pos))) {
      auto const piece = sv.substr(0, static_cast<std::size_t>(body_result.parsed_point() - sv.begin()));
      return {piece, piece.end()};
    }

    return parse_failure;
  }
};

struct regular_string_literal_parser {
  static constexpr parser_result<std::string_view> operator()(std::string_view sv) noexcept
  {
    constexpr auto quotation = literal_string_parser{"\""};
    constexpr auto basic_s_char = none_of_parser{"\"\\\n"};
    constexpr auto s_char = basic_s_char | detail::escape_sequence;

    constexpr auto body = surrounded_by_parser{quotation, *s_char, quotation};
    constexpr auto parser = as_span_parser{-detail::encoding_prefix >> body};
    return parser(sv);
  }
};

struct string_literal_parser {
  static constexpr parser_result<std::string_view> operator()(std::string_view sv) noexcept
  {
    constexpr auto parser = raw_string_literal_parser{} | regular_string_literal_parser{};
    return parser(sv);
  }
};

}  // namespace yk::asteroid::preprocess

#endif  // YK_ASTEROID_PREPROCESS_LITERAL_HPP
