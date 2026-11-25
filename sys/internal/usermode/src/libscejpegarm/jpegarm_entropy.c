/*
 * SceJpegArm bitstream reader, restart-marker handling, and baseline/progressive
 * Huffman entropy decoders.
 */

#include "jpegarm_internal.h"

static inline uint32_t ror32(uint32_t val, int shift)
{
	unsigned int s = (unsigned int)shift & 31U;
	return (s == 0U) ? val : ((val >> s) | (val << (32U - s)));
}

static inline uint8_t clampByteScalar(int val)
{
	if (val < 0)
		return 0;
	if (val > 255)
		return 255;
	return (uint8_t)val;
}

uint32_t jpegArmRefillBitBuf(const uint8_t **streamCur, const uint8_t *streamEnd, uint32_t curBits)
{
	const uint8_t *endMinus1 = streamEnd - 1;
	const uint8_t *ptr = *streamCur;
	uint32_t bits = curBits;

	if (ptr >= endMinus1)
		return bits;

	uint8_t b0 = ptr[0];
	uint8_t b1 = ptr[1];
	ptr++;
	bits = (bits & 0xFFFF00FFU) | ((uint32_t)b0 << 8);

	if (b0 == 0xFF) {
		if (b1 != 0x00) {
			*streamCur = ptr - 1;
			return bits;
		}
		if (ptr < endMinus1)
			ptr++;
		b1 = *ptr;
	}

	bits = (bits & 0xFFFFFF00U) | (uint32_t)b1;
	uint8_t b2;
	if (ptr >= endMinus1) {
		b2 = b1;
	} else {
		ptr++;
		b2 = *ptr;
	}

	if (b1 == 0xFF) {
		if (b2 != 0x00) {
			*streamCur = ptr - 1;
			return bits;
		}
		if (ptr < endMinus1)
			ptr++;
		b2 = *ptr;
	}

	if (ptr < endMinus1)
		ptr++;
	bits = (bits & 0x00FFFFFFU) | ((uint32_t)b2 << 24);

	if (b2 == 0xFF) {
		if (*ptr == 0x00) {
			if (ptr < endMinus1)
				ptr++;
		} else {
			ptr--;
		}
	}

	*streamCur = ptr;
	return bits;
}

static inline uint32_t refillThreeBytes(const uint8_t **streamCur, const uint8_t *streamEnd, uint32_t curBits)
{
	const uint8_t *ptr = *streamCur;
	if (ptr + 8 >= streamEnd)
		return jpegArmRefillBitBuf(streamCur, streamEnd, curBits);

	uint32_t bits = (curBits & 0xFFFF00FFU) | ((uint32_t)ptr[0] << 8);
	uint8_t b1 = ptr[1];

	if (ptr[0] == 0xFF) {
		if (b1 != 0x00)
			return bits;
		ptr++;
		b1 = ptr[1];
	}

	bits = (bits & 0xFFFFFF00U) | (uint32_t)b1;
	uint8_t b2 = ptr[2];

	if (b1 == 0xFF) {
		ptr++;
		if (b2 != 0x00) {
			*streamCur = ptr;
			return bits;
		}
		b2 = ptr[2];
	}

	bits = (bits & 0x00FFFFFFU) | ((uint32_t)b2 << 24);
	if (b2 == 0xFF) {
		if (ptr[3] == 0x00)
			ptr++;
	}
	*streamCur = ptr + 3;
	return bits;
}

static inline void ensureBits(uint32_t *bitBuf, int *bitCount, const uint8_t **streamCur, const uint8_t *streamEnd)
{
	if (*bitCount >= 0) {
		*bitCount -= 24;
		uint32_t aligned = ror32(*bitBuf, *bitCount);
		uint32_t filled = refillThreeBytes(streamCur, streamEnd, aligned);
		*bitBuf = ror32(filled, -(*bitCount));
	}
}

static inline uint32_t readExtraBits(uint32_t *bitBuf, int *bitCount, int nbits,
	const uint8_t **streamCur, const uint8_t *streamEnd)
{
	uint32_t val = (*bitBuf) >> (32 - nbits);
	*bitBuf = ror32(*bitBuf, -nbits);
	*bitCount += nbits;
	if (*bitCount > 0) {
		*bitCount -= 24;
		uint32_t aligned = ror32(*bitBuf, *bitCount);
		uint32_t filled = refillThreeBytes(streamCur, streamEnd, aligned);
		*bitBuf = ror32(filled, -(*bitCount));
		if (*bitCount >= -15)
			val = (val & 0xFFFFFF00U) | (*bitBuf & 0xFFU);
	}
	return val;
}

