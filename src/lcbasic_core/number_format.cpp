#include "number_format.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

// Trim trailing zeros from a fixed-point decimal string, and a trailing '.'.
std::string trim_trailing_zeros(std::string s) {
  if (s.find('.') == std::string::npos) return s;
  std::size_t last = s.find_last_not_of('0');
  if (last != std::string::npos && s[last] == '.') last--;
  return s.substr(0, last + 1);
}

// Convert a value to scientific notation in GW-BASIC style:
// "1.5E+10", "1E-08" (two-digit exponent with sign, no trailing zeros in
// the mantissa).
std::string format_scientific(double a) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.6E", a);
  std::string s = buf;
  // s looks like "1.500000E+10".
  std::size_t epos = s.find('E');
  if (epos == std::string::npos) return s;
  std::string mantissa = s.substr(0, epos);
  std::string exponent = s.substr(epos);  // includes 'E'
  mantissa = trim_trailing_zeros(mantissa);
  return mantissa + exponent;
}

}  // namespace

std::string format_basic_number(double v) {
  std::string sign;
  double a = v;
  if (v < 0.0 || (v == 0.0 && std::signbit(v))) {
    sign = "-";
    a = -v;
  } else {
    sign = " ";
  }

  std::string body;
  if (a == 0.0) {
    body = "0";
  } else if (a == std::floor(a) && a < 1e15) {
    // Integer: print in full (GW-BASIC prints large integers literally).
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.0f", a);
    body = buf;
  } else if (a >= 1e-7 && a < 1e15) {
    // Fixed-point: up to 7 decimal places, trailing zeros trimmed,
    // leading zero dropped for fractions.
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.7f", a);
    body = trim_trailing_zeros(buf);
    // GW-BASIC prints reals in a field that limits total significant digits;
    // for values that need more than ~7 significant digits, prefer %g-style
    // rounding to keep output stable.
    if (body.size() >= 2 && body[0] == '0' && body[1] == '.') {
      body = body.substr(1);
    }
  } else {
    body = format_scientific(a);
    // GW-BASIC also drops the leading zero for scientific mantissas < 1
    // (rare, since mantissa is 1..9.99).
  }

  return sign + body + " ";
}
