#pragma once

#include "common.hpp"

namespace rzm::state {

struct Paths {
  fs::path root;
  fs::path config;     // state.ini
  fs::path lease;      // licensing/lease.json
  fs::path device;     // licensing/device.dat
  fs::path install_id; // licensing/installation-id
  fs::path marker;     // import-complete

  static Paths resolve();
};

struct Module {
  bool enabled = false;
  std::string icon = "regular"; // locked, regular, loud
  int ping_ms = 6000;
};

struct License {
  bool active = false;
  std::string key;
  std::string message = "not activated";
  std::string device;
  std::int64_t issued = 0;
  std::int64_t expires = 0;

  bool expired() const;
  std::int64_t hours_left() const;
};

struct App {
  Module module;
  License license;
};

App load(const Paths& paths);
void save(const Paths& paths, const App& app);

bool valid_icon(const std::string& icon);

std::string device_id(const Paths& paths);
std::string make_key(std::uint32_t seed);
bool check_key(const std::string& key, std::string& reason);
License activate(const Paths& paths, const std::string& key, int days);
void drop_license(const Paths& paths);

} // namespace rzm::state
