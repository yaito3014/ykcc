#ifndef YK_ASTEROID_CORE_PARSER_OPTIONAL_HPP
#define YK_ASTEROID_CORE_PARSER_OPTIONAL_HPP

#include <yk/asteroid/parser/common.hpp>

#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace yk::asteroid {

template<parser Subject>
class optional_parser {
public:
  using value_type = std::optional<parser_value_t<Subject>>;

  template<class SubjectT>
  constexpr optional_parser(SubjectT&& subject) noexcept(std::is_nothrow_constructible_v<Subject, SubjectT>) : subject_(std::forward<SubjectT>(subject))
  {
  }

  constexpr parser_result<value_type> operator()(std::string_view sv) const
  {
    if (auto res = subject_(sv)) {
      return {std::optional{std::move(res).value()}, res.parsed_point()};
    }
    return {std::optional<parser_value_t<Subject>>{std::nullopt}, sv.begin()};
  }

private:
  [[no_unique_address]] Subject subject_;
};

template<class SubjectT>
  requires parser<std::remove_cvref_t<SubjectT>>
optional_parser(SubjectT) -> optional_parser<std::remove_cvref_t<SubjectT>>;

inline namespace parser_ops {

template<class SubjectT>
  requires parser<std::remove_cvref_t<SubjectT>>
constexpr optional_parser<std::remove_cvref_t<SubjectT>> operator-(SubjectT&& subject)
{
  return {std::forward<SubjectT>(subject)};
}

}  // namespace parser_ops

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_CORE_PARSER_OPTIONAL_HPP
