// proxy/frame_capture.cpp
//
// Render-device frame capture for Milestone 1 (headset-free verification of the
// legacy-framebuffer path). See frame_capture.h for the high-level mechanism.
//
// D3D8 COM methods are called via manual vtable indexing so we need no DirectX
// SDK headers -- only fixed, well-documented vtable layouts. Method indices used:
//   IDirect3D8::CreateDevice            = 15
//   IDirect3DDevice8::Present           = 15
//   IDirect3DDevice8::GetBackBuffer     = 16
//   IDirect3DDevice8::CreateImageSurface= 27
//   IDirect3DDevice8::CopyRects         = 28
//   IDirect3DSurface8::Release          = 2
//   IDirect3DSurface8::GetDesc          = 8
//   IDirect3DSurface8::LockRect         = 9
//   IDirect3DSurface8::UnlockRect       = 10

#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "vr_host.h"
#include "frame_decode.h"
#include "perf_stats.h"
#include "readback_ring.h"
#include "focus_hook.h"

// ---- Minimal D3D8 types we touch (layouts are fixed/ABI-stable) -------------

// D3DFORMAT values we care about.
enum {
    FMT_A8R8G8B8 = 21,
    FMT_X8R8G8B8 = 22,
    FMT_R5G6B5   = 23,
    FMT_X1R5G5B5 = 24,
    FMT_A1R5G5B5 = 25,
};

struct D3DSURFACE_DESC_8 {  // 8 x 4-byte fields
    UINT  Format;
    UINT  Type;
    DWORD Usage;
    UINT  Pool;
    UINT  Size;
    UINT  MultiSampleType;
    UINT  Width;
    UINT  Height;
};

struct D3DLOCKED_RECT_8 {
    INT   Pitch;
    void* pBits;
};

struct D3DPRESENT_PARAMETERS_8 {  // fixed D3D8 ABI layout
    UINT  BackBufferWidth;
    UINT  BackBufferHeight;
    UINT  BackBufferFormat;
    UINT  BackBufferCount;
    UINT  MultiSampleType;
    UINT  SwapEffect;
    HWND  hDeviceWindow;
    BOOL  Windowed;
    BOOL  EnableAutoDepthStencil;
    UINT  AutoDepthStencilFormat;
    DWORD Flags;
    UINT  FullScreen_RefreshRateInHz;
    UINT  FullScreen_PresentationInterval;
};

#define D3DLOCK_READONLY 0x00000010L

// COM methods are __stdcall. First arg is the interface (this).
typedef void*  (WINAPI *Direct3DCreate8_t)(UINT SDKVersion);
typedef HRESULT(WINAPI *CreateDevice_t)(void* self, UINT Adapter, UINT DeviceType,
                                        HWND hFocusWindow, DWORD BehaviorFlags,
                                        void* pPresentationParameters,
                                        void** ppReturnedDeviceInterface);
typedef HRESULT(WINAPI *Reset_t)(void* self, D3DPRESENT_PARAMETERS_8* pp);
typedef HRESULT(WINAPI *Present_t)(void* self, const RECT* pSourceRect,
                                   const RECT* pDestRect, HWND hDestWindowOverride,
                                   const void* pDirtyRegion);
typedef HRESULT(WINAPI *GetBackBuffer_t)(void* self, UINT iBackBuffer, UINT Type,
                                         void** ppBackBuffer);
typedef HRESULT(WINAPI *CreateImageSurface_t)(void* self, UINT Width, UINT Height,
                                              UINT Format, void** ppSurface);
typedef HRESULT(WINAPI *CopyRects_t)(void* self, void* pSourceSurface,
                                     const RECT* pSourceRectsArray, UINT cRects,
                                     void* pDestinationSurface,
                                     const POINT* pDestPointsArray);
typedef HRESULT(WINAPI *GetDesc_t)(void* self, D3DSURFACE_DESC_8* pDesc);
typedef HRESULT(WINAPI *LockRect_t)(void* self, D3DLOCKED_RECT_8* pLockedRect,
                                    const RECT* pRect, DWORD Flags);
