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
#include <d3d11.h>
#include <dxgi.h>
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
    Log("overlay created + shown (0.2.2 D3D11 texture path)");

    // D3D11 path for the overlay content (0.2.2): SetOverlayRaw makes the
    // compositor tear down and recreate the overlay's texture on every call,
    // which strobes at video-update rates even when the calls are rate-capped
    // (confirmed on hardware in 0.2.1). SetOverlayTexture with persistent
    // D3D11 textures is the supported way to stream frames into an overlay.
    // Two textures alternate so the compositor never samples one mid-update,
    // and DXGI_FORMAT_B8G8R8A8_UNORM matches the latch, so no swizzle needed.
    // Falls back to the old SetOverlayRaw path if device creation fails.
    ID3D11Device*        dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    ID3D11Texture2D*     tex[2] = { nullptr, nullptr };
    int texW = 0, texH = 0, texNext = 0;
    {
        // The device must live on the adapter the HMD is attached to.
        int32_t adapterIdx = 0;
        sys->GetDXGIOutputInfo(&adapterIdx);
        if (adapterIdx < 0) adapterIdx = 0;
        IDXGIFactory1* fac = nullptr;
        IDXGIAdapter1* adapter = nullptr;
        if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&fac)) && fac)
            fac->EnumAdapters1((UINT)adapterIdx, &adapter);
        D3D_FEATURE_LEVEL got{};
        HRESULT hr = D3D11CreateDevice(adapter,
                                       adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                                       nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                       &dev, &got, &ctx);
        if (adapter) adapter->Release();
        if (fac) fac->Release();
        if (FAILED(hr)) {
            dev = nullptr; ctx = nullptr;
            Log("D3D11 device creation failed -- falling back to SetOverlayRaw");
        } else {
            Log("D3D11 device created on HMD adapter for overlay textures");
        }
    }

    // SetOverlayRaw wants RGBA8888; VrHostCopyLatestFrame gives BGRA8. Swap R<->B
    // into this scratch buffer each frame. (If colours look swapped on the
    // headset, this swap is the single knob to flip.)
    int cap = 0;
    uint8_t* bgra = nullptr;
    uint8_t* rgba = nullptr;

    // FLICKER FIX (0.2.1): the 0.2.0 loop re-uploaded the frame with
    // SetOverlayRaw every ~8 ms whether or not anything new had been captured,
    // so the compositor kept sampling a texture that was being replaced up to
    // 120x/s -- visible as high-speed strobing. Uploads are now gated on the
    // latch's sequence number (only NEW frames upload) and rate-capped; when
    // frames stop arriving (game quit/paused) the overlay hides instead of
    // pinning a stale frame to the viewer's face.
    const DWORD kMinUploadIntervalMs = 11;   // ~90 Hz ceiling on uploads
    const DWORD kStarveHideMs        = 5000; // no new frames -> hide overlay
    const DWORD kStatsMs             = 5000; // DebugView heartbeat
    unsigned lastUploadedSeq  = 0;
    unsigned lastSeenSeq      = 0;
    unsigned statsBaseSeq     = 0;
    unsigned statsUploads     = 0;
    DWORD    lastUploadTick    = 0;
    DWORD    lastSeqChangeTick = GetTickCount();
    DWORD    lastStatsTick     = GetTickCount();
    bool     overlayVisible    = true;

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

        // --- Latest game frame -> overlay, only when there IS a new frame ---
        const DWORD    now = GetTickCount();
        const unsigned seq = VrHostLatestFrameSeq();
        if (seq != lastSeenSeq) {
            lastSeenSeq = seq;
            lastSeqChangeTick = now;
            if (!overlayVisible) {
                ov->ShowOverlay(overlay);
                overlayVisible = true;
                Log("frames resumed -- overlay shown");
            }
        }
        if (seq != lastUploadedSeq && (DWORD)(now - lastUploadTick) >= kMinUploadIntervalMs) {
            int w = 0, h = 0;
            unsigned copiedSeq = 0;
            if (cap == 0) { cap = 4096 * 4096 * 4; bgra = (uint8_t*)malloc(cap); rgba = (uint8_t*)malloc(cap); }
            if (bgra && rgba && VrHostCopyLatestFrameEx(bgra, cap, &w, &h, &copiedSeq) && w > 0 && h > 0) {
                bool sent = false;
                if (dev && ctx) {
                    // Persistent-texture path (no strobe, no swizzle).
                    if (!tex[0] || texW != w || texH != h) {
                        for (int t = 0; t < 2; ++t) {
                            if (tex[t]) { tex[t]->Release(); tex[t] = nullptr; }
                            D3D11_TEXTURE2D_DESC td{};
                            td.Width = (UINT)w; td.Height = (UINT)h;
                            td.MipLevels = 1; td.ArraySize = 1;
                            td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                            td.SampleDesc.Count = 1;
                            td.Usage = D3D11_USAGE_DEFAULT;
                            td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                            if (FAILED(dev->CreateTexture2D(&td, nullptr, &tex[t]))) {
                                tex[t] = nullptr;
                            }
                        }
                        texW = w; texH = h;
                        if (!tex[0] || !tex[1]) Log("overlay texture create failed -- using SetOverlayRaw");
                    }
                    if (tex[0] && tex[1]) {
                        ID3D11Texture2D* t = tex[texNext];
                        texNext ^= 1;
                        ctx->UpdateSubresource(t, 0, nullptr, bgra, (UINT)w * 4, 0);
                        ctx->Flush();  // copy fully submitted before the compositor sees it
                        vr::Texture_t vt{ (void*)t, vr::TextureType_DirectX, vr::ColorSpace_Auto };
                        sent = (ov->SetOverlayTexture(overlay, &vt) == vr::VROverlayError_None);
                        if (!sent) Log("SetOverlayTexture failed -- using SetOverlayRaw");
                    }
                }
                if (!sent) {
                    // Fallback: CPU upload. RGBA byte order, so swap R<->B.
                    const int n = w * h;
                    for (int i = 0; i < n; ++i) {
                        rgba[i * 4 + 0] = bgra[i * 4 + 2];  // R <- B
                        rgba[i * 4 + 1] = bgra[i * 4 + 1];  // G
                        rgba[i * 4 + 2] = bgra[i * 4 + 0];  // B <- R
                        rgba[i * 4 + 3] = 255;
                    }
                    ov->SetOverlayRaw(overlay, rgba, (uint32_t)w, (uint32_t)h, 4);
                }
                lastUploadedSeq = copiedSeq;
                lastUploadTick  = now;
                ++statsUploads;
            }
        }

        // --- Frame starvation: game quit or stopped presenting ---
        if (overlayVisible && (DWORD)(now - lastSeqChangeTick) > kStarveHideMs) {
            ov->HideOverlay(overlay);
            overlayVisible = false;
            Log("no new frames for 5 s -- overlay hidden (game quit or paused)");
        }

        // --- DebugView heartbeat: game fps vs upload rate, for the next report ---
        if ((DWORD)(now - lastStatsTick) >= kStatsMs) {
            char b[128];
            _snprintf(b, sizeof(b), "stats: %u new frames latched, %u uploaded in %lu ms",
                      seq - statsBaseSeq, statsUploads, (unsigned long)(now - lastStatsTick));
            b[127] = 0;
            Log(b);
            statsBaseSeq  = seq;
            statsUploads  = 0;
            lastStatsTick = now;
        }

        // --- Events (react to SteamVR quitting) ---
        vr::VREvent_t e{};
        while (ov->PollNextOverlayEvent(overlay, &e, sizeof(e))) {
            if (e.eventType == vr::VREvent_Quit) {
                sys->AcknowledgeQuit_Exiting();
                InterlockedExchange(&g_run, 0);
            }
        }
        vr::VREvent_t se{};
        while (sys->PollNextEvent(&se, sizeof(se))) {
            if (se.eventType == vr::VREvent_Quit) {
                sys->AcknowledgeQuit_Exiting();
                InterlockedExchange(&g_run, 0);
            }
        }

        Sleep(4);  // poll fast for pose/latch; uploads are gated above
    }

    free(bgra);
    free(rgba);
    ov->ClearOverlayTexture(overlay);
    ov->DestroyOverlay(overlay);
    for (int t = 0; t < 2; ++t) if (tex[t]) tex[t]->Release();
    if (ctx) ctx->Release();
    if (dev) dev->Release();
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
        // 5 s: the loop wakes within ~4 ms, but teardown includes VR_Shutdown,
        // which can take a while when the compositor is busy.
        if (WaitForSingleObject(g_thread, 5000) == WAIT_TIMEOUT)
            Log("host thread did not exit within 5 s -- teardown may be unclean");
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
}

void SignalStopSteamVrHost() {
    InterlockedExchange(&g_run, 0);
}
