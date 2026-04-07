#include "test.hpp"

#include <yk/asteroid/preprocess/char_literal.hpp>

TEST_CASE("char_literal: basic")
{
  yk::asteroid::preprocess::char_literal_parser p{};

  CHECK(p("'a'").value() == "'a'");
  CHECK(p("'0'").value() == "'0'");
  CHECK(p("' '").value() == "' '");
  CHECK(p("'a' rest").value() == "'a'");
}

TEST_CASE("char_literal: multi-character")
{
  yk::asteroid::preprocess::char_literal_parser p{};

  CHECK(p("'ab'").value() == "'ab'");
  CHECK(p("'abc'").value() == "'abc'");
}

TEST_CASE("char_literal: encoding prefix")
{
  yk::asteroid::preprocess::char_literal_parser p{};

  CHECK(p("L'a'").value() == "L'a'");
  CHECK(p("u'a'").value() == "u'a'");
  CHECK(p("U'a'").value() == "U'a'");
  CHECK(p("u8'a'").value() == "u8'a'");
  CHECK(p("L'ab'").value() == "L'ab'");
  CHECK(p("u8'ab'").value() == "u8'ab'");
}

TEST_CASE("char_literal: simple escape sequences")
{
  yk::asteroid::preprocess::char_literal_parser p{};

  CHECK(p("'\\n'").value() == "'\\n'");
  CHECK(p("'\\t'").value() == "'\\t'");
  CHECK(p("'\\r'").value() == "'\\r'");
  CHECK(p("'\\0'").value() == "'\\0'");
  CHECK(p("'\\a'").value() == "'\\a'");
  CHECK(p("'\\b'").value() == "'\\b'");
  CHECK(p("'\\f'").value() == "'\\f'");
  CHECK(p("'\\v'").value() == "'\\v'");
  CHECK(p("'\\''").value() == "'\\''");
  CHECK(p("'\\\"'").value() == "'\\\"'");
  CHECK(p("'\\?'").value() == "'\\?'");
  CHECK(p("'\\\\'").value() == "'\\\\'");
}

TEST_CASE("char_literal: octal escape sequences")
{
  yk::asteroid::preprocess::char_literal_parser p{};

  CHECK(p("'\\0'").value() == "'\\0'");
  CHECK(p("'\\7'").value() == "'\\7'");
  CHECK(p("'\\77'").value() == "'\\77'");
  CHECK(p("'\\177'").value() == "'\\177'");
  // fourth octal digit is a separate plain char
  CHECK(p("'\\1770'").value() == "'\\1770'");
}

TEST_CASE("char_literal: hexadecimal escape sequences")
{
  yk::asteroid::preprocess::char_literal_parser p{};

  CHECK(p("'\\x0'").value() == "'\\x0'");
  CHECK(p("'\\xff'").value() == "'\\xff'");
  CHECK(p("'\\xFF'").value() == "'\\xFF'");
  CHECK(p("'\\x0123'").value() == "'\\x0123'");
  CHECK(p("'\\xDEAD'").value() == "'\\xDEAD'");
}

TEST_CASE("char_literal: universal character names")
{
  yk::asteroid::preprocess::char_literal_parser p{};

  CHECK(p("'\\u0041'").value() == "'\\u0041'");
  CHECK(p("'\\uFFFF'").value() == "'\\uFFFF'");
  CHECK(p("'\\U00000041'").value() == "'\\U00000041'");
  CHECK(p("'\\U0001F600'").value() == "'\\U0001F600'");
}

TEST_CASE("char_literal: mixed content")
{
  yk::asteroid::preprocess::char_literal_parser p{};

  CHECK(p("'a\\nb'").value() == "'a\\nb'");
  CHECK(p("'\\x41B'").value() == "'\\x41B'");
  CHECK(p("'\\0a'").value() == "'\\0a'");
}

TEST_CASE("char_literal: non-match")
{
  yk::asteroid::preprocess::char_literal_parser p{};

  CHECK(!p("").has_value());
  CHECK(!p("a").has_value());
  CHECK(!p("\"a\"").has_value());
  CHECK(!p("'\n'").has_value());
  CHECK(!p("'").has_value());
  CHECK(!p("''").has_value());
}

TEST_CASE("char_literal: invalid escape sequences")
{
  yk::asteroid::preprocess::char_literal_parser p{};

  CHECK(!p("'\\c'").has_value());
  CHECK(!p("'\\g'").has_value());
  CHECK(!p("'\\8'").has_value());
  CHECK(!p("'\\9'").has_value());
  CHECK(!p("'\\x'").has_value());
  CHECK(!p("'\\u004'").has_value());   // too few hex digits for \u
  CHECK(!p("'\\U0000004'").has_value());  // too few hex digits for \U
}

TEST_CASE("char_literal: unterminated")
{
  yk::asteroid::preprocess::char_literal_parser p{};

  CHECK(!p("'a").has_value());
  CHECK(!p("'\\n").has_value());
  CHECK(!p("'\\'").has_value());
}
