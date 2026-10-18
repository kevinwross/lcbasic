#include "builtins.h"

#include <cctype>
#include <unordered_map>

namespace {

std::string upper(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  return s;
}

const std::unordered_map<std::string, Builtin>& table() {
  static const std::unordered_map<std::string, Builtin> t = {
      {"ABS", Builtin::Abs},
      {"SGN", Builtin::Sgn},
      {"INT", Builtin::Int},
      {"FIX", Builtin::Fix},
      {"SQR", Builtin::Sqr},
      {"SIN", Builtin::Sin},
      {"COS", Builtin::Cos},
      {"TAN", Builtin::Tan},
      {"ATN", Builtin::Atn},
      {"LOG", Builtin::Log},
      {"EXP", Builtin::Exp},
      {"RND", Builtin::Rnd},
      {"CINT", Builtin::CInt},
      {"CSNG", Builtin::CSng},
      {"CDBL", Builtin::CDbl},
      {"LEN", Builtin::Len},
      {"LEFT$", Builtin::Left},
      {"RIGHT$", Builtin::Right},
      {"MID$", Builtin::Mid},
      {"INSTR", Builtin::Instr},
      {"VAL", Builtin::Val},
      {"STR$", Builtin::Str},
      {"CHR$", Builtin::Chr},
      {"ASC", Builtin::Asc},
      {"SPACE$", Builtin::Space},
      {"STRING$", Builtin::StringFn},
      {"UCASE$", Builtin::UCase},
      {"LCASE$", Builtin::LCase},
      {"HEX$", Builtin::Hex},
      {"OCT$", Builtin::Oct},
      {"TAB", Builtin::Tab},
      {"SPC", Builtin::Spc},
      {"FRE", Builtin::Fre},
      {"POS", Builtin::Pos},
      {"CSRLIN", Builtin::Csrlin},
      {"INKEY$", Builtin::Inkey},
      {"DATE$", Builtin::Date},
      {"TIME$", Builtin::Time},
      {"INPUT$", Builtin::InputDollar},
      {"EOF", Builtin::Eof},
      {"LOF", Builtin::Lof},
      {"LOC", Builtin::Loc},
      {"FREEFILE", Builtin::Freefile},
      {"CVI", Builtin::CVi},
      {"CVS", Builtin::CVs},
      {"CVD", Builtin::CVd},
      {"MKI$", Builtin::Mki},
      {"MKS$", Builtin::Mks},
      {"MKD$", Builtin::Mkd},
  };
  return t;
}

}  // namespace

std::optional<Builtin> find_builtin(const std::string& name) {
  auto it = table().find(upper(name));
  if (it == table().end()) return std::nullopt;
  return it->second;
}
