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
#include <memory>
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
  std::vector<std::indirect<std::string>> synth_strings_;
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
        if (t.kind == pp_token_kind::punctuator && t.spelling == "...") {
          if (!expecting_param) {
            report(diagnostic_level::error, t.location, "'...' must follow '(' or ','");
            return;
          }
          m.is_variadic = true;
          ++i;
          while (i < toks.size() && toks[i].kind == pp_token_kind::whitespace) ++i;
          if (i >= toks.size() || toks[i].kind != pp_token_kind::punctuator || toks[i].spelling != ")") {
            report(diagnostic_level::error, t.location, "expected ')' after '...'");
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

    if (raw_args.size() == 1 && raw_args[0].empty() && m.parameters.empty() && !m.is_variadic) {
      raw_args.clear();
    }

    if (m.is_variadic) {
      if (raw_args.size() < m.parameters.size()) {
        report(diagnostic_level::error, name_tok.location, "too few arguments to variadic macro '" + m.name + "'");
        queue.erase(queue.begin(), queue.begin() + rparen_idx + 1);
        return true;
      }
      std::vector<pp_token> merged;
      for (std::size_t k = m.parameters.size(); k < raw_args.size(); ++k) {
        if (k > m.parameters.size()) {
          pp_token comma;
          comma.kind = pp_token_kind::punctuator;
          comma.spelling = std::string_view{","};
          merged.push_back(comma);
        }
        for (auto& t : raw_args[k]) merged.push_back(std::move(t));
      }
      raw_args.resize(m.parameters.size());
      raw_args.push_back(std::move(merged));
    } else {
      if (raw_args.size() != m.parameters.size()) {
        report(diagnostic_level::error, name_tok.location, "wrong number of arguments to function-like macro '" + m.name + "'");
        queue.erase(queue.begin(), queue.begin() + rparen_idx + 1);
        return true;
      }
    }

    std::vector<std::vector<pp_token>> expanded_args;
    expanded_args.reserve(raw_args.size());
    for (auto const& raw : raw_args) {
      expanded_args.push_back(expand_tokens(raw));
    }

    auto substituted = substitute_body(m, raw_args, expanded_args);

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

  // Resolve __VA_OPT__(content) in the replacement list to either content or a placemarker,
  // based on whether __VA_ARGS__ is non-empty. Called before the main substitution pass.
  constexpr std::vector<pp_token> expand_va_opt(macro_definition const& m, std::vector<std::vector<pp_token>> const& raw_args)
  {
    auto const& repl = m.replacement;
    std::size_t const n = repl.size();

    if (!m.is_variadic) {
      for (auto const& t : repl) {
        if (t.kind == pp_token_kind::identifier && t.spelling == "__VA_OPT__") {
          report(diagnostic_level::error, t.location, "'__VA_OPT__' is only valid inside variadic macros");
          break;
        }
      }
      return repl;
    }

    bool va_non_empty = false;
    for (auto const& t : raw_args[m.parameters.size()]) {
      if (t.kind != pp_token_kind::whitespace) {
        va_non_empty = true;
        break;
      }
    }

    std::vector<pp_token> out;
    out.reserve(n);

    std::size_t i = 0;
    while (i < n) {
      auto const& t = repl[i];
      if (t.kind == pp_token_kind::identifier && t.spelling == "__VA_OPT__") {
        std::size_t j = i + 1;
        while (j < n && repl[j].kind == pp_token_kind::whitespace) ++j;
        if (j >= n || repl[j].kind != pp_token_kind::punctuator || repl[j].spelling != "(") {
          report(diagnostic_level::error, t.location, "'__VA_OPT__' must be followed by '('");
          out.push_back(t);
          ++i;
          continue;
        }
        int depth = 1;
        std::size_t k = j + 1;
        std::size_t const content_start = k;
        while (k < n && depth > 0) {
          auto const& tk = repl[k];
          if (tk.kind == pp_token_kind::punctuator && tk.spelling == "(") ++depth;
          else if (tk.kind == pp_token_kind::punctuator && tk.spelling == ")") {
            --depth;
            if (depth == 0) break;
          } else if (tk.kind == pp_token_kind::identifier && tk.spelling == "__VA_OPT__") {
            report(diagnostic_level::error, tk.location, "'__VA_OPT__' cannot be nested");
          }
          ++k;
        }
        if (k >= n) {
          report(diagnostic_level::error, t.location, "unterminated '__VA_OPT__'");
          break;
        }
        if (va_non_empty) {
          for (std::size_t p = content_start; p < k; ++p) out.push_back(repl[p]);
        } else {
          out.push_back(make_placemarker(t.location));
        }
        i = k + 1;
        continue;
      }
      out.push_back(t);
      ++i;
    }

    return out;
  }

  // Two-pass substitution. Pass 1 resolves `#` (stringify) and parameter substitution
  // (raw tokens if adjacent to `##`, otherwise fully expanded tokens), leaving `##`
  // tokens in place. Pass 2 processes `##` left to right with placemarker collapsing.
  constexpr std::vector<pp_token> substitute_body(
      macro_definition const& m,
      std::vector<std::vector<pp_token>> const& raw_args,
      std::vector<std::vector<pp_token>> const& expanded_args
  )
  {
    auto const repl = expand_va_opt(m, raw_args);
    std::size_t const n = repl.size();

    auto is_paste = [&](std::size_t idx) -> bool {
      return idx < n && repl[idx].kind == pp_token_kind::punctuator && repl[idx].spelling == "##";
    };
    auto is_stringify = [&](std::size_t idx) -> bool {
      return idx < n && repl[idx].kind == pp_token_kind::punctuator && repl[idx].spelling == "#";
    };
    auto skip_ws_fwd = [&](std::size_t idx) -> std::size_t {
      while (idx < n && repl[idx].kind == pp_token_kind::whitespace) ++idx;
      return idx;
    };
    auto param_idx = [&](pp_token const& t) -> std::size_t {
      if (t.kind != pp_token_kind::identifier) return static_cast<std::size_t>(-1);
      if (m.is_variadic && t.spelling == "__VA_ARGS__") return m.parameters.size();
      auto it = std::ranges::find(m.parameters, t.spelling);
      if (it == m.parameters.end()) return static_cast<std::size_t>(-1);
      return static_cast<std::size_t>(it - m.parameters.begin());
    };
    auto paste_adjacent = [&](std::size_t idx) -> bool {
      if (is_paste(skip_ws_fwd(idx + 1))) return true;
      std::size_t k = idx;
      while (k > 0 && repl[k - 1].kind == pp_token_kind::whitespace) --k;
      return k > 0 && is_paste(k - 1);
    };

    std::vector<pp_token> stage1;
    stage1.reserve(n);

    std::size_t i = 0;
    while (i < n) {
      auto const& t = repl[i];

      if (is_stringify(i)) {
        std::size_t const j = skip_ws_fwd(i + 1);
        if (j >= n) {
          report(diagnostic_level::error, t.location, "'#' at end of macro replacement list");
          break;
        }
        auto const pi = param_idx(repl[j]);
        if (pi == static_cast<std::size_t>(-1)) {
          report(diagnostic_level::error, t.location, "'#' is not followed by a macro parameter");
          stage1.push_back(t);
          ++i;
          continue;
        }
        stage1.push_back(stringify(raw_args[pi], t.location));
        i = j + 1;
        continue;
      }

      if (auto const pi = param_idx(t); pi != static_cast<std::size_t>(-1)) {
        if (paste_adjacent(i)) {
          auto const& r = raw_args[pi];
          std::size_t lo = 0, hi = r.size();
          while (lo < hi && r[lo].kind == pp_token_kind::whitespace) ++lo;
          while (hi > lo && r[hi - 1].kind == pp_token_kind::whitespace) --hi;
          if (lo == hi) {
            stage1.push_back(make_placemarker(t.location));
          } else {
            for (std::size_t k = lo; k < hi; ++k) stage1.push_back(r[k]);
          }
        } else {
          stage1.append_range(expanded_args[pi]);
        }
        ++i;
        continue;
      }

      stage1.push_back(t);
      ++i;
    }

    std::size_t const sn = stage1.size();
    auto s_is_paste = [&](std::size_t idx) -> bool {
      return idx < sn && stage1[idx].kind == pp_token_kind::punctuator && stage1[idx].spelling == "##";
    };
    auto s_skip_ws_fwd = [&](std::size_t idx) -> std::size_t {
      while (idx < sn && stage1[idx].kind == pp_token_kind::whitespace) ++idx;
      return idx;
    };

    std::vector<pp_token> out;
    out.reserve(sn);

    std::size_t i2 = 0;
    while (i2 < sn) {
      if (s_is_paste(i2)) {
        std::size_t const j = s_skip_ws_fwd(i2 + 1);
        if (j >= sn) {
          report(diagnostic_level::error, stage1[i2].location, "'##' at end of macro replacement list");
          break;
        }
        while (!out.empty() && out.back().kind == pp_token_kind::whitespace) out.pop_back();
        if (out.empty()) {
          report(diagnostic_level::error, stage1[i2].location, "'##' at start of macro replacement list");
          out.push_back(stage1[j]);
        } else {
          pp_token left = std::move(out.back());
          out.pop_back();
          out.push_back(paste_tokens(left, stage1[j]));
        }
        i2 = j + 1;
        continue;
      }
      out.push_back(stage1[i2]);
      ++i2;
    }

    std::erase_if(out, [](pp_token const& tok) { return tok.kind == pp_token_kind::placemarker; });
    return out;
  }

  constexpr pp_token stringify(std::vector<pp_token> const& raw, source_location loc)
  {
    std::size_t lo = 0, hi = raw.size();
    while (lo < hi && raw[lo].kind == pp_token_kind::whitespace) ++lo;
    while (hi > lo && raw[hi - 1].kind == pp_token_kind::whitespace) --hi;

    std::string body;
    bool prev_ws = false;
    bool started = false;
    for (std::size_t i = lo; i < hi; ++i) {
      auto const& t = raw[i];
      if (t.kind == pp_token_kind::whitespace || t.kind == pp_token_kind::newline) {
        if (started) prev_ws = true;
        continue;
      }
      if (prev_ws) body.push_back(' ');
      prev_ws = false;
      started = true;

      bool const lit = (t.kind == pp_token_kind::string_literal || t.kind == pp_token_kind::character_literal);
      for (char c : t.spelling) {
        if (lit && (c == '\\' || c == '"')) body.push_back('\\');
        body.push_back(c);
      }
    }

    std::string literal;
    literal.reserve(body.size() + 2);
    literal.push_back('"');
    literal.append(body);
    literal.push_back('"');

    synth_strings_.emplace_back(std::move(literal));
    pp_token out;
    out.kind = pp_token_kind::string_literal;
    out.spelling = std::string_view{*synth_strings_.back()};
    out.location = loc;
    return out;
  }

  constexpr pp_token make_placemarker(source_location loc) const
  {
    pp_token t;
    t.kind = pp_token_kind::placemarker;
    t.spelling = "";
    t.location = loc;
    return t;
  }

  constexpr pp_token paste_tokens(pp_token const& a, pp_token const& b)
  {
    bool const a_pm = a.kind == pp_token_kind::placemarker;
    bool const b_pm = b.kind == pp_token_kind::placemarker;
    if (a_pm && b_pm) return make_placemarker(a.location);
    if (a_pm) return b;
    if (b_pm) return a;

    std::string joined;
    joined.reserve(a.spelling.size() + b.spelling.size());
    joined.append(a.spelling);
    joined.append(b.spelling);

    auto kind = classify_pasted(joined);
    synth_strings_.emplace_back(std::move(joined));
    pp_token t;
    t.kind = kind;
    t.spelling = std::string_view{*synth_strings_.back()};
    t.location = a.location;
    return t;
  }

  static constexpr pp_token_kind classify_pasted(std::string_view s) noexcept
  {
    if (s.empty()) return pp_token_kind::placemarker;
    char c = s.front();
    if (c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) return pp_token_kind::identifier;
    if (c >= '0' && c <= '9') return pp_token_kind::pp_number;
    return pp_token_kind::other;
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
