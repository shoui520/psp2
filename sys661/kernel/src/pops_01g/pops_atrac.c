#define POPS_NO_GP_REG
#include "pops_internal.h"

/* ATRAC3 LP2 (132 kbps) software decoder for PBP compressed CD-DA tracks.
 * Compiled without the global $gp ($28) scratchpad binding because Sony's
 * decoder uses $gp as a general-purpose channel counter. */

/* Band frequency scale factor boundaries (33 entries, in groups of 8 samples) */
static const u8 g_atracBandEdges[33] = {
     0,   1,   2,   3,   4,   5,   6,   7,   8,  10,  12,  14,  16,  18,  20,  22,
    24,  28,  32,  36,  40,  44,  48,  52,  56,  60,  64,  72,  80,  88,  96, 112,
   128
};

/* Base offset in g_atracQuantTable for each Huffman selector (1..6) */
static const u8 g_atracQuantOffsets[7] = {
    62, 10, 15, 22, 31, 46, 77
};

/* QMF synthesis 24-tap filterbank coefficients */
static const float g_qmfCoeffsA[23] = {
       3.8413913f,    -16.570951f,     35.608105f,    -53.583360f,
      51.723530f,       4.184255f,   -168.456820f,    533.664000f,
   -1286.640600f,    2982.158200f,  -9034.713000f, -31755.890000f,
    6798.269000f,   -3716.105700f,   2331.900600f,  -1486.831300f,
     919.457950f,    -536.305900f,    288.348450f,   -139.134500f,
      58.344273f,     -20.601358f,      6.296898f
};

static const float g_qmfCoeffsB[23] = {
     -20.601358f,      58.344273f,   -139.134500f,    288.348450f,
    -536.305900f,     919.457950f,  -1486.831300f,   2331.900600f,
   -3716.105700f,    6798.269000f, -31755.890000f,  -9034.713000f,
    2982.158200f,   -1286.640600f,    533.664000f,   -168.456820f,
       4.184255f,      51.723530f,    -53.583360f,     35.608105f,
     -16.570951f,       3.8413913f,      6.296898f
};

/* Per-channel ATRAC3 decoder workspace (6,768 bytes per channel x 2 channels) */
typedef struct PopsAtracChannel {
    u32 flags;
    s32 activeSubbands;
    float qmfDelayAndOut[1024 + 138]; /* +8..+4655: QMF history + PCM out */
    float imdctOverlap[4][128];       /* +4656..+6703: 4 subbands x 128 overlap */
    u8 prevGainPts[4][16];            /* +6704..+6767: previous frame gain control */
} PopsAtracChannel;

POPS_STATIC_ASSERT(sizeof(PopsAtracChannel) == 6768, atrac_chan_size);

typedef struct PopsAtracContext {
    u32 headerCopyA[6];         /* +0..+23 */
    u32 headerCopyB[6];         /* +24..+47 */
    s32 channelCount;           /* +48 (0x106400) */
    s32 frameBytesPerChan;      /* +52 (0x106404) */
    u32 reservedWord;           /* +56 (0x106408) */
    const u8 *bitstreamPtr;     /* +60 (0x10640C) */
    s32 bitPos;                 /* +64 (0x106410) */
    s32 decodeError;            /* +68 (0x106414) */
    u32 padWords[2];            /* +72..+79 */
    PopsAtracChannel chans[2];  /* +80..+13615 (0x106420) */
    s32 jointStereoFlag;        /* +13616 */
} PopsAtracContext;

static PopsAtracContext g_atracCtx;

static __inline__ u32 atracLoadBe32(const u8 *base, s32 bitOffset)
{
    const u8 *ptr = base + (bitOffset >> 3);
    return ((u32)ptr[0] << 24) | ((u32)ptr[1] << 16) |
           ((u32)ptr[2] << 8) | (u32)ptr[3];
}

static __inline__ u32 atracReadBits(const u8 *base, s32 bitOffset, u32 width)
{
    u32 word = atracLoadBe32(base, bitOffset);
    return (word << (bitOffset & 7)) >> (32U - width);
}

