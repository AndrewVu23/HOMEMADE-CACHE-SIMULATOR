#pragma once

#include "MainMem.h"
#include "Cache.h"

class MemSys {
    private:
        MainMem mainMem;
        Cache cache;
    public:
        MemSys(const CacheConfig& config);
        ~MemSys();

        uint32_t Read(uint32_t address);
        void Write(uint32_t address, uint32_t data);
        void LoadMainMem(const std::string& path);
        void SeedPattern();                                  // Fill main mem with value == address & 0xFF
        void PrintMainMem();

        // Profiling pass-throughs to the cache
        bool Contains(uint32_t address) const;               // Is this address currently cached? (no side effects)
        const CacheStats& GetStats() const;                  // Raw counters + derived metrics
        void PrintStats() const;                             // Print the profiling report
        void DumpCache() const;                              // Print per-set cache contents
};
