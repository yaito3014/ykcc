#include <catch2/catch_test_macros.hpp>

#include <yk/asteroid/preprocess/lexer.hpp>

#include <vector>

using yk::asteroid::preprocess::pp_token_kind;

TEST_CASE("lexer: basic tokenization")
{
  yk::asteroid::preprocess::lexer lex{"int x = 42;\n"};

  std::vector<yk::asteroid::preprocess::pp_token> tokens;
  for (auto const& tok : lex) {
    tokens.push_back(tok);
  }

  REQUIRE(tokens.size() == 9);

  CHECK(tokens[0].kind == pp_token_kind::identifier);
  CHECK(tokens[0].piece == "int");

  CHECK(tokens[1].kind == pp_token_kind::whitespace);
  CHECK(tokens[1].piece == " ");

  CHECK(tokens[2].kind == pp_token_kind::identifier);
  CHECK(tokens[2].piece == "x");

  CHECK(tokens[3].kind == pp_token_kind::whitespace);
  CHECK(tokens[3].piece == " ");

  CHECK(tokens[4].kind == pp_token_kind::op_or_punc);
  CHECK(tokens[4].piece == "=");

  CHECK(tokens[5].kind == pp_token_kind::whitespace);
  CHECK(tokens[5].piece == " ");

  CHECK(tokens[6].kind == pp_token_kind::number);
  CHECK(tokens[6].piece == "42");

  CHECK(tokens[7].kind == pp_token_kind::op_or_punc);
  CHECK(tokens[7].piece == ";");

  CHECK(tokens[8].kind == pp_token_kind::newline);
  CHECK(tokens[8].piece == "\n");
}

TEST_CASE("lexer: operators")
{
  yk::asteroid::preprocess::lexer lex{"a->*b"};

  std::vector<yk::asteroid::preprocess::pp_token> tokens;
  for (auto const& tok : lex) {
    tokens.push_back(tok);
  }

  REQUIRE(tokens.size() == 3);
  CHECK(tokens[0].kind == pp_token_kind::identifier);
  CHECK(tokens[0].piece == "a");
  CHECK(tokens[1].kind == pp_token_kind::op_or_punc);
  CHECK(tokens[1].piece == "->*");
  CHECK(tokens[2].kind == pp_token_kind::identifier);
  CHECK(tokens[2].piece == "b");
}

TEST_CASE("lexer: non-whitespace fallback")
{
  yk::asteroid::preprocess::lexer lex{"@$"};

  std::vector<yk::asteroid::preprocess::pp_token> tokens;
  for (auto const& tok : lex) {
    tokens.push_back(tok);
  }

  REQUIRE(tokens.size() == 2);
  CHECK(tokens[0].kind == pp_token_kind::non_whitespace_char);
  CHECK(tokens[0].piece == "@");
  CHECK(tokens[1].kind == pp_token_kind::non_whitespace_char);
  CHECK(tokens[1].piece == "$");
}

TEST_CASE("lexer: alternative tokens")
{
  yk::asteroid::preprocess::lexer lex{"a and b or not c"};

  std::vector<yk::asteroid::preprocess::pp_token> tokens;
  for (auto const& tok : lex) {
    tokens.push_back(tok);
  }

  REQUIRE(tokens.size() == 11);
  CHECK(tokens[0].kind == pp_token_kind::identifier);
  CHECK(tokens[0].piece == "a");
  CHECK(tokens[2].kind == pp_token_kind::op_or_punc);
  CHECK(tokens[2].piece == "and");
  CHECK(tokens[4].kind == pp_token_kind::identifier);
  CHECK(tokens[4].piece == "b");
  CHECK(tokens[6].kind == pp_token_kind::op_or_punc);
  CHECK(tokens[6].piece == "or");
  CHECK(tokens[8].kind == pp_token_kind::op_or_punc);
  CHECK(tokens[8].piece == "not");
  CHECK(tokens[10].kind == pp_token_kind::identifier);
  CHECK(tokens[10].piece == "c");
}

TEST_CASE("lexer: alternative token prefix is identifier")
{
  yk::asteroid::preprocess::lexer lex{"android"};

  std::vector<yk::asteroid::preprocess::pp_token> tokens;
  for (auto const& tok : lex) {
    tokens.push_back(tok);
  }

  REQUIRE(tokens.size() == 1);
  CHECK(tokens[0].kind == pp_token_kind::identifier);
  CHECK(tokens[0].piece == "android");
}

TEST_CASE("lexer: source location")
{
  yk::asteroid::preprocess::lexer lex{"ab cd\nef 12"};

  std::vector<yk::asteroid::preprocess::pp_token> tokens;
  for (auto const& tok : lex) {
    tokens.push_back(tok);
  }

  REQUIRE(tokens.size() == 7);

  // "ab" at line 1, column 1
  CHECK(tokens[0].location.line == 1);
  CHECK(tokens[0].location.column == 1);

  // " " at line 1, column 3
  CHECK(tokens[1].location.line == 1);
  CHECK(tokens[1].location.column == 3);

  // "cd" at line 1, column 4
  CHECK(tokens[2].location.line == 1);
  CHECK(tokens[2].location.column == 4);

  // "\n" at line 1, column 6
  CHECK(tokens[3].location.line == 1);
  CHECK(tokens[3].location.column == 6);

  // "ef" at line 2, column 1
  CHECK(tokens[4].location.line == 2);
  CHECK(tokens[4].location.column == 1);

  // " " at line 2, column 3
  CHECK(tokens[5].location.line == 2);
  CHECK(tokens[5].location.column == 3);

  // "12" at line 2, column 4
  CHECK(tokens[6].location.line == 2);
  CHECK(tokens[6].location.column == 4);
}

