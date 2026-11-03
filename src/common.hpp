#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rzm {

namespace fs = std::filesystem;

using Bytes = std::vector<std::uint8_t>;

class Error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

[[noreturn]] void fail(const std::string& message);

// files
Bytes read_file(const fs::path& path);
void write_file(const fs::path& path, const Bytes& data);
void write_text(const fs::path& path, std::string_view text);
std::string read_text(const fs::path& path);
bool file_exists(const fs::path& path);

// formatting
std::string hex(std::uint64_t value);
std::string to_hex(const Bytes& data, std::size_t max_bytes = 0);
std::string trim(std::string_view text);
std::string lower(std::string_view text);
std::size_t display_width(const std::string& text);
std::string pad(const std::string& text, std::size_t width);

// logging
void set_quiet(bool quiet);
void set_verbose(bool verbose);
void say(const std::string& message);   // normal output, always shown
void note(const std::string& message);  // debug only, hidden unless --verbose
void warn(const std::string& message);  // stderr

// hashing
std::uint32_t fnv1a(std::string_view text);
std::uint32_t crc32(const std::uint8_t* data, std::size_t size);

} // namespace rzm
