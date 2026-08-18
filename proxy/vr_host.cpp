// proxy/vr_host.cpp
#include "vr_host.h"
#include <windows.h>
#include <d3d11.h>
#include <cstdio>
#include <cstring>

static ID3D11Device*        g_dev = nullptr;
static ID3D11DeviceContext* g_ctx = nullptr;
static ID3D11Texture2D*     g_tex = nullptr;  // DEFAULT usage, BGRA8 (swapchain-shaped)
static int g_w = 0, g_h = 0;

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
    if (!g_dev) VrHostInit();  // lazy: create the D3D11 device off the DllMain/loader-lock path
    if (!g_dev || !g_ctx || !bgra || width <= 0 || height <= 0) return;
    if (!EnsureTexture(width, height)) return;
    // DEFAULT texture: upload with UpdateSubresource (source is tightly packed).
    g_ctx->UpdateSubresource(g_tex, 0, nullptr, bgra, (UINT)width * 4, 0);
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

bool VrHostDebugSaveTexture(const char* path) {
    if (!g_dev || !g_ctx || !g_tex) return false;
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
