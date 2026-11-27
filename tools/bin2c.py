#!/usr/bin/env python3
"""Convert a compiled binary image into a C source/header array initializer."""

import argparse
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser(description="Emit a C array definition from a raw binary file.")
    ap.add_argument("input", type=Path)
    ap.add_argument("-o", "--output", type=Path, required=True)
    ap.add_argument("--symbol", required=True, help="C array symbol name")
    ap.add_argument("--type", default="unsigned char", help="C element type (default: unsigned char)")
    ap.add_argument("--const", dest="is_const", action="store_true", help="Mark array const")
    ap.add_argument("--section", default=None, help="Optional ELF section attribute")
    ap.add_argument("--align", type=int, default=0, help="Optional alignment attribute")
    args = ap.parse_args()

    data = args.input.read_bytes()
    attrs = []
    if args.section:
        attrs.append(f'section("{args.section}")')
    if args.align:
        attrs.append(f"aligned({args.align})")
    attr_str = f" __attribute__(({', '.join(attrs)}))" if attrs else ""
    qual = "const " if args.is_const else ""

    lines = [
        f"/* Auto-generated from {args.input.name} ({len(data)} bytes) by tools/bin2c.py. */",
        f"{qual}{args.type}{attr_str} {args.symbol}[{len(data)}] = {{",
    ]
    for i in range(0, len(data), 16):
        chunk = ", ".join(f"0x{b:02x}" for b in data[i:i + 16])
        lines.append(f"    {chunk},")
    lines.append("};")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
