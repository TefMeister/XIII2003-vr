# 2026-09-11f — flat stereo on the dev PC: the hooks hold, the spin is gone, and the shadow is stuck to the screen

Dev PC `DESKTOP-V8GTSIR`, `/lm` (auto-picked), three launches, one reader helper. I drove everything
(launch, menus, movement, closing). Evidence:
`dev-archive/recon/2026-09-11f-flat-stereo-run-the-hooks-hold-and-the-shadow-is-stuck-to-the-screen/`.
Dossier: §11h.

---

## The headline

**The first run of the instrumented stereo build answered four of the board's five never-run rows in
one evening, and the fifth turned out to need a headset.**

| Row | Answer |
| --- | --- |
| State-block watchdog (§11b) | **Nothing is rewritten.** No `VTABLE SLOT REWRITTEN` line in two complete sessions and the last ~5 min of a third `[verified-live 2026-09-11, n=2 full sessions]`. §11b is no longer a live suspect on this machine. |
| Self-spin | **Fixed.** `CameraLiveHmd=1`, no runtime: hook installed as `LiveHmd via pose_math`, zero `view:` lines, and two hands-off screenshots 6 s apart are identical (mean pixel difference 0.41/255) `[verified-live 2026-09-11, n=1]`. The old bug turned ~30°/s. |
| HUD / XYZRHW | **The visible HUD is the ortho route, not pre-transformed.** In first-person play (beach) `mono-ortho=3`, `rhw-mono=0`, `rhw-stereo=0`. The bank adds `rhw-stereo=12` per frame of *unknown* content — nothing on screen looks doubled. |
| Character shadows | **The texture-matrix suspect is confirmed, and the error is measured.** `texmat` is non-zero in every level (stages 0 and 1; 18/frame in the bank, 134 on the beach). The player's arm shadow on the hut floor sits almost still between the eyes while the floor under it shifts ~25 px (at a magnified eye distance): **the shadow is stuck to the screen instead of lying on the floor.** |
| Head roll / pitch | **Cannot be tested flat.** Its log line needs a real head pose, and this PC has no runtime DLLs (reader). Moved to the headset. |

And the render half of M2 works flat on this PC: **side-by-side world with correct parallax.**

## 1. Two eyes, measured

Bank lobby (`Banque01`, reached by Continue), eye distance 3.40 units:

- near pillars shift **−9 px** between the eyes, the floor **−4 px**, the far balcony **0** — correct
  depth order, correct sign `[measured 2026-09-11]`;
- numpad 6 ×3 (eye distance 4.15, **+22 %**) → near pillars −11 px (**+22 %**); numpad 5 (swap) flips
  every sign; back to 3.40 returns exactly the first numbers `[measured 2026-09-11]`;
- the log agrees: `vs-stereo=21` with **`vs-mismatch=0`**, and the first three shader draws report
  `c0..c3 == W*V*P (max |diff| 0) -> per-eye constants path engaged` — **the outline and toon-shader
  path gets depth too**. The character ink outlines are visibly in both eyes.

Each eye's picture is squeezed to half width (the engine's projection is used unchanged,
`fov=engine`); that is expected for a flat side-by-side check and is not what the headset path uses.

(The home PC already ran this build family in the headset on 2026-09-10; this is the first run with
the 11b–11g instruments in it, and the first on the dev PC.)

## 2. The shadow, measured

**The beach level has character shadows; the bank lobby shows none.** A cutscene character in the
lifeguard hut casts a clear projected shadow; at the normal eye distance the cutscene camera is far
away and every disparity is under a pixel, so nothing can be told apart there.

So in first-person play I raised the eye distance to **13.40** (numpad 6 ×40) to magnify depth, and
measured the player's own arm shadow on the hut floor:

| Patch | Right-eye shift |
| --- | --- |
| floor around the shadow (5 patches) | **−23 … −27 px** (match quality 0.99–1.00) |
| cabinet | −20 px |
| far floor | −6.7 px |
| **the shadow (dark-blob centroid)** | **−2.5 px** |

**The shadow does not follow the floor it lies on — it stays almost put on the screen.** At the normal
eye distance that is ~5–6 px of wrong depth: the shadow floats at roughly screen depth instead of
marking the floor `[measured 2026-09-11, n=1 frame]`. That is exactly what dossier §11f predicted for a
projected-texture shadow with camera-space texture coordinates and a texture matrix built for the
centre view `[hypothesis: the texgen mode itself has not been read yet]`.

