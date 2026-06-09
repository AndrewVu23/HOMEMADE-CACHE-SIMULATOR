#pragma once

#include "Processor.h"
#include <cstdint>
#include <vector>
#include <string>

// A single memory operation in an access trace.
enum class Op { 
    Read, 
    Write 
};

struct Access {
    Op op;
    uint32_t address;
    uint32_t value = 0;             // Only meaningful for writes
};

// A trace is just an ordered list of accesses to replay against a MemSys
using Trace = std::vector<Access>;

// Replay every access in the trace against the memory system, issuing the
// matching Read/Write. Reads' return values are ignored (we only care about
// the resulting cache behavior / stats).
void RunTrace(MemSys& mem, const Trace& trace);

// --------------------------- Synthetic generators ---------------------------
// Each generator returns a Trace we can pass to RunTrace. They are read-only
// unless noted -> pass the result through RunTrace to drive the cache

// Linear walk: addresses start, start+stride, ... for `count` accesses
// High spatial locality (consecutive bytes share a cache line)
Trace GenSequential(uint32_t start, uint32_t count, uint32_t stride = 4);

// Large fixed stride to stress conflict misses (e.g. stride == one line or set span)
Trace GenStrided(uint32_t start, uint32_t count, uint32_t stride);

// Uniformly random addresses in [0, addrSpace) 
// Low locality -> high miss rate
Trace GenRandom(uint32_t count, uint32_t addrSpace, unsigned seed);

// Repeatedly sweep a working set of `span` bytes, `loops` times
// Strong temporal locality -> exposes differences between policies
Trace GenLooping(uint32_t start, uint32_t span, uint32_t loops, uint32_t stride = 4);

// ----------------------------- Trace-file loader ----------------------------
// Parse a text trace file. One access per line:
//   R 0x20            (read)
//   W 0x24 0xdeadbeef (write with value)
// Hex (0x...) or decimal accepted. Blank lines and lines starting with '#'
// are ignored. Returns the parsed trace (empty if the file cannot be opened).
Trace LoadTrace(const std::string& path);
