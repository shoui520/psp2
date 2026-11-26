/*
 * ePSP 6.61 libaudiocodec2.prx (sceAudiocodec_Library2)
 * Software ATRAC3 audio codec library
 */

#include <common/types.h>
#include "atrac3_tables.h"

extern void *sceKernelMemset(void *dst, int c, u32 size);

static inline s32 maxSigned(s32 left, s32 right) { return left > right ? left : right; }
static inline s32 minSigned(s32 left, s32 right) { return left < right ? left : right; }

static inline s32 roundFloatToInt(float sample)
{
    float rounded;
    s32 result;
    __asm__ volatile (
        "round.w.s %0, %2\n\t"
        "mfc1 %1, %0"
        : "=&f"(rounded), "=r"(result)
        : "f"(sample)
    );
    return result;
}

typedef struct {
    s32 isJointStereo;          /* 0x0000 */
    s32 activeQmf;              /* 0x0004 */
    float pcmWork[1162];        /* 0x0008..0x122F */
    float overlap[4][128];      /* 0x1230..0x1A2F */
    s32 bandCfg[4][16];         /* 0x1A30..0x1B2F */
} Atrac3ChannelCtx;             /* size 0x1B30 */

typedef struct {
    const float *jsWeights[4];      /* 0x00 */
    const float *jsScale;           /* 0x10 */
    s32 jsNumQmf;                   /* 0x14 */
    const float *prevJsWeights[4];  /* 0x18 */
    const float *prevJsScale;       /* 0x28 */
    s32 prevJsNumQmf;               /* 0x2C */
    s32 channels;                   /* 0x30 */
    s32 frameBytes;                 /* 0x34 */
    s32 varFrameFlag;               /* 0x38 */
    u8 *bitstream;                  /* 0x3C */
    s32 bitpos;                     /* 0x40 */
    s32 errCode;                    /* 0x44 */
    Atrac3ChannelCtx ch[2];         /* 0x48..0x36A7 */
} Atrac3Ctx;

static void *atrac3Memset(void *dst, int c, u32 size);
static s32 atrac3ParseMode(u32 *work);
static u32 atrac3ModeToJointStereo(u32 mode);
static s32 atrac3InitCtx(s32 channels, s32 jointStereo, s32 frameBytes, Atrac3Ctx *ctx);
static void atrac3DecodeSpectrum(Atrac3Ctx *ctx, const Atrac3HuffInfo *huffInfo, s32 tblIdx, float *out, float *end, float scale);
static s32 atrac3DecodeFrame(u8 *inBuf, s32 **outChPtrs, Atrac3Ctx *ctx);
static s32 atrac3CodecInit(u32 *work);
static s32 atrac3CodecDecode(u32 *work);

static __attribute__((noinline)) s32 atrac3InitCtx(s32 channels, s32 jointStereo, s32 frameBytes, Atrac3Ctx *ctx)
{
    if (frameBytes > 0x100 || ctx == NULL)
        return 0;

    s32 extraCh = maxSigned(channels - 2, 0);
    atrac3Memset(ctx, 0, (u32)(extraCh * sizeof(Atrac3ChannelCtx) + sizeof(Atrac3Ctx)));

    ctx->channels = channels;
    if (jointStereo != 0)
        frameBytes *= channels;
    ctx->frameBytes = frameBytes;
    ctx->jsNumQmf = 4;

    const float *defaultJsWeight = &kJointStereoWeights[3];
    for (s32 band = 0; band < 4; band++) {
        ctx->jsWeights[band] = defaultJsWeight;
        for (s32 ch = 0; ch < channels; ch++) {
            ctx->ch[ch].bandCfg[band][0] = 0xff;
            ctx->ch[ch].bandCfg[band][1] = 4;
        }
    }
    ctx->ch[1].isJointStereo = jointStereo;
    for (s32 band = 0; band < 4; band++)
        ctx->prevJsWeights[band] = ctx->jsWeights[band];
    ctx->prevJsScale = ctx->jsScale;
    ctx->prevJsNumQmf = ctx->jsNumQmf;
    return (s32)(u32)ctx;
}

static __attribute__((noinline, used)) s32 atrac3GetVersion(void)
{
    return 0x10600;
}

