#include "doctest.h"
#include "Processor.h"
#include "test_helpers.h"

// All policy tests use a single 4-way set so tags 0..N collide and eviction
// order is fully controlled. TagAddr(t) returns the address whose tag is t.

static int CountResident(MemSys& mem, uint32_t maxTag) {
    int n = 0;
    for (uint32_t t = 0; t < maxTag; t++) {
        if (mem.Contains(TagAddr(t))) {
            n++;
        }
    }
    return n;
}

TEST_CASE("LRU evicts the least-recently-used line, sparing a touched one") {
    MemSys mem(TestConfig(ReplacementPolicy::LRU, 1, 4, 64));
    mem.SeedPattern();

    for (uint32_t t = 0; t < 4; t++) mem.Read(TagAddr(t));   // fill ways with tags 0..3
    mem.Read(TagAddr(0));                                    // touch tag 0 -> now MRU; tag 1 is LRU
    mem.Read(TagAddr(4));                                    // miss -> evicts LRU (tag 1)

    CHECK(mem.Contains(TagAddr(0)));                         // spared (recently used)
    CHECK_FALSE(mem.Contains(TagAddr(1)));                   // evicted
    CHECK(mem.Contains(TagAddr(2)));
    CHECK(mem.Contains(TagAddr(3)));
    CHECK(mem.Contains(TagAddr(4)));
}

TEST_CASE("FIFO evicts the oldest insertion regardless of access") {
    MemSys mem(TestConfig(ReplacementPolicy::FIFO, 1, 4, 64));
    mem.SeedPattern();

    for (uint32_t t = 0; t < 4; t++) mem.Read(TagAddr(t));   // insertion order 0,1,2,3
    mem.Read(TagAddr(0));                                    // access tag 0 (FIFO ignores this)
    mem.Read(TagAddr(4));                                    // miss -> evicts oldest insert (tag 0)

    CHECK_FALSE(mem.Contains(TagAddr(0)));                   // evicted despite recent access
    CHECK(mem.Contains(TagAddr(1)));
    CHECK(mem.Contains(TagAddr(2)));
    CHECK(mem.Contains(TagAddr(3)));
    CHECK(mem.Contains(TagAddr(4)));
}

TEST_CASE("PLRU protects a recently-used line and keeps capacity") {
    MemSys mem(TestConfig(ReplacementPolicy::PLRU, 1, 4, 64));
    mem.SeedPattern();

    for (uint32_t t = 0; t < 4; t++) mem.Read(TagAddr(t));   // fill
    mem.Read(TagAddr(0));                                    // mark tag 0 recently used
    mem.Read(TagAddr(4));                                    // miss -> evicts a not-recently-used way

    CHECK(mem.Contains(TagAddr(0)));                         // pseudo-LRU spares the touched line
    CHECK(mem.Contains(TagAddr(4)));                         // newcomer resident
    CHECK(CountResident(mem, 5) == 4);                       // exactly one of 0..4 was evicted
}

TEST_CASE("Random keeps the set within capacity and counts misses") {
    MemSys mem(TestConfig(ReplacementPolicy::Random, 1, 4, 64));
    mem.SeedPattern();

    const uint32_t distinct = 10;
    for (uint32_t t = 0; t < distinct; t++) mem.Read(TagAddr(t));

    // Every distinct tag was a cold miss; the set never exceeds its ways
    CHECK(mem.GetStats().readMisses == distinct);
    CHECK(CountResident(mem, distinct) == 4);
}
