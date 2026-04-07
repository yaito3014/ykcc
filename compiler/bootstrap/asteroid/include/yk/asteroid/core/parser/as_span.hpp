#ifndef YK_ASTEROID_CORE_PARSER_AS_SPAN_HPP
#define YK_ASTEROID_CORE_PARSER_AS_SPAN_HPP

#include <yk/asteroid/core/parser/common.hpp>

#include <string_view>
#include <type_traits>
#include <utility>

namespace yk::asteroid {

template<parser Subject>
class as_span_parser {
public:
  template<class SubjectT>
  constexpr as_span_parser(SubjectT&& subject) noexcept(std::is_nothrow_constructible_v<Subject, SubjectT>) : subject_(std::forward<SubjectT>(subject))
  {
  }

  constexpr parser_result<std::string_view> operator()(std::string_view sv) const
  {
    if (auto res = subject_(sv)) {
      return {std::string_view(sv.begin(), res.parsed_point()), res.parsed_point()};
    }
    return parse_failure;
  }

private:
  [[no_unique_address]] Subject subject_;
};

template<class SubjectT>
  requires parser<std::remove_cvref_t<SubjectT>>
as_span_parser(SubjectT&&) -> as_span_parser<std::remove_cvref_t<SubjectT>>;

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_CORE_PARSER_AS_SPAN_HPP
