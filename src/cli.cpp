#include "cli.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "common.hpp"
#include "patch.hpp"
#include "pe.hpp"
#include "scan.hpp"
#include "state.hpp"
#include "tui.hpp"

namespace rzm::cli {
namespace {

struct Args {
  std::vector<std::string> positional;
  std::map<std::string, std::string> values;
  std::vector<std::string> flags;

  bool has(const std::string& flag) const {
    return std::find(flags.begin(), flags.end(), flag) != flags.end();
  }
  std::string get(const std::string& key, const std::string& fallback = {}) const {
    const auto found = values.find(key);
    return found == values.end() ? fallback : found->second;
  }
};

Args parse(int argc, char** argv, int from) {
  Args args;
  for (int i = from; i < argc; ++i) {
    const std::string token = argv[i];
    if (token.rfind("--", 0) != 0) {
      args.positional.push_back(token);
      continue;
    }
    const bool next_is_value = i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0;
    if (next_is_value) {
      args.values[token] = argv[++i];
    } else {
      args.flags.push_back(token);
    }
  }
  return args;
}

void table(const std::vector<std::vector<std::string>>& rows) {
  if (rows.empty()) {
    return;
  }
  std::vector<std::size_t> widths;
  for (const auto& row : rows) {
    widths.resize(std::max(widths.size(), row.size()), 0);
    for (std::size_t i = 0; i < row.size(); ++i) {
      widths[i] = std::max(widths[i], display_width(row[i]));
    }
  }
  for (const auto& row : rows) {
    std::string line;
    for (std::size_t i = 0; i < row.size(); ++i) {
      line += pad(row[i], widths[i]);
      if (i + 1 != row.size()) {
        line += "  ";
      }
    }
    std::cout << line << '\n';
  }
}

std::string human_size(std::uint64_t value) {
  const char* units[] = {"B", "KiB", "MiB", "GiB"};
  double size = static_cast<double>(value);
  int unit = 0;
  while (size >= 1024.0 && unit < 3) {
    size /= 1024.0;
    ++unit;
  }
  char buffer[32] = {};
  std::snprintf(buffer, sizeof(buffer), unit == 0 ? "%.0f %s" : "%.1f %s", size, units[unit]);
  return buffer;
}

std::string escape_json(const std::string& text) {
  std::string out;
  for (const char c : text) {
    if (c == '\\' || c == '"') {
      out += '\\';
    }
    out += c;
  }
  return out;
}

std::string state_root() { return state::Paths::resolve().root.string(); }

int cmd_status(const Args& args) {
  const state::Paths paths = state::Paths::resolve();
  const state::App app = state::load(paths);
  if (args.has("--json")) {
    std::cout << "{\n"
              << "  \"active\": " << (app.license.active ? "true" : "false") << ",\n"
              << "  \"message\": \"" << escape_json(app.license.message) << "\",\n"
              << "  \"device\": \"" << escape_json(app.license.device) << "\",\n"
              << "  \"hours_left\": " << app.license.hours_left() << ",\n"
              << "  \"module\": " << (app.module.enabled ? "true" : "false") << ",\n"
              << "  \"icon\": \"" << app.module.icon << "\",\n"
              << "  \"ping_ms\": " << app.module.ping_ms << ",\n"
              << "  \"home\": \"" << escape_json(paths.root.string()) << "\"\n}\n";
    return 0;
  }
  table({
      {"license", app.license.active ? app.license.message : "not activated"},
      {"device", app.license.device.empty() ? "-" : app.license.device},
      {"hours left", std::to_string(app.license.hours_left())},
      {"module", app.module.enabled ? "on" : "off"},
      {"voice icon", app.module.icon},
      {"ping cooldown", std::to_string(app.module.ping_ms) + " ms"},
      {"home", state_root()},
  });
  return 0;
}

int cmd_keygen(const Args& args) {
  const std::string seed = args.get("--seed", "1");
  std::uint32_t value = 1;
  try {
    value = static_cast<std::uint32_t>(std::stoul(seed));
  } catch (...) {
    warn("bad --seed value, using 1");
  }
  say(state::make_key(value));
  return 0;
}

int cmd_activate(const Args& args) {
  const std::string key = args.get("--key");
  if (key.empty()) {
    warn("usage: activate --key RZM-XXXX-XXXX-XXXX-XXXX [--days N]");
    return 2;
  }
  int days = 30;
  try {
    days = std::stoi(args.get("--days", "30"));
  } catch (...) {
    warn("bad --days value, using 30");
  }
  const state::License license = state::activate(state::Paths::resolve(), key, days);
  if (!license.active) {
    warn(license.message);
    return 1;
  }
  say("ok, lease written for " + std::to_string(days) + " days");
  say("device " + license.device);
  return 0;
}

int cmd_reset() {
  state::drop_license(state::Paths::resolve());
  say("license removed, module disabled");
  return 0;
}

int cmd_module(const Args& args) {
  if (args.positional.empty() || (args.positional.front() != "on" && args.positional.front() != "off")) {
    warn("usage: module on|off");
    return 2;
  }
  const bool enable = args.positional.front() == "on";
  const state::Paths paths = state::Paths::resolve();
  state::App app = state::load(paths);
  if (enable && !app.license.active) {
    warn("no active license: " + app.license.message);
    return 1;
  }
  app.module.enabled = enable;
  state::save(paths, app);
  say(enable ? "module on" : "module off");
  return 0;
}

int cmd_icon(const Args& args) {
  if (args.positional.empty()) {
    warn("usage: icon locked|regular|loud");
    return 2;
  }
  const std::string icon = args.positional.front();
  if (!state::valid_icon(icon)) {
    warn("unknown icon: " + icon);
    return 2;
  }
  state::App app = state::load(state::Paths::resolve());
  app.module.icon = icon;
  state::save(state::Paths::resolve(), app);
  say("voice icon set to " + icon);
  return 0;
}

int cmd_ping(const Args& args) {
  if (args.positional.empty()) {
    warn("usage: ping <milliseconds>");
    return 2;
  }
  int value = 0;
  try {
    value = std::stoi(args.positional.front());
  } catch (...) {
    warn("ping value must be a number");
    return 2;
  }
  if (value < 500 || value > 60000) {
    warn("ping cooldown must be between 500 and 60000 ms");
    return 2;
  }
  state::App app = state::load(state::Paths::resolve());
  app.module.ping_ms = value;
  state::save(state::Paths::resolve(), app);
  say("ping cooldown set to " + std::to_string(value) + " ms");
  return 0;
}

int cmd_sigs(const Args& args) {
  const auto& entries = scan::catalog();
  if (args.has("--json")) {
    std::cout << "[\n";
    for (std::size_t i = 0; i < entries.size(); ++i) {
      std::cout << "  {\"name\": \"" << entries[i].name << "\", \"kind\": \"" << entries[i].kind
                << "\", \"pattern\": \"" << entries[i].pattern << "\"}"
                << (i + 1 == entries.size() ? "\n" : ",\n");
    }
    std::cout << "]\n";
    return 0;
  }
  table({{"name", "kind", "pattern"}});
  for (const auto& entry : entries) {
    table({{entry.name, entry.kind, entry.pattern}});
  }
  std::cout << '\n' << entries.size() << " signatures, see docs/03-signatures.md\n";
  return 0;
}

pe::File open_target(const Args& args) {
  if (args.positional.empty()) {
    fail("no target given");
  }
  return pe::File::open(args.positional.front());
}

int cmd_pe(const Args& args) {
  const pe::File file = open_target(args);
  const auto imports = file.imports();
  const auto exports = file.exports();

  if (args.has("--json")) {
    std::cout << "{\n"
              << "  \"arch\": \"" << file.arch() << "\",\n"
              << "  \"image_base\": \"" << hex(file.image_base()) << "\",\n"
              << "  \"entry\": \"" << hex(file.entry_rva()) << "\",\n"
              << "  \"sections\": " << file.sections().size() << ",\n"
              << "  \"import_libraries\": " << imports.size() << ",\n"
              << "  \"exports\": " << exports.size() << "\n}\n";
    return 0;
  }

  say("target    " + args.positional.front());
  table({
      {"format", file.is64() ? "PE32+" : "PE32"},
      {"machine", file.arch()},
      {"size", human_size(file.size())},
      {"image base", hex(file.image_base())},
      {"entry rva", hex(file.entry_rva())},
      {"sections", std::to_string(file.sections().size())},
      {"import libraries", std::to_string(imports.size())},
      {"exports", std::to_string(exports.size())},
  });

  std::cout << "\nsections\n";
  table({{"name", "rva", "virtual", "raw", "flags"}});
  for (const auto& section : file.sections()) {
    std::string flags;
    if (section.executable()) {
      flags += "X";
    }
    if (section.writable()) {
      flags += "W";
    }
    table({{section.name, hex(section.rva), std::to_string(section.virtual_size),
            std::to_string(section.raw_size), flags}});
  }

  if (!imports.empty()) {
    std::cout << "\nimports\n";
    for (std::size_t i = 0; i < imports.size() && i < 12; ++i) {
      std::cout << "  " << imports[i].library << " (" << imports[i].functions.size() << ")\n";
    }
  }
  if (!exports.empty()) {
    std::cout << "\nexports\n";
    for (const auto& symbol : exports) {
      std::cout << "  " << hex(symbol.rva) << "  " << symbol.name << '\n';
    }
  }
  return 0;
}

scan::Pattern pick_pattern(const Args& args) {
  const std::string name = args.get("--sig");
  if (!name.empty()) {
    const scan::Signature* entry = scan::find_signature(name);
    if (entry == nullptr) {
      fail("unknown signature: " + name);
    }
    return scan::compile(entry->pattern);
  }
  const std::string raw = args.get("--pattern");
  if (!raw.empty()) {
    return scan::compile(raw);
  }
  fail("pass --sig <name> or --pattern \"48 89 ??\"");
}

int cmd_scan(const Args& args) {
  const pe::File file = open_target(args);
  const scan::Pattern pattern = pick_pattern(args);
  const auto hits = scan::find(file.bytes(), pattern);

  if (args.has("--json")) {
    std::cout << scan::as_json(args.positional.front(), file.size(), pattern, hits);
    return hits.empty() ? 1 : 0;
  }
  say("pattern   " + pattern.text);
  say("hits      " + std::to_string(hits.size()));
  for (const auto& hit : hits) {
    std::cout << "  offset " << hex(hit.offset) << "  length " << hit.length << '\n';
  }
  return hits.empty() ? 1 : 0;
}

int cmd_plan(const Args& args) {
  const pe::File file = open_target(args);
  const scan::Pattern pattern = pick_pattern(args);
  const auto hits = scan::find(file.bytes(), pattern);
  const std::string strategy = args.get("--strategy", "invert");
  const std::string label = args.get("--sig", "inline-pattern");
  const patch::Plan plan = patch::build(file, label, hits, strategy);
  std::cout << patch::render(plan);
  return plan.changes.empty() ? 1 : 0;
}

int cmd_report(const Args& args) {
  const pe::File file = open_target(args);
  const std::string out = args.get("--out", "report.md");
  const auto imports = file.imports();
  const auto exports = file.exports();

  std::ostringstream text;
  text << "# artifact report\n\n";
  text << "- file: `" << args.positional.front() << "`\n";
  text << "- size: " << file.size() << " bytes (" << human_size(file.size()) << ")\n";
  text << "- format: " << (file.is64() ? "PE32+" : "PE32") << ", " << file.arch() << "\n";
  text << "- image base: " << hex(file.image_base()) << "\n";
  text << "- entry rva: " << hex(file.entry_rva()) << "\n";
  text << "- crc32: " << hex(crc32(file.bytes().data(), file.bytes().size())) << "\n\n";
  text << "## sections\n\n| name | rva | virtual | raw | x |\n| --- | --- | --- | --- | --- |\n";
  for (const auto& section : file.sections()) {
    text << "| " << section.name << " | " << hex(section.rva) << " | " << section.virtual_size << " | "
         << section.raw_size << " | " << (section.executable() ? "yes" : "") << " |\n";
  }
  text << "\n## imports\n\n";
  for (const auto& imported : imports) {
    text << "- " << imported.library << ": " << imported.functions.size() << " functions\n";
  }
  text << "\n## exports\n\n";
  for (const auto& symbol : exports) {
    text << "- `" << symbol.name << "` at " << hex(symbol.rva) << "\n";
  }
  write_text(out, text.str());
  say("wrote " + out);
  return 0;
}

void help() {
  std::cout << "razmut-recreate 0.2.0 - inspect a Razmut build and mirror its module state\n\n";
  const std::vector<std::vector<std::string>> rows = {
      {"status [--json]", "license and module state"},
      {"keygen [--seed N]", "print a valid key for testing"},
      {"activate --key K [--days N]", "write an offline lease"},
      {"reset", "drop the lease and disable the module"},
      {"module on|off", "toggle the module flag"},
      {"icon locked|regular|loud", "voice icon mode"},
      {"ping <ms>", "ping cooldown value"},
      {"sigs [--json]", "list built in signatures"},
      {"pe <file> [--json]", "sections, imports, exports"},
      {"scan <file> --sig NAME [--json]", "search a signature, exit 1 when nothing matched"},
      {"plan <file> --sig NAME [--strategy invert|nop]", "dry run byte plan, writes nothing"},
      {"report <file> [--out path]", "markdown report"},
      {"ui", "full screen interface"},
      {"help, version", "this text, build info"},
  };
  for (const auto& row : rows) {
    std::cout << "  " << pad(row[0], 46) << row[1] << '\n';
  }
  std::cout << "\nflags: --json, --quiet, --verbose; env: RZM_HOME, NO_COLOR\n";
}

bool stdin_is_console() {
#ifdef _WIN32
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(fileno(stdin)) != 0;
#endif
}

} // namespace

int run(int argc, char** argv) {
  if (argc < 2) {
    if (!stdin_is_console()) {
      help();
      return 0;
    }
    return tui::run();
  }

  const std::string command = argv[1];
  const Args args = parse(argc, argv, 2);

  try {
    if (command == "help" || command == "--help" || command == "-h") {
      help();
      return 0;
    }
    if (command == "version" || command == "--version") {
      std::cout << "razmut-recreate 0.2.0 (" << __VERSION__ << ", built " << __DATE__ << ")\n";
      return 0;
    }
    if (args.has("--quiet")) {
      set_quiet(true);
    }
    if (args.has("--verbose")) {
      set_verbose(true);
    }

    if (command == "status") {
      return cmd_status(args);
    }
    if (command == "keygen") {
      return cmd_keygen(args);
    }
    if (command == "activate") {
      return cmd_activate(args);
    }
    if (command == "reset") {
      return cmd_reset();
    }
    if (command == "module") {
      return cmd_module(args);
    }
    if (command == "icon") {
      return cmd_icon(args);
    }
    if (command == "ping") {
      return cmd_ping(args);
    }
    if (command == "sigs") {
      return cmd_sigs(args);
    }
    if (command == "pe") {
      return cmd_pe(args);
    }
    if (command == "scan") {
      return cmd_scan(args);
    }
    if (command == "plan") {
      return cmd_plan(args);
    }
    if (command == "report") {
      return cmd_report(args);
    }
    if (command == "ui") {
      return tui::run();
    }
    warn("unknown command: " + command + " (see help)");
    return 2;
  } catch (const std::exception& error) {
    warn(error.what());
    return 1;
  }
}

} // namespace rzm::cli
