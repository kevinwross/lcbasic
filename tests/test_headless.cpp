#include "test_runner.h"
#include "headless_screen.h"

TEST(headless_screen_captures_printed_text) {
  HeadlessScreen screen(25, 80);
  screen.print("Hello");
  screen.refresh();

  CHECK(screen.last_output() == "Hello");
}

TEST(headless_screen_clear_resets_grid_and_cursor) {
  HeadlessScreen screen(5, 10);
  screen.print("abc");
  screen.clear();

  CHECK(screen.last_output().empty());
  auto [rows, cols] = screen.size();
  CHECK(rows == 5);
  CHECK(cols == 10);
}
