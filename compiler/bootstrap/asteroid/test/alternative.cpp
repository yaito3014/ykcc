#include "test.hpp"

#include <yk/asteroid/core/parser/alternative.hpp>

#include <variant>

struct upper_parser {
  constexpr yk::asteroid::parser_result<std::string> operator()(std::string_view sv) const
  {
    std::size_t const pos = sv.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    if (pos == 0) {
      return yk::asteroid::parse_failure;
    } else {
      return {std::string(sv.substr(0, pos)), sv.begin() + (pos == std::string_view::npos ? sv.size() : pos)};
    }
  }
};

TEST_CASE("alternative: left matches")
{
  yk::asteroid::alternative alt{alphabet_parser{}, upper_parser{}};
  std::string_view input = "abc123";
  auto const res = alt(input);
  REQUIRE(res.has_value());
  CHECK(res.value() == "abc");
}

TEST_CASE("alternative: right matches")
{
  yk::asteroid::alternative alt{alphabet_parser{}, upper_parser{}};
  std::string_view input = "ABC123";
  auto const res = alt(input);
  REQUIRE(res.has_value());
  CHECK(res.value() == "ABC");
}

TEST_CASE("alternative: neither matches")
{
  yk::asteroid::alternative alt{alphabet_parser{}, upper_parser{}};
  std::string_view input = "123";
  auto const res = alt(input);
  CHECK(!res.has_value());
}

TEST_CASE("alternative: different types produces variant")
{
  yk::asteroid::alternative alt{numeric_parser<int>{}, alphabet_parser{}};
  std::string_view input = "42rest";
  auto const res = alt(input);
  REQUIRE(res.has_value());
  CHECK(std::holds_alternative<int>(res.value()));
  CHECK(std::get<int>(res.value()) == 42);

  std::string_view input2 = "abc123";
  auto const res2 = alt(input2);
  REQUIRE(res2.has_value());
  CHECK(std::holds_alternative<std::string>(res2.value()));
  CHECK(std::get<std::string>(res2.value()) == "abc");
}

TEST_CASE("alternative: left takes priority")
{
  yk::asteroid::alternative alt{alphabet_parser{}, alphabet_parser{}};
  std::string_view input = "abc";
  auto const res = alt(input);
  REQUIRE(res.has_value());
  CHECK(res.value() == "abc");
}
