#include "patch.hpp"

#include <iomanip>
#include <sstream>

namespace rzm::patch {

Bytes invert_bool() { return {0x34, 0x01, 0x84, 0xC0, 0x0F, 0x95, 0xC0}; }

Bytes nops(std::size_t count) { return Bytes(count, 0x90); }

std::size_t Plan::changed_bytes() const {
  std::size_t total = 0;
  for (const Change& change : changes) {
    const std::size_t common = change.before.size() < change.after.size() ? change.before.size()
                                                                         : change.after.size();
    for (std::size_t i = 0; i < common; ++i) {
      if (change.before[i] != change.after[i]) {
        ++total;
      }
    }
    total += change.before.size() > change.after.size() ? change.before.size() - change.after.size()
                                                        : change.after.size() - change.before.size();
  }
  return total;
}

Plan build(const pe::File& file, const std::string& label, const std::vector<scan::Hit>& hits,
           const std::string& strategy) {
  Bytes replacement;
  if (strategy == "invert") {
    replacement = invert_bool();
  } else if (strategy == "nop") {
    replacement = nops(hits.empty() ? 0 : hits.front().length);
  } else {
    fail("unknown patch strategy: " + strategy);
  }

  Plan plan;
  plan.target = file.arch();
  for (const scan::Hit& hit : hits) {
    if (hit.offset + hit.length > file.size()) {
      continue;
    }
    Change change;
    change.va = file.image_base() + hit.rva;
    change.label = label + " at rva " + hex(hit.rva);
    change.why = "signature matched, edit shown for review only";
    change.before.assign(file.bytes().begin() + static_cast<std::ptrdiff_t>(hit.offset),
                         file.bytes().begin() + static_cast<std::ptrdiff_t>(hit.offset + hit.length));
    change.after = replacement;
    plan.changes.push_back(std::move(change));
  }
  return plan;
}

std::string render(const Plan& plan) {
  std::ostringstream out;
  out << "target    " << plan.target << '\n';
  out << "policy    dry run, nothing is written\n";
  out << "changes   " << plan.changes.size() << " sites, " << plan.changed_bytes() << " bytes\n";
  for (const Change& change : plan.changes) {
    out << '\n';
    out << "  " << change.label << '\n';
    out << "    va    " << hex(change.va) << '\n';
    out << "    why   " << change.why << '\n';
    out << "    from  " << to_hex(change.before) << '\n';
    out << "    to    " << to_hex(change.after) << '\n';
  }
  return out.str();
}

} // namespace rzm::patch
