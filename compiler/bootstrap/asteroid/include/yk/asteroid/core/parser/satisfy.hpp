#ifndef YK_ASTEROID_CORE_PARSER_SATISFY_HPP
#define YK_ASTEROID_CORE_PARSER_SATISFY_HPP

#include <yk/asteroid/core/parser/common.hpp>

#include <concepts>
#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace yk::asteroid {

template<class Pred>
class satisfy_parser {
public:
  template<class PredT>
  constexpr satisfy_parser(PredT&& pred) noexcept(std::is_nothrow_constructible_v<Pred, PredT>) : pred_(std::forward<PredT>(pred))
  {
  }

  constexpr parser_result<char> operator()(std::string_view sv) const noexcept(std::is_nothrow_invocable_v<Pred const&, char>)
  {
    if (!sv.empty() && std::invoke(pred_, sv[0])) {
      return {sv[0], sv.begin() + 1};
    }
    return parse_failure;
  }

private:
  [[no_unique_address]] Pred pred_;
};

template<class PredT>
  requires std::predicate<std::remove_cvref_t<PredT>, char>
satisfy_parser(PredT&&) -> satisfy_parser<std::remove_cvref_t<PredT>>;

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_CORE_PARSER_SATISFY_HPP
