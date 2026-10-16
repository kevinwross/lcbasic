#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Host interface: filesystem, clock, randomness, key input.
// The CLI provides a real implementation; tests use a scripted/virtual one.
class Host {
public:
  virtual ~Host() = default;

  // ----- Filesystem -----
  // Open a file. Returns a file handle index (>= 0) or -1 on failure.
  virtual int file_open(const std::string& filename, const std::string& mode) = 0;
  virtual void file_close(int handle) = 0;
  virtual bool file_eof(int handle) = 0;
  virtual void file_write(int handle, const std::string& text) = 0;
  virtual std::string file_read_line(int handle) = 0;

  // ----- Clock / randomness -----
  virtual double timer() = 0;   // seconds since midnight (GW-BASIC TIMER)
  virtual double rnd(double seed) = 0;
};

// Real Host implementation backed by the OS filesystem.
class RealHost : public Host {
public:
  int file_open(const std::string& filename, const std::string& mode) override;
  void file_close(int handle) override;
  bool file_eof(int handle) override;
  void file_write(int handle, const std::string& text) override;
  std::string file_read_line(int handle) override;

  double timer() override;
  double rnd(double seed) override;

private:
  struct FileEntry {
    std::fstream fs;
    bool writing = false;
  };
  std::vector<FileEntry> files_; // index 0 unused (file numbers start at 1)
};
