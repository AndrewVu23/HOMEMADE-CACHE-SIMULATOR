#include "doctest.h"
#include "Processor.h"
#include "test_helpers.h"
#include <cmath>

TEST_CASE("Counters track a known access sequence exactly") {
    MemSys mem(TestConfig(ReplacementPolicy::LRU, 1, 4, 64));
    mem.SeedPattern();

    mem.Read(0x00);          // miss (tag 0)
    mem.Read(0x04);          // hit  (same line)
    mem.Read(0x40);          // miss (tag 1)
    mem.Write(0x00, 0x1234); // write hit (tag 0)
    mem.Write(0x80, 0x5678); // write miss (tag 2), no-write-allocate by default

    const CacheStats& s = mem.GetStats();
    CHECK(s.reads == 3);
    CHECK(s.writes == 2);
    CHECK(s.readHits == 1);
    CHECK(s.readMisses == 2);
    CHECK(s.writeHits == 1);
    CHECK(s.writeMisses == 1);
    CHECK(s.accesses() == 5);
    CHECK(s.hits() == 2);
    CHECK(s.misses() == 3);
}

TEST_CASE("Hit/miss rates and AMAT match the formula") {
    MemSys mem(TestConfig(ReplacementPolicy::LRU, 1, 4, 64));
    mem.SeedPattern();

    // 1 miss + 3 hits within one line -> 4 accesses, hitRate 0.75
    mem.Read(0x00);
    mem.Read(0x04);
    mem.Read(0x08);
    mem.Read(0x0C);

    const CacheStats& s = mem.GetStats();
    CHECK(s.hitRate() == doctest::Approx(0.75));
    CHECK(s.missRate() == doctest::Approx(0.25));

    // AMAT = hitTime + missRate * missPenalty = 1 + 0.25 * 100 = 26
    CHECK(s.amat(1, 100) == doctest::Approx(26.0));
    // Different latencies: 2 + 0.25 * 200 = 52
    CHECK(s.amat(2, 200) == doctest::Approx(52.0));
}

TEST_CASE("Empty cache reports zero rates without dividing by zero") {
    MemSys mem(TestConfig());
    const CacheStats& s = mem.GetStats();
    CHECK(s.accesses() == 0);
    CHECK(s.hitRate() == doctest::Approx(0.0));
    CHECK(s.missRate() == doctest::Approx(0.0));
    CHECK(s.amat(1, 100) == doctest::Approx(1.0));   // hitTime + 0
}
