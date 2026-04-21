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
