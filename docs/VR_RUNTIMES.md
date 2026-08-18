# Which VR runtime does this mod use?

The proxy `D3DDrv.dll` can drive the headset through **either** of two VR
runtimes. They are mutually exclusive: enable **exactly one** at a time. Both
are turned off by default, so with no configuration the game runs normally on a
flat monitor.

Both are selected in `system\XIII.ini`, in a `[VR]` section:

```ini
[VR]
; --- pick ONE of these two (never both) ---
SteamVR=1        ; use the SteamVR / OpenVR runtime (recommended for first tests)
OpenXR=0         ; use an OpenXR runtime instead

; --- head-look camera (independent of the runtime above) ---
CameraLiveHmd=1  ; rotate the in-game camera to match the real head orientation
```

## Option A — SteamVR (OpenVR). Recommended.

**Runtime:** the **SteamVR** runtime, installed and launched through Steam.

- Set `[VR] SteamVR=1` (and leave `OpenXR=0`).
- Ship `openvr_api.dll` next to `D3DDrv.dll` in the game's `system\` folder.
- Start SteamVR before (or just after) launching the game.
- On a **Quest 3**, reach SteamVR over **Virtual Desktop**: run the Virtual
  Desktop streamer on the PC, connect the headset, and choose to launch/stream
  **SteamVR** from inside Virtual Desktop. (Air Link / Quest Link into SteamVR
  also works.)

**What you should see:** a flat framebuffer of the game floats on a screen
locked in front of your head; turning your head rotates the in-game camera
(when `CameraLiveHmd=1`), so you look around the game world by looking around.

**Why this is the recommended first path:** the SteamVR overlay upload is a
plain CPU copy (`IVROverlay::SetOverlayRaw`) — no shared D3D texture, swapchain,
or per-eye projection — which is simpler and more reliable on this 32-bit
target than an OpenXR session. It is the lower-risk way to confirm the pipeline
end to end on the headset first.

## Option B — OpenXR

**Runtime:** an **OpenXR** runtime (not SteamVR's). On a Quest 3 over Virtual
Desktop this is **VDXR** (Virtual Desktop's OpenXR runtime); Oculus's OpenXR
runtime works too. Whichever you use must be set as the **active** OpenXR
runtime.

- Set `[VR] OpenXR=1` (and leave `SteamVR=0`).
- Ship `openxr_loader.dll` next to `D3DDrv.dll` in `system\` (see
  `third_party/openxr/fetch_loader.md`).
- Start the OpenXR runtime + headset, then launch the game.

**What you should see:** the same flat framebuffer, submitted as an OpenXR quad
layer 2 m in front, with the head pose read from the runtime.

## Safety / fallback

Both `openvr_api.dll` and `openxr_loader.dll` are **delay-loaded**: the proxy
only touches a runtime's DLL when its flag is on. If the flag is off, or the
DLL/runtime is missing, that host quietly disables itself and the game is
unaffected. This is why a machine without either runtime still runs the game
normally.

## Diagnostics

Both hosts log through `OutputDebugString`, visible in **DebugView**
(run it as administrator, enable *Capture Global Win32*):

- SteamVR host lines are prefixed `[xiii-steamvr]`
- OpenXR host lines are prefixed `[xiii-openxr]`
