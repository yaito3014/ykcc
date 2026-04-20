#ifndef YK_ASTEROID_DIAGNOSTICS_DIAGNOSTIC_HPP
#define YK_ASTEROID_DIAGNOSTICS_DIAGNOSTIC_HPP

#include <yk/asteroid/diagnostics/source_location.hpp>

#include <string>

namespace yk::asteroid {

enum class diagnostic_level {
  note,
  warning,
  error,
  fatal,
};

struct diagnostic {
  diagnostic_level level;
  source_location location;
  std::string message;
};

struct no_diagnostic_sink {
  constexpr void operator()(diagnostic const&) const noexcept {}
};

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_DIAGNOSTICS_DIAGNOSTIC_HPP
