// proxy/openxr_host.h
#pragma once

// Starts the OpenXR VR host on a background thread IF [VR] OpenXR=1 in XIII.ini.
// It opens an XR session bound to its own D3D11 device (on the runtime's required
// adapter), creates a swapchain sized to the captured game frame, and each XR
// frame uploads the VR host's latest frame into a QUAD layer (a flat framebuffer
// floating in front, same image for both eyes -- the Milestone 1 legacy-
// framebuffer goal), while feeding the real HMD orientation back through
// VrHostSetHeadPose so the camera hook's LiveHmd mode follows the head.
//
// IMPORTANT: this is UNTESTED on the headset-less dev machine. It compiles here,
// but only runs meaningfully on a machine with an OpenXR runtime + headset. The
// openxr_loader is delay-loaded, so when [VR] OpenXR is off (or the loader/
// runtime is absent) nothing is loaded and the game is unaffected.
void StartOpenXrHost();
void StopOpenXrHost();
