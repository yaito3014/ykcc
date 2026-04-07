#include "test.hpp"

#include <yk/asteroid/core/parser/separated_by.hpp>

struct comma_parser {
  constexpr yk::asteroid::parser_result<char> operator()(std::string_view sv) const noexcept
  {
    if (sv.starts_with(',')) {
      return {',', sv.begin() + 1};
    } else {
      return yk::asteroid::parse_failure;
    }
  }
};

TEST_CASE("separated_by")
{
  yk::asteroid::parser auto subject = numeric_parser<int>{};
  yk::asteroid::parser auto comma = comma_parser{};
  yk::asteroid::separated_by_parser sep_by{subject, comma};
  auto const res = sep_by("12,345,6789");
  REQUIRE(res.has_value());
  auto const val = *res;
  CHECK(val.at(0) == 12);
  CHECK(val.at(1) == 345);
  CHECK(val.at(2) == 6789);
}
