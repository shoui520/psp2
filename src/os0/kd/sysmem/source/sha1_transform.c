/* SHA-1 compression, with the original writable sixteen-word schedule.
 * The round arithmetic is shared with SsRngSha1Transform's implementation;
 * this entry consumes host-order words and updates the caller's ring in place.
 */
#include "sha1_internal.h"

uint32_t *Sha1Transform(uint32_t block[16], uint32_t state[5])
{
	uint32_t initialA = state[0];
	uint32_t initialB = state[1];
	uint32_t initialC = state[2];
	uint32_t initialD = state[3];
	uint32_t initialE = state[4];
	uint32_t a = initialA;
	uint32_t b = initialB;
	uint32_t c = initialC;
	uint32_t d = initialD;
	uint32_t e = initialE;
	uint32_t word, function;
	uint32_t constant = 0x5a827999;
	int round = -17;

	for (;;) {
		uint32_t next;

		round++;
		e += (a << 5) | (a >> 27);
		word = block[round & 15];
		if (round < 0)
			goto choose;
		word ^= block[(round + 2) & 15];
		word ^= block[(round + 8) & 15];
		word ^= block[(round + 13) & 15];
		word = (word << 1) | (word >> 31);
		block[round & 15] = word;
		if (round < 24) {
			if (round < 4)
				goto choose;
			function = b ^ c ^ d;
			constant = 0x6ed9eba1;
		} else {
			constant = 0xca62c1d6;
			function = b ^ c;
			if (round < 44) {
				function = (function & d) + (b & c);
				constant = 0x8f1bbcdc;
			} else {
				function ^= d;
				if (round == 63)
					break;
			}
		}
		goto advance;

choose:
		function = (c & b) + (d & ~b);
advance:
		next = e + word + function + constant;
		e = d;
		d = c;
		c = (b >> 2) | (b << 30);
		b = a;
		a = next;
	}
	state[0] = initialA + e + word + function + constant;
	state[1] = initialB + a;
	state[2] = initialC + ((b >> 2) | (b << 30));
	state[3] = initialD + c;
	state[4] = initialE + d;
	return state + 5;
}
