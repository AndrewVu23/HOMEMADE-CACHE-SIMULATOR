#include "Cache.h"

CacheSet::CacheSet() {
    // Set number of ways in the replacement algo
    replacement.SetWays(CACHE_WAYS);
}

CacheSet::~CacheSet() {

}

// This particular method assumes that we have 1 set and CACHE_WAYS ways
// This is reasonable since we are going to pick the exact set using
// setIndex bits first before iterating through this loop
CacheLine* CacheSet::Find(uint32_t tag) {
    // Iterate through each way to find the cache line
    for (uint8_t way = 0; way < CACHE_WAYS; way++) {
        if (lines[way].tag == tag && lines[way].valid == true) {
            return &lines[way];                           // Return the value of that cache line (Hit)
        }
    }
    return nullptr;                                       // Cache Miss
}

CacheLine* CacheSet::Replace(uint32_t tag, uint8_t* src_data) {
    // Choosing a victim line
    uint8_t victim = replacement.GetVictim();

    // Set new cache line attributes
    lines[victim].valid = true;
    lines[victim].tag = tag;

    // Copy 64-byte data from main mem to the new space in the cache line
    std::memcpy(lines[victim].data.data(), src_data, CACHE_LINE_SIZE);

    return &lines[victim];
}

void Cache::Initialize(MainMem* memory) {
    MainMem = memory;                                     // Initialize the memory pointer (saving the address)     
}

void Cache::Read(uint32_t address) {
    AddressParts AddressParts(address);

    // Find the requested cache line
    // Keep in mind that set is an array containing CacheSet as the data,
    // which further contains Find as the method which returns a pointer to CacheLine.
    // Right above, we can see the Find() method iterates through each way to compare
    // the tags & valid bits, and as mentioned before, we choose a set from the set array
    // then check the cache line, which is what happens here
    CacheLine* line = sets[AddressParts.setIndex].Find(AddressParts.tag);

    // Cache hit
    if (line) {
        // While the CPU can request various byte size, we are using 32 bits here for simplicity
        // Remember that data is stored as 1-byte chunk in an array, so to read 32 bits,
        // we need to use pointer reinterpretation.
        // &line->data[AddressParts.byteOffset] = line goes to data and grab the starting memory address using byteOffset
        // *reinterpret_cast<uint32_t*> forces the compiler to look at a 4-byte chunk instead of just the starting-address chunk.
        // Then we dereference the pointer to get the 4-byte value, starting from the starting-address chunk.
        return *reinterpret_cast<uint32_t*>(&line->data[AddressParts.byteOffset]);
    }
    else {

    }
}

void Cache::Write(uint32_t address, uint32_t data) {

}