# Every UE2 vertex stream in XIII, decoded to an FVF (2026-09-13, `/pd`, static)

Evidence for **dossier §11i**. Nothing was run; the game was not launched. These read the shipped
`Engine.dll` and `D3DDrv_Original.dll` as data.

## Files

| file | what it is |
| --- | --- |
| `decl.py` | decodes every `FVertexStream::GetComponents` in a UE2 module into an FVF + stride |
| `decl-output.txt` | its output for this game's seven candidate modules |

Run it as `python decl.py <module.dll> [<module.dll> …]`. It needs `capstone`.

## The finding in one line

**All 14 vertex streams declare position as `Float3` (→ `D3DFVF_XYZ`). Not one declares `Float4`,**
which is the only way the D3D8 runtime's FVF could carry `D3DFVF_XYZRHW` — so the engine has no
pre-transformed drawing path, and the bank level's twelve "pre-transformed" draws were being
misidentified. `[inferred-static 2026-09-13]`

## How the two enums were obtained — reusable on any UE2 D3D driver

They are not guessed and not taken from any published source. The stock driver's
`FD3DVertexShader` base constructor (`D3DDrv_Original.dll` VA `0x11615FB0`) converts the engine's
component list into a D3D8 declaration, and its two switch ladders **are** the mapping:

- component `Type` → `D3DVSDT_*`: `0→FLOAT4`, `1→FLOAT3`, `2→FLOAT2`, `3→FLOAT1`, `4→D3DCOLOR`
- component `Function` → `D3DVSDE_*` register: `0→POSITION`, `1→NORMAL`, `2→DIFFUSE`,
  `3→SPECULAR`, `4..11→TEXCOORD0..7`

**Cross-check that makes it more than a reading:** `FCanvasUtil::GetComponents` writes
`(1,0) (4,2) (2,4)` and `GetStride()` returns `0x18`. Decoded that is `XYZ|DIFFUSE|TEX1`, 24 bytes —
which is `FCanvasVertex(FVector, FColor, float, float)` to the byte, **and** it is the literal
constant `0x142` the driver pushes to `SetVertexShader` at VA `0x11607B6B`. Three independent things
agreeing is why the table above is treated as settled rather than plausible.

## Addresses used (this build of XIII — Steam "XIII - Classic", `D3DDrv_Original.dll`, ImageBase `0x11600000`)

| VA | what |
| --- | --- |
| `0x1160D740` | `UD3DRenderDevice::GetVertexShader(EVertexShader, FShaderDeclaration&)` — a cache walk; only enum 0 may be created |
| `0x116162F0` | `FD3DFixedVertexShader` ctor — calls `CreateVertexShader` via `[vtbl+0x12C]` with **`pFunction = 0`** |
| `0x11615FB0` | `FD3DVertexShader` base ctor — the component → declaration conversion above |
| `0x11601DE8`, `0x11607B6B`, `0x11608BBC` | the driver's only `SetVertexShader` calls with a literal FVF: `0x242`, `0x142`, `0x242` |

⚠️ Addresses are for this build only. Re-derive from the exports (`?GetVertexShader@UD3DRenderDevice@@…`)
rather than trusting them on a different copy of the game.

## What this does NOT show

That the twelve draws specifically were declaration handles. That needs one flat launch — see the
board row and §11i.
