#include "Processor.h"
#include "Workload.h"
#include "MainMem.h"
#include <cstdint>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>
#include <format>

// =============================================================================
// cache_sim - command-line front-end for the configurable cache simulator.
//
//   ./cache_sim [options]            run one configuration over one workload
//   ./cache_sim --sweep [options]    design-space exploration -> CSV table
//   ./cache_sim --demo               the original Test 0-6 walkthrough
//   ./cache_sim --help               full option list
//
// Everything the cache needs is captured in a CacheConfig; the flags below just
// fill one in. Workloads come from a trace file (--trace) or a synthetic
// generator (--gen). Output is either a human report or a CSV row (--csv).
// =============================================================================

namespace {

// ------------------------------- small helpers -------------------------------

uint32_t ParseUint(const std::string& s) {
    // base 0 -> auto-detect 0x (hex) vs decimal
    return static_cast<uint32_t>(std::stoul(s, nullptr, 0));
}

const char* PolicyName(ReplacementPolicy p) {
    switch (p) {
        case ReplacementPolicy::Random: return "random";
        case ReplacementPolicy::LRU:    return "lru";
        case ReplacementPolicy::FIFO:   return "fifo";
        case ReplacementPolicy::PLRU:   return "plru";
    }
    return "?";
}

bool ParsePolicy(const std::string& s, ReplacementPolicy& out) {
    if (s == "random") { out = ReplacementPolicy::Random; return true; }
    if (s == "lru")    { out = ReplacementPolicy::LRU;    return true; }
    if (s == "fifo")   { out = ReplacementPolicy::FIFO;   return true; }
    if (s == "plru")   { out = ReplacementPolicy::PLRU;   return true; }
    return false;
}

// All knobs the CLI can set, with tool-friendly defaults (quiet, LRU).
struct Options {
    CacheConfig cfg{
        .numSets = 64,
        .numWays = 4,
        .lineSize = 64,
        .addressBits = 32,
        .writeBack = false,
        .writeAllocate = false,
        .policy = ReplacementPolicy::LRU,
        .hitTime = 1,
        .missPenalty = 100,
        .verbose = false,
    };

    // Workload: a trace file wins; otherwise a synthetic generator.
    std::string trace;
    std::string gen = "looping";
    uint32_t genStart = 0x1000;
    uint32_t genCount = 4096;     // sequential / strided / random
    uint32_t genStride = 4;
    uint32_t genSpan = 4096;      // looping working set / random address span
    uint32_t genLoops = 8;        // looping
    unsigned genSeed = 1;         // random

