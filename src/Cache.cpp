#include "Cache.h"
#include <iostream>
#include <format>
#include <string>
#include <cstring>
#include <bit>

// Ensure that the specs follow the guidelines
bool CacheConfig::Validate() const {
    if (numWays < 1) {
        std::cout << "Invalid cache config: numWays must be >= 1" << std::endl;
        return false;
    }
    // std::has_single_bit is true only for powers of two (C++20)
    if (numSets == 0 || !std::has_single_bit(numSets)) {
        std::cout << "Invalid cache config: numSets must be a power of two" << std::endl;
        return false;
    }
    if (lineSize == 0 || !std::has_single_bit(lineSize)) {
        std::cout << "Invalid cache config: lineSize must be a power of two" << std::endl;
        return false;
    }
    return true;
}

void CacheSet::Init(const CacheConfig& config) {
    ways = config.numWays;

    // Create 'ways' empty CacheLine objects
    lines.resize(ways);

    // Allocate the ways and size each line's data buffer to the line size
    for (uint32_t i = 0; i < lines.size(); i++) {
        lines[i].data.assign(config.lineSize, 0);
    }

    // Build the pluggable replacement algo and tell it how many ways exist
    replacement = MakeReplacement(config.policy);
    replacement->SetWays(ways);
}

CacheLine* CacheSet::Find(uint32_t tag) {
    // Iterate through each way to find the cache line
    for (uint32_t way = 0; way < ways; way++) {
        if (lines[way].tag == tag && lines[way].valid == true) {
            return &lines[way];                           // Return the value of that cache line (Hit)
        }
    }
    return nullptr;                                       // Cache Miss
}

void CacheSet::Touch(CacheLine* line) {
    // Translate the line pointer back into a way index for the replacement algo:
    // lines.data() = pointer to the first element in CacheLine*
    // line = pointing at some other element in that same vector
    // using the returned value (&lines[way]) from Find()
    // line - lines.data() = find which way got touched ( ͡° ͜ʖ ͡°)
    uint8_t way = static_cast<uint8_t>(line - lines.data());
    replacement->Touch(way);
}

CacheLine* CacheSet::Replace(uint32_t tag, uint32_t setIndex, uint8_t* src_data,
    MainMem* mainMem, const CacheConfig& config,
    uint32_t byteOffsetBits, uint32_t setIndexBits) {
    // Choosing a victim line
    uint8_t victim = replacement->GetVictim();
    CacheLine& line = lines[victim];

    // Write-back: a valid + dirty victim must be flushed to main mem before
    // we overwrite it, otherwise its modified bytes would be lost
    if (config.writeBack && line.valid && line.dirty) {
        // Rebuild the victim's start address from its tag and this set index
        uint32_t victim_addr = (line.tag << (byteOffsetBits + setIndexBits))        // Pusing the tag bits back to its orignal position (past byte + set index)
            | (setIndex << byteOffsetBits);                                         // Pushing the set bits + combine both fields into the orignal address
                                                                                    // byteOffset bits = 0 since we want the base starting address (flushing the entire cache line)
        std::string message = std::format("Flushing dirty line to main memory (start address: 0x{:x})", victim_addr);
        std::cout << message << std::endl;
        mainMem->Write(victim_addr, static_cast<uint8_t>(config.lineSize), line.data.data());
    }

    // Set new cache line attributes
    line.valid = true;
    line.dirty = false;
    line.tag = tag;

    // Copy the line-sized data from main mem into the cache line
    std::memcpy(line.data.data(), src_data, config.lineSize);

    // Notify the replacement algo that this way was just (re)filled
    replacement->Touch(victim);

    return &line;
}

