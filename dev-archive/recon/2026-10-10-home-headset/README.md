# 2026-10-10 home PC, real headset (Quest 3 via VDXR 32-bit, sensor covered, nobody wearing it)

- Installed `D3DDrv-m2-stereo-2026-09-13-vscensus.dll` (4d494dc32730) as System\D3DDrv.dll; the previous one
  (fcd83c2d2608) is in System\_backup-2026-10-10.
- It starts: OpenXR host thread, PROJECTION layer path, "M2 stereo (mode 1, ipd 3.40 units, scale 52.5 u/m,
  hmdfov 1)", log lines `stereo=ON ipd=3.20u(hmd)` at ~24-30 frames/s on the Ubisoft logo; the build's own frame
  capture shows the logo side by side (frame300.png) `[verified-live 2026-10-10, n=1]`.
- Not reached: gameplay. The game window captures black, and this build stops ticking whenever the window is not
  in front (known hazard), so blind menu driving was not attempted. Closed with WM_CLOSE, clean.
- So the four questions (roll, pitch, HUD, shadows) are still Tefa's. Video:
  MEGA Videos/XIII2003-vr/headset newest stereo build_2026-10-10_22-12-21.mp4.
