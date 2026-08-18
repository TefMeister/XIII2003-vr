// proxy/shutdown_hook.cpp
//
// Clean-shutdown path for the VR mod (0.2.3). See shutdown_hook.h for why the
// hook point is kernel32!ExitProcess: it is the only spot that runs on a normal
// quit while every thread is still alive, so the VR host threads can be joined
// and their D3D11 / OpenVR / OpenXR resources released BEFORE the OS starts
// terminating threads. Terminating the host thread mid-driver-call is what
// wedged XIII.exe unkillably inside nvwgf2um.dll (observed 2026-08-18).
//
// Same IAT-patching technique as frame_capture.cpp's Direct3DCreate8 hook,
// applied to kernel32.dll!ExitProcess across ALL loaded modules, because the
// quit-time caller may be the exe's static CRT or msvcrt.dll.

#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include "shutdown_hook.h"
#include "steamvr_host.h"
#include "openxr_host.h"
#include "vr_host.h"

static void Log(const char* m) {
    OutputDebugStringA("[xiii-shutdown] "); OutputDebugStringA(m); OutputDebugStringA("\n");
}

typedef VOID (WINAPI *ExitProcess_t)(UINT);
static ExitProcess_t s_realExitProcess = nullptr;

void VrModShutdown() {
    static LONG once = 0;
    if (InterlockedCompareExchange(&once, 1, 0) != 0) return;
    Log("quit detected -- stopping VR hosts");
    StopSteamVrHost();   // signals the loop, joins the thread (bounded wait)
    StopOpenXrHost();
    VrHostShutdown();    // releases the lazily created debug D3D11 device
    Log("VR hosts stopped -- handing off to real ExitProcess");
}

static VOID WINAPI Hook_ExitProcess(UINT uExitCode) {
    VrModShutdown();
    s_realExitProcess(uExitCode);
}

// Patch kernel32!ExitProcess in one module's IAT. Returns true if a slot was
// patched. Bound imports are handled implicitly: matching is by import NAME
// (OriginalFirstThunk), the write goes to the live IAT (FirstThunk).
static bool HookModuleIat(HMODULE hMod) {
    BYTE* base = (BYTE*)hMod;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    IMAGE_DATA_DIRECTORY& impDir =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!impDir.VirtualAddress) return false;

    bool patched = false;
    IMAGE_IMPORT_DESCRIPTOR* imp =
        (IMAGE_IMPORT_DESCRIPTOR*)(base + impDir.VirtualAddress);
    for (; imp->Name; ++imp) {
        const char* dllName = (const char*)(base + imp->Name);
        if (lstrcmpiA(dllName, "kernel32.dll") != 0) continue;

        IMAGE_THUNK_DATA* orig =
            (IMAGE_THUNK_DATA*)(base + (imp->OriginalFirstThunk
                                            ? imp->OriginalFirstThunk
                                            : imp->FirstThunk));
        IMAGE_THUNK_DATA* iat = (IMAGE_THUNK_DATA*)(base + imp->FirstThunk);
        for (; orig->u1.AddressOfData; ++orig, ++iat) {
            if (orig->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue; // by-ordinal
            IMAGE_IMPORT_BY_NAME* byName =
                (IMAGE_IMPORT_BY_NAME*)(base + orig->u1.AddressOfData);
            if (lstrcmpA((const char*)byName->Name, "ExitProcess") != 0)
                continue;
            DWORD oldProt = 0;
            if (VirtualProtect(&iat->u1.Function, sizeof(void*),
                               PAGE_EXECUTE_READWRITE, &oldProt)) {
                iat->u1.Function = (DWORD_PTR)&Hook_ExitProcess;
                DWORD tmp = 0;
                VirtualProtect(&iat->u1.Function, sizeof(void*), oldProt, &tmp);
                patched = true;
            }
        }
    }
    return patched;
}

void InstallShutdownHook() {
    s_realExitProcess = (ExitProcess_t)GetProcAddress(
        GetModuleHandleA("kernel32.dll"), "ExitProcess");
    if (!s_realExitProcess) { Log("GetProcAddress(ExitProcess) failed"); return; }

    int hooked = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 me; me.dwSize = sizeof(me);
        if (Module32First(snap, &me)) {
            do {
                if (HookModuleIat(me.hModule)) ++hooked;
            } while (Module32Next(snap, &me));
        }
        CloseHandle(snap);
    }
    char b[96];
    _snprintf(b, sizeof(b), "v0.2.3: ExitProcess IAT-hooked in %d module(s)", hooked);
    b[95] = 0;
    Log(b);
    if (hooked == 0) Log("WARNING: no module imports ExitProcess by name -- quit will be unclean");
}
