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

#define D3DLOCK_READONLY 0x00000010L

// COM methods are __stdcall. First arg is the interface (this).
typedef void*  (WINAPI *Direct3DCreate8_t)(UINT SDKVersion);
typedef HRESULT(WINAPI *CreateDevice_t)(void* self, UINT Adapter, UINT DeviceType,
                                        HWND hFocusWindow, DWORD BehaviorFlags,
                                        void* pPresentationParameters,
                                        void** ppReturnedDeviceInterface);
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
static void**            s_iatSlot       = nullptr;  // patched IAT entry
static LONG              s_devHooked      = 0;
static LONG              s_d3dHooked      = 0;
static LONG              s_frame          = 0;

// Frames to capture (spread across the first several seconds to skip black
// loading screens); capture stops after the last one.
static const LONG kCaptureAt[] = { 60, 180, 300, 600, 900 };

// Decode a locked D3D8 surface into a tightly-packed BGRA8 top-down buffer
// (caller frees). Returns nullptr on failure. Matches DXGI_FORMAT_B8G8R8A8.
static uint8_t* DecodeSurfaceToBGRA(const D3DSURFACE_DESC_8& d,
                                    const D3DLOCKED_RECT_8& lr) {
    const int W = (int)d.Width, H = (int)d.Height;
    if (W <= 0 || H <= 0 || W > 16384 || H > 16384 || !lr.pBits) return nullptr;
    uint8_t* out = (uint8_t*)malloc((size_t)W * H * 4);
    if (!out) return nullptr;
    const uint8_t* base = (const uint8_t*)lr.pBits;
    for (int y = 0; y < H; ++y) {
        const uint8_t* src = base + (size_t)y * lr.Pitch;
        uint8_t* dst = out + (size_t)y * W * 4;
        for (int x = 0; x < W; ++x) {
            uint8_t r, g, b;
            if (d.Format == FMT_A8R8G8B8 || d.Format == FMT_X8R8G8B8) {
                const uint8_t* p = src + x * 4; b = p[0]; g = p[1]; r = p[2];
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
            } else { r = g = b = 0; }
            dst[x * 4 + 0] = b; dst[x * 4 + 1] = g; dst[x * 4 + 2] = r; dst[x * 4 + 3] = 0xFF;
        }
    }
    return out;
}

// ---- Present hook -----------------------------------------------------------

static HRESULT WINAPI Hook_Present(void* dev, const RECT* s, const RECT* d,
                                   HWND hwnd, const void* dirty) {
    LONG n = InterlockedIncrement(&s_frame);
    bool want = false;
    for (int i = 0; i < (int)(sizeof(kCaptureAt) / sizeof(kCaptureAt[0])); ++i)
        if (kCaptureAt[i] == n) { want = true; break; }

    if (want && dev) {
        void* bb = nullptr;
        GetBackBuffer_t GetBackBuffer = (GetBackBuffer_t)VtblEntry(dev, 16);
        if (GetBackBuffer && GetBackBuffer(dev, 0, /*MONO*/0, &bb) == 0 && bb) {
            GetDesc_t GetDesc = (GetDesc_t)VtblEntry(bb, 8);
            D3DSURFACE_DESC_8 desc; memset(&desc, 0, sizeof(desc));
            if (GetDesc && GetDesc(bb, &desc) == 0) {
                void* sys = nullptr;
                CreateImageSurface_t CreateImageSurface =
                    (CreateImageSurface_t)VtblEntry(dev, 27);
                if (CreateImageSurface &&
                    CreateImageSurface(dev, desc.Width, desc.Height, desc.Format,
                                       &sys) == 0 && sys) {
                    CopyRects_t CopyRects = (CopyRects_t)VtblEntry(dev, 28);
                    if (CopyRects && CopyRects(dev, bb, nullptr, 0, sys,
                                               nullptr) == 0) {
                        LockRect_t LockRect   = (LockRect_t)VtblEntry(sys, 9);
                        UnlockRect_t UnlockRect = (UnlockRect_t)VtblEntry(sys, 10);
                        D3DLOCKED_RECT_8 lr; memset(&lr, 0, sizeof(lr));
                        if (LockRect &&
                            LockRect(sys, &lr, nullptr, D3DLOCK_READONLY) == 0) {
                            char dir[MAX_PATH]; CaptureDir(dir, sizeof(dir));
                            char path[MAX_PATH];
                            _snprintf(path, sizeof(path), "%sxiii_frame_%03ld.bmp",
                                      dir, n);
                            path[sizeof(path) - 1] = 0;
                            WriteBMP(path, desc, lr);
                            // 0.2.0 frame bridge: push this frame into the VR
                            // host's D3D11 texture, then read it back, so we can
                            // confirm the D3D8 -> CPU -> D3D11 path is intact by
                            // comparing d3d11_frame_NNN.bmp with xiii_frame_NNN.bmp.
                            if (uint8_t* bgra = DecodeSurfaceToBGRA(desc, lr)) {
                                VrHostSubmitFrame(bgra, (int)desc.Width, (int)desc.Height);
                                char d11[MAX_PATH];
                                _snprintf(d11, sizeof(d11), "%sd3d11_frame_%03ld.bmp", dir, n);
                                d11[sizeof(d11) - 1] = 0;
                                VrHostDebugSaveTexture(d11);
                                free(bgra);
                            }
                            if (UnlockRect) UnlockRect(sys);
                            Log(path);
                        }
                    }
                    ((Release_t)VtblEntry(sys, 2))(sys);
                }
            }
            ((Release_t)VtblEntry(bb, 2))(bb);
        }
    }

    // Always forward the real Present so the desktop window keeps updating.
    return ((Present_t)s_realPresent)(dev, s, d, hwnd, dirty);
}

// ---- CreateDevice hook: grab the device, hook its Present --------------------

static HRESULT WINAPI Hook_CreateDevice(void* self, UINT Adapter, UINT DeviceType,
                                        HWND hFocusWindow, DWORD BehaviorFlags,
                                        void* pPresentationParameters,
                                        void** ppReturnedDeviceInterface) {
    HRESULT hr = ((CreateDevice_t)s_realCreateDev)(
        self, Adapter, DeviceType, hFocusWindow, BehaviorFlags,
        pPresentationParameters, ppReturnedDeviceInterface);
    if (hr == 0 && ppReturnedDeviceInterface && *ppReturnedDeviceInterface &&
        InterlockedCompareExchange(&s_devHooked, 1, 0) == 0) {
        void* dev = *ppReturnedDeviceInterface;
        s_realPresent = HookVtbl(dev, 15, (void*)&Hook_Present);
        Log(s_realPresent ? "Present hooked" : "Present hook FAILED");
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
