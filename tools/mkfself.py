#!/usr/bin/env python3
# A fake SELF (fSELF): an SCE executable in the SELF container, laid out as Sony's own SELFs
# are, with its segments not encrypted and no signature (attribute 0xC0, as the SDK's
# psp2bin -of fself writes it). It is what a .skprx, .suprx or eboot.bin of ours is shipped as.
#
#   mkfself.py [--like SELF] [--auth-id ID] [--self-type N] [--app-version V] [--compress]
#              module.velf module.self
#
# The build (cmake/psp2.cmake) makes each module's SELF as the firmware ships it: the auth ID
# from its fw/<FW>.cmake (MODULE_AUTH_ID), type 7 for a kernel module and 13 for a user one, the
# firmware's version, compressed unless the original is not (MODULE_SELF_UNCOMPRESSED). --like
# takes all of these from one of Sony's SELFs instead.
#
# The header, 0x1000 bytes:
#   0x000  SCE header: "SCE\0", version 3, attribute 0xC0, category 1 (a SELF), the extended
#          header's size (0x600), the ELF's offset (0x1000) and size, the file's size
#   0x030  SELF header: where the parts below are
#   0x080  app info: program authority ID, vendor ID (0), SELF type and version (Sony's: type 7
#          a kernel module, 13 a user module or program, 11 a secure module; the firmware
#          version in bits 32-63, 3.65 as 0x365 << 32). The defaults are psp2bin's.
#   0x0a0  the ELF header, then its program headers
#          segment infos: each segment's offset in the file, size, compression (1 none,
#          2 zlib) and encryption (2 none)
#          SCE version
#          control infos, as Sony's system modules carry them: 4 (the ELF's SHA-256), 6 (its
#          first word 1), 7 (empty)
# Then the ELF: whole at 0x1000, or, compressed (--compress, as Sony's kernel modules), its
# headers there and each segment deflated (zlib's default level, which gives Sony's sizes),
# in file order from the first segment's own offset on, each keeping the gap before it and
# its offset modulo 16. Secure modules (MeP) have another header (0x800 bytes), which this does not write.

import argparse
import hashlib
import struct
import sys
import zlib

HEADER_SIZE = 0x1000
EXT_HEADER_SIZE = 0x600
ATTRIBUTE_FSELF = 0xC0
CATEGORY_SELF = 1
COMPRESSION_NONE, COMPRESSION_ZLIB = 1, 2
ENCRYPTION_NONE = 2

# psp2bin's app info
AUTH_ID = 0x2F00000000000001
SELF_TYPE = 8
VERSION = 0x0001000000000000

# control info 4 begins with this constant in every SELF, before the ELF's digest
CONTROL4_CONSTANT = bytes.fromhex('627cb1808ab938e32c8c091708726a579e2586e4')


def align(value, alignment):
	return (value + alignment - 1) & -alignment


def like(path):
	"""A SELF's app info (authority ID, vendor ID, SELF type, version) and whether its
	segments are compressed."""
	with open(path, 'rb') as f:
		header = f.read(0x1000)
	if header[:4] != b'SCE\0':
		sys.exit(f'{path} is not a SELF')
	appinfo_offset, = struct.unpack_from('<Q', header, 0x38)
	segment_offset, = struct.unpack_from('<Q', header, 0x58)
	compression, = struct.unpack_from('<Q', header, segment_offset + 0x10)
	return struct.unpack_from('<QIIQ', header, appinfo_offset), compression == COMPRESSION_ZLIB


def control_infos(elf):
	infos = [(4, struct.pack('<20s32s4xQ', CONTROL4_CONSTANT, hashlib.sha256(elf).digest(), 0)),
		 (6, struct.pack('<I252x', 1)),
		 (7, bytes(0x40))]
	out = b''
	for i, (kind, body) in enumerate(infos):
		out += struct.pack('<IIQ', kind, 0x10 + len(body), int(i + 1 < len(infos))) + body
	return out


