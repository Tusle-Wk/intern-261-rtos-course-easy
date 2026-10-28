"""Do do tre bo loc: seq dau tien >= 100 ma ax dau ra >= 50% buoc nhay (5000)."""
import re
import sys

for f in sys.argv[1:]:
    seqs, found, lost, prev = [], None, 0, None
    for l in open(f, errors="ignore"):
        if not re.match(r"^\d+,-?\d+,-?\d+,-?\d+\s*$", l):
            continue
        s, ax, _, _ = map(int, l.split(","))
        if prev is not None and s > prev + 1:
            lost += s - prev - 1
        prev = s
        if found is None and s >= 100 and ax >= 5000:
            found = s - 100
    print(f"{f}: tre = {found} mau ({found * 5 if found is not None else '?'} ms) | mau mat = {lost}")