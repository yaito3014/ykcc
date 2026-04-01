#include "test.hpp"

#include <yk/asteroid/preprocess/char_literal.hpp>

TEST_CASE("char_literal: basic")
{
  yk::asteroid::preprocess::char_literal_parser p{};

  CHECK(p("'a'").value() == "'a'");
  CHECK(p("'a' rest").value() == "'a'");
  CHECK(p("'\\n'").value() == "'\\n'");
  CHECK(p("'\\''").value() == "'\\''");
  CHECK(p("'\\\\'").value() == "'\\\\'");
}

TEST_CASE("char_literal: encoding prefix")
{
  yk::asteroid::preprocess::char_literal_parser p{};

  CHECK(p("L'a'").value() == "L'a'");
  CHECK(p("u'a'").value() == "u'a'");
  CHECK(p("U'a'").value() == "U'a'");
  CHECK(p("u8'a'").value() == "u8'a'");
}

TEST_CASE("char_literal: non-match")
{
  yk::asteroid::preprocess::char_literal_parser p{};

  CHECK(!p("").has_value());
  CHECK(!p("a").has_value());
  CHECK(!p("\"a\"").has_value());
  CHECK(!p("'\n'").has_value());
  CHECK(!p("'").has_value());
}
