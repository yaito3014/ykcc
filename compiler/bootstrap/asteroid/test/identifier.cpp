#include "test.hpp"

#include <yk/asteroid/preprocess/identifier.hpp>

TEST_CASE("identifier")
{
  yk::asteroid::preprocess::identifier_parser ident{};
  std::string_view input = "foo123-bar";
  auto const res = ident(input);
  REQUIRE(res.has_value());
  CHECK(res.value() == "foo123");
  CHECK(std::string_view(res.parsed_point(), input.end()) == "-bar");
}
