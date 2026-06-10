#include "doctest.h"
#include "Cache.h"
#include "test_helpers.h"

// ------------------------- Address decoding -------------------------

TEST_CASE("AddressParts splits tag/set/offset for 64B lines, 64 sets") {
    // lineSize 64 -> 6 offset bits; numSets 64 -> 6 set-index bits
    const uint32_t offBits = 6;
    const uint32_t setBits = 6;

    AddressParts a(0x1234, offBits, setBits);
    CHECK(a.byteOffset == (0x1234u & 0x3F));         // 0x34
    CHECK(a.setIndex == ((0x1234u >> 6) & 0x3F));    // 0x8
    CHECK(a.tag == (0x1234u >> 12));                 // 0x1
}

TEST_CASE("AddressParts at a line boundary has zero offset") {
    AddressParts a(0x40, 6, 6);                      // 0x40 = one 64B line in
    CHECK(a.byteOffset == 0);
    CHECK(a.setIndex == 1);
    CHECK(a.tag == 0);
}

TEST_CASE("AddressParts adapts to different line/set sizes") {
    // lineSize 16 -> 4 offset bits; numSets 8 -> 3 set-index bits
    AddressParts a(0xABCD, 4, 3);
    CHECK(a.byteOffset == (0xABCDu & 0xF));          // 0xD
    CHECK(a.setIndex == ((0xABCDu >> 4) & 0x7));     // lower 3 bits above offset
    CHECK(a.tag == (0xABCDu >> 7));
}

// --------------------------- Validation -----------------------------

TEST_CASE("Validate accepts a sane power-of-two config") {
    CHECK(TestConfig().Validate());
    CHECK(TestConfig(ReplacementPolicy::Random, 64, 4, 64).Validate());
}

TEST_CASE("Validate rejects non-power-of-two numSets") {
    CHECK_FALSE(TestConfig(ReplacementPolicy::LRU, 3, 4, 64).Validate());
}

TEST_CASE("Validate rejects non-power-of-two lineSize") {
    CHECK_FALSE(TestConfig(ReplacementPolicy::LRU, 1, 4, 48).Validate());
}

TEST_CASE("Validate rejects zero ways") {
    CHECK_FALSE(TestConfig(ReplacementPolicy::LRU, 1, 0, 64).Validate());
}

TEST_CASE("Validate rejects PLRU with non-power-of-two ways") {
    CHECK_FALSE(TestConfig(ReplacementPolicy::PLRU, 1, 3, 64).Validate());
    // ...but accepts PLRU when ways is a power of two
    CHECK(TestConfig(ReplacementPolicy::PLRU, 1, 4, 64).Validate());
}

TEST_CASE("Non-PLRU policies allow non-power-of-two ways") {
    CHECK(TestConfig(ReplacementPolicy::LRU, 1, 3, 64).Validate());
    CHECK(TestConfig(ReplacementPolicy::FIFO, 1, 3, 64).Validate());
}
