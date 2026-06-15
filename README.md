# HOMEMADE CACHE SIMULATOR

A configurable, single-level cache simulator (not a full CPU or a multi-level hierarchy) written in C++. 
We describe the cache (geometry, write policy, replacement policy), feed it a memory access pattern, 
and read back how it behaves: hits and misses, why those misses happened, and a simple
access-time estimate.

The goal is to make cache design experiments easy: change a knob from the command
line, rerun, compare results — no recompiling, no hand-editing test code.

---

## What we can do

- **Model a cache** with configurable sets, associativity, line size, write policy,
  and replacement policy.
- **Replay workloads** using built-in generators or a text trace file.
- **Profile runs** with hit/miss rates, AMAT (average memory access time), and the
  classic **three C's** miss breakdown (compulsory, capacity, conflict).
- **Explore the design space** with `--sweep`, which runs one workload across many
  cache configurations and outputs CSV for plotting.
- **Verify correctness** with an automated test suite (`make test`).

---

## How the simulator is organized

```
Processor (MemSys)  →  Cache  →  Main Memory
         ↑
Workload / trace replay
```

| Piece | Role |
| --- | --- |
| **MemSys** | The interface we drive. Reads and writes go through the cache. |
| **Cache** | The model itself. Handles hits, misses, fills, evictions, dirty writebacks, and stats. |
| **Main Memory** | A fixed 4 MB backing store. Runs are seeded with predictable byte values so reads are repeatable. |
| **Workload layer** | Builds or loads a sequence of reads/writes and replays them against MemSys. |

Everything about the cache is controlled by one **`CacheConfig`**: number of sets,
ways per set, line size, write-back vs write-through, write-allocate vs
no-write-allocate, replacement policy, and optional latency knobs for AMAT.

We pass those values in at runtime (via CLI flags or test helpers) instead of hardcoding them.

---

## The cache model

### Address mapping

Each 32-bit address is split into three fields:

```
[ tag | set index | byte offset within line ]
```

Set count and line size determine how many bits go to the set index and offset.
The rest is the tag stored in each cache line.

### On a read

1. Decode the address → find the right set.
2. Search the set's ways for a matching tag.
3. **Hit** → return data from the line; update replacement state.
4. **Miss** → fetch the full line from main memory, install it (possibly evicting
   someone), then return the requested word.

### On a write

Behavior depends on two independent choices:

| Write policy | On a hit | On a miss |
| --- | --- | --- |
| **Write-through** (default) | Update cache + main memory immediately | Depends on allocate policy |
| **Write-back** | Update cache only; mark line dirty | Depends on allocate policy |
| **Write-allocate** | (same as above) | Bring line into cache, then write |
| **No-write-allocate** (default) | (same as above) | Write main memory only; don't load line |

On eviction under write-back, a dirty line is flushed to main memory before it is
overwritten.

### Replacement policies

When a set is full and a new line must be installed, one way is chosen as the
victim:

