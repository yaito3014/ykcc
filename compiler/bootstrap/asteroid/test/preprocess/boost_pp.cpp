#include <catch2/catch_test_macros.hpp>

#include <yk/asteroid/preprocess/lexer.hpp>
#include <yk/asteroid/preprocess/line_splicer.hpp>
#include <yk/asteroid/preprocess/preprocessor.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using yk::asteroid::diagnostic;
using yk::asteroid::lexer;
using yk::asteroid::line_splicer;
using yk::asteroid::pp_token_kind;
using yk::asteroid::preprocessor;

namespace fs = std::filesystem;

namespace {

std::vector<fs::path>& search_dirs()
{
  static std::vector<fs::path> v;
  return v;
}

bool fs_resolver(std::string_view header, std::string_view /*from*/,
                 std::string& out_path, std::string& out_content)
{
  if (header.size() < 2) return false;
  char open = header.front(), close = header.back();
  if (!((open == '<' && close == '>') || (open == '"' && close == '"'))) return false;
  std::string_view name = header.substr(1, header.size() - 2);
  for (auto const& dir : search_dirs()) {
    fs::path p = dir / std::string(name);
    std::ifstream in(p);
    if (!in) continue;
    std::ostringstream ss;
    ss << in.rdbuf();
    out_path = p.string();
    out_content = ss.str();
    return true;
  }
  return false;
}

struct pp_result {
  std::vector<std::string> spellings;
  std::vector<diagnostic> diags;
};

pp_result run(std::string_view src)
{
  static line_splicer storage{""};
  storage = line_splicer{src};
  pp_result out;
  auto sink = [&out](diagnostic const& d) { out.diags.push_back(d); };
  lexer lx{storage, "<test>", sink};
  preprocessor pp{lx, sink};
  pp.set_include_source(&fs_resolver);
  while (!pp.at_end()) {
    auto t = pp.next();
    if (t.kind == pp_token_kind::end_of_file) break;
    if (t.kind == pp_token_kind::whitespace || t.kind == pp_token_kind::newline) continue;
    out.spellings.emplace_back(t.spelling);
  }
  return out;
}

bool boost_pp_available()
{
  return fs::exists("/usr/include/boost/preprocessor/cat.hpp");
}

}  // namespace

TEST_CASE("Boost.PP: BOOST_PP_CAT")
{
  if (!boost_pp_available()) SKIP("Boost.PP not installed at /usr/include");
  search_dirs() = {"/usr/include"};
  auto out = run(
      "#include <boost/preprocessor/cat.hpp>\n"
      "BOOST_PP_CAT(foo, 42)\n");
  INFO("diags: " << out.diags.size());
  for (auto const& d : out.diags) INFO(d.message);
  REQUIRE(out.spellings.size() == 1);
  CHECK(out.spellings[0] == "foo42");
}

TEST_CASE("Boost.PP: BOOST_PP_INC")
{
  if (!boost_pp_available()) SKIP("Boost.PP not installed at /usr/include");
  search_dirs() = {"/usr/include"};
  auto out = run(
      "#include <boost/preprocessor/arithmetic/inc.hpp>\n"
      "BOOST_PP_INC(3)\n");
  for (auto const& d : out.diags) INFO(d.message);
  REQUIRE(out.spellings.size() == 1);
  CHECK(out.spellings[0] == "4");
}

TEST_CASE("Boost.PP: BOOST_PP_ADD")
{
  if (!boost_pp_available()) SKIP("Boost.PP not installed at /usr/include");
  search_dirs() = {"/usr/include"};
  auto out = run(
      "#include <boost/preprocessor/arithmetic/add.hpp>\n"
      "BOOST_PP_ADD(2, 3)\n");
  for (auto const& d : out.diags) INFO(d.message);
  REQUIRE(out.spellings.size() == 1);
  CHECK(out.spellings[0] == "5");
}

TEST_CASE("Boost.PP: BOOST_PP_REPEAT emits N copies")
{
  if (!boost_pp_available()) SKIP("Boost.PP not installed at /usr/include");
  search_dirs() = {"/usr/include"};
  auto out = run(
      "#include <boost/preprocessor/repetition/repeat.hpp>\n"
      "#define M(z, n, data) x ## n\n"
      "BOOST_PP_REPEAT(3, M, ~)\n");
  for (auto const& d : out.diags) INFO(d.message);
  REQUIRE(out.spellings.size() == 3);
  CHECK(out.spellings[0] == "x0");
  CHECK(out.spellings[1] == "x1");
  CHECK(out.spellings[2] == "x2");
}

