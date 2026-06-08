#include "Processor.h"
#include <cstdint>
#include <iostream>
#include <format>

MemSys::MemSys(const CacheConfig& config) {
    // Wiring the cache to main mem and configuring it from the user-supplied specs
    cache.Initialize(config, &mainMem);
}

MemSys::~MemSys() {

}

uint32_t MemSys::Read(uint32_t address) { 
    return cache.Read(address);
}

void MemSys::Write(uint32_t address, uint32_t data) {
    return cache.Write(address, data);
}

void MemSys::LoadMainMem(const std::string& path) {
    mainMem.Load(path);
}

void MemSys::PrintMainMem() {
    mainMem.Print();
} 

static void Test(const std::string& title) {
    std::cout << std::endl << title << std::endl;
}

int main() {
    // Fully parametrizable cache spec. Change these to reconfigure the cache;
    // the rest of the program adapts automatically.
    CacheConfig config{
        .numSets = 64,
        .numWays = 4,
        .lineSize = 64,
        .addressBits = 32,
        .writeBack = false,        // write-through
        .writeAllocate = false,    // no-write-allocate
        .policy = ReplacementPolicy::Random
    };

    if (!config.Validate()) {
        return 1;
    }

    MemSys memory(config);

    // Test 0: Load the data file and show the initial state of main mem
    // Values loaded are the same as addresses for ease of debugging 
    // (ex: 0x20 lives in address 0x20)
    Test("Test 0: Load data.bin & show main memory");
    memory.LoadMainMem("data.bin");
    memory.PrintMainMem();

    // Test 1: Read miss then hit (set 0)
    // 0x20 is a cold miss -> pulls the line from main mem
    // 0x24 is in the same 64-byte line -> should be a cache hit
    // The expected size when reading should be 32-bit
    Test("Test 1: Read miss then hit (set 0)");
    uint32_t value = memory.Read(0x20);
    std::cout << std::format("Read(0x20) = 0x{:x} (expect 0x23222120)", value) << std::endl;
    value = memory.Read(0x24);
    std::cout << std::format("Read(0x24) = 0x{:x} (expect 0x27262524)", value) << std::endl;

    // Test 2: Write hit + write-through
    // 0x24 is already cached, so the write updates the cache & writes through to main mem
    Test("Test 2: Write hit + write-through");
    // Write 0xDEADBEEF starting at address 0x24
    memory.Write(0x24, 0xDEADBEEF);
    memory.PrintMainMem();
    value = memory.Read(0x24);
    std::cout << std::format("Read(0x24) = 0x{:x} (expect 0xdeadbeef)", value) << std::endl;

    // Test 3: Write miss / no-write-allocate (set 2)
    // 0x80 is not cached -> write straight to main mem (not loaded into cache)
    // -> the following read is a miss -> read from main mem
    Test("Test 3: Write miss / no-write-allocate (set 2)");
    memory.Write(0x80, 0xCAFEBABE);
    memory.PrintMainMem();
    value = memory.Read(0x80);
    std::cout << std::format("Read(0x80) = 0x{:x} (expect 0xcafebabe)", value) << std::endl;

    // Test 4: Eviction (set 1, 4 ways)
    // All five addresses map to set 1 with different tags (0x1000 apart)
    // The first four fill the four ways; the fifth forces a random eviction
    Test("Test 4: Fill set 1 then force a random eviction");
    memory.Read(0x40);
    memory.Read(0x1040);
    memory.Read(0x2040);
    memory.Read(0x3040);
    std::cout << "- set 1 now holds 4 lines; next read must evict a victim -" << std::endl;
    memory.Read(0x4040);
    // This read should be from the cache
    memory.Read(0x4040);

    // Test 5: Write miss behavior - write-allocate vs no-write-allocate (set 3)
    // 0xC0 is not cached yet, so the write is a MISS.
    //   - writeAllocate = true : the line is pulled into the cache, the write is
    //     applied, so the FOLLOWING read of 0xC0 is a cache HIT.
    //   - writeAllocate = false: the write goes straight to main mem and nothing
    //     is cached, so the FOLLOWING read of 0xC0 is a MISS (reads main mem).
    // Either way the value read back is 0x11223344.
    Test("Test 5: Write miss - write-allocate vs no-write-allocate (set 3)");
    memory.Write(0xC0, 0x11223344);
    std::cout << "- now read 0xC0: HIT if write-allocate, MISS if no-write-allocate -" << std::endl;
    value = memory.Read(0xC0);
    std::cout << std::format("Read(0xC0) = 0x{:x} (expect 0x11223344)", value) << std::endl;

    // Test 6: Write-back defers the memory write until eviction (set 4)
    // Step 1: read 0x100 to bring its line into the cache.
    // Step 2: write to 0x100 (a write HIT).
    //   - writeBack = true : only the cached copy changes + the line is marked
    //     dirty; main mem is NOT updated yet (the Print below still shows the old
    //     bytes 0x0 0x1 0x2 0x3 at 0x100).
    //   - writeBack = false: write-through updates main mem immediately (the Print
    //     shows 0x34 0x12 0xcd 0xab at 0x100).
    Test("Test 6: Write-back defers write until eviction (set 4)");
    memory.Read(0x100);
    memory.Write(0x100, 0xABCD1234);
    std::cout << "- main mem after write hit: OLD bytes if write-back, NEW bytes if write-through -" << std::endl;
    memory.PrintMainMem();

    // Step 3: hammer set 4 with new tags to force eviction of the dirty line.
    // Each address is +0x1000 apart -> same set 4, different tag. When the dirty
    // 0x100 line is chosen as the victim, write-back flushes it to main mem and
    // you will see a "Flushing dirty line to main memory" log.
    // NOTE: replacement is Random, so the exact eviction order varies between
    // runs; the extra reads make it very likely the dirty line gets evicted.
    std::cout << "- forcing evictions on set 4 (watch for a dirty-line flush under write-back) -" << std::endl;
    memory.Read(0x1100);
    memory.Read(0x2100);
    memory.Read(0x3100);
    memory.Read(0x4100);
    memory.Read(0x5100);
    memory.Read(0x6100);
    memory.Read(0x7100);
    memory.Read(0x8100);
    std::cout << "- under write-back, 0x100 in main mem should now read 0x34 0x12 0xcd 0xab if it was flushed -" << std::endl;
    memory.PrintMainMem();

    return 0;
}