#include "test.hpp"

#include <yk/asteroid/core/parser/satisfy.hpp>

TEST_CASE("satisfy: matches when predicate true")
{
  yk::asteroid::satisfy_parser p{[](char c) { return c >= '0' && c <= '9'; }};
  auto const res = p("42");
  REQUIRE(res.has_value());
  CHECK(res.value() == '4');
}

TEST_CASE("satisfy: fails when predicate false")
{
  yk::asteroid::satisfy_parser p{[](char c) { return c >= '0' && c <= '9'; }};
  CHECK(!p("abc").has_value());
}

TEST_CASE("satisfy: fails on empty")
{
  yk::asteroid::satisfy_parser p{[](char c) { return c >= '0' && c <= '9'; }};
  CHECK(!p("").has_value());
}