/* 0x01a870: Decode Huffman-coded spectral coefficients into an MDCT subband slice */
s32 popsAtracDecodeSpectrum(void *workCtx, const u8 *specTable, s32 selector,
                            s32 scaleIdx, float *dstStart, float *dstEnd)
{
    const s8 *huffTable = &g_atracHuffDecodeTable[8U * (u32)specTable[selector + 6]];
    float scaleFactor = g_atracQuantTable[140 + scaleIdx];
    u32 shiftBits = specTable[selector + 13];
    s32 bitPos = g_atracCtx.bitPos;
    const u8 *stream = g_atracCtx.bitstreamPtr;
    float *cur = dstStart;

    (void)workCtx;

    if (selector != 0) {
        const float *quantTbl = &g_atracQuantTable[g_atracQuantOffsets[selector]];
        const u8 *lenTbl = &g_atracBitLenTable[specTable[selector - 1]];

        while (cur != dstEnd) {
            s32 sym = huffTable[atracLoadBe32(stream, bitPos) << (bitPos & 7) >> shiftBits];
            if (sym < 0) {
                g_atracCtx.decodeError = -7;
                return sym;
            }
            bitPos += lenTbl[sym];
            *cur += quantTbl[sym] * scaleFactor;
            cur++;
        }
        g_atracCtx.bitPos = bitPos;
        return 0;
    }

    while (cur != dstEnd) {
        u32 word = atracLoadBe32(stream, bitPos);
        s32 baseBit = bitPos - (bitPos & 7);
        s32 localShift = bitPos & 7;
        s32 pair;

        for (pair = 0; pair < 4; pair++) {
            s32 sym = huffTable[(word << localShift) >> shiftBits];
            if (sym < 0) {
                g_atracCtx.decodeError = -7;
                return sym;
            }
            localShift += (sym & 7);
            {
                const float *qPair = &g_atracQuantTable[sym >> 3];
                cur[2 * pair] += qPair[0] * scaleFactor;
                cur[2 * pair + 1] += qPair[1] * scaleFactor;
            }
        }
        bitPos = baseBit + localShift;
        cur += 8;
    }

    g_atracCtx.bitPos = bitPos;
    return 0;
}

