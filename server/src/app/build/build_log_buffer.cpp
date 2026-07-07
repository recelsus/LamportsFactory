#include "app/build/build_log_buffer.hpp"

#include <sstream>

namespace lf {

build_log_buffer::build_log_buffer(int max_lines) : max_lines(max_lines) {}

void build_log_buffer::append(const std::string& text) {
  std::lock_guard<std::mutex> lock(mutex);
  std::size_t start = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\n') {
      const std::string line = partial + text.substr(start, i - start);
      lines.push_back(line);
      partial.clear();
      start = i + 1;
    }
  }
  if (start < text.size()) {
    partial += text.substr(start);
  }
  while (static_cast<int>(lines.size()) > max_lines) {
    lines.pop_front();
  }
}

std::string build_log_buffer::tail_joined(std::size_t count) const {
  std::lock_guard<std::mutex> lock(mutex);
  std::ostringstream out;
  if (count > lines.size()) {
    count = lines.size();
  }
  const std::size_t offset = lines.size() - count;
  for (std::size_t i = 0; i < count; ++i) {
    out << lines[offset + i];
    if (i + 1 < count) {
      out << '\n';
    }
  }
  if (!partial.empty()) {
    if (count != 0U) {
      out << '\n';
    }
    out << partial;
  }
  return out.str();
}

int build_log_buffer::capacity() const {
  return max_lines;
}

}  // namespace lf
