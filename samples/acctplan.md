# acctplan.md — a complete implementation plan for `samples/accounting.bas`

Status: describes the program as committed at **83f2bb0** (the date-validation commit).
Reference implementation: [`samples/accounting.bas`](samples/accounting.bas) in this repository.
This document is written so another AI, on another machine, can rebuild the program from
scratch (or audit an existing implementation) without this conversation.

---

## 1. What the program is

A single-file GW-BASIC-style accounting sample for the `lwbasic` interpreter. It keeps a
double-entry journal in memory and in a small pipe-delimited text file, and provides:

- a **journal entry** screen (any two accounts, one debit and one credit);
- a **billing** screen (bill a customer: debit A/R, credit a chosen income account);
- an **accept payment** screen (debit cash, credit A/R, against an open invoice);
- four reports: **journal**, **trial balance**, **income statement**, **balance sheet**.

Money is formatted with `PRINT USING`; input forms are laid out with `LOCATE`; the date
fields are pre-filled with today and edited with a tiny `INKEY$` line editor.

The program is a sample, not a general ledger: one debit line and one credit line per
entry, no editing/deleting, no closing entries, no sales-tax automation, no multi-currency.

---

## 2. Environment, build and run

The program runs under `lwbasic` from this repository.

Build (from the repository root):

```powershell
cmake -S . -B out -G "Visual Studio 18 2026" -A x64
cmake --build out --config Release --parallel
```

If that generator is not installed, run `cmake --help` and use the Visual Studio generator
it lists (the build needs the C++ x64 workload). `cmake` is not always on `PATH`; Visual
Studio ships one under
`...\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`. The repository's
`tools\verify.ps1 -NoRetry` builds and runs `ctest`; the accounting sample is interpreted,
so it does not affect the compiled test suite.

Run, **from the repository root** (the data path is relative):

```powershell
out\src\lwbasic-cli\Release\lwbasic.exe samples\accounting.bas
```

Useful switches (see `--help`):

| Switch | Meaning |
|---|---|
| `--cli` | Console backend instead of the default D3D11 window; requires a real console (conhost/Windows Terminal). |
| `--keys KEYS` | Feed typed input to the windowed terminal; `\r`-style CR ends a line. Used for scripted verification. |
| `--gpu-shot FILE` | After the run, write the screen as a binary PPM (640×400). |

The screen is **80 columns × 25 rows**. `LOCATE row,column` is 1-based. Row 25 is the
bottom. The program uses rows 1–24.

The data file is `samples\accounting.dat` relative to the working directory, created on
first run, and ignored by git ([`.gitignore`](.gitignore) has `samples/accounting.dat`).

If the program is run from another directory, `samples\` may not exist and the file open
fails. Document "run from the repo root" to users.

---

## 3. Dialect rules and constraints

Target the GW-BASIC subset implemented by `lwbasic`:

- Every line is numbered; line numbers ascend; blank lines are allowed.
- Arrays: `DIM`, numeric and string; `A(1)`, `NM$(3)`.
- Control flow: `GOTO`, `GOSUB`/`RETURN`, `FOR`/`NEXT`, single-line `IF ... THEN`.
  `ELSE` exists but this program avoids it for simplicity.
- Strings: `+` concatenates, `MID$`, `LEFT$`, `LEN`, `INSTR`, `SPACE$`, `STRING$`,
  `CHR$`, `ASC`, `STR$`, `VAL`.
- Screens: `CLS`, `LOCATE`, `PRINT`, `PRINT USING`, `TAB`, `INKEY$`.
- Files: `OPEN path FOR APPEND AS #1`, `OPEN path FOR INPUT AS #1`, `CLOSE #1`,
  `LINE INPUT #1, L$`, `PRINT #1, L$`, `EOF(1)`.
- Numbers: `INT`, `MOD`; all arithmetic is host double precision.

Name variables with **at most two significant characters** and never embed a reserved
word (see §13). The reference uses short names throughout.

---

## 4. Program structure and line map

The reference numbering is deliberate: each area has a block, so statements can be added
without renumbering. Keep the blocks if you reimplement; the map also tells you where to
look in the reference.

| Lines | Area | Entry point |
|---|---|---|
| 10–60 | header comments | — |
| 70–98 | `DIM`s, data path, today's date, report format strings | executed at start |
| 99 | read the chart of accounts | `GOSUB 5000` |
| 100 | load the journal | `GOSUB 5100` |
| 110 | main menu | `GOSUB 200` |
| 120 | `END` | — |
| 200–410 | main menu | 200 |
| 1000–1495 | journal entry screen | 1000 |
| 1500–1895 | billing screen | 1500 |
| 2000–2450 | accept-payment screen | 2000 |
| 2600–2800 | trial balance | 2600 |
| 2900–3200 | income statement | 2900 |
| 3250–3730 | balance sheet | 3250 |
| 3750–3950 | journal report | 3750 |
| 4000–4050 | find account by number | 4000 |
| 4100–4150 | find billing by invoice number | 4100 |
| 4200–4250 | total payments against an invoice | 4200 |
| 4400–4450 | split a pipe-delimited line | 4400 |
| 5000–5060 | read the chart from `DATA` | 5000 |
| 5100–5260 | create/load the data file | 5100 |
| 5300–5350 | append one entry | 5300 |
| 6000–6080 | compute account balances | 6000 |
| 7000–7030 | show the message on the error line | 7000 |
| 7200–7220 | clear the error line | 7200 |
| 7600–7750 | pre-filled field editor (`INKEY$`) | 7600 |
| 7800–7980 | date validator | 7800 |
| 8000–8040 | "press enter" pause | 8000 |
| 9000–9140 | chart `DATA` | — |

