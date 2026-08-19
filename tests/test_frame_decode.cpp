#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "frame_decode.h"

#include <cstring>
#include <vector>

using namespace xiii;

TEST_CASE("32-bit X8R8G8B8 decodes as BGR copy with alpha forced opaque") {
    // Source memory order is B,G,R,X per pixel; X (garbage alpha) must become FF.
    const uint8_t src[2 * 1 * 4] = {
        0x10, 0x20, 0x30, 0x00,   // pixel 0: B=10 G=20 R=30, garbage alpha
        0x40, 0x50, 0x60, 0x7F,   // pixel 1
    };
    uint8_t dst[2 * 1 * 4] = {};
    REQUIRE(DecodeToBGRA(kFmtX8R8G8B8, 2, 1, src, 2 * 4, dst));
    const uint8_t want[] = { 0x10, 0x20, 0x30, 0xFF, 0x40, 0x50, 0x60, 0xFF };
    CHECK(std::memcmp(dst, want, sizeof(want)) == 0);
}

TEST_CASE("source pitch padding is skipped between rows") {
    // 1x2 image with 4 bytes of padding after each 4-byte row (pitch 8).
    const uint8_t src[2 * 8] = {
        0x01, 0x02, 0x03, 0x00,  0xAA, 0xAA, 0xAA, 0xAA,   // row 0 + pad
        0x04, 0x05, 0x06, 0x00,  0xBB, 0xBB, 0xBB, 0xBB,   // row 1 + pad
    };
    uint8_t dst[1 * 2 * 4] = {};
    REQUIRE(DecodeToBGRA(kFmtA8R8G8B8, 1, 2, src, 8, dst));
    const uint8_t want[] = { 0x01, 0x02, 0x03, 0xFF, 0x04, 0x05, 0x06, 0xFF };
    CHECK(std::memcmp(dst, want, sizeof(want)) == 0);
}

TEST_CASE("R5G6B5 expands pure red, green, blue to full 8-bit channels") {
    const uint16_t px[3] = { 0xF800, 0x07E0, 0x001F };  // red, green, blue
    uint8_t dst[3 * 4] = {};
    REQUIRE(DecodeToBGRA(kFmtR5G6B5, 3, 1, (const uint8_t*)px, 3 * 2, dst));
    const uint8_t want[] = {
        0x00, 0x00, 0xFF, 0xFF,   // red   -> B,G,R,A
        0x00, 0xFF, 0x00, 0xFF,   // green
        0xFF, 0x00, 0x00, 0xFF,   // blue
    };
    CHECK(std::memcmp(dst, want, sizeof(want)) == 0);
}

TEST_CASE("X1R5G5B5 expands 5-bit channels") {
    const uint16_t px[2] = { 0x7C00, 0x03E0 };  // red, green
    uint8_t dst[2 * 4] = {};
    REQUIRE(DecodeToBGRA(kFmtX1R5G5B5, 2, 1, (const uint8_t*)px, 2 * 2, dst));
    const uint8_t want[] = { 0x00, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF };
    CHECK(std::memcmp(dst, want, sizeof(want)) == 0);
}

TEST_CASE("unknown format decodes to opaque black, not failure") {
    const uint8_t src[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    uint8_t dst[4] = { 1, 2, 3, 4 };
    REQUIRE(DecodeToBGRA(999, 1, 1, src, 4, dst));
    const uint8_t want[] = { 0x00, 0x00, 0x00, 0xFF };
    CHECK(std::memcmp(dst, want, sizeof(want)) == 0);
}

TEST_CASE("bad arguments are rejected") {
    uint8_t buf[16] = {};
    CHECK_FALSE(DecodeToBGRA(kFmtX8R8G8B8, 0, 1, buf, 4, buf));       // zero width
    CHECK_FALSE(DecodeToBGRA(kFmtX8R8G8B8, 1, -1, buf, 4, buf));      // bad height
    CHECK_FALSE(DecodeToBGRA(kFmtX8R8G8B8, 1, 1, nullptr, 4, buf));   // null src
    CHECK_FALSE(DecodeToBGRA(kFmtX8R8G8B8, 1, 1, buf, 4, nullptr));   // null dst
    CHECK_FALSE(DecodeToBGRA(kFmtX8R8G8B8, 2, 1, buf, 4, buf));       // pitch < row
    CHECK_FALSE(DecodeToBGRA(kFmtX8R8G8B8, 20000, 1, buf, 4, buf));   // absurd dims
}

TEST_CASE("large 32-bit frame round-trips exactly (fast path equivalence)") {
    // Wide enough that any vectorized/memcpy fast path exercises full rows plus
    // a ragged tail; every byte position gets a distinct value.
    const int W = 61, H = 7, pitch = W * 4 + 12;
    std::vector<uint8_t> src((size_t)pitch * H);
    for (size_t i = 0; i < src.size(); ++i) src[i] = (uint8_t)(i * 31 + 7);
    std::vector<uint8_t> dst((size_t)W * H * 4, 0);
    REQUIRE(DecodeToBGRA(kFmtA8R8G8B8, W, H, src.data(), pitch, dst.data()));
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const uint8_t* s = &src[(size_t)y * pitch + (size_t)x * 4];
            const uint8_t* d = &dst[((size_t)y * W + x) * 4];
            REQUIRE(d[0] == s[0]);
            REQUIRE(d[1] == s[1]);
            REQUIRE(d[2] == s[2]);
            REQUIRE(d[3] == 0xFF);
        }
    }
}
