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


def object_for_ld(data):
	"""An SNC object as GNU ld links it: REL relocations, weak COMDAT definitions."""
	if any(h['type'] == 4 for h in Sections(data).headers):
		data = object_rela_to_rel(data)
	if any(h['type'] == 17 for h in Sections(data).headers):
		data = object_comdat_weak(data)
	return data
