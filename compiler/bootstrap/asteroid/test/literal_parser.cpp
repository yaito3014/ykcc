#include "test.hpp"

#include <yk/asteroid/core/parser/literal.hpp>

TEST_CASE("any_char_parser: matches single char")
{
  yk::asteroid::any_char_parser p{};
  auto const res = p("abc");
  REQUIRE(res.has_value());
  CHECK(res.value() == 'a');
  CHECK(res.parsed_point() == std::string_view("abc").begin() + 1);
}

TEST_CASE("any_char_parser: fails on empty")
{
  yk::asteroid::any_char_parser p{};
  CHECK(!p("").has_value());
}

TEST_CASE("literal_string_parser: matches prefix")
{
  yk::asteroid::literal_string_parser p{"hello"};
  auto const res = p("hello world");
  REQUIRE(res.has_value());
  CHECK(res.value() == "hello");
}

TEST_CASE("literal_string_parser: fails on mismatch")
{
  yk::asteroid::literal_string_parser p{"hello"};
  CHECK(!p("world").has_value());
}

TEST_CASE("literal_string_parser: fails on too short input")
{
  yk::asteroid::literal_string_parser p{"hello"};
  CHECK(!p("hel").has_value());
}

TEST_CASE("literal_string_parser: empty string matches anything")
{
  yk::asteroid::literal_string_parser p{""};
  auto const res = p("abc");
  REQUIRE(res.has_value());
  CHECK(res.value() == "");
}
