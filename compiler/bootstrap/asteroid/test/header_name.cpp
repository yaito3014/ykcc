#include "test.hpp"

#include <yk/asteroid/preprocess/header_name.hpp>

TEST_CASE("header_name: angle bracket")
{
  yk::asteroid::preprocess::header_name_parser p{};

  CHECK(p("<stdio.h>").value() == "<stdio.h>");
  CHECK(p("<stdio.h> rest").value() == "<stdio.h>");
  CHECK(p("<a>").value() == "<a>");
}

TEST_CASE("header_name: quoted")
{
  yk::asteroid::preprocess::header_name_parser p{};

  CHECK(p("\"myheader.h\"").value() == "\"myheader.h\"");
  CHECK(p("\"myheader.h\" rest").value() == "\"myheader.h\"");
  CHECK(p("\"a\"").value() == "\"a\"");
}

TEST_CASE("header_name: non-match")
{
  yk::asteroid::preprocess::header_name_parser p{};

  CHECK(!p("").has_value());
  CHECK(!p("<").has_value());
  CHECK(!p("\"").has_value());
  CHECK(!p("<>").has_value());
  CHECK(!p("\"\"").has_value());
  CHECK(!p("abc").has_value());
  CHECK(!p("<foo\nbar>").has_value());
  CHECK(!p("\"foo\nbar\"").has_value());
}
