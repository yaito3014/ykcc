#ifndef YK_ASTEROID_PREPROCESS_LEXER_HPP
#define YK_ASTEROID_PREPROCESS_LEXER_HPP

#include <yk/asteroid/diagnostics/diagnostic.hpp>
#include <yk/asteroid/diagnostics/source_location.hpp>
#include <yk/asteroid/preprocess/line_splicer.hpp>
#include <yk/asteroid/preprocess/pp_token.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

namespace yk::asteroid {

namespace detail {

struct punctuator_entry {
  std::string_view text;
};

inline constexpr std::array<punctuator_entry, 1> punctuators_len4 = {{
    {"%:%:"},
}};

inline constexpr std::array<punctuator_entry, 5> punctuators_len3 = {{
    {"<<="},
    {">>="},
    {"..."},
    {"->*"},
    {"<=>"},
}};

inline constexpr std::array<punctuator_entry, 30> punctuators_len2 = {{
    {"##"}, {"<%"}, {"%>"}, {"<:"}, {":>"}, {"%:"}, {"::"}, {".*"}, {"->"}, {"++"}, {"--"}, {"<<"}, {">>"}, {"<="}, {">="},
    {"=="}, {"!="}, {"&&"}, {"||"}, {"+="}, {"-="}, {"*="}, {"/="}, {"%="}, {"^="}, {"&="}, {"|="}, {"[:"}, {":]"}, {"^^"},
}};

inline constexpr std::string_view punctuators_len1 = "{}[]();:?.~!+-*/%^&|=<>,#";

}  // namespace detail

template<class Sink = no_diagnostic_sink>
class lexer {
public:
  constexpr lexer(line_splicer const& splicer, std::string_view file_name) : splicer_(&splicer), file_name_(file_name) {}

  constexpr lexer(line_splicer const& splicer, std::string_view file_name, Sink sink) : splicer_(&splicer), file_name_(file_name), sink_(std::move(sink)) {}

  constexpr pp_token next()
  {
    auto const src = splicer_->spliced();
    if (pos_ >= src.size()) {
      return pp_token{pp_token_kind::end_of_file, {}, location_at(src.size())};
    }

    std::size_t const start = pos_;
    char const c = src[pos_];

    if (c == '\n') {
      ++pos_;
      return make_token(pp_token_kind::newline, start);
    }

    if (is_h_space(c)) {
      return lex_whitespace(start);
    }

    if (c == '/' && pos_ + 1 < src.size() && src[pos_ + 1] == '/') {
      return lex_line_comment(start);
    }

    if (c == '/' && pos_ + 1 < src.size() && src[pos_ + 1] == '*') {
      return lex_block_comment(start);
    }

    if (c == '\'' || c == '"' || c == 'R' || c == 'u' || c == 'U' || c == 'L') {
      if (auto lit = try_lex_raw_string_literal(start)) return *lit;
      if (auto lit = try_lex_char_or_string_literal(start)) return *lit;
    }

    if (is_id_start(c)) {
      return lex_identifier(start);
    }

    if (is_digit(c) || (c == '.' && pos_ + 1 < src.size() && is_digit(src[pos_ + 1]))) {
      return lex_pp_number(start);
    }

    if (auto punct = try_lex_punctuator(start)) return *punct;

    ++pos_;
    return make_token(pp_token_kind::other, start);
  }

  // Alternate entry point for header-name context. Only valid inside #include,
  // __has_include, or import directives — the directive parser calls this after it
  // has skipped horizontal whitespace and established that a header-name is expected.
  // Returns nullopt if the current position does not start a header-name.
  constexpr std::optional<pp_token> try_lex_header_name()
  {
    auto const src = splicer_->spliced();
    if (pos_ >= src.size()) return std::nullopt;
    char const open = src[pos_];
    if (open != '<' && open != '"') return std::nullopt;
    char const close = (open == '<') ? '>' : '"';

    std::size_t const start = pos_;
    ++pos_;
    while (pos_ < src.size() && src[pos_] != close && src[pos_] != '\n') ++pos_;
    if (pos_ < src.size() && src[pos_] == close) {
      ++pos_;
      return make_token(pp_token_kind::header_name, start);
    }
    pos_ = start;
    return std::nullopt;
  }

  constexpr bool at_end() const noexcept { return pos_ >= splicer_->spliced().size(); }

  constexpr std::size_t position() const noexcept { return pos_; }

private:
  line_splicer const* splicer_;
  std::string_view file_name_;
  [[no_unique_address]] Sink sink_{};
  std::size_t pos_ = 0;