static inline int decodeSlowHuffSymbol(const JpegArmHuffTable *tbl, uint32_t firstByte,
	uint32_t *bitBuf, int *bitCount, const uint8_t **streamCur, const uint8_t *streamEnd,
	int *outShiftBits)
{
	*bitBuf <<= 8;
	*bitCount += 8;
	if (*bitCount >= 0) {
		*bitCount -= 24;
		uint32_t aligned = ror32(*bitBuf, *bitCount);
		uint32_t filled = refillThreeBytes(streamCur, streamEnd, aligned);
		*bitBuf = ror32(filled, -(*bitCount));
	}

	uint32_t code16 = (firstByte << 8) | ((*bitBuf) >> 24);
	int shift = (code16 < tbl->maxCode[7]) ? 7 : 6;
	if (code16 >= tbl->maxCode[6]) {
		shift--;
		if (code16 >= tbl->maxCode[5]) {
			shift--;
			if (code16 >= tbl->maxCode[4]) {
				shift--;
				if (code16 >= tbl->maxCode[3]) {
					shift--;
					if (code16 >= tbl->maxCode[2])
						shift += (code16 < tbl->maxCode[1]) ? -1 : -2;
				}
			}
		}
	}

	int idx = (int)tbl->valOffset[shift] - (int)(code16 >> (shift & 31));
	*outShiftBits = 8 - shift;
	return ((const uint8_t *)tbl)[idx + 759];
}

static inline void saveCheckpoint(const JpegArmDecoder *dec, JpegArmMcuCheckpoint *cp)
{
	cp->streamCur = dec->streamCur;
	cp->bitBuf = dec->bitBuf;
	cp->bitCount = dec->bitCount;
	cp->eobRun = dec->progEobRun;
	cp->dcPred0 = dec->components[0].dcPred;
	cp->outPtr0 = dec->components[0].outPtr;
	cp->coefPtr0 = dec->components[0].coefPtr;
	cp->dcPred1 = dec->components[1].dcPred;
	cp->outPtr1 = dec->components[1].outPtr;
	cp->coefPtr1 = dec->components[1].coefPtr;
	cp->dcPred2 = dec->components[2].dcPred;
	cp->outPtr2 = dec->components[2].outPtr;
	cp->coefPtr2 = dec->components[2].coefPtr;
	cp->dcPred3 = dec->components[3].dcPred;
	cp->outPtr3 = dec->components[3].outPtr;
	cp->coefPtr3 = dec->components[3].coefPtr;
}

