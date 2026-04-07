#ifndef YK_ASTEROID_PREPROCESS_CHAR_LITERAL_HPP
#define YK_ASTEROID_PREPROCESS_CHAR_LITERAL_HPP

#include <yk/asteroid/core/parser/alternative.hpp>
#include <yk/asteroid/core/parser/as_span.hpp>
#include <yk/asteroid/core/parser/common.hpp>
#include <yk/asteroid/core/parser/literal.hpp>
#include <yk/asteroid/core/parser/one_of.hpp>
#include <yk/asteroid/core/parser/optional.hpp>
#include <yk/asteroid/core/parser/plus.hpp>
#include <yk/asteroid/core/parser/satisfy.hpp>
#include <yk/asteroid/core/parser/sequence.hpp>
#include <yk/asteroid/core/parser/surrounded_by.hpp>

#include <string_view>

namespace yk::asteroid::preprocess {

struct char_literal_parser {
  static constexpr parser_result<std::string_view> operator()(std::string_view sv) noexcept
  {
    constexpr auto encoding_prefix = literal_string_parser{"u8"} | literal_string_parser{"u"} | literal_string_parser{"U"} | literal_string_parser{"L"};
    constexpr auto quote = literal_string_parser{"'"};

    constexpr auto backslash = literal_string_parser{"\\"};
    constexpr auto simple_escape_char = one_of_parser{R"('"?\abfnrtv)"};
    constexpr auto octal_digit = one_of_parser{"01234567"};
    constexpr auto hex_digit = one_of_parser{"0123456789abcdefABCDEF"};
    constexpr auto hex4 = hex_digit >> hex_digit >> hex_digit >> hex_digit;

    constexpr auto simple_escape = backslash >> simple_escape_char;
    constexpr auto octal_escape = backslash >> octal_digit >> -octal_digit >> -octal_digit;
    constexpr auto hex_escape = literal_string_parser{"\\x"} >> +hex_digit;
    constexpr auto universal_escape = (literal_string_parser{"\\U"} >> hex4 >> hex4) | (literal_string_parser{"\\u"} >> hex4);
    constexpr auto escape_sequence = simple_escape | octal_escape | hex_escape | universal_escape;

    constexpr auto plain_char = none_of_parser{"'\\\n"};
    constexpr auto c_char = plain_char | escape_sequence;

    constexpr auto body = surrounded_by_parser{quote, +c_char, quote};
    constexpr auto parser = as_span_parser{-encoding_prefix >> body};
    return parser(sv);
  }
};

}  // namespace yk::asteroid::preprocess

#endif  // YK_ASTEROID_PREPROCESS_CHAR_LITERAL_HPP
