// proxy/steamvr_host.cpp
//
// SteamVR / OpenVR VR host: pushes the captured game frame to a head-locked
// in-scene overlay and reads back the HMD orientation for the camera hook.
// See steamvr_host.h. UNTESTED on the dev machine (no runtime/headset) --
// written to compile cleanly and be spec-faithful; it runs for real only on
// the user's home machine with SteamVR + a headset.
//
// RUNTIME: SteamVR (via Steam). Not OpenXR. Gated behind [VR] SteamVR=1.

#include <windows.h>
#include <openvr.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "steamvr_host.h"
#include "vr_host.h"

static void Log(const char* m) {
    OutputDebugStringA("[xiii-steamvr] "); OutputDebugStringA(m); OutputDebugStringA("\n");
}

static HANDLE        g_thread = nullptr;
static volatile LONG g_run    = 0;

static bool ConfigEnabled() {
    char exe[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, exe, MAX_PATH)) return false;
    char* slash = strrchr(exe, '\\');
    if (!slash) return false;
    lstrcpyA(slash + 1, "XIII.ini");
    return GetPrivateProfileIntA("VR", "SteamVR", 0, exe) != 0;
}

// Identity 3x4 pose with the overlay placed 2 m in front of the HMD. OpenVR is
// right-handed, +Y up, -Z forward, so a forward offset is negative Z.
static vr::HmdMatrix34_t HeadRelativeTransform() {
    vr::HmdMatrix34_t m{};
    m.m[0][0] = 1.0f; m.m[1][1] = 1.0f; m.m[2][2] = 1.0f;
    m.m[2][3] = -2.0f;  // 2 m forward
    return m;
}

// Extract the rotation quaternion (x, y, z, w) from an OpenVR 3x4 pose matrix.
// Standard branch-by-largest-diagonal conversion; the 3x3 upper-left is the
// rotation, column 3 is translation (ignored here).
static void MatrixToQuat(const vr::HmdMatrix34_t& m, float& qx, float& qy, float& qz, float& qw) {
    const float m00 = m.m[0][0], m01 = m.m[0][1], m02 = m.m[0][2];
    const float m10 = m.m[1][0], m11 = m.m[1][1], m12 = m.m[1][2];
    const float m20 = m.m[2][0], m21 = m.m[2][1], m22 = m.m[2][2];
    const float trace = m00 + m11 + m22;
    if (trace > 0.0f) {
        float s = std::sqrt(trace + 1.0f) * 2.0f;  // s = 4*qw
        qw = 0.25f * s; qx = (m21 - m12) / s; qy = (m02 - m20) / s; qz = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        qw = (m21 - m12) / s; qx = 0.25f * s; qy = (m01 + m10) / s; qz = (m02 + m20) / s;
    } else if (m11 > m22) {
        float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        qw = (m02 - m20) / s; qx = (m01 + m10) / s; qy = 0.25f * s; qz = (m12 + m21) / s;
    } else {
        float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        qw = (m10 - m01) / s; qx = (m02 + m20) / s; qy = (m12 + m21) / s; qz = 0.25f * s;
    }
}

