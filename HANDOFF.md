# lcbasic — Handoff

_Last updated: 2026-10-18_

This file is the starting point for continuing work on a new computer.

## TL;DR

- **Project:** `lcbasic` — a GW-BASIC-compatible terminal BASIC interpreter in
  C++20 (CMake build, ncurses screen, custom test harness).
- **Repository:** <https://github.com/kevinwross/lcbasic>
- **Branch:** `master` (default), tracking `origin/master`
- **Latest code commit before this handoff:** `d2b769c Add bundled sample
  programs and data` (the handoff commit itself follows it)
- **Tests:** 168 unit tests + 44 differential programs vs PC-BASIC, all passing.
- **Milestones:** M0–M6 complete. M7 (GPU screen backend, sound, time-travel
  debugging) is intentionally deferred.
- **Next recommended task:** number formatting fidelity (single vs double
  precision — see §6).
- **GitHub credentials are NOT stored in the repository.** See §9 to set them up
  on the new machine.

---

## 1. What this project is

`lcbasic` runs classic GW-BASIC programs in a terminal. It implements:

- The GW-BASIC language: line-numbered programs, direct mode, expressions,
  control flow, arrays, strings, `DATA`/`READ`/`RESTORE`, `DEF FN`, error
  handling (`ON ERROR`/`RESUME`), event trapping.
- Sequential and random-access file I/O.
- A curses full-screen display and a headless/teletype mode.
- A debugger library and a minimal DAP server (`lcbasic-dap`) for editor
  integration.

It is **text-mode only**. Graphics and sound statements parse as no-ops and are
deferred to M7. Tokenized `.BAS` support was deliberately dropped; only plain
text is read/written.

---

## 2. Quick start on the new machine

Dependencies: CMake, a C++20 compiler (GCC/Clang), ncurses development headers,
Python 3, and PC-BASIC for the differential tests.

```bash
sudo apt-get update
sudo apt-get install -y libncurses-dev cmake g++ python3-pip
pip install pcbasic

git clone https://github.com/kevinwross/lcbasic.git
cd lcbasic

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4

# Unit tests
ctest --test-dir build --output-on-failure
# (or run the binary directly)
./build/bin/lcbasic_tests

# Differential tests vs PC-BASIC
python3 tests/differential/difftest.py
```

Binaries:

- `build/bin/lcbasic` — interpreter / REPL
- `build/bin/lcbasic-dap` — DAP debug adapter
- `build/bin/lcbasic_tests` — unit tests

There is also a convenience `Makefile` that builds into `build-make/`
(`make all`, `make test`, `make clean`). CMake is the canonical build.

### Running programs

```bash
./build/bin/lcbasic startrek.bas              # full-screen (ncurses)
./build/bin/lcbasic FamousBirthdays.bas        # run from the repo root
./build/bin/lcbasic --headless prog.bas        # teletype / redirected I/O
./build/bin/lcbasic                            # direct-mode REPL
```

`startrek.bas` / `FamousBirthdays.bas` are interactive; feed keys from a
terminal. Pipeline input also works for simple sequences, e.g.
`printf 'XXX\nNO\n' | ./build/bin/lcbasic --headless startrek.bas`.

---

## 3. Repository state

- `master` is clean and in sync with `origin/master`.
- CI: `.github/workflows/ci.yml` builds with CMake on Ubuntu, runs `ctest`,
  installs PC-BASIC with `pip`, and runs the differential suite.
- Bundled samples: `startrek.bas`, `FamousBirthdays.bas`,
  `FamousBirthdays.dat`, and `samples/FamousBirthdays.{bas,dat}`.
- `.gitignore` still ignores stray `*.bas` files; the bundled samples are
  explicitly un-ignored.

---

## 4. What is implemented (highlights)

### Language / VM

