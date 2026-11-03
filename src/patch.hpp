#pragma once

#include "common.hpp"
#include "pe.hpp"
#include "scan.hpp"

namespace rzm::patch {

// one planned byte edit. the "after" bytes are computed, never written: this tool
// does not modify binaries, it only shows what an edit would look like.
struct Change {
  std::uint64_t va = 0;
  std::string label;
  std::string why;
  Bytes before;
  Bytes after;
};

struct Plan {
  std::string target;
  std::vector<Change> changes;
  std::size_t changed_bytes() const;
};

// encodings used by the planner
Bytes invert_bool();          // xor al,1 / test al,al / setne al
Bytes nops(std::size_t count);

// strategy is "invert" or "nop"; unknown strategies throw
Plan build(const pe::File& file, const std::string& label, const std::vector<scan::Hit>& hits,
           const std::string& strategy);

std::string render(const Plan& plan);

} // namespace rzm::patch
