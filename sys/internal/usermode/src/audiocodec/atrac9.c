/*
 * SceAudiocodec (vs0:sys/external/audiocodec.suprx):
 * ATRAC9 software decoder (256-sample frame modes, mono / dual-mono / CPE stereo).
 */
#include <arm_neon.h>
#include "audiocodec_internal.h"
#include "atrac9_tables.h"

#define ATRAC9_ERR_FAIL		((int32_t)0x80004005U)
#define ATRAC9_MAX_BITS		8192U

typedef struct Atrac9Channel {
	uint8_t scaleFactors[31];	/* 0x000..0x01E */
	uint8_t precision[32];		/* 0x01F..0x03E */
	uint8_t sfModeBase;		/* 0x03F (0 for primary/mono, 4 for secondary CPE) */
	union {
		float spectra[256];	/* 0x040..0x43F */
		int16_t pcm[512];
	} u;
	float overlap[128];		/* 0x440..0x63F */
} Atrac9Channel;			/* size 0x640 (1600B) */

typedef struct Atrac9BlockHeader {
	uint8_t gradCurve[44];		/* 0x00..0x2B */
	const uint8_t *bitstream;	/* 0x2C */
	uint32_t bitPos;		/* 0x30 */
	int32_t gradScale;		/* 0x34 */
	uint32_t gradBoundary;		/* 0x38 */
	uint32_t isStereo;		/* 0x3C */
	uint32_t quantUnits;		/* 0x40 */
	uint32_t prevQuantUnits;	/* 0x44 */
	uint32_t hasBandExt;		/* 0x48 */
	uint32_t bandExtUnits;		/* 0x4C */
	uint32_t stereoUnits;		/* 0x50 */
	uint32_t primaryChannel;	/* 0x54 */
	uint32_t stereoSigns;		/* 0x58 */
	uint32_t isReuseFrame;		/* 0x5C */
} Atrac9BlockHeader;			/* size 0x60 (96B) */

typedef struct Atrac9Block {
	Atrac9BlockHeader hdr;		/* 0x000..0x05F */
	Atrac9Channel ch[1];		/* 0x060..0x69F */
} Atrac9Block;				/* size 0x6A0 (1696B) */

typedef struct Atrac9Ctx {
	int32_t sampleRateIdx;		/* 0x00 */
	uint32_t channelConfigIdx;	/* 0x04 */
	int32_t frameBytes;		/* 0x08 */
	int32_t numBlocks;		/* 0x0C */
	uint32_t superframeIdx;		/* 0x10 */
	int32_t superframeBits;		/* 0x14 */
	uint32_t reserved[6];		/* 0x18..0x2F */
	uint32_t frameIndex;		/* 0x30 */
	uint32_t superframeBitPos;	/* 0x34 */
	int32_t lastBlockErr;		/* 0x38 */
	int32_t errCode;		/* 0x3C */
	union {
		Atrac9Block mono[2];	/* 0x40..0xD7F (2 * 0x6A0 = 0xD40) */
		struct {
			Atrac9BlockHeader hdr;
			Atrac9Channel ch[2];
		} stereo;		/* 0x40..0xD1F (0xCE0) */
	} blocks;
} Atrac9Ctx;

static inline uint32_t readBe16Word(const uint8_t *buf)
{
	return __builtin_bswap32((uint32_t)(*(const uint16_t *)buf));
}

static inline uint32_t peekBe32Aligned2(const uint8_t *buf, uint32_t bitPos)
{
	return __builtin_bswap32(*(const uint32_t *)(buf + 2U * (bitPos >> 4))) << (bitPos & 15U);
}

static inline uint32_t peekBe32Byte(const uint8_t *buf, uint32_t bitPos)
{
	return __builtin_bswap32(*(const uint32_t *)(buf + (bitPos >> 3))) << (bitPos & 7U);
}

static inline uint32_t peekBe16Byte(const uint8_t *buf, uint32_t bitPos)
{
	return __builtin_bswap32((uint32_t)(*(const uint16_t *)(buf + (bitPos >> 3)))) << (bitPos & 7U);
}

static inline float bitsToFloat(uint32_t bits)
{
	union {
		uint32_t u;
		float f;
	} v;
	v.u = bits;
	return v.f;
}

static inline float32x4_t reverseFloat32x4(float32x4_t v)
{
	float32x4_t rev = vrev64q_f32(v);
	return vcombine_f32(vget_high_f32(rev), vget_low_f32(rev));
}

static __attribute__((noinline, noclone)) int32_t
atrac9GetFrameSamples(int32_t sampleRateIdx, uint32_t *outSamples)
{
	if ((uint32_t)(sampleRateIdx - 5) > 2U)
		return ATRAC9_ERR_FAIL;
	*outSamples = 256U;
	return 0;
}

static __attribute__((noinline, noclone)) int32_t
atrac9ParseConfig(Atrac9Ctx *ctx, const uint8_t *configData, uint32_t *outSampleRateIdx,
		  uint32_t *outChannelCfgIdx, uint32_t *outFrameBytes, uint32_t *outReservedBit,
		  uint32_t *outSuperframeIdx)
{
	if (configData[0] != 0xFEU) {
		ctx->errCode = 519;
		return ATRAC9_ERR_FAIL;
	}

	uint32_t byte1 = configData[1];
	uint32_t word2 = __builtin_bswap32((uint32_t)(*(const uint16_t *)(configData + 2)));
	*outSampleRateIdx = byte1 >> 4;
	*outChannelCfgIdx = (byte1 >> 1) & 7U;
	*outReservedBit = byte1 & 1U;
	*outFrameBytes = (word2 >> 21) + 1U;
	*outSuperframeIdx = (word2 >> 19) & 3U;

	if (*outReservedBit != 0U) {
		ctx->errCode = 520;
		return ATRAC9_ERR_FAIL;
	}
	return 0;
}

static __attribute__((noinline)) int32_t
atrac9Configure(Atrac9Ctx *ctx, int32_t sampleRateIdx, uint32_t channelCfgIdx, int32_t frameBytes,
		int32_t reservedBit, uint32_t superframeIdx)
{
	if ((uint32_t)(sampleRateIdx - 5) > 2U) {
		ctx->errCode = 513;
		return ATRAC9_ERR_FAIL;
	}
	if (channelCfgIdx > 2U) {
		ctx->errCode = 514;
		return ATRAC9_ERR_FAIL;
	}
	if ((uint32_t)(frameBytes - 1) >= 0x400U) {
		ctx->errCode = 515;
		return ATRAC9_ERR_FAIL;
	}
	if (superframeIdx > 2U) {
		ctx->errCode = 516;
		return ATRAC9_ERR_FAIL;
	}
	if (reservedBit != 0) {
		ctx->errCode = 517;
		return ATRAC9_ERR_FAIL;
	}

	ctx->sampleRateIdx = sampleRateIdx;
	ctx->channelConfigIdx = channelCfgIdx;
	ctx->frameBytes = frameBytes;
	ctx->numBlocks = (channelCfgIdx == 1U) ? 2 : 1;
	ctx->superframeIdx = superframeIdx;

	if (frameBytes > 256) {
		ctx->errCode = 518;
		return ATRAC9_ERR_FAIL;
	}
	return 0;
}

static __attribute__((noinline)) int32_t
atrac9Reset(Atrac9Ctx *ctx)
{
	if ((uint32_t)(ctx->sampleRateIdx - 5) > 2U) {
		ctx->errCode = 540;
		return ATRAC9_ERR_FAIL;
	}

	sceClibMemset(&ctx->blocks, 0, sizeof(ctx->blocks));
	if (ctx->channelConfigIdx == 2U) {
		ctx->blocks.stereo.hdr.isStereo = 1U;
		ctx->blocks.stereo.ch[1].sfModeBase = 4U;
	}

	ctx->lastBlockErr = 0;
	ctx->frameIndex = 0;
	ctx->superframeBitPos = 0;
	ctx->superframeBits = ctx->frameBytes << (ctx->superframeIdx + 3U);
	return 0;
}

