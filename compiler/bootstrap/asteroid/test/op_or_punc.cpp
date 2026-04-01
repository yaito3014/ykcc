#include "test.hpp"

#include <yk/asteroid/preprocess/op_or_punc.hpp>

TEST_CASE("op_or_punc: 3-char operators")
{
  yk::asteroid::preprocess::op_or_punc_parser p{};

  CHECK(p("<=>x").value() == "<=>");
  CHECK(p(">>=x").value() == ">>=");
  CHECK(p("<<=x").value() == "<<=");
  CHECK(p("->*x").value() == "->*");
  CHECK(p("...x").value() == "...");
}

TEST_CASE("op_or_punc: 2-char operators")
{
  yk::asteroid::preprocess::op_or_punc_parser p{};

  CHECK(p("##x").value() == "##");
  CHECK(p("::x").value() == "::");
  CHECK(p("->x").value() == "->");
  CHECK(p("==x").value() == "==");
  CHECK(p("++x").value() == "++");
  CHECK(p("<<x").value() == "<<");
}

TEST_CASE("op_or_punc: 1-char operators")
{
  yk::asteroid::preprocess::op_or_punc_parser p{};

  CHECK(p("{x").value() == "{");
  CHECK(p("+x").value() == "+");
  CHECK(p("#x").value() == "#");
  CHECK(p(";x").value() == ";");
  CHECK(p(".x").value() == ".");
}

TEST_CASE("op_or_punc: greedy matching")
{
  yk::asteroid::preprocess::op_or_punc_parser p{};

  CHECK(p("<<=").value() == "<<=");
  CHECK(p("->*").value() == "->*");
  CHECK(p("...").value() == "...");
  CHECK(p("##x").value() == "##");
}

TEST_CASE("op_or_punc: digraphs")
{
  yk::asteroid::preprocess::op_or_punc_parser p{};

  CHECK(p("<%x").value() == "<%");
  CHECK(p("%>x").value() == "%>");
  CHECK(p("<:x").value() == "<:");
  CHECK(p(":>x").value() == ":>");
  CHECK(p("%:x").value() == "%:");
  CHECK(p("%:%:x").value() == "%:%:");
}

TEST_CASE("op_or_punc: %:%: greedy over %:")
{
  yk::asteroid::preprocess::op_or_punc_parser p{};

  CHECK(p("%:%:").value() == "%:%:");
}

TEST_CASE("op_or_punc: <:: exception")
{
  yk::asteroid::preprocess::op_or_punc_parser p{};

  // <::a> — exception applies, < by itself
  CHECK(p("<::a>").value() == "<");

  // <::> — exception does not apply (4th char is >), <: digraph
  CHECK(p("<::>").value() == "<:");

  // <:::> — exception does not apply (4th char is :), <: digraph
  CHECK(p("<:::>").value() == "<:");

  // <:: at end of input — exception applies
  CHECK(p("<::").value() == "<");
}

TEST_CASE("op_or_punc: splice operators")
{
  yk::asteroid::preprocess::op_or_punc_parser p{};

  CHECK(p("[:x").value() == "[:");
  CHECK(p(":]x").value() == ":]");
}

TEST_CASE("op_or_punc: [:: exception")
{
  yk::asteroid::preprocess::op_or_punc_parser p{};

  // [::a — exception applies (4th char is not :), [ by itself
  CHECK(p("[::a").value() == "[");

  // [::> — exception applies (4th char is not :), [ by itself
  CHECK(p("[::>").value() == "[");

  // [::: — exception does not apply (4th char is :), [: splice
  CHECK(p("[:::").value() == "[:");

  // [:: at end of input — exception applies
  CHECK(p("[::").value() == "[");

  // [:> — always [ by itself
  CHECK(p("[:>").value() == "[");
}

TEST_CASE("op_or_punc: non-match")
{
  yk::asteroid::preprocess::op_or_punc_parser p{};

  CHECK(!p("@").has_value());
  CHECK(!p("123").has_value());
  CHECK(!p("abc").has_value());
  CHECK(!p("").has_value());
}
