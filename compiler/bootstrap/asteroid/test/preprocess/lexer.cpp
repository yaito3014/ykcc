#include "test.hpp"

#include <yk/asteroid/preprocess/lexer.hpp>
#include <yk/asteroid/preprocess/line_splicer.hpp>

#include <vector>

using yk::asteroid::diagnostic;
using yk::asteroid::diagnostic_level;
using yk::asteroid::lexer;
using yk::asteroid::line_splicer;
using yk::asteroid::pp_token;
using yk::asteroid::pp_token_kind;

namespace {

struct lex_output {
  std::vector<pp_token> tokens;
  std::vector<diagnostic> diags;
};

// Returns tokens for a source that is assumed to already terminate with a newline
// when appropriate. The synthesized trailing newline from line_splicer is dropped.
lex_output lex_all(std::string_view src, std::string_view file = "<test>")
{
  static line_splicer storage{""};
  storage = line_splicer{src};
  lex_output out;
  lexer lx{storage, file, [&](diagnostic const& d) { out.diags.push_back(d); }};
  while (!lx.at_end()) {
    auto tok = lx.next();
    if (tok.kind == pp_token_kind::end_of_file) break;
    out.tokens.push_back(tok);
  }
  bool const original_ended_with_newline = !src.empty() && src.back() == '\n';
  if (!original_ended_with_newline && !out.tokens.empty()
      && out.tokens.back().kind == pp_token_kind::newline) {
    out.tokens.pop_back();
  }
  return out;
}

std::vector<pp_token_kind> kinds(lex_output const& o)
{
  std::vector<pp_token_kind> ks;
  for (auto const& t : o.tokens) ks.push_back(t.kind);
  return ks;
}

}  // namespace

TEST_CASE("lexer: empty input yields only EOF")
{
  auto out = lex_all("");
  CHECK(out.tokens.empty());
}

TEST_CASE("lexer: horizontal whitespace coalesces")
{
  auto out = lex_all("  \t\tabc");
  REQUIRE(out.tokens.size() == 2);
  CHECK(out.tokens[0].kind == pp_token_kind::whitespace);
  CHECK(out.tokens[0].spelling == "  \t\t");
  CHECK(out.tokens[1].kind == pp_token_kind::identifier);
  CHECK(out.tokens[1].spelling == "abc");
}

TEST_CASE("lexer: newline is its own token")
{
  auto out = lex_all("a\nb");
  auto ks = kinds(out);
  REQUIRE(ks.size() == 3);
  CHECK(ks[0] == pp_token_kind::identifier);
  CHECK(ks[1] == pp_token_kind::newline);
  CHECK(ks[2] == pp_token_kind::identifier);
  CHECK(out.tokens[1].spelling == "\n");
}

TEST_CASE("lexer: line comment treated as whitespace")
{
  auto out = lex_all("a // comment here\nb");
  auto ks = kinds(out);
  REQUIRE(ks.size() == 5);
  CHECK(ks[0] == pp_token_kind::identifier);
  CHECK(ks[1] == pp_token_kind::whitespace);
  CHECK(ks[2] == pp_token_kind::whitespace);
  CHECK(out.tokens[2].spelling == "// comment here");
  CHECK(ks[3] == pp_token_kind::newline);
  CHECK(ks[4] == pp_token_kind::identifier);
}

TEST_CASE("lexer: block comment treated as whitespace")
{
  auto out = lex_all("a/* multi\nline */b");
  REQUIRE(out.tokens.size() == 3);
  CHECK(out.tokens[0].kind == pp_token_kind::identifier);
  CHECK(out.tokens[1].kind == pp_token_kind::whitespace);
  CHECK(out.tokens[1].spelling == "/* multi\nline */");
  CHECK(out.tokens[2].kind == pp_token_kind::identifier);
}

TEST_CASE("lexer: unterminated block comment reports error")
{
  auto out = lex_all("/* oops");
  REQUIRE(!out.diags.empty());
  CHECK(out.diags[0].level == diagnostic_level::error);
}

TEST_CASE("lexer: identifier with digits and underscores")
{
  auto out = lex_all("foo_Bar123");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::identifier);
  CHECK(out.tokens[0].spelling == "foo_Bar123");
}

TEST_CASE("lexer: identifier starting with underscore")
{
  auto out = lex_all("_x");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::identifier);
}

TEST_CASE("lexer: simple pp-number")
{
  auto out = lex_all("123");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::pp_number);
  CHECK(out.tokens[0].spelling == "123");
}

TEST_CASE("lexer: pp-number starting with dot")
{
  auto out = lex_all(".5");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::pp_number);
  CHECK(out.tokens[0].spelling == ".5");
}

