#ifndef YK_ASTEROID_CORE_PARSER_COMMON_HPP
#define YK_ASTEROID_CORE_PARSER_COMMON_HPP

#include <concepts>
#include <exception>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace yk::asteroid {

class bad_parser_result_access : public std::exception {
  virtual char const* what() const noexcept override { return "bad parser_result access"; }
};

struct parse_failure_t {
  constexpr explicit parse_failure_t() noexcept = default;
};

inline constexpr parse_failure_t parse_failure{};

template<class T>
class parser_result {
  struct parsed {
    T val;
    std::string_view::iterator parsed_point;
  };

public:
  using value_type = T;

  constexpr parser_result(T const& val, std::string_view::iterator parsed_point) : result_(parsed{val, parsed_point}) {}
  constexpr parser_result(T&& val, std::string_view::iterator parsed_point) : result_(parsed{std::move(val), parsed_point}) {}

  constexpr parser_result(parse_failure_t) noexcept {}

  constexpr value_type const& operator*() const& noexcept { return result_->val; }
  constexpr value_type&& operator*() && noexcept { return std::move(result_->val); }

  constexpr value_type const* operator->() const noexcept { return std::addressof(result_->val); }

  constexpr value_type const& value() const& { return has_value() ? result_->val : throw bad_parser_result_access{}; }
  constexpr value_type&& value() && { return has_value() ? std::move(result_->val) : throw bad_parser_result_access{}; }

  constexpr std::string_view::iterator parsed_point() const { return has_value() ? result_->parsed_point : throw bad_parser_result_access{}; }

  constexpr bool has_value() const noexcept { return result_.has_value(); }
  constexpr explicit operator bool() const noexcept { return has_value(); }

private:
  std::optional<parsed> result_;
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