static __attribute__((noinline)) int32_t
atrac9DecodeScaleFactors(Atrac9Block *blk, uint32_t chIdx)
{
	Atrac9Channel *ch = &blk->ch[chIdx];
	const uint8_t *bs = blk->hdr.bitstream;
	uint32_t bitPos = blk->hdr.bitPos;
	uint32_t numUnits = blk->hdr.bandExtUnits;
	uint32_t modeBits = 0;
	uint32_t rawWord = 0;

	if (bitPos < ATRAC9_MAX_BITS) {
		uint32_t w = peekBe32Byte(bs, bitPos);
		modeBits = w >> 30;
		rawWord = w << 2;
	}

	uint32_t sfMode = (uint32_t)ch->sfModeBase + modeBits;
	uint32_t unitIdx = 0;
	uint32_t bitLen = 0;
	uint8_t baseVal = 0;
	int32_t doRawTail = 0;

	switch (sfMode) {
	case 0:
	case 4: {
		uint32_t baseOffset = (rawWord >> 24) & 0x1FU;
		uint32_t tblSel = (rawWord >> 22) & 3U;
		uint32_t bitsInit = tblSel + 2U;
		uint32_t curveIdx = rawWord >> 29;
		const uint8_t *baseline = &kAtrac9SfTables.body[30U * curveIdx];
		int32_t acc = ((int32_t)(rawWord << 10)) >> (31U - bitsInit);
		const uint8_t *huff = kAtrac9SfTables.huffTbl[tblSel + 2U];
		bitPos += 13U + bitsInit;
		ch->scaleFactors[0] = (uint8_t)(baseOffset - baseline[0] + (uint32_t)acc);

		if (bitsInit == 5U) {
			for (uint32_t i = 1; i < numUnits; ++i) {
				if (bitPos >= ATRAC9_MAX_BITS)
					break;
				uint32_t code = peekBe16Byte(bs, bitPos) >> 24;
				uint32_t entry = huff[code];
				uint32_t step = (code > 0xCBU) ? 7U : 3U;
				acc = (acc + (int32_t)entry) & 0x3F;
				bitPos += step + (entry >> 6);
				ch->scaleFactors[i] =
					(uint8_t)(baseOffset - baseline[i] + (uint32_t)acc);
			}
		} else {
			uint16_t hdr = (uint16_t)((uint32_t)huff[-2] | ((uint32_t)huff[-1] << 8));
			uint32_t shift = hdr & 0xFFU;
			uint32_t mask = hdr >> 8;
			for (uint32_t i = 1; i < numUnits; ++i) {
				if (bitPos >= ATRAC9_MAX_BITS)
					break;
				uint32_t code = peekBe16Byte(bs, bitPos) >> shift;
				uint32_t entry = huff[code];
				acc = (acc + (int32_t)entry) & (int32_t)mask;
				bitPos += (entry >> 5) + 1U;
				ch->scaleFactors[i] =
					(uint8_t)(baseOffset - baseline[i] + (uint32_t)acc);
			}
		}
		break;
	}
	case 1: {
		uint32_t lenSel = rawWord >> 30;
		bitLen = lenSel + 2U;
		if (lenSel == 3U) {
			baseVal = 0;
			bitPos += 4U;
		} else {
			baseVal = (uint8_t)((rawWord >> 25) & 0x1FU);
			bitPos += 9U;
		}
		unitIdx = 0;
		doRawTail = 1;
		break;
	}
	case 5: {
		uint16_t cfg = kAtrac9SfTables.modeDiffCfg[rawWord >> 30];
		const uint8_t *basePtr = (const uint8_t *)&kAtrac9SfTables + (cfg >> 5);
		uint32_t shift = cfg & 0x1FU;
		Atrac9Channel *refCh = &blk->ch[chIdx - 1U];
		bitPos += 4U;
		for (uint32_t i = 0; i < numUnits; ++i) {
			if (bitPos >= ATRAC9_MAX_BITS)
				break;
			uint32_t entry = basePtr[peekBe16Byte(bs, bitPos) >> shift];
			ch->scaleFactors[i] = (uint8_t)((refCh->scaleFactors[i] + entry) & 0x1FU);
			bitPos += (entry >> 5) + 1U;
		}
		break;
	}
	case 6: {
		uint32_t tblSel = (rawWord >> 25) & 3U;
		uint8_t baseOff = (uint8_t)((rawWord >> 27) - 16U);
		int32_t acc = ((int32_t)(rawWord << 7)) >> (31U - tblSel);
		const uint8_t *huff = kAtrac9SfTables.huffTbl[tblSel];
		uint16_t hdr = (uint16_t)((uint32_t)huff[-2] | ((uint32_t)huff[-1] << 8));
		uint32_t shift = hdr & 0xFFU;
		uint32_t mask = hdr >> 8;
		Atrac9Channel *refCh = &blk->ch[chIdx - 1U];
		bitPos += 10U + tblSel;
		ch->scaleFactors[0] = (uint8_t)(refCh->scaleFactors[0] + baseOff + (uint8_t)acc);
		for (uint32_t i = 1; i < numUnits; ++i) {
			if (bitPos >= ATRAC9_MAX_BITS)
				break;
			uint32_t entry = huff[peekBe16Byte(bs, bitPos) >> shift];
			acc = (acc + (int32_t)entry) & (int32_t)mask;
			bitPos += (entry >> 5) + 1U;
			ch->scaleFactors[i] =
				(uint8_t)(refCh->scaleFactors[i] + baseOff + (uint8_t)acc);
		}
		break;
	}
	default: {
		if (blk->hdr.isReuseFrame == 0U)
			return 270;
		uint32_t prevUnits = blk->hdr.prevQuantUnits;
		if (numUnits < prevUnits)
			prevUnits = numUnits;
		if (sfMode == 3U) {
			uint32_t tblSel = (rawWord >> 25) & 3U;
			int32_t acc = ((int32_t)(rawWord << 7)) >> (31U - tblSel);
			uint8_t baseOff = (uint8_t)((rawWord >> 27) - 16U);
			const uint8_t *huff = kAtrac9SfTables.huffTbl[tblSel];
			uint16_t hdr = (uint16_t)((uint32_t)huff[-2] | ((uint32_t)huff[-1] << 8));
			uint32_t shift = hdr & 0xFFU;
			uint32_t mask = hdr >> 8;
			bitPos += 10U + tblSel;
			ch->scaleFactors[0] = (uint8_t)(ch->scaleFactors[0] + baseOff + (uint8_t)acc);
			for (uint32_t i = 1; i < prevUnits; ++i) {
				if (bitPos >= ATRAC9_MAX_BITS)
					break;
				uint32_t entry = huff[peekBe16Byte(bs, bitPos) >> shift];
				acc = (acc + (int32_t)entry) & (int32_t)mask;
				bitPos += (entry >> 5) + 1U;
				ch->scaleFactors[i] =
					(uint8_t)(ch->scaleFactors[i] + baseOff + (uint8_t)acc);
			}
		} else {
			uint16_t cfg = kAtrac9SfTables.modeDiffCfg[rawWord >> 30];
			const uint8_t *basePtr = (const uint8_t *)&kAtrac9SfTables + (cfg >> 5);
			uint32_t shift = cfg & 0x1FU;
			bitPos += 4U;
			for (uint32_t i = 0; i < prevUnits; ++i) {
				if (bitPos >= ATRAC9_MAX_BITS)
					break;
				uint32_t entry = basePtr[peekBe16Byte(bs, bitPos) >> shift];
				ch->scaleFactors[i] =
					(uint8_t)((ch->scaleFactors[i] + entry) & 0x1FU);
				bitPos += (entry >> 5) + 1U;
			}
		}
		if (numUnits != prevUnits) {
			unitIdx = prevUnits;
			baseVal = 0;
			bitLen = 5U;
			doRawTail = 1;
		}
		break;
	}
	}

	if (doRawTail) {
		uint32_t tailBits = bitLen * (numUnits - unitIdx);
		if (bitPos + tailBits >= ATRAC9_MAX_BITS) {
			bitPos += tailBits;
		} else {
			for (; unitIdx < numUnits; ++unitIdx) {
				uint32_t val = peekBe16Byte(bs, bitPos) >> (32U - bitLen);
				bitPos += bitLen;
				ch->scaleFactors[unitIdx] = (uint8_t)(val + baseVal);
			}
		}
	}

	blk->hdr.bitPos = bitPos;
	uint64_t *tailZero = (uint64_t *)&ch->scaleFactors[numUnits];
	tailZero[0] = 0ULL;
	tailZero[1] = 0ULL;

	int8x16_t sfLo = vld1q_s8((const int8_t *)&ch->scaleFactors[0]);
	int8x16_t sfHi = vld1q_s8((const int8_t *)&ch->scaleFactors[16]);
	int8x16_t gradLo = vld1q_s8((const int8_t *)&blk->hdr.gradCurve[0]);
	int8x16_t gradHi = vld1q_s8((const int8_t *)&blk->hdr.gradCurve[16]);
	vst1q_s8((int8_t *)&ch->precision[0], vsubq_s8(sfLo, gradLo));
	int8x16_t orMask = sfLo;
	if (numUnits > 15U) {
		orMask = vorrq_s8(sfLo, sfHi);
		vst1q_s8((int8_t *)&ch->precision[16], vsubq_s8(sfHi, gradHi));
	}

	uint32x4_t lanes = vreinterpretq_u32_s8(orMask);
	uint32_t combined = vgetq_lane_u32(lanes, 0) | vgetq_lane_u32(lanes, 1) |
			    vgetq_lane_u32(lanes, 2) | vgetq_lane_u32(lanes, 3);
	if ((combined & 0xE0E0E0E0U) != 0U)
		return 267;
	return 0;
}

