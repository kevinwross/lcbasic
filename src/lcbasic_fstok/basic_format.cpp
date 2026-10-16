#include "basic_format.h"
#include "tokenizer.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fstok {

// ----- Protected encoding -----

// GW-BASIC protected files XOR each byte (after the first two) with a
// protection key. The key is derived from the first byte of the file.
static std::uint8_t protection_key(std::uint8_t first_byte) {
  // Simple key derivation: rotate bits.
  return static_cast<std::uint8_t>((first_byte << 3) | (first_byte >> 5));
}

static void apply_protection(std::vector<std::uint8_t>& data,
                             std::uint8_t first_byte) {
  std::uint8_t key = protection_key(first_byte);
  for (std::size_t i = 2; i < data.size(); i++) {
    data[i] ^= key;
  }
}

static void remove_protection(std::vector<std::uint8_t>& data,
                              std::uint8_t first_byte) {
  std::uint8_t key = protection_key(first_byte);
  for (std::size_t i = 2; i < data.size(); i++) {
    data[i] ^= key;
  }
}

// ----- Encode -----

std::vector<std::uint8_t> encode_basic(const BasicProgram& program,
                                       bool protect) {
  std::vector<std::uint8_t> out;

  // Sort by line number.
  std::vector<std::pair<std::int32_t, std::string>> lines(program.begin(),
                                                          program.end());
  std::sort(lines.begin(), lines.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  for (std::size_t i = 0; i < lines.size(); i++) {
    std::int32_t line_num = lines[i].first;
    const std::string& text = lines[i].second;

    // Tokenize the statement portion.
    std::vector<std::uint8_t> tokens = tokenize_line(text);

    // Line header: line number (2 bytes LE) + next-line offset (2 bytes LE).
    // Offset is from start of this line to start of next line, or 0 if last.
    std::uint32_t next_offset = 0;
    if (i + 1 < lines.size()) {
      // 2 (line num) + 2 (offset) + tokens + 1 (terminator)
      next_offset = 4 + tokens.size() + 1;
    }

    // We need to know the full line size to write the offset, but the offset
    // refers to the next line. Since we write sequentially, we can compute
    // it: the current line's total size is 4 + tokens.size() + 1.
    // The next_line_offset field points to the start of the next line relative
    // to the start of this line. So it equals this line's total size.
    // For simplicity and GW-BASIC compatibility, next_offset = size of this
    // line record (including header and terminator).
    next_offset = 4 + tokens.size() + 1;

    // Write line number (2 bytes LE).
    std::int16_t ln16 = static_cast<std::int16_t>(line_num);
    out.push_back(static_cast<std::uint8_t>(ln16 & 0xFF));
    out.push_back(static_cast<std::uint8_t>((ln16 >> 8) & 0xFF));

    // Write next-line offset (2 bytes LE).
    out.push_back(static_cast<std::uint8_t>(next_offset & 0xFF));
    out.push_back(static_cast<std::uint8_t>((next_offset >> 8) & 0xFF));

    // Write tokenized bytes.
    out.insert(out.end(), tokens.begin(), tokens.end());

    // Line terminator.
    out.push_back(0x00);
  }

  // End-of-file marker.
  out.push_back(0xFF);
  out.push_back(0xFF);

  if (protect && !out.empty()) {
    apply_protection(out, out[0]);
  }

  return out;
}

// ----- Decode -----

// Validate that a decoded program looks sane: all line numbers are
// non-negative and every line's text starts with a known keyword or a
// variable name (letter).
static bool validate_program(const BasicProgram& prog) {
  if (prog.empty()) return false;
  for (const auto& [ln, text] : prog) {
    if (ln < 0 || ln > 99999) return false;
    // Trim leading spaces.
    std::size_t s = text.find_first_not_of(" \t");
    if (s == std::string::npos) return false;
    char c = text[s];
    if (!std::isalpha(static_cast<unsigned char>(c)) && c != '%' && c != '&' &&
        c != '!' && c != '#' && c != '$') {
      return false;
    }
  }
  return true;
}

BasicProgram decode_basic(const std::vector<std::uint8_t>& data) {
  if (data.empty()) return {};

  auto try_decode = [&](const std::vector<std::uint8_t>& src) -> BasicProgram {
    BasicProgram result;
    std::size_t pos = 0;
    std::size_t n = src.size();

    while (pos + 4 <= n) {
      std::uint8_t b0 = src[pos];
      std::uint8_t b1 = src[pos + 1];

      // End-of-file marker: 0xFF 0xFF
      if (b0 == 0xFF && b1 == 0xFF) break;

      std::int16_t line_num = static_cast<std::int16_t>(
          static_cast<std::uint16_t>(b0) | (static_cast<std::uint16_t>(b1) << 8));
      std::uint8_t off0 = src[pos + 2];
      std::uint8_t off1 = src[pos + 3];
      std::uint16_t next_off =
          static_cast<std::uint16_t>(off0) | (static_cast<std::uint16_t>(off1) << 8);

      pos += 4;  // skip header

      // Determine end of this line's token data.
      std::size_t line_end;
      if (next_off > 0 && pos + (next_off - 4) <= n) {
        line_end = pos + (next_off - 4);
      } else {
        line_end = n;
      }
      if (line_end < n && src[line_end] == 0x00) {
        // terminator present; line data is [pos, line_end)
      }
      // If no terminator, just use available bytes.

      std::vector<std::uint8_t> line_bytes(
          src.begin() + pos, src.begin() + line_end);
      pos = line_end;
      if (pos < n && src[pos] == 0x00) pos++;  // skip terminator

      // Detokenize.
      std::size_t dpos = 0;
      std::string text = detokenize_line(line_bytes, dpos);
      if (line_num != 0 || !text.empty()) {
        result[line_num] = text;
      }
    }

    return result;
  };

  // Try unprotected first.
  BasicProgram result = try_decode(data);
  if (validate_program(result)) return result;

  // Try protected: XOR bytes from index 2 onward.
  std::vector<std::uint8_t> working = data;
  if (working.size() > 2) {
    std::uint8_t key = protection_key(working[0]);
    for (std::size_t i = 2; i < working.size(); i++) {
      working[i] ^= key;
    }
  }
  result = try_decode(working);
  if (validate_program(result)) return result;

  return {};
}

// ----- File I/O -----

BasFileResult load_bas_file(const std::string& path) {
  BasFileResult result;

  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "lcbasic: cannot open '" << path << "'\n";
    return result;
  }

  // Read entire file.
  std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());

  if (data.size() >= 2 && data[0] == 0xFF && data[1] == 0xFF) {
    // Empty binary file.
    return result;
  }

  // Detect binary vs text: binary files start with a line number (small
  // integer bytes), text files start with ASCII digits (0x30-0x39).
  bool is_binary = false;
  if (data.size() >= 4) {
    // If first byte is a digit (0x30-0x39), it's text.
    if (data[0] >= 0x30 && data[0] <= 0x39) {
      is_binary = false;
    } else {
      // Could be binary. Try decoding.
      is_binary = true;
    }
  }

  if (is_binary) {
    result.program = decode_basic(data);
    result.is_binary = true;
  } else {
    // Text mode: parse line-numbered source.
    std::string text(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.empty()) continue;
      std::istringstream liss(line);
      std::int32_t ln = 0;
      liss >> ln;
      if (liss.fail()) continue;
      std::string rest;
      std::getline(liss, rest);
      std::size_t s = rest.find_first_not_of(" \t");
      if (s != std::string::npos) rest = rest.substr(s);
      result.program[ln] = rest;
    }
    result.is_binary = false;
  }

  return result;
}

void save_bas_file(const std::string& path, const BasicProgram& program,
                   bool binary, bool protect) {
  if (binary) {
    std::vector<std::uint8_t> data = encode_basic(program, protect);
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
      std::cerr << "lcbasic: cannot create '" << path << "'\n";
      return;
    }
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
  } else {
    // Text mode: write lines sorted by line number.
    std::vector<std::pair<std::int32_t, std::string>> lines(program.begin(),
                                                            program.end());
    std::sort(lines.begin(), lines.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    std::ofstream file(path);
    if (!file.is_open()) {
      std::cerr << "lcbasic: cannot create '" << path << "'\n";
      return;
    }
    for (const auto& [ln, src] : lines) {
      file << ln << " " << src << "\n";
    }
  }
}

}  // namespace fstok
