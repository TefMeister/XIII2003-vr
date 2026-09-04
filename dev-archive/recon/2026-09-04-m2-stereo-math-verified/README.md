# M2 stereo maths — numeric verification and mutation record (2026-09-04, home PC, `/pd`)

Record of the checks behind `modding-notes/2026-09-04-m2-stereo-built-route-b-plus-constants-path.md`
§2b. Nothing here was run inside the game. Our own generated data; no game content.

## Build

`staging/XIII2003-vr/src/repo`, `build.bat` (MSVC 19.44 / VS2022 Build Tools v17.14, Win32, `/O2 /MT`):
exit 0 → `proxy/D3DDrv.dll` **211,968 bytes**, md5 `a748d3bbb92a1d2931082eb6c946eff3`.
Export table: 40 names, identical to the deployed 0.2.7 `D3DDrv.dll` (75,776 B, md5 `f15983294e269104a7d947a5ba563763`).

CMake (`-A Win32`, RelWithDebInfo) builds both test executables; `ctest`: 2/2 passed.

## Tests (`tests/test_stereo_math.cpp`, doctest)

```
[doctest] test cases:   11 |   11 passed | 0 failed | 0 skipped
[doctest] assertions: 9209 | 9209 passed | 0 failed |
[doctest] Status: SUCCESS!
```

The pre-existing `pose_math_tests` still pass (10 cases, 24 assertions).

## Mutation check — the tests can fail

Each row is a deliberate one-line breakage of a scratch copy of `pose_math/stereo_math.cpp`,
compiled with the UNCHANGED test source (`cl /O2 /EHsc /std:c++17`), run once.

| variant | test cases | assertions | exit |
|---|---|---|---|
| BASELINE (unmodified shipped code) | 11 passed, 0 failed | 9209 passed, 0 failed | 0 |
| A: eye-shift sign flipped (`t[12] = +eyeOffsetX`) | 9 passed, **2 failed** | 7203 passed, **2006 failed** | 1 |
| B: off-centre x term sign flipped (`p[8] = +(r+l)/dx`) | 10 passed, **1 failed** | 9169 passed, **40 failed** | 1 |
| C: `Mat4Mul` in column-vector order (a, b swapped) | 8 passed, **3 failed** | 968 passed, **250 failed** (run aborts early on REQUIREs) | 1 |
| D: right half no longer takes the odd pixel (`out->w = halfW`) | 10 passed, **1 failed** | 9203 passed, **6 failed** | 1 |
| E: near/far row dropped from the eye projection (`p[14] = 0`) | 9 passed, **2 failed** | 9205 passed, **4 failed** | 1 |

Script: a Python driver capturing the `vcvars32` environment, compiling each variant into a scratch
folder and reading the doctest summary. Not committed (throwaway); the table is the evidence.

## Ground truth used by the tests (independent of the code under test)

- `D3DXMatrixPerspectiveFovLH` written out in double.
- Parallax: a camera moved right by `e` sees view-space `x - e`, so `ndc.x` moves by `-e*m00/z`;
  the left eye (`e = -ipd/2`) must therefore see every finite point further RIGHT by `ipd*m00/z`.
- A test-only general 4x4 inverse, to check the hook's recompose `W*V_eye*P_eye` against
  `(W*V*P) * P^-1 * T(-e) * P_eye`.
- XIII's measured frustum from the 2026-09-03 run: fovY 68.996°, 4:3, near 5, far 65541.
