#pragma once

#include <cstdint>

class Random {
    private:
        // Number of ways
        uint8_t ways = 0;
    public:
        // Passing num_ways -> ways = reusability + flexibility
        // (no hardcoded)
        void SetWays(uint8_t num_ways);
        // const at the end = read-only
        uint8_t GetVictim() const;
};