#include "Cache.h"
#include <iostream>
#include <ostream>
#include <format>
#include <string>
#include <cstring>
#include <bit>

// --------------------------- CacheStats ---------------------------

double CacheStats::hitRate() const {
    uint64_t total = accesses();
    return (total == 0) ? 0.0 : static_cast<double>(hits()) / static_cast<double>(total);
}

double CacheStats::missRate() const {
    uint64_t total = accesses();
    return (total == 0) ? 0.0 : static_cast<double>(misses()) / static_cast<double>(total);
}

double CacheStats::amat(uint32_t hitTime, uint32_t missPenalty) const {
    // Average Memory Access Time = hit time + miss rate * miss penalty
    return static_cast<double>(hitTime) + missRate() * static_cast<double>(missPenalty);
}

void CacheStats::Report(uint32_t hitTime, uint32_t missPenalty) const {
    std::cout << "==================== Cache Profile ====================" << std::endl;
    std::cout << std::format("Accesses        : {} (reads: {}, writes: {})", accesses(), reads, writes) << std::endl;
    std::cout << std::format("Hits            : {} (read: {}, write: {})", hits(), readHits, writeHits) << std::endl;
    std::cout << std::format("Misses          : {} (read: {}, write: {})", misses(), readMisses, writeMisses) << std::endl;
    std::cout << std::format("  compulsory    : {}", compulsoryMisses) << std::endl;
    std::cout << std::format("  capacity      : {}", capacityMisses) << std::endl;
    std::cout << std::format("  conflict      : {}", conflictMisses) << std::endl;
    std::cout << std::format("Hit rate        : {:.2f}%", hitRate() * 100.0) << std::endl;
    std::cout << std::format("Miss rate       : {:.2f}%", missRate() * 100.0) << std::endl;
    std::cout << std::format("Evictions       : {}", evictions) << std::endl;
    std::cout << std::format("Dirty writebacks: {}", dirtyWritebacks) << std::endl;
    std::cout << std::format("AMAT            : {:.2f} cycles (hitTime={}, missPenalty={})",
        amat(hitTime, missPenalty), hitTime, missPenalty) << std::endl;
    std::cout << "=======================================================" << std::endl;
}

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
    // Tree-based PLRU indexes a binary tree of ways, so it only works when the
    // number of ways is a power of two
    if (policy == ReplacementPolicy::PLRU && !std::has_single_bit(numWays)) {
        std::cout << "Invalid cache config: PLRU requires numWays to be a power of two" << std::endl;
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

bool CacheSet::Contains(uint32_t tag) const {
    // Same scan as Find() but const and side-effect free (no Touch / stats)
    for (uint32_t way = 0; way < ways; way++) {
        if (lines[way].valid && lines[way].tag == tag) {
            return true;
        }
    }
    return false;
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

void CacheSet::Dump(uint32_t setIndex, std::ostream& os) const {
    for (uint32_t way = 0; way < ways; way++) {
        const CacheLine& line = lines[way];
        if (!line.valid) {
            continue;                                    // Skip empty ways to keep the dump readable
        }
        os << std::format("  set {:>4} | way {:>2} | valid={} dirty={} tag=0x{:x}",
            setIndex, way, line.valid ? 1 : 0, line.dirty ? 1 : 0, line.tag) << std::endl;
    }
}

CacheLine* CacheSet::Replace(uint32_t tag, uint32_t setIndex, uint8_t* src_data,
    MainMem* mainMem, const CacheConfig& config,
    uint32_t byteOffsetBits, uint32_t setIndexBits, CacheStats& stats) {
    // Choosing a victim line
    uint8_t victim = replacement->GetVictim();
    CacheLine& line = lines[victim];

    // Overwriting a valid line counts as an eviction
    if (line.valid) {
        stats.evictions++;
    }

    // Write-back: a valid + dirty victim must be flushed to main mem before
    // we overwrite it, otherwise its modified bytes would be lost
    if (config.writeBack && line.valid && line.dirty) {
        // Rebuild the victim's start address from its tag and this set index
        uint32_t victim_addr = (line.tag << (byteOffsetBits + setIndexBits))        // Pusing the tag bits back to its orignal position (past byte + set index)
            | (setIndex << byteOffsetBits);                                         // Pushing the set bits + combine both fields into the orignal address
                                                                                    // byteOffset bits = 0 since we want the base starting address (flushing the entire cache line)
        stats.dirtyWritebacks++;
        if (config.verbose) {
            std::string message = std::format("Flushing dirty line to main memory (start address: 0x{:x})", victim_addr);
            std::cout << message << std::endl;
        }
        mainMem->Write(victim_addr, static_cast<uint8_t>(config.lineSize), line.data.data());
    }

    // Set new cache line attributes
    line.valid = true;
    line.dirty = false;
    line.tag = tag;

    // Copy the line-sized data from main mem into the cache line
    std::memcpy(line.data.data(), src_data, config.lineSize);

    // Notify the replacement algo that this way was just (re)filled.
    // Insert() (not Touch()) so FIFO records insertion order while ignoring
    // later accesses; recency algos (LRU/PLRU) treat the fill as an access.
    replacement->Insert(victim);

    return &line;
}

void Cache::Initialize(const CacheConfig& cfg, MainMem* memory) {
    config = cfg;
    mainMem = memory;                                     // Initialize the memory pointer (saving the address)
    mainMem->SetVerbose(config.verbose);                  // Propagate logging preference to main mem
    stats = CacheStats{};                                 // Fresh counters for this configuration

    // Derive the decode bit widths from the sizes using log2
    // log2 accelerator: counting trailing zeroes -> equivalent log2(size)
    byteOffsetBits = static_cast<uint32_t>(std::countr_zero(config.lineSize));  
    setIndexBits = static_cast<uint32_t>(std::countr_zero(config.numSets));      

    // Build the sets
    sets.resize(config.numSets);
    for (CacheSet& set : sets) {
        set.Init(config);
    }

    // Reset the shadow cache models
    seenBlocks.clear();
    shadowOrder.clear();
    shadowIndex.clear();
    shadowCapacity = static_cast<size_t>(config.numSets) * config.numWays;
}

// Shadow cache model running in parallel
void Cache::Account(uint32_t blockAddr, bool isMiss, bool installs) {
    if (isMiss) {
        if (seenBlocks.find(blockAddr) == seenBlocks.end()) {
            stats.compulsoryMisses++;                    // Never seen this block before -> cold miss
        }
        else if (shadowIndex.find(blockAddr) == shadowIndex.end()) {
            stats.capacityMisses++;                      // A same-size fully-assoc cache would miss too -> capacity
        }
        else {
            stats.conflictMisses++;                      // Fully-assoc still holds it -> conflicts
        }
    }
    seenBlocks.insert(blockAddr);

    // Advance the fully-associative LRU shadow. A block already present is moved
    // to MRU (a hit, regardless of installs); an absent block is filled only when
    // the real cache would install it, evicting the shadow's LRU block if full
    auto it = shadowIndex.find(blockAddr);
    if (it != shadowIndex.end()) {                      // Cache hit (since C++ maps return .end() if .find() fails)
        shadowOrder.erase(it->second);                  // First = Key (or uint32_t blockAddr); Second = Value (list::iterator)
        shadowOrder.push_front(blockAddr);              // MRU
        it->second = shadowOrder.begin();               // list::iterator now points to the MRU block
    }
    else if (installs && shadowCapacity > 0) {          
        shadowOrder.push_front(blockAddr);              // MRU
        shadowIndex[blockAddr] = shadowOrder.begin();   // Create a link pointing to the new MRU block
        if (shadowIndex.size() > shadowCapacity) {      // Shadow cache overflowing -> eviction
            uint32_t lru = shadowOrder.back();          // LRU policy
            shadowOrder.pop_back();                     // Cut the victim off from the list
            shadowIndex.erase(lru);                     // Erase the victim from the map
        }
    }
}

uint32_t Cache::Read(uint32_t address) {
    AddressParts addressParts(address, byteOffsetBits, setIndexBits);
    stats.reads++;
    uint32_t blockAddr = address >> byteOffsetBits;

    // Find the requested cache line in the chosen set
    CacheLine* line = sets[addressParts.setIndex].Find(addressParts.tag);

    // Cache hit
    if (line) {
        stats.readHits++;
        Account(blockAddr, /*isMiss=*/false, /*installs=*/true);
        if (config.verbose) {
            std::string message = std::format("Reading from cache (start address: 0x{:x}), set: {}, tag: {})",
                address, addressParts.setIndex, addressParts.tag);
            std::cout << message << std::endl;
        }
        sets[addressParts.setIndex].Touch(line);
        // While the CPU can request various byte size, we are using 32 bits here for simplicity
        return *reinterpret_cast<uint32_t*>(&line->data[addressParts.byteOffset]);
    }
    else {
        stats.readMisses++;
        Account(blockAddr, /*isMiss=*/true, /*installs=*/true);                           // A read always fills the line
        uint32_t line_start = address & ~(config.lineSize - 1);                          // Clear the byte-offset bits to land on the line start
        std::vector<uint8_t> buffer(config.lineSize);                                    // Buffer to hold the entire cache line
        mainMem->Read(line_start, static_cast<uint8_t>(config.lineSize), buffer.data()); // Read the requested data in main mem
        CacheLine* new_line = sets[addressParts.setIndex].Replace(addressParts.tag,      // Fill the cache line in the set
            addressParts.setIndex, buffer.data(), mainMem, config, byteOffsetBits, setIndexBits, stats);
        return *reinterpret_cast<uint32_t*>(&new_line->data[addressParts.byteOffset]);
    }
}

void Cache::Write(uint32_t address, uint32_t data) {
    AddressParts addressParts(address, byteOffsetBits, setIndexBits);
    stats.writes++;
    uint32_t blockAddr = address >> byteOffsetBits;

    CacheLine* line = sets[addressParts.setIndex].Find(addressParts.tag);

    // Write hit
    if (line) {
        stats.writeHits++;
        Account(blockAddr, /*isMiss=*/false, /*installs=*/true);
        if (config.verbose) {
            std::string message = std::format("Writing to cache (start address: 0x{:x}), set: {}, tag: {})",
                address, addressParts.setIndex, addressParts.tag);
            std::cout << message << std::endl;
        }
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
    stats.writeMisses++;
    // Under write-allocate the block is brought in; under no-write-allocate it is not
    Account(blockAddr, /*isMiss=*/true, /*installs=*/config.writeAllocate);
    if (config.writeAllocate) {
        // Write-allocate: pull the line into the cache, then apply the write
        uint32_t line_start = address & ~(config.lineSize - 1);
        std::vector<uint8_t> buffer(config.lineSize);
        mainMem->Read(line_start, static_cast<uint8_t>(config.lineSize), buffer.data());
        CacheLine* new_line = sets[addressParts.setIndex].Replace(addressParts.tag,
            addressParts.setIndex, buffer.data(), mainMem, config, byteOffsetBits, setIndexBits, stats);

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

bool Cache::Contains(uint32_t address) const {
    AddressParts addressParts(address, byteOffsetBits, setIndexBits);
    return sets[addressParts.setIndex].Contains(addressParts.tag);
}

void Cache::Dump(std::ostream& os) const {
    os << "------------------------ Cache Contents ------------------------" << std::endl;
    for (uint32_t i = 0; i < sets.size(); i++) {
        sets[i].Dump(i, os);                             // Each set prints only its valid ways
    }
    os << "----------------------------------------------------------------" << std::endl;
}

void Cache::PrintStats() const {
    stats.Report(config.hitTime, config.missPenalty);
}
