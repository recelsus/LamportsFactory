#pragma once

#include <deque>
#include <mutex>
#include <string>

namespace lf {

class build_log_buffer {
public:
  explicit build_log_buffer(int max_lines);
  void append(const std::string& text);
  std::string tail_joined(std::size_t count) const;
  int capacity() const;

private:
  int max_lines;
  mutable std::mutex mutex;
  std::deque<std::string> lines;
  std::string partial;
};

}  // namespace lf

