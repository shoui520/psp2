"""Minimal ELF32 LE section/symbol/REL reader for linked (raw, pre-velf) ELFs.

vitaelf deliberately ignores section headers (decrypted SELFs carry stale
ones); post-link unwind tooling needs them on toolchain output only.
"""
import struct


class Sections:
	def __init__(self, data):
		self.d = bytes(data)
		if self.d[:4] != b"\x7fELF" or self.d[4:6] != b"\x01\x01":
			raise ValueError("not an ELF32 LE file")
		shoff, = struct.unpack_from("<I", self.d, 32)
		shentsize, shnum, shstrndx = struct.unpack_from("<HHH", self.d, 46)
		self.headers = []
		for i in range(shnum):
			v = struct.unpack_from("<10I", self.d, shoff + i * shentsize)
			self.headers.append(dict(zip(("name", "type", "flags", "addr", "offset", "size",
						      "link", "info", "addralign", "entsize"), v),
						 index=i, header_offset=shoff + i * shentsize))
		strtab = self.headers[shstrndx] if shnum else None
		for h in self.headers:
			h["name"] = self._str(strtab, h["name"]) if strtab else ""

	def _str(self, sec, off):
		start = sec["offset"] + off
		return self.d[start:self.d.index(b"\0", start)].decode("ascii", "replace")

	def get(self, name):
		found = [h for h in self.headers if h["name"] == name]
		if len(found) > 1:
			raise ValueError(f"duplicate section {name}")
		return found[0] if found else None

	def content(self, h):
		return self.d[h["offset"]:h["offset"] + h["size"]] if h["type"] != 8 else bytes(h["size"])

	def symbols(self):
		"""[(name, value, size, info, shndx)] from .symtab."""
		sym = next((h for h in self.headers if h["type"] == 2), None)
		if sym is None:
			return []
		strtab = self.headers[sym["link"]]
		out = []
		for o in range(sym["offset"], sym["offset"] + sym["size"], 16):
			name, value, size, info, _, shndx = struct.unpack_from("<IIIBBH", self.d, o)
			out.append((self._str(strtab, name), value, size, info, shndx))
		return out

	def rels_for(self, h):
		"""REL section applying to section `h`, or None."""
		found = [r for r in self.headers if r["type"] == 9 and r["info"] == h["index"]]
		return found[0] if found else None


def linked_rela_to_rel(data):
	"""Adapt linked ARM ELF32 RELA records for vita-elf-create's REL reader.

	This is only valid after the linker applied relocations (ET_EXEC). The
	packager decodes targets from the linked instruction/data bytes, so the
	explicit addends are redundant here. Do not use this on ET_REL objects.
	Only relocation records and their section headers change; every loaded
	byte, symbol index and relocation offset remains intact.
	"""
	sections = Sections(data)
	elf_type, machine = struct.unpack_from('<HH', data, 16)
	if (elf_type, machine) != (2, 40):
		raise ValueError('RELA adaptation requires linked ET_EXEC ARM ELF32')
	out = bytearray(data)
	for h in sections.headers:
		if h['type'] != 4:
			continue
		if h['flags'] & 2 or h['entsize'] != 12 or h['size'] % 12:
			raise ValueError('expected non-allocated ELF32 RELA records')
		start, end = h['offset'], h['offset'] + h['size']
		if end > len(data):
			raise ValueError('RELA records exceed file')
		records = b''.join(struct.pack('<II', offset, info)
				   for offset, info, _ in struct.iter_unpack('<IIi', data[start:end]))
		out[start:end] = records + bytes(h['size'] - len(records))
		struct.pack_into('<I', out, h['header_offset'] + 4, 9)  # SHT_REL
		struct.pack_into('<I', out, h['header_offset'] + 20, len(records))
		struct.pack_into('<I', out, h['header_offset'] + 36, 8)
	return bytes(out)


