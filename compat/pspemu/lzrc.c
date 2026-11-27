/*
 * ScePspemu LZRC (Lempel-Ziv Range Coder) decompressor (0x81002a2c).
 */
#include "host_common.h"
#include <string.h>

#define PSPEMU_LZRC_ERR_BAD_DIST    (-2141847040) /* 0x80520000 */
#define PSPEMU_LZRC_ERR_OVERFLOW    (-2141847039) /* 0x80520001 */

#define LZRC_PROB_TABLE_SIZE        3240U /* 0xCA8 */
#define LZRC_OFF_DIST_LONG          2033U
#define LZRC_OFF_DIST_SHORT         2281U
#define LZRC_OFF_DIST_EXTRA         2344U
#define LZRC_OFF_MATCH_STATE        2920U
#define LZRC_OFF_LEN_EXTRA          2984U

static inline uint32_t lzrcReadBe32(const uint8_t *src)
{
	return ((uint32_t)src[0] << 24) |
	       ((uint32_t)src[1] << 16) |
	       ((uint32_t)src[2] << 8) |
	       (uint32_t)src[3];
}

static inline uint32_t lzrcDecodeBitShift3(uint32_t *range, uint32_t *code,
                                           const uint8_t **src, uint8_t *prob)
{
	uint32_t curRange = *range;
	uint32_t curCode = *code;
	uint32_t probVal;
	uint32_t bound;
	uint32_t updated;

	if ((curRange & 0xFF000000U) == 0U) {
		curRange <<= 8;
		curCode = (curCode << 8) | (uint32_t)(*(*src)++);
	}

	probVal = *prob;
	bound = probVal * (curRange >> 8);
	updated = probVal - (probVal >> 3);

	if (curCode >= bound) {
		*code = curCode - bound;
		*range = curRange - bound;
		*prob = (uint8_t)updated;
		return 0U;
	}

	*code = curCode;
	*range = bound;
	*prob = (uint8_t)(updated + 31U);
	return 1U;
}

static inline uint32_t lzrcDecodeBitShift4(uint32_t *range, uint32_t *code,
                                           const uint8_t **src, uint8_t *prob)
{
	uint32_t curRange = *range;
	uint32_t curCode = *code;
	uint32_t probVal;
	uint32_t bound;
	uint32_t updated;

	if ((curRange & 0xFF000000U) == 0U) {
		curRange <<= 8;
		curCode = (curCode << 8) | (uint32_t)(*(*src)++);
	}

	probVal = *prob;
	bound = probVal * (curRange >> 8);
	updated = probVal - (probVal >> 4);

	if (curCode >= bound) {
		*code = curCode - bound;
		*range = curRange - bound;
		*prob = (uint8_t)updated;
		return 0U;
	}

	*code = curCode;
	*range = bound;
	*prob = (uint8_t)(updated + 15U);
	return 1U;
}

static inline uint32_t lzrcDecodeNumber(uint32_t *range, uint32_t *code,
                                        const uint8_t **src, uint8_t *baseProb,
                                        int bitGroups, int extraBits)
{
	uint32_t value;
	uint32_t highAcc;

	if (lzrcDecodeBitShift4(range, code, src, &baseProb[0]) != 0U) {
		highAcc = 6U;
		value = 3U;
	} else {
		highAcc = 4U;
		value = 2U;
	}

	if (extraBits >= 0) {
		if (extraBits != 0) {
			if (lzrcDecodeBitShift4(range, code, src, &baseProb[0]) != 0U)
				highAcc++;

			if (extraBits != 1) {
				if ((*range & 0xFF000000U) == 0U) {
					*range <<= 8;
					*code = (*code << 8) | (uint32_t)(*(*src)++);
				}
				do {
					*range >>= 1;
					extraBits--;
					highAcc = (highAcc << 1) | ((*code < *range) ? 1U : 0U);
					if (*code >= *range)
						*code -= *range;
				} while (extraBits != 1);
			}
			highAcc <<= 1;
		}
		if (lzrcDecodeBitShift4(range, code, src, &baseProb[24]) != 0U)
			highAcc++;
		value = highAcc;
	}

	if (bitGroups > 0) {
		value = (value << 1) | lzrcDecodeBitShift4(range, code, src, &baseProb[8]);
		if (bitGroups != 1)
			value = (value << 1) | lzrcDecodeBitShift4(range, code, src, &baseProb[16]);
	}

	return value;
}

