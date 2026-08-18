// proxy/shutdown_hook.h
#pragma once

// Clean-shutdown path (0.2.3). The engine gives the proxy no usable quit
// notification: UD3DRenderDevice::Exit is dispatched virtually through the
// original DLL's vtable (the .def forwarder never executes), and by the time
// DLL_PROCESS_DETACH runs inside ExitProcess the OS has already terminated the
// VR host thread -- possibly mid-call inside the D3D11 driver, which is what
// produced the unkillable XIII.exe wedged in nvwgf2um.dll.
//
// InstallShutdownHook IAT-hooks kernel32!ExitProcess in every module loaded at
// install time; the hook runs VrModShutdown (stop SteamVR/OpenXR hosts, release
// the VR-host D3D11 device) while all threads are still alive, then forwards to
// the real ExitProcess.
void InstallShutdownHook();

// Idempotent full teardown of the VR hosts; safe to call from any thread that
// does not hold the loader lock.
void VrModShutdown();
