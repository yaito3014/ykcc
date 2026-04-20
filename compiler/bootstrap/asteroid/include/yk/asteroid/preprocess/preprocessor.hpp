#ifndef YK_ASTEROID_PREPROCESS_PREPROCESSOR_HPP
#define YK_ASTEROID_PREPROCESS_PREPROCESSOR_HPP

#include <yk/asteroid/diagnostics/diagnostic.hpp>
#include <yk/asteroid/preprocess/directive.hpp>
#include <yk/asteroid/preprocess/lexer.hpp>
#include <yk/asteroid/preprocess/macro.hpp>
#include <yk/asteroid/preprocess/pp_token.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace yk::asteroid {

template<class Sink = no_diagnostic_sink, class Handler = no_directive_handler>
class preprocessor {
public:
  constexpr explicit preprocessor(lexer<Sink>& lx) : lexer_(&lx) {}

  constexpr preprocessor(lexer<Sink>& lx, Sink sink) : lexer_(&lx), sink_(std::move(sink)) {}

  constexpr preprocessor(lexer<Sink>& lx, Sink sink, Handler handler) : lexer_(&lx), sink_(std::move(sink)), handler_(std::move(handler)) {}

  constexpr macro_table& macros() noexcept { return macros_; }
  constexpr macro_table const& macros() const noexcept { return macros_; }

  constexpr pp_token next()
  {
    while (true) {
      pending_token pt;
      if (!take_next(pt)) {
        eof_emitted_ = true;
        pp_token eof;
        eof.kind = pp_token_kind::end_of_file;
        return eof;
      }

      if (at_line_start_ && pt.tok.kind == pp_token_kind::punctuator && pt.tok.spelling == "#") {
        process_directive(pt.tok);
        at_line_start_ = true;
        continue;
      }

      if (try_expand_into(pt, pending_, /*lexer_refill=*/true)) continue;

      if (pt.tok.kind == pp_token_kind::newline) {
        at_line_start_ = true;
      } else if (pt.tok.kind != pp_token_kind::whitespace) {
        at_line_start_ = false;
      }
      return pt.tok;
    }
  }

  constexpr bool at_end() const noexcept { return eof_emitted_ && pending_.empty(); }

private:
  struct pending_token {
    pp_token tok;
    std::vector<std::string> hideset;
  };

  using expansion_queue = std::vector<pending_token>;

  lexer<Sink>* lexer_;
  [[no_unique_address]] Sink sink_{};
  [[no_unique_address]] Handler handler_{};
  macro_table macros_;
  expansion_queue pending_;
  bool at_line_start_ = true;
  bool eof_emitted_ = false;

  constexpr bool take_next(pending_token& pt)
  {
    if (!pending_.empty()) {
      pt = std::move(pending_.front());
      pending_.erase(pending_.begin());
      return true;
    }
    auto tok = lexer_->next();
    if (tok.kind == pp_token_kind::end_of_file) return false;
    pt.tok = std::move(tok);
    pt.hideset.clear();
    return true;
  }

  constexpr void collect_until_newline(std::vector<pp_token>& out)
  {
    while (true) {
      auto t = lexer_->next();
      if (t.kind == pp_token_kind::newline || t.kind == pp_token_kind::end_of_file) return;
      out.push_back(t);
    }
  }

  constexpr void process_directive(pp_token const& hash_tok)
  {
    parsed_directive d;
    d.location = hash_tok.location;

    pp_token name_tok;
    do {
      name_tok = lexer_->next();
    } while (name_tok.kind == pp_token_kind::whitespace);

    if (name_tok.kind == pp_token_kind::newline || name_tok.kind == pp_token_kind::end_of_file) {
      handler_(d);
      return;
    }

    if (name_tok.kind != pp_token_kind::identifier) {
      d.kind = directive_kind::unknown;
      d.tokens.push_back(name_tok);
      collect_until_newline(d.tokens);
      handler_(d);
      return;
    }

    d.name_spelling = name_tok.spelling;
    d.kind = classify_directive_name(name_tok.spelling);

    if (directive_expects_header_name(d.kind)) {
      while (true) {
        if (auto hn = lexer_->try_lex_header_name()) {
          d.tokens.push_back(*hn);
          break;
        }
        auto t = lexer_->next();
        if (t.kind == pp_token_kind::newline || t.kind == pp_token_kind::end_of_file) {
          handler_(d);
          return;
        }
        d.tokens.push_back(t);
        if (t.kind != pp_token_kind::whitespace) break;
      }
    }

    collect_until_newline(d.tokens);

    switch (d.kind) {
      case directive_kind::define:
        handle_define(d);
        break;
      case directive_kind::undef:
        handle_undef(d);
        break;
      default:
        break;
    }

    handler_(d);
  }

