// proxy/dllmain.cpp
#include <windows.h>
#include "frame_capture.h"
#include "camera_hook.h"
#include "vr_host.h"
#include "openxr_host.h"
#include "steamvr_host.h"
#include "shutdown_hook.h"

// Force the original render-device DLL to load so its static/global
// constructors run and self-register UD3DRenderDevice into the engine's
// native-class registrant list. Without this, the engine's BindPackage ->
// ProcessRegistrants path finds no registered D3DRenderDevice class and
// aborts with "Failed to find object 'Class D3DDrv.D3DRenderDevice'".
// (Export forwarding alone never triggers the original to load, because the
// engine resolves the class via the registrant list, not GetProcAddress.)
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        LoadLibraryA("D3DDrv_Original.dll");
        // NB: the VR host's D3D11 device is created lazily on the first captured
        // frame (on the render thread) -- NOT here, because D3D11CreateDevice
        // under the DllMain loader lock is unsafe.
        // Install the render-device present hook now that the original's
        // d3d8.dll import table is resolved. Capture happens later, when the
        // engine actually creates the device and starts presenting.
        InstallFrameCapture();
        // Camera-injection hook (synthetic yaw sweep for headset-free
        // verification). Engine.dll is already loaded by the time the render
        // device proxy's DllMain runs.
        InstallCameraHook();
        // OpenXR VR host (only if [VR] OpenXR=1; the worker thread runs after
        // DllMain returns and the loader lock releases). Delay-loaded loader,
        // so this is inert on machines without OpenXR.
        StartOpenXrHost();
        // SteamVR / OpenVR VR host (only if [VR] SteamVR=1). Delay-loaded
        // openvr_api.dll, so this is inert on machines without SteamVR. Enable
        // EXACTLY ONE of OpenXR / SteamVR at a time -- never both.
        StartSteamVrHost();
        // Clean-shutdown path (0.2.3): IAT-hook ExitProcess so the VR host
        // threads are stopped and their D3D11/OpenVR resources released BEFORE
        // the OS terminates threads on quit. Terminating the host thread
        // mid-driver-call is what wedged XIII.exe unkillably in nvwgf2um.dll.
        InstallShutdownHook();
    } else if (reason == DLL_PROCESS_DETACH) {
        // Last-resort only -- the real teardown happens in the ExitProcess
        // hook. Here the loader lock is held: never join threads or touch
        // D3D/OpenVR. During process termination (lpReserved != NULL) the host
        // threads are already dead anyway; on a FreeLibrary they are about to
        // run unmapped code and a signal is the best we can do.
        (void)lpReserved;
        SignalStopSteamVrHost();
        SignalStopOpenXrHost();
    }
    return TRUE;
}
