#ifndef YK_ASTEROID_PREPROCESS_LEXER_HPP
#define YK_ASTEROID_PREPROCESS_LEXER_HPP

#include <yk/asteroid/preprocess/header_name.hpp>
#include <yk/asteroid/preprocess/identifier.hpp>
#include <yk/asteroid/preprocess/literal.hpp>
#include <yk/asteroid/preprocess/op_or_punc.hpp>
#include <yk/asteroid/preprocess/pp_number.hpp>

#include <iterator>
#include <optional>
#include <string_view>
#include <type_traits>

#include <cstddef>
#include <cstdint>

namespace yk::asteroid::preprocess {

enum class pp_token_kind : std::uint8_t {
  header_name,
  identifier,
  number,
  character_literal,
  string_literal,
  op_or_punc,
  whitespace,
  newline,
  non_whitespace_char,
};

struct source_location {
  std::uint32_t line;
  std::uint32_t column;
};

struct pp_token {
  pp_token_kind kind;
  std::string_view piece;
  source_location location;
};

class lexer;

namespace detail {

inline constexpr std::string_view whitespace_chars = " \t\v\f";

template<bool Const, class T>
using maybe_const = std::conditional_t<Const, T const, T>;

template<bool Const>
class lexer_iterator {
  using parent_type = maybe_const<Const, lexer>;

public:
  using value_type = pp_token;
  using difference_type = std::ptrdiff_t;

  constexpr lexer_iterator() noexcept = default;

  constexpr value_type const& operator*() const noexcept { return *current_; }
  constexpr value_type const* operator->() const noexcept { return &*current_; }

  constexpr void next_header_name() noexcept
  {
    auto const rest = remaining();
    if (auto res = header_name_parser{}(rest)) {
      current_ = make_token(pp_token_kind::header_name, res.value());
    } else {
      current_ = next();
    }
  }

  constexpr lexer_iterator& operator++()
  {
    current_ = next();
    return *this;
  }

  constexpr lexer_iterator operator++(int)
  {
    auto tmp = *this;
    ++*this;
    return tmp;
  }

private:
  friend lexer;

  // next() -> make_token() -> next_location() reads current_, so current_ must be initialized (to nullopt) before next() runs.
  constexpr explicit lexer_iterator(parent_type* parent) : parent_(parent) { current_ = next(); }

  template<bool Const_>
  friend constexpr bool operator==(lexer_iterator<Const_> const&, std::default_sentinel_t) noexcept;

  constexpr std::string_view remaining() const noexcept
  {
    auto const begin = current_ ? current_->piece.end() : parent_->source_.begin();
    return std::string_view(begin, parent_->source_.end());
  }

  constexpr source_location next_location() const noexcept
  {
    if (!current_) return {1, 1};
    auto const& prev = *current_;

    std::uint32_t newlines = 0;
    std::uint32_t cols_after_last_nl = 0;
    for (auto c : prev.piece) {
      if (c == '\n') {
        ++newlines;
        cols_after_last_nl = 0;
      } else {
        ++cols_after_last_nl;
      }
    }

    if (newlines > 0) return {prev.location.line + newlines, cols_after_last_nl + 1};
    return {prev.location.line, prev.location.column + static_cast<std::uint32_t>(prev.piece.size())};
  }

  constexpr pp_token make_token(pp_token_kind kind, std::string_view piece) const noexcept { return pp_token{kind, piece, next_location()}; }

  constexpr std::optional<pp_token> next() noexcept
  {
    auto const rest = remaining();
    if (rest.empty()) return std::nullopt;

    if (rest[0] == '\n') {
      return make_token(pp_token_kind::newline, rest.substr(0, 1));
    }

    if (whitespace_chars.contains(rest[0])) {
      auto const end = rest.find_first_not_of(whitespace_chars);
      auto const len = end != std::string_view::npos ? end : rest.size();
      return make_token(pp_token_kind::whitespace, rest.substr(0, len));
    }

    if (rest.starts_with("//")) {
      auto const end = rest.find('\n');
      auto const len = end != std::string_view::npos ? end : rest.size();
      return make_token(pp_token_kind::whitespace, rest.substr(0, len));
    }

    if (rest.starts_with("/*")) {
      auto const end = rest.find("*/", 2);
      if (end != std::string_view::npos) {
        return make_token(pp_token_kind::whitespace, rest.substr(0, end + 2));
      }
      return make_token(pp_token_kind::whitespace, rest);
    }

    if (auto res = char_literal_parser{}(rest)) {
      return make_token(pp_token_kind::character_literal, res.value());
    }

    if (auto res = string_literal_parser{}(rest)) {
      return make_token(pp_token_kind::string_literal, res.value());
    }

    if (auto res = identifier_parser{}(rest)) {
      auto const kind = is_alternative_token(res.value()) ? pp_token_kind::op_or_punc : pp_token_kind::identifier;
      return make_token(kind, res.value());
    }

    if (auto res = pp_number_parser{}(rest)) {
      return make_token(pp_token_kind::number, res.value());
    }

    if (auto res = op_or_punc_parser{}(rest)) {
      return make_token(pp_token_kind::op_or_punc, res.value());
    }

    return make_token(pp_token_kind::non_whitespace_char, rest.substr(0, 1));
  }

  parent_type* parent_ = nullptr;
  std::optional<pp_token> current_ = std::nullopt;
};

template<bool Const>
constexpr bool operator==(lexer_iterator<Const> const& it, std::default_sentinel_t) noexcept
{
  return !it.current_.has_value();
}

}  // namespace detail

class lexer {
public:
  using iterator = detail::lexer_iterator</* Const = */ false>;
  using const_iterator = detail::lexer_iterator</* Const = */ true>;

  constexpr explicit lexer(std::string_view source) noexcept : source_(source) {}

  constexpr iterator begin() noexcept { return iterator{this}; }
  constexpr const_iterator begin() const noexcept { return const_iterator{this}; }

  constexpr std::default_sentinel_t end() const noexcept { return {}; }

private:
  friend iterator;
  friend const_iterator;

  std::string_view source_;
};

}  // namespace yk::asteroid::preprocess

#endif  // YK_ASTEROID_PREPROCESS_LEXER_HPP