TEST_CASE("lexer: pp-number with exponent and sign")
{
  auto out = lex_all("1.0e+10");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::pp_number);
  CHECK(out.tokens[0].spelling == "1.0e+10");
}

TEST_CASE("lexer: pp-number with hex float exponent")
{
  auto out = lex_all("0x1.8p-3");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::pp_number);
  CHECK(out.tokens[0].spelling == "0x1.8p-3");
}

TEST_CASE("lexer: pp-number with digit separators")
{
  auto out = lex_all("1'234'567");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::pp_number);
  CHECK(out.tokens[0].spelling == "1'234'567");
}

TEST_CASE("lexer: pp-number user-defined suffix")
{
  auto out = lex_all("42ull");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::pp_number);
  CHECK(out.tokens[0].spelling == "42ull");
}

TEST_CASE("lexer: 3-char punctuators")
{
  for (std::string_view s : {"<<=", ">>=", "...", "->*", "<=>"}) {
    auto out = lex_all(s);
    REQUIRE(out.tokens.size() == 1);
    CHECK(out.tokens[0].kind == pp_token_kind::punctuator);
    CHECK(out.tokens[0].spelling == s);
  }
}

TEST_CASE("lexer: 2-char punctuators")
{
  for (std::string_view s : {"::", "->", "++", "--", "<=", ">=", "==", "!=", "&&", "||", "<<", ">>", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=", ".*"}) {
    auto out = lex_all(s);
    REQUIRE(out.tokens.size() == 1);
    CHECK(out.tokens[0].kind == pp_token_kind::punctuator);
    CHECK(out.tokens[0].spelling == s);
  }
}

TEST_CASE("lexer: reflection operator ^^")
{
  auto out = lex_all("^^T");
  REQUIRE(out.tokens.size() == 2);
  CHECK(out.tokens[0].kind == pp_token_kind::punctuator);
  CHECK(out.tokens[0].spelling == "^^");
  CHECK(out.tokens[1].kind == pp_token_kind::identifier);
  CHECK(out.tokens[1].spelling == "T");
}

TEST_CASE("lexer: splicer delimiters [: and :]")
{
  auto out = lex_all("[: R :]");
  std::vector<std::string_view> spellings;
  for (auto const& t : out.tokens) {
    if (t.kind != pp_token_kind::whitespace) spellings.push_back(t.spelling);
  }
  CHECK(spellings == std::vector<std::string_view>{"[:", "R", ":]"});
}

TEST_CASE("lexer: a[::b] tokenizes as [ :: ] not [: :]")
{
  auto out = lex_all("a[::b]");
  REQUIRE(out.tokens.size() == 5);
  CHECK(out.tokens[0].spelling == "a");
  CHECK(out.tokens[1].spelling == "[");
  CHECK(out.tokens[2].spelling == "::");
  CHECK(out.tokens[3].spelling == "b");
  CHECK(out.tokens[4].spelling == "]");
}

TEST_CASE("lexer: <::T disambiguates to < ::")
{
  auto out = lex_all("<::T");
  REQUIRE(out.tokens.size() == 3);
  CHECK(out.tokens[0].spelling == "<");
  CHECK(out.tokens[1].spelling == "::");
  CHECK(out.tokens[2].spelling == "T");
}

TEST_CASE("lexer: <:: followed by : is the digraph <: :")
{
  auto out = lex_all("<:::");
  REQUIRE(out.tokens.size() == 2);
  CHECK(out.tokens[0].spelling == "<:");
  CHECK(out.tokens[1].spelling == "::");
}

TEST_CASE("lexer: <:: followed by > is the digraph <: :>")
{
  auto out = lex_all("<::>");
  REQUIRE(out.tokens.size() == 2);
  CHECK(out.tokens[0].spelling == "<:");
  CHECK(out.tokens[1].spelling == ":>");
}

TEST_CASE("lexer: [:> tokenizes as [ and :>")
{
  auto out = lex_all("[:>");
  REQUIRE(out.tokens.size() == 2);
  CHECK(out.tokens[0].spelling == "[");
  CHECK(out.tokens[1].spelling == ":>");
}

TEST_CASE("lexer: 1-char punctuators")
{
  for (std::string_view s : {"{", "}", "[", "]", "(", ")", ";", ":", "?", ".", "~", "!", "+", "-", "*", "/", "%", "^", "&", "|", "=", "<", ">", ",", "#"}) {
    auto out = lex_all(s);
    REQUIRE(out.tokens.size() == 1);
    CHECK(out.tokens[0].kind == pp_token_kind::punctuator);
    CHECK(out.tokens[0].spelling == s);
  }
}

TEST_CASE("lexer: digraph punctuators")
{
  auto out = lex_all("<% %> <: :> %:");
  std::vector<std::string_view> puncts;
  for (auto const& t : out.tokens) {
    if (t.kind == pp_token_kind::punctuator) puncts.push_back(t.spelling);
  }
  CHECK(puncts == std::vector<std::string_view>{"<%", "%>", "<:", ":>", "%:"});
}

TEST_CASE("lexer: %:%: digraph recognized as 4-char punctuator")
{
  auto out = lex_all("%:%:");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].spelling == "%:%:");
}

