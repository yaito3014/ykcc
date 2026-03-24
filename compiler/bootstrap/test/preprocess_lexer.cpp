#include <yk/asteroid/preprocess/lexer.hpp>

// Verify constexpr evaluation works
consteval bool test_lexer_constexpr() {
  using namespace yk::asteroid::preprocess;
  Lexer lexer("int main() {}");
  auto tok = lexer.next();
  return tok.kind == PPTokenKind::Identifier && tok.spelling == "int";
}
static_assert(test_lexer_constexpr());

int main() {}
