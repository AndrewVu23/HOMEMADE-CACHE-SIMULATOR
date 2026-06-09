#pragma once

#include <cstdint>
#include <memory>
#include <vector>

// Selects which algo a cache set uses.
// Add new entries here as more algorithms are implemented
enum class ReplacementPolicy {
    Random,
    LRU,        // Least Recently Used 
    FIFO,       // First In First Out
    PLRU        // Tree-based pseudo-LRU (requires power-of-two ways)
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

        // Notify the algo that a way was just accessed (a cache hit).
        // Stateful algorithms (LRU/PLRU) use this -> the default is a no-op
        // so stateless algos (like Random) and access-agnostic algos (FIFO)
        // need not override it.
        virtual void Touch(uint8_t way) { (void)way; }

        // Notify the algo that a way was just (re)filled with a new line.
        // The default delegates to Touch so recency-based algos (LRU/PLRU)
        // treat a fill like an access. FIFO overrides this to record the
        // insertion order while leaving Touch a no-op (accesses don't reorder)
        virtual void Insert(uint8_t way) { Touch(way); }
};

// Random replacement: evicts a uniformly random way
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

// Least Recently Used: evicts the way whose last access is oldest
// order is kept front = most-recently-used, back = least-recently-used
class LRU : public ReplacementAlgo {
    private:
        uint32_t ways = 0;
        std::vector<uint8_t> order;
    public:
        void SetWays(uint32_t num_ways) override;
        uint8_t GetVictim() override;
        void Touch(uint8_t way) override;       // Insert() delegates here too
};

// First In First Out: evicts the way that was filled longest ago,
// regardless of how recently it was accessed.
// order is kept front = oldest insertion, back = newest insertion
class FIFO : public ReplacementAlgo {
    private:
        uint32_t ways = 0;
        std::vector<uint8_t> order;
    public:
        void SetWays(uint32_t num_ways) override;
        uint8_t GetVictim() override;
        void Insert(uint8_t way) override;      // record insertion order
        // Touch is intentionally left as the base no-op (accesses don't reorder)
};

// Tree-based pseudo-LRU: approximates LRU with (ways - 1) bits arranged as a
// binary tree. Each bit points to the subtree to evict from. Requires the
// number of ways to be a power of two.
class PLRU : public ReplacementAlgo {
    private:
        uint32_t ways = 0;
        // Heap-indexed internal nodes 1..ways-1 (index 0 unused).
        // bit == 0 -> victim lives in the left subtree, 1 -> right subtree.
        std::vector<uint8_t> tree;
    public:
        void SetWays(uint32_t num_ways) override;
        uint8_t GetVictim() override;
        void Touch(uint8_t way) override;       // Insert() delegates here too
};

// Factory: build a replacement algo from a policy enum
std::unique_ptr<ReplacementAlgo> MakeReplacement(ReplacementPolicy policy);