TEST_CASE("lexer: maximal munch prefers longest punctuator")
{
  auto out = lex_all("<<=");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].spelling == "<<=");
}

TEST_CASE("lexer: a+++b -> a ++ + b")
{
  auto out = lex_all("a+++b");
  REQUIRE(out.tokens.size() == 4);
  CHECK(out.tokens[0].spelling == "a");
  CHECK(out.tokens[1].spelling == "++");
  CHECK(out.tokens[2].spelling == "+");
  CHECK(out.tokens[3].spelling == "b");
}

TEST_CASE("lexer: source_location tracks line and column")
{
  auto out = lex_all("foo\n  bar");
  REQUIRE(out.tokens.size() >= 4);
  CHECK(out.tokens[0].location.line == 1);
  CHECK(out.tokens[0].location.column == 1);
  CHECK(out.tokens[3].spelling == "bar");
  CHECK(out.tokens[3].location.line == 2);
  CHECK(out.tokens[3].location.column == 3);
}

TEST_CASE("lexer: simple string literal")
{
  auto out = lex_all("\"hello\"");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::string_literal);
  CHECK(out.tokens[0].spelling == "\"hello\"");
}

TEST_CASE("lexer: empty string literal")
{
  auto out = lex_all("\"\"");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::string_literal);
  CHECK(out.tokens[0].spelling == "\"\"");
}

TEST_CASE("lexer: string literal with escape sequences")
{
  auto out = lex_all("\"a\\n\\t\\\"b\"");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::string_literal);
  CHECK(out.tokens[0].spelling == "\"a\\n\\t\\\"b\"");
}

TEST_CASE("lexer: string literal encoding prefixes")
{
  for (std::string_view prefix : {"L", "u8", "u", "U"}) {
    std::string src = std::string(prefix) + "\"x\"";
    auto out = lex_all(src);
    REQUIRE(out.tokens.size() == 1);
    CHECK(out.tokens[0].kind == pp_token_kind::string_literal);
    CHECK(out.tokens[0].spelling == src);
  }
}

TEST_CASE("lexer: string literal with UDL suffix")
{
  auto out = lex_all("\"abc\"sv");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::string_literal);
  CHECK(out.tokens[0].spelling == "\"abc\"sv");
}

TEST_CASE("lexer: unterminated string reports error")
{
  auto out = lex_all("\"oops");
  REQUIRE(!out.diags.empty());
  CHECK(out.diags[0].level == diagnostic_level::error);
}

TEST_CASE("lexer: string literal does not span newlines")
{
  auto out = lex_all("\"a\nb\"");
  // First '"a' is unterminated (stops at newline); error reported; then newline; then '\"b\"'.
  REQUIRE(!out.diags.empty());
  CHECK(out.tokens[0].kind == pp_token_kind::string_literal);
  CHECK(out.tokens[0].spelling == "\"a");
}

TEST_CASE("lexer: simple character literal")
{
  auto out = lex_all("'a'");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::character_literal);
  CHECK(out.tokens[0].spelling == "'a'");
}

TEST_CASE("lexer: character literal with escape")
{
  for (std::string_view src : {"'\\n'", "'\\''", "'\\\\'", "'\\x41'", "'\\0'"}) {
    auto out = lex_all(src);
    REQUIRE(out.tokens.size() == 1);
    CHECK(out.tokens[0].kind == pp_token_kind::character_literal);
    CHECK(out.tokens[0].spelling == src);
  }
}

TEST_CASE("lexer: character literal encoding prefixes")
{
  for (std::string_view prefix : {"L", "u8", "u", "U"}) {
    std::string src = std::string(prefix) + "'a'";
    auto out = lex_all(src);
    REQUIRE(out.tokens.size() == 1);
    CHECK(out.tokens[0].kind == pp_token_kind::character_literal);
    CHECK(out.tokens[0].spelling == src);
  }
}

TEST_CASE("lexer: u identifier without quote falls through to identifier")
{
  auto out = lex_all("u8foo");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::identifier);
  CHECK(out.tokens[0].spelling == "u8foo");
}

