#include "app/utils.hpp"

#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace lf {

std::string get_env_string(const std::string& name, const std::string& fallback) {
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
  std::string lower(value);
  for (auto& c : lower) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
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
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
    start += 1;
  }
  std::size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
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
    if (c == '(' || c == ')' || c == '{' || c == '}' || c == '|' || c == '^' || c == '$') {
      pattern.push_back('\\');
      pattern.push_back(c);
      continue;
    }
    pattern.push_back(c);
  }
  pattern += "$";
  return pattern;
}

struct sha1_state {
  std::array<std::uint32_t, 5> h{};
  std::array<std::uint8_t, 64> buffer{};
  std::uint64_t length = 0;
  std::size_t buffer_size = 0;
};

std::uint32_t left_rotate(std::uint32_t value, std::uint32_t count) {
  return (value << count) | (value >> (32 - count));
}

void sha1_init(sha1_state& state) {
  state.h = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
  state.length = 0;
  state.buffer_size = 0;
}

void sha1_process_block(sha1_state& state) {
  std::array<std::uint32_t, 80> w{};
  for (std::size_t i = 0; i < 16; ++i) {
    const std::size_t idx = i * 4;
    w[i] = (state.buffer[idx] << 24) | (state.buffer[idx + 1] << 16) |
           (state.buffer[idx + 2] << 8) | state.buffer[idx + 3];
  }
  for (std::size_t i = 16; i < 80; ++i) {
    w[i] = left_rotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  }
  std::uint32_t a = state.h[0];
  std::uint32_t b = state.h[1];
  std::uint32_t c = state.h[2];
  std::uint32_t d = state.h[3];
  std::uint32_t e = state.h[4];
  for (std::size_t i = 0; i < 80; ++i) {
    std::uint32_t f = 0;
    std::uint32_t k = 0;
    if (i < 20) {
      f = (b & c) | ((~b) & d);
      k = 0x5A827999u;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1u;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDCu;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6u;
    }
    const std::uint32_t temp = left_rotate(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = left_rotate(b, 30);
    b = a;
    a = temp;
  }
  state.h[0] += a;
  state.h[1] += b;
  state.h[2] += c;
  state.h[3] += d;
  state.h[4] += e;
}

void sha1_update(sha1_state& state, const std::uint8_t* data, std::size_t length) {
  state.length += static_cast<std::uint64_t>(length) * 8;
  std::size_t offset = 0;
  while (length > 0) {
    const std::size_t space = 64 - state.buffer_size;
    const std::size_t chunk = std::min(space, length);
    std::memcpy(state.buffer.data() + state.buffer_size, data + offset, chunk);
    state.buffer_size += chunk;
    offset += chunk;
    length -= chunk;
    if (state.buffer_size == 64) {
      sha1_process_block(state);
      state.buffer_size = 0;
    }
  }
}

std::array<std::uint8_t, 20> sha1_finalize(sha1_state& state) {
  state.buffer[state.buffer_size++] = 0x80;
  if (state.buffer_size > 56) {
    while (state.buffer_size < 64) {
      state.buffer[state.buffer_size++] = 0;
    }
    sha1_process_block(state);
    state.buffer_size = 0;
  }
  while (state.buffer_size < 56) {
    state.buffer[state.buffer_size++] = 0;
  }
  for (int i = 7; i >= 0; --i) {
    state.buffer[state.buffer_size++] =
        static_cast<std::uint8_t>((state.length >> (i * 8)) & 0xFFu);
  }
  sha1_process_block(state);
  std::array<std::uint8_t, 20> digest{};
  for (std::size_t i = 0; i < 5; ++i) {
    digest[i * 4] = static_cast<std::uint8_t>((state.h[i] >> 24) & 0xFFu);
    digest[i * 4 + 1] = static_cast<std::uint8_t>((state.h[i] >> 16) & 0xFFu);
    digest[i * 4 + 2] = static_cast<std::uint8_t>((state.h[i] >> 8) & 0xFFu);
    digest[i * 4 + 3] = static_cast<std::uint8_t>(state.h[i] & 0xFFu);
  }
  return digest;
}

}  // namespace

