// proxy/steamvr_host.h
#pragma once

// Starts the SteamVR / OpenVR VR host on a background thread IF [VR] SteamVR=1
// in XIII.ini.
//
// RUNTIME: this path talks to the **SteamVR runtime** (installed and launched
// through Steam). It does NOT use OpenXR. Enable EXACTLY ONE host at a time --
// either [VR] SteamVR=1 or [VR] OpenXR=1, never both.
//
// It creates a head-locked in-scene overlay and, every iteration, pushes the
// VR host's latest captured game frame straight into that overlay via
// IVROverlay::SetOverlayRaw (a CPU upload -- no D3D11 device, swapchain, or
// shared texture needed, which is why the SteamVR path is simpler and more
// reliable on this 32-bit target than OpenXR). The overlay is attached to the
// HMD 2 m in front, so it stays centred in view; turning the head instead
// rotates the in-game camera through the camera hook, which reads the head
// orientation this host feeds back via VrHostSetHeadPose. That combination is
// the Milestone 1 legacy-framebuffer head-look proof.
//
// UNTESTED on the headset-less dev machine: it compiles here, but only runs
// meaningfully with SteamVR + a headset. openvr_api.dll is delay-loaded, so
// when [VR] SteamVR is off (or the DLL/runtime is absent) nothing loads and the
// game is unaffected.
void StartSteamVrHost();
void StopSteamVrHost();