int jpegArmDecodeBlockBaseline(JpegArmDecoder *dec, int skipOutput)
{
	int blockIdx = 0;
	JpegArmComponent *comp = dec->mcuCompPtrs[0];
	uint32_t bitBuf = dec->bitBuf;
	int bitCount = dec->bitCount;
	const uint8_t *streamEnd = dec->streamEnd;

	while (comp != NULL) {
		int16_t block[64];
		for (int i = 0; i < 64; i++)
			block[i] = 0;

		const uint8_t *streamCur = dec->streamCur;
		const JpegArmHuffTable *dcTbl = comp->dcTable;
		uint32_t top8 = bitBuf >> 24;
		int fastBits = dcTbl->fastBits[top8];
		int fastVal = dcTbl->fastVal[top8];
		int16_t dcVal = comp->dcPred;

		if (fastBits > 0) {
			bitCount += fastBits;
			dcVal = (int16_t)(dcVal + fastVal);
			bitBuf <<= fastBits;
			comp->dcPred = dcVal;
			ensureBits(&bitBuf, &bitCount, &streamCur, streamEnd);
		} else {
			int cat;
			if (fastBits < 0) {
				int codeLen = -fastBits;
				bitCount += codeLen;
				bitBuf = ror32(bitBuf, -codeLen);
				cat = -fastVal;
			} else {
				int remBits;
				int sym = (int)(int8_t)decodeSlowHuffSymbol(dcTbl, top8, &bitBuf, &bitCount,
					&streamCur, streamEnd, &remBits);
				bitCount += remBits;
				bitBuf = ror32(bitBuf, -remBits);
				cat = -sym;
			}
			ensureBits(&bitBuf, &bitCount, &streamCur, streamEnd);

			if (cat != 0) {
				uint32_t signMask = (bitBuf >> 31) - 1U;
				uint32_t raw = readExtraBits(&bitBuf, &bitCount, cat, &streamCur, streamEnd);
				int diff = (int)(raw - (signMask >> (32 - cat)));
				dcVal = (int16_t)(dcVal + diff);
				comp->dcPred = dcVal;
			}
		}

		block[0] = dcVal;

		const JpegArmHuffTable *acTbl = comp->acTable;
		int k = 0;
		int maxOffset = 0;

		while (k < 63) {
			top8 = bitBuf >> 24;
			int8_t entry = acTbl->fastBits[top8];
			if (entry < 0) {
				int shift = -(entry >> 4);
				int run = entry & 0x0F;
				bitCount += shift;
				bitBuf = ror32(bitBuf, -shift);
				k += run + 1;
				if (k > 63)
					break;
				int byteOff = g_jpegArmZigzagByteOffset[k - 1];
				if (byteOff > maxOffset)
					maxOffset = byteOff;
				*(int16_t *)((uint8_t *)block + byteOff) = acTbl->fastVal[top8];
				if (bitCount > 0)
					ensureBits(&bitBuf, &bitCount, &streamCur, streamEnd);
				continue;
			}

			int sym;
			if ((entry >> 4) == 0) {
				bitBuf <<= (entry & 31);
				bitCount += entry;
				sym = (uint8_t)acTbl->fastVal[top8];
			} else {
				int remBits;
				sym = decodeSlowHuffSymbol(acTbl, top8, &bitBuf, &bitCount,
					&streamCur, streamEnd, &remBits);
				bitBuf <<= (remBits & 31);
				bitCount += remBits;
			}
			ensureBits(&bitBuf, &bitCount, &streamCur, streamEnd);

			int cat = sym & 0x0F;
			int run = sym >> 4;
			if (cat == 0) {
				if (run == 15) {
					k += 16;
					continue;
				}
				k = 63;
				break;
			}

			uint32_t signMask = (bitBuf >> 31) - 1U;
			uint32_t raw = readExtraBits(&bitBuf, &bitCount, cat, &streamCur, streamEnd);
			int coeff = (int)(raw - (signMask >> (32 - cat)));

			k += run + 1;
			if (k > 63)
				break;
			int byteOff = g_jpegArmZigzagByteOffset[k - 1];
			if (byteOff > maxOffset)
				maxOffset = byteOff;
			*(int16_t *)((uint8_t *)block + byteOff) = (int16_t)coeff;
		}

		if (dec->splitState != 0 && streamCur + 4 >= streamEnd)
			return SCE_JPEGARM_ERROR_INPUT_SUSPENDED;
		if (k > 63)
			return SCE_JPEGARM_ERROR_DECODE_ERROR;

		dec->streamCur = streamCur;
		dec->bitBuf = bitBuf;
		dec->bitCount = (int8_t)bitCount;

		if (!skipOutput) {
			uint8_t *dst = comp->outPtr;
			int16_t *quant = comp->quantTable;
			switch (comp->downscale) {
			case 1:
				if (maxOffset <= 63)
					jpegArmIdct8x8Half(block, quant, dst, (int)comp->outStride);
				else
					jpegArmIdct8x8(block, quant, dst, (int)comp->outStride);
				break;
			case 2:
				jpegArmIdct4x4(block, quant, dst, (int)comp->outStride);
				break;
			case 4:
				jpegArmIdct2x2(block, quant, dst, (int)comp->outStride);
				break;
			default:
				dst[0] = clampByteScalar((block[0] * quant[0] + 1024) >> 3);
				break;
			}
		}

		comp->outPtr += dec->mcuBlockSteps[blockIdx];
		blockIdx++;
		comp = dec->mcuCompPtrs[blockIdx];
	}

	return 0;
}

__attribute__((noinline))
int jpegArmRestoreMcuCheckpoint(JpegArmDecoder *dec, int err, int16_t mcuCol, const JpegArmMcuCheckpoint *cp)
{
	if (err >= 0)
		return err;

	if (err == SCE_JPEGARM_ERROR_INPUT_SUSPENDED) {
		if (dec->splitState != 0) {
			dec->streamCur = cp->streamCur;
			dec->bitBuf = cp->bitBuf;
			dec->bitCount = cp->bitCount;
			dec->progEobRun = cp->eobRun;
			dec->components[0].dcPred = cp->dcPred0;
			dec->components[0].outPtr = cp->outPtr0;
			dec->components[0].coefPtr = cp->coefPtr0;
			dec->components[1].dcPred = cp->dcPred1;
			dec->components[1].outPtr = cp->outPtr1;
			dec->components[1].coefPtr = cp->coefPtr1;
			dec->components[2].dcPred = cp->dcPred2;
			dec->components[2].outPtr = cp->outPtr2;
			dec->components[2].coefPtr = cp->coefPtr2;
			dec->components[3].dcPred = cp->dcPred3;
			dec->components[3].outPtr = cp->outPtr3;
			dec->components[3].coefPtr = cp->coefPtr3;
			dec->mcuColOffset = mcuCol;
		} else {
			dec->curMcuRow = -1;
			return SCE_JPEGARM_ERROR_DECODE_ERROR;
		}
	} else {
		dec->curMcuRow = -1;
	}

	return err;
}

