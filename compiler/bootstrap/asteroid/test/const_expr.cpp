#include <catch2/catch_test_macros.hpp>

#include <yk/asteroid/preprocess/const_expr.hpp>

#include <string_view>
#include <vector>

using yk::asteroid::preprocess::evaluate_const_expr;
using yk::asteroid::preprocess::lexer;
using yk::asteroid::preprocess::pp_token;
using yk::asteroid::preprocess::pp_token_kind;

namespace {

std::vector<pp_token> tokenize(std::string_view src)
{
  lexer lex{src};
  std::vector<pp_token> tokens;
  for (auto const& tok : lex) {
    if (tok.kind != pp_token_kind::newline) tokens.push_back(tok);
  }
  return tokens;
}

std::intmax_t eval(std::string_view src)
{
  auto tokens = tokenize(src);
  auto result = evaluate_const_expr(tokens, [](std::string_view) { return false; });
  REQUIRE(result.has_value());
  return result->value;
}

std::intmax_t eval_with_defined(std::string_view src, auto&& lookup)
{
  auto tokens = tokenize(src);
  auto result = evaluate_const_expr(tokens, lookup);
  REQUIRE(result.has_value());
  return result->value;
}

bool eval_fails(std::string_view src)
{
  auto tokens = tokenize(src);
  auto result = evaluate_const_expr(tokens, [](std::string_view) { return false; });
  return !result.has_value();
}

}  // namespace

TEST_CASE("const_expr: decimal literals")
{
  CHECK(eval("0") == 0);
  CHECK(eval("42") == 42);
  CHECK(eval("1000") == 1000);
}

TEST_CASE("const_expr: hex literals")
{
  CHECK(eval("0xFF") == 255);
  CHECK(eval("0XFF") == 255);
  CHECK(eval("0x0") == 0);
  CHECK(eval("0xDEAD") == 0xDEAD);
}

TEST_CASE("const_expr: octal literals")
{
  CHECK(eval("077") == 63);
  CHECK(eval("00") == 0);
  CHECK(eval("010") == 8);
}

TEST_CASE("const_expr: binary literals")
{
  CHECK(eval("0b1010") == 10);
  CHECK(eval("0B1111") == 15);
  CHECK(eval("0b0") == 0);
}

TEST_CASE("const_expr: integer suffixes")
{
  CHECK(eval("42U") == 42);
  CHECK(eval("42L") == 42);
  CHECK(eval("42UL") == 42);
  CHECK(eval("42LL") == 42);
  CHECK(eval("42ULL") == 42);
  CHECK(eval("42uLL") == 42);
}

TEST_CASE("const_expr: character literals")
{
  CHECK(eval("'A'") == 65);
  CHECK(eval("'0'") == 48);
  CHECK(eval("' '") == 32);
}

TEST_CASE("const_expr: character escape sequences")
{
  CHECK(eval("'\\n'") == 10);
  CHECK(eval("'\\t'") == 9);
  CHECK(eval("'\\0'") == 0);
  CHECK(eval("'\\x41'") == 65);
  CHECK(eval("'\\77'") == 63);
  CHECK(eval("'\\\\'") == 92);
  CHECK(eval("'\\''") == 39);
}

TEST_CASE("const_expr: true and false")
{
  CHECK(eval("true") == 1);
  CHECK(eval("false") == 0);
}

TEST_CASE("const_expr: unknown identifiers")
{
  CHECK(eval("FOO") == 0);
  CHECK(eval("BAR") == 0);
}

TEST_CASE("const_expr: unary operators")
{
  CHECK(eval("+1") == 1);
  CHECK(eval("-1") == -1);
  CHECK(eval("!0") == 1);
  CHECK(eval("!1") == 0);
  CHECK(eval("!42") == 0);
  CHECK(eval("~0") == -1);
  CHECK(eval("-(-1)") == 1);
}

TEST_CASE("const_expr: additive")
{
  CHECK(eval("2+3") == 5);
  CHECK(eval("10-3") == 7);
  CHECK(eval("1+2+3") == 6);
}

