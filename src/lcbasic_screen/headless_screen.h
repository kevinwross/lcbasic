#pragma once

#include "screen.h"

#include <optional>
#include <string>
#include <vector>

// Deterministic in-memory grid capture for tests. Records everything written
// to the screen so output can be asserted without a terminal.
class HeadlessScreen : public Screen {
public:
  struct Cell {
    char ch = ' ';
    int fg = 7;
    int bg = 0;
  };

  HeadlessScreen(int rows = 25, int cols = 80);

  void clear() override;
  void locate(int row, int col) override;
  void color(int foreground, int background) override;
  void putc(char ch) override;
  void print(const std::string& text) override;
  void refresh() override;
  Size size() const override;
  std::optional<int> get_key(bool blocking) override;

  // Test helpers
  std::string grid_as_string() const;
  std::string last_output() const;

private:
  int rows_;
  int cols_;
  int row_ = 1;
  int col_ = 1;
  int fg_ = 7;
  int bg_ = 0;
  std::vector<Cell> grid_;
  std::string output_;
};
