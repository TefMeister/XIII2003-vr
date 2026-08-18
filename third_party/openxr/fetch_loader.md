# Fetching the OpenXR loader (not committed)

The OpenXR headers in `include/openxr/` are vendored (OpenXR 1.0.10). The loader
binaries are gitignored (redistributable, and to respect repo size limits).

To build, fetch the **x86 (Win32)** loader from the `OpenXR.Loader` NuGet package
and place:

- `lib/openxr_loader.lib`  <- from `native/Win32/release/lib/openxr_loader.lib`
- `bin/openxr_loader.dll`  <- from `native/Win32/release/bin/openxr_loader.dll`

```
curl -sSL -o oxr.nupkg \
  https://api.nuget.org/v3-flatcontainer/openxr.loader/1.0.10.2/openxr.loader.1.0.10.2.nupkg
unzip oxr.nupkg -d oxr
cp oxr/native/Win32/release/lib/openxr_loader.lib third_party/openxr/lib/
cp oxr/native/Win32/release/bin/openxr_loader.dll third_party/openxr/bin/
```

At runtime the mod ships `openxr_loader.dll` next to `D3DDrv.dll` in the game's
`system\` folder (it finds the installed OpenXR runtime). The proxy delay-loads
it, so it is only required when OpenXR is enabled ([VR] OpenXR=1).
