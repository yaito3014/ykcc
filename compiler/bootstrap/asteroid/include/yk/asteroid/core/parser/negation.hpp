#ifndef YK_ASTEROID_CORE_PARSER_NEGATION_HPP
#define YK_ASTEROID_CORE_PARSER_NEGATION_HPP

#include <yk/asteroid/core/parser/common.hpp>

#include <string_view>
#include <type_traits>
#include <utility>

namespace yk::asteroid {

template<parser Subject>
class negation_parser {
public:
  template<class SubjectT>
  constexpr negation_parser(SubjectT&& subject) noexcept(std::is_nothrow_constructible_v<Subject, SubjectT>) : subject_(std::forward<SubjectT>(subject))
  {
  }

  constexpr parser_result<std::monostate> operator()(std::string_view sv) const
  {
    if (subject_(sv)) return parse_failure;
    return {std::monostate{}, sv.begin()};
  }

private:
  [[no_unique_address]] Subject subject_;
};

template<class SubjectT>
  requires parser<std::remove_cvref_t<SubjectT>>
negation_parser(SubjectT&&) -> negation_parser<std::remove_cvref_t<SubjectT>>;

inline namespace parser_ops {

template<class SubjectT>
  requires parser<std::remove_cvref_t<SubjectT>>
constexpr negation_parser<std::remove_cvref_t<SubjectT>> operator!(SubjectT&& subject)
{
  return {std::forward<SubjectT>(subject)};
}

}  // namespace parser_ops

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_CORE_PARSER_NEGATION_HPP
