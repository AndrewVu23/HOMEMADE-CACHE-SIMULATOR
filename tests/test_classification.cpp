#include "doctest.h"
#include "Processor.h"
#include "Workload.h"
#include "test_helpers.h"

// The three-C's classifier compares the real cache against an offline,
// same-capacity fully-associative LRU model:
//   compulsory = first-ever reference to a block
//   capacity   = the fully-associative model would miss too
//   conflict   = only the real (limited-associativity) cache missed
// In every run: compulsory + capacity + conflict == total misses.

TEST_CASE("Conflict miss: blocks collide in one set but fit in a fuller cache") {
    // 2 sets, direct-mapped -> capacity 2 lines. Addresses 0 and 128 are
    // different blocks that both map to set 0 (block index 0 and 2, set = block&1).
    MemSys mem(TestConfig(ReplacementPolicy::LRU, /*numSets=*/2, /*numWays=*/1, 64));
    mem.SeedPattern();

    mem.Read(0);        // compulsory (block 0 -> set 0)
    mem.Read(128);      // compulsory (block 2 -> set 0, evicts block 0 in real cache)
    mem.Read(0);        // MISS in direct-mapped, but a 2-line fully-assoc still holds it

    const CacheStats& s = mem.GetStats();
    CHECK(s.misses() == 3);
    CHECK(s.compulsoryMisses == 2);
    CHECK(s.conflictMisses == 1);
    CHECK(s.capacityMisses == 0);
    CHECK(s.compulsoryMisses + s.capacityMisses + s.conflictMisses == s.misses());
}

TEST_CASE("Capacity miss: working set exceeds total cache size") {
    // Same 2-line cache. Touch 3 distinct blocks then revisit the first: even a
    // fully-associative 2-line cache cannot have kept it -> capacity, not conflict.
    MemSys mem(TestConfig(ReplacementPolicy::LRU, /*numSets=*/2, /*numWays=*/1, 64));
    mem.SeedPattern();

    mem.Read(0);        // compulsory (block 0, set 0)
    mem.Read(64);       // compulsory (block 1, set 1)
    mem.Read(128);      // compulsory (block 2, set 0)
    mem.Read(0);        // block 0 was evicted from the 2-line working set -> capacity

    const CacheStats& s = mem.GetStats();
    CHECK(s.misses() == 4);
    CHECK(s.compulsoryMisses == 3);
    CHECK(s.capacityMisses == 1);
    CHECK(s.conflictMisses == 0);
    CHECK(s.compulsoryMisses + s.capacityMisses + s.conflictMisses == s.misses());
}

TEST_CASE("Fully-associative cache never reports conflict misses") {
    // One set with 4 ways == fully associative -> a miss is only ever
    // compulsory (cold) or capacity (working set too big), never conflict.
    MemSys mem(TestConfig(ReplacementPolicy::LRU, /*numSets=*/1, /*numWays=*/4, 64));
    mem.SeedPattern();

    // 8 distinct lines through a 4-line cache, looped twice -> lots of misses.
    RunTrace(mem, GenLooping(/*start=*/0, /*span=*/8 * 64, /*loops=*/2, /*stride=*/64));

    const CacheStats& s = mem.GetStats();
    CHECK(s.conflictMisses == 0);
    CHECK(s.compulsoryMisses + s.capacityMisses + s.conflictMisses == s.misses());
}

TEST_CASE("The three C's always sum to the total miss count") {
    MemSys mem(TestConfig(ReplacementPolicy::FIFO, /*numSets=*/8, /*numWays=*/2, 64));
    mem.SeedPattern();

    RunTrace(mem, GenRandom(/*count=*/2000, /*addrSpace=*/0x8000, /*seed=*/7));

    const CacheStats& s = mem.GetStats();
    CHECK(s.compulsoryMisses + s.capacityMisses + s.conflictMisses == s.misses());
    CHECK(s.compulsoryMisses >= 1);   // at least one cold miss must occur
}
