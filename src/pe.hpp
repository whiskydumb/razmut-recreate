#pragma once

#include "common.hpp"

namespace rzm::pe {

struct Section {
  std::string name;
  std::uint32_t rva = 0;
  std::uint32_t virtual_size = 0;
  std::uint32_t raw_offset = 0;
  std::uint32_t raw_size = 0;
  std::uint32_t flags = 0;

  bool executable() const { return (flags & 0x20000000u) != 0; }
  bool writable() const { return (flags & 0x80000000u) != 0; }
  std::uint32_t span() const { return virtual_size > raw_size ? virtual_size : raw_size; }
};

struct Import {
  std::string library;
  std::vector<std::string> functions;
};

struct Export {
  std::string name;
  std::uint32_t rva = 0;
};

// read only view over a PE32/PE32+ file. enough to walk sections, imports and
// exports and to translate rva to file offset for pattern scanning.
class File {
public:
  static File open(const fs::path& path);

  explicit File(Bytes data);

  const Bytes& bytes() const { return data_; }
  std::uint64_t size() const { return static_cast<std::uint64_t>(data_.size()); }

  bool is64() const { return is64_; }
  std::string arch() const;
  std::uint64_t image_base() const { return image_base_; }
  std::uint32_t entry_rva() const { return entry_rva_; }
  const std::vector<Section>& sections() const { return sections_; }

  const Section* section_at(std::uint32_t rva) const;
  std::uint64_t rva_to_offset(std::uint32_t rva) const;
  bool executable(std::uint32_t rva) const;

  std::vector<Import> imports() const;
  std::vector<Export> exports() const;

  std::uint8_t u8(std::uint64_t offset) const;
  std::uint16_t u16(std::uint64_t offset) const;
  std::uint32_t u32(std::uint64_t offset) const;
  std::uint64_t u64(std::uint64_t offset) const;
  const char* cstr(std::uint64_t offset) const;

private:
  void parse();

  Bytes data_;
  bool is64_ = false;
  std::uint16_t machine_ = 0;
  std::uint64_t image_base_ = 0;
  std::uint32_t entry_rva_ = 0;
  std::uint32_t headers_size_ = 0;
  std::uint32_t dir_count_ = 0;
  std::uint64_t dir_offset_ = 0;
  std::vector<Section> sections_;
};

} // namespace rzm::pe
