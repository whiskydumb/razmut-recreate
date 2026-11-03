// self contained test binary, no external framework
#include <cstdio>
#include <string>

#include "common.hpp"
#include "patch.hpp"
#include "scan.hpp"
#include "state.hpp"

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(condition)                                                                          \
  do {                                                                                            \
    ++g_checks;                                                                                   \
    if (!(condition)) {                                                                           \
      ++g_failures;                                                                               \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                            \
    }                                                                                             \
  } while (false)

void test_scan() {
  const auto pattern = rzm::scan::compile("48 89 5C 24 ?? 57");
  CHECK(pattern.bytes.size() == 6);
  CHECK(pattern.compare[4] == false);
  CHECK(pattern.literals == 5);

  const rzm::Bytes data{0x00, 0x48, 0x89, 0x5C, 0x24, 0x99, 0x57, 0x00};
  const auto hits = rzm::scan::find(data, pattern);
  CHECK(hits.size() == 1);
  CHECK(hits[0].offset == 1);

  const auto empty = rzm::scan::find(data, rzm::scan::compile("90 90 90 90"));
  CHECK(empty.empty());

  bool threw = false;
  try {
    rzm::scan::compile("zz");
  } catch (const rzm::Error&) {
    threw = true;
  }
  CHECK(threw);
}

void test_catalog() {
  CHECK(!rzm::scan::catalog().empty());
  CHECK(rzm::scan::find_signature("bool_invert") != nullptr);
  CHECK(rzm::scan::find_signature("nope") == nullptr);
  for (const auto& entry : rzm::scan::catalog()) {
    const auto pattern = rzm::scan::compile(entry.pattern);
    CHECK(!pattern.bytes.empty());
  }
}

void test_patch() {
  const auto invert = rzm::patch::invert_bool();
  CHECK(invert.size() == 7);
  CHECK(invert.front() == 0x34);
  CHECK(invert.back() == 0xC0);

  const auto nops = rzm::patch::nops(4);
  CHECK(nops.size() == 4);
  CHECK(nops[0] == 0x90);

  rzm::patch::Plan plan;
  rzm::patch::Change change;
  change.before = {0x34, 0x00};
  change.after = {0x34, 0x01};
  plan.changes.push_back(change);
  CHECK(plan.changed_bytes() == 1);
}

void test_hash() {
  CHECK(rzm::crc32(reinterpret_cast<const std::uint8_t*>("123456789"), 9) == 0xCBF43926u);
  CHECK(rzm::fnv1a("") == 2166136261u);
  CHECK(rzm::fnv1a("abc") != rzm::fnv1a("abd"));
}

void test_keys() {
  bool any_valid = false;
  for (std::uint32_t seed = 1; seed <= 16; ++seed) {
    const std::string key = rzm::state::make_key(seed);
    std::string reason;
    if (rzm::state::check_key(key, reason)) {
      any_valid = true;
    }
  }
  CHECK(any_valid);

  std::string reason;
  CHECK(!rzm::state::check_key("RZM-0000-0000-0000-0000", reason));
  CHECK(!reason.empty());
}

void test_state_roundtrip() {
  rzm::state::Paths paths;
  paths.root = rzm::fs::current_path() / "test-state-tmp";
  paths.config = paths.root / "state.ini";
  paths.lease = paths.root / "licensing" / "lease.json";
  paths.device = paths.root / "licensing" / "device.dat";
  paths.install_id = paths.root / "licensing" / "installation-id";
  paths.marker = paths.root / "import-complete";

  std::error_code code;
  rzm::fs::remove_all(paths.root, code);

  const std::string key = rzm::state::make_key(42);
  const auto license = rzm::state::activate(paths, key, 7);
  CHECK(license.active);
  CHECK(rzm::file_exists(paths.lease));

  auto app = rzm::state::load(paths);
  CHECK(app.license.active);
  CHECK(app.license.hours_left() > 0);

  app.module.enabled = true;
  app.module.icon = "loud";
  app.module.ping_ms = 2500;
  rzm::state::save(paths, app);

  const auto again = rzm::state::load(paths);
  CHECK(again.module.enabled);
  CHECK(again.module.icon == "loud");
  CHECK(again.module.ping_ms == 2500);

  rzm::state::drop_license(paths);
  const auto cleared = rzm::state::load(paths);
  CHECK(!cleared.license.active);
  rzm::fs::remove_all(paths.root, code);
}

} // namespace

int main() {
  test_scan();
  test_catalog();
  test_patch();
  test_hash();
  test_keys();
  test_state_roundtrip();
  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
