# lcbasic — Project Plan

A terminal BASIC interpreter targeting the **GW-BASIC dialect**, with an
in-process debugger library, a Debug Adapter Protocol (DAP) server, and a
display abstraction designed to grow into a graphics engine later.

Status legend: ✅ done · 🚧 in progress · ⏳ planned

---

## Decisions (agreed)

| Area | Decision |
|---|---|
| Language | C++ |
| Dialect | GW-BASIC 3.23 behavior; "like" fidelity — host `double` math with GW-BASIC-style `PRINT` formatting |
| Core | Bytecode VM (line-numbered) |
| Debugger | In-process **library API** + **DAP server** adapter |
| v1 coverage | Everything text-mode **except sound** (PLAY/SOUND/BEEP stubbed); graphics statements parse but report "not implemented" |
| Display | Full-screen **ScreenDevice abstraction** (80×25 text grid + cursor); ANSI terminal backend now, GPU window later |
| CLI | `lcbasic prog.bas` runner + direct-mode REPL (no full-screen editor) |
| Binary files | Tokenized `.BAS` read **and** write (incl. protected decode) |
| Testing | Unit + golden fixtures + **differential vs PC-BASIC**, with GitHub Actions CI |
| Time-travel | Deferred; VM designed for cheap snapshots, rewind implemented later |
| Layout | CMake build, binary name `lcbasic` |

---

## 1. Architecture / crate layout

```
lcbasic/
  CMakeLists.txt            # build
  src/
    lcbasic-core/            # values, lexer, parser, bytecode compiler, VM, errors
    lcbasic-screen/          # ScreenDevice impls: ANSI terminal + headless
    lcbasic-fstok/           # tokenized .BAS codec (read/write, protected)
    lcbasic-debug/           # debugger library: breakpoints, stepping, eval, stack, snapshots
    lcbasic-dap/             # Debug Adapter Protocol server over lcbasic-debug
    lcbasic-cli/             # `lcbasic` binary: file runner + direct-mode REPL
  fixtures/                 # *.bas + expected output
  tests/                    # differential harness + conformance corpus
```

Key seams:

- **`Screen` interface** (in `lcbasic-core::screen`) — text grid, cursor, `CLS`,
  `LOCATE`, `COLOR`, key input, line input. The single point where text,
  graphics, and input attach. `lcbasic-screen` provides `AnsiScreen` and
  `HeadlessScreen` (deterministic grid capture for tests).
- **`Host` interface** (planned) — clock, randomness, key input, filesystem.
  Real impl for the CLI; scripted/virtual impl for tests.

---

## 2. Front-end (`lcbasic-core`)

- Physical lines → line number + statements; raw source retained for `LIST`.
- Lexer: keywords, operators, identifiers with `%! #&$` suffixes, numeric
  literals (`&H`/`&O`, `E`/`D` exponents), strings, `REM`/`'`, `DATA` raw.
- Statement grammar: `LET`, `PRINT`, `INPUT`/`LINE INPUT`, `IF/THEN/ELSE`,
  `FOR/NEXT`, `WHILE/WEND`, `GOTO`/`GOSUB`/`RETURN`/`ON … GOTO|GOSUB`,
  `END`/`STOP`/`REM`, `DIM`/`OPTION BASE`, `DATA`/`READ`/`RESTORE`,
  `ON ERROR`/`RESUME`, event trapping, file I/O + DOS commands, screen ops.
- Program store: ordered map line-number → raw source; direct-mode statements
  execute immediately in the same VM.

## 3. Bytecode VM (`lcbasic-core`)

- Stack-based instruction set (`Instr`), with a **line table** (line number →
  pc range) that powers breakpoints, source stepping, and error reporting.
- Stacks kept in `std::vector`s so VM state is cheap to snapshot (prerequisite for
  time-travel debugging).
- Values: `Int(int16_t)`, `Long(int32_t)`, `Sgl(float)`, `Dbl(double)`, `Str(std::string)`; math is `double`.
- **Number formatting** module implementing GW-BASIC-style `PRINT` output.
- Error model: GW-BASIC error-code table + messages.

## 4. Tokenized `.BAS` codec (`lcbasic-fstok`)

Decode/encode the binary format (next-line offsets, token bytes, hex/octal,
numeric encodings) and the protected decode/encode algorithm.

## 5. Debugger (`lcbasic-debug` + `lcbasic-dap`)

Library API: breakpoints (line + conditional), stepping (into/over/out,
instruction-level), variable/array inspection, expression evaluation, call
stack (GOSUB + FOR frames), memory read/patch, bytecode disassembly, snapshots.
`lcbasic-dap` maps these onto the DAP subset used by VS Code / Neovim / Emacs.

---

## 6. Testing strategy

1. **Unit tests** — lexer, parser, formatter, VM ops, codec round-trips.
2. **Golden fixtures** — `fixtures/**/*.bas` + expected output, deterministic
   `Host`.
3. **Differential vs PC-BASIC** — same programs through `pcbasic`; normalized
   comparison; floats tolerance-compared; `xfail` registry for known
   divergences.
4. **Conformance corpus** — examples derived from the GW-BASIC User's Guide.
5. **CI** — `clang-format` + `clang-tidy` + tests, plus a job installing `pcbasic`.
6. *(later)* `libFuzzer` on lexer/parser and the `.BAS` decoder.

---

## 7. Milestones

| # | Deliverable | Status |
|---|---|---|
| M0 | Workspace skeleton, `Screen` interface + ANSI/headless backends, `lcbasic file.bas` runs trivial `PRINT`, CI | ✅ |
| M1 | Lexer/parser/line store, direct-mode REPL, `LIST`/`NEW`/`RUN`, expressions, core control flow | ✅ |
| M2 | Types, arrays (`DIM`/`OPTION BASE`/`DEF*`), strings, `DEF FN`, `DATA`/`READ`/`RESTORE` | ✅ |
| M3 | File I/O + DOS commands, error handling (`ON ERROR`/`RESUME`), event trapping | ✅ |
| M4 | Tokenized `.BAS` read + write (incl. protected) | ✅ |
| M5 | `lcbasic-debug` library + `lcbasic-dap` | ⏳ |
| M6 | Differential hardening, `xfail` registry, conformance corpus | ⏳ |
| M7 | GPU screen backend, sound, time-travel debugging | ⏳ |

---

## 8. Risks

- **"Like" fidelity vs differential oracle** — scope the corpus to
  fidelity-stable features; normalize floats; maintain `xfail` registry.
- **`PRINT` formatting quirks** — isolated in one module with a test suite.
- **Event trapping / input nondeterminism** — virtualize via `Host`.
- **DAP scope creep** — land a documented subset first.

## 9. Assumptions

- MIT license (compatible with referencing the open-sourced GW-BASIC sources).
- C++ toolchain (C++17 or later); terminal library (e.g., ncurses or a custom ANSI backend) for terminal I/O.
- Binary/command name is `lcbasic`.
