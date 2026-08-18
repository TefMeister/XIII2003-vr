// proxy/openxr_host.cpp
//
// OpenXR VR host: submits the captured game frame to a headset as a quad layer
// and reads back the head pose. See openxr_host.h. UNTESTED on the dev machine
// (no runtime/headset) -- written to compile cleanly and be spec-faithful; it
// runs for real only on the user's home machine + Quest.

#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_PLATFORM_WIN32
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "openxr_host.h"
#include "vr_host.h"

static void Log(const char* m) {
    OutputDebugStringA("[xiii-openxr] "); OutputDebugStringA(m); OutputDebugStringA("\n");
}
static void Logf(const char* what, XrResult r) {
    char b[128]; _snprintf(b, sizeof(b), "%s -> XrResult %d", what, (int)r); b[127] = 0; Log(b);
}

static HANDLE          g_thread  = nullptr;
static volatile LONG   g_run     = 0;

static bool ConfigEnabled() {
    char exe[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, exe, MAX_PATH)) return false;
    char* slash = strrchr(exe, '\\');
    if (!slash) return false;
    lstrcpyA(slash + 1, "XIII.ini");
    return GetPrivateProfileIntA("VR", "OpenXR", 0, exe) != 0;
}

// Create a D3D11 device on the adapter the runtime requires (matched by LUID).
static bool CreateMatchingD3D11(const LUID& luid, ID3D11Device** dev,
                                ID3D11DeviceContext** ctx, D3D_FEATURE_LEVEL minFL) {
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory))) return false;
    IDXGIAdapter1* adapter = nullptr;
    IDXGIAdapter1* chosen = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) == S_OK; ++i) {
        DXGI_ADAPTER_DESC1 ad{};
        if (SUCCEEDED(adapter->GetDesc1(&ad)) &&
            ad.AdapterLuid.LowPart == luid.LowPart &&
            ad.AdapterLuid.HighPart == luid.HighPart) {
            chosen = adapter; break;
        }
        adapter->Release();
    }
    factory->Release();
    if (!chosen) return false;
    D3D_FEATURE_LEVEL wanted[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL got{};
    HRESULT hr = D3D11CreateDevice(chosen, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
                                   wanted, 2, D3D11_SDK_VERSION, dev, &got, ctx);
    chosen->Release();
    (void)minFL;
    return SUCCEEDED(hr);
}