int jpegArmHandleRestartMarker(JpegArmDecoder *dec, int mcuCol)
{
	const uint8_t *ptr = dec->streamCur;
	const uint8_t *end = dec->streamEnd;
	const uint8_t *limit = ptr + 1536;

	while (ptr + 1 < end) {
		if (*ptr == 0xFF) {
			uint8_t marker = ptr[1];
			if (marker == dec->nextRestartMarker) {
				dec->components[0].dcPred = 0;
				dec->components[1].dcPred = 0;
				dec->components[2].dcPred = 0;
				dec->components[3].dcPred = 0;
				dec->progEobRun = 0;

				const uint8_t *cur = ptr + 2;
				uint32_t bits = 0;
				int loaded = 0;

				for (int i = 0; i < 4; i++) {
					if (cur >= end)
						break;
					uint8_t byte = *cur;
					if (byte == 0xFF) {
						if (cur + 1 >= end) {
							cur++;
							break;
						}
						if (cur[1] != 0x00)
							break;
						cur++;
					}
					cur++;
					bits |= (uint32_t)byte << (24 - i * 8);
					loaded++;
				}

				dec->streamCur = cur;
				dec->bitBuf = bits;
				dec->bitCount = (int8_t)(8 - loaded * 8);

				if (dec->splitState != 0 && (dec->bitCount > 0 || cur >= end)) {
					dec->restartCountdown = 1;
					return SCE_JPEGARM_ERROR_INPUT_SUSPENDED;
				}

				dec->nextRestartMarker = (uint8_t)((marker + 1) & 0xF7);
				dec->restartCountdown = dec->restartInterval;
				return 0;
			}

			if (marker == 0xD9) {
				dec->streamCur = ptr;
				return 0xD9;
			}
			if (dec->isProgressive != 0 &&
				dec->curMcuRow + 1 >= (int)dec->scanMcuRows &&
				mcuCol + 1 >= (int)dec->scanMcuCols) {
				return 0;
			}
			break;
		}
		ptr++;
		if (ptr == limit)
			break;
	}

	if (dec->splitState != 0) {
		dec->restartCountdown = 1;
		return SCE_JPEGARM_ERROR_INPUT_SUSPENDED;
	}
	return SCE_JPEGARM_ERROR_BAD_RESTART_MARKER;
}

int jpegArmDecodeRowProgressiveDcFirst(JpegArmDecoder *dec)
{
	JpegArmMcuCheckpoint cp;
	int col = dec->mcuColOffset;
	int bitCount = dec->bitCount;
	uint32_t bitBuf = dec->bitBuf;
	uint8_t approxLow = dec->approxLow;
	const uint8_t *streamEnd = dec->streamEnd;

	for (;;) {
		if (dec->splitState != 0)
			saveCheckpoint(dec, &cp);

		const uint8_t *streamCur = dec->streamCur;
		int blockIdx = 0;
		JpegArmComponent *comp = dec->mcuCompPtrs[0];

		while (comp != NULL) {
			const JpegArmHuffTable *dcTbl = comp->dcTable;
			uint32_t top8 = bitBuf >> 24;
			int fastBits = dcTbl->fastBits[top8];
			int fastVal = dcTbl->fastVal[top8];
			int16_t dcVal = comp->dcPred;

			if (fastBits > 0) {
				bitCount += fastBits;
				dcVal = (int16_t)(dcVal + fastVal);
				bitBuf <<= fastBits;
				comp->dcPred = dcVal;
				ensureBits(&bitBuf, &bitCount, &streamCur, streamEnd);
			} else {
				int cat;
				if (fastBits < 0) {
					int codeLen = -fastBits;
					bitCount += codeLen;
					bitBuf = ror32(bitBuf, -codeLen);
					cat = -fastVal;
				} else {
					int remBits;
					int sym = (int)(int8_t)decodeSlowHuffSymbol(dcTbl, top8, &bitBuf, &bitCount,
						&streamCur, streamEnd, &remBits);
					bitCount += remBits;
					bitBuf = ror32(bitBuf, -remBits);
					cat = -sym;
				}
				ensureBits(&bitBuf, &bitCount, &streamCur, streamEnd);

				if (cat != 0) {
					uint32_t signMask = (bitBuf >> 31) - 1U;
					uint32_t raw = readExtraBits(&bitBuf, &bitCount, cat, &streamCur, streamEnd);
					dcVal = (int16_t)(dcVal + (int)(raw - (signMask >> (32 - cat))));
					comp->dcPred = dcVal;
				}
			}

			if (dec->splitState != 0 && streamCur + 4 >= streamEnd) {
				dec->mcuColOffset = 0;
				return jpegArmRestoreMcuCheckpoint(dec, SCE_JPEGARM_ERROR_INPUT_SUSPENDED, (int16_t)col, &cp);
			}

			dec->streamCur = streamCur;
			int16_t *dstCoef = comp->coefPtr;
			for (int i = 0; i < 64; i++)
				dstCoef[i] = 0;
			dstCoef[0] = (int16_t)(dcVal << approxLow);

			comp->coefPtr = (int16_t *)((uint8_t *)dstCoef + dec->coefBlockSteps[blockIdx]);
			blockIdx++;
			comp = dec->mcuCompPtrs[blockIdx];
		}

		dec->bitBuf = bitBuf;
		dec->bitCount = (int8_t)bitCount;

		if (dec->restartCountdown != 0) {
			dec->restartCountdown--;
			if (dec->restartCountdown == 0) {
				int rst = jpegArmHandleRestartMarker(dec, col);
				if (rst != 0) {
					dec->mcuColOffset = 0;
					return jpegArmRestoreMcuCheckpoint(dec, rst, (int16_t)col, &cp);
				}
				bitCount = dec->bitCount;
				bitBuf = dec->bitBuf;
			}
		}

		col++;
		if (col >= (int)dec->scanMcuCols) {
			dec->mcuColOffset = 0;
			dec->bitBuf = bitBuf;
			dec->bitCount = (int8_t)bitCount;
			return 0;
		}
	}
}