**The fix is small:** per eye, put a sideways translation in front of the texture matrix — the same
half-eye-distance shift the view already gets. Queued to the reader to build (with a numeric test that
must fail when the sign is wrong).

## 3. The HUD, measured

- **Menus**, stereo on: only `mono-ortho` draws, `rhw` 0/0; menus look normal.
- **First-person play, beach:** `mono-ortho=3`, `rhw-mono=0`, `rhw-stereo=0`. The health counter and
  the crosshair are drawn **once, at their full-screen position**: health lands in the left eye's half
  only, and the crosshair dot sits on the seam between the halves. In a headset that means one eye
  sees the health counter and neither sees a centred crosshair.
- **Bank:** the same 3 ortho draws **plus 12 pre-transformed draws per frame that ARE stereo'd**. What
  they are is unknown — nothing visible is doubled. Do not read them as the HUD.

⇒ The board's "rhw-stereo > 0 ⇒ that is the mechanism" reading does **not** apply cleanly: the HUD
people see is the ortho bucket. The fix is "each eye gets the whole HUD inside its own half" — draw
the ortho bucket once per eye with that eye's viewport (plus an optional HUD-depth shift), and remap
pre-transformed coordinates the same way. Queued to the reader.

## 4. The watchdog, measured

- Run 1 had no listener for its first 9 minutes (the watchdog then wrote only to the debug channel);
  the last ~5 minutes were captured and clean.
- Run 2: listener up **before** launch, `Present hooked` captured, stereo on from the first frame,
  the whole session through a level load and a clean exit: **no rewrite**.
- Run 3 (the reader's new build, which writes these lines to the normal log): **no rewrite**.

## 5. Launch routes (automation)

- **Steam:** `steam://rungameid/1170760`, then **Enter ×3** (profile "XIII" is pre-selected →
  Continue → PLAY) lands in `Banque01`, then Space dismisses the objectives card. ⚠️ Up-arrow on the
  profile screen wraps to "Create new profile"; Escape backs out. Mouse clicks do nothing (the menu
  cursor does not follow the Windows cursor).
- **⭐ Direct:** `XIII.exe Plage01` from `system\` loads the beach level straight away (no Steam
  round-trip, `Browse: Plage01` in `XIII.log`), plays its short cutscene and hands over control
  `[verified-live 2026-09-11, n=1]`.
- Keys: `keybd_event` with the virtual-key code to the foreground window — the stereo keys need
  `VK_NUMPAD4…7`. `U`/`J` turn (a 300 ms hold is ~45°, 120 ms ~20°), `W/A/S/D` move.

## 6. The reader

- **Launch route + log-line table** (inbox, drained into §11h): the key catch was that the watchdog,
  spin and roll lines went only to the debug channel, so a file-only read would have been a false
  "all clear". It also found that `Stereo=2` zeroes the HUD counters until numpad 7 is pressed, that
  roll/pitch need a headset, that the recon pass must not share a launch with stereo, and that the
  275,968-byte DLL set aside today is **our own 0.2.9 automation build** (the earlier "not ours"
  wording was wrong; corrected in the board).
- **Built the "diagnostics to the log file" fix** (`2de67150a128`); I deployed it for run 3 and it
  works: `CameraRollScale`, `LiveHmd via pose_math`, the VR-host and shutdown lines all land in
  `xiii_stereo.log` with no listener `[verified-live 2026-09-11, n=1]`.
- **Now building:** the per-eye HUD and the per-eye shadow fix (one DLL).

## Automation, scored

| Capability | Status |
| --- | --- |
| Self-launch | ✅ Steam appid 1170760 ×2; ✅ direct `XIII.exe Plage01` ×1 |
| Menu → gameplay | ✅ Enter ×3 (profile → Continue → PLAY), Space for the objectives card |
| Commands | ⛔ this build has no command harness (0.2.9 has it); not needed tonight |
| Character + camera | ✅ `W/A/S/D`, `U/J` turning in small steps, numpad stereo keys |
| Self-close | ✅ window close message ×3, clean exit every time |

## Not established

- What the bank's 12 pre-transformed stereo'd draws are.
- XIII's texture-coordinate mode on the shadow stage (camera-space position is the working guess).
- Whether the shadow error is the same for NPC shadows as for the player's arm shadow (one frame, one
  shadow).
- Roll and pitch: headset only.