typedef HRESULT(WINAPI *UnlockRect_t)(void* self);
typedef ULONG  (WINAPI *Release_t)(void* self);

// ---- helpers ----------------------------------------------------------------

static void Log(const char* msg) {
    OutputDebugStringA("[xiii-capture] ");
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
}

// Read a vtable slot: *(void***)obj gives the vtable; [idx] is the method.
static inline void* VtblEntry(void* obj, int idx) {
    return (*(void***)obj)[idx];
}

// Overwrite a vtable slot (patches the shared vtable, affecting all instances).
// Returns the previous entry, or nullptr on failure.
static void* HookVtbl(void* obj, int idx, void* hook) {
    void** vtbl = *(void***)obj;
    DWORD oldProt = 0;
    if (!VirtualProtect(&vtbl[idx], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt))
        return nullptr;
    void* prev = vtbl[idx];
    vtbl[idx] = hook;
    DWORD tmp = 0;
    VirtualProtect(&vtbl[idx], sizeof(void*), oldProt, &tmp);
    return prev;
}

static void CaptureDir(char* out, size_t n) {
    char tmp[MAX_PATH];
    DWORD len = GetTempPathA(MAX_PATH, tmp);
    if (len == 0 || len > MAX_PATH) { lstrcpynA(out, ".\\", (int)n); return; }
    _snprintf(out, n, "%sxiii_capture\\", tmp);
    out[n - 1] = 0;
    CreateDirectoryA(out, nullptr);
}

// Write a 24-bit bottom-up BMP from a locked D3D surface.
static void WriteBMP(const char* path, const D3DSURFACE_DESC_8& d,
                     const D3DLOCKED_RECT_8& lr) {
    const int W = (int)d.Width, H = (int)d.Height;
    if (W <= 0 || H <= 0 || W > 16384 || H > 16384 || !lr.pBits) return;

    const int rowBytes = W * 3;
    const int pad = (4 - (rowBytes & 3)) & 3;
    const int stride = rowBytes + pad;
    const DWORD pixBytes = (DWORD)stride * H;

#pragma pack(push, 1)
    struct { WORD bfType; DWORD bfSize; WORD r1, r2; DWORD bfOffBits; } fh;
    struct { DWORD biSize; LONG biW, biH; WORD biPlanes, biBitCount;
             DWORD biCompression, biSizeImage; LONG xppm, yppm;
             DWORD clrUsed, clrImp; } ih;
#pragma pack(pop)
    fh.bfType = 0x4D42; // 'BM'
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + pixBytes;
    fh.r1 = fh.r2 = 0;
    ih.biSize = sizeof(ih); ih.biW = W; ih.biH = H; ih.biPlanes = 1;
    ih.biBitCount = 24; ih.biCompression = 0; ih.biSizeImage = pixBytes;
    ih.xppm = ih.yppm = 2835; ih.clrUsed = ih.clrImp = 0;

    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD wr = 0;
    WriteFile(f, &fh, sizeof(fh), &wr, nullptr);
    WriteFile(f, &ih, sizeof(ih), &wr, nullptr);

    uint8_t* rowbuf = (uint8_t*)malloc(stride);
    if (rowbuf) {
        memset(rowbuf, 0, stride);
        const uint8_t* base = (const uint8_t*)lr.pBits;
        // BMP is bottom-up: write from last source row to first.
        for (int y = H - 1; y >= 0; --y) {
            const uint8_t* src = base + (size_t)y * lr.Pitch;
            uint8_t* dst = rowbuf;
            for (int x = 0; x < W; ++x) {
                uint8_t r, g, b;
                if (d.Format == FMT_A8R8G8B8 || d.Format == FMT_X8R8G8B8) {
                    const uint8_t* p = src + x * 4;    // memory: B,G,R,A
                    b = p[0]; g = p[1]; r = p[2];
                } else if (d.Format == FMT_R5G6B5) {
                    uint16_t p = *(const uint16_t*)(src + x * 2);
                    r = (uint8_t)(((p >> 11) & 0x1F) * 255 / 31);
                    g = (uint8_t)(((p >> 5)  & 0x3F) * 255 / 63);
                    b = (uint8_t)(( p        & 0x1F) * 255 / 31);
                } else if (d.Format == FMT_X1R5G5B5 || d.Format == FMT_A1R5G5B5) {
                    uint16_t p = *(const uint16_t*)(src + x * 2);
                    r = (uint8_t)(((p >> 10) & 0x1F) * 255 / 31);
                    g = (uint8_t)(((p >> 5)  & 0x1F) * 255 / 31);
                    b = (uint8_t)(( p        & 0x1F) * 255 / 31);
                } else {
                    r = g = b = 0; // unknown format -> black, still proves the path
                }
                dst[0] = b; dst[1] = g; dst[2] = r; // BMP is BGR
                dst += 3;
            }
            WriteFile(f, rowbuf, stride, &wr, nullptr);
        }
        free(rowbuf);
    }
    CloseHandle(f);
}

