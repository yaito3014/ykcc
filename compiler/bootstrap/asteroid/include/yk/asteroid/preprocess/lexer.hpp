#ifndef YK_ASTEROID_PREPROCESS_LEXER_HPP
#define YK_ASTEROID_PREPROCESS_LEXER_HPP

#include <expected>
#include <iterator>
#include <memory>
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

private:
  friend lexer;

  constexpr explicit lexer_iterator(parent_type* parent) : parent_(parent) {}

  template<bool Const_>
  friend constexpr bool operator==(lexer_iterator<Const_> const&, std::default_sentinel_t) noexcept;

  struct entry {
    pp_token token;
    std::string_view::iterator parsed_point_;
  };

  constexpr std::optional<entry> next() const noexcept
  {
    return std::nullopt;  // TODO: implement
  }

  parent_type* parent_ = nullptr;
  std::optional<entry> current_ = std::nullopt;
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
