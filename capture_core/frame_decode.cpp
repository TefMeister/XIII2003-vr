// capture_core/frame_decode.cpp
#include "frame_decode.h"

#include <cstring>

namespace xiii {

bool DecodeToBGRA(uint32_t format, int width, int height,
                  const uint8_t* src, int srcPitch, uint8_t* dst) {
    if (!src || !dst) return false;
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384) return false;

    const bool is32 = (format == kFmtA8R8G8B8 || format == kFmtX8R8G8B8);
    const bool is16 = (format == kFmtR5G6B5 || format == kFmtX1R5G5B5 ||
                       format == kFmtA1R5G5B5);
    const int bpp = is32 ? 4 : (is16 ? 2 : 0);
    if (bpp != 0 && srcPitch < width * bpp) return false;

    for (int y = 0; y < height; ++y) {
        const uint8_t* s = src + (size_t)y * srcPitch;
        uint8_t* d = dst + (size_t)y * width * 4;
        if (is32) {
            // Source is already B,G,R,X in memory -- a straight copy, then force
            // the alpha byte opaque. Two tight passes the compiler vectorizes,
            // instead of the old per-pixel branch on format.
            std::memcpy(d, s, (size_t)width * 4);
            for (int x = 0; x < width; ++x) d[x * 4 + 3] = 0xFF;
        } else if (format == kFmtR5G6B5) {
            for (int x = 0; x < width; ++x) {
                const uint16_t p = (uint16_t)(s[x * 2] | (s[x * 2 + 1] << 8));
                d[x * 4 + 0] = (uint8_t)((p & 0x1F) * 255 / 31);
                d[x * 4 + 1] = (uint8_t)(((p >> 5) & 0x3F) * 255 / 63);
                d[x * 4 + 2] = (uint8_t)(((p >> 11) & 0x1F) * 255 / 31);
                d[x * 4 + 3] = 0xFF;
            }
        } else if (is16) {  // X1R5G5B5 / A1R5G5B5
            for (int x = 0; x < width; ++x) {
                const uint16_t p = (uint16_t)(s[x * 2] | (s[x * 2 + 1] << 8));
                d[x * 4 + 0] = (uint8_t)((p & 0x1F) * 255 / 31);
                d[x * 4 + 1] = (uint8_t)(((p >> 5) & 0x1F) * 255 / 31);
                d[x * 4 + 2] = (uint8_t)(((p >> 10) & 0x1F) * 255 / 31);
                d[x * 4 + 3] = 0xFF;
            }
        } else {
            // Unknown format: opaque black still proves the capture path.
            for (int x = 0; x < width; ++x) {
                d[x * 4 + 0] = 0; d[x * 4 + 1] = 0; d[x * 4 + 2] = 0;
                d[x * 4 + 3] = 0xFF;
            }
        }
    }
    return true;
}

}  // namespace xiii
