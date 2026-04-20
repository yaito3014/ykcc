#include "test.hpp"

#include <yk/asteroid/preprocess/line_splicer.hpp>

using yk::asteroid::line_splicer;

TEST_CASE("line_splicer: empty input produces single newline")
{
  line_splicer ls{""};
  CHECK(ls.spliced() == "\n");
}

TEST_CASE("line_splicer: appends trailing newline when missing")
{
  line_splicer ls{"abc"};
  CHECK(ls.spliced() == "abc\n");
}

TEST_CASE("line_splicer: preserves existing trailing newline")
{
  line_splicer ls{"abc\n"};
  CHECK(ls.spliced() == "abc\n");
}

TEST_CASE("line_splicer: splices backslash-newline")
{
  line_splicer ls{"ab\\\ncd\n"};
  CHECK(ls.spliced() == "abcd\n");
}

TEST_CASE("line_splicer: splices backslash-crlf")
{
  line_splicer ls{"ab\\\r\ncd\n"};
  CHECK(ls.spliced() == "abcd\n");
}

TEST_CASE("line_splicer: splices backslash-cr")
{
  line_splicer ls{"ab\\\rcd\n"};
  CHECK(ls.spliced() == "abcd\n");
}

TEST_CASE("line_splicer: multiple consecutive splices")
{
  line_splicer ls{"a\\\nb\\\nc\n"};
  CHECK(ls.spliced() == "abc\n");
}

TEST_CASE("line_splicer: splice at end of file adds newline")
{
  line_splicer ls{"abc\\\n"};
  CHECK(ls.spliced() == "abc\n");
}

TEST_CASE("line_splicer: trailing bare backslash is preserved")
{
  line_splicer ls{"abc\\"};
  CHECK(ls.spliced() == "abc\\\n");
}

TEST_CASE("line_splicer: backslash not followed by newline is kept")
{
  line_splicer ls{"a\\b\n"};
  CHECK(ls.spliced() == "a\\b\n");
}

TEST_CASE("line_splicer: normalizes crlf to lf")
{
  line_splicer ls{"a\r\nb\r\n"};
  CHECK(ls.spliced() == "a\nb\n");
}

TEST_CASE("line_splicer: normalizes bare cr to lf")
{
  line_splicer ls{"a\rb\r"};
  CHECK(ls.spliced() == "a\nb\n");
}

TEST_CASE("line_splicer: physical_offset identity when no splices")
{
  line_splicer ls{"abc\n"};
  CHECK(ls.physical_offset(0) == 0);
  CHECK(ls.physical_offset(1) == 1);
  CHECK(ls.physical_offset(2) == 2);
  CHECK(ls.physical_offset(3) == 3);
}

TEST_CASE("line_splicer: physical_offset skips spliced bytes")
{
  //              0 1 2  3 4 5 6
  // source:      a b \\ \n c d \n
  // spliced:     a b c d \n
  line_splicer ls{"ab\\\ncd\n"};
  REQUIRE(ls.spliced() == "abcd\n");
  CHECK(ls.physical_offset(0) == 0);
  CHECK(ls.physical_offset(1) == 1);
  CHECK(ls.physical_offset(2) == 4);
  CHECK(ls.physical_offset(3) == 5);
  CHECK(ls.physical_offset(4) == 6);
}

TEST_CASE("line_splicer: physical_location reports 1-based line/column")
{
  line_splicer ls{"ab\ncd\n"};
  auto loc0 = ls.physical_location(0);
  CHECK(loc0.line == 1);
  CHECK(loc0.column == 1);

  auto loc_c = ls.physical_location(3);
  CHECK(loc_c.line == 2);
  CHECK(loc_c.column == 1);

  auto loc_d = ls.physical_location(4);
  CHECK(loc_d.line == 2);
  CHECK(loc_d.column == 2);
}

TEST_CASE("line_splicer: physical_location accounts for splices")
{
  // source: "ab\\\ncd\n"
  //          a b \ \n c d \n
  //          1 2 3 4  1 2 3  (line/column in source)
  // spliced: "abcd\n"
  // 'c' at logical index 2 should map back to line 2, col 1.
  line_splicer ls{"ab\\\ncd\n"};
  auto loc_c = ls.physical_location(2);
  CHECK(loc_c.line == 2);
  CHECK(loc_c.column == 1);
}
