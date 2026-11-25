/*
 * PS Vita ePSP 6.61 libasfparser.prx (sceASFParser)
 * WMA v2 superframe, bitstream, and Huffman VLC frame boundary parser.
 */
#include "asf_int.h"

/* Bit reader helpers */
static void wmaBrInit(SceWmaBitReader *br, u32 packetBits)
{
	br->packetBits = packetBits;
	br->consumedBits = 0;
	br->curBit = 0;
	br->endBit = 0;
	br->buf = NULL;
}

static void wmaBrResetBuffer(SceWmaBitReader *br, const u8 *buf, u32 numBits)
{
	br->buf = buf;
	if (numBits > br->packetBits)
		numBits = br->packetBits;
	br->endBit = numBits;
	br->curBit = 0;
	br->consumedBits = 0;
}

static void wmaBrContinueBuffer(SceWmaBitReader *br, const u8 *buf, u32 numBits)
{
	u32 bitOff = br->consumedBits & 7;
	br->buf = buf;
	br->curBit = bitOff;
	if (br->packetBits < br->consumedBits + numBits)
		br->endBit = br->packetBits - br->consumedBits + bitOff;
	else
		br->endBit = numBits + bitOff;
}

static void wmaBrMarkPacketDone(SceWmaBitReader *br)
{
	br->consumedBits = br->packetBits;
	br->curBit = 0;
}

static s32 wmaBrIsPacketDone(const SceWmaBitReader *br)
{
	return ((br->consumedBits & ~7U) + br->curBit) >= br->packetBits;
}

static u32 wmaBrBitsConsumedInWindow(const SceWmaBitReader *br)
{
	return br->curBit - (br->consumedBits & 7);
}

static u32 wmaBrWholeBytesConsumed(const SceWmaBitReader *br)
{
	return br->curBit >> 3;
}

static void wmaBrCommitWindow(SceWmaBitReader *br)
{
	u32 nextPos = (br->consumedBits & ~7U) + br->curBit;
	if (nextPos > br->packetBits)
		nextPos = br->packetBits;
	br->consumedBits = nextPos;
	br->curBit = 0;
	br->endBit = 0;
}

static u32 wmaBrPeek32(const SceWmaBitReader *br)
{
	const u8 *p = br->buf + (br->curBit >> 3);
	u32 word = ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
	return word << (br->curBit & 7);
}

static s32 wmaBrSkipBits(SceWmaBitReader *br, u32 nBits)
{
	u32 nextBit = br->curBit + nBits;
	if (nextBit > br->endBit) {
		br->curBit = br->endBit;
		return SCE_ASF_ERROR_WMA_BITSTREAM_END;
	}
	br->curBit = nextBit;
	return 0;
}

static s32 wmaBrReadBits(SceWmaBitReader *br, u32 nBits, u32 *out)
{
	u32 word = wmaBrPeek32(br);
	*out = (word >> (32 - nBits)) & (0xFFFFFFFFU >> (32 - nBits));
	return wmaBrSkipBits(br, nBits);
}

static s32 wmaBrCopyBits(SceWmaBitReader *br, u8 *dst, u32 nBits)
{
	s32 ret = 0;
	u32 val;

	while (nBits >= 8) {
		ret = wmaBrReadBits(br, 8, &val);
		*dst++ = (u8)val;
		nBits -= 8;
	}
	if (nBits != 0) {
		ret = wmaBrReadBits(br, nBits, &val);
		*dst = (u8)(val << (8 - nBits));
	}
	return ret;
}

/* Multi-level Huffman VLC decoder (8 + 2 + 2 + 2 + 2 + 1 + 1 + 1 + 1 + 1 + 1 bits) */
static s32 wmaDecodeVlc(const u16 *table, SceWmaBitReader *br, s32 *outSym)
{
	u32 bits = wmaBrPeek32(br);
	const u16 *p = &table[bits >> 24];
	u32 entry = *p;

	if ((entry & 0x8000U) == 0) {
		p = &p[entry + ((bits >> 22) & 3)];
		entry = *p;
		if ((entry & 0x8000U) == 0) {
			p = &p[entry + ((bits >> 20) & 3)];
			entry = *p;
			if ((entry & 0x8000U) == 0) {
				p = &p[entry + ((bits >> 18) & 3)];
				entry = *p;
				if ((entry & 0x8000U) == 0) {
					p = &p[entry + ((bits >> 16) & 3)];
					entry = *p;
					if ((entry & 0x8000U) == 0) {
						p = &p[entry + ((bits >> 15) & 1)];
						entry = *p;
						if ((entry & 0x8000U) == 0) {
							p = &p[entry + ((bits >> 14) & 1)];
							entry = *p;
							if ((entry & 0x8000U) == 0) {
								p = &p[entry + ((bits >> 13) & 1)];
								entry = *p;
								if ((entry & 0x8000U) == 0) {
									p = &p[entry + ((bits >> 12) & 1)];
									entry = *p;
									if ((entry & 0x8000U) == 0) {
										p = &p[entry + ((bits >> 11) & 1)];
										entry = *p;
										if ((entry & 0x8000U) == 0)
											entry = p[entry + ((bits >> 10) & 1)];
									}
								}
							}
						}
					}
				}
			}
		}
	}

	*outSym = (s32)(entry & 0x3FF);
	return wmaBrSkipBits(br, (entry >> 10) & 0x1F);
}

