/*
 * SceAudiocodec (vs0:sys/external/audiocodec.suprx):
 * ATRAC3 software decoder (modes 4, 6, 11 stereo and 14, 15 mono).
 */
#include <arm_neon.h>
#include "audiocodec_internal.h"
#include "atrac3_tables.h"

typedef struct Atrac3ChannelCtx {
	int32_t isJointStereo;		/* 0x0000 */
	int32_t activeQmf;		/* 0x0004 */
	float pcmWork[1162];		/* 0x0008..0x122F */
	float overlap[4][128];		/* 0x1230..0x1A2F */
	int32_t bandCfg[4][16];		/* 0x1A30..0x1B2F */
} Atrac3ChannelCtx;			/* size 0x1B30 (6960B) */

typedef struct Atrac3Ctx {
	const float *jsWeights[4];	/* 0x00 */
	const float *jsScale;		/* 0x10 */
	int32_t jsNumQmf;		/* 0x14 */
	const float *prevJsWeights[4];	/* 0x18 */
	const float *prevJsScale;	/* 0x28 */
	int32_t prevJsNumQmf;		/* 0x2C */
	int32_t channels;		/* 0x30 */
	int32_t frameBytes;		/* 0x34 */
	int32_t varFrameFlag;		/* 0x38 */
	uint8_t *bitstream;		/* 0x3C */
	int32_t bitpos;			/* 0x40 */
	int32_t errCode;		/* 0x44 */
	Atrac3ChannelCtx ch[2];		/* 0x48..0x36A7 (13992B) */
	uint8_t inputCopy[384];		/* 0x36A8..0x3827 (14376B) */
} Atrac3Ctx;

static inline int32_t maxSigned(int32_t a, int32_t b)
{
	return (a > b) ? a : b;
}

static inline int32_t minSigned(int32_t a, int32_t b)
{
	return (a < b) ? a : b;
}

static __attribute__((noinline)) Atrac3Ctx *
atrac3InitCtx(int32_t channels, int32_t jointStereo, int32_t frameBytes, Atrac3Ctx *ctx)
{
	if (frameBytes > 256 || ctx == NULL)
		return NULL;

	int32_t extraCh = maxSigned(channels - 1, 0);
	sceClibMemset(ctx, 0, (size_t)extraCh * sizeof(Atrac3ChannelCtx) + 13992U);

	ctx->channels = channels;
	if (jointStereo != 0)
		frameBytes *= channels;
	ctx->frameBytes = frameBytes;
	ctx->jsNumQmf = 4;

	const float *defaultJsWeight = &kJointStereoWeights[3];
	for (int32_t band = 0; band < 4; ++band) {
		ctx->jsWeights[band] = defaultJsWeight;
		for (int32_t ch = 0; ch < channels; ++ch) {
			ctx->ch[ch].bandCfg[band][0] = 0xFF;
			ctx->ch[ch].bandCfg[band][1] = 4;
		}
	}
	ctx->ch[1].isJointStereo = jointStereo;
	for (int32_t band = 0; band < 4; ++band)
		ctx->prevJsWeights[band] = ctx->jsWeights[band];
	ctx->prevJsScale = ctx->jsScale;
	ctx->prevJsNumQmf = ctx->jsNumQmf;
	return ctx;
}