int jpegArmDecodeRowProgressiveAcFirst(JpegArmDecoder *dec)
{
	JpegArmMcuCheckpoint cp;
	int16_t tmpBlock[64];
	int col = dec->mcuColOffset;
	int bitCount = dec->bitCount;
	uint32_t bitBuf = dec->bitBuf;
	uint16_t eobRun = dec->progEobRun;
	int scanStart = dec->scanStart;
	int scanEnd = dec->scanEnd;
	int approxLow = dec->approxLow;
	const uint8_t *streamEnd = dec->streamEnd;

	for (;;) {
		if (dec->splitState != 0)
			saveCheckpoint(dec, &cp);

		const uint8_t *streamCur = dec->streamCur;
		int blockIdx = 0;
		JpegArmComponent *comp = dec->mcuCompPtrs[0];

		while (comp != NULL) {
			if (eobRun != 0) {
				eobRun--;
				comp->coefPtr = (int16_t *)((uint8_t *)comp->coefPtr + dec->coefBlockSteps[blockIdx]);
				blockIdx++;
				comp = dec->mcuCompPtrs[blockIdx];
				continue;
			}

			int16_t *dstCoef = comp->coefPtr;
			int16_t *work = dstCoef;
			if (dec->splitState != 0) {
				for (int i = 0; i < 64; i++)
					tmpBlock[i] = dstCoef[i];
				work = tmpBlock;
			}

			const JpegArmHuffTable *acTbl = comp->acTable;
			int k = scanStart - 1;

			while (k < scanEnd) {
				uint32_t top8 = bitBuf >> 24;
				int8_t entry = acTbl->fastBits[top8];

				if (entry < 0) {
					int shift = -(entry >> 4);
					int run = entry & 0x0F;
					bitCount += shift;
					bitBuf = ror32(bitBuf, -shift);
					k += run + 1;
					if (k > scanEnd)
						break;
					work[k] = (int16_t)((int)acTbl->fastVal[top8] << approxLow);
					if (bitCount > 0)
						ensureBits(&bitBuf, &bitCount, &streamCur, streamEnd);
					continue;
				}

				int sym;
				if ((entry >> 4) == 0) {
					bitBuf <<= (entry & 31);
					bitCount += entry;
					sym = (uint8_t)acTbl->fastVal[top8];
				} else {
					int remBits;
					sym = decodeSlowHuffSymbol(acTbl, top8, &bitBuf, &bitCount,
						&streamCur, streamEnd, &remBits);
					bitBuf <<= (remBits & 31);
					bitCount += remBits;
				}
				ensureBits(&bitBuf, &bitCount, &streamCur, streamEnd);

				int cat = sym & 0x0F;
				int run = sym >> 4;
				if (cat == 0) {
					if (run == 15) {
						k += 16;
						continue;
					}
					k++;
					eobRun = (uint16_t)((1U << run) - 1U);
					if (run != 0) {
						uint32_t extra = readExtraBits(&bitBuf, &bitCount, run, &streamCur, streamEnd);
						eobRun = (uint16_t)(eobRun + extra);
					}
					break;
				}

				uint32_t signMask = (bitBuf >> 31) - 1U;
				uint32_t raw = readExtraBits(&bitBuf, &bitCount, cat, &streamCur, streamEnd);
				int coeff = (int)(raw - (signMask >> (32 - cat)));

				k += run + 1;
				if (k > scanEnd)
					break;
				work[k] = (int16_t)(coeff << approxLow);
			}

			if (dec->splitState != 0 && streamCur + 4 >= streamEnd) {
				dec->mcuColOffset = 0;
				return jpegArmRestoreMcuCheckpoint(dec, SCE_JPEGARM_ERROR_INPUT_SUSPENDED, (int16_t)col, &cp);
			}
			if (k > scanEnd) {
				dec->mcuColOffset = 0;
				return jpegArmRestoreMcuCheckpoint(dec, SCE_JPEGARM_ERROR_DECODE_ERROR, (int16_t)col, &cp);
			}

			if (dec->splitState != 0) {
				for (int i = 0; i < 64; i++)
					dstCoef[i] = work[i];
			}

			dec->streamCur = streamCur;
			comp->coefPtr = (int16_t *)((uint8_t *)dstCoef + dec->coefBlockSteps[blockIdx]);
			blockIdx++;
			comp = dec->mcuCompPtrs[blockIdx];
		}

		dec->bitBuf = bitBuf;
		dec->bitCount = (int8_t)bitCount;
		dec->progEobRun = eobRun;

		if (dec->restartCountdown != 0) {
			dec->restartCountdown--;
			if (dec->restartCountdown == 0) {
				int rst = jpegArmHandleRestartMarker(dec, col);
				if (rst != 0) {
					dec->mcuColOffset = 0;
					return jpegArmRestoreMcuCheckpoint(dec, rst, (int16_t)col, &cp);
				}
				bitCount = dec->bitCount;
				bitBuf = dec->bitBuf;
				eobRun = dec->progEobRun;
			}
		}

		col++;
		if (col >= (int)dec->scanMcuCols) {
			dec->mcuColOffset = 0;
			dec->bitBuf = bitBuf;
			dec->bitCount = (int8_t)bitCount;
			return 0;
		}
	}
}

