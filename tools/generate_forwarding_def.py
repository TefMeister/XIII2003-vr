#!/usr/bin/env python3
"""Generate proxy/proxy.def from `dumpbin /exports` output on the real D3DDrv.dll.

Usage: python generate_forwarding_def.py re-notes/d3ddrv-exports-raw.txt > proxy/proxy.def
"""
import re
import sys


def main():
    exports_path = sys.argv[1]
    names = []
    with open(exports_path, "r") as f:
        lines = f.readlines()

    # dumpbin /exports rows look like:
    #   ordinal hint RVA      name
    #        1    0 00001000 SomeExportedName
    row_re = re.compile(r"^\s*\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)")
    for line in lines:
        m = row_re.match(line)
        if m:
            names.append(m.group(1))

    if not names:
        sys.exit("No exports found - check the dumpbin output format")

    print("LIBRARY D3DDrv")
    print("EXPORTS")
    for name in names:
        print(f"    {name}=D3DDrv_Original.{name}")


if __name__ == "__main__":
    main()
