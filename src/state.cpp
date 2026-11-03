#include "state.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>
#include <random>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace rzm::state {
namespace {

using Ini = std::map<std::string, std::map<std::string, std::string>>;

constexpr std::array<char, 32> kAlphabet{
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
    'G', 'H', 'J', 'K', 'M', 'N', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y'};

std::int64_t now() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

Ini parse_ini(const std::string& text) {
  Ini ini;
  std::string section = "root";
  std::istringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    const std::string clean = trim(line);
    if (clean.empty() || clean[0] == '#') {
      continue;
    }
    if (clean.front() == '[' && clean.back() == ']') {
      section = clean.substr(1, clean.size() - 2);
      continue;
    }
    const std::size_t equals = clean.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    ini[section][trim(clean.substr(0, equals))] = trim(clean.substr(equals + 1));
  }
  return ini;
}

std::string get(const Ini& ini, const std::string& section, const std::string& key,
                const std::string& fallback = {}) {
  const auto outer = ini.find(section);
  if (outer == ini.end()) {
    return fallback;
  }
  const auto inner = outer->second.find(key);
  return inner == outer->second.end() ? fallback : inner->second;
}

bool get_bool(const Ini& ini, const std::string& section, const std::string& key, bool fallback) {
  const std::string value = get(ini, section, key);
  if (value.empty()) {
    return fallback;
  }
  return value == "true" || value == "1";
}

std::string compact_key(const std::string& key) {
  std::string out;
  for (const char c : key) {
    if (c == '-' || c == ' ') {
      continue;
    }
    out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  if (out.rfind("RZM", 0) == 0) {
    out = out.substr(3);
  }
  return out;
}

std::uint8_t crc8(const std::string& text) {
  std::uint8_t crc = 0xFFu;
  for (const char c : text) {
    crc ^= static_cast<std::uint8_t>(c);
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80u) != 0u ? static_cast<std::uint8_t>((crc << 1u) ^ 0x1Du)
                                : static_cast<std::uint8_t>(crc << 1u);
    }
  }
  return crc;
}

std::string encode(std::uint32_t value, std::size_t width) {
  std::string out(width, '0');
  for (std::size_t i = 0; i < width; ++i) {
    out[width - 1 - i] = kAlphabet[value & 31u];
    value >>= 5u;
  }
  return out;
}

std::string json_field(const std::string& json, const std::string& name) {
  const std::string needle = "\"" + name + "\":";
  const std::size_t found = json.find(needle);
  if (found == std::string::npos) {
    return {};
  }
  std::size_t cursor = found + needle.size();
  while (cursor < json.size() && (json[cursor] == ' ' || json[cursor] == '\t')) {
    ++cursor;
  }
  if (cursor < json.size() && json[cursor] == '"') {
    const std::size_t end = json.find('"', cursor + 1);
    return json.substr(cursor + 1, end - cursor - 1);
  }
  const std::size_t end = json.find_first_of(",\n}", cursor);
  return trim(json.substr(cursor, end - cursor));
}

std::string lease_text(const App& app) {
  std::ostringstream out;
  out << "{\n";
  out << "  \"iss\": \"razmut-recreate\",\n";
  out << "  \"aud\": \"me.razmut.desktop\",\n";
  out << "  \"sub\": \"razmut.preview\",\n";
  out << "  \"deviceId\": \"" << app.license.device << "\",\n";
  out << "  \"iat\": " << app.license.issued << ",\n";
  out << "  \"exp\": " << app.license.expires << ",\n";
  out << "  \"active\": " << (app.license.active ? "true" : "false") << ",\n";
  out << "  \"key\": \"" << app.license.key << "\"\n";
  out << "}\n";
  return out.str();
}

std::string device_blob(const std::string& id) {
  std::string out;
  for (std::size_t i = 0; i < id.size(); ++i) {
    const auto key = static_cast<std::uint8_t>(0x5B + i * 31u);
    char buffer[4] = {};
    std::snprintf(buffer, sizeof(buffer), "%02x", static_cast<unsigned>(static_cast<std::uint8_t>(id[i]) ^ key));
    out += buffer;
  }
  return out;
}

std::string random_uuid() {
  std::random_device device;
  std::mt19937_64 engine(device());
  const auto high = engine();
  const auto low = engine();
  char buffer[40] = {};
  std::snprintf(buffer, sizeof(buffer), "%08llx-%04llx-%04llx-%04llx-%012llx",
                static_cast<unsigned long long>(high >> 32u) & 0xFFFFFFFFull,
                static_cast<unsigned long long>(high >> 16u) & 0xFFFFull,
                static_cast<unsigned long long>(high) & 0xFFFFull,
                static_cast<unsigned long long>(low >> 48u) & 0xFFFFull,
                static_cast<unsigned long long>(low) & 0xFFFFFFFFFFFFull);
  return buffer;
}

std::string host_name() {
  char buffer[256] = {};
#ifdef _WIN32
  DWORD size = sizeof(buffer);
  if (GetComputerNameA(buffer, &size) == 0) {
    buffer[0] = '\0';
  }
#else
  if (gethostname(buffer, sizeof(buffer)) != 0) {
    buffer[0] = '\0';
  }
#endif
  return buffer;
}

} // namespace

Paths Paths::resolve() {
  Paths paths;
  const char* home = std::getenv("RZM_HOME");
  const char* local = std::getenv("LOCALAPPDATA");
  if (home != nullptr && home[0] != '\0') {
    paths.root = fs::path(home);
  } else if (local != nullptr) {
    paths.root = fs::path(local) / "razmut-recreate";
  } else {
    paths.root = fs::current_path() / ".razmut-recreate";
  }
  paths.config = paths.root / "state.ini";
  paths.lease = paths.root / "licensing" / "lease.json";
  paths.device = paths.root / "licensing" / "device.dat";
  paths.install_id = paths.root / "licensing" / "installation-id";
  paths.marker = paths.root / "import-complete";
  return paths;
}

