#!/usr/bin/env python3
# Sony's module layout: the SCE tables as Sony's linker lays them out, and the
# linked ELF turned into an SCE relocatable executable (ET_SCE_RELEXEC).
#
#   mkprx.py tables -o scetables.S --exports exports.yml --nids kernel.yml [...]
#   mkprx.py elf module.elf module.velf
#
# tables: assembly for the import stubs, SceModuleInfo, the export and import
#   records (0x20 / 0x24 bytes) and their NID, entry, name and stub tables, and
#   module_sdk_version. cmake/prx.ld places the sections in Sony's order:
#
#     .text  .sceStub.text  .sceModuleInfo.rodata  .sceLib.ent  .sceLib.stub
#     .sceExport.rodata  .sceImport.rodata  .sceFNID.rodata  .sceVNID.rodata
#     .sceFStub.rodata  .sceVStub.rodata  .rodata  .ARM.exidx  .ARM.extab
#     .sceRefs.rodata | .data  .bss
#
#   Imported functions get 16-byte ARM stubs (mvn r0, #0; bx lr; nop; 0) with an
#   empty unwind entry each, in the original's import order. Imported variables
#   are absolute symbols at 0; the loader patches their references from the
#   .sceRefs table.
#
# elf: the module ELF linked with -q (prx.ld) becomes the SCE executable: the
#   loaded segments as linked, the .sceRefs table of every imported variable
#   appended to the text segment, and the relocations of every absolute
#   reference as format 0 SCE relocations (PT_SCE_RELA).

import argparse
import struct
import sys

BASE = 0x81000000
NID_START, NID_STOP, NID_EXIT, NID_BOOTSTART = 0x935CD196, 0x79F8E492, 0x913482A9, 0x5C424D40
NID_SDK_VERSION, NID_MODULE_INFO = 0x936C8A78, 0x6C2224BA
MAIN = (("start", NID_START), ("stop", NID_STOP), ("bootstart", NID_BOOTSTART),
	("exit", NID_EXIT))


# ---------------------------------------------------------------- input files

def parse_yaml(path):
	"""The subset of YAML exports.yml uses: nested maps and lists of one-key maps."""
	lines = []
	for raw in open(path):
		line = raw.split("#", 1)[0].rstrip()
		if line.strip():
			lines.append((len(line) - len(line.lstrip()), line.strip()))

	def block(i, indent):
		if lines[i][1].startswith("- "):
			out = []
			while i < len(lines) and lines[i][0] == indent and lines[i][1].startswith("- "):
				key, _, val = lines[i][1][2:].partition(":")
				out.append((key.strip(), val.strip()))
				i += 1
			return out, i
		out = {}
		while i < len(lines) and lines[i][0] == indent:
			key, _, val = lines[i][1].partition(":")
			i += 1
			if val.strip():
				out[key.strip()] = val.strip()
			elif i < len(lines) and lines[i][0] > indent:
				out[key.strip()], i = block(i, lines[i][0])
			else:
				out[key.strip()] = {}
		return out, i

	return block(0, 0)[0]


def parse_nids(paths):
	"""(library nid, nid) -> (library, symbol) from vita-libs-gen-2 style databases."""
	out, names = {}, {}
	for path in paths:
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
				names.setdefault(lib_nid, lib)
			elif indent == 8 and key in ("functions", "variables"):
				kind = key[0]
			elif indent == 10 and kind:
				out.setdefault((lib_nid, int(val, 16)), key)
	return out, names


# ---------------------------------------------------------------- tables

class Asm:
	def __init__(self):
		self.lines = []

	def __call__(self, *lines):
		self.lines += lines

	def section(self, name, flags="a", kind="%progbits"):
		self(f"\t.section {name}, \"{flags}\", {kind}")

	def string(self, label, s):
		self(f"{label}:", f"\t.asciz \"{s}\"", "\t.balign 4")

	def words(self, values):
		for v in values:
			self(f"\t.word {v}")


def hash_bits(e):
	"""An export record's hashinfo: 2^(hashinfo + 2) index buckets over its function
	NIDs, the largest power of 4 not above their count, from 16 on."""
	n, bits = len(e["funcs"]), 0
	while n >= 1 << (bits + 4):
		bits += 2
	return bits


