# SCE relocations (PT_SCE_RELA, p_type 0x60000000) of PRX2 modules.
#
#   relocate(read, write, segs, data)   segs: [(vaddr_linked, vaddr_loaded, size)] by index
#
# Formats (first word bit fields, LSB first; layouts as in vita-toolchain sce-elf.h,
# semantics as implemented by Vita3K kernel/src/relocation.cpp):
#   0 (12 B) fmt:4 symseg:4 code:8 patchseg:4 code2:8 dist2:4 | addend | offset
#   1 ( 8 B) fmt:4 symseg:4 code:8 patchseg:4 offset_lo:12 | offset_hi:10 addend:22
#   2 ( 8 B) fmt:4 symseg:4 code:8 offset:16 | addend          (offset += previous)
#   3 ( 8 B) fmt:4 symseg:4 thumb:1 offset:18 dist2:5 | addend:22   (MOVW/MOVT pair)
#   4 ( 4 B) fmt:4 offset:23 dist2:5                           (pair, previous symbol)
#   5 ( 4 B) fmt:4 dist1:9 dist2:5 dist3:9 dist4:5             (two pairs)
#   6 ( 4 B) fmt:4 offset:28                                   (ABS32, addend from content)
#   7/8/9 (4 B) fmt:4 offsets:28 as 4x7 / 7x4 / 14x2 bit word deltas (ABS32 runs)
# Symbol segment 0xf means absolute (S = 0).
import struct

ABS32, REL32, CALL, JUMP24, THM_CALL, TARGET1, TARGET2, PREL31 = 2, 3, 28, 29, 10, 38, 41, 42
MOVW, MOVT, THM_MOVW, THM_MOVT = 43, 44, 47, 48

def _patch(read, write, code, S, A, P):
	v = (S + A) & 0xffffffff
	if code in (ABS32, TARGET1):
		write(P, struct.pack("<I", v))
	elif code in (REL32, TARGET2):
		write(P, struct.pack("<I", (v - P) & 0xffffffff))
	elif code == PREL31:
		write(P, struct.pack("<I", (v - P) & 0x7fffffff))
	elif code in (MOVW, MOVT):
		imm = v & 0xffff if code == MOVW else v >> 16
		w, = struct.unpack("<I", read(P, 4))
		write(P, struct.pack("<I", (w & 0xfff0f000) | (imm >> 12 & 0xf) << 16 | (imm & 0xfff)))
	elif code in (THM_MOVW, THM_MOVT):
		imm = v & 0xffff if code == THM_MOVW else v >> 16
		h1, h2 = struct.unpack("<HH", read(P, 4))
		h1 = (h1 & 0xfbf0) | (imm >> 12 & 0xf) | ((imm >> 11 & 1) << 10)
		h2 = (h2 & 0x8f00) | ((imm >> 8 & 7) << 12) | (imm & 0xff)
		write(P, struct.pack("<HH", h1, h2))
	elif code == THM_CALL:
		off = (v - P) & 0xffffffff          # addend carries the PC bias
		s = off >> 24 & 1
		i1, i2 = off >> 23 & 1, off >> 22 & 1
		j1, j2 = (i1 ^ 1) ^ s, (i2 ^ 1) ^ s
		h1, h2 = struct.unpack("<HH", read(P, 4))
		h1 = (h1 & 0xf800) | s << 10 | (off >> 12 & 0x3ff)
		h2 = (h2 & 0xd000) | j1 << 13 | j2 << 11 | (off >> 1 & 0x7ff)
		write(P, struct.pack("<HH", h1, h2))
	elif code in (CALL, JUMP24):
		w, = struct.unpack("<I", read(P, 4))
		write(P, struct.pack("<I", (w & 0xff000000) | ((v - P) >> 2 & 0xffffff)))
	else:
		return False
	return True

