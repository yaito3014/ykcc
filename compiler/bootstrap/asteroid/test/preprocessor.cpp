#include <catch2/catch_test_macros.hpp>

#include <yk/asteroid/preprocess/preprocessor.hpp>

using yk::asteroid::preprocess::pp_token_kind;
using yk::asteroid::preprocess::preprocessor;

TEST_CASE("preprocessor: text line")
{
  preprocessor pp{"int x = 42;\n"};
  pp.run();

  CHECK(pp.directives.empty());
  REQUIRE(pp.text_lines.size() == 1);

  auto const& tokens = pp.text_lines[0].tokens;
  REQUIRE(tokens.size() == 8);
  CHECK(tokens[0].piece == "int");
  CHECK(tokens[1].kind == pp_token_kind::whitespace);
  CHECK(tokens[2].piece == "x");
  CHECK(tokens[3].kind == pp_token_kind::whitespace);
  CHECK(tokens[4].piece == "=");
  CHECK(tokens[5].kind == pp_token_kind::whitespace);
  CHECK(tokens[6].piece == "42");
  CHECK(tokens[7].piece == ";");
}

TEST_CASE("preprocessor: include directive with angle bracket")
{
  preprocessor pp{"#include <stdio.h>\n"};
  pp.run();

  CHECK(pp.text_lines.empty());
  REQUIRE(pp.directives.size() == 1);

  auto const& dir = pp.directives[0];
  CHECK(dir.hash.piece == "#");
  CHECK(dir.name.piece == "include");
  REQUIRE(dir.tokens.size() == 2);
  CHECK(dir.tokens[0].kind == pp_token_kind::whitespace);
  CHECK(dir.tokens[1].kind == pp_token_kind::header_name);
  CHECK(dir.tokens[1].piece == "<stdio.h>");
}

TEST_CASE("preprocessor: include directive with quoted header")
{
  preprocessor pp{"#include \"myheader.h\"\n"};
  pp.run();

  REQUIRE(pp.directives.size() == 1);
  auto const& dir = pp.directives[0];
  CHECK(dir.name.piece == "include");
  REQUIRE(dir.tokens.size() == 2);
  CHECK(dir.tokens[0].kind == pp_token_kind::whitespace);
  CHECK(dir.tokens[1].kind == pp_token_kind::header_name);
  CHECK(dir.tokens[1].piece == "\"myheader.h\"");
}

TEST_CASE("preprocessor: include without whitespace")
{
  preprocessor pp{"#include<a.h>\n"};
  pp.run();

  REQUIRE(pp.directives.size() == 1);
  auto const& dir = pp.directives[0];
  REQUIRE(dir.tokens.size() == 1);
  CHECK(dir.tokens[0].kind == pp_token_kind::header_name);
  CHECK(dir.tokens[0].piece == "<a.h>");
}

TEST_CASE("preprocessor: define directive")
{
  preprocessor pp{"#define FOO 42\n"};
  pp.run();

  REQUIRE(pp.directives.size() == 1);
  auto const& dir = pp.directives[0];
  CHECK(dir.name.piece == "define");
  REQUIRE(dir.tokens.size() == 4);
  CHECK(dir.tokens[0].kind == pp_token_kind::whitespace);
  CHECK(dir.tokens[1].piece == "FOO");
  CHECK(dir.tokens[2].kind == pp_token_kind::whitespace);
  CHECK(dir.tokens[3].piece == "42");
}

TEST_CASE("preprocessor: line splice")
{
  preprocessor pp{"#def\\\nine FOO\n"};
  pp.run();

  REQUIRE(pp.directives.size() == 1);
  CHECK(pp.directives[0].name.piece == "define");
}

TEST_CASE("preprocessor: multiple lines")
{
  preprocessor pp{"#include <a.h>\nint x;\n"};
  pp.run();

  REQUIRE(pp.directives.size() == 1);
  CHECK(pp.directives[0].name.piece == "include");

  REQUIRE(pp.text_lines.size() == 1);
  CHECK(pp.text_lines[0].tokens[0].piece == "int");
}

TEST_CASE("preprocessor: include with spaceship as header-name")
{
  preprocessor pp{"#include <=>\n"};
  pp.run();

  REQUIRE(pp.directives.size() == 1);
  auto const& dir = pp.directives[0];
  REQUIRE(dir.tokens.size() == 2);
  CHECK(dir.tokens[0].kind == pp_token_kind::whitespace);
  CHECK(dir.tokens[1].kind == pp_token_kind::header_name);
  CHECK(dir.tokens[1].piece == "<=>");
}

TEST_CASE("preprocessor: include with comment-like header-name")
{
  preprocessor pp{"#include </**/>\n"};
  pp.run();

  REQUIRE(pp.directives.size() == 1);
  auto const& dir = pp.directives[0];
  REQUIRE(dir.tokens.size() == 2);
  CHECK(dir.tokens[0].kind == pp_token_kind::whitespace);
  CHECK(dir.tokens[1].kind == pp_token_kind::header_name);
  CHECK(dir.tokens[1].piece == "</**/>");
}

TEST_CASE("preprocessor: empty input")
{
  preprocessor pp{""};
  pp.run();

  CHECK(pp.directives.empty());
  CHECK(pp.text_lines.empty());
}

TEST_CASE("preprocessor: null directive")
{
  preprocessor pp{"#\n"};
  pp.run();

  REQUIRE(pp.directives.size() == 1);
  CHECK(pp.directives[0].tokens.empty());
}
