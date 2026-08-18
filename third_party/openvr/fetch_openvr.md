# Fetching the OpenVR (SteamVR) API binaries (not committed)

The OpenVR header in `headers/openvr.h` is vendored (from OpenVR SDK v2.5.1).
The `openvr_api` binaries are gitignored (redistributable, and to respect repo
size limits).

To build, fetch the **x86 (win32)** binaries from Valve's OpenVR repo and place:

- `lib/openvr_api.lib`  <- from `lib/win32/openvr_api.lib`
- `bin/openvr_api.dll`  <- from `bin/win32/openvr_api.dll`

```
TAG=v2.5.1
curl -sSL -o third_party/openvr/lib/openvr_api.lib \
  https://github.com/ValveSoftware/openvr/raw/$TAG/lib/win32/openvr_api.lib
curl -sSL -o third_party/openvr/bin/openvr_api.dll \
  https://github.com/ValveSoftware/openvr/raw/$TAG/bin/win32/openvr_api.dll
```

At runtime the mod ships `openvr_api.dll` next to `D3DDrv.dll` in the game's
`system\` folder. The proxy delay-loads it, so it is only required when the
SteamVR host is enabled ([VR] SteamVR=1); machines without it are unaffected.

## Which runtime does this use?

This is the **SteamVR / OpenVR** path. It talks to the SteamVR runtime (the one
installed and launched through Steam). It does NOT use OpenXR. See the repo
README for exactly when to use this versus the OpenXR path.
