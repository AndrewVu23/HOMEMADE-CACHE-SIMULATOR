#include "MainMem.h"
#include <array>
#include <iostream>
#include <format>
#include <string>

MainMem::MainMem() {

    // Allocate memory
    memory = std::make_unique<std::array<uint8_t, MAIN_MEMORY_SIZE>>();

    // Fill main memory with zeros
    memory.get()->fill(0x00);
}

MainMem::~MainMem() {

}

void MainMem::Read(uint32_t start_addr, uint8_t size, uint8_t* des_ptr) {
    // Read & copy data from mem
    std::memcpy(des_ptr, &memory.get()->at(start_addr), size);
}

void MainMem::Write(uint32_t start_addr, uint8_t size, uint8_t* src_ptr) {
    // Copy data from source to mem
    std::memcpy(&memory.get()->at(start_addr), src_ptr, size);
}

void MainMem::Print() {
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
    std::cout << endl;
}