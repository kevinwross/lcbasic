#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace fstok {

// A decoded .BAS program: line number -> raw source text.
using BasicProgram = std::unordered_map<std::int32_t, std::string>;

// ----- Encode: text program -> tokenized binary bytes -----

// Encode a program (line number -> source text) into GW-BASIC tokenized
// binary format. If protect is true, applies the protected encoding (XOR).
std::vector<std::uint8_t> encode_basic(const BasicProgram& program,
                                       bool protect = false);

// ----- Decode: tokenized binary bytes -> text program -----

// Decode tokenized binary bytes back into a text program. Automatically
// detects protected files and decodes them.
BasicProgram decode_basic(const std::vector<std::uint8_t>& data);

// ----- File helpers -----

// Read a .BAS file. Returns the decoded program and whether it was binary.
struct BasFileResult {
  BasicProgram program;
  bool is_binary = false;
};

BasFileResult load_bas_file(const std::string& path);

// Write a .BAS file. If binary is true, writes tokenized format.
// The protect flag applies protected encoding.
void save_bas_file(const std::string& path, const BasicProgram& program,
                   bool binary = false, bool protect = false);

}  // namespace fstok
