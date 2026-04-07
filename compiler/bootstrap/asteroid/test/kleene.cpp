#include "test.hpp"

#include <yk/asteroid/core/parser/kleene.hpp>
#include <yk/asteroid/core/parser/one_of.hpp>

TEST_CASE("kleene: matches zero")
{
  auto const p = *yk::asteroid::one_of_parser{"0123456789"};
  auto const res = p("abc");
  REQUIRE(res.has_value());
  CHECK(res.value().empty());
}

TEST_CASE("kleene: matches multiple")
{
  auto const p = *yk::asteroid::one_of_parser{"0123456789"};
  auto const res = p("123abc");
  REQUIRE(res.has_value());
  REQUIRE(res.value().size() == 3);
  CHECK(res.value()[0] == '1');
  CHECK(res.value()[1] == '2');
  CHECK(res.value()[2] == '3');
}

TEST_CASE("kleene: succeeds on empty input")
{
  auto const p = *yk::asteroid::one_of_parser{"0123456789"};
  auto const res = p("");
  REQUIRE(res.has_value());
  CHECK(res.value().empty());
}
