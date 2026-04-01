#ifndef YK_ASTEROID_PREPROCESS_PREPROCESSOR_HPP
#define YK_ASTEROID_PREPROCESS_PREPROCESSOR_HPP

#include <yk/asteroid/preprocess/lexer.hpp>
#include <yk/asteroid/preprocess/line_splice.hpp>

#include <string_view>
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

class preprocessor {
public:
  explicit preprocessor(std::string_view source) : original_source_(source) {}

  void run()
  {
    phase2();
    phase3();
    phase4();
  }

  std::vector<pp_directive> directives;
  std::vector<pp_text_line> text_lines;

private:
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
    // TODO: process collected directives and text lines
  }

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
      handle_directive(std::move(directive));
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

    handle_directive(std::move(directive));
  }

  void process_text_line(iterator& it, std::default_sentinel_t end)
  {
    pp_text_line line{};

    collect_until_newline(line.tokens, it, end);
    if (it != end) {
      line.newline = *it;
      ++it;
    }

    handle_text_line(std::move(line));
  }

  static void collect_until_newline(std::vector<pp_token>& tokens, iterator& it, std::default_sentinel_t end)
  {
    while (it != end && it->kind != pp_token_kind::newline) {
      tokens.push_back(*it);
      ++it;
    }
  }

  void handle_directive(pp_directive&& directive) { directives.push_back(std::move(directive)); }

  void handle_text_line(pp_text_line&& line) { text_lines.push_back(std::move(line)); }

  std::string_view original_source_;
  spliced_source spliced_;
};

}  // namespace yk::asteroid::preprocess

#endif  // YK_ASTEROID_PREPROCESS_PREPROCESSOR_HPP
