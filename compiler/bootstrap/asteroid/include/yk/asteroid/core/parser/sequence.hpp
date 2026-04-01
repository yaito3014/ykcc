#ifndef YK_ASTEROID_CORE_PARSER_SEQUENCE_HPP
#define YK_ASTEROID_CORE_PARSER_SEQUENCE_HPP

#include <yk/asteroid/core/parser/common.hpp>

#include <tuple>
#include <utility>

namespace yk::asteroid {

template<class... Ts>
class sequence_result : private std::tuple<Ts...> {
  using base_type = std::tuple<Ts...>;

public:
  constexpr base_type& base() & { return *this; }
  constexpr base_type const& base() const& { return *this; }
  constexpr base_type&& base() && { return std::move(*this); }
  constexpr base_type const&& base() const&& { return std::move(*this); }

  using base_type::base_type;
};

template<std::size_t I, class... Ts>
constexpr std::tuple_element_t<I, std::tuple<Ts...>>& get(sequence_result<Ts...>& seq_res) noexcept
{
  return std::get<I>(seq_res.base());
}

template<std::size_t I, class... Ts>
constexpr std::tuple_element_t<I, std::tuple<Ts...>> const& get(sequence_result<Ts...> const& seq_res) noexcept
{
  return std::get<I>(seq_res.base());
}

template<std::size_t I, class... Ts>
constexpr std::tuple_element_t<I, std::tuple<Ts...>>&& get(sequence_result<Ts...>&& seq_res) noexcept
{
  return std::get<I>(std::move(seq_res).base());
}

template<std::size_t I, class... Ts>
constexpr std::tuple_element_t<I, std::tuple<Ts...>> const&& get(sequence_result<Ts...> const&& seq_res) noexcept
{
  return std::get<I>(std::move(seq_res).base());
}

namespace detail {

template<class T, class U>
struct merge_sequence_result_t_impl {
  using type = sequence_result<T, U>;
};

template<class T, class... Us>
struct merge_sequence_result_t_impl<T, sequence_result<Us...>> {
  using type = sequence_result<T, Us...>;
};

template<class... Ts, class U>
struct merge_sequence_result_t_impl<sequence_result<Ts...>, U> {
  using type = sequence_result<Ts..., U>;
};

template<class... Ts, class... Us>
struct merge_sequence_result_t_impl<sequence_result<Ts...>, sequence_result<Us...>> {
  using type = sequence_result<Ts..., Us...>;
};

template<class T, class U>
using merge_sequence_result_t = detail::merge_sequence_result_t_impl<T, U>::type;

template<class T>
struct wrap_into_sequence_result_impl {
  static constexpr sequence_result<T> apply(T const& t) noexcept(std::is_nothrow_copy_constructible_v<T>) { return sequence_result<T>{t}; }
  static constexpr sequence_result<T> apply(T&& t) noexcept(std::is_nothrow_move_constructible_v<T>) { return sequence_result<T>{std::move(t)}; }
};

template<class... Ts>
struct wrap_into_sequence_result_impl<sequence_result<Ts...>> {
  static constexpr sequence_result<Ts...> const& apply(sequence_result<Ts...> const& sr) noexcept { return sr; }
  static constexpr sequence_result<Ts...> apply(sequence_result<Ts...>&& sr) noexcept(std::is_move_constructible_v<sequence_result<Ts...>>) { return sr; }
};

template<class T>
constexpr decltype(auto) wrap_into_sequence_result(T&& t) noexcept(noexcept(wrap_into_sequence_result_impl<std::remove_cvref_t<T>>::apply(std::forward<T>(t))))
{
  return wrap_into_sequence_result_impl<std::remove_cvref_t<T>>::apply(std::forward<T>(t));
}

template<class... Ts>
constexpr sequence_result<Ts...> sequence_result_from_tuple(std::tuple<Ts...> const& t) noexcept(noexcept(std::make_from_tuple<sequence_result<Ts...>>(t)))
{
  return std::make_from_tuple<sequence_result<Ts...>>(t);
}

template<class... Ts>
constexpr sequence_result<Ts...> sequence_result_from_tuple(std::tuple<Ts...>&& t) noexcept(
    noexcept(std::make_from_tuple<sequence_result<Ts...>>(std::move(t)))
)
{
  return std::make_from_tuple<sequence_result<Ts...>>(std::move(t));
}

template<class T, class U>
constexpr auto merge_sequence_result(T&& t, U&& u) noexcept(noexcept(detail::sequence_result_from_tuple(
    std::tuple_cat(detail::wrap_into_sequence_result(std::forward<T>(t)).base(), detail::wrap_into_sequence_result(std::forward<U>(u)).base())
)))
{
  return detail::sequence_result_from_tuple(
      std::tuple_cat(detail::wrap_into_sequence_result(std::forward<T>(t)).base(), detail::wrap_into_sequence_result(std::forward<U>(u)).base())
  );
}

}  // namespace detail

template<parser LeftParser, parser RightParser>
class sequence {
public:
  using value_type = detail::merge_sequence_result_t<parser_value_t<LeftParser>, parser_value_t<RightParser>>;

  template<class LeftParserT, class RightParserT>
  constexpr sequence(
      LeftParserT&& left, RightParserT&& right
  ) noexcept(std::conjunction_v<std::is_nothrow_constructible<LeftParser, LeftParserT>, std::is_nothrow_constructible<RightParser, RightParserT>>)
      : left_(std::forward<LeftParserT>(left)), right_(std::forward<RightParserT>(right))
  {
  }

  constexpr parser_result<value_type> operator()(std::string_view sv) const noexcept(
      noexcept(detail::merge_sequence_result(std::declval<parser_value_t<LeftParser>>(), std::declval<parser_value_t<RightParser>>()))
  )
  {
    if (auto const left_result = left_(sv)) {
      if (auto const right_result = right_(std::string_view(left_result.parsed_point(), sv.end()))) {
        return {detail::merge_sequence_result(left_result.value(), right_result.value()), right_result.parsed_point()};
      } else {
        return parse_failure;
      }
    } else {
      return parse_failure;
    }
  }

private:
  [[no_unique_address]] LeftParser left_;
  [[no_unique_address]] RightParser right_;
};

template<parser LeftParserT, parser RightParserT>
sequence(LeftParserT&&, RightParserT&&) -> sequence<std::remove_cvref_t<LeftParserT>, std::remove_cvref_t<RightParserT>>;

}  // namespace yk::asteroid

template<class... Ts>
struct std::tuple_size<yk::asteroid::sequence_result<Ts...>> : tuple_size<std::tuple<Ts...>> {};

template<std::size_t I, class... Ts>
struct std::tuple_element<I, yk::asteroid::sequence_result<Ts...>> : tuple_element<I, std::tuple<Ts...>> {};

#endif  // YK_ASTEROID_CORE_PARSER_SEQUENCE_HPP
