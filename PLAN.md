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
| Program files | Plain-text `.BAS` read/write (tokenized binary support dropped) |
| Testing | Unit + golden fixtures + **differential vs PC-BASIC**, with GitHub Actions CI |
| Time-travel | Deferred; VM designed for cheap snapshots, rewind implemented later |
| Layout | CMake build, binary name `lcbasic` |

---

## 1. Architecture / crate layout

```
lcbasic/
  CMakeLists.txt            # build
  src/
    lcbasic_core/           # values, lexer, parser, compiler, VM, errors
    lcbasic_screen/         # ScreenDevice impls: ANSI terminal + headless
    lcbasic_debug/          # debugger library: breakpoints, stepping, eval, stack
    lcbasic_dap/            # Debug Adapter Protocol server over lcbasic_debug
    lcbasic_cli/            # `lcbasic` binary: file runner + direct-mode REPL
  tests/
    differential/           # differential harness + corpus vs PC-BASIC
  fixtures/                 # *.bas + expected output
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

## 4. Text `.BAS` I/O (`program_loader`)

Load plain-text `.BAS` files (line-numbered source), parse them into a
`Program`, and save programs back as text. Tokenized binary support
(including protected files) was dropped.

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
| M4 | Plain-text `.BAS` read + write (tokenized support dropped) | ✅ |
| M5 | `lcbasic-debug` library + `lcbasic-dap` | ✅ |
| M6 | Differential hardening, `xfail` registry, conformance corpus | ✅ |
| M7 | GPU screen backend, sound, time-travel debugging | ⏳ |

Notes:
- DOS commands (`CHDIR`/`MKDIR`/`RMDIR`/`KILL`/`NAME`/`FILES`/`SHELL`) are
  implemented via `Host` using `std::filesystem`.
- Event trapping: `ON KEY/TIMER/PLAY/COM/PEN/STRIG ... GOSUB` and the
  `... ON/OFF/STOP` controls are parsed and stored. **TIMER** events fire
  at line boundaries using the `Host` clock; KEY/PLAY/COM/PEN/STRIG
  handlers are armed but not fired (no real event source yet — planned to
  be virtualised through `Host`).
- Logical/bitwise operators `AND`/`OR`/`NOT`/`XOR`/`EQV`/`IMP` are
  implemented on 16-bit integers (true = -1), with GW-BASIC precedence.
- Arithmetic: `^` (left-associative), `\` integer division, `MOD`.
- Lexer is case-insensitive for keywords and variables; `'` apostrophe
  comments and `?` (PRINT) are supported.
- `DIM` bounds may be expressions; `DATA` accepts unquoted strings and
  negative numbers; `RESTORE <line>` is supported.
- `INPUT`/`LINE INPUT` support array elements and re-prompt on bad input;
  `INPUT #` / `LINE INPUT #` / `WRITE #` support multiple values;
  `PRINT #` writes separators to the file.
- `STOP` prints "Break in N" and is resumable with `CONT`; `SWAP` works on
  scalars and array elements; re-DIM raises "Duplicate definition".
- Built-ins also include `INKEY$`, `DATE$`, `TIME$`, `INPUT$`.
- KEY events fire when a key is available; COM/PEN/STRIG remain armed
  (no event source).
- Real-program compatibility (verified with the classic `startrek.bas`):
  `DIM` accepts multiple arrays; `LET` assigns array elements; numbers may
  start with `.` (`.98`); nested `IF ... THEN IF ...`; `INPUT PROMPT
  "text" : var` (ANSI form); `THEN` clauses may contain `RETURN`,
  `ON ... GOTO/GOSUB`, and array assignments; REM-only lines remain valid
  `GOTO` targets. Variable indices are 16-bit (programs may have >256
  variables) and the constant pool is 32-bit.
- `IF ... THEN ...` follows GW-BASIC's "rest of the line" rule: statements
  after `THEN`/`ELSE`, including colon-separated ones, are conditional.
  `THEN` clauses support `GOSUB`, `RETURN`, `ON ... GOTO`, `CLS`, `LOCATE`,
  nested `IF`, and assignments.
- `PRINT USING` supports `#`/`.` numeric fields and `\ ... \` string fields
  (plus `!` and `&`); `EOF(n)` is available for file loops.
- DOS-style paths (`\`, drive letters) are translated to POSIX paths.
- The screen is flushed at line boundaries so interactive `INKEY$` loops
  display their output. `INKEY$` maps Enter to CR (13), backspace to 8,
  and arrow keys to 130 (up) / 136 (down) / 139 (left) / 140 (right).
- Screens handle `\n`/`\r` explicitly: ncurses' `waddch('\n')` clears the rest
  of the line, which used to erase text drawn to the right (e.g. form
  fields). Newlines now just advance to the next row.
- `LINE INPUT #` strips the CR from CRLF text files, and `POS`/`CSRLIN`
  are 1-based like GW-BASIC.
- Random-access files: `OPEN ... FOR RANDOM [LEN=]`, `FIELD`, `LSET`/`RSET`,
  `GET`/`PUT`, `CLOSE`.
- Both `OPEN` syntaxes are supported: `OPEN file FOR mode AS #n [LEN=]` and
  the legacy `OPEN mode, [#]n, file [, reclen]` with `"I"/"O"/"A"/"R"`
  (or `"INPUT"/"OUTPUT"/"APPEND"/"RANDOM"`). `CLOSE [#]n` and bare
  `CLOSE` (close all) work; `LOF`, `LOC`, `FREEFILE`, and
  `CVI`/`CVS`/`CVD`/`MKI$`/`MKS$`/`MKD$` are implemented. File numbers
  may be expressions (`PRINT #N`, `CLOSE N`, `OPEN ... AS #N`).
- Built-in functions: numeric (`ABS`…`CDBL`, `RND`), string (`LEN`…`OCT$`),
  and `TAB`/`SPC`/`FRE`/`POS`/`CSRLIN`.
- Graphics (`PSET`/`LINE`/`CIRCLE`/…) and sound (`SOUND`/`BEEP`/`PLAY`)
  statements parse as no-ops pending a later milestone.
- Tokenized binary `.BAS` support was dropped; only plain text is read/written.

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