def fself(elf, info, compress):
	if elf[:4] != b'\x7fELF' or elf[4] != 1 or elf[5] != 1:
		sys.exit('not a 32-bit little-endian ELF')
	phoff, shoff = struct.unpack_from('<II', elf, 0x1c)
	phentsize, phnum, _, shnum = struct.unpack_from('<HHHH', elf, 0x2a)
	phdrs = [struct.unpack_from('<8I', elf, phoff + i * phentsize) for i in range(phnum)]
	elf_header = elf[:0x34]

	appinfo_offset = 0x80
	elf_offset = appinfo_offset + 0x20
	phdr_offset = align(elf_offset + len(elf_header), 0x10)
	segment_offset = align(phdr_offset + phnum * 0x20, 0x10)
	version_offset = segment_offset + phnum * 0x20
	control_offset = version_offset + 0x10
	controls = control_infos(elf)

	segments = [None] * phnum
	if not compress:
		body = bytearray(elf)
		for i, p in enumerate(phdrs):
			segments[i] = (HEADER_SIZE + p[1], p[4], COMPRESSION_NONE)
		shdr_offset = HEADER_SIZE + shoff if shnum else 0
	else:
		# in file order, each segment keeps the gap before it that it has in the ELF, and its
		# offset modulo 16
		start = min((p[1] for p in phdrs if p[4]), default=len(elf))
		body = bytearray(elf[:start])
		end = elf_end = 0
		for i in sorted(range(phnum), key=lambda i: phdrs[i][1]):
			p = phdrs[i]
			data = zlib.compress(elf[p[1]:p[1] + p[4]]) if p[4] else b''
			offset = end + p[1] - elf_end
			offset += (p[1] - offset) % 0x10
			body += bytes(max(offset - len(body), 0))
			body[offset:offset + len(data)] = data
			segments[i] = (HEADER_SIZE + offset, len(data), COMPRESSION_ZLIB)
			end, elf_end = offset + len(data), p[1] + p[4]
		shdr_offset = 0

	header = bytearray(HEADER_SIZE)
	struct.pack_into('<4sIHHIQQQQ', header, 0, b'SCE\0', 3, ATTRIBUTE_FSELF, CATEGORY_SELF,
			 EXT_HEADER_SIZE, HEADER_SIZE, len(elf), HEADER_SIZE + len(body), 0)
	struct.pack_into('<10Q', header, 0x30, 4, appinfo_offset, elf_offset, phdr_offset, shdr_offset,
			 segment_offset, version_offset, control_offset, len(controls), 0)
	struct.pack_into('<QIIQ', header, appinfo_offset, *info)
	header[elf_offset:elf_offset + len(elf_header)] = elf_header
	for i, p in enumerate(phdrs):
		struct.pack_into('<8I', header, phdr_offset + i * 0x20, *p)
	for i, (offset, size, compression) in enumerate(segments):
		struct.pack_into('<4Q', header, segment_offset + i * 0x20, offset, size, compression,
				 ENCRYPTION_NONE)
	struct.pack_into('<4I', header, version_offset, 1, 0, 0x10, 0)
	header[control_offset:control_offset + len(controls)] = controls
	if control_offset + len(controls) > EXT_HEADER_SIZE:
		sys.exit('the header does not fit (too many segments)')
	return bytes(header + body)


def main():
	ap = argparse.ArgumentParser(description='Pack an SCE executable (.velf) as a fake SELF.')
	ap.add_argument('input')
	ap.add_argument('output')
	ap.add_argument('--like', metavar='SELF',
			help="the app info and compression of one of Sony's SELFs")
	ap.add_argument('--auth-id', type=lambda s: int(s, 0))
	ap.add_argument('--self-type', type=lambda s: int(s, 0))
	ap.add_argument('--app-version', type=lambda s: int(s, 0))
	ap.add_argument('--compress', action=argparse.BooleanOptionalAction,
			help='deflate each segment')
	a = ap.parse_args()
	(auth_id, vendor_id, self_type, version), compress = (like(a.like) if a.like else
							     ((AUTH_ID, 0, SELF_TYPE, VERSION), False))
	info = (auth_id if a.auth_id is None else a.auth_id, vendor_id,
		self_type if a.self_type is None else a.self_type,
		version if a.app_version is None else a.app_version)
	compress = compress if a.compress is None else a.compress
	with open(a.input, 'rb') as f:
		elf = f.read()
	with open(a.output, 'wb') as f:
		f.write(fself(elf, info, compress))


if __name__ == '__main__':
	main()
