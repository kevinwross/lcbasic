#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// GW-BASIC numeric types. All math is performed in double (host f64) with
// GW-BASIC-style PRINT formatting applied on output.
using Int16 = std::int16_t;   // integer %
using Int32 = std::int32_t;   // long &
using Sgl   = float;          // single !
using Dbl   = double;         // double #
using Str   = std::string;    // string $

enum class VarType : std::uint8_t {
  VT_INT16,  // %
  VT_INT32,  // &
  VT_SGL,    // !
  VT_DBL,    // # or no suffix
  VT_STR,    // $
};

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

  // Convert this value to the given VarType, truncating/coercing as needed.
  Value coerce(VarType t) const {
    switch (t) {
      case VarType::VT_INT16:
        return Value(static_cast<Int16>(as_double()));
      case VarType::VT_INT32:
        return Value(static_cast<Int32>(as_double()));
      case VarType::VT_SGL:
        return Value(static_cast<Sgl>(as_double()));
      case VarType::VT_DBL:
        return Value(as_double());
      case VarType::VT_STR:
        return Value(as_string());
    }
    return *this;
  }
};

// Deduce a variable's type from its name suffix.
inline VarType deduce_var_type(const std::string& name) {
  if (name.empty()) return VarType::VT_DBL;
  char c = name.back();
  switch (c) {
    case '%': return VarType::VT_INT16;
    case '&': return VarType::VT_INT32;
    case '!': return VarType::VT_SGL;
    case '#': return VarType::VT_DBL;
    case '$': return VarType::VT_STR;
    default:  return VarType::VT_DBL;
  }
}

// Strip the type suffix from a variable name.
inline std::string strip_var_suffix(const std::string& name) {
  if (name.empty()) return name;
  char c = name.back();
  if (c == '%' || c == '&' || c == '!' || c == '#' || c == '$')
    return name.substr(0, name.size() - 1);
  return name;
}

// In-memory variable store with type enforcement.
class VariableStore {
public:
  Value& get(const std::string& name);
  const Value& get(const std::string& name) const;
  void set(const std::string& name, const Value& v);
  bool contains(const std::string& name) const;
  VarType type_of(const std::string& name) const;

private:
  struct VariableInfo {
    Value value;
    VarType type = VarType::VT_DBL;
  };
  std::unordered_map<std::string, VariableInfo> vars_;
};

// ---------- Arrays ----------

struct ArrayDim {
  std::int32_t lower;
  std::int32_t upper; // inclusive
};

struct ArrayInfo {
  VarType type = VarType::VT_DBL;
  std::int32_t option_base = 0; // 0 or 1
  std::vector<ArrayDim> dims;
  std::vector<Value> data; // row-major flattened
};

// Array store for DIM'd arrays.
class ArrayStore {
public:
  // Declare/redimension an array. upper_bounds are inclusive per dimension.
  void dim(const std::string& name, std::int32_t option_base,
           const std::vector<std::int32_t>& upper_bounds, VarType type);

  Value& at(const std::string& name,
            const std::vector<std::int32_t>& indices);
  const Value& at(const std::string& name,
                  const std::vector<std::int32_t>& indices) const;
  bool contains(const std::string& name) const;

private:
  std::size_t offset(const ArrayInfo& info,
                     const std::vector<std::int32_t>& indices) const;

  std::unordered_map<std::string, ArrayInfo> arrays_;
};

// ---------- DATA pool ----------

// Sequential pool of values for READ/DATA/RESTORE.
class DataPool {
public:
  void add(const Value& v);
  Value read(); // returns next value, advances pointer; numeric zero if exhausted
  void restore(); // reset pointer to start
  bool exhausted() const;

private:
  std::vector<Value> pool_;
  std::size_t ptr_ = 0;
};