  constexpr void handle_define(parsed_directive const& d)
  {
    auto const& toks = d.tokens;
    std::size_t i = 0;
    while (i < toks.size() && toks[i].kind == pp_token_kind::whitespace) ++i;
    if (i >= toks.size() || toks[i].kind != pp_token_kind::identifier) {
      report(diagnostic_level::error, d.location, "#define expects an identifier");
      return;
    }
    auto const& name_tok = toks[i++];

    macro_definition m;
    m.name = std::string{name_tok.spelling};
    m.location = name_tok.location;

    bool const function_like = i < toks.size() && toks[i].kind == pp_token_kind::punctuator && toks[i].spelling == "(";
    if (function_like) {
      m.is_function_like = true;
      ++i;  // consume `(`
      bool expecting_param = true;
      while (true) {
        while (i < toks.size() && toks[i].kind == pp_token_kind::whitespace) ++i;
        if (i >= toks.size()) {
          report(diagnostic_level::error, name_tok.location, "unterminated parameter list in #define");
          return;
        }
        auto const& t = toks[i];
        if (t.kind == pp_token_kind::punctuator && t.spelling == ")") {
          if (expecting_param && !m.parameters.empty()) {
            report(diagnostic_level::error, t.location, "expected parameter before ')'");
            return;
          }
          ++i;
          break;
        }
        if (expecting_param) {
          if (t.kind != pp_token_kind::identifier) {
            report(diagnostic_level::error, t.location, "expected parameter name in #define");
            return;
          }
          if (std::ranges::contains(m.parameters, t.spelling)) {
            report(diagnostic_level::error, t.location, "duplicate parameter name in #define");
            return;
          }
          m.parameters.emplace_back(t.spelling);
          ++i;
          expecting_param = false;
        } else {
          if (t.kind != pp_token_kind::punctuator || t.spelling != ",") {
            report(diagnostic_level::error, t.location, "expected ',' or ')' in parameter list");
            return;
          }
          ++i;
          expecting_param = true;
        }
      }
    }

    if (i < toks.size() && toks[i].kind == pp_token_kind::whitespace) ++i;

    std::size_t end = toks.size();
    while (end > i && toks[end - 1].kind == pp_token_kind::whitespace) --end;

    m.replacement.assign(toks.begin() + i, toks.begin() + end);

    if (auto prev = macros_.lookup(m.name); prev && !macros_equivalent(*prev, m)) {
      report(diagnostic_level::warning, name_tok.location, "macro '" + m.name + "' redefined with a different body");
    }
    macros_.define(std::move(m));
  }

  constexpr void handle_undef(parsed_directive const& d)
  {
    auto const& toks = d.tokens;
    std::size_t i = 0;
    while (i < toks.size() && toks[i].kind == pp_token_kind::whitespace) ++i;
    if (i >= toks.size() || toks[i].kind != pp_token_kind::identifier) {
      report(diagnostic_level::error, d.location, "#undef expects an identifier");
      return;
    }
    macros_.undefine(toks[i].spelling);
  }

  constexpr bool try_expand_into(pending_token const& pt, expansion_queue& queue, bool lexer_refill)
  {
    if (pt.tok.kind != pp_token_kind::identifier) return false;
    if (in_hideset(pt.hideset, pt.tok.spelling)) return false;
    auto m = macros_.lookup(pt.tok.spelling);
    if (!m) return false;

    if (!m->is_function_like) {
      auto new_hideset = hideset_with(pt.hideset, pt.tok.spelling);
      push_front_tokens(queue, m->replacement, new_hideset);
      return true;
    }

    return parse_function_like_call(pt.tok, *m, pt.hideset, queue, lexer_refill);
  }