static __attribute__((noinline)) void
atrac3DecodeSpectrum(Atrac3Ctx *ctx, const Atrac3HuffInfo *huffInfo, int32_t tblIdx,
		     float *out, float *end, float scale)
{
	const int8_t *valTbl = huffInfo->valTbl[tblIdx];
	int32_t maxBits = huffInfo->maxBits[tblIdx];
	const int8_t *lenTbl = huffInfo->lenTbl[tblIdx];
	int32_t bitpos = ctx->bitpos;
	uint8_t *bs = ctx->bitstream;

	if (tblIdx == 0) {
		uint32_t mask = 0xFFFFFFFFU >> maxBits;
		do {
			int32_t byteOff = bitpos >> 3;
			int32_t bitRem = bitpos & 7;
			int32_t shift = maxBits - bitRem;
			uint32_t word = ((uint32_t)bs[byteOff] << 24) |
					((uint32_t)bs[byteOff + 1] << 16) |
					((uint32_t)bs[byteOff + 2] << 8) |
					(uint32_t)bs[byteOff + 3];
			bitpos &= ~7;
			for (int pairIdx = 0; pairIdx < 4; ++pairIdx) {
				uint32_t code = ((uint32_t)((int32_t)word >> shift)) & mask;
				int32_t sym = valTbl[code];
				if (sym < 0) {
					ctx->errCode = 0x189;
					return;
				}
				const float *pair = &kQuantPairValues[sym];
				out[pairIdx * 2 + 0] += scale * pair[0];
				out[pairIdx * 2 + 1] += scale * pair[1];
				shift -= lenTbl[sym];
			}
			bitpos += (maxBits - shift);
			out += 8;
		} while (out < end);
		ctx->bitpos = bitpos;
	} else {
		const float *quantTbl = kQuantLevelTables[tblIdx - 1];
		uint32_t mask = (uint32_t)(0xFFFF >> maxBits);
		while (out < end) {
			int32_t byteOff = bitpos >> 3;
			uint32_t word = ((uint32_t)bs[byteOff] << 8) | (uint32_t)bs[byteOff + 1];
			int32_t shift = maxBits - (bitpos & 7);
			uint32_t code = (word >> shift) & mask;
			int32_t sym = valTbl[code];
			if (sym < 0) {
				ctx->errCode = 0x189;
				return;
			}
			*out += quantTbl[sym] * scale;
			bitpos += lenTbl[sym];
			++out;
		}
		ctx->bitpos = bitpos;
	}
}

