// proxy/focus_hook.h
//
// Keep-rendering-unfocused hook (0.2.6). XIII.exe's main loop polls
// GetForegroundWindow() every iteration and, when the foreground belongs to
// another process, stops ticking the engine entirely (8 ms sleep-poll loop) --
// freezing presentation and therefore the VR overlay whenever anything steals
// focus. This hook patches user32!GetForegroundWindow in XIII.EXE's IAT only,
// reporting the game's device window while a foreign window is foreground.
//
// Installed only when a VR host is enabled AND [VR] KeepRenderingUnfocused=1
// (the default); with VR off the stock behavior is untouched.

#pragma once
#include <windows.h>

// Read config and patch XIII.exe's IAT. Call from DllMain (process attach).
void InstallFocusHook();

// Record the game's device window (from the CreateDevice/Reset hooks). Until
// a window is known the hook reports the truth, so nothing changes before the
// render device exists.
void FocusHookSetGameWindow(HWND wnd);
