#pragma once

#include "screen.h"

#include <ncurses.h>

// ncurses-backed full-screen terminal implementation of Screen.
class AnsiScreen : public Screen {
public:
  AnsiScreen();
  ~AnsiScreen() override;

  void clear() override;
  void locate(int row, int col) override;
  void color(int foreground, int background) override;
  void putc(char ch) override;
  void print(const std::string& text) override;
  void refresh() override;
  Size size() const override;
  std::optional<int> get_key(bool blocking) override;

private:
  WINDOW* window_ = nullptr;
  int rows_ = 0;
  int cols_ = 0;
};