static __attribute__((noinline)) void
atrac3DecodeChannel(Atrac3Ctx *ctx, Atrac3ChannelCtx *chCtx, int32_t numQmf)
{
	int32_t localBandCfg[64];
	float localSpec[1024];
	uint8_t *bs = ctx->bitstream;
	int32_t bitpos = ctx->bitpos;
	int32_t maxSpecPos = 0;
	int32_t gainOk = 1;

	for (int32_t band = 0; band <= numQmf && gainOk; ++band) {
		uint32_t regWord = ((uint32_t)bs[bitpos >> 3] << 8) | bs[(bitpos >> 3) + 1];
		int32_t numRegions = (int32_t)((regWord >> (13 - (bitpos & 7))) & 7U);
		bitpos += 3;
		ctx->bitpos = bitpos;
		int32_t regIdx = 0;
		if (numRegions != 0)
			maxSpecPos = (band + 1) * 256;
		for (; regIdx < numRegions; ++regIdx) {
			uint32_t levelWord =
				((uint32_t)bs[bitpos >> 3] << 8) | bs[(bitpos >> 3) + 1];
			int32_t gainLevel = (int32_t)((levelWord >> (12 - (bitpos & 7))) & 0x0FU);
			int32_t locBitpos = bitpos + 4;
			uint32_t locWord =
				((uint32_t)bs[locBitpos >> 3] << 8) | bs[(locBitpos >> 3) + 1];
			int32_t locVal = (int32_t)((locWord >> (8 - (locBitpos & 7))) & 0xF8U);
			localBandCfg[band * 16 + regIdx * 2 + 0] = locVal;
			localBandCfg[band * 16 + regIdx * 2 + 1] = gainLevel;
			bitpos += 9;
			if (regIdx > 0 && localBandCfg[band * 16 + (regIdx - 1) * 2] >= locVal) {
				ctx->bitpos = bitpos;
				ctx->errCode = 0x185;
				gainOk = 0;
				break;
			}
		}
		if (gainOk) {
			ctx->bitpos = bitpos;
			localBandCfg[band * 16 + regIdx * 2 + 0] = 0xFF;
			localBandCfg[band * 16 + regIdx * 2 + 1] = 4;
		}
	}

	if (!gainOk)
		return;

	for (int32_t band = numQmf + 1; band < 4; ++band) {
		localBandCfg[band * 16 + 0] = 0xFF;
		localBandCfg[band * 16 + 1] = 4;
	}

	sceClibMemset(localSpec, 0, sizeof(localSpec));

	uint32_t tonalWord = ((uint32_t)bs[bitpos >> 3] << 8) | bs[(bitpos >> 3) + 1];
	int32_t numTonalGroups = (int32_t)((tonalWord >> (11 - (bitpos & 7))) & 0x1FU);
	bitpos += 5;
	ctx->bitpos = bitpos;

	if (numTonalGroups != 0) {
		uint32_t modeWord = ((uint32_t)bs[bitpos >> 3] << 8) | bs[(bitpos >> 3) + 1];
		int32_t tonalMode = (int32_t)((modeWord >> (14 - (bitpos & 7))) & 3U);
		bitpos += 2;
		ctx->bitpos = bitpos;
		if (tonalMode == 2) {
			maxSpecPos = 0;
			ctx->errCode = 0x184;
		} else {
			int32_t bandMask = (-0x40) >> numQmf;
			int32_t shiftStep = 3 - numQmf;
			int32_t totalTonalComps = 0;
			int32_t tonalAborted = 0;

			for (int32_t grp = 0; grp < numTonalGroups && !tonalAborted; ++grp) {
				int32_t lenBitpos = bitpos + numQmf + 4;
				uint32_t maskWord =
					((uint32_t)bs[bitpos >> 3] << 8) | bs[(bitpos >> 3) + 1];
				uint32_t lenWord = ((uint32_t)bs[lenBitpos >> 3] << 8) |
						   bs[(lenBitpos >> 3) + 1];
				int32_t rawMask = (int32_t)((maskWord >> (9 - (bitpos & 7))) & 0x7FU);
				int32_t rawLen = (int32_t)((lenWord >> (13 - (lenBitpos & 7))) & 7U);
				int32_t bandBits = rawMask & bandMask;
				int32_t valsPerComp = ((rawMask - bandBits) >> shiftStep) + 1;
				int32_t huffNum = rawLen - 1;
				bitpos += numQmf + 7;
				ctx->bitpos = bitpos;
				if (huffNum <= 0) {
					maxSpecPos = 0;
					ctx->errCode = 0x181;
					break;
				}
				int32_t huffSel = tonalMode;
				if (tonalMode >= 2) {
					huffSel = (bs[bitpos >> 3] >> ((~bitpos) & 7)) & 1;
					bitpos += 1;
					ctx->bitpos = bitpos;
				}
				const Atrac3HuffInfo *huffInfo = &kHuffTableSet[huffSel];
				int32_t subbandPos = 0;
				while (!tonalAborted) {
					int32_t groupDone = 0;
					if ((subbandPos & 0xFF) == 0) {
						do {
							bandBits <<= 1;
							if (bandBits & 0x80)
								break;
							subbandPos += 0x100;
							if ((bandBits & 0xFF) == 0) {
								groupDone = 1;
								break;
							}
						} while ((subbandPos & 0xFF) == 0);
					}
					if (groupDone)
						break;
					uint32_t compWord = ((uint32_t)bs[bitpos >> 3] << 8) |
							    bs[(bitpos >> 3) + 1];
					int32_t numComps =
						(int32_t)((compWord >> (13 - (bitpos & 7))) & 7U);
					bitpos += 3;
					ctx->bitpos = bitpos;
					for (int32_t compIdx = 0; compIdx < numComps; ++compIdx) {
						int32_t sfBitpos = bitpos;
						int32_t posBitpos = bitpos + 6;
						bitpos += 12;
						ctx->bitpos = bitpos;
						uint32_t sfWord =
							((uint32_t)bs[sfBitpos >> 3] << 8) |
							bs[(sfBitpos >> 3) + 1];
						uint32_t posWord =
							((uint32_t)bs[posBitpos >> 3] << 8) |
							bs[(posBitpos >> 3) + 1];
						int32_t relPos =
							(int32_t)((posWord >> (10 - (posBitpos & 7))) &
								  0x3FU);
						int32_t absPos = subbandPos + relPos;
						int32_t endPos =
							minSigned(absPos + valsPerComp, 1024);
						int32_t sfIdx =
							(int32_t)((sfWord >> (10 - (sfBitpos & 7))) &
								  0x3FU);
						maxSpecPos = maxSigned(maxSpecPos, endPos);
						++totalTonalComps;
						if (totalTonalComps == 65) {
							maxSpecPos = 0;
							ctx->errCode = 0x180;
							tonalAborted = 1;
							break;
						}
						atrac3DecodeSpectrum(ctx, huffInfo, huffNum,
								     &localSpec[absPos],
								     &localSpec[endPos],
								     kScaleFactorTable[sfIdx]);
						bs = ctx->bitstream;
						bitpos = ctx->bitpos;
					}
					subbandPos += 0x40;
				}
			}
		}
	}

	int32_t tblBitpos = bitpos + 5;
	uint32_t sbWord = ((uint32_t)bs[bitpos >> 3] << 8) | bs[(bitpos >> 3) + 1];
	uint8_t tblByte = bs[tblBitpos >> 3];
	int32_t numSubbands = (int32_t)(((sbWord >> (11 - (bitpos & 7))) & 0x1FU) + 1U);
	bitpos += 6;
	ctx->bitpos = bitpos;
	int32_t huffSel = (tblByte >> ((~tblBitpos) & 7)) & 1;
	const Atrac3HuffInfo *huffInfo = &kHuffTableSet[huffSel];
	int32_t *sbHuff = (int32_t *)chCtx->pcmWork;
	int32_t numCodedBits = 0;
	for (int32_t sb = 0; sb < numSubbands; ++sb) {
		uint32_t huffWord = ((uint32_t)bs[bitpos >> 3] << 8) | bs[(bitpos >> 3) + 1];
		int32_t huffIdx = (int32_t)((huffWord >> (13 - (bitpos & 7))) & 7U) - 1;
		bitpos += 3;
		ctx->bitpos = bitpos;
		sbHuff[sb] = huffIdx;
		if (huffIdx >= 0)
			numCodedBits += 6;
	}
	int32_t sfBitpos = bitpos;
	bitpos += numCodedBits;
	ctx->bitpos = bitpos;
	for (int32_t sb = 0; sb < numSubbands; ++sb) {
		int32_t huffIdx = sbHuff[sb];
		if (huffIdx >= 0) {
			uint32_t sfWord = ((uint32_t)ctx->bitstream[sfBitpos >> 3] << 8) |
					  ctx->bitstream[(sfBitpos >> 3) + 1];
			int32_t sfIdx = (int32_t)((sfWord >> (10 - (sfBitpos & 7))) & 0x3FU);
			sfBitpos += 6;
			atrac3DecodeSpectrum(ctx, huffInfo, huffIdx,
					     &localSpec[kSubbandBoundaries[sb]],
					     &localSpec[kSubbandBoundaries[sb + 1]],
					     kScaleFactorTable[sfIdx]);
		}
	}

	int32_t maxPos = maxSigned(kSubbandBoundaries[numSubbands], maxSpecPos);
	int32_t activeQmf = (maxPos + 0xFF) >> 8;
	int32_t prevActive = chCtx->activeQmf;
	chCtx->activeQmf = activeQmf;
	int32_t runQmf = maxSigned(prevActive, activeQmf);

	sceClibMemcpy_safe(chCtx->pcmWork, &chCtx->pcmWork[1024], 138U * sizeof(float));

	for (int32_t band = 0; band < runQmf; ++band) {
		float *specBand = &localSpec[band * 256];
		if ((band & 1) == 0) {
			for (int idx = 0; idx < 128; ++idx)
				specBand[idx] -= specBand[255 - idx];
		} else {
			for (int idx = 0; idx < 128; ++idx) {
				float topVal = specBand[idx];
				float botVal = specBand[255 - idx];
				specBand[255 - idx] = topVal;
				specBand[idx] = botVal - topVal;
			}
		}

		float midCos = kImdctCosTable[63];
		for (int idx = 0; idx < 128; ++idx) {
			float leftVal = specBand[idx];
			float rightScaled = specBand[idx + 128] * midCos;
			specBand[idx + 128] = leftVal - rightScaled;
			specBand[idx] = leftVal + rightScaled;
		}

		for (int32_t step = 64; step != 2; step >>= 1) {
			int32_t half = step >> 1;
			int32_t cosIdx = 127 - half;
			for (float *blk = specBand; cosIdx >= 0; blk += step * 2, cosIdx -= step) {
				for (int32_t idx = 0; idx < half; ++idx)
					blk[idx] -= blk[step * 2 - 1 - idx];
				float cosVal = kImdctCosTable[cosIdx];
				for (int32_t idx = 0; idx < step; ++idx) {
					float leftVal = blk[idx];
					float rightScaled = blk[idx + step] * cosVal;
					blk[idx + step] = leftVal - rightScaled;
					blk[idx] = leftVal + rightScaled;
				}
			}
		}

		const float *cosPtr = &kImdctCosTable[126];
		for (int idx = 0; idx < 256; idx += 4, cosPtr -= 2) {
			float diffEven = specBand[idx + 0] - specBand[idx + 3];
			float diffOdd = specBand[idx + 1] - specBand[idx + 2];
			float prodEven = specBand[idx + 2] * cosPtr[0];
			float prodOdd = specBand[idx + 3] * cosPtr[0];
			specBand[idx + 0] = diffEven + prodEven;
			specBand[idx + 1] = diffOdd + prodOdd;
			specBand[idx + 2] = diffEven - prodEven;
			specBand[idx + 3] = diffOdd - prodOdd;
		}

		float gainInit = kGainStepTable[localBandCfg[band * 16 + 1]];
		float *overlapBase = chCtx->overlap[band];
		float *pcmBand = &chCtx->pcmWork[138 + band];
		for (int pair = 0; pair < 64; ++pair) {
			for (int sub = 0; sub < 2; ++sub) {
				uint32_t revIdx = kImdctBitRevIdx[pair * 2 + sub];
				const float *tw = kImdctTwiddleTable[revIdx >> 2];
				float specEven = specBand[pair * 4 + sub];
				float specOdd = specBand[pair * 4 + 2 + sub];
				float deltaVal = (specEven - specOdd) - tw[3] * specOdd;
				float sumVal = (specEven - specOdd) + tw[3] * specOdd;
				float scaledDelta = deltaVal * (tw[2] * gainInit);
				float prevOverlap = overlapBase[revIdx >> 2];
				overlapBase[revIdx >> 2] = sumVal * tw[1];
				pcmBand[508 - revIdx] = prevOverlap - tw[0] * scaledDelta;
				pcmBand[revIdx + 512] = prevOverlap * tw[0] + scaledDelta;
			}
		}

		int32_t *cfgPtr = chCtx->bandCfg[band];
		float *curPtr = pcmBand;
		float *endPtr = pcmBand + 1024;
		while (curPtr < endPtr) {
			int32_t nextLoc = cfgPtr[0];
			int32_t curGain = cfgPtr[1];
			float *limPtr = pcmBand + (nextLoc << 2) + 4;
			if (curGain != 4) {
				float gainFactor = kGainStepTable[curGain];
				*curPtr *= gainFactor;
				curPtr += 4;
				while (curPtr < limPtr) {
					*curPtr *= gainFactor;
					curPtr += 4;
				}
			} else if (limPtr < endPtr) {
				curPtr = limPtr;
			} else {
				break;
			}
			cfgPtr += 2;
			int32_t nextGain = cfgPtr[1];
			int32_t stepIdx = (curGain << 5) + ((nextGain - curGain) << 2);
			for (int interp = 0; interp < 7; ++interp) {
				float fineGain = kGainStepTable[((stepIdx & 0x1C) + 0x40) >> 2];
				float coarseGain = kGainStepTable[(stepIdx & 0x1E0) >> 5];
				*curPtr = (*curPtr * fineGain) * coarseGain;
				curPtr += 4;
				stepIdx += (nextGain - curGain) << 2;
			}
		}
	}

	for (int32_t band = runQmf; band < 4; ++band) {
		sceClibMemset(chCtx->overlap[band], 0, sizeof(chCtx->overlap[band]));
		for (int idx = 0; idx < 256; ++idx)
			chCtx->pcmWork[138 + band + idx * 4] = 0.0f;
	}

	int32_t jsOk = 1;
	if (chCtx->isJointStereo != 0) {
		const float eighthStep = 0.125f;
		const float twoScale = 2.0f;
		int32_t prevQmf = ctx->prevJsNumQmf;
		int32_t curQmf = ctx->jsNumQmf;

		for (int32_t band = 0; band < 4; ++band) {
			float prevScaleLeft = twoScale;
			float prevScaleRight = twoScale;
			if (band > prevQmf) {
				prevScaleLeft = ctx->prevJsScale[0];
				prevScaleRight = ctx->prevJsScale[1];
			}
			float curScaleLeft = twoScale;
			float curScaleRight = twoScale;
			if (band > curQmf) {
				curScaleLeft = ctx->jsScale[0];
				curScaleRight = ctx->jsScale[1];
			}
			const float *weightCur = ctx->jsWeights[band];
			const float *weightPrev = ctx->prevJsWeights[band];
			float *leftPcm = &ctx->ch[0].pcmWork[138 + band];
			float *rightPcm = &ctx->ch[1].pcmWork[138 + band];
			int32_t sampleIdx = 0;

			if (curScaleLeft != prevScaleLeft || weightCur != weightPrev) {
				float deltaScaleLeft = (curScaleLeft - prevScaleLeft) * eighthStep;
				float deltaScaleRight = (curScaleRight - prevScaleRight) * eighthStep;
				float deltaWeightLeft = (weightCur[0] - weightPrev[0]) * eighthStep;
				float deltaWeightRight = (weightCur[1] - weightPrev[1]) * eighthStep;
				for (; sampleIdx < 8; ++sampleIdx) {
					float ramp = (float)(sampleIdx - 8);
					float mixWeightLeft = weightCur[0] + deltaWeightLeft * ramp;
					float mixWeightRight = weightCur[1] + deltaWeightRight * ramp;
					float rampScaleLeft = curScaleLeft + deltaScaleLeft * ramp;
					float rampScaleRight = curScaleRight + deltaScaleRight * ramp;
					float leftSample = leftPcm[sampleIdx * 4];
					float rightSample = rightPcm[sampleIdx * 4];
					float mixed = leftSample * mixWeightLeft +
						      rightSample * mixWeightRight;
					leftPcm[sampleIdx * 4] = mixed * rampScaleLeft;
					rightPcm[sampleIdx * 4] = (leftSample - mixed) * rampScaleRight;
				}
			}
			for (; sampleIdx < 256; ++sampleIdx) {
				float leftSample = leftPcm[sampleIdx * 4];
				float rightSample = rightPcm[sampleIdx * 4];
				float mixed = leftSample * weightCur[0] + rightSample * weightCur[1];
				leftPcm[sampleIdx * 4] = curScaleLeft * mixed;
				rightPcm[sampleIdx * 4] = curScaleRight * (leftSample - mixed);
			}
		}

		for (int32_t band = 0; band < 4; ++band)
			ctx->prevJsWeights[band] = ctx->jsWeights[band];
		ctx->prevJsScale = ctx->jsScale;
		ctx->prevJsNumQmf = ctx->jsNumQmf;
		ctx->jsNumQmf = numQmf;

		uint8_t firstByte = ctx->bitstream[0];
		ctx->jsScale = kJointStereoScalePairs[firstByte >> 4];
		int32_t jsBits = (((int32_t)firstByte << 8) | ctx->bitstream[1]) >> 4;
		for (int32_t band = 3; band >= 0; --band) {
			int32_t sel = jsBits & 3;
			if (sel == 2) {
				ctx->errCode = 0x18A;
				jsOk = 0;
				break;
			}
			ctx->jsWeights[band] = &kJointStereoWeights[sel];
			jsBits >>= 2;
		}
	}

	if (jsOk)
		sceClibMemcpy_safe(chCtx->bandCfg, localBandCfg, sizeof(localBandCfg));
}

