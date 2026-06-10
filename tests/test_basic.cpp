#include "doctest.h"
#include "Processor.h"
#include "test_helpers.h"

// Read miss then hit, with value correctness via the address pattern fill.

TEST_CASE("Read miss then hit returns pattern data and updates stats") {
    MemSys mem(TestConfig());
    mem.SeedPattern();                               // memory[i] = i & 0xFF

    // First read: cold miss, pulls the line from main memory
    uint32_t v0 = mem.Read(0x20);
    // value = little-endian bytes 0x20,0x21,0x22,0x23 -> 0x23222120
    CHECK(v0 == 0x23222120u);
    CHECK(mem.GetStats().readMisses == 1);
    CHECK(mem.GetStats().readHits == 0);

    // Second read in the same 64B line: hit
    uint32_t v1 = mem.Read(0x24);
    CHECK(v1 == 0x27262524u);
    CHECK(mem.GetStats().readHits == 1);
    CHECK(mem.GetStats().readMisses == 1);

    // Both addresses now resolve in cache without side effects
    CHECK(mem.Contains(0x20));
    CHECK(mem.Contains(0x24));
}

TEST_CASE("Contains does not perturb cache state") {
    MemSys mem(TestConfig());
    mem.SeedPattern();
    mem.Read(0x20);

    // Probing many times must not change counters or evict anything
    for (int i = 0; i < 100; i++) {
        CHECK(mem.Contains(0x20));
        CHECK_FALSE(mem.Contains(0x40));
    }
    CHECK(mem.GetStats().accesses() == 1);           // only the single Read above
}
