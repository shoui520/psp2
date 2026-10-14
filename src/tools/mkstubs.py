#!/usr/bin/env python3
# Build import stub archives from a vita-libs-gen-2 style NID database.
#
#   mkstubs.py nids.yml outdir
#
# Writes outdir/lib<Library>_<NID>_stub.a (libraries may share a name), one
# relocatable object per function or variable, byte-identical in content to what vita-libs-gen-2 + as produce
# (a 16-byte .vitalink.[fv]stubs.<Library> record), without one assembler run
# per symbol.

import os
import struct
import subprocess
import sys

AR = os.path.join(os.environ.get("VITASDK", "/usr/local/vitasdk"), "bin", "arm-vita-eabi-ar")

ARM_ATTRIBUTES = bytes.fromhex("411b000000616561626900011100000005374100060a074108010902")
NOP = 0xe320f000

SHT_PROGBITS, SHT_SYMTAB, SHT_STRTAB, SHT_ARM_ATTRIBUTES = 1, 2, 3, 0x70000003
SHF_ALLOC, SHF_EXECINSTR = 0x2, 0x4
STT_OBJECT, STT_FUNC, STT_SECTION = 1, 2, 3
STB_LOCAL, STB_GLOBAL = 0, 1


def parse_db(path):
	"""Yield (library, library nid, kind, symbol, nid); kind is 'f' or 'v'."""
	lib = lib_nid = kind = None
	for raw in open(path):
		line = raw.split("#", 1)[0].rstrip()
		if not line.strip():
			continue
		indent = len(line) - len(line.lstrip())
		key, _, val = line.strip().partition(":")
		val = val.strip()
		if indent == 6:
			lib, lib_nid, kind = key, None, None
		elif indent == 8 and key == "nid":
			lib_nid = int(val, 16)
		elif indent == 8 and key in ("functions", "variables"):
			kind = key[0]
		elif indent == 10 and kind:
			yield lib, lib_nid, kind, key, int(val, 16)


class Strtab:
	def __init__(self):
		self.data = bytearray(b"\0")

	def add(self, s):
		off = len(self.data)
		self.data += s.encode() + b"\0"
		return off


def stub_object(lib, lib_nid, kind, sym, nid):
	shstr, strtab = Strtab(), Strtab()
	secname = f".vitalink.{kind}stubs.{lib}"
	body = struct.pack("<4I", 0x10, lib_nid, nid, NOP if kind == "f" else 0)

	n_sec, n_attr, n_sym = shstr.add(secname), shstr.add(".ARM.attributes"), shstr.add(".symtab")
	n_str, n_shstr = shstr.add(".strtab"), shstr.add(".shstrtab")
	n_d, n_g = strtab.add("$d"), strtab.add(sym)

	symtab = bytearray(16)                                   # null symbol
	symtab += struct.pack("<IIIBBH", 0, 0, 0, STT_SECTION, 0, 1)
	symtab += struct.pack("<IIIBBH", n_d, 0, 0, 0, 0, 1)
	symtab += struct.pack("<IIIBBH", n_g, 0, 0,
			      (STB_GLOBAL << 4) | (STT_FUNC if kind == "f" else STT_OBJECT), 0, 1)

	out = bytearray(52)
	def place(data, align):
		while len(out) % align:
			out.append(0)
		off = len(out)
		out.extend(data)
		return off

	o_body = place(body, 16)
	o_attr = place(ARM_ATTRIBUTES, 1)
	o_sym = place(symtab, 4)
	o_str = place(strtab.data, 1)
	o_shstr = place(shstr.data, 1)
	sh = [
		(0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
		(n_sec, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR if kind == "f" else 0, 0, o_body, 16, 0, 0, 16, 0),
		(n_attr, SHT_ARM_ATTRIBUTES, 0, 0, o_attr, len(ARM_ATTRIBUTES), 0, 0, 1, 0),
		(n_sym, SHT_SYMTAB, 0, 0, o_sym, len(symtab), 4, 3, 4, 16),
		(n_str, SHT_STRTAB, 0, 0, o_str, len(strtab.data), 0, 0, 1, 0),
		(n_shstr, SHT_STRTAB, 0, 0, o_shstr, len(shstr.data), 0, 0, 1, 0),
	]
	shoff = place(b"", 4)
	for s in sh:
		out += struct.pack("<10I", *s)
	out[:52] = struct.pack("<16sHHIIIIIHHHHHH", b"\x7fELF\x01\x01\x01" + bytes(9), 1, 40, 1, 0, 0,
			       shoff, 0x05000000, 52, 0, 0, 40, len(sh), 5)
	return bytes(out)


def main():
	db, outdir = sys.argv[1], sys.argv[2]
	os.makedirs(outdir, exist_ok=True)
	libs = {}
	for lib, lib_nid, kind, sym, nid in parse_db(db):
		libs.setdefault((lib, lib_nid), []).append((f"{sym}.o", stub_object(lib, lib_nid, kind, sym, nid)))
	work = os.path.join(outdir, "obj")
	os.makedirs(work, exist_ok=True)
	for (lib, lib_nid), objs in libs.items():
		d = os.path.join(work, f"{lib}_{lib_nid:08X}")
		os.makedirs(d, exist_ok=True)
		names = []
		for name, data in objs:
			with open(os.path.join(d, name), "wb") as f:
				f.write(data)
			names.append(name)
		archive = os.path.abspath(os.path.join(outdir, f"lib{lib}_{lib_nid:08X}_stub.a"))
		if os.path.exists(archive):
			os.unlink(archive)
		subprocess.run([AR, "rcs", archive] + names, cwd=d, check=True)
	print(f"{len(libs)} stub archives")


if __name__ == "__main__":
	main()