bool valid_icon(const std::string& icon) {
  return icon == "locked" || icon == "regular" || icon == "loud";
}

bool License::expired() const { return active && expires != 0 && now() > expires; }

std::int64_t License::hours_left() const {
  if (!active || expires == 0) {
    return 0;
  }
  const std::int64_t left = expires - now();
  return left > 0 ? left / 3600 : 0;
}

App load(const Paths& paths) {
  App app;
  if (!file_exists(paths.config)) {
    return app;
  }
  const Ini ini = parse_ini(read_text(paths.config));

  app.module.enabled = get_bool(ini, "module", "enabled", false);
  app.module.icon = get(ini, "module", "icon", "regular");
  if (!valid_icon(app.module.icon)) {
    app.module.icon = "regular";
  }
  try {
    app.module.ping_ms = std::stoi(get(ini, "module", "ping_ms", "6000"));
  } catch (...) {
    app.module.ping_ms = 6000;
  }

  app.license.active = get_bool(ini, "license", "active", false);
  app.license.key = get(ini, "license", "key");
  app.license.message = get(ini, "license", "message", "not activated");
  app.license.device = get(ini, "license", "device");
  if (file_exists(paths.lease)) {
    const std::string text = read_text(paths.lease);
    try {
      app.license.issued = std::stoll(json_field(text, "iat"));
      app.license.expires = std::stoll(json_field(text, "exp"));
    } catch (...) {
      app.license.issued = 0;
      app.license.expires = 0;
    }
    const std::string device = json_field(text, "deviceId");
    if (!device.empty()) {
      app.license.device = device;
    }
  }
  if (app.license.active) {
    app.license.message = app.license.expired() ? "expired" : "active";
  }
  return app;
}

void save(const Paths& paths, const App& app) {
  std::ostringstream text;
  text << "[app]\n";
  text << "version = 0.2.0\n\n";
  text << "[module]\n";
  text << "enabled = " << (app.module.enabled ? "true" : "false") << '\n';
  text << "icon = " << app.module.icon << '\n';
  text << "ping_ms = " << app.module.ping_ms << "\n\n";
  text << "[license]\n";
  text << "active = " << (app.license.active ? "true" : "false") << '\n';
  text << "key = " << app.license.key << '\n';
  text << "message = " << app.license.message << '\n';
  text << "device = " << app.license.device << '\n';
  write_text(paths.config, text.str());

  if (app.license.active) {
    write_text(paths.lease, lease_text(app));
    write_text(paths.device, device_blob(app.license.device));
  }
}

std::string device_id(const Paths& paths) {
  std::string id;
  if (file_exists(paths.install_id)) {
    id = trim(read_text(paths.install_id));
  } else {
    id = random_uuid();
    write_text(paths.install_id, id + "\n");
    write_text(paths.marker, "1");
  }
  const std::uint32_t digest = fnv1a(id + "|" + host_name());
  char buffer[16] = {};
  std::snprintf(buffer, sizeof(buffer), "%08x", digest);
  return buffer;
}

std::string make_key(std::uint32_t seed) {
  std::string body;
  std::uint32_t value = seed;
  for (int group = 0; group < 3; ++group) {
    value = value * 1664525u + 1013904223u;
    body += encode(value & 0xFFFFFu, 4);
  }
  const std::uint32_t check = crc8(body);
  const std::string full = body + encode(check & 0xFFFFu, 4);

  std::string out = "RZM";
  for (int group = 0; group < 4; ++group) {
    out += '-';
    out += full.substr(static_cast<std::size_t>(group) * 4, 4);
  }
  return out;
}

bool check_key(const std::string& key, std::string& reason) {
  const std::string body = compact_key(key);
  if (body.size() != 16) {
    reason = "key must be 16 characters in 4 groups (RZM-XXXX-XXXX-XXXX-XXXX)";
    return false;
  }
  for (const char c : body) {
    if (std::find(kAlphabet.begin(), kAlphabet.end(), c) == kAlphabet.end()) {
      reason = std::string("unexpected character in key: ") + c;
      return false;
    }
  }
  std::uint32_t actual = 0;
  for (const char c : body.substr(12)) {
    actual = actual * 32u;
    for (std::size_t i = 0; i < kAlphabet.size(); ++i) {
      if (kAlphabet[i] == c) {
        actual += static_cast<std::uint32_t>(i);
        break;
      }
    }
  }
  if (crc8(body.substr(0, 12)) != (actual & 0xFFu)) {
    reason = "key checksum does not match";
    return false;
  }
  reason.clear();
  return true;
}

License activate(const Paths& paths, const std::string& key, int days) {
  License license;
  std::string reason;
  if (!check_key(key, reason)) {
    license.message = "error: " + reason;
    return license;
  }
  license.active = true;
  license.key = key;
  license.device = device_id(paths);
  license.issued = now();
  license.expires = license.issued + static_cast<std::int64_t>(days) * 86400;
  license.message = "active (offline lease)";

  App app = load(paths);
  app.license = license;
  save(paths, app);
  return license;
}

void drop_license(const Paths& paths) {
  std::error_code code;
  fs::remove(paths.lease, code);
  fs::remove(paths.device, code);
  App app = load(paths);
  app.license = License{};
  app.module.enabled = false;
  save(paths, app);
}

} // namespace rzm::state
