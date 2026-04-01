#include "test.hpp"

#include <yk/asteroid/preprocess/string_literal.hpp>

TEST_CASE("string_literal: basic")
{
  yk::asteroid::preprocess::string_literal_parser p{};

  CHECK(p("\"hello\"").value() == "\"hello\"");
  CHECK(p("\"hello\" rest").value() == "\"hello\"");
  CHECK(p("\"\\n\"").value() == "\"\\n\"");
  CHECK(p("\"\\\"\"").value() == "\"\\\"\"");
  CHECK(p("\"\\\\\"").value() == "\"\\\\\"");
  CHECK(p("\"\"").value() == "\"\"");
}

TEST_CASE("string_literal: encoding prefix")
{
  yk::asteroid::preprocess::string_literal_parser p{};

  CHECK(p("L\"a\"").value() == "L\"a\"");
  CHECK(p("u\"a\"").value() == "u\"a\"");
  CHECK(p("U\"a\"").value() == "U\"a\"");
  CHECK(p("u8\"a\"").value() == "u8\"a\"");
}

TEST_CASE("string_literal: raw")
{
  yk::asteroid::preprocess::string_literal_parser p{};

  CHECK(p("R\"(hello)\"").value() == "R\"(hello)\"");
  CHECK(p("R\"delim(he\"llo)delim\"").value() == "R\"delim(he\"llo)delim\"");
  CHECK(p("R\"(line\nbreak)\"").value() == "R\"(line\nbreak)\"");
}

TEST_CASE("string_literal: raw with encoding prefix")
{
  yk::asteroid::preprocess::string_literal_parser p{};

  CHECK(p("LR\"(hello)\"").value() == "LR\"(hello)\"");
  CHECK(p("uR\"(hello)\"").value() == "uR\"(hello)\"");
  CHECK(p("UR\"(hello)\"").value() == "UR\"(hello)\"");
  CHECK(p("u8R\"(hello)\"").value() == "u8R\"(hello)\"");
}

TEST_CASE("string_literal: non-match")
{
  yk::asteroid::preprocess::string_literal_parser p{};

  CHECK(!p("").has_value());
  CHECK(!p("a").has_value());
  CHECK(!p("'a'").has_value());
  CHECK(!p("\"\n\"").has_value());
  CHECK(!p("\"").has_value());
}
