#pragma once

#include <string>

// GW-BASIC-style numeric output for PRINT. Positive numbers get a leading
// space (sign position) and every number gets a trailing space. Values below
// 1 are printed without a leading zero (".5" not "0.5"). Very small/large
// magnitudes use exponent notation.
std::string format_basic_number(double v);