// ---- hook state -------------------------------------------------------------

static Direct3DCreate8_t s_realCreate8   = nullptr;
static void*             s_realCreateDev = nullptr;  // IDirect3D8::CreateDevice
static void*             s_realPresent   = nullptr;  // IDirect3DDevice8::Present
static void*             s_realReset     = nullptr;  // IDirect3DDevice8::Reset
static HWND              s_focusWnd      = nullptr;  // from CreateDevice
static void**            s_iatSlot       = nullptr;  // patched IAT entry
static LONG              s_devHooked      = 0;
static LONG              s_d3dHooked      = 0;
static LONG              s_frame          = 0;
// Cached system-memory readback surfaces (recreated only on size/format
// change). Two slots: the pipelined readback (0.2.5) CopyRects into one while
// locking the other, whose copy was issued a full capture interval earlier.
static void*             s_sysSurf[2]     = { nullptr, nullptr };
static UINT              s_sysW = 0, s_sysH = 0, s_sysFmt = 0;
static xiii::ReadbackRing s_ring;

static void ReleaseSysSurfaces() {
    for (int i = 0; i < 2; ++i) {
        if (s_sysSurf[i]) {
            ((Release_t)VtblEntry(s_sysSurf[i], 2))(s_sysSurf[i]);
            s_sysSurf[i] = nullptr;
        }
    }
    s_sysW = s_sysH = s_sysFmt = 0;
    s_ring.Reset();
}

// Frames to capture (spread across the first several seconds to skip black
// loading screens); capture stops after the last one.
static const LONG kCaptureAt[] = { 60, 180, 300, 600, 900 };

// ---- Capture-path performance (0.2.4) ----------------------------------------
//
// The readback (CopyRects -> LockRect -> decode -> latch) used to run at game
// fps on the game's render thread, though the overlay upload is capped at
// ~90 Hz -- and it ran even with VR disabled. Now:
//   * capture only runs when a VR host is enabled in XIII.ini (or for the
//     occasional debug BMP frame),
//   * it is rate-capped ([VR] CaptureMinIntervalMs, default 11 ms ~= 90 Hz,
//     0 = uncapped),
//   * every phase is timed and summarized in a [xiii-perf] heartbeat so the
//     next hardware report says where the remaining time goes.
static bool     s_vrConsumer    = false;  // any VR host enabled in XIII.ini
static UINT     s_minIntervalMs = 11;
// 0.2.5: lock the PREVIOUS tick's surface instead of the one just copied, so
// the lock never waits on the copy just issued ([VR] PipelinedReadback; costs
// one capture interval of overlay latency). Default OFF: the dev-machine A/B
// showed the stall lives INSIDE CopyRects (driver syncs at blit time), where
// double-buffering cannot help -- flip it on per-machine to test whether the
// local driver defers the blit instead. Debug BMP frames always capture
// synchronously so the BMP shows the exact frame.
static bool     s_pipelined     = false;
static LARGE_INTEGER   s_qpcFreq  = {};
static xiii::PhaseStats s_stCopy, s_stLock, s_stDecode, s_stSubmit;
static uint32_t s_stPresents = 0, s_stCaptured = 0, s_stSkipped = 0;
// Reused decode target (was a malloc/free of the whole frame every Present).
static uint8_t* s_bgraBuf = nullptr;
static size_t   s_bgraCap = 0;

