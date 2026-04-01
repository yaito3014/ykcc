#include <catch2/catch_test_macros.hpp>

#include <yk/asteroid/preprocess/line_splice.hpp>

using yk::asteroid::preprocess::splice_lines;

TEST_CASE("line_splice: no splices")
{
  auto const result = splice_lines("int x = 42;\n");
  CHECK(result.text == "int x = 42;\n");
  CHECK(result.splices.empty());
}

TEST_CASE("line_splice: basic splice")
{
  auto const result = splice_lines("int \\\nx = 42;\n");
  CHECK(result.text == "int x = 42;\n");
  REQUIRE(result.splices.size() == 1);
  CHECK(result.splices[0].original_pos == 4);
  CHECK(result.splices[0].length == 2);
}

TEST_CASE("line_splice: multiple splices")
{
  auto const result = splice_lines("a\\\nb\\\nc");
  CHECK(result.text == "abc\n");
  REQUIRE(result.splices.size() == 2);
}

TEST_CASE("line_splice: to_original mapping")
{
  auto const result = splice_lines("ab\\\ncd");
  // spliced: "abcd\n", original: "ab\\\ncd"
  CHECK(result.to_original(0) == 0);  // a
  CHECK(result.to_original(1) == 1);  // b
  CHECK(result.to_original(2) == 4);  // c
  CHECK(result.to_original(3) == 5);  // d
}

TEST_CASE("line_splice: original_range")
{
  std::string_view original = "R\"(he\\\nllo)\"\n";
  auto const result = splice_lines(original);
  // spliced: R"(hello)"\n
  // raw string content "hello" is at spliced [3, 8)
  auto const orig = result.original_range(original, 3, 8);
  CHECK(orig == "he\\\nllo");
}

TEST_CASE("line_splice: trailing backslash without newline")
{
  auto const result = splice_lines("abc\\");
  CHECK(result.text == "abc\\\n");
  CHECK(result.splices.empty());
}

TEST_CASE("line_splice: appends newline to non-empty input without trailing newline")
{
  auto const result = splice_lines("abc");
  CHECK(result.text == "abc\n");
}

TEST_CASE("line_splice: does not append newline to input ending with newline")
{
  auto const result = splice_lines("abc\n");
  CHECK(result.text == "abc\n");
}

TEST_CASE("line_splice: empty input stays empty")
{
  auto const result = splice_lines("");
  CHECK(result.text.empty());
}
