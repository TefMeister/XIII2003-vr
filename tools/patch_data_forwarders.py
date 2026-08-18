#!/usr/bin/env python3
"""Post-build patch: add PE-level forwarder exports for D3DDrv.dll's two DATA
exports (GPackage, autoclassUD3DRenderDevice).

Why this exists: MSVC's link.exe supports export *forwarding* via .def files
("entryname=OtherDll.exportname") only for CODE exports. It has no support for
forwarding DATA exports -- attempting `Name=OtherDll.Name DATA` in a .def file
still produces LNK2001 "unresolved external symbol". This is a documented
link.exe limitation, not a mistake in the .def syntax.

The real D3DDrv.dll exports two DATA symbols (confirmed via dumpbin: both RVAs
fall inside its .data section, not .text):
  - GPackage                    (UPackage* -- Unreal's native-package pointer)
  - autoclassUD3DRenderDevice   (native-class auto-registration object)

Both are load-bearing: Unreal Engine's native-package loader does
GetProcAddress(hModule, "GPackage") while binding the "D3DDrv" package during
UGameEngine::Init. Omitting them causes a hard failure at startup:
  "Failed to find object 'Class D3DDrv.D3DRenderDevice'"
  (History: UObject::SafeLoadError <- UObject::StaticLoadClass <-
   UGameEngine::Init <- InitEngine)
This was verified empirically by building a proxy without these two exports
and launching the game.

Since the PE format itself has no restriction on forwarders being CODE vs
DATA (the OS loader/GetProcAddress resolves a forwarder by testing whether
the export's RVA falls inside the Export Directory's own address range --
uniformly for functions and data), the fix is to bypass link.exe's front-end
restriction: rebuild the *entire* export directory as a fresh, freshly-sorted,
all-forwarder table (all 42 original exports, not just the 40 link.exe could
express) in a new PE section appended after linking, and repoint the Export
Data Directory at it. This makes every export -- including the two data ones
-- a genuine, zero-copy PE forwarder, identical in mechanism to how e.g.
kernel32.dll forwards exports to ntdll.dll.

Usage:
    python tools/patch_data_forwarders.py <dll_path> <exports_txt_path>

Patches <dll_path> in place. <exports_txt_path> is dumpbin /exports output on
the real D3DDrv.dll (re-notes/d3ddrv-exports-raw.txt), used to recover the
full 42-name export list (link.exe's build only kept 40 in the .def; this
script needs all of them to rebuild a complete table).
"""
import re
import struct
import sys

FORWARD_PREFIX = "D3DDrv_Original."


def parse_export_names(exports_path):
    row_re = re.compile(r"^\s*\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)")
    names = []
    with open(exports_path, "r") as f:
        for line in f:
            m = row_re.match(line)
            if m:
                names.append(m.group(1))
    if not names:
        sys.exit("No exports found in " + exports_path)
    return names


def align(value, alignment):
    return (value + alignment - 1) // alignment * alignment


def build_export_section(names, section_rva):
    """Build the raw bytes of a self-contained export directory blob that
    forwards every name in `names` to D3DDrv_Original.<name>, placed at
    `section_rva`."""
    n = len(names)

    # Fixed-size region layout (offsets from section_rva):
    EXPORT_DIR_SIZE = 40
    off_dir = 0
    off_addr_functions = off_dir + EXPORT_DIR_SIZE
    off_addr_names = off_addr_functions + 4 * n
    off_addr_ordinals = off_addr_names + 4 * n
    off_strings = off_addr_ordinals + 2 * n

    module_name = b"D3DDrv.dll\x00"
    module_name_rva = section_rva + off_strings
    cursor = off_strings + len(module_name)

    # Name strings, in original ordinal order (index i == ordinal i+1).
    name_rvas = []
    name_bytes_list = []
    for name in names:
        b = name.encode("ascii") + b"\x00"
        name_bytes_list.append(b)
        name_rvas.append(section_rva + cursor)
        cursor += len(b)

    # Forwarder strings "D3DDrv_Original.<name>\0", original ordinal order.
    forwarder_rvas = []
    forwarder_bytes_list = []
    for name in names:
        b = (FORWARD_PREFIX + name).encode("ascii") + b"\x00"
        forwarder_bytes_list.append(b)
        forwarder_rvas.append(section_rva + cursor)
        cursor += len(b)

    total_size = cursor

    # AddressOfFunctions: ordinal order, each entry is a forwarder RVA.
    addr_functions = b"".join(struct.pack("<I", rva) for rva in forwarder_rvas)

    # Export Name Pointer Table must be sorted ascending (the loader binary-
    # searches it in GetProcAddress-by-name). AddressOfNameOrdinals holds,
    # for each sorted name, the 0-based index into AddressOfFunctions.
    order = sorted(range(n), key=lambda i: names[i])
    addr_names = b"".join(struct.pack("<I", name_rvas[i]) for i in order)
    addr_ordinals = b"".join(struct.pack("<H", i) for i in order)

    export_dir = struct.pack(
        "<IIHHIIIIIII",
        0,               # Characteristics
        0,                # TimeDateStamp
        0, 0,             # MajorVersion, MinorVersion
        module_name_rva,  # Name
        1,                # Base (ordinal base)
        n,                # NumberOfFunctions
        n,                # NumberOfNames
        section_rva + off_addr_functions,
        section_rva + off_addr_names,
        section_rva + off_addr_ordinals,
    )
    assert len(export_dir) == EXPORT_DIR_SIZE

    blob = bytearray(total_size)
    blob[off_dir:off_dir + EXPORT_DIR_SIZE] = export_dir
    blob[off_addr_functions:off_addr_functions + len(addr_functions)] = addr_functions
    blob[off_addr_names:off_addr_names + len(addr_names)] = addr_names
    blob[off_addr_ordinals:off_addr_ordinals + len(addr_ordinals)] = addr_ordinals
    pos = off_strings
    blob[pos:pos + len(module_name)] = module_name
    pos += len(module_name)
    for b in name_bytes_list:
        blob[pos:pos + len(b)] = b
        pos += len(b)
    for b in forwarder_bytes_list:
        blob[pos:pos + len(b)] = b
        pos += len(b)
    assert pos == total_size

    return bytes(blob)