void Cache::Initialize(const CacheConfig& cfg, MainMem* memory) {
    config = cfg;
    mainMem = memory;                                     // Initialize the memory pointer (saving the address)

    // Derive the decode bit widths from the sizes using log2
    // log2 accelerator: counting trailing zeroes -> equivalent log2(size)
    byteOffsetBits = static_cast<uint32_t>(std::countr_zero(config.lineSize));  
    setIndexBits = static_cast<uint32_t>(std::countr_zero(config.numSets));      

    // Build the sets
    sets.resize(config.numSets);
    for (CacheSet& set : sets) {
        set.Init(config);
    }
}

uint32_t Cache::Read(uint32_t address) {
    AddressParts addressParts(address, byteOffsetBits, setIndexBits);

    // Find the requested cache line in the chosen set
    CacheLine* line = sets[addressParts.setIndex].Find(addressParts.tag);

    // Cache hit
    if (line) {
        std::string message = std::format("Reading from cache (start address: 0x{:x}), set: {}, tag: {})",
            address, addressParts.setIndex, addressParts.tag);
        std::cout << message << std::endl;
        sets[addressParts.setIndex].Touch(line);
        // While the CPU can request various byte size, we are using 32 bits here for simplicity
        return *reinterpret_cast<uint32_t*>(&line->data[addressParts.byteOffset]);
    }
    else {
        uint32_t line_start = address & ~(config.lineSize - 1);                          // Clear the byte-offset bits to land on the line start
        std::vector<uint8_t> buffer(config.lineSize);                                    // Buffer to hold the entire cache line
        mainMem->Read(line_start, static_cast<uint8_t>(config.lineSize), buffer.data()); // Read the requested data in main mem
        CacheLine* new_line = sets[addressParts.setIndex].Replace(addressParts.tag,      // Fill the cache line in the set
            addressParts.setIndex, buffer.data(), mainMem, config, byteOffsetBits, setIndexBits);
        return *reinterpret_cast<uint32_t*>(&new_line->data[addressParts.byteOffset]);
    }
}

void Cache::Write(uint32_t address, uint32_t data) {
    AddressParts addressParts(address, byteOffsetBits, setIndexBits);

    CacheLine* line = sets[addressParts.setIndex].Find(addressParts.tag);

    // Write hit
    if (line) {
        std::string message = std::format("Writing to cache (start address: 0x{:x}), set: {}, tag: {})",
            address, addressParts.setIndex, addressParts.tag);
        std::cout << message << std::endl;
        // Reinterpret the cache memory as a 32-bit integer and store
        *reinterpret_cast<uint32_t*>(&line->data[addressParts.byteOffset]) = data;
        sets[addressParts.setIndex].Touch(line);

        if (config.writeBack) {
            // Write-back: keep the change in cache only, flag for later flush
            line->dirty = true;
        }
        else {
            // Write-through: also push the change to main mem immediately
            mainMem->Write(address, sizeof(uint32_t), reinterpret_cast<uint8_t*>(&data));
        }
        return;
    }

    // Write miss
    if (config.writeAllocate) {
        // Write-allocate: pull the line into the cache, then apply the write
        uint32_t line_start = address & ~(config.lineSize - 1);
        std::vector<uint8_t> buffer(config.lineSize);
        mainMem->Read(line_start, static_cast<uint8_t>(config.lineSize), buffer.data());
        CacheLine* new_line = sets[addressParts.setIndex].Replace(addressParts.tag,
            addressParts.setIndex, buffer.data(), mainMem, config, byteOffsetBits, setIndexBits);

        *reinterpret_cast<uint32_t*>(&new_line->data[addressParts.byteOffset]) = data;
        sets[addressParts.setIndex].Touch(new_line);

        if (config.writeBack) {
            new_line->dirty = true;
        }
        else {
            mainMem->Write(address, sizeof(uint32_t), reinterpret_cast<uint8_t*>(&data));
        }
        return;
    }

    // No-write-allocate: write straight to main mem, don't load into cache
    mainMem->Write(address, sizeof(uint32_t), reinterpret_cast<uint8_t*>(&data));
}
