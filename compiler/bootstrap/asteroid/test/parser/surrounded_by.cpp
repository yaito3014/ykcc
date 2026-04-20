#include "test.hpp"

#include <yk/asteroid/parser/literal.hpp>
#include <yk/asteroid/parser/plus.hpp>
#include <yk/asteroid/parser/one_of.hpp>
#include <yk/asteroid/parser/surrounded_by.hpp>

TEST_CASE("surrounded_by: matches open subject close")
{
  auto const p = yk::asteroid::surrounded_by_parser{
      yk::asteroid::literal_string_parser{"("},
      +yk::asteroid::one_of_parser{"0123456789"},
      yk::asteroid::literal_string_parser{")"},
  };
  auto const res = p("(123)rest");
  REQUIRE(res.has_value());
  REQUIRE(res.value().size() == 3);
  CHECK(res.value()[0] == '1');
}

TEST_CASE("surrounded_by: fails when open missing")
{
  auto const p = yk::asteroid::surrounded_by_parser{
      yk::asteroid::literal_string_parser{"("},
      +yk::asteroid::one_of_parser{"0123456789"},
      yk::asteroid::literal_string_parser{")"},
  };
  CHECK(!p("123)").has_value());
}

TEST_CASE("surrounded_by: fails when close missing")
{
  auto const p = yk::asteroid::surrounded_by_parser{
      yk::asteroid::literal_string_parser{"("},
      +yk::asteroid::one_of_parser{"0123456789"},
      yk::asteroid::literal_string_parser{")"},
  };
  CHECK(!p("(123").has_value());
}

TEST_CASE("surrounded_by: fails when subject fails")
{
  auto const p = yk::asteroid::surrounded_by_parser{
      yk::asteroid::literal_string_parser{"("},
      +yk::asteroid::one_of_parser{"0123456789"},
      yk::asteroid::literal_string_parser{")"},
  };
  CHECK(!p("(abc)").has_value());
}