def patch(dll_path, exports_path):
    import pefile

    names = parse_export_names(exports_path)

    pe = pefile.PE(dll_path)
    oh = pe.OPTIONAL_HEADER
    fh = pe.FILE_HEADER
    section_alignment = oh.SectionAlignment
    file_alignment = oh.FileAlignment

    last_section = pe.sections[-1]
    new_section_rva = align(
        last_section.VirtualAddress + last_section.Misc_VirtualSize,
        section_alignment,
    )

    data = bytearray(pe.__data__)
    new_section_file_offset = align(len(data), file_alignment)
    # Pad file up to the aligned offset before appending new section data.
    data.extend(b"\x00" * (new_section_file_offset - len(data)))

    blob = build_export_section(names, new_section_rva)
    raw_size = align(len(blob), file_alignment)
    padded_blob = blob + b"\x00" * (raw_size - len(blob))

    # --- New IMAGE_SECTION_HEADER ---
    section_name = b".fwddata"
    assert len(section_name) == 8
    new_section_header = struct.pack(
        "<8sIIIIIIHHI",
        section_name,
        len(blob),               # VirtualSize
        new_section_rva,          # VirtualAddress
        raw_size,                 # SizeOfRawData
        new_section_file_offset,  # PointerToRawData
        0, 0,                      # PointerToRelocations, PointerToLinenumbers
        0, 0,                      # NumberOfRelocations, NumberOfLinenumbers
        0x40000040,                # Characteristics: INITIALIZED_DATA | MEM_READ
    )
    assert len(new_section_header) == 40

    section_table_offset = pe.sections[0].get_file_offset() + fh.NumberOfSections * (-40)
    # (computed properly below; the line above is unused, kept out for clarity)
    first_section_offset = pe.sections[0].get_file_offset()
    new_section_header_offset = first_section_offset + fh.NumberOfSections * 40

    data[new_section_header_offset:new_section_header_offset + 40] = new_section_header

    # --- Patch NumberOfSections ---
    nos_offset = fh.get_file_offset() + 2  # Machine(2) precedes NumberOfSections
    struct.pack_into("<H", data, nos_offset, fh.NumberOfSections + 1)

    # --- Patch SizeOfImage ---
    new_size_of_image = align(new_section_rva + len(blob), section_alignment)
    size_of_image_field = oh.__field_offsets__["SizeOfImage"]
    size_of_image_offset = oh.get_file_offset() + size_of_image_field
    struct.pack_into("<I", data, size_of_image_offset, new_size_of_image)

    # --- Patch Export Data Directory entry [0] ---
    export_dd = oh.DATA_DIRECTORY[0]
    dd_offset = export_dd.get_file_offset()
    struct.pack_into("<II", data, dd_offset, new_section_rva, len(blob))

    # --- Append the new section's raw data ---
    data[new_section_file_offset:new_section_file_offset + raw_size] = padded_blob

    pe.close()  # release the mmap'd handle before reopening dll_path for write

    with open(dll_path, "wb") as f:
        f.write(data)

    print(f"Patched {dll_path}: added {len(names)}-entry forwarder export "
          f"table in new section .fwddata @ RVA 0x{new_section_rva:x} "
          f"({len(blob)} bytes, file offset 0x{new_section_file_offset:x}).")


def main():
    if len(sys.argv) != 3:
        sys.exit(f"Usage: {sys.argv[0]} <dll_path> <exports_txt_path>")
    patch(sys.argv[1], sys.argv[2])


if __name__ == "__main__":
    main()
