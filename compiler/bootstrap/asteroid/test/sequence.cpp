#include "test.hpp"

#include <yk/asteroid/core/parser/sequence.hpp>

TEST_CASE("sequence")
{
  yk::asteroid::parser auto const left = numeric_parser<int>{};
  yk::asteroid::parser auto const right = alphabet_parser{};
  yk::asteroid::sequence const seq{left, right};
  auto const res = seq("12abc");
  REQUIRE(res.has_value());
  auto const lr_res = *res;
  CHECK(get<0>(lr_res) == 12);
  CHECK(get<1>(lr_res) == "abc");
}

TEST_CASE("chained sequence")
{
  yk::asteroid::parser auto const parser0 = numeric_parser<int>{};
  yk::asteroid::parser auto const parser1 = alphabet_parser{};
  yk::asteroid::parser auto const parser2 = numeric_parser<int>{};
  yk::asteroid::parser auto const parser3 = alphabet_parser{};
  yk::asteroid::sequence const seq0{parser0, parser1};
  yk::asteroid::sequence const seq1{parser2, parser3};
  yk::asteroid::sequence const seq{seq0, seq1};
  auto const res = seq("12abc34def");
  REQUIRE(res.has_value());
  auto const val = *res;
  CHECK(get<0>(val) == 12);
  CHECK(get<1>(val) == "abc");
  CHECK(get<2>(val) == 34);
  CHECK(get<3>(val) == "def");
}
