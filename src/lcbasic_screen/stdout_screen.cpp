#include "stdout_screen.h"

StdoutScreen::StdoutScreen(std::ostream& out, int rows, int cols)
  : out_(out), rows_(rows), cols_(cols) {}

void StdoutScreen::clear() {
  // Emit a form-feed-like clear so the byte stream marks the boundary.
  // For differential testing the harness normalises this away.
  row_ = 1;
  col_ = 1;
}

void StdoutScreen::locate(int row, int col) {
  row_ = row;
  col_ = col;
}

void StdoutScreen::color(int /*foreground*/, int /*background*/) {
  // Colours are not represented in the teletype output.
}

void StdoutScreen::putc(char ch) {
  out_ << ch;
  if (ch == '\n') {
    row_++;
    col_ = 1;
  } else {
    col_++;
    if (col_ > cols_) {
      col_ = 1;
      row_++;
    }
  }
}

void StdoutScreen::print(const std::string& text) {
  for (char ch : text) putc(ch);
}

void StdoutScreen::refresh() { out_.flush(); }

StdoutScreen::Size StdoutScreen::size() const { return Size{rows_, cols_}; }

int StdoutScreen::column() const { return col_; }
int StdoutScreen::row() const { return row_; }

std::optional<int> StdoutScreen::get_key(bool /*blocking*/) {
  return std::nullopt;
}
