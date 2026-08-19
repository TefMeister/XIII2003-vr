// capture_core/frame_decode.h
//
// Pure pixel-decode for the capture path: converts a locked D3D8 backbuffer
// surface into tightly packed BGRA8 top-down pixels with alpha forced opaque
// (matches DXGI_FORMAT_B8G8R8A8_UNORM, what the VR hosts upload).
//
// Lives in capture_core (no Windows/D3D dependencies) so it is unit-testable;
// the proxy links it, tests exercise it directly.

#pragma once
#include <cstdint>

namespace xiii {

// D3D8 D3DFORMAT codes the game's backbuffer can plausibly use.
enum : uint32_t {
    kFmtA8R8G8B8 = 21,
    kFmtX8R8G8B8 = 22,
    kFmtR5G6B5   = 23,
    kFmtX1R5G5B5 = 24,
    kFmtA1R5G5B5 = 25,
};

// Decode `height` rows of `width` pixels from `src` (row stride `srcPitch`
// bytes) into `dst` (tightly packed, width*height*4 bytes, caller-owned).
// Unknown formats decode to opaque black (the frame still proves the path).
// Returns false only on bad arguments (null pointers, non-positive or absurd
// dimensions, pitch smaller than a source row).
bool DecodeToBGRA(uint32_t format, int width, int height,
                  const uint8_t* src, int srcPitch, uint8_t* dst);

}  // namespace xiii
