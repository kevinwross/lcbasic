#include "headless_screen.h"

HeadlessScreen::HeadlessScreen(int rows, int cols)
  : rows_(rows), cols_(cols), grid_(static_cast<std::size_t>(rows) * cols) {}

void HeadlessScreen::clear() {
  std::fill(grid_.begin(), grid_.end(), Cell{});
  row_ = 1;
  col_ = 1;
  output_.clear();
}

void HeadlessScreen::locate(int row, int col) {
  row_ = row;
  col_ = col;
}

void HeadlessScreen::color(int foreground, int background) {
  fg_ = foreground;
  bg_ = background;
}

void HeadlessScreen::putc(char ch) {
  if (row_ < 1 || row_ > rows_ || col_ < 1 || col_ > cols_) return;
  std::size_t idx = static_cast<std::size_t>((row_ - 1) * cols_ + (col_ - 1));
  grid_[idx].ch = ch;
  grid_[idx].fg = fg_;
  grid_[idx].bg = bg_;
  output_ += ch;
  col_++;
  if (col_ > cols_) {
    col_ = 1;
    row_++;
  }
}

void HeadlessScreen::print(const std::string& text) {
  for (char ch : text) putc(ch);
}

void HeadlessScreen::refresh() {}

HeadlessScreen::Size HeadlessScreen::size() const { return Size{rows_, cols_}; }

std::optional<int> HeadlessScreen::get_key(bool /*blocking*/) {
  return std::nullopt;
}

std::string HeadlessScreen::grid_as_string() const {
  std::string result;
  for (int r = 0; r < rows_; ++r) {
    for (int c = 0; c < cols_; ++c) {
      std::size_t idx = static_cast<std::size_t>(r * cols_ + c);
      result += grid_[idx].ch;
    }
    result += '\n';
  }
  return result;
}

std::string HeadlessScreen::last_output() const { return output_; }
