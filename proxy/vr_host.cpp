// proxy/vr_host.cpp
#include "vr_host.h"
#include <windows.h>
#include <d3d11.h>
#include <cstdio>
#include <cstring>
#include <cmath>

static ID3D11Device*        g_dev = nullptr;
static ID3D11DeviceContext* g_ctx = nullptr;
static ID3D11Texture2D*     g_tex = nullptr;  // DEFAULT usage, BGRA8 (swapchain-shaped)
static int g_w = 0, g_h = 0;

// Latest captured frame as a CPU BGRA buffer (for the OpenXR host to upload
// into a swapchain image), guarded by a critical section.
static CRITICAL_SECTION g_lock;
static bool     g_lockInit = false;
static uint8_t* g_latest   = nullptr;
static int      g_latestW = 0, g_latestH = 0;
static unsigned g_latestSeq = 0;  // ++ per submit; consumers skip stale frames

// Head pose (quaternion). g_havePose flips true once OpenXR feeds a real pose;
// until then VrHostGetHeadPose returns a synthetic yaw. Plain globals -- a rare
// torn read is only a one-frame cosmetic glitch.
static volatile bool g_havePose = false;
static volatile float g_pose[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

static void Log(const char* m) {
    OutputDebugStringA("[xiii-vrhost] "); OutputDebugStringA(m); OutputDebugStringA("\n");
}

bool VrHostInit() {
    UINT flags = 0;
    D3D_FEATURE_LEVEL got{};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                   nullptr, 0, D3D11_SDK_VERSION, &g_dev, &got, &g_ctx);
    if (FAILED(hr)) {
        // Software fallback -- fine for the dev machine; perf isn't judged here.
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
                               nullptr, 0, D3D11_SDK_VERSION, &g_dev, &got, &g_ctx);
    }
    if (FAILED(hr)) { Log("D3D11CreateDevice failed"); return false; }
    if (!g_lockInit) { InitializeCriticalSection(&g_lock); g_lockInit = true; }
    Log("D3D11 device created");
    return true;
}

void VrHostShutdown() {
    if (g_tex) { g_tex->Release(); g_tex = nullptr; }
    if (g_ctx) { g_ctx->Release(); g_ctx = nullptr; }
    if (g_dev) { g_dev->Release(); g_dev = nullptr; }
    g_w = g_h = 0;
}

static bool EnsureTexture(int w, int h) {
    if (g_tex && w == g_w && h == g_h) return true;
    if (g_tex) { g_tex->Release(); g_tex = nullptr; }
    D3D11_TEXTURE2D_DESC d{};
    d.Width = w; d.Height = h; d.MipLevels = 1; d.ArraySize = 1;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;  // sampleable, as a swapchain image is
    HRESULT hr = g_dev->CreateTexture2D(&d, nullptr, &g_tex);
    if (FAILED(hr)) { Log("CreateTexture2D failed"); g_tex = nullptr; return false; }
    g_w = w; g_h = h;
    return true;
}

void VrHostSubmitFrame(const uint8_t* bgra, int width, int height) {
    if (!bgra || width <= 0 || height <= 0) return;
    // Latch a CPU copy for the VR hosts. NOTE: no per-frame D3D11 upload here
    // any more -- that was a full-framebuffer UpdateSubresource on the game's
    // render thread every Present, and the only consumer was the occasional
    // debug BMP. VrHostDebugSaveTexture now uploads on demand instead.
    if (!g_lockInit) { InitializeCriticalSection(&g_lock); g_lockInit = true; }
    EnterCriticalSection(&g_lock);
    const size_t bytes = (size_t)width * height * 4;
    if (g_latestW != width || g_latestH != height) {
        free(g_latest);
        g_latest = (uint8_t*)malloc(bytes);
        g_latestW = g_latest ? width : 0;
        g_latestH = g_latest ? height : 0;
    }
    if (g_latest) {
        memcpy(g_latest, bgra, bytes);
        ++g_latestSeq;
    }
    LeaveCriticalSection(&g_lock);
}

bool VrHostCopyLatestFrameEx(uint8_t* dst, int cap, int* width, int* height,
                             unsigned* seq) {
    if (!g_lockInit || !dst) return false;
    bool ok = false;
    EnterCriticalSection(&g_lock);
    const size_t bytes = (size_t)g_latestW * g_latestH * 4;
    if (g_latest && bytes > 0 && (int)bytes <= cap) {
        memcpy(dst, g_latest, bytes);
        if (width)  *width  = g_latestW;
        if (height) *height = g_latestH;
        if (seq)    *seq    = g_latestSeq;
        ok = true;
    }
    LeaveCriticalSection(&g_lock);
    return ok;
}

bool VrHostCopyLatestFrame(uint8_t* dst, int cap, int* width, int* height) {
    return VrHostCopyLatestFrameEx(dst, cap, width, height, nullptr);
}

