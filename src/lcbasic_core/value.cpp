#include "value.h"

Value& VariableStore::get(const std::string& name) {
  return vars_[name]; // operator[] default-constructs to numeric zero
}

const Value& VariableStore::get(const std::string& name) const {
  auto it = vars_.find(name);
  if (it != vars_.end()) return it->second;
  // Return a static zero for uninitialised variables.
  static const Value zero;
  return zero;
}

void VariableStore::set(const std::string& name, const Value& v) {
  vars_[name] = v;
}

bool VariableStore::contains(const std::string& name) const {
  return vars_.find(name) != vars_.end();
}
