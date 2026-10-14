/* Shared SHA-224/SHA-256 compression, with state committed after all blocks. */
#include "sha2_internal.h"

#define ROR32(value, count) \
	(((value) >> (count)) | ((value) << (32 - (count))))

const unsigned char *Sha256Transform(const unsigned char *plain,
	unsigned int blocks, uint32_t state[8])
{
	const unsigned char *end = plain + (blocks << 6);
	uint32_t chain[8];
	unsigned int i;

	for (i = 0; i < 8; i++)
		chain[i] = state[i];
	do {
		uint32_t words[16];
		uint32_t a = chain[0], b = chain[1];
		uint32_t c = chain[2], d = chain[3];
		uint32_t e = chain[4], f = chain[5];
		uint32_t g = chain[6], h = chain[7];

		for (i = 0; i < 16; i++) {
			words[i] = ((uint32_t)plain[0] << 24) |
				((uint32_t)plain[1] << 16) |
				((uint32_t)plain[2] << 8) | plain[3];
			plain += 4;
		}
		for (i = 0; i < 64; i++) {
			uint32_t first, second;

			if (i >= 16) {
				uint32_t previous = words[(i - 2) & 15];
				uint32_t early = words[(i - 15) & 15];

				words[i & 15] += words[(i - 7) & 15] +
					(ROR32(previous, 17) ^
					 ROR32(previous, 19) ^ (previous >> 10)) +
					(ROR32(early, 7) ^ ROR32(early, 18) ^
					 (early >> 3));
			}
			first = h + ((e & f) + (~e & g)) +
				sha256RoundConstants[i] + words[i & 15] +
				ROR32(e ^ ROR32(e, 19) ^ ROR32(e, 5), 6);
			second = ROR32(a ^ ROR32(a, 20) ^ ROR32(a, 11), 2) +
				((a ^ b) & c) + (a & b);
			h = g;
			g = f;
			f = e;
			e = d + first;
			d = c;
			c = b;
			b = a;
			a = first + second;
		}
		chain[0] += a;
		chain[1] += b;
		chain[2] += c;
		chain[3] += d;
		chain[4] += e;
		chain[5] += f;
		chain[6] += g;
		chain[7] += h;
	} while (plain != end);
	for (i = 0; i < 8; i++)
		state[i] = chain[i];
	return plain;
}