static __attribute__((noinline)) void
atrac9DecodeSpectra(Atrac9Block *blk, uint32_t chIdx)
{
	Atrac9Channel *ch = &blk->ch[chIdx];
	uint32_t quantUnits = blk->hdr.quantUnits;
	uint32_t codedUnits = (blk->hdr.primaryChannel == chIdx) ? quantUnits : blk->hdr.stereoUnits;
	int32_t gradScale = blk->hdr.gradScale;

	if (gradScale != 8) {
		uint32_t limit = (codedUnits < quantUnits) ? (codedUnits + 1U) : codedUnits;
		for (uint32_t i = 1; i < limit; ++i) {
			int32_t diff = (int32_t)ch->scaleFactors[i] - (int32_t)ch->scaleFactors[i - 1U];
			int32_t neg = diff >> 31;
			int32_t absDiff = (neg != 0) ? -diff : diff;
			if (absDiff > 1) {
				int32_t boost = (absDiff - 1 > 5) ? 5 : (absDiff - 1);
				ch->precision[i + (uint32_t)neg] =
					(uint8_t)(ch->precision[i + (uint32_t)neg] + boost);
			}
		}
	}

	int32_t avgThresh = 127;
	uint8_t savedSf = ch->scaleFactors[codedUnits];
	ch->scaleFactors[codedUnits] = 127U;
	if (codedUnits > 12U) {
		uint32_t sum = 0;
		for (uint32_t i = 0; i < 12U; ++i)
			sum += ch->scaleFactors[i];
		avgThresh = (int32_t)(171U * sum + 1024U);
	}

	uint32_t huffSetMask = 0;
	for (uint32_t i = 8; i < codedUnits; ++i) {
		int32_t cur = (int32_t)ch->scaleFactors[i];
		int32_t dPrev = cur - (int32_t)ch->scaleFactors[i - 1U];
		int32_t dNext = cur - (int32_t)ch->scaleFactors[i + 1U];
		int32_t maxD = (dNext < dPrev) ? dPrev : dNext;
		if (i == 20U)
			--avgThresh;
		else if (i == 12U)
			avgThresh >>= 11;
		int32_t sumD = dPrev + dNext;
		if (sumD < maxD)
			sumD = maxD;
		if (sumD > 2 || (cur >= avgThresh && maxD > 1))
			huffSetMask |= (0x80000000U >> i);
	}
	ch->scaleFactors[codedUnits] = savedSf;

	const uint8_t *bs = blk->hdr.bitstream;
	uint32_t bitPos = blk->hdr.bitPos;
	float *specOut = ch->u.spectra;
	int32_t huffMaskShift = (int32_t)huffSetMask;
	int32_t hasFinePass = 0;

	for (uint32_t unit = 0; unit < codedUnits; ++unit) {
		uint32_t sf = ch->scaleFactors[unit];
		uint32_t width = kAtrac9BandMeta[unit];
		int32_t extraBit = (blk->hdr.gradBoundary > unit) ? 1 : 0;
		int32_t prec = (gradScale * (int32_t)(int8_t)ch->precision[unit]) >> 3;
		prec = (prec < 1) ? (extraBit + 1) : (prec + extraBit);
		ch->precision[unit] = (uint8_t)prec;
		float *unitEnd = specOut + width;

		if (prec > 6) {
			uint32_t bitsPerSample;
			uint32_t stepIdx;
			if (prec <= 15) {
				stepIdx = (uint32_t)(prec - 1);
				bitsPerSample = (uint32_t)(prec + 1);
			} else {
				hasFinePass = 1;
				stepIdx = 14U;
				bitsPerSample = 16U;
				prec = 15;
			}
			uint32_t nextBitPos = bitPos + bitsPerSample * width;
			float scale = bitsToFloat(kAtrac9SpecData[stepIdx] + (sf << 23));
			if (nextBitPos >= ATRAC9_MAX_BITS) {
				bitPos = nextBitPos;
				blk->hdr.bitPos = bitPos;
				return;
			}
			uint32_t rshift = 31U - (uint32_t)prec;
			while (specOut != unitEnd) {
				int32_t q = ((int32_t)peekBe32Aligned2(bs, bitPos)) >> rshift;
				bitPos += bitsPerSample;
				*specOut++ = (float)q * scale;
			}
		} else {
			uint32_t lz = (uint32_t)__builtin_clz(width);
			uint32_t sizeSel = (huffMaskShift < 0) ? (lz - 27U) : (lz - 24U);
			uint32_t scaleBits = kAtrac9SpecData[prec - 1] + (sf << 23);
			float32x2_t scaleVec = vdup_n_f32(bitsToFloat(scaleBits));
			const uint16_t *offTbl =
				(const uint16_t *)&kAtrac9SpecData[4 * prec + 11];
			const uint8_t *huffBase =
				(const uint8_t *)kAtrac9SpecData + offTbl[sizeSel];
			uint8_t huffShift = huffBase[-1];
			uint16_t lenBack = (uint16_t)(kAtrac9SpecData[4 * prec + 14] >> 16);
			const uint8_t *lenTbl = huffBase - lenBack;

			if (prec > 3) {
				while (specOut != unitEnd) {
					if (bitPos >= ATRAC9_MAX_BITS)
						break;
					uint32_t word = peekBe32Byte(bs, bitPos);
					int8_t s0 = (int8_t)huffBase[word >> huffShift];
					uint32_t l0 = lenTbl[s0];
					int8_t s1 = (int8_t)huffBase[(word << l0) >> huffShift];
					int32x2_t pair = { (int32_t)s0, (int32_t)s1 };
					vst1_f32(specOut, vmul_lane_f32(vcvt_f32_s32(pair), scaleVec, 1));
					specOut += 2;
					bitPos += l0 + (uint32_t)lenTbl[s1];
				}
			} else if (huffShift > 31U) {
				uint32_t shift = huffShift - 32U;
				uint32_t signedScale = scaleBits + 0x80000000U;
				while (specOut != unitEnd) {
					if (bitPos >= ATRAC9_MAX_BITS)
						break;
					int8_t sym = (int8_t)huffBase[peekBe32Aligned2(bs, bitPos) >> shift];
					uint32_t u0 = (uint32_t)((int32_t)sym >> 6);
					uint32_t u1 = (uint32_t)(((int32_t)sym << 26) >> 30);
					uint32_t u2 = (uint32_t)(((int32_t)sym << 28) >> 30);
					uint32_t u3 = (uint32_t)(((int32_t)sym << 30) >> 30);
					specOut[0] = bitsToFloat(signedScale & ((u0 >> 1) | (u0 << 31)));
					specOut[1] = bitsToFloat(signedScale & ((u1 >> 1) | (u1 << 31)));
					specOut[2] = bitsToFloat(signedScale & ((u2 >> 1) | (u2 << 31)));
					specOut[3] = bitsToFloat(signedScale & ((u3 >> 1) | (u3 << 31)));
					specOut += 4;
					bitPos += (uint32_t)lenTbl[sym];
				}
			} else {
				uint32_t hiShift = (uint32_t)(prec + 1);
				uint32_t loShift = (uint32_t)(31 - prec);
				while (specOut != unitEnd) {
					if (bitPos >= ATRAC9_MAX_BITS)
						break;
					int32_t sym = (int8_t)huffBase[peekBe32Aligned2(bs, bitPos) >> huffShift];
					int32x2_t pair = { sym >> hiShift, (sym << loShift) >> loShift };
					vst1_f32(specOut, vmul_lane_f32(vcvt_f32_s32(pair), scaleVec, 1));
					specOut += 2;
					bitPos += (uint32_t)lenTbl[sym];
				}
			}
		}

		huffMaskShift <<= 1;
		specOut = unitEnd;
	}

	if (hasFinePass) {
		float *finePtr = ch->u.spectra;
		for (uint32_t unit = 0; unit < codedUnits; ++unit) {
			uint32_t prec = ch->precision[unit];
			uint32_t width = kAtrac9BandMeta[unit];
			if (prec > 15U) {
				uint32_t finePrec = prec - 15U;
				if (finePrec > 15U)
					finePrec = 15U;
				uint32_t fineBits = finePrec + 1U;
				uint32_t nextBitPos = bitPos + width * fineBits;
				float fineScale =
					bitsToFloat(kAtrac9SpecData[finePrec - 1U] +
						    ((uint32_t)ch->scaleFactors[unit] << 23)) *
					0.000015259022f;
				if (nextBitPos >= ATRAC9_MAX_BITS) {
					bitPos = nextBitPos;
					break;
				}
				uint32_t rshift = 31U - finePrec;
				for (uint32_t i = 0; i < width; ++i) {
					int32_t q = ((int32_t)peekBe32Aligned2(bs, bitPos)) >> rshift;
					bitPos += fineBits;
					finePtr[i] += (float)q * fineScale;
				}
			}
			finePtr += width;
		}
	}

	blk->hdr.bitPos = bitPos;
}

