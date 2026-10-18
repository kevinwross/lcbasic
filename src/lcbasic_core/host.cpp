#include "host.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

// Simple DOS-style wildcard match supporting '*' and '?'.
bool wildcard_match(const std::string& pattern, const std::string& name) {
  std::size_t p = 0, n = 0;
  std::size_t star = std::string::npos, mark = 0;
  while (n < name.size()) {
    if (p < pattern.size() &&
        (pattern[p] == '?' || pattern[p] == name[n])) {
      p++;
      n++;
    } else if (p < pattern.size() && pattern[p] == '*') {
      star = p++;
      mark = n;
    } else if (star != std::string::npos) {
      p = star + 1;
      n = ++mark;
    } else {
      return false;
    }
  }
  while (p < pattern.size() && pattern[p] == '*') p++;
  return p == pattern.size();
}

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

}  // namespace

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

double RealHost::rnd(double seed) {
  if (seed < 0.0) {
    rng_.seed(static_cast<std::uint32_t>(-seed));
    last_rnd_ = std::uniform_real_distribution<double>(0.0, 1.0)(rng_);
    return last_rnd_;
  }
  if (seed == 0.0) return last_rnd_;
  last_rnd_ = std::uniform_real_distribution<double>(0.0, 1.0)(rng_);
  return last_rnd_;
}

std::string RealHost::input_line() {
  std::string line;
  if (!std::getline(std::cin, line)) return "";
  if (!line.empty() && line.back() == '\r') line.pop_back();
  return line;
}

// ----- DOS / filesystem -----

bool RealHost::dos_chdir(const std::string& path) {
  std::error_code ec;
  std::filesystem::current_path(path, ec);
  return !ec;
}

bool RealHost::dos_mkdir(const std::string& path) {
  std::error_code ec;
  return std::filesystem::create_directory(path, ec);
}

bool RealHost::dos_rmdir(const std::string& path) {
  std::error_code ec;
  return std::filesystem::remove(path, ec);
}

bool RealHost::dos_kill(const std::string& path) {
  std::error_code ec;
  return std::filesystem::remove(path, ec);
}

bool RealHost::dos_rename(const std::string& from, const std::string& to) {
  std::error_code ec;
  std::filesystem::rename(from, to, ec);
  return !ec;
}

std::vector<std::string> RealHost::dos_files(const std::string& pattern) {
  std::vector<std::string> out;
  std::error_code ec;
  std::string pat = pattern.empty() ? "*.*" : pattern;
  std::string pat_lower = lower(pat);
  for (const auto& entry : std::filesystem::directory_iterator(".", ec)) {
    std::string name = entry.path().filename().string();
    if (wildcard_match(pat_lower, lower(name))) {
      out.push_back(name);
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

int RealHost::dos_shell(const std::string& command) {
  if (command.empty()) return 0;
  return std::system(command.c_str());
}
