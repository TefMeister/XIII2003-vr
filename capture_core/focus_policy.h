// capture_core/focus_policy.h
//
// Decision logic for the keep-rendering-unfocused hook (0.2.6). XIII.exe's
// main loop polls GetForegroundWindow() every iteration and, when the
// foreground window is not the game's, stops ticking the engine entirely
// (8 ms sleep-poll loop) -- which freezes the VR overlay the moment anything
// else takes focus. The hook lies to THAT poll only: when the real foreground
// belongs to another process, it reports the game's own device window
// instead. Pure logic, no Windows dependencies; HWNDs travel as void*.

#pragma once

namespace xiii {

// What the hooked GetForegroundWindow should return.
//   fgBelongsToUs: the real foreground window is owned by this process
//   fg:            the real foreground window (may be null, e.g. mid session
//                  switch)
//   gameWnd:       the game's device window, or null before CreateDevice
// Truth is reported whenever it already keeps the game active (fg is ours) or
// no game window is known yet; otherwise the game window is reported.
void* ForegroundToReport(bool fgBelongsToUs, void* fg, void* gameWnd);

}  // namespace xiii
