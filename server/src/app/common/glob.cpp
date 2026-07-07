#include "app/common/utils.hpp"

#include <regex>

namespace lf {
namespace {

std::string glob_to_regex_pattern(const std::string& glob) {
  std::string pattern = "^";
  for (std::size_t i = 0; i < glob.size(); ++i) {
    const char c = glob[i];
    if (c == '*') {
      if (i + 1 < glob.size() && glob[i + 1] == '*') {
        const bool has_slash = (i + 2 < glob.size() && glob[i + 2] == '/');
        pattern += ".*";
        if (has_slash) {
          i += 2;
        } else {
          i += 1;
        }
        continue;
      }
      pattern += "[^/]*";
      continue;
    }
    if (c == '?') {
      pattern += ".";
      continue;
    }
    if (c == '.') {
      pattern += "\\.";
      continue;
    }
    if (c == '+') {
      pattern += "\\+";
      continue;
    }
    if (c == '(' || c == ')' || c == '{' || c == '}' || c == '|' ||
        c == '^' || c == '$') {
      pattern.push_back('\\');
      pattern.push_back(c);
      continue;
    }
    pattern.push_back(c);
  }
  pattern += "$";
  return pattern;
}

}  // namespace

std::vector<std::string> split_multi_glob(const std::string& text) {
  std::vector<std::string> result;
  std::string current;
  for (char c : text) {
    if (c == ';' || c == ',') {
      current = trim_copy(current);
      if (!current.empty()) {
        result.push_back(current);
      }
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  current = trim_copy(current);
  if (!current.empty()) {
    result.push_back(current);
  }
  if (result.empty()) {
    result.push_back("**/*.tex");
  }
  return result;
}

std::vector<std::regex> compile_globs(const std::vector<std::string>& globs) {
  std::vector<std::regex> patterns;
  patterns.reserve(globs.size());
  for (const auto& glob : globs) {
    const std::string trimmed = trim_copy(glob);
    if (trimmed.empty()) {
      continue;
    }
    patterns.emplace_back(glob_to_regex_pattern(trimmed),
                          std::regex::ECMAScript | std::regex::icase);
  }
  return patterns;
}

}  // namespace lf
