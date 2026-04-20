#include "test.hpp"

#include <yk/asteroid/parser/one_of.hpp>

TEST_CASE("one_of: matches char in set")
{
  yk::asteroid::one_of_parser p{"aeiou"};
  auto const res = p("apple");
  REQUIRE(res.has_value());
  CHECK(res.value() == 'a');
}

TEST_CASE("one_of: fails on char not in set")
{
  yk::asteroid::one_of_parser p{"aeiou"};
  CHECK(!p("xyz").has_value());
}

TEST_CASE("one_of: fails on empty")
{
  yk::asteroid::one_of_parser p{"aeiou"};
  CHECK(!p("").has_value());
}

TEST_CASE("none_of: matches char not in set")
{
  yk::asteroid::none_of_parser p{"aeiou"};
  auto const res = p("xyz");
  REQUIRE(res.has_value());
  CHECK(res.value() == 'x');
}

TEST_CASE("none_of: fails on char in set")
{
  yk::asteroid::none_of_parser p{"aeiou"};
  CHECK(!p("apple").has_value());
}

TEST_CASE("none_of: fails on empty")
{
  yk::asteroid::none_of_parser p{"aeiou"};
  CHECK(!p("").has_value());
}
