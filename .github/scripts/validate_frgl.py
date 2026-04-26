#!/usr/bin/env python3
"""Lightweight .frgl validator.

Verifies:
  * file starts with "FRGL v1"
  * every section header is recognised
  * every note line in a difficulty section is structurally valid
  * note times are monotonically non-decreasing
  * lane in 0..3, type in {T,H,B,S,F,X}
  * anomalies and audio rows have the right column count
"""
from __future__ import annotations
import sys, pathlib

VALID_SECTIONS = {"meta", "audio", "easy", "normal", "fun", "nitro",
                  "anomalies", "events", "character"}
NOTE_TYPES = set("THBSFX")


def fail(path, line_no, msg):
    print(f"::error file={path},line={line_no}::{msg}")


def validate(path: pathlib.Path) -> bool:
    text = path.read_text(encoding="utf-8", errors="replace").splitlines()
    if not text or not text[0].startswith("FRGL"):
        fail(path, 1, "missing FRGL magic")
        return False

    section = None
    diff_seen = False
    last_t = -1
    ok = True

    for i, raw in enumerate(text[1:], start=2):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            name = line[1:-1].strip()
            if name not in VALID_SECTIONS:
                fail(path, i, f"unknown section [{name}]")
                ok = False
            section = name
            last_t = -1
            if name in ("easy", "normal", "fun", "nitro"):
                diff_seen = True
            continue
        if section in ("easy", "normal", "fun", "nitro"):
            if line.startswith("diff="):
                continue
            parts = line.split(",")
            if len(parts) < 3:
                fail(path, i, f"note needs >=3 fields, got {len(parts)}: {line!r}")
                ok = False; continue
            try:
                t = int(parts[0]); lane = int(parts[1])
            except ValueError:
                fail(path, i, f"non-integer time/lane: {line!r}")
                ok = False; continue
            if not (0 <= lane <= 3):
                fail(path, i, f"lane out of range 0..3: {lane}")
                ok = False
            ttype = parts[2]
            if ttype not in NOTE_TYPES:
                fail(path, i, f"unknown note type {ttype!r}")
                ok = False
            if t < last_t:
                fail(path, i, f"note time {t} regresses below {last_t}")
                ok = False
            last_t = t
        elif section == "anomalies":
            parts = line.split(",")
            if len(parts) < 4:
                fail(path, i, "anomaly needs threshold,start,end,modifier[,arg]")
                ok = False
        elif section == "audio":
            parts = line.split(",")
            if len(parts) < 3:
                fail(path, i, "audio row needs time,freq,duration[,vol]")
                ok = False
        elif section == "meta":
            if "=" not in line:
                fail(path, i, f"meta needs key=value, got {line!r}")
                ok = False

    if not diff_seen:
        fail(path, 0, "no difficulty section present")
        ok = False
    return ok


def main(argv: list[str]) -> int:
    if not argv:
        print("usage: validate_frgl.py <file.frgl> [...]", file=sys.stderr)
        return 2
    bad = 0
    for arg in argv:
        p = pathlib.Path(arg)
        if not validate(p):
            bad += 1
        else:
            print(f"ok  {p}")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