unsigned VrHostLatestFrameSeq() {
    if (!g_lockInit) return 0;
    EnterCriticalSection(&g_lock);
    unsigned s = g_latestSeq;
    LeaveCriticalSection(&g_lock);
    return s;
}

void VrHostSetHeadPose(float x, float y, float z, float w) {
    g_pose[0] = x; g_pose[1] = y; g_pose[2] = z; g_pose[3] = w;
    g_havePose = true;
}

// Write a 24-bit bottom-up BMP from a mapped BGRA8 D3D11 readback.
static void WriteBGRA_BMP(const char* path, const uint8_t* bgra, int W, int H, int rowPitch) {
    const int rowBytes = W * 3;
    const int pad = (4 - (rowBytes & 3)) & 3;
    const int stride = rowBytes + pad;
    const DWORD pixBytes = (DWORD)stride * H;
#pragma pack(push, 1)
    struct { WORD bfType; DWORD bfSize; WORD r1, r2; DWORD bfOffBits; } fh;
    struct { DWORD biSize; LONG biW, biH; WORD biPlanes, biBitCount;
             DWORD biCompression, biSizeImage; LONG xppm, yppm; DWORD clrUsed, clrImp; } ih;
#pragma pack(pop)
    fh.bfType = 0x4D42; fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + pixBytes; fh.r1 = fh.r2 = 0;
    ih.biSize = sizeof(ih); ih.biW = W; ih.biH = H; ih.biPlanes = 1;
    ih.biBitCount = 24; ih.biCompression = 0; ih.biSizeImage = pixBytes;
    ih.xppm = ih.yppm = 2835; ih.clrUsed = ih.clrImp = 0;

    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD wr = 0;
    WriteFile(f, &fh, sizeof(fh), &wr, nullptr);
    WriteFile(f, &ih, sizeof(ih), &wr, nullptr);
    uint8_t* row = (uint8_t*)malloc(stride);
    if (row) {
        memset(row, 0, stride);
        for (int y = H - 1; y >= 0; --y) {           // BMP is bottom-up
            const uint8_t* src = bgra + (size_t)y * rowPitch;  // BGRA source
            for (int x = 0; x < W; ++x) {
                row[x * 3 + 0] = src[x * 4 + 0];     // B
                row[x * 3 + 1] = src[x * 4 + 1];     // G
                row[x * 3 + 2] = src[x * 4 + 2];     // R
            }
            WriteFile(f, row, stride, &wr, nullptr);
        }
        free(row);
    }
    CloseHandle(f);
}

bool VrHostGetHeadPose(float* x, float* y, float* z, float* w) {
    if (!x || !y || !z || !w) return false;
    if (g_havePose) {  // real OpenXR pose once available
        *x = g_pose[0]; *y = g_pose[1]; *z = g_pose[2]; *w = g_pose[3];
        return true;
    }
    // Synthetic slow yaw about the Y axis (~one revolution per 12s) standing in
    // for the OpenXR HMD pose. Replaced by the real headset orientation later.
    const float twoPi = 6.28318530717958647692f;
    float t = (float)GetTickCount() * 0.001f;
    float angle = fmodf(t * (twoPi / 12.0f), twoPi);
    float h = angle * 0.5f;
    *x = 0.0f; *y = sinf(h); *z = 0.0f; *w = cosf(h);
    return true;
}

bool VrHostDebugSaveTexture(const char* path) {
    if (!g_dev) VrHostInit();  // lazy; only debug frames pay for the device
    if (!g_dev || !g_ctx || !g_lockInit) return false;
    // Upload the latched frame on demand (the per-frame upload is gone from
    // VrHostSubmitFrame), so this still proves the D3D8 -> CPU -> D3D11 bridge.
    EnterCriticalSection(&g_lock);
    bool uploaded = false;
    if (g_latest && g_latestW > 0 && g_latestH > 0 &&
        EnsureTexture(g_latestW, g_latestH)) {
        g_ctx->UpdateSubresource(g_tex, 0, nullptr, g_latest, (UINT)g_latestW * 4, 0);
        uploaded = true;
    }
    LeaveCriticalSection(&g_lock);
    if (!uploaded || !g_tex) return false;
    D3D11_TEXTURE2D_DESC d{};
    g_tex->GetDesc(&d);
    D3D11_TEXTURE2D_DESC sd = d;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.BindFlags = 0;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    sd.MiscFlags = 0;
    ID3D11Texture2D* stg = nullptr;
    if (FAILED(g_dev->CreateTexture2D(&sd, nullptr, &stg))) return false;
    g_ctx->CopyResource(stg, g_tex);
    D3D11_MAPPED_SUBRESOURCE ms{};
    bool ok = false;
    if (SUCCEEDED(g_ctx->Map(stg, 0, D3D11_MAP_READ, 0, &ms))) {
        WriteBGRA_BMP(path, (const uint8_t*)ms.pData, (int)d.Width, (int)d.Height,
                      (int)ms.RowPitch);
        g_ctx->Unmap(stg, 0);
        ok = true;
    }
    stg->Release();
    return ok;
}
