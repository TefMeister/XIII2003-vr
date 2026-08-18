// proxy/frame_capture.h
#pragma once

// Installs the render-device frame-capture hook. Must be called AFTER
// D3DDrv_Original.dll is loaded (its d3d8.dll import table must be resolved),
// e.g. from the proxy's DllMain right after LoadLibraryA("D3DDrv_Original.dll").
//
// Mechanism (all self-contained, no external detour lib, no DirectX SDK headers):
//   IAT-hook d3d8.dll!Direct3DCreate8 in D3DDrv_Original  ->
//   vtable-hook IDirect3D8::CreateDevice (grab the device) ->
//   vtable-hook IDirect3DDevice8::Present -> copy backbuffer to a .bmp, then
//   always chain to the real Present so the desktop window keeps updating.
//
// Captured frames are written to %TEMP%\xiii_capture\xiii_frame_NNN.bmp.
void InstallFrameCapture();
