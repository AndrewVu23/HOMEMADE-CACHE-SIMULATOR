#pragma once

#include <cstdint>
#include <memory>

// Selects which algo a cache set uses.
// Add new entries here as more algorithms are implemented
enum class ReplacementPolicy {
    Random
};

// Abstract interface every replacement algorithm implements
// The cache only ever talks to this base class, so swapping or
// adding algorithms never touches the cache logic
class ReplacementAlgo {
    public:
        virtual ~ReplacementAlgo() = default;

        // Tell the algo how many ways the set has
        virtual void SetWays(uint32_t num_ways) = 0;

        // Pick the way index to evict
        virtual uint8_t GetVictim() = 0;

        // Notify the algo that a way was just accessed
        // Stateful algorithms (LRU/FIFO) use this -> the default is a no-op
        // so stateless algos (like Random) need not override it.
        virtual void Touch(uint8_t way) { (void)way; }
};

// Random replacement: evicts a uniformly random way.
class Random : public ReplacementAlgo {
    private:
        // #Ways
        uint32_t ways = 0;
    public:
        // Passing num_ways -> ways = reusability + flexibility
        // (no hardcoded)
        void SetWays(uint32_t num_ways) override;
        uint8_t GetVictim() override;
};

// Factory: build a replacement algo from a policy enum
std::unique_ptr<ReplacementAlgo> MakeReplacement(ReplacementPolicy policy);
