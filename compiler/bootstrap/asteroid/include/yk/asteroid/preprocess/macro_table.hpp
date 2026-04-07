#ifndef YK_ASTEROID_PREPROCESS_MACRO_TABLE_HPP
#define YK_ASTEROID_PREPROCESS_MACRO_TABLE_HPP

#include <yk/asteroid/preprocess/lexer.hpp>

#include <deque>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace yk::asteroid::preprocess {

struct macro_definition {
  std::string name;
  bool is_function_like = false;
  bool is_variadic = false;
  std::vector<std::string> parameters;        // for function-like macros
  std::vector<pp_token> replacement;           // replacement token list (whitespace-normalized)
};

class macro_table {
public:
  void define(macro_definition def)
  {
    auto name = def.name;
    macros_.insert_or_assign(std::move(name), std::move(def));
  }

  void undef(std::string_view name)
  {
    auto it = macros_.find(name);
    if (it != macros_.end()) macros_.erase(it);
  }

  bool is_defined(std::string_view name) const { return macros_.contains(name); }

  macro_definition const* find(std::string_view name) const
  {
    auto it = macros_.find(name);
    if (it != macros_.end()) return &it->second;
    return nullptr;
  }

  // Expand all macros in a token sequence
  std::vector<pp_token> expand(std::vector<pp_token> const& tokens) const
  {
    std::set<std::string_view> expanding;
    return expand_impl(tokens, expanding);
  }

private:
  std::map<std::string, macro_definition, std::less<>> macros_;

  std::vector<pp_token> expand_impl(std::vector<pp_token> const& tokens, std::set<std::string_view>& expanding) const
  {
    std::vector<pp_token> result;
    std::size_t i = 0;

    while (i < tokens.size()) {
      auto const& tok = tokens[i];

      if (tok.kind != pp_token_kind::identifier && tok.kind != pp_token_kind::op_or_punc) {
        result.push_back(tok);
        ++i;
        continue;
      }

      auto const* def = find(tok.piece);
      if (!def || expanding.contains(tok.piece)) {
        result.push_back(tok);
        ++i;
        continue;
      }

      if (def->is_function_like) {
        auto args = try_parse_arguments(tokens, i + 1, def->parameters.size(), def->is_variadic);
        if (!args) {
          // No parenthesized arguments: not an invocation
          result.push_back(tok);
          ++i;
          continue;
        }

        auto substituted = substitute(def, args->first);
        expanding.insert(tok.piece);
        auto expanded = expand_impl(substituted, expanding);
        expanding.erase(tok.piece);

        result.append_range(expanded);
        i = args->second;  // skip past closing paren
      } else {
        expanding.insert(tok.piece);
        auto expanded = expand_impl(def->replacement, expanding);
        expanding.erase(tok.piece);

        result.append_range(expanded);
        ++i;
      }
    }

    return result;
  }

  // Try to parse parenthesized argument list starting at pos
  // Returns (arguments, next_position_after_close_paren) or nullopt
  static std::optional<std::pair<std::vector<std::vector<pp_token>>, std::size_t>>
  try_parse_arguments(std::vector<pp_token> const& tokens, std::size_t pos, std::size_t param_count, bool is_variadic)
  {
    // Skip whitespace to find '('
    while (pos < tokens.size() && tokens[pos].kind == pp_token_kind::whitespace) ++pos;
    if (pos >= tokens.size() || tokens[pos].piece != "(") return std::nullopt;
    ++pos;  // skip '('

    std::vector<std::vector<pp_token>> args;
    std::vector<pp_token> current_arg;
    int depth = 0;

    while (pos < tokens.size()) {
      auto const& tok = tokens[pos];

      if (tok.piece == "(" && tok.kind == pp_token_kind::op_or_punc) {
        ++depth;
        current_arg.push_back(tok);
      } else if (tok.piece == ")" && tok.kind == pp_token_kind::op_or_punc) {
        if (depth == 0) {
          // Don't push an empty arg for zero-param macros with empty invocation: FOO()
          if (!current_arg.empty() || !args.empty()) {
            args.push_back(trim_whitespace(std::move(current_arg)));
          }
          return std::pair{std::move(args), pos + 1};
        }
        --depth;
        current_arg.push_back(tok);
      } else if (tok.piece == "," && tok.kind == pp_token_kind::op_or_punc && depth == 0) {
        // For variadic macros, if we've filled all named params, remaining commas belong to __VA_ARGS__
        if (is_variadic && args.size() >= param_count) {
          current_arg.push_back(tok);
        } else {
          args.push_back(trim_whitespace(std::move(current_arg)));
          current_arg.clear();
        }
      } else {
        current_arg.push_back(tok);
      }
      ++pos;
    }

    return std::nullopt;  // unmatched paren
  }

