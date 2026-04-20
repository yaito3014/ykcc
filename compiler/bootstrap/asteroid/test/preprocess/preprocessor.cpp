#include "test.hpp"

#include <yk/asteroid/preprocess/lexer.hpp>
#include <yk/asteroid/preprocess/line_splicer.hpp>
#include <yk/asteroid/preprocess/preprocessor.hpp>

#include <vector>

using yk::asteroid::diagnostic;
using yk::asteroid::directive_kind;
using yk::asteroid::lexer;
using yk::asteroid::line_splicer;
using yk::asteroid::parsed_directive;
using yk::asteroid::pp_token;
using yk::asteroid::pp_token_kind;
using yk::asteroid::preprocessor;

namespace {

struct pp_output {
  std::vector<pp_token> tokens;
  std::vector<parsed_directive> directives;
  std::vector<diagnostic> diags;
};

pp_output run(std::string_view src)
{
  static line_splicer storage{""};
  storage = line_splicer{src};
  pp_output out;
  auto sink = [&out](diagnostic const& d) { out.diags.push_back(d); };
  auto handler = [&out](parsed_directive const& d) { out.directives.push_back(d); };
  lexer lx{storage, "<test>", sink};
  preprocessor pp{lx, sink, handler};
  while (!pp.at_end()) {
    auto t = pp.next();
    if (t.kind == pp_token_kind::end_of_file) break;
    out.tokens.push_back(t);
  }
  return out;
}

std::vector<std::string_view> non_trivial_spellings(std::vector<pp_token> const& toks)
{
  std::vector<std::string_view> out;
  for (auto const& t : toks) {
    if (t.kind != pp_token_kind::whitespace && t.kind != pp_token_kind::newline) {
      out.push_back(t.spelling);
    }
  }
  return out;
}

}  // namespace

TEST_CASE("preprocessor: plain code passes through")
{
  auto out = run("int x = 1;\n");
  CHECK(out.directives.empty());
  std::vector<std::string_view> spellings;
  for (auto const& t : out.tokens) {
    if (t.kind != pp_token_kind::whitespace && t.kind != pp_token_kind::newline) {
      spellings.push_back(t.spelling);
    }
  }
  CHECK(spellings == std::vector<std::string_view>{"int", "x", "=", "1", ";"});
}

TEST_CASE("preprocessor: null directive")
{
  auto out = run("#\n");
  REQUIRE(out.directives.size() == 1);
  CHECK(out.directives[0].kind == directive_kind::null_);
  CHECK(out.tokens.empty());
}

TEST_CASE("preprocessor: #define captured")
{
  auto out = run("#define X 42\n");
  REQUIRE(out.directives.size() == 1);
  CHECK(out.directives[0].kind == directive_kind::define);
  CHECK(out.directives[0].name_spelling == "define");
}

TEST_CASE("preprocessor: all C-style directives classified")
{
  struct case_ {
    std::string_view name;
    directive_kind kind;
  };
  case_ const cases[] = {
      {"include", directive_kind::include},
      {"embed", directive_kind::embed},
      {"define", directive_kind::define},
      {"undef", directive_kind::undef},
      {"if", directive_kind::if_},
      {"ifdef", directive_kind::ifdef},
      {"ifndef", directive_kind::ifndef},
      {"elif", directive_kind::elif},
      {"elifdef", directive_kind::elifdef},
      {"elifndef", directive_kind::elifndef},
      {"else", directive_kind::else_},
      {"endif", directive_kind::endif},
      {"line", directive_kind::line},
      {"error", directive_kind::error},
      {"warning", directive_kind::warning},
      {"pragma", directive_kind::pragma},
  };
  for (auto const& c : cases) {
    std::string src = "#";
    src += c.name;
    src += "\n";
    auto out = run(src);
    REQUIRE(out.directives.size() == 1);
    CHECK(out.directives[0].kind == c.kind);
    CHECK(out.directives[0].name_spelling == c.name);
  }
}

TEST_CASE("preprocessor: unknown directive name")
{
  auto out = run("#frobnicate 1 2\n");
  REQUIRE(out.directives.size() == 1);
  CHECK(out.directives[0].kind == directive_kind::unknown);
}

TEST_CASE("preprocessor: #include with angle header-name")
{
  auto out = run("#include <vector>\n");
  REQUIRE(out.directives.size() == 1);
  CHECK(out.directives[0].kind == directive_kind::include);
  // Tokens should contain the header-name (and possibly leading whitespace).
  bool found_hn = false;
  for (auto const& t : out.directives[0].tokens) {
    if (t.kind == pp_token_kind::header_name) {
      found_hn = true;
      CHECK(t.spelling == "<vector>");
    }
  }
  CHECK(found_hn);
}

