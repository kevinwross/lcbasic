#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Built-in functions available in expressions.
enum class Builtin : std::uint8_t {
  Abs, Sgn, Int, Fix, Sqr, Sin, Cos, Tan, Atn, Log, Exp, Rnd,
  CInt, CSng, CDbl,
  Len, Left, Right, Mid, Instr, Val, Str, Chr, Asc, Space, StringFn,
  UCase, LCase, Hex, Oct,
  Tab, Spc, Fre, Pos, Csrlin,
};

// Look up a built-in by (case-insensitive) name. Names include '$' where
// applicable (LEFT$, MID$, etc.).
std::optional<Builtin> find_builtin(const std::string& name);
