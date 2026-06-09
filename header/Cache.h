#pragma once

#include "MainMem.h"
#include "ReplacementAlgo.h"
#include <vector>
#include <cstdint>
#include <iosfwd>

// User-facing cache specs. Fill this in Processor.cpp->main() and the
// cache builds itself
struct CacheConfig {
    uint32_t numSets;                                           // #Sets (power of two)
    uint32_t numWays;                                           // #Ways (>= 1)
    uint32_t lineSize;                                          // Cache line size in bytes (power of two)
    uint32_t addressBits = 32;                                  // Address width in bits
    bool writeBack = false;                                     // false = write-through, true = write-back
    bool writeAllocate = false;                                 // false = no-write-allocate, true = write-allocate
    ReplacementPolicy policy = ReplacementPolicy::Random;       // Replacement Policy

    // Profiling knobs (cycles). AMAT = hitTime + missRate * missPenalty
    uint32_t hitTime = 1;                                       // Cost of a cache hit
    uint32_t missPenalty = 100;                                 // Extra cost of going to main memory on a miss

    bool verbose = true;                                        // Gate per-access cout logging (tests set this false)

    // Method to check if the user follow the specs:
    // Powers of two required for clean bit-field address decoding &
    // associativity only needs to be >= 1. Returns false on bad specs.
    bool Validate() const;
};

// Runtime counters gathered while the cache runs, plus derived profiling
// metrics (hit/miss rate and Average Memory Access Time (AMAT))
struct CacheStats {
    uint64_t reads = 0;
    uint64_t writes = 0;
    uint64_t readHits = 0;
    uint64_t readMisses = 0;
    uint64_t writeHits = 0;
    uint64_t writeMisses = 0;
    uint64_t evictions = 0;                                     // Valid lines overwritten by a fill
    uint64_t dirtyWritebacks = 0;                               // Dirty victims flushed to main mem

    uint64_t accesses() const { return reads + writes; }
    uint64_t hits() const { return readHits + writeHits; }
    uint64_t misses() const { return readMisses + writeMisses; }

    double hitRate() const;                                     // hits / accesses (0 if no accesses)
    double missRate() const;                                    // 1 - hitRate
    double amat(uint32_t hitTime, uint32_t missPenalty) const;  // Average Memory Access Time (cycles)

    void Report(uint32_t hitTime, uint32_t missPenalty) const; // Pretty-print the profile
};

// Cache Line = [tag][data][valid][dirty]
struct CacheLine {
    uint32_t tag = 0;
    std::vector<uint8_t> data;                      // Sized to lineSize at init
    bool valid = false;
    bool dirty = false;                             // Used by write-back to flag pending flush
};

// Address decoding = [tag][setIndex][byteOffset]
// Bit widths are derived from the config at runtime instead of being fixed
struct AddressParts {
    uint32_t tag;
    uint32_t setIndex;
    uint32_t byteOffset;

    // Decoding stage using runtime bit widths
    AddressParts(uint32_t address, uint32_t byteOffsetBits, uint32_t setIndexBits) {
        byteOffset = address & ((1 << byteOffsetBits) - 1);       // Lower byteOffsetBits mask
        setIndex = (address >> byteOffsetBits)                    // Remove the byte-offset bits
            & ((1 << setIndexBits) - 1);                          // Lower setIndexBits mask
        tag = address >> (byteOffsetBits + setIndexBits);         // Keep the remaining bits
    }
};

class CacheSet {
    private:
        std::vector<CacheLine> lines;                             // Runtime-sized array of cache lines
        std::unique_ptr<ReplacementAlgo> replacement;             // Pluggable replacement algo
        uint32_t ways = 0;
    public:
        // Pass the configs to the config function
        void Init(const CacheConfig& config);

        // Return a pointer to that cache line if hit
        CacheLine* Find(uint32_t tag);                            

        // Read-only presence check: true if the tag is cached, with NO side
        // effects (no Touch / fill / stats). Used by the inspection API so
        // tests can probe state without disturbing replacement order
        bool Contains(uint32_t tag) const;

        // Return a pointer to the line that was (re)filled. setIndex is needed
        // so write-back can rebuild a victim's address before flushing it.
        // stats is updated with evictions / dirty write-backs as they happen
        CacheLine* Replace(uint32_t tag, uint32_t setIndex, uint8_t* src_data,
            MainMem* mainMem, const CacheConfig& config,
            uint32_t byteOffsetBits, uint32_t setIndexBits, CacheStats& stats);

        // Notify the replacement algo that a way was accessed 
        //(for other replacement algos like LRU/FIFO)
        void Touch(CacheLine* line);

        // Structural debug dump of this set's ways (used by Cache::Dump)
        void Dump(uint32_t setIndex, std::ostream& os) const;
};

class Cache {
    private:
        std::vector<CacheSet> sets;                               // Runtime-sized array of cache sets
        MainMem* mainMem = nullptr;                               // Pointer to main mem in case of cache miss
        CacheConfig config;                                       // Cache specification
        uint32_t byteOffsetBits = 0;                              // log2(lineSize)
        uint32_t setIndexBits = 0;                                // log2(numSets)
        CacheStats stats;                                         // Runtime profiling counters
    public:
        // Store config & compute decode bit widths & build the sets
        void Initialize(const CacheConfig& cfg, MainMem* memory);
        uint32_t Read(uint32_t address);                          // Return data to CPU
        void Write(uint32_t address, uint32_t data);              // Doesn't need to return to CPU

        // Profiling
        const CacheStats& GetStats() const { return stats; }     // Raw counters + derived metrics
        const CacheConfig& GetConfig() const { return config; }  // Access hitTime / missPenalty etc.
        bool Contains(uint32_t address) const;                   // Non-mutating presence check
        void Dump(std::ostream& os) const;                       // Per-set [valid, dirty, tag] listing
        void PrintStats() const;                                 // GetStats().Report(...) convenience
};