- Case-insensitive keywords and variables; `'` comments; `?` = `PRINT`.
- Operators: `^` (left-associative, matching GW-BASIC: `2^3^2 = 64`), `\`,
  `MOD`, `AND`/`OR`/`NOT`/`XOR`/`EQV`/`IMP` on 16-bit integers (true = -1).
- `IF ... THEN ...` follows GW-BASIC's **rest-of-line** rule: everything after
  `THEN`/`ELSE`, including colon-separated statements, is conditional.
  `THEN` clauses support `GOSUB`, `RETURN`, `ON ... GOTO`, `CLS`, `LOCATE`,
  nested `IF`, and assignments.
- `DIM` accepts multiple arrays and expression bounds; `OPTION BASE`; `ERASE`;
  `CLEAR`; re-`DIM` raises error 10.
- `DATA` accepts unquoted strings and negative numbers; `RESTORE <line>`.
- `DEF FN` uses per-call parameter frames (`FnBind`/`FnRet`), so it is
  recursion-safe.
- `INPUT`/`LINE INPUT` support array elements and re-prompt on bad input;
  ANSI `INPUT PROMPT "text" : var` is supported.
- `STOP` prints `Break in N`; the REPL keeps the VM alive and `CONT` resumes.
- `SWAP` works on scalars and array elements (untyped temp slots).
- `RANDOMIZE [seed]`; `INKEY$`, `DATE$`, `TIME$`, `INPUT$` (keyboard and file).
- `PRINT USING` supports `#`/`.` numeric fields and `\ ... \` string fields
  (plus `!` and `&`).
- `EOF(n)`, `LOF(n)`, `LOC(n)`, `FREEFILE`.
- `CVI`/`CVS`/`CVD` and `MKI$`/`MKS$`/`MKD$` (2/4/8-byte little-endian).
- Variable indices are 16-bit and the constant pool is 32-bit, so programs
  with >256 variables or >256 constants work.
- `POS(0)` and `CSRLIN` are 1-based, like GW-BASIC.

### File I/O

Both `OPEN` forms are supported:

```basic
OPEN file$ FOR INPUT|OUTPUT|APPEND|RANDOM [LEN=reclen] AS #n
OPEN "I"|"O"|"A"|"R", [#]n, file$ [, reclen]
```

Long mode names (`"INPUT"`, `"OUTPUT"`, `"APPEND"`, `"RANDOM"`) are accepted.
File numbers may be expressions (`PRINT #N`, `CLOSE N`, `OPEN ... AS #N`).
`CLOSE [#]n` and bare `CLOSE` (close all) work. `PRINT #` writes separators to
the file; `INPUT #`/`LINE INPUT #`/`WRITE #` support multiple values and array
elements.

Random files: `FIELD`, `LSET`/`RSET`, `GET`/`PUT`, `LOF`/`LOC`.

**Known limitation:** `FIELD`, `GET`, and `PUT` still require a **literal**
file number (the other file statements accept expressions).

### Screen / terminal

- Two backends: ncurses (`AnsiScreen`) and teletype (`StdoutScreen`); plus
  `HeadlessScreen` for tests.
- Screens handle `\n`/`\r` themselves. This matters: ncurses'
  `waddch('\n')` clears the rest of the line and used to erase form fields.
- The screen is flushed at line boundaries so `INKEY$` loops display output.
- `INKEY$` maps Enter → CR (13), backspace → 8, arrows → 130/136/139/140, with
  a CSI/SS3 escape-sequence fallback.
