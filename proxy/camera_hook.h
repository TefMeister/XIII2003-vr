// proxy/camera_hook.h
#pragma once

// Installs the camera-injection hook: an inline hook on
// APlayerController::eventPlayerCalcView (Engine.dll), the per-frame function
// that computes the render camera's rotation. We call the original, then modify
// the output FRotator's yaw/pitch -- rotating what the player SEES without
// touching gameplay state (aim/physics use a separate rotation).
//
// Must be called after Engine.dll is loaded (it is, by the time the proxy's
// DllMain runs). Returns true if the hook was installed.
//
// For Milestone 1 headset-free verification this runs a synthetic yaw sweep so
// the effect is visible from the desktop/captured frames alone. A real HMD pose
// source replaces the sweep later.
bool InstallCameraHook();