/* Math helpers */
static s32 wmaRoundFloat(float x)
{
	return (x < 0.0f) ? (s32)(x - 0.5f) : (s32)(x + 0.5f);
}

static u32 wmaLog2Floor(u32 v)
{
	u32 n = 0;
	while (v >= 2) {
		v >>= 1;
		n++;
	}
	return n;
}

static s32 wmaBlockRatioToIndex(u32 ratio)
{
	switch (ratio) {
	case 1:  return 0;
	case 2:  return 1;
	case 4:  return 2;
	case 8:  return 3;
	case 16: return 4;
	default: return 0;
	}
}

/* WMA configuration & band table setup */
static s32 wmaValidateWaveFormat(const SceAsfWmaWaveFormat *fmt)
{
	if ((u16)(fmt->channels - 1) >= 2)
		return SCE_ASF_ERROR_WMA_BAD_CHANNELS;
	if (fmt->sampleRate - 1 > 48000 - 1)
		return SCE_ASF_ERROR_WMA_BAD_SAMPLERATE;
	if ((s32)fmt->avgBytesPerSec < 0)
		return SCE_ASF_ERROR_WMA_BAD_BITRATE;
	if (fmt->blockAlign == 0)
		return SCE_ASF_ERROR_WMA_BAD_BLOCK_ALIGN;
	return 0;
}

static u32 wmaSelectFrameLen(u32 sampleRate)
{
	if (sampleRate <= 16000)
		return 512;
	if (sampleRate <= 22050)
		return 1024;
	return 2048;
}

static void wmaInitNoiseCoding(SceWmaParserCtx *ctx, float bps, float bpsStereo)
{
	u32 sr = ctx->sampleRate;
	float nyquist = (float)sr * 0.5f;

	ctx->useNoiseCoding = 1;
	ctx->highBandStartFreq = nyquist;

	if (sr >= 44100) {
		if (bpsStereo < 0.61f) {
			ctx->highBandStartFreq = nyquist * 0.4f;
			return;
		}
		ctx->useNoiseCoding = 0;
		return;
	}
	if (sr >= 22050) {
		if (bpsStereo >= 1.16f) {
			ctx->useNoiseCoding = 0;
			return;
		}
		if (bpsStereo < 0.72f)
			ctx->highBandStartFreq = nyquist * 0.6f;
		else
			ctx->highBandStartFreq = nyquist * 0.7f;
		return;
	}
	if (sr >= 16000) {
		if (bps > 0.5f)
			ctx->highBandStartFreq = nyquist * 0.5f;
		else
			ctx->highBandStartFreq = nyquist * 0.3f;
		return;
	}
	if (sr >= 11025) {
		ctx->highBandStartFreq = nyquist * 0.7f;
		return;
	}
	if (sr >= 8000) {
		if (bps <= 0.625f)
			ctx->highBandStartFreq = nyquist * 0.5f;
		else if (bps <= 0.75f)
			ctx->highBandStartFreq = nyquist * 0.65f;
		else
			ctx->useNoiseCoding = 0;
		return;
	}
	if (bps < 0.8f) {
		if (bps < 0.6f)
			ctx->highBandStartFreq = nyquist * 0.5f;
		else
			ctx->highBandStartFreq = nyquist * 0.6f;
	} else {
		ctx->highBandStartFreq = nyquist * 0.75f;
	}
}

