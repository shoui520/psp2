/*
 * SKBL: ARZL decompression and ARM Thumb-2 branch/MOVT filter (arlz.c, name
 * ours: Sony's function names sceArlzDecode and sceArlzArmFilter come from
 * KblArchive's debug messages).
 *
 * Used by SKBL to unpack the TrustZone SceSysmem archive in Compat SRAM and
 * the Non-secure KBL (NSKBL) image at 0x51000000.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <scetypes.h>

#define SCE_ARLZ_ERROR_INVALID_DATA	((SceSSize)0x80560200)
#define SCE_ARLZ_ERROR_BUFFER_OVERFLOW	((SceSSize)0x80560201)

#define ARLZ_LITERAL_CTX_COUNT		8
#define ARLZ_LITERAL_TREE_SIZE		255
#define ARLZ_DIST_TREE_SIZE		31
#define ARLZ_SHORT_DIST_TREE_SIZE	7
#define ARLZ_DIST_SLOTS			18
#define ARLZ_STATE_COUNT		8
#define ARLZ_LEN_SLOTS			8
#define ARLZ_EOF_LEN			255

/* name ours: range-coder probability tables (3240 bytes on stack) */
typedef struct SceArlzProbs {
	uint8_t lit[ARLZ_LITERAL_CTX_COUNT][ARLZ_LITERAL_TREE_SIZE];	/* 0x000..0x7f7 */
	uint8_t distTree[ARLZ_DIST_TREE_SIZE][8];			/* 0x7f8..0x8ef */
	uint8_t shortDistTree[ARLZ_SHORT_DIST_TREE_SIZE][8];		/* 0x8f0..0x927 */
	uint8_t distBits[ARLZ_DIST_SLOTS][4][8];			/* 0x928..0xb67 */
	uint8_t isMatch[ARLZ_LEN_SLOTS][ARLZ_STATE_COUNT];		/* 0xb68..0xba7 */
	uint8_t lenBits[ARLZ_LEN_SLOTS][4][8];				/* 0xba8..0xca7 */
} SceArlzProbs;

_Static_assert(sizeof(SceArlzProbs) == 0xca8, "SceArlzProbs size");

#define ARLZ_NORMALIZE(range, code, p) do {		\
	if (((range) >> 24) == 0) {			\
		(range) <<= 8;				\
		(code) = ((code) << 8) | *(p)++;	\
	}						\
} while (0)

/*
 * name ours: decodes the variable-length suffix bits for a match length
 * (table = probs->lenBits, slot = 0..6) or match distance
 * (table = probs->distBits, slot = 0..17).
 */
static inline uint32_t ArlzDecodeVarBits(uint8_t table[][4][8], int slot,
					 uint32_t posCtx, int lowCtx,
					 uint32_t *pRange, uint32_t *pCode,
					 const uint8_t **pSrc)
{
	uint32_t range = *pRange;
	uint32_t code = *pCode;
	const uint8_t *p = *pSrc;
	uint8_t *probPtr;
	uint32_t prob, nextProb, bound, val, high;
	int extra = slot - 3;

	ARLZ_NORMALIZE(range, code, p);
	probPtr = &table[slot][(posCtx << slot) & 3][lowCtx & 7];
	prob = probPtr[0];
	nextProb = prob - (prob >> 4);
	bound = prob * (range >> 8);
	if (code >= bound) {
		code -= bound;
		range -= bound;
		high = 4;
		val = 2;
	} else {
		nextProb += 15;
		range = bound;
		high = 6;
		val = 3;
	}
	probPtr[0] = (uint8_t)nextProb;

	if (extra >= 0) {
		if (extra != 0) {
			ARLZ_NORMALIZE(range, code, p);
			prob = (uint8_t)nextProb;
			nextProb = prob - (prob >> 4);
			bound = prob * (range >> 8);
			if (code >= bound) {
				range -= bound;
				code -= bound;
			} else {
				nextProb += 15;
				high++;
				range = bound;
			}
			probPtr[0] = (uint8_t)nextProb;

			if (extra != 1) {
				ARLZ_NORMALIZE(range, code, p);
				do {
					range >>= 1;
					extra--;
					high = (high << 1) + (code < range);
					if (code >= range)
						code -= range;
				} while (extra != 1);
			}
			high <<= 1;
		}

		ARLZ_NORMALIZE(range, code, p);
		prob = probPtr[24];
		nextProb = prob - (prob >> 4);
		bound = prob * (range >> 8);
		if (code >= bound) {
			code -= bound;
			range -= bound;
		} else {
			high++;
			range = bound;
			nextProb += 15;
		}
		val = high;
		probPtr[24] = (uint8_t)nextProb;
	}

	if (slot > 0) {
		ARLZ_NORMALIZE(range, code, p);
		prob = probPtr[8];
		val <<= 1;
		nextProb = prob - (prob >> 4);
		bound = prob * (range >> 8);
		if (code >= bound) {
			range -= bound;
			code -= bound;
		} else {
			nextProb += 15;
			val++;
			range = bound;
		}
		probPtr[8] = (uint8_t)nextProb;

		if (slot != 1) {
			ARLZ_NORMALIZE(range, code, p);
			prob = probPtr[16];
			val <<= 1;
			nextProb = prob - (prob >> 4);
			bound = prob * (range >> 8);
			if (code >= bound) {
				code -= bound;
				range -= bound;
			} else {
				val++;
				range = bound;
				nextProb += 15;
			}
			probPtr[16] = (uint8_t)nextProb;
		}
	}

	*pRange = range;
	*pCode = code;
	*pSrc = p;
	return val;
}