  static constexpr bool is_id_start(char c) noexcept { return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
  static constexpr bool is_id_continue(char c) noexcept { return is_id_start(c) || (c >= '0' && c <= '9'); }
  static constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }
  static constexpr bool is_h_space(char c) noexcept { return c == ' ' || c == '\t' || c == '\v' || c == '\f'; }

  constexpr source_location location_at(std::size_t logical_offset) const noexcept
  {
    auto const loc = splicer_->physical_location(logical_offset);
    return source_location{file_name_, splicer_->physical_offset(logical_offset), loc.line, loc.column};
  }

  constexpr void report(diagnostic_level level, source_location loc, std::string msg) const { sink_(diagnostic{level, std::move(loc), std::move(msg)}); }

  constexpr pp_token make_token(pp_token_kind kind, std::size_t start) const noexcept
  {
    return pp_token{kind, splicer_->spliced().substr(start, pos_ - start), location_at(start)};
  }

  constexpr pp_token lex_whitespace(std::size_t start) noexcept
  {
    auto const src = splicer_->spliced();
    while (pos_ < src.size() && is_h_space(src[pos_])) ++pos_;
    return make_token(pp_token_kind::whitespace, start);
  }

  constexpr pp_token lex_line_comment(std::size_t start) noexcept
  {
    auto const src = splicer_->spliced();
    pos_ += 2;
    while (pos_ < src.size() && src[pos_] != '\n') ++pos_;
    return make_token(pp_token_kind::whitespace, start);
  }

  constexpr pp_token lex_block_comment(std::size_t start)
  {
    auto const src = splicer_->spliced();
    pos_ += 2;
    while (pos_ + 1 < src.size() && !(src[pos_] == '*' && src[pos_ + 1] == '/')) ++pos_;
    if (pos_ + 1 < src.size()) {
      pos_ += 2;
    } else {
      pos_ = src.size();
      report(diagnostic_level::error, location_at(start), "unterminated block comment");
    }
    return make_token(pp_token_kind::whitespace, start);
  }

  constexpr pp_token lex_identifier(std::size_t start) noexcept
  {
    auto const src = splicer_->spliced();
    ++pos_;
    while (pos_ < src.size() && is_id_continue(src[pos_])) ++pos_;
    return make_token(pp_token_kind::identifier, start);
  }

  constexpr pp_token lex_pp_number(std::size_t start) noexcept
  {
    auto const src = splicer_->spliced();
    ++pos_;
    while (pos_ < src.size()) {
      char const c = src[pos_];
      if (c == 'e' || c == 'E' || c == 'p' || c == 'P') {
        ++pos_;
        if (pos_ < src.size() && (src[pos_] == '+' || src[pos_] == '-')) ++pos_;
        continue;
      }
      if (c == '\'') {
        if (pos_ + 1 < src.size() && (is_id_continue(src[pos_ + 1]))) {
          pos_ += 2;
          continue;
        }
        break;
      }
      if (c == '.' || is_id_continue(c)) {
        ++pos_;
        continue;
      }
      break;
    }
    return make_token(pp_token_kind::pp_number, start);
  }

  constexpr std::optional<pp_token> try_lex_punctuator(std::size_t start) noexcept
  {
    auto const src = splicer_->spliced();
    auto const remaining = src.substr(pos_);

    // [lex.pptoken] disambiguation: if the next three characters are `<::` or `[::`
    // and the subsequent character is neither `:` nor `>`, treat the first character
    // as a single-char punctuator, not as the opening of the alternative token / splice.
    if (remaining.size() >= 3 && (remaining[0] == '<' || remaining[0] == '[') && remaining[1] == ':' && remaining[2] == ':') {
      if (remaining.size() == 3 || (remaining[3] != ':' && remaining[3] != '>')) {
        ++pos_;
        return make_token(pp_token_kind::punctuator, start);
      }
    }

    // `[:>` must tokenize as `[` followed by the digraph `:>` (i.e., `]`), not as
    // the splicer-open `[:` followed by `>`.
    if (remaining.size() >= 3 && remaining[0] == '[' && remaining[1] == ':' && remaining[2] == '>') {
      ++pos_;
      return make_token(pp_token_kind::punctuator, start);
    }

    for (auto const& e : detail::punctuators_len4) {
      if (remaining.starts_with(e.text)) {
        pos_ += e.text.size();
        return make_token(pp_token_kind::punctuator, start);
      }
    }
    for (auto const& e : detail::punctuators_len3) {
      if (remaining.starts_with(e.text)) {
        pos_ += e.text.size();
        return make_token(pp_token_kind::punctuator, start);
      }
    }
    for (auto const& e : detail::punctuators_len2) {
      if (remaining.starts_with(e.text)) {
        pos_ += e.text.size();
        return make_token(pp_token_kind::punctuator, start);
      }
    }
    if (!remaining.empty() && detail::punctuators_len1.find(remaining.front()) != std::string_view::npos) {
      ++pos_;
      return make_token(pp_token_kind::punctuator, start);
    }
    return std::nullopt;
  }