static void wmaInitExponentBands(SceWmaParserCtx *ctx, u32 numSizes)
{
	float invSr = 1.0f / (float)ctx->sampleRate;
	u32 i;

	for (i = 0; i < numSizes; i++) {
		s32 *bounds = ctx->bandBounds[i];
		u32 sr = ctx->sampleRate;
		u32 blockDouble = (2 * ctx->frameLen) >> i;
		s32 useHardcoded = 1;

		bounds[0] = 0;
		if (sr < 44100) {
			if (sr < 32000) {
				if (sr >= 22050 && blockDouble == 512) {
					static const s32 b22_512[14] = {
						5, 12, 18, 25, 34, 46, 63, 86, 102, 123, 149, 179, 221, 256
					};
					ctx->bandCounts[i] = 14;
					sceKernelMemcpy(&bounds[1], b22_512, sizeof(b22_512));
				} else if (sr >= 22050 && blockDouble == 256) {
					static const s32 b22_256[10] = {
						5, 11, 17, 23, 31, 43, 62, 89, 110, 128
					};
					ctx->bandCounts[i] = 10;
					sceKernelMemcpy(&bounds[1], b22_256, sizeof(b22_256));
				} else {
					useHardcoded = 0;
				}
			} else {
				if (blockDouble == 256) {
					static const s32 b32_256[11] = {
						4, 9, 14, 19, 25, 35, 51, 76, 96, 124, 128
					};
					ctx->bandCounts[i] = 11;
					sceKernelMemcpy(&bounds[1], b32_256, sizeof(b32_256));
				} else if (blockDouble == 512) {
					static const s32 b32_512[15] = {
						5, 10, 15, 20, 28, 37, 50, 70, 85, 102, 123, 152, 192, 248, 256
					};
					ctx->bandCounts[i] = 15;
					sceKernelMemcpy(&bounds[1], b32_512, sizeof(b32_512));
				} else if (blockDouble == 1024) {
					static const s32 b32_1024[16] = {
						6, 13, 20, 29, 41, 55, 74, 101, 141, 170, 205, 246, 304, 384, 496, 512
					};
					ctx->bandCounts[i] = 16;
					sceKernelMemcpy(&bounds[1], b32_1024, sizeof(b32_1024));
				} else {
					useHardcoded = 0;
				}
			}
		} else {
			if (blockDouble == 256) {
				static const s32 b44_256[12] = {
					4, 9, 12, 16, 21, 26, 37, 45, 55, 70, 90, 128
				};
				ctx->bandCounts[i] = 12;
				sceKernelMemcpy(&bounds[1], b44_256, sizeof(b44_256));
			} else if (blockDouble == 512) {
				static const s32 b44_512[15] = {
					5, 11, 17, 23, 31, 37, 43, 51, 62, 74, 89, 110, 139, 180, 256
				};
				ctx->bandCounts[i] = 15;
				sceKernelMemcpy(&bounds[1], b44_512, sizeof(b44_512));
			} else if (blockDouble == 1024) {
				static const s32 b44_1024[17] = {
					5, 12, 18, 25, 34, 46, 54, 63, 86, 102, 123, 149, 179, 221, 279, 360, 512
				};
				ctx->bandCounts[i] = 17;
				sceKernelMemcpy(&bounds[1], b44_1024, sizeof(b44_1024));
			} else {
				useHardcoded = 0;
			}
		}

		if (useHardcoded) {
			s32 k;
			for (k = 0; k < ctx->bandCounts[i]; k++) {
				s32 v = bounds[k + 1] + 2;
				bounds[k + 1] = (v / 4) * 4;
			}
		} else {
			s32 halfLen = (s32)(blockDouble >> 1);
			s32 k;
			s32 cnt = 1;
			for (k = 0; k < 25; k++) {
				s32 pos = 4 * (s32)((((float)g_wmaBarkFreqs[k] * ((float)blockDouble * invSr)) + 2.0f) * 0.25f);
				if (bounds[cnt - 1] < pos) {
					bounds[cnt] = pos;
					cnt++;
				}
				if (bounds[cnt - 1] >= halfLen)
					break;
			}
			bounds[cnt - 1] = halfLen;
			ctx->bandCounts[i] = cnt - 1;
		}
	}

	ctx->curBandBounds = ctx->bandBounds[0];
	ctx->curBandCount = ctx->bandCounts[0];
}

static s32 wmaInitHighBandStarts(SceWmaParserCtx *ctx, u32 numSizes)
{
	float invSr = 1.0f / (float)ctx->sampleRate;
	u32 i;

	for (i = 0; i < numSizes; i++) {
		u32 blockDouble = (2 * ctx->frameLen) >> i;
		s32 thresh = (s32)((ctx->highBandStartFreq * (float)blockDouble) * invSr + 0.5f);
		s32 count = ctx->bandCounts[i];
		s32 b;

		ctx->highBandStart[i] = count - 1;
		for (b = 1; b < count; b++) {
			if (thresh < ctx->bandBounds[i][b]) {
				ctx->highBandStart[i] = b - 1;
				break;
			}
		}
		if (ctx->highBandStart[i] <= 0)
			return SCE_ASF_ERROR_WMA_NOISE_BANDS;
	}
	ctx->curHighBandStart = ctx->highBandStart[0];
	return 0;
}

static s32 wmaSetupCodecParams(SceWmaParserCtx *ctx, u32 avgBytesPerSec, u32 encodeOptions)
{
	float bps = ((float)avgBytesPerSec * 8.0f) / (float)(ctx->sampleRate * ctx->channels);
	float bpsStereo = (ctx->channels == 2) ? (bps * 1.6f) : bps;
	u32 maxRatio = 1;

	ctx->useExpVlc = encodeOptions & 1;
	ctx->useBitReservoir = (encodeOptions & 2) != 0;
	ctx->useVariableBlockLen = ((encodeOptions & 2) != 0 && (encodeOptions & 4) != 0);

	if (ctx->useVariableBlockLen) {
		u32 shift = (encodeOptions >> 3) & 3;
		if (avgBytesPerSec / ctx->channels >= 4000)
			maxRatio = 8U << shift;
		else
			maxRatio = 2U << shift;
	}
	if ((ctx->frameLen >> 7) < maxRatio)
		maxRatio = ctx->frameLen >> 7;

	u32 numSizes = wmaLog2Floor(maxRatio) + 1;
	ctx->blockLenBits = wmaLog2Floor(wmaLog2Floor(maxRatio)) + 1;
	ctx->minBlockLen = ctx->frameLen / maxRatio;

	wmaInitNoiseCoding(ctx, bps, bpsStereo);

	ctx->frameLenLog2 = wmaLog2Floor(ctx->frameLen);
	u32 cutoff = ctx->frameLen - (9 * ctx->frameLen / 100);
	ctx->coefsEnd = cutoff;
	ctx->maxCoefsEnd = cutoff;

	ctx->coefVlcSet = 0;
	if (ctx->sampleRate >= 32000) {
		if (bpsStereo < 0.72f)
			ctx->coefVlcSet = 1;
		else if (bpsStereo < 1.16f)
			ctx->coefVlcSet = 2;
	}

	s32 avgFrameBytes = wmaRoundFloat((bps * (float)ctx->frameLen) * 0.125f);
	ctx->frameLenBits = wmaLog2Floor((u32)avgFrameBytes) + 2;

	wmaInitExponentBands(ctx, numSizes);
	if (ctx->useNoiseCoding != 0)
		return wmaInitHighBandStarts(ctx, numSizes);
	return 0;
}

