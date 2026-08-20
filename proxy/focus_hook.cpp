// proxy/focus_hook.cpp
//
// See focus_hook.h for the mechanism. Root cause (found live in the debugger,
// 2026-08-20): XIII.exe's CMainLoop-style main loop checks, every iteration,
//     GetForegroundWindow -> GetWindowThreadProcessId == main thread
//     && !IsIconic(GetForegroundWindow())
// and when the check fails it skips Engine->Tick and idles in an 8 ms
// appSleep poll loop (caught mid-Sleep(8) with the whole process at zero
// CPU). No ini setting controls it. Hooking XIII.EXE's IAT only keeps
// WinDrv/Engine honest about focus, so input capture and WM_KILLFOCUS
// handling still behave; the engine's mute-on-deactivate is skipped together
// with the pause, so audio keeps playing while unfocused (intended for VR).
// A minimized game window still pauses: the main-loop check IsIconic()s the
// window we report, and we report the game's own window.
//
// Same IAT-patching technique as shutdown_hook.cpp, scoped to one module.

#include <windows.h>
#include "focus_hook.h"
#include "focus_policy.h"

static void Log(const char* m) {
    OutputDebugStringA("[xiii-focus] "); OutputDebugStringA(m); OutputDebugStringA("\n");
}

typedef HWND (WINAPI *GetForegroundWindow_t)(void);
static GetForegroundWindow_t s_real    = nullptr;
static HWND volatile         s_gameWnd = nullptr;

void FocusHookSetGameWindow(HWND wnd) {
    s_gameWnd = wnd;
}

static HWND WINAPI Hook_GetForegroundWindow() {
    HWND fg = s_real ? s_real() : nullptr;
    bool ours = false;
    if (fg) {
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        ours = (pid == GetCurrentProcessId());
    }
    return (HWND)xiii::ForegroundToReport(ours, (void*)fg, (void*)s_gameWnd);
}

static UINT ReadVrInt(const char* key, UINT def) {
    char ini[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, ini, MAX_PATH)) return def;
    char* slash = strrchr(ini, '\\');
    if (!slash) return def;
    lstrcpyA(slash + 1, "XIII.ini");
    return (UINT)GetPrivateProfileIntA("VR", key, (INT)def, ini);
}

void InstallFocusHook() {
    const bool vrConsumer =
        ReadVrInt("SteamVR", 0) != 0 || ReadVrInt("OpenXR", 0) != 0;
    if (!vrConsumer) return;  // stock behavior without a VR host
    if (ReadVrInt("KeepRenderingUnfocused", 1) == 0) {
        Log("disabled via [VR] KeepRenderingUnfocused=0");
        return;
    }

    s_real = (GetForegroundWindow_t)GetProcAddress(
        GetModuleHandleA("user32.dll"), "GetForegroundWindow");
    if (!s_real) { Log("GetProcAddress(GetForegroundWindow) failed"); return; }

    // Patch the game EXE's IAT only.
    BYTE* base = (BYTE*)GetModuleHandleA(nullptr);
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
        if (lstrcmpiA(dllName, "user32.dll") != 0) continue;

        IMAGE_THUNK_DATA* orig =
            (IMAGE_THUNK_DATA*)(base + (imp->OriginalFirstThunk
                                            ? imp->OriginalFirstThunk
                                            : imp->FirstThunk));
        IMAGE_THUNK_DATA* iat = (IMAGE_THUNK_DATA*)(base + imp->FirstThunk);
        for (; orig->u1.AddressOfData; ++orig, ++iat) {
            if (orig->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue; // by-ordinal
            IMAGE_IMPORT_BY_NAME* byName =
                (IMAGE_IMPORT_BY_NAME*)(base + orig->u1.AddressOfData);
            if (lstrcmpA((const char*)byName->Name, "GetForegroundWindow") != 0)
                continue;
            DWORD oldProt = 0;
            if (VirtualProtect(&iat->u1.Function, sizeof(void*),
                               PAGE_EXECUTE_READWRITE, &oldProt)) {
                iat->u1.Function = (DWORD_PTR)&Hook_GetForegroundWindow;
                DWORD tmp = 0;
                VirtualProtect(&iat->u1.Function, sizeof(void*), oldProt, &tmp);
                Log("GetForegroundWindow IAT-hooked in XIII.exe (keep rendering unfocused)");
            }
            return;
        }
    }
    Log("GetForegroundWindow import not found in XIII.exe");
}