    // Output / mode.
    bool csv = false;
    bool csvHeader = false;
    bool dump = false;
    bool demo = false;
    bool sweep = false;
    bool help = false;
};

void PrintUsage(std::ostream& os) {
    os <<
"cache_sim - configurable cache simulator\n"
"\n"
"Usage:\n"
"  cache_sim [options]          run one config over one workload\n"
"  cache_sim --sweep [options]  design-space exploration, emits a CSV table\n"
"  cache_sim --demo             guided Test 0-6 walkthrough (verbose)\n"
"  cache_sim --help             this message\n"
"\n"
"Geometry:\n"
"  --sets N           number of sets, power of two           [64]\n"
"  --ways N           associativity (>=1)                    [4]\n"
"  --line N           line size in bytes, power of two       [64]\n"
"  --addr-bits N      address width in bits                  [32]\n"
"\n"
"Policy:\n"
"  --policy P         random | lru | fifo | plru             [lru]\n"
"  --write-back       defer writes to eviction (else write-through)\n"
"  --write-through    write straight through to memory       (default)\n"
"  --write-allocate   fill line on write miss (else no-write-allocate)\n"
"  --no-write-allocate                                       (default)\n"
"  --hit-time N       hit cost in cycles                     [1]\n"
"  --miss-penalty N   extra miss cost in cycles              [100]\n"
"\n"
"Workload (pick one; default = looping generator):\n"
"  --trace FILE       replay a text trace file (R/W lines)\n"
"  --gen TYPE         sequential | strided | random | looping\n"
"    --gen-start A    base address                           [0x1000]\n"
"    --gen-count N    accesses (sequential/strided/random)   [4096]\n"
"    --gen-stride N   stride in bytes                        [4]\n"
"    --gen-span N     working set / address span in bytes    [4096]\n"
"    --gen-loops N    sweeps over the working set (looping)  [8]\n"
"    --gen-seed N     RNG seed (random)                      [1]\n"
"\n"
"Output:\n"
"  --csv              emit one CSV row instead of the report\n"
"  --csv-header       also print the CSV header (with --csv)\n"
"  --dump             dump cache contents after the run\n"
"  --verbose          log every access (default: quiet)\n"
"  --quiet            silence per-access logging             (default)\n";
}

// --------------------------------- CSV output --------------------------------

void WriteCsvHeader(std::ostream& os) {
    os << "policy,sets,ways,line_bytes,size_bytes,write_back,write_allocate,"
          "accesses,hits,misses,hit_rate,miss_rate,compulsory,capacity,conflict,"
          "evictions,dirty_writebacks,amat\n";
}

void WriteCsvRow(std::ostream& os, const CacheConfig& c, const CacheStats& s) {
    uint64_t sizeBytes = static_cast<uint64_t>(c.numSets) * c.numWays * c.lineSize;
    os << PolicyName(c.policy) << ',' << c.numSets << ',' << c.numWays << ','
       << c.lineSize << ',' << sizeBytes << ',' << (c.writeBack ? 1 : 0) << ','
       << (c.writeAllocate ? 1 : 0) << ',' << s.accesses() << ',' << s.hits() << ','
       << s.misses() << ',' << std::format("{:.6f}", s.hitRate()) << ','
       << std::format("{:.6f}", s.missRate()) << ',' << s.compulsoryMisses << ','
       << s.capacityMisses << ',' << s.conflictMisses << ',' << s.evictions << ','
       << s.dirtyWritebacks << ','
       << std::format("{:.4f}", s.amat(c.hitTime, c.missPenalty)) << '\n';
}

// ------------------------------- workload build ------------------------------

Trace BuildTrace(const Options& o) {
    if (!o.trace.empty()) {
        return LoadTrace(o.trace);
    }
    if (o.gen == "sequential") return GenSequential(o.genStart, o.genCount, o.genStride);
    if (o.gen == "strided")    return GenStrided(o.genStart, o.genCount, o.genStride);
    if (o.gen == "random")     return GenRandom(o.genCount, o.genSpan, o.genSeed);
    return GenLooping(o.genStart, o.genSpan, o.genLoops, o.genStride);  // default
}

// Main memory is a fixed 4 MB array; an access past the end would throw. Catch
// it up front with a friendly message instead of crashing mid-run.
bool TraceInRange(const Trace& trace, uint32_t& badAddr) {
    for (const Access& a : trace) {
        if (a.address + sizeof(uint32_t) > MAIN_MEMORY_SIZE) {
            badAddr = a.address;
            return false;
        }
    }
    return true;
}

// --------------------------------- run modes ---------------------------------

int RunSingle(const Options& o) {
    if (!o.cfg.Validate()) {
        return 1;
    }

    Trace trace = BuildTrace(o);
    if (trace.empty()) {
        std::cerr << "No accesses to run (empty trace / generator)." << std::endl;
        return 1;
    }
    uint32_t badAddr = 0;
    if (!TraceInRange(trace, badAddr)) {
        std::cerr << std::format(
            "Address 0x{:x} is outside the {} KB main memory; shrink the workload.",
            badAddr, MAIN_MEMORY_SIZE / 1024) << std::endl;
        return 1;
    }

    MemSys mem(o.cfg);
    mem.SeedPattern();                       // predictable bytes (value == address & 0xFF)
    RunTrace(mem, trace);

    if (o.csv) {
        if (o.csvHeader) {
            WriteCsvHeader(std::cout);
        }
        WriteCsvRow(std::cout, o.cfg, mem.GetStats());
    }
    else {
        mem.PrintStats();
    }
    if (o.dump) {
        mem.DumpCache();
    }
    return 0;
}

int RunSweep(const Options& o) {
    // Build the workload once and replay it against every configuration so the
    // comparison is apples-to-apples.
    Trace trace = BuildTrace(o);
    if (trace.empty()) {
        std::cerr << "No accesses to run (empty trace / generator)." << std::endl;
        return 1;
    }
    uint32_t badAddr = 0;
    if (!TraceInRange(trace, badAddr)) {
        std::cerr << std::format(
            "Address 0x{:x} is outside the {} KB main memory; shrink the workload.",
            badAddr, MAIN_MEMORY_SIZE / 1024) << std::endl;
        return 1;
    }

    const std::vector<uint32_t> setsGrid = {16, 32, 64, 128, 256};
    const std::vector<uint32_t> waysGrid = {1, 2, 4, 8};
    const std::vector<ReplacementPolicy> policies = {
        ReplacementPolicy::Random, ReplacementPolicy::LRU,
        ReplacementPolicy::FIFO,   ReplacementPolicy::PLRU};

    WriteCsvHeader(std::cout);
    for (ReplacementPolicy pol : policies) {
        for (uint32_t sets : setsGrid) {
            for (uint32_t ways : waysGrid) {
                CacheConfig c = o.cfg;       // inherit line size, write policy, latencies
                c.numSets = sets;
                c.numWays = ways;
                c.policy = pol;
                c.verbose = false;
                if (!c.Validate()) {
                    continue;                // e.g. PLRU with non-power-of-two ways
                }
                MemSys mem(c);
                mem.SeedPattern();
                RunTrace(mem, trace);
                WriteCsvRow(std::cout, c, mem.GetStats());
            }
        }
    }
    return 0;
}

// --------------------------------- arg parse ---------------------------------

// Returns false on a parse error (message already printed).
bool ParseArgs(int argc, char** argv, Options& o) {
    auto need = [&](int& i, const char* flag) -> std::string {
        if (i + 1 >= argc) {
            std::cerr << "Missing value for " << flag << std::endl;
            return std::string();
        }
        return argv[++i];
    };

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { o.help = true; }
        else if (arg == "--demo")  { o.demo = true; }
        else if (arg == "--sweep") { o.sweep = true; }
        else if (arg == "--sets")        { o.cfg.numSets = ParseUint(need(i, "--sets")); }
        else if (arg == "--ways")        { o.cfg.numWays = ParseUint(need(i, "--ways")); }
        else if (arg == "--line")        { o.cfg.lineSize = ParseUint(need(i, "--line")); }
        else if (arg == "--addr-bits")   { o.cfg.addressBits = ParseUint(need(i, "--addr-bits")); }
        else if (arg == "--hit-time")    { o.cfg.hitTime = ParseUint(need(i, "--hit-time")); }
        else if (arg == "--miss-penalty"){ o.cfg.missPenalty = ParseUint(need(i, "--miss-penalty")); }
        else if (arg == "--write-back")    { o.cfg.writeBack = true; }
        else if (arg == "--write-through") { o.cfg.writeBack = false; }
        else if (arg == "--write-allocate")    { o.cfg.writeAllocate = true; }
        else if (arg == "--no-write-allocate") { o.cfg.writeAllocate = false; }
        else if (arg == "--verbose") { o.cfg.verbose = true; }
        else if (arg == "--quiet")   { o.cfg.verbose = false; }
        else if (arg == "--policy") {
            std::string p = need(i, "--policy");
            if (!ParsePolicy(p, o.cfg.policy)) {
                std::cerr << "Unknown policy '" << p << "' (use random|lru|fifo|plru)" << std::endl;
                return false;
            }
        }
        else if (arg == "--trace")     { o.trace = need(i, "--trace"); }
        else if (arg == "--gen")       { o.gen = need(i, "--gen"); }
        else if (arg == "--gen-start") { o.genStart = ParseUint(need(i, "--gen-start")); }
        else if (arg == "--gen-count") { o.genCount = ParseUint(need(i, "--gen-count")); }
        else if (arg == "--gen-stride"){ o.genStride = ParseUint(need(i, "--gen-stride")); }
        else if (arg == "--gen-span")  { o.genSpan = ParseUint(need(i, "--gen-span")); }
        else if (arg == "--gen-loops") { o.genLoops = ParseUint(need(i, "--gen-loops")); }
        else if (arg == "--gen-seed")  { o.genSeed = ParseUint(need(i, "--gen-seed")); }
        else if (arg == "--csv")        { o.csv = true; }
        else if (arg == "--csv-header") { o.csvHeader = true; }
        else if (arg == "--dump")       { o.dump = true; }
        else {
            std::cerr << "Unknown argument: " << arg << " (try --help)" << std::endl;
            return false;
        }
    }
    return true;
}

}  // namespace

