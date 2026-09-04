# Your in-place device-vtable patches can be rewritten by anyone recording a D3D state block — check this before reading the first M2 stereo launch

Filed by: `/sr`, 2026-09-04 (cross-engine sweep; the mechanism is D3D8/D3D9 runtime behaviour, not
engine-specific). Library write-up:
[techniques → recording a state block rewrites the device's method table](https://github.com/TefMeister/flat-to-vr-cross-engine-research/blob/main/docs/techniques/README.md#recording-a-state-block-rewrites-the-devices-method-table--and-your-in-place-vtable-patch-with-it)

## Why this is addressed to this project specifically

`stereo_hook.cpp` patches the **device** vtable in place — its own header names `SetTransform` at
`[vtbl+0x94]`, and the M2 build additionally covers viewport, render-target, `SetVertexShaderConstant`
and the four `Draw*` slots. **`SetTransform` and `SetVertexShaderConstant` are state-setting methods**,
which is exactly the class the mechanism below rewrites. The build is `[compile-verified 2026-09-04]`
and **has never rendered a frame**, so if this bites, it will bite on a first launch whose outcome
table is already written — and it would present as a partial, confusing result rather than an error.

## The mechanism `[reported]`

`IDirect3DDevice::BeginStateBlock` puts the runtime into recording mode by swapping the device's
**state-setting** methods for recording variants; `EndStateBlock` writes the runtime's **own
originals** back, overwriting any third-party pointer in those slots. Non-state-setting methods
(`Present`, `Reset`, the `Draw*` calls, the creation calls) are untouched. So the signature is: some of
your hooks keep working forever, others die permanently, in the same table, silently.

Two independent public witnesses, one D3D9 and one D3D8, verified against their sources:

- gho, author of DxWnd, diagnosing it while chasing D3D9 device-`Reset` trouble (2014-06-02):
  *"D3DDevice9::BeginStateBlock recover all COM method pointers invalidating the hook patching. It's
  sufficient to hook this method to restore back the DxWnd routines and the trick is done!"*
  <https://sourceforge.net/p/dxwnd/discussion/general/thread/9b1c8171/>
- Paul Roussin, on Microsoft's retired DirectX newsgroup, answering a failed D3D8 vtable hook:
  *"BeginStateblock will reset the device table so you have to make the code return control back to you
  so you can reset your modified addresses."* (Survives only on a third-party Usenet mirror; treat as an
  archived public post, not a vendor source.)

**You are usually not the one recording.** The caller is another resident of the process — an overlay,
anything on `ID3DXSprite`/`ID3DXFont`, or the engine itself — and such residents most often
re-initialise **after a device reset**, which is why this reads as "the reset killed my hook".

## What it would look like on the M2 run

Your `vs-mismatch` counter and the per-second class counts would go quiet or partial while `Present`
keeps running and the picture looks mono-but-fine. That is indistinguishable, from a screenshot, from
"the equality check failed" or "stereo was never armed" — three different causes with the same
appearance. **Distinguish them in the log, not by eye.**

## Cheapest thing to add before the launch, in order

1. **Detect.** Once per `Present`, compare each patched slot against your own function pointers; log the
   first mismatch with the new pointer value and the module it belongs to. A handful of lines, and it
   converts an invisible failure into one log line.
2. **Re-arm.** Re-apply a slot **only when it has reverted to the runtime's original pointer** — the one
   unambiguous case. A *foreign* pointer may be a later hook chaining to you; log it once and leave it.
3. **Or sidestep it for the state-setting slots**: a code hook on the function body is immune, because
   nothing about state-block recording touches the function's first instructions.

`enslaved-vr` has built exactly this (re-arm plus `BeginStateBlock`/`EndStateBlock` logging), also not
yet run — worth reading rather than re-deriving:
<https://github.com/TefMeister/enslaved-vr> `engine-research/ENGINE-DOSSIER.md` §9a.

## Also worth one minute: your proxy does NOT have the reload bug, and it is worth knowing why

The sweep audited every proxy in the estate for a separate trap — a proxy that loads the real DLL by
system path and never `FreeLibrary`s it on `DLL_PROCESS_DETACH` gets **bypassed** if the game unloads
and reloads that DLL, because Windows resolves a bare module name to whatever is already resident.
**This project is immune, structurally**, because it loads a *renamed* original
(`D3DDrv_Original.dll`), so no resident module ever shares the proxy's base name
`[inferred-static 2026-09-04]`. Keep the rename if the proxy is ever restructured; it is doing more
work than it looks like.
