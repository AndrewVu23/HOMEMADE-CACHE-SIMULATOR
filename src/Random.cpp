#include "Random.h"
#include <cstdlib>

void Random::SetWays(uint8_t num_ways) {
    // Set number of ways
    ways = num_ways;
}

uint8_t Random::GetVictim() const {
    // Choose a random victim based
    // on the number of ways
    return rand() % ways;
}