static xiii::RateLimiter& CaptureLimiter() {
    static xiii::RateLimiter rl(s_minIntervalMs);  // first use is after config load
    return rl;
}
static xiii::RateLimiter& StatsLimiter() {
    static xiii::RateLimiter rl(5000);  // heartbeat cadence, matches the hosts
    return rl;
}

static UINT ReadVrInt(const char* key, UINT def) {
    char ini[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, ini, MAX_PATH)) return def;
    char* slash = strrchr(ini, '\\');
    if (!slash) return def;
    lstrcpyA(slash + 1, "XIII.ini");
    return (UINT)GetPrivateProfileIntA("VR", key, (INT)def, ini);
}

static uint32_t DeltaUs(const LARGE_INTEGER& a, const LARGE_INTEGER& b) {
    if (!s_qpcFreq.QuadPart) QueryPerformanceFrequency(&s_qpcFreq);
    if (!s_qpcFreq.QuadPart) return 0;
    return (uint32_t)((b.QuadPart - a.QuadPart) * 1000000 / s_qpcFreq.QuadPart);
}

static void ReportPerfStats() {
    char b[256];
    // Single OutputDebugString call so the line arrives atomically (multi-call
    // logging interleaves across threads and splits into separate DBWIN events).
    _snprintf(b, sizeof(b),
              "[xiii-perf] present=%lu captured=%lu skipped=%lu (cap %ums pipe=%d) | us avg/max: "
              "copy=%u/%u lock=%u/%u decode=%u/%u submit=%u/%u\n",
              (unsigned long)s_stPresents, (unsigned long)s_stCaptured,
              (unsigned long)s_stSkipped, s_minIntervalMs, s_pipelined ? 1 : 0,
              s_stCopy.AvgUs(), s_stCopy.maxUs, s_stLock.AvgUs(), s_stLock.maxUs,
              s_stDecode.AvgUs(), s_stDecode.maxUs,
              s_stSubmit.AvgUs(), s_stSubmit.maxUs);
    b[sizeof(b) - 1] = 0;
    OutputDebugStringA(b);
    s_stCopy.Reset(); s_stLock.Reset(); s_stDecode.Reset(); s_stSubmit.Reset();
    s_stPresents = s_stCaptured = s_stSkipped = 0;
}

// ---- Windowed backbuffer clamp ----------------------------------------------
//
// In windowed mode the game asks for a DESKTOP-sized backbuffer (observed
// 3440x1440 on hardware) but sets its viewport to the window's client size
// (1280x960), so it renders into the top-left corner and the rest stays black.
// The desktop window looks fine (Present clips to the window), but the VR
// capture reads the whole backbuffer, so the headset overlay showed the game
// squished into a corner of dead space. Clamping the requested backbuffer to
// the device window's client size makes the viewport fill it exactly. Applied
// on CreateDevice AND Reset (res changes go through Reset with fresh params).
static void ClampWindowedBackbuffer(D3DPRESENT_PARAMETERS_8* pp) {
    if (!pp || !pp->Windowed) return;
    HWND w = pp->hDeviceWindow ? pp->hDeviceWindow : s_focusWnd;
    RECT rc;
    if (!w || !GetClientRect(w, &rc) || rc.right <= 0 || rc.bottom <= 0) return;
    if (pp->BackBufferWidth == (UINT)rc.right && pp->BackBufferHeight == (UINT)rc.bottom)
        return;
    char b[128];
    _snprintf(b, sizeof(b), "windowed backbuffer clamped %ux%u -> %ldx%ld",
              pp->BackBufferWidth, pp->BackBufferHeight, rc.right, rc.bottom);
    b[127] = 0;
    Log(b);
    pp->BackBufferWidth  = (UINT)rc.right;
    pp->BackBufferHeight = (UINT)rc.bottom;
}

