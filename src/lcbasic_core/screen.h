#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

// Abstract screen interface. Implementations render text to a terminal
// (ncurses), capture to an in-memory grid (headless for tests), or later a
// GPU window. This is the single seam where text, graphics, and input attach.
class Screen {
public:
  struct Size {
    int rows;
    int cols;
  };

  virtual ~Screen() = default;

  // Clear the screen and home the cursor.
  virtual void clear() = 0;

  // Move the cursor to 1-based (row, col). GW-BASIC LOCATE semantics.
  virtual void locate(int row, int col) = 0;

  // Set foreground/background colour. Colours are GW-BASIC colour indices 0-15.
  virtual void color(int foreground, int background) = 0;

  // Write a single character at the current cursor position, advancing the cursor.
  virtual void putc(char ch) = 0;

  // Write a null-terminated string at the current cursor position.
  virtual void print(const std::string& text) = 0;

  // Flush any buffered output to the physical display.
  virtual void refresh() = 0;

  // Return the physical dimensions of the display (e.g. 25 x 80).
  virtual Size size() const = 0;

  // Read a single keypress; returns std::nullopt if no key is available
  // (non-blocking) or the implementation blocks until a key is pressed.
  virtual std::optional<int> get_key(bool blocking) = 0;
};
