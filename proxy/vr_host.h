// proxy/vr_host.h
#pragma once
#include <cstdint>

// The "VR Host": owns an independent D3D11 device and holds the latest game
// frame in a D3D11 texture -- the form an OpenXR swapchain image takes. For
// Milestone 1 / 0.2.0 groundwork this is the bridge from the game's D3D8
// backbuffer (captured on the CPU) into D3D11; the OpenXR session + frame
// submit is layered on top later (and needs a headset to validate).
//
// The game's D3D8 device and this D3D11 device are independent; the frame
// crosses between them via a CPU copy (simple and reliable -- perf is a home-
// machine concern, not a dev-machine one).

bool VrHostInit();
void VrHostShutdown();

// Upload one frame into the host's D3D11 texture. Pixels are tightly packed
// BGRA8 (B,G,R,A per pixel), top-down, `width*height*4` bytes -- matching
// DXGI_FORMAT_B8G8R8A8_UNORM, which is also the natural byte order of a D3D8
// X8R8G8B8/A8R8G8B8 surface.
void VrHostSubmitFrame(const uint8_t* bgra, int width, int height);

// Copy the most recently submitted frame (BGRA8, top-down) into `dst` (capacity
// `cap` bytes). Returns false if there's no frame or `dst` is too small; on
// success writes the frame's width/height. Used by the OpenXR host to upload
// into a swapchain image.
bool VrHostCopyLatestFrame(uint8_t* dst, int cap, int* width, int* height);

// As VrHostCopyLatestFrame, but also returns the frame's sequence number (one
// increment per VrHostSubmitFrame). Consumers that re-present the frame on
// their own clock use this to skip work -- and, critically, redundant
// compositor uploads -- when no new frame has arrived.
bool VrHostCopyLatestFrameEx(uint8_t* dst, int cap, int* width, int* height,
                             unsigned* seq);

// Sequence number of the latest latched frame (0 = none yet). Cheap poll for
// "is there anything new?" without copying the pixels.
unsigned VrHostLatestFrameSeq();

// Push the real HMD orientation (from OpenXR). Once set, VrHostGetHeadPose
// returns this instead of the synthetic yaw.
void VrHostSetHeadPose(float x, float y, float z, float w);

// Current head orientation as a quaternion (x, y, z, w). Returns the real HMD
// pose if one has been set via VrHostSetHeadPose; otherwise a synthetic
// time-based yaw (so the pose pipeline can be exercised on a monitor). Returns
// false only on a null argument.
bool VrHostGetHeadPose(float* x, float* y, float* z, float* w);

// Debug/verification (headset-free): read the current D3D11 texture back and
// write it as a 24-bit BMP. Confirms the D3D8 -> CPU -> D3D11 bridge is intact.
bool VrHostDebugSaveTexture(const char* path);