TEST_CASE("preprocessor: #include with quoted header-name")
{
  auto out = run("#include \"foo.h\"\n");
  REQUIRE(out.directives.size() == 1);
  CHECK(out.directives[0].kind == directive_kind::include);
  bool found_hn = false;
  for (auto const& t : out.directives[0].tokens) {
    if (t.kind == pp_token_kind::header_name) {
      found_hn = true;
      CHECK(t.spelling == "\"foo.h\"");
    }
  }
  CHECK(found_hn);
}

TEST_CASE("preprocessor: #embed also recognizes header-name")
{
  auto out = run("#embed <image.png>\n");
  REQUIRE(out.directives.size() == 1);
  CHECK(out.directives[0].kind == directive_kind::embed);
  bool found_hn = false;
  for (auto const& t : out.directives[0].tokens) {
    if (t.kind == pp_token_kind::header_name) {
      found_hn = true;
      CHECK(t.spelling == "<image.png>");
    }
  }
  CHECK(found_hn);
}

TEST_CASE("preprocessor: directive tokens preserve whitespace")
{
  auto out = run("#define   X   42\n");
  REQUIRE(out.directives.size() == 1);
  auto const& d = out.directives[0];
  bool has_whitespace = false;
  for (auto const& t : d.tokens) {
    if (t.kind == pp_token_kind::whitespace) has_whitespace = true;
  }
  CHECK(has_whitespace);
}

TEST_CASE("preprocessor: # only at line start is a directive")
{
  auto out = run("x # y\n");
  CHECK(out.directives.empty());
  std::vector<std::string_view> spellings;
  for (auto const& t : out.tokens) {
    if (t.kind != pp_token_kind::whitespace && t.kind != pp_token_kind::newline) {
      spellings.push_back(t.spelling);
    }
  }
  CHECK(spellings == std::vector<std::string_view>{"x", "#", "y"});
}

TEST_CASE("preprocessor: whitespace before # still qualifies as line start")
{
  auto out = run("   #define X 1\n");
  REQUIRE(out.directives.size() == 1);
  CHECK(out.directives[0].kind == directive_kind::define);
}

TEST_CASE("preprocessor: multiple directives across lines")
{
  auto out = run("#define A 1\n#define B 2\n#undef A\n");
  REQUIRE(out.directives.size() == 3);
  CHECK(out.directives[0].kind == directive_kind::define);
  CHECK(out.directives[1].kind == directive_kind::define);
  CHECK(out.directives[2].kind == directive_kind::undef);
}

TEST_CASE("preprocessor: directive at EOF without trailing newline")
{
  auto out = run("#define X 42");
  REQUIRE(out.directives.size() == 1);
  CHECK(out.directives[0].kind == directive_kind::define);
}

TEST_CASE("preprocessor: code between directives passes through")
{
  auto out = run("#define X 1\nint y;\n#undef X\n");
  REQUIRE(out.directives.size() == 2);
  std::vector<std::string_view> code;
  for (auto const& t : out.tokens) {
    if (t.kind != pp_token_kind::whitespace && t.kind != pp_token_kind::newline) {
      code.push_back(t.spelling);
    }
  }
  CHECK(code == std::vector<std::string_view>{"int", "y", ";"});
}

TEST_CASE("preprocessor: directive source location points at #")
{
  auto out = run("\n\n   #define X 1\n");
  REQUIRE(out.directives.size() == 1);
  CHECK(out.directives[0].location.line == 3);
  CHECK(out.directives[0].location.column == 4);
}

