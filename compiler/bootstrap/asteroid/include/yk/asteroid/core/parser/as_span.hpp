#ifndef YK_ASTEROID_CORE_PARSER_AS_SPAN_HPP
#define YK_ASTEROID_CORE_PARSER_AS_SPAN_HPP

#include <yk/asteroid/core/parser/common.hpp>

#include <string_view>
#include <type_traits>
#include <utility>

namespace yk::asteroid {

template<parser P>
class as_span_parser {
public:
  template<class PT>
  constexpr as_span_parser(PT&& inner) noexcept(std::is_nothrow_constructible_v<P, PT>) : inner_(std::forward<PT>(inner))
  {
  }

  constexpr parser_result<std::string_view> operator()(std::string_view sv) const
  {
    if (auto res = inner_(sv)) {
      return {std::string_view(sv.begin(), res.parsed_point()), res.parsed_point()};
    }
    return parse_failure;
  }

private:
  [[no_unique_address]] P inner_;
};

template<class PT>
  requires parser<std::remove_cvref_t<PT>>
as_span_parser(PT&&) -> as_span_parser<std::remove_cvref_t<PT>>;

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_CORE_PARSER_AS_SPAN_HPP
