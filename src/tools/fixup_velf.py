#!/usr/bin/env python3
# Patch fields of a vita-elf-create output that its config cannot express.
#
#   fixup_velf.py file.velf --nid 0xNID [--attr N] [--import LIBNID:VER:ATTR ...]
#
# --nid     module NID in SceModuleInfo (vita-elf-create leaves it 0)
# --attr    module attributes (vita-elf-create writes 0)
# --import  version/attributes of the import record for library LIBNID
# --export  attributes of the export record for library LIBNID (LIBNID:ATTR)
# --nid0    splice the extra NID 0 variables of the generated nid0.S table into the
#           module's own export record (see cmake/psp2.cmake MODULE_NID0_VARS)

import argparse
import struct

MI_ATTR, MI_EXPORT_TOP, MI_IMPORT_TOP, MI_NID = 0x00, 0x24, 0x2c, 0x34
EXP_NVARS, EXP_NID, EXP_NIDS, EXP_ENTRIES = 0x08, 0x10, 0x18, 0x1c
NID0_ROOM = 8


def find_imports(d, seg_off, mi, nids):
	"""vita-elf-create leaves the import range 0 in modules without entry points;
	the 0x34-byte import records follow the export data, find them by NID."""
	_, exp_end = struct.unpack_from("<II", d, mi + MI_EXPORT_TOP)
	for off in range(seg_off + exp_end, len(d) - 0x34, 4):
		if d[off] != 0x34 or d[off + 1] != 0:
			continue
		seen, o = set(), off
		while o + 0x34 <= len(d) and d[o] == 0x34:
			seen.add(struct.unpack_from("<I", d, o + 16)[0])
			o += 0x34
		if seen == nids:
			return off - seg_off, o - seg_off
	raise SystemExit("fixup_velf: import records not found")


class Relocs:
	"""Format 0 SCE relocations (all vita-elf-create emits): 12 bytes,
	fmt:4 symseg:4 code:8 patchseg:4 code2:8 dist2:4 | addend | offset."""

	def __init__(self, d, phdrs):
		self.d, self.phdrs = d, phdrs
		ph = next(p for p in phdrs if p[0] == 0x60000000)
		self.off, self.size = ph[1], ph[2]

	def find(self, va):
		"""Offset of the relocation patching va."""
		for o in range(self.off, self.off + self.size, 12):
			w0, _, off = struct.unpack_from("<III", self.d, o)
			if w0 & 0xf:
				raise SystemExit("fixup_velf: relocation format %d" % (w0 & 0xf))
			if self.phdrs[w0 >> 16 & 0xf][3] + off == va:
				return o
		raise SystemExit("fixup_velf: no relocation at %#x" % va)

	def move(self, old, new):
		o = self.find(old)
		w0, = struct.unpack_from("<I", self.d, o)
		struct.pack_into("<I", self.d, o + 8, new - self.phdrs[w0 >> 16 & 0xf][3])

	def retarget(self, va, value):
		"""Point the absolute pointer at va (patched by a relocation) at value."""
		o = self.find(va)
		w0, = struct.unpack_from("<I", self.d, o)
		struct.pack_into("<I", self.d, o + 4, value - self.phdrs[w0 >> 4 & 0xf][3])


