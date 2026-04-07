#include "test.hpp"

#include <yk/asteroid/core/parser/as_span.hpp>
#include <yk/asteroid/core/parser/one_of.hpp>
#include <yk/asteroid/core/parser/plus.hpp>

TEST_CASE("as_span: converts result to string_view")
{
  auto const p = yk::asteroid::as_span_parser{+yk::asteroid::one_of_parser{"0123456789"}};
  auto const res = p("123abc");
  REQUIRE(res.has_value());
  CHECK(res.value() == "123");
}

TEST_CASE("as_span: fails when subject fails")
{
  auto const p = yk::asteroid::as_span_parser{+yk::asteroid::one_of_parser{"0123456789"}};
  CHECK(!p("abc").has_value());
}

TEST_CASE("as_span: parsed_point advances correctly")
{
  auto const p = yk::asteroid::as_span_parser{+yk::asteroid::one_of_parser{"0123456789"}};
  std::string_view input = "123abc";
  auto const res = p(input);
  REQUIRE(res.has_value());
  CHECK(res.parsed_point() == input.begin() + 3);
}
