#pragma once

#include "MainMem.h"
#include "ReplacementAlgo.h"
#include <array>

const uint8_t MEMORY_ADDRESS_SIZE = 32;                         // 32-bit memory addresses
const uint8_t CACHE_LINE_SIZE = 64;                             // 64-byte cache lines
const uint8_t CACHE_SETS = 64;                                  // 64 sets
const uint8_t CACHE_WAYS = 4;                                   // 4-way set-associative cache

const uint8_t CACHE_LINE_BYTE_OFFSET_SIZE = 6;                  // 64 bytes -> 6-bit indexing 
const uint8_t CACHE_LINE_SET_INDEX_SIZE = 6;                    // 64 sets -> 6-bit indexing
const uint8_t CACHE_LINE_TAG_SIZE = 20;                         // Remaining bits

// Cache Line = [tag][data][valid] = [20 bits][64 bytes][1 bit]
struct CacheLine {
    uint32_t tag = 0;
    std::array<uint8_t, CACHE_LINE_SIZE> data;
    bool valid = false;
};

// Address decoding = [tag][setIndex][byteOffset] = [20 bits][6 bits][6 bits] = 32-bit address
struct AddressParts {
    uint32_t tag;
    uint8_t setIndex;
    uint32_t byteOffset;
    
    // Decoding stage
    AddressParts(uint32_t address) {
        byteOffset = address & (CACHE_LINE_SIZE - 1);            // 63 = 8'b00111111 = lower 6-bit mask
        setIndex = (address >> CACHE_LINE_BYTE_OFFSET_SIZE)      // Remove the byte-offset bits
            & ((1 << CACHE_LINE_SET_INDEX_SIZE) - 1);            // 8'b01000000 = 64. 64 - 1 = lower 6-bit mask again
        tag = address >> (CACHE_LINE_BYTE_OFFSET_SIZE            // Keep the remaining bits
            + CACHE_LINE_SET_INDEX_SIZE);
    }
};

class CacheSet {
    private: 
        std::array<CacheLine, CACHE_WAYS> lines;                 // Array of cache lines
        Random replacement;                                      // Random Replacement Algo
    public:
        CacheSet();
        ~CacheSet();

        CacheLine* Find(uint32_t tag);                           // Return a pointer to that cache line if hit
        CacheLine* Replace(uint32_t tag, uint8_t* src_data);     // Return a pointer to the cache line which is going
                                                                 // to be replaced (address of src_data is also passed in)
};

class Cache {
    private:
        std::array<CacheSet, CACHE_SETS> sets;                   // Array of cache sets
        MainMem* MainMem;                                        // Pointer to main mem in case of cache miss   
    public:
        void Initialize(MainMem* memory);
        uint32_t Read(uint32_t address);                         // Return address to CPU
        void Write(uint32_t address, uint32_t data);             // Doesn't need to return address to CPU
};