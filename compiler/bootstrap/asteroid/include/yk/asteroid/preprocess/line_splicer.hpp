#ifndef YK_ASTEROID_PREPROCESS_LINE_SPLICER_HPP
#define YK_ASTEROID_PREPROCESS_LINE_SPLICER_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace yk::asteroid {

class line_splicer {
public:
  struct location {
    std::size_t line;
    std::size_t column;
  };

  constexpr explicit line_splicer(std::string_view source);

  constexpr std::string_view source() const noexcept { return source_; }
  constexpr std::string_view spliced() const noexcept { return spliced_; }

  constexpr std::size_t physical_offset(std::size_t logical_offset) const noexcept { return phys_offsets_[logical_offset]; }

  constexpr location physical_location(std::size_t logical_offset) const noexcept;

private:
  std::string_view source_;
  std::string spliced_;
  std::vector<std::size_t> phys_offsets_;
};

constexpr line_splicer::line_splicer(std::string_view source) : source_(source)
{
  spliced_.reserve(source.size());
  phys_offsets_.reserve(source.size() + 1);

  std::size_t i = 0;
  while (i < source.size()) {
    if (source[i] == '\\') {
      std::size_t j = i + 1;
      if (j < source.size() && source[j] == '\n') {
        i = j + 1;
        continue;
      }
      if (j < source.size() && source[j] == '\r') {
        i = (j + 1 < source.size() && source[j + 1] == '\n') ? j + 2 : j + 1;
        continue;
      }
      phys_offsets_.push_back(i);
      spliced_.push_back('\\');
      ++i;
      continue;
    }

    if (source[i] == '\r') {
      phys_offsets_.push_back(i);
      spliced_.push_back('\n');
      i = (i + 1 < source.size() && source[i + 1] == '\n') ? i + 2 : i + 1;
      continue;
    }

    phys_offsets_.push_back(i);
    spliced_.push_back(source[i]);
    ++i;
  }

  if (spliced_.empty() || spliced_.back() != '\n') {
    phys_offsets_.push_back(source.size());
    spliced_.push_back('\n');
  }

  phys_offsets_.push_back(source.size());
}

constexpr line_splicer::location line_splicer::physical_location(std::size_t logical_offset) const noexcept
{
  std::size_t const phys = phys_offsets_[logical_offset];
  std::size_t line = 1;
  std::size_t column = 1;
  for (std::size_t k = 0; k < phys && k < source_.size(); ++k) {
    if (source_[k] == '\n') {
      ++line;
      column = 1;
    } else if (source_[k] == '\r') {
      ++line;
      column = 1;
      if (k + 1 < source_.size() && source_[k + 1] == '\n') {
        ++k;
      }
    } else {
      ++column;
    }
  }
  return {line, column};
}

}  // namespace yk::asteroid

#endif  // YK_ASTEROID_PREPROCESS_LINE_SPLICER_HPP
