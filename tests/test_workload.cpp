#include "doctest.h"
#include "Processor.h"
#include "Workload.h"
#include "test_helpers.h"
#include <fstream>
#include <cstdio>

TEST_CASE("Generators produce the expected number of accesses") {
    CHECK(GenSequential(0, 10).size() == 10);
    CHECK(GenStrided(0, 7, 64).size() == 7);
    CHECK(GenRandom(20, 4096, 1).size() == 20);
    // Looping: (span/stride) accesses per sweep, times loops
    CHECK(GenLooping(0, 64, 3, 4).size() == (64 / 4) * 3);
}

TEST_CASE("RunTrace issues exactly one access per entry") {
    MemSys mem(TestConfig(ReplacementPolicy::LRU, 64, 4, 64));
    mem.SeedPattern();

    Trace t = GenSequential(0x0, 32, 4);
    RunTrace(mem, t);
    CHECK(mem.GetStats().accesses() == t.size());
}

TEST_CASE("Sequential walk over one line is 1 miss then all hits") {
    MemSys mem(TestConfig(ReplacementPolicy::LRU, 1, 4, 64));
    mem.SeedPattern();

    // 16 reads of 4 bytes each = one 64B line: first misses, rest hit
    RunTrace(mem, GenSequential(0x0, 16, 4));
    CHECK(mem.GetStats().readMisses == 1);
    CHECK(mem.GetStats().readHits == 15);
}

TEST_CASE("LoadTrace parses reads and writes, ignoring comments") {
    // Write a small trace to a temp file and round-trip it
    const char* path = "tests_tmp.trace";
    {
        std::ofstream f(path);
        f << "# comment line\n";
        f << "\n";                       // blank
        f << "R 0x20\n";
        f << "W 0x24 0xdeadbeef\n";
        f << "r 16\n";                   // decimal, lowercase
    }

    Trace t = LoadTrace(path);
    std::remove(path);

    REQUIRE(t.size() == 3);
    CHECK(t[0].op == Op::Read);
    CHECK(t[0].address == 0x20u);
    CHECK(t[1].op == Op::Write);
    CHECK(t[1].address == 0x24u);
    CHECK(t[1].value == 0xdeadbeefu);
    CHECK(t[2].op == Op::Read);
    CHECK(t[2].address == 16u);
}

TEST_CASE("Looping workload has a higher hit rate than uniform random") {
    CacheConfig cfg = TestConfig(ReplacementPolicy::LRU, 16, 4, 64);

    MemSys loopMem(cfg);
    loopMem.SeedPattern();
    // Working set (1KB) sweeps repeatedly -> strong temporal/spatial locality
    RunTrace(loopMem, GenLooping(0x0, 1024, 8, 4));

    MemSys randMem(cfg);
    randMem.SeedPattern();
    RunTrace(randMem, GenRandom(2048, 1u << 20, 12345));

    CHECK(loopMem.GetStats().hitRate() > randMem.GetStats().hitRate());
}