static __attribute__((noinline)) int32_t
atrac3DecodeFrame(uint8_t *inBuf, float **outChPtrs, Atrac3Ctx *ctx)
{
	int32_t frameBytes = ctx->frameBytes;
	int32_t channels = ctx->channels;
	ctx->bitstream = inBuf;
	ctx->errCode = 0;
	if (channels <= 0)
		return 0;

	for (int32_t ch = 0; ch < channels; ++ch)
		outChPtrs[ch] = ctx->ch[ch].pcmWork;

	int32_t totalConsumed = 0;
	int32_t availBytes = frameBytes;
	for (int32_t ch = 0; ch < channels; ++ch) {
		Atrac3ChannelCtx *chCtx = &ctx->ch[ch];
		int32_t isJointCh = chCtx->isJointStereo;
		uint8_t *bs = ctx->bitstream;
		int32_t headerOk = 1;

		if (isJointCh != 0) {
			uint8_t *endPtr = bs + frameBytes - 1;
			while (endPtr > bs && *endPtr == 0xF8U)
				--endPtr;
			int32_t prevBytes = (ctx->bitpos + 7) >> 3;
			availBytes = (int32_t)(endPtr - bs) - prevBytes + 1;
			for (uint8_t *l = bs, *r = endPtr; l < r; ++l, --r) {
				uint8_t tmp = *l;
				*l = *r;
				*r = tmp;
			}
			ctx->bitpos = 16;
			if ((bs[1] & 0x0CU) != 0x0CU) {
				ctx->errCode = 0x183;
				headerOk = 0;
			}
		} else {
			uint8_t hdrByte = bs[totalConsumed];
			ctx->bitpos = (totalConsumed << 3) + 8;
			if ((hdrByte & 0xFCU) != 0xA0U) {
				ctx->errCode = 0x182;
				headerOk = 0;
			}
		}

		if (headerOk) {
			int32_t numQmf = (isJointCh != 0) ? (bs[1] & 3) : (bs[totalConsumed] & 3);
			atrac3DecodeChannel(ctx, chCtx, numQmf);
		}

		int32_t bitpos = ctx->bitpos;
		if ((availBytes << 3) + 16 < bitpos)
			ctx->errCode = 0x18B;

		frameBytes = ctx->frameBytes;
		if (ctx->varFrameFlag != 0) {
			int32_t usedBytes = (bitpos + 7) >> 3;
			if (ctx->ch[1].isJointStereo != 0)
				ctx->errCode = 1;
			totalConsumed = usedBytes;
			if (usedBytes < frameBytes) {
				while (totalConsumed < frameBytes &&
				       ctx->bitstream[totalConsumed] == 0)
					++totalConsumed;
			}
		} else {
			totalConsumed += frameBytes;
			if (ctx->ch[1].isJointStereo == 0)
				availBytes += frameBytes;
		}
	}

	int32_t err = ctx->errCode;
	if (err != 0 && err != 0x18B)
		return -err;

	for (int32_t ch = 0; ch < channels; ++ch) {
		float *pcm = ctx->ch[ch].pcmWork;
		float *pcmEnd = pcm + 1024;
		while (pcm != pcmEnd) {
			pcm += 4;
			float bandZero = pcm[134];
			float bandOne = pcm[135];
			float bandTwo = pcm[137];
			float bandThree = pcm[136];
			float diffLo = bandZero - bandOne;
			float sumLo = bandZero + bandOne;
			float sumHi = bandTwo + bandThree;
			float diffHi = bandTwo - bandThree;
			pcm[135] = sumLo;
			pcm[137] = sumHi;
			pcm[134] = diffLo;
			pcm[136] = diffHi;

			float accLoEven = diffLo + pcm[42] * kQmfPrototypeCoeffs[0];
			float accLoOdd = sumLo * kQmfPrototypeCoeffs[0] + pcm[43];
			float accHiEven = diffHi + pcm[44] * kQmfPrototypeCoeffs[0];
			float accHiOdd = sumHi * kQmfPrototypeCoeffs[0] + pcm[45];
			for (int tap = 1; tap < 23; ++tap) {
				const float *hist = &pcm[134 - tap * 4];
				float coeffRev = kQmfPrototypeCoeffs[23 - tap];
				float coeffFwd = kQmfPrototypeCoeffs[tap];
				accLoEven += hist[0] * coeffRev;
				accHiEven += hist[2] * coeffRev;
				accLoOdd += hist[1] * coeffFwd;
				accHiOdd += hist[3] * coeffFwd;
			}

			float stageDiffEven = accLoEven - accHiEven;
			float stageSumEven = accLoEven + accHiEven;
			float stageDiffOdd = accLoOdd - accHiOdd;
			float stageSumOdd = accLoOdd + accHiOdd;
			float prevZero = pcm[-4];
			float prevOne = pcm[-3];
			float prevTwo = pcm[-2];
			float prevThree = pcm[-1];
			pcm[43] = stageSumEven;
			pcm[45] = stageSumOdd;
			pcm[42] = stageDiffEven;
			pcm[44] = stageDiffOdd;

			float outEvenLo = prevZero * kQmfPrototypeCoeffs[0] + stageDiffEven;
			float outOddLo = prevOne + stageSumEven * kQmfPrototypeCoeffs[0];
			float outEvenHi = prevTwo * kQmfPrototypeCoeffs[0] + stageDiffOdd;
			float outOddHi = prevThree + stageSumOdd * kQmfPrototypeCoeffs[0];
			for (int tap = 0; tap < 22; ++tap) {
				const float *hist = &pcm[40 - tap * 2];
				float coeffRev = kQmfPrototypeCoeffs[22 - tap];
				float coeffFwd = kQmfPrototypeCoeffs[1 + tap];
				outEvenLo += hist[0] * coeffRev;
				outOddLo += hist[1] * coeffFwd;
				outEvenHi += hist[2] * coeffRev;
				outOddHi += hist[3] * coeffFwd;
			}

			pcm[-4] = outEvenLo;
			pcm[-3] = outOddLo;
			pcm[-2] = outEvenHi;
			pcm[-1] = outOddHi;
		}
	}

	if (ctx->errCode != 0)
		return -ctx->errCode;
	return totalConsumed;
}

