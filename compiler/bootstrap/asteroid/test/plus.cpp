#include "test.hpp"

#include <yk/asteroid/core/parser/one_of.hpp>
#include <yk/asteroid/core/parser/plus.hpp>

TEST_CASE("plus: matches one or more")
{
  auto const p = +yk::asteroid::one_of_parser{"0123456789"};
  auto const res = p("123abc");
  REQUIRE(res.has_value());
  REQUIRE(res.value().size() == 3);
  CHECK(res.value()[0] == '1');
  CHECK(res.value()[1] == '2');
  CHECK(res.value()[2] == '3');
}

TEST_CASE("plus: fails on zero matches")
{
  auto const p = +yk::asteroid::one_of_parser{"0123456789"};
  CHECK(!p("abc").has_value());
}

TEST_CASE("plus: fails on empty")
{
  auto const p = +yk::asteroid::one_of_parser{"0123456789"};
  CHECK(!p("").has_value());
}
