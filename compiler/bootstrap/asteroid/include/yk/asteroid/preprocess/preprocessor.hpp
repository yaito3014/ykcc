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

struct include_result {
  std::string path;     // canonical path used for diagnostics and #pragma once
  std::string content;  // raw file contents (pre-splicing)
};

template<class Sink = no_diagnostic_sink, class Handler = no_directive_handler>
class preprocessor {
public:
  // Resolver signatures for __has_include / __has_embed. Receive the full header
  // name including delimiters (`<foo.h>` or `"foo.h"`). Null = not found.
  // std::function_ref would allow captures but its operator() is not constexpr
  // per [func.wrap.ref.class] — type-erased invocation can't be evaluated in a
  // constant expression, so function_ref is unusable in our compile-time tests.
  using include_resolver_t = bool (*)(std::string_view header_name);
  using embed_resolver_t = int (*)(std::string_view header_name);

  // #include resolver: receives the header name (with delimiters) and the
  // current file's path; on hit, writes the canonical path and file contents
  // into the two out-parameters and returns true. Null resolver → #include
  // errors.
  //
  // Why out-parameters instead of returning a struct? gcc-trunk (as of
  // 16.0.1) rejects with "dereferencing a null pointer" when a captureless
  // lambda is converted via `+[]` to a function pointer and called through
  // that pointer in constant evaluation, if the return type is a class with
  // non-trivial members (std::string, std::optional<std::string>, …). A
  // named `constexpr` function with the same return type works; a trivial
  // return type (e.g. `struct { int; int; }`) through the same `+[]`
  // conversion also works. Likely cause: sret ABI uses a hidden return
  // pointer that gcc's constexpr interpreter doesn't wire up through the
  // synthesized lambda→fp thunk. Returning bool keeps the return trivial
  // and preserves the `+[](...){ ... }` idiom in tests.
  using include_source_t = bool (*)(std::string_view header_name, std::string_view from_file,
                                     std::string& out_path, std::string& out_content);

  static constexpr std::size_t max_include_depth = 200;

  constexpr explicit preprocessor(lexer<Sink>& lx) : lexer_(&lx) {}

  constexpr preprocessor(lexer<Sink>& lx, Sink sink) : lexer_(&lx), sink_(std::move(sink)) {}

  constexpr preprocessor(lexer<Sink>& lx, Sink sink, Handler handler) : lexer_(&lx), sink_(std::move(sink)), handler_(std::move(handler)) {}

  constexpr macro_table& macros() noexcept { return macros_; }
  constexpr macro_table const& macros() const noexcept { return macros_; }

  constexpr void set_include_resolver(include_resolver_t r) noexcept { include_resolver_ = r; }
  constexpr void set_embed_resolver(embed_resolver_t r) noexcept { embed_resolver_ = r; }
  constexpr void set_include_source(include_source_t s) noexcept { include_source_ = s; }

