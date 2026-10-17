#!/usr/bin/env python3
# Fill in the hashes of SceKernelDebugInfo records in a linked module ELF, in place,
# from the non-allocated .sce_dbgsrc side table. See sys/internal/include/sysmem/debug.h.
#
#   dbginfo.py module.elf

import hashlib
import struct
import sys


def sha32(s):
	return int.from_bytes(hashlib.sha1(s).digest()[:4], "big")


def main():
	path = sys.argv[1]
	with open(path, "r+b") as f:
		d = bytearray(f.read())
		phoff, shoff = struct.unpack_from("<II", d, 0x1c)
		phsz, phnum, shsz, shnum, shstrndx = struct.unpack_from("<HHHHH", d, 0x2a)
		segs = []
		for i in range(phnum):
			t, off, va, _, fsz = struct.unpack_from("<5I", d, phoff + i * phsz)
			if t == 1:
				segs.append((va, off, fsz))

		def foff(va):
			for v, off, n in segs:
				if v <= va < v + n:
					return off + va - v
			raise SystemExit(f"dbginfo: {va:#x} not in a loaded segment")

		def cstr(va):
			o = foff(va)
			return bytes(d[o:d.index(b"\0", o)])

		def shdr(i):
			return struct.unpack_from("<10I", d, shoff + i * shsz)

		names = shdr(shstrndx)[4]
		for i in range(shnum):
			name, _, _, _, off, size = shdr(i)[:6]
			if bytes(d[names + name:d.index(b"\0", names + name)]) != b".sce_dbgsrc":
				continue
			for o in range(off, off + size, 16):
				rec, fp, line, fn = struct.unpack_from("<4I", d, o)
				file, func = cstr(fp), cstr(fn)
				struct.pack_into("<3I", d, foff(rec), sha32(file),
						 sha32(file + b":" + str(line).encode()), sha32(func))
		f.seek(0)
		f.write(d)


if __name__ == "__main__":
	main()
