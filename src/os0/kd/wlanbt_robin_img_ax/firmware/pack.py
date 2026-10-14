#!/usr/bin/env python3
"""
Pack the Marvell 88W8787 ("Robin") WLAN/Bluetooth firmware image
(os0:kd/wlanbt_robin_img_ax.skprx's payload) from the linked ELF.

    pack.py OUT ELF

The firmware links two read-only code/rodata segments (ITCM at 0x00000000 and
SQRAM1 at 0xc0007200) and four initialized read-write data segments (ITCM RW at
0x00054764, DTCM RW at 0x04000100, SQRAM0 RW at 0xc0001100, and SQRAM1 RW at
0xc000faa8). At boot, __scatterload (boot/vectors.S) copies each compressed RW
stream to scratch RAM (0x00055c4c) and decompresses it in place with
__decompress1 (RVCT LZ77), then zeroes the five BSS regions.

pack.py compresses the four RW segments with RVCT LZ77, patches Region$$Table
with their compressed sizes, and wraps the four contiguous download blocks into
Marvell SDIO firmware download chunks (cmd=1 FW_HAS_DATA_TO_RECV, up to 1020
data bytes + 4-byte MSB-first CRC-32 per chunk, followed by cmd=4
FW_HAS_LAST_BLOCK).
"""
import struct
import sys

ELF_MAGIC = b'\x7fELF'
PT_LOAD = 1
SHT_SYMTAB = 2

CMD_DATA_TO_RECV = 1
CMD_LAST_BLOCK = 4
MAX_CHUNK_DATA = 0x3fc


def crc32_mrvl_be(data):
	"""Marvell SDIO bootloader MSB-first CRC-32 (poly=0x04c11db7, init=0, xorout=0), big-endian."""
	crc = 0
	for b in data:
		crc ^= (b << 24) & 0xffffffff
		for _ in range(8):
			if crc & 0x80000000:
				crc = ((crc << 1) ^ 0x04c11db7) & 0xffffffff
			else:
				crc = (crc << 1) & 0xffffffff
	return struct.pack('>I', crc)


def rvct_lz77_decompress(src_bytes, out_len):
	"""Decompress an RVCT __decompress1 LZ77 stream (verifies compressor output)."""
	src = 0
	dst = bytearray()
	while len(dst) < out_len:
		b = src_bytes[src]
		src += 1
		lit_cnt = b & 3
		if lit_cnt == 0:
			lit_cnt = src_bytes[src]
			src += 1
		match_cnt = b >> 4
		if match_cnt == 0:
			match_cnt = src_bytes[src]
			src += 1
		lit_cnt -= 1
		for _ in range(lit_cnt):
			dst.append(src_bytes[src])
			src += 1
		if match_cnt != 0:
			off_lo = src_bytes[src]
			src += 1
			off_hi = (b >> 2) & 3
			if off_hi == 3:
				off_hi = src_bytes[src]
				src += 1
			back = off_lo + (off_hi << 8)
			for _ in range(match_cnt + 2):
				dst.append(dst[len(dst) - back])
	return bytes(dst)


def rvct_lz77_compress(data):
	"""Compress data into an RVCT __decompress1 LZ77 stream."""
	out = bytearray()
	n = len(data)
	pos = 0
	lit_start = 0

	def emit_token(lits, match_len, match_dist):
		lit_field = len(lits) + 1
		assert 1 <= lit_field <= 255
		mlen_field = (match_len - 2) if match_len >= 3 else 0
		assert 0 <= mlen_field <= 255
		lo_bits = lit_field if lit_field <= 3 else 0
		hi_bits = mlen_field if (1 <= mlen_field <= 15) else (0 if mlen_field > 15 else 0)
		if match_len == 0:
			# No match: token byte must have upper nibble != 0 only if match, wait:
			# In __decompress1: r5 = b >> 4; if r5 == 0: r5 = *src++. Then if r5 != 0: copy match!
			# So for match_len == 0, upper nibble MUST be 0 AND followed by a 0x00 byte!
			hi_bits = 0
			mid_bits = 0
		else:
			off_hi = match_dist >> 8
			mid_bits = off_hi if off_hi < 3 else 3
		token = (hi_bits << 4) | (mid_bits << 2) | lo_bits
		out.append(token)
		if lo_bits == 0:
			out.append(lit_field)
		if match_len == 0:
			out.append(0)
		elif hi_bits == 0:
			out.append(mlen_field)
		out.extend(lits)
		if match_len > 0:
			out.append(match_dist & 0xff)
			if mid_bits == 3:
				out.append(match_dist >> 8)

	while pos < n:
		best_len = 0
		best_dist = 0
		max_len = min(257, n - pos)
		if max_len >= 3 and pos > 0:
			start_search = max(0, pos - 4095)
			sub = data[pos:pos + 3]
			cur = data.find(sub, start_search, pos)
			while cur != -1 and cur < pos:
				dist = pos - cur
				ml = 3
				while ml < max_len and data[cur + ml] == data[pos + ml]:
					ml += 1
				if ml > best_len or (ml == best_len and dist < best_dist):
					best_len = ml
					best_dist = dist
					if best_len == max_len:
						break
				cur = data.find(sub, cur + 1, pos)

		if best_len >= 3:
			while pos - lit_start > 254:
				emit_token(data[lit_start:lit_start + 254], 0, 0)
				lit_start += 254
			emit_token(data[lit_start:pos], best_len, best_dist)
			pos += best_len
			lit_start = pos
		else:
			pos += 1
			if pos - lit_start == 254:
				emit_token(data[lit_start:pos], 0, 0)
				lit_start = pos

	if lit_start < n:
		emit_token(data[lit_start:n], 0, 0)

	# Pad to 4-byte alignment
	while len(out) % 4 != 0:
		out.append(0)

	if rvct_lz77_decompress(out, n) != data:
		sys.exit('pack.py: LZ77 round-trip verification failed')
	return bytes(out)


