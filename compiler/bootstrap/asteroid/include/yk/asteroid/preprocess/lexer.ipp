#ifndef YK_ASTEROID_PREPROCESS_LEXER_IPP
#define YK_ASTEROID_PREPROCESS_LEXER_IPP

namespace yk::asteroid::preprocess {

constexpr Lexer::Lexer(std::string_view source) noexcept : source_(source) {}

// Phase 2: skip past backslash-newline sequences
constexpr std::size_t Lexer::resolve(std::size_t pos) const noexcept {
  while (pos + 1 < source_.size() && source_[pos] == '\\' && source_[pos + 1] == '\n') {
    pos += 2;
  }
  return pos;
}

constexpr nullable_char Lexer::current() const noexcept {
  auto p = resolve(pos_);
  return p < source_.size() ? nullable_char{source_[p]} : nullable_char{};
}

constexpr nullable_char Lexer::peek_char(std::size_t offset) const noexcept {
  auto p = resolve(pos_);
  for (std::size_t i = 0; i < offset; ++i) {
    if (p >= source_.size()) return {};
    ++p;
    p = resolve(p);
  }
  return p < source_.size() ? nullable_char{source_[p]} : nullable_char{};
}

constexpr nullable_char Lexer::advance() noexcept {
  auto p = resolve(pos_);
  if (p >= source_.size()) return {};
  char c = source_[p];
  ++p;
  if (c == '\n') {
    ++line_;
    column_ = 1;
  } else {
    ++column_;
  }
  while (pos_ < p) {
    ++pos_;
  }
  pos_ = resolve(p);
  return c;
}

constexpr void Lexer::skip(std::size_t count) noexcept {
  for (std::size_t i = 0; i < count; ++i) {
    advance();
  }
}

constexpr bool Lexer::at_end() const noexcept { return resolve(pos_) >= source_.size(); }

constexpr SourceLocation Lexer::current_location() const noexcept { return {line_, column_}; }

constexpr PPToken Lexer::make_token(PPTokenKind kind, std::size_t start, SourceLocation loc) const noexcept {
  return {kind, source_.substr(start, pos_ - start), loc};
}

constexpr PPToken Lexer::peek() {
  auto saved_pos = pos_;
  auto saved_line = line_;
  auto saved_col = column_;
  auto saved_hash = after_directive_hash_;
  auto saved_include = after_include_keyword_;

  auto tok = next();

  pos_ = saved_pos;
  line_ = saved_line;
  column_ = saved_col;
  after_directive_hash_ = saved_hash;
  after_include_keyword_ = saved_include;

  return tok;
}

constexpr PPToken Lexer::next() {
  if (at_end()) {
    return {PPTokenKind::EndOfFile, {}, current_location()};
  }

  auto c = current();

  // Newline — significant for preprocessor
  if (c == '\n') {
    return lex_newline();
  }

  // Whitespace (not newline)
  if (is_whitespace(c.value())) {
    return lex_whitespace();
  }

  // Comments — replaced by a single whitespace token
  if (c == '/' && (peek_char() == '/' || peek_char() == '*')) {
    auto loc = current_location();
    auto start = pos_;
    if (peek_char() == '/') {
      try_skip_line_comment();
    } else {
      try_skip_block_comment();
    }
    return {PPTokenKind::Whitespace, source_.substr(start, pos_ - start), loc};
  }

  // Header name — only after #include
  if (after_include_keyword_ && (c == '<' || c == '"')) {
    after_include_keyword_ = false;
    return lex_header_name();
  }

  // String and character literals (with possible encoding prefix)
  // Prefixes: u8, u, U, L, followed by ' or "
  // Also R for raw strings: R"delim(...)delim", u8R"...", etc.
  if (c == '"' || c == '\'') {
    return lex_string_or_char_literal(0);
  }
  if (c == 'L' && (peek_char() == '"' || peek_char() == '\'')) {
    return lex_string_or_char_literal(1);
  }
  if (c == 'U' && (peek_char() == '"' || peek_char() == '\'')) {
    return lex_string_or_char_literal(1);
  }
  if (c == 'u') {
    if (peek_char() == '"' || peek_char() == '\'') {
      return lex_string_or_char_literal(1);
    }
    if (peek_char() == '8' && (peek_char(2) == '"' || peek_char(2) == '\'')) {
      return lex_string_or_char_literal(2);
    }
    // u8R"..." or uR"..."
    if (peek_char() == '8' && peek_char(2) == 'R' && peek_char(3) == '"') {
      return lex_string_or_char_literal(3);
    }
    if (peek_char() == 'R' && peek_char(2) == '"') {
      return lex_string_or_char_literal(2);
    }
  }
  if (c == 'L' && peek_char() == 'R' && peek_char(2) == '"') {
    return lex_string_or_char_literal(2);
  }
  if (c == 'U' && peek_char() == 'R' && peek_char(2) == '"') {
    return lex_string_or_char_literal(2);
  }
  if (c == 'R' && peek_char() == '"') {
    return lex_string_or_char_literal(1);
  }

  // pp-number: starts with digit, or . followed by digit
  if (is_digit(c.value()) || (c == '.' && peek_char() && is_digit(peek_char().value()))) {
    return lex_pp_number();
  }

  // Identifier
  if (is_identifier_start(c.value())) {
    auto tok = lex_identifier();

    // Track preprocessor directive context
    if (after_directive_hash_ && tok.spelling == "include") {
      after_include_keyword_ = true;
    }
    after_directive_hash_ = false;
    return tok;
  }

  // # — track for directive context
  if (c == '#') {
    auto tok = lex_op_or_punc();
    if (tok.spelling == "#") {
      after_directive_hash_ = true;
    }
    return tok;
  }

  // Operators and punctuators
  if (c == '{' || c == '}' || c == '[' || c == ']' || c == '(' || c == ')' || c == ';' ||
      c == ':' || c == '.' || c == '?' || c == '~' || c == '!' || c == '+' || c == '-' ||
      c == '*' || c == '/' || c == '%' || c == '^' || c == '&' || c == '|' || c == '=' ||
      c == '<' || c == '>' || c == ',') {
    return lex_op_or_punc();
  }

  // Non-whitespace character that doesn't match any above
  auto loc = current_location();
  auto start = pos_;
  advance();
  return make_token(PPTokenKind::NonWhitespaceChar, start, loc);
}

constexpr PPToken Lexer::lex_newline() {
  auto loc = current_location();
  auto start = pos_;
  advance();  // consume '\n'
  after_directive_hash_ = false;
  after_include_keyword_ = false;
  return make_token(PPTokenKind::Newline, start, loc);
}

constexpr PPToken Lexer::lex_whitespace() {
  auto loc = current_location();
  auto start = pos_;
  while (!at_end() && is_whitespace(current().value()) && current() != '\n') {
    advance();
  }
  return make_token(PPTokenKind::Whitespace, start, loc);
}

constexpr PPToken Lexer::lex_header_name() {
  auto loc = current_location();
  auto start = pos_;
  char delim = current().value();  // < or "
  char end_delim = (delim == '<') ? '>' : '"';
  advance();  // skip opening delimiter

  while (!at_end() && current() != end_delim && current() != '\n') {
    advance();
  }

  if (!at_end() && current() == end_delim) {
    advance();  // skip closing delimiter
  }
  // TODO: diagnostic if unterminated

  return make_token(PPTokenKind::HeaderName, start, loc);
}

constexpr PPToken Lexer::lex_identifier() {
  auto loc = current_location();
  auto start = pos_;
  advance();  // consume first character
  while (!at_end() && is_identifier_continue(current().value())) {
    advance();
  }
  return make_token(PPTokenKind::Identifier, start, loc);
}

// [lex.ppnumber]
// pp-number:
//   digit
//   . digit
//   pp-number digit
//   pp-number identifier-nondigit
//   pp-number ' digit
//   pp-number ' nondigit
//   pp-number e sign
//   pp-number E sign
//   pp-number p sign
//   pp-number P sign
//   pp-number .
constexpr PPToken Lexer::lex_pp_number() {
  auto loc = current_location();
  auto start = pos_;

  // Consume leading . if present
  if (current() == '.') {
    advance();
  }

  advance();  // consume first digit

  while (!at_end()) {
    auto c = current();

    if (is_digit(c.value()) || is_identifier_continue(c.value()) || c == '.') {
      advance();
      continue;
    }

    // digit separator: '
    if (c == '\'' && !at_end()) {
      auto next_c = peek_char();
      if (next_c && (is_digit(next_c.value()) || is_identifier_start(next_c.value()))) {
        advance();  // consume '
        advance();  // consume digit/nondigit
        continue;
      }
    }

    // exponent sign: e/E/p/P followed by +/-
    if ((c == 'e' || c == 'E' || c == 'p' || c == 'P') &&
        (peek_char() == '+' || peek_char() == '-')) {
      advance();  // consume e/E/p/P
      advance();  // consume +/-
      continue;
    }

    break;
  }

  return make_token(PPTokenKind::Number, start, loc);
}

constexpr PPToken Lexer::lex_string_or_char_literal(std::size_t prefix_len) {
  auto loc = current_location();
  auto start = pos_;

  // Skip the encoding prefix
  for (std::size_t i = 0; i < prefix_len; ++i) {
    advance();
  }

  bool is_raw = (current() == 'R');
  if (is_raw) {
    advance();  // skip R
  }

  char quote = current().value();  // " or '
  advance();               // skip opening quote

  if (is_raw && quote == '"') {
    // Raw string: R"delim(content)delim"
    // Collect delimiter
    std::size_t delim_start = pos_;
    while (!at_end() && current() != '(') {
      advance();
    }
    std::string_view delim = source_.substr(delim_start, pos_ - delim_start);

    if (!at_end()) advance();  // skip (

    // Scan for )delim"
    while (!at_end()) {
      if (current() == ')') {
        advance();
        // Check if followed by delim + "
        bool match = true;
        auto saved = pos_;
        auto saved_line2 = line_;
        auto saved_col2 = column_;
        for (std::size_t i = 0; i < delim.size(); ++i) {
          if (at_end() || current() != delim[i]) {
            match = false;
            break;
          }
          advance();
        }
        if (match && !at_end() && current() == '"') {
          advance();  // skip closing "
          break;
        }
        // Not a match, restore and continue
        pos_ = saved;
        line_ = saved_line2;
        column_ = saved_col2;
      } else {
        advance();
      }
    }
  } else {
    // Regular string or character literal
    while (!at_end() && current() != quote && current() != '\n') {
      if (current() == '\\') {
        advance();  // skip backslash
        if (!at_end() && current() != '\n') {
          advance();  // skip escaped character
        }
      } else {
        advance();
      }
    }

    if (!at_end() && current() == quote) {
      advance();  // skip closing quote
    }
    // TODO: diagnostic if unterminated
  }

  auto kind = (quote == '\'') ? PPTokenKind::CharacterLiteral : PPTokenKind::StringLiteral;
  return make_token(kind, start, loc);
}

constexpr PPToken Lexer::lex_op_or_punc() {
  auto loc = current_location();
  auto start = pos_;
  char c = current().value();
  advance();

  // Multi-character operators — try longest match
  auto try_next = [&](char expected) -> bool {
    if (!at_end() && current() == expected) {
      advance();
      return true;
    }
    return false;
  };

  switch (c) {
    case '#':
      try_next('#');  // ##
      break;
    case '+':
      if (!try_next('+')) try_next('=');  // ++ or +=
      break;
    case '-':
      if (!try_next('-') && !try_next('=')) {
        if (try_next('>')) try_next('*');  // ->*
      }
      break;
    case '*':
      try_next('=');
      break;
    case '/':
      try_next('=');
      break;
    case '%':
      if (!try_next('=') && !try_next('>')) try_next(':');  // %> or %:
      break;
    case '^':
      try_next('=');
      break;
    case '&':
      if (!try_next('&')) try_next('=');
      break;
    case '|':
      if (!try_next('|')) try_next('=');
      break;
    case '=':
      try_next('=');
      break;
    case '!':
      try_next('=');
      break;
    case '<':
      if (try_next('<')) {
        try_next('=');  // <<=
      } else if (!try_next('=')) {
        try_next(':');  // <: (digraph for [)
      }
      break;
    case '>':
      if (try_next('>')) {
        try_next('=');  // >>=
      } else {
        try_next('=');  // >=
      }
      break;
    case '.':
      if (current() == '.' && peek_char() == '.') {
        advance();
        advance();  // ...
      } else {
        try_next('*');  // .*
      }
      break;
    case ':':
      try_next(':');  // ::
      break;
    default:
      break;
  }

  return make_token(PPTokenKind::OpOrPunc, start, loc);
}

constexpr bool Lexer::try_skip_line_comment() noexcept {
  if (current() != '/' || peek_char() != '/') return false;
  advance();  // skip /
  advance();  // skip /
  while (!at_end() && current() != '\n') {
    advance();
  }
  // Don't consume the newline — it's a separate token
  return true;
}

constexpr bool Lexer::try_skip_block_comment() noexcept {
  if (current() != '/' || peek_char() != '*') return false;
  advance();  // skip /
  advance();  // skip *
  while (!at_end()) {
    if (current() == '*' && peek_char() == '/') {
      advance();  // skip *
      advance();  // skip /
      return true;
    }
    advance();
  }
  // TODO: diagnostic for unterminated block comment
  return true;
}

constexpr bool Lexer::is_identifier_start(char c) noexcept {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

constexpr bool Lexer::is_identifier_continue(char c) noexcept {
  return is_identifier_start(c) || is_digit(c);
}

constexpr bool Lexer::is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

constexpr bool Lexer::is_whitespace(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}

}  // namespace yk::asteroid::preprocess

#endif  // YK_ASTEROID_PREPROCESS_LEXER_IPP
