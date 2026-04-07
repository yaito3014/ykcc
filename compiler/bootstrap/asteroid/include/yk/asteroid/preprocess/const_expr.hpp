#ifndef YK_ASTEROID_PREPROCESS_CONST_EXPR_HPP
#define YK_ASTEROID_PREPROCESS_CONST_EXPR_HPP

#include <yk/asteroid/preprocess/lexer.hpp>

#include <charconv>
#include <concepts>
#include <expected>
#include <span>
#include <string_view>

#include <cstdint>

namespace yk::asteroid::preprocess {

struct const_expr_error {
  std::string_view message;
};

struct const_expr_result {
  std::intmax_t value;
};

namespace detail {

struct parsed_char {
  char value;
  std::size_t length;
};

template<std::predicate<std::string_view> DefinedLookup>
class const_expr_evaluator {
public:
  constexpr const_expr_evaluator(std::span<pp_token const> tokens, DefinedLookup defined_lookup) noexcept : tokens_(tokens), defined_lookup_(defined_lookup) {}

  constexpr std::expected<const_expr_result, const_expr_error> evaluate()
  {
    skip_whitespace();
    auto result = ternary();
    if (!result) return result;
    skip_whitespace();
    if (pos_ < tokens_.size()) {
      return std::unexpected(const_expr_error{"unexpected token after expression"});
    }
    return result;
  }

private:
  std::span<pp_token const> tokens_;
  std::size_t pos_ = 0;
  DefinedLookup defined_lookup_;

  constexpr pp_token const* peek() const noexcept
  {
    if (pos_ >= tokens_.size()) return nullptr;
    return &tokens_[pos_];
  }

  constexpr void advance() noexcept
  {
    if (pos_ < tokens_.size()) ++pos_;
    skip_whitespace();
  }

  constexpr void skip_whitespace() noexcept
  {
    while (pos_ < tokens_.size() && tokens_[pos_].kind == pp_token_kind::whitespace) ++pos_;
  }

  constexpr bool match(std::string_view piece) const noexcept
  {
    auto const* tok = peek();
    return tok && tok->piece == piece;
  }

  constexpr bool consume(std::string_view piece) noexcept
  {
    if (match(piece)) {
      advance();
      return true;
    }
    return false;
  }

  // ternary: logical_or ('?' expr ':' ternary)?
  constexpr std::expected<const_expr_result, const_expr_error> ternary()
  {
    auto cond = logical_or();
    if (!cond) return cond;

    if (consume("?")) {
      auto then_val = ternary();
      if (!then_val) return then_val;

      if (!consume(":")) {
        return std::unexpected(const_expr_error{"expected ':' in ternary expression"});
      }

      auto else_val = ternary();
      if (!else_val) return else_val;

      return const_expr_result{cond->value ? then_val->value : else_val->value};
    }

    return cond;
  }

  constexpr std::expected<const_expr_result, const_expr_error> logical_or()
  {
    auto lhs = logical_and();
    if (!lhs) return lhs;

    while (match("||") || match("or")) {
      advance();
      auto rhs = logical_and();
      if (!rhs) return rhs;
      lhs = const_expr_result{(lhs->value || rhs->value) ? 1 : 0};
    }
    return lhs;
  }

  constexpr std::expected<const_expr_result, const_expr_error> logical_and()
  {
    auto lhs = bitwise_or();
    if (!lhs) return lhs;

    while (match("&&") || match("and")) {
      advance();
      auto rhs = bitwise_or();
      if (!rhs) return rhs;
      lhs = const_expr_result{(lhs->value && rhs->value) ? 1 : 0};
    }
    return lhs;
  }

  constexpr std::expected<const_expr_result, const_expr_error> bitwise_or()
  {
    auto lhs = bitwise_xor();
    if (!lhs) return lhs;

    while (match("|") || match("bitor")) {
      advance();
      auto rhs = bitwise_xor();
      if (!rhs) return rhs;
      lhs = const_expr_result{lhs->value | rhs->value};
    }
    return lhs;
  }

