#include "value.h"

#include <stdexcept>

Value& VariableStore::get(const std::string& name) {
  auto it = vars_.find(name);
  if (it == vars_.end()) {
    VariableInfo info;
    info.type = deduce_var_type(name);
    info.value = Value(); // default zero for that type
    it = vars_.emplace(name, std::move(info)).first;
  }
  return it->second.value;
}

const Value& VariableStore::get(const std::string& name) const {
  auto it = vars_.find(name);
  if (it != vars_.end()) return it->second.value;
  static const Value zero;
  return zero;
}

void VariableStore::set(const std::string& name, const Value& v) {
  auto it = vars_.find(name);
  if (it == vars_.end()) {
    VariableInfo info;
    info.type = deduce_var_type(name);
    info.value = v.coerce(info.type);
    vars_.emplace(name, std::move(info));
  } else {
    it->second.value = v.coerce(it->second.type);
  }
}

bool VariableStore::contains(const std::string& name) const {
  return vars_.find(name) != vars_.end();
}

VarType VariableStore::type_of(const std::string& name) const {
  auto it = vars_.find(name);
  if (it != vars_.end()) return it->second.type;
  return deduce_var_type(name);
}

void ArrayStore::dim(const std::string& name, std::int32_t option_base,
                     const std::vector<std::int32_t>& upper_bounds,
                     VarType type) {
  ArrayInfo info;
  info.type = type;
  info.option_base = option_base;
  info.dims.reserve(upper_bounds.size());

  std::size_t total = 1;
  for (std::int32_t ub : upper_bounds) {
    std::int32_t lb = option_base;
    std::int32_t count = ub - lb + 1;
    if (count < 0) count = 0;
    info.dims.push_back(ArrayDim{lb, ub});
    total *= static_cast<std::size_t>(count);
  }

  // Initialise elements by type: empty strings for string arrays, zero for
  // numeric arrays (GW-BASIC default values).
  info.data.assign(total, default_value(type));
  arrays_[name] = std::move(info);
}

std::size_t ArrayStore::offset(const ArrayInfo& info,
                               const std::vector<std::int32_t>& indices) const {
  if (indices.size() != info.dims.size()) {
    throw std::out_of_range("wrong number of subscripts");
  }
  std::size_t off = 0;
  std::size_t stride = 1;
  // Row-major: last dimension varies fastest.
  for (int i = static_cast<int>(indices.size()) - 1; i >= 0; --i) {
    std::int32_t idx = indices[i];
    std::int32_t lb = info.dims[i].lower;
    std::int32_t ub = info.dims[i].upper;
    if (idx < lb || idx > ub) {
      throw std::out_of_range("subscript out of range");
    }
    std::int32_t count = ub - lb + 1;
    off += static_cast<std::size_t>(idx - lb) * stride;
    stride *= static_cast<std::size_t>(count);
  }
  return off;
}

Value& ArrayStore::at(const std::string& name,
                      const std::vector<std::int32_t>& indices) {
  auto it = arrays_.find(name);
  if (it == arrays_.end()) {
    throw std::out_of_range("undimensioned array: " + name);
  }
  std::size_t off = offset(it->second, indices);
  return it->second.data[off];
}

const Value& ArrayStore::at(const std::string& name,
                            const std::vector<std::int32_t>& indices) const {
  auto it = arrays_.find(name);
  if (it == arrays_.end()) {
    throw std::out_of_range("undimensioned array: " + name);
  }
  std::size_t off = offset(it->second, indices);
  return it->second.data[off];
}

bool ArrayStore::contains(const std::string& name) const {
  return arrays_.find(name) != arrays_.end();
}

VarType ArrayStore::type_of(const std::string& name) const {
  auto it = arrays_.find(name);
  if (it == arrays_.end()) return VarType::VT_DBL;
  return it->second.type;
}

std::size_t ArrayStore::dimensions(const std::string& name) const {
  auto it = arrays_.find(name);
  if (it == arrays_.end()) return 0;
  return it->second.dims.size();
}

void ArrayStore::erase(const std::string& name) {
  arrays_.erase(name);
}

void DataPool::add(const Value& v) { pool_.push_back(v); }

Value DataPool::read() {
  if (ptr_ >= pool_.size()) return Value{}; // numeric zero if exhausted
  return pool_[ptr_++];
}

void DataPool::restore() { ptr_ = 0; }

bool DataPool::exhausted() const { return ptr_ >= pool_.size(); }