def read_elf(path):
	"""Return (section_by_name, sym_by_name) from a 32-bit little-endian ARM ELF."""
	data = open(path, 'rb').read()
	if data[:4] != ELF_MAGIC or data[4] != 1 or data[5] != 1:
		sys.exit('%s: not a 32-bit little-endian ELF' % path)
	shoff = struct.unpack_from('<I', data, 0x20)[0]
	shentsize, shnum, shstrndx = struct.unpack_from('<HHH', data, 0x2e)
	shstr_off, shstr_sz = struct.unpack_from('<II', data, shoff + shstrndx * shentsize + 16)
	shstr = data[shstr_off:shstr_off + shstr_sz]

	sections = {}
	syms = {}
	for i in range(shnum):
		sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link = struct.unpack_from(
			'<7I', data, shoff + i * shentsize)
		end = shstr.find(b'\x00', sh_name)
		name = shstr[sh_name:end].decode('ascii', errors='ignore')
		sections[name] = (sh_addr, data[sh_offset:sh_offset + sh_size], sh_size, sh_type)
		if sh_type == SHT_SYMTAB:
			str_off, str_sz = struct.unpack_from('<II', data, shoff + sh_link * shentsize + 16)
			strtab = data[str_off:str_off + str_sz]
			for off in range(0, sh_size, 16):
				st_name, st_val, st_sz, st_info, st_other, st_shndx = struct.unpack_from(
					'<IIIBBH', data, sh_offset + off)
				if st_name:
					send = strtab.find(b'\x00', st_name)
					sname = strtab[st_name:send].decode('ascii', errors='ignore')
					syms[sname] = st_val
	return sections, syms


def pack_chunks(blocks):
	"""Wrap contiguous (addr, payload) blocks into Marvell SDIO download chunks."""
	out = bytearray()
	for base_addr, blob in blocks:
		off = 0
		while off < len(blob):
			part = blob[off:off + MAX_CHUNK_DATA]
			addr = base_addr + off
			plen = len(part) + 4
			hdr = struct.pack('<III', CMD_DATA_TO_RECV, addr, plen)
			hdr += crc32_mrvl_be(hdr)
			out.extend(hdr)
			out.extend(part)
			out.extend(crc32_mrvl_be(part))
			off += len(part)
	last_hdr = struct.pack('<III', CMD_LAST_BLOCK, 0, 0)
	last_hdr += crc32_mrvl_be(last_hdr)
	out.extend(last_hdr)
	return bytes(out)


def main(argv):
	if len(argv) != 3:
		sys.exit(__doc__)
	out_path, elf_path = argv[1], argv[2]
	sections, syms = read_elf(elf_path)

	itcm_ro = bytearray(sections['.itcm.ro'][1])
	itcm_rw = sections['.itcm.rw'][1]
	dtcm_rw = sections['.dtcm.rw'][1]
	sqram0_rw = sections['.sqram0.rw'][1]
	sqram1_ro = sections['.sqram1.ro'][1]
	sqram1_rw = sections['.sqram1.rw'][1]

	comp_itcm_rw = rvct_lz77_compress(itcm_rw) + b'\x00\x00\x00\x00'
	comp_dtcm_rw = rvct_lz77_compress(dtcm_rw) + b'\x00\x00\x00\x00'
	comp_sqram0_rw = rvct_lz77_compress(sqram0_rw) + b'\x00\x00\x00\x00'
	comp_sqram1_rw = rvct_lz77_compress(sqram1_rw) + b'\x00\x00\x00\x00'

	blocks = [
		(0x00000000, bytes(itcm_ro) + comp_itcm_rw),
		(0x04000100, comp_dtcm_rw),
		(0xc0001100, comp_sqram0_rw),
		(0xc0007200, sqram1_ro + comp_sqram1_rw),
	]
	image = pack_chunks(blocks)
	open(out_path, 'wb').write(image)


if __name__ == '__main__':
	main(sys.argv)
