#ifndef YK_ASTEROID_PREPROCESS_MACRO_HPP
#define YK_ASTEROID_PREPROCESS_MACRO_HPP

#include <yk/asteroid/diagnostics/source_location.hpp>
#include <yk/asteroid/preprocess/pp_token.hpp>

#include <flat_map>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace yk::asteroid {

struct macro_definition {
  std::string name;
  bool is_function_like = false;
  bool is_variadic = false;
  std::vector<std::string> parameters;
  std::vector<pp_token> replacement;
  source_location location;
};

constexpr bool macros_equivalent(macro_definition const& a, macro_definition const& b) noexcept
{
  if (a.name != b.name) return false;
  if (a.is_function_like != b.is_function_like) return false;
  if (a.is_variadic != b.is_variadic) return false;
  if (a.parameters != b.parameters) return false;
  if (a.replacement.size() != b.replacement.size()) return false;
  for (std::size_t i = 0; i < a.replacement.size(); ++i) {
    auto const& x = a.replacement[i];
    auto const& y = b.replacement[i];
    if (x.kind != y.kind) return false;
    if (x.spelling != y.spelling) return false;
  }
  return true;
}

class macro_table {
public:
  constexpr std::optional<macro_definition const&> lookup(std::string_view name) const noexcept
  {
    auto it = macros_.find(name);
    if (it == macros_.end()) return std::nullopt;
    return it->second;
  }

  constexpr bool defined(std::string_view name) const noexcept { return macros_.find(name) != macros_.end(); }

  constexpr void define(macro_definition def)
  {
    auto name = def.name;
    macros_.insert_or_assign(std::move(name), std::move(def));
  }

  constexpr bool undefine(std::string_view name)
  {
    auto it = macros_.find(name);
    if (it == macros_.end()) return false;
    macros_.erase(it);
    return true;
  }

  constexpr std::size_t size() const noexcept { return macros_.size(); }

  constexpr void clear() noexcept { macros_.clear(); }

private:
  std::flat_map<std::string, macro_definition, std::less<>> macros_;
};

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_PREPROCESS_MACRO_HPP
