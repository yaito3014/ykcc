#include <catch2/catch_test_macros.hpp>

#include <yk/asteroid/core/parser/sequence.hpp>

#include <charconv>

template<class T>
struct numeric_parser {
  constexpr auto operator()(std::string_view sv) const -> yk::asteroid::parser_result<int>
  {
    char const* it = std::to_address(sv.cbegin());
    char const* se = std::to_address(sv.cend());
    T result;
    if (auto [ptr, ec] = std::from_chars(it, se, result); ec == std::errc{}) {
      return {result, sv.substr(ptr - it)};
    } else {
      return {sv};
    }
  };
};

struct alphabet_parser {
  constexpr yk::asteroid::parser_result<std::string> operator()(std::string_view sv) const
  {
    std::size_t const pos = sv.find_first_not_of("abcdefghijklmnopqrstuvwxyz");
    if (pos == 0) {
      return {sv};
    } else {
      return {std::string(sv.substr(0, pos)), sv.substr(pos == std::string_view::npos ? sv.size() : pos)};
    }
  };
};

TEST_CASE("sequence")
{
  yk::asteroid::parser auto const left = numeric_parser<int>{};
  yk::asteroid::parser auto const right = alphabet_parser{};
  yk::asteroid::sequence const seq{left, right};
  auto const res = seq("12abc");
  REQUIRE(res.has_value());
  auto const lr_res = *res;
  CHECK(get<0>(lr_res) == 12);
  CHECK(get<1>(lr_res) == "abc");
}

TEST_CASE("chained sequence")
{
  yk::asteroid::parser auto const parser0 = numeric_parser<int>{};
  yk::asteroid::parser auto const parser1 = alphabet_parser{};
  yk::asteroid::parser auto const parser2 = numeric_parser<int>{};
  yk::asteroid::parser auto const parser3 = alphabet_parser{};
  yk::asteroid::sequence const seq0{parser0, parser1};
  yk::asteroid::sequence const seq1{parser2, parser3};
  yk::asteroid::sequence const seq{seq0, seq1};
  auto const res = seq("12abc34def");
  REQUIRE(res.has_value());
  auto const val = *res;
  CHECK(get<0>(val) == 12);
  CHECK(get<1>(val) == "abc");
  CHECK(get<2>(val) == 34);
  CHECK(get<3>(val) == "def");
}
