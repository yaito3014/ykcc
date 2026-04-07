#ifndef YK_ASTEROID_PREPROCESS_PREPROCESSOR_HPP
#define YK_ASTEROID_PREPROCESS_PREPROCESSOR_HPP

#include <yk/asteroid/preprocess/const_expr.hpp>
#include <yk/asteroid/preprocess/lexer.hpp>
#include <yk/asteroid/preprocess/line_splice.hpp>
#include <yk/asteroid/preprocess/macro_table.hpp>

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace yk::asteroid::preprocess {

struct pp_directive {
  pp_token hash;
  pp_token name;
  std::vector<pp_token> tokens;  // tokens after directive name, excluding newline
  pp_token newline;
};

struct pp_text_line {
  std::vector<pp_token> tokens;  // excluding newline
  pp_token newline;
};

using pp_line = std::variant<pp_directive, pp_text_line>;

struct include_result {
  std::string source;
};

struct include_error {
  std::string message;
};

using include_handler_t = std::function<std::expected<include_result, include_error>(std::string_view header_name)>;

class preprocessor {
public:
  explicit preprocessor(std::string_view source) : original_source_(source) {}

  preprocessor(std::string_view source, include_handler_t include_handler)
      : original_source_(source), include_handler_(std::move(include_handler))
  {
  }

  void run()
  {
    phase2();
    phase3();
    phase4();
  }

  std::vector<pp_line> output;
  std::vector<pp_directive> directives;
  std::vector<pp_text_line> text_lines;

private:
  // Private constructor for recursive includes sharing macro state
  preprocessor(std::string_view source, include_handler_t include_handler, macro_table& shared_macros)
      : original_source_(source), include_handler_(std::move(include_handler)), macros_(shared_macros), owns_macros_(false)
  {
  }

  using iterator = lexer::iterator;

  // Phase 2: Line splicing
  void phase2() { spliced_ = splice_lines(original_source_); }

  // Phase 3: Tokenization into preprocessing tokens
  void phase3()
  {
    lexer lex{spliced_.text};
    auto it = lex.begin();
    auto const end = lex.end();

    while (it != end) {
      skip_whitespace(it, end);
      if (it == end) break;

      if (is_start_of_directive(it)) {
        process_directive(it, end);
      } else {
        process_text_line(it, end);
      }
    }
  }

  // Phase 4: Directive execution, macro expansion
  void phase4()
  {
    for (auto& line : lines_) {
      std::visit([this](auto& l) { execute(l); }, line);
    }
  }

  // --- Conditional directive state ---

  struct conditional_state {
    bool any_branch_taken;  // has any branch in this #if group been taken?
    bool current_active;    // is the current branch active?
  };

  std::vector<conditional_state> cond_stack_;
  macro_table owned_macros_;
  macro_table& macros_ = owned_macros_;
  bool owns_macros_ = true;

  bool active() const noexcept
  {
    return cond_stack_.empty() || cond_stack_.back().current_active;
  }

  bool parent_active() const noexcept
  {
    if (cond_stack_.size() < 2) return true;
    return cond_stack_[cond_stack_.size() - 2].current_active;
  }

  bool is_defined(std::string_view name) const { return macros_.is_defined(name); }

  void execute(pp_directive& dir)
  {
    auto const name = dir.name.piece;

    // Conditional directives are always processed regardless of active state
    if (name == "if") {
      exec_if(dir);
      return;
    }
    if (name == "ifdef") {
      exec_ifdef(dir);
      return;
    }
    if (name == "ifndef") {
      exec_ifndef(dir);
      return;
    }
    if (name == "elif") {
      exec_elif(dir);
      return;
    }
    if (name == "elifdef") {
      exec_elifdef(dir);
      return;
    }
    if (name == "elifndef") {
      exec_elifndef(dir);
      return;
    }
    if (name == "else") {
      exec_else(dir);
      return;
    }
    if (name == "endif") {
      exec_endif(dir);
      return;
    }

    if (!active()) return;

    if (name == "define") {
      exec_define(dir);
      return;
    }
    if (name == "undef") {
      exec_undef(dir);
      return;
    }
    if (name == "include") {
      exec_include(dir);
      return;
    }

    // Other directives passed through
    directives.push_back(dir);
    output.emplace_back(dir);
  }

  void execute(pp_text_line& line)
  {
    if (!active()) return;

    pp_text_line expanded_line;
    expanded_line.tokens = macros_.expand(line.tokens);
    expanded_line.newline = line.newline;

    text_lines.push_back(expanded_line);
    output.emplace_back(std::move(expanded_line));
  }

  void exec_if(pp_directive& dir)
  {
    if (!active()) {
      cond_stack_.push_back({true, false});
      return;
    }
    bool const cond = evaluate_condition(dir.tokens);
    cond_stack_.push_back({cond, cond});
  }

  void exec_ifdef(pp_directive& dir)
  {
    if (!active()) {
      cond_stack_.push_back({true, false});
      return;
    }
    auto const name = extract_identifier(dir.tokens);
    bool const cond = !name.empty() && is_defined(name);
    cond_stack_.push_back({cond, cond});
  }

  void exec_ifndef(pp_directive& dir)
  {
    if (!active()) {
      cond_stack_.push_back({true, false});
      return;
    }
    auto const name = extract_identifier(dir.tokens);
    bool const cond = name.empty() || !is_defined(name);
    cond_stack_.push_back({cond, cond});
  }

  void exec_elif(pp_directive& dir)
  {
    if (cond_stack_.empty()) return;
    auto& state = cond_stack_.back();
    if (!parent_active() || state.any_branch_taken) {
      state.current_active = false;
      return;
    }
    bool const cond = evaluate_condition(dir.tokens);
    state.current_active = cond;
    if (cond) state.any_branch_taken = true;
  }

  void exec_elifdef(pp_directive& dir)
  {
    if (cond_stack_.empty()) return;
    auto& state = cond_stack_.back();
    if (!parent_active() || state.any_branch_taken) {
      state.current_active = false;
      return;
    }
    auto const name = extract_identifier(dir.tokens);
    bool const cond = !name.empty() && is_defined(name);
    state.current_active = cond;
    if (cond) state.any_branch_taken = true;
  }

  void exec_elifndef(pp_directive& dir)
  {
    if (cond_stack_.empty()) return;
    auto& state = cond_stack_.back();
    if (!parent_active() || state.any_branch_taken) {
      state.current_active = false;
      return;
    }
    auto const name = extract_identifier(dir.tokens);
    bool const cond = name.empty() || !is_defined(name);
    state.current_active = cond;
    if (cond) state.any_branch_taken = true;
  }

  void exec_else(pp_directive&)
  {
    if (cond_stack_.empty()) return;
    auto& state = cond_stack_.back();
    state.current_active = parent_active() && !state.any_branch_taken;
    state.any_branch_taken = true;
  }

  void exec_endif(pp_directive&)
  {
    if (!cond_stack_.empty()) cond_stack_.pop_back();
  }

  void exec_define(pp_directive& dir)
  {
    auto def = parse_define(dir.tokens);
    if (def) macros_.define(std::move(*def));
  }

  void exec_undef(pp_directive& dir)
  {
    auto const name = extract_identifier(dir.tokens);
    if (!name.empty()) macros_.undef(name);
  }

  static std::optional<macro_definition> parse_define(std::vector<pp_token> const& tokens)
  {
    std::size_t pos = 0;

    // Skip leading whitespace
    while (pos < tokens.size() && tokens[pos].kind == pp_token_kind::whitespace) ++pos;
    if (pos >= tokens.size() || tokens[pos].kind != pp_token_kind::identifier) return std::nullopt;

    macro_definition def;
    def.name = std::string(tokens[pos].piece);
    ++pos;

    // Check for function-like macro: '(' immediately after name (no whitespace)
    if (pos < tokens.size() && tokens[pos].piece == "(" && tokens[pos].kind == pp_token_kind::op_or_punc) {
      def.is_function_like = true;
      ++pos;  // skip '('

      // Parse parameters
      while (pos < tokens.size()) {
        // Skip whitespace
        while (pos < tokens.size() && tokens[pos].kind == pp_token_kind::whitespace) ++pos;
        if (pos >= tokens.size()) return std::nullopt;

        if (tokens[pos].piece == ")") {
          ++pos;
          break;
        }

        if (tokens[pos].piece == "..." && tokens[pos].kind == pp_token_kind::op_or_punc) {
          def.is_variadic = true;
          ++pos;
          while (pos < tokens.size() && tokens[pos].kind == pp_token_kind::whitespace) ++pos;
          if (pos < tokens.size() && tokens[pos].piece == ")") ++pos;
          break;
        }

        if (tokens[pos].kind == pp_token_kind::identifier) {
          def.parameters.emplace_back(tokens[pos].piece);
          ++pos;
        }

        while (pos < tokens.size() && tokens[pos].kind == pp_token_kind::whitespace) ++pos;
        if (pos < tokens.size() && tokens[pos].piece == ",") ++pos;
      }
    }

    // Skip whitespace before replacement list
    while (pos < tokens.size() && tokens[pos].kind == pp_token_kind::whitespace) ++pos;

    // Remaining tokens are the replacement list
    while (pos < tokens.size()) {
      def.replacement.push_back(tokens[pos]);
      ++pos;
    }

    // Trim trailing whitespace from replacement list
    while (!def.replacement.empty() && def.replacement.back().kind == pp_token_kind::whitespace) {
      def.replacement.pop_back();
    }

    return def;
  }

  void exec_include(pp_directive& dir)
  {
    if (!include_handler_) {
      directives.push_back(dir);
      output.emplace_back(dir);
      return;
    }

    auto const header_name = extract_header_name(dir.tokens);
    if (header_name.empty()) return;

    auto result = include_handler_(header_name);
    if (!result) return;

    auto sub = std::unique_ptr<preprocessor>(new preprocessor{std::string_view{}, include_handler_, macros_});
    sub->original_source_ = (sub->included_sources_.emplace_back(std::move(result->source)), sub->included_sources_.back());
    sub->run();

    output.insert(output.end(), sub->output.begin(), sub->output.end());
    directives.insert(directives.end(), sub->directives.begin(), sub->directives.end());
    text_lines.insert(text_lines.end(), sub->text_lines.begin(), sub->text_lines.end());

    included_subs_.push_back(std::move(sub));
  }

  static std::string_view extract_header_name(std::vector<pp_token> const& tokens)
  {
    for (auto const& tok : tokens) {
      if (tok.kind == pp_token_kind::whitespace) continue;
      if (tok.kind == pp_token_kind::header_name) return tok.piece;
      break;
    }
    return {};
  }

  bool evaluate_condition(std::vector<pp_token> const& tokens) const
  {
    auto protected_tokens = protect_defined(tokens);
    auto expanded = macros_.expand(protected_tokens);
    auto restored = restore_defined(expanded);
    auto result = evaluate_const_expr(std::span{restored}, [this](std::string_view name) { return is_defined(name); });
    return result.has_value() && result->value != 0;
  }

  // Replace `defined X` and `defined(X)` with placeholder tokens to prevent macro expansion of the operand
  static constexpr std::string_view defined_placeholder_ = "\x01""defined";

  static std::vector<pp_token> protect_defined(std::vector<pp_token> const& tokens)
  {
    std::vector<pp_token> result;
    std::size_t i = 0;
    while (i < tokens.size()) {
      if (tokens[i].kind == pp_token_kind::identifier && tokens[i].piece == "defined") {
        result.push_back(make_placeholder_token(tokens[i]));
        ++i;
        // Copy whitespace
        while (i < tokens.size() && tokens[i].kind == pp_token_kind::whitespace) {
          result.push_back(tokens[i]);
          ++i;
        }
        if (i < tokens.size() && tokens[i].piece == "(") {
          result.push_back(tokens[i]);
          ++i;
          // Protect everything up to and including ')' by marking identifiers as non_whitespace_char
          while (i < tokens.size() && tokens[i].piece != ")") {
            auto tok = tokens[i];
            if (tok.kind == pp_token_kind::identifier) tok.kind = pp_token_kind::non_whitespace_char;
            result.push_back(tok);
            ++i;
          }
          if (i < tokens.size()) {
            result.push_back(tokens[i]);
            ++i;
          }
        } else if (i < tokens.size() && tokens[i].kind == pp_token_kind::identifier) {
          // defined X form — protect the identifier
          auto tok = tokens[i];
          tok.kind = pp_token_kind::non_whitespace_char;
          result.push_back(tok);
          ++i;
        }
      } else {
        result.push_back(tokens[i]);
        ++i;
      }
    }
    return result;
  }

  static std::vector<pp_token> restore_defined(std::vector<pp_token> const& tokens)
  {
    std::vector<pp_token> result;
    for (auto tok : tokens) {
      if (tok.piece == defined_placeholder_) {
        tok.piece = "defined";
        tok.kind = pp_token_kind::identifier;
      } else if (tok.kind == pp_token_kind::non_whitespace_char && is_identifier_like(tok.piece)) {
        tok.kind = pp_token_kind::identifier;
      }
      result.push_back(tok);
    }
    return result;
  }

  static bool is_identifier_like(std::string_view sv) noexcept
  {
    if (sv.empty()) return false;
    if (sv[0] != '_' && !(sv[0] >= 'a' && sv[0] <= 'z') && !(sv[0] >= 'A' && sv[0] <= 'Z')) return false;
    return true;
  }

  static pp_token make_placeholder_token(pp_token const& original)
  {
    auto tok = original;
    tok.piece = defined_placeholder_;
    tok.kind = pp_token_kind::non_whitespace_char;  // won't match any macro
    return tok;
  }

  static std::string_view extract_identifier(std::vector<pp_token> const& tokens)
  {
    for (auto const& tok : tokens) {
      if (tok.kind == pp_token_kind::whitespace) continue;
      if (tok.kind == pp_token_kind::identifier) return tok.piece;
      break;
    }
    return {};
  }

  // --- Tokenization helpers ---

  static void skip_whitespace(iterator& it, std::default_sentinel_t end)
  {
    while (it != end && it->kind == pp_token_kind::whitespace) ++it;
  }

  static bool is_start_of_directive(iterator const& it) { return it->kind == pp_token_kind::op_or_punc && it->piece == "#"; }

  static bool is_include_directive(std::string_view name) { return name == "include" || name == "embed"; }

  void process_directive(iterator& it, std::default_sentinel_t end)
  {
    pp_directive directive{};
    directive.hash = *it;
    ++it;

    skip_whitespace(it, end);
    if (it == end) return;

    if (it->kind != pp_token_kind::identifier) {
      // null directive or malformed — collect until newline
      collect_until_newline(directive.tokens, it, end);
      directive.newline = *it;
      ++it;
      lines_.emplace_back(std::move(directive));
      return;
    }

    directive.name = *it;
    bool const need_header_name = is_include_directive(it->piece);

    if (need_header_name) {
      it.next_header_name();
      while (it != end && it->kind == pp_token_kind::whitespace) {
        directive.tokens.push_back(*it);
        it.next_header_name();
      }
    } else {
      ++it;
    }

    collect_until_newline(directive.tokens, it, end);
    if (it != end) {
      directive.newline = *it;
      ++it;
    }

    lines_.emplace_back(std::move(directive));
  }

  void process_text_line(iterator& it, std::default_sentinel_t end)
  {
    pp_text_line line{};

    collect_until_newline(line.tokens, it, end);
    if (it != end) {
      line.newline = *it;
      ++it;
    }

    lines_.emplace_back(std::move(line));
  }

  static void collect_until_newline(std::vector<pp_token>& tokens, iterator& it, std::default_sentinel_t end)
  {
    while (it != end && it->kind != pp_token_kind::newline) {
      tokens.push_back(*it);
      ++it;
    }
  }

  std::string_view original_source_;
  include_handler_t include_handler_;
  spliced_source spliced_;
  std::vector<pp_line> lines_;  // phase 3 output, phase 4 input
  std::vector<std::string> included_sources_;  // keeps included source text alive
  std::vector<std::unique_ptr<preprocessor>> included_subs_;  // keeps sub-preprocessor token data alive
};

}  // namespace yk::asteroid::preprocess

#endif  // YK_ASTEROID_PREPROCESS_PREPROCESSOR_HPP
