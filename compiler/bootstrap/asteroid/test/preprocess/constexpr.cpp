#include "test.hpp"

#include <yk/asteroid/preprocess/lexer.hpp>
#include <yk/asteroid/preprocess/line_splicer.hpp>
#include <yk/asteroid/preprocess/preprocessor.hpp>

#include <initializer_list>
#include <string>
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

constexpr bool check_include(std::string_view src,
                             bool (*resolver)(std::string_view),
                             std::initializer_list<std::string_view> expected)
{
  line_splicer splicer{src};
  lexer lx{splicer, "<test>"};
  preprocessor pp{lx};
  pp.set_include_resolver(resolver);
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

constexpr bool check_embed(std::string_view src,
                           int (*resolver)(std::string_view),
                           std::initializer_list<std::string_view> expected)
{
  line_splicer splicer{src};
  lexer lx{splicer, "<test>"};
  preprocessor pp{lx};
  pp.set_embed_resolver(resolver);
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

constexpr bool check_with_source(
    std::string_view src,
    bool (*source)(std::string_view, std::string_view, std::string&, std::string&),
    std::initializer_list<std::string_view> expected)
{
  line_splicer splicer{src};
  lexer lx{splicer, "<test>"};
  preprocessor pp{lx};
  pp.set_include_source(source);
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

// `##` paste: parameters use raw (unexpanded) argument tokens.
static_assert(check("#define CAT(a,b) a ## b\nCAT(foo, bar)\n", {"foobar"}));
static_assert(check("#define CAT(a,b) a ## b\nCAT(x_, 3)\n", {"x_3"}));
static_assert(check("#define P(x) x ## _suffix\nP(hello)\n", {"hello_suffix"}));
// Non-paste parameters still use expanded args.
static_assert(check("#define ID 42\n#define CAT(a,b) a ## b\nCAT(ID, X)\n", {"IDX"}));
static_assert(check("#define ID 42\n#define USE(x) x\nUSE(ID)\n", {"42"}));
// Empty argument adjacent to ## becomes a placemarker that collapses.
static_assert(check("#define CAT(a,b) a ## b\nCAT(foo,)\n", {"foo"}));
static_assert(check("#define CAT(a,b) a ## b\nCAT(,bar)\n", {"bar"}));
static_assert(check("#define CAT(a,b) a ## b\nCAT(,)\n", {}));
// Rescan after paste: if the pasted token names a macro, it expands.
static_assert(check("#define FOO 99\n#define CAT(a,b) a ## b\nCAT(FO, O)\n", {"99"}));

// Variadic macros and __VA_ARGS__.
static_assert(check("#define F(...) __VA_ARGS__\nF(1, 2, 3)\n", {"1", ",", "2", ",", "3"}));
static_assert(check("#define F(x, ...) x , __VA_ARGS__\nF(a, 1, 2)\n", {"a", ",", "1", ",", "2"}));
static_assert(check("#define F(x, ...) x\nF(a, 1, 2)\n", {"a"}));
static_assert(check("#define F(...) __VA_ARGS__\nF()\n", {}));

// __VA_OPT__ basic: non-empty variadic inlines content.
static_assert(check("#define F(...) f(0 __VA_OPT__(,) __VA_ARGS__)\nF(1)\n",
                    {"f", "(", "0", ",", "1", ")"}));
static_assert(check("#define F(...) f(0 __VA_OPT__(,) __VA_ARGS__)\nF()\n",
                    {"f", "(", "0", ")"}));

// __VA_OPT__ with ## adjacency (matches GCC on `H4(X,...) __VA_OPT__(a X ## b) ## c`).
static_assert(check("#define H(X, ...) __VA_OPT__(a X ## b) ## c\nH(1)\n", {"c"}));
static_assert(check("#define H(X, ...) __VA_OPT__(a X ## b) ## c\nH(1, 2)\n", {"a", "1bc"}));

// __VA_OPT__ external to ## on the right side.
static_assert(check("#define H(X, ...) a ## __VA_OPT__(b) c\nH(1)\n", {"a", "c"}));
static_assert(check("#define H(X, ...) a ## __VA_OPT__(b) c\nH(1, 2)\n", {"ab", "c"}));

// `#` stringify: uses raw argument, collapses internal whitespace, trims ends.
static_assert(check("#define S(x) # x\nS(foo)\n", {"\"foo\""}));
static_assert(check("#define S(x) # x\nS(  a   b  )\n", {"\"a b\""}));
// Stringify uses the RAW argument (no macro expansion).
static_assert(check("#define X 42\n#define S(x) # x\nS(X)\n", {"\"X\""}));
// Stringify with variadic.
static_assert(check("#define S(...) # __VA_ARGS__\nS(1, 2, 3)\n", {"\"1, 2, 3\""}));
// Stringify escapes " and \ inside string literals in the argument.
static_assert(check("#define S(x) # x\nS(\"a\\n\")\n", {"\"\\\"a\\\\n\\\"\""}));
// Stringify followed by paste.
static_assert(check("#define SC(a,b) # a ## b\nSC(foo, bar)\n", {"\"foo\"bar"}));

// -------------------------------------------------------------------------
// Conditional directives: #ifdef / #ifndef / #if / #elif / #else / #endif.
// -------------------------------------------------------------------------

// Basic #ifdef / #ifndef.
static_assert(check("#define X\n#ifdef X\nyes\n#endif\n", {"yes"}));
static_assert(check("#ifdef X\nyes\n#endif\n", {}));
static_assert(check("#ifndef X\nyes\n#endif\n", {"yes"}));
static_assert(check("#define X\n#ifndef X\nyes\n#endif\n", {}));

// #else.
static_assert(check("#ifdef X\na\n#else\nb\n#endif\n", {"b"}));
static_assert(check("#define X\n#ifdef X\na\n#else\nb\n#endif\n", {"a"}));

// #elif chain.
static_assert(check("#if 0\na\n#elif 1\nb\n#else\nc\n#endif\n", {"b"}));
static_assert(check("#if 0\na\n#elif 0\nb\n#else\nc\n#endif\n", {"c"}));
static_assert(check("#if 1\na\n#elif 1\nb\n#else\nc\n#endif\n", {"a"}));

// defined() in #if.
static_assert(check("#define X\n#if defined(X)\nyes\n#endif\n", {"yes"}));
static_assert(check("#define X\n#if defined X\nyes\n#endif\n", {"yes"}));
static_assert(check("#if defined(X)\nyes\n#else\nno\n#endif\n", {"no"}));
static_assert(check("#define X\n#if !defined(Y) && defined(X)\nyes\n#endif\n", {"yes"}));

// Arithmetic in #if.
static_assert(check("#if 1 + 2 == 3\nyes\n#endif\n", {"yes"}));
static_assert(check("#if (1 << 4) == 16\nyes\n#endif\n", {"yes"}));
static_assert(check("#if 10 / 3\nyes\n#endif\n", {"yes"}));
static_assert(check("#if 10 % 3 == 1\nyes\n#endif\n", {"yes"}));

// Ternary.
static_assert(check("#if 1 ? 2 : 3\na\n#endif\n", {"a"}));
static_assert(check("#if 0 ? 2 : 0\na\n#else\nb\n#endif\n", {"b"}));

// Macro expansion inside #if.
static_assert(check("#define V 42\n#if V > 10\nyes\n#endif\n", {"yes"}));
static_assert(check("#define V 0\n#if V\nyes\n#else\nno\n#endif\n", {"no"}));

// Undefined identifier evaluates to 0.
static_assert(check("#if UNDEF\na\n#else\nb\n#endif\n", {"b"}));
static_assert(check("#if !UNDEF\na\n#endif\n", {"a"}));

// Nested conditionals.
static_assert(check("#define A\n#define B\n#ifdef A\n  #ifdef B\nboth\n  #else\nonlyA\n  #endif\n#endif\n", {"both"}));
static_assert(check("#define A\n#ifdef A\n  #ifdef B\nBset\n  #else\nBunset\n  #endif\n#endif\n", {"Bunset"}));

// Conditional disables directive inside skipped branch.
static_assert(!defines("#if 0\n#define X 1\n#endif\n", "X"));
static_assert(defines("#if 1\n#define X 1\n#endif\n", "X"));

// #elifdef / #elifndef.
static_assert(check("#ifdef A\na\n#elifdef B\nb\n#else\nc\n#endif\n", {"c"}));
static_assert(check("#define B\n#ifdef A\na\n#elifdef B\nb\n#else\nc\n#endif\n", {"b"}));
static_assert(check("#ifdef A\na\n#elifndef B\nb\n#else\nc\n#endif\n", {"b"}));

// Character literals in #if.
static_assert(check("#if 'a' == 97\nyes\n#endif\n", {"yes"}));
static_assert(check("#if '\\n' == 10\nyes\n#endif\n", {"yes"}));

// -------------------------------------------------------------------------
// __has_cpp_attribute: built-in version table.
// -------------------------------------------------------------------------
static_assert(check("#if __has_cpp_attribute(nodiscard)\ny\n#endif\n", {"y"}));
static_assert(check("#if __has_cpp_attribute(nodiscard) >= 201603L\ny\n#endif\n", {"y"}));
static_assert(check("#if __has_cpp_attribute(nodiscard) == 201907\ny\n#endif\n", {"y"}));
static_assert(check("#if __has_cpp_attribute(fallthrough)\ny\n#endif\n", {"y"}));
static_assert(check("#if __has_cpp_attribute(no_unique_address)\ny\n#endif\n", {"y"}));
static_assert(check("#if __has_cpp_attribute(imaginary_attr)\ny\n#else\nn\n#endif\n", {"n"}));
// Scoped attribute: unknown scope → 0.
static_assert(check("#if __has_cpp_attribute(gnu::always_inline)\ny\n#else\nn\n#endif\n", {"n"}));

// -------------------------------------------------------------------------
// __has_include: overridable resolver, defaults to not-found.
// -------------------------------------------------------------------------
static_assert(check("#if __has_include(<foo.h>)\ny\n#else\nn\n#endif\n", {"n"}));
static_assert(check("#if __has_include(\"foo.h\")\ny\n#else\nn\n#endif\n", {"n"}));

static_assert(check_include(
    "#if __has_include(<foo.h>)\ny\n#else\nn\n#endif\n",
    +[](std::string_view h) { return h == "<foo.h>"; }, {"y"}));
static_assert(check_include(
    "#if __has_include(<bar.h>)\ny\n#else\nn\n#endif\n",
    +[](std::string_view h) { return h == "<foo.h>"; }, {"n"}));
static_assert(check_include(
    "#if __has_include(\"my/path.h\")\ny\n#else\nn\n#endif\n",
    +[](std::string_view h) { return h == "\"my/path.h\""; }, {"y"}));
// Header name with '/' and '.' between '<' '>'.
static_assert(check_include(
    "#if __has_include(<my/deep/path.h>)\ny\n#else\nn\n#endif\n",
    +[](std::string_view h) { return h == "<my/deep/path.h>"; }, {"y"}));

// -------------------------------------------------------------------------
// __has_embed: overridable resolver, values 0 (not-found), 1 (found), 2 (empty).
// -------------------------------------------------------------------------
static_assert(check("#if __has_embed(<data.bin>)\ny\n#else\nn\n#endif\n", {"n"}));
static_assert(check_embed(
    "#if __has_embed(<data.bin>) == 1\ny\n#else\nn\n#endif\n",
    +[](std::string_view h) { return h == "<data.bin>" ? 1 : 0; }, {"y"}));
static_assert(check_embed(
    "#if __has_embed(<empty.bin>) == 2\ny\n#else\nn\n#endif\n",
    +[](std::string_view h) { return h == "<empty.bin>" ? 2 : 0; }, {"y"}));
// Extra trailing pp-parameters are skipped for lookup (space-separated per C23).
static_assert(check_embed(
    "#if __has_embed(<data.bin> prefix(x) suffix(y))\ny\n#else\nn\n#endif\n",
    +[](std::string_view h) { return h == "<data.bin>" ? 1 : 0; }, {"y"}));

// -------------------------------------------------------------------------
// #include: resolver-driven source loading.
// -------------------------------------------------------------------------

// Basic #include pulling in tokens from another file.
static_assert(check_with_source(
    "#include <a.h>\nafter\n",
    +[](std::string_view h, std::string_view, std::string& p, std::string& c) {
      if (h == "<a.h>") { p = "a.h"; c = "x y z\n"; return true; }
      return false;
    },
    {"x", "y", "z", "after"}));

// Included file can define a macro visible after the #include.
static_assert(check_with_source(
    "#include <m.h>\nFOO\n",
    +[](std::string_view h, std::string_view, std::string& p, std::string& c) {
      if (h == "<m.h>") { p = "m.h"; c = "#define FOO 42\n"; return true; }
      return false;
    },
    {"42"}));

// Quoted header form.
static_assert(check_with_source(
    "#include \"b.h\"\n",
    +[](std::string_view h, std::string_view, std::string& p, std::string& c) {
      if (h == "\"b.h\"") { p = "b.h"; c = "hello\n"; return true; }
      return false;
    },
    {"hello"}));

// Nested #include: a.h includes b.h.
static_assert(check_with_source(
    "#include <a.h>\n",
    +[](std::string_view h, std::string_view, std::string& p, std::string& c) {
      if (h == "<a.h>") { p = "a.h"; c = "A\n#include <b.h>\nC\n"; return true; }
      if (h == "<b.h>") { p = "b.h"; c = "B\n"; return true; }
      return false;
    },
    {"A", "B", "C"}));

// Conditional inside included file skips correctly.
static_assert(check_with_source(
    "#define FLAG\n#include <c.h>\n",
    +[](std::string_view h, std::string_view, std::string& p, std::string& c) {
      if (h == "<c.h>") { p = "c.h"; c = "#ifdef FLAG\nyes\n#else\nno\n#endif\n"; return true; }
      return false;
    },
    {"yes"}));

// #pragma once: second include is a no-op.
static_assert(check_with_source(
    "#include <once.h>\n#include <once.h>\n",
    +[](std::string_view h, std::string_view, std::string& p, std::string& c) {
      if (h == "<once.h>") { p = "once.h"; c = "#pragma once\nx\n"; return true; }
      return false;
    },
    {"x"}));

// Missing header: resolver returns false, diagnostic emitted, preprocessing continues.
static_assert(check_with_source(
    "#include <missing.h>\nafter\n",
    +[](std::string_view, std::string_view, std::string&, std::string&) { return false; },
    {"after"}));

// Macro-expanded header name: `#include HDR` where HDR expands to `<foo.h>`.
static_assert(check_with_source(
    "#define HDR <foo.h>\n#include HDR\n",
    +[](std::string_view h, std::string_view, std::string& p, std::string& c) {
      if (h == "<foo.h>") { p = "foo.h"; c = "ok\n"; return true; }
      return false;
    },
    {"ok"}));

}  // namespace

TEST_CASE("constexpr: compile-time tests compiled and linked")
{
  // The real assertions are static_asserts above; this test case exists so the
  // translation unit gets picked up by CTest.
  CHECK(true);
}
