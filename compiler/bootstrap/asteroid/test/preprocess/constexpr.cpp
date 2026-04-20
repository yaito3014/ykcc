#include "test.hpp"

#include <yk/asteroid/preprocess/lexer.hpp>
#include <yk/asteroid/preprocess/line_splicer.hpp>
#include <yk/asteroid/preprocess/preprocessor.hpp>

#include <initializer_list>
#include <string_view>

namespace {

using namespace yk::asteroid;

// Run the preprocessor on `src` and compare the sequence of non-whitespace
// spellings against `expected`. The check runs in the same scope as the
// line_splicer so pp_token spellings remain valid.
constexpr bool check(std::string_view src, std::initializer_list<std::string_view> expected)
{
  line_splicer splicer{src};
  lexer lx{splicer, "<test>"};
  preprocessor pp{lx};
  auto it = expected.begin();
  while (!pp.at_end()) {
    auto t = pp.next();
    if (t.kind == pp_token_kind::end_of_file) break;
    if (t.kind == pp_token_kind::whitespace || t.kind == pp_token_kind::newline) continue;
    if (it == expected.end()) return false;
    if (t.spelling != *it++) return false;
  }
  return it == expected.end();
}

constexpr bool defines(std::string_view src, std::string_view name)
{
  line_splicer splicer{src};
  lexer lx{splicer, "<test>"};
  preprocessor pp{lx};
  while (!pp.at_end()) {
    auto t = pp.next();
    if (t.kind == pp_token_kind::end_of_file) break;
  }
  return pp.macros().defined(name);
}

static_assert(check("int x = 1;\n", {"int", "x", "=", "1", ";"}));
static_assert(check("#define X 42\nX\n", {"42"}));
static_assert(check("#define X X\nX\n", {"X"}));
static_assert(check("#define A B\n#define B A\nA\n", {"A"}));
static_assert(check("#define A B\n#define B 42\nA\n", {"42"}));
static_assert(check("#define F(x) x + 1\nF(5)\n", {"5", "+", "1"}));
static_assert(check("#define ADD(a,b) a + b\nADD(3, 4)\n", {"3", "+", "4"}));
static_assert(check("#define F(x) x\nF(F(7))\n", {"7"}));
static_assert(check("#define F(x) F(x)\nF(1)\n", {"F", "(", "1", ")"}));
static_assert(check("#define PI 3 . 14\nPI\n", {"3", ".", "14"}));

static_assert(defines("#define A 1\n#define B 2\n", "A"));
static_assert(defines("#define A 1\n#define B 2\n", "B"));
static_assert(!defines("#define A 1\n#undef A\n", "A"));

}  // namespace

TEST_CASE("constexpr: compile-time tests compiled and linked")
{
  // The real assertions are static_asserts above; this test case exists so the
  // translation unit gets picked up by CTest.
  CHECK(true);
}