def tables(a):
	y = parse_yaml(a.exports)
	(mod_name, mod), = y.items()
	symbols, libnames = parse_nids(a.nids)
	version = mod.get("version", {})
	main = mod.get("main", {})
	nid0_vars = [(int(n, 0), s) for n, s in (v.split(":") for v in a.nid0)]
	export_attr = dict((int(l, 0), int(t, 0)) for l, t in (v.split(":") for v in a.export_attr))
	export_aux = dict((int(l, 0), int(t, 0)) for l, t in (v.split(":") for v in a.export_aux))

	# imports: library order and versions, then every NID in the original's order
	libs = []
	for spec in a.imports:
		nid, ver, attr = (int(x, 0) for x in spec.split(":"))
		libs.append(dict(nid=nid, ver=ver, attr=attr, funcs=[], vars=[]))
	by_nid = {l["nid"]: l for l in libs}
	for spec in a.import_nids:
		lib, kind, nid = spec.split(":")
		lib, nid = int(lib, 0), int(nid, 0)
		sym = symbols.get((lib, nid))
		if sym is None:
			raise SystemExit(f"mkprx: no symbol for import {lib:#010x}:{nid:#010x}")
		by_nid[lib]["funcs" if kind == "F" else "vars"].append((nid, sym))

	s = Asm()
	s(f"@ generated by mkprx.py: SCE tables of {mod_name}", "\t.syntax unified", "\t.arch armv7-a", "")

	# import stubs
	s.section(".sceStub.text", "ax")
	s("\t.arm", "\t.balign 4")
	for l in libs:
		for nid, sym in l["funcs"]:
			s(f"\t.globl {sym}", f"\t.type {sym}, %function", f"{sym}:", "\t.fnstart",
			  "\tmvn r0, #0", "\tbx lr", "\tmov r0, r0", "\t.word 0", "\t.fnend",
			  f"\t.size {sym}, 16")
	for l in libs:
		for nid, sym in l["vars"]:
			s(f"\t.globl {sym}", f"\t.type {sym}, %object", f"\t.set {sym}, 0")
	# unwind entries name their personality routine; the module has no unwinder
	for pr in ("__aeabi_unwind_cpp_pr0", "__aeabi_unwind_cpp_pr1", "__aeabi_unwind_cpp_pr2"):
		s(f"\t.weak {pr}", f"\t.set {pr}, 0")
	s("")

	# module info: segment offsets (no relocations)
	def off(sym):
		return f"{sym} - {BASE:#x}"
	starts = dict((k, main[k]) for k, _ in MAIN if k in main)
	s.section(".sceModuleInfo.rodata")
	s("\t.balign 16", "\t.globl module_info", "\t.type module_info, %object", "module_info:")
	attr = int(a.attr if a.attr is not None else mod.get("attributes", "0"), 0)
	s(f"\t.short {attr:#x}",
	  f"\t.byte {version.get('minor', '1')}, {version.get('major', '1')}",	# version[2]: minor, major
	  f"\t.ascii \"{mod_name}\"", f"\t.space {27 - len(mod_name)}", "\t.byte 6")
	s.words(["0", off("__sce_libent_top"), off("__sce_libent_end"), off("__sce_libstub_top"),
		 off("__sce_libstub_end"), f"{int(a.nid, 0):#010x}", "0", "0", "0",
		 off(starts["start"]) if "start" in starts else "0xffffffff",
		 off(starts["stop"]) if "stop" in starts else "0xffffffff",
		 off("__exidx_start"), off("__exidx_end"), off("__extab_start"), off("__extab_end")])
	s("\t.size module_info, . - module_info", "")

	# exports: the module's own record (NID 0) first
	exports = [dict(nid=0, ver=0, attr=0x8000, name=None,
			funcs=[(n, starts[k]) for k, n in MAIN if k in starts],
			vars=([(NID_SDK_VERSION, "module_sdk_version")] if a.sdk_version is not None else [])
			     + nid0_vars + [(NID_MODULE_INFO, "module_info")])]
	for name, lib in mod.get("libraries", {}).items():
		nid = int(lib["nid"], 0)
		attr = 0x4001 if lib.get("syscall") == "true" else 0x0001
		exports.append(dict(nid=nid, ver=int(lib.get("version", "1"), 0),
				    attr=export_attr.get(nid, attr), name=name,
				    funcs=[(int(n, 0), f) for f, n in lib.get("functions", [])],
				    vars=[(int(n, 0), v) for v, n in lib.get("variables", [])]))
	for nid, sym in nid0_vars:			# placeholders until the module defines them
		s(f"\t.weak {sym}")
	if a.nid0_var_order:
		order = [int(n, 0) for n in a.nid0_var_order]
		own = exports[0]["vars"]
		if sorted(order) != sorted(n for n, _ in own):
			raise SystemExit("mkprx: --nid0-var-order does not match the module's variables")
		own.sort(key=lambda v: order.index(v[0]))
	if a.export_order:
		order = [int(n, 0) for n in a.export_order]
		if sorted(order) != sorted(e["nid"] for e in exports):
			raise SystemExit("mkprx: --export-order does not match the exports")
		exports.sort(key=lambda e: order.index(e["nid"]))
	s.section(".sceLib.ent")
	s("\t.balign 4")
	for i, e in enumerate(exports):
		s(f"\t.byte 0x20, {export_aux.get(e['nid'], 0)}", f"\t.short {e['ver']:#x}, {e['attr']:#x}",
		  f"\t.short {len(e['funcs'])}, {len(e['vars'])}, 0", f"\t.byte {hash_bits(e)}, 0, 0, 0",
		  f"\t.word {e['nid']:#010x}", f"\t.word {'.Lexp_name%d' % i if e['name'] else 0}",
		  f"\t.word .Lexp_nids{i}, .Lexp_ents{i}")
	s("")
	s.section(".sceExport.rodata")
	s("\t.balign 4")
	for i, e in enumerate(exports):
		if e["name"]:
			s.string(f".Lexp_name{i}", e["name"])
		s(f".Lexp_nids{i}:")
		s.words(f"{n:#010x}" for n, _ in e["funcs"] + e["vars"])
		if hash_bits(e):
			# lookup index: per bucket (a NID's top bits), the number of function NIDs below it
			nids = [n for n, _ in e["funcs"]]
			bits = hash_bits(e) + 2
			s(f"\t.short {', '.join(str(sum(n >> 32 - bits < k for n in nids)) for k in range(1 << bits))}")
		s(f".Lexp_ents{i}:")
		s.words(sym for _, sym in e["funcs"] + e["vars"])
	s("")

	# imports: 0x24-byte records, or 0x34 with reserved words and TLS tables (none)
	s.section(".sceLib.stub")
	s("\t.balign 4")
	size = int(a.import_format, 0)
	for i, l in enumerate(libs):
		f = f".Lfnid{i}, .Lfstub{i}" if l["funcs"] else "0, 0"
		v = f".Lvnid{i}, .Lvstub{i}" if l["vars"] else "0, 0"
		s(f"\t.short {size:#x}, {l['ver']:#x}, {l['attr']:#x}, {len(l['funcs'])}, {len(l['vars'])}, 0")
		if size == 0x34:
			s(f"\t.word 0, {l['nid']:#010x}, .Limp_name{i}, 0, {f}, {v}, 0, 0")
		else:
			s(f"\t.word {l['nid']:#010x}, .Limp_name{i}, {f}, {v}")
	s.section(".sceImport.rodata")
	s("\t.balign 4")
	for i, l in enumerate(libs):
		if l["nid"] not in libnames:
			raise SystemExit(f"mkprx: no library {l['nid']:#010x} in the NID database")
		s.string(f".Limp_name{i}", libnames[l["nid"]])
	for sec, key, label, value in ((".sceFNID.rodata", "funcs", "fnid", 0), (".sceVNID.rodata", "vars", "vnid", 0),
				       (".sceFStub.rodata", "funcs", "fstub", 1), (".sceVStub.rodata", "vars", "vstub", 1)):
		s.section(sec)
		s("\t.balign 4")
		for i, l in enumerate(libs):
			if l[key]:
				s(f".L{label}{i}:")
				s.words(f"{nid:#010x}" if not value else sym if key == "funcs" else "0"
					for nid, sym in l[key])
	# the variable of each .sceVStub.rodata entry, for the .sceRefs table (mkprx.py elf)
	s.section(".sce_vstub", "", "%progbits @")
	for l in libs:
		for nid, sym in l["vars"]:
			s(f"\t.asciz \"{sym}\"")
	s("")

	if a.sdk_version is not None:
		s.section(".rodata.sceSdkVersion")
		s("\t.balign 4", "\t.globl module_sdk_version", "\t.type module_sdk_version, %object",
		  "module_sdk_version:", f"\t.word {int(a.sdk_version, 0):#010x}",
		  "\t.size module_sdk_version, 4")
	s.section(".sceRefs.rodata")
	s("\t.balign 4", "")
	with open(a.o, "w") as f:
		f.write("\n".join(s.lines) + "\n")


