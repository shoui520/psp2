#!/usr/bin/env python3
# Bind the linked ARM unwind tables into SceModuleInfo.
#
#   fixup_unwind.py module.elf module.velf
#
# vita-elf-create leaves exidx_top/exidx_end/extab_top/extab_end zero. Take
# the .ARM.exidx and .ARM.extab ranges of the linked ELF and store them,
# relative to the first segment, in the module info of the VELF. Nothing
# else in the file changes.

import struct
import sys

MI_EXIDX_TOP = 0x4c
SECTIONS = (b".ARM.exidx", b".ARM.extab")


def section_ranges(path):
	with open(path, "rb") as f:
		d = f.read()
	shoff, = struct.unpack_from("<I", d, 0x20)
	shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x2e)
	shdrs = [struct.unpack_from("<10I", d, shoff + shentsize * i) for i in range(shnum)]
	strtab = shdrs[shstrndx][4]
	found = {}
	for sh in shdrs:
		name = d[strtab + sh[0]:d.index(b"\0", strtab + sh[0])]
		if name in SECTIONS:
			if name in found:
				raise SystemExit("fixup_unwind: duplicate %s" % name.decode())
			found[name] = (sh[3], sh[3] + sh[5])
	if len(found) != len(SECTIONS):
		raise SystemExit("fixup_unwind: unwind sections missing")
	return found[SECTIONS[0]] + found[SECTIONS[1]]


def main():
	if len(sys.argv) != 3:
		raise SystemExit("usage: fixup_unwind.py module.elf module.velf")
	ranges = section_ranges(sys.argv[1])
	with open(sys.argv[2], "r+b") as f:
		d = bytearray(f.read())
		entry, phoff = struct.unpack_from("<II", d, 0x18)
		seg_off, seg_va = struct.unpack_from("<II", d, phoff + 4)
		mi = seg_off + (entry & 0x3fffffff)
		fields = tuple(value - seg_va for value in ranges)
		old = struct.unpack_from("<4I", d, mi + MI_EXIDX_TOP)
		if old not in ((0, 0, 0, 0), fields):
			raise SystemExit("fixup_unwind: unexpected unwind fields")
		struct.pack_into("<4I", d, mi + MI_EXIDX_TOP, *fields)
		f.seek(0)
		f.write(d)


if __name__ == "__main__":
	main()
