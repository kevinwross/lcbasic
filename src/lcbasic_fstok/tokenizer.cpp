#include "tokenizer.h"

#include <cctype>
#include <cstring>
#include <sstream>

namespace fstok {

namespace {

struct KeywordEntry {
  const char* word;
  std::uint8_t token;
};

const KeywordEntry kKeywords[] = {
    {"END", TOK_END},
    {"FOR", TOK_FOR},
    {"NEXT", TOK_NEXT},
    {"DATA", TOK_DATA},
    {"INPUT#", TOK_INPUTHASH},
    {"INPUT", TOK_INPUT},
    {"DIM", TOK_DIM},
    {"READ", TOK_READ},
    {"LET", TOK_LET},
    {"GOTO", TOK_GOTO},
    {"RUN", TOK_RUN},
    {"IF", TOK_IF},
    {"RESTORE", TOK_RESTORE},
    {"GOSUB", TOK_GOSUB},
    {"RETURN", TOK_RETURN},
    {"REM", TOK_REM},
    {"STOP", TOK_STOP},
    {"PRINT#", TOK_PRINTHASH},
    {"PRINT", TOK_PRINT},
    {"CLEAR", TOK_CLEAR},
    {"LIST", TOK_LIST},
    {"NEW", TOK_NEW},
    {"TAB(", TOK_TAB},
    {"TO", TOK_TO},
    {"FN", TOK_FN},
    {"SPC(", TOK_SPC},
    {"THEN", TOK_THEN},
    {"NOT", TOK_NOT},
    {"OR", TOK_OR},
    {"AND", TOK_AND},
    {"XOR", TOK_XOR},
    {"EQV", TOK_EQV},
    {"?", TOK_QUESTION},
    {"MOD", TOK_MOD},
};

std::uint8_t find_token(const std::string& word) {
  for (const auto& e : kKeywords) {
    if (word == e.word) return e.token;
  }
  return 0;
}

std::string find_keyword(std::uint8_t tok) {
  for (const auto& e : kKeywords) {
    if (e.token == tok) return e.word;
  }
  return "";
}

bool is_alpha(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool is_alnum(char c) {
  return is_alpha(c) || std::isdigit(static_cast<unsigned char>(c));
}

bool is_digit(char c) {
  return std::isdigit(static_cast<unsigned char>(c));
}

}  // namespace

std::uint8_t keyword_token(const std::string& word) { return find_token(word); }

std::string token_keyword(std::uint8_t tok) { return find_keyword(tok); }

// ----- Tokenizer -----

// We tokenize a statement portion (no line number). Keywords are matched
// case-insensitively (GW-BASIC uppercase keywords, but accepts lowercase).
std::vector<std::uint8_t> tokenize_line(const std::string& text) {
  std::vector<std::uint8_t> out;
  std::size_t i = 0;
  std::size_t n = text.size();

  while (i < n) {
    char c = text[i];

    // Whitespace: skip (GW-BASIC tokenized form doesn't preserve spaces
    // except inside strings and DATA).
    if (std::isspace(static_cast<unsigned char>(c))) {
      i++;
      continue;
    }

    // String literal
    if (c == '"') {
      out.push_back(static_cast<std::uint8_t>('"'));
      i++;
      while (i < n && text[i] != '"') {
        out.push_back(static_cast<std::uint8_t>(text[i]));
        i++;
      }
      if (i < n) {  // closing quote
        out.push_back(static_cast<std::uint8_t>('"'));
        i++;
      }
      continue;
    }

    // Number
    if (is_digit(c) || (c == '.' && i + 1 < n && is_digit(text[i + 1]))) {
      // Scan the full numeric literal including &H / &O / E / D suffixes.
      std::size_t start = i;
      while (i < n &&
             (is_alnum(text[i]) || text[i] == '.' || text[i] == '&' ||
              text[i] == 'O' || text[i] == 'H' || text[i] == 'E' ||
              text[i] == 'D' || text[i] == '+' || text[i] == '-')) {
        i++;
      }
      std::string num = text.substr(start, i - start);
      double val = std::stod(num);

      // GW-BASIC encoding: if it fits in int16, use INT16 marker.
      if (val == static_cast<int>(val) &&
          val >= -32768.0 && val <= 32767.0 &&
          num.find('.') == std::string::npos &&
          num.find('e') == std::string::npos &&
          num.find('E') == std::string::npos &&
          num.find('D') == std::string::npos &&
          num.find('d') == std::string::npos) {
        out.push_back(NUM_INT16);
        int16_t ival = static_cast<int16_t>(val);
        out.push_back(static_cast<std::uint8_t>(ival & 0xFF));
        out.push_back(static_cast<std::uint8_t>((ival >> 8) & 0xFF));
      } else {
        // Default to double precision.
        out.push_back(NUM_DOUBLE);
        double dval = val;
        const unsigned char* bytes =
            reinterpret_cast<const unsigned char*>(&dval);
        for (int k = 0; k < 8; k++) out.push_back(bytes[k]);
      }
      continue;
    }

    // Identifier or keyword
    if (is_alpha(c)) {
      std::size_t start = i;
      while (i < n && (is_alnum(text[i]) || text[i] == '%' ||
                       text[i] == '!' || text[i] == '#' || text[i] == '$' ||
                       text[i] == '&')) {
        i++;
      }
      std::string word = text.substr(start, i - start);
      std::uint8_t tok = find_token(word);
      if (tok) {
        out.push_back(tok);
      } else {
        // Identifier: copy the raw characters.
        for (char ch : word) out.push_back(static_cast<std::uint8_t>(ch));
      }
      continue;
    }

    // Operators and punctuation
    switch (c) {
      case '+': out.push_back('+'); i++; break;
      case '-': out.push_back('-'); i++; break;
      case '*': out.push_back('*'); i++; break;
      case '/': out.push_back('/'); i++; break;
      case '=': out.push_back('='); i++; break;
      case '<':
        if (i + 1 < n && text[i + 1] == '>') { out.push_back('<'); out.push_back('>'); i += 2; }
        else if (i + 1 < n && text[i + 1] == '=') { out.push_back('<'); out.push_back('='); i += 2; }
        else { out.push_back('<'); i++; }
        break;
      case '>':
        if (i + 1 < n && text[i + 1] == '=') { out.push_back('>'); out.push_back('='); i += 2; }
        else { out.push_back('>'); i++; }
        break;
      case '(': out.push_back('('); i++; break;
      case ')': out.push_back(')'); i++; break;
      case ',': out.push_back(','); i++; break;
      case ';': out.push_back(';'); i++; break;
      case ':': out.push_back(':'); i++; break;
      case '&':  // &H / &O prefix handled as part of number above, but lone &
        out.push_back('&'); i++; break;
      default:
        // Unknown character: pass through as-is.
        out.push_back(static_cast<std::uint8_t>(c));
        i++;
        break;
    }
  }

  return out;
}

std::string detokenize_line(const std::vector<std::uint8_t>& bytes,
                            std::size_t& pos) {
  std::string out;
  std::size_t n = bytes.size();

  // Track whether the last emitted character was alphanumeric, so we can
  // insert spaces between adjacent tokens (e.g. PRINT X → "PRINT X" not
  // "PRINTX").
  bool last_was_alnum = false;

  auto needs_space_before = [&]() {
    return last_was_alnum;
  };

  auto emit_raw = [&](char c) {
    out += c;
    last_was_alnum = std::isalnum(static_cast<unsigned char>(c));
  };

  auto emit_char = [&](char c) {
    if (needs_space_before() &&
        std::isalnum(static_cast<unsigned char>(c))) {
      out += ' ';
    }
    out += c;
    last_was_alnum = std::isalnum(static_cast<unsigned char>(c));
  };

  auto emit_str = [&](const std::string& s) {
    if (!s.empty()) {
      if (needs_space_before() &&
          std::isalnum(static_cast<unsigned char>(s.front()))) {
        out += ' ';
      }
      for (char c : s) out += c;
      last_was_alnum =
          std::isalnum(static_cast<unsigned char>(s.back()));
    }
  };

  while (pos < n) {
    std::uint8_t b = bytes[pos];
    pos++;

    if (b == 0x00) {  // line terminator
      break;
    }

    if (b == '"') {  // string literal
      emit_raw('"');
      while (pos < n && bytes[pos] != '"') {
        emit_raw(static_cast<char>(bytes[pos]));
        pos++;
      }
      if (pos < n) { emit_raw('"'); pos++; }
      continue;
    }

    // Numeric constants
    if (b == NUM_INT16) {
      if (pos + 1 >= n) break;
      int16_t val = static_cast<int16_t>(
          bytes[pos] | (bytes[pos + 1] << 8));
      pos += 2;
      emit_str(std::to_string(val));
      continue;
    }
    if (b == NUM_DOUBLE) {
      if (pos + 7 >= n) break;
      double dval;
      unsigned char* bytes_ptr = reinterpret_cast<unsigned char*>(&dval);
      for (int k = 0; k < 8; k++) bytes_ptr[k] = bytes[pos + k];
      pos += 8;
      emit_str(std::to_string(dval));
      continue;
    }

    // Keyword token
    std::string kw = find_keyword(b);
    if (!kw.empty()) {
      emit_str(kw);
      continue;
    }

    // ASCII character
    emit_char(static_cast<char>(b));
  }

  return out;
}

}  // namespace fstok
