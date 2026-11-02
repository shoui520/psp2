#!/usr/bin/env python3
"""The words of PDS programs, from the C headers Imagination's pdsasm writes.

    pdsbin.py OUTPUT HEADER...

pdsasm writes a program (its data segment, then its code) as the array g_pui32PDS<name> of a
C header. OUTPUT gets the programs' words one after another: little-endian bytes for a .bin
(for .incbin), or a C initializer list for a .inc (for #include in an array's braces).
"""
import re
import struct
import sys
from pathlib import Path


def words(header):
	text = Path(header).read_text()
	m = re.search(r'g_pui32PDS\w*\[(\d+)\]\s*=\s*\{(.*?)\};', text, re.S)
	if not m:
		sys.exit(f'{header}: no pdsasm program')
	values = [int(v, 16) for v in re.findall(r'0x[0-9A-Fa-f]+', m.group(2))]
	if len(values) != int(m.group(1)):
		sys.exit(f'{header}: {len(values)} words, the array has {m.group(1)}')
	return values


def main(argv):
	if len(argv) < 2:
		sys.exit(__doc__)
	output = Path(argv[0])
	program = [w for header in argv[1:] for w in words(header)]
	if output.suffix == '.bin':
		output.write_bytes(struct.pack(f'<{len(program)}I', *program))
	elif output.suffix == '.inc':
		lines = [', '.join(f'0x{w:08x}' for w in program[i:i + 8]) + ','
			 for i in range(0, len(program), 8)]
		output.write_text(''.join(f'\t{line}\n' for line in lines))
	else:
		sys.exit(f'{output}: .bin or .inc')


if __name__ == '__main__':
	main(sys.argv[1:])
