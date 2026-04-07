#ifndef YK_ASTEROID_CORE_PARSER_SURROUNDED_BY_HPP
#define YK_ASTEROID_CORE_PARSER_SURROUNDED_BY_HPP

#include <yk/asteroid/core/parser/common.hpp>

#include <string_view>
#include <type_traits>
#include <utility>

namespace yk::asteroid {

template<parser Open, parser Subject, parser Close>
class surrounded_by_parser {
public:
  using value_type = parser_value_t<Subject>;

  template<class OpenT, class SubjectT, class CloseT>
  constexpr surrounded_by_parser(OpenT&& open, SubjectT&& subject, CloseT&& close) noexcept(
      std::conjunction_v<
          std::is_nothrow_constructible<Open, OpenT>, std::is_nothrow_constructible<Subject, SubjectT>, std::is_nothrow_constructible<Close, CloseT>>
  )
      : open_(std::forward<OpenT>(open)), subject_(std::forward<SubjectT>(subject)), close_(std::forward<CloseT>(close))
  {
  }

  constexpr parser_result<value_type> operator()(std::string_view sv) const
  {
    auto const open_result = open_(sv);
    if (!open_result) return parse_failure;

    auto subject_result = subject_(std::string_view(open_result.parsed_point(), sv.end()));
    if (!subject_result) return parse_failure;

    auto const close_result = close_(std::string_view(subject_result.parsed_point(), sv.end()));
    if (!close_result) return parse_failure;

    return {std::move(subject_result).value(), close_result.parsed_point()};
  }

private:
  [[no_unique_address]] Open open_;
  [[no_unique_address]] Subject subject_;
  [[no_unique_address]] Close close_;
};

template<class OpenT, class SubjectT, class CloseT>
  requires parser<std::remove_cvref_t<OpenT>> && parser<std::remove_cvref_t<SubjectT>> && parser<std::remove_cvref_t<CloseT>>
surrounded_by_parser(OpenT&&, SubjectT&&, CloseT&&)
    -> surrounded_by_parser<std::remove_cvref_t<OpenT>, std::remove_cvref_t<SubjectT>, std::remove_cvref_t<CloseT>>;

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_CORE_PARSER_SURROUNDED_BY_HPP