def relocate(read, write, segs, data):
	"""Apply a relocation stream; returns (applied, unhandled codes)."""
	def base(i):
		return 0 if i == 0xf else segs[i][1]
	def seg_of(value):
		for linked, loaded, size in segs:
			if linked <= value < linked + size:
				return linked, loaded
		return 0, 0
	g = dict(addr=0, offset=0, patchseg=0, S=0, A=0, type=0, type2=0)
	applied, unknown = 0, set()
	def do(code, S, A, P):
		nonlocal applied
		if code == 0:
			return
		if _patch(read, write, code, S, A, P):
			applied += 1
		else:
			unknown.add(code)
	def abs_from_content():
		P = g["addr"] + g["offset"]
		orig, = struct.unpack("<I", read(P, 4))
		linked, loaded = seg_of(orig)
		g.update(S=loaded, type=ABS32, type2=0)
		do(ABS32, loaded, orig - linked, P)
	i = 0
	while i + 4 <= len(data):
		w0, = struct.unpack_from("<I", data, i)
		fmt = w0 & 0xf
		w1 = struct.unpack_from("<I", data, i + 4)[0] if i + 8 <= len(data) else 0
		if fmt == 0:
			w2, = struct.unpack_from("<I", data, i + 8)
			symseg, code, pseg = w0 >> 4 & 0xf, w0 >> 8 & 0xff, w0 >> 16 & 0xf
			code2, dist2 = w0 >> 20 & 0xff, w0 >> 28
			S, P = base(symseg), segs[pseg][1] + w2
			do(code, S, w1, P)
			if code2:
				do(code2, S, w1, P + dist2 * 2)
			g.update(addr=segs[pseg][1], offset=w2, patchseg=pseg, S=S, A=w1, type=code, type2=code2)
			i += 12
		elif fmt == 1:
			symseg, code, pseg = w0 >> 4 & 0xf, w0 >> 8 & 0xff, w0 >> 16 & 0xf
			off = (w0 >> 20) | (w1 & 0x3ff) << 12
			A, S = w1 >> 10, base(symseg)
			do(code, S, A, segs[pseg][1] + off)
			g.update(addr=segs[pseg][1], offset=off, patchseg=pseg, S=S, A=A, type=code, type2=0)
			i += 8
		elif fmt == 2:
			symseg, code = w0 >> 4 & 0xf, w0 >> 8 & 0xff
			g["offset"] += w0 >> 16
			g.update(S=base(symseg), A=w1, type=code, type2=0)
			do(code, g["S"], w1, g["addr"] + g["offset"])
			i += 8
		elif fmt == 3:
			symseg, thumb = w0 >> 4 & 0xf, w0 >> 8 & 1
			g["offset"] += w0 >> 9 & 0x3ffff
			dist2 = w0 >> 27
			A = w1 & 0x3fffff
			t1, t2 = (THM_MOVW, THM_MOVT) if thumb else (MOVW, MOVT)
			g.update(S=base(symseg), A=A, type=t1, type2=t2)
			P = g["addr"] + g["offset"]
			do(t1, g["S"], A, P)
			do(t2, g["S"], A, P + dist2)
			i += 8
		elif fmt == 4:
			g["offset"] += w0 >> 4 & 0x7fffff
			P = g["addr"] + g["offset"]
			do(g["type"], g["S"], g["A"], P)
			do(g["type2"], g["S"], g["A"], P + (w0 >> 27))
			i += 4
		elif fmt == 5:
			d1, d2, d3, d4 = w0 >> 4 & 0x1ff, w0 >> 13 & 0x1f, w0 >> 18 & 0x1ff, w0 >> 27
			g["offset"] += d1
			P = g["addr"] + g["offset"]
			do(g["type"], g["S"], g["A"], P)
			do(g["type2"], g["S"], g["A"], P + d2)
			g["offset"] += d3
			P = g["addr"] + g["offset"]
			do(g["type"], g["S"], g["A"], P)
			do(g["type2"], g["S"], g["A"], P + d4)
			i += 4
		elif fmt == 6:
			g["offset"] += w0 >> 4
			abs_from_content()
			i += 4
		elif fmt in (7, 8, 9):
			bits = {7: 7, 8: 4, 9: 2}[fmt]
			offs = w0 >> 4
			while True:
				g["offset"] += (offs & ((1 << bits) - 1)) * 4
				abs_from_content()
				offs >>= bits
				if not offs:
					break
			i += 4
		else:
			raise ValueError(f"unknown SCE relocation format {fmt} at {i:#x}")
	return applied, unknown