int32_t atrac3CheckNeedMem(uint32_t *work)
{
	uint32_t mode = work[8];
	work[0] = 0;
	work[1] = 0;
	switch (mode) {
	case 4U:
	case 6U:
	case 11U:
	case 14U:
	case 15U:
		work[2] = 14376U;
		return 0;
	default:
		return AUDIOCODEC_ERROR_UNSUPPORTED;
	}
}

int32_t atrac3Init(uint32_t *work)
{
	uint32_t mode = work[8];
	Atrac3Ctx *ctx = (Atrac3Ctx *)(uintptr_t)work[1];
	int32_t idx;

	switch (mode) {
	case 4U:
		idx = 0;
		break;
	case 6U:
		idx = 1;
		break;
	case 11U:
		idx = 2;
		break;
	case 14U:
		idx = 3;
		break;
	case 15U:
		idx = 4;
		break;
	default:
		return AUDIOCODEC_ERROR_UNSUPPORTED;
	}

	uint32_t frameBytes = kAtrac3ModeTable[idx].frameBytes;
	uint32_t channels = kAtrac3ModeTable[idx].channels;
	work[9] = 44100U;
	work[10] = frameBytes;
	work[11] = channels;

	uint32_t bit = 1U << (mode - 4U);
	int32_t jointStereo;
	if ((bit & 0xC05U) != 0)
		jointStereo = 0;
	else if ((bit & 0x80U) != 0)
		jointStereo = 1;
	else
		return AUDIOCODEC_ERROR_UNSUPPORTED;

	if (atrac3InitCtx((int32_t)channels, jointStereo, (int32_t)(frameBytes / channels), ctx) !=
	    NULL)
		return 0;
	return AUDIOCODEC_ERROR_DECODE;
}