/* Helper: 256-point IMDCT + overlap-add + gain compensation for one QMF subband */
static void popsAtracImdctSubband(PopsAtracChannel *ch, s32 bandIdx, float *spec,
                                  const u8 *curGainPts)
{
    s32 step;
    s32 pairIdx;
    float *outBase;
    const u8 *prevGain;
    u32 gainLevel;
    float *gainPtr;
    float *gainLimit;

    /* Pre-folding / spectral reversal for odd subbands */
    if ((bandIdx & 1) != 0) {
        s32 idx;
        for (idx = 0; idx < 128; idx++) {
            float lo = spec[idx];
            float hi = spec[255 - idx];
            spec[255 - idx] = lo;
            spec[idx] = hi - lo;
        }
    } else {
        s32 idx;
        for (idx = 0; idx < 128; idx++) {
            spec[idx] = spec[idx] - spec[255 - idx];
        }
    }

    /* Stage 0 butterfly (sqrt(2) scaling on upper half) */
    for (pairIdx = 0; pairIdx < 128; pairIdx++) {
        float a = spec[pairIdx];
        float b = spec[128 + pairIdx] * 1.4142135f;
        spec[pairIdx] = a + b;
        spec[128 + pairIdx] = a - b;
    }

    /* Radix-2 decimation-in-frequency stages (size 64 down to 4) */
    for (step = 64; step > 2; step >>= 1) {
        s32 twiddleIdx = 127 - (step >> 1);
        float *block = spec;

        while (twiddleIdx >= 0) {
            s32 half = step;
            float tw = g_atracButterflyTable[twiddleIdx];
            s32 k;

            for (k = 0; k < half; k++) {
                block[k] -= block[2 * half - 1 - k];
            }
            for (k = 0; k < half; k++) {
                float a = block[k];
                float b = block[half + k] * tw;
                block[k] = a + b;
                block[half + k] = a - b;
            }
            twiddleIdx -= step;
            block += 2 * step;
        }
    }

    /* Final radix-4 / length-2 butterflies */
    {
        const float *twPtr = &g_atracButterflyTable[126];
        float *blk = spec;
        s32 groups;

        for (groups = 0; groups < 16; groups++) {
            s32 sub;
            for (sub = 0; sub < 4; sub++) {
                float w = twPtr[-2 * sub];
                float evenA = blk[4 * sub];
                float oddA = blk[4 * sub + 1];
                float evenB = blk[4 * sub + 2];
                float oddB = blk[4 * sub + 3];
                float diffA = evenA - oddB;
                float diffB = oddA - evenB;
                float scaledA = evenB * w;
                float scaledB = oddB * w;
                blk[4 * sub] = diffA + scaledA;
                blk[4 * sub + 1] = diffB + scaledB;
                blk[4 * sub + 2] = diffA - scaledA;
                blk[4 * sub + 3] = diffB - scaledB;
            }
            twPtr -= 8;
            blk += 16;
        }
    }

    /* Bit-reversed complex twiddle rotation and overlap-add into QMF input */
    {
        float gainInit = popsBitsToFloat((u32)(131 - (s32)curGainPts[1]) << 23);
        float *overlap = ch->imdctOverlap[bandIdx];
        outBase = &ch->qmfDelayAndOut[138 + bandIdx];

        for (pairIdx = 0; pairIdx < 64; pairIdx++) {
            u32 idxA = g_atracBitRevPairs[2 * pairIdx];
            u32 idxB = g_atracBitRevPairs[2 * pairIdx + 1];
            const float *twA = &g_atracImdctSinCos[idxA];
            const float *twB = &g_atracImdctSinCos[idxB];
            float reA = spec[4 * pairIdx] - spec[4 * pairIdx + 1];
            float imA = spec[4 * pairIdx + 1] * twA[3];
            float reB = spec[4 * pairIdx + 2] - spec[4 * pairIdx + 3];
            float imB = spec[4 * pairIdx + 3] * twB[3];
            float prevA = overlap[idxA >> 2];
            float prevB = overlap[idxB >> 2];
            float prodA = (reA - imA) * (twA[2] * gainInit);
            float prodB = (reB - imB) * (twB[2] * gainInit);

            overlap[idxA >> 2] = (reA + imA) * twA[1];
            overlap[idxB >> 2] = (reB + imB) * twB[1];

            outBase[508 - idxA] = prevA - twA[0] * prodA;
            outBase[508 - idxB] = prevB - twB[0] * prodB;
            outBase[512 + idxA] = prevA * twA[0] + prodA;
            outBase[512 + idxB] = prevB * twB[0] + prodB;
        }
    }

    /* Piecewise gain-control interpolation across the 256 subband samples */
    prevGain = ch->prevGainPts[bandIdx];
    gainLevel = prevGain[1];
    gainPtr = outBase;
    gainLimit = outBase + 1024;

    for (;;) {
        float *segEnd = outBase + 4U * (u32)prevGain[0] + 4U;
        if (gainLevel != 4U) {
            float scale = popsBitsToFloat((131U - gainLevel) << 23);
            while (gainPtr < segEnd) {
                *gainPtr *= scale;
                gainPtr += 4;
            }
        } else {
            gainPtr = segEnd;
        }

        if (gainPtr >= gainLimit) {
            break;
        }

        {
            u32 nextLevel;
            s32 slope;
            u32 accum;
            s32 rampStep;

            prevGain += 2;
            nextLevel = prevGain[1];
            slope = (s32)nextLevel - (s32)gainLevel;
            accum = 8U * gainLevel + (u32)slope;
            for (rampStep = 0; rampStep < 7; rampStep++) {
                u32 baseBits = popsFloatBits(g_atracGainScaleTable[accum & 7U]);
                float rampScale = popsBitsToFloat(baseBits - (((accum >> 3) & 0xFU) << 23));
                *gainPtr *= rampScale;
                gainPtr += 4;
                accum += (u32)slope;
            }
            gainLevel = nextLevel;
            if (gainPtr >= gainLimit) {
                break;
            }
        }
    }
}