static __attribute__((noinline)) void atrac3DecodeSpectrum(
    Atrac3Ctx *ctx,
    const Atrac3HuffInfo *huffInfo,
    s32 tblIdx,
    float *out,
    float *end,
    float scale)
{
    const s8 *valTbl = huffInfo->valTbl[tblIdx];
    s32 maxBits = huffInfo->maxBits[tblIdx];
    const s8 *lenTbl = huffInfo->lenTbl[tblIdx];
    s32 bitpos = ctx->bitpos;
    u8 *bs = ctx->bitstream;

    if (tblIdx == 0) {
        u32 mask = 0xffffffffu >> maxBits;
        do {
            s32 byteOff = bitpos >> 3;
            s32 bitRem = bitpos & 7;
            s32 shift = maxBits - bitRem;
            u32 word = ((u32)bs[byteOff] << 24) | ((u32)bs[byteOff + 1] << 16) |
                       ((u32)bs[byteOff + 2] << 8) | (u32)bs[byteOff + 3];
            bitpos &= ~7;
            for (int pairIdx = 0; pairIdx < 4; pairIdx++) {
                u32 code = ((u32)((s32)word >> shift)) & mask;
                s32 sym = valTbl[code];
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
        u32 mask = (u32)(0xffff >> maxBits);
        while (out < end) {
            s32 byteOff = bitpos >> 3;
            u32 word = ((u32)bs[byteOff] << 8) | (u32)bs[byteOff + 1];
            s32 shift = maxBits - (bitpos & 7);
            u32 code = (word >> shift) & mask;
            s32 sym = valTbl[code];
            if (sym < 0) {
                ctx->errCode = 0x189;
                return;
            }
            *out += quantTbl[sym] * scale;
            bitpos += lenTbl[sym];
            out++;
        }
        ctx->bitpos = bitpos;
    }
}

static __attribute__((noinline)) s32 atrac3DecodeFrame(u8 *inBuf, s32 **outChPtrs, Atrac3Ctx *ctx)
{
    s32 frameBytes = ctx->frameBytes;
    s32 channels = ctx->channels;
    ctx->bitstream = inBuf;
    ctx->errCode = 0;
    if (channels <= 0)
        return 0;

    for (s32 ch = 0; ch < channels; ch++)
        outChPtrs[ch] = (s32 *)ctx->ch[ch].pcmWork;

    s32 localBandCfg[64];
    float localSpec[1024];

    s32 totalConsumed = 0;
    s32 availBytes = frameBytes;
    for (s32 ch = 0; ch < channels; ch++) {
        Atrac3ChannelCtx *chCtx = &ctx->ch[ch];
        s32 isJointCh = chCtx->isJointStereo;
        u8 *bs = ctx->bitstream;
        s32 bitpos = 0;
        s32 headerOk = 1;

        if (isJointCh != 0) {
            u8 *endPtr = bs + frameBytes - 1;
            while (endPtr > bs && *endPtr == 0xf8)
                endPtr--;
            s32 prevBytes = (ctx->bitpos + 7) >> 3;
            availBytes = (s32)(endPtr - bs) - prevBytes + 1;
            for (u8 *leftPtr = bs, *rightPtr = endPtr; leftPtr < rightPtr; leftPtr++, rightPtr--) {
                u8 swapVal = *leftPtr;
                *leftPtr = *rightPtr;
                *rightPtr = swapVal;
            }
            bitpos = 16;
            ctx->bitpos = 16;
            if ((bs[1] & 0x0c) != 0x0c) {
                ctx->errCode = 0x183;
                headerOk = 0;
            }
        } else {
            u8 hdrByte = bs[totalConsumed];
            bitpos = (totalConsumed << 3) + 8;
            ctx->bitpos = bitpos;
            if ((hdrByte & 0xfc) != 0xa0) {
                ctx->errCode = 0x182;
                headerOk = 0;
            }
        }

        if (headerOk) {
            s32 numQmf = (isJointCh != 0) ? (bs[1] & 3) : (bs[totalConsumed] & 3);
            s32 maxSpecPos = 0;
            s32 gainOk = 1;

            for (s32 band = 0; band <= numQmf && gainOk; band++) {
                u32 regWord = ((u32)bs[bitpos >> 3] << 8) | bs[(bitpos >> 3) + 1];
                s32 numRegions = (regWord >> (13 - (bitpos & 7))) & 7;
                bitpos += 3;
                ctx->bitpos = bitpos;
                s32 regIdx = 0;
                if (numRegions != 0)
                    maxSpecPos = (band + 1) * 256;
                for (; regIdx < numRegions; regIdx++) {
                    u32 levelWord = ((u32)bs[bitpos >> 3] << 8) | bs[(bitpos >> 3) + 1];
                    s32 gainLevel = (levelWord >> (12 - (bitpos & 7))) & 0x0f;
                    s32 locBitpos = bitpos + 4;
                    u32 locWord = ((u32)bs[locBitpos >> 3] << 8) | bs[(locBitpos >> 3) + 1];
                    s32 locVal = (locWord >> (8 - (locBitpos & 7))) & 0xf8;
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
                    localBandCfg[band * 16 + regIdx * 2 + 0] = 0xff;
                    localBandCfg[band * 16 + regIdx * 2 + 1] = 4;
                }
            }

            if (gainOk) {
                for (s32 band = numQmf + 1; band < 4; band++) {
                    localBandCfg[band * 16 + 0] = 0xff;
                    localBandCfg[band * 16 + 1] = 4;
                }

                for (int idx = 0; idx < 1024; idx++)
                    localSpec[idx] = kZeroSpectrum[idx];

                u32 tonalWord = ((u32)bs[bitpos >> 3] << 8) | bs[(bitpos >> 3) + 1];
                s32 numTonalGroups = (tonalWord >> (11 - (bitpos & 7))) & 0x1f;
                bitpos += 5;
                ctx->bitpos = bitpos;

                if (numTonalGroups != 0) {
                    u32 modeWord = ((u32)bs[bitpos >> 3] << 8) | bs[(bitpos >> 3) + 1];
                    s32 tonalMode = (modeWord >> (14 - (bitpos & 7))) & 3;
                    bitpos += 2;
                    ctx->bitpos = bitpos;
                    if (tonalMode == 2) {
                        maxSpecPos = 0;
                        ctx->errCode = 0x184;
                    } else {
                        s32 bandMask = (-0x40) >> numQmf;
                        s32 shiftStep = 3 - numQmf;
                        s32 totalTonalComps = 0;
                        s32 tonalAborted = 0;

                        for (s32 grp = 0; grp < numTonalGroups && !tonalAborted; grp++) {
                            s32 lenBitpos = bitpos + numQmf + 4;
                            u32 maskWord = ((u32)bs[bitpos >> 3] << 8) | bs[(bitpos >> 3) + 1];
                            u32 lenWord = ((u32)bs[lenBitpos >> 3] << 8) | bs[(lenBitpos >> 3) + 1];
                            s32 rawMask = (maskWord >> (9 - (bitpos & 7))) & 0x7f;
                            s32 rawLen = (lenWord >> (13 - (lenBitpos & 7))) & 7;
                            s32 bandBits = rawMask & bandMask;
                            s32 valsPerComp = ((rawMask - bandBits) >> shiftStep) + 1;
                            s32 huffNum = rawLen - 1;
                            bitpos += numQmf + 7;
                            ctx->bitpos = bitpos;
                            if (huffNum <= 0) {
                                maxSpecPos = 0;
                                ctx->errCode = 0x181;
                                break;
                            }
                            s32 huffSel = tonalMode;
                            if (tonalMode >= 2) {
                                huffSel = (bs[bitpos >> 3] >> ((~bitpos) & 7)) & 1;
                                bitpos += 1;
                                ctx->bitpos = bitpos;
                            }
                            const Atrac3HuffInfo *huffInfo = &kHuffTableSet[huffSel];
                            s32 subbandPos = 0;
                            while (!tonalAborted) {
                                s32 groupDone = 0;
                                if ((subbandPos & 0xff) == 0) {
                                    do {
                                        bandBits <<= 1;
                                        if (bandBits & 0x80)
                                            break;
                                        subbandPos += 0x100;
                                        if ((bandBits & 0xff) == 0) {
                                            groupDone = 1;
                                            break;
                                        }
                                    } while ((subbandPos & 0xff) == 0);
                                }
                                if (groupDone)
                                    break;
                                u32 compWord = ((u32)bs[bitpos >> 3] << 8) | bs[(bitpos >> 3) + 1];
                                s32 numComps = (compWord >> (13 - (bitpos & 7))) & 7;
                                bitpos += 3;
                                ctx->bitpos = bitpos;
                                for (s32 compIdx = 0; compIdx < numComps; compIdx++) {
                                    s32 sfBitpos = bitpos;
                                    s32 posBitpos = bitpos + 6;
                                    bitpos += 12;
                                    ctx->bitpos = bitpos;
                                    u32 sfWord = ((u32)bs[sfBitpos >> 3] << 8) | bs[(sfBitpos >> 3) + 1];
                                    u32 posWord = ((u32)bs[posBitpos >> 3] << 8) | bs[(posBitpos >> 3) + 1];
                                    s32 relPos = (posWord >> (10 - (posBitpos & 7))) & 0x3f;
                                    s32 absPos = subbandPos + relPos;
                                    s32 endPos = minSigned(absPos + valsPerComp, 1024);
                                    s32 sfIdx = (sfWord >> (10 - (sfBitpos & 7))) & 0x3f;
                                    maxSpecPos = maxSigned(maxSpecPos, endPos);
                                    totalTonalComps++;
                                    if (totalTonalComps == 65) {
                                        maxSpecPos = 0;
                                        ctx->errCode = 0x180;
                                        tonalAborted = 1;
                                        break;
                                    }
                                    atrac3DecodeSpectrum(ctx, huffInfo, huffNum, &localSpec[absPos], &localSpec[endPos], kScaleFactorTable[sfIdx]);
                                    bs = ctx->bitstream;
                                    bitpos = ctx->bitpos;
                                }
                                subbandPos += 0x40;
                            }
                        }
                    }
                }

                s32 tblBitpos = bitpos + 5;
                u32 sbWord = ((u32)bs[bitpos >> 3] << 8) | bs[(bitpos >> 3) + 1];
                u8 tblByte = bs[tblBitpos >> 3];
                s32 numSubbands = ((sbWord >> (11 - (bitpos & 7))) & 0x1f) + 1;
                bitpos += 6;
                ctx->bitpos = bitpos;
                s32 huffSel = (tblByte >> ((~tblBitpos) & 7)) & 1;
                const Atrac3HuffInfo *huffInfo = &kHuffTableSet[huffSel];
                s32 *sbHuff = (s32 *)chCtx->pcmWork;
                s32 numCodedBits = 0;
                for (s32 sb = 0; sb < numSubbands; sb++) {
                    u32 huffWord = ((u32)bs[bitpos >> 3] << 8) | bs[(bitpos >> 3) + 1];
                    s32 huffIdx = ((huffWord >> (13 - (bitpos & 7))) & 7) - 1;
                    bitpos += 3;
                    ctx->bitpos = bitpos;
                    sbHuff[sb] = huffIdx;
                    if (huffIdx >= 0)
                        numCodedBits += 6;
                }
                s32 sfBitpos = bitpos;
                bitpos += numCodedBits;
                ctx->bitpos = bitpos;
                for (s32 sb = 0; sb < numSubbands; sb++) {
                    s32 huffIdx = sbHuff[sb];
                    if (huffIdx >= 0) {
                        u32 sfWord = ((u32)ctx->bitstream[sfBitpos >> 3] << 8) | ctx->bitstream[(sfBitpos >> 3) + 1];
                        s32 sfIdx = (sfWord >> (10 - (sfBitpos & 7))) & 0x3f;
                        sfBitpos += 6;
                        atrac3DecodeSpectrum(ctx, huffInfo, huffIdx, &localSpec[kSubbandBoundaries[sb]], &localSpec[kSubbandBoundaries[sb + 1]], kScaleFactorTable[sfIdx]);
                    }
                }

                s32 maxPos = maxSigned(kSubbandBoundaries[numSubbands], maxSpecPos);
                s32 activeQmf = (maxPos + 0xff) >> 8;
                s32 prevActive = chCtx->activeQmf;
                chCtx->activeQmf = activeQmf;
                s32 runQmf = maxSigned(prevActive, activeQmf);

                float *histSrc = &chCtx->pcmWork[1024];
                for (int idx = 0; idx < 138; idx++)
                    chCtx->pcmWork[idx] = histSrc[idx];

                for (s32 band = 0; band < runQmf; band++) {
                    float *specBand = &localSpec[band * 256];
                    if ((band & 1) == 0) {
                        for (int idx = 0; idx < 128; idx++)
                            specBand[idx] -= specBand[255 - idx];
                    } else {
                        for (int idx = 0; idx < 128; idx++) {
                            float topVal = specBand[idx];
                            float botVal = specBand[255 - idx];
                            specBand[255 - idx] = topVal;
                            specBand[idx] = botVal - topVal;
                        }
                    }

                    float midCos = kImdctCosTable[63];
                    for (int idx = 0; idx < 128; idx++) {
                        float leftVal = specBand[idx];
                        float rightScaled = specBand[idx + 128] * midCos;
                        specBand[idx + 128] = leftVal - rightScaled;
                        specBand[idx] = leftVal + rightScaled;
                    }

                    for (s32 step = 64; step != 2; step >>= 1) {
                        s32 half = step >> 1;
                        s32 cosIdx = 127 - half;
                        for (float *blk = specBand; cosIdx >= 0; blk += step * 2, cosIdx -= step) {
                            for (s32 idx = 0; idx < half; idx++)
                                blk[idx] -= blk[step * 2 - 1 - idx];
                            float cosVal = kImdctCosTable[cosIdx];
                            for (s32 idx = 0; idx < step; idx++) {
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
                    for (int pair = 0; pair < 64; pair++) {
                        for (int sub = 0; sub < 2; sub++) {
                            u32 revIdx = kImdctBitRevIdx[pair * 2 + sub];
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

                    s32 *cfgPtr = chCtx->bandCfg[band];
                    float *curPtr = pcmBand;
                    float *endPtr = pcmBand + 1024;
                    while (curPtr < endPtr) {
                        s32 nextLoc = cfgPtr[0];
                        s32 curGain = cfgPtr[1];
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
                        s32 nextGain = cfgPtr[1];
                        s32 stepIdx = (curGain << 5) + ((nextGain - curGain) << 2);
                        for (int interp = 0; interp < 7; interp++) {
                            float fineGain = kGainStepTable[((stepIdx & 0x1c) + 0x40) >> 2];
                            float coarseGain = kGainStepTable[(stepIdx & 0x1e0) >> 5];
                            *curPtr = (*curPtr * fineGain) * coarseGain;
                            curPtr += 4;
                            stepIdx += (nextGain - curGain) << 2;
                        }
                    }
                }

                for (s32 band = runQmf; band < 4; band++) {
                    atrac3Memset(chCtx->overlap[band], 0, sizeof(chCtx->overlap[band]));
                    for (int idx = 0; idx < 256; idx++)
                        chCtx->pcmWork[138 + band + idx * 4] = 0.0f;
                }

                s32 jsOk = 1;
                if (isJointCh != 0) {
                    const float eighthStep = 0.125f;
                    const float twoScale = 2.0f;
                    s32 prevQmf = ctx->prevJsNumQmf;
                    s32 curQmf = ctx->jsNumQmf;

                    for (s32 band = 0; band < 4; band++) {
                        float prevScaleLeft = twoScale;
                        float prevScaleRight = twoScale;
                        if (band > prevQmf) {
                            const float *prevPair = ctx->prevJsScale;
                            prevScaleLeft = prevPair[0];
                            prevScaleRight = prevPair[1];
                        }
                        float curScaleLeft = twoScale;
                        float curScaleRight = twoScale;
                        if (band > curQmf) {
                            const float *curPair = ctx->jsScale;
                            curScaleLeft = curPair[0];
                            curScaleRight = curPair[1];
                        }
                        const float *weightCur = ctx->jsWeights[band];
                        const float *weightPrev = ctx->prevJsWeights[band];
                        float *leftPcm = &ctx->ch[0].pcmWork[138 + band];
                        float *rightPcm = &ctx->ch[1].pcmWork[138 + band];
                        s32 sampleIdx = 0;

                        if (curScaleLeft != prevScaleLeft || weightCur != weightPrev) {
                            float deltaScaleLeft = (curScaleLeft - prevScaleLeft) * eighthStep;
                            float deltaScaleRight = (curScaleRight - prevScaleRight) * eighthStep;
                            float deltaWeightLeft = (weightCur[0] - weightPrev[0]) * eighthStep;
                            float deltaWeightRight = (weightCur[1] - weightPrev[1]) * eighthStep;
                            for (; sampleIdx < 8; sampleIdx++) {
                                float ramp = (float)(sampleIdx - 8);
                                float mixWeightLeft = weightCur[0] + deltaWeightLeft * ramp;
                                float mixWeightRight = weightCur[1] + deltaWeightRight * ramp;
                                float rampScaleLeft = curScaleLeft + deltaScaleLeft * ramp;
                                float rampScaleRight = curScaleRight + deltaScaleRight * ramp;
                                float leftSample = leftPcm[sampleIdx * 4];
                                float rightSample = rightPcm[sampleIdx * 4];
                                float mixed = leftSample * mixWeightLeft + rightSample * mixWeightRight;
                                leftPcm[sampleIdx * 4] = mixed * rampScaleLeft;
                                rightPcm[sampleIdx * 4] = (leftSample - mixed) * rampScaleRight;
                            }
                        }
                        for (; sampleIdx < 256; sampleIdx++) {
                            float leftSample = leftPcm[sampleIdx * 4];
                            float rightSample = rightPcm[sampleIdx * 4];
                            float mixed = leftSample * weightCur[0] + rightSample * weightCur[1];
                            leftPcm[sampleIdx * 4] = curScaleLeft * mixed;
                            rightPcm[sampleIdx * 4] = curScaleRight * (leftSample - mixed);
                        }
                    }

                    for (s32 band = 0; band < 4; band++)
                        ctx->prevJsWeights[band] = ctx->jsWeights[band];
                    ctx->prevJsScale = ctx->jsScale;
                    ctx->prevJsNumQmf = ctx->jsNumQmf;
                    ctx->jsNumQmf = numQmf;

                    u8 firstByte = ctx->bitstream[0];
                    ctx->jsScale = kJointStereoScalePairs[firstByte >> 4];
                    s32 jsBits = (((s32)firstByte << 8) | ctx->bitstream[1]) >> 4;
                    for (s32 band = 3; band >= 0; band--) {
                        s32 sel = jsBits & 3;
                        if (sel == 2) {
                            ctx->errCode = 0x18a;
                            jsOk = 0;
                            break;
                        }
                        ctx->jsWeights[band] = &kJointStereoWeights[sel];
                        jsBits >>= 2;
                    }
                }

                if (jsOk) {
                    for (int idx = 0; idx < 64; idx++)
                        ((s32 *)chCtx->bandCfg)[idx] = localBandCfg[idx];
                }
            }
        }

        bitpos = ctx->bitpos;
        if ((availBytes << 3) + 16 < bitpos)
            ctx->errCode = 0x18b;

        frameBytes = ctx->frameBytes;
        if (ctx->varFrameFlag != 0) {
            s32 usedBytes = (bitpos + 7) >> 3;
            if (ctx->ch[1].isJointStereo != 0)
                ctx->errCode = 1;
            totalConsumed = usedBytes;
            if (usedBytes < frameBytes) {
                while (totalConsumed < frameBytes && ctx->bitstream[totalConsumed] == 0)
                    totalConsumed++;
            }
        } else {
            totalConsumed += frameBytes;
            if (ctx->ch[1].isJointStereo == 0)
                availBytes += frameBytes;
        }
    }

    s32 err = ctx->errCode;
    if (err != 0 && err != 0x18b)
        return -err;

    for (s32 ch = 0; ch < channels; ch++) {
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
            for (int tap = 1; tap < 23; tap++) {
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
            for (int tap = 0; tap < 22; tap++) {
                const float *hist = &pcm[40 - tap * 2];
                float coeffRev = kQmfPrototypeCoeffs[22 - tap];
                float coeffFwd = kQmfPrototypeCoeffs[1 + tap];
                outEvenLo += hist[0] * coeffRev;
                outOddLo += hist[1] * coeffFwd;
                outEvenHi += hist[2] * coeffRev;
                outOddHi += hist[3] * coeffFwd;
            }

            ((s32 *)pcm)[-4] = roundFloatToInt(outEvenLo);
            ((s32 *)pcm)[-3] = roundFloatToInt(outOddLo);
            ((s32 *)pcm)[-2] = roundFloatToInt(outEvenHi);
            ((s32 *)pcm)[-1] = roundFloatToInt(outOddHi);
        }
    }

    if (ctx->errCode != 0)
        return -ctx->errCode;
    return totalConsumed;
}

s32 sceAudiocodec2_AA2ED267(u32 *work, s32 type)
{
    if (work == NULL || type != 0x1001)
        return (s32)0x807f00ff;
    work[2] = 0;
    work[3] = 0;
    work[4] = 0x36a8;
    return 0;
}

s32 sceAudiocodec2_E063D402(u32 *work, s32 type)
{
    if (work == NULL || work[3] == 0 || type != 0x1001)
        return (s32)0x807f00ff;
    return atrac3CodecInit(work);
}

s32 sceAudiocodec2_21D5A83F(u32 *work, s32 type)
{
    if (work == NULL || work[3] == 0 || work[6] == 0 || work[8] == 0 || type != 0x1001)
        return (s32)0x807f00ff;
    return atrac3CodecDecode(work);
}

static __attribute__((noinline)) s32 atrac3CodecInit(u32 *work)
{
    s32 res = atrac3ParseMode(work);
    if (res != 0)
        return res;
    u32 jointStereo = atrac3ModeToJointStereo(work[10]);
    if (jointStereo == 0x270f)
        return (s32)0x807f00ff;
    u32 channels = work[13];
    u32 frameBytes = work[12] / channels;
    s32 ptr = atrac3InitCtx((s32)channels, (s32)jointStereo, (s32)frameBytes, (Atrac3Ctx *)work[3]);
    return (ptr != 0) ? 0 : (s32)0x807f00fd;
}

static __attribute__((noinline)) s32 atrac3CodecDecode(u32 *work)
{
    s32 *chPtrs[4];
    s32 res = atrac3DecodeFrame((u8 *)work[6], chPtrs, (Atrac3Ctx *)work[3]);
    if (res < 0) {
        work[2] = (u32)(-res);
        return (s32)0x807f00fd;
    }
    s32 *leftPcm = chPtrs[0];
    s32 *rightPcm = chPtrs[1];
    u32 *outStereo = (u32 *)work[8];
    for (int idx = 0; idx < 1024; idx++) {
        if (leftPcm[idx] > 0x7fff)
            leftPcm[idx] = 0x7fff;
        if (leftPcm[idx] < -0x8000)
            leftPcm[idx] = -0x8000;
        if (rightPcm[idx] > 0x7fff)
            rightPcm[idx] = 0x7fff;
        if (rightPcm[idx] < -0x8000)
            rightPcm[idx] = -0x8000;
        outStereo[idx] = ((u32)rightPcm[idx] << 16) | (u16)leftPcm[idx];
    }
    work[9] = work[13] << 11;
    work[7] = work[12];
    return 0;
}

static __attribute__((noinline)) s32 atrac3ParseMode(u32 *work)
{
    u32 mode = work[10];
    if (mode != 6) {
        u32 expected = (mode < 7) ? 4u : 11u;
        if (mode != expected)
            return (s32)0x807f00ff;
    }
    s32 idx = 0;
    for (; idx < 3; idx++) {
        if (kAtrac3ModeTable[idx].mode == mode)
            break;
    }
    work[11] = 0xac44;
    work[12] = kAtrac3ModeTable[idx].frameBytes;
    work[13] = kAtrac3ModeTable[idx].channels;
    return 0;
}

static __attribute__((noinline)) u32 atrac3ModeToJointStereo(u32 mode)
{
    u32 jointStereo = 0;
    if (mode != 6) {
        u32 expected = 4;
        if (mode >= 7) {
            jointStereo = 1;
            expected = 11;
        }
        if (mode != expected)
            return 0x270f;
    }
    return jointStereo;
}

static __attribute__((noinline)) void *atrac3Memset(void *dst, int c, u32 size)
{
    sceKernelMemset(dst, c, size);
    return dst;
}
