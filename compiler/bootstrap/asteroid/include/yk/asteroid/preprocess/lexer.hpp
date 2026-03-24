#ifndef YK_ASTEROID_PREPROCESS_LEXER_HPP
#define YK_ASTEROID_PREPROCESS_LEXER_HPP

#include <yk/asteroid/preprocess/token.hpp>

#include <cstddef>
#include <string_view>

namespace yk::asteroid::preprocess {

// Phase 1-3 lexer: decomposes source text into preprocessing tokens.
// Handles line splicing (phase 2) transparently.
class Lexer {
public:
  constexpr explicit Lexer(std::string_view source) noexcept;

  // Returns the next pp-token. Returns EndOfFile when done.
  constexpr PPToken next();

  // Peek at the next pp-token without consuming it.
  constexpr PPToken peek();

  // True if all input has been consumed.
  constexpr bool at_end() const noexcept;

private:
  std::string_view source_;
  std::size_t pos_ = 0;
  std::uint32_t line_ = 1;
  std::uint32_t column_ = 1;

  // Whether we are at the start of a line (for #include header-name context).
  // Set to true after a newline, cleared after first non-whitespace token.
  bool after_directive_hash_ = false;
  bool after_include_keyword_ = false;

  // Character access with line-splicing (phase 2: backslash-newline removal)
  constexpr char current() const noexcept;
  constexpr char peek_char(std::size_t offset = 1) const noexcept;
  constexpr char advance() noexcept;
  constexpr void skip(std::size_t count = 1) noexcept;

  // Resolve position past any backslash-newline sequences
  constexpr std::size_t resolve(std::size_t pos) const noexcept;

  constexpr SourceLocation current_location() const noexcept;
  constexpr PPToken make_token(PPTokenKind kind, std::size_t start, SourceLocation loc) const noexcept;

  // Lexing routines for each pp-token kind
  constexpr PPToken lex_header_name();
  constexpr PPToken lex_identifier();
  constexpr PPToken lex_pp_number();
  constexpr PPToken lex_string_or_char_literal(std::size_t prefix_len);
  constexpr PPToken lex_whitespace();
  constexpr PPToken lex_newline();
  constexpr PPToken lex_op_or_punc();

  // Helpers
  constexpr bool try_skip_line_comment() noexcept;
  constexpr bool try_skip_block_comment() noexcept;
  static constexpr bool is_identifier_start(char c) noexcept;
  static constexpr bool is_identifier_continue(char c) noexcept;
  static constexpr bool is_digit(char c) noexcept;
  static constexpr bool is_whitespace(char c) noexcept;
};

}  // namespace yk::asteroid::preprocess

#include <yk/asteroid/preprocess/lexer.ipp>

#endif  // YK_ASTEROID_PREPROCESS_LEXER_HPP
