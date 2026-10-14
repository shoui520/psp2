#!/usr/bin/env python3
"""
Pack the VIP image (os0:kd/vipimg.skprx's payload) from the linked programs.

    pack.py OUT --fw1 BOOT PU0 PU1 --fw2 BASE PARSER0 PARSER1 CODEC RECORDS

Each firmware is linked at 0x800000: fw1 is the image's first 0x80000 bytes,
FW2 follows at +0x80000. A firmware's region holds its programs' loaded bytes
(by load address) and zeros, its uninitialised DRAM data included; fw1's region
is 0xff from the 4 KB after its last data on. The image is stored as the cores
read it, big-endian, with every 8-byte group reversed (they fetch little-endian
64-bit words).
"""
import struct
import sys

BASE = 0x800000
FW1_SIZE = 0x80000
SHT_NOBITS = 8
SHF_ALLOC = 2
PT_LOAD = 1


def read_elf(path):
	"""The loaded bytes (load address, data) and the zeroed ranges of an ELF."""
	data = open(path, 'rb').read()
	if data[:4] != b'\x7fELF' or data[4] != 1 or data[5] != 2:
		sys.exit('%s: not a 32-bit big-endian ELF' % path)
	phoff, shoff = struct.unpack_from('>II', data, 0x1c)
	phentsize, phnum, shentsize, shnum = struct.unpack_from('>HHHH', data, 0x2a)
	loads = []
	for i in range(phnum):
		p_type, offset, vaddr, paddr, filesz, memsz = struct.unpack_from(
			'>IIIIII', data, phoff + i * phentsize)
		if p_type == PT_LOAD and filesz:
			loads.append((paddr, data[offset:offset + filesz]))
	zeros = []
	for i in range(shnum):
		name, sh_type, flags, addr, offset, size = struct.unpack_from(
			'>IIIIII', data, shoff + i * shentsize)
		if sh_type == SHT_NOBITS and flags & SHF_ALLOC and size and \
				BASE <= addr < BASE + FW1_SIZE:
			zeros.append((addr, size))
	return loads, zeros


def build(paths, name):
	"""A firmware's region: its bytes from BASE, and where its data ends."""
	image = {}
	end = 0
	for path in paths:
		loads, zeros = read_elf(path)
		for address, chunk in loads:
			if not BASE <= address or address + len(chunk) > BASE + FW1_SIZE:
				sys.exit('%s: %s loads at 0x%x, outside the firmware' %
					(name, path, address))
			for i, byte in enumerate(chunk):
				offset = address - BASE + i
				if offset in image:
					sys.exit('%s: %s overlaps at 0x%x' % (name, path, address + i))
				image[offset] = byte
			end = max(end, address - BASE + len(chunk))
		for address, size in zeros:
			end = max(end, address - BASE + size)
	region = bytearray(end)
	for offset, byte in image.items():
		region[offset] = byte
	return region, end


def main(argv):
	if len(argv) != 12 or argv[2] != '--fw1' or argv[6] != '--fw2':
		sys.exit(__doc__)
	out = argv[1]
	fw1, fw1_end = build(argv[3:6], 'fw1')
	fw2, fw2_end = build(argv[7:12], 'fw2')

	used = (fw1_end + 0xfff) & ~0xfff
	if used > FW1_SIZE:
		sys.exit('fw1: 0x%x bytes, more than 0x%x' % (used, FW1_SIZE))
	image = fw1 + bytes(used - len(fw1)) + b'\xff' * (FW1_SIZE - used)
	image += fw2 + bytes(-len(fw2) % 8)

	swapped = bytearray(len(image))
	for i in range(0, len(image), 8):
		swapped[i:i + 8] = image[i:i + 8][::-1]
	open(out, 'wb').write(swapped)


if __name__ == '__main__':
	main(sys.argv)