int sceLzrcDecompress(void *dst, size_t dstSize, const void *src, const void **outEnd)
{
	const uint8_t *srcBytes = (const uint8_t *)src;
	uint8_t header = srcBytes[0];
	uint32_t code = lzrcReadBe32(srcBytes + 1);
	const uint8_t *srcCur = srcBytes + 5;
	uint8_t *dstStart = (uint8_t *)dst;
	uint8_t *dstEnd = dstStart + dstSize;
	uint8_t *dstCur = dstStart;
	uint32_t range = 0xFFFFFFFFU;
	uint32_t litShift;
	uint32_t pendingByte = 0U;
	int state = 0;
	uint8_t probs[LZRC_PROB_TABLE_SIZE];

	if ((header & 0x80U) != 0U) {
		if (dstSize < code)
			return PSPEMU_LZRC_ERR_OVERFLOW;
		memcpy(dstStart, srcCur, code);
		if (outEnd != NULL)
			*outEnd = srcCur + code;
		return (int)code;
	}

	memset(probs, (int)(uint8_t)(16U * (8U - ((header >> 3) & 3U))), sizeof(probs));
	litShift = (uint32_t)(header & 7U);

	if (dstCur == dstEnd)
		return PSPEMU_LZRC_ERR_OVERFLOW;

	for (;;) {
		/* Decode one literal byte */
		uint32_t ctxInput = (pendingByte & 0xFFU) | (((uintptr_t)dstCur & 7U) << 8);
		uint32_t litCtx = (ctxInput >> litShift) & 7U;
		uint8_t *litProbs = &probs[255U * litCtx];
		uint32_t sym = 1U;

		do {
			uint32_t bit = lzrcDecodeBitShift3(&range, &code, &srcCur, &litProbs[sym - 1U]);
			sym = (sym << 1) | bit;
		} while (sym <= 255U);

		pendingByte = sym & 0xFFU;

		/* Write pending byte and process subsequent matches while isMatch == 1 */
		for (;;) {
			uint8_t *nextOut;
			uint8_t *stateProbs = &probs[LZRC_OFF_MATCH_STATE + (uint32_t)state];
			int lenClass;
			uint32_t matchLen;
			uint32_t distLimit;
			uint8_t *distTreeBase;
			uint32_t distNode;
			uint32_t distOffset;
			uint32_t dist;

			*dstCur = (uint8_t)pendingByte;
			nextOut = dstCur + 1;

			if (lzrcDecodeBitShift4(&range, &code, &srcCur, &stateProbs[0]) == 0U) {
				dstCur = nextOut;
				if (state != 0)
					state--;
				if (dstCur == dstEnd)
					return PSPEMU_LZRC_ERR_OVERFLOW;
				break;
			}

			if (lzrcDecodeBitShift4(&range, &code, &srcCur, &stateProbs[8]) == 0U) {
				lenClass = -1;
				matchLen = 1U;
			} else {
				int extraLen;
				uint32_t lenProbIdx;

				lenClass = 0;
				while (lenClass < 6 &&
				       lzrcDecodeBitShift4(&range, &code, &srcCur,
				                           &stateProbs[16 + 8 * lenClass]) != 0U) {
					lenClass++;
				}

				extraLen = lenClass - 3;
				lenProbIdx = (uint32_t)(32 * lenClass) |
				             (8U * ((((uintptr_t)nextOut) << lenClass) & 3U)) |
				             ((uint32_t)state & 7U);
				matchLen = lzrcDecodeNumber(&range, &code, &srcCur,
				                            &probs[LZRC_OFF_LEN_EXTRA + lenProbIdx],
				                            lenClass, extraLen);
				if (matchLen == 255U) {
					if (outEnd != NULL)
						*outEnd = srcCur;
					return (int)(nextOut - dstStart);
				}
			}

			if (matchLen > 2U) {
				distLimit = 256U;
				distTreeBase = &probs[LZRC_OFF_DIST_LONG + (uint32_t)lenClass];
			} else {
				distLimit = 64U;
				distTreeBase = &probs[LZRC_OFF_DIST_SHORT + (uint32_t)lenClass];
			}

			distNode = 8U;
			do {
				uint32_t bit = lzrcDecodeBitShift3(&range, &code, &srcCur, &distTreeBase[distNode]);
				distNode = (distNode << 1) + (bit << 3);
			} while (distNode < distLimit);

			distOffset = distNode - distLimit;
			if (distOffset != 0U) {
				uint32_t slot = distOffset >> 3;
				int slotIdx = (int)slot - 1;
				int extraDist = (int)slot - 4;
				uint32_t lenFlag = (matchLen > 2U) ? 1U : 0U;
				uint32_t distProbIdx = (8U * ((lenFlag << slotIdx) & 3U)) |
				                       (uint32_t)(32 * slotIdx) |
				                       ((uint32_t)lenClass & 7U);
				dist = lzrcDecodeNumber(&range, &code, &srcCur,
				                        &probs[LZRC_OFF_DIST_EXTRA + distProbIdx],
				                        slotIdx, extraDist) - 1U;
			} else {
				dist = 0U;
			}

			if ((size_t)(nextOut - dstStart) <= dist)
				return PSPEMU_LZRC_ERR_BAD_DIST;
			if (nextOut + matchLen >= dstEnd)
				return PSPEMU_LZRC_ERR_OVERFLOW;

			{
				const uint8_t *copySrc = nextOut - dist - 1U;
				uint8_t *copyDst = nextOut;
				uint32_t i;

				for (i = 0U; i < matchLen; i++)
					copyDst[i] = copySrc[i];
				pendingByte = copySrc[matchLen];
			}

			state = 7;
			dstCur = nextOut + matchLen;
		}
	}
}