/* Helper: 4-band QMF synthesis filterbank producing 1,024 rounded PCM samples */
static void popsAtracQmfSynthesize(PopsAtracChannel *ch)
{
    float *cur = ch->qmfDelayAndOut;
    float *end = ch->qmfDelayAndOut + 1024;

    while (cur != end) {
        float subA = cur[138];
        float subB = cur[139];
        float subC = cur[140];
        float subD = cur[141];
        float hiA = subA - subB;
        float loA = subA + subB;
        float loB = subD + subC;
        float hiB = subD - subC;
        float sumA = hiA + cur[46] * g_qmfCoeffsA[22];
        float sumB = hiB + cur[48] * g_qmfCoeffsA[22];
        float sumC = loA * g_qmfCoeffsB[22] + cur[47];
        float sumD = loB * g_qmfCoeffsB[22] + cur[49];
        s32 tap;

        cur[138] = hiA;
        cur[139] = loA;
        cur[140] = hiB;
        cur[141] = loB;

        for (tap = 0; tap < 22; tap++) {
            s32 idx = 134 - 4 * tap;
            sumA += cur[idx] * g_qmfCoeffsA[tap];
            sumB += cur[idx + 2] * g_qmfCoeffsA[tap];
            sumC += cur[idx + 1] * g_qmfCoeffsB[tap];
            sumD += cur[idx + 3] * g_qmfCoeffsB[tap];
        }

        {
            float mixA = sumA - sumB;
            float mixB = sumA + sumB;
            float mixC = sumC - sumD;
            float mixD = sumC + sumD;
            float pcmA = cur[0] * g_qmfCoeffsA[22] + mixA;
            float pcmB = cur[1] + mixB * g_qmfCoeffsB[22];
            float pcmC = cur[2] * g_qmfCoeffsA[22] + mixC;
            float pcmD = cur[3] + mixD * g_qmfCoeffsB[22];
            s32 *outWords = (s32 *)(void *)cur;

            cur[46] = mixA;
            cur[47] = mixB;
            cur[48] = mixC;
            cur[49] = mixD;

            for (tap = 0; tap < 22; tap++) {
                s32 idx = 44 - 2 * tap;
                pcmA += cur[idx] * g_qmfCoeffsA[tap];
                pcmB += cur[idx + 1] * g_qmfCoeffsB[tap];
                pcmC += cur[idx + 2] * g_qmfCoeffsA[tap];
                pcmD += cur[idx + 3] * g_qmfCoeffsB[tap];
            }

            outWords[0] = popsRoundToInt(pcmA);
            outWords[1] = popsRoundToInt(pcmB);
            outWords[2] = popsRoundToInt(pcmC);
            outWords[3] = popsRoundToInt(pcmD);
        }
        cur += 4;
    }
}

