#ifndef YK_ASTEROID_PREPROCESS_DIRECTIVE_HPP
#define YK_ASTEROID_PREPROCESS_DIRECTIVE_HPP

#include <yk/asteroid/diagnostics/source_location.hpp>
#include <yk/asteroid/preprocess/pp_token.hpp>

#include <string_view>
#include <vector>

namespace yk::asteroid {

enum class directive_kind {
  null_,
  include,
  embed,
  define,
  undef,
  if_,
  ifdef,
  ifndef,
  elif,
  elifdef,
  elifndef,
  else_,
  endif,
  line,
  error,
  warning,
  pragma,
  unknown,
};

struct parsed_directive {
  directive_kind kind = directive_kind::null_;
  std::string_view name_spelling;
  source_location location;
  std::vector<pp_token> tokens;
};

struct no_directive_handler {
  constexpr void operator()(parsed_directive const&) const noexcept {}
};

constexpr directive_kind classify_directive_name(std::string_view name) noexcept
{
  if (name == "include") return directive_kind::include;
  if (name == "embed") return directive_kind::embed;
  if (name == "define") return directive_kind::define;
  if (name == "undef") return directive_kind::undef;
  if (name == "if") return directive_kind::if_;
  if (name == "ifdef") return directive_kind::ifdef;
  if (name == "ifndef") return directive_kind::ifndef;
  if (name == "elif") return directive_kind::elif;
  if (name == "elifdef") return directive_kind::elifdef;
  if (name == "elifndef") return directive_kind::elifndef;
  if (name == "else") return directive_kind::else_;
  if (name == "endif") return directive_kind::endif;
  if (name == "line") return directive_kind::line;
  if (name == "error") return directive_kind::error;
  if (name == "warning") return directive_kind::warning;
  if (name == "pragma") return directive_kind::pragma;
  return directive_kind::unknown;
}

constexpr bool directive_expects_header_name(directive_kind k) noexcept { return k == directive_kind::include || k == directive_kind::embed; }

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_PREPROCESS_DIRECTIVE_HPP