static HRESULT WINAPI Hook_Reset(void* dev, D3DPRESENT_PARAMETERS_8* pp) {
    ClampWindowedBackbuffer(pp);
    if (pp && pp->hDeviceWindow) FocusHookSetGameWindow(pp->hDeviceWindow);
    // The cached readback surfaces belong to the pre-Reset device state; drop
    // them so the next Present recreates them at the new size.
    ReleaseSysSurfaces();
    return ((Reset_t)s_realReset)(dev, pp);
}

// ---- Present hook -----------------------------------------------------------

// One readback + decode + latch, with each phase timed into the perf stats.
static void CaptureFrame(void* dev, LONG n, bool debugFrame) {
    void* bb = nullptr;
    GetBackBuffer_t GetBackBuffer = (GetBackBuffer_t)VtblEntry(dev, 16);
    if (!GetBackBuffer || GetBackBuffer(dev, 0, /*MONO*/0, &bb) != 0 || !bb) return;
    GetDesc_t GetDesc = (GetDesc_t)VtblEntry(bb, 8);
    D3DSURFACE_DESC_8 desc; memset(&desc, 0, sizeof(desc));
    if (GetDesc && GetDesc(bb, &desc) == 0) {
        // (Re)create both cached readback slots only on size/format change.
        if (!s_sysSurf[0] || !s_sysSurf[1] || s_sysW != desc.Width ||
            s_sysH != desc.Height || s_sysFmt != desc.Format) {
            ReleaseSysSurfaces();
            CreateImageSurface_t CreateImageSurface =
                (CreateImageSurface_t)VtblEntry(dev, 27);
            if (CreateImageSurface &&
                CreateImageSurface(dev, desc.Width, desc.Height, desc.Format,
                                   &s_sysSurf[0]) == 0 &&
                CreateImageSurface(dev, desc.Width, desc.Height, desc.Format,
                                   &s_sysSurf[1]) == 0) {
                s_sysW = desc.Width; s_sysH = desc.Height; s_sysFmt = desc.Format;
            } else { ReleaseSysSurfaces(); }
        }
        if (s_sysSurf[0] && s_sysSurf[1]) {
            // Pipelined mode locks the slot whose copy was committed on the
            // PREVIOUS capture tick, so the choice must be made before
            // CommitCopy() below. Debug BMP frames (and the gate being off)
            // stay synchronous: lock the slot we are about to fill, so the
            // BMP shows this exact frame and works on the priming tick too.
            const bool sync     = debugFrame || !s_pipelined;
            const int  copySlot = s_ring.CopySlot();
            const int  lockSlot = sync ? copySlot : s_ring.LockableSlot();

            LARGE_INTEGER t0, t1, t2, t3, t4;
            CopyRects_t CopyRects = (CopyRects_t)VtblEntry(dev, 28);
            QueryPerformanceCounter(&t0);
            if (CopyRects &&
                CopyRects(dev, bb, nullptr, 0, s_sysSurf[copySlot], nullptr) == 0) {
                QueryPerformanceCounter(&t1);
                s_stCopy.Add(DeltaUs(t0, t1));
                s_ring.CommitCopy();
                void* lockSurf = (lockSlot >= 0) ? s_sysSurf[lockSlot] : nullptr;
                LockRect_t LockRect =
                    lockSurf ? (LockRect_t)VtblEntry(lockSurf, 9) : nullptr;
                UnlockRect_t UnlockRect =
                    lockSurf ? (UnlockRect_t)VtblEntry(lockSurf, 10) : nullptr;
                D3DLOCKED_RECT_8 lr; memset(&lr, 0, sizeof(lr));
                // lockSlot -1 = pipelined priming tick (fresh surfaces, no
                // committed copy yet): skip the decode, next tick has data.
                if (LockRect && LockRect(lockSurf, &lr, nullptr, D3DLOCK_READONLY) == 0) {
                    QueryPerformanceCounter(&t2);
                    s_stLock.Add(DeltaUs(t1, t2));
                    const size_t need = (size_t)desc.Width * desc.Height * 4;
                    if (s_bgraCap < need) {
                        free(s_bgraBuf);
                        s_bgraBuf = (uint8_t*)malloc(need);
                        s_bgraCap = s_bgraBuf ? need : 0;
                    }
                    if (s_bgraBuf &&
                        xiii::DecodeToBGRA(desc.Format, (int)desc.Width, (int)desc.Height,
                                           (const uint8_t*)lr.pBits, lr.Pitch, s_bgraBuf)) {
                        QueryPerformanceCounter(&t3);
                        s_stDecode.Add(DeltaUs(t2, t3));
                        VrHostSubmitFrame(s_bgraBuf, (int)desc.Width, (int)desc.Height);
                        QueryPerformanceCounter(&t4);
                        s_stSubmit.Add(DeltaUs(t3, t4));
                        ++s_stCaptured;
                        if (debugFrame) {
                            char dir[MAX_PATH]; CaptureDir(dir, sizeof(dir));
                            char path[MAX_PATH];
                            _snprintf(path, sizeof(path), "%sxiii_frame_%03ld.bmp", dir, n);
                            path[sizeof(path) - 1] = 0;
                            WriteBMP(path, desc, lr);
                            char d11[MAX_PATH];
                            _snprintf(d11, sizeof(d11), "%sd3d11_frame_%03ld.bmp", dir, n);
                            d11[sizeof(d11) - 1] = 0;
                            VrHostDebugSaveTexture(d11);
                            Log(path);
                        }
                    }
                    if (UnlockRect) UnlockRect(lockSurf);
                }
            }
        }
    }
    ((Release_t)VtblEntry(bb, 2))(bb);
}

