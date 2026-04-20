#include "test.hpp"

#include <yk/asteroid/parser/negation.hpp>

#include <variant>

TEST_CASE("negation: fails when subject matches")
{
  yk::asteroid::negation_parser neg{alphabet_parser{}};
  auto const res = neg("abc");
  CHECK(!res.has_value());
}

TEST_CASE("negation: succeeds when subject fails")
{
  yk::asteroid::negation_parser neg{alphabet_parser{}};
  auto const res = neg("123");
  REQUIRE(res.has_value());
  CHECK(res.parsed_point() == std::string_view("123").begin());
}

TEST_CASE("negation: succeeds on empty input when subject fails")
{
  yk::asteroid::negation_parser neg{alphabet_parser{}};
  auto const res = neg("");
  REQUIRE(res.has_value());
}

TEST_CASE("negation: does not consume input")
{
  yk::asteroid::negation_parser neg{alphabet_parser{}};
  std::string_view input = "123abc";
  auto const res = neg(input);
  REQUIRE(res.has_value());
  CHECK(res.parsed_point() == input.begin());
}

TEST_CASE("negation: operator!")
{
  auto const neg = yk::asteroid::negation_parser{alphabet_parser{}};
  CHECK(!neg("abc").has_value());
  REQUIRE(neg("123").has_value());
}