  constexpr pp_token next()
  {
    while (true) {
      pending_token pt;
      if (!take_next(pt)) {
        eof_emitted_ = true;
        if (!cond_stack_.empty()) {
          report(diagnostic_level::error, cond_stack_.back().location, "unterminated conditional directive");
        }
        pp_token eof;
        eof.kind = pp_token_kind::end_of_file;
        return eof;
      }

      if (at_line_start_ && pt.tok.kind == pp_token_kind::punctuator && pt.tok.spelling == "#") {
        process_directive(pt.tok);
        at_line_start_ = true;
        continue;
      }

      if (skipping()) {
        if (pt.tok.kind == pp_token_kind::newline) at_line_start_ = true;
        else if (pt.tok.kind != pp_token_kind::whitespace) at_line_start_ = false;
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

  struct cond_frame {
    source_location location;  // of the #if/#ifdef/#ifndef that opened this frame
    bool any_taken = false;    // has any branch at this level been the selected one?
    bool currently_active = false;  // is the current branch the selected one?
    bool saw_else = false;     // has #else been seen (forbids further #elif/#else)
  };

  struct include_frame {
    std::string path;     // canonical path — backing store for lexer's file_name view
    std::string content;  // raw source — backing store for splicer's source view
    line_splicer splicer;
    lexer<Sink> lx;
    std::size_t cond_stack_depth_at_push;

    constexpr include_frame(std::string p, std::string c, std::size_t cd, Sink sink)
        : path(std::move(p)),
          content(std::move(c)),
          splicer(std::string_view{content}),
          lx(splicer, std::string_view{path}, std::move(sink)),
          cond_stack_depth_at_push(cd) {}

    include_frame(include_frame const&) = delete;
    include_frame(include_frame&&) = delete;
    include_frame& operator=(include_frame const&) = delete;
    include_frame& operator=(include_frame&&) = delete;
  };

  lexer<Sink>* lexer_;
  [[no_unique_address]] Sink sink_{};
  [[no_unique_address]] Handler handler_{};
  macro_table macros_;
  expansion_queue pending_;
  std::vector<std::indirect<std::string>> synth_strings_;
  std::vector<cond_frame> cond_stack_;
  std::vector<std::unique_ptr<include_frame>> include_stack_;
  // Popped frames are retired, not destroyed: pp_token spellings and macro
  // replacement bodies are views into frame content, so the backing storage
  // must outlive the preprocessor's token stream.
  std::vector<std::unique_ptr<include_frame>> retired_frames_;
  std::vector<std::string> pragma_once_paths_;
  include_resolver_t include_resolver_ = nullptr;
  embed_resolver_t embed_resolver_ = nullptr;
  include_source_t include_source_ = nullptr;
  bool at_line_start_ = true;
  bool eof_emitted_ = false;

  constexpr lexer<Sink>& active_lexer() noexcept
  {
    if (!include_stack_.empty()) return include_stack_.back()->lx;
    return *lexer_;
  }

  constexpr std::string_view current_file_path() const noexcept
  {
    if (!include_stack_.empty()) return std::string_view{include_stack_.back()->path};
    return lexer_->file_name();
  }

  constexpr bool take_next(pending_token& pt)
  {
    if (!pending_.empty()) {
      pt = std::move(pending_.front());
      pending_.erase(pending_.begin());
      return true;
    }
    while (true) {
      auto tok = active_lexer().next();
      if (tok.kind != pp_token_kind::end_of_file) {
        pt.tok = std::move(tok);
        pt.hideset.clear();
        return true;
      }
      if (include_stack_.empty()) return false;
      auto const depth = include_stack_.back()->cond_stack_depth_at_push;
      if (cond_stack_.size() > depth) {
        report(diagnostic_level::error, cond_stack_.back().location,
               "unterminated conditional directive in included file");
        cond_stack_.resize(depth);
      }
      retired_frames_.push_back(std::move(include_stack_.back()));
      include_stack_.pop_back();
      at_line_start_ = true;
    }
  }

  constexpr void collect_until_newline(std::vector<pp_token>& out)
  {
    while (true) {
      auto t = active_lexer().next();
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
      name_tok = active_lexer().next();
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
        if (auto hn = active_lexer().try_lex_header_name()) {
          d.tokens.push_back(*hn);
          break;
        }
        auto t = active_lexer().next();
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
      case directive_kind::if_:       handle_if(d); break;
      case directive_kind::ifdef:     handle_ifdef(d); break;
      case directive_kind::ifndef:    handle_ifndef(d); break;
      case directive_kind::elif:      handle_elif(d); break;
      case directive_kind::elifdef:   handle_elifdef(d); break;
      case directive_kind::elifndef:  handle_elifndef(d); break;
      case directive_kind::else_:     handle_else(d); break;
      case directive_kind::endif:     handle_endif(d); break;
      case directive_kind::define:
        if (!skipping()) handle_define(d);
        break;
      case directive_kind::undef:
        if (!skipping()) handle_undef(d);
        break;
      case directive_kind::include:
        if (!skipping()) handle_include(d);
        break;
      case directive_kind::pragma:
        if (!skipping()) handle_pragma(d);
        break;
      default:
        break;
    }

    handler_(d);
  }

  constexpr bool skipping() const noexcept
  {
    for (auto const& f : cond_stack_) {
      if (!f.currently_active) return true;
    }
    return false;
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

  // ------------------------------------------------------------------------
  // Conditional directive handlers.
  // ------------------------------------------------------------------------

  constexpr void handle_ifdef(parsed_directive const& d)
  {
    auto name = extract_single_identifier(d, "#ifdef");
    bool const cond = !name.empty() && macros_.defined(name);
    push_if(d.location, skipping() ? false : cond);
  }

  constexpr void handle_ifndef(parsed_directive const& d)
  {
    auto name = extract_single_identifier(d, "#ifndef");
    bool const cond = !name.empty() && !macros_.defined(name);
    push_if(d.location, skipping() ? false : cond);
  }

  constexpr void handle_if(parsed_directive const& d)
  {
    bool cond = false;
    if (!skipping()) cond = evaluate_condition(d.tokens, d.location);
    push_if(d.location, cond);
  }

  constexpr void handle_elif(parsed_directive const& d)
  {
    if (cond_stack_.empty()) {
      report(diagnostic_level::error, d.location, "#elif without matching #if");
      return;
    }
    auto& f = cond_stack_.back();
    if (f.saw_else) {
      report(diagnostic_level::error, d.location, "#elif after #else");
      f.currently_active = false;
      return;
    }
    if (f.any_taken || !parent_active()) {
      f.currently_active = false;
      return;
    }
    bool const cond = evaluate_condition(d.tokens, d.location);
    f.currently_active = cond;
    f.any_taken = cond;
  }

  constexpr void handle_elifdef(parsed_directive const& d)
  {
    handle_elif_defined_like(d, "#elifdef", /*negate=*/false);
  }

  constexpr void handle_elifndef(parsed_directive const& d)
  {
    handle_elif_defined_like(d, "#elifndef", /*negate=*/true);
  }

  constexpr void handle_elif_defined_like(parsed_directive const& d, std::string_view name, bool negate)
  {
    if (cond_stack_.empty()) {
      report(diagnostic_level::error, d.location, std::string{name} + " without matching #if");
      return;
    }
    auto& f = cond_stack_.back();
    if (f.saw_else) {
      report(diagnostic_level::error, d.location, std::string{name} + " after #else");
      f.currently_active = false;
      return;
    }
    if (f.any_taken || !parent_active()) {
      f.currently_active = false;
      return;
    }
    auto id = extract_single_identifier(d, name);
    bool cond = !id.empty() && macros_.defined(id);
    if (negate) cond = !cond;
    f.currently_active = cond;
    f.any_taken = cond;
  }

  constexpr void handle_else(parsed_directive const& d)
  {
    if (cond_stack_.empty()) {
      report(diagnostic_level::error, d.location, "#else without matching #if");
      return;
    }
    auto& f = cond_stack_.back();
    if (f.saw_else) {
      report(diagnostic_level::error, d.location, "#else after #else");
      f.currently_active = false;
      return;
    }
    f.saw_else = true;
    f.currently_active = parent_active() && !f.any_taken;
    if (f.currently_active) f.any_taken = true;
  }

  constexpr bool parent_active() const noexcept
  {
    if (cond_stack_.empty()) return true;
    for (std::size_t i = 0; i + 1 < cond_stack_.size(); ++i) {
      if (!cond_stack_[i].currently_active) return false;
    }
    return true;
  }

  constexpr void handle_endif(parsed_directive const& d)
  {
    if (cond_stack_.empty()) {
      report(diagnostic_level::error, d.location, "#endif without matching #if");
      return;
    }
    cond_stack_.pop_back();
  }

  constexpr void push_if(source_location loc, bool cond)
  {
    cond_frame f;
    f.location = loc;
    f.currently_active = cond;
    f.any_taken = cond;
    cond_stack_.push_back(f);
  }

  constexpr std::string_view extract_single_identifier(parsed_directive const& d, std::string_view directive_name)
  {
    auto const& toks = d.tokens;
    std::size_t i = 0;
    while (i < toks.size() && toks[i].kind == pp_token_kind::whitespace) ++i;
    if (i >= toks.size() || toks[i].kind != pp_token_kind::identifier) {
      report(diagnostic_level::error, d.location, std::string{directive_name} + " expects an identifier");
      return {};
    }
    return toks[i].spelling;
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

  constexpr void handle_include(parsed_directive const& d)
  {
    auto const& toks = d.tokens;
    std::size_t i = 0;
    while (i < toks.size() && toks[i].kind == pp_token_kind::whitespace) ++i;
    if (i >= toks.size()) {
      report(diagnostic_level::error, d.location, "#include expects a header name");
      return;
    }

    std::string header = extract_header_name(toks, i, d.location);
    if (header.empty()) return;

    if (!include_source_) {
      report(diagnostic_level::error, d.location, "#include not supported: no source resolver configured");
      return;
    }

    std::string resolved_path;
    std::string resolved_content;
    bool const ok = include_source_(
        std::string_view{header}, current_file_path(),
        resolved_path, resolved_content);
    if (!ok) {
      report(diagnostic_level::error, d.location, "cannot open #include: " + header);
      return;
    }

    if (std::ranges::contains(pragma_once_paths_, resolved_path)) {
      return;
    }

    if (include_stack_.size() >= max_include_depth) {
      report(diagnostic_level::error, d.location, "#include depth exceeded");
      return;
    }

    include_stack_.push_back(std::make_unique<include_frame>(
        std::move(resolved_path), std::move(resolved_content),
        cond_stack_.size(), sink_));
    at_line_start_ = true;
  }

  constexpr void handle_pragma(parsed_directive const& d)
  {
    auto const& toks = d.tokens;
    std::size_t i = 0;
    while (i < toks.size() && toks[i].kind == pp_token_kind::whitespace) ++i;
    if (i < toks.size() && toks[i].kind == pp_token_kind::identifier && toks[i].spelling == "once") {
      std::string path{current_file_path()};
      if (!std::ranges::contains(pragma_once_paths_, path)) {
        pragma_once_paths_.push_back(std::move(path));
      }
    }
    (void)d;
  }

  // Parse a header name from `toks[i..]` — either a literal header_name token,
  // a "..." string literal, a <...> sequence, or a macro-expanded form.
  constexpr std::string extract_header_name(
      std::vector<pp_token> const& toks, std::size_t i, source_location loc)
  {
    if (toks[i].kind == pp_token_kind::header_name) {
      return std::string{toks[i].spelling};
    }

    std::vector<pp_token> rest;
    rest.reserve(toks.size() - i);
    for (std::size_t k = i; k < toks.size(); ++k) rest.push_back(toks[k]);
    auto expanded = expand_tokens(std::move(rest));
    return reconstruct_header_from_tokens(expanded, loc);
  }

  constexpr std::string reconstruct_header_from_tokens(
      std::vector<pp_token> const& v, source_location loc)
  {
    std::size_t j = 0;
    while (j < v.size() && (v[j].kind == pp_token_kind::whitespace || v[j].kind == pp_token_kind::newline)) ++j;
    if (j >= v.size()) {
      report(diagnostic_level::error, loc, "#include expects a header name");
      return {};
    }
    if (v[j].kind == pp_token_kind::string_literal) {
      return std::string{v[j].spelling};
    }
    if (v[j].kind == pp_token_kind::header_name) {
      return std::string{v[j].spelling};
    }
    if (v[j].kind == pp_token_kind::punctuator && v[j].spelling == "<") {
      std::string out;
      out.push_back('<');
      ++j;
      while (j < v.size()) {
        auto const& tk = v[j];
        if (tk.kind == pp_token_kind::punctuator && tk.spelling == ">") {
          out.push_back('>');
          return out;
        }
        if (tk.kind == pp_token_kind::newline) break;
        if (tk.kind != pp_token_kind::whitespace) out.append(tk.spelling);
        ++j;
      }
      report(diagnostic_level::error, loc, "unterminated '<...>' header name in #include");
      return {};
    }
    report(diagnostic_level::error, loc, "#include expects a header name");
    return {};
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
        auto t = active_lexer().next();
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

  // ------------------------------------------------------------------------
  // #if constant-expression evaluator.
  // ------------------------------------------------------------------------

  struct ce_ctx {
    std::vector<pp_token> const* toks;
    std::size_t pos = 0;
    bool had_error = false;
    source_location err_loc;
    std::string err_msg;
  };

  constexpr bool evaluate_condition(std::vector<pp_token> const& raw, source_location loc)
  {
    auto after_defined = apply_defined(raw, loc);
    auto after_has = apply_has_like(std::move(after_defined));
    auto expanded = expand_tokens(std::move(after_has));
    ce_ctx c{&expanded};
    long long v = ce_parse_ternary(c);
    ce_skip_ws(c);
    if (!c.had_error && c.pos < expanded.size()) {
      c.had_error = true;
      c.err_loc = expanded[c.pos].location;
      c.err_msg = "unexpected token in #if expression";
    }
    if (c.had_error) {
      report(diagnostic_level::error, c.err_msg.empty() ? loc : c.err_loc, c.err_msg.empty() ? "invalid #if expression" : c.err_msg);
      return false;
    }
    return v != 0;
  }

  constexpr std::vector<pp_token> apply_defined(std::vector<pp_token> const& in, source_location loc)
  {
    std::vector<pp_token> out;
    out.reserve(in.size());
    std::size_t i = 0;
    auto skip_ws = [&](std::size_t j) {
      while (j < in.size() && in[j].kind == pp_token_kind::whitespace) ++j;
      return j;
    };
    while (i < in.size()) {
      auto const& t = in[i];
      if (t.kind == pp_token_kind::identifier && t.spelling == "defined") {
        std::size_t j = skip_ws(i + 1);
        bool paren = false;
        if (j < in.size() && in[j].kind == pp_token_kind::punctuator && in[j].spelling == "(") {
          paren = true;
          j = skip_ws(j + 1);
        }
        if (j >= in.size() || in[j].kind != pp_token_kind::identifier) {
          report(diagnostic_level::error, t.location, "'defined' must be followed by an identifier");
          out.push_back(t);
          ++i;
          continue;
        }
        bool is_def = macros_.defined(in[j].spelling);
        std::size_t next = j + 1;
        if (paren) {
          next = skip_ws(next);
          if (next >= in.size() || in[next].kind != pp_token_kind::punctuator || in[next].spelling != ")") {
            report(diagnostic_level::error, t.location, "expected ')' after argument to 'defined'");
            out.push_back(t);
            ++i;
            continue;
          }
          ++next;
        }
        pp_token n;
        n.kind = pp_token_kind::pp_number;
        n.spelling = is_def ? std::string_view{"1"} : std::string_view{"0"};
        n.location = t.location;
        out.push_back(n);
        i = next;
        continue;
      }
      out.push_back(t);
      ++i;
    }
    (void)loc;
    return out;
  }

  // Resolve __has_include / __has_embed / __has_cpp_attribute to integer tokens.
  // Runs after apply_defined and before macro expansion.
  constexpr std::vector<pp_token> apply_has_like(std::vector<pp_token> in)
  {
    std::vector<pp_token> out;
    out.reserve(in.size());
    std::size_t i = 0;
    while (i < in.size()) {
      auto const& t = in[i];
      if (t.kind == pp_token_kind::identifier) {
        if (t.spelling == "__has_include") {
          auto [v, next] = eval_has_include(in, i);
          out.push_back(make_pp_number_token(v, t.location));
          i = next;
          continue;
        }
        if (t.spelling == "__has_embed") {
          auto [v, next] = eval_has_embed(in, i);
          out.push_back(make_pp_number_token(v, t.location));
          i = next;
          continue;
        }
        if (t.spelling == "__has_cpp_attribute") {
          auto [v, next] = eval_has_cpp_attribute(in, i);
          out.push_back(make_pp_number_token(v, t.location));
          i = next;
          continue;
        }
      }
      out.push_back(t);
      ++i;
    }
    return out;
  }

  static constexpr std::size_t skip_ws_at(std::vector<pp_token> const& v, std::size_t j) noexcept
  {
    while (j < v.size() && v[j].kind == pp_token_kind::whitespace) ++j;
    return j;
  }

  // Parse the `( header-or-identifier )` suffix of a __has_include / __has_embed call.
  // Returns (header_name_string, next_index). On failure, header is empty and next_index
  // points past whatever we consumed.
  constexpr std::pair<std::string, std::size_t> parse_header_arg(
      std::vector<pp_token> const& in, std::size_t start, source_location loc)
  {
    std::size_t j = skip_ws_at(in, start);
    if (j >= in.size() || in[j].kind != pp_token_kind::punctuator || in[j].spelling != "(") {
      report(diagnostic_level::error, loc, "expected '(' after __has_include/__has_embed");
      return {{}, j};
    }
    j = skip_ws_at(in, j + 1);
    std::string header;
    if (j < in.size() && in[j].kind == pp_token_kind::string_literal) {
      header.assign(in[j].spelling);
      ++j;
    } else if (j < in.size() && in[j].kind == pp_token_kind::punctuator && in[j].spelling == "<") {
      header.push_back('<');
      ++j;
      bool closed = false;
      while (j < in.size()) {
        auto const& tk = in[j];
        if (tk.kind == pp_token_kind::punctuator && tk.spelling == ">") {
          header.push_back('>');
          ++j;
          closed = true;
          break;
        }
        if (tk.kind == pp_token_kind::newline) break;
        if (tk.kind != pp_token_kind::whitespace) header.append(tk.spelling);
        ++j;
      }
      if (!closed) {
        report(diagnostic_level::error, loc, "unterminated '<...>' header name");
        return {{}, j};
      }
    } else {
      report(diagnostic_level::error, loc, "expected '<...>' or \"...\" header name");
      return {{}, j};
    }
    // Skip any trailing pp-parameters (space-separated, balanced parens).
    // E.g. `__has_embed(<h> limit(0) prefix(x))`.
    {
      int depth = 0;
      while (j < in.size()) {
        auto const& tk = in[j];
        if (tk.kind == pp_token_kind::punctuator && tk.spelling == "(") ++depth;
        else if (tk.kind == pp_token_kind::punctuator && tk.spelling == ")") {
          if (depth == 0) break;
          --depth;
        }
        ++j;
      }
    }
    if (j >= in.size() || in[j].kind != pp_token_kind::punctuator || in[j].spelling != ")") {
      report(diagnostic_level::error, loc, "expected ')'");
      return {std::move(header), j};
    }
    ++j;
    return {std::move(header), j};
  }

  constexpr std::pair<long long, std::size_t> eval_has_include(
      std::vector<pp_token> const& in, std::size_t i)
  {
    auto [header, next] = parse_header_arg(in, i + 1, in[i].location);
    if (header.empty()) return {0, next};
    bool found = include_resolver_ && include_resolver_(header);
    return {found ? 1 : 0, next};
  }

  constexpr std::pair<long long, std::size_t> eval_has_embed(
      std::vector<pp_token> const& in, std::size_t i)
  {
    auto [header, next] = parse_header_arg(in, i + 1, in[i].location);
    if (header.empty()) return {0, next};
    int v = embed_resolver_ ? embed_resolver_(header) : 0;
    return {v, next};
  }

  constexpr std::pair<long long, std::size_t> eval_has_cpp_attribute(
      std::vector<pp_token> const& in, std::size_t i)
  {
    auto const loc = in[i].location;
    std::size_t j = skip_ws_at(in, i + 1);
    if (j >= in.size() || in[j].kind != pp_token_kind::punctuator || in[j].spelling != "(") {
      report(diagnostic_level::error, loc, "expected '(' after __has_cpp_attribute");
      return {0, j};
    }
    j = skip_ws_at(in, j + 1);
    if (j >= in.size() || in[j].kind != pp_token_kind::identifier) {
      report(diagnostic_level::error, loc, "expected attribute name");
      return {0, j};
    }
    std::string name{in[j].spelling};
    ++j;
    std::size_t k = skip_ws_at(in, j);
    if (k < in.size() && in[k].kind == pp_token_kind::punctuator && in[k].spelling == "::") {
      k = skip_ws_at(in, k + 1);
      if (k < in.size() && in[k].kind == pp_token_kind::identifier) {
        name.append("::");
        name.append(in[k].spelling);
        j = k + 1;
      }
    }
    j = skip_ws_at(in, j);
    if (j >= in.size() || in[j].kind != pp_token_kind::punctuator || in[j].spelling != ")") {
      report(diagnostic_level::error, loc, "expected ')'");
      return {0, j};
    }
    ++j;
    return {cpp_attribute_version(name), j};
  }

  static constexpr long long cpp_attribute_version(std::string_view name) noexcept
  {
    if (name == "assume") return 202207LL;
    if (name == "carries_dependency") return 200809LL;
    if (name == "deprecated") return 201309LL;
    if (name == "fallthrough") return 201603LL;
    if (name == "likely") return 201803LL;
    if (name == "maybe_unused") return 201603LL;
    if (name == "no_unique_address") return 201803LL;
    if (name == "nodiscard") return 201907LL;
    if (name == "noreturn") return 200809LL;
    if (name == "unlikely") return 201803LL;
    return 0;
  }

  constexpr pp_token make_pp_number_token(long long v, source_location loc)
  {
    std::string s;
    if (v == 0) {
      s = "0";
    } else {
      unsigned long long u = static_cast<unsigned long long>(v);
      std::string digits;
      while (u > 0) {
        digits.push_back(static_cast<char>('0' + (u % 10)));
        u /= 10;
      }
      for (auto it = digits.rbegin(); it != digits.rend(); ++it) s.push_back(*it);
    }
    synth_strings_.emplace_back(std::move(s));
    pp_token t;
    t.kind = pp_token_kind::pp_number;
    t.spelling = std::string_view{*synth_strings_.back()};
    t.location = loc;
    return t;
  }

  static constexpr void ce_skip_ws(ce_ctx& c) noexcept
  {
    while (c.pos < c.toks->size()) {
      auto k = (*c.toks)[c.pos].kind;
      if (k == pp_token_kind::whitespace || k == pp_token_kind::newline) ++c.pos;
      else break;
    }
  }

  static constexpr bool ce_peek_punct(ce_ctx& c, std::string_view sp) noexcept
  {
    ce_skip_ws(c);
    if (c.pos >= c.toks->size()) return false;
    auto const& t = (*c.toks)[c.pos];
    return t.kind == pp_token_kind::punctuator && t.spelling == sp;
  }

  static constexpr bool ce_match_punct(ce_ctx& c, std::string_view sp) noexcept
  {
    if (!ce_peek_punct(c, sp)) return false;
    ++c.pos;
    return true;
  }

  static constexpr void ce_error(ce_ctx& c, std::string msg) noexcept
  {
    if (c.had_error) return;
    c.had_error = true;
    if (c.pos < c.toks->size()) c.err_loc = (*c.toks)[c.pos].location;
    c.err_msg = std::move(msg);
  }

  static constexpr long long ce_parse_ternary(ce_ctx& c)
  {
    long long cond = ce_parse_logor(c);
    if (ce_match_punct(c, "?")) {
      long long lhs = ce_parse_ternary(c);
      if (!ce_match_punct(c, ":")) {
        ce_error(c, "expected ':' in ternary");
        return 0;
      }
      long long rhs = ce_parse_ternary(c);
      return cond ? lhs : rhs;
    }
    return cond;
  }

  static constexpr long long ce_parse_logor(ce_ctx& c)
  {
    long long lhs = ce_parse_logand(c);
    while (ce_match_punct(c, "||")) {
      long long rhs = ce_parse_logand(c);
      lhs = (lhs || rhs) ? 1 : 0;
    }
    return lhs;
  }

  static constexpr long long ce_parse_logand(ce_ctx& c)
  {
    long long lhs = ce_parse_bitor(c);
    while (ce_match_punct(c, "&&")) {
      long long rhs = ce_parse_bitor(c);
      lhs = (lhs && rhs) ? 1 : 0;
    }
    return lhs;
  }

  static constexpr long long ce_parse_bitor(ce_ctx& c)
  {
    long long lhs = ce_parse_bitxor(c);
    while (true) {
      ce_skip_ws(c);
      if (ce_peek_punct(c, "||")) break;  // don't mis-split ||
      if (!ce_match_punct(c, "|")) break;
      long long rhs = ce_parse_bitxor(c);
      lhs = static_cast<long long>(static_cast<unsigned long long>(lhs) | static_cast<unsigned long long>(rhs));
    }
    return lhs;
  }

  static constexpr long long ce_parse_bitxor(ce_ctx& c)
  {
    long long lhs = ce_parse_bitand(c);
    while (ce_match_punct(c, "^")) {
      long long rhs = ce_parse_bitand(c);
      lhs = static_cast<long long>(static_cast<unsigned long long>(lhs) ^ static_cast<unsigned long long>(rhs));
    }
    return lhs;
  }

  static constexpr long long ce_parse_bitand(ce_ctx& c)
  {
    long long lhs = ce_parse_eq(c);
    while (true) {
      if (ce_peek_punct(c, "&&")) break;
      if (!ce_match_punct(c, "&")) break;
      long long rhs = ce_parse_eq(c);
      lhs = static_cast<long long>(static_cast<unsigned long long>(lhs) & static_cast<unsigned long long>(rhs));
    }
    return lhs;
  }

  static constexpr long long ce_parse_eq(ce_ctx& c)
  {
    long long lhs = ce_parse_rel(c);
    while (true) {
      if (ce_match_punct(c, "==")) {
        long long rhs = ce_parse_rel(c);
        lhs = (lhs == rhs) ? 1 : 0;
      } else if (ce_match_punct(c, "!=")) {
        long long rhs = ce_parse_rel(c);
        lhs = (lhs != rhs) ? 1 : 0;
      } else break;
    }
    return lhs;
  }

  static constexpr long long ce_parse_rel(ce_ctx& c)
  {
    long long lhs = ce_parse_shift(c);
    while (true) {
      if (ce_match_punct(c, "<=")) {
        long long rhs = ce_parse_shift(c);
        lhs = (lhs <= rhs) ? 1 : 0;
      } else if (ce_match_punct(c, ">=")) {
        long long rhs = ce_parse_shift(c);
        lhs = (lhs >= rhs) ? 1 : 0;
      } else if (ce_match_punct(c, "<")) {
        long long rhs = ce_parse_shift(c);
        lhs = (lhs < rhs) ? 1 : 0;
      } else if (ce_match_punct(c, ">")) {
        long long rhs = ce_parse_shift(c);
        lhs = (lhs > rhs) ? 1 : 0;
      } else break;
    }
    return lhs;
  }

  static constexpr long long ce_parse_shift(ce_ctx& c)
  {
    long long lhs = ce_parse_add(c);
    while (true) {
      if (ce_match_punct(c, "<<")) {
        long long rhs = ce_parse_add(c);
        unsigned long long u = static_cast<unsigned long long>(lhs);
        lhs = static_cast<long long>(u << (rhs & 63));
      } else if (ce_match_punct(c, ">>")) {
        long long rhs = ce_parse_add(c);
        lhs = lhs >> (rhs & 63);
      } else break;
    }
    return lhs;
  }

  static constexpr long long ce_parse_add(ce_ctx& c)
  {
    long long lhs = ce_parse_mul(c);
    while (true) {
      if (ce_match_punct(c, "+")) {
        long long rhs = ce_parse_mul(c);
        lhs = static_cast<long long>(static_cast<unsigned long long>(lhs) + static_cast<unsigned long long>(rhs));
      } else if (ce_match_punct(c, "-")) {
        long long rhs = ce_parse_mul(c);
        lhs = static_cast<long long>(static_cast<unsigned long long>(lhs) - static_cast<unsigned long long>(rhs));
      } else break;
    }
    return lhs;
  }

  static constexpr long long ce_parse_mul(ce_ctx& c)
  {
    long long lhs = ce_parse_unary(c);
    while (true) {
      if (ce_match_punct(c, "*")) {
        long long rhs = ce_parse_unary(c);
        lhs = static_cast<long long>(static_cast<unsigned long long>(lhs) * static_cast<unsigned long long>(rhs));
      } else if (ce_match_punct(c, "/")) {
        long long rhs = ce_parse_unary(c);
        if (rhs == 0) { ce_error(c, "division by zero in #if"); return 0; }
        lhs = lhs / rhs;
      } else if (ce_match_punct(c, "%")) {
        long long rhs = ce_parse_unary(c);
        if (rhs == 0) { ce_error(c, "modulo by zero in #if"); return 0; }
        lhs = lhs % rhs;
      } else break;
    }
    return lhs;
  }

  static constexpr long long ce_parse_unary(ce_ctx& c)
  {
    if (ce_match_punct(c, "+")) return ce_parse_unary(c);
    if (ce_match_punct(c, "-")) return -ce_parse_unary(c);
    if (ce_match_punct(c, "!")) return ce_parse_unary(c) == 0 ? 1 : 0;
    if (ce_match_punct(c, "~")) return static_cast<long long>(~static_cast<unsigned long long>(ce_parse_unary(c)));
    return ce_parse_primary(c);
  }

  static constexpr long long ce_parse_primary(ce_ctx& c)
  {
    ce_skip_ws(c);
    if (c.pos >= c.toks->size()) {
      ce_error(c, "unexpected end of #if expression");
      return 0;
    }
    auto const& t = (*c.toks)[c.pos];
    if (t.kind == pp_token_kind::punctuator && t.spelling == "(") {
      ++c.pos;
      long long v = ce_parse_ternary(c);
      if (!ce_match_punct(c, ")")) ce_error(c, "expected ')'");
      return v;
    }
    if (t.kind == pp_token_kind::pp_number) {
      ++c.pos;
      return parse_integer_literal(t.spelling);
    }
    if (t.kind == pp_token_kind::character_literal) {
      ++c.pos;
      return parse_character_literal(t.spelling);
    }
    if (t.kind == pp_token_kind::identifier) {
      auto sp = t.spelling;
      ++c.pos;
      if (sp == "true") return 1;
      if (sp == "false") return 0;
      return 0;  // undefined identifiers evaluate to 0
    }
    ce_error(c, "expected integer constant in #if expression");
    ++c.pos;
    return 0;
  }

  static constexpr long long parse_integer_literal(std::string_view s) noexcept
  {
    std::string norm;
    norm.reserve(s.size());
    for (char ch : s) {
      if (ch == '\'') continue;  // digit separator
      norm.push_back(ch);
    }
    std::size_t i = 0;
    int base = 10;
    if (norm.size() >= 2 && norm[0] == '0' && (norm[1] == 'x' || norm[1] == 'X')) {
      base = 16;
      i = 2;
    } else if (norm.size() >= 2 && norm[0] == '0' && (norm[1] == 'b' || norm[1] == 'B')) {
      base = 2;
      i = 2;
    } else if (!norm.empty() && norm[0] == '0') {
      base = 8;
    }
    unsigned long long val = 0;
    while (i < norm.size()) {
      char ch = norm[i];
      int d = -1;
      if (ch >= '0' && ch <= '9') d = ch - '0';
      else if (ch >= 'a' && ch <= 'f') d = 10 + (ch - 'a');
      else if (ch >= 'A' && ch <= 'F') d = 10 + (ch - 'A');
      else break;
      if (d >= base) break;
      val = val * static_cast<unsigned long long>(base) + static_cast<unsigned long long>(d);
      ++i;
    }
    return static_cast<long long>(val);
  }

  static constexpr long long parse_character_literal(std::string_view s) noexcept
  {
    std::size_t i = 0;
    if (i < s.size() && (s[i] == 'L' || s[i] == 'u' || s[i] == 'U')) {
      ++i;
      if (i < s.size() && s[i] == '8') ++i;
    }
    if (i >= s.size() || s[i] != '\'') return 0;
    ++i;
    if (i >= s.size()) return 0;
    if (s[i] == '\\') {
      ++i;
      if (i >= s.size()) return 0;
      char e = s[i];
      ++i;
      switch (e) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '"';
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'v': return '\v';
        case '?': return '?';
        case 'x': {
          unsigned long long v = 0;
          while (i < s.size() && s[i] != '\'') {
            char ch = s[i];
            int d = -1;
            if (ch >= '0' && ch <= '9') d = ch - '0';
            else if (ch >= 'a' && ch <= 'f') d = 10 + (ch - 'a');
            else if (ch >= 'A' && ch <= 'F') d = 10 + (ch - 'A');
            else break;
            v = v * 16 + static_cast<unsigned long long>(d);
            ++i;
          }
          return static_cast<long long>(v);
        }
        default:
          if (e >= '0' && e <= '7') {
            unsigned long long v = static_cast<unsigned long long>(e - '0');
            int count = 1;
            while (i < s.size() && count < 3 && s[i] >= '0' && s[i] <= '7') {
              v = v * 8 + static_cast<unsigned long long>(s[i] - '0');
              ++i;
              ++count;
            }
            return static_cast<long long>(v);
          }
          return static_cast<unsigned char>(e);
      }
    }
    return static_cast<unsigned char>(s[i]);
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