// The full session lifecycle. Runs on the worker thread. Any failure just logs
// and returns -- the game is never affected.
static void XrThreadMain() {
    // --- Instance (require the D3D11 graphics extension) ---
    const char* exts[] = { XR_KHR_D3D11_ENABLE_EXTENSION_NAME };
    XrInstanceCreateInfo ici{ XR_TYPE_INSTANCE_CREATE_INFO };
    ici.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    lstrcpynA(ici.applicationInfo.applicationName, "XIII2003VR", XR_MAX_APPLICATION_NAME_SIZE);
    ici.enabledExtensionCount = 1;
    ici.enabledExtensionNames = exts;

    XrInstance instance = XR_NULL_HANDLE;
    XrResult r = xrCreateInstance(&ici, &instance);
    if (XR_FAILED(r)) { Logf("xrCreateInstance", r); return; }

    XrSystemGetInfo sgi{ XR_TYPE_SYSTEM_GET_INFO };
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    r = xrGetSystem(instance, &sgi, &systemId);
    if (XR_FAILED(r)) { Logf("xrGetSystem", r); xrDestroyInstance(instance); return; }

    // --- D3D11 graphics requirements (adapter LUID) via the extension proc ---
    PFN_xrGetD3D11GraphicsRequirementsKHR getReq = nullptr;
    xrGetInstanceProcAddr(instance, "xrGetD3D11GraphicsRequirementsKHR",
                          (PFN_xrVoidFunction*)&getReq);
    if (!getReq) { Log("xrGetD3D11GraphicsRequirementsKHR unavailable"); xrDestroyInstance(instance); return; }
    XrGraphicsRequirementsD3D11KHR req{ XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
    r = getReq(instance, systemId, &req);
    if (XR_FAILED(r)) { Logf("GetD3D11GraphicsRequirements", r); xrDestroyInstance(instance); return; }

    ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr;
    if (!CreateMatchingD3D11(req.adapterLuid, &dev, &ctx, req.minFeatureLevel)) {
        Log("CreateMatchingD3D11 failed"); xrDestroyInstance(instance); return;
    }

    // --- Session ---
    XrGraphicsBindingD3D11KHR binding{ XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
    binding.device = dev;
    XrSessionCreateInfo sci{ XR_TYPE_SESSION_CREATE_INFO };
    sci.next = &binding;
    sci.systemId = systemId;
    XrSession session = XR_NULL_HANDLE;
    r = xrCreateSession(instance, &sci, &session);
    if (XR_FAILED(r)) { Logf("xrCreateSession", r); ctx->Release(); dev->Release(); xrDestroyInstance(instance); return; }

    // --- Reference space (LOCAL) for the quad layer ---
    XrReferenceSpaceCreateInfo rsci{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    rsci.poseInReferenceSpace.orientation.w = 1.0f;
    XrSpace space = XR_NULL_HANDLE;
    xrCreateReferenceSpace(session, &rsci, &space);

    const XrViewConfigurationType viewCfg = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

    // Swapchain is created lazily once we know the captured frame size.
    XrSwapchain swapchain = XR_NULL_HANDLE;
    XrSwapchainImageD3D11KHR* images = nullptr;
    uint32_t imageCount = 0;
    int scW = 0, scH = 0;
    uint8_t* frameBuf = nullptr; int frameCap = 0;
    bool sessionRunning = false;

    while (InterlockedCompareExchange(&g_run, 1, 1) == 1) {
        // --- Events ---
        XrEventDataBuffer ev{ XR_TYPE_EVENT_DATA_BUFFER };
        while (xrPollEvent(instance, &ev) == XR_SUCCESS) {
            if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                auto* ssc = reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);
                if (ssc->state == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo bi{ XR_TYPE_SESSION_BEGIN_INFO };
                    bi.primaryViewConfigurationType = viewCfg;
                    if (XR_SUCCEEDED(xrBeginSession(session, &bi))) sessionRunning = true;
                } else if (ssc->state == XR_SESSION_STATE_STOPPING) {
                    xrEndSession(session); sessionRunning = false;
                } else if (ssc->state == XR_SESSION_STATE_EXITING ||
                           ssc->state == XR_SESSION_STATE_LOSS_PENDING) {
                    InterlockedExchange(&g_run, 0);
                }
            }
            ev = XrEventDataBuffer{ XR_TYPE_EVENT_DATA_BUFFER };
        }

        if (!sessionRunning) { Sleep(10); continue; }

        // --- Lazily create the swapchain once a frame size is known ---
        if (swapchain == XR_NULL_HANDLE) {
            int w = 0, h = 0;
            // Probe the latest frame size (allocate a generous scratch buffer).
            if (frameCap == 0) { frameCap = 4096 * 4096 * 4; frameBuf = (uint8_t*)malloc(frameCap); }
            if (!frameBuf || !VrHostCopyLatestFrame(frameBuf, frameCap, &w, &h) || w <= 0 || h <= 0) {
                Sleep(16); continue;  // no frame yet
            }
            XrSwapchainCreateInfo scci{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
            scci.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            scci.format = DXGI_FORMAT_B8G8R8A8_UNORM;
            scci.width = (uint32_t)w; scci.height = (uint32_t)h;
            scci.sampleCount = 1; scci.faceCount = 1; scci.arraySize = 1; scci.mipCount = 1;
            if (XR_FAILED(xrCreateSwapchain(session, &scci, &swapchain))) { Log("xrCreateSwapchain failed"); break; }
            xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
            images = (XrSwapchainImageD3D11KHR*)calloc(imageCount, sizeof(XrSwapchainImageD3D11KHR));
            for (uint32_t i = 0; i < imageCount; ++i) images[i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
            xrEnumerateSwapchainImages(swapchain, imageCount,
                                       &imageCount, (XrSwapchainImageBaseHeader*)images);
            scW = w; scH = h;
        }

        // --- Frame ---
        XrFrameWaitInfo fwi{ XR_TYPE_FRAME_WAIT_INFO };
        XrFrameState fs{ XR_TYPE_FRAME_STATE };
        if (XR_FAILED(xrWaitFrame(session, &fwi, &fs))) { Sleep(4); continue; }
        XrFrameBeginInfo fbi{ XR_TYPE_FRAME_BEGIN_INFO };
        xrBeginFrame(session, &fbi);

        // Head pose from the primary view -> feed the camera hook.
        XrViewLocateInfo vli{ XR_TYPE_VIEW_LOCATE_INFO };
        vli.viewConfigurationType = viewCfg;
        vli.displayTime = fs.predictedDisplayTime;
        vli.space = space;
        XrViewState vs{ XR_TYPE_VIEW_STATE };
        XrView views[2] = { { XR_TYPE_VIEW }, { XR_TYPE_VIEW } };
        uint32_t viewCount = 0;
        if (XR_SUCCEEDED(xrLocateViews(session, &vli, &vs, 2, &viewCount, views)) && viewCount > 0 &&
            (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {
            const XrQuaternionf& q = views[0].pose.orientation;
            VrHostSetHeadPose(q.x, q.y, q.z, q.w);
        }

        XrCompositionLayerQuad quad{ XR_TYPE_COMPOSITION_LAYER_QUAD };
        bool haveLayer = false;
        if (fs.shouldRender && swapchain != XR_NULL_HANDLE) {
            uint32_t idx = 0;
            XrSwapchainImageAcquireInfo ai{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
            if (XR_SUCCEEDED(xrAcquireSwapchainImage(swapchain, &ai, &idx))) {
                XrSwapchainImageWaitInfo wi{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
                wi.timeout = XR_INFINITE_DURATION;
                if (XR_SUCCEEDED(xrWaitSwapchainImage(swapchain, &wi))) {
                    int w = 0, h = 0;
                    if (VrHostCopyLatestFrame(frameBuf, frameCap, &w, &h) && w == scW && h == scH) {
                        ctx->UpdateSubresource(images[idx].texture, 0, nullptr,
                                               frameBuf, (UINT)w * 4, 0);
                    }
                    XrSwapchainImageReleaseInfo ri{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
                    xrReleaseSwapchainImage(swapchain, &ri);

                    quad.layerFlags = 0;
                    quad.space = space;
                    quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                    quad.subImage.swapchain = swapchain;
                    quad.subImage.imageRect.offset = { 0, 0 };
                    quad.subImage.imageRect.extent = { scW, scH };
                    quad.subImage.imageArrayIndex = 0;
                    quad.pose.orientation.w = 1.0f;
                    quad.pose.position = { 0.0f, 0.0f, -2.0f };  // 2m in front
                    quad.size = { 2.4f, 1.8f };                  // metres (4:3)
                    haveLayer = true;
                }
            }
        }

        const XrCompositionLayerBaseHeader* layers[1] = {
            reinterpret_cast<XrCompositionLayerBaseHeader*>(&quad) };
        XrFrameEndInfo fei{ XR_TYPE_FRAME_END_INFO };
        fei.displayTime = fs.predictedDisplayTime;
        fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        fei.layerCount = haveLayer ? 1 : 0;
        fei.layers = haveLayer ? layers : nullptr;
        xrEndFrame(session, &fei);
    }

    // --- Teardown ---
    free(frameBuf);
    free(images);
    if (swapchain) xrDestroySwapchain(swapchain);
    if (space) xrDestroySpace(space);
    xrDestroySession(session);
    ctx->Release(); dev->Release();
    xrDestroyInstance(instance);
    Log("OpenXR host stopped");
}

// Wrapper that swallows a delay-load failure (openxr_loader.dll absent) so a
// machine without OpenXR is never harmed by enabling the flag.
static DWORD WINAPI XrThreadProc(LPVOID) {
    __try {
        XrThreadMain();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("OpenXR unavailable (loader/runtime missing) -- host disabled");
    }
    return 0;
}

void StartOpenXrHost() {
    if (!ConfigEnabled()) { Log("[VR] OpenXR=0 -- host not started"); return; }
    InterlockedExchange(&g_run, 1);
    g_thread = CreateThread(nullptr, 0, XrThreadProc, nullptr, 0, nullptr);
    Log("OpenXR host thread started");
}

void StopOpenXrHost() {
    InterlockedExchange(&g_run, 0);
    if (g_thread) {
        WaitForSingleObject(g_thread, 2000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
}