  constexpr std::expected<const_expr_result, const_expr_error> bitwise_xor()
  {
    auto lhs = bitwise_and();
    if (!lhs) return lhs;

    while (match("^") || match("xor")) {
      advance();
      auto rhs = bitwise_and();
      if (!rhs) return rhs;
      lhs = const_expr_result{lhs->value ^ rhs->value};
    }
    return lhs;
  }

  constexpr std::expected<const_expr_result, const_expr_error> bitwise_and()
  {
    auto lhs = equality();
    if (!lhs) return lhs;

    while (match("&") || match("bitand")) {
      advance();
      auto rhs = equality();
      if (!rhs) return rhs;
      lhs = const_expr_result{lhs->value & rhs->value};
    }
    return lhs;
  }

  constexpr std::expected<const_expr_result, const_expr_error> equality()
  {
    auto lhs = relational();
    if (!lhs) return lhs;

    for (;;) {
      if (match("==")) {
        advance();
        auto rhs = relational();
        if (!rhs) return rhs;
        lhs = const_expr_result{(lhs->value == rhs->value) ? 1 : 0};
      } else if (match("!=") || match("not_eq")) {
        advance();
        auto rhs = relational();
        if (!rhs) return rhs;
        lhs = const_expr_result{(lhs->value != rhs->value) ? 1 : 0};
      } else {
        break;
      }
    }
    return lhs;
  }

  constexpr std::expected<const_expr_result, const_expr_error> relational()
  {
    auto lhs = shift();
    if (!lhs) return lhs;

    for (;;) {
      if (match("<")) {
        advance();
        auto rhs = shift();
        if (!rhs) return rhs;
        lhs = const_expr_result{(lhs->value < rhs->value) ? 1 : 0};
      } else if (match(">")) {
        advance();
        auto rhs = shift();
        if (!rhs) return rhs;
        lhs = const_expr_result{(lhs->value > rhs->value) ? 1 : 0};
      } else if (match("<=")) {
        advance();
        auto rhs = shift();
        if (!rhs) return rhs;
        lhs = const_expr_result{(lhs->value <= rhs->value) ? 1 : 0};
      } else if (match(">=")) {
        advance();
        auto rhs = shift();
        if (!rhs) return rhs;
        lhs = const_expr_result{(lhs->value >= rhs->value) ? 1 : 0};
      } else {
        break;
      }
    }
    return lhs;
  }

  constexpr std::expected<const_expr_result, const_expr_error> shift()
  {
    auto lhs = additive();
    if (!lhs) return lhs;

    for (;;) {
      if (match("<<")) {
        advance();
        auto rhs = additive();
        if (!rhs) return rhs;
        lhs = const_expr_result{lhs->value << rhs->value};
      } else if (match(">>")) {
        advance();
        auto rhs = additive();
        if (!rhs) return rhs;
        lhs = const_expr_result{lhs->value >> rhs->value};
      } else {
        break;
      }
    }
    return lhs;
  }

  constexpr std::expected<const_expr_result, const_expr_error> additive()
  {
    auto lhs = multiplicative();
    if (!lhs) return lhs;

    for (;;) {
      if (match("+")) {
        advance();
        auto rhs = multiplicative();
        if (!rhs) return rhs;
        lhs = const_expr_result{lhs->value + rhs->value};
      } else if (match("-")) {
        advance();
        auto rhs = multiplicative();
        if (!rhs) return rhs;
        lhs = const_expr_result{lhs->value - rhs->value};
      } else {
        break;
      }
    }
    return lhs;
  }

