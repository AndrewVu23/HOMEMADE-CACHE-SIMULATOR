#include "doctest.h"
#include "Processor.h"
#include "test_helpers.h"

// Direct-mapped (1 set, 1 way) makes eviction fully deterministic: any new tag
// evicts the single resident line. Perfect for isolating write-policy behavior.

TEST_CASE("Write-through updates memory immediately and never flushes") {
    CacheConfig cfg = TestConfig(ReplacementPolicy::LRU, 1, 1, 64,
                                 /*writeBack=*/false, /*writeAllocate=*/false);
    MemSys mem(cfg);
    mem.SeedPattern();

    mem.Read(0x0);                                   // cache tag 0
    mem.Write(0x0, 0xAABBCCDD);                      // write hit -> write-through to memory
    CHECK(mem.GetStats().dirtyWritebacks == 0);      // write-through never marks dirty

    mem.Read(0x40);                                  // evict tag 0 (clean -> no flush)
    CHECK(mem.GetStats().dirtyWritebacks == 0);

    // tag 0 was evicted; reading it again comes from memory and already has the value
    CHECK(mem.Read(0x0) == 0xAABBCCDD);
}

TEST_CASE("Write-back defers the store until the dirty line is evicted") {
    CacheConfig cfg = TestConfig(ReplacementPolicy::LRU, 1, 1, 64,
                                 /*writeBack=*/true, /*writeAllocate=*/false);
    MemSys mem(cfg);
    mem.SeedPattern();

    mem.Read(0x0);                                   // cache tag 0
    mem.Write(0x0, 0xAABBCCDD);                      // write hit -> dirty, memory NOT updated yet
    CHECK(mem.GetStats().dirtyWritebacks == 0);

    mem.Read(0x40);                                  // evict the dirty tag 0 -> flush to memory
    CHECK(mem.GetStats().dirtyWritebacks == 1);
    CHECK(mem.GetStats().evictions == 1);

    // The flushed value is now in memory
    CHECK(mem.Read(0x0) == 0xAABBCCDD);
}

TEST_CASE("Write-allocate pulls the line in on a write miss") {
    CacheConfig cfg = TestConfig(ReplacementPolicy::LRU, 1, 4, 64,
                                 /*writeBack=*/false, /*writeAllocate=*/true);
    MemSys mem(cfg);
    mem.SeedPattern();

    CHECK_FALSE(mem.Contains(0x80));                 // not cached yet
    mem.Write(0x80, 0x11223344);                     // write miss -> allocate
    CHECK(mem.GetStats().writeMisses == 1);
    CHECK(mem.Contains(0x80));                        // now resident
    CHECK(mem.Read(0x80) == 0x11223344);             // and reads as a hit
    CHECK(mem.GetStats().readHits == 1);
}

TEST_CASE("No-write-allocate leaves the cache untouched on a write miss") {
    CacheConfig cfg = TestConfig(ReplacementPolicy::LRU, 1, 4, 64,
                                 /*writeBack=*/false, /*writeAllocate=*/false);
    MemSys mem(cfg);
    mem.SeedPattern();

    CHECK_FALSE(mem.Contains(0x80));
    mem.Write(0x80, 0x11223344);                     // write miss -> straight to memory
    CHECK(mem.GetStats().writeMisses == 1);
    CHECK_FALSE(mem.Contains(0x80));                  // still not cached

    // The value reached memory; reading it is a miss that returns the written data
    CHECK(mem.Read(0x80) == 0x11223344);
    CHECK(mem.GetStats().readMisses == 1);
}
