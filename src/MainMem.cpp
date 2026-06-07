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
    std::string message = std::format("Reading from main memory (start address: 0x{:x})", start_addr);
    std::cout << message << std::endl;
    // Read & copy data from mem
    std::memcpy(des_ptr, &memory.get()->at(start_addr), size);
}

void MainMem::Write(uint32_t start_addr, uint8_t size, uint8_t* src_ptr) {
    std::string message = std::format("Writing to main memory (start address: 0x{:x})", start_addr);
    std::cout << message << std::endl;
    // Copy data from source to mem
    std::memcpy(&memory.get()->at(start_addr), src_ptr, size);
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

void MainMem::Print() {
    // 24 x 12 = 288 bytes -> mapping 2D array onto the first 288 bytes of the 
    // 4MB 1D array for debugging (mapping the whole 4MB = impossible)
    const uint32_t ROWS = 24;
    const uint32_t COLUMNS = 12;

    for (uint32_t row = 0; row < ROWS; row++) {
        for (uint32_t column = 0; column < COLUMNS; column++) {
            // Turn 1D -> 2D array
            // Format raw byte value (not address hence no &) -> hexadecimal
            std::string value = std ::format("0x{:x}\t", memory.get()->at(row * COLUMNS + column));
            std::cout << value;
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
    // Long separating line
    for (uint32_t line = 0; line < 80; line++) {
        std::cout << "-";
    }
    std::cout << std::endl;
}