int RunDemo();   // guided walkthrough, defined at the bottom of this file

int main(int argc, char** argv) {
    Options o;
    if (!ParseArgs(argc, argv, o)) {
        return 2;
    }
    if (o.help) {
        PrintUsage(std::cout);
        return 0;
    }
    if (o.demo)  { return RunDemo(); }
    if (o.sweep) { return RunSweep(o); }
    return RunSingle(o);
}

// =============================================================================
// --demo: the original guided walkthrough (Test 0-6 + a profiling showcase).
// Kept verbose so the per-access logs explain what each policy/write mode does.
// =============================================================================

namespace {
void DemoBanner(const std::string& title) {
    std::cout << std::endl << title << std::endl;
}
}  // namespace

int RunDemo() {
    CacheConfig config{
        .numSets = 64,
        .numWays = 4,
        .lineSize = 64,
        .addressBits = 32,
        .writeBack = false,        // write-through
        .writeAllocate = false,    // no-write-allocate
        .policy = ReplacementPolicy::Random,
        .verbose = true,
    };
    if (!config.Validate()) {
        return 1;
    }

    MemSys memory(config);

    // Test 0: Load the data file and peek at main mem to confirm the load.
    // Values loaded match addresses for ease of debugging (0x20 lives at 0x20).
    DemoBanner("Test 0: Load data.bin & peek main memory");
    memory.LoadMainMem("data.bin");
    std::cout << std::format("PeekMem(0x20) = 0x{:x} (expect 0x23222120 from data.bin)",
        memory.PeekMem(0x20)) << std::endl;

    // Test 1: Read miss then hit (set 0).
    DemoBanner("Test 1: Read miss then hit (set 0)");
    uint32_t value = memory.Read(0x20);
    std::cout << std::format("Read(0x20) = 0x{:x} (expect 0x23222120)", value) << std::endl;
    value = memory.Read(0x24);
    std::cout << std::format("Read(0x24) = 0x{:x} (expect 0x27262524)", value) << std::endl;

    // Test 2: Write hit + write-through.
    DemoBanner("Test 2: Write hit + write-through");
    memory.Write(0x24, 0xDEADBEEF);
    std::cout << std::format("PeekMem(0x24) = 0x{:x} (write-through -> expect 0xdeadbeef in main mem)",
        memory.PeekMem(0x24)) << std::endl;
    value = memory.Read(0x24);
    std::cout << std::format("Read(0x24) = 0x{:x} (expect 0xdeadbeef)", value) << std::endl;

    // Test 3: Write miss / no-write-allocate (set 2).
    DemoBanner("Test 3: Write miss / no-write-allocate (set 2)");
    memory.Write(0x80, 0xCAFEBABE);
    std::cout << std::format("PeekMem(0x80) = 0x{:x} (expect 0xcafebabe in main mem)",
        memory.PeekMem(0x80)) << std::endl;
    value = memory.Read(0x80);
    std::cout << std::format("Read(0x80) = 0x{:x} (expect 0xcafebabe)", value) << std::endl;

    // Test 4: Eviction (set 1, 4 ways).
    DemoBanner("Test 4: Fill set 1 then force a random eviction");
    memory.Read(0x40);
    memory.Read(0x1040);
    memory.Read(0x2040);
    memory.Read(0x3040);
    std::cout << "- set 1 now holds 4 lines; next read must evict a victim -" << std::endl;
    memory.Read(0x4040);
    memory.Read(0x4040);

    // Test 5: Write miss behavior - write-allocate vs no-write-allocate (set 3).
    DemoBanner("Test 5: Write miss - write-allocate vs no-write-allocate (set 3)");
    memory.Write(0xC0, 0x11223344);
    std::cout << "- now read 0xC0: HIT if write-allocate, MISS if no-write-allocate -" << std::endl;
    value = memory.Read(0xC0);
    std::cout << std::format("Read(0xC0) = 0x{:x} (expect 0x11223344)", value) << std::endl;

    // Test 6: Write-back defers the memory write until eviction (set 4).
    DemoBanner("Test 6: Write-back defers write until eviction (set 4)");
    memory.Read(0x100);
    memory.Write(0x100, 0xABCD1234);
    std::cout << "- main mem after write hit: OLD bytes if write-back, NEW bytes if write-through -" << std::endl;
    std::cout << std::format("PeekMem(0x100) = 0x{:x} (write-back -> still 0x3020100; write-through -> 0xabcd1234)",
        memory.PeekMem(0x100)) << std::endl;
    std::cout << "- forcing evictions on set 4 (watch for a dirty-line flush under write-back) -" << std::endl;
    memory.Read(0x1100);
    memory.Read(0x2100);
    memory.Read(0x3100);
    memory.Read(0x4100);
    memory.Read(0x5100);
    memory.Read(0x6100);
    memory.Read(0x7100);
    memory.Read(0x8100);
    std::cout << "- under write-back, 0x100 in main mem should now read 0xabcd1234 if it was flushed -" << std::endl;
    std::cout << std::format("PeekMem(0x100) = 0x{:x} (expect 0xabcd1234 if the dirty line was flushed)",
        memory.PeekMem(0x100)) << std::endl;

    // Showcase: drive a synthetic workload, then use the debug + profiling tools.
    DemoBanner("Showcase: replay a looping workload then profile the cache");
    RunTrace(memory, GenLooping(0x2000, /*span=*/512, /*loops=*/4, /*stride=*/4));
    memory.DumpCache();
    memory.PrintStats();

    return 0;
}