TEST_CASE("const_expr: multiplicative")
{
  CHECK(eval("2*3") == 6);
  CHECK(eval("7/2") == 3);
  CHECK(eval("7%3") == 1);
}

TEST_CASE("const_expr: shift")
{
  CHECK(eval("1<<4") == 16);
  CHECK(eval("16>>2") == 4);
}

TEST_CASE("const_expr: relational")
{
  CHECK(eval("1<2") == 1);
  CHECK(eval("2<1") == 0);
  CHECK(eval("2>1") == 1);
  CHECK(eval("1>2") == 0);
  CHECK(eval("1<=1") == 1);
  CHECK(eval("1>=1") == 1);
}

TEST_CASE("const_expr: equality")
{
  CHECK(eval("1==1") == 1);
  CHECK(eval("1==2") == 0);
  CHECK(eval("1!=2") == 1);
  CHECK(eval("1!=1") == 0);
}

TEST_CASE("const_expr: bitwise")
{
  CHECK(eval("0xFF&0x0F") == 0x0F);
  CHECK(eval("0xF0|0x0F") == 0xFF);
  CHECK(eval("0xFF^0x0F") == 0xF0);
}

TEST_CASE("const_expr: logical")
{
  CHECK(eval("1&&1") == 1);
  CHECK(eval("1&&0") == 0);
  CHECK(eval("0&&1") == 0);
  CHECK(eval("0||0") == 0);
  CHECK(eval("0||1") == 1);
  CHECK(eval("1||0") == 1);
}

TEST_CASE("const_expr: ternary")
{
  CHECK(eval("1?2:3") == 2);
  CHECK(eval("0?2:3") == 3);
  CHECK(eval("1?1?4:5:6") == 4);
}

TEST_CASE("const_expr: parentheses and precedence")
{
  CHECK(eval("2+3*4") == 14);
  CHECK(eval("(2+3)*4") == 20);
  CHECK(eval("1+2*3+4") == 11);
  CHECK(eval("(1+2)*(3+4)") == 21);
}

TEST_CASE("const_expr: defined")
{
  auto lookup = [](std::string_view name) { return name == "FOO" || name == "BAR"; };

  CHECK(eval_with_defined("defined FOO", lookup) == 1);
  CHECK(eval_with_defined("defined(FOO)", lookup) == 1);
  CHECK(eval_with_defined("defined BAR", lookup) == 1);
  CHECK(eval_with_defined("defined(BAR)", lookup) == 1);
  CHECK(eval_with_defined("defined BAZ", lookup) == 0);
  CHECK(eval_with_defined("defined(BAZ)", lookup) == 0);
}

TEST_CASE("const_expr: alternative tokens")
{
  CHECK(eval("1 and 1") == 1);
  CHECK(eval("1 and 0") == 0);
  CHECK(eval("0 or 1") == 1);
  CHECK(eval("0 or 0") == 0);
  CHECK(eval("not 0") == 1);
  CHECK(eval("not 1") == 0);
  CHECK(eval("compl 0") == -1);
  CHECK(eval("1 not_eq 2") == 1);
  CHECK(eval("0xFF bitand 0x0F") == 0x0F);
  CHECK(eval("0xF0 bitor 0x0F") == 0xFF);
  CHECK(eval("0xFF xor 0x0F") == 0xF0);
}

TEST_CASE("const_expr: complex expressions")
{
  CHECK(eval("1 + 2 * 3 - 4 / 2") == 5);
  CHECK(eval("(1 << 8) | 0xFF") == 0x1FF);
  CHECK(eval("defined FOO || 1") == 1);
}

TEST_CASE("const_expr: errors")
{
  CHECK(eval_fails("1/0"));
  CHECK(eval_fails("1%0"));
  CHECK(eval_fails("(1+2"));
  CHECK(eval_fails(""));
  CHECK(eval_fails("1 2"));
  CHECK(eval_fails("1+"));
}
