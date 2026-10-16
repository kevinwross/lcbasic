#!/usr/bin/env python3
"""Differential test harness: compares lcbasic against PC-BASIC.

For each .bas program in the corpus, runs it through both interpreters and
compares the normalized output. Known divergences are listed in xfail.txt.

Usage:
    difftest.py [--lcbasic PATH] [--pcbasic PATH] [--corpus DIR]
                [--verbose] [--update-xfail]

Exit status is 0 if every program passes (or fails as expected), 1 otherwise.
"""

import argparse
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_CORPUS = os.path.join(HERE, "corpus")
DEFAULT_XFAIL = os.path.join(HERE, "xfail.txt")


def run_lcbasic(lcbasic, program, cwd):
    """Run a program through lcbasic in headless mode."""
    try:
        proc = subprocess.run(
            [lcbasic, "--headless", program],
            capture_output=True, text=True, timeout=15, cwd=cwd,
        )
        return proc.stdout, proc.stderr, proc.returncode
    except subprocess.TimeoutExpired:
        return "", "TIMEOUT", 124


def run_pcbasic(pcbasic, program, cwd):
    """Run a program through PC-BASIC capturing screen output to stdout."""
    try:
        proc = subprocess.run(
            [pcbasic, "--interface=none", "--quit", "--output=STDIO:", program],
            capture_output=True, text=True, timeout=30, cwd=cwd,
        )
        return proc.stdout, proc.stderr, proc.returncode
    except subprocess.TimeoutExpired:
        return "", "TIMEOUT", 124


def normalize(text):
    """Normalize line endings and strip trailing whitespace per line."""
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    lines = [line.rstrip() for line in text.split("\n")]
    # Drop trailing blank lines.
    while lines and lines[-1] == "":
        lines.pop()
    return "\n".join(lines)


_NUMBER_RE = re.compile(r"-?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?")


def numeric_normalize(text):
    """Round numeric tokens to 5 significant digits so that formatting-only
    differences (precision, exponent vs fixed) compare equal."""

    def repl(m):
        tok = m.group(0)
        try:
            v = float(tok)
        except ValueError:
            return tok
        if v == 0:
            return "0"
        return "%.5g" % v

    return _NUMBER_RE.sub(repl, text)


def load_xfail(path):
    """Load the xfail registry: lines of 'name  # reason'."""
    xfail = {}
    if not os.path.exists(path):
        return xfail
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            name = line.split("#", 1)[0].strip()
            reason = line.split("#", 1)[1].strip() if "#" in line else ""
            xfail[name] = reason
    return xfail


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lcbasic", default=os.path.join(HERE, "..", "..", "build", "bin", "lcbasic"))
    ap.add_argument("--pcbasic", default="pcbasic")
    ap.add_argument("--corpus", default=DEFAULT_CORPUS)
    ap.add_argument("--xfail", default=DEFAULT_XFAIL)
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    lcbasic = os.path.abspath(args.lcbasic)
    if not os.path.exists(lcbasic):
        print(f"error: lcbasic binary not found at {lcbasic}", file=sys.stderr)
        return 1

    xfail = load_xfail(args.xfail)

    programs = sorted(
        f for f in os.listdir(args.corpus) if f.endswith(".bas")
    )
    if not programs:
        print(f"no .bas programs found in {args.corpus}", file=sys.stderr)
        return 1

    n_pass = n_fail = n_xfail = n_xpass = 0
    failures = []

    for name in programs:
        path = os.path.join(args.corpus, name)
        stem = name[:-4]

        lc_out, lc_err, _ = run_lcbasic(lcbasic, path, args.corpus)
        pc_out, pc_err, _ = run_pcbasic(args.pcbasic, path, args.corpus)

        lc_norm = numeric_normalize(normalize(lc_out))
        pc_norm = numeric_normalize(normalize(pc_out))

        matched = lc_norm == pc_norm
        expected_fail = stem in xfail

        if matched and expected_fail:
            n_xpass += 1
            status = "XPASS"
        elif matched:
            n_pass += 1
            status = "PASS"
        elif expected_fail:
            n_xfail += 1
            status = "XFAIL"
        else:
            n_fail += 1
            status = "FAIL"
            failures.append((name, lc_norm, pc_norm))

        if args.verbose or status in ("FAIL", "XPASS"):
            print(f"[{status}] {name}")
        if status in ("FAIL", "XPASS") and (args.verbose or status == "FAIL"):
            print(f"    lcbasic: {lc_norm!r}")
            print(f"    pcbasic: {pc_norm!r}")

    print()
    print(f"{len(programs)} programs: "
          f"{n_pass} pass, {n_fail} fail, {n_xfail} xfail, {n_xpass} xpass")

    if n_fail:
        print("\nFailing programs (not in xfail registry):")
        for name, lc, pc in failures:
            print(f"  {name}")
            print(f"    lcbasic: {lc!r}")
            print(f"    pcbasic: {pc!r}")

    return 1 if (n_fail or n_xpass) else 0


if __name__ == "__main__":
    sys.exit(main())
