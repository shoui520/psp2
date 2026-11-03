#!/usr/bin/env python3
"""The words of PDS and USE programs, from the files their assemblers write.

    pdsbin.py [--code] OUTPUT INPUT...

An INPUT is either the C header Imagination's pdsasm writes (a program's data segment, then
its code, as the array g_pui32PDS<name>) or the raw little-endian binary useasm writes.
OUTPUT gets the programs' words one after another: little-endian bytes for a .bin (for
.incbin), a C initializer list for a .inc (for #include in an array's braces), or a #define
per word for a .h (for a program a module stores word by word).

--code leaves out a header input's data segment (its PDS_*_DATA_SEGMENT_SIZE bytes), for a
caller that fills the data itself and only needs the code.
"""
import re
import struct
import sys
from pathlib import Path


def header(header):
	"""A program's data segment size and words."""
	text = Path(header).read_text()
	m = re.search(r'g_pui32PDS\w*\[(\d+)\]\s*=\s*\{(.*?)\};', text, re.S)
	if not m:
		sys.exit(f'{header}: no pdsasm program')
	values = [int(v, 16) for v in re.findall(r'0x[0-9A-Fa-f]+', m.group(2))]
	if len(values) != int(m.group(1)):
		sys.exit(f'{header}: {len(values)} words, the array has {m.group(1)}')
	m = re.search(r'#define PDS_\w*DATA_SEGMENT_SIZE\s*\((\d+)UL\)', text)
	if not m:
		sys.exit(f'{header}: no data segment size')
	return int(m.group(1)) // 4, values


def words_of(name, code_only):
	"""A program's words: a useasm binary is all code, a pdsasm header has a data segment."""
	if Path(name).suffix == '.bin':
		data = Path(name).read_bytes()
		if len(data) % 4:
			sys.exit(f'{name}: {len(data)} bytes, not whole words')
		return list(struct.unpack(f'<{len(data) // 4}I', data))
	data, values = header(name)
	return values[data:] if code_only else values


def main(argv):
	code_only = False
	if argv[:1] == ['--code']:
		code_only = True
		argv = argv[1:]
	if len(argv) < 2:
		sys.exit(__doc__)
	output = Path(argv[0])
	program = []
	for name in argv[1:]:
		program += words_of(name, code_only)
	if output.suffix == '.bin':
		output.write_bytes(struct.pack(f'<{len(program)}I', *program))
	elif output.suffix == '.inc':
		lines = [', '.join(f'0x{w:08x}' for w in program[i:i + 8]) + ','
			 for i in range(0, len(program), 8)]
		output.write_text(''.join(f'\t{line}\n' for line in lines))
	elif output.suffix == '.h':
		prefix = re.sub(r'\W', '_', output.stem).upper()
		output.write_text(''.join(f'#define PDS_{prefix}_INSN{i} 0x{w:08x}\n'
					  for i, w in enumerate(program)))
	else:
		sys.exit(f'{output}: .bin, .inc or .h')


if __name__ == '__main__':
	main(sys.argv[1:])