  constexpr bool parse_function_like_call(
      pp_token const& name_tok, macro_definition const& m, std::vector<std::string> const& caller_hideset, expansion_queue& queue, bool lexer_refill
  )
  {
    auto ensure = [&](std::size_t idx) -> bool {
      while (queue.size() <= idx) {
        if (!lexer_refill) return false;
        auto t = lexer_->next();
        if (t.kind == pp_token_kind::end_of_file) return false;
        queue.push_back(pending_token{std::move(t), {}});
      }
      return true;
    };

    std::size_t peek = 0;
    while (ensure(peek)) {
      auto k = queue[peek].tok.kind;
      if (k == pp_token_kind::whitespace || k == pp_token_kind::newline) {
        ++peek;
        continue;
      }
      break;
    }
    if (peek >= queue.size()) return false;
    if (queue[peek].tok.kind != pp_token_kind::punctuator || queue[peek].tok.spelling != "(") {
      return false;
    }

    std::size_t depth = 1;
    std::size_t i = peek + 1;
    std::vector<std::vector<pp_token>> raw_args;
    raw_args.emplace_back();
    while (true) {
      if (!ensure(i)) {
        report(diagnostic_level::error, name_tok.location, "unterminated macro invocation");
        return false;
      }
      auto const& t = queue[i].tok;
      if (t.kind == pp_token_kind::punctuator && t.spelling == "(") {
        ++depth;
        raw_args.back().push_back(t);
      } else if (t.kind == pp_token_kind::punctuator && t.spelling == ")") {
        if (--depth == 0) break;
        raw_args.back().push_back(t);
      } else if (depth == 1 && t.kind == pp_token_kind::punctuator && t.spelling == ",") {
        raw_args.emplace_back();
      } else {
        raw_args.back().push_back(t);
      }
      ++i;
    }
    std::size_t const rparen_idx = i;
    auto const rparen_hideset = queue[rparen_idx].hideset;

    if (raw_args.size() == 1 && raw_args[0].empty() && m.parameters.empty()) {
      raw_args.clear();
    }

    if (raw_args.size() != m.parameters.size()) {
      report(diagnostic_level::error, name_tok.location, "wrong number of arguments to function-like macro '" + m.name + "'");
      queue.erase(queue.begin(), queue.begin() + rparen_idx + 1);
      return true;
    }

    std::vector<std::vector<pp_token>> expanded_args;
    expanded_args.reserve(raw_args.size());
    for (auto const& raw : raw_args) {
      expanded_args.push_back(expand_tokens(raw));
    }

    auto substituted = substitute_body(m, expanded_args);

    auto new_hideset = hideset_intersect(caller_hideset, rparen_hideset);
    new_hideset = hideset_with(std::move(new_hideset), m.name);

    queue.erase(queue.begin(), queue.begin() + rparen_idx + 1);
    push_front_tokens(queue, substituted, new_hideset);
    return true;
  }

  constexpr std::vector<pp_token> expand_tokens(std::vector<pp_token> tokens)
  {
    expansion_queue q;
    q.reserve(tokens.size());
    for (auto& t : tokens) q.push_back(pending_token{std::move(t), {}});
    std::vector<pp_token> out;
    while (!q.empty()) {
      pending_token pt = std::move(q.front());
      q.erase(q.begin());
      if (try_expand_into(pt, q, /*lexer_refill=*/false)) continue;
      out.push_back(std::move(pt.tok));
    }
    return out;
  }

  // Plain parameter substitution; does not yet implement # (stringify) or ## (paste).
  constexpr std::vector<pp_token> substitute_body(macro_definition const& m, std::vector<std::vector<pp_token>> const& expanded_args) const
  {
    std::vector<pp_token> out;
    out.reserve(m.replacement.size());
    for (auto const& t : m.replacement) {
      if (t.kind == pp_token_kind::identifier) {
        auto it = std::ranges::find(m.parameters, t.spelling);
        if (it != m.parameters.end()) {
          auto idx = static_cast<std::size_t>(it - m.parameters.begin());
          out.insert(out.end(), expanded_args[idx].begin(), expanded_args[idx].end());
          continue;
        }
      }
      out.push_back(t);
    }
    return out;
  }

  constexpr void push_front_tokens(expansion_queue& queue, std::vector<pp_token> const& tokens, std::vector<std::string> const& hideset)
  {
    std::vector<pending_token> batch;
    batch.reserve(tokens.size());
    for (auto const& t : tokens) batch.push_back(pending_token{t, hideset});
    queue.insert(queue.begin(), std::make_move_iterator(batch.begin()), std::make_move_iterator(batch.end()));
  }

  constexpr void report(diagnostic_level lvl, source_location const& loc, std::string msg) const { sink_(diagnostic{lvl, loc, std::move(msg)}); }

  static constexpr bool in_hideset(std::vector<std::string> const& hs, std::string_view name) noexcept { return std::ranges::contains(hs, name); }

  static constexpr std::vector<std::string> hideset_intersect(std::vector<std::string> const& a, std::vector<std::string> const& b)
  {
    std::vector<std::string> out;
    for (auto const& s : a) {
      if (in_hideset(b, s)) out.push_back(s);
    }
    return out;
  }

  static constexpr std::vector<std::string> hideset_with(std::vector<std::string> hs, std::string_view name)
  {
    if (!in_hideset(hs, name)) hs.emplace_back(name);
    return hs;
  }
};

template<class Sink>
preprocessor(lexer<Sink>&) -> preprocessor<Sink, no_directive_handler>;

template<class Sink>
preprocessor(lexer<Sink>&, Sink) -> preprocessor<Sink, no_directive_handler>;

template<class Sink, class Handler>
preprocessor(lexer<Sink>&, Sink, Handler) -> preprocessor<Sink, Handler>;

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_PREPROCESS_PREPROCESSOR_HPP
