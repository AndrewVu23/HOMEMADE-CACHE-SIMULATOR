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

void MemSys::PrintMainMem() {
    mainMem.Print();
} 

int main() {
    MemSys memory;

    // Print messages every time the system tries to read/write data
    memory.PrintMainMem();

    uint32_t data1 = memory.Read(0x20);
    std::cout << std::format("Value: 0x{:x}", data1) << std::endl;
    data1 = memory.Read(0x20);
    std::cout << std::format("Value: 0x{:x}", data1) << std::endl;

    return 0;
}