TEST_CASE("Boost.PP: BOOST_PP_SEQ_FOR_EACH")
{
  if (!boost_pp_available()) SKIP("Boost.PP not installed at /usr/include");
  search_dirs() = {"/usr/include"};
  auto out = run(
      "#include <boost/preprocessor/seq/for_each.hpp>\n"
      "#define M(r, data, elem) elem ;\n"
      "BOOST_PP_SEQ_FOR_EACH(M, ~, (a)(b)(c))\n");
  for (auto const& d : out.diags) INFO(d.message);
  REQUIRE(out.spellings.size() == 6);
  CHECK(out.spellings[0] == "a");
  CHECK(out.spellings[1] == ";");
  CHECK(out.spellings[2] == "b");
  CHECK(out.spellings[3] == ";");
  CHECK(out.spellings[4] == "c");
  CHECK(out.spellings[5] == ";");
}

TEST_CASE("Boost.PP: BOOST_PP_TUPLE_ELEM / TUPLE_SIZE")
{
  if (!boost_pp_available()) SKIP("Boost.PP not installed at /usr/include");
  search_dirs() = {"/usr/include"};
  auto out = run(
      "#include <boost/preprocessor/tuple/elem.hpp>\n"
      "#include <boost/preprocessor/tuple/size.hpp>\n"
      "BOOST_PP_TUPLE_ELEM(1, (a, b, c))\n"
      "BOOST_PP_TUPLE_SIZE((a, b, c, d))\n");
  for (auto const& d : out.diags) INFO(d.message);
  REQUIRE(out.spellings.size() == 2);
  CHECK(out.spellings[0] == "b");
  CHECK(out.spellings[1] == "4");
}

TEST_CASE("Boost.PP: BOOST_PP_LIST_FOR_EACH")
{
  if (!boost_pp_available()) SKIP("Boost.PP not installed at /usr/include");
  search_dirs() = {"/usr/include"};
  auto out = run(
      "#include <boost/preprocessor/list/for_each.hpp>\n"
      "#define M(r, data, elem) elem\n"
      "BOOST_PP_LIST_FOR_EACH(M, ~, (a, (b, (c, BOOST_PP_NIL))))\n");
  for (auto const& d : out.diags) INFO(d.message);
  REQUIRE(out.spellings.size() == 3);
  CHECK(out.spellings[0] == "a");
  CHECK(out.spellings[1] == "b");
  CHECK(out.spellings[2] == "c");
}

TEST_CASE("Boost.PP: BOOST_PP_ENUM_PARAMS")
{
  if (!boost_pp_available()) SKIP("Boost.PP not installed at /usr/include");
  search_dirs() = {"/usr/include"};
  auto out = run(
      "#include <boost/preprocessor/repetition/enum_params.hpp>\n"
      "BOOST_PP_ENUM_PARAMS(3, T)\n");
  for (auto const& d : out.diags) INFO(d.message);
  REQUIRE(out.spellings.size() == 5);
  CHECK(out.spellings[0] == "T0");
  CHECK(out.spellings[1] == ",");
  CHECK(out.spellings[2] == "T1");
  CHECK(out.spellings[3] == ",");
  CHECK(out.spellings[4] == "T2");
}

TEST_CASE("Boost.PP: BOOST_PP_IIF / BOOL")
{
  if (!boost_pp_available()) SKIP("Boost.PP not installed at /usr/include");
  search_dirs() = {"/usr/include"};
  auto out = run(
      "#include <boost/preprocessor/control/iif.hpp>\n"
      "#include <boost/preprocessor/logical/bool.hpp>\n"
      "BOOST_PP_IIF(BOOST_PP_BOOL(5), yes, no)\n"
      "BOOST_PP_IIF(BOOST_PP_BOOL(0), yes, no)\n");
  for (auto const& d : out.diags) INFO(d.message);
  REQUIRE(out.spellings.size() == 2);
  CHECK(out.spellings[0] == "yes");
  CHECK(out.spellings[1] == "no");
}