static void SteamVrThreadMain() {
    vr::EVRInitError err = vr::VRInitError_None;
    // VRApplication_Overlay: an overlay-only client -- it does not need to be the
    // scene app, so it floats a screen in whatever SteamVR is currently showing.
    vr::IVRSystem* sys = vr::VR_Init(&err, vr::VRApplication_Overlay);
    if (err != vr::VRInitError_None || !sys) {
        char b[128];
        _snprintf(b, sizeof(b), "VR_Init failed (EVRInitError %d)", (int)err); b[127] = 0;
        Log(b);
        return;
    }
    vr::IVROverlay* ov = vr::VROverlay();
    if (!ov) { Log("VROverlay() null"); vr::VR_Shutdown(); return; }

    vr::VROverlayHandle_t overlay = vr::k_ulOverlayHandleInvalid;
    if (ov->CreateOverlay("xiii2003vr.framebuffer", "XIII VR", &overlay) != vr::VROverlayError_None) {
        Log("CreateOverlay failed"); vr::VR_Shutdown(); return;
    }
    ov->SetOverlayWidthInMeters(overlay, 2.4f);  // 2.4 m wide (4:3 => ~1.8 m tall)
    vr::HmdMatrix34_t xform = HeadRelativeTransform();
    ov->SetOverlayTransformTrackedDeviceRelative(overlay, vr::k_unTrackedDeviceIndex_Hmd, &xform);
    ov->ShowOverlay(overlay);
    Log("overlay created + shown");

    // SetOverlayRaw wants RGBA8888; VrHostCopyLatestFrame gives BGRA8. Swap R<->B
    // into this scratch buffer each frame. (If colours look swapped on the
    // headset, this swap is the single knob to flip.)
    int cap = 0;
    uint8_t* bgra = nullptr;
    uint8_t* rgba = nullptr;

    while (InterlockedCompareExchange(&g_run, 1, 1) == 1) {
        // --- Head pose -> camera hook ---
        vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
        sys->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f,
                                             poses, vr::k_unMaxTrackedDeviceCount);
        const vr::TrackedDevicePose_t& hmd = poses[vr::k_unTrackedDeviceIndex_Hmd];
        if (hmd.bPoseIsValid) {
            float qx, qy, qz, qw;
            MatrixToQuat(hmd.mDeviceToAbsoluteTracking, qx, qy, qz, qw);
            VrHostSetHeadPose(qx, qy, qz, qw);
        }

        // --- Latest game frame -> overlay ---
        int w = 0, h = 0;
        if (cap == 0) { cap = 4096 * 4096 * 4; bgra = (uint8_t*)malloc(cap); rgba = (uint8_t*)malloc(cap); }
        if (bgra && rgba && VrHostCopyLatestFrame(bgra, cap, &w, &h) && w > 0 && h > 0) {
            const int n = w * h;
            for (int i = 0; i < n; ++i) {
                rgba[i * 4 + 0] = bgra[i * 4 + 2];  // R <- B
                rgba[i * 4 + 1] = bgra[i * 4 + 1];  // G
                rgba[i * 4 + 2] = bgra[i * 4 + 0];  // B <- R
                rgba[i * 4 + 3] = 255;
            }
            ov->SetOverlayRaw(overlay, rgba, (uint32_t)w, (uint32_t)h, 4);
        }

        // --- Events (react to SteamVR quitting) ---
        vr::VREvent_t e{};
        while (ov->PollNextOverlayEvent(overlay, &e, sizeof(e))) {
            if (e.eventType == vr::VREvent_Quit) {
                sys->AcknowledgeQuit_Exiting();
                InterlockedExchange(&g_run, 0);
            }
        }

        Sleep(8);  // ~120 Hz upper bound; the real cap is the game's frame rate
    }

    free(bgra);
    free(rgba);
    ov->DestroyOverlay(overlay);
    vr::VR_Shutdown();
    Log("SteamVR host stopped");
}

// Wrapper that swallows a delay-load failure (openvr_api.dll absent) so a
// machine without SteamVR is never harmed by enabling the flag.
static DWORD WINAPI SteamVrThreadProc(LPVOID) {
    __try {
        SteamVrThreadMain();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("SteamVR unavailable (openvr_api/runtime missing) -- host disabled");
    }
    return 0;
}

void StartSteamVrHost() {
    if (!ConfigEnabled()) { Log("[VR] SteamVR=0 -- host not started"); return; }
    InterlockedExchange(&g_run, 1);
    g_thread = CreateThread(nullptr, 0, SteamVrThreadProc, nullptr, 0, nullptr);
    Log("SteamVR host thread started");
}

void StopSteamVrHost() {
    InterlockedExchange(&g_run, 0);
    if (g_thread) {
        WaitForSingleObject(g_thread, 2000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
}