TEST_CASE("expand: object-like macro replaces identifier")
{
  auto out = run("#define X 42\nX\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"42"});
}

TEST_CASE("expand: undefined identifier passes through unchanged")
{
  auto out = run("Y\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"Y"});
}

TEST_CASE("expand: non-identifier tokens never expand")
{
  auto out = run("#define + -\n+\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"+"});
}

TEST_CASE("expand: self-referential macro terminates")
{
  auto out = run("#define X X\nX\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"X"});
}

TEST_CASE("expand: mutually recursive macros terminate at first repeat")
{
  auto out = run("#define A B\n#define B A\nA\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"A"});
}

TEST_CASE("expand: rescan replaces a macro inside another macro's body")
{
  auto out = run("#define A B\n#define B 42\nA\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"42"});
}

TEST_CASE("expand: multi-token replacement emits all tokens")
{
  auto out = run("#define PI 3 . 14\nPI\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"3", ".", "14"});
}

TEST_CASE("expand: #undef disables subsequent expansion")
{
  auto out = run("#define X 1\n#undef X\nX\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"X"});
}

TEST_CASE("expand: redefinition replaces the previous body")
{
  auto out = run("#define X 1\n#undef X\n#define X 2\nX\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"2"});
}

TEST_CASE("expand: macro used multiple times expands each occurrence")
{
  auto out = run("#define X 42\nX X X\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"42", "42", "42"});
}

TEST_CASE("expand: does not touch identifiers inside string literals")
{
  auto out = run("#define X 42\n\"X\"\n");
  auto spellings = non_trivial_spellings(out.tokens);
  REQUIRE(spellings.size() == 1);
  CHECK(spellings[0] == "\"X\"");
}

TEST_CASE("expand: empty replacement removes the identifier")
{
  auto out = run("#define X\nX y\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"y"});
}

TEST_CASE("expand: redefinition with different body emits a diagnostic")
{
  auto out = run("#define X 1\n#define X 2\n");
  bool saw_warning = false;
  for (auto const& d : out.diags) {
    if (d.level == yk::asteroid::diagnostic_level::warning) saw_warning = true;
  }
  CHECK(saw_warning);
}

TEST_CASE("expand: redefinition with identical body is silent")
{
  auto out = run("#define X 1\n#define X 1\n");
  bool saw_warning = false;
  for (auto const& d : out.diags) {
    if (d.level == yk::asteroid::diagnostic_level::warning) saw_warning = true;
  }
  CHECK_FALSE(saw_warning);
}

TEST_CASE("expand: function-like macro without '(' passes through unexpanded")
{
  auto out = run("#define F(x) x\nF\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"F"});
}

TEST_CASE("expand: function-like macro with one argument")
{
  auto out = run("#define F(x) x + 1\nF(5)\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"5", "+", "1"});
}

TEST_CASE("expand: function-like macro with two arguments")
{
  auto out = run("#define ADD(a,b) a + b\nADD(3, 4)\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"3", "+", "4"});
}

TEST_CASE("expand: function-like macro with zero parameters")
{
  auto out = run("#define PI() 3\nPI()\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"3"});
}

TEST_CASE("expand: function-like macro with empty replacement")
{
  auto out = run("#define NOP(x)\nNOP(ignored)\n");
  CHECK(non_trivial_spellings(out.tokens).empty());
}

TEST_CASE("expand: parameter appearing multiple times substitutes each occurrence")
{
  auto out = run("#define DUP(x) x x\nDUP(hi)\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"hi", "hi"});
}

TEST_CASE("expand: argument tokens are expanded before substitution")
{
  auto out = run("#define A 5\n#define F(x) x\nF(A)\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"5"});
}

TEST_CASE("expand: call whose result is itself a macro name is rescanned")
{
  auto out = run("#define A B\n#define B 42\n#define F(x) x\nF(A)\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"42"});
}

TEST_CASE("expand: nested parens inside argument are preserved")
{
  auto out = run("#define ID(x) x\nID((1,2))\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"(", "1", ",", "2", ")"});
}

TEST_CASE("expand: nested function-like call in argument")
{
  auto out = run("#define F(x) x\nF(F(7))\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"7"});
}

TEST_CASE("expand: call may span newlines between name and arguments")
{
  auto out = run("#define F(x) x\nF\n(\n9\n)\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"9"});
}

TEST_CASE("expand: wrong number of arguments emits an error diagnostic")
{
  auto out = run("#define F(x,y) x\nF(1)\n");
  bool saw_error = false;
  for (auto const& d : out.diags) {
    if (d.level == yk::asteroid::diagnostic_level::error) saw_error = true;
  }
  CHECK(saw_error);
}

TEST_CASE("expand: function-like self-recursion terminates via hideset")
{
  auto out = run("#define F(x) F(x)\nF(1)\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"F", "(", "1", ")"});
}

TEST_CASE("expand: parameter matching uses name, not identity")
{
  auto out = run("#define F(x) x + y\nF(1)\n");
  CHECK(non_trivial_spellings(out.tokens) == std::vector<std::string_view>{"1", "+", "y"});
}

TEST_CASE("expand: macros() accessor reflects registered definitions")
{
  line_splicer storage{"#define A 1\n#define B 2\n#undef A\n"};
  lexer lx{storage, "<test>"};
  preprocessor pp{lx};
  while (!pp.at_end()) {
    auto t = pp.next();
    if (t.kind == pp_token_kind::end_of_file) break;
  }
  CHECK_FALSE(pp.macros().defined("A"));
  CHECK(pp.macros().defined("B"));
  auto b = pp.macros().lookup("B");
  REQUIRE(b.has_value());
  CHECK(b->replacement.size() == 1);
  CHECK(b->replacement[0].spelling == "2");
}