Execution starts at the lowest line (10), runs the setup, loads the chart and journal,
enters the menu, and ends at `120 END` when the menu returns (`8 QUIT`). Everything else
is reached by `GOSUB`/`GOTO`. Do not fall off the end of a screen routine into the next
block; every screen ends with `RETURN`.

---

## 5. Data model

### Chart of accounts (in memory)

| Array | Meaning |
|---|---|
| `AC(20)` | account number, e.g. 1000 |
| `NM$(20)` | account name, uppercase |
| `TY(20)` | type code: 1 asset, 2 liability, 3 equity, 4 income, 5 expense |
| `BL(20)` | balance, debit-positive (see §10) |
| `NA` | number of accounts (13) |

The chart is loaded from `DATA` at 9000–9140 by `GOSUB 5000`: `READ AN, Q$, TT`, stop when
`AN = 0`, store `AC/ NM$/ TY`. A sentinel `DATA 0,END,0` terminates it.

### Journal (in memory)

| Array | Meaning |
|---|---|
| `JR$(300)` | reference (invoice number, journal reference, or payment's invoice) |
| `JT$(300)` | date, validated MM/DD/YYYY |
| `JX$(300)` | description; for billings it is the customer; for payments `"PAYMENT " + customer` |
| `JD(300)` | debit account number |
| `JC(300)` | credit account number |
| `JM(300)` | amount, rounded to cents |
| `JY(300)` | transaction type: 1 journal, 2 billing, 3 payment |
| `JE` | number of entries (0–300) |

### Other variables

| Variable | Use |
|---|---|
| `DF$` | data file path `"samples\accounting.dat"` |
| `TD$` | today as MM/DD/YYYY (see §9) |
| `FB$`, `FI$`, `FJ$`, `FK$` | `PRINT USING` format strings (§11) |
| `RF$` | reference / invoice number being entered |
| `DT$` | date being entered / just validated |
| `DS$`, `CU$` | description / customer |
| `DA`, `CA`, `IA` | debit account, credit account, income account |
| `DI`, `CI` | chart indexes for the debit/credit accounts |
| `AM` | amount being entered |
| `AN`, `AI` | lookup argument and result (account) |
| `BI` | index of a billing entry |
| `PD`, `OP` | paid total and open balance for an invoice |
| `MSG$` | current error/status message |
| `Z$` | confirmation / pause answers |
| `K$`, `DV$`, `ED`, `R0`, `C0`, `C`, `W` | pre-filled field editor |
| `OK`, `VC`, `MM`, `DD`, `YY`, `MD` | date validator |
| `L$`, `NF`, `P`, `Q`, `F$(10)` | data-file line and split fields |
| `TD`, `TC` | trial-balance debit/credit totals |
| `D`, `C` | one account's debit/credit columns |
| `RV`, `EX`, `NI` | revenue, expenses, net income |
| `TA`, `TL`, `TE`, `V` | balance-sheet totals and one value |
| `TS`, `ROW`, `T$` | journal total, payment list row, type letter |

`INPUT #` splits on commas and, in `lwbasic`, cannot read into array elements. That is why
the journal file is not CSV: it uses `LINE INPUT #` plus a pipe splitter, and the split
fields are converted with `VAL` straight into the arrays (lines 5180–5210). Keep the
format and the splitter together; do not switch one without the other.

---

## 6. Chart of accounts

Exactly these 13 accounts, in this order, types as shown:

| Number | Name | Type |
|---|---|---|
| 1000 | CASH | 1 asset |
| 1100 | ACCOUNTS RECEIVABLE | 1 asset |
| 1200 | SUPPLIES | 1 asset |
| 1500 | EQUIPMENT | 1 asset |
| 2000 | ACCOUNTS PAYABLE | 2 liability |
| 2100 | SALES TAX PAYABLE | 2 liability |
| 3000 | OWNERS EQUITY | 3 equity |
| 3100 | RETAINED EARNINGS | 3 equity |
| 4000 | SERVICE REVENUE | 4 income |
| 5000 | RENT EXPENSE | 5 expense |
| 5100 | SALARIES EXPENSE | 5 expense |
| 5200 | UTILITIES EXPENSE | 5 expense |
| 5300 | SUPPLIES EXPENSE | 5 expense |

Put them in `DATA` exactly as `number,name,type` (unquoted names, no commas inside a
name). The three screens' help text lists them; keep that list in sync.

Sales tax exists as an account but is **not** computed by billing; it is available for
manual journal entries. State this limitation.

---

## 7. The persistent data file

Path: `samples\accounting.dat`, one record per line, fields separated by `|`:

```
type|reference|date|description|debit account|credit account|amount
```

Examples:

```
1|JE-1|10/20/2026|OWNER INVESTMENT|1000|3000|5000
2|INV-1|10/21/2026|ACME CORP|1100|4000|1000
3|INV-1|10/21/2026|PAYMENT ACME CORP|1000|1100|400
```

Rules:

- `type` is 1 (journal), 2 (billing), 3 (payment).
- Text fields must not contain `|`; every screen rejects it with a message.
- Commas, spaces, slashes and quotes are legal in text fields (the pipe is the only
  separator). Descriptions can contain commas because `LINE INPUT` is used.
- Numbers are written without the `STR$` sign space:
  `MID$(STR$(value), 2)` (`250` → `"250"`, `125.5` → `"125.5"`). Negative amounts never
  occur; account numbers are positive.
- The file is created on first run by `OPEN DF$ FOR APPEND AS #1 : CLOSE #1`, then opened
  for input. This is how the program avoids error handling for a missing file.
- Load (`GOSUB 5100`): `LINE INPUT #1, L$`; split on `|` (`GOSUB 4400`); require exactly
  **7** fields. A line with any other field count means the file is not this program's
  format: clear the screen, print
  `THE DATA FILE IS NOT IN THIS PROGRAM'S FORMAT.` /
  `RENAME OR DELETE samples\accounting.dat, THEN RUN AGAIN.`, and `END`.
  This deliberately rejects the earlier CSV format rather than silently misreading it.
- Loading stops at 300 entries; extra lines are ignored (the file is not truncated).
- Save (`GOSUB 5300`): build `L$` by concatenation, `OPEN ... FOR APPEND`, `PRINT #1, L$`,
  `CLOSE #1`. Each posted entry is appended immediately, so a crash loses at most the
  entry being posted.
- There is no rewrite, edit or delete; the only reset is deleting the file.

The split routine (4400):

```
NF = 0 : P = 1
Q = INSTR(P, L$, "|")
IF Q = 0 THEN NF = NF + 1 : F$(NF) = MID$(L$, P) : RETURN
NF = NF + 1 : F$(NF) = MID$(L$, P, Q - P)
P = Q + 1 : GOTO ...
```

This handles empty fields and a trailing separator. Do not use `INPUT #` for this file:
`INPUT #` splits on commas and quoted strings, which is the old format's problem.

---

## 8. The screens

All forms share these conventions:

- `CLS` first; title at row 2.
- Labels at column 5; input fields at column 22 (payment's date is at column 18).
- Fields are cleared with `LOCATE row,col : PRINT SPACE$(n);` before input.
- The error/status line is row 22, cleared before each new field (`GOSUB 7200`) and written
  by `GOSUB 7000`.
- Confirmation prompts are on row 22 (billing/payment) or row 24 (journal); `Enter` posts,
  `N`/`n` cancels.
- Every screen returns to the menu (`RETURN`); the menu redraws.

### 8.1 Main menu (lines 200–410)

```
row 2, col 31  LWBASIC ACCOUNTING
row 4, col 29  1  JOURNAL ENTRY
row 5, col 29  2  BILL A CUSTOMER
row 6, col 29  3  ACCEPT A PAYMENT
row 7, col 29  4  JOURNAL
row 8, col 29  5  TRIAL BALANCE
row 9, col 29  6  INCOME STATEMENT
row 10, col 29 7  BALANCE SHEET
row 11, col 29 8  QUIT
row 13, col 29 CHOICE:
row 13, col 37 input
```

Routing: 1→1000, 2→1500, 3→2000, 4→3750+pause, 5→2600+pause, 6→2900+pause,
7→3250+pause, 8→return. Reports call `GOSUB 8000` after printing, which waits for Enter on
row 24. Invalid choices redraw the menu.

### 8.2 Journal entry (lines 1000–1495)

```
row 4,  col 5 REFERENCE  :   col 22 input
row 5,  col 5 DATE       :   col 22 pre-filled editor
row 6,  col 5 DESCRIPTION:   col 22 LINE INPUT
row 8,  col 5 DEBIT ACCT :   col 22 numeric + name at col 34
row 9,  col 5 CREDIT ACCT:   col 22 numeric + name at col 34
row 10, col 5 AMOUNT     :   col 22 numeric
rows 13–19 chart help (same text as §6)
row 21 BLANK REFERENCE RETURNS TO THE MENU.
```

Flow and validation:

1. **Reference** — `INPUT "", RF$` (empty prompt, no `? `). Blank returns to the menu.
   Reject `|` in it.
2. **Date** — pre-filled editor, then validator (§9). Reject `|` (unreachable after
   validation, kept as defence).
3. **Description** — `LINE INPUT "", DS$` (commas allowed; `INPUT` would split on commas).
   Required, reject `|`.
4. **Debit account** — numeric `INPUT`; set `DA = 0` first so an empty entry cannot reuse a
   previous value. Look up by number (`GOSUB 4000`); on failure
   `ACCOUNT n IS NOT IN THE CHART`; show the name at column 34.
5. **Credit account** — same; must differ from the debit (`DEBIT AND CREDIT MUST BE
   DIFFERENT ACCOUNTS`).
6. **Amount** — set `AM = 0` first; must be > 0; round `AM = INT(AM * 100 + 0.5) / 100`;
   redisplay with `PRINT USING "####,###.##"`.
7. Capacity check `JE >= 300` → `THE JOURNAL IS FULL` and return.
8. Show `DR #####   CR #####   AMOUNT ####,###.##` on row 23; confirm on row 24.
9. Post: `JY=1`, store all fields, `GOSUB 5300`, return.

### 8.3 Billing (lines 1500–1895)

```
row 4, col 5 INVOICE #  :  col 22
row 5, col 5 DATE       :  col 22 pre-filled editor
row 6, col 5 CUSTOMER   :  col 22 LINE INPUT
row 8, col 5 INCOME ACCT:  col 22 numeric, default 4000
row 9, col 5 AMOUNT     :  col 22 numeric
rows 11–12 income account help
row 14 POSTS DR 1100 A/R AND CR THE INCOME ACCOUNT.
```

Flow:

1. Invoice number: required; reject `|`; use `GOSUB 4100` and reject a duplicate
   (`INVOICE x HAS ALREADY BEEN BILLED`).
2. Date: same editor/validator as the journal.
3. Customer: `LINE INPUT`; required; reject `|`.
4. Income account: set `IA = 4000` **before** the numeric `INPUT` so an empty entry keeps
   the default (numeric `INPUT` on an empty field keeps the variable's old value). Look up;
   must exist and `TY(AI) = 4`, else `THAT ACCOUNT IS NOT AN INCOME ACCOUNT`. Show the name.
5. Amount: as journal (positive, cents).
6. Capacity check; summary on row 20: `DR 1100 A/R   CR #####   AMOUNT ####,###.##`;
   confirm on row 22.
7. Post as type 2: debit **1100**, credit the chosen income account, description = customer
   (not `"BILL "`), reference = invoice number.

### 8.4 Accept a payment (lines 2000–2450)

```
row 2, col 28 ACCEPT A PAYMENT
row 3, col 5  OPEN INVOICES
row 4  header: INVOICE (col 3), CUSTOMER (15), BILLED (42), PAID (57), OPEN (70)
row 5  underline
rows 6–13 up to 8 open invoices
row 14 ... MORE OPEN INVOICES (if applicable)
row 16 INVOICE #  :  col 18 input
row 18 OPEN ####,###.##   AMOUNT:   col 33 input
row 19 DATE       :  col 18 pre-filled editor
row 21 summary
row 22 confirm
```

Open-invoice listing:

- Loop all entries; only type 2.
- For each, `RF$ = JR$(J)`, `GOSUB 4200` to sum payments (`PD`), `OP = JM(J) - PD`.
- Skip `OP <= 0.005` (fully paid).
- Print row with `FK$`; if more than 8 open, print the "... MORE" line at row 14.
- If no open invoices: print `(NO OPEN INVOICES)` at row 16, wait for Enter (`INPUT "", Z$`),
  return.

Payment flow:

1. Invoice number: required; blank returns to the menu.
2. `GOSUB 4100`; if not a billing, `INVOICE x IS NOT IN THE LEDGER`, re-prompt.
3. `GOSUB 4200` for payments; `OP = JM(BI) - PD`; if `OP <= 0.005`,
   `INVOICE x IS ALREADY PAID`, re-prompt.
4. Show `OPEN ####,###.##   AMOUNT: ` at row 18, set `AM = OP` before `INPUT` so Enter
   pays the full balance.
5. Validate `AM > 0` and `AM <= OP + 0.005` (`THE PAYMENT EXCEEDS THE OPEN BALANCE`);
   round to cents; redisplay.
6. Date: same editor/validator.
7. Capacity check; summary: `DR 1000 CASH   CR 1100 A/R   AMOUNT ####,###.##`;
   confirm on row 22.
8. Post as type 3: debit **1000**, credit **1100**, reference = invoice number,
   description = `"PAYMENT " + JX$(BI)` (the customer of the billing).

The 0.005 tolerance absorbs cents rounding; keep it.

---

## 9. Dates: pre-fill, editor, validation

### 9.1 Today's date

`DATE$` in `lwbasic` is **MM-DD-YYYY** (see `SystemHost::DateString`). Convert once at
startup:

```
TD$ = MID$(DATE$, 1, 2) + "/" + MID$(DATE$, 4, 2) + "/" + MID$(DATE$, 7, 4)
```

Every date field opens with `DV$ = TD$` in **MM/DD/YYYY**. The report headings still print
`DATE$` (MM-DD-YYYY); this is the current behaviour. If consistency is preferred, print
`TD$` in headings instead — note the deliberate divergence.

### 9.2 The pre-filled editor (7600, requires `INKEY$`)

`INPUT` cannot pre-fill an editable buffer, so the date field is a small editor:

- `R0`,`C0` are the field row/column; `DV$` is the buffer, initialised by the caller.
- Display `DV$`, then 25 spaces (to clear the rest of the field), place the cursor after
  the text.
- Loop on `K$ = INKEY$`; if empty, run `FOR W = 1 TO 300 : NEXT W` to avoid spinning, then
  poll again.
- `CHR$(13)` (Enter): redraw and return.
- `CHR$(8)` (Backspace): delete the last character of `DV$`, mark edited (`ED = 1`).
- Other keys: only single characters with `ASC` in 32–126 are accepted. On the first
  printable character, clear `DV$` (`ED = 1`) so typing replaces the pre-fill; then append.
- Redraw after every keystroke: `LOCATE R0,C0 : PRINT DV$; SPACE$(25); : LOCATE R0,C0+LEN(DV$)`.
  **The extra colon after the semicolon is required** (see §13).
- No arrow keys, no mid-string insert; typing replaces, Backspace edits from the end.

Callers set `DV$` and call `GOSUB 7600`, then copy `DT$ = DV$`.

### 9.3 Validation (7800)

Accept only real dates in **MM/DD/YYYY**, years **1900–2099**. Return `OK = 1` when valid;
otherwise `OK = 0` and `VC = 1` (format) or `VC = 2` (range).

Checks, in order:

1. `LEN(DT$) = 10`.
2. `MID$(DT$,3,1) = "/"` and `MID$(DT$,6,1) = "/"`.
3. Every other character is a digit (`ASC` 48–57).
4. `MM = VAL(MID$(DT$,1,2))`, `DD = VAL(MID$(DT$,4,2))`, `YY = VAL(MID$(DT$,7,4))`.
5. `MM` in 1–12; `YY` in 1900–2099 → else `VC = 2`.
6. Month length: 31 default; 30 for 4,6,9,11; for February start at 28, 29 if
   `YY MOD 4 = 0`, then force 28 if `YY MOD 100 = 0 AND YY MOD 400 <> 0`.
7. `DD` in 1..length → else `VC = 2`.
8. `OK = 1`.

Caller pattern (identical on all three screens):

```
GOSUB 7800 : IF OK = 1 THEN <next field>
IF VC = 1 THEN MSG$ = "USE MM/DD/YYYY (E.G. 10/20/2026)" ELSE MSG$ = "THAT DATE DOES NOT EXIST"
GOSUB 7000 : DV$ = DT$ : GOTO <editor line>
```

The error stays on row 22 while the editor is active; the field reopens with the offending
text so a single wrong digit can be fixed with Backspace. `GOSUB 7200` is called before the
*first* attempt only, not on re-entry, or the message would be erased immediately.

Dates loaded from the data file are **not** re-validated; validation is an entry rule.
Entries written before this rule can contain other formats. Reset means deleting the data
file.

---

## 10. Accounting rules

- **Sign convention:** balances are debit-positive. `BL(I) = sum(debits to I) - sum(credits
  to I)`. Each posted entry has one debit and one credit of equal amount, so the sum of all
  balances is zero.
- **Normal balances:** assets and expenses debit-positive; liabilities, equity and income
  credit-positive. Reports flip the sign where needed:
  - income value = `-BL` for type 4;
  - liability/equity value = `-BL` for types 2 and 3;
  - asset/expense value = `BL` for types 1 and 5.
- **Trial balance:** for each account, `D = BL` if positive else 0; `C = -BL` if negative
  else 0. Totals must be equal.
- **Income statement:** revenue = sum of `-BL` over type 4; expenses = sum of `BL` over
  type 5; net = revenue - expenses. Label `NET INCOME` if ≥ 0, `NET LOSS` if < 0.
- **Balance sheet:** assets = sum `BL` type 1; liabilities = sum `-BL` type 2; equity =
  sum `-BL` type 3; **current earnings** = revenue - expenses (income and expense accounts
  are never closed); total equity includes current earnings; the check is
  `assets = liabilities + equity`.
- **Open invoice:** `paid` = sum of type-3 amounts whose `JR$` equals the invoice number;
  `open` = billing amount - paid. Payments are capped at open. Partial payments are
  expected; multiple payments per invoice are allowed.
- **No invoice/customer master file:** the customer is the billing's description. The
  journal report shows it.

---

## 11. Reports

All reports `CLS`, print a title at row 1, a date line, and a blank line. Money uses
`PRINT USING` with `####,###.##` (width 11, thousands separators, 2 decimals). Values
above 9,999,999.99 overflow and print `%`; document that limit.

### 11.1 Format strings (lines 95–98)

Build them with concatenation so widths are self-documenting:

```
FB$ = "  \" + SPACE$(34) + "\  ####,###.##  ####,###.##"
FI$ = "  \" + SPACE$(34) + "\  ####,###.##"
FJ$ = "  !  \" + SPACE$(8) + "\  \" + SPACE$(8) + "\  \" + SPACE$(20) + "\  ####/####  ####,###.##"
FK$ = "  \" + SPACE$(8) + "\  \" + SPACE$(18) + "\  ####,###.##  ####,###.##  ####,###.##"
```

`USING` rules used here:

- `\` + n spaces + `\` is a string field of width n+2; strings are left-justified and
  right-padded.
- `!` is a one-character string field (used for the J/B/P type letter).
- `####,###.##` is a numeric field of width 11.
- Literal characters between fields (`  `, `/`) are printed as-is; the format repeats for
  a longer value list.

### 11.2 Trial balance (2600)

- Header: `  ACCOUNT` at column 3, `DEBIT` right-aligned to column 51 (`TAB(47)`),
  `CREDIT` right-aligned to column 64 (`TAB(59)`).
- Underline: `"  " + STRING$(36,"-") + "  " + STRING$(11,"-") + "  " + STRING$(11,"-")`.
- One line per account using `FB$` and `NM$(I); D; C`.
- Totals line with `"TOTAL"`.
- If totals differ: `OUT OF BALANCE - CHECK THE JOURNAL.`; else `DEBITS EQUAL CREDITS.`

### 11.3 Income statement (2900)

- Title, `PERIOD ENDING ; DATE$`.
- `REVENUE`, one line per non-zero type-4 account with value `-BL`, then `TOTAL REVENUE`.
- Blank line, `EXPENSES`, one line per non-zero type-5 account with value `BL`, then
  `TOTAL EXPENSES`.
- Blank line, `NET INCOME` or `NET LOSS` with the signed net.
- Uses `FI$`; the amount column ends at column 51.

### 11.4 Balance sheet (3250)

- `ASSETS`: type-1 accounts with `BL`, then `TOTAL ASSETS`.
- `LIABILITIES`: type-2 accounts with `-BL`, then `TOTAL LIABILITIES`.
- `EQUITY`: type-3 accounts with `-BL`; then `CURRENT EARNINGS` (computed as revenue minus
  expenses over types 4 and 5); then `TOTAL EQUITY` (equity accounts + current earnings).
- Blank line, `LIABILITIES + EQUITY`.
- If `assets <> liabilities + equity`: `CHECK: ASSETS DO NOT EQUAL LIABILITIES PLUS
  EQUITY.`; else `BALANCED.`

### 11.5 Journal (3750)

- Header: `T` at 3, `DATE` at 6, `REF` at 18, `DESCRIPTION` at 30, `DR/CR` at 54,
  `AMOUNT` at 65 (`TAB` positions).
- Underline with widths 1/10/10/22/4+1+4/11.
- Rows use `FJ$`: type letter (`J`, `B`, `P` from `JY`), date, reference, description,
  debit account, credit account, amount.
- If no entries: `(NO ENTRIES YET)`.
- After the rows: underline, then
  `  ENTRIES: ###   TOTAL POSTED: ####,###.##` where the total is the sum of all amounts
  (a volume, not a debit/credit total). Say so in the help text if you explain it.

### 11.6 Payment list (inside 2000)

Header via `TAB` as in §8.4; rows use `FK$`: invoice, customer, billed, paid, open.
Open amounts are computed as in §10.

---

## 12. Error messages

All appear on row 22 unless noted. Use these exact texts (they are part of the sample's
behaviour and its tests):

| Message | When |
|---|---|
| `NO | IN THE REFERENCE` | journal ref contains a pipe |
| `USE MM/DD/YYYY (E.G. 10/20/2026)` | date format wrong (VC=1) |
| `THAT DATE DOES NOT EXIST` | date well-formed but impossible (VC=2) |
| `NO | IN THE DATE` | date contains a pipe (defensive; unreachable) |
| `A DESCRIPTION IS REQUIRED` | blank journal description |
| `NO | IN THE DESCRIPTION` | pipe in description |
| `ACCOUNT n IS NOT IN THE CHART` | unknown debit/credit/income account |
| `DEBIT AND CREDIT MUST BE DIFFERENT ACCOUNTS` | same account both sides |
| `THE AMOUNT MUST BE POSITIVE` | amount <= 0 |
| `THE JOURNAL IS FULL` | 300 entries already |
| `INVOICE x HAS ALREADY BEEN BILLED` | duplicate billing reference |
| `A CUSTOMER NAME IS REQUIRED` | blank customer |
| `NO | IN THE CUSTOMER NAME` | pipe in customer |
| `THAT ACCOUNT IS NOT AN INCOME ACCOUNT` | billing credit account type ≠ 4 |
| `INVOICE x IS NOT IN THE LEDGER` | payment references no billing |
| `INVOICE x IS ALREADY PAID` | payment on a settled invoice |
| `THE PAYMENT EXCEEDS THE OPEN BALANCE` | amount > open + 0.005 |
| `(NO OPEN INVOICES)` | payment screen with nothing open |
| `DEBITS EQUAL CREDITS.` | trial balance in balance |
| `OUT OF BALANCE - CHECK THE JOURNAL.` | trial balance not in balance |
| `BALANCED.` / `CHECK: ASSETS DO NOT EQUAL LIABILITIES PLUS EQUITY.` | balance sheet |
| `THE DATA FILE IS NOT IN THIS PROGRAM'S FORMAT.` and `RENAME OR DELETE samples\accounting.dat, THEN RUN AGAIN.` | fatal, at load |
| `PRESS ENTER TO CONTINUE: ` | report pause (row 24) |
| `PRESS ENTER TO POST, OR N TO CANCEL: ` | confirmation |

`GOSUB 7000` clears 70 columns at row 22, writes `MSG$`, and leaves the cursor. `GOSUB 7200`
just clears that line. Call 7200 before starting a field, not when re-prompting after an
error, or the message disappears.

---

## 13. Dialect pitfalls that cost time (read this first)

These are real behaviours of the interpreter; another AI will hit them.

1. **Reserved words inside variable names.** `TOT` is tokenised as `TO` + `T` and produces
   `?Syntax error`. Names such as `TOTAL`, `FORT`, `PRINTX` are unsafe. This program uses
   `TS` instead of `TOT`. Keep names short and avoid embedded keywords.
2. **A colon is required after a trailing `PRINT;` when another statement follows.**
   `PRINT DV$; SPACE$(25); LOCATE ...` is parsed as a PRINT item list containing `LOCATE`
   and is a syntax error. Write `PRINT DV$; SPACE$(25); : LOCATE ...`. The same applies to
   `PRINT "DATE :"; : DV$ = TD$`.
3. **`INPUT` splits on commas.** `INPUT A$` with `SMITH, JOHN` gives `?Redo from start`.
   Use `LINE INPUT "", A$` for free text (descriptions, customer names). `LINE INPUT`
   keeps the whole line, commas included.
4. **Numeric `INPUT` on a blank field keeps the old value.** This is exploited for the
   billing default: set `IA = 4000` before `INPUT "", IA`, and Enter keeps 4000. For amounts
   set `AM = 0` first so Enter cannot silently reuse a previous amount. For account fields
   set `DA = 0`/`CA = 0` first.
5. **`DATE$` is MM-DD-YYYY**, not MM/DD/YYYY. Convert to `TD$` for the fields; the reports
   still use `DATE$`.
6. **`INKEY$` returns `""` when no key is ready.** Poll with a short `FOR` delay; handle
   `CHR$(13)` (Enter) and `CHR$(8)` (Backspace); ignore control and extended keys. The
   windowed backend returns characters one at a time from its message queue.
7. **`MID$(STR$(x),2)` strips the sign space** for positive numbers. `STR$(250)` is
   `" 250"`; the data file must contain `250`, so use MID$.
8. **`PRINT USING` string fields:** `\` + spaces + `\` is width spaces+2; `!` is one
   character; numeric `####,###.##` is 11 wide; overflow prints `%`. Type mismatches are
   `Type mismatch`, not a generic error.
9. **`TAB(n)` moves forward only**; if the target is at or behind the cursor, the next line
   is used. Headers here only tab forward, so they are safe.
10. **`INPUT #` cannot read into array elements** in this interpreter
    (`Unsupported statement: array INPUT #`). Read scalars, then assign arrays.
11. **`OPEN ... FOR APPEND` creates a missing file.** Use that instead of `ON ERROR` for
    first-run creation. There is no error handler; a genuinely unopenable path aborts.
12. **`PRINT #1, L$` writes the string as-is plus CRLF.** Build the pipe record yourself;
    do not use `WRITE #` (it writes CSV-style quotes/commas).
13. **Blank lines in a `.bas` file are ignored** by the loader; line numbers must be unique
    and ascending.
14. **Screen is 80×25.** `LOCATE` is 1-based. Do not print past column 80 or row 25;
    wrapping destroys a form's alignment.
15. **`PRINT USING` string values are padded right** to the field width; numeric values
    right-justified. Plan column positions accordingly.
16. **The format string cycles** over the value list; literal text between fields prints
    once per cycle. Keep exactly one field per value.
17. **No `ELSE` in this program**: the style is `IF cond THEN <line-or-statement>` and a
    separate line for the negative case. `ELSE` does work if you prefer it, but do not mix
    it with the line-number style carelessly.
18. **`END` inside a subroutine** (the file-format guard) terminates the program; ordinary
    screen routines must `RETURN`.
19. **Helpers use their own loop variables.** `4000` uses `I`, `4100`/`4200` use `K`,
    report loops use `I`/`J`, the editor and validator use `W` and `C`. The payment list
    calls `4200` while its own `J` loop is running; if a helper reused `J`, the list would
    skip rows. Keep them distinct.
20. **Account lookup breaks its `FOR` by assigning `I = NA`** inside the loop; `NEXT`
    increments past the bound and the loop ends. This idiom works here; a `WHILE` or a
    flag would be clearer if you rewrite it.

---

## 14. Suggested implementation sequence

1. Build the repo and run the existing sample once. Confirm `out\src\lwbasic-cli\Release\lwbasic.exe`
   and a 0-byte `samples\accounting.dat`.
2. Write the header, `DIM`s, `TD$`, the four format strings, and the chart `DATA`; implement
   `GOSUB 5000` and print the chart to check it.
3. Implement the splitter, loader and saver; test with a hand-made pipe record, a restart,
   and an old-format file (must refuse with the message).
4. Implement the menu with stubs for the three screens and four reports.
5. Implement the journal screen. Reuse the date editor and validator from the start;
   they are shared by all three forms.
6. Implement billing (unique invoice, income-account type check, default 4000).
7. Implement the payment list and posting (open balances, partial payments, cap).
8. Implement 6000 and the trial balance; check with one balanced entry.
9. Implement income statement and balance sheet; verify the accounting equation.
10. Implement the journal report (J/B/P) and the pause routine.
11. Run the acceptance tests in §16; check the exact data file and report figures.
12. Update the help text in the screens if you change the chart.

---

## 15. Scripted verification

Interactive testing is slow; the CLI's `--keys` switch drives the windowed backend with a
string of characters (`\r` is Enter). Run from the repo root. Example harness in PowerShell:

```powershell
$keys = "1`rJE-1`r10/20/2026`rOWNER INVESTMENT`r1000`r3000`r5000`r`r8`r"
$quoted = '"' + $keys + '"'
$p = Start-Process -FilePath "out\src\lwbasic-cli\Release\lwbasic.exe" `
  -ArgumentList '--keys', $quoted, 'samples\accounting.bas' `
  -RedirectStandardOutput out\stdout.txt -RedirectStandardError out\stderr.txt `
  -PassThru -WindowStyle Hidden
$p.WaitForExit(30000)
```

Quoting matters:

- If the key string contains **spaces**, wrap the whole value in double quotes (as above);
  otherwise the CLI sees several arguments and prints its usage.
- For a value that is **only** a carriage return, pass it **without** quotes: an argument
  `"<CR>"` with literal quotes gets typed as two quote characters, and the editor will wait
  for a real Enter.
- Backspace is `[char]8`; build the string with `+ ([string][char]8) * n`.
- The process exits 0 on a clean run; runtime errors are printed on **stdout**, syntax
  errors on **stderr** (they look like `?Syntax error in 1230`).

Use `--gpu-shot out\screen.ppm` to capture the final screen. It is a binary PPM (P6,
640×400, header `P6\n640 400\n255\n`); convert it with .NET `System.Drawing` if you need a
PNG, or decode the 8×16 character cells to inspect text.

### 15.1 Worked example

Start with a deleted data file and enter:

1. Journal: ref `JE-1`, date `10/20/2026`, description `OWNER INVESTMENT`, debit `1000`,
   credit `3000`, amount `5000`.
2. Billing: invoice `INV-1`, date `10/21/2026`, customer `ACME CORP`, income account
   blank (→ 4000), amount `1000`.
3. Payment: invoice `INV-1`, amount `400`, date `10/21/2026`.

Expected file:

```
1|JE-1|10/20/2026|OWNER INVESTMENT|1000|3000|5000
2|INV-1|10/21/2026|ACME CORP|1100|4000|1000
3|INV-1|10/21/2026|PAYMENT ACME CORP|1000|1100|400
```

Expected reports:

- Trial balance: CASH 5,400.00 D; ACCOUNTS RECEIVABLE 600.00 D; OWNERS EQUITY 5,000.00 C;
  SERVICE REVENUE 1,000.00 C; total 6,000.00 each side; "DEBITS EQUAL CREDITS."
- Income statement: SERVICE REVENUE 1,000.00; TOTAL EXPENSES 0.00; NET INCOME 1,000.00.
- Balance sheet: TOTAL ASSETS 6,000.00; TOTAL LIABILITIES 0.00; CURRENT EARNINGS 1,000.00;
  TOTAL EQUITY 6,000.00; LIABILITIES + EQUITY 6,000.00; "BALANCED."
- Journal: three rows with type letters J, B, P; `ENTRIES: 3  TOTAL POSTED: 6,400.00`.

### 15.2 Acceptance matrix

| Case | Expected |
|---|---|
| Empty ledger | journal report `(NO ENTRIES YET)`; all totals 0.00 |
| Journal entry | posts type 1; balances move by the amount |
| Billing | posts type 2; DR 1100, CR chosen income |
| Partial payment | open drops by the payment |
| Full payment (blank amount) | open becomes 0; next payment screen lists no invoice |
| Overpayment | `THE PAYMENT EXCEEDS THE OPEN BALANCE`, no post |
| Unknown invoice | `INVOICE x IS NOT IN THE LEDGER`, no post |
| Duplicate invoice | `INVOICE x HAS ALREADY BEEN BILLED`, no post |
| Same debit/credit | `DEBIT AND CREDIT MUST BE DIFFERENT ACCOUNTS` |
| Unknown account | `ACCOUNT n IS NOT IN THE CHART` |
| Non-income billing account | `THAT ACCOUNT IS NOT AN INCOME ACCOUNT` |
| Amount 0 / negative | `THE AMOUNT MUST BE POSITIVE` |
| Date `02/29/2024` | accepted (leap) |
| Date `02/29/2025`, `02/30/2026`, `04/31/2026` | `THAT DATE DOES NOT EXIST` |
| Date `13/01/2026`, `2/3/2026`, `10-20-2026` | refused with the appropriate message |
| Date by Backspace edit | pre-fill `10/20/2026`, 4 backspaces, `2027` → `10/20/2027` |
| 300 entries | `THE JOURNAL IS FULL`, no post |
| Old CSV file | fatal format message and exit |
| Restart | entries reload in order; reports unchanged |
| `|` in any text field | the field's "NO | ..." message |
| Comma in description/customer | preserved through save/reload |

---

## 16. Definition of done

- `samples/accounting.bas` exists, is numbered, and runs from the repository root with the
  release binary.
- The first run creates an empty `samples/accounting.dat`; the file is ignored by git.
- Menu, journal, billing and payment screens match §8; dates pre-fill today in MM/DD/YYYY,
  can be edited with Backspace or replaced by typing, and are validated as real dates.
- All postings are balanced one-debit/one-credit records; the A = L + E check passes for
  every test ledger.
- Partial and full payments, duplicate invoices, unknown invoices, overpayments, invalid
  dates and capacity limits behave as in §15.2.
- The four reports match §11 and the worked example in §15.1.
- Restarting reloads the journal; an old-format file is refused with the documented
  message rather than misread.
- The sample's comments still say: run from the repo root; dates are MM/DD/YYYY; the data
  format is type|ref|date|description|debit|credit|amount.
- The build is unaffected: `ctest` remains green (the sample is interpreted, not compiled).

---

## 17. Deliberate limits

- One debit line and one credit line per entry; no split postings.
- One customer string per billing; no customer master file, statements or aging buckets.
- No sales-tax computation on billing (2100 exists for manual entries).
- No edit, delete, void or reversing entries; to correct a ledger, reset the file (or add a
  corrective journal entry — the program does not do it for you).
- No closing entries: income and expense balances are shown as "current earnings" every
  time.
- Amounts above 9,999,999.99 overflow the `####,###.##` field.
- Dates are free text at entry time only validated; loaded records are not re-validated.
- The file is a local sample format, not GW-BASIC's random-access file format.

---

## 18. File map quick reference

```
samples/accounting.bas      the program (this plan's subject)
samples/accounting.dat      runtime journal, created on first run, git-ignored
out/                        build output, git-ignored
```

Nothing else is required. The interpreter is built from this repository; third-party
dependencies (Catch2, RapidCheck) are vendored, so no network access is needed to build.