static __attribute__((noinline)) int32_t
atrac9DecodeBlock(Atrac9Block *blk, uint32_t hdrBits, uint32_t bitPos)
{
	int32_t err = 0;

	if ((hdrBits & 0x40000000U) != 0U) {
		if ((int32_t)hdrBits >= 0) {
			blk->hdr.prevQuantUnits = blk->hdr.bandExtUnits;
			return 280;
		}
	} else {
		uint32_t qIdx = (hdrBits >> 26) & 0x0FU;
		uint32_t units = kAtrac9BandUnits[qIdx];
		uint32_t rem = hdrBits << 6;
		blk->hdr.quantUnits = units;

		if (blk->hdr.isStereo != 0U) {
			uint32_t stIdx = rem >> 28;
			if (qIdx < stIdx) {
				blk->hdr.prevQuantUnits = blk->hdr.bandExtUnits;
				return 282;
			}
			blk->hdr.stereoUnits = kAtrac9BandUnits[stIdx];
			rem = hdrBits << 10;
			bitPos += 4U;
		}

		blk->hdr.hasBandExt = rem >> 31;
		if ((rem & 0x80000000U) != 0U) {
			uint32_t beIdx = (rem >> 27) & 0x0FU;
			if (qIdx > beIdx) {
				blk->hdr.prevQuantUnits = blk->hdr.bandExtUnits;
				return 283;
			}
			units = kAtrac9BandUnits[beIdx];
			bitPos += 4U;
		}
		bitPos += 5U;
		blk->hdr.bandExtUnits = units;
	}

	const uint8_t *bs = blk->hdr.bitstream;
	uint32_t gradStart, gradEnd, gradStartVal, gradEndVal;
	uint32_t gradRem;
	int32_t gradScale;

	if (bitPos < ATRAC9_MAX_BITS) {
		uint32_t gw = peekBe32Aligned2(bs, bitPos);
		uint32_t gMode = gw >> 30;
		if (gMode != 0U) {
			gradScale = 5 - (int32_t)gMode;
			gradStart = (gw << 2) >> 27;
			gradEnd = 31U;
			gradStartVal = (gw >> 20) & 0x1FU;
			gradEndVal = 31U;
			gradRem = gw << 12;
		} else {
			gradStart = (gw << 2) >> 26;
			if (gradStart > 47U) {
				blk->hdr.prevQuantUnits = blk->hdr.bandExtUnits;
				return 262;
			}
			gradEnd = ((gw >> 18) & 0x3FU) + 1U;
			if (gradEnd > 48U) {
				blk->hdr.prevQuantUnits = blk->hdr.bandExtUnits;
				return 263;
			}
			if (gradStart > gradEnd) {
				blk->hdr.prevQuantUnits = blk->hdr.bandExtUnits;
				return 264;
			}
			uint32_t gw2 = (bitPos + 14U < ATRAC9_MAX_BITS)
					       ? peekBe32Aligned2(bs, bitPos + 14U)
					       : 0U;
			gradStartVal = gw2 >> 27;
			gradEndVal = (gw2 >> 22) & 0x1FU;
			bitPos += 12U;
			gradRem = gw2 << 10;
			gradScale = 8;
		}
	} else {
		gradStart = 0U;
		gradEnd = 1U;
		gradStartVal = 0U;
		gradEndVal = 0U;
		bitPos += 12U;
		gradRem = 0U;
		gradScale = 8;
	}

	uint32_t gradBoundary = gradRem >> 28;
	blk->hdr.gradScale = gradScale;
	blk->hdr.gradBoundary = gradBoundary;
	if (gradBoundary > blk->hdr.quantUnits) {
		blk->hdr.prevQuantUnits = blk->hdr.bandExtUnits;
		return 261;
	}

	sceClibMemset(blk->hdr.gradCurve, (int)gradStartVal, 30U);
	uint32_t quantUnits = blk->hdr.quantUnits;
	if (gradStart < quantUnits) {
		int32_t valDiff = (int32_t)gradEndVal - (int32_t)gradStartVal;
		if (gradEnd != gradStart && valDiff != 0) {
			uint32_t spanIdx = gradEnd - gradStart - 2U;
			if (spanIdx >= 45U)
				spanIdx = 45U;
			const uint8_t *stepPtr = kAtrac9GradSteps[spanIdx];
			int32_t accumBase = (valDiff > 0) ? ((int32_t)(gradStartVal << 10) + 1024)
							  : ((int32_t)(gradStartVal << 10) - 1);
			int32_t stepSlope = 33 * ((valDiff > 0) ? (valDiff - 1) : (valDiff + 1));
			uint32_t limit = (gradEnd < quantUnits) ? gradEnd : quantUnits;
			int32_t accum = accumBase + stepSlope;
			uint32_t pos = gradStart;
			do {
				uint8_t step = *stepPtr++;
				uint8_t val = (uint8_t)(accum >> 10);
				uint32_t count = step >> 5;
				for (uint32_t k = 0; k < 4U; ++k)
					blk->hdr.gradCurve[pos + k] = val;
				pos += count;
				accum += (int32_t)(step & 0x1FU) * stepSlope;
			} while (pos < limit);
		}
		if (gradEnd < quantUnits && (gradEnd == gradStart || valDiff != 0)) {
			for (uint32_t pos = gradEnd; pos < quantUnits; ++pos)
				blk->hdr.gradCurve[pos] = (uint8_t)gradEndVal;
		}
	}

	bitPos += 16U;
	if (blk->hdr.isStereo != 0U) {
		if (bitPos >= ATRAC9_MAX_BITS) {
			blk->hdr.primaryChannel = 0U;
			blk->hdr.stereoSigns = 0U;
		} else {
			uint32_t sw = peekBe32Byte(bs, bitPos);
			blk->hdr.primaryChannel = sw >> 31;
			uint32_t hasSigns = sw & 0x40000000U;
			if (hasSigns != 0U) {
				blk->hdr.stereoSigns = sw << 2;
				bitPos += blk->hdr.quantUnits - blk->hdr.stereoUnits;
			} else {
				blk->hdr.stereoSigns = 0U;
			}
		}
		bitPos += 2U;
	}

	if (blk->hdr.hasBandExt != 0U)
		bitPos += blk->hdr.isStereo + 1U;

	if (bitPos >= ATRAC9_MAX_BITS) {
		bitPos += 1U;
	} else {
		uint32_t extW = peekBe16Byte(bs, bitPos);
		if ((extW & 0x80000000U) != 0U)
			bitPos += 8U + ((extW >> 24) & 0x1FU);
		else
			bitPos += 1U;
	}

	blk->hdr.bitPos = bitPos;
	for (uint32_t ch = 0; ch <= blk->hdr.isStereo; ++ch) {
		err = atrac9DecodeScaleFactors(blk, ch);
		if (err != 0)
			break;
		atrac9DecodeSpectra(blk, ch);
	}

	blk->hdr.prevQuantUnits = blk->hdr.bandExtUnits;
	return err;
}

