#include "app/common/utils.hpp"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace lf {

std::string get_env_string(const std::string& name,
                           const std::string& fallback) {
  const char* value = std::getenv(name.c_str());
  if (value == nullptr) {
    return fallback;
  }
  return std::string(value);
}

int get_env_int(const std::string& name, int fallback) {
  const char* value = std::getenv(name.c_str());
  if (value == nullptr || value[0] == '\0') {
    return fallback;
  }
  try {
    return std::stoi(value);
  } catch (...) {
    return fallback;
  }
}

bool get_env_bool(const std::string& name, bool fallback) {
  const char* value = std::getenv(name.c_str());
  if (value == nullptr) {
    return fallback;
  }
  const std::string lower = to_lower_copy(value);
  if (lower == "1" || lower == "true" || lower == "yes" || lower == "on") {
    return true;
  }
  if (lower == "0" || lower == "false" || lower == "no" || lower == "off") {
    return false;
  }
  return fallback;
}

std::vector<std::string> split_semicolon(const std::string& text) {
  std::vector<std::string> parts;
  std::string part;
  std::istringstream stream(text);
  while (std::getline(stream, part, ';')) {
    if (!part.empty()) {
      parts.push_back(part);
    }
  }
  return parts;
}

std::vector<std::string> split_whitespace(const std::string& text) {
  std::vector<std::string> parts;
  std::istringstream stream(text);
  std::string item;
  while (stream >> item) {
    parts.push_back(item);
  }
  return parts;
}

std::string trim_copy(const std::string& text) {
  std::size_t start = 0;
  while (start < text.size() &&
         std::isspace(static_cast<unsigned char>(text[start])) != 0) {
    start += 1;
  }
  std::size_t end = text.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    end -= 1;
  }
  return text.substr(start, end - start);
}

std::string to_lower_copy(const std::string& text) {
  std::string lower(text);
  for (auto& c : lower) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return lower;
}

}  // namespace lf
