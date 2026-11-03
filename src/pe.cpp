#include "pe.hpp"

#include <algorithm>
#include <cstring>

namespace rzm::pe {
namespace {

constexpr std::uint16_t kMZ = 0x5A4D;
constexpr std::uint32_t kPE = 0x00004550;
constexpr std::uint16_t kPE32 = 0x010B;
constexpr std::uint16_t kPE32Plus = 0x020B;

std::string read_name(const std::uint8_t* raw) {
  char buffer[9] = {};
  std::memcpy(buffer, raw, 8);
  return trim(buffer);
}

} // namespace

File::File(Bytes data) : data_(std::move(data)) { parse(); }

File File::open(const fs::path& path) { return File(read_file(path)); }

void File::parse() {
  if (data_.size() < 0x40) {
    fail("file is too small for a PE image");
  }
  if (u16(0) != kMZ) {
    fail("not a PE image: missing MZ header");
  }
  const std::uint32_t nt = u32(0x3C);
  if (static_cast<std::uint64_t>(nt) + 24 >= size()) {
    fail("not a PE image: bad e_lfanew");
  }
  if (u32(nt) != kPE) {
    fail("not a PE image: missing PE signature");
  }

  const std::uint64_t file_header = static_cast<std::uint64_t>(nt) + 4;
  machine_ = u16(file_header);
  const std::uint16_t section_count = u16(file_header + 2);
  const std::uint16_t optional_size = u16(file_header + 16);

  const std::uint64_t optional = file_header + 20;
  const std::uint16_t magic = u16(optional);
  if (magic == kPE32Plus) {
    is64_ = true;
  } else if (magic == kPE32) {
    is64_ = false;
  } else {
    fail("unsupported optional header magic");
  }

  entry_rva_ = u32(optional + 16);
  image_base_ = is64_ ? u64(optional + 24) : u32(optional + 28);
  headers_size_ = u32(optional + 60);
  dir_count_ = u32(optional + (is64_ ? 108 : 92));
  dir_offset_ = optional + (is64_ ? 112 : 96);

  const std::uint64_t table = optional + optional_size;
  sections_.reserve(section_count);
  for (std::uint16_t i = 0; i < section_count; ++i) {
    const std::uint64_t entry = table + static_cast<std::uint64_t>(i) * 40;
    if (entry + 40 > size()) {
      fail("section table is truncated");
    }
    Section section;
    section.name = read_name(data_.data() + entry);
    section.virtual_size = u32(entry + 8);
    section.rva = u32(entry + 12);
    section.raw_size = u32(entry + 16);
    section.raw_offset = u32(entry + 20);
    section.flags = u32(entry + 36);
    sections_.push_back(std::move(section));
  }

  std::sort(sections_.begin(), sections_.end(), [](const Section& a, const Section& b) {
    return a.rva < b.rva;
  });
}

std::string File::arch() const {
  switch (machine_) {
  case 0x8664:
    return "x86-64";
  case 0x14C:
    return "x86";
  case 0xAA64:
    return "arm64";
  case 0x1C4:
    return "arm";
  default:
    return "unknown";
  }
}

const Section* File::section_at(std::uint32_t rva) const {
  for (const Section& section : sections_) {
    if (rva >= section.rva && rva < section.rva + section.span()) {
      return &section;
    }
  }
  return nullptr;
}

std::uint64_t File::rva_to_offset(std::uint32_t rva) const {
  if (rva < headers_size_) {
    return rva < size() ? rva : UINT64_MAX;
  }
  const Section* section = section_at(rva);
  if (section == nullptr) {
    return UINT64_MAX;
  }
  const std::uint32_t delta = rva - section->rva;
  if (delta >= section->raw_size) {
    return UINT64_MAX;
  }
  const std::uint64_t offset = static_cast<std::uint64_t>(section->raw_offset) + delta;
  return offset < size() ? offset : UINT64_MAX;
}

bool File::executable(std::uint32_t rva) const {
  const Section* section = section_at(rva);
  return section != nullptr && section->executable();
}

std::uint8_t File::u8(std::uint64_t offset) const {
  if (offset >= size()) {
    fail("read past end of file");
  }
  return data_[static_cast<std::size_t>(offset)];
}

std::uint16_t File::u16(std::uint64_t offset) const {
  if (offset + 2 > size()) {
    fail("read past end of file");
  }
  std::uint16_t value = 0;
  std::memcpy(&value, data_.data() + offset, sizeof(value));
  return value;
}

std::uint32_t File::u32(std::uint64_t offset) const {
  if (offset + 4 > size()) {
    fail("read past end of file");
  }
  std::uint32_t value = 0;
  std::memcpy(&value, data_.data() + offset, sizeof(value));
  return value;
}

std::uint64_t File::u64(std::uint64_t offset) const {
  if (offset + 8 > size()) {
    fail("read past end of file");
  }
  std::uint64_t value = 0;
  std::memcpy(&value, data_.data() + offset, sizeof(value));
  return value;
}

const char* File::cstr(std::uint64_t offset) const {
  if (offset >= size()) {
    fail("string offset past end of file");
  }
  return reinterpret_cast<const char*>(data_.data() + offset);
}

std::vector<Import> File::imports() const {
  std::vector<Import> result;
  if (dir_count_ < 2) {
    return result;
  }
  const std::uint32_t rva = u32(dir_offset_ + 8);
  if (rva == 0) {
    return result;
  }
  const std::uint64_t base = rva_to_offset(rva);
  if (base == UINT64_MAX) {
    return result;
  }

  const std::uint32_t thunk_size = is64_ ? 8 : 4;
  const std::uint64_t ordinal_bit = is64_ ? 0x8000000000000000ull : 0x80000000ull;

  for (std::uint64_t index = 0; index < 1024; ++index) {
    const std::uint64_t entry = base + index * 20;
    if (entry + 20 > size()) {
      break;
    }
    const std::uint32_t original_thunk = u32(entry);
    const std::uint32_t name_rva = u32(entry + 12);
    const std::uint32_t first_thunk = u32(entry + 16);
    if (original_thunk == 0 && name_rva == 0 && first_thunk == 0) {
      break;
    }
    const std::uint64_t name_offset = rva_to_offset(name_rva);
    if (name_offset == UINT64_MAX) {
      continue;
    }
    Import imported;
    imported.library = cstr(name_offset);

    std::uint64_t thunk = rva_to_offset(original_thunk != 0 ? original_thunk : first_thunk);
    if (thunk != UINT64_MAX) {
      for (std::uint64_t slot = 0; slot < 4096; ++slot) {
        if (thunk + thunk_size > size()) {
          break;
        }
        const std::uint64_t value = is64_ ? u64(thunk) : u32(thunk);
        if (value == 0) {
          break;
        }
        if ((value & ordinal_bit) != 0) {
          imported.functions.push_back("ordinal " + std::to_string(value & 0xFFFFull));
        } else {
          const std::uint64_t named = rva_to_offset(static_cast<std::uint32_t>(value & 0x7FFFFFFFull));
          if (named != UINT64_MAX) {
            imported.functions.push_back(cstr(named + 2));
          }
        }
        thunk += thunk_size;
      }
    }
    result.push_back(std::move(imported));
  }
  return result;
}

std::vector<Export> File::exports() const {
  std::vector<Export> result;
  if (dir_count_ < 1) {
    return result;
  }
  const std::uint32_t rva = u32(dir_offset_);
  if (rva == 0) {
    return result;
  }
  const std::uint64_t base = rva_to_offset(rva);
  if (base == UINT64_MAX) {
    return result;
  }

  const std::uint32_t function_count = u32(base + 20);
  const std::uint32_t name_count = u32(base + 24);
  const std::uint64_t functions = rva_to_offset(u32(base + 28));
  const std::uint64_t names = rva_to_offset(u32(base + 32));
  const std::uint64_t ordinals = rva_to_offset(u32(base + 36));
  if (functions == UINT64_MAX || names == UINT64_MAX || ordinals == UINT64_MAX) {
    return result;
  }

  for (std::uint32_t i = 0; i < name_count && i < function_count; ++i) {
    const std::uint32_t name_rva = u32(names + static_cast<std::uint64_t>(i) * 4);
    const std::uint16_t ordinal = u16(ordinals + static_cast<std::uint64_t>(i) * 2);
    const std::uint64_t name_offset = rva_to_offset(name_rva);
    if (name_offset == UINT64_MAX) {
      continue;
    }
    Export symbol;
    symbol.name = cstr(name_offset);
    symbol.rva = u32(functions + static_cast<std::uint64_t>(ordinal) * 4);
    if (symbol.rva != 0) {
      result.push_back(std::move(symbol));
    }
  }
  return result;
}

} // namespace rzm::pe