static __attribute__((noinline)) void
atrac9ImdctBlock(Atrac9Block *blk)
{
	uint32_t quantUnits = blk->hdr.quantUnits;
	uint32_t isStereo = blk->hdr.isStereo;
	uint32_t codedBins = (quantUnits > 29U) ? 256U : (uint32_t)kAtrac9BandMeta[20U + quantUnits];

	if (isStereo != 0U) {
		uint32_t stUnit = blk->hdr.stereoUnits;
		if (quantUnits > stUnit) {
			uint32_t prim = blk->hdr.primaryChannel;
			uint32_t sec = 1U - prim;
			Atrac9Channel *primCh = &blk->ch[prim];
			Atrac9Channel *secCh = &blk->ch[sec];
			uint32_t signBits = blk->hdr.stereoSigns;
			uint32_t binOff = (uint32_t)kAtrac9BandMeta[20U + stUnit];
			float *src = &primCh->u.spectra[binOff];
			float *dst = &secCh->u.spectra[binOff];
			for (uint32_t u = stUnit; u < quantUnits; ++u) {
				uint32_t width = kAtrac9BandMeta[u];
				int32_t sfDelta = (int32_t)secCh->scaleFactors[u] -
						  (int32_t)primCh->scaleFactors[u];
				uint32_t scaleBits =
					(signBits & 0x80000000U) + 0x3F800000U +
					((uint32_t)sfDelta << 23);
				float32x4_t scaleVec = vdupq_n_f32(bitsToFloat(scaleBits));
				for (uint32_t k = 0; k < width; k += 4U) {
					vst1q_f32(dst, vmulq_f32(vld1q_f32(src), scaleVec));
					src += 4;
					dst += 4;
				}
				signBits <<= 1;
			}
		}
	}

	for (int32_t chIdx = (int32_t)isStereo; chIdx >= 0; --chIdx) {
		Atrac9Channel *ch = &blk->ch[chIdx];
		float *spec = ch->u.spectra;
		float *activeEnd = spec + codedBins;

		if (codedBins > 128U) {
			float32x4_t sqrt2 = vld1q_dup_f32(&kAtrac9ImdctTable[0]);
			float *lo = spec + (256U - codedBins);
			float *hi = activeEnd;
			while (lo != hi) {
				hi -= 4;
				float32x4_t rev = reverseFloat32x4(vld1q_f32(hi));
				vst1q_f32(lo, vsubq_f32(vld1q_f32(lo), rev));
				lo += 4;
			}
			float *mid = spec + 128;
			while (mid < activeEnd) {
				float32x4_t a0 = vld1q_f32(mid - 128);
				float32x4_t a1 = vld1q_f32(mid - 124);
				float32x4_t b0 = vmulq_f32(sqrt2, vld1q_f32(mid));
				float32x4_t b1 = vmulq_f32(sqrt2, vld1q_f32(mid + 4));
				vst1q_f32(mid - 128, vaddq_f32(a0, b0));
				vst1q_f32(mid - 124, vaddq_f32(a1, b1));
				vst1q_f32(mid, vsubq_f32(a0, b0));
				vst1q_f32(mid + 4, vsubq_f32(a1, b1));
				mid += 8;
			}
		} else if (codedBins < 128U) {
			sceClibMemset(activeEnd, 0, (128U - codedBins) * sizeof(float));
			activeEnd = spec + 128;
		}

		if (activeEnd != spec + 256)
			sceClibMemcpy_safe(activeEnd, activeEnd - 128,
					   (size_t)((spec + 256) - activeEnd) * sizeof(float));

		const float *tw = &kAtrac9ImdctTable[1];
		for (int half = 0; half < 2; ++half) {
			float *blkPtr = spec + half * 128;
			for (int k = 0; k < 64; k += 4) {
				float32x4_t hiRev = reverseFloat32x4(vld1q_f32(blkPtr + 124 - k));
				vst1q_f32(blkPtr + k, vsubq_f32(vld1q_f32(blkPtr + k), hiRev));
			}
			float32x4_t c = vld1q_dup_f32(tw++);
			for (int k = 0; k < 64; k += 8) {
				float32x4_t a0 = vld1q_f32(blkPtr + k);
				float32x4_t a1 = vld1q_f32(blkPtr + k + 4);
				float32x4_t b0 = vmulq_f32(c, vld1q_f32(blkPtr + k + 64));
				float32x4_t b1 = vmulq_f32(c, vld1q_f32(blkPtr + k + 68));
				vst1q_f32(blkPtr + k, vaddq_f32(a0, b0));
				vst1q_f32(blkPtr + k + 4, vaddq_f32(a1, b1));
				vst1q_f32(blkPtr + k + 64, vsubq_f32(a0, b0));
				vst1q_f32(blkPtr + k + 68, vsubq_f32(a1, b1));
			}
		}

		for (int qtr = 3; qtr >= 0; --qtr) {
			float *blkPtr = spec + qtr * 64;
			float32x4_t r0 = vld1q_f32(blkPtr + 32);
			float32x4_t r1 = vld1q_f32(blkPtr + 36);
			float32x4_t r2 = vld1q_f32(blkPtr + 40);
			float32x4_t r3 = vld1q_f32(blkPtr + 44);
			float32x4_t r4 = vld1q_f32(blkPtr + 48);
			float32x4_t r5 = vld1q_f32(blkPtr + 52);
			float32x4_t r6 = vld1q_f32(blkPtr + 56);
			float32x4_t r7 = vld1q_f32(blkPtr + 60);

			float32x4_t d0 = vsubq_f32(vld1q_f32(blkPtr + 0), reverseFloat32x4(r7));
			float32x4_t d1 = vsubq_f32(vld1q_f32(blkPtr + 4), reverseFloat32x4(r6));
			float32x4_t d2 = vsubq_f32(vld1q_f32(blkPtr + 8), reverseFloat32x4(r5));
			float32x4_t d3 = vsubq_f32(vld1q_f32(blkPtr + 12), reverseFloat32x4(r4));
			float32x4_t d4 = vsubq_f32(vld1q_f32(blkPtr + 16), reverseFloat32x4(r3));
			float32x4_t d5 = vsubq_f32(vld1q_f32(blkPtr + 20), reverseFloat32x4(r2));
			float32x4_t d6 = vsubq_f32(vld1q_f32(blkPtr + 24), reverseFloat32x4(r1));
			float32x4_t d7 = vsubq_f32(vld1q_f32(blkPtr + 28), reverseFloat32x4(r0));

			float cVal = *tw++;
			float32x4_t p0 = vmulq_n_f32(r0, cVal);
			float32x4_t p1 = vmulq_n_f32(r1, cVal);
			float32x4_t p2 = vmulq_n_f32(r2, cVal);
			float32x4_t p3 = vmulq_n_f32(r3, cVal);
			float32x4_t p4 = vmulq_n_f32(r4, cVal);
			float32x4_t p5 = vmulq_n_f32(r5, cVal);
			float32x4_t p6 = vmulq_n_f32(r6, cVal);
			float32x4_t p7 = vmulq_n_f32(r7, cVal);

			vst1q_f32(blkPtr + 0, vaddq_f32(d0, p0));
			vst1q_f32(blkPtr + 4, vaddq_f32(d1, p1));
			vst1q_f32(blkPtr + 8, vaddq_f32(d2, p2));
			vst1q_f32(blkPtr + 12, vaddq_f32(d3, p3));
			vst1q_f32(blkPtr + 16, vaddq_f32(d4, p4));
			vst1q_f32(blkPtr + 20, vaddq_f32(d5, p5));
			vst1q_f32(blkPtr + 24, vaddq_f32(d6, p6));
			vst1q_f32(blkPtr + 28, vaddq_f32(d7, p7));
			vst1q_f32(blkPtr + 32, vsubq_f32(d0, p0));
			vst1q_f32(blkPtr + 36, vsubq_f32(d1, p1));
			vst1q_f32(blkPtr + 40, vsubq_f32(d2, p2));
			vst1q_f32(blkPtr + 44, vsubq_f32(d3, p3));
			vst1q_f32(blkPtr + 48, vsubq_f32(d4, p4));
			vst1q_f32(blkPtr + 52, vsubq_f32(d5, p5));
			vst1q_f32(blkPtr + 56, vsubq_f32(d6, p6));
			vst1q_f32(blkPtr + 60, vsubq_f32(d7, p7));
		}

		float *ovlPtr = ch->overlap;
		int16_t *pcmPtr = ch->u.pcm;
		for (int seg = 0; seg < 8; ++seg) {
			const float *segIn = spec + seg * 32;
			float32x4_t x0 = vld1q_f32(segIn + 0);
			float32x4_t x1 = vld1q_f32(segIn + 4);
			float32x4_t x2 = vld1q_f32(segIn + 8);
			float32x4_t x3 = vld1q_f32(segIn + 12);
			float32x4_t x4 = vld1q_f32(segIn + 16);
			float32x4_t x5 = vld1q_f32(segIn + 20);
			float32x4_t x6 = vld1q_f32(segIn + 24);
			float32x4_t x7 = vld1q_f32(segIn + 28);

			/* Stage 32 -> 16 */
			float c0 = tw[0];
			float32x4_t a0 = vsubq_f32(x0, reverseFloat32x4(x7));
			float32x4_t a1 = vsubq_f32(x1, reverseFloat32x4(x6));
			float32x4_t a2 = vsubq_f32(x2, reverseFloat32x4(x5));
			float32x4_t a3 = vsubq_f32(x3, reverseFloat32x4(x4));
			float32x4_t m0 = vmulq_n_f32(x4, c0);
			float32x4_t m1 = vmulq_n_f32(x5, c0);
			float32x4_t m2 = vmulq_n_f32(x6, c0);
			float32x4_t m3 = vmulq_n_f32(x7, c0);
			float32x4_t s0 = vaddq_f32(a0, m0);
			float32x4_t s1 = vaddq_f32(a1, m1);
			float32x4_t s2 = vaddq_f32(a2, m2);
			float32x4_t s3 = vaddq_f32(a3, m3);
			float32x4_t s4 = vsubq_f32(a0, m0);
			float32x4_t s5 = vsubq_f32(a1, m1);
			float32x4_t s6 = vsubq_f32(a2, m2);
			float32x4_t s7 = vsubq_f32(a3, m3);

			/* Stage 16 -> 8 */
			float c1 = tw[1], c2 = tw[2];
			float32x4_t b0 = vsubq_f32(s0, reverseFloat32x4(s3));
			float32x4_t b1 = vsubq_f32(s1, reverseFloat32x4(s2));
			float32x4_t b4 = vsubq_f32(s4, reverseFloat32x4(s7));
			float32x4_t b5 = vsubq_f32(s5, reverseFloat32x4(s6));
			float32x4_t n0 = vmulq_n_f32(s2, c1);
			float32x4_t n1 = vmulq_n_f32(s3, c1);
			float32x4_t n4 = vmulq_n_f32(s6, c2);
			float32x4_t n5 = vmulq_n_f32(s7, c2);
			float32x4_t t0 = vaddq_f32(b0, n0);
			float32x4_t t1 = vaddq_f32(b1, n1);
			float32x4_t t2 = vsubq_f32(b0, n0);
			float32x4_t t3 = vsubq_f32(b1, n1);
			float32x4_t t4 = vaddq_f32(b4, n4);
			float32x4_t t5 = vaddq_f32(b5, n5);
			float32x4_t t6 = vsubq_f32(b4, n4);
			float32x4_t t7 = vsubq_f32(b5, n5);

			/* Stage 8 -> 4 */
			float c3 = tw[3], c4 = tw[4], c5 = tw[5], c6 = tw[6];
			float32x4_t e0 = vsubq_f32(t0, reverseFloat32x4(t1));
			float32x4_t e2 = vsubq_f32(t2, reverseFloat32x4(t3));
			float32x4_t e4 = vsubq_f32(t4, reverseFloat32x4(t5));
			float32x4_t e6 = vsubq_f32(t6, reverseFloat32x4(t7));
			float32x4_t p0 = vmulq_n_f32(t1, c3);
			float32x4_t p2 = vmulq_n_f32(t3, c4);
			float32x4_t p4 = vmulq_n_f32(t5, c5);
			float32x4_t p6 = vmulq_n_f32(t7, c6);
			float32x4_t u0 = vaddq_f32(e0, p0);
			float32x4_t u1 = vsubq_f32(e0, p0);
			float32x4_t u2 = vaddq_f32(e2, p2);
			float32x4_t u3 = vsubq_f32(e2, p2);
			float32x4_t u4 = vaddq_f32(e4, p4);
			float32x4_t u5 = vsubq_f32(e4, p4);
			float32x4_t u6 = vaddq_f32(e6, p6);
			float32x4_t u7 = vsubq_f32(e6, p6);

			/* Stage 4 -> 2 */
			float32x4_t v0L = vcombine_f32(vget_low_f32(u0), vget_low_f32(u1));
			float32x4_t v0R = vcombine_f32(vget_high_f32(u0), vget_high_f32(u1));
			float32x4_t v1L = vcombine_f32(vget_low_f32(u2), vget_low_f32(u3));
			float32x4_t v1R = vcombine_f32(vget_high_f32(u2), vget_high_f32(u3));
			float32x4_t v2L = vcombine_f32(vget_low_f32(u4), vget_low_f32(u5));
			float32x4_t v2R = vcombine_f32(vget_high_f32(u4), vget_high_f32(u5));
			float32x4_t v3L = vcombine_f32(vget_low_f32(u6), vget_low_f32(u7));
			float32x4_t v3R = vcombine_f32(vget_high_f32(u6), vget_high_f32(u7));

			float32x4_t f0 = vsubq_f32(v0L, vrev64q_f32(v0R));
			float32x4_t f1 = vsubq_f32(v1L, vrev64q_f32(v1R));
			float32x4_t f2 = vsubq_f32(v2L, vrev64q_f32(v2R));
			float32x4_t f3 = vsubq_f32(v3L, vrev64q_f32(v3R));

			float32x4_t q0 = vcombine_f32(vmul_n_f32(vget_low_f32(v0R), tw[7]),
						      vmul_n_f32(vget_high_f32(v0R), tw[8]));
			float32x4_t q1 = vcombine_f32(vmul_n_f32(vget_low_f32(v1R), tw[9]),
						      vmul_n_f32(vget_high_f32(v1R), tw[10]));
			float32x4_t q2 = vcombine_f32(vmul_n_f32(vget_low_f32(v2R), tw[11]),
						      vmul_n_f32(vget_high_f32(v2R), tw[12]));
			float32x4_t q3 = vcombine_f32(vmul_n_f32(vget_low_f32(v3R), tw[13]),
						      vmul_n_f32(vget_high_f32(v3R), tw[14]));
			tw += 15;

			/* Stage 2 -> 1 + post-twiddle + overlap-add */
			float32x4x2_t z0 = vuzpq_f32(vaddq_f32(f0, q0), vsubq_f32(f0, q0));
			float32x4x2_t z1 = vuzpq_f32(vaddq_f32(f1, q1), vsubq_f32(f1, q1));
			float32x4x2_t z2 = vuzpq_f32(vaddq_f32(f2, q2), vsubq_f32(f2, q2));
			float32x4x2_t z3 = vuzpq_f32(vaddq_f32(f3, q3), vsubq_f32(f3, q3));

			float32x4_t t3_0 = vld1q_f32(tw + 0);
			float32x4_t t3_1 = vld1q_f32(tw + 4);
			float32x4_t t3_2 = vld1q_f32(tw + 8);
			float32x4_t t3_3 = vld1q_f32(tw + 12);
			float32x4_t t2_0 = vld1q_f32(tw + 16);
			float32x4_t t2_1 = vld1q_f32(tw + 20);
			float32x4_t t2_2 = vld1q_f32(tw + 24);
			float32x4_t t2_3 = vld1q_f32(tw + 28);
			float32x4_t t1_0 = vld1q_f32(tw + 32);
			float32x4_t t1_1 = vld1q_f32(tw + 36);
			float32x4_t t1_2 = vld1q_f32(tw + 40);
			float32x4_t t1_3 = vld1q_f32(tw + 44);
			float32x4_t w0 = vld1q_f32(tw + 48);
			float32x4_t w1 = vld1q_f32(tw + 52);
			float32x4_t w2 = vld1q_f32(tw + 56);
			float32x4_t w3 = vld1q_f32(tw + 60);
			tw += 64;

			float32x4_t d_0 = vsubq_f32(z0.val[0], z0.val[1]);
			float32x4_t d_1 = vsubq_f32(z1.val[0], z1.val[1]);
			float32x4_t d_2 = vsubq_f32(z2.val[0], z2.val[1]);
			float32x4_t d_3 = vsubq_f32(z3.val[0], z3.val[1]);
			float32x4_t r_0 = vmulq_f32(t3_0, z0.val[1]);
			float32x4_t r_1 = vmulq_f32(t3_1, z1.val[1]);
			float32x4_t r_2 = vmulq_f32(t3_2, z2.val[1]);
			float32x4_t r_3 = vmulq_f32(t3_3, z3.val[1]);

			float32x4_t delta0 = vmulq_f32(vsubq_f32(d_0, r_0), t2_0);
			float32x4_t delta1 = vmulq_f32(vsubq_f32(d_1, r_1), t2_1);
			float32x4_t delta2 = vmulq_f32(vsubq_f32(d_2, r_2), t2_2);
			float32x4_t delta3 = vmulq_f32(vsubq_f32(d_3, r_3), t2_3);

			float32x4_t prev0 = vld1q_f32(ovlPtr + 0);
			float32x4_t prev1 = vld1q_f32(ovlPtr + 4);
			float32x4_t prev2 = vld1q_f32(ovlPtr + 8);
			float32x4_t prev3 = vld1q_f32(ovlPtr + 12);

			vst1q_f32(ovlPtr + 0, vmulq_f32(vaddq_f32(d_0, r_0), t1_0));
			vst1q_f32(ovlPtr + 4, vmulq_f32(vaddq_f32(d_1, r_1), t1_1));
			vst1q_f32(ovlPtr + 8, vmulq_f32(vaddq_f32(d_2, r_2), t1_2));
			vst1q_f32(ovlPtr + 12, vmulq_f32(vaddq_f32(d_3, r_3), t1_3));
			ovlPtr += 16;

			int32x4_t o0 = vcvtq_n_s32_f32(vmlsq_f32(prev0, delta0, w0), 1);
			int32x4_t o1 = vcvtq_n_s32_f32(vmlsq_f32(prev1, delta1, w1), 1);
			int32x4_t o2 = vcvtq_n_s32_f32(vmlsq_f32(prev2, delta2, w2), 1);
			int32x4_t o3 = vcvtq_n_s32_f32(vmlsq_f32(prev3, delta3, w3), 1);
			int32x4_t o4 = vcvtq_n_s32_f32(vmlaq_f32(delta0, prev0, w0), 1);
			int32x4_t o5 = vcvtq_n_s32_f32(vmlaq_f32(delta1, prev1, w1), 1);
			int32x4_t o6 = vcvtq_n_s32_f32(vmlaq_f32(delta2, prev2, w2), 1);
			int32x4_t o7 = vcvtq_n_s32_f32(vmlaq_f32(delta3, prev3, w3), 1);

			o0 = vaddq_s32(vshrq_n_s32(o0, 31), o0);
			o1 = vaddq_s32(vshrq_n_s32(o1, 31), o1);
			o2 = vaddq_s32(vshrq_n_s32(o2, 31), o2);
			o3 = vaddq_s32(vshrq_n_s32(o3, 31), o3);
			o4 = vaddq_s32(vshrq_n_s32(o4, 31), o4);
			o5 = vaddq_s32(vshrq_n_s32(o5, 31), o5);
			o6 = vaddq_s32(vshrq_n_s32(o6, 31), o6);
			o7 = vaddq_s32(vshrq_n_s32(o7, 31), o7);

			vst1_s16(pcmPtr + 0, vqrshrn_n_s32(o0, 1));
			vst1_s16(pcmPtr + 4, vqrshrn_n_s32(o4, 1));
			vst1_s16(pcmPtr + 8, vqrshrn_n_s32(o1, 1));
			vst1_s16(pcmPtr + 12, vqrshrn_n_s32(o5, 1));
			vst1_s16(pcmPtr + 16, vqrshrn_n_s32(o2, 1));
			vst1_s16(pcmPtr + 20, vqrshrn_n_s32(o6, 1));
			vst1_s16(pcmPtr + 24, vqrshrn_n_s32(o3, 1));
			vst1_s16(pcmPtr + 28, vqrshrn_n_s32(o7, 1));
			pcmPtr += 32;
		}
	}
}

