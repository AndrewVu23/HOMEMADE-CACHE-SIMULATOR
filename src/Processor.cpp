#include "Processor.h"
#include <cstdint>
#include <iostream>
#include <format>

MemSys::MemSys() {
    // Wiring the cache to main mem by passing the address of the data
    cache.Initialize(&mainMem); 
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
    MemSys memory;

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

    return 0;
}