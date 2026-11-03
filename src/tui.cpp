#include "tui.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "common.hpp"
#include "scan.hpp"
#include "state.hpp"

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace rzm::tui {
namespace {

constexpr std::size_t kWidth = 74;
constexpr std::size_t kLogLines = 5;

struct KeyState {
  bool done = false;
};

void enable_vt() {
#ifdef _WIN32
  HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode) != 0) {
    SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  }
  SetConsoleOutputCP(CP_UTF8);
#endif
}

int read_key() {
#ifdef _WIN32
  const int first = _getch();
  if (first == 0 || first == 0xE0) {
    const int second = _getch();
    switch (second) {
    case 72:
      return 'u';
    case 80:
      return 'd';
    case 75:
      return 'l';
    case 77:
      return 'r';
    default:
      return 0;
    }
  }
  return first;
#else
  return std::getchar();
#endif
}

std::string rule(char left, char fill, char right) {
  return std::string(1, left) + std::string(kWidth - 2, fill) + std::string(1, right);
}

struct Screen {
  state::Paths paths;
  state::App app;
  std::size_t cursor = 0;
  std::deque<std::string> log;

  Screen() : paths(state::Paths::resolve()) {
    app = state::load(paths);
    log.push_back("this build never touches a game process, only files you point it at");
    log.push_back("use the cli for real work: pe, scan, plan, report");
  }

  void push(const std::string& line) {
    log.push_back(line);
    while (log.size() > kLogLines) {
      log.pop_front();
    }
  }

  void commit() {
    state::save(paths, app);
    app = state::load(paths);
  }
};

constexpr std::size_t kItems = 6;

std::string item_text(const Screen& screen, std::size_t index) {
  switch (index) {
  case 0:
    return std::string("license: ") + (screen.app.license.active ? screen.app.license.message
                                                                 : std::string("not activated"));
  case 1:
    return std::string("module: ") + (screen.app.module.enabled ? "on" : "off");
  case 2:
    return "voice icon: " + screen.app.module.icon;
  case 3:
    return "ping cooldown: " + std::to_string(screen.app.module.ping_ms) + " ms";
  case 4:
    return "signatures: show catalog in the log";
  default:
    return "quit";
  }
}

void draw(const Screen& screen) {
  std::ostringstream out;
  out << "\x1b[?25l\x1b[H\x1b[2J";
  out << rule('+', '-', '+') << '\n';
  out << "| " << pad("razmut-recreate 0.2.0", kWidth - 3) << "|\n";
  out << "| " << pad("clean room cli/tui rewrite, no tauri, no webview", kWidth - 3) << "|\n";
  out << rule('+', '-', '+') << '\n';
  for (std::size_t i = 0; i < kItems; ++i) {
    const bool selected = i == screen.cursor;
    std::string line = selected ? "> " : "  ";
    line += item_text(screen, i);
    if (selected) {
      line = std::string("\x1b[7m") + pad(line, kWidth - 3) + "\x1b[0m";
    } else {
      line = pad(line, kWidth - 3);
    }
    out << "| " << line << "|\n";
  }
  out << rule('+', '-', '+') << '\n';
  out << "| " << pad("home: " + screen.paths.root.string(), kWidth - 3) << "|\n";
  out << "| " << pad("log", kWidth - 3) << "|\n";
  for (std::size_t i = 0; i < kLogLines; ++i) {
    out << "| " << pad(i < screen.log.size() ? "  " + screen.log[i] : "", kWidth - 3) << "|\n";
  }
  out << rule('+', '-', '+') << '\n';
  out << "| " << pad("up/down move   left/right change   enter apply   q quit", kWidth - 3) << "|\n";
  out << rule('+', '-', '+') << '\n';
  std::cout << out.str();
  std::cout.flush();
}

void apply(Screen& screen) {
  switch (screen.cursor) {
  case 0:
    screen.push("activation is a cli step: activate --key <key>");
    break;
  case 1:
    if (!screen.app.license.active) {
      screen.push("no active license");
      break;
    }
    screen.app.module.enabled = !screen.app.module.enabled;
    screen.commit();
    screen.push(screen.app.module.enabled ? "module on" : "module off");
    break;
  case 2:
    screen.push("change the icon with left/right");
    break;
  case 3:
    screen.push("change the cooldown with left/right");
    break;
  case 4:
    for (const auto& entry : scan::catalog()) {
      screen.push(entry.name + " (" + entry.kind + ") " + entry.pattern);
    }
    break;
  default:
    break;
  }
}

bool adjust(Screen& screen, int delta) {
  if (screen.cursor == 1) {
    screen.app.module.enabled = delta > 0;
    screen.commit();
    return true;
  }
  if (screen.cursor == 2) {
    static const std::vector<std::string> icons{"locked", "regular", "loud"};
    const auto found = std::find(icons.begin(), icons.end(), screen.app.module.icon);
    int index = static_cast<int>(std::distance(icons.begin(), found));
    index = (index + delta + static_cast<int>(icons.size())) % static_cast<int>(icons.size());
    screen.app.module.icon = icons[static_cast<std::size_t>(index)];
    screen.commit();
    return true;
  }
  if (screen.cursor == 3) {
    const long long next = static_cast<long long>(screen.app.module.ping_ms) + delta * 500;
    screen.app.module.ping_ms = static_cast<int>(std::clamp<long long>(next, 500, 60000));
    screen.commit();
    return true;
  }
  return false;
}

} // namespace

int run() {
  enable_vt();
  Screen screen;
  std::cout << "\x1b[?1049h";
  bool running = true;
  while (running) {
    draw(screen);
    const int key = read_key();
    switch (key) {
    case 'q':
    case 27:
      running = false;
      break;
    case 'u':
    case 'k':
      screen.cursor = (screen.cursor + kItems - 1) % kItems;
      break;
    case 'd':
    case 'j':
    case '\t':
      screen.cursor = (screen.cursor + 1) % kItems;
      break;
    case 'r':
    case 'l':
      adjust(screen, +1);
      break;
    case 'h':
      adjust(screen, -1);
      break;
    case '\r':
    case '\n':
      if (screen.cursor + 1 == kItems) {
        running = false;
      } else {
        apply(screen);
      }
      break;
    default:
      break;
    }
  }
  std::cout << "\x1b[?1049l\x1b[?25h";
  return 0;
}

} // namespace rzm::tui
