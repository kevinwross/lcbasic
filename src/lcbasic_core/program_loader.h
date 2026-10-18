#pragma once

#include "parser.h"

#include <string>

// Parse line-numbered BASIC source text into a Program. Returns false and
// sets `error` on a syntax error.
bool parse_program_text(const std::string& text, Program& out,
                        std::string& error);

// Read a plain-text .BAS file and parse it.
bool load_program_file(const std::string& path, Program& out,
                       std::string& error);