  constexpr std::expected<const_expr_result, const_expr_error> multiplicative()
  {
    auto lhs = unary();
    if (!lhs) return lhs;

    for (;;) {
      if (match("*")) {
        advance();
        auto rhs = unary();
        if (!rhs) return rhs;
        lhs = const_expr_result{lhs->value * rhs->value};
      } else if (match("/")) {
        advance();
        auto rhs = unary();
        if (!rhs) return rhs;
        if (rhs->value == 0) return std::unexpected(const_expr_error{"division by zero"});
        lhs = const_expr_result{lhs->value / rhs->value};
      } else if (match("%")) {
        advance();
        auto rhs = unary();
        if (!rhs) return rhs;
        if (rhs->value == 0) return std::unexpected(const_expr_error{"division by zero"});
        lhs = const_expr_result{lhs->value % rhs->value};
      } else {
        break;
      }
    }
    return lhs;
  }

  constexpr std::expected<const_expr_result, const_expr_error> unary()
  {
    if (consume("+")) {
      return unary();
    }
    if (consume("-")) {
      auto val = unary();
      if (!val) return val;
      return const_expr_result{-val->value};
    }
    if (consume("!") || consume("not")) {
      auto val = unary();
      if (!val) return val;
      return const_expr_result{val->value ? 0 : 1};
    }
    if (consume("~") || consume("compl")) {
      auto val = unary();
      if (!val) return val;
      return const_expr_result{~val->value};
    }
    return primary();
  }

  constexpr std::expected<const_expr_result, const_expr_error> primary()
  {
    auto const* tok = peek();
    if (!tok) return std::unexpected(const_expr_error{"unexpected end of expression"});

    if (tok->kind == pp_token_kind::number) {
      advance();
      return parse_number(tok->piece);
    }

    if (tok->kind == pp_token_kind::character_literal) {
      advance();
      return parse_char_literal(tok->piece);
    }

    if (tok->kind == pp_token_kind::identifier) {
      if (tok->piece == "true") {
        advance();
        return const_expr_result{1};
      }
      if (tok->piece == "false") {
        advance();
        return const_expr_result{0};
      }
      if (tok->piece == "defined") {
        return parse_defined();
      }
      // unknown identifier → 0
      advance();
      return const_expr_result{0};
    }

    if (consume("(")) {
      auto val = ternary();
      if (!val) return val;
      if (!consume(")")) {
        return std::unexpected(const_expr_error{"expected ')'"});
      }
      return val;
    }

    return std::unexpected(const_expr_error{"unexpected token"});
  }

  constexpr std::expected<const_expr_result, const_expr_error> parse_defined()
  {
    advance();  // consume 'defined'

    bool const has_paren = consume("(");

    auto const* tok = peek();
    if (!tok || tok->kind != pp_token_kind::identifier) {
      return std::unexpected(const_expr_error{"expected identifier after 'defined'"});
    }
    auto const name = tok->piece;
    advance();

    if (has_paren && !consume(")")) {
      return std::unexpected(const_expr_error{"expected ')' after 'defined(identifier'"});
    }

    return const_expr_result{defined_lookup_(name) ? 1 : 0};
  }

  static constexpr std::expected<const_expr_result, const_expr_error> parse_number(std::string_view sv)
  {
    // strip integer suffixes from end
    auto digits = strip_integer_suffix(sv);

    int base = 10;
    if (digits.size() >= 2 && digits[0] == '0') {
      if (digits[1] == 'x' || digits[1] == 'X') {
        base = 16;
        digits = digits.substr(2);
      } else if (digits[1] == 'b' || digits[1] == 'B') {
        base = 2;
        digits = digits.substr(2);
      } else {
        base = 8;
      }
    }

    // strip digit separators
    std::intmax_t value = 0;
    auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value, base);
    if (result.ec != std::errc{}) {
      return std::unexpected(const_expr_error{"invalid integer literal"});
    }
    return const_expr_result{value};
  }

  static constexpr std::string_view strip_integer_suffix(std::string_view sv) noexcept
  {
    // suffixes: combinations of u/U, l/L/ll/LL, z/Z
    auto end = sv.size();
    while (end > 0) {
      char const c = sv[end - 1];
      if (c == 'u' || c == 'U' || c == 'l' || c == 'L' || c == 'z' || c == 'Z') {
        --end;
      } else {
        break;
      }
    }
    return sv.substr(0, end);
  }

