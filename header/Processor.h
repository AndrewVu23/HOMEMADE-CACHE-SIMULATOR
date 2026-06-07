#pragma once

#include "MainMem.h"
#include "Cache.h"

class MemSys {
    private:
        MainMem mainMem;
        Cache cache;
    public:
        MemSys();
        ~MemSys();

        uint32_t Read(uint32_t address);
        void Write(uint32_t address, uint32_t data);
        void LoadMainMem(const std::string& path);
        void PrintMainMem();
};