static HRESULT WINAPI Hook_Present(void* dev, const RECT* s, const RECT* d,
                                   HWND hwnd, const void* dirty) {
    LONG n = InterlockedIncrement(&s_frame);
    bool debugFrame = false;  // occasionally also dump BMPs for verification
    for (int i = 0; i < (int)(sizeof(kCaptureAt) / sizeof(kCaptureAt[0])); ++i)
        if (kCaptureAt[i] == n) { debugFrame = true; break; }

    ++s_stPresents;
    // Capture only when someone consumes frames: a VR host, or a debug BMP
    // frame (which must capture regardless of the rate cap, so desk
    // verification on a VR-less machine still produces its BMPs).
    if (dev && (s_vrConsumer || debugFrame)) {
        if (debugFrame || CaptureLimiter().Allow(GetTickCount()))
            CaptureFrame(dev, n, debugFrame);
        else
            ++s_stSkipped;
    }
    if (s_vrConsumer && StatsLimiter().Allow(GetTickCount()))
        ReportPerfStats();

    // Always forward the real Present so the desktop window keeps updating.
    return ((Present_t)s_realPresent)(dev, s, d, hwnd, dirty);
}

// ---- CreateDevice hook: grab the device, hook its Present --------------------

static HRESULT WINAPI Hook_CreateDevice(void* self, UINT Adapter, UINT DeviceType,
                                        HWND hFocusWindow, DWORD BehaviorFlags,
                                        void* pPresentationParameters,
                                        void** ppReturnedDeviceInterface) {
    s_focusWnd = hFocusWindow;
    ClampWindowedBackbuffer((D3DPRESENT_PARAMETERS_8*)pPresentationParameters);
    {
        D3DPRESENT_PARAMETERS_8* pp =
            (D3DPRESENT_PARAMETERS_8*)pPresentationParameters;
        HWND w = (pp && pp->hDeviceWindow) ? pp->hDeviceWindow : hFocusWindow;
        if (w) FocusHookSetGameWindow(w);
    }
    HRESULT hr = ((CreateDevice_t)s_realCreateDev)(
        self, Adapter, DeviceType, hFocusWindow, BehaviorFlags,
        pPresentationParameters, ppReturnedDeviceInterface);
    if (hr == 0 && ppReturnedDeviceInterface && *ppReturnedDeviceInterface &&
        InterlockedCompareExchange(&s_devHooked, 1, 0) == 0) {
        void* dev = *ppReturnedDeviceInterface;
        s_realPresent = HookVtbl(dev, 15, (void*)&Hook_Present);
        Log(s_realPresent ? "Present hooked" : "Present hook FAILED");
        s_realReset = HookVtbl(dev, 14, (void*)&Hook_Reset);
        Log(s_realReset ? "Reset hooked" : "Reset hook FAILED");
    }
    return hr;
}