int jpegArmDecodeRowProgressiveDcRefine(JpegArmDecoder *dec)
{
	JpegArmMcuCheckpoint cp;
	int col = dec->mcuColOffset;
	int bitCount = dec->bitCount;
	uint32_t bitBuf = dec->bitBuf;
	uint8_t approxLow = dec->approxLow;
	const uint8_t *streamEnd = dec->streamEnd;

	for (;;) {
		if (dec->splitState != 0)
			saveCheckpoint(dec, &cp);

		const uint8_t *streamCur = dec->streamCur;
		int blockIdx = 0;
		JpegArmComponent *comp = dec->mcuCompPtrs[0];

		while (comp != NULL) {
			uint32_t bit = readExtraBits(&bitBuf, &bitCount, 1, &streamCur, streamEnd);

			if (dec->splitState != 0 && streamCur + 4 >= streamEnd) {
				dec->mcuColOffset = 0;
				return jpegArmRestoreMcuCheckpoint(dec, SCE_JPEGARM_ERROR_INPUT_SUSPENDED, (int16_t)col, &cp);
			}

			dec->streamCur = streamCur;
			comp->coefPtr[0] = (int16_t)((uint16_t)comp->coefPtr[0] | (uint16_t)((bit & 1U) << approxLow));
			comp->coefPtr = (int16_t *)((uint8_t *)comp->coefPtr + dec->coefBlockSteps[blockIdx]);
			blockIdx++;
			comp = dec->mcuCompPtrs[blockIdx];
		}

		dec->bitBuf = bitBuf;
		dec->bitCount = (int8_t)bitCount;

		if (dec->restartCountdown != 0) {
			dec->restartCountdown--;
			if (dec->restartCountdown == 0) {
				int rst = jpegArmHandleRestartMarker(dec, col);
				if (rst != 0) {
					dec->mcuColOffset = 0;
					return jpegArmRestoreMcuCheckpoint(dec, rst, (int16_t)col, &cp);
				}
				bitCount = dec->bitCount;
				bitBuf = dec->bitBuf;
			}
		}

		col++;
		if (col >= (int)dec->scanMcuCols) {
			dec->mcuColOffset = 0;
			dec->bitBuf = bitBuf;
			dec->bitCount = (int8_t)bitCount;
			return 0;
		}
	}
}

int jpegArmDecodeRowBaselineYCbCr(JpegArmDecoder *dec)
{
	JpegArmMcuCheckpoint cp;
	uint32_t activeCols = (dec->curMcuRow < (int)dec->regionStartY) ? 0U : (uint32_t)dec->regionWidthMcu;
	int col = dec->mcuColOffset;

	for (;;) {
		if (dec->splitState != 0)
			saveCheckpoint(dec, &cp);

		int ret = jpegArmDecodeBlockBaseline(dec, (uint32_t)col >= activeCols);
		if (ret < 0) {
			dec->mcuColOffset = (int16_t)(-(int)dec->regionStartX);
			return jpegArmRestoreMcuCheckpoint(dec, ret, (int16_t)col, &cp);
		}

		if (dec->restartCountdown != 0) {
			dec->restartCountdown--;
			if (dec->restartCountdown == 0) {
				ret = jpegArmHandleRestartMarker(dec, 0);
				if (ret != 0) {
					dec->mcuColOffset = (int16_t)(-(int)dec->regionStartX);
					if (ret < 0)
						return jpegArmRestoreMcuCheckpoint(dec, ret, (int16_t)col, &cp);
					return ret;
				}
			}
		}

		col++;
		if (col >= (int)dec->mcuCols - (int)dec->regionStartX) {
			dec->mcuColOffset = (int16_t)(-(int)dec->regionStartX);
			return 0;
		}
	}
}