SceSSize sceArlzDecode(void *dst, SceSize dstSize, const void *src, const void **nextSrc)
{
	const uint8_t *in = (const uint8_t *)src;
	const uint8_t *p = in + 5;
	uint8_t *out = (uint8_t *)dst;
	uint8_t *outEnd = out + dstSize;
	uint8_t prop = in[0];
	uint32_t code = ((uint32_t)in[1] << 24) | ((uint32_t)in[2] << 16) |
			((uint32_t)in[3] << 8) | (uint32_t)in[4];
	SceArlzProbs probs;
	uint32_t range;
	uint32_t litShift;
	int sym;
	int state;

	if (prop & 0x80) {
		if (dstSize < code)
			return SCE_ARLZ_ERROR_BUFFER_OVERFLOW;
		memcpy(dst, p, code);
		if (nextSrc != NULL)
			*nextSrc = p + code;
		return (SceSSize)code;
	}

	memset(&probs, ((8 - ((prop >> 3) & 3)) << 4) & 0xf0, sizeof(probs));
	if (out == outEnd)
		return SCE_ARLZ_ERROR_BUFFER_OVERFLOW;

	litShift = prop & 7;
	sym = 0;
	state = 0;
	range = 0xffffffffU;

decode_literal:
	{
		uint32_t ctx = ((uint32_t)sym & ~0x700U) | (((uintptr_t)out & 7) << 8);
		uint8_t *litProb = probs.lit[(ctx >> litShift) & 7];

		sym = 1;
		do {
			uint32_t prob, nextProb, bound;

			ARLZ_NORMALIZE(range, code, p);
			prob = litProb[sym - 1];
			sym <<= 1;
			bound = prob * (range >> 8);
			nextProb = prob - (prob >> 3);
			range -= bound;
			if (code < bound) {
				sym++;
				nextProb += 31;
				range = bound;
			} else {
				code -= bound;
			}
			litProb[(sym >> 1) - 1] = (uint8_t)nextProb;
		} while (sym <= 0xff);
	}

	for (;;) {
		uint8_t *nextOut = out + 1;
		uint8_t *matchProb = &probs.isMatch[0][state];
		uint32_t prob, nextProb, bound;
		uint32_t len, dist, treeLimit, treeIdx, rem;
		uint8_t *treeBase, *matchEnd, *copyDst, *copyRef;
		intptr_t negDist;
		int lenSlot;

		*out = (uint8_t)sym;

		/* isMatch[0][state]: 0 = literal, 1 = match */
		ARLZ_NORMALIZE(range, code, p);
		prob = matchProb[0];
		nextProb = prob - (prob >> 4);
		bound = prob * (range >> 8);
		if (code >= bound) {
			out = nextOut;
			state = (state != 0) ? state - 1 : 0;
			range -= bound;
			code -= bound;
			matchProb[0] = (uint8_t)nextProb;
			if (out != outEnd)
				goto decode_literal;
			return SCE_ARLZ_ERROR_BUFFER_OVERFLOW;
		}
		matchProb[0] = (uint8_t)(nextProb + 15);
		range = bound;

		/* isMatch[1][state]: 0 = 2-byte match (len = 1), 1 = longer match */
		ARLZ_NORMALIZE(range, code, p);
		prob = matchProb[8];
		nextProb = prob - (prob >> 4);
		bound = prob * (range >> 8);
		if (code >= bound) {
			range -= bound;
			code -= bound;
			matchProb[8] = (uint8_t)nextProb;
			lenSlot = -1;
			len = 1;
			treeBase = &probs.shortDistTree[0][0];
			treeLimit = 64;
		} else {
			matchProb[8] = (uint8_t)(nextProb + 15);
			range = bound;
			for (lenSlot = 0; lenSlot < 6; lenSlot++) {
				ARLZ_NORMALIZE(range, code, p);
				prob = matchProb[(lenSlot + 2) * 8];
				nextProb = prob - (prob >> 4);
				bound = prob * (range >> 8);
				if (code >= bound) {
					matchProb[(lenSlot + 2) * 8] = (uint8_t)nextProb;
					code -= bound;
					range -= bound;
					break;
				}
				matchProb[(lenSlot + 2) * 8] = (uint8_t)(nextProb + 15);
				range = bound;
			}
			len = ArlzDecodeVarBits(probs.lenBits, lenSlot, (uintptr_t)nextOut,
						state, &range, &code, &p);
			if (len == ARLZ_EOF_LEN) {
				if (nextSrc != NULL)
					*nextSrc = p;
				return (SceSSize)(nextOut - (uint8_t *)dst);
			}
			treeBase = &probs.distTree[0][lenSlot + 1];
			if (len > 2) {
				treeLimit = 256;
			} else {
				treeBase += sizeof(probs.distTree);
				treeLimit = 64;
			}
		}

		treeIdx = 8;
		do {
			uint8_t *nodeProb = treeBase + treeIdx - 8;

			ARLZ_NORMALIZE(range, code, p);
			prob = *nodeProb;
			treeIdx <<= 1;
			bound = prob * (range >> 8);
			nextProb = prob - (prob >> 3);
			range -= bound;
			if (code < bound) {
				treeIdx += 8;
				range = bound;
				nextProb += 31;
			} else {
				code -= bound;
			}
			*nodeProb = (uint8_t)nextProb;
		} while (treeIdx < treeLimit);

		rem = treeIdx - treeLimit;
		if (rem != 0) {
			int distSlot = (int)(rem >> 3) - 1;

			dist = ArlzDecodeVarBits(probs.distBits, distSlot,
						 (len > 2) ? 1U : 0U, lenSlot,
						 &range, &code, &p) - 1;
		} else {
			dist = 0;
		}

		if ((size_t)(nextOut - (uint8_t *)dst) <= dist)
			return SCE_ARLZ_ERROR_INVALID_DATA;

		matchEnd = nextOut + len;
		negDist = ~(intptr_t)dist;
		sym = *(const int8_t *)(nextOut + negDist);
		if (outEnd <= matchEnd)
			return SCE_ARLZ_ERROR_BUFFER_OVERFLOW;

		copyDst = nextOut;
		if ((uintptr_t)matchEnd > 30 && (uintptr_t)(matchEnd - 31) > (uintptr_t)(out + 2)) {
			uint8_t *d = out - 1;
			uint8_t *s = out + negDist;
			intptr_t pldOff = negDist + 35;

			do {
				uint8_t b0;

				__builtin_prefetch(copyDst + 34);
				d += 32;
				d[-30] = (uint8_t)sym;
				s += 32;
				b0 = s[-30];
				__builtin_prefetch(copyDst + pldOff);
				copyDst += 32;
				d[-29] = b0;
				d[-28] = s[-29];
				d[-27] = s[-28];
				d[-26] = s[-27];
				d[-25] = s[-26];
				d[-24] = s[-25];
				d[-23] = s[-24];
				d[-22] = s[-23];
				d[-21] = s[-22];
				d[-20] = s[-21];
				d[-19] = s[-20];
				d[-18] = s[-19];
				d[-17] = s[-18];
				d[-16] = s[-17];
				d[-15] = s[-16];
				d[-14] = s[-15];
				d[-13] = s[-14];
				d[-12] = s[-13];
				d[-11] = s[-12];
				d[-10] = s[-11];
				d[-9] = s[-10];
				d[-8] = s[-9];
				d[-7] = s[-8];
				d[-6] = s[-7];
				d[-5] = s[-6];
				d[-4] = s[-5];
				d[-3] = s[-4];
				d[-2] = s[-3];
				d[-1] = s[-2];
				d[0] = s[-1];
				d[1] = s[0];
				sym = *(const int8_t *)&s[1];
			} while ((uintptr_t)(copyDst + 1) < (uintptr_t)(matchEnd - 31));
		}

		copyRef = copyDst + negDist;
		copyDst--;
		do {
			*++copyDst = (uint8_t)sym;
			sym = *(const int8_t *)++copyRef;
		} while (copyRef != matchEnd + negDist);

		state = 7;
		out = matchEnd;
	}
}