int32_t atrac3Decode(uint32_t *work)
{
	Atrac3Ctx *ctx = (Atrac3Ctx *)(uintptr_t)work[1];
	float *chPtrs[2];

	sceClibMemcpy_safe(ctx->inputCopy, (const void *)(uintptr_t)work[4], work[10]);
	int32_t ret = atrac3DecodeFrame(ctx->inputCopy, chPtrs, ctx);
	if (ret < 0) {
		work[0] = (uint32_t)(-ret);
		return AUDIOCODEC_ERROR_DECODE;
	}

	int32x4_t maxVal = vdupq_n_s32(0x7FFF);
	int32x4_t minVal = vdupq_n_s32(-0x8000);
	if (work[11] == 2U) {
		const float *left = chPtrs[0];
		const float *right = chPtrs[1];
		int16_t *out = (int16_t *)(uintptr_t)work[6];
		for (int i = 0; i < 1024; i += 4) {
			int32x4_t lv = vmaxq_s32(vminq_s32(vcvtq_s32_f32(vld1q_f32(&left[i])),
							   maxVal),
						 minVal);
			int32x4_t rv = vmaxq_s32(vminq_s32(vcvtq_s32_f32(vld1q_f32(&right[i])),
							   maxVal),
						 minVal);
			int16x8x2_t interleaved =
				vtrnq_s16(vreinterpretq_s16_s32(lv), vreinterpretq_s16_s32(rv));
			vst1q_s16(&out[i * 2], interleaved.val[0]);
		}
	} else {
		const float *mono = chPtrs[0];
		int16_t *out = (int16_t *)(uintptr_t)work[6];
		for (int i = 0; i < 1024; i += 4) {
			int32x4_t mv = vmaxq_s32(vminq_s32(vcvtq_s32_f32(vld1q_f32(&mono[i])),
							   maxVal),
						 minVal);
			vst1_s16(&out[i], vmovn_s32(mv));
		}
	}

	work[5] = work[10];
	work[7] = work[11] << 11;
	return 0;
}
