// proxy/dllmain.cpp
#include <windows.h>

// Force the original render-device DLL to load so its static/global
// constructors run and self-register UD3DRenderDevice into the engine's
// native-class registrant list. Without this, the engine's BindPackage ->
// ProcessRegistrants path finds no registered D3DRenderDevice class and
// aborts with "Failed to find object 'Class D3DDrv.D3DRenderDevice'".
// (Export forwarding alone never triggers the original to load, because the
// engine resolves the class via the registrant list, not GetProcAddress.)
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        LoadLibraryA("D3DDrv_Original.dll");
    }
    return TRUE;
}
