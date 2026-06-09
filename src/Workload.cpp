#include "Workload.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>

void RunTrace(MemSys& mem, const Trace& trace) {
    for (const Access& a : trace) {
        if (a.op == Op::Read) {
            mem.Read(a.address);
        }
        else {
            mem.Write(a.address, a.value);
        }
    }
}

// --------------------------- Synthetic generators ---------------------------

Trace GenSequential(uint32_t start, uint32_t count, uint32_t stride) {
    Trace trace;
    trace.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        trace.push_back({Op::Read, start + i * stride, 0});
    }
    return trace;
}

Trace GenStrided(uint32_t start, uint32_t count, uint32_t stride) {
    // Same shape as sequential but named separately to make intent (large
    // stride to provoke conflict misses) explicit at the call site.
    return GenSequential(start, count, stride);
}

Trace GenRandom(uint32_t count, uint32_t addrSpace, unsigned seed) {
    Trace trace;
    trace.reserve(count);
    std::srand(seed);
    for (uint32_t i = 0; i < count; i++) {
        // Align to 4 bytes so reads stay within a single word
        uint32_t addr = (static_cast<uint32_t>(std::rand()) % addrSpace) & ~0x3u;
        trace.push_back({Op::Read, addr, 0});
    }
    return trace;
}

Trace GenLooping(uint32_t start, uint32_t span, uint32_t loops, uint32_t stride) {
    Trace trace;
    uint32_t perSweep = (stride == 0) ? 0 : (span / stride);
    trace.reserve(static_cast<size_t>(perSweep) * loops);
    for (uint32_t l = 0; l < loops; l++) {
        for (uint32_t offset = 0; offset < span; offset += stride) {
            trace.push_back({Op::Read, start + offset, 0});
        }
    }
    return trace;
}

// ----------------------------- Trace-file loader ----------------------------

Trace LoadTrace(const std::string& path) {
    Trace trace;
    std::ifstream file(path);
    if (!file) {
        std::cout << "Could not open trace file: " << path << std::endl;
        return trace;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string opToken;
        if (!(iss >> opToken)) {
            continue;                       // Blank line
        }
        if (opToken.empty() || opToken[0] == '#') {
            continue;                       // Comment line
        }

        Access access{};
        if (opToken == "R" || opToken == "r") {
            access.op = Op::Read;
        }
        else if (opToken == "W" || opToken == "w") {
            access.op = Op::Write;
        }
        else {
            continue;                       // Unknown op -> skip
        }

        std::string addrToken;
        if (!(iss >> addrToken)) {
            continue;                       // Missing address -> skip
        }
        // std::stoul with base 0 auto-detects 0x (hex) vs decimal
        access.address = static_cast<uint32_t>(std::stoul(addrToken, nullptr, 0));

        std::string valToken;
        if (access.op == Op::Write && (iss >> valToken)) {
            access.value = static_cast<uint32_t>(std::stoul(valToken, nullptr, 0));
        }

        trace.push_back(access);
    }
    return trace;
}