- DOS-style paths (`\`, drive letters) are translated to POSIX.
- `LINE INPUT #` strips the CR from CRLF text files.

### Debugger / DAP

- `lcbasic_debug` has breakpoints, stepping, disassembly, expression
  evaluation, and snapshots.
- `lcbasic_dap` implements a minimal but working DAP subset.

---

## 5. Architecture / where to look

```
src/
  lcbasic_core/
    lexer.{h,cpp}          tokenizer (case-insensitive, one physical line at a time)
    parser.{h,cpp}         AST + recursive-descent / precedence-climbing parser
    compiler.{h,cpp}       AST -> bytecode; line table, fixups, DATA pool,
                           DEF FN descriptors
    vm.{h,cpp}             bytecode VM, builtins dispatch, file handles,
                           random files, events, stepping hooks
    host.{h,cpp}           Host interface + RealHost (files, clock, RND, input)
    value.{h,cpp}          Value, VarType, ArrayStore, DataPool, coercion
    builtins.{h,cpp}       builtin function table
    number_format.{h,cpp}  PRINT number formatting (see §6)
    program_loader.{h,cpp} plain-text .BAS loading
    screen.h               Screen interface
  lcbasic_screen/
    ansi_screen.{h,cpp}    ncurses backend
    stdout_screen.{h,cpp}  teletype backend
    headless_screen.{h,cpp} in-memory grid for tests
  lcbasic_cli/main.cpp     CLI, REPL, RUN/SAVE/LOAD/LIST/NEW, --headless
  lcbasic_debug/           debugger library
  lcbasic_dap/             DAP server

tests/
  test_runner.h            tiny custom test harness (no Catch2 dependency)
  e2e_util.h               compile+run helpers (HeadlessScreen + RealHost)
  test_*.cpp               unit/e2e tests
  differential/difftest.py differential harness vs PC-BASIC
  differential/corpus/     .bas programs
  differential/xfail.txt   known divergences

PLAN.md                    project plan, milestones, notes
HANDOFF.md                 this file
```

Key invariants / conventions:

- True is **-1**; false is 0.
- Integer coercion **rounds** (GW-BASIC behavior), it does not truncate.
- `.BAS` I/O is text-only.
- String literals preserve case; identifiers/keywords are upper-cased by the
  lexer.
- Error codes in use include 3, 5, 6, 9, 10, 11, 13, 52, 53, 54, 67, 76.

---

## 6. Known limitations and next work (prioritized)

### 6.1 Number formatting fidelity — **recommended next**

GW-BASIC has **single** (7 significant digits) and **double** (up to 16)
precision. `lcbasic` currently stores every number as a C++ `double` and
formats fixed-point values with **7 decimal places** instead of 7 significant
digits. Examples (PC-BASIC vs current lcbasic):

| Program | PC-BASIC | lcbasic |
|---|---|---|
| `PRINT 10 / 3` | ` 3.333333 ` | ` 3.3333333 ` |
| `PRINT 1 / 3` | ` .3333334 ` | ` .3333333 ` |
| `PRINT 1E+08` | ` 1E+08 ` | ` 100000000 ` |
| `PRINT 1.2345678` | ` 1.2345678 ` | ` 1.2345678 ` (ok by luck) |
| `PRINT 0.00012345678` | ` .00012345678 ` | ` .0001235 ` |

Notes:

- Numeric literals with ≤7 digits are single; >7 digits, a `#` suffix, or a
  `D` exponent are double. Variables default to single unless `DEFDBL`/`#`.
- Proper fix: add a precision tag to `Value` (single/double), propagate it
  through literals, `StoreVar`/arrays, and arithmetic (`float` rounding for
  single operations), then make `format_basic_number` use 7 vs 16 significant
  digits and the right scientific-notation thresholds.
- The differential harness (`tests/differential/difftest.py`) **rounds numbers
  to 5 significant digits** (`numeric_normalize`), so it does not catch
  formatting-only differences. Add exact unit tests for formatting.
- A pragmatic partial fix (7 significant digits for non-integers) improves the
  common case but cannot distinguish `1E+08` (single) from `100000000`
  (double). Prefer the tagged-value approach.

### 6.2 REPL / editor commands

Not implemented: `RENUM`, `DELETE`, `EDIT`, `AUTO`, and `RUN <line>`.
(`LIST`, `SAVE`, `LOAD`, `NEW`, `RUN`, `CONT` already work.)

### 6.3 DAP / debugger polish

- Conditional breakpoints.
- `setVariable`.
- Real `pause`/`continue` semantics.
- Stack frames and variable scopes.
- (Basic breakpoints, stepping, evaluate, disassembly already work.)

### 6.4 Remaining low / machine-specific GW-BASIC features

`PEEK`/`POKE`/`INP`/`OUT`/`WAIT`/`CALL`/`USR`, `ENVIRON$`/`COMMAND$`,
`KEY n,"..."`, `MERGE`, `TRON`/`TROFF`, `FOR ... BINARY`, and expression file
numbers in `FIELD`/`GET`/`PUT`.

### 6.5 Fidelity approximations

- `TAB`/`SPC` are approximate.
- `PRINT` line wrapping is not emulated (PC-BASIC's STDIO output doesn't wrap
  either, so the differential tests pass).
- `RND` uses `std::mt19937`; its sequence differs from GW-BASIC.
- COM/PEN/STRIG events are armed but never fire (no event source); KEY and
  TIMER do fire.

### 6.6 M7 (deferred)

GPU/full-screen graphics backend, sound (`PLAY`/`SOUND`/`BEEP`), and
time-travel debugging. Graphics/sound statements currently parse as no-ops.

---

## 7. Testing

- **Unit tests:** `./build/bin/lcbasic_tests` (168 passing at handoff), or
  `ctest --test-dir build`.
- **Differential tests:** `python3 tests/differential/difftest.py`
  (44 programs passing at handoff). It runs each corpus program with lcbasic
  and PC-BASIC (`pcbasic --interface=none --quit --output=STDIO:`) and compares
  normalized output.
- Add new tests in `tests/test_extensions.cpp` (or a new `test_*.cpp` and add
  it to `tests/CMakeLists.txt` and the `Makefile`).
- Add differential programs under `tests/differential/corpus/`; put known
  divergences in `tests/differential/xfail.txt`.

---

## 8. Bundled sample programs

- `startrek.bas` — classic Super Star Trek (ANSI-adapted). Run
  `./build/bin/lcbasic startrek.bas`. Type `XXX` to resign, then `NO` to
  decline a new mission.
- `FamousBirthdays.bas` — form-based database demo. **Run from the repository
  root** (the program opens `samples\FamousBirthdays.dat`):
  `./build/bin/lcbasic FamousBirthdays.bas`.
  Menu: `1` add, `2` edit, `3` delete, `4`/`5`/`6` reports, `7` quit.
  In the form: arrows move between fields, Enter accepts, Esc cancels.
- `samples/FamousBirthdays.bas` and `samples/FamousBirthdays.dat` are the same
  program/data used by the path above.

If you run the samples from a different directory, make sure the working
directory still resolves `samples/FamousBirthdays.dat`.

---

## 9. Git / GitHub credentials on the new machine

The repository is already pushed. The GitHub token used on the old machine was
stored **outside the repository** in `~/.git-credentials` with
`git config --global credential.helper store` (plaintext, mode 600). It is
**not** in any commit and must not be committed.

On the new machine, choose one:

```bash
# Option A: HTTPS + token in the credential store
git config --global credential.helper store
printf 'protocol=https\nhost=github.com\nusername=kevinwross\npassword=YOUR_NEW_TOKEN\n\n' \
  | git credential approve
chmod 600 ~/.git-credentials

# Option B: GitHub CLI
gh auth login

# Option C: SSH (add your key to GitHub, then use the SSH remote)
git remote set-url origin git@github.com:kevinwross/lcbasic.git
```

If the old token was exposed, revoke/rotate it on GitHub before reusing it.

Remote:

```
origin  https://github.com/kevinwross/lcbasic.git
```

---

## 10. Resume checklist

1. Clone the repo and install the dependencies (§2).
2. Configure GitHub credentials (§9).
3. Build and run both test suites; confirm 168 unit tests and 44 differential
   programs pass.
4. Read `PLAN.md` (milestones and notes) and this file.
5. Start with **number formatting fidelity** (§6.1), then REPL editing (§6.2),
   then DAP polish (§6.3).
6. When done: run the tests, commit with a descriptive message, and push to
   `origin master`.
