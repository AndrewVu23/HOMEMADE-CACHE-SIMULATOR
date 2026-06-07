#pragma once

#include <memory>
#include <array>
#include <cstdint>
#include <string>

// Initialize a 4MB 32-bit main memory
const uint32_t MAIN_MEMORY_SIZE = 4 * 1024 * 1024;

class MainMem {
    private:
        // Since we can't just allocate 4MB on the stack
        // we have to use pointer to allocate it dynamically.
        // Each row = 1 byte (8 bit), with a total of 4MB rows
        // std::unique_ptr = smart pointer w/ exclusive ownership 
        // -> cannot be accidentally copied or shared + automatically
        // delete memory when obj MainMem is destroyed
        std::unique_ptr<std::array<uint8_t, MAIN_MEMORY_SIZE>> memory;
    public:
        MainMem();                                                          // Constructor = initialize + setup obj
        ~MainMem();                                                         // Deconstructor = cleans up + free obj

        // address = 32-bit indexing
        // size = 1 byte
        // size of des_ptr = src_ptr = size = 1 byte 
        // since uint8_t* = the size of the data the pointers are pointing to
        void Read(uint32_t start_addr, uint8_t size, uint8_t* des_ptr);     // Read from main mem
        void Write(uint32_t start_addr, uint8_t size, uint8_t* src_ptr);    // Write to main mem
        void Load(const std::string& path);                                 // Load a binary file into main mem starting at address 0
        void Print();                                                       // For debugging
};