  static std::vector<pp_token> trim_whitespace(std::vector<pp_token> tokens)
  {
    while (!tokens.empty() && tokens.front().kind == pp_token_kind::whitespace) tokens.erase(tokens.begin());
    while (!tokens.empty() && tokens.back().kind == pp_token_kind::whitespace) tokens.pop_back();
    return tokens;
  }

  // Stringize a token sequence (# operator)
  static std::string stringize(std::vector<pp_token> const& tokens)
  {
    std::string result;
    result += '"';
    bool prev_was_space = false;
    for (auto const& tok : tokens) {
      if (tok.kind == pp_token_kind::whitespace) {
        prev_was_space = true;
        continue;
      }
      if (prev_was_space && !result.empty() && result.back() != '"') {
        result += ' ';
      }
      prev_was_space = false;

      if (tok.kind == pp_token_kind::string_literal || tok.kind == pp_token_kind::character_literal) {
        // Escape backslashes and quotes within string/char literals
        for (char c : tok.piece) {
          if (c == '\\' || c == '"') result += '\\';
          result += c;
        }
      } else {
        result += tok.piece;
      }
    }
    result += '"';
    return result;
  }

  // Token paste (## operator): concatenate two tokens
  static pp_token paste_tokens(pp_token const& lhs, pp_token const& rhs)
  {
    // The result piece needs storage — use a static thread_local buffer pool
    // For simplicity, create a new string and store it
    auto& storage = string_storage();
    storage.push_back(std::string(lhs.piece) + std::string(rhs.piece));
    std::string_view pasted = storage.back();

    // Determine kind of pasted token heuristically
    pp_token_kind kind = pp_token_kind::non_whitespace_char;
    if (!pasted.empty()) {
      char c = pasted[0];
      if (c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        kind = pp_token_kind::identifier;
      } else if (c >= '0' && c <= '9') {
        kind = pp_token_kind::number;
      } else {
        kind = pp_token_kind::op_or_punc;
      }
    }

    return pp_token{kind, pasted, lhs.location};
  }

  static std::deque<std::string>& string_storage()
  {
    static thread_local std::deque<std::string> storage;
    return storage;
  }

