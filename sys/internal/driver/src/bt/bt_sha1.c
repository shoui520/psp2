/* Bluetooth SHA-1; the original source filename is unavailable. */
#include <stdint.h>
#include <string.h>
#include <sysmem/sysclib.h>

#if FW_VERSION >= 0x04000000
#define SHA1_COPY(destination, source, length) \
	__memcpy_chk(destination, source, length, 64)
#else
#define SHA1_COPY(destination, source, length) memcpy(destination, source, length)
#endif

#define SHA1_ROTATE(value, bits) \
	(((value) << (bits)) | ((value) >> (32 - (bits))))
#define SHA1_EXPAND(index) \
	(block[(index) & 15] = SHA1_ROTATE(block[((index) + 13) & 15] ^ \
		block[((index) + 8) & 15] ^ block[((index) + 2) & 15] ^ \
		block[(index) & 15], 1))

uint32_t BtSha1Compress(uint32_t block[16], uint32_t state[5])
{
	uint32_t a = state[0];
	uint32_t b = state[1];
	uint32_t c = state[2];
	uint32_t d = state[3];
	uint32_t e = state[4];
	uint32_t word = block[0];
	uint32_t next;
	int round;

	round = 0;
	for (;;) {
		next = SHA1_ROTATE(a, 5) + ((b & c) | (~b & d)) +
		       e + word + 0x5a827999;
		e = d;
		d = c;
		c = SHA1_ROTATE(b, 30);
		b = a;
		a = next;
		if (++round == 20)
			break;
		word = round <= 15 ? block[round & 15] : SHA1_EXPAND(round);
	}
	for (; round < 40; round++) {
		word = SHA1_EXPAND(round);
		next = SHA1_ROTATE(a, 5) + (b ^ c ^ d) + e + word + 0x6ed9eba1;
		e = d;
		d = c;
		c = SHA1_ROTATE(b, 30);
		b = a;
		a = next;
	}
	for (; round < 60; round++) {
		word = SHA1_EXPAND(round);
		next = SHA1_ROTATE(a, 5) + (((c | d) & b) | (c & d)) +
		       e + word + 0x8f1bbcdc;
		e = d;
		d = c;
		c = SHA1_ROTATE(b, 30);
		b = a;
		a = next;
	}
	for (; round < 80; round++) {
		word = SHA1_EXPAND(round);
		next = SHA1_ROTATE(a, 5) + (b ^ c ^ d) + e + word + 0xca62c1d6;
		e = d;
		d = c;
		c = SHA1_ROTATE(b, 30);
		b = a;
		a = next;
	}
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	return state[2];
}

uint32_t BtSha1Digest(const uint8_t *source, int length, uint8_t *digest)
{
	uint32_t state[5] = {
		0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0,
	};
	uint32_t block[16];
	uint8_t tail[64];

	uint32_t bitLength = (uint32_t)length * 8;
	int remaining = length;
	int index;

	while (remaining > 55) {
		const uint8_t *chunk = source;

		if (remaining <= 63) {
			SHA1_COPY(tail, source, remaining);
			tail[remaining] = 0x80;
			if (remaining != 63)
				memset(tail + remaining + 1, 0, 63 - remaining);
			chunk = tail;
		}
		for (index = 0; index < 16; index++) {
			block[index] = (uint32_t)chunk[index * 4] << 24 |
				       (uint32_t)chunk[index * 4 + 1] << 16 |
				       (uint32_t)chunk[index * 4 + 2] << 8 |
				       chunk[index * 4 + 3];
		}
		BtSha1Compress(block, state);
		remaining -= 64;
		/* The final padded block can advance past the input allocation. */
		source = (const uint8_t *)((uintptr_t)source + 64);
	}
	if (remaining > 0) {
		SHA1_COPY(tail, source, remaining);
		tail[remaining] = 0x80;
		if (remaining != 55)
			memset(tail + remaining + 1, 0, 55 - remaining);
		for (index = 0; index < 14; index++) {
			block[index] = (uint32_t)tail[index * 4] << 24 |
				       (uint32_t)tail[index * 4 + 1] << 16 |
				       (uint32_t)tail[index * 4 + 2] << 8 |
				       tail[index * 4 + 3];
		}
	} else {
		block[0] = remaining == 0 ? 0x80000000 : 0;
		memset(block + 1, 0, 13 * sizeof(*block));
	}
	block[14] = 0;
	block[15] = bitLength;
	BtSha1Compress(block, state);
	for (index = 0; index < 5; index++) {
		digest[index * 4] = state[index] >> 24;
		digest[index * 4 + 1] = state[index] >> 16;
		digest[index * 4 + 2] = state[index] >> 8;
		digest[index * 4 + 3] = state[index];
	}
	return state[3] >> 16;
}