def splice_nid0(d, phdrs, seg_off, seg_va, mi):
	"""Move the NID 0 record's tables into the nid0.S table, ahead of its extras."""
	def off(va):
		return seg_off + va - seg_va
	at = d.find(b"NID0", seg_off)
	if at < 0 or at & 3:
		raise SystemExit("fixup_velf: nid0 table not found")
	extra, = struct.unpack_from("<I", d, at + 4)
	nids_va = seg_va + at + 8 - seg_off
	ents_va = nids_va + 4 * (NID0_ROOM + extra)
	top, end = struct.unpack_from("<II", d, mi + MI_EXPORT_TOP)
	rec = next(seg_off + o for o in range(top, end, 0x20)
		   if struct.unpack_from("<I", d, seg_off + o + EXP_NID)[0] == 0)
	nf, nv, nt = struct.unpack_from("<HHH", d, rec + 6)
	n = nf + nv + nt
	if n > NID0_ROOM or nt:
		raise SystemExit("fixup_velf: NID 0 record too large")
	old_nids, old_ents = struct.unpack_from("<II", d, rec + EXP_NIDS)
	new_nids = nids_va + 4 * (NID0_ROOM - n)
	new_ents = ents_va + 4 * (NID0_ROOM - n)
	r = Relocs(d, phdrs)
	for i in range(n):
		d[off(new_nids) + 4 * i:off(new_nids) + 4 * i + 4] = d[off(old_nids) + 4 * i:off(old_nids) + 4 * i + 4]
		d[off(new_ents) + 4 * i:off(new_ents) + 4 * i + 4] = d[off(old_ents) + 4 * i:off(old_ents) + 4 * i + 4]
		r.move(old_ents + 4 * i, new_ents + 4 * i)
	struct.pack_into("<H", d, rec + EXP_NVARS, nv + extra)
	struct.pack_into("<II", d, rec + EXP_NIDS, new_nids, new_ents)
	r.retarget(rec - seg_off + seg_va + EXP_NIDS, new_nids)
	r.retarget(rec - seg_off + seg_va + EXP_ENTRIES, new_ents)
	d[at:at + 8] = bytes(8)


def main():
	ap = argparse.ArgumentParser()
	ap.add_argument("velf")
	ap.add_argument("--nid", type=lambda s: int(s, 0), required=True)
	ap.add_argument("--attr", type=lambda s: int(s, 0))
	ap.add_argument("--import", dest="imports", nargs="*", default=[])
	ap.add_argument("--export", dest="exports", nargs="*", default=[])
	ap.add_argument("--nid0", action="store_true")
	a = ap.parse_args()

	with open(a.velf, "r+b") as f:
		d = bytearray(f.read())
		entry, phoff = struct.unpack_from("<II", d, 0x18)
		phnum, = struct.unpack_from("<H", d, 0x2c)
		# (type, offset, filesz, vaddr) per program header
		phdrs = []
		for i in range(phnum):
			p_type, p_offset, p_vaddr, _, p_filesz = struct.unpack_from("<5I", d, phoff + 32 * i)
			phdrs.append((p_type, p_offset, p_filesz, p_vaddr))
		seg_off, = struct.unpack_from("<I", d, phoff + 4)
		seg_va, = struct.unpack_from("<I", d, phoff + 8)
		mi = seg_off + (entry & 0x3fffffff)
		struct.pack_into("<I", d, mi + MI_NID, a.nid)
		if a.attr is not None:
			struct.pack_into("<H", d, mi + MI_ATTR, a.attr)

		want = {}
		for spec in a.imports:
			lib, ver, attr = (int(x, 0) for x in spec.split(":"))
			want[lib] = (ver, attr)
		top, end = struct.unpack_from("<II", d, mi + MI_IMPORT_TOP)
		if top == 0 and want:
			top, end = find_imports(d, seg_off, mi, set(want))
			struct.pack_into("<II", d, mi + MI_IMPORT_TOP, top, end)
		off = seg_off + top
		while off < seg_off + end:
			size = d[off]
			nid, = struct.unpack_from("<I", d, off + (12 if size == 0x24 else 16))
			if nid in want:
				struct.pack_into("<HH", d, off + 2, *want[nid])
			off += size
		attrs = dict((int(x, 0) for x in spec.split(":")) for spec in a.exports)
		top, end = struct.unpack_from("<II", d, mi + MI_EXPORT_TOP)
		for off in range(seg_off + top, seg_off + end, 0x20):
			nid, = struct.unpack_from("<I", d, off + 0x10)
			if nid in attrs:
				struct.pack_into("<H", d, off + 4, attrs[nid])
		if a.nid0:
			splice_nid0(d, phdrs, seg_off, seg_va, mi)
		f.seek(0)
		f.write(d)


if __name__ == "__main__":
	main()
