#pragma once

#include "Processor.h"
#include "Cache.h"

// Build a quiet CacheConfig for tests (verbose=false keeps doctest output clean).
// Defaults give a single-set, 4-way cache so different tags collide in one set,
// which makes replacement-policy behavior easy to control and assert.
inline CacheConfig TestConfig(
    ReplacementPolicy policy = ReplacementPolicy::LRU,
    uint32_t numSets = 1,
    uint32_t numWays = 4,
    uint32_t lineSize = 64,
    bool writeBack = false,
    bool writeAllocate = false) {
    CacheConfig cfg{};
    cfg.numSets = numSets;
    cfg.numWays = numWays;
    cfg.lineSize = lineSize;
    cfg.addressBits = 32;
    cfg.writeBack = writeBack;
    cfg.writeAllocate = writeAllocate;
    cfg.policy = policy;
    cfg.hitTime = 1;
    cfg.missPenalty = 100;
    cfg.verbose = false;
    return cfg;
}

// With a single set and 64-byte lines, address = tag * lineSize selects a
// distinct line that maps to set 0. Handy for controlled eviction tests.
inline uint32_t TagAddr(uint32_t tag, uint32_t lineSize = 64) {
    return tag * lineSize;
}
