#ifndef YK_ASTEROID_PREPROCESS_PP_TOKEN_HPP
#define YK_ASTEROID_PREPROCESS_PP_TOKEN_HPP

#include <yk/asteroid/diagnostics/source_location.hpp>

#include <string_view>

namespace yk::asteroid {

enum class pp_token_kind {
  whitespace,
  newline,
  identifier,
  pp_number,
  character_literal,
  string_literal,
  header_name,
  punctuator,
  other,
  end_of_file,
};

struct pp_token {
  pp_token_kind kind = pp_token_kind::end_of_file;
  std::string_view spelling;
  source_location location;
};

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_PREPROCESS_PP_TOKEN_HPP