/* Subframe & frame decoding (for boundary discovery) */
static void wmaSelectStereoCoefVlc(const u16 **outVlc, u32 vlcSet, u32 msStereo)
{
	if (vlcSet == 0)
		*outVlc = msStereo ? g_wmaVlcCoef1 : g_wmaVlcCoef0;
	else if (vlcSet == 1)
		*outVlc = msStereo ? g_wmaVlcCoef5 : g_wmaVlcCoef4;
	else
		*outVlc = msStereo ? g_wmaVlcCoef3 : g_wmaVlcCoef2;
}

static s32 wmaDecodeBlockLen(SceWmaParserCtx *ctx)
{
	u32 nextLen = ctx->frameLen;
	if (ctx->useVariableBlockLen != 0) {
		u32 code;
		s32 ret = wmaBrReadBits(&ctx->br, ctx->blockLenBits, &code);
		if (ret < 0)
			return ret;
		nextLen = ctx->frameLen / (1U << (code & 0xFF));
	}
	ctx->nextBlockLen = (u16)nextLen;
	ctx->blockPos = (u16)(ctx->blockPos + ctx->curBlockLen);
	if (ctx->blockPos < ctx->frameLen && (s32)ctx->frameLen < (s32)(ctx->blockPos + nextLen))
		return SCE_ASF_ERROR_WMA_FRAME_CORRUPT;
	return 0;
}

static void wmaUpdateBlockParams(SceWmaParserCtx *ctx)
{
	u32 curLen = ctx->curBlockLen;
	s32 idx = wmaBlockRatioToIndex(ctx->frameLen / curLen);

	ctx->curBandCount = ctx->bandCounts[idx];
	ctx->curBandBounds = ctx->bandBounds[idx];
	ctx->coefsEnd = (curLen * ctx->maxCoefsEnd) / ctx->frameLen;

	if (ctx->useNoiseCoding == 1) {
		s32 start = (s32)(((ctx->highBandStartFreq * (float)curLen) * 2.0f) / (float)ctx->sampleRate + 0.5f);
		if (start > (s32)curLen)
			start = (s32)curLen;
		ctx->curHighBandCoefStart = start;
		ctx->curHighBandStart = ctx->highBandStart[idx];
	}

	u32 c;
	for (c = 0; c < ctx->channels; c++)
		ctx->ch[c].coefsCount = ctx->coefsEnd;
}

static s32 wmaDecodeQuantStep(SceWmaParserCtx *ctx)
{
	s32 total = 1;
	u32 val;

	while (1) {
		s32 ret = wmaBrReadBits(&ctx->br, 7, &val);
		if (ret < 0)
			return ret;
		total += (s32)val;
		if (val != 127)
			break;
	}

	if (total < 15)
		ctx->coefBitDepth = 13;
	else if (total < 32)
		ctx->coefBitDepth = 12;
	else if (total < 40)
		ctx->coefBitDepth = 11;
	else if (total < 45)
		ctx->coefBitDepth = 10;
	else
		ctx->coefBitDepth = 9;
	return 0;
}

static s32 wmaDecodeNoiseCoding(SceWmaParserCtx *ctx)
{
	u8 highBandCoded[2] = { 0, 0 };
	u32 c;
	s32 ret = 0;

	for (c = 0; c < ctx->channels; c++) {
		if (ctx->ch[c].coded != 0) {
			s32 band = ctx->curHighBandStart;
			highBandCoded[c] = 0;
			ctx->ch[c].coefsCount = (u32)ctx->curHighBandCoefStart;

			while (1) {
				s32 lo = ctx->curBandBounds[band];
				if (lo < ctx->curHighBandCoefStart)
					lo = ctx->curHighBandCoefStart;
				if (lo >= (s32)ctx->coefsEnd)
					break;

				u32 coded;
				ret = wmaBrReadBits(&ctx->br, 1, &coded);
				if (ret < 0)
					return ret;
				if (coded != 0) {
					highBandCoded[c]++;
				} else {
					s32 hi = ctx->curBandBounds[band + 1];
					if (hi > (s32)ctx->coefsEnd)
						hi = (s32)ctx->coefsEnd;
					ctx->ch[c].coefsCount += (u32)(hi - lo);
				}
				band++;
			}
		} else {
			ctx->ch[c].coefsCount = 0;
		}
	}

	for (c = 0; c < ctx->channels; c++) {
		if (ctx->ch[c].coded != 0 && highBandCoded[c] != 0) {
			ret = wmaBrSkipBits(&ctx->br, 7);
			if (ret < 0)
				return ret;
			s32 b;
			for (b = 1; b < (s32)highBandCoded[c]; b++) {
				s32 sym;
				ret = wmaDecodeVlc(g_wmaVlcNoise, &ctx->br, &sym);
				if (ret < 0)
					return ret;
			}
		}
	}
	return 0;
}

static s32 wmaDecodeExpLpc(SceWmaParserCtx *ctx, s32 hasExp)
{
	u32 c;
	for (c = 0; c < ctx->channels; c++) {
		if (ctx->ch[c].coded != 0 && hasExp == 1) {
			s32 k;
			for (k = 0; k < 10; k++) {
				u32 bits = (k == 0 || k == 8 || k == 9) ? 3 : 4;
				s32 ret = wmaBrSkipBits(&ctx->br, bits);
				if (ret < 0)
					return ret;
			}
		}
	}
	return 0;
}

