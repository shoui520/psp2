#!/usr/bin/env python3
# kernel_boot_loader's ELF, as the firmware's kernel_boot_loader.self holds it: SKBL's segments,
# then NSKBL's ARZL archive, which SKBL unpacks to 0x51000000 (kbl_main.c).
#
#   mkkbl.py skbl.elf nskbl.arzl kernel_boot_loader.elf
#
# Sony's layout, five program headers each at a 0x100 boundary:
#   0  the ELF header and the program headers themselves (type 0, flags R)
#   1  the reset vector page, at 0 (RWX)
#   2  SKBL's code and read-only data, at 0x40020000 (RX), the entry
#   3  its data, then .bss in memory only (RW)
#   4  the archive at 0x50000000 (RX)
# tools/mkfself.py packs it as a SELF (type 9, a boot loader).
import struct
import sys

ARCHIVE_ADDRESS = 0x50000000
EF_ARM = 0x5000402		# EABI 5, BE8 off, hard-float off, as Sony's
SEGMENT_ALIGN = 0x100
PT_NULL, PT_LOAD = 0, 1
PF_R, PF_W, PF_X = 4, 2, 1


def align(value, alignment):
	return (value + alignment - 1) & -alignment


def loads(elf):
	"""SKBL's loaded segments: (vaddr, data, memsz, flags)"""
	phoff, = struct.unpack_from('<I', elf, 0x1c)
	phentsize, phnum = struct.unpack_from('<HH', elf, 0x2a)
	out = []
	for i in range(phnum):
		ptype, offset, vaddr, _, filesz, memsz, flags, _ = struct.unpack_from(
			'<8I', elf, phoff + i * phentsize)
		if ptype == PT_LOAD:
			out.append((vaddr, elf[offset:offset + filesz], memsz, flags))
	return out


def compose(skbl, archive):
	segments = loads(skbl)
	if len(segments) != 3:
		sys.exit(f'mkkbl.py: SKBL has {len(segments)} loaded segments, not 3 (skbl.ld)')
	segments.append((ARCHIVE_ADDRESS, archive, len(archive), PF_R | PF_X))
	entry, = struct.unpack_from('<I', skbl, 0x18)
	phnum = 1 + len(segments)
	headers = 0x34 + phnum * 0x20

	body = bytearray()
	phdrs = [(PT_NULL, 0, 0, 0, headers, headers, PF_R, 4)]
	offset = align(headers, SEGMENT_ALIGN)
	for vaddr, data, memsz, flags in segments:
		phdrs.append((PT_LOAD, offset, vaddr, vaddr, len(data), memsz, flags, SEGMENT_ALIGN))
		body += bytes(offset - headers - len(body)) + data
		offset = align(offset + len(data), SEGMENT_ALIGN)

	elf = bytearray(struct.pack('<4sBBBB8xHHIIIIIHHHHHH', b'\x7fELF', 1, 1, 1, 0,
				    2, 40, 1, entry, 0x34, 0, EF_ARM, 0x34, 0x20, phnum, 0x28, 0, 0))
	for p in phdrs:
		elf += struct.pack('<8I', *p)
	return bytes(elf + body)


def main():
	if len(sys.argv) != 4:
		sys.exit('usage: mkkbl.py skbl.elf nskbl.arzl kernel_boot_loader.elf')
	with open(sys.argv[1], 'rb') as f:
		skbl = f.read()
	with open(sys.argv[2], 'rb') as f:
		archive = f.read()
	with open(sys.argv[3], 'wb') as f:
		f.write(compose(skbl, archive))


if __name__ == '__main__':
	main()
