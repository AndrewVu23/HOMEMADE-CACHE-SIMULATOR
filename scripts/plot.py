#!/usr/bin/env python3
"""Plot cache_sim --sweep results.

The simulator's sweep mode prints a CSV table (one row per configuration). This
script turns that table into the classic cache design-space curves:

  1. miss rate vs associativity   (fixed set count, one line per policy)
  2. miss rate vs cache size       (fixed associativity, one line per policy)
  3. the three C's stacked         (compulsory / capacity / conflict per size)

Usage:
  ./cache_sim --sweep --gen looping --gen-span 16384 > sweep.csv
  python3 scripts/plot.py sweep.csv                 # read a file
  ./cache_sim --sweep | python3 scripts/plot.py     # or read stdin

Options let you pin the fixed dimension; see --help. Figures are written next to
the CSV (or the current directory) as PNGs.

Requires: matplotlib  (pip install matplotlib)
"""

import argparse
import csv
import sys
from collections import defaultdict

try:
    import matplotlib
    matplotlib.use("Agg")  # headless: just write PNGs
    import matplotlib.pyplot as plt
except ImportError:
    sys.exit("matplotlib is required: pip install matplotlib")


def load_rows(path):
    stream = sys.stdin if path in (None, "-") else open(path, newline="")
    with stream as f:
        rows = list(csv.DictReader(f))
    if not rows:
        sys.exit("No CSV rows found (did you run cache_sim --sweep?).")
    # Coerce the numeric columns we use.
    ints = ("sets", "ways", "line_bytes", "size_bytes",
            "compulsory", "capacity", "conflict", "accesses")
    floats = ("miss_rate", "hit_rate", "amat")
    for r in rows:
        for k in ints:
            r[k] = int(r[k])
        for k in floats:
            r[k] = float(r[k])
    return rows


def policies(rows):
    # Preserve first-seen order.
    seen = []
    for r in rows:
        if r["policy"] not in seen:
            seen.append(r["policy"])
    return seen


def plot_miss_vs_assoc(rows, sets, out):
    subset = [r for r in rows if r["sets"] == sets]
    if not subset:
        print(f"[skip] no rows with sets={sets} for miss-vs-assoc")
        return
    plt.figure()
    for pol in policies(subset):
        pts = sorted((r["ways"], r["miss_rate"]) for r in subset if r["policy"] == pol)
        xs = [w for w, _ in pts]
        ys = [m * 100 for _, m in pts]
        plt.plot(xs, ys, marker="o", label=pol)
    plt.xscale("log", base=2)
    plt.xlabel("associativity (ways)")
    plt.ylabel("miss rate (%)")
    plt.title(f"Miss rate vs associativity (sets={sets})")
    plt.legend()
    plt.grid(True, which="both", alpha=0.3)
    plt.savefig(out, dpi=120, bbox_inches="tight")
    print(f"[wrote] {out}")


def plot_miss_vs_size(rows, ways, out):
    subset = [r for r in rows if r["ways"] == ways]
    if not subset:
        print(f"[skip] no rows with ways={ways} for miss-vs-size")
        return
    plt.figure()
    for pol in policies(subset):
        pts = sorted((r["size_bytes"], r["miss_rate"]) for r in subset if r["policy"] == pol)
        xs = [s for s, _ in pts]
        ys = [m * 100 for _, m in pts]
        plt.plot(xs, ys, marker="o", label=pol)
    plt.xscale("log", base=2)
    plt.xlabel("cache size (bytes)")
    plt.ylabel("miss rate (%)")
    plt.title(f"Miss rate vs cache size (ways={ways})")
    plt.legend()
    plt.grid(True, which="both", alpha=0.3)
    plt.savefig(out, dpi=120, bbox_inches="tight")
    print(f"[wrote] {out}")


def plot_three_cs(rows, policy, ways, out):
    subset = [r for r in rows if r["policy"] == policy and r["ways"] == ways]
    if not subset:
        print(f"[skip] no rows with policy={policy}, ways={ways} for three-C's")
        return
    subset.sort(key=lambda r: r["size_bytes"])
    labels = [str(r["size_bytes"]) for r in subset]
    acc = [max(r["accesses"], 1) for r in subset]
    comp = [100 * r["compulsory"] / a for r, a in zip(subset, acc)]
    cap = [100 * r["capacity"] / a for r, a in zip(subset, acc)]
    con = [100 * r["conflict"] / a for r, a in zip(subset, acc)]

    x = range(len(labels))
    plt.figure()
    plt.bar(x, comp, label="compulsory")
    plt.bar(x, cap, bottom=comp, label="capacity")
    plt.bar(x, con, bottom=[c + p for c, p in zip(comp, cap)], label="conflict")
    plt.xticks(list(x), labels, rotation=45, ha="right")
    plt.xlabel("cache size (bytes)")
    plt.ylabel("misses (% of accesses)")
    plt.title(f"Three C's breakdown ({policy}, ways={ways})")
    plt.legend()
    plt.grid(True, axis="y", alpha=0.3)
    plt.savefig(out, dpi=120, bbox_inches="tight")
    print(f"[wrote] {out}")


def main():
    ap = argparse.ArgumentParser(description="Plot cache_sim --sweep CSV output.")
    ap.add_argument("csv", nargs="?", default="-",
                    help="sweep CSV file (default: stdin)")
    ap.add_argument("--sets", type=int,
                    help="set count for the miss-vs-assoc plot (default: largest)")
    ap.add_argument("--ways", type=int,
                    help="associativity for the miss-vs-size / three-C's plots (default: smallest)")
    ap.add_argument("--policy", default="lru",
                    help="policy for the three-C's plot (default: lru)")
    ap.add_argument("--prefix", default="cache_",
                    help="output filename prefix (default: cache_)")
    args = ap.parse_args()

    rows = load_rows(args.csv)
    sets = args.sets if args.sets is not None else max(r["sets"] for r in rows)
    ways = args.ways if args.ways is not None else min(r["ways"] for r in rows)

    plot_miss_vs_assoc(rows, sets, f"{args.prefix}miss_vs_assoc.png")
    plot_miss_vs_size(rows, ways, f"{args.prefix}miss_vs_size.png")
    plot_three_cs(rows, args.policy, ways, f"{args.prefix}three_cs.png")


if __name__ == "__main__":
    main()
