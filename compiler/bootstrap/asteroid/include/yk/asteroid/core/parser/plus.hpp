#ifndef YK_ASTEROID_CORE_PARSER_PLUS_HPP
#define YK_ASTEROID_CORE_PARSER_PLUS_HPP

#include <yk/asteroid/core/parser/common.hpp>

#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace yk::asteroid {

template<parser Subject>
class plus_parser {
public:
  using value_type = std::vector<parser_value_t<Subject>>;

  template<class SubjectT>
  constexpr plus_parser(SubjectT&& subject) noexcept(std::is_nothrow_constructible_v<Subject, SubjectT>) : subject_(std::forward<SubjectT>(subject))
  {
  }

  constexpr parser_result<value_type> operator()(std::string_view sv) const
  {
    auto first = subject_(sv);
    if (!first) return parse_failure;

    std::vector<parser_value_t<Subject>> result;
    result.emplace_back(std::move(first).value());
    auto parsed_to = first.parsed_point();

    while (auto res = subject_(std::string_view(parsed_to, sv.end()))) {
      result.emplace_back(std::move(res).value());
      parsed_to = res.parsed_point();
    }

    return {std::move(result), parsed_to};
  }

private:
  [[no_unique_address]] Subject subject_;
};

template<class SubjectT>
  requires parser<std::remove_cvref_t<SubjectT>>
plus_parser(SubjectT) -> plus_parser<std::remove_cvref_t<SubjectT>>;

inline namespace parser_ops {

template<class SubjectT>
  requires parser<std::remove_cvref_t<SubjectT>>
constexpr plus_parser<std::remove_cvref_t<SubjectT>> operator+(SubjectT&& subject)
{
  return {std::forward<SubjectT>(subject)};
}

}  // namespace parser_ops

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_CORE_PARSER_PLUS_HPP
