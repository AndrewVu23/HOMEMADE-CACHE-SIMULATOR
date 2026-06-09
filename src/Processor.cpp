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

void MemSys::SeedPattern() {
    mainMem.FillPattern();
}

void MemSys::PrintMainMem() {
    mainMem.Print();
}

bool MemSys::Contains(uint32_t address) const {
    return cache.Contains(address);
}

const CacheStats& MemSys::GetStats() const {
    return cache.GetStats();
}

void MemSys::PrintStats() const {
    cache.PrintStats();
}

void MemSys::DumpCache() const {
    cache.Dump(std::cout);
}