static __attribute__((noinline, noclone)) void
atrac9ReorderPcm(Atrac9Ctx *ctx, int16_t **outPtrs, int32_t numSamples)
{
	const int16_t *ch0 = ctx->blocks.mono[0].ch[0].u.pcm;
	int16_t *dst0 = outPtrs[0];

	if (ctx->channelConfigIdx != 0U) {
		const int16_t *ch1 = (ctx->blocks.stereo.hdr.isStereo != 0U)
					     ? ctx->blocks.stereo.ch[1].u.pcm
					     : ctx->blocks.mono[1].ch[0].u.pcm;
		int16_t *dst1 = outPtrs[1];
		const uint8_t *lut = kAtrac9PcmReorder;
		for (int32_t rem = numSamples; rem != 0; rem -= 4) {
			uint32_t idx = *lut++;
			uint32_t rev = idx ^ 0x40U;
			dst0[0] = ch0[idx];
			dst0[1] = ch0[idx + 128U];
			dst0[2] = ch0[rev + 128U];
			dst0[3] = ch0[rev];
			dst1[0] = ch1[idx];
			dst1[1] = ch1[idx + 128U];
			dst1[2] = ch1[rev + 128U];
			dst1[3] = ch1[rev];
			dst0 += 4;
			dst1 += 4;
		}
	} else {
		const uint8_t *lut = kAtrac9PcmReorder;
		for (int32_t rem = numSamples; rem != 0; rem -= 4) {
			uint32_t idx = *lut++;
			uint32_t rev = idx ^ 0x40U;
			dst0[0] = ch0[idx];
			dst0[1] = ch0[idx + 128U];
			dst0[2] = ch0[rev + 128U];
			dst0[3] = ch0[rev];
			dst0 += 4;
		}
	}
}

static __attribute__((noinline, noclone)) int32_t
atrac9DecodeFrame(Atrac9Ctx *ctx, const uint8_t *inBuf, int16_t **outPtrs, uint32_t *outConsumed,
		  int32_t skipSynthesis)
{
	uint32_t availBits = (uint32_t)(ctx->superframeBits - (int32_t)ctx->superframeBitPos);
	if (availBits == 0U) {
		ctx->errCode = 574;
		return ATRAC9_ERR_FAIL;
	}

	int32_t numBlocks = ctx->numBlocks;
	uint32_t alignedBits = 0;
	uint32_t hdrBits = readBe16Word(inBuf);
	uint32_t reuseBit = hdrBits >> 31;

	for (int32_t blkIdx = 0; blkIdx < numBlocks; ++blkIdx) {
		Atrac9Block *blk = &ctx->blocks.mono[blkIdx];
		blk->hdr.isReuseFrame = reuseBit;
		if (reuseBit != (uint32_t)(ctx->frameIndex != 0U)) {
			ctx->errCode = 571;
			return ATRAC9_ERR_FAIL;
		}

		blk->hdr.bitstream = inBuf;
		int32_t blkErr = atrac9DecodeBlock(blk, hdrBits, alignedBits + 2U);
		ctx->lastBlockErr = blkErr;
		if (blkErr != 0) {
			ctx->errCode = 573;
			return ATRAC9_ERR_FAIL;
		}

		alignedBits = (blk->hdr.bitPos + 7U) & ~7U;
		if (skipSynthesis == 0)
			atrac9ImdctBlock(blk);

		if (blkIdx + 1 < numBlocks) {
			if (availBits <= alignedBits) {
				ctx->errCode = 574;
				return ATRAC9_ERR_FAIL;
			}
			hdrBits = readBe16Word(inBuf + (alignedBits >> 3));
			reuseBit = hdrBits >> 31;
			if (reuseBit != ctx->blocks.mono[0].hdr.isReuseFrame) {
				ctx->errCode = 571;
				return ATRAC9_ERR_FAIL;
			}
		}
	}

	if (availBits < alignedBits) {
		ctx->errCode = 574;
		return ATRAC9_ERR_FAIL;
	}

	uint32_t consumedBytes = alignedBits >> 3;
	uint32_t nextFrameIdx = ctx->frameIndex + 1U;
	ctx->frameIndex = nextFrameIdx;
	ctx->superframeBitPos += alignedBits;

	if ((nextFrameIdx >> ctx->superframeIdx) != 0U) {
		uint32_t availBytes = availBits >> 3;
		while (consumedBytes < availBytes) {
			if (inBuf[consumedBytes] != 1U) {
				ctx->errCode = 577;
				return ATRAC9_ERR_FAIL;
			}
			++consumedBytes;
		}
		ctx->frameIndex = 0U;
		ctx->superframeBitPos = 0U;
	}

	*outConsumed = consumedBytes;
	if (skipSynthesis == 0)
		atrac9ReorderPcm(ctx, outPtrs, 256);
	return 0;
}