/* 0x018fa8: Decode one stereo/mono ATRAC3 LP2 frame (384 bytes -> 1,024 samples/ch) */
s32 popsAtracDecodeFrame(const u8 *inBuf, float **outChans, void *workCtx)
{
    s32 byteCursor = 0;
    s32 chanIdx;

    g_atracCtx.bitstreamPtr = inBuf;
    g_atracCtx.decodeError = 0;

    for (chanIdx = 0; chanIdx < g_atracCtx.channelCount; chanIdx++) {
        PopsAtracChannel *ch = &g_atracCtx.chans[chanIdx];
        u8 curGainPts[4][16];
        float spectrum[1024];
        u8 headerByte;
        s32 bitPos;
        s32 bandCount = 0;
        s32 maxSamples = 0;
        s32 band;
        s32 numTones;
        s32 numCodingUnits;
        const u8 *specTbl;
        s8 cuSelectors[32];
        s32 cu;
        s32 subbandLimit;

        outChans[chanIdx] = ch->qmfDelayAndOut;
        headerByte = inBuf[byteCursor];
        bitPos = 8 * byteCursor + 8;

        if ((headerByte & 0xFCU) != 0xA0U) {
            g_atracCtx.decodeError = -8;
        } else {
            bandCount = (s32)(headerByte & 3U);
            curGainPts[1][0] = 0xFFU; curGainPts[1][1] = 0x04U;
            curGainPts[2][0] = 0xFFU; curGainPts[2][1] = 0x04U;
            curGainPts[3][0] = 0xFFU; curGainPts[3][1] = 0x04U;

            for (band = 0; band <= bandCount; band++) {
                u32 numPts = atracReadBits(inBuf, bitPos, 3U);
                u8 *ptDst = curGainPts[band];
                bitPos += 3;

                if (numPts != 0U) {
                    s32 prevLoc = -1;
                    u32 pt;
                    maxSamples = (band << 8) + 256;
                    for (pt = 0U; pt < numPts; pt++) {
                        u32 code = atracReadBits(inBuf, bitPos, 9U);
                        s32 loc = (s32)((code & 0x1FU) << 3);
                        bitPos += 9;
                        ptDst[1] = (u8)(code >> 5);
                        if (prevLoc >= loc) {
                            g_atracCtx.decodeError = -5;
                            break;
                        }
                        prevLoc = loc;
                        ptDst[0] = (u8)loc;
                        ptDst += 2;
                    }
                    if (g_atracCtx.decodeError != 0) {
                        break;
                    }
                }
                ptDst[0] = 0xFFU;
                ptDst[1] = 0x04U;
            }
        }

        if (g_atracCtx.decodeError == 0) {
            numTones = (s32)atracReadBits(inBuf, bitPos, 5U);
            bitPos += 5;
            g_atracCtx.bitPos = bitPos;
            sceKernelMemset(spectrum, 0, sizeof(spectrum));

            /* Decode tonal components if present */
            if (numTones != 0) {
                u32 toneMode = atracReadBits(inBuf, bitPos, 2U);
                s32 totalComps = 0;

                if (toneMode == 2U) {
                    maxSamples = 0;
                    g_atracCtx.decodeError = -1;
                } else {
                    s32 toneIdx;
                    s32 maskShift = -64 >> bandCount;
                    s32 lenShift = 3 - bandCount;
                    bitPos += 2;

                    for (toneIdx = 0; toneIdx < numTones && g_atracCtx.decodeError == 0; toneIdx++) {
                        u32 toneHdr = atracReadBits(inBuf, bitPos, 7U);
                        u32 huffSel = atracReadBits(inBuf, bitPos + bandCount + 4, 3U);
                        s32 bandMask = maskShift & (s32)toneHdr;
                        s32 compLen = (((s32)toneHdr - bandMask) >> lenShift) + 1;
                        u32 useAltTbl = toneMode;
                        s32 bandOff = 0;

                        bitPos += bandCount + 7;
                        if (huffSel <= 1U) {
                            maxSamples = 0;
                            g_atracCtx.decodeError = -2;
                            break;
                        }
                        if ((toneMode & 2U) != 0U) {
                            useAltTbl = atracReadBits(inBuf, bitPos, 1U);
                            bitPos++;
                        }
                        specTbl = useAltTbl ? g_atracSpecTableB : g_atracSpecTableA;

                        while (bandOff < 1024 && g_atracCtx.decodeError == 0) {
                            if ((bandOff & 0xFF) == 0) {
                                s32 active = (bandMask & 0x40) != 0;
                                bandMask <<= 1;
                                if (!active) {
                                    bandOff += 256;
                                    if ((u8)bandMask == 0U) {
                                        break;
                                    }
                                    continue;
                                }
                            }
                            {
                                u32 compCount = atracReadBits(inBuf, bitPos, 3U);
                                u32 compIdx;
                                bitPos += 3;
                                for (compIdx = 0U; compIdx < compCount; compIdx++) {
                                    u32 rawComp = atracReadBits(inBuf, bitPos, 12U);
                                    s32 pos = (s32)(rawComp & 0x3FU);
                                    s32 scale = (s32)(rawComp >> 6);
                                    s32 endPos = __builtin_allegrex_min(bandOff + pos + compLen, 1024);

                                    bitPos += 12;
                                    totalComps++;
                                    maxSamples = __builtin_allegrex_max(maxSamples, endPos);
                                    if (totalComps == 65) {
                                        maxSamples = 0;
                                        g_atracCtx.decodeError = -3;
                                        break;
                                    }
                                    g_atracCtx.bitPos = bitPos;
                                    popsAtracDecodeSpectrum(workCtx, specTbl, (s32)huffSel - 1, scale,
                                                            &spectrum[bandOff + pos],
                                                            &spectrum[endPos]);
                                    bitPos = g_atracCtx.bitPos;
                                }
                            }
                            bandOff += 64;
                        }
                    }
                    g_atracCtx.bitPos = bitPos;
                }
            }

            /* Decode main MDCT coding units */
            bitPos = g_atracCtx.bitPos;
            {
                u32 cuHdr = atracReadBits(inBuf, bitPos, 6U);
                s32 scaleBits = 0;
                specTbl = ((cuHdr & 1U) != 0U) ? g_atracSpecTableB : g_atracSpecTableA;
                numCodingUnits = (s32)(cuHdr >> 1) + 1;
                bitPos += 6;

                for (cu = 0; cu < numCodingUnits; cu++) {
                    u32 sel = atracReadBits(inBuf, bitPos, 3U);
                    cuSelectors[cu] = (s8)((s32)sel - 1);
                    if (sel != 0U) {
                        scaleBits += 6;
                    }
                    bitPos += 3;
                }
                g_atracCtx.bitPos = bitPos + scaleBits;

                for (cu = 0; cu < numCodingUnits; cu++) {
                    if (cuSelectors[cu] >= 0) {
                        u32 scale = atracReadBits(inBuf, bitPos, 6U);
                        bitPos += 6;
                        popsAtracDecodeSpectrum(workCtx, specTbl, cuSelectors[cu], (s32)scale,
                                                &spectrum[8U * g_atracBandEdges[cu]],
                                                &spectrum[8U * g_atracBandEdges[cu + 1]]);
                    }
                }
            }

            {
                s32 prevBands = ch->activeSubbands;
                s32 curBands = (__builtin_allegrex_max(maxSamples,
                                                       8 * (s32)g_atracBandEdges[numCodingUnits]) + 255) >> 8;
                ch->activeSubbands = curBands;
                subbandLimit = __builtin_allegrex_max(curBands, prevBands);
            }

            /* Shift QMF delay line history (138 floats = 552 bytes) */
            sceKernelMemcpy(ch->qmfDelayAndOut, &ch->qmfDelayAndOut[1024], 552U);

            for (band = 0; band < subbandLimit; band++) {
                popsAtracImdctSubband(ch, band, &spectrum[256 * band], curGainPts[band]);
            }
            for (; band < 4; band++) {
                s32 sample;
                sceKernelMemset(ch->imdctOverlap[band], 0, 512U);
                for (sample = band; sample < 1024; sample += 4) {
                    ch->qmfDelayAndOut[138 + sample] = 0.0f;
                }
            }

            sceKernelMemcpy(ch->prevGainPts, curGainPts, sizeof(curGainPts));
        }

        byteCursor += g_atracCtx.frameBytesPerChan;
        if (8 * byteCursor + 16 < g_atracCtx.bitPos) {
            g_atracCtx.decodeError = -9;
        }
        if (g_atracCtx.decodeError != 0) {
            return -g_atracCtx.decodeError;
        }

        popsAtracQmfSynthesize(ch);
    }

    return byteCursor;
}