TEST_CASE("Boost.PP: BOOST_PP_WHILE counts down to 0")
{
  if (!boost_pp_available()) SKIP("Boost.PP not installed at /usr/include");
  search_dirs() = {"/usr/include"};
  auto out = run(
      "#include <boost/preprocessor/control/while.hpp>\n"
      "#include <boost/preprocessor/arithmetic/dec.hpp>\n"
      "#define PRED(d, state) state\n"
      "#define OP(d, state) BOOST_PP_DEC(state)\n"
      "BOOST_PP_WHILE(PRED, OP, 5)\n");
  for (auto const& d : out.diags) INFO(d.message);
  REQUIRE(out.spellings.size() == 1);
  CHECK(out.spellings[0] == "0");
}

TEST_CASE("Boost.PP: BOOST_PP_MUL (WHILE-based)")
{
  if (!boost_pp_available()) SKIP("Boost.PP not installed at /usr/include");
  search_dirs() = {"/usr/include"};
  auto out = run(
      "#include <boost/preprocessor/arithmetic/mul.hpp>\n"
      "BOOST_PP_MUL(4, 5)\n");
  for (auto const& d : out.diags) INFO(d.message);
  REQUIRE(out.spellings.size() == 1);
  CHECK(out.spellings[0] == "20");
}

TEST_CASE("Boost.PP: BOOST_PP_DIV / MOD")
{
  if (!boost_pp_available()) SKIP("Boost.PP not installed at /usr/include");
  search_dirs() = {"/usr/include"};
  auto out = run(
      "#include <boost/preprocessor/arithmetic/div.hpp>\n"
      "#include <boost/preprocessor/arithmetic/mod.hpp>\n"
      "BOOST_PP_DIV(23, 4)\n"
      "BOOST_PP_MOD(23, 4)\n");
  for (auto const& d : out.diags) INFO(d.message);
  REQUIRE(out.spellings.size() == 2);
  CHECK(out.spellings[0] == "5");
  CHECK(out.spellings[1] == "3");
}

TEST_CASE("Boost.PP: BOOST_PP_COMMA_IF")
{
  if (!boost_pp_available()) SKIP("Boost.PP not installed at /usr/include");
  search_dirs() = {"/usr/include"};
  auto out = run(
      "#include <boost/preprocessor/punctuation/comma_if.hpp>\n"
      "x BOOST_PP_COMMA_IF(0) y\n"
      "x BOOST_PP_COMMA_IF(1) y\n");
  for (auto const& d : out.diags) INFO(d.message);
  REQUIRE(out.spellings.size() == 5);
  CHECK(out.spellings[0] == "x");
  CHECK(out.spellings[1] == "y");
  CHECK(out.spellings[2] == "x");
  CHECK(out.spellings[3] == ",");
  CHECK(out.spellings[4] == "y");
}

TEST_CASE("Boost.PP: BOOST_PP_ARRAY_ELEM / ARRAY_SIZE")
{
  if (!boost_pp_available()) SKIP("Boost.PP not installed at /usr/include");
  search_dirs() = {"/usr/include"};
  auto out = run(
      "#include <boost/preprocessor/array/elem.hpp>\n"
      "#include <boost/preprocessor/array/size.hpp>\n"
      "BOOST_PP_ARRAY_ELEM(2, (4, (a, b, c, d)))\n"
      "BOOST_PP_ARRAY_SIZE((4, (a, b, c, d)))\n");
  for (auto const& d : out.diags) INFO(d.message);
  REQUIRE(out.spellings.size() == 2);
  CHECK(out.spellings[0] == "c");
  CHECK(out.spellings[1] == "4");
}

TEST_CASE("Boost.PP: nested BOOST_PP_REPEAT")
{
  if (!boost_pp_available()) SKIP("Boost.PP not installed at /usr/include");
  search_dirs() = {"/usr/include"};
  auto out = run(
      "#include <boost/preprocessor/repetition/repeat.hpp>\n"
      "#define INNER(z, j, i) x ## i ## j\n"
      "#define OUTER(z, i, data) BOOST_PP_REPEAT_ ## z (2, INNER, i)\n"
      "BOOST_PP_REPEAT(2, OUTER, ~)\n");
  for (auto const& d : out.diags) INFO(d.message);
  REQUIRE(out.spellings.size() == 4);
  CHECK(out.spellings[0] == "x00");
  CHECK(out.spellings[1] == "x01");
  CHECK(out.spellings[2] == "x10");
  CHECK(out.spellings[3] == "x11");
}