# ---------------------------------------------------------------- SCE executable

R_ARM_ABS32, R_ARM_TARGET1, R_ARM_PREL31 = 2, 38, 42
R_ARM_THM_MOVW_ABS_NC, R_ARM_THM_MOVT_ABS, R_ARM_MOVW_ABS_NC, R_ARM_MOVT_ABS = 47, 48, 43, 44
PC_RELATIVE = {3, 10, 28, 29, 30, 42, 51}	# REL32 THM_CALL CALL JUMP24 THM_JUMP24 PREL31 THM_JUMP19
MOVW = (R_ARM_THM_MOVW_ABS_NC, R_ARM_MOVW_ABS_NC)
MOVT = (R_ARM_THM_MOVT_ABS, R_ARM_MOVT_ABS)
THUMB = (R_ARM_THM_MOVW_ABS_NC, R_ARM_THM_MOVT_ABS)
SHN_UNDEF, SHN_ABS = 0, 0xfff1
MI_EXIDX_END = 0x50


class Elf:
	"""The linked module: its sections, symbols, relocations (-q) and loaded segments."""

	def __init__(self, path):
		d = self.d = bytes(open(path, "rb").read())
		phoff, shoff = struct.unpack_from("<II", d, 0x1c)
		phnum, = struct.unpack_from("<H", d, 0x2c)
		shnum, shstrndx = struct.unpack_from("<HH", d, 0x30)
		phdrs = [struct.unpack_from("<8I", d, phoff + 32 * i) for i in range(phnum)]
		self.shdrs = [struct.unpack_from("<10I", d, shoff + 40 * i) for i in range(shnum)]
		strs = self.shdrs[shstrndx][4]
		self.names = [self.cstr(strs + sh[0]) for sh in self.shdrs]
		sh = self.shdrs[self.names.index(".symtab")]
		symstr = self.shdrs[sh[6]][4]
		self.syms = []
		for o in range(sh[4], sh[4] + sh[5], 16):
			name, value, size, info, other, shndx = struct.unpack_from("<IIIBBH", d, o)
			self.syms.append((self.cstr(symstr + name), value, shndx, info >> 4))
		self.symbol = {}
		for name, value, shndx, bind in self.syms:
			if name and shndx != SHN_UNDEF:
				self.symbol.setdefault(name, value)
		# segments as [base, bytes, bss size, flags, align]
		self.segs = []
		for p in phdrs:
			if p[0] != 1:
				continue
			align = max([h[8] for h in self.shdrs if h[2] & 2 and p[2] <= h[3] < p[2] + max(p[5], 1)]
				    + [0x10])
			self.segs.append([p[2], bytearray(d[p[1]:p[1] + p[4]]), p[5] - p[4], p[6], align])

	def cstr(self, o):
		return self.d[o:self.d.index(b"\0", o)].decode()

	def contents(self, name):
		if name not in self.names:
			return b""
		sh = self.shdrs[self.names.index(name)]
		return self.d[sh[4]:sh[4] + sh[5]]

	def relocs(self):
		"""(section, place, type, symbol) of every allocated section's REL entries."""
		for sh in self.shdrs:
			if sh[1] != 9 or not self.shdrs[sh[7]][2] & 2:
				continue
			target = self.shdrs[sh[7]]
			for o in range(sh[4], sh[4] + sh[5], 8):
				place, info = struct.unpack_from("<II", self.d, o)
				if place < target[3]:		# ld writes the unwind tables' relative to the section
					place += target[3]
				yield self.names[sh[7]], place, info & 0xff, self.syms[info >> 8]

	# memory of the loaded segments
	def seg(self, va):
		for i, (base, data, bss, _, _) in enumerate(self.segs):
			if base <= va < base + len(data) + bss or va == base + len(data) + bss:
				return i
		return None

	def word(self, va):
		base, data = self.segs[self.seg(va)][:2]
		return struct.unpack_from("<I", data, va - base)[0]

	def put(self, va, value):
		base, data = self.segs[self.seg(va)][:2]
		struct.pack_into("<I", data, va - base, value & 0xffffffff)

	def imm16(self, code, va):
		"""(immediate, destination register) of a MOVW/MOVT."""
		w = self.word(va)
		if code in THUMB:
			h1, h2 = w & 0xffff, w >> 16
			return (h1 & 0xf) << 12 | (h1 >> 10 & 1) << 11 | (h2 >> 12 & 7) << 8 | (h2 & 0xff), h2 >> 8 & 0xf
		return (w >> 16 & 0xf) << 12 | (w & 0xfff), w >> 12 & 0xf

	def set_imm16(self, code, va, imm):
		w = self.word(va)
		if code in THUMB:
			h1, h2 = w & 0xffff, w >> 16
			h1 = (h1 & 0xfbf0) | (imm >> 12 & 0xf) | (imm >> 11 & 1) << 10
			h2 = (h2 & 0x8f00) | (imm >> 8 & 7) << 12 | (imm & 0xff)
			self.put(va, h1 | h2 << 16)
		else:
			self.put(va, (w & 0xfff0f000) | (imm >> 12 & 0xf) << 16 | (imm & 0xfff))