int jpegArmDecodeRowBaselineRgba(JpegArmDecoder *dec)
{
	JpegArmMcuCheckpoint cp;
	uint8_t mcuBuf[3076];
	void (*cscFn)(JpegArmDecoder *);
	uint32_t activeCols = (dec->curMcuRow < (int)dec->regionStartY) ? 0U : (uint32_t)dec->regionWidthMcu;
	int col = dec->mcuColOffset;

	if (dec->hSampMask == 0)
		cscFn = jpegArmCscYCbCrH1ToRgba;
	else if (dec->hSampMask == 1)
		cscFn = jpegArmCscYCbCrH2ToRgba;
	else
		cscFn = jpegArmCscYCbCrH4ToRgba;

	uint8_t *plane0 = mcuBuf;
	uint8_t *plane1 = plane0 + dec->components[0].outStride * dec->components[0].outHeight;
	uint8_t *plane2 = plane1 + dec->components[1].outStride * dec->components[1].outHeight;
	uint8_t *plane3 = plane2 + dec->components[2].outStride * dec->components[2].outHeight;

	dec->components[0].outPtr = plane0;
	dec->components[1].outPtr = plane1;
	dec->components[2].outPtr = plane2;
	dec->components[3].outPtr = plane3;

	uint32_t rowHeight = dec->components[0].outHeight;
	if (dec->rgbaCurY + rowHeight > dec->rgbaHeight)
		rowHeight = dec->rgbaHeight - dec->rgbaCurY;
	dec->rgbaRowHeight = (int16_t)rowHeight;

	if (col + (int)dec->regionStartX == 0) {
		dec->rgbaCurPtr = dec->rgbaOutPtr;
		dec->rgbaCurX = 0;
	}

	for (;;) {
		if (dec->splitState != 0)
			saveCheckpoint(dec, &cp);

		int ret = jpegArmDecodeBlockBaseline(dec, (uint32_t)col >= activeCols);
		if (ret < 0) {
			dec->mcuColOffset = (int16_t)(-(int)dec->regionStartX);
			return jpegArmRestoreMcuCheckpoint(dec, ret, (int16_t)col, &cp);
		}

		if (dec->restartCountdown != 0) {
			dec->restartCountdown--;
			if (dec->restartCountdown == 0) {
				ret = jpegArmHandleRestartMarker(dec, 0);
				if (ret < 0) {
					dec->mcuColOffset = (int16_t)(-(int)dec->regionStartX);
					return jpegArmRestoreMcuCheckpoint(dec, ret, (int16_t)col, &cp);
				}
			}
		}

		dec->components[0].outPtr = plane0;
		dec->components[1].outPtr = plane1;
		dec->components[2].outPtr = plane2;
		dec->components[3].outPtr = plane3;
		if (dec->rgbaRowHeight > 0)
			cscFn(dec);

		col++;
		if (col >= (int)dec->mcuCols - (int)dec->regionStartX) {
			dec->rgbaCurY += dec->components[0].outHeight;
			dec->rgbaOutPtr += dec->rgbaWidth * 4 * dec->components[0].outHeight;
			dec->mcuColOffset = (int16_t)(-(int)dec->regionStartX);
			return 0;
		}
	}
}

static inline void refineNonZeroCoeff(int16_t *slot, int16_t posBit, int16_t negBit,
	uint32_t *bitBuf, int *bitCount, const uint8_t **streamCur, const uint8_t *streamEnd)
{
	uint32_t bit = readExtraBits(bitBuf, bitCount, 1, streamCur, streamEnd);
	if (bit & 1U) {
		if (*slot > 0)
			*slot = (int16_t)(*slot + posBit);
		else
			*slot = (int16_t)(*slot + negBit);
	}
}

