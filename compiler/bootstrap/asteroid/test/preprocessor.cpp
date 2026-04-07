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
  preprocessor pp{"#define FOO\n#if defined(FOO)\nint x;\n#endif\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  CHECK(pp.text_lines[0].tokens[0].piece == "int");
}

TEST_CASE("preprocessor: line splice")
{
  preprocessor pp{"#def\\\nine FOO\n#ifdef FOO\nint x;\n#endif\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  CHECK(pp.text_lines[0].tokens[0].piece == "int");
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

TEST_CASE("preprocessor: #if true")
{
  preprocessor pp{"#if 1\nint x;\n#endif\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  CHECK(pp.text_lines[0].tokens[0].piece == "int");
}

TEST_CASE("preprocessor: #if false")
{
  preprocessor pp{"#if 0\nint x;\n#endif\n"};
  pp.run();

  CHECK(pp.text_lines.empty());
}

TEST_CASE("preprocessor: #if else")
{
  preprocessor pp{"#if 0\nint x;\n#else\nint y;\n#endif\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  CHECK(pp.text_lines[0].tokens[0].piece == "int");
  CHECK(pp.text_lines[0].tokens[2].piece == "y");
}

TEST_CASE("preprocessor: #if elif")
{
  preprocessor pp{"#if 0\nint a;\n#elif 1\nint b;\n#else\nint c;\n#endif\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  CHECK(pp.text_lines[0].tokens[2].piece == "b");
}

TEST_CASE("preprocessor: #if elif chain")
{
  preprocessor pp{"#if 0\nint a;\n#elif 0\nint b;\n#elif 1\nint c;\n#else\nint d;\n#endif\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  CHECK(pp.text_lines[0].tokens[2].piece == "c");
}

TEST_CASE("preprocessor: #if elif all false falls to else")
{
  preprocessor pp{"#if 0\nint a;\n#elif 0\nint b;\n#else\nint c;\n#endif\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  CHECK(pp.text_lines[0].tokens[2].piece == "c");
}

TEST_CASE("preprocessor: #ifdef undefined")
{
  preprocessor pp{"#ifdef FOO\nint x;\n#endif\n"};
  pp.run();

  CHECK(pp.text_lines.empty());
}

TEST_CASE("preprocessor: #ifndef undefined")
{
  preprocessor pp{"#ifndef FOO\nint x;\n#endif\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  CHECK(pp.text_lines[0].tokens[0].piece == "int");
}

TEST_CASE("preprocessor: nested #if")
{
  preprocessor pp{"#if 1\n#if 0\nint a;\n#else\nint b;\n#endif\nint c;\n#endif\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 2);
  CHECK(pp.text_lines[0].tokens[2].piece == "b");
  CHECK(pp.text_lines[1].tokens[2].piece == "c");
}

TEST_CASE("preprocessor: nested #if outer false")
{
  preprocessor pp{"#if 0\n#if 1\nint a;\n#endif\nint b;\n#endif\n"};
  pp.run();

  CHECK(pp.text_lines.empty());
}

TEST_CASE("preprocessor: #if with expression")
{
  preprocessor pp{"#if 1 + 1 == 2\nint x;\n#endif\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  CHECK(pp.text_lines[0].tokens[0].piece == "int");
}

TEST_CASE("preprocessor: #if defined")
{
  preprocessor pp{"#if defined FOO\nint x;\n#endif\n"};
  pp.run();

  // FOO is not defined
  CHECK(pp.text_lines.empty());
}

TEST_CASE("preprocessor: #if first branch taken skips rest")
{
  preprocessor pp{"#if 1\nint a;\n#elif 1\nint b;\n#else\nint c;\n#endif\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  CHECK(pp.text_lines[0].tokens[2].piece == "a");
}

TEST_CASE("preprocessor: #if no branch taken no else")
{
  preprocessor pp{"#if 0\nint a;\n#elif 0\nint b;\n#endif\n"};
  pp.run();

  CHECK(pp.text_lines.empty());
}

TEST_CASE("preprocessor: non-conditional directive in active branch")
{
  preprocessor pp{"#if 1\n#pragma once\n#endif\n"};
  pp.run();

  REQUIRE(pp.directives.size() == 1);
  CHECK(pp.directives[0].name.piece == "pragma");
}

TEST_CASE("preprocessor: non-conditional directive in inactive branch")
{
  preprocessor pp{"#if 0\n#pragma once\n#endif\n"};
  pp.run();

  CHECK(pp.directives.empty());
}

TEST_CASE("preprocessor: #define and #ifdef")
{
  preprocessor pp{"#define FOO\n#ifdef FOO\nint x;\n#endif\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  CHECK(pp.text_lines[0].tokens[0].piece == "int");
}

TEST_CASE("preprocessor: #undef")
{
  preprocessor pp{"#define FOO\n#undef FOO\n#ifdef FOO\nint x;\n#endif\n"};
  pp.run();

  CHECK(pp.text_lines.empty());
}

TEST_CASE("preprocessor: #define in inactive branch ignored")
{
  preprocessor pp{"#if 0\n#define FOO\n#endif\n#ifdef FOO\nint x;\n#endif\n"};
  pp.run();

  CHECK(pp.text_lines.empty());
}

TEST_CASE("preprocessor: #include basic")
{
  using yk::asteroid::preprocess::include_handler_t;
  using yk::asteroid::preprocess::include_result;

  include_handler_t handler = [](std::string_view name) -> std::expected<include_result, yk::asteroid::preprocess::include_error> {
    if (name == "\"foo.h\"") {
      return include_result{"int foo;\n"};
    }
    return std::unexpected(yk::asteroid::preprocess::include_error{"not found"});
  };

  preprocessor pp{"#include \"foo.h\"\nint main;\n", handler};
  pp.run();

  REQUIRE(pp.text_lines.size() == 2);
  CHECK(pp.text_lines[0].tokens[2].piece == "foo");
  CHECK(pp.text_lines[1].tokens[2].piece == "main");
}

TEST_CASE("preprocessor: #include with include guard")
{
  using yk::asteroid::preprocess::include_handler_t;
  using yk::asteroid::preprocess::include_result;

  include_handler_t handler = [](std::string_view name) -> std::expected<include_result, yk::asteroid::preprocess::include_error> {
    if (name == "\"guard.h\"") {
      return include_result{"#ifndef GUARD_H\n#define GUARD_H\nint guarded;\n#endif\n"};
    }
    return std::unexpected(yk::asteroid::preprocess::include_error{"not found"});
  };

  preprocessor pp{"#include \"guard.h\"\n#include \"guard.h\"\n", handler};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  CHECK(pp.text_lines[0].tokens[2].piece == "guarded");
}

TEST_CASE("preprocessor: #include not found")
{
  using yk::asteroid::preprocess::include_handler_t;
  using yk::asteroid::preprocess::include_result;

  include_handler_t handler = [](std::string_view) -> std::expected<include_result, yk::asteroid::preprocess::include_error> {
    return std::unexpected(yk::asteroid::preprocess::include_error{"not found"});
  };

  preprocessor pp{"#include \"missing.h\"\nint x;\n", handler};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  CHECK(pp.text_lines[0].tokens[0].piece == "int");
}

TEST_CASE("preprocessor: #include in inactive branch")
{
  using yk::asteroid::preprocess::include_handler_t;
  using yk::asteroid::preprocess::include_result;

  bool called = false;
  include_handler_t handler = [&called](std::string_view) -> std::expected<include_result, yk::asteroid::preprocess::include_error> {
    called = true;
    return include_result{"int bad;\n"};
  };

  preprocessor pp{"#if 0\n#include \"foo.h\"\n#endif\n", handler};
  pp.run();

  CHECK(!called);
  CHECK(pp.text_lines.empty());
}

TEST_CASE("preprocessor: #include without handler")
{
  preprocessor pp{"#include <stdio.h>\n"};
  pp.run();

  REQUIRE(pp.directives.size() == 1);
  CHECK(pp.directives[0].name.piece == "include");
}

TEST_CASE("preprocessor: object-like macro expansion")
{
  preprocessor pp{"#define FOO 42\nint x = FOO;\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  // int(0) ws(1) x(2) ws(3) =(4) ws(5) 42(6) ;(7)
  CHECK(tokens[0].piece == "int");
  CHECK(tokens[6].piece == "42");
}

TEST_CASE("preprocessor: object-like macro with multiple tokens")
{
  preprocessor pp{"#define EXPR 1 + 2\nint x = EXPR;\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  // int(0) ws(1) x(2) ws(3) =(4) ws(5) 1(6) ws(7) +(8) ws(9) 2(10) ;(11)
  CHECK(tokens[0].piece == "int");
  CHECK(tokens[6].piece == "1");
  CHECK(tokens[8].piece == "+");
  CHECK(tokens[10].piece == "2");
}

TEST_CASE("preprocessor: macro not expanded after #undef")
{
  preprocessor pp{"#define FOO 42\n#undef FOO\nint x = FOO;\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  // int(0) ws(1) x(2) ws(3) =(4) ws(5) FOO(6) ;(7)
  CHECK(tokens[6].piece == "FOO");
}

TEST_CASE("preprocessor: recursive macro expansion")
{
  preprocessor pp{"#define A B\n#define B 42\nint x = A;\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[6].piece == "42");
}

TEST_CASE("preprocessor: self-referential macro does not infinitely recurse")
{
  preprocessor pp{"#define FOO FOO\nint x = FOO;\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[6].piece == "FOO");
}

TEST_CASE("preprocessor: #if with macro expansion")
{
  preprocessor pp{"#define VERSION 3\n#if VERSION > 2\nint x;\n#endif\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  CHECK(pp.text_lines[0].tokens[0].piece == "int");
}

TEST_CASE("preprocessor: function-like macro")
{
  preprocessor pp{"#define ADD(a, b) a + b\nint x = ADD(1, 2);\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  // int(0) ws(1) x(2) ws(3) =(4) ws(5) 1(6) ws(7) +(8) ws(9) 2(10) ;(11)
  CHECK(tokens[6].piece == "1");
  CHECK(tokens[8].piece == "+");
  CHECK(tokens[10].piece == "2");
}

TEST_CASE("preprocessor: function-like macro with nested parens in arg")
{
  preprocessor pp{"#define F(x) x\nint a = F((1+2));\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  // int(0) ws(1) a(2) ws(3) =(4) ws(5) ((6) 1(7) +(8) 2(9) )(10) ;(11)
  CHECK(tokens[6].piece == "(");
  CHECK(tokens[7].piece == "1");
}

TEST_CASE("preprocessor: function-like macro not invoked without parens")
{
  preprocessor pp{"#define F(x) x\nint F;\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  // int(0) ws(1) F(2) ;(3)
  CHECK(tokens[2].piece == "F");
}

TEST_CASE("preprocessor: function-like macro no args")
{
  preprocessor pp{"#define F() 42\nint x = F();\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  // int(0) ws(1) x(2) ws(3) =(4) ws(5) 42(6) ;(7)
  CHECK(tokens[6].piece == "42");
}

TEST_CASE("preprocessor: macro redefinition")
{
  preprocessor pp{"#define FOO 1\n#define FOO 2\nint x = FOO;\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[6].piece == "2");
}

// --- # (stringizing) operator tests ---

TEST_CASE("preprocessor: # stringize simple identifier")
{
  preprocessor pp{"#define STR(x) #x\nSTR(hello)\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[0].piece == "\"hello\"");
  CHECK(tokens[0].kind == pp_token_kind::string_literal);
}

TEST_CASE("preprocessor: # stringize multiple tokens")
{
  preprocessor pp{"#define STR(x) #x\nSTR(a + b)\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[0].piece == "\"a + b\"");
}

TEST_CASE("preprocessor: # stringize empty argument")
{
  preprocessor pp{"#define STR(x) #x\nSTR()\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[0].piece == "\"\"");
}

TEST_CASE("preprocessor: # stringize string literal escapes quotes")
{
  preprocessor pp{"#define STR(x) #x\nSTR(\"hello\")\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[0].piece == "\"\\\"hello\\\"\"");
}

TEST_CASE("preprocessor: # stringize variadic args")
{
  preprocessor pp{R"(#define STR(...) #__VA_ARGS__
STR(a, b, c)
)"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[0].piece == "\"a, b, c\"");
}

// --- ## (token pasting) operator tests ---

TEST_CASE("preprocessor: ## paste two identifiers")
{
  preprocessor pp{"#define PASTE(a, b) a ## b\nint PASTE(foo, bar);\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  // int(0) ws(1) foobar(2) ;(3)
  CHECK(tokens[2].piece == "foobar");
  CHECK(tokens[2].kind == pp_token_kind::identifier);
}

TEST_CASE("preprocessor: ## paste identifier and number")
{
  preprocessor pp{"#define MAKE(prefix, n) prefix ## n\nint MAKE(var, 42);\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[2].piece == "var42");
}

TEST_CASE("preprocessor: ## paste with fixed token")
{
  preprocessor pp{"#define MAKE_FN(name) fn_ ## name\nint MAKE_FN(test);\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[2].piece == "fn_test");
}

TEST_CASE("preprocessor: ## paste with empty argument")
{
  preprocessor pp{"#define PASTE(a, b) a ## b\nint PASTE(, foo);\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[2].piece == "foo");
}

TEST_CASE("preprocessor: ## paste creating number")
{
  preprocessor pp{"#define VER(a, b) a ## b\nint x = VER(1, 0);\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[6].piece == "10");
}

TEST_CASE("preprocessor: # and ## in same macro")
{
  preprocessor pp{R"(#define MAKE(name) name ## _id = #name
MAKE(foo)
)"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[0].piece == "foo_id");
  CHECK(tokens[1].piece == "=");
  // Find the stringized token
  bool found_str = false;
  for (auto const& t : tokens) {
    if (t.kind == pp_token_kind::string_literal && t.piece == "\"foo\"") {
      found_str = true;
      break;
    }
  }
  CHECK(found_str);
}

TEST_CASE("preprocessor: ## without whitespace")
{
  preprocessor pp{"#define GLUE(a, b) a##b\nint GLUE(x, y);\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[2].piece == "xy");
}

TEST_CASE("preprocessor: ## chained triple paste")
{
  preprocessor pp{"#define CAT3(a, b, c) a ## b ## c\nint CAT3(x, y, z);\n"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[2].piece == "xyz");
}

TEST_CASE("preprocessor: ## paste then expand")
{
  preprocessor pp{R"(#define HELPER(n) helper_ ## n
#define INDIRECT(n) HELPER(n)
int INDIRECT(42);
)"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[2].piece == "helper_42");
}

TEST_CASE("preprocessor: # stringize variadic with commas")
{
  preprocessor pp{R"(#define VSTR(...) #__VA_ARGS__
VSTR(a, b, c)
)"};
  pp.run();

  REQUIRE(pp.text_lines.size() == 1);
  auto const& tokens = pp.text_lines[0].tokens;
  CHECK(tokens[0].piece == "\"a, b, c\"");
}