TEST_CASE("lexer: L alone is an identifier")
{
  auto out = lex_all("L x");
  CHECK(out.tokens[0].kind == pp_token_kind::identifier);
  CHECK(out.tokens[0].spelling == "L");
}

TEST_CASE("lexer: raw string literal basic")
{
  auto out = lex_all("R\"(hello)\"");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::string_literal);
  CHECK(out.tokens[0].spelling == "R\"(hello)\"");
}

TEST_CASE("lexer: raw string literal with delimiter")
{
  auto out = lex_all("R\"foo(hello)foo\"");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::string_literal);
  CHECK(out.tokens[0].spelling == "R\"foo(hello)foo\"");
}

TEST_CASE("lexer: raw string contains quote and parens")
{
  auto out = lex_all("R\"d(a \"b\" (c))d\"");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].spelling == "R\"d(a \"b\" (c))d\"");
}

TEST_CASE("lexer: raw string with encoding prefix")
{
  for (std::string_view prefix : {"L", "u8", "u", "U"}) {
    std::string src = std::string(prefix) + "R\"(x)\"";
    auto out = lex_all(src);
    REQUIRE(out.tokens.size() == 1);
    CHECK(out.tokens[0].kind == pp_token_kind::string_literal);
    CHECK(out.tokens[0].spelling == src);
  }
}

TEST_CASE("lexer: raw string with splice in content preserves backslash-newline")
{
  // Source has `\` immediately followed by `\n` inside raw string. Phase 2 would have
  // spliced it, but raw-string rules revert the splice — the token's spelling keeps
  // the original `\<newline>` bytes.
  std::string_view src = "R\"(a\\\nb)\"";
  auto out = lex_all(src);
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::string_literal);
  CHECK(out.tokens[0].spelling == src);
}

TEST_CASE("lexer: raw string unterminated reports error")
{
  auto out = lex_all("R\"(no end");
  REQUIRE(!out.diags.empty());
  CHECK(out.diags[0].level == diagnostic_level::error);
}

TEST_CASE("lexer: raw string with UDL suffix")
{
  auto out = lex_all("R\"(x)\"_s");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::string_literal);
  CHECK(out.tokens[0].spelling == "R\"(x)\"_s");
}

TEST_CASE("lexer: R not followed by quote is identifier")
{
  auto out = lex_all("Rfoo");
  REQUIRE(out.tokens.size() == 1);
  CHECK(out.tokens[0].kind == pp_token_kind::identifier);
  CHECK(out.tokens[0].spelling == "Rfoo");
}

TEST_CASE("lexer: header-name with angle brackets")
{
  static line_splicer ls{"<vector>"};
  lexer lx{ls, "<test>"};
  auto tok = lx.try_lex_header_name();
  REQUIRE(tok.has_value());
  CHECK(tok->kind == pp_token_kind::header_name);
  CHECK(tok->spelling == "<vector>");
}

TEST_CASE("lexer: header-name with quotes")
{
  static line_splicer ls{"\"local.h\""};
  lexer lx{ls, "<test>"};
  auto tok = lx.try_lex_header_name();
  REQUIRE(tok.has_value());
  CHECK(tok->kind == pp_token_kind::header_name);
  CHECK(tok->spelling == "\"local.h\"");
}

TEST_CASE("lexer: header-name is not consumed by next() outside directive context")
{
  auto out = lex_all("<vector>");
  // Should lex as `<` `vector` `>`, not a header-name.
  std::vector<std::string_view> spellings;
  for (auto const& t : out.tokens) {
    if (t.kind != pp_token_kind::whitespace) spellings.push_back(t.spelling);
  }
  CHECK(spellings == std::vector<std::string_view>{"<", "vector", ">"});
}

TEST_CASE("lexer: header-name rejects unterminated")
{
  static line_splicer ls{"<oops"};
  lexer lx{ls, "<test>"};
  auto tok = lx.try_lex_header_name();
  CHECK(!tok.has_value());
}

TEST_CASE("lexer: header-name nullopt when no angle/quote")
{
  static line_splicer ls{"foo"};
  lexer lx{ls, "<test>"};
  auto tok = lx.try_lex_header_name();
  CHECK(!tok.has_value());
}

TEST_CASE("lexer: mixed sequence")
{
  auto out = lex_all("int x = 42;");
  std::vector<std::string_view> non_ws;
  for (auto const& t : out.tokens) {
    if (t.kind != pp_token_kind::whitespace) non_ws.push_back(t.spelling);
  }
  CHECK(non_ws == std::vector<std::string_view>{"int", "x", "=", "42", ";"});
}