  constexpr std::optional<pp_token> try_lex_char_or_string_literal(std::size_t start)
  {
    auto const src = splicer_->spliced();
    std::size_t const p = pos_;

    std::size_t prefix_len = 0;
    if (p < src.size()) {
      if (src[p] == 'u') {
        prefix_len = (p + 1 < src.size() && src[p + 1] == '8') ? 2 : 1;
      } else if (src[p] == 'U' || src[p] == 'L') {
        prefix_len = 1;
      }
    }

    std::size_t const q = p + prefix_len;
    if (q >= src.size()) return std::nullopt;

    char const quote = src[q];
    if (quote != '\'' && quote != '"') return std::nullopt;

    pos_ = q + 1;
    while (pos_ < src.size() && src[pos_] != quote && src[pos_] != '\n') {
      if (src[pos_] == '\\' && pos_ + 1 < src.size()) {
        pos_ += 2;
      } else {
        ++pos_;
      }
    }
    if (pos_ < src.size() && src[pos_] == quote) {
      ++pos_;
    } else {
      report(diagnostic_level::error, location_at(start), quote == '\'' ? "unterminated character literal" : "unterminated string literal");
    }

    while (pos_ < src.size() && is_id_continue(src[pos_])) ++pos_;

    return make_token(quote == '\'' ? pp_token_kind::character_literal : pp_token_kind::string_literal, start);
  }

  constexpr std::optional<pp_token> try_lex_raw_string_literal(std::size_t start)
  {
    auto const spliced = splicer_->spliced();
    auto const source = splicer_->source();
    std::size_t p = pos_;

    std::size_t prefix_len = 0;
    if (p < spliced.size() && spliced[p] == 'u') {
      prefix_len = (p + 1 < spliced.size() && spliced[p + 1] == '8') ? 2 : 1;
    } else if (p < spliced.size() && (spliced[p] == 'U' || spliced[p] == 'L')) {
      prefix_len = 1;
    }

    if (p + prefix_len + 1 >= spliced.size()) return std::nullopt;
    if (spliced[p + prefix_len] != 'R' || spliced[p + prefix_len + 1] != '"') return std::nullopt;

    std::size_t const dchar_begin = p + prefix_len + 2;
    std::size_t q = dchar_begin;
    while (q < spliced.size() && q - dchar_begin < 16) {
      char const ch = spliced[q];
      if (ch == '(' || ch == ')' || ch == '"' || ch == '\\' || ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f' || ch == '\n') break;
      ++q;
    }
    if (q >= spliced.size() || spliced[q] != '(') return std::nullopt;

    std::string_view const delim = spliced.substr(dchar_begin, q - dchar_begin);
    std::size_t const phys_after_open_paren = splicer_->physical_offset(q + 1);

    std::string terminator;
    terminator.reserve(delim.size() + 2);
    terminator.push_back(')');
    terminator.append(delim);
    terminator.push_back('"');

    std::size_t const found = source.find(terminator, phys_after_open_paren);
    if (found == std::string_view::npos) {
      report(diagnostic_level::error, location_at(start), "unterminated raw string literal");
      pos_ = spliced.size();
      return pp_token{pp_token_kind::string_literal, source.substr(splicer_->physical_offset(start)), location_at(start)};
    }

    std::size_t const phys_end = found + terminator.size();

    pos_ = q + 1;
    while (pos_ < spliced.size() && splicer_->physical_offset(pos_) < phys_end) ++pos_;

    while (pos_ < spliced.size() && is_id_continue(spliced[pos_])) ++pos_;

    std::size_t const phys_start = splicer_->physical_offset(start);
    std::size_t const phys_suffix_end = splicer_->physical_offset(pos_);

    return pp_token{pp_token_kind::string_literal, source.substr(phys_start, phys_suffix_end - phys_start), location_at(start)};
  }
};

lexer(line_splicer const&, std::string_view) -> lexer<no_diagnostic_sink>;

template<class Sink>
lexer(line_splicer const&, std::string_view, Sink) -> lexer<Sink>;

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_PREPROCESS_LEXER_HPP