static s32 wmaDecodeExpVlc(SceWmaParserCtx *ctx, s32 hasExp)
{
	u32 c;
	for (c = 0; c < ctx->channels; c++) {
		if (ctx->ch[c].coded != 0 && hasExp == 1) {
			s32 b;
			for (b = 0; b < ctx->curBandCount; b++) {
				s32 sym;
				s32 ret = wmaDecodeVlc(g_wmaVlcScale, &ctx->br, &sym);
				if (ret < 0)
					return ret;
			}
		}
	}
	return 0;
}

static s32 wmaDecodeChannelCoefs(SceWmaParserCtx *ctx, const SceWmaChannel *ch)
{
	u32 pos = 0;
	s32 ret = 0;

	while (pos < ch->coefsCount) {
		s32 sym;
		ret = wmaDecodeVlc(ch->coefVlc, &ctx->br, &sym);
		if (ret < 0)
			return ret;
		if (sym == 1023) {
			/* Escape code: skip level bits + read run + sign */
			u32 runVal;
			ret = wmaBrSkipBits(&ctx->br, ctx->coefBitDepth);
			if (ret < 0)
				return ret;
			ret = wmaBrReadBits(&ctx->br, ctx->frameLenLog2 + 1, &runVal);
			if (ret < 0)
				return ret;
			pos += (runVal >> 1) + 1;
		} else if (sym == 1021) {
			/* End-of-block marker */
			break;
		} else {
			pos += (u32)sym;
		}
	}

	if (pos > ctx->curBlockLen)
		return SCE_ASF_ERROR_WMA_COEF_OVERFLOW;
	return ret;
}

static s32 wmaDecodeSubframeHeader(SceWmaParserCtx *ctx)
{
	u32 bit;
	s32 allZero;
	s32 ret;

	ret = wmaDecodeBlockLen(ctx);
	if (ret < 0)
		return ret;
	wmaUpdateBlockParams(ctx);

	if (ctx->channels == 1) {
		ret = wmaBrReadBits(&ctx->br, 1, &bit);
		if (ret < 0)
			return ret;
		ctx->ch[0].coded = bit;
		allZero = (bit == 0);
	} else {
		u32 msStereo;
		ret = wmaBrReadBits(&ctx->br, 1, &msStereo);
		if (ret < 0)
			return ret;
		ret = wmaBrReadBits(&ctx->br, 1, &bit);
		if (ret < 0)
			return ret;
		ctx->ch[0].coded = bit;
		ret = wmaBrReadBits(&ctx->br, 1, &bit);
		if (ret < 0)
			return ret;
		ctx->ch[1].coded = bit;
		allZero = (ctx->ch[0].coded == 0 && ctx->ch[1].coded == 0);
		wmaSelectStereoCoefVlc(&ctx->ch[1].coefVlc, ctx->coefVlcSet, msStereo);
	}

	if (allZero) {
		if (ctx->useExpVlc != 0)
			return 0;
		return wmaDecodeExpLpc(ctx, ctx->subframeIndex == 0);
	}

	ret = wmaDecodeQuantStep(ctx);
	if (ret < 0)
		return ret;

	if (ctx->useNoiseCoding == 1) {
		ret = wmaDecodeNoiseCoding(ctx);
		if (ret < 0)
			return ret;
	}

	s32 hasExp = 1;
	if (ctx->useVariableBlockLen != 0 && ctx->curBlockLen < ctx->frameLen) {
		ret = wmaBrReadBits(&ctx->br, 1, &bit);
		if (ret < 0)
			return ret;
		hasExp = (s32)bit;
		if (ctx->subframeIndex == 0 && hasExp != 1)
			return SCE_ASF_ERROR_WMA_FRAME_CORRUPT;
	}

	if (ctx->useExpVlc == 1)
		return wmaDecodeExpVlc(ctx, hasExp);
	return wmaDecodeExpLpc(ctx, hasExp);
}

static s32 wmaDecodeSubframe(SceWmaParserCtx *ctx)
{
	if (ctx->curBlockLen < ctx->minBlockLen)
		return SCE_ASF_ERROR_WMA_BAD_BLOCK_LEN;

	s32 ret = wmaDecodeSubframeHeader(ctx);
	if (ret < 0)
		return ret;

	u32 c;
	for (c = 0; c < ctx->channels; c++) {
		if (ctx->ch[c].coded != 0) {
			ret = wmaDecodeChannelCoefs(ctx, &ctx->ch[c]);
			if (ret < 0)
				return ret;
		}
	}
	ctx->subframeIndex++;
	return 0;
}

static void wmaFinishFrameState(SceWmaParserCtx *ctx)
{
	if (ctx->useBitReservoir != 0) {
		ctx->blockPos = 0;
		ctx->framesLeft--;
	}
	ctx->packetReady = 1;
}

static s32 wmaDecodeFrame(SceWmaParserCtx *ctx, u32 *outPrevLen, u32 *outCurLen)
{
	if (ctx->packetReady != 1)
		return SCE_ASF_ERROR_WMA_NOT_READY;

	ctx->subframeIndex = 0;
	ctx->blockPos = 0;
	*outPrevLen = ctx->prevBlockLen;
	*outCurLen = ctx->curBlockLen;

	while (1) {
		s32 ret = wmaDecodeSubframe(ctx);
		ctx->prevBlockLen = ctx->curBlockLen;
		ctx->curBlockLen = ctx->nextBlockLen;
		if (ret < 0)
			return ret;
		if (ctx->blockPos >= ctx->frameLen) {
			wmaFinishFrameState(ctx);
			return 0;
		}
	}
}

