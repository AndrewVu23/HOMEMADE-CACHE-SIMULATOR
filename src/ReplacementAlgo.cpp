#include "ReplacementAlgo.h"
#include <cstdlib>

void Random::SetWays(uint32_t num_ways) {
    // Set number of ways
    ways = num_ways;
}

uint8_t Random::GetVictim() {
    // Choose a random victim based on the number of ways
    // The result can only be 0 -> (ways - 1)
    return static_cast<uint8_t>(rand() % ways);
}

// Choose the algos
std::unique_ptr<ReplacementAlgo> MakeReplacement(ReplacementPolicy policy) {
    switch (policy) {
        case ReplacementPolicy::Random:
        default:
            return std::make_unique<Random>();
    }
}