/* 0x01aad8: Initialize ATRAC3 LP2 decoder context */
void *popsAtracInitDecoder(s32 channels, s32 jointStereo, s32 frameBytes, s32 modeParam)
{
    s32 maxCh;
    s32 band;
    s32 ch;

    if (modeParam != 0 || channels >= 3 || frameBytes >= 257) {
        return NULL;
    }

    maxCh = __builtin_allegrex_max(jointStereo, channels);
    scePaf_D9E2D6E1(&g_atracCtx, 0, (u32)(6768 * maxCh + 80));

    g_atracCtx.headerCopyA[5] = 4U;
    g_atracCtx.frameBytesPerChan = (jointStereo != 0) ? (frameBytes * channels) : frameBytes;
    g_atracCtx.channelCount = channels;

    for (band = 0; band < 4; band++) {
        for (ch = 0; ch < channels; ch++) {
            g_atracCtx.chans[ch].prevGainPts[band][0] = 0xFFU;
            g_atracCtx.chans[ch].prevGainPts[band][1] = 0x04U;
        }
    }

    g_atracCtx.jointStereoFlag = jointStereo;
    scePaf_B4652CFE(g_atracCtx.headerCopyB, g_atracCtx.headerCopyA, 24U);
    return &g_atracCtx;
}
