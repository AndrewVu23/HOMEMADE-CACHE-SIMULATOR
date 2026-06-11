#include "MainMem.h"
#include <array>
#include <iostream>
#include <format>
#include <string>
#include <cstring>
#include <fstream>

MainMem::MainMem() {

    // Allocate memory
    memory = std::make_unique<std::array<uint8_t, MAIN_MEMORY_SIZE>>();

    // Fill main memory with zeros
    memory.get()->fill(0x00);
}

MainMem::~MainMem() {

}

void MainMem::Read(uint32_t start_addr, uint8_t size, uint8_t* des_ptr) {
    if (verbose) {
        std::string message = std::format("Reading from main memory (start address: 0x{:x})", start_addr);
        std::cout << message << std::endl;
    }
    // Read & copy data from mem
    std::memcpy(des_ptr, &memory.get()->at(start_addr), size);
}

void MainMem::Write(uint32_t start_addr, uint8_t size, uint8_t* src_ptr) {
    if (verbose) {
        std::string message = std::format("Writing to main memory (start address: 0x{:x})", start_addr);
        std::cout << message << std::endl;
    }
    // Copy data from source to mem
    std::memcpy(&memory.get()->at(start_addr), src_ptr, size);
}

void MainMem::FillPattern() {
    // Seed each byte with the low 8 bits of its address (value == address & 0xFF),
    // the same convention as data.bin. Gives tests predictable read values with no file
    std::array<uint8_t, MAIN_MEMORY_SIZE>& mem = *memory.get();
    for (uint32_t i = 0; i < MAIN_MEMORY_SIZE; i++) {
        mem[i] = static_cast<uint8_t>(i & 0xFF);
    }
}

void MainMem::Load(const std::string& path) {
    // Open the file in binary mode, positioned at the end so we can measure its size
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cout << std::format("Could not open data file: {}", path) << std::endl;
        return;
    }

    // Measure the file, then clamp to main memory capacity
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size > static_cast<std::streamsize>(MAIN_MEMORY_SIZE)) {
        size = MAIN_MEMORY_SIZE;
    }

    // Read the bytes into main memory starting at address 0
    // Since std::ifstream reads char* pointer type, we need to
    // reinterpret uint_8* as char* -> not changing the data,
    // just the type of pointer
    file.read(reinterpret_cast<char*>(memory.get()->data()), size);
    // Load exactly 288 bytes to fit the 2D array below for ease of debugging
    std::cout << std::format("Loaded {} bytes from {} into main memory", size, path) << std::endl;
}