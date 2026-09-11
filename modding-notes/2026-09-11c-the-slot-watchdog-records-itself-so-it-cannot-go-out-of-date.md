# 2026-09-11c — the vtable watchdog registers itself, so it cannot go out of date

**`/pd`, dev PC (`DESKTOP-V8GTSIR`), auto-picked. The game was NOT launched. Nothing here has been run.**

Implements the ⭐ row queued this morning from the `/sr` drop (dossier §11b): detect a state-block
rewrite of our patched device-vtable slots.

---

## 1. The failure it exists to catch

`IDirect3DDevice8::EndStateBlock` writes the D3D runtime's **own** method pointers back over any
third-party pointer sitting in the **state-setting** slots, and leaves `Present`, `Reset` and the
`Draw*` calls untouched `[reported]`. So the signature is **selective and silent**: some of our hooks
keep working forever while others die permanently, in the same table, with no error anywhere.

**Which of our ten patched slots are at risk is not a guess — it follows from which are
state-setting** `[inferred-static 2026-09-11]`:

| slot | what | state-setting? |
| --- | --- | --- |
| 37 | `SetTransform` | **yes — at risk** |
| 40 | `SetViewport` | **yes — at risk** |
| 76 | `SetVertexShader` | **yes — at risk** |
| 79 | `SetVertexShaderConstant` | **yes — at risk** |
| 31 | `SetRenderTarget` | probably not |
| 70–73 | the four `Draw*` | no |
| 75 | `CreateVertexShader` | no |

**🚨 That list is alarming in a specific way.** `SetTransform` is where `s_projPersp` comes from — the
*entire* 2D-versus-3D classification — and `SetVertexShaderConstant` is how `c0..c3` are shadowed for
the programmable path. **Both are in the at-risk column while every `Draw*` hook is not.** So the
predicted failure is exactly: draws keep flowing, the picture looks mono-but-fine, and the
classification silently stops being fed. From a screenshot that is **indistinguishable** from "the
equality check failed" and from "stereo was never armed" — three causes, one appearance.

⚠️ **We are usually not the one recording.** The caller is another resident of the process — an
overlay, anything on `ID3DXSprite`/`ID3DXFont`, or the engine itself — and such residents most often
re-initialise **after a device reset**, which is why this reads as *"the reset killed my hook"*.

## 2. ⭐ The design decision worth keeping: it registers itself

The obvious implementation is a list of slot numbers to check. **That list would be wrong the first
time somebody adds a hook** — and it would be wrong silently, at exactly the moment nobody is thinking
about it, which is the same class of failure the watchdog exists to catch.

So the recording lives **inside `HookVtbl` itself**: every call that patches a slot also records
`(obj, idx, our pointer)`. **The watchdog therefore covers every hook automatically, including hooks
that do not exist yet**, and there is no second list to keep in step.

`FrameCaptureCheckPatchedSlots(dev)` runs once per `Present` — ten pointer compares — and on the first
change to a slot logs the slot index, our pointer, the new pointer, **and the module that now owns
it**, so the log names the culprit rather than just the symptom:

```
VTABLE SLOT REWRITTEN: slot 37 was ours=00A1B2C0 now=6F2D4410 (d3d8.dll) --
a state block or another hooker restored it; this hook is DEAD from here on
```

Three deliberate details:

- **Reported once per slot**, not once per frame — a rewritten table would otherwise fill the log and
  bury everything else.
- **Only slots recorded against the device are checked.** `HookVtbl` is also used on surfaces, and
  reading the vtable of an object that may since have been freed would be a worse bug than the one
  being hunted.
- **Detection only. It does not re-patch.** Re-patching is the follow-up; knowing *whether this
  happens on XIII at all* comes first, and a silent auto-repair would destroy the very evidence the
  row asks for.

**`[compile-verified 2026-09-11]`** — exit 0; the log string is confirmed present in the binary by
string search; exports re-checked against the stock device, **32 names each side, none missing**
`[verified-numerically 2026-09-11]`. Deployed as `system\D3DDrv-m2-stereo.dll` (previous kept as
`.bak-2026-09-11-pre-sbwatch`), re-stamped, 4 files ALL MATCH, dated artefact preserved in `staging`.

## 3. What one launch now answers, free, on any run

The watchdog costs nothing and needs no special mode — **any** launch of the stereo build reports it:

- **Nothing in the log** → no slot was ever rewritten on this run. The mechanism is real but **XIII
  does not trigger it**, and §11b stops being a live suspect for anything.
- **One or more `VTABLE SLOT REWRITTEN` lines** → it happens here. The slot number says which
  capability died, the module names who did it, and **re-patching after `EndStateBlock` becomes a
  real, justified piece of work** instead of defensive coding against a hypothetical.

⚠️ **A rewritten slot 37 or 79 would also retro-actively explain any past run where the stereo
classification looked broken for no reason** — worth remembering before blaming the maths next time.

---

## What is NOT established

- **Nothing here has been run.** No frame has been rendered with the watchdog, on either machine.
- **That XIII triggers this at all.** §11b is a documented mechanism with two independent public
  witnesses; whether anything in *this* process records a state block is exactly what the watchdog is
  for, and is currently unknown.
- **That the at-risk column above is exactly right.** Which methods a D3D8 state block captures is
  taken from the documented behaviour of state blocks, not measured on this runtime — the watchdog
  reports whatever actually changes, so it will correct the table if the table is wrong.
- **That detection implies a fix is warranted.** If it fires once at startup and never again, the
  right response may be to re-patch once, not to police the table forever.
