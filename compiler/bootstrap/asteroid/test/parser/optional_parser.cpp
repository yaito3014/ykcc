#include "test.hpp"

#include <yk/asteroid/parser/optional.hpp>

TEST_CASE("optional: returns value when subject matches")
{
  auto const p = yk::asteroid::optional_parser{alphabet_parser{}};
  auto const res = p("abc123");
  REQUIRE(res.has_value());
  REQUIRE(res.value().has_value());
  CHECK(*res.value() == "abc");
}

TEST_CASE("optional: returns nullopt when subject fails")
{
  auto const p = yk::asteroid::optional_parser{alphabet_parser{}};
  auto const res = p("123");
  REQUIRE(res.has_value());
  CHECK(!res.value().has_value());
  CHECK(res.parsed_point() == std::string_view("123").begin());
}

TEST_CASE("optional: returns nullopt on empty input")
{
  auto const p = yk::asteroid::optional_parser{alphabet_parser{}};
  auto const res = p("");
  REQUIRE(res.has_value());
  CHECK(!res.value().has_value());
}