// ---- Direct3DCreate8 hook: hook CreateDevice on the returned IDirect3D8 ------

static void* WINAPI Hook_Direct3DCreate8(UINT SDKVersion) {
    void* d3d = s_realCreate8 ? s_realCreate8(SDKVersion) : nullptr;
    if (d3d && InterlockedCompareExchange(&s_d3dHooked, 1, 0) == 0) {
        s_realCreateDev = HookVtbl(d3d, 15, (void*)&Hook_CreateDevice);
        Log(s_realCreateDev ? "CreateDevice hooked" : "CreateDevice hook FAILED");
    }
    return d3d;
}

// ---- IAT hook of d3d8.dll!Direct3DCreate8 in D3DDrv_Original -----------------

void InstallFrameCapture() {
    // Config first (before any Present can run): does anything consume frames,
    // and how hard may the readback hit the render thread?
    s_vrConsumer = ReadVrInt("SteamVR", 0) != 0 || ReadVrInt("OpenXR", 0) != 0;
    s_minIntervalMs = ReadVrInt("CaptureMinIntervalMs", 11);
    s_pipelined = ReadVrInt("PipelinedReadback", 0) != 0;
    {
        char b[128];
        _snprintf(b, sizeof(b),
                  "capture: vr consumer=%d, min interval=%u ms, pipelined=%d",
                  s_vrConsumer ? 1 : 0, s_minIntervalMs, s_pipelined ? 1 : 0);
        b[127] = 0;
        Log(b);
    }

    HMODULE hMod = GetModuleHandleA("D3DDrv_Original.dll");
    if (!hMod) { Log("D3DDrv_Original not loaded; cannot install capture"); return; }

    BYTE* base = (BYTE*)hMod;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;

    IMAGE_DATA_DIRECTORY& impDir =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!impDir.VirtualAddress) return;

    IMAGE_IMPORT_DESCRIPTOR* imp =
        (IMAGE_IMPORT_DESCRIPTOR*)(base + impDir.VirtualAddress);
    for (; imp->Name; ++imp) {
        const char* dllName = (const char*)(base + imp->Name);
        if (lstrcmpiA(dllName, "d3d8.dll") != 0) continue;

        IMAGE_THUNK_DATA* orig =
            (IMAGE_THUNK_DATA*)(base + (imp->OriginalFirstThunk
                                            ? imp->OriginalFirstThunk
                                            : imp->FirstThunk));
        IMAGE_THUNK_DATA* iat = (IMAGE_THUNK_DATA*)(base + imp->FirstThunk);
        for (; orig->u1.AddressOfData; ++orig, ++iat) {
            if (orig->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue; // by-ordinal
            IMAGE_IMPORT_BY_NAME* byName =
                (IMAGE_IMPORT_BY_NAME*)(base + orig->u1.AddressOfData);
            if (lstrcmpA((const char*)byName->Name, "Direct3DCreate8") != 0)
                continue;

            s_iatSlot = (void**)&iat->u1.Function;
            s_realCreate8 = (Direct3DCreate8_t)*s_iatSlot;
            DWORD oldProt = 0;
            if (VirtualProtect(s_iatSlot, sizeof(void*),
                               PAGE_EXECUTE_READWRITE, &oldProt)) {
                *s_iatSlot = (void*)&Hook_Direct3DCreate8;
                DWORD tmp = 0;
                VirtualProtect(s_iatSlot, sizeof(void*), oldProt, &tmp);
                Log("Direct3DCreate8 IAT hooked");
            }
            return;
        }
    }
    Log("Direct3DCreate8 import not found");
}