/* name ours: Thumb-2 MOVT / BL bitfield layout used by sceArlzArmFilter */
typedef union SceArlzThumbInsn {
	uint32_t word;
	struct {
		uint32_t imm11_hi : 11;
		uint32_t op_hi    : 5;
		uint32_t imm11_lo : 11;
		uint32_t op_lo    : 5;
	} bl;
	struct {
		uint32_t lo  : 11;
		uint32_t hi  : 11;
		uint32_t top : 10;
	} target;
	struct {
		uint32_t imm4 : 4;
		uint32_t pad0 : 20;
		uint32_t rd   : 4;
		uint32_t pad1 : 4;
	} movt;
} SceArlzThumbInsn;

SceSSize sceArlzArmFilter(void *buffer, SceSize size, int encode)
{
	uint32_t *p = (uint32_t *)buffer;
	uint8_t *end = (uint8_t *)buffer + size;

	if (size == 0)
		return (SceSSize)size;

	if (encode == 0) {
		do {
			uint32_t *next = p;
			uint32_t insn = *next++;
			uint32_t op;

			__asm__("pld [%0, #0x84]" : : "r"(p));
			op = insn & 0xf800f800U;
			if ((insn & 0x8000fbf0U) == 0x0000f2c0U) {
				SceArlzThumbInsn u, v;

				u.word = insn;
				v.word = (insn & ~0xfU) | u.movt.rd;
				v.movt.rd = u.movt.imm4;
				*p = v.word;
			} else if (op == 0xf800f000U) {
				SceArlzThumbInsn u, t, v;
				uint32_t rel;

				u.word = insn;
				t.word = insn >> 16;
				t.target.hi = u.bl.imm11_hi;
				rel = t.word - (((uint8_t *)next - (uint8_t *)buffer) >> 1);
				v.word = 0xf800f000U;
				v.bl.imm11_lo = rel;
				v.bl.imm11_hi = rel >> 11;
				*p = v.word;
			} else if ((op >> 27) == 0x1e) {
				next = (uint32_t *)((uint8_t *)p + 2);
			}
			p = next;
		} while ((uint8_t *)p < end);
	} else {
		do {
			uint32_t *next = p;
			uint32_t insn = *next++;
			uint32_t op;

			__asm__("pld [%0, #0x84]" : : "r"(p));
			op = insn & 0xf800f800U;
			if ((insn & 0x8000fbf0U) == 0x0000f2c0U) {
				SceArlzThumbInsn u, v;

				u.word = insn;
				v.word = (insn & ~0xfU) | u.movt.rd;
				v.movt.rd = u.movt.imm4;
				*p = v.word;
			} else if (op == 0xf800f000U) {
				SceArlzThumbInsn u, t, v;
				uint32_t rel;

				u.word = insn;
				t.word = insn >> 16;
				t.target.hi = u.bl.imm11_hi;
				rel = t.word + (((uint8_t *)next - (uint8_t *)buffer) >> 1);
				v.word = 0xf800f000U;
				v.bl.imm11_lo = rel;
				v.bl.imm11_hi = rel >> 11;
				*p = v.word;
			} else if ((op >> 27) == 0x1e) {
				next = (uint32_t *)((uint8_t *)p + 2);
			}
			p = next;
		} while ((uint8_t *)p < end);
	}

	return (SceSSize)size;
}
