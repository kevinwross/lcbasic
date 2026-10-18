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

int AnsiScreen::row() const {
  return getcury(window_) + 1;
}

std::optional<int> AnsiScreen::get_key(bool blocking) {
  // Use a short timeout (rather than a hard nodelay) when non-blocking so
  // ncurses has time to assemble multi-byte escape sequences (arrow keys).
  wtimeout(window_, blocking ? -1 : 30);
  int ch = wgetch(window_);
  if (ch == ERR) return std::nullopt;
  // Present keys the way GW-BASIC programs expect from INKEY$/ASC:
  // Enter as CR (13), arrow keys as the sample's CP437 codes,
  // and backspace as 8.
  switch (ch) {
    case KEY_UP: return 130;
    case KEY_DOWN: return 136;
    case KEY_LEFT: return 139;
    case KEY_RIGHT: return 140;
    case '\n': return 13;
    case KEY_BACKSPACE:
    case 127: return 8;
    case 27: {
      // Fallback for terminals that send CSI/SS3 sequences ncurses did not
      // translate (ESC [ A or ESC O A). Only consume if the sequence follows
      // promptly, so a lone ESC still cancels.
      wtimeout(window_, 30);
      int c1 = wgetch(window_);
      if (c1 == '[' || c1 == 'O') {
        int c2 = wgetch(window_);
        if (c2 == 'A') return 130;
        if (c2 == 'B') return 136;
        if (c2 == 'C') return 139;
        if (c2 == 'D') return 140;
      }
      return 27;
    }
    default: return ch;
  }
}