def object_rela_to_rel(data):
	"""Encode explicit ARM addends before GNU ld consumes SNC objects.

	GNU ld's ARM Thumb-call RELA handling does not preserve SNC's -4 PC bias.
	Use its conventional REL path instead. Keep the original objects separate;
	unsupported relocation types fail rather than produce runnable-looking code.
	"""
	import scereloc
	sections = Sections(data)
	if struct.unpack_from('<HH', data, 16) != (1, 40):
		raise ValueError('object adaptation requires ET_REL ARM ELF32')
	out = bytearray(data)
	for h in sections.headers:
		if h['type'] != 4:
			continue
		if h['flags'] & 2 or h['entsize'] != 12 or h['size'] % 12:
			raise ValueError('expected non-allocated ELF32 RELA records')
		target = sections.headers[h['info']]
		if target['type'] == 8:
			raise ValueError('RELA target must be file-backed')
		start, end = h['offset'], h['offset'] + h['size']
		if end > len(data):
			raise ValueError('RELA records exceed file')
		records = []
		for offset, info, addend in struct.iter_unpack('<IIi', data[start:end]):
			code = info & 0xff
			if offset + 4 > target['size']:
				raise ValueError('object relocation exceeds target section')
			patch = target['offset'] + offset
			if code != 0:
				def read(_address, size):
					return bytes(out[patch:patch + size])
				def write(_address, value):
					out[patch:patch + len(value)] = value
				if not scereloc._patch(read, write, code, 0, addend, 0):
					raise ValueError(f'unsupported explicit ARM relocation {code}')
			records.append(struct.pack('<II', offset, info))
		records = b''.join(records)
		out[start:end] = records + bytes(h['size'] - len(records))
		struct.pack_into('<I', out, h['header_offset'] + 4, 9)
		struct.pack_into('<I', out, h['header_offset'] + 20, len(records))
		struct.pack_into('<I', out, h['header_offset'] + 36, 8)
	return bytes(out)


def object_comdat_weak(data):
	"""Make an SNC object's definitions in COMDAT groups weak, as GCC's are.

	SNC defines what it may emit in several objects (a vtable of a class without a key
	function, an inline function written out of line) as a global symbol in a COMDAT group.
	Sony's linker keeps one copy of each group; GNU ld reports such a global defined in two
	objects before it discards the second group. Only symbol bindings change.
	"""
	sections = Sections(data)
	members = set()
	for h in sections.headers:
		if h['type'] == 17:  # SHT_GROUP
			members.update(struct.unpack_from(f'<{h["size"] // 4}I', data, h['offset'])[1:])
	out = bytearray(data)
	for h in sections.headers:
		if h['type'] != 2:  # SHT_SYMTAB
			continue
		for o in range(h['offset'], h['offset'] + h['size'], 16):
			info = out[o + 12]
			shndx, = struct.unpack_from('<H', out, o + 14)
			if info >> 4 == 1 and shndx in members:  # STB_GLOBAL
				out[o + 12] = 2 << 4 | info & 0xf  # STB_WEAK
	return bytes(out)


def _thumb_branch(code, at):
	"""(length, target offset or None) of the Thumb instruction at `at` in `code`."""
	hw1, = struct.unpack_from('<H', code, at)
	if hw1 >> 11 < 0b11101:
		if hw1 & 0xf800 == 0xe000:  # B (T2)
			imm = (hw1 & 0x7ff) << 1
			return 2, at + 4 + (imm - (1 << 12) if imm & (1 << 11) else imm)
		if hw1 & 0xf000 == 0xd000 and (hw1 >> 8) & 0xf < 14:  # B<cond> (T1)
			imm = (hw1 & 0xff) << 1
			return 2, at + 4 + (imm - (1 << 9) if imm & (1 << 8) else imm)
		return 2, None
	if at + 4 > len(code):
		return 2, None
	hw2, = struct.unpack_from('<H', code, at + 2)
	if hw1 & 0xf800 != 0xf000 or not hw2 & 0x8000:
		return 4, None
	s, j1, j2 = (hw1 >> 10) & 1, (hw2 >> 13) & 1, (hw2 >> 11) & 1
	if hw2 & 0x5000 == 0 and (hw1 >> 6) & 0xf < 14:  # B<cond>.W (T3)
		imm = s << 20 | j2 << 19 | j1 << 18 | (hw1 & 0x3f) << 12 | (hw2 & 0x7ff) << 1
		return 4, at + 4 + (imm - (1 << 21) if s else imm)
	if hw2 & 0x5000 == 0:
		return 4, None
	# B.W (T4), BL, BLX
	i1, i2 = ~(j1 ^ s) & 1, ~(j2 ^ s) & 1
	imm = s << 24 | i1 << 23 | i2 << 22 | (hw1 & 0x3ff) << 12 | (hw2 & 0x7ff) << 1
	return 4, at + 4 + (imm - (1 << 25) if s else imm)