  static constexpr std::expected<const_expr_result, const_expr_error> parse_char_literal(std::string_view sv)
  {
    // strip encoding prefix
    if (sv.starts_with("u8")) {
      sv = sv.substr(2);
    } else if (sv.starts_with("u") || sv.starts_with("U") || sv.starts_with("L")) {
      sv = sv.substr(1);
    }

    // strip quotes
    sv = sv.substr(1, sv.size() - 2);

    if (sv.empty()) {
      return std::unexpected(const_expr_error{"empty character literal"});
    }

    std::intmax_t value = 0;
    std::size_t pos = 0;
    while (pos < sv.size()) {
      auto ch = parse_one_char(sv.substr(pos));
      if (!ch) return std::unexpected(ch.error());
      value = (value << 8) | (static_cast<unsigned char>(ch->value));
      pos += ch->length;
    }

    return const_expr_result{value};
  }

  static constexpr std::expected<parsed_char, const_expr_error> parse_one_char(std::string_view sv)
  {
    if (sv.empty()) return std::unexpected(const_expr_error{"unexpected end of character literal"});

    if (sv[0] != '\\') {
      return parsed_char{sv[0], 1};
    }

    if (sv.size() < 2) return std::unexpected(const_expr_error{"incomplete escape sequence"});

    switch (sv[1]) {
      case '\'':
        return parsed_char{'\'', 2};
      case '"':
        return parsed_char{'"', 2};
      case '?':
        return parsed_char{'?', 2};
      case '\\':
        return parsed_char{'\\', 2};
      case 'a':
        return parsed_char{'\a', 2};
      case 'b':
        return parsed_char{'\b', 2};
      case 'f':
        return parsed_char{'\f', 2};
      case 'n':
        return parsed_char{'\n', 2};
      case 'r':
        return parsed_char{'\r', 2};
      case 't':
        return parsed_char{'\t', 2};
      case 'v':
        return parsed_char{'\v', 2};
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7': {
        // octal: up to 3 digits
        unsigned val = static_cast<unsigned>(sv[1] - '0');
        std::size_t len = 2;
        for (std::size_t i = 2; i < sv.size() && i < 4; ++i) {
          if (sv[i] >= '0' && sv[i] <= '7') {
            val = val * 8 + static_cast<unsigned>(sv[i] - '0');
            ++len;
          } else {
            break;
          }
        }
        return parsed_char{static_cast<char>(val), len};
      }
      case 'x': {
        // hex: one or more hex digits
        std::size_t len = 2;
        unsigned val = 0;
        while (len < sv.size()) {
          char const c = sv[len];
          if (c >= '0' && c <= '9') {
            val = val * 16 + static_cast<unsigned>(c - '0');
          } else if (c >= 'a' && c <= 'f') {
            val = val * 16 + static_cast<unsigned>(c - 'a' + 10);
          } else if (c >= 'A' && c <= 'F') {
            val = val * 16 + static_cast<unsigned>(c - 'A' + 10);
          } else {
            break;
          }
          ++len;
        }
        if (len == 2) return std::unexpected(const_expr_error{"invalid hex escape"});
        return parsed_char{static_cast<char>(val), len};
      }
      default:
        return std::unexpected(const_expr_error{"unknown escape sequence"});
    }
  }
};

}  // namespace detail

template<std::predicate<std::string_view> DefinedLookup>
constexpr std::expected<const_expr_result, const_expr_error> evaluate_const_expr(std::span<pp_token const> tokens, DefinedLookup defined_lookup)
{
  detail::const_expr_evaluator evaluator{tokens, defined_lookup};
  return evaluator.evaluate();
}

}  // namespace yk::asteroid::preprocess

#endif  // YK_ASTEROID_PREPROCESS_CONST_EXPR_HPP
