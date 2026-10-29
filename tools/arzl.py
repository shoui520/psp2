#!/usr/bin/env python3
# An ARZL archive, the form SKBL carries the TrustZone SceSysmem in: "ARZL", then one stream
# that SKBL's sceArlzDecode unpacks and sceArlzArmFilter(..., 0) turns back into the image
# (sys/internal/kernel/src/kernel_boot_loader/skbl/arlz.c).
#
#   arzl.py pack image archive
#
# The stream's first byte is its properties. Sony's archives are range coded (bit 7 clear);
# bit 7 set is the decoder's other form, the data stored as it is: a big-endian 32-bit size,
# then that many bytes. This writes that form, of the image after sceArlzArmFilter's encode
# direction, so SKBL unpacks the image unchanged.
#
# sceArlzArmFilter walks the image a word at a time: a Thumb-2 MOVT swaps its destination
# register with its imm4 field, a BL's 22-bit offset becomes its target (encode adds the
# position, decode subtracts it), and a word whose second halfword starts a 32-bit
# instruction moves on by a halfword only. Neither direction changes what the walk tests, so
# each undoes the other.
import struct
import sys

ARZL_MAGIC = b"ARZL"
ARZL_STORED = 0x80


def arm_filter(data, encode):
	"""sceArlzArmFilter on a copy of data"""
	buf = bytearray(data)
	end = len(buf)
	pos = 0
	while pos < end:
		word = buf[pos:pos + 4].ljust(4, b"\0")
		insn = struct.unpack("<I", word)[0]
		step = 4
		op = insn & 0xf800f800
		if insn & 0x8000fbf0 == 0x0000f2c0:
			# MOVT: Rd (bits 24-27) and imm4 (bits 0-3) trade places
			rd = (insn >> 24) & 0xf
			imm4 = insn & 0xf
			insn = (insn & ~0x0f00000f) | rd | imm4 << 24
		elif op == 0xf800f000:
			# BL: imm11 of the first halfword above imm11 of the second
			target = (insn >> 16) & 0x7ff | (insn & 0x7ff) << 11
			if encode:
				rel = target + ((pos + 4) >> 1)
			else:
				rel = target - ((pos + 4) >> 1)
			insn = 0xf800f000 | (rel & 0x7ff) << 16 | (rel >> 11) & 0x7ff
		elif op >> 27 == 0x1e:
			step = 2
		if pos + 4 <= end:
			buf[pos:pos + 4] = struct.pack("<I", insn & 0xffffffff)
		pos += step
	return bytes(buf)


def pack(image):
	"""the archive of image, stored"""
	stream = arm_filter(image, True)
	archive = ARZL_MAGIC + bytes([ARZL_STORED]) + struct.pack(">I", len(stream)) + stream
	if arm_filter(stream, False) != image:
		raise SystemExit("arzl.py: the ARM filter does not give the image back")
	return archive


def main():
	if len(sys.argv) != 4 or sys.argv[1] != "pack":
		raise SystemExit("usage: arzl.py pack image archive")
	with open(sys.argv[2], "rb") as f:
		image = f.read()
	with open(sys.argv[3], "wb") as f:
		f.write(pack(image))


if __name__ == "__main__":
	main()