def object_split_text(data):
	"""Give each function of an SNC object's .text a section of its own (.text.<name>).

	SNC puts all of a file's functions in one .text, and Sony linked with psp2ld
	--strip-unused, which drops the unreferenced functions inside it; GNU ld's --gc-sections
	drops only whole sections. Splitting at the function symbols keeps every byte, symbol
	and relocation where it was (a section's alignment is what its offset had in .text), so
	a link that keeps every function lays the text out as before. .ARM.exidx is split with
	it, one table per function section. Works on the RELA object as SNC writes it; refuses
	what it cannot move exactly: a relocation against the .text section symbol, a branch
	without a relocation from one function into another, code before the first function.
	"""
	sections = Sections(data)
	if struct.unpack_from('<HH', data, 16) != (1, 40):
		raise ValueError('splitting requires an ET_REL ARM ELF32')
	text = sections.get('.text')
	if text is None or text['size'] == 0 or text['type'] != 1:
		return data
	ti = text['index']
	symh = next(h for h in sections.headers if h['type'] == 2)
	strh = sections.headers[symh['link']]
	syms = [list(struct.unpack_from('<IIIBBH', data, o))
		for o in range(symh['offset'], symh['offset'] + symh['size'], 16)]
	for h in sections.headers:
		if h['type'] == 17 and ti in struct.unpack_from(f'<{h["size"] // 4}I', data, h['offset'])[1:]:
			raise ValueError('.text is in a COMDAT group')
	funcs = {}
	for sym in syms:
		if sym[5] == ti and sym[3] & 0xf == 2:
			name = sections._str(strh, sym[0])
			start = sym[1] & ~1
			if start not in funcs or (sym[3] >> 4 == 1 and funcs[start][1] != 1):
				funcs[start] = (name, sym[3] >> 4)
	starts = sorted(funcs)
	if not starts or starts[0] != 0:
		raise ValueError('.text does not start with a function')
	ends = starts[1:] + [text['size']]
	import bisect
	def piece(offset):
		return bisect.bisect_right(starts, offset) - 1
	code = sections.content(text)

	def relas(h):
		return [list(r) for r in struct.iter_unpack('<IIi', sections.content(h))]
	text_section_syms = {i for i, sym in enumerate(syms) if sym[3] & 0xf == 3 and sym[5] == ti}
	for h in sections.headers:
		if h['type'] == 4 and any(r[1] >> 8 in text_section_syms for r in relas(h)):
			raise ValueError(f'{h["name"]} relocates against the .text section symbol')
	text_rela = next((h for h in sections.headers if h['type'] == 4 and h['info'] == ti), None)
	text_records = relas(text_rela) if text_rela else []
	relocated = {r[0] for r in text_records}
	# Thumb code ranges from the mapping symbols
	marks = sorted((sym[1], sections._str(strh, sym[0])[:2]) for sym in syms
		       if sym[5] == ti and sections._str(strh, sym[0])[:2] in ('$t', '$a', '$d'))
	for k, (start, kind) in enumerate(marks):
		if kind != '$t':
			continue
		end = marks[k + 1][0] if k + 1 < len(marks) else text['size']
		at = start
		while at < end:
			length, target = _thumb_branch(code, at)
			if target is not None and at not in relocated and piece(target) != piece(at):
				raise ValueError(f'branch at .text+{at:#x} into another function without a relocation')
			at += length

	exidx = next((h for h in sections.headers if h['type'] == 0x70000001 and h['link'] == ti), None)
	exidx_rela = exidx and next((h for h in sections.headers if h['type'] == 4 and h['info'] == exidx['index']), None)
	exidx_piece = {}
	if exidx:
		if exidx['size'] % 8:
			raise ValueError('.ARM.exidx is not a table of 8-byte entries')
		for offset, info, addend in relas(exidx_rela) if exidx_rela else []:
			if offset % 8 == 0 and info & 0xff == 42:  # R_ARM_PREL31 to the function
				sym = syms[info >> 8]
				if sym[5] != ti:
					raise ValueError(f'.ARM.exidx+{offset:#x} does not point into .text')
				exidx_piece[offset // 8] = piece((sym[1] & ~1) + addend)
		if len(exidx_piece) != exidx['size'] // 8:
			raise ValueError('.ARM.exidx entry without a function relocation')

	# the new section list: old index -> new index (or [new index per piece])
	out_sections, index_of, pieces_of = [], {}, {}
	def add(h, content, name=None, **changes):
		entry = dict(h, name=name or h['name'], content=content, **changes)
		out_sections.append(entry)
		return len(out_sections) - 1
	for h in sections.headers:
		i = h['index']
		if i == ti:
			pieces_of[i] = []
			for k, (start, end) in enumerate(zip(starts, ends)):
				align = text['addralign'] or 1
				while align > 1 and start % align:
					align //= 2
				pieces_of[i].append(add(h, code[start:end], f'.text.{funcs[start][0]}',
							size=end - start, addralign=align))
		elif text_rela is not None and i == text_rela['index']:
			pieces_of[i] = []
			for k, (start, end) in enumerate(zip(starts, ends)):
				records = [(r[0] - start, r[1], r[2]) for r in text_records if piece(r[0]) == k]
				blob = b''.join(struct.pack('<IIi', *r) for r in records)
				pieces_of[i].append(add(h, blob, f'.rela.text.{funcs[start][0]}', size=len(blob),
							piece=k))
		elif exidx is not None and i == exidx['index']:
			pieces_of[i] = {}
			table = sections.content(h)
			for k in sorted(set(exidx_piece.values())):
				entries = [e for e in range(len(table) // 8) if exidx_piece[e] == k]
				blob = b''.join(table[8 * e:8 * e + 8] for e in entries)
				pieces_of[i][k] = add(h, blob, f'.ARM.exidx.text.{funcs[starts[k]][0]}',
						      size=len(blob), piece=k, entries=entries)
		elif exidx_rela is not None and i == exidx_rela['index']:
			pieces_of[i] = {}
			records = relas(h)
			for k, j in pieces_of[exidx['index']].items():
				entries = out_sections[j]['entries']
				moved = []
				for n, e in enumerate(entries):
					moved += [(r[0] - 8 * e + 8 * n, r[1], r[2]) for r in records if r[0] // 8 == e]
				blob = b''.join(struct.pack('<IIi', *r) for r in moved)
				pieces_of[i][k] = add(h, blob, f'.rela.ARM.exidx.text.{funcs[starts[k]][0]}',
						      size=len(blob), piece=k)
		else:
			index_of[i] = add(h, sections.content(h) if h['type'] != 8 else b'')

	def remap(i):
		if i == 0 or i >= 0xff00:
			return i
		if i in pieces_of:  # a section symbol of a split section: its first piece
			first = pieces_of[i]
			return first[0] if isinstance(first, list) else first[min(first)]
		return index_of[i]
	for sym in syms:
		if sym[5] == ti:
			k = piece(sym[1] & ~1) if sym[3] & 0xf != 3 else 0
			sym[1] -= starts[k]
			sym[5] = pieces_of[ti][k]
		else:
			sym[5] = remap(sym[5])
	out_sections[index_of[symh['index']]]['content'] = b''.join(struct.pack('<IIIBBH', *s) for s in syms)
	for i, h in enumerate(out_sections):
		old = h['index']
		if old == ti:
			pass
		elif text_rela is not None and old == text_rela['index']:
			h['link'], h['info'] = remap(h['link']), pieces_of[ti][h['piece']]
		elif exidx is not None and old == exidx['index']:
			h['link'] = pieces_of[ti][h['piece']]
		elif exidx_rela is not None and old == exidx_rela['index']:
			h['link'], h['info'] = remap(h['link']), pieces_of[exidx['index']][h['piece']]
		else:
			if h['link']:
				h['link'] = remap(h['link'])
			if h['type'] in (4, 9) or h['flags'] & 0x40:
				h['info'] = remap(h['info'])
			if h['type'] == 17:
				words = struct.unpack_from(f'<{h["size"] // 4}I', h['content'])
				h['content'] = struct.pack(f'<{len(words)}I', words[0], *map(remap, words[1:]))

	shstr = bytearray(b'\0')
	for h in out_sections:
		h['name_offset'] = len(shstr) if h['name'] else 0
		if h['name']:
			shstr += h['name'].encode() + b'\0'
	shstrndx = index_of[struct.unpack_from('<H', data, 50)[0]]
	out_sections[shstrndx]['content'] = bytes(shstr)
	out = bytearray(data[:52])
	for h in out_sections:
		if h['type'] == 8 or h['index'] == 0:
			h['offset'] = 0 if h['index'] == 0 else len(out)
			continue
		align = max(h['addralign'], 1)
		out += bytes(-len(out) % align)
		h['offset'] = len(out)
		h['size'] = len(h['content'])
		out += h['content']
	out += bytes(-len(out) % 4)
	shoff = len(out)
	for h in out_sections:
		out += struct.pack('<10I', h['name_offset'], h['type'], h['flags'], h['addr'], h['offset'],
				   h['size'], h['link'], h['info'], h['addralign'], h['entsize'])
	struct.pack_into('<I', out, 32, shoff)
	struct.pack_into('<HH', out, 48, len(out_sections), shstrndx)
	return bytes(out)


def object_for_ld(data, split=False):
	"""An SNC object as GNU ld links it: REL relocations, weak COMDAT definitions; with
	`split`, a section per function (object_split_text) for a module that links with
	--gc-sections as Sony's did with --strip-unused."""
	if split and any(h['type'] == 4 for h in Sections(data).headers):
		data = object_split_text(data)
	if any(h['type'] == 4 for h in Sections(data).headers):
		data = object_rela_to_rel(data)
	if any(h['type'] == 17 for h in Sections(data).headers):
		data = object_comdat_weak(data)
	return data
