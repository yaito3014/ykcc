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

TEST_CASE("lexer: empty input")
{
  yk::asteroid::preprocess::lexer lex{""};

  std::vector<yk::asteroid::preprocess::pp_token> tokens;
  for (auto const& tok : lex) {
    tokens.push_back(tok);
  }

  CHECK(tokens.empty());
}
