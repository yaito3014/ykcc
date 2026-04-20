#ifndef YK_ASTEROID_CORE_PARSER_ALTERNATIVE_HPP
#define YK_ASTEROID_CORE_PARSER_ALTERNATIVE_HPP

#include <yk/asteroid/parser/common.hpp>

#include <type_traits>
#include <utility>
#include <variant>

namespace yk::asteroid {

namespace detail {

template<class L, class R>
struct alternative_value {
  using type = std::variant<L, R>;
};

template<class T>
struct alternative_value<T, T> {
  using type = T;
};

template<class L, class R>
using alternative_value_t = alternative_value<L, R>::type;

}  // namespace detail

template<parser LeftParser, parser RightParser>
class alternative_parser {
public:
  using value_type = detail::alternative_value_t<parser_value_t<LeftParser>, parser_value_t<RightParser>>;

  template<class LeftParserT, class RightParserT>
  constexpr alternative_parser(
      LeftParserT&& left, RightParserT&& right
  ) noexcept(std::conjunction_v<std::is_nothrow_constructible<LeftParser, LeftParserT>, std::is_nothrow_constructible<RightParser, RightParserT>>)
      : left_(std::forward<LeftParserT>(left)), right_(std::forward<RightParserT>(right))
  {
  }

  constexpr parser_result<value_type> operator()(std::string_view sv) const
  {
    if (auto left_result = left_(sv)) {
      return {wrap_into_value_type(std::move(left_result).value()), left_result.parsed_point()};
    }
    if (auto right_result = right_(sv)) {
      return {wrap_into_value_type(std::move(right_result).value()), right_result.parsed_point()};
    }
    return parse_failure;
  }

private:
  template<class T>
  static constexpr decltype(auto) wrap_into_value_type(T&& x)
  {
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, value_type>) {
      return std::forward<T>(x);
    } else {
      return value_type(std::forward<T>(x));
    }
  }

  [[no_unique_address]] LeftParser left_;
  [[no_unique_address]] RightParser right_;
};

template<class LeftParserT, class RightParserT>
  requires parser<std::remove_cvref_t<LeftParserT>> && parser<std::remove_cvref_t<RightParserT>>
alternative_parser(LeftParserT&&, RightParserT&&) -> alternative_parser<std::remove_cvref_t<LeftParserT>, std::remove_cvref_t<RightParserT>>;

inline namespace parser_ops {

template<class LeftParserT, class RightParserT>
  requires parser<std::remove_cvref_t<LeftParserT>> && parser<std::remove_cvref_t<RightParserT>>
constexpr alternative_parser<std::remove_cvref_t<LeftParserT>, std::remove_cvref_t<RightParserT>> operator|(LeftParserT&& left, RightParserT&& right)
{
  return {std::forward<LeftParserT>(left), std::forward<RightParserT>(right)};
}

}  // namespace parser_ops

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_CORE_PARSER_ALTERNATIVE_HPP
