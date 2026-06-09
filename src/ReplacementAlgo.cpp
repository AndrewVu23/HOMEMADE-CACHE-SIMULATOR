#include "ReplacementAlgo.h"
#include <cstdlib>
#include <algorithm>

// ----------------------------- Random -----------------------------

void Random::SetWays(uint32_t num_ways) {
    // Set number of ways
    ways = num_ways;
}

uint8_t Random::GetVictim() {
    // Choose a random victim based on the number of ways
    // The result can only be 0 -> (ways - 1)
    return static_cast<uint8_t>(rand() % ways);
}

// ------------------------------ LRU -------------------------------

void LRU::SetWays(uint32_t num_ways) {
    ways = num_ways;
    order.clear();
}

uint8_t LRU::GetVictim() {
    // Prefer an empty (never-filled) way so we don't evict a live line
    // while free slots remain
    if (order.size() < ways) {
        for (uint8_t way = 0; way < ways; way++) {
            if (std::find(order.begin(), order.end(), way) == order.end()) {
                return way;
            }
        }
    }
    // Set is full -> evict the least-recently-used way (back of the list)
    return order.back();
}

void LRU::Touch(uint8_t way) {
    // Move this way to the front (most-recently-used)
    order.erase(std::remove(order.begin(), order.end(), way), order.end());
    order.insert(order.begin(), way);
}

// ------------------------------ FIFO ------------------------------

void FIFO::SetWays(uint32_t num_ways) {
    ways = num_ways;
    order.clear();
}

uint8_t FIFO::GetVictim() {
    // Prefer an empty (never-filled) way first
    if (order.size() < ways) {
        for (uint8_t way = 0; way < ways; way++) {
            if (std::find(order.begin(), order.end(), way) == order.end()) {
                return way;
            }
        }
    }
    // Set is full -> evict the oldest insertion (front of the queue)
    return order.front();
}

void FIFO::Insert(uint8_t way) {
    // Record insertion order: drop any stale entry, then enqueue as newest
    // Accesses (Touch) deliberately do NOT change this order
    order.erase(std::remove(order.begin(), order.end(), way), order.end());
    order.push_back(way);
}

// ------------------------------ PLRU ------------------------------

void PLRU::SetWays(uint32_t num_ways) {
    ways = num_ways;
    // (ways - 1) internal nodes, heap-indexed 1..ways-1; index 0 is unused
    tree.assign(ways, 0);
}

uint8_t PLRU::GetVictim() {
    // Walk from the root following each bit to the subtree it points at,
    // until we reach a leaf. Leaves are heap indices ways..2*ways-1
    uint32_t node = 1;
    while (node < ways) {
        node = (tree[node] == 0) ? (2 * node) : (2 * node + 1);
    }
    return static_cast<uint8_t>(node - ways);
}

void PLRU::Touch(uint8_t way) {
    // Mark this way as recently used by flipping every ancestor pointer to
    // point away from the path we just took (toward the other subtree)
    uint32_t node = way + ways;          // leaf heap index for this way
    while (node > 1) {
        uint32_t parent = node / 2;
        // If we came up from the left child, point the parent right (1),
        // otherwise point it left (0)
        tree[parent] = (node == 2 * parent) ? 1 : 0;
        node = parent;
    }
}

// ----------------------------- Factory ----------------------------

// Choose the algos
std::unique_ptr<ReplacementAlgo> MakeReplacement(ReplacementPolicy policy) {
    switch (policy) {
        case ReplacementPolicy::LRU:
            return std::make_unique<LRU>();
        case ReplacementPolicy::FIFO:
            return std::make_unique<FIFO>();
        case ReplacementPolicy::PLRU:
            return std::make_unique<PLRU>();
        case ReplacementPolicy::Random:
        default:
            return std::make_unique<Random>();
    }
}
