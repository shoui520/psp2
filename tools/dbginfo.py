#!/usr/bin/env python3
# Fill in the hashes of SceKernelDebugInfo records in a linked module ELF, in place,
# from the non-allocated .sce_dbgsrc side table. See sys/internal/include/sysmem/debug.h.
#
#   dbginfo.py module.elf [link.map [link directory]]
#
# A link with --gc-sections can discard the strings a kept record's side row names
# (a release record has no pointers of its own to keep them). With the link's map
# (default: module.elf.map; its object paths are relative to the link directory, by
# default the map's), such a row's strings are read from the object that defined it.

import hashlib
import os
import struct
import sys


def sha32(s):
	return int.from_bytes(hashlib.sha1(s).digest()[:4], "big")


def sections(d):
	"""{name: (index, header)} and the header list of an ELF32 file."""
	shoff, = struct.unpack_from("<I", d, 0x20)
	shsz, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x2e)
	headers = [struct.unpack_from("<10I", d, shoff + i * shsz) for i in range(shnum)]
	names = headers[shstrndx][4]
	byname = {}
	for i, h in enumerate(headers):
		byname.setdefault(bytes(d[names + h[0]:d.index(b"\0", names + h[0])]), (i, h))
	return byname, headers


def object_rows(path):
	"""The side rows of a relocatable object: [(file, line, func)], in order."""
	d = open(path, "rb").read()
	byname, headers = sections(d)
	if b".debug_sce_dbgsrc" not in byname:
		return []
	index, side = byname[b".debug_sce_dbgsrc"]
	rel = next(h for h in headers if h[1] == 9 and h[7] == index)
	symtab = headers[rel[6]]
	targets = {}
	for o in range(rel[4], rel[4] + rel[5], 8):
		offset, info = struct.unpack_from("<II", d, o)
		value, shndx = struct.unpack_from("<I6xH", d, symtab[4] + (info >> 8) * 16 + 4)
		targets[offset] = (shndx, value)

	def string(offset):
		shndx, value = targets[offset]
		addend, = struct.unpack_from("<I", d, side[4] + offset)
		o = headers[shndx][4] + value + addend
		return bytes(d[o:d.index(b"\0", o)])

	return [(string(o + 4), struct.unpack_from("<I", d, side[4] + o + 8)[0], string(o + 12))
		for o in range(0, side[5], 16)]


def map_rows(path, base):
	"""{offset in the side table: (file, line, func)} from the link map's objects."""
	lines = open(path).read().splitlines()
	out = {}
	for i, l in enumerate(lines):
		words = l.split()
		if not words or words[0] != ".debug_sce_dbgsrc":
			continue
		rest = words[1:] or lines[i + 1].split()
		if len(rest) != 3:
			continue
		start, size, obj = int(rest[0], 16), int(rest[1], 16), rest[2]
		rows = object_rows(os.path.join(base, obj))
		if len(rows) * 16 != size:
			raise SystemExit(f"dbginfo: {obj}: side table does not match the map")
		for k, row in enumerate(rows):
			out[start + k * 16] = row
	return out


def main():
	path = sys.argv[1]
	map_path = sys.argv[2] if len(sys.argv) > 2 else path + ".map"
	base = sys.argv[3] if len(sys.argv) > 3 else os.path.dirname(map_path)
	with open(path, "r+b") as f:
		d = bytearray(f.read())
		phoff, = struct.unpack_from("<I", d, 0x1c)
		phsz, phnum = struct.unpack_from("<HH", d, 0x2a)
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

		byname, _ = sections(d)
		if b".sce_dbgsrc" in byname:
			_, h = byname[b".sce_dbgsrc"]
			off, size = h[4], h[5]
			fallback = None
			for o in range(off, off + size, 16):
				rec, fp, line, fn = struct.unpack_from("<4I", d, o)
				if rec == 0:
					# the record's section was discarded (--gc-sections)
					continue
				if fp and fn:
					file, func = cstr(fp), cstr(fn)
				else:
					if fallback is None:
						if not os.path.exists(map_path):
							raise SystemExit(f"dbginfo: {rec:#x}: strings discarded, no {map_path}")
						fallback = map_rows(map_path, base)
					file, line, func = fallback[o - off]
				struct.pack_into("<3I", d, foff(rec), sha32(file),
						 sha32(file + b":" + str(line).encode()), sha32(func))
		f.seek(0)
		f.write(d)


if __name__ == "__main__":
	main()
