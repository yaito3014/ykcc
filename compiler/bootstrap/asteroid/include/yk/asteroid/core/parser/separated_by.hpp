#ifndef YK_ASTEROID_CORE_PARSER_SEPARATED_BY_HPP
#define YK_ASTEROID_CORE_PARSER_SEPARATED_BY_HPP

#include <yk/asteroid/core/parser/common.hpp>

#include <type_traits>
#include <utility>
#include <vector>

namespace yk::asteroid {

template<parser Subject, parser Separator>
class separated_by {
public:
  using value_type = std::vector<parser_value_t<Subject>>;

  template<class SubjectT, class SeparatorT>
  constexpr separated_by(
      SubjectT&& main, SeparatorT&& sub
  ) noexcept(std::conjunction_v<std::is_nothrow_constructible<Subject, SubjectT>, std::is_nothrow_constructible<Separator, SeparatorT>>)
      : subject_(std::forward<SubjectT>(main)), separator_(std::forward<SeparatorT>(sub))
  {
  }

  constexpr parser_result<value_type> operator()(std::string_view sv) const  // TODO: add noexcept
  {
    if (auto const first = subject_(sv)) {
      std::vector<parser_value_t<Subject>> result{first.value()};
      std::string_view rest = first.rest();
      while (true) {
        if (auto const sep = separator_(rest)) {
          if (auto const elem = subject_(sep.rest())) {
            result.push_back(std::move(elem).value());
            rest = elem.rest();
          } else {
            return {elem.rest()};
          }
        } else {
          return {result, sep.rest()};
        }
      }
    } else {
      return {sv};
    }
  }

private:
  [[no_unique_address]] Subject subject_;
  [[no_unique_address]] Separator separator_;
};

template<parser SubjectT, parser SeparatorT>
separated_by(SubjectT&&, SeparatorT&&) -> separated_by<std::remove_cvref_t<SubjectT>, std::remove_cvref_t<SeparatorT>>;

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_CORE_PARSER_SEPARATED_BY_HPP
