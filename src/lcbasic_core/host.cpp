#include "host.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>

int RealHost::file_open(const std::string& filename,
                        const std::string& mode) {
  std::ios_base::openmode om = std::ios_base::in | std::ios_base::out;
  bool writing = false;
  if (mode == "O" || mode == "o") { // Output
    om = std::ios_base::out | std::ios_base::trunc;
    writing = true;
  } else if (mode == "I" || mode == "i") { // Input
    om = std::ios_base::in;
  } else if (mode == "A" || mode == "a") { // Append
    om = std::ios_base::out | std::ios_base::app;
    writing = true;
  }

  auto entry = std::make_unique<FileEntry>();
  entry->fs.open(filename, om);
  entry->writing = writing;
  if (!entry->fs.is_open()) return -1;

  int handle = static_cast<int>(files_.size());
  files_.push_back(std::move(*entry));
  return handle;
}

void RealHost::file_close(int handle) {
  if (handle < 0 || handle >= static_cast<int>(files_.size())) return;
  files_[handle].fs.close();
}

bool RealHost::file_eof(int handle) {
  if (handle < 0 || handle >= static_cast<int>(files_.size())) return true;
  return files_[handle].fs.eof();
}

void RealHost::file_write(int handle, const std::string& text) {
  if (handle < 0 || handle >= static_cast<int>(files_.size())) return;
  files_[handle].fs << text;
}

std::string RealHost::file_read_line(int handle) {
  if (handle < 0 || handle >= static_cast<int>(files_.size())) return "";
  std::string line;
  std::getline(files_[handle].fs, line);
  return line;
}

double RealHost::timer() {
  using namespace std::chrono;
  auto now = system_clock::now().time_since_epoch();
  auto ms = duration_cast<milliseconds>(now).count() % 86400000;
  return ms / 1000.0;
}

double RealHost::rnd(double /*seed*/) {
  return static_cast<double>(std::rand()) / RAND_MAX;
}