| Policy | Idea |
| --- | --- |
| **Random** | Pick a random way |
| **LRU** | Evict the least recently used way |
| **FIFO** | Evict the way that was filled longest ago (accesses don't reorder) |
| **PLRU** | Tree-based pseudo-LRU (requires power-of-two associativity) |

Policies are pluggable behind a common interface, so adding another one is
straightforward.

---

## Profiling and miss classification

Every run accumulates **CacheStats**:

- Access counts (reads, writes, hits, misses)
- Evictions and dirty writebacks
- Hit rate, miss rate, and **AMAT** = `hitTime + missRate × missPenalty`

Each miss is also classified into the **three C's**:

| Type | Meaning | Typical fix |
| --- | --- | --- |
| **Compulsory** | First time this block is ever touched (cold miss) | Prefetching, larger lines |
| **Capacity** | Working set too big even for a fully associative cache of the same size | Bigger cache |
| **Conflict** | Block was evicted from *this set* only; a fully associative cache would still have it | More associativity |

These three always add up to the total miss count.

### Debugging output

Instead of dumping raw memory bytes, we expose tools that match what actually matters
when debugging cache behavior:

- **`PrintStats()`** — human-readable profile (including the three C's)
- **`DumpCache()`** — which tags are in which sets/ways, and whether lines are dirty
- **`Contains(address)`** — is this address cached right now? (read-only probe)
- **`PeekMem(address)`** — read main memory directly, bypassing the cache (useful for
  checking write-through vs write-back behavior)

Add **`--verbose`** for a log line on every access.

---

## Workloads

A **workload** is just an ordered list of reads and writes. We can supply one in
two ways:

### 1. Synthetic generators

| Generator | What it does | Good for |
| --- | --- | --- |
| **sequential** | Walk addresses with a fixed stride | Spatial locality |
| **strided** | Same shape, but intent is a large stride | Conflict stress |
| **random** | Random addresses in a range | Low locality / worst case |
| **looping** | Repeat a sweep over a working set | Temporal locality; comparing policies |

### 2. Trace files

Plain text, one access per line:

```
R 0x20              # read
W 0x24 0xdeadbeef   # write
# lines starting with # are comments
```

See [`traces/sample.trace`](traces/sample.trace) for an example.

Before a run, memory is filled with a simple pattern (`byte value = address & 0xFF`)
so we don't need a data file unless running `--demo`.

---

## Build and run

**Requirements:** a C++20 compiler (`clang++` or `g++`) and `make`.

```bash
make              # build cache_sim
make test         # build and run the test suite
make clean        # remove build artifacts
./cache_sim --help
```

### Quick examples

**Default run** (64 sets × 4 ways, LRU, looping workload):

```bash
./cache_sim
```

**Custom cache + policy:**

```bash
./cache_sim --sets 128 --ways 8 --policy plru --write-back --write-allocate
```

**Replay a trace file:**

```bash
./cache_sim --trace traces/sample.trace --policy lru
```

**Random workload, CSV output:**

```bash
./cache_sim --gen random --gen-count 5000 --gen-span 65536 --csv --csv-header
```

**Guided walkthrough** (original hand-written tests, verbose logging, uses `data.bin`):

```bash
./cache_sim --demo
```

**Dump cache contents after a run:**

```bash
./cache_sim --gen looping --dump
```

---

## Design-space exploration (sweep + plots)

`--sweep` replays **one** workload against a grid of configurations:

- Sets: 16, 32, 64, 128, 256
- Ways: 1, 2, 4, 8
- Policies: random, lru, fifo, plru

Line size and write settings come from the other CLI flags. Output is a CSV table
(one row per configuration) with hit/miss rates, the three C's, evictions, and AMAT.

```bash
./cache_sim --sweep --gen looping --gen-span 16384 --gen-loops 4 > sweep.csv
```

To turn that into charts (requires Python + matplotlib):

```bash
python3 scripts/plot.py sweep.csv
```

This produces three PNGs: miss rate vs associativity, miss rate vs cache size, and a
stacked three-C's breakdown.

---

## Testing

The test suite uses [doctest](https://github.com/doctest/doctest) (vendored under
`third_party/`). Tests run quietly with fixed small-cache configs and assert exact
behavior — eviction order per policy, write-policy semantics, stats math, workload
parsing, and three-C's invariants.

```bash
make test
```

Test files live in `tests/` and are picked up automatically by the Makefile.

---

## Project layout

```
header/          Cache, MainMem, Processor (MemSys), ReplacementAlgo, Workload
src/             Implementations + main.cpp (CLI)
tests/           doctest suite
traces/          Sample trace files
scripts/         plot.py for sweep CSV
third_party/     doctest.h
Makefile
```

For deeper implementation notes (API details, extension points, test design), see
[`documentation.md`](documentation.md) if available locally — that file is kept
as a longer-form reference and may not be tracked in git.

---

## Typical workflow

1. Pick a workload that matches the behavior we want to study (looping for temporal
   locality, strided for conflicts, trace file for a specific pattern).
2. Run `./cache_sim` with the cache geometry and policy we care about.
3. Read `PrintStats()` output or `--csv` for numbers; add `--dump` when we need to
   see what's actually in the cache.
4. Use `--sweep` + `plot.py` when we want curves (miss rate vs size or associativity)
   instead of a single point.
5. Run `make test` after changing cache logic to confirm nothing regressed.