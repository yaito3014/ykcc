#ifndef YK_ASTEROID_PREPROCESS_LINE_SPLICE_HPP
#define YK_ASTEROID_PREPROCESS_LINE_SPLICE_HPP

#include <string>
#include <string_view>
#include <vector>

#include <cstddef>

namespace yk::asteroid::preprocess {

struct splice_record {
  std::size_t original_pos;  // position of '\' in original
  std::size_t length;        // number of chars removed (e.g., 2 for \\\n)
};

struct spliced_source {
  std::string text;
  std::vector<splice_record> splices;

  // Map a position in spliced text to a position in original text
  constexpr std::size_t to_original(std::size_t spliced_pos) const noexcept
  {
    std::size_t offset = 0;
    for (auto const& s : splices) {
      if (s.original_pos - offset > spliced_pos) break;
      offset += s.length;
    }
    return spliced_pos + offset;
  }

  // Get range [begin, end) in original source from range in spliced text
  constexpr std::string_view original_range(std::string_view original, std::size_t spliced_begin, std::size_t spliced_end) const noexcept
  {
    return original.substr(to_original(spliced_begin), to_original(spliced_end) - to_original(spliced_begin));
  }
};

constexpr spliced_source splice_lines(std::string_view source)
{
  spliced_source result;
  result.text.reserve(source.size());

  for (std::size_t i = 0; i < source.size(); ++i) {
    if (source[i] == '\\' && i + 1 < source.size() && source[i + 1] == '\n') {
      result.splices.push_back({i, 2});
      ++i;  // skip \n (loop increments past \\)
    } else {
      result.text.push_back(source[i]);
    }
  }

  if (!result.text.empty() && result.text.back() != '\n') {
    result.text.push_back('\n');
  }

  return result;
}

}  // namespace yk::asteroid::preprocess

#endif  // YK_ASTEROID_PREPROCESS_LINE_SPLICE_HPP
