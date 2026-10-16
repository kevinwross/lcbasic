#include "json.h"

#include <cmath>
#include <cstdio>
#include <sstream>

namespace dap {

const Json& Json::operator[](const std::string& key) const {
  static const Json null_json;
  if (!is_object()) return null_json;
  auto it = as_object().find(key);
  if (it == as_object().end()) return null_json;
  return it->second;
}

Json& Json::operator[](const std::string& key) {
  if (!is_object()) data_ = Object{};
  return as_object()[key];
}

bool Json::contains(const std::string& key) const {
  if (!is_object()) return false;
  return as_object().find(key) != as_object().end();
}

std::string Json::get_string(const std::string& key,
                             const std::string& def) const {
  const Json& v = (*this)[key];
  return v.is_string() ? v.as_string() : def;
}

int Json::get_int(const std::string& key, int def) const {
  const Json& v = (*this)[key];
  return v.is_number() ? static_cast<int>(v.as_number()) : def;
}

bool Json::get_bool(const std::string& key, bool def) const {
  const Json& v = (*this)[key];
  return v.is_bool() ? v.as_bool() : def;
}

// ----- Serialization -----

namespace {

void escape_string(const std::string& s, std::string& out) {
  out += '"';
  for (char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x",
                        static_cast<unsigned char>(c));
          out += buf;
        } else {
          out += c;
        }
    }
  }
  out += '"';
}

void dump_impl(const Json& j, std::string& out) {
  if (j.is_null()) {
    out += "null";
  } else if (j.is_bool()) {
    out += j.as_bool() ? "true" : "false";
  } else if (j.is_number()) {
    double d = j.as_number();
    if (d == static_cast<double>(static_cast<std::int64_t>(d)) &&
        std::abs(d) < 1e15) {
      out += std::to_string(static_cast<std::int64_t>(d));
    } else {
      std::ostringstream oss;
      oss << d;
      out += oss.str();
    }
  } else if (j.is_string()) {
    escape_string(j.as_string(), out);
  } else if (j.is_array()) {
    out += '[';
    bool first = true;
    for (const auto& e : j.as_array()) {
      if (!first) out += ',';
      first = false;
      dump_impl(e, out);
    }
    out += ']';
  } else if (j.is_object()) {
    out += '{';
    bool first = true;
    for (const auto& [k, v] : j.as_object()) {
      if (!first) out += ',';
      first = false;
      escape_string(k, out);
      out += ':';
      dump_impl(v, out);
    }
    out += '}';
  }
}

// ----- Parsing -----

class Parser {
public:
  explicit Parser(const std::string& text) : text_(text) {}

  bool parse(Json& out) {
    skip_ws();
    if (!parse_value(out)) return false;
    skip_ws();
    return pos_ == text_.size();
  }

private:
  const std::string& text_;
  std::size_t pos_ = 0;

  void skip_ws() {
    while (pos_ < text_.size() &&
           (text_[pos_] == ' ' || text_[pos_] == '\t' ||
            text_[pos_] == '\n' || text_[pos_] == '\r')) {
      pos_++;
    }
  }

  bool parse_value(Json& out) {
    skip_ws();
    if (pos_ >= text_.size()) return false;
    char c = text_[pos_];
    if (c == '{') return parse_object(out);
    if (c == '[') return parse_array(out);
    if (c == '"') {
      std::string s;
      if (!parse_string(s)) return false;
      out = Json(s);
      return true;
    }
    if (c == 't' || c == 'f') return parse_bool(out);
    if (c == 'n') return parse_null(out);
    if (c == '-' || (c >= '0' && c <= '9')) return parse_number(out);
    return false;
  }

  bool parse_object(Json& out) {
    Json::Object obj;
    pos_++;  // '{'
    skip_ws();
    if (pos_ < text_.size() && text_[pos_] == '}') {
      pos_++;
      out = Json(std::move(obj));
      return true;
    }
    while (true) {
      skip_ws();
      std::string key;
      if (!parse_string(key)) return false;
      skip_ws();
      if (pos_ >= text_.size() || text_[pos_] != ':') return false;
      pos_++;
      Json val;
      if (!parse_value(val)) return false;
      obj[key] = std::move(val);
      skip_ws();
      if (pos_ >= text_.size()) return false;
      if (text_[pos_] == ',') {
        pos_++;
        continue;
      }
      if (text_[pos_] == '}') {
        pos_++;
        break;
      }
      return false;
    }
    out = Json(std::move(obj));
    return true;
  }