/* Superframe header parsing */
static s32 wmaParseSuperframeHeader(SceWmaParserCtx *ctx, s32 checkSeq)
{
	s32 seqErr = 0;
	ctx->framesLeft = 1;
	if (ctx->useBitReservoir == 0)
		return 0;

	u32 seq, numFrames, skipBits;
	s32 ret = wmaBrReadBits(&ctx->br, 4, &seq);
	if (ret < 0)
		return ret;

	if (checkSeq != 0 && seq != ((ctx->packetSeq + 1) & 0xF))
		seqErr = SCE_ASF_ERROR_WMA_SEQ_MISMATCH;
	ctx->packetSeq = seq;

	ret = wmaBrReadBits(&ctx->br, 4, &numFrames);
	if (ret < 0)
		return ret;
	ctx->framesLeft = (s16)numFrames;

	ret = wmaBrReadBits(&ctx->br, ctx->frameLenBits + 3, &skipBits);
	ctx->bitReservoirSkipBits = skipBits;
	if (ret < 0)
		return ret;
	return seqErr;
}

static s32 wmaReadInitialBlockLens(SceWmaParserCtx *ctx)
{
	if (ctx->useVariableBlockLen != 0) {
		u32 prevCode, curCode;
		s32 ret = wmaBrReadBits(&ctx->br, ctx->blockLenBits, &prevCode);
		if (ret < 0)
			return ret;
		ret = wmaBrReadBits(&ctx->br, ctx->blockLenBits, &curCode);
		if (ret < 0)
			return ret;
		ctx->prevBlockLen = (u16)(ctx->frameLen / (1U << (prevCode & 0xFF)));
		ctx->curBlockLen = (u16)(ctx->frameLen / (1U << (curCode & 0xFF)));
		ctx->blockPos = 0;
	}
	ctx->packetReady = 1;
	return 0;
}

static s32 wmaSkipRemainingPacketBits(SceWmaParserCtx *ctx, SceAsfStream *stream)
{
	u32 used = ctx->br.consumedBits + wmaBrBitsConsumedInWindow(&ctx->br);
	if (used >= ctx->br.packetBits)
		return 0;
	s32 ret = asfPacketSkipBytes(&stream->pkt, (ctx->br.packetBits - used + 7) >> 3);
	wmaBrMarkPacketDone(&ctx->br);
	return ret;
}

static s32 wmaAdvancePayloadIfNeeded(SceAsfStream *stream)
{
	SceWmaParserCtx *ctx = (SceWmaParserCtx *)stream->codecCtx;
	if (asfPacketIsPayloadDone(&stream->pkt)) {
		s32 ret = asfPacketAdvancePayload(&stream->pkt);
		if (ret >= 0) {
			ctx->packetReady = 0;
			ctx->curTimeMs = asfPacketGetSendTime(&stream->pkt);
		}
		return ret;
	}
	ctx->packetReady = 0;
	return 0;
}

static s32 wmaLoadSuperframeHeader(SceWmaParserCtx *ctx, SceAsfStream *stream,
                                   s32 appendTail, SceAsfWmaFrame *frame, s32 checkSeq)
{
	u8 hdrBuf[4112];
	u32 readBits = ctx->br.packetBits;
	if (readBits > 32816)
		readBits = 32816;

	s32 ret = asfPacketPeekPayload(hdrBuf, &stream->pkt, readBits >> 3);
	if (ret < 0)
		return ret;

	wmaBrResetBuffer(&ctx->br, hdrBuf, readBits);
	ret = wmaParseSuperframeHeader(ctx, checkSeq);
	if (ret < 0) {
		if (ret != SCE_ASF_ERROR_WMA_SEQ_MISMATCH)
			return ret;
		if (appendTail != 0) {
			ctx->flagNextDiscontinuity = 1;
		} else {
			ctx->flagPacketLoss = 1;
			ctx->flagDiscontinuity = 1;
		}
		appendTail = 0;
		ret = 0;
	}

	if (ctx->bitReservoirSkipBits != 0) {
		if (appendTail != 0) {
			ret = wmaBrCopyBits(&ctx->br, frame->data + ((frame->bitLength + 7) >> 3),
			                    ctx->bitReservoirSkipBits);
			frame->bitLength += ctx->bitReservoirSkipBits;
		} else {
			ret = wmaBrSkipBits(&ctx->br, ctx->bitReservoirSkipBits);
		}
	}

	if (ret < 0)
		return ret;

	if (ctx->framesLeft != 0) {
		ret = wmaReadInitialBlockLens(ctx);
		if (ret < 0)
			return ret;
	}

	ret = asfPacketSkipBytes(&stream->pkt, wmaBrWholeBytesConsumed(&ctx->br));
	if (ret >= 0)
		wmaBrCommitWindow(&ctx->br);
	return ret;
}

