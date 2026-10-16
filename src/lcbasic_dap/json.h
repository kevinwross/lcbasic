#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace dap {

// Minimal JSON value type covering the subset needed for the Debug Adapter
// Protocol: null, bool, number, string, array, object.
class Json {
public:
  using Array = std::vector<Json>;
  using Object = std::map<std::string, Json>;

  Json() : data_(nullptr) {}
  Json(std::nullptr_t) : data_(nullptr) {}
  Json(bool b) : data_(b) {}
  Json(double d) : data_(d) {}
  Json(int i) : data_(static_cast<double>(i)) {}
  Json(const char* s) : data_(std::string(s)) {}
  Json(std::string s) : data_(std::move(s)) {}
  Json(Array a) : data_(std::move(a)) {}
  Json(Object o) : data_(std::move(o)) {}

  bool is_null() const { return std::holds_alternative<std::nullptr_t>(data_); }
  bool is_bool() const { return std::holds_alternative<bool>(data_); }
  bool is_number() const { return std::holds_alternative<double>(data_); }
  bool is_string() const { return std::holds_alternative<std::string>(data_); }
  bool is_array() const { return std::holds_alternative<Array>(data_); }
  bool is_object() const { return std::holds_alternative<Object>(data_); }

  bool as_bool() const { return std::get<bool>(data_); }
  double as_number() const { return std::get<double>(data_); }
  std::int64_t as_int() const {
    return static_cast<std::int64_t>(std::get<double>(data_));
  }
  const std::string& as_string() const { return std::get<std::string>(data_); }
  const Array& as_array() const { return std::get<Array>(data_); }
  Array& as_array() { return std::get<Array>(data_); }
  const Object& as_object() const { return std::get<Object>(data_); }
  Object& as_object() { return std::get<Object>(data_); }

  // Object access helpers.
  const Json& operator[](const std::string& key) const;
  Json& operator[](const std::string& key);
  bool contains(const std::string& key) const;
  std::string get_string(const std::string& key,
                         const std::string& def = "") const;
  int get_int(const std::string& key, int def = 0) const;
  bool get_bool(const std::string& key, bool def = false) const;

  // Serialize to a JSON string.
  std::string dump() const;

  // Parse a JSON string. Returns false on error.
  static bool parse(const std::string& text, Json& out);

private:
  std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data_;
};

}  // namespace dap