  bool parse_array(Json& out) {
    Json::Array arr;
    pos_++;  // '['
    skip_ws();
    if (pos_ < text_.size() && text_[pos_] == ']') {
      pos_++;
      out = Json(std::move(arr));
      return true;
    }
    while (true) {
      Json val;
      if (!parse_value(val)) return false;
      arr.push_back(std::move(val));
      skip_ws();
      if (pos_ >= text_.size()) return false;
      if (text_[pos_] == ',') {
        pos_++;
        continue;
      }
      if (text_[pos_] == ']') {
        pos_++;
        break;
      }
      return false;
    }
    out = Json(std::move(arr));
    return true;
  }

  bool parse_string(std::string& out) {
    if (pos_ >= text_.size() || text_[pos_] != '"') return false;
    pos_++;
    out.clear();
    while (pos_ < text_.size()) {
      char c = text_[pos_++];
      if (c == '"') return true;
      if (c == '\\') {
        if (pos_ >= text_.size()) return false;
        char esc = text_[pos_++];
        switch (esc) {
          case '"': out += '"'; break;
          case '\\': out += '\\'; break;
          case '/': out += '/'; break;
          case 'n': out += '\n'; break;
          case 'r': out += '\r'; break;
          case 't': out += '\t'; break;
          case 'b': out += '\b'; break;
          case 'f': out += '\f'; break;
          case 'u': {
            if (pos_ + 4 > text_.size()) return false;
            unsigned int code = 0;
            for (int i = 0; i < 4; i++) {
              char h = text_[pos_++];
              code <<= 4;
              if (h >= '0' && h <= '9') code |= (h - '0');
              else if (h >= 'a' && h <= 'f') code |= (h - 'a' + 10);
              else if (h >= 'A' && h <= 'F') code |= (h - 'A' + 10);
              else return false;
            }
            // UTF-8 encode (basic BMP only).
            if (code < 0x80) {
              out += static_cast<char>(code);
            } else if (code < 0x800) {
              out += static_cast<char>(0xC0 | (code >> 6));
              out += static_cast<char>(0x80 | (code & 0x3F));
            } else {
              out += static_cast<char>(0xE0 | (code >> 12));
              out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
              out += static_cast<char>(0x80 | (code & 0x3F));
            }
            break;
          }
          default: return false;
        }
      } else {
        out += c;
      }
    }
    return false;
  }

  bool parse_bool(Json& out) {
    if (text_.compare(pos_, 4, "true") == 0) {
      pos_ += 4;
      out = Json(true);
      return true;
    }
    if (text_.compare(pos_, 5, "false") == 0) {
      pos_ += 5;
      out = Json(false);
      return true;
    }
    return false;
  }

  bool parse_null(Json& out) {
    if (text_.compare(pos_, 4, "null") == 0) {
      pos_ += 4;
      out = Json(nullptr);
      return true;
    }
    return false;
  }

  bool parse_number(Json& out) {
    std::size_t start = pos_;
    if (pos_ < text_.size() && text_[pos_] == '-') pos_++;
    while (pos_ < text_.size() &&
           ((text_[pos_] >= '0' && text_[pos_] <= '9') ||
            text_[pos_] == '.' || text_[pos_] == 'e' || text_[pos_] == 'E' ||
            text_[pos_] == '+' || text_[pos_] == '-')) {
      pos_++;
    }
    std::string num = text_.substr(start, pos_ - start);
    try {
      out = Json(std::stod(num));
    } catch (...) {
      return false;
    }
    return true;
  }
};

}  // namespace

std::string Json::dump() const {
  std::string out;
  dump_impl(*this, out);
  return out;
}

bool Json::parse(const std::string& text, Json& out) {
  Parser p(text);
  return p.parse(out);
}

}  // namespace dap