TEST_CASE("lexer: header-name not recognized by default")
{
  yk::asteroid::preprocess::lexer lex{"<stdio.h>"};

  std::vector<yk::asteroid::preprocess::pp_token> tokens;
  for (auto const& tok : lex) {
    tokens.push_back(tok);
  }

  // Without expect_header_name, '<' is lexed as op_or_punc
  CHECK(tokens[0].kind == pp_token_kind::op_or_punc);
  CHECK(tokens[0].piece == "<");
}

TEST_CASE("lexer: header-name recognized with next_header_name")
{
  yk::asteroid::preprocess::lexer lex{"#include <stdio.h>\n"};

  auto it = lex.begin();
  CHECK(it->piece == "#");
  ++it;
  CHECK(it->piece == "include");
  ++it;
  CHECK(it->kind == pp_token_kind::whitespace);
  it.next_header_name();
  CHECK(it->kind == pp_token_kind::header_name);
  CHECK(it->piece == "<stdio.h>");
}

TEST_CASE("lexer: <=> as header-name vs spaceship")
{
  {
    yk::asteroid::preprocess::lexer lex{"#include <=>\n"};

    auto it = lex.begin();
    CHECK(it->piece == "#");
    ++it;
    CHECK(it->piece == "include");
    ++it;
    CHECK(it->kind == pp_token_kind::whitespace);
    it.next_header_name();
    CHECK(it->kind == pp_token_kind::header_name);
    CHECK(it->piece == "<=>");
  }
  {
    yk::asteroid::preprocess::lexer lex{"<=>"};

    auto it = lex.begin();
    CHECK(it->kind == pp_token_kind::op_or_punc);
    CHECK(it->piece == "<=>");
  }
}

TEST_CASE("lexer: line comment")
{
  yk::asteroid::preprocess::lexer lex{"a // comment\nb"};

  std::vector<yk::asteroid::preprocess::pp_token> tokens;
  for (auto const& tok : lex) {
    tokens.push_back(tok);
  }

  REQUIRE(tokens.size() == 5);
  CHECK(tokens[0].piece == "a");
  CHECK(tokens[1].kind == pp_token_kind::whitespace);
  CHECK(tokens[1].piece == " ");
  CHECK(tokens[2].kind == pp_token_kind::whitespace);
  CHECK(tokens[2].piece == "// comment");
  CHECK(tokens[3].kind == pp_token_kind::newline);
  CHECK(tokens[4].piece == "b");
}

TEST_CASE("lexer: block comment")
{
  yk::asteroid::preprocess::lexer lex{"a /* comment */ b"};

  std::vector<yk::asteroid::preprocess::pp_token> tokens;
  for (auto const& tok : lex) {
    tokens.push_back(tok);
  }

  REQUIRE(tokens.size() == 5);
  CHECK(tokens[0].piece == "a");
  CHECK(tokens[1].kind == pp_token_kind::whitespace);
  CHECK(tokens[2].kind == pp_token_kind::whitespace);
  CHECK(tokens[2].piece == "/* comment */");
  CHECK(tokens[3].kind == pp_token_kind::whitespace);
  CHECK(tokens[4].piece == "b");
}

TEST_CASE("lexer: block comment spanning lines")
{
  yk::asteroid::preprocess::lexer lex{"a /* line1\nline2 */ b"};

  std::vector<yk::asteroid::preprocess::pp_token> tokens;
  for (auto const& tok : lex) {
    tokens.push_back(tok);
  }

  REQUIRE(tokens.size() == 5);
  CHECK(tokens[0].piece == "a");
  CHECK(tokens[1].kind == pp_token_kind::whitespace);
  CHECK(tokens[2].kind == pp_token_kind::whitespace);
  CHECK(tokens[2].piece == "/* line1\nline2 */");
  CHECK(tokens[3].kind == pp_token_kind::whitespace);
  CHECK(tokens[4].piece == "b");
}

TEST_CASE("lexer: comment in string not treated as comment")
{
  yk::asteroid::preprocess::lexer lex{"\"a // b\""};

  std::vector<yk::asteroid::preprocess::pp_token> tokens;
  for (auto const& tok : lex) {
    tokens.push_back(tok);
  }

  REQUIRE(tokens.size() == 1);
  CHECK(tokens[0].kind == pp_token_kind::string_literal);
  CHECK(tokens[0].piece == "\"a // b\"");
}

TEST_CASE("lexer: source location after multi-line block comment")
{
  yk::asteroid::preprocess::lexer lex{"a /* x\ny */ b"};

  std::vector<yk::asteroid::preprocess::pp_token> tokens;
  for (auto const& tok : lex) {
    tokens.push_back(tok);
  }

  // a at (1,1), " " at (1,2), "/* x\ny */" at (1,3), " " at (2,5), b at (2,6)
  REQUIRE(tokens.size() == 5);
  CHECK(tokens[0].location.line == 1);
  CHECK(tokens[0].location.column == 1);
  CHECK(tokens[2].location.line == 1);
  CHECK(tokens[2].location.column == 3);
  CHECK(tokens[3].location.line == 2);
  CHECK(tokens[3].location.column == 5);
  CHECK(tokens[4].location.line == 2);
  CHECK(tokens[4].location.column == 6);
}

TEST_CASE("lexer: empty input")
{
  yk::asteroid::preprocess::lexer lex{""};

  std::vector<yk::asteroid::preprocess::pp_token> tokens;
  for (auto const& tok : lex) {
    tokens.push_back(tok);
  }

  CHECK(tokens.empty());
}