class Reference:
	"""An absolute reference: patched by the loader (SCE relocation) or, for an
	imported variable, listed in .sceRefs."""

	def __init__(self, code, place, target, var=None, anchor=None):
		self.code, self.place, self.target, self.var = code, place, target, var
		self.stub = None
		# an address in the target's segment: the symbol, when the target lies outside
		self.anchor = target if anchor is None else anchor

	def store(self, e):
		if self.var:				# the loader writes the import's address
			if self.code in MOVW + MOVT:
				e.set_imm16(self.code, self.place, 0)
			else:
				e.put(self.place, 0)
		elif self.code in MOVW:
			e.set_imm16(self.code, self.place, self.target & 0xffff)
		elif self.code in MOVT:
			e.set_imm16(self.code, self.place, self.target >> 16 & 0xffff)
		else:
			e.put(self.place, self.target)


def prel31(e, va):
	w = e.word(va) & 0x7fffffff
	return va + w - ((w & 0x40000000) << 1)


def references(e):
	"""Every absolute reference of the module; PC-relative ones must stay in their segment."""
	refs, movw, movt = [], {}, []
	for sec, place, code, (name, value, shndx, bind) in sorted(e.relocs(), key=lambda r: r[1]):
		if sec in (".sceModuleInfo.rodata", ".ARM.exidx", ".ARM.extab"):
			continue			# segment offsets and PREL31 (handled by move())
		if shndx == SHN_UNDEF:
			if bind != 2 or value:
				raise SystemExit(f"mkprx: undefined {name} at {place:#x}")
			continue			# undefined weak: 0
		var = name if shndx == SHN_ABS and name else None
		if code in PC_RELATIVE:
			if var or e.seg(value) != e.seg(place):
				raise SystemExit(f"mkprx: PC-relative reference to {name} at {place:#x} across segments")
			continue
		if var:
			refs.append(Reference(code, place, 0, var))
		elif code in (R_ARM_ABS32, R_ARM_TARGET1):
			target = e.word(place)
			refs.append(Reference(code, place, target, anchor=target if e.seg(target) is not None else value))
		elif code in MOVW:
			lo, rd = e.imm16(code, place)
			r = Reference(code, place, value + ((lo - value + 0x8000) & 0xffff) - 0x8000, anchor=value)
			movw.setdefault((sec, name, value, rd), []).append(r)
			refs.append(r)
		elif code in MOVT:
			movt.append((sec, place, code, name, value))
		else:
			raise SystemExit(f"mkprx: relocation type {code} at {place:#x} ({name})")
	for sec, place, code, name, value in movt:
		hi, rd = e.imm16(code, place)
		# the paired MOVW (same symbol and register) gives the low half
		pair = [r.target for r in movw.get((sec, name, value, rd), [])
			if abs(r.place - place) < 0x100 and r.target >> 16 == hi]
		refs.append(Reference(code, place, pair[0] if pair else hi << 16 | (value & 0xffff), anchor=value))
	return refs


