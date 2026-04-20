#include "test.hpp"

#include <yk/asteroid/preprocess/macro.hpp>
#include <yk/asteroid/preprocess/pp_token.hpp>

#include <string>

using yk::asteroid::macro_definition;
using yk::asteroid::macro_table;
using yk::asteroid::macros_equivalent;
using yk::asteroid::pp_token;
using yk::asteroid::pp_token_kind;

namespace {

pp_token tok(pp_token_kind k, std::string_view spelling)
{
  pp_token t;
  t.kind = k;
  t.spelling = spelling;
  return t;
}

macro_definition make_object_macro(std::string name, std::vector<pp_token> replacement)
{
  macro_definition m;
  m.name = std::move(name);
  m.is_function_like = false;
  m.replacement = std::move(replacement);
  return m;
}

}  // namespace

TEST_CASE("macro_table: lookup on empty table returns nullopt")
{
  macro_table t;
  CHECK_FALSE(t.lookup("X").has_value());
  CHECK_FALSE(t.defined("X"));
  CHECK(t.size() == 0);
}

TEST_CASE("macro_table: define and lookup")
{
  macro_table t;
  t.define(make_object_macro("X", {tok(pp_token_kind::pp_number, "42")}));
  REQUIRE(t.defined("X"));
  auto m = t.lookup("X");
  REQUIRE(m.has_value());
  CHECK(m->name == "X");
  CHECK_FALSE(m->is_function_like);
  REQUIRE(m->replacement.size() == 1);
  CHECK(m->replacement[0].spelling == "42");
  CHECK(t.size() == 1);
}

TEST_CASE("macro_table: redefine replaces previous definition")
{
  macro_table t;
  t.define(make_object_macro("X", {tok(pp_token_kind::pp_number, "1")}));
  t.define(make_object_macro("X", {tok(pp_token_kind::pp_number, "2")}));
  auto m = t.lookup("X");
  REQUIRE(m.has_value());
  REQUIRE(m->replacement.size() == 1);
  CHECK(m->replacement[0].spelling == "2");
  CHECK(t.size() == 1);
}

TEST_CASE("macro_table: undefine removes definition")
{
  macro_table t;
  t.define(make_object_macro("X", {tok(pp_token_kind::pp_number, "1")}));
  CHECK(t.undefine("X"));
  CHECK_FALSE(t.defined("X"));
  CHECK_FALSE(t.lookup("X").has_value());
  CHECK(t.size() == 0);
}

TEST_CASE("macro_table: undefine of missing macro returns false")
{
  macro_table t;
  CHECK_FALSE(t.undefine("missing"));
}

TEST_CASE("macro_table: lookup by string_view substring does not need null terminator")
{
  macro_table t;
  t.define(make_object_macro("FOO", {tok(pp_token_kind::pp_number, "1")}));
  std::string src = "FOOBAR";
  std::string_view sv{src.data(), 3};
  auto m = t.lookup(sv);
  REQUIRE(m.has_value());
  CHECK(m->name == "FOO");
}

TEST_CASE("macro_table: multiple distinct macros coexist")
{
  macro_table t;
  t.define(make_object_macro("A", {tok(pp_token_kind::pp_number, "1")}));
  t.define(make_object_macro("B", {tok(pp_token_kind::pp_number, "2")}));
  t.define(make_object_macro("C", {tok(pp_token_kind::pp_number, "3")}));
  CHECK(t.size() == 3);
  CHECK(t.lookup("A")->replacement[0].spelling == "1");
  CHECK(t.lookup("B")->replacement[0].spelling == "2");
  CHECK(t.lookup("C")->replacement[0].spelling == "3");
}

TEST_CASE("macro_table: clear removes all definitions")
{
  macro_table t;
  t.define(make_object_macro("A", {}));
  t.define(make_object_macro("B", {}));
  t.clear();
  CHECK(t.size() == 0);
  CHECK_FALSE(t.defined("A"));
  CHECK_FALSE(t.defined("B"));
}

TEST_CASE("macros_equivalent: identical object-like macros")
{
  auto a = make_object_macro("X", {tok(pp_token_kind::pp_number, "1"), tok(pp_token_kind::whitespace, " "),
                                   tok(pp_token_kind::punctuator, "+"), tok(pp_token_kind::whitespace, " "),
                                   tok(pp_token_kind::pp_number, "2")});
  auto b = make_object_macro("X", {tok(pp_token_kind::pp_number, "1"), tok(pp_token_kind::whitespace, " "),
                                   tok(pp_token_kind::punctuator, "+"), tok(pp_token_kind::whitespace, " "),
                                   tok(pp_token_kind::pp_number, "2")});
  CHECK(macros_equivalent(a, b));
}

TEST_CASE("macros_equivalent: differing replacement tokens are not equivalent")
{
  auto a = make_object_macro("X", {tok(pp_token_kind::pp_number, "1")});
  auto b = make_object_macro("X", {tok(pp_token_kind::pp_number, "2")});
  CHECK_FALSE(macros_equivalent(a, b));
}

TEST_CASE("macros_equivalent: differing parameter lists are not equivalent")
{
  macro_definition a;
  a.name = "F";
  a.is_function_like = true;
  a.parameters = {"x"};
  a.replacement = {tok(pp_token_kind::identifier, "x")};

  macro_definition b = a;
  b.parameters = {"y"};
  b.replacement = {tok(pp_token_kind::identifier, "y")};

  CHECK_FALSE(macros_equivalent(a, b));
}

TEST_CASE("macros_equivalent: object-like vs function-like are not equivalent")
{
  macro_definition obj;
  obj.name = "F";
  obj.is_function_like = false;

  macro_definition fn;
  fn.name = "F";
  fn.is_function_like = true;

  CHECK_FALSE(macros_equivalent(obj, fn));
}
