#pragma once

#include "screen.h"

#include <iostream>
#include <string>

// Teletype-style screen that writes program output directly to a stream
// (stdout by default). Used for headless execution and differential testing
// against PC-BASIC, where the rendered screen is captured as a character
// stream rather than an 80x25 grid.
class StdoutScreen : public Screen {
public:
  explicit StdoutScreen(std::ostream& out = std::cout, int rows = 25,
                        int cols = 80);

  void clear() override;
  void locate(int row, int col) override;
  void color(int foreground, int background) override;
  void putc(char ch) override;
  void print(const std::string& text) override;
  void refresh() override;
  Size size() const override;
  int column() const override;
  std::optional<int> get_key(bool blocking) override;

private:
  std::ostream& out_;
  int rows_;
  int cols_;
  int row_ = 1;
  int col_ = 1;
};
