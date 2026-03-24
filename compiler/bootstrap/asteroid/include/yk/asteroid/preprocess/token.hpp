#ifndef YK_ASTEROID_PREPROCESS_TOKEN_HPP
#define YK_ASTEROID_PREPROCESS_TOKEN_HPP

#include <cstdint>
#include <string_view>

namespace yk::asteroid::preprocess {

// [lex.pptoken]
enum class PPTokenKind : std::uint8_t {
  // core pp-token kinds
  HeaderName,        // <foo> or "foo"
  Identifier,        // includes keywords (not distinguished yet)
  Number,            // pp-number: digit or .digit followed by valid continuations
  CharacterLiteral,  // 'x', u'x', U'x', L'x', u8'x'
  StringLiteral,     // "...", u"...", U"...", L"...", u8"...", R"...(...)..."
  OpOrPunc,          // operators and punctuators

  // not a standard pp-token kind, but tracked for preprocessor directives
  Whitespace,
  Newline,

  // each non-whitespace character that cannot be one of the above [lex.pptoken]/2
  NonWhitespaceChar,

  EndOfFile,
};

struct SourceLocation {
  std::uint32_t line = 1;
  std::uint32_t column = 1;
};

struct PPToken {
  PPTokenKind kind;
  std::string_view spelling;  // view into the original source buffer
  SourceLocation location;
};

}  // namespace yk::asteroid::preprocess

#endif  // YK_ASTEROID_PREPROCESS_TOKEN_HPP
