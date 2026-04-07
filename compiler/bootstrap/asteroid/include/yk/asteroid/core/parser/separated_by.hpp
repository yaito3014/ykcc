#ifndef YK_ASTEROID_CORE_PARSER_SEPARATED_BY_HPP
#define YK_ASTEROID_CORE_PARSER_SEPARATED_BY_HPP

#include <yk/asteroid/core/parser/common.hpp>

#include <type_traits>
#include <utility>
#include <vector>

namespace yk::asteroid {

template<parser Subject, parser Separator>
class separated_by_parser {
public:
  using value_type = std::vector<parser_value_t<Subject>>;

  template<class SubjectT, class SeparatorT>
  constexpr separated_by_parser(
      SubjectT&& main, SeparatorT&& sub
  ) noexcept(std::conjunction_v<std::is_nothrow_constructible<Subject, SubjectT>, std::is_nothrow_constructible<Separator, SeparatorT>>)
      : subject_(std::forward<SubjectT>(main)), separator_(std::forward<SeparatorT>(sub))
  {
  }

  constexpr parser_result<value_type> operator()(std::string_view sv) const  // TODO: add noexcept
  {
    if (auto const first = subject_(sv)) {
      std::vector<parser_value_t<Subject>> result{first.value()};
      std::string_view::iterator parsed_to = first.parsed_point();
      while (true) {
        if (auto const sep = separator_(std::string_view(parsed_to, sv.end()))) {
          if (auto const elem = subject_(std::string_view(sep.parsed_point(), sv.end()))) {
            result.emplace_back(std::move(elem).value());
            parsed_to = elem.parsed_point();
          } else {
            return parse_failure;
          }
        } else {
          return {result, parsed_to};
        }
      }
    } else {
      return parse_failure;
    }
  }

private:
  [[no_unique_address]] Subject subject_;
  [[no_unique_address]] Separator separator_;
};

template<class SubjectT, class SeparatorT>
  requires parser<std::remove_cvref_t<SubjectT>> && parser<std::remove_cvref_t<SeparatorT>>
separated_by_parser(SubjectT&&, SeparatorT&&) -> separated_by_parser<std::remove_cvref_t<SubjectT>, std::remove_cvref_t<SeparatorT>>;

inline namespace parser_ops {

template<class SubjectT, class SeparatorT>
  requires parser<std::remove_cvref_t<SubjectT>> && parser<std::remove_cvref_t<SeparatorT>>
constexpr separated_by_parser<std::remove_cvref_t<SubjectT>, std::remove_cvref_t<SeparatorT>> operator%(SubjectT&& subject, SeparatorT&& separator)
{
  return {std::forward<SubjectT>(subject), std::forward<SeparatorT>(separator)};
}

}  // namespace parser_ops

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_CORE_PARSER_SEPARATED_BY_HPP