int jpegArmDecodeRowProgressiveAcRefine(JpegArmDecoder *dec)
{
	JpegArmMcuCheckpoint cp;
	int16_t tmpBlock[64];
	int col = dec->mcuColOffset;
	int bitCount = dec->bitCount;
	uint32_t bitBuf = dec->bitBuf;
	uint16_t eobRun = dec->progEobRun;
	int scanStart = dec->scanStart;
	int scanEnd = dec->scanEnd;
	int16_t posBit = (int16_t)(1 << dec->approxLow);
	int16_t negBit = (int16_t)(-posBit);
	const uint8_t *streamEnd = dec->streamEnd;

	for (;;) {
		if (dec->splitState != 0)
			saveCheckpoint(dec, &cp);

		const uint8_t *streamCur = dec->streamCur;
		int blockIdx = 0;
		JpegArmComponent *comp = dec->mcuCompPtrs[0];

		while (comp != NULL) {
			int16_t *dstCoef = comp->coefPtr;
			int16_t *work = dstCoef;
			if (dec->splitState != 0) {
				for (int i = 0; i < 64; i++)
					tmpBlock[i] = dstCoef[i];
				work = tmpBlock;
			}

			int k = scanStart;
			const JpegArmHuffTable *acTbl = comp->acTable;

			if (eobRun == 0) {
				while (k <= scanEnd) {
					uint32_t top8 = bitBuf >> 24;
					int8_t entry = acTbl->fastBits[top8];
					int sym;
					int newCoeff = 0;
					int run;
					int hasNew = 0;

					if (entry < 0) {
						int shift = -(entry >> 4);
						run = entry & 0x0F;
						bitCount += shift;
						bitBuf = ror32(bitBuf, -shift);
						newCoeff = (acTbl->fastVal[top8] >= 0) ? posBit : negBit;
						hasNew = 1;
						if (bitCount > 0)
							ensureBits(&bitBuf, &bitCount, &streamCur, streamEnd);
					} else {
						if ((entry >> 4) == 0) {
							bitBuf <<= (entry & 31);
							bitCount += entry;
							sym = (uint8_t)acTbl->fastVal[top8];
						} else {
							int remBits;
							sym = decodeSlowHuffSymbol(acTbl, top8, &bitBuf, &bitCount,
								&streamCur, streamEnd, &remBits);
							bitBuf <<= (remBits & 31);
							bitCount += remBits;
						}
						ensureBits(&bitBuf, &bitCount, &streamCur, streamEnd);

						int cat = sym & 0x0F;
						run = sym >> 4;
						if (cat == 0) {
							if (run != 15) {
								eobRun = (uint16_t)(1U << run);
								if (run != 0) {
									uint32_t extra = readExtraBits(&bitBuf, &bitCount, run, &streamCur, streamEnd);
									eobRun = (uint16_t)(eobRun + extra);
								}
								break;
							}
						} else if (cat == 1) {
							uint32_t signBit = readExtraBits(&bitBuf, &bitCount, 1, &streamCur, streamEnd);
							newCoeff = (signBit & 1U) ? posBit : negBit;
							hasNew = 1;
						} else {
							dec->mcuColOffset = 0;
							return jpegArmRestoreMcuCheckpoint(dec, SCE_JPEGARM_ERROR_DECODE_ERROR, (int16_t)col, &cp);
						}
					}

					while (k <= scanEnd) {
						if (work[k] != 0) {
							refineNonZeroCoeff(&work[k], posBit, negBit, &bitBuf, &bitCount, &streamCur, streamEnd);
						} else {
							if (run == 0) {
								if (hasNew)
									work[k] = (int16_t)newCoeff;
								k++;
								break;
							}
							run--;
						}
						k++;
					}

					if (run > 0 && k > scanEnd) {
						dec->mcuColOffset = 0;
						return jpegArmRestoreMcuCheckpoint(dec, SCE_JPEGARM_ERROR_DECODE_ERROR, (int16_t)col, &cp);
					}
				}
			}

			if (eobRun > 0) {
				while (k <= scanEnd) {
					if (work[k] != 0)
						refineNonZeroCoeff(&work[k], posBit, negBit, &bitBuf, &bitCount, &streamCur, streamEnd);
					k++;
				}
				eobRun--;
			}

			if (dec->splitState != 0 && streamCur + 4 >= streamEnd) {
				dec->mcuColOffset = 0;
				return jpegArmRestoreMcuCheckpoint(dec, SCE_JPEGARM_ERROR_INPUT_SUSPENDED, (int16_t)col, &cp);
			}

			if (dec->splitState != 0) {
				for (int i = 0; i < 64; i++)
					dstCoef[i] = work[i];
			}

			dec->streamCur = streamCur;
			comp->coefPtr = (int16_t *)((uint8_t *)dstCoef + dec->coefBlockSteps[blockIdx]);
			blockIdx++;
			comp = dec->mcuCompPtrs[blockIdx];
		}

		dec->bitBuf = bitBuf;
		dec->bitCount = (int8_t)bitCount;
		dec->progEobRun = eobRun;

		if (dec->restartCountdown != 0) {
			dec->restartCountdown--;
			if (dec->restartCountdown == 0) {
				int rst = jpegArmHandleRestartMarker(dec, col);
				if (rst != 0) {
					dec->mcuColOffset = 0;
					return jpegArmRestoreMcuCheckpoint(dec, rst, (int16_t)col, &cp);
				}
				bitCount = dec->bitCount;
				bitBuf = dec->bitBuf;
				eobRun = dec->progEobRun;
			}
		}

		col++;
		if (col >= (int)dec->scanMcuCols) {
			dec->mcuColOffset = 0;
			dec->bitBuf = bitBuf;
			dec->bitCount = (int8_t)bitCount;
			return 0;
		}
	}
}