  // Substitute parameters in replacement list with arguments, handling # and ##
  static std::vector<pp_token> substitute(macro_definition const* def, std::vector<std::vector<pp_token>> const& args)
  {
    std::vector<pp_token> result;
    auto const& repl = def->replacement;

    for (std::size_t i = 0; i < repl.size(); ++i) {
      auto const& tok = repl[i];

      // Handle # (stringizing) operator
      if (tok.kind == pp_token_kind::op_or_punc && tok.piece == "#") {
        // Skip whitespace to find operand
        std::size_t j = i + 1;
        while (j < repl.size() && repl[j].kind == pp_token_kind::whitespace) ++j;
        if (j < repl.size()) {
          auto param_idx = resolve_parameter(def, repl[j]);
          if (param_idx) {
            auto const& arg_tokens = *param_idx < args.size() ? args[*param_idx] : empty_tokens();
            auto str = stringize(arg_tokens);
            auto& storage = string_storage();
            storage.push_back(std::move(str));
            result.push_back(pp_token{pp_token_kind::string_literal, storage.back(), tok.location});
            i = j;  // skip past the parameter token
            continue;
          }
        }
        // Not followed by a parameter — keep as-is (technically ill-formed)
        result.push_back(tok);
        continue;
      }

      // Check if next non-whitespace token is ##
      {
        std::size_t peek = i + 1;
        while (peek < repl.size() && repl[peek].kind == pp_token_kind::whitespace) ++peek;
        bool const next_is_paste = (peek < repl.size() && repl[peek].kind == pp_token_kind::op_or_punc && repl[peek].piece == "##");

        if (next_is_paste) {
          // Get LHS tokens (don't expand — ## suppresses expansion)
          auto lhs_param = resolve_parameter(def, tok);
          std::vector<pp_token> lhs_tokens;
          if (lhs_param) {
            lhs_tokens = *lhs_param < args.size() ? args[*lhs_param] : std::vector<pp_token>{};
          } else if (tok.kind != pp_token_kind::whitespace) {
            lhs_tokens.push_back(tok);
          }

          // Loop to handle chained ## (e.g. a ## b ## c)
          std::size_t pos = peek;
          while (pos < repl.size() && repl[pos].kind == pp_token_kind::op_or_punc && repl[pos].piece == "##") {
            std::size_t rhs_start = pos + 1;
            while (rhs_start < repl.size() && repl[rhs_start].kind == pp_token_kind::whitespace) ++rhs_start;
            if (rhs_start >= repl.size()) break;

            auto rhs_param = resolve_parameter(def, repl[rhs_start]);
            std::vector<pp_token> rhs_tokens;
            if (rhs_param) {
              rhs_tokens = *rhs_param < args.size() ? args[*rhs_param] : std::vector<pp_token>{};
            } else {
              rhs_tokens.push_back(repl[rhs_start]);
            }

            // Paste: if either side is empty, use the other side as-is
            if (lhs_tokens.empty()) {
              lhs_tokens = std::move(rhs_tokens);
            } else if (!rhs_tokens.empty()) {
              // Paste last of LHS with first of RHS
              lhs_tokens.back() = paste_tokens(lhs_tokens.back(), rhs_tokens.front());
              for (std::size_t k = 1; k < rhs_tokens.size(); ++k) {
                lhs_tokens.push_back(rhs_tokens[k]);
              }
            }

            pos = rhs_start + 1;
            // Skip whitespace to check for another ##
            while (pos < repl.size() && repl[pos].kind == pp_token_kind::whitespace) ++pos;
          }

          result.append_range(lhs_tokens);
          i = pos - 1;  // for loop will ++i
          continue;
        }
      }

      // Normal token — substitute parameters
      if (tok.kind == pp_token_kind::identifier) {
        auto param_idx = find_parameter(def, tok.piece);
        if (param_idx) {
          if (*param_idx < args.size()) {
            result.append_range(args[*param_idx]);
          }
          continue;
        }

        if (def->is_variadic && tok.piece == "__VA_ARGS__") {
          if (def->parameters.size() < args.size()) {
            result.append_range(args[def->parameters.size()]);
          }
          continue;
        }
      }

      result.push_back(tok);
    }

    return result;
  }

  static std::vector<pp_token> const& empty_tokens()
  {
    static std::vector<pp_token> const empty;
    return empty;
  }

  // Resolve a token to a parameter index (handles both named params and __VA_ARGS__)
  static std::optional<std::size_t> resolve_parameter(macro_definition const* def, pp_token const& tok)
  {
    if (tok.kind != pp_token_kind::identifier) return std::nullopt;
    auto idx = find_parameter(def, tok.piece);
    if (idx) return idx;
    if (def->is_variadic && tok.piece == "__VA_ARGS__") return def->parameters.size();
    return std::nullopt;
  }

  static std::optional<std::size_t> find_parameter(macro_definition const* def, std::string_view name)
  {
    for (std::size_t i = 0; i < def->parameters.size(); ++i) {
      if (def->parameters[i] == name) return i;
    }
    return std::nullopt;
  }
};

}  // namespace yk::asteroid::preprocess

#endif  // YK_ASTEROID_PREPROCESS_MACRO_TABLE_HPP