static s32 wmaStartSuperframe(SceWmaParserCtx *ctx, SceAsfStream *stream, s32 checkSeq)
{
	s32 ret = wmaLoadSuperframeHeader(ctx, stream, 0, NULL, checkSeq);
	if (ret < 0)
		return ret;

	while (ctx->framesLeft == 0) {
		ret = wmaSkipRemainingPacketBits(ctx, stream);
		if (ret < 0)
			return ret;
		ret = wmaAdvancePayloadIfNeeded(stream);
		if (ret < 0)
			return ret;
		ret = wmaLoadSuperframeHeader(ctx, stream, 0, NULL, 1);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static s32 wmaContinueSplitFrame(SceWmaParserCtx *ctx, SceAsfStream *stream, SceAsfWmaFrame *frame)
{
	s32 ret = wmaAdvancePayloadIfNeeded(stream);
	if (ret < 0)
		return ret;
	ret = wmaLoadSuperframeHeader(ctx, stream, 1, frame, 1);
	if (ret >= 0 && ctx->framesLeft == 0)
		return wmaSkipRemainingPacketBits(ctx, stream);
	return ret;
}

static u32 wmaCollectFrameFlags(const SceWmaParserCtx *ctx)
{
	u32 flags = (ctx->flagDiscontinuity != 0) ? 1U : 0U;
	if (ctx->flagNextDiscontinuity != 0)
		flags |= 2U;
	if (ctx->flagPacketLoss != 0)
		flags |= 4U;
	return flags;
}

static void wmaClearBitReservoirFlags(SceWmaParserCtx *ctx)
{
	ctx->flagDiscontinuity = (ctx->flagNextDiscontinuity != 0) ? 1U : 0U;
	ctx->flagPacketLoss = 0;
	ctx->flagNextDiscontinuity = 0;
}

/* Public WMA parser entry points */
s32 wmaInitParser(SceWmaParserCtx *ctx, const SceAsfWmaWaveFormat *fmt)
{
	if (ctx == NULL || fmt == NULL)
		return SCE_ASF_ERROR_WMA_INVALID_PARAM;

	s32 ret = wmaValidateWaveFormat(fmt);
	if (ret < 0)
		return ret;

	sceKernelMemset(ctx, 0, 716);
	ctx->packetReady = 0;
	ctx->coefBitDepth = 9;
	ctx->curHighBandCoefStart = 0;
	ctx->frameLen = wmaSelectFrameLen(fmt->sampleRate);
	ctx->channels = fmt->channels;
	ctx->sampleRate = fmt->sampleRate;

	ret = wmaSetupCodecParams(ctx, fmt->avgBytesPerSec, fmt->encodeOptions);
	if (ret < 0)
		return ret;

	ctx->subframeIndex = 0;
	ctx->prevBlockLen = (u16)ctx->frameLen;
	ctx->curBlockLen = (u16)ctx->frameLen;
	ctx->nextBlockLen = (u16)ctx->frameLen;
	ctx->blockPos = 0;

	u32 c;
	for (c = 0; c < ctx->channels; c++) {
		ctx->ch[c].coefVlc = NULL;
		ctx->ch[c].coefsCount = ctx->coefsEnd;
		ctx->ch[c].coded = 0;
	}
	if (ctx->coefVlcSet == 0)
		ctx->ch[0].coefVlc = g_wmaVlcCoef0;
	else if (ctx->coefVlcSet == 1)
		ctx->ch[0].coefVlc = g_wmaVlcCoef4;
	else
		ctx->ch[0].coefVlc = g_wmaVlcCoef2;

	wmaBrInit(&ctx->br, (u32)fmt->blockAlign * 8U);
	ctx->packetReady = 0;

	if (ctx->sampleRate == 0)
		return SCE_ASF_ERROR_WMA_BAD_SAMPLERATE;

	ctx->frameDurationMs = (1000U * ctx->frameLen) / ctx->sampleRate;
	ctx->flagDiscontinuity = 1;
	ctx->flagNextDiscontinuity = 0;
	ctx->flagPacketLoss = 0;
	return 0;
}

static s32 wmaReadFrameReservoir(SceWmaParserCtx *ctx, SceAsfStream *stream, SceAsfWmaFrame *frame)
{
	s32 splitFrame = 0;
	s32 ret;

	if (ctx->packetReady == 0) {
		ret = wmaStartSuperframe(ctx, stream, ctx->flagDiscontinuity == 0);
		if (ret < 0)
			return ret;
	}

	u32 remBytes = (ctx->br.packetBits >> 3) - (ctx->br.consumedBits >> 3);
	u32 remBits = ctx->br.packetBits - ctx->br.consumedBits;
	if (remBytes >= 14862)
		remBytes = 14861;
	if (remBits > 118888)
		remBits = 118888;

	ret = asfPacketPeekPayload(frame->data, &stream->pkt, remBytes);
	if (ret < 0)
		return ret;

	wmaBrContinueBuffer(&ctx->br, frame->data, remBits);
	frame->bitOffset = ctx->br.consumedBits & 7;

	ret = wmaDecodeFrame(ctx, &frame->prevBlockLen, &frame->curBlockLen);
	frame->bitLength = wmaBrBitsConsumedInWindow(&ctx->br);
	if (ret < 0) {
		if (ret != SCE_ASF_ERROR_WMA_BITSTREAM_END)
			return ret;
		if (ctx->framesLeft != 1)
			return SCE_ASF_ERROR_WMA_FRAME_CORRUPT;
		wmaFinishFrameState(ctx);
		splitFrame = 1;
	}

	ret = asfPacketSkipBytes(&stream->pkt, wmaBrWholeBytesConsumed(&ctx->br));
	if (ret < 0)
		return ret;
	wmaBrCommitWindow(&ctx->br);

	if (ctx->framesLeft == 0) {
		if (splitFrame != 0)
			ret = wmaContinueSplitFrame(ctx, stream, frame);
		else
			ret = wmaSkipRemainingPacketBits(ctx, stream);
		if (ret < 0)
			return ret;
	}

	frame->flags = wmaCollectFrameFlags(ctx);
	wmaClearBitReservoirFlags(ctx);
	return 0;
}

static s32 wmaReadFrameFixed(SceWmaParserCtx *ctx, SceAsfStream *stream, SceAsfWmaFrame *frame)
{
	u32 pktBits = ctx->br.packetBits;
	s32 ret = asfPacketPeekPayload(frame->data, &stream->pkt, pktBits >> 3);
	if (ret < 0)
		return ret;

	wmaBrResetBuffer(&ctx->br, frame->data, pktBits);
	if (ctx->packetReady == 0)
		wmaReadInitialBlockLens(ctx);

	ret = wmaDecodeFrame(ctx, &frame->prevBlockLen, &frame->curBlockLen);
	if (ret < 0)
		return ret;

	frame->bitOffset = 0;
	frame->bitLength = wmaBrBitsConsumedInWindow(&ctx->br);
	frame->flags = 0;
	wmaBrMarkPacketDone(&ctx->br);

	ret = asfPacketSkipBytes(&stream->pkt, pktBits >> 3);
	if (ret >= 0) {
		ctx->flagPacketLoss = 0;
		ctx->flagDiscontinuity = 0;
		ctx->flagNextDiscontinuity = 0;
	}
	return ret;
}

s32 wmaReadNextFrame(SceWmaParserCtx *ctx, SceAsfStream *stream, SceAsfWmaFrame *frame)
{
	if (ctx == NULL)
		return SCE_ASF_ERROR_WMA_INVALID_PARAM;

	if (wmaBrIsPacketDone(&ctx->br)) {
		s32 ret = wmaAdvancePayloadIfNeeded(stream);
		if (ret < 0)
			return ret;
	}

	s32 ret;
	if (ctx->useBitReservoir != 0)
		ret = wmaReadFrameReservoir(ctx, stream, frame);
	else
		ret = wmaReadFrameFixed(ctx, stream, frame);

	frame->ptsMs = ctx->curTimeMs;
	ctx->curTimeMs += ctx->frameDurationMs;
	return ret;
}

s32 wmaSeekFrame(SceWmaParserCtx *ctx, SceAsfStream *stream, u32 *timeMs)
{
	u8 scratchBuf[14864];
	s32 didSeek = 0;
	u32 targetMs = *timeMs;
	s32 ret = 0;

	if (ctx == NULL)
		return SCE_ASF_ERROR_WMA_INVALID_PARAM;

	if (asfPacketTimeDelta(&stream->pkt, targetMs) != 0) {
		ret = asfPacketSeekToTime(&stream->pkt, targetMs);
		didSeek = 1;
	} else if (targetMs < ctx->curTimeMs) {
		ret = asfPacketSeekToPacket(&stream->pkt, stream->pkt.packetNumber);
		didSeek = 1;
	}

	if (ret < 0)
		return ret;

	if (didSeek != 0) {
		ctx->packetReady = 0;
		ctx->curTimeMs = asfPacketGetSendTime(&stream->pkt);
		ctx->prevBlockLen = (u16)ctx->frameLen;
		ctx->curBlockLen = (u16)ctx->frameLen;
	}

	if (ctx->packetReady == 0 && ctx->useBitReservoir != 0) {
		s32 checkSeq = (didSeek == 0 && ctx->flagDiscontinuity == 0);
		ret = wmaStartSuperframe(ctx, stream, checkSeq);
		if (ret < 0)
			return ret;
	}

	if (ctx->framesLeft > 0) {
		u32 remBytes = (ctx->br.packetBits >> 3) - (ctx->br.consumedBits >> 3);
		u32 remBits = ctx->br.packetBits - ctx->br.consumedBits;
		if (remBytes >= 14862)
			remBytes = 14861;
		if (remBits > 118888)
			remBits = 118888;

		ret = asfPacketPeekPayload(scratchBuf, &stream->pkt, remBytes);
		if (ret < 0)
			return ret;

		wmaBrContinueBuffer(&ctx->br, scratchBuf, remBits);
		while (targetMs >= ctx->curTimeMs + ctx->frameDurationMs && ctx->framesLeft >= 2) {
			u32 prevLen, curLen;
			if (wmaDecodeFrame(ctx, &prevLen, &curLen) < 0)
				return SCE_ASF_ERROR_WMA_FRAME_CORRUPT;
			if (ctx->framesLeft <= 0)
				break;
			ctx->curTimeMs += ctx->frameDurationMs;
		}

		ret = asfPacketSkipBytes(&stream->pkt, wmaBrWholeBytesConsumed(&ctx->br));
		if (ret < 0)
			return ret;
		wmaBrCommitWindow(&ctx->br);
	} else {
		ret = wmaSkipRemainingPacketBits(ctx, stream);
		if (ret < 0)
			return ret;
	}

	*timeMs = ctx->curTimeMs;
	ctx->flagDiscontinuity = 1;
	return ret;
}

void wmaGetCurTime(const SceWmaParserCtx *ctx, u32 *timeMs)
{
	*timeMs = ctx->curTimeMs;
}
