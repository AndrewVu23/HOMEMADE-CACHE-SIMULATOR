#include "Cache.h"
#include <iostream>
#include <format>
#include <string>
#include <cstring>

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
    mainMem = memory;                                     // Initialize the memory pointer (saving the address)     
}

uint32_t Cache::Read(uint32_t address) {
    AddressParts addressParts(address);

    // Find the requested cache line
    // Keep in mind that set is an array containing CacheSet as the data,
    // which further contains Find as the method which returns a pointer to CacheLine.
    // Right above, we can see the Find() method iterates through each way to compare
    // the tags & valid bits, and as mentioned before, we choose a set from the set array
    // then check the cache line, which is what happens here
    CacheLine* line = sets[addressParts.setIndex].Find(addressParts.tag);

    // Cache hit
    if (line) {
        std::string message = std::format("Reading from cache (start address: 0x{:x}), set: {}, tag: {})", 
            address, addressParts.setIndex, addressParts.tag);
        std::cout << message << std::endl;
        // While the CPU can request various byte size, we are using 32 bits here for simplicity
        // Remember that data is stored as 1-byte chunk in an array, so to read 32 bits,
        // we need to use pointer reinterpretation.
        // &line->data[addressParts.byteOffset] = line goes to data and grab the starting memory address using byteOffset
        // *reinterpret_cast<uint32_t*> forces the compiler to look at a 4-byte chunk instead of just the starting-address chunk.
        // Then we dereference the pointer to get the 4-byte value, starting from the starting-address chunk.
        return *reinterpret_cast<uint32_t*>(&line->data[addressParts.byteOffset]);
    }
    else {
        uint32_t line_start = address & ~(CACHE_LINE_SIZE - 1);                                     // Clear the 6 LSBs since each cache line is 64 bytes apart
        std::array<uint8_t, CACHE_LINE_SIZE> buffer;                                                // Create a buffer to whole the entire cache line
        mainMem->Read(line_start, CACHE_LINE_SIZE, buffer.data());                                  // Read the requested data in main mem
        CacheLine* new_line = sets[addressParts.setIndex].Replace(addressParts.tag, buffer.data()); // Replace the cache line in the set
        return *reinterpret_cast<uint32_t*>(&new_line->data[addressParts.byteOffset]);
    }
}
 
void Cache::Write(uint32_t address, uint32_t data) {
    AddressParts addressParts(address);

    CacheLine* line = sets[addressParts.setIndex].Find(addressParts.tag);
    
    // We are using write-through & no-write-allocate policies
    if (line) {
        std::string message = std::format("Writing to cache (start address: 0x{:x}), set: {}, tag: {})", 
            address, addressParts.setIndex, addressParts.tag);
        std::cout << message << std::endl;
        // Reinterpret the cache memory as a 32-bit integer
        *reinterpret_cast<uint32_t*>(&line->data[addressParts.byteOffset]) = data;
    }
    // Write-through & no-write-allocate policies
    // The CPU will write straight to main mem regardless of a hit/miss
    // The std::memcpy in the Write() method runs an implicit loop where it will
    // read the 1-byte data 4 times until it reaches 32 bits since we declared the
    // size as sizeof(uint32_t).
    // reinterpret_cast<uint8_t*>(&data) will chops off 1 byte each time in this loop
    // to fit the 1-byte slot in main mem
    mainMem->Write(address, sizeof(uint32_t), reinterpret_cast<uint8_t*>(&data));
}