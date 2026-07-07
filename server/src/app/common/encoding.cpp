#include "app/common/utils.hpp"

#include <array>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace lf {
namespace {

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
  state.h = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u,
             0xC3D2E1F0u};
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

void sha1_update(sha1_state& state, const std::uint8_t* data,
                 std::size_t length) {
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
    digest[i * 4 + 1] =
        static_cast<std::uint8_t>((state.h[i] >> 16) & 0xFFu);
    digest[i * 4 + 2] =
        static_cast<std::uint8_t>((state.h[i] >> 8) & 0xFFu);
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

}  // namespace lf