int32_t atrac9CheckNeedMem(uint32_t *work)
{
	work[0] = 0U;
	work[1] = 0U;
	work[2] = 0x2000U;
	return 0;
}

int32_t atrac9Init(uint32_t *work)
{
	uint32_t rawMem = work[1];
	if (rawMem == 0U)
		return AUDIOCODEC_ERROR_INVALID_ARG;

	Atrac9Ctx *ctx = (Atrac9Ctx *)(uintptr_t)((rawMem + 63U) & ~63U);
	uint32_t sampleRateIdx, channelCfgIdx, frameBytes, reservedBit, superframeIdx;
	ctx->errCode = 0;

	int32_t ret = atrac9ParseConfig(ctx, (const uint8_t *)&work[8], &sampleRateIdx,
					&channelCfgIdx, &frameBytes, &reservedBit, &superframeIdx);
	if (ret == 0)
		ret = atrac9GetFrameSamples((int32_t)sampleRateIdx, &work[15]);
	if (ret == 0)
		ret = atrac9Configure(ctx, (int32_t)sampleRateIdx, channelCfgIdx,
				      (int32_t)frameBytes, (int32_t)reservedBit, superframeIdx);
	if (ret == 0) {
		work[9] = (channelCfgIdx == 0U) ? 1U : 2U;
		ret = atrac9Reset(ctx);
	}
	if (ret != 0) {
		work[0] = (uint32_t)ret;
		return AUDIOCODEC_ERROR_DECODE;
	}
	return 0;
}

int32_t atrac9Decode(uint32_t *work)
{
	uint32_t rawMem = work[1];
	if (rawMem == 0U)
		return AUDIOCODEC_ERROR_INVALID_ARG;

	const uint8_t *inBuf = (const uint8_t *)(uintptr_t)work[4];
	int16_t **outPtrs = (int16_t **)(uintptr_t)work[6];
	Atrac9Ctx *ctx = (Atrac9Ctx *)(uintptr_t)((rawMem + 63U) & ~63U);
	ctx->errCode = 0;
	int32_t ret = atrac9DecodeFrame(ctx, inBuf, outPtrs, &work[5], 0);
	if (ret != 0)
		return AUDIOCODEC_ERROR_DECODE;

	work[7] = work[15] << work[9];
	return 0;
}
