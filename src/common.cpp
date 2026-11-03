#include "common.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace rzm {
namespace {

bool g_quiet = false;
bool g_verbose = false;

bool color_enabled() {
  static const bool value = std::getenv("NO_COLOR") == nullptr;
  return value;
}

} // namespace

void fail(const std::string& message) { throw Error(message); }

Bytes read_file(const fs::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    fail("can not open " + path.string() + " for reading");
  }
  stream.seekg(0, std::ios::end);
  const auto size = stream.tellg();
  if (size < 0) {
    fail("can not get size of " + path.string());
  }
  stream.seekg(0, std::ios::beg);
  Bytes data(static_cast<std::size_t>(size));
  if (!data.empty()) {
    stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!stream) {
      fail("short read from " + path.string());
    }
  }
  return data;
}

void write_file(const fs::path& path, const Bytes& data) {
  if (path.has_parent_path()) {
    fs::create_directories(path.parent_path());
  }
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    fail("can not open " + path.string() + " for writing");
  }
  if (!data.empty()) {
    stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  }
  if (!stream) {
    fail("short write to " + path.string());
  }
}

void write_text(const fs::path& path, std::string_view text) {
  write_file(path, Bytes(text.begin(), text.end()));
}

std::string read_text(const fs::path& path) {
  const Bytes data = read_file(path);
  return std::string(data.begin(), data.end());
}

bool file_exists(const fs::path& path) {
  std::error_code code;
  return fs::exists(path, code);
}

std::string hex(std::uint64_t value) {
  char buffer[32] = {};
  std::snprintf(buffer, sizeof(buffer), "0x%llx", static_cast<unsigned long long>(value));
  return buffer;
}

std::string to_hex(const Bytes& data, std::size_t max_bytes) {
  const std::size_t count = max_bytes == 0 ? data.size() : std::min(max_bytes, data.size());
  std::string out;
  out.reserve(count * 3);
  for (std::size_t i = 0; i < count; ++i) {
    char buffer[4] = {};
    std::snprintf(buffer, sizeof(buffer), i + 1 == count ? "%02x" : "%02x ", static_cast<unsigned>(data[i]));
    out += buffer;
  }
  if (count != data.size()) {
    out += " ...";
  }
  return out;
}

std::string trim(std::string_view text) {
  std::size_t first = 0;
  while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first])) != 0) {
    ++first;
  }
  std::size_t last = text.size();
  while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1])) != 0) {
    --last;
  }
  return std::string(text.substr(first, last - first));
}

std::string lower(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

std::size_t display_width(const std::string& text) {
  std::size_t width = 0;
  for (const unsigned char c : text) {
    if ((c & 0xC0u) != 0x80u) {
      ++width;
    }
  }
  return width;
}

std::string pad(const std::string& text, std::size_t width) {
  const std::size_t current = display_width(text);
  if (current >= width) {
    return text;
  }
  return text + std::string(width - current, ' ');
}

void set_quiet(bool quiet) { g_quiet = quiet; }
void set_verbose(bool verbose) { g_verbose = verbose; }

void say(const std::string& message) {
  if (g_quiet) {
    return;
  }
  std::cout << message << '\n';
}

void note(const std::string& message) {
  if (!g_verbose || g_quiet) {
    return;
  }
  std::cout << "[debug] " << message << '\n';
}

void warn(const std::string& message) {
  if (color_enabled()) {
    std::cerr << "\x1b[33m" << message << "\x1b[0m\n";
  } else {
    std::cerr << message << '\n';
  }
}

std::uint32_t fnv1a(std::string_view text) {
  std::uint32_t hash = 2166136261u;
  for (const char c : text) {
    hash ^= static_cast<std::uint8_t>(c);
    hash *= 16777619u;
  }
  return hash;
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
  static std::uint32_t table[256] = {};
  static bool ready = false;
  if (!ready) {
    for (std::uint32_t i = 0; i < 256; ++i) {
      std::uint32_t value = i;
      for (int bit = 0; bit < 8; ++bit) {
        value = (value & 1u) != 0u ? (value >> 1u) ^ 0xEDB88320u : value >> 1u;
      }
      table[i] = value;
    }
    ready = true;
  }
  std::uint32_t crc = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < size; ++i) {
    crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8u);
  }
  return crc ^ 0xFFFFFFFFu;
}

} // namespace rzm