def move(e, refs, lo, hi, delta, prel=()):
	"""Move the bytes [lo, hi) by delta: retarget the references into them, move the
	references in them, and keep the PREL31 words (places) that cross the boundary."""
	inside = lambda va: lo <= va < hi
	for place in prel:
		target = prel31(e, place)
		off = target + (delta if inside(target) else 0) - place - (delta if inside(place) else 0)
		e.put(place, (e.word(place) & 0x80000000) | (off & 0x7fffffff))
	for r in refs:
		if inside(r.anchor) and not r.var:
			r.target += delta
			r.anchor += delta
	for r in refs:
		if inside(r.place):
			r.place += delta


def elf(a):
	e = Elf(a.input)
	if len(e.segs) not in (1, 2):
		raise SystemExit("mkprx: expected one or two loaded segments")
	sym = e.symbol
	text = e.segs[0]
	refs = references(e)
	mi = sym["module_info"]

	# ld ends the unwind table with an EXIDX_CANTUNWIND entry for the end of the text;
	# Sony's linker does not: drop it, moving what follows (.ARM.extab) down
	start, end = sym["__exidx_start"], sym["__exidx_end"]
	if end - start >= 8 and e.word(end - 4) == 1 and prel31(e, end - 8) == sym["__sce_text_end"]:
		# the PREL31 words that cross the moved range: exidx entries' pointers into
		# .ARM.extab, and .ARM.extab's own (personality routines)
		prel = [p + 4 for p in range(start, end - 8, 8) if e.word(p + 4) >> 31 == 0 and e.word(p + 4) != 1]
		prel += [place for sec, place, code, _ in e.relocs() if sec == ".ARM.extab" and code == R_ARM_PREL31]
		# .ARM.extab stays 16-byte aligned (prx.ld): what follows moves by delta
		extab = sym["__extab_start"]
		delta = ((end - 8 + 15) & ~15) - extab
		move(e, refs, extab, text[0] + len(text[1]), delta, prel)
		e.put(mi + MI_EXIDX_END, end - 8 - text[0])
		for field in (MI_EXIDX_END + 4, MI_EXIDX_END + 8):	# extab_top/end
			e.put(mi + field, e.word(mi + field) + delta)
		text[1][end - 8 - text[0]:extab - text[0]] = bytes(extab + delta - (end - 8))

	# .sceRefs (16-byte aligned, after .ARM.extab): the references the loader patches
	# with an import's address. Per imported variable, in .sceVStub.rodata order:
	# (count << 7 | 0x40), then (code << 8 | 1, offset in its segment) per reference in
	# address order.
	# (The 1 is segment << 4 | 1, the segment of the reference.) Then per imported
	# function whose address is taken, in stub order: the same, led
	# by (0xff << 8 | 1, stub offset), and the stub's last word points at the entry.
	stubs = (sym.get("__sce_stub_top", 0), sym["__sce_text_end"])
	tables = (sym["__sce_text_end"], sym["__sce_tables_end"])	# FStub entries point at stubs too
	for r in refs:
		if not r.var and stubs[0] <= r.target < stubs[1] and not tables[0] <= r.place < tables[1]:
			r.var, r.stub = r.target & ~15, r.target & ~15
	text[1] += bytes(-len(text[1]) % 16)
	top = text[0] + len(text[1])
	table = bytearray()

	def entry(key, lead=()):
		uses = sorted((r for r in refs if r.var == key), key=lambda r: r.place)
		table.extend(struct.pack("<I", (len(lead) + len(uses)) << 7 | 0x40))
		for code, place in list(lead) + [(r.code, r.place) for r in uses]:
			seg = e.seg(place)		# the data segment's move comes later: offsets hold
			table.extend(struct.pack("<II", code << 8 | seg << 4 | 1, place - e.segs[seg][0]))

	names = [n.decode() for n in e.contents(".sce_vstub").split(b"\0")[:-1]]
	vstub = sym.get("__sce_vstub_top", 0)
	for i, var in enumerate(names):
		refs.append(Reference(R_ARM_ABS32, vstub + 4 * i, top + len(table)))
		entry(var)
	for stub in sorted({r.var for r in refs if isinstance(r.var, int)}):
		refs.append(Reference(R_ARM_ABS32, stub + 12, top + len(table)))
		entry(stub, [(0xff, stub)])
	unlisted = {r.var for r in refs if isinstance(r.var, str)} - set(names)
	if unlisted:
		raise SystemExit(f"mkprx: imported variables without a stub entry: {sorted(unlisted)}")
	text[1] += table

	# Sony's data segment starts at the next 4 KiB after the text, .sceRefs included
	if len(e.segs) > 1:
		data = e.segs[1]
		base = (text[0] + len(text[1]) + 0xfff) & ~0xfff
		if base != data[0]:
			move(e, refs, data[0], data[0] + len(data[1]) + data[2], base - data[0])
			data[0] = base
	for r in refs:
		r.store(e)

	# SCE relocations, format 0: fmt:4 symseg:4 code:8 patchseg:4 | addend | offset
	rel = bytearray()
	for r in sorted((r for r in refs if not r.var), key=lambda r: (e.seg(r.place), r.place)):
		symseg, patchseg = e.seg(r.anchor), e.seg(r.place)
		if symseg is None:
			raise SystemExit(f"mkprx: reference at {r.place:#x} to {r.target:#x} outside the module")
		rel += struct.pack("<III", symseg << 4 | r.code << 8 | patchseg << 16,
				   (r.target - e.segs[symseg][0]) & 0xffffffff, r.place - e.segs[patchseg][0])

	phnum = len(e.segs) + 1
	out = bytearray(52 + 32 * phnum)
	phdrs = []
	for base, data, bss, flags, align in e.segs:
		while len(out) % align:
			out.append(0)
		phdrs.append((1, len(out), base, 0, len(data), len(data) + bss, flags, align))
		out += data
	while len(out) % 0x10:
		out.append(0)
	phdrs.append((0x60000000, len(out), 0, 0, len(rel), 0, 0, 0x10))
	out += rel
	struct.pack_into("<16sHHIIIIIHHHHHH", out, 0, b"\x7fELF\x01\x01\x01" + bytes(9), 0xfe04, 40, 1,
			 mi - text[0], 52, 0, 0x05000000, 52, 32, phnum, 40, 0, 0)
	for i, ph in enumerate(phdrs):
		struct.pack_into("<8I", out, 52 + 32 * i, *ph)
	open(a.output, "wb").write(out)


def main():
	ap = argparse.ArgumentParser()
	sub = ap.add_subparsers(dest="cmd", required=True)
	t = sub.add_parser("tables")
	t.add_argument("-o", required=True)
	t.add_argument("--exports", required=True)
	t.add_argument("--nids", nargs="+", required=True)
	t.add_argument("--nid", required=True)
	t.add_argument("--attr")
	t.add_argument("--sdk-version")
	t.add_argument("--import", dest="imports", nargs="*", default=[])
	t.add_argument("--import-nids", nargs="*", default=[])
	t.add_argument("--import-format", default="0x24")
	t.add_argument("--export-attr", nargs="*", default=[])
	t.add_argument("--export-order", nargs="*", default=[])
	t.add_argument("--export-aux", nargs="*", default=[])
	t.add_argument("--nid0", nargs="*", default=[])
	t.add_argument("--nid0-var-order", nargs="*", default=[])
	x = sub.add_parser("elf")
	x.add_argument("input")
	x.add_argument("output")
	a = ap.parse_args()
	tables(a) if a.cmd == "tables" else elf(a)


if __name__ == "__main__":
	main()
