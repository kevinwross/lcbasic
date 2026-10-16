#pragma once

#include <cstdint>
#include <string>
#include <variant>

// GW-BASIC numeric types. All math is performed in double (host f64) with
// GW-BASIC-style PRINT formatting applied on output.
using Int16 = std::int16_t;   // integer %
using Int32 = std::int32_t;   // long &
using Sgl   = float;          // single !
using Dbl   = double;         // double #
using Str   = std::string;    // string $

struct Value {
  using Storage = std::variant<Int16, Int32, Sgl, Dbl, Str>;
  Storage data;

  // Default-constructs to numeric zero (double 0.0), matching GW-BASIC's
  // default value for uninitialised numeric variables.
  Value() : data(Dbl{0.0}) {}

  template <typename T>
  explicit Value(T v) : data(v) {}

  bool is_string() const { return std::holds_alternative<Str>(data); }
  bool is_numeric() const {
    return std::holds_alternative<Int16>(data) ||
           std::holds_alternative<Int32>(data) ||
           std::holds_alternative<Sgl>(data) ||
           std::holds_alternative<Dbl>(data);
  }

  double as_double() const {
    return std::visit(
        [](auto&& v) -> double {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, Str>) {
            // A string used where a number is expected converts to 0.0,
            // mirroring GW-BASIC's loose typing.
            return 0.0;
          } else {
            return static_cast<double>(v);
          }
        },
        data);
  }

  std::string as_string() const {
    return std::visit(
        [](auto&& v) -> std::string {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, Str>) {
            return v;
          } else {
            return std::to_string(v);
          }
        },
        data);
  }
};
