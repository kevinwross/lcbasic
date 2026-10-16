#include "ansi_screen.h"

#include <ncurses.h>
#include <stdexcept>

AnsiScreen::AnsiScreen() {
  window_ = initscr();
  if (!window_) throw std::runtime_error("failed to initialise ncurses");
  cbreak();
  noecho();
  keypad(window_, TRUE);
  nodelay(window_, TRUE);
  curs_set(0);
  getmaxyx(window_, rows_, cols_);
  start_color();
  use_default_colors();
}

AnsiScreen::~AnsiScreen() {
  endwin();
}

void AnsiScreen::clear() { werase(window_); }

void AnsiScreen::locate(int row, int col) {
  wmove(window_, row - 1, col - 1);
}

void AnsiScreen::color(int foreground, int background) {
  // GW-BASIC colour indices 0-15 mapped to ncurses pairs. Lazily init pair 1.
  static bool initialised = false;
  if (!initialised) {
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    initialised = true;
  }
  attron(COLOR_PAIR(1));
}

void AnsiScreen::putc(char ch) { waddch(window_, ch); }

void AnsiScreen::print(const std::string& text) {
  waddnstr(window_, text.c_str(), static_cast<int>(text.size()));
}

void AnsiScreen::refresh() { wrefresh(window_); }

AnsiScreen::Size AnsiScreen::size() const {
  return Size{rows_, cols_};
}

int AnsiScreen::column() const {
  return getcurx(window_) + 1;
}

std::optional<int> AnsiScreen::get_key(bool blocking) {
  nodelay(window_, !blocking);
  int ch = wgetch(window_);
  if (ch == ERR) return std::nullopt;
  return ch;
}
