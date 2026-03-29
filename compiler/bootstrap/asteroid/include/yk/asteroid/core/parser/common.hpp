#ifndef YK_ASTEROID_CORE_PARSER_COMMON_HPP
#define YK_ASTEROID_CORE_PARSER_COMMON_HPP

#include <concepts>
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>

namespace yk::asteroid {

class bad_parser_result_access : public std::exception {
  virtual char const* what() const noexcept override { return "bad parser_result access"; }
};

template<class T>
class parser_result {
public:
  using value_type = T;

  constexpr parser_result(T const& val, std::string_view rest) : val_(val), rest_(rest) {}
  constexpr parser_result(T&& val, std::string_view rest) : val_(std::move(val)), rest_(rest) {}

  constexpr parser_result(std::string_view rest) : rest_(rest) {}

  constexpr value_type const& operator*() const& noexcept { return *val_; }
  constexpr value_type&& operator*() && noexcept { return *std::move(val_); }

  constexpr value_type const* operator->() const noexcept { return val_.operator->(); }

  constexpr value_type const& value() const& { return has_value() ? *val_ : throw bad_parser_result_access{}; }
  constexpr value_type&& value() && { return has_value() ? *std::move(val_) : throw bad_parser_result_access{}; }

  constexpr std::string_view rest() const noexcept { return rest_; }

  constexpr bool has_value() const noexcept { return val_.has_value(); }
  constexpr explicit operator bool() const noexcept { return has_value(); }

private:
  std::optional<T> val_;
  std::string_view rest_;
};

namespace detail {

template<class T, template<class...> class TT>
struct is_specialization_of : std::false_type {};

template<template<class...> class TT, class... Ts>
struct is_specialization_of<TT<Ts...>, TT> : std::true_type {};

template<class T, template<class...> class TT>
concept specialization_of = is_specialization_of<T, TT>::value;

}  // namespace detail

template<class T>
concept parser = requires(T t, std::string_view sv) {
  { t(sv) } -> detail::specialization_of<parser_result>;
};

template<parser P>
using parser_value_t = std::invoke_result_t<P, std::string_view>::value_type;

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_CORE_PARSER_COMMON_HPP
