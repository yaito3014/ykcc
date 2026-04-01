#include "test.hpp"

#include <yk/asteroid/preprocess/pp_number.hpp>

TEST_CASE("pp_number: simple integer")
{
  yk::asteroid::preprocess::pp_number_parser pp_num{};
  std::string_view input = "123abc";
  auto const res = pp_num(input);
  REQUIRE(res.has_value());
  CHECK(res.value() == "123abc");
}

TEST_CASE("pp_number: float")
{
  yk::asteroid::preprocess::pp_number_parser pp_num{};
  std::string_view input = "1.0 ";
  auto const res = pp_num(input);
  REQUIRE(res.has_value());
  CHECK(res.value() == "1.0");
}

TEST_CASE("pp_number: exponent with sign")
{
  yk::asteroid::preprocess::pp_number_parser pp_num{};
  std::string_view input = "1e+5;";
  auto const res = pp_num(input);
  REQUIRE(res.has_value());
  CHECK(res.value() == "1e+5");
}

TEST_CASE("pp_number: hex")
{
  yk::asteroid::preprocess::pp_number_parser pp_num{};
  std::string_view input = "0x1F ";
  auto const res = pp_num(input);
  REQUIRE(res.has_value());
  CHECK(res.value() == "0x1F");
}

TEST_CASE("pp_number: digit separator")
{
  yk::asteroid::preprocess::pp_number_parser pp_num{};
  std::string_view input = "1'000'000;";
  auto const res = pp_num(input);
  REQUIRE(res.has_value());
  CHECK(res.value() == "1'000'000");
}

TEST_CASE("pp_number: dot start")
{
  yk::asteroid::preprocess::pp_number_parser pp_num{};
  std::string_view input = ".5e-3";
  auto const res = pp_num(input);
  REQUIRE(res.has_value());
  CHECK(res.value() == ".5e-3");
}

TEST_CASE("pp_number: non-match")
{
  yk::asteroid::preprocess::pp_number_parser pp_num{};
  std::string_view input = "abc";
  auto const res = pp_num(input);
  CHECK_FALSE(res.has_value());
}

TEST_CASE("pp_number: dot without digit")
{
  yk::asteroid::preprocess::pp_number_parser pp_num{};
  std::string_view input = ".abc";
  auto const res = pp_num(input);
  CHECK_FALSE(res.has_value());
}

TEST_CASE("pp_number: hex float exponent")
{
  yk::asteroid::preprocess::pp_number_parser pp_num{};
  std::string_view input = "0x1.2p+3";
  auto const res = pp_num(input);
  REQUIRE(res.has_value());
  CHECK(res.value() == "0x1.2p+3");
}
