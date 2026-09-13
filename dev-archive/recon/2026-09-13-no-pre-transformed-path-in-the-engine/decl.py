# -*- coding: utf-8 -*-
"""Decode every FVertexStream::GetComponents in a UE2 module into a D3D FVF.

How the mapping was established (all from the shipped binaries, nothing run):

  D3DDrv_Original.dll!FD3DVertexShader base ctor (VA 0x11615FB0) turns the engine's
  component list into a D3D8 vertex-shader DECLARATION. Its two switch ladders give
  the enums exactly:

    component TYPE  -> D3DVSDT size code:  0->3 (FLOAT4) 1->2 (FLOAT3) 2->1 (FLOAT2)
                                           3->0 (FLOAT1) 4->4 (D3DCOLOR)
    component FUNC  -> D3DVSDE register:   0->0 POSITION  1->3 NORMAL  2->5 DIFFUSE
                                           3->6 SPECULAR  4..11->7..14 TEXCOORD0..7

  So a stream whose POSITION component is declared FLOAT4 is a PRE-TRANSFORMED
  (screen-space, XYZRHW) stream, and one declared FLOAT3 goes through the transform.
  That is the only thing that can make the runtime's FVF carry D3DFVF_XYZRHW (0x004).
"""
import struct, sys
import capstone

PATHS = sys.argv[1:] or [
    r"D:/Program Files (x86)/Steam/steamapps/common/XIII - Classic/system/Engine.dll",
]

TYPE_BYTES = {0: 16, 1: 12, 2: 8, 3: 4, 4: 4}
TYPE_NAME  = {0: "Float4", 1: "Float3", 2: "Float2", 3: "Float1", 4: "Color"}
FUNC_NAME  = {0: "Position", 1: "Normal", 2: "Diffuse", 3: "Specular"}
for i in range(8):
    FUNC_NAME[4 + i] = "TexCoord%d" % i


def load(p):
    d = open(p, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    optsz = struct.unpack_from("<H", d, pe + 20)[0]
    base = struct.unpack_from("<I", d, pe + 24 + 28)[0]
    edir = struct.unpack_from("<I", d, pe + 24 + 96)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    sec = pe + 24 + optsz
    secs = []
    for i in range(nsec):
        o = sec + i * 40
        vsz, va, rsz, ptr = struct.unpack_from("<IIII", d, o + 8)
        secs.append((va + base, max(vsz, rsz), ptr))
    def r2o(va):
        for sva, sz, ptr in secs:
            if sva <= va < sva + sz:
                return ptr + (va - sva)
        return None
    exports = []
    if edir:
        eo = r2o(edir + base)
        nname = struct.unpack_from("<I", d, eo + 24)[0]
        afun, anam, aord = struct.unpack_from("<III", d, eo + 28)
        fo, no, oo = r2o(afun + base), r2o(anam + base), r2o(aord + base)
        for i in range(nname):
            nr = struct.unpack_from("<I", d, no + 4 * i)[0]
            off = r2o(nr + base); end = d.index(b"\0", off)
            nm = d[off:end].decode("ascii", "replace")
            ordl = struct.unpack_from("<H", d, oo + 2 * i)[0]
            fr = struct.unpack_from("<I", d, fo + 4 * ordl)[0]
            exports.append((nm, base + fr))
    return d, r2o, exports


def fvf_of(comps):
    """comps: ordered list of (type, func). Returns (fvf, stride, note)."""
    fvf, stride, note = 0, 0, ""
    for t, f in comps:
        stride += TYPE_BYTES.get(t, 0)
        if f == 0:
            fvf |= 0x004 if t == 0 else 0x002
            if t not in (0, 1): note = "position is %s -- unusual" % TYPE_NAME.get(t, t)
        elif f == 1: fvf |= 0x010
        elif f == 2: fvf |= 0x040
        elif f == 3: fvf |= 0x080
        elif 4 <= f <= 11:
            n = f - 3
            if n > ((fvf >> 8) & 0xF): fvf = (fvf & ~0xF00) | (n << 8)
    return fvf, stride, note


def decode(d, r2o, va, maxins=60):
    """Read the simple `mov [eax+N], imm` / `mov [eax+N], reg` body of a GetComponents."""
    off = r2o(va)
    code = d[off:off + 8 * maxins]
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    regs = {}
    slots = {}
    ret = None
    for ins in md.disasm(code, va):
        if ins.mnemonic == "ret":
            break
        ops = ins.operands
        if ins.mnemonic == "mov" and len(ops) == 2:
            dst, src = ops
            if dst.type == capstone.x86.X86_OP_REG and src.type == capstone.x86.X86_OP_IMM:
                regs[dst.reg] = src.imm
                if dst.reg == capstone.x86.X86_REG_EAX:
                    ret = src.imm          # the last EAX immediate is the component count
            elif dst.type == capstone.x86.X86_OP_MEM and dst.mem.disp >= 0 and dst.mem.index == 0:
                val = None
                if src.type == capstone.x86.X86_OP_IMM: val = src.imm
                elif src.type == capstone.x86.X86_OP_REG and src.reg in regs: val = regs[src.reg]
                if val is not None:
                    slots[dst.mem.disp] = val
    comps = []
    i = 0
    while (8 * i) in slots and (8 * i + 4) in slots:
        comps.append((slots[8 * i], slots[8 * i + 4]))
        i += 1
    return comps, ret


for p in PATHS:
    name = p.replace("\\", "/").rsplit("/", 1)[-1]
    try:
        d, r2o, exports = load(p)
    except Exception as e:
        print("== %s: cannot read (%s)" % (name, e)); continue
    gc = [(nm, va) for nm, va in exports if nm.startswith("?GetComponents@")]
    if not gc:
        print("== %s: no GetComponents exports" % name); continue
    print("== %s ==" % name)
    seen = {}
    for nm, va in sorted(gc, key=lambda r: r[0]):
        cls = nm.split("@")[1]
        comps, cnt = decode(d, r2o, va)
        if cnt is not None and 0 < cnt <= len(comps):
            comps = comps[:cnt]
        fvf, stride, note = fvf_of(comps)
        pre = "  <== PRE-TRANSFORMED (XYZRHW)" if (fvf & 0x00E) == 0x004 else ""
        desc = ", ".join("%s %s" % (TYPE_NAME.get(t, t), FUNC_NAME.get(f, f)) for t, f in comps)
        print("  %-34s fvf=0x%03X stride=%2d  [%s]%s%s"
              % (cls, fvf, stride, desc, ("  " + note) if note else "", pre))
