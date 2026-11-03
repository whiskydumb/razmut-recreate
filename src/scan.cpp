#include "scan.hpp"

#include <cstdio>
#include <sstream>

namespace rzm::scan {
namespace {

int nibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

std::string escape(const std::string& text) {
  std::string out;
  for (const char c : text) {
    if (c == '"' || c == '\\') {
      out += '\\';
    }
    out += c;
  }
  return out;
}

} // namespace

Pattern compile(const std::string& text) {
  Pattern pattern;
  std::istringstream stream(text);
  std::string token;
  while (stream >> token) {
    if (!pattern.text.empty()) {
      pattern.text += ' ';
    }
    if (token.find('?') != std::string::npos) {
      pattern.text += "??";
      pattern.bytes.push_back(0);
      pattern.compare.push_back(false);
      continue;
    }
    if (token.size() != 2) {
      fail("bad pattern token: " + token);
    }
    const int high = nibble(token[0]);
    const int low = nibble(token[1]);
    if (high < 0 || low < 0) {
      fail("bad hex in pattern token: " + token);
    }
    pattern.text += token;
    pattern.bytes.push_back(static_cast<std::uint8_t>((high << 4) | low));
    pattern.compare.push_back(true);
    ++pattern.literals;
  }
  if (pattern.bytes.empty()) {
    fail("empty pattern");
  }
  if (pattern.literals == 0) {
    fail("pattern has no fixed bytes");
  }
  return pattern;
}

std::vector<Hit> find(const Bytes& data, const Pattern& pattern, std::uint32_t base_rva, std::size_t limit) {
  std::vector<Hit> hits;
  const std::size_t length = pattern.bytes.size();
  if (length == 0 || data.size() < length) {
    return hits;
  }

  std::size_t anchor = 0;
  while (anchor < length && !pattern.compare[anchor]) {
    ++anchor;
  }
  if (anchor == length) {
    return hits;
  }

  const std::size_t last = data.size() - length;
  for (std::size_t offset = 0; offset <= last; ++offset) {
    if (data[offset + anchor] != pattern.bytes[anchor]) {
      continue;
    }
    bool matched = true;
    for (std::size_t i = 0; i < length; ++i) {
      if (pattern.compare[i] && data[offset + i] != pattern.bytes[i]) {
        matched = false;
        break;
      }
    }
    if (!matched) {
      continue;
    }
    Hit hit;
    hit.offset = offset;
    hit.rva = base_rva + offset;
    hit.length = length;
    hit.literals = pattern.literals;
    hits.push_back(hit);
    if (hits.size() >= limit) {
      break;
    }
  }
  return hits;
}

// the catalog only contains shapes that are useful while triaging an x64 image.
// none of them encodes a game specific patch; the interesting ones (full function
// templates of a particular client build) are described in docs, not shipped.
const std::vector<Signature>& catalog() {
  static const std::vector<Signature> entries = {
      {"msvc_prologue", "prologue", "48 89 5C 24 ?? 57 48 83 EC",
       "save rbx, push rdi, allocate stack. strong hint for a function entry"},
      {"rust_prologue", "prologue", "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57",
       "three saved registers then push rdi. rustc emits this very often"},
      {"stack_alloc_small", "prologue", "48 83 EC 28",
       "sub rsp, 0x28. extremely common, use only as part of a bigger pattern"},
      {"bool_invert", "codegen", "34 01 84 C0 0F 95 C0",
       "xor al,1 / test al,al / setne al. a predicate result is negated"},
      {"member_getter", "codegen", "48 8B 41 28 C3",
       "mov rax,[rcx+0x28] / ret. one instruction getter at a fixed offset"},
      {"virtual_call", "codegen", "FF 92 ?? ?? ?? ??",
       "call qword [rdx+disp32]. indirect dispatch through a vtable slot"},
      {"rtti_class", "data", "2E 3F 41 56",
       "ascii .?AV, the msvc type descriptor prefix for classes"},
      {"rust_panic", "data", "70 61 6E 69 63 6B 65 64 20 61 74",
       "ascii 'panicked at'. confirms rust and usually sits next to source paths"},
      {"import_thunk", "data", "FF 25 ?? ?? ?? ??",
       "jmp qword [rip+disp32]. every bound import is a thunk of this shape"},
  };
  return entries;
}

const Signature* find_signature(const std::string& name) {
  for (const Signature& entry : catalog()) {
    if (entry.name == name) {
      return &entry;
    }
  }
  return nullptr;
}

std::vector<Hit> to_rvas(const pe::File& file, const std::vector<Hit>& hits) {
  std::vector<Hit> out;
  out.reserve(hits.size());
  for (const Hit& hit : hits) {
    Hit copy = hit;
    copy.rva = 0;
    for (const pe::Section& section : file.sections()) {
      if (hit.offset >= section.raw_offset &&
          hit.offset < static_cast<std::uint64_t>(section.raw_offset) + section.raw_size) {
        copy.rva = section.rva + (hit.offset - section.raw_offset);
        break;
      }
    }
    out.push_back(copy);
  }
  return out;
}

std::string as_json(const std::string& target, std::uint64_t size, const Pattern& pattern,
                    const std::vector<Hit>& hits) {
  std::ostringstream out;
  out << "{\n";
  out << "  \"target\": \"" << escape(target) << "\",\n";
  out << "  \"target_bytes\": " << size << ",\n";
  out << "  \"pattern\": \"" << escape(pattern.text) << "\",\n";
  out << "  \"hits\": " << hits.size() << ",\n";
  out << "  \"offsets\": [";
  for (std::size_t i = 0; i < hits.size(); ++i) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "\"0x%llx\"", static_cast<unsigned long long>(hits[i].offset));
    out << (i == 0 ? "" : ", ") << buffer;
  }
  out << "]\n}\n";
  return out.str();
}

} // namespace rzm::scan