std::string json_escape(const std::string& text) {
  std::ostringstream out;
  for (char ch : text) {
    switch (ch) {
      case '\\':
        out << "\\\\";
        break;
      case '"':
        out << "\\\"";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<int>(static_cast<unsigned char>(ch));
        } else {
          out << ch;
        }
        break;
    }
  }
  return out.str();
}

std::string base64_encode(const std::string& input) {
  static const char* table =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  result.reserve(((input.size() + 2) / 3) * 4);
  std::uint32_t buffer = 0;
  int bits = 0;
  for (unsigned char c : input) {
    buffer = (buffer << 8) | c;
    bits += 8;
    while (bits >= 6) {
      bits -= 6;
      result.push_back(table[(buffer >> bits) & 0x3F]);
    }
  }
  if (bits > 0) {
    buffer <<= (6 - bits);
    result.push_back(table[buffer & 0x3F]);
  }
  while (result.size() % 4 != 0) {
    result.push_back('=');
  }
  return result;
}

std::string sha1_hash_base64(const std::string& text) {
  sha1_state state;
  sha1_init(state);
  sha1_update(state, reinterpret_cast<const std::uint8_t*>(text.data()),
              text.size());
  const auto digest = sha1_finalize(state);
  return base64_encode(std::string(reinterpret_cast<const char*>(digest.data()),
                                   digest.size()));
}

std::int64_t to_epoch_millis(const std::filesystem::file_time_type& ft) {
  using namespace std::chrono;
  const auto sctp = time_point_cast<milliseconds>(
      ft - std::filesystem::file_time_type::clock::now() + system_clock::now());
  return duration_cast<milliseconds>(sctp.time_since_epoch()).count();
}

std::int64_t now_epoch_millis() {
  using namespace std::chrono;
  const auto now = time_point_cast<milliseconds>(system_clock::now());
  return now.time_since_epoch().count();
}

bool path_is_within(const std::filesystem::path& parent,
                    const std::filesystem::path& child) {
  std::error_code ec;
  auto parent_canonical = std::filesystem::weakly_canonical(parent, ec);
  if (ec) {
    ec.clear();
    parent_canonical =
        std::filesystem::weakly_canonical(std::filesystem::absolute(parent),
                                          ec);
    if (ec) {
      return false;
    }
  }
  auto child_canonical = std::filesystem::weakly_canonical(child, ec);
  if (ec) {
    ec.clear();
    child_canonical =
        std::filesystem::weakly_canonical(std::filesystem::absolute(child),
                                          ec);
    if (ec) {
      return false;
    }
  }
  auto parent_it = parent_canonical.begin();
  auto child_it = child_canonical.begin();
  for (; parent_it != parent_canonical.end() && child_it != child_canonical.end();
       ++parent_it, ++child_it) {
    if (*parent_it != *child_it) {
      return false;
    }
  }
  return parent_it == parent_canonical.end();
}

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

std::filesystem::path pdf_path_for(const std::string& out_dir,
                                   const std::string& tex_main,
                                   const std::string& fallback_pdf) {
  if (tex_main.empty()) {
    if (!fallback_pdf.empty()) {
      return std::filesystem::path(fallback_pdf);
    }
    std::filesystem::path base(out_dir);
    return base / "main.pdf";
  }
  std::filesystem::path base(out_dir);
  if (base.empty()) {
    base = std::filesystem::current_path();
  }
  std::filesystem::path main_path(tex_main);
  std::string stem = main_path.stem().string();
  if (stem.empty()) {
    if (!fallback_pdf.empty()) {
      return std::filesystem::path(fallback_pdf);
    }
    stem = "main";
  }
  return base / (stem + ".pdf");
}

}  // namespace lf
