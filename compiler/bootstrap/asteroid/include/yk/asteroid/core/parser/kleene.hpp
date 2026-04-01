#ifndef YK_ASTEROID_CORE_PARSER_KLEENE_HPP
#define YK_ASTEROID_CORE_PARSER_KLEENE_HPP

#include <yk/asteroid/core/parser/common.hpp>

#include <string_view>
#include <vector>

namespace yk::asteroid {

template<parser Subject>
class kleene_parser {
public:
  using value_type = std::vector<parser_value_t<Subject>>;

  constexpr parser_result<value_type> operator()(std::string_view sv) const
  {
    std::vector<parser_value_t<Subject>> result;
    std::string_view::iterator parsed_to = sv.begin();
    while (auto res = subject_(std::string_view(parsed_to, sv.end()))) {
      result.emplace_back(std::move(res).value());
      parsed_to = res.parsed_point();
    }
    return {result, parsed_to};
  }

private:
  [[no_unique_address]] Subject subject_;
};

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_CORE_PARSER_KLEENE_HPP
