#pragma once

#include "common.hpp"
#include "pe.hpp"

namespace rzm::scan {

// a signature is a short catalog entry; the pattern is the compiled form
struct Signature {
  std::string name;
  std::string kind;     // prologue, codegen, data
  std::string pattern;  // hex bytes, ?? is a wildcard
  std::string note;     // what a hit usually means
};

struct Pattern {
  std::string text;
  std::vector<std::uint8_t> bytes;
  std::vector<bool> compare; // false = wildcard
  std::size_t literals = 0;
};

struct Hit {
  std::uint64_t rva = 0;
  std::uint64_t offset = 0;
  std::size_t length = 0;
  std::size_t literals = 0;
};

Pattern compile(const std::string& text);
std::vector<Hit> find(const Bytes& data, const Pattern& pattern, std::uint32_t base_rva = 0,
                      std::size_t limit = 256);

const std::vector<Signature>& catalog();
const Signature* find_signature(const std::string& name);

// converts raw file offsets of hits into rvas using the section table
std::vector<Hit> to_rvas(const pe::File& file, const std::vector<Hit>& hits);

std::string as_json(const std::string& target, std::uint64_t size, const Pattern& pattern,
                    const std::vector<Hit>& hits);

} // namespace rzm::scan
