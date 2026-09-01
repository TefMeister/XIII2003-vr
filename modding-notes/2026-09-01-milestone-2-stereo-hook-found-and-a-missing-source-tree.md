# 2026-09-01 — Milestone 2's stereo hook found statically; and the source tree is missing

**Date:** 2026-09-01, dev machine. **Static PE analysis only — the game was never launched**
(a parallel session owns the machine's one "game may run" slot), and no code was changed, because
**there is no code here to change** — see §2.

Two results, one good and one that needs the user.

---

## 1. The Milestone 2 stereo hook: `FD3DRenderInterface::SetTransform`

M2 is "native-ABI true stereo depth". The current mod is Milestone 1 — the flat framebuffer copied
into a VR overlay with head-look — so true stereo needs the engine's own **view** and **projection**
matrices, per eye. In UE2 those do not go through a UE1-style `SetSceneNode`; they go through the
render interface's `SetTransform`, and XIII's implementation of it is a **single small function
that forwards straight to Direct3D 8**.

**`FD3DRenderInterface::SetTransform` — `D3DDrv.dll + 0x129E0`** (VA `0x116129E0` at the DLL's
preferred base `0x11600000`; the module has a `.reloc`, so always work in RVA).
`__thiscall`, `ret 8`, i.e. `void __thiscall SetTransform(ETransformType type, const FMatrix *m)`.

`[inferred-static 2026-09-01, n=1 — decoded from D3DDrv_Original.dll, not yet observed live]`

Located from the UE2 `guard()` string `"FD3DRenderInterface::SetTransform"` at `0x1164EF6C`,
whose only reference is the guard block of this function.

### What it does

```
if (matrix unchanged from the cached copy) return;      // 0x11609420 = 64-byte compare
dev = *(void **)(*(char **)(this + 4) + 0x66C);         // the D3D8 device wrapper
dev->vtbl[0x94](dev, <d3d transform state>, m);         // IDirect3DDevice8::SetTransform
memcpy(cache, m, 64);                                   // FMatrix is 4x4 floats = 64 bytes
```

| `type` | UE2 meaning | cached at | forwarded as |
|---|---|---|---|
| `0` | `TT_LocalToWorld` | `this + 0x50` | `0x100` = `D3DTS_WORLD` |
| `1` | `TT_WorldToCamera` | `this + 0x90` | `2` = `D3DTS_VIEW` |
| `2` | `TT_CameraToScreen` | `this + 0xD0` | `3` = `D3DTS_PROJECTION` |

The three constants `256 / 2 / 3` are exactly D3D8's `D3DTS_WORLD` / `D3DTS_VIEW` /
`D3DTS_PROJECTION`, and vtable byte offset `0x94` is index 37, which is
`IDirect3DDevice8::SetTransform`. Those two independent facts agreeing is what makes the mapping
solid rather than a guess.

### Why this is the right hook for M2

* **Both halves of stereo are here.** Per-eye view = translate `TT_WorldToCamera` along the
  camera's right axis by ±IPD/2. Per-eye projection = an asymmetric frustum written into
  `TT_CameraToScreen`. No engine-internals archaeology and no shader work.
* **One function covers everything the engine draws**, because every UE2 pass sets its transforms
  through the render interface.
* **Two layers to choose from.** The mod already replaces `D3DDrv.dll` wholesale, so it can act
  either at this UE2-level function or one step lower at `IDirect3DDevice8::SetTransform`. The UE2
  level is better: `type` says *what* the matrix is, whereas at the D3D8 level that has to be
  re-derived from the state constant, and the engine's own cache is invisible.

### ⚠️ The trap to design around: the unchanged-matrix early-out

The function **returns without doing anything if the incoming matrix equals its cached copy.**
For stereo that is a live hazard, not a detail: render eye 1 with a modified view matrix, and when
the engine sets the *same* matrix again for eye 2 it compares equal to whatever was cached last
and the call is skipped — so eye 2 silently inherits eye 1's view and the stereo collapses with
no error anywhere.

Any stereo implementation must therefore either keep the cache consistent with what the engine
*thinks* it set (write the engine's matrix into the cache, send the modified one to D3D), or
invalidate the cache between eyes. **Decide this before writing the hook**, not after debugging a
flat-looking stereo image.

### Supporting addresses (Engine.dll, preferred base `0x10000000`)

| Symbol | VA |
|---|---|
| `FCameraSceneNode::FCameraSceneNode(UViewport*, AActor*, FVector, FRotator, float Fov)` | `0x103CC190` |
| `FLevelSceneNode::Render(FRenderInterface*)` | `0x103CBFF0` |
| `FLevelSceneNode::GetViewFrustum()` | `0x103CB560` |
| `FLevelSceneNode::GetWorldFrustumPoints(FVector*)` | `0x103CBCD0` |
| `FSceneNode::Deproject(const FPlane&)` | `0x103C9A40` |
| vtables: `FRenderInterface` `0x1046F298` · `FSceneNode` `0x1046F42C` · `FCameraSceneNode` `0x1046F474` | |

The `FCameraSceneNode` constructor taking location, rotation **and** FOV in one call is the
alternative stereo route (build the camera per eye rather than patching its matrices). Recorded as
a second option; `SetTransform` is cheaper and does not need to reproduce engine behaviour.

`UD3DRenderDevice::Lock` returns the `FRenderInterface*` — export
`?Lock@UD3DRenderDevice@@UAEPAVFRenderInterface@@PAVUViewport@@PAEPAH@Z` at `0x1160DBF0` — which is
where a proxy gets a handle on the object whose `SetTransform` is being hooked.

---

## 2. ⚠️ The XIII proxy source is not in git, anywhere

While looking for somewhere to put the above, the source turned out not to exist in this project.

**Verified today:**

* The consolidated `XIII2003-vr` repo contains **zero** `.c/.cpp/.h/.hpp` files, across **all 59
  commits of its entire history** (`git rev-list --objects --all`, not just the current tree).
* The private `staging` monorepo's `XIII2003-vr/` folder holds `D3DDrv.dll`, `BUILD.md`,
  `README.md`, `CONTRIBUTING.md` — **no source**.
* `staging/XIII2003-vr/BUILD.md` cites the source as *"branch `milestone-1-vr-headlook`, commit
  `8f38348`"* of the old `XIII2003-vr-dev-archive` repo. **That repo no longer exists** (GitHub
  returns 404), the consolidated repo has only `main`, and `git cat-file 8f38348` reports
  "not a valid object name". The commit is not reachable anywhere we still hold.
* On this dev PC, `D:\XIII2003VR-*` was deleted on 2026-08-31 as part of the old-clone cleanup,
  on the recorded basis that everything was "verified preserved in the new repos". For the git
  branches that is true; **for this source tree it is not, because the source was never committed.**

**The one copy we know of** is `C:\Users\TD3KX\XIII-VR-mod-src` on the **home PC**, which
`MACHINES.md` explicitly describes as *"plain folder, NOT a git clone"* — so it is unversioned and
unbacked-up, on a single disk.

This is the project's state: **Milestone 1 is complete and verified in a real Quest 3, and its
source survives in exactly one un-backed-up folder.** Every other active project keeps its source
in `staging/`; XIII is the only one that does not.

### What the user needs to do (home PC, five minutes, no headset needed)

Copy `C:\Users\TD3KX\XIII-VR-mod-src` into the `staging` clone as
`staging/XIII2003-vr/src/`, commit, push. `staging` is private, so this is a free action under the
push-freely rule. Once it lands, M2 can be implemented from the dev machine against §1 above.

**Not attempted from here**, deliberately: nothing on this machine can recover a source tree that
only exists on the other one, and guessing at a reconstruction would produce a second, divergent
"source" — worse than none.

---

## Status

* §1 is a **static finding, deployed nowhere and run nowhere.** It changes no code and cannot,
  until §2 is resolved.
* Dossier updated with §1 as a new section 7a.
* The game was not launched; no game file was modified.

🤖 Static PE analysis of `D3DDrv_Original.dll` and `Engine.dll` only.
