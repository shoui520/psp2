#include <arm_neon.h>
#include <stddef.h>
#include <stdint.h>

#include "audiocodec_internal.h"
#include "atrac3plus_tables.h"

#define AT3P_ERR_FAIL		((int32_t)0x80004005)
#define AT3P_ERR_NOTIMPL	((int32_t)0x80004001)

#define AT3P_HELPER		static __attribute__((noinline, noclone))

typedef struct Atrac3pBandFlags {
	int32_t hasData;
	int32_t useBit;
	int32_t val[16];
} Atrac3pBandFlags;

typedef struct Atrac3pGainBand {
	int32_t numPoints;
	int32_t locCode[7];
	int32_t levCode[7];
	int32_t winShape;
} Atrac3pGainBand;

typedef struct Atrac3pGainInfo {
	Atrac3pGainBand band[16];
	Atrac3pBandFlags winShape;
} Atrac3pGainInfo;

typedef struct Atrac3pWaveParam {
	int32_t freqIndex;
	int32_t ampSf;
	int32_t phaseIndex;
	int32_t ampIndex;
} Atrac3pWaveParam;

typedef struct Atrac3pWaveBand {
	int32_t envStartFlag;
	int32_t envStopFlag;
	int32_t envStartPos;
	int32_t envStopPos;
	int32_t hasStartPoint;
	int32_t hasStopPoint;
	int32_t startPoint;
	int32_t stopPoint;
	int32_t numWaves;
	Atrac3pWaveParam *waves;
} Atrac3pWaveBand;

typedef struct Atrac3pWavesHeader {
	int32_t tonesPresent;
	int32_t amplitudeMode;
	int32_t numToneBands;
	Atrac3pWaveParam wavePool[48];
	Atrac3pWaveParam *wavePoolNext;
	Atrac3pBandFlags toneSharing;
	Atrac3pBandFlags phaseShift;
	Atrac3pBandFlags toneMaster;
} Atrac3pWavesHeader;

typedef struct Atrac3pWavesData {
	Atrac3pWavesHeader *header;
	Atrac3pWaveBand bands[16];
} Atrac3pWavesData;

typedef struct Atrac3pChannelUnitCtx {
	int32_t unitType;
	int32_t reserved04;
	int32_t numChannels;
	int32_t numCodedQus;
	int32_t numQus;
	int32_t numSubbands;
	int32_t usedSubbands;
	int32_t muteFlag;
	Atrac3pBandFlags negateCoeffs;
	Atrac3pBandFlags swapChannels;
	int32_t useFullTable;
	int32_t noisePresent;
	int32_t noiseLevelIndex;
	int32_t noiseTableIndex;
	int32_t wlNumPairs;
	int32_t waveMatchIdx[48];
} Atrac3pChannelUnitCtx;

struct Atrac3pChannel;

typedef struct Atrac3pChannelUnit {
	int32_t reserved00;
	struct Atrac3pChannel *channels;
	int32_t reserved08;
	Atrac3pChannelUnitCtx ctx;
	Atrac3pWavesHeader wavesHdr[2];
	int32_t errorCode;
	int32_t reserved964;
} Atrac3pChannelUnit;

typedef struct Atrac3pChannel {
	int32_t chNum;
	Atrac3pChannelUnitCtx *unitCtx;
	Atrac3pGainInfo *gainData;
	Atrac3pGainInfo *gainDataPrev;
	Atrac3pGainInfo gainBuf[2];
	Atrac3pWavesData *wavesInfoPrev;
	Atrac3pWavesData *wavesInfo;
	Atrac3pWavesData wavesBuf[2];
	struct Atrac3pChannel *ch0;
	int32_t reservedDB4;
	Atrac3pChannelUnit *unit;
	int32_t tableType;
	int32_t ctMode;
	int32_t ctNumCodedQus;
	int32_t ctNumCodedFlag;
	int32_t ctMask[32];
	int32_t gcPresent;
	int32_t gcExtendFlag;
	int32_t gcTotalBands;
	int32_t gcCodedBands;
	int32_t gcNumPointsMode;
	int32_t gcNumPointsBits;
	int32_t gcNumPointsMin;
	int32_t gcLevMode;
	int32_t gcLevBits;
	int32_t gcLevMin;
	int32_t gcLevBandFlag[16];
	int32_t gcLocMode;
	int32_t gcLocBits;
	int32_t gcLocMin;
	int32_t gcLocBandFlag[16];
	int32_t powerComp[5];
	int32_t wlMode;
	int32_t wlNumDirect;
	int32_t wlDeltaBits;
	int32_t wlDeltaMin;
	int32_t wlWeightIdx;
	int32_t wlHuffIdx;
	int32_t wlTailMode;
	int32_t wlCodedQus;
	int32_t wlTailRun;
	int32_t wlCurveIdx;
	int32_t wlCurveBase;
	int32_t wlPairFlag;
	int32_t sfMode;
	int32_t sfNumDirect;
	int32_t sfDeltaBits;
	int32_t sfDeltaMin;
	int32_t sfHuffIdx;
	int32_t sfWeightIdx;
	int32_t sfCurveIdx;
	int32_t sfCurveBase;
	int32_t ghaEnvMode;
	int32_t ghaNumWavesMode;
	int32_t ghaFreqMode;
	int32_t ghaAmpMode;
	int32_t ghaFreqDir[16];
	int32_t ghaBandActive[16];
	int32_t ghaAmpIdxMode;
	float imdctOverlap[16][64];
	union {
		float ipqfBuf[2432];
		struct {
			float ipqfState[384];
			float ipqfScratchPad[1152];
			int32_t quCodeTable[32];
			int32_t quWordlen[32];
			int32_t quSf[32];
			int16_t quSpec[2048];
		} s;
	} u;
} Atrac3pChannel;

typedef struct Atrac3pDecoder {
	int32_t sampleRate;
	int32_t chanConfig;
	int32_t numChannels;
	int32_t numUnits;
	int32_t frameBytes;
	int32_t errorCode;
	int32_t skipFrames;
	int32_t reserved1C;
	int32_t outputChannels;
	Atrac3pChannelUnit unit;
	int32_t unitActive[1];
} Atrac3pDecoder;

_Static_assert(sizeof(Atrac3pGainBand) == 64, "Atrac3pGainBand size");
_Static_assert(sizeof(Atrac3pGainInfo) == 1096, "Atrac3pGainInfo size");
_Static_assert(sizeof(Atrac3pWavesHeader) == 1000, "Atrac3pWavesHeader size");
_Static_assert(sizeof(Atrac3pWavesData) == 644, "Atrac3pWavesData size");
_Static_assert(sizeof(Atrac3pChannelUnitCtx) == 388, "Atrac3pChannelUnitCtx size");
_Static_assert(sizeof(Atrac3pChannelUnit) == 2408, "Atrac3pChannelUnit size");
_Static_assert(sizeof(Atrac3pChannel) == 18808, "Atrac3pChannel size");
_Static_assert(sizeof(Atrac3pDecoder) == 2448, "Atrac3pDecoder size");

/* ============================================================================
 * Bitstream & utility helpers
 * ============================================================================ */

AT3P_HELPER int32_t atrac3pGetBits(int32_t nbits, const uint8_t *buf, int32_t *bitPos, int32_t *outVal)
{
	int32_t pos = *bitPos;
	if (pos <= 0x4000) {
		int32_t bytePos = pos >> 3;
		uint32_t word = ((uint32_t)buf[bytePos] << 16)
			      | ((uint32_t)buf[bytePos + 1] << 8)
			      | (uint32_t)buf[bytePos + 2];
		*outVal = (int32_t)((word << (pos & 7)) & 0xFFFFFFu) >> (24 - nbits);
	} else {
		*outVal = 0;
	}
	pos += nbits;
	*bitPos = pos;
	return pos;
}

AT3P_HELPER int32_t atrac3pParseFrameHeaderBits(int32_t *sampleRate, int32_t *chanConfig,
						int32_t *frameBytes, const uint8_t *hdr)
{
	int32_t srIdx, sizeCode;
	int32_t bitPos = 0;

	atrac3pGetBits(3, hdr, &bitPos, &srIdx);
	atrac3pGetBits(3, hdr, &bitPos, chanConfig);
	atrac3pGetBits(10, hdr, &bitPos, &sizeCode);

	if (srIdx == 1) {
		*sampleRate = 44100;
	} else if (srIdx == 2) {
		*sampleRate = 48000;
	} else if (srIdx == 0) {
		*sampleRate = 32000;
	} else {
		return 0;
	}

	*frameBytes = 8 * (sizeCode + 1);
	return 1;
}

AT3P_HELPER int32_t atrac3pParseFrameHeader(int32_t *sampleRate, int32_t *chanConfig,
					    int32_t *frameBytes, const uint8_t *hdr)
{
	if (atrac3pParseFrameHeaderBits(sampleRate, chanConfig, frameBytes, hdr) != 0)
		return 0;
	return AT3P_ERR_FAIL;
}

AT3P_HELPER int32_t atrac3pGetChannelsForConfig(int32_t chanConfig)
{
	return kAt3pChanConfigChannels[chanConfig];
}

AT3P_HELPER int32_t atrac3pGetUnitChannels(int32_t chanConfig, int32_t unitIdx)
{
	return kAt3pChanConfigUnitCh[chanConfig][unitIdx];
}

AT3P_HELPER int32_t atrac3pIsValidUnit(const Atrac3pDecoder *dec, int32_t unitIdx)
{
	return unitIdx >= 0 && unitIdx < dec->numUnits;
}

AT3P_HELPER int32_t atrac3pParseEa3Header(Atrac3pDecoder *dec, const uint8_t *inBuf,
					  uint16_t *outHdr, uint32_t *outTag)
{
	if ((((uint32_t)inBuf[0] << 8) | inBuf[1]) == 0x0FD0u) {
		*outHdr = *(const uint16_t *)(inBuf + 2);
		*outTag = *(const uint32_t *)(inBuf + 4);
		return 0;
	}
	dec->errorCode = 529;
	return AT3P_ERR_FAIL;
}

AT3P_HELPER int32_t atrac3pUpdateFrameParams(Atrac3pDecoder *dec, int32_t sampleRate,
					     int32_t chanConfig, int32_t frameBytes)
{
	if (dec->sampleRate != sampleRate) {
		dec->errorCode = 530;
		return AT3P_ERR_FAIL;
	}
	if (dec->chanConfig != chanConfig) {
		dec->errorCode = 531;
		return AT3P_ERR_FAIL;
	}
	dec->frameBytes = frameBytes;
	return 0;
}

AT3P_HELPER int32_t atrac3pCalcBitrate(const Atrac3pDecoder *dec, int32_t *outBitrate)
{
	int32_t fb = dec->frameBytes;
	int32_t sr;

	if ((uint32_t)(fb - 1) >= 0x800u)
		return AT3P_ERR_FAIL;
	sr = dec->sampleRate;
	if (sr != 48000 && sr != 44100)
		return AT3P_ERR_FAIL;

	if ((sr == 48000 && fb == 176) || (sr == 44100 && fb == 192)) {
		*outBitrate = 32;
		return 0;
	}
	*outBitrate = sr * 8 * fb / 2048000;
	return 0;
}

AT3P_HELPER int32_t atrac3pGetError(const Atrac3pDecoder *dec, int32_t *outErr)
{
	*outErr = dec->errorCode;
	return 0;
}

AT3P_HELPER int32_t atrac3pGetUnitChannelCount(Atrac3pDecoder *dec, int32_t unitIdx, int32_t *outCh)
{
	if (atrac3pIsValidUnit(dec, unitIdx)) {
		*outCh = atrac3pGetUnitChannels(dec->chanConfig, unitIdx);
		return 0;
	}
	dec->errorCode = 517;
	return AT3P_ERR_FAIL;
}

AT3P_HELPER int32_t atrac3pGetTotalChannels(Atrac3pDecoder *dec, int32_t *outTotal)
{
	int32_t i = 0;
	int32_t ch = 0;

	*outTotal = 0;
	while (i < dec->numUnits) {
		if (atrac3pGetUnitChannelCount(dec, i, &ch) != 0)
			return AT3P_ERR_FAIL;
		*outTotal += ch;
		i++;
	}
	return 0;
}

AT3P_HELPER int32_t atrac3pInitParams(Atrac3pDecoder *dec, int32_t sampleRate,
				      int32_t chanConfig, int32_t frameBytes)
{
	int32_t res = 0;
	int32_t cfg, nCh, nUnits, i;

	if (sampleRate != 44100 && sampleRate != 48000) {
		dec->errorCode = 513;
		return AT3P_ERR_FAIL;
	}
	if ((uint32_t)(chanConfig - 1) > 1u) {
		dec->errorCode = 2;
		res = AT3P_ERR_NOTIMPL;
	}
	if ((uint32_t)(frameBytes - 1) >= 0x800u) {
		dec->errorCode = 515;
		return AT3P_ERR_FAIL;
	}

	cfg = (chanConfig >= 2) ? 2 : chanConfig;
	nCh = kAt3pChanConfigChannels[cfg];
	nUnits = kAt3pChanConfigUnits[cfg];

	dec->sampleRate = sampleRate;
	dec->chanConfig = chanConfig;
	dec->frameBytes = frameBytes;
	dec->numChannels = nCh;
	dec->numUnits = nUnits;

	for (i = 0; i < nUnits; i++)
		dec->unitActive[i] = 1;

	return res;
}

AT3P_HELPER void *atrac3pInitChannelUnit(Atrac3pChannelUnit *unit, int32_t numCh, Atrac3pChannel *chBuf)
{
	void *res = NULL;
	int32_t i;

	unit->ctx.numChannels = numCh;
	unit->channels = chBuf;

	if (numCh <= 0) {
		sceClibMemset(&unit->wavesHdr[0], 0, sizeof(Atrac3pWavesHeader));
		res = sceClibMemset(&unit->wavesHdr[1], 0, sizeof(Atrac3pWavesHeader));
	} else {
		for (i = 0; i < numCh; i++) {
			Atrac3pChannel *ch = &chBuf[i];
			sceClibMemset(ch, 0, sizeof(Atrac3pChannel));
			ch->wavesBuf[0].header = &unit->wavesHdr[0];
			ch->unitCtx = &unit->ctx;
			ch->wavesInfo = &ch->wavesBuf[0];
			ch->gainData = &ch->gainBuf[0];
			ch->gainDataPrev = &ch->gainBuf[1];
			ch->wavesBuf[1].header = &unit->wavesHdr[1];
			ch->wavesInfoPrev = &ch->wavesBuf[1];
			ch->chNum = i;
			ch->unit = unit;
			ch->ch0 = unit->channels;
		}
		sceClibMemset(&unit->wavesHdr[0], 0, sizeof(Atrac3pWavesHeader));
		sceClibMemset(&unit->wavesHdr[1], 0, sizeof(Atrac3pWavesHeader));
		for (i = 0; i < numCh; i++) {
			Atrac3pChannel *ch = &unit->channels[i];
			sceClibMemset(ch->wavesInfo->bands, 0, sizeof(ch->wavesInfo->bands));
			sceClibMemset(ch->wavesInfoPrev->bands, 0, sizeof(ch->wavesInfoPrev->bands));
		}
		for (i = 0; i < numCh; i++) {
			Atrac3pChannel *ch = &unit->channels[i];
			res = sceClibMemcpy_safe(ch->gainDataPrev, ch->gainData, sizeof(Atrac3pGainInfo));
		}
	}

	unit->errorCode = 0;
	return res;
}

AT3P_HELPER int32_t atrac3pSetupUnit(Atrac3pDecoder *dec, int32_t unitIdx, void *chBuf)
{
	int32_t nCh = atrac3pGetUnitChannels(dec->chanConfig, unitIdx);
	if (atrac3pIsValidUnit(dec, unitIdx)) {
		atrac3pInitChannelUnit(&dec->unit + unitIdx, nCh, (Atrac3pChannel *)chBuf);
		dec->unitActive[unitIdx] = 1;
		return 0;
	}
	dec->errorCode = 517;
	return AT3P_ERR_FAIL;
}

AT3P_HELPER int32_t atrac3pSetupChannels(Atrac3pDecoder *dec, int32_t outCh, int32_t chMem)
{
	int32_t cfg = dec->chanConfig;

	if ((chMem & 7) != 0)
		chMem += 8 - (chMem & 7);

	dec->outputChannels = outCh;
	dec->numUnits = 1;
	dec->errorCode = 0;
	dec->skipFrames = 1;
	dec->unit.errorCode = 0;
	dec->unitActive[0] = 0;

	if ((uint32_t)(cfg - 1) <= 1u)
		return atrac3pSetupUnit(dec, 0, (void *)(uintptr_t)chMem);

	atrac3pSetupUnit(dec, 0, (void *)(uintptr_t)chMem);
	dec->errorCode = 527;
	return 1;
}

AT3P_HELPER int32_t atrac3pReadUnitId(const uint8_t *buf, int32_t *bitPos, int32_t *outPos)
{
	int32_t id;
	atrac3pGetBits(2, buf, bitPos, &id);
	*outPos = *bitPos;
	return id;
}

AT3P_HELPER int32_t atrac3pSkipExtData(const uint8_t *buf, int32_t *bitPos, int32_t *outId)
{
	int32_t count;
	int32_t dummy;
	int32_t i;

	atrac3pGetBits(5, buf, bitPos, outId);
	atrac3pGetBits(11, buf, bitPos, &count);
	for (i = 0; i < count; i++)
		atrac3pGetBits(8, buf, bitPos, &dummy);
	return count;
}

AT3P_HELPER int32_t atrac3pGetVlc(const Atrac3pVlcDesc *vlc, const uint8_t *buf, int32_t *bitPos)
{
	int32_t code;
	int32_t sym;

	atrac3pGetBits(vlc->maxBits, buf, bitPos, &code);
	sym = vlc->lut[code];
	*bitPos += (int32_t)vlc->lens[sym] - (int32_t)vlc->maxBits;
	return sym;
}

AT3P_HELPER int32_t atrac3pDecodeBandFlags(Atrac3pBandFlags *flags, const uint8_t *buf,
					   int32_t *bitPos, int32_t numBands)
{
	int32_t i;

	for (i = 0; i < 16; i++)
		flags->val[i] = 0;

	atrac3pGetBits(1, buf, bitPos, &flags->hasData);
	if (flags->hasData != 0) {
		atrac3pGetBits(1, buf, bitPos, &flags->useBit);
		if (flags->useBit != 0) {
			for (i = 0; i < numBands; i++)
				atrac3pGetBits(1, buf, bitPos, &flags->val[i]);
		} else {
			for (i = 0; i < numBands; i++)
				flags->val[i] = 1;
		}
	} else {
		for (i = 0; i < numBands; i++)
			flags->val[i] = 0;
	}
	return 0;
}

AT3P_HELPER int32_t atrac3pBuildCtMask(const int32_t *wlCur, const int32_t *wlRef,
				       int32_t chNum, int32_t numQus, int32_t *dst)
{
	int32_t i;

	sceClibMemset(dst, 0, (SceSize)(4 * numQus));
	if (chNum != 0) {
		for (i = 0; i < numQus; i++) {
			if (wlCur[i] > 0)
				dst[i] = 1;
			else if (wlRef[i] > 0)
				dst[i] = 2;
		}
	} else {
		for (i = 0; i < numQus; i++) {
			if (wlCur[i] > 0)
				dst[i] = 1;
		}
	}
	return 0;
}

AT3P_HELPER void atrac3pCountUsedQus(const int32_t *wl0, const int32_t *wl1,
				     int32_t numQus, int32_t *outCodedQus, int32_t *outUsedSubbands)
{
	int32_t qu = numQus;

	while (qu > 0 && wl0[qu - 1] == 0 && wl1[qu - 1] == 0)
		qu--;

	*outCodedQus = qu;
	*outUsedSubbands = kAt3pQuToSubband[qu] + 1;
}

AT3P_HELPER void atrac3pMatchGhaWaves(const Atrac3pWaveBand *refBands, const Atrac3pWaveBand *curBands,
				      const int32_t *active, int32_t numBands, int32_t *matchOut)
{
	int32_t refFreqs[16];
	int32_t outIdx = 0;
	int32_t b;

	sceClibMemset(refFreqs, 0, sizeof(refFreqs));

	for (b = 0; b < numBands; b++) {
		int32_t nCur = curBands[b].numWaves;
		if (active[b] != 0 && nCur > 0) {
			int32_t nRef = refBands[b].numWaves;
			int32_t i;

			for (i = 0; i < nRef; i++)
				refFreqs[i] = refBands[b].waves[i].freqIndex;

			for (i = 0; i < nCur; i++) {
				int32_t f = curBands[b].waves[i].freqIndex;
				if (nRef <= 0) {
					matchOut[outIdx] = -1;
				} else {
					int32_t bestDist = 1024;
					int32_t bestIdx = 0;
					int32_t j;

					for (j = 0; j < nRef; j++) {
						int32_t d = f - refFreqs[j];
						if (d < 0)
							d = -d;
						if (d < bestDist) {
							bestDist = d;
							bestIdx = j;
						}
					}
					if (bestDist <= 7)
						matchOut[outIdx] = bestIdx;
					else if (i < nRef)
						matchOut[outIdx] = i;
					else
						matchOut[outIdx] = -1;
				}
				outIdx++;
			}
		}
	}
}

AT3P_HELPER void atrac3pBuildGhaActiveMask(const int32_t *sharing, int32_t *active,
					   int32_t chNum, int32_t numBands)
{
	int32_t i;

	if (chNum != 0) {
		for (i = 0; i < numBands; i++)
			active[i] = (sharing[i] == 0) ? 1 : 0;
	} else {
		for (i = 0; i < numBands; i++)
			active[i] = 1;
	}
}

AT3P_HELPER int32_t atrac3pPhaseOffset(int32_t phaseIdx)
{
	return (phaseIdx & 0x1F) << 6;
}

AT3P_HELPER float atrac3pPow2(int32_t exp)
{
	if (exp >= 0)
		return (float)(1 << exp);
	return 1.0f / (float)(1 << (-exp));
}

/* ============================================================================
 * Channel unit header & side-information decoders
 * ============================================================================ */

AT3P_HELPER int32_t atrac3pDecodeChannelUnitHeader(Atrac3pChannel *ch, const uint8_t *buf, int32_t *bitPos)
{
	int32_t val;

	atrac3pGetBits(5, buf, bitPos, &ch->unitCtx->numQus);
	val = ch->unitCtx->numQus;
	ch->unitCtx->numQus = val + 1;
	if ((uint32_t)(val - 28) <= 2u)
		ch->unit->errorCode = 4;
	ch->unitCtx->numSubbands = kAt3pQuToSubband[val + 1] + 1;
	atrac3pGetBits(1, buf, bitPos, &ch->unitCtx->muteFlag);
	return 1;
}

AT3P_HELPER int32_t atrac3pDecodeNoiseInfo(Atrac3pChannel *ch, const uint8_t *buf, int32_t *bitPos)
{
	atrac3pGetBits(1, buf, bitPos, &ch->unitCtx->noisePresent);
	if (ch->unitCtx->noisePresent != 0) {
		atrac3pGetBits(4, buf, bitPos, &ch->unitCtx->noiseLevelIndex);
		atrac3pGetBits(4, buf, bitPos, &ch->unitCtx->noiseTableIndex);
	}
	return 1;
}

/* --- Wordlength decoding --- */

AT3P_HELPER void atrac3pReadWlTailParams(Atrac3pChannel *ch, const uint8_t *buf, int32_t *bitPos)
{
	if (ch->wlTailMode != 0) {
		atrac3pGetBits(5, buf, bitPos, &ch->wlCodedQus);
		if (ch->wlTailMode == 3) {
			atrac3pGetBits(2, buf, bitPos, &ch->wlTailRun);
			if (ch->chNum != 0)
				ch->wlTailRun += 3;
			else
				ch->wlTailRun += 1;
		}
	} else {
		ch->wlCodedQus = ch->unitCtx->numQus;
	}
}

AT3P_HELPER void atrac3pDecodeWlTail(Atrac3pChannel *ch, const uint8_t *buf, int32_t *bitPos)
{
	int32_t i;
	int32_t numQus = ch->unitCtx->numQus;

	switch (ch->wlTailMode) {
	case 1:
		for (i = ch->wlCodedQus; i < numQus; i++)
			ch->u.s.quWordlen[i] = 0;
		break;
	case 2:
		if (ch->chNum != 0) {
			for (i = ch->wlCodedQus; i < numQus; i++)
				atrac3pGetBits(1, buf, bitPos, &ch->u.s.quWordlen[i]);
		} else {
			for (i = ch->wlCodedQus; i < numQus; i++)
				ch->u.s.quWordlen[i] = 1;
		}
		break;
	case 3: {
		int32_t start = ch->wlCodedQus;
		int32_t split = (ch->chNum != 0) ? (start + ch->wlTailRun) : (numQus - ch->wlTailRun);
		for (i = start; i < split; i++)
			ch->u.s.quWordlen[i] = 1;
		for (i = split; i < numQus; i++)
			ch->u.s.quWordlen[i] = 0;
		break;
	}
	default:
		break;
	}
}

AT3P_HELPER void atrac3pDecodeWlHuff(Atrac3pChannel *ch, const uint8_t *buf, int32_t *bitPos)
{
	int32_t i;

	atrac3pGetBits(2, buf, bitPos, &ch->wlWeightIdx);
	atrac3pGetBits(2, buf, bitPos, &ch->wlTailMode);
	atrac3pReadWlTailParams(ch, buf, bitPos);

	if (ch->wlCodedQus > 0) {
		const Atrac3pVlcDesc *vlc;
		atrac3pGetBits(2, buf, bitPos, &ch->wlHuffIdx);
		vlc = &kAt3pVlcWl[ch->wlHuffIdx];
		atrac3pGetBits(3, buf, bitPos, &ch->u.s.quWordlen[0]);
		for (i = 1; i < ch->wlCodedQus; i++) {
			int32_t d = atrac3pGetVlc(vlc, buf, bitPos);
			ch->u.s.quWordlen[i] = (ch->u.s.quWordlen[i - 1] + d) & 7;
		}
	}

	atrac3pDecodeWlTail(ch, buf, bitPos);

	if (ch->wlWeightIdx != 0) {
		int32_t numQus = ch->unitCtx->numQus;
		const uint8_t *w = kAt3pWlWeights[3 * ch->chNum + (ch->wlWeightIdx - 1)];
		for (i = 0; i < numQus; i++)
			ch->u.s.quWordlen[i] += (int8_t)w[i];
	}
}

AT3P_HELPER int32_t atrac3pDecodeWordlengths(Atrac3pChannel *chs, const uint8_t *buf,
					     int32_t *bitPos, int32_t numCh)
{
	Atrac3pChannelUnitCtx *ctx = chs[0].unitCtx;
	int32_t c, i;
	int32_t ret = 1;
	int32_t pairFlagVal;
	int32_t groupBase[10];

	for (c = 0; c < numCh; c++)
		sceClibMemset(chs[c].u.s.quWordlen, 0, sizeof(chs[c].u.s.quWordlen));

	for (c = 0; c < numCh; c++) {
		Atrac3pChannel *ch = &chs[c];
		atrac3pGetBits(2, buf, bitPos, &ch->wlMode);

		if (ch->chNum != 0) {
			switch (ch->wlMode) {
			case 0:
				for (i = 0; i < ch->unitCtx->numQus; i++)
					atrac3pGetBits(3, buf, bitPos, &ch->u.s.quWordlen[i]);
				break;
			case 1:
				atrac3pGetBits(2, buf, bitPos, &ch->wlTailMode);
				atrac3pReadWlTailParams(ch, buf, bitPos);
				if (ch->wlCodedQus > 0) {
					const Atrac3pVlcDesc *vlc;
					atrac3pGetBits(2, buf, bitPos, &ch->wlHuffIdx);
					vlc = &kAt3pVlcWl[ch->wlHuffIdx];
					for (i = 0; i < ch->wlCodedQus; i++) {
						int32_t d = atrac3pGetVlc(vlc, buf, bitPos);
						ch->u.s.quWordlen[i] = (ch->ch0->u.s.quWordlen[i] + d) & 7;
					}
				}
				atrac3pDecodeWlTail(ch, buf, bitPos);
				break;
			case 2:
				atrac3pGetBits(2, buf, bitPos, &ch->wlTailMode);
				atrac3pReadWlTailParams(ch, buf, bitPos);
				if (ch->wlCodedQus > 0) {
					const Atrac3pVlcDesc *vlc;
					atrac3pGetBits(2, buf, bitPos, &ch->wlHuffIdx);
					vlc = &kAt3pVlcWl[ch->wlHuffIdx];
					ch->u.s.quWordlen[0] = (atrac3pGetVlc(vlc, buf, bitPos) + ch->ch0->u.s.quWordlen[0]) & 7;
					for (i = 1; i < ch->wlCodedQus; i++) {
						int32_t d = atrac3pGetVlc(vlc, buf, bitPos);
						ch->u.s.quWordlen[i] = (ch->ch0->u.s.quWordlen[i] - ch->ch0->u.s.quWordlen[i - 1]
									+ ch->u.s.quWordlen[i - 1] + d) & 7;
					}
				}
				atrac3pDecodeWlTail(ch, buf, bitPos);
				break;
			case 3:
				atrac3pDecodeWlHuff(ch, buf, bitPos);
				break;
			default:
				break;
			}
		} else {
			switch (ch->wlMode) {
			case 0:
				for (i = 0; i < ch->unitCtx->numQus; i++)
					atrac3pGetBits(3, buf, bitPos, &ch->u.s.quWordlen[i]);
				break;
			case 1:
				atrac3pGetBits(2, buf, bitPos, &ch->wlWeightIdx);
				atrac3pGetBits(2, buf, bitPos, &ch->wlTailMode);
				atrac3pReadWlTailParams(ch, buf, bitPos);
				if (ch->wlCodedQus > 0) {
					atrac3pGetBits(5, buf, bitPos, &ch->wlNumDirect);
					atrac3pGetBits(2, buf, bitPos, &ch->wlDeltaBits);
					atrac3pGetBits(3, buf, bitPos, &ch->wlDeltaMin);
					for (i = 0; i < ch->wlNumDirect; i++)
						atrac3pGetBits(3, buf, bitPos, &ch->u.s.quWordlen[i]);
					if (ch->wlDeltaBits <= 0) {
						for (i = ch->wlNumDirect; i < ch->wlCodedQus; i++)
							ch->u.s.quWordlen[i] = ch->wlDeltaMin;
					} else {
						for (i = ch->wlNumDirect; i < ch->wlCodedQus; i++) {
							atrac3pGetBits(ch->wlDeltaBits, buf, bitPos, &ch->u.s.quWordlen[i]);
							ch->u.s.quWordlen[i] += ch->wlDeltaMin;
						}
					}
				}
				atrac3pDecodeWlTail(ch, buf, bitPos);
				if (ch->wlWeightIdx != 0) {
					int32_t numQus = ch->unitCtx->numQus;
					const uint8_t *w = kAt3pWlWeights[3 * ch->chNum + (ch->wlWeightIdx - 1)];
					for (i = 0; i < numQus; i++)
						ch->u.s.quWordlen[i] += (int8_t)w[i];
				}
				break;
			case 2: {
				Atrac3pChannelUnitCtx *uctx = ch->unitCtx;
				atrac3pGetBits(2, buf, bitPos, &ch->wlTailMode);
				atrac3pReadWlTailParams(ch, buf, bitPos);
				if (ch->wlCodedQus > 0) {
					const Atrac3pVlcDesc *vlc;
					int32_t nCoded;
					atrac3pGetBits(1, buf, bitPos, &ch->wlPairFlag);
					atrac3pGetBits(1, buf, bitPos, &ch->wlHuffIdx);
					atrac3pGetBits(3, buf, bitPos, &ch->wlCurveBase);
					atrac3pGetBits(4, buf, bitPos, &ch->wlCurveIdx);

					nCoded = ch->wlCodedQus;
					groupBase[0] = ch->wlCurveBase;
					if (nCoded > 0) {
						int32_t maxGrp = kAt3pQuToGroup[nCoded - 1];
						const int8_t *curve = kAt3pWlCurve[ch->wlCurveBase][ch->wlCurveIdx];
						for (i = 0; i < maxGrp; i++)
							groupBase[i + 1] = groupBase[0] - curve[i];
						for (i = 0; i < nCoded; i++)
							ch->u.s.quWordlen[i] = groupBase[kAt3pQuToGroup[i]];
					}

					vlc = &kAt3pVlcWl[ch->wlHuffIdx];
					if (ch->wlPairFlag != 0) {
						int32_t np = nCoded >> 1;
						uctx->wlNumPairs = np;
						for (i = 0; i < np; i++) {
							atrac3pGetBits(1, buf, bitPos, &pairFlagVal);
							if (pairFlagVal == 0) {
								ch->u.s.quWordlen[2 * i] += atrac3pGetVlc(vlc, buf, bitPos);
								ch->u.s.quWordlen[2 * i + 1] += atrac3pGetVlc(vlc, buf, bitPos);
							}
						}
						for (i = 2 * uctx->wlNumPairs; i < ch->wlCodedQus; i++)
							ch->u.s.quWordlen[i] += atrac3pGetVlc(vlc, buf, bitPos);
					} else {
						for (i = 0; i < ch->wlCodedQus; i++)
							ch->u.s.quWordlen[i] += atrac3pGetVlc(vlc, buf, bitPos);
					}

					for (i = 0; i < ch->wlCodedQus; i++)
						ch->u.s.quWordlen[i] &= 7;
				}
				atrac3pDecodeWlTail(ch, buf, bitPos);
				break;
			}
			case 3:
				atrac3pDecodeWlHuff(ch, buf, bitPos);
				break;
			default:
				break;
			}
		}
	}

	if (numCh == 2)
		atrac3pCountUsedQus(chs[0].u.s.quWordlen, chs[1].u.s.quWordlen,
				    ctx->numQus, &ctx->numCodedQus, &ctx->usedSubbands);
	else
		atrac3pCountUsedQus(chs[0].u.s.quWordlen, chs[0].u.s.quWordlen,
				    ctx->numQus, &ctx->numCodedQus, &ctx->usedSubbands);

	for (c = 0; c < numCh; c++) {
		for (i = 0; i < ctx->numQus; i++) {
			if ((uint32_t)chs[c].u.s.quWordlen[i] > 7u) {
				ret = 0;
				chs[0].unit->errorCode = 264;
				break;
			}
		}
	}
	return ret;
}

/* --- Scalefactor decoding --- */

AT3P_HELPER void atrac3pApplySfCurve(Atrac3pChannel *ch)
{
	int32_t groupVal[10];
	int32_t base = ch->sfCurveBase;
	int32_t numCoded = ch->unitCtx->numCodedQus;
	int32_t i;

	groupVal[0] = base;
	if (numCoded > 0) {
		int32_t maxGrp = kAt3pQuToGroup[numCoded - 1];
		const int8_t *curve = kAt3pSfCurve[ch->sfCurveIdx];
		for (i = 0; i < maxGrp; i++)
			groupVal[i + 1] = groupVal[0] - curve[i];
		for (i = 0; i < numCoded; i++)
			ch->u.s.quSf[i] = groupVal[kAt3pQuToGroup[i]];
	}
}

AT3P_HELPER int32_t atrac3pDecodeScalefactors(Atrac3pChannel *chs, const uint8_t *buf,
					      int32_t *bitPos, int32_t numCh)
{
	int32_t c, i;
	int32_t ret = 1;
	int32_t tmpVal;
	int32_t deltaArr[32];

	for (c = 0; c < numCh; c++)
		sceClibMemset(chs[c].u.s.quSf, 0, sizeof(chs[c].u.s.quSf));

	if (chs[0].unitCtx->numCodedQus > 0) {
		for (c = 0; c < numCh; c++) {
			Atrac3pChannel *ch = &chs[c];
			int32_t numCoded = ch->unitCtx->numCodedQus;

			atrac3pGetBits(2, buf, bitPos, &ch->sfMode);
			if (ch->chNum != 0) {
				switch (ch->sfMode) {
				case 0:
					for (i = 0; i < numCoded; i++)
						atrac3pGetBits(6, buf, bitPos, &ch->u.s.quSf[i]);
					break;
				case 1: {
					const Atrac3pVlcDesc *vlc;
					atrac3pGetBits(2, buf, bitPos, &ch->sfHuffIdx);
					vlc = &kAt3pVlcSfTable[1 + ch->sfHuffIdx];
					for (i = 0; i < numCoded; i++) {
						int32_t d = atrac3pGetVlc(vlc, buf, bitPos);
						ch->u.s.quSf[i] = (ch->ch0->u.s.quSf[i] + d) & 0x3F;
					}
					break;
				}
				case 2: {
					const Atrac3pVlcDesc *vlc;
					atrac3pGetBits(2, buf, bitPos, &ch->sfHuffIdx);
					vlc = &kAt3pVlcSfTable[1 + ch->sfHuffIdx];
					ch->u.s.quSf[0] = (atrac3pGetVlc(vlc, buf, bitPos) + ch->ch0->u.s.quSf[0]) & 0x3F;
					for (i = 1; i < numCoded; i++) {
						int32_t d = atrac3pGetVlc(vlc, buf, bitPos);
						ch->u.s.quSf[i] = (ch->ch0->u.s.quSf[i] - ch->ch0->u.s.quSf[i - 1]
								   + ch->u.s.quSf[i - 1] + d) & 0x3F;
					}
					break;
				}
				case 3:
					for (i = 0; i < numCoded; i++)
						ch->u.s.quSf[i] = ch->ch0->u.s.quSf[i];
					break;
				default:
					break;
				}
			} else {
				switch (ch->sfMode) {
				case 0:
					for (i = 0; i < numCoded; i++)
						atrac3pGetBits(6, buf, bitPos, &ch->u.s.quSf[i]);
					break;
				case 1:
					atrac3pGetBits(2, buf, bitPos, &ch->sfWeightIdx);
					if (ch->sfWeightIdx == 3) {
						atrac3pGetBits(6, buf, bitPos, &ch->sfCurveBase);
						atrac3pGetBits(6, buf, bitPos, &ch->sfCurveIdx);
						atrac3pApplySfCurve(ch);
						atrac3pGetBits(5, buf, bitPos, &ch->sfNumDirect);
						atrac3pGetBits(2, buf, bitPos, &ch->sfDeltaBits);
						atrac3pGetBits(4, buf, bitPos, &ch->sfDeltaMin);
						ch->sfDeltaMin -= 7;
						for (i = 0; i < ch->sfNumDirect; i++) {
							atrac3pGetBits(4, buf, bitPos, &tmpVal);
							ch->u.s.quSf[i] += tmpVal - 7;
						}
						if (ch->sfDeltaBits > 0) {
							for (i = ch->sfNumDirect; i < numCoded; i++) {
								atrac3pGetBits(ch->sfDeltaBits, buf, bitPos, &tmpVal);
								ch->u.s.quSf[i] += tmpVal;
							}
						}
						for (i = ch->sfNumDirect; i < numCoded; i++)
							ch->u.s.quSf[i] += ch->sfDeltaMin;
						for (i = 0; i < numCoded; i++)
							ch->u.s.quSf[i] &= 0x3F;
					} else {
						atrac3pGetBits(5, buf, bitPos, &ch->sfNumDirect);
						atrac3pGetBits(3, buf, bitPos, &ch->sfDeltaBits);
						atrac3pGetBits(6, buf, bitPos, &ch->sfDeltaMin);
						for (i = 0; i < ch->sfNumDirect; i++)
							atrac3pGetBits(6, buf, bitPos, &ch->u.s.quSf[i]);
						if (ch->sfDeltaBits <= 0) {
							for (i = ch->sfNumDirect; i < numCoded; i++)
								ch->u.s.quSf[i] = ch->sfDeltaMin;
						} else {
							for (i = ch->sfNumDirect; i < numCoded; i++) {
								atrac3pGetBits(ch->sfDeltaBits, buf, bitPos, &ch->u.s.quSf[i]);
								ch->u.s.quSf[i] += ch->sfDeltaMin;
							}
						}
						if (ch->sfWeightIdx != 0) {
							int32_t div = ~ch->sfWeightIdx;
							for (i = 0; i < numCoded; i++)
								ch->u.s.quSf[i] += i / div;
						}
					}
					break;
				case 2: {
					const Atrac3pVlcDesc *vlc;
					atrac3pGetBits(2, buf, bitPos, &ch->sfHuffIdx);
					atrac3pGetBits(6, buf, bitPos, &ch->sfCurveBase);
					atrac3pGetBits(6, buf, bitPos, &ch->sfCurveIdx);
					atrac3pApplySfCurve(ch);
					vlc = &kAt3pVlcSfTable[5 + ch->sfHuffIdx];
					for (i = 0; i < numCoded; i++) {
						int32_t d = atrac3pGetVlc(vlc, buf, bitPos);
						int32_t sd = (d & 8) ? (d | ~0xF) : (d & 0xF);
						ch->u.s.quSf[i] = (ch->u.s.quSf[i] + sd) & 0x3F;
					}
					break;
				}
				case 3:
					atrac3pGetBits(2, buf, bitPos, &ch->sfWeightIdx);
					atrac3pGetBits(2, buf, bitPos, &ch->sfHuffIdx);
					if (ch->sfWeightIdx == 3) {
						const Atrac3pVlcDesc *vlc = &kAt3pVlcSfTable[5 + ch->sfHuffIdx];
						atrac3pGetBits(6, buf, bitPos, &ch->sfCurveBase);
						atrac3pGetBits(6, buf, bitPos, &ch->sfCurveIdx);
						atrac3pApplySfCurve(ch);
						atrac3pGetBits(4, buf, bitPos, &deltaArr[0]);
						deltaArr[0] = (deltaArr[0] - 8) & 0x3F;
						for (i = 1; i < numCoded; i++) {
							int32_t d = atrac3pGetVlc(vlc, buf, bitPos);
							int32_t sd = (d & 8) ? (d | ~0xF) : (d & 0xF);
							deltaArr[i] = (deltaArr[i - 1] + sd) & 0x3F;
						}
						for (i = 0; i < numCoded; i++)
							ch->u.s.quSf[i] = (ch->u.s.quSf[i] + deltaArr[i]) & 0x3F;
					} else {
						const Atrac3pVlcDesc *vlc = &kAt3pVlcSfTable[1 + ch->sfHuffIdx];
						atrac3pGetBits(6, buf, bitPos, &ch->u.s.quSf[0]);
						for (i = 1; i < numCoded; i++) {
							int32_t d = atrac3pGetVlc(vlc, buf, bitPos);
							ch->u.s.quSf[i] = (ch->u.s.quSf[i - 1] + d) & 0x3F;
						}
						if (ch->sfWeightIdx != 0) {
							int32_t div = ~ch->sfWeightIdx;
							for (i = 0; i < numCoded; i++)
								ch->u.s.quSf[i] += i / div;
						}
					}
					break;
				default:
					break;
				}
			}
		}
	}

	for (c = 0; c < numCh; c++) {
		int32_t numCoded = chs[c].unitCtx->numCodedQus;
		for (i = 0; i < numCoded; i++) {
			if ((uint32_t)chs[c].u.s.quSf[i] > 0x3Fu) {
				ret = 0;
				chs[0].unit->errorCode = 262;
				break;
			}
		}
	}
	return ret;
}

/* --- Code table decoding --- */

AT3P_HELPER int32_t atrac3pReadCtCodedCount(Atrac3pChannel *ch, const uint8_t *buf, int32_t *bitPos)
{
	atrac3pGetBits(1, buf, bitPos, &ch->ctNumCodedFlag);
	if (ch->ctNumCodedFlag == 0)
		return ch->unitCtx->numCodedQus;
	atrac3pGetBits(5, buf, bitPos, &ch->ctNumCodedQus);
	return ch->ctNumCodedQus;
}

AT3P_HELPER void atrac3pDecodeCtMode0(Atrac3pChannel *ch, const uint8_t *buf, int32_t *bitPos)
{
	int32_t nCoded = atrac3pReadCtCodedCount(ch, buf, bitPos);
	int32_t numQus = ch->unitCtx->numCodedQus;
	int32_t nbits = kAt3pCtChanBits[ch->unitCtx->useFullTable];
	int32_t i;

	for (i = 0; i < nCoded; i++) {
		int32_t m = ch->ctMask[i];
		if (m == 1)
			atrac3pGetBits(nbits, buf, bitPos, &ch->u.s.quCodeTable[i]);
		else if (m == 2)
			atrac3pGetBits(1, buf, bitPos, &ch->u.s.quCodeTable[i]);
		else
			ch->u.s.quCodeTable[i] = 0;
	}
	for (i = nCoded; i < numQus; i++)
		ch->u.s.quCodeTable[i] = 0;
}

AT3P_HELPER void atrac3pDecodeCtMode1(Atrac3pChannel *ch, const uint8_t *buf, int32_t *bitPos)
{
	int32_t nCoded = atrac3pReadCtCodedCount(ch, buf, bitPos);
	int32_t numQus = ch->unitCtx->numCodedQus;
	const Atrac3pVlcDesc *vlc = (ch->unitCtx->useFullTable != 0) ? &kAt3pVlcCtMode1 : &kAt3pVlcCtMode0;
	int32_t i;

	for (i = 0; i < nCoded; i++) {
		int32_t m = ch->ctMask[i];
		if (m == 1)
			ch->u.s.quCodeTable[i] = atrac3pGetVlc(vlc, buf, bitPos);
		else if (m == 2)
			atrac3pGetBits(1, buf, bitPos, &ch->u.s.quCodeTable[i]);
		else
			ch->u.s.quCodeTable[i] = 0;
	}
	for (i = nCoded; i < numQus; i++)
		ch->u.s.quCodeTable[i] = 0;
}

AT3P_HELPER void atrac3pDecodeCtMode2(Atrac3pChannel *ch, const uint8_t *buf, int32_t *bitPos)
{
	int32_t nCoded = atrac3pReadCtCodedCount(ch, buf, bitPos);
	int32_t numQus = ch->unitCtx->numCodedQus;
	int32_t i;

	if (nCoded > 0) {
		const Atrac3pVlcDesc *vlcFirst;
		const Atrac3pVlcDesc *vlcDelta;
		int32_t mask, pred = 0;

		if (ch->unitCtx->useFullTable != 0) {
			vlcFirst = &kAt3pVlcCtMode1;
			vlcDelta = &kAt3pVlcCtMode2;
			mask = 7;
		} else {
			vlcFirst = &kAt3pVlcCtMode0;
			vlcDelta = &kAt3pVlcCtMode0;
			mask = 3;
		}

		if (ch->ctMask[0] == 1) {
			pred = atrac3pGetVlc(vlcFirst, buf, bitPos);
			ch->u.s.quCodeTable[0] = pred;
		} else if (ch->ctMask[0] == 2) {
			atrac3pGetBits(1, buf, bitPos, &ch->u.s.quCodeTable[0]);
		} else {
			ch->u.s.quCodeTable[0] = 0;
		}

		for (i = 1; i < nCoded; i++) {
			int32_t m = ch->ctMask[i];
			if (m == 1) {
				pred = (pred + atrac3pGetVlc(vlcDelta, buf, bitPos)) & mask;
				ch->u.s.quCodeTable[i] = pred;
			} else if (m == 2) {
				atrac3pGetBits(1, buf, bitPos, &ch->u.s.quCodeTable[i]);
			} else {
				ch->u.s.quCodeTable[i] = 0;
			}
		}
	}

	for (i = nCoded; i < numQus; i++)
		ch->u.s.quCodeTable[i] = 0;
}

AT3P_HELPER int32_t atrac3pDecodeCodeTable(Atrac3pChannel *chs, const uint8_t *buf,
					   int32_t *bitPos, int32_t numCh)
{
	Atrac3pChannelUnitCtx *ctx = chs[0].unitCtx;
	int32_t c, i;
	int32_t ret = 1;
	int32_t maxVal;

	for (c = 0; c < numCh; c++)
		sceClibMemset(chs[c].u.s.quCodeTable, 0, sizeof(chs[c].u.s.quCodeTable));

	if (ctx->numCodedQus <= 0)
		return 1;

	atrac3pGetBits(1, buf, bitPos, &ctx->useFullTable);

	for (c = 0; c < numCh; c++) {
		Atrac3pChannel *ch = &chs[c];

		atrac3pGetBits(1, buf, bitPos, &ch->tableType);
		atrac3pGetBits(2, buf, bitPos, &ch->ctMode);
		atrac3pBuildCtMask(ch->u.s.quWordlen, ch->ch0->u.s.quWordlen,
				   ch->chNum, ch->unitCtx->numCodedQus, ch->ctMask);

		switch (ch->ctMode) {
		case 0:
			atrac3pDecodeCtMode0(ch, buf, bitPos);
			break;
		case 1:
			atrac3pDecodeCtMode1(ch, buf, bitPos);
			break;
		case 2:
			atrac3pDecodeCtMode2(ch, buf, bitPos);
			break;
		case 3:
			if (ch->chNum != 0) {
				int32_t nCoded = atrac3pReadCtCodedCount(ch, buf, bitPos);
				int32_t numQus = ch->unitCtx->numCodedQus;
				int32_t mask = (ch->unitCtx->useFullTable != 0) ? 7 : 3;
				const Atrac3pVlcDesc *vlc = (ch->unitCtx->useFullTable != 0)
					? &kAt3pVlcCtMode3 : &kAt3pVlcCtMode0;

				for (i = 0; i < nCoded; i++) {
					int32_t m = ch->ctMask[i];
					if (m == 1)
						ch->u.s.quCodeTable[i] = (atrac3pGetVlc(vlc, buf, bitPos)
									  + ch->ch0->u.s.quCodeTable[i]) & mask;
					else if (m == 2)
						atrac3pGetBits(1, buf, bitPos, &ch->u.s.quCodeTable[i]);
					else
						ch->u.s.quCodeTable[i] = 0;
				}
				for (i = nCoded; i < numQus; i++)
					ch->u.s.quCodeTable[i] = 0;
			} else {
				int32_t numQus = ch->unitCtx->numCodedQus;
				for (i = 0; i < numQus; i++)
					ch->u.s.quCodeTable[i] = 0;
			}
			break;
		default:
			break;
		}
	}

	maxVal = (ctx->useFullTable != 0) ? 8 : 4;
	for (c = 0; c < numCh; c++) {
		int32_t numQus = chs[c].unitCtx->numCodedQus;
		for (i = 0; i < numQus; i++) {
			if (chs[c].u.s.quCodeTable[i] < 0 || chs[c].u.s.quCodeTable[i] >= maxVal) {
				ret = 0;
				chs[0].unit->errorCode = 260;
				break;
			}
		}
	}
	return ret;
}

/* --- Spectrum VLC decoding --- */

AT3P_HELPER void atrac3pReadSpecBlock(int32_t count, const Atrac3pSpecCbDesc *cb,
				      const uint8_t *lut, const uint8_t *buf, int32_t *bitPos,
				      uint16_t *syms, int32_t *signCounts, int32_t *signBits)
{
	uint32_t mask0 = cb->valMask;
	uint32_t shift = cb->valBits;
	uint32_t mask1 = mask0 << shift;
	uint32_t mask2 = mask0 << (2 * shift);
	uint32_t mask3 = mask0 << (3 * shift);
	int32_t i;

	for (i = 0; i < count; i++) {
		int32_t code;
		uint32_t sym;

		atrac3pGetBits(cb->maxBits, buf, bitPos, &code);
		sym = lut[code];
		*bitPos += (int32_t)cb->lens[sym] - (int32_t)cb->maxBits;
		syms[i] = (uint16_t)sym;

		if (cb->isSigned != 0 && sym != 0) {
			int32_t cnt = 0;
			signBits[i] = 0;
			signCounts[i] = 0;
			switch (cb->groupSize) {
			case 1:
				if ((sym & mask0) != 0) {
					cnt = 1;
					signCounts[i] = 1;
				}
				break;
			case 2:
				cnt = ((sym & mask1) != 0) ? 1 : 0;
				if ((sym & mask0) != 0)
					cnt++;
				signCounts[i] = cnt;
				break;
			case 4:
				cnt = ((sym & mask3) != 0) ? 1 : 0;
				if ((sym & mask2) != 0)
					cnt++;
				if ((sym & mask1) != 0)
					cnt++;
				if ((sym & mask0) != 0)
					cnt++;
				signCounts[i] = cnt;
				break;
			default:
				cnt = 0;
				break;
			}
			atrac3pGetBits(cnt, buf, bitPos, &signBits[i]);
		}
	}
}

AT3P_HELPER void atrac3pDecodeSpecQu(const Atrac3pSpecCbDesc *cb, int16_t *out,
				     int32_t numSpecs, const uint8_t *buf, int32_t *bitPos)
{
	uint16_t syms[128];
	int32_t signCounts[128];
	int32_t signBits[128];
	int32_t flag;
	int32_t groupSize = cb->groupSize;
	int32_t blockSize = cb->blockSize;
	int32_t numSyms = numSpecs >> cb->shift;
	int32_t valBits = cb->valBits;
	int32_t valMask = cb->valMask;
	int32_t i;

	if (blockSize == 1) {
		atrac3pReadSpecBlock(numSyms, cb, cb->lut, buf, bitPos, syms, signCounts, signBits);
	} else if (blockSize > 1) {
		for (i = 0; i < numSyms; i += blockSize) {
			atrac3pGetBits(1, buf, bitPos, &flag);
			if (flag != 0)
				atrac3pReadSpecBlock(blockSize, cb, cb->lut, buf, bitPos,
						     &syms[i], &signCounts[i], &signBits[i]);
			else
				sceClibMemset(&syms[i], 0, (SceSize)(2 * blockSize));
		}
	}

	if (cb->isSigned != 0) {
		if (groupSize == 1) {
			for (i = 0; i < numSpecs; i++) {
				int32_t v = syms[i];
				if (v != 0) {
					v &= valMask;
					if (v != 0 && signBits[i] != 0)
						v = -v;
					out[i] = (int16_t)v;
				}
			}
		} else if (groupSize == 2) {
			for (i = 0; i < numSyms; i++) {
				int32_t s = (int16_t)syms[i];
				if (s != 0) {
					int32_t v0 = (s >> valBits) & valMask;
					int32_t v1 = s & valMask;
					int32_t bit = 1 << (signCounts[i] - 1);
					int32_t sb = signBits[i];
					if (v0 != 0) {
						if ((sb & bit) != 0)
							v0 = -v0;
						bit >>= 1;
					}
					if (v1 != 0 && (sb & bit) != 0)
						v1 = -v1;
					out[2 * i] = (int16_t)v0;
					out[2 * i + 1] = (int16_t)v1;
				}
			}
		} else if (groupSize == 4) {
			for (i = 0; i < numSyms; i++) {
				int32_t s = syms[i];
				if (s != 0) {
					int32_t v0 = (s >> (3 * valBits)) & valMask;
					int32_t v1 = (s >> (2 * valBits)) & valMask;
					int32_t v2 = (s >> valBits) & valMask;
					int32_t v3 = s & valMask;
					int32_t bit = 1 << (signCounts[i] - 1);
					int32_t sb = signBits[i];
					if (v0 != 0) {
						if ((sb & bit) != 0)
							v0 = -v0;
						bit >>= 1;
					}
					if (v1 != 0) {
						if ((sb & bit) != 0)
							v1 = -v1;
						bit >>= 1;
					}
					if (v2 != 0) {
						if ((sb & bit) != 0)
							v2 = -v2;
						bit >>= 1;
					}
					if (v3 != 0 && (sb & bit) != 0)
						v3 = -v3;
					out[4 * i] = (int16_t)v0;
					out[4 * i + 1] = (int16_t)v1;
					out[4 * i + 2] = (int16_t)v2;
					out[4 * i + 3] = (int16_t)v3;
				}
			}
		}
	} else {
		int16_t signExt = (int16_t)(-1 << valBits);
		int32_t signBit = (1 << (valBits - 1)) & valMask;

		if (groupSize == 1) {
			for (i = 0; i < numSpecs; i++) {
				int32_t s = (int16_t)syms[i];
				int32_t v = s & valMask;
				if ((s & signBit) != 0)
					v |= (uint16_t)signExt;
				out[i] = (int16_t)v;
			}
		} else if (groupSize == 2) {
			for (i = 0; i < numSyms; i++) {
				int32_t s = (int16_t)syms[i];
				int32_t v1 = s & valMask;
				int32_t hi = s >> valBits;
				int32_t v0 = hi & valMask;
				if ((s & signBit) != 0)
					v1 |= signExt;
				if ((hi & signBit) != 0)
					v0 |= signExt;
				out[2 * i] = (int16_t)v0;
				out[2 * i + 1] = (int16_t)v1;
			}
		} else if (groupSize == 4) {
			for (i = 0; i < numSyms; i++) {
				int32_t s = (int16_t)syms[i];
				int32_t k;
				for (k = 3; k >= 0; k--) {
					int32_t v = s & valMask;
					if ((s & signBit) != 0)
						v |= (uint16_t)signExt;
					out[4 * i + k] = (int16_t)v;
					s >>= valBits;
				}
			}
		}
	}
}

AT3P_HELPER int32_t atrac3pDecodeSpectrum(Atrac3pChannel *chs, const uint8_t *buf,
					  int32_t *bitPos, int32_t numCh)
{
	Atrac3pChannelUnitCtx *ctx = chs[0].unitCtx;
	int32_t c, i;

	if (numCh <= 0)
		return 1;

	for (c = 0; c < numCh; c++)
		sceClibMemset(chs[c].u.s.quSpec, 0, sizeof(chs[c].u.s.quSpec));

	for (c = 0; c < numCh; c++) {
		Atrac3pChannel *ch = &chs[c];
		int32_t numCoded = ctx->numCodedQus;

		for (i = 0; i < 5; i++)
			ch->powerComp[i] = 15;

		if (numCoded > 0) {
			for (i = 0; i < numCoded; i++) {
				int32_t wl = ch->u.s.quWordlen[i];
				if (wl > 0) {
					int32_t wlIdx = wl - 1;
					int32_t ct = ch->u.s.quCodeTable[i];
					int32_t tblType = ch->tableType;
					if (ctx->useFullTable == 0)
						ct = kAt3pWlPerm[7 * tblType + wlIdx][ct];
					atrac3pDecodeSpecQu(&kAt3pSpecCodebooks[56 * tblType + 7 * ct + wlIdx],
							    &ch->u.s.quSpec[kAt3pQuToSpecPos[i]],
							    kAt3pQuNumSpecs[i], buf, bitPos);
				}
			}
			if (numCoded > 2) {
				int32_t nGroups = kAt3pWlToCbGroup[ctx->usedSubbands - 1];
				for (i = 0; i <= nGroups; i++)
					atrac3pGetBits(4, buf, bitPos, &ch->powerComp[i]);
			}
		}
	}

	if (numCh == 2) {
		atrac3pDecodeBandFlags(&ctx->swapChannels, buf, bitPos, ctx->usedSubbands);
		atrac3pDecodeBandFlags(&ctx->negateCoeffs, buf, bitPos, ctx->usedSubbands);
	}
	return 1;
}

AT3P_HELPER int32_t atrac3pDecodeWindowShape(Atrac3pChannel *chs, const uint8_t *buf,
					     int32_t *bitPos, int32_t numCh)
{
	Atrac3pChannelUnitCtx *ctx = chs[0].unitCtx;
	int32_t c;

	for (c = 0; c < numCh; c++)
		atrac3pDecodeBandFlags(&chs[c].gainData->winShape, buf, bitPos, ctx->numSubbands);
	return 1;
}

/* --- Gain control decoding --- */

AT3P_HELPER void atrac3pReadGainLoc(Atrac3pGainBand *band, const uint8_t *buf,
				    int32_t *bitPos, int32_t ptIdx)
{
	if (ptIdx == 0) {
		atrac3pGetBits(5, buf, bitPos, &band->locCode[0]);
	} else {
		int32_t prev = band->locCode[ptIdx - 1];
		int32_t tmp = 0;
		if (prev <= 14) {
			atrac3pGetBits(5, buf, bitPos, &band->locCode[ptIdx]);
		} else if (prev <= 22) {
			atrac3pGetBits(4, buf, bitPos, &tmp);
			band->locCode[ptIdx] = prev + tmp + 1;
		} else if (prev <= 26) {
			atrac3pGetBits(3, buf, bitPos, &tmp);
			band->locCode[ptIdx] = prev + tmp + 1;
		} else if (prev <= 28) {
			atrac3pGetBits(2, buf, bitPos, &tmp);
			band->locCode[ptIdx] = prev + tmp + 1;
		} else if (prev == 29) {
			atrac3pGetBits(1, buf, bitPos, &tmp);
			band->locCode[ptIdx] = tmp + 30;
		} else if (prev == 30) {
			band->locCode[ptIdx] = 31;
		}
	}
}

AT3P_HELPER void atrac3pDecodeGainLevelsRaw(Atrac3pGainBand *bands, const int32_t *numBands,
					    const uint8_t *buf, int32_t *bitPos)
{
	int32_t b, p;

	for (b = 0; b < *numBands; b++) {
		for (p = 0; p < bands[b].numPoints; p++)
			atrac3pGetBits(4, buf, bitPos, &bands[b].levCode[p]);
	}
}

AT3P_HELPER void atrac3pDecodeGainLocsRaw(Atrac3pGainBand *bands, const int32_t *numBands,
					  const uint8_t *buf, int32_t *bitPos)
{
	int32_t b, p;

	for (b = 0; b < *numBands; b++) {
		for (p = 0; p < bands[b].numPoints; p++)
			atrac3pReadGainLoc(&bands[b], buf, bitPos, p);
	}
}

AT3P_HELPER void atrac3pDecodeGainLocHuff(Atrac3pGainBand *band, const uint8_t *buf, int32_t *bitPos)
{
	int32_t p;

	atrac3pGetBits(5, buf, bitPos, &band->locCode[0]);
	for (p = 1; p < band->numPoints; p++) {
		const Atrac3pVlcDesc *vlc = (band->levCode[p] - band->levCode[p - 1] <= 0)
			? &kAt3pVlcGainLev0 : &kAt3pVlcGainLev2;
		band->locCode[p] = band->locCode[p - 1] + atrac3pGetVlc(vlc, buf, bitPos);
	}
}

AT3P_HELPER int32_t atrac3pDecodeGainControl(Atrac3pChannel *chs, const uint8_t *buf,
					     int32_t *bitPos, int32_t numCh)
{
	int32_t c, b, p;
	int32_t ret = 1;

	if (numCh <= 0)
		return 1;

	for (c = 0; c < numCh; c++)
		sceClibMemset(chs[c].gainData->band, 0, sizeof(chs[c].gainData->band));

	for (c = 0; c < numCh; c++) {
		Atrac3pChannel *ch = &chs[c];
		Atrac3pGainBand *bands = ch->gainData->band;
		Atrac3pGainBand *refBands = ch->ch0->gainData->band;

		atrac3pGetBits(1, buf, bitPos, &ch->gcPresent);
		if (ch->gcPresent == 0) {
			ch->gcTotalBands = 0;
			continue;
		}

		atrac3pGetBits(4, buf, bitPos, &ch->gcCodedBands);
		ch->gcCodedBands++;
		atrac3pGetBits(1, buf, bitPos, &ch->gcExtendFlag);
		if (ch->gcExtendFlag != 0) {
			atrac3pGetBits(4, buf, bitPos, &ch->gcTotalBands);
			ch->gcTotalBands++;
		} else {
			ch->gcTotalBands = ch->gcCodedBands;
		}

		/* Number of gain points per subband */
		atrac3pGetBits(2, buf, bitPos, &ch->gcNumPointsMode);
		if (ch->chNum != 0) {
			switch (ch->gcNumPointsMode) {
			case 0:
				for (b = 0; b < ch->gcCodedBands; b++)
					atrac3pGetBits(3, buf, bitPos, &bands[b].numPoints);
				break;
			case 1:
				for (b = 0; b < ch->gcCodedBands; b++)
					bands[b].numPoints = atrac3pGetVlc(&kAt3pVlcGainLoc0, buf, bitPos);
				break;
			case 2:
				for (b = 0; b < ch->gcCodedBands; b++)
					bands[b].numPoints = (atrac3pGetVlc(&kAt3pVlcGainLoc1, buf, bitPos)
							      + refBands[b].numPoints) & 7;
				break;
			case 3:
				for (b = 0; b < ch->gcCodedBands; b++)
					bands[b].numPoints = refBands[b].numPoints;
				break;
			default:
				break;
			}
		} else {
			switch (ch->gcNumPointsMode) {
			case 0:
				for (b = 0; b < ch->gcCodedBands; b++)
					atrac3pGetBits(3, buf, bitPos, &bands[b].numPoints);
				break;
			case 1:
				for (b = 0; b < ch->gcCodedBands; b++)
					bands[b].numPoints = atrac3pGetVlc(&kAt3pVlcGainLoc0, buf, bitPos);
				break;
			case 2:
				bands[0].numPoints = atrac3pGetVlc(&kAt3pVlcGainLoc0, buf, bitPos);
				for (b = 1; b < ch->gcCodedBands; b++)
					bands[b].numPoints = (atrac3pGetVlc(&kAt3pVlcGainLoc1, buf, bitPos)
							      + bands[b - 1].numPoints) & 7;
				break;
			case 3:
				atrac3pGetBits(2, buf, bitPos, &ch->gcNumPointsBits);
				atrac3pGetBits(3, buf, bitPos, &ch->gcNumPointsMin);
				if (ch->gcNumPointsBits <= 0) {
					for (b = 0; b < ch->gcCodedBands; b++)
						bands[b].numPoints = ch->gcNumPointsMin;
				} else {
					for (b = 0; b < ch->gcCodedBands; b++) {
						atrac3pGetBits(ch->gcNumPointsBits, buf, bitPos, &bands[b].numPoints);
						bands[b].numPoints += ch->gcNumPointsMin;
					}
				}
				break;
			default:
				break;
			}
		}

		/* Gain level codes */
		atrac3pGetBits(2, buf, bitPos, &ch->gcLevMode);
		if (ch->chNum != 0) {
			switch (ch->gcLevMode) {
			case 0:
				atrac3pDecodeGainLevelsRaw(bands, &ch->gcCodedBands, buf, bitPos);
				break;
			case 1:
				for (b = 0; b < ch->gcCodedBands; b++) {
					for (p = 0; p < bands[b].numPoints; p++) {
						int32_t d = atrac3pGetVlc(&kAt3pVlcGainNpoints2, buf, bitPos);
						int32_t pred = (p < refBands[b].numPoints) ? refBands[b].levCode[p] : 7;
						bands[b].levCode[p] = (pred + d) & 0xF;
					}
				}
				break;
			case 2:
				for (b = 0; b < ch->gcCodedBands; b++) {
					if (bands[b].numPoints > 0) {
						atrac3pGetBits(1, buf, bitPos, &ch->gcLevBandFlag[b]);
						if (ch->gcLevBandFlag[b] != 0) {
							bands[b].levCode[0] = atrac3pGetVlc(&kAt3pVlcGainLoc3, buf, bitPos);
							for (p = 1; p < bands[b].numPoints; p++) {
								int32_t d = atrac3pGetVlc(&kAt3pVlcGainNpoints0, buf, bitPos);
								bands[b].levCode[p] = (bands[b].levCode[p - 1] + d) & 0xF;
							}
						} else {
							for (p = 0; p < bands[b].numPoints; p++)
								bands[b].levCode[p] = (p < refBands[b].numPoints)
									? refBands[b].levCode[p] : 7;
						}
					}
				}
				break;
			case 3:
				for (b = 0; b < ch->gcCodedBands; b++) {
					for (p = 0; p < bands[b].numPoints; p++)
						bands[b].levCode[p] = (p < refBands[b].numPoints)
							? refBands[b].levCode[p] : 7;
				}
				break;
			default:
				break;
			}
		} else {
			switch (ch->gcLevMode) {
			case 0:
				atrac3pDecodeGainLevelsRaw(bands, &ch->gcCodedBands, buf, bitPos);
				break;
			case 1:
				for (b = 0; b < ch->gcCodedBands; b++) {
					if (bands[b].numPoints > 0) {
						bands[b].levCode[0] = atrac3pGetVlc(&kAt3pVlcGainLoc3, buf, bitPos);
						for (p = 1; p < bands[b].numPoints; p++) {
							int32_t d = atrac3pGetVlc(&kAt3pVlcGainNpoints0, buf, bitPos);
							bands[b].levCode[p] = (bands[b].levCode[p - 1] + d) & 0xF;
						}
					}
				}
				break;
			case 2:
				if (bands[0].numPoints > 0) {
					bands[0].levCode[0] = atrac3pGetVlc(&kAt3pVlcGainLoc3, buf, bitPos);
					for (p = 1; p < bands[0].numPoints; p++) {
						int32_t d = atrac3pGetVlc(&kAt3pVlcGainNpoints0, buf, bitPos);
						bands[0].levCode[p] = (bands[0].levCode[p - 1] + d) & 0xF;
					}
				}
				for (b = 1; b < ch->gcCodedBands; b++) {
					for (p = 0; p < bands[b].numPoints; p++) {
						int32_t d = atrac3pGetVlc(&kAt3pVlcGainNpoints1, buf, bitPos);
						int32_t pred = (p < bands[b - 1].numPoints) ? bands[b - 1].levCode[p] : 7;
						bands[b].levCode[p] = (pred + d) & 0xF;
					}
				}
				break;
			case 3:
				atrac3pGetBits(2, buf, bitPos, &ch->gcLevBits);
				atrac3pGetBits(4, buf, bitPos, &ch->gcLevMin);
				if (ch->gcLevBits <= 0) {
					for (b = 0; b < ch->gcCodedBands; b++) {
						for (p = 0; p < bands[b].numPoints; p++)
							bands[b].levCode[p] = ch->gcLevMin;
					}
				} else {
					for (b = 0; b < ch->gcCodedBands; b++) {
						for (p = 0; p < bands[b].numPoints; p++) {
							atrac3pGetBits(ch->gcLevBits, buf, bitPos, &bands[b].levCode[p]);
							bands[b].levCode[p] += ch->gcLevMin;
						}
					}
				}
				break;
			default:
				break;
			}
		}

		/* Gain location codes */
		atrac3pGetBits(2, buf, bitPos, &ch->gcLocMode);
		if (ch->chNum == 0) {
			switch (ch->gcLocMode) {
			case 0:
				atrac3pDecodeGainLocsRaw(bands, &ch->gcCodedBands, buf, bitPos);
				break;
			case 1:
				for (b = 0; b < ch->gcCodedBands; b++) {
					if (bands[b].numPoints > 0)
						atrac3pDecodeGainLocHuff(&bands[b], buf, bitPos);
				}
				break;
			case 2:
				for (p = 0; p < bands[0].numPoints; p++)
					atrac3pReadGainLoc(&bands[0], buf, bitPos, p);
				for (b = 1; b < ch->gcCodedBands; b++) {
					if (bands[b].numPoints > 0) {
						int32_t d = atrac3pGetVlc(&kAt3pVlcGainLev1, buf, bitPos);
						if (bands[b - 1].numPoints <= 0)
							bands[b].locCode[0] = d;
						else
							bands[b].locCode[0] = (bands[b - 1].locCode[0] + d) & 0x1F;
						for (p = 1; p < bands[b].numPoints; p++) {
							int32_t noPrev = (p >= bands[b - 1].numPoints) ? 1 : 0;
							int32_t inc = (bands[b].levCode[p] - bands[b].levCode[p - 1] > 0) ? 1 : 0;
							const Atrac3pVlcDesc *vlc;
							if (noPrev)
								vlc = inc ? &kAt3pVlcGainLev2 : &kAt3pVlcGainLev0;
							else
								vlc = inc ? &kAt3pVlcGainLev3 : &kAt3pVlcGainLev1;
							d = atrac3pGetVlc(vlc, buf, bitPos);
							if (noPrev)
								bands[b].locCode[p] = bands[b].locCode[p - 1] + d;
							else
								bands[b].locCode[p] = (bands[b - 1].locCode[p] + d) & 0x1F;
						}
					}
				}
				break;
			case 3:
				atrac3pGetBits(2, buf, bitPos, &ch->gcLocBits);
				ch->gcLocBits++;
				atrac3pGetBits(5, buf, bitPos, &ch->gcLocMin);
				for (b = 0; b < ch->gcCodedBands; b++) {
					for (p = 0; p < bands[b].numPoints; p++) {
						atrac3pGetBits(ch->gcLocBits, buf, bitPos, &bands[b].locCode[p]);
						bands[b].locCode[p] += ch->gcLocMin + p;
					}
				}
				break;
			default:
				break;
			}
		} else {
			switch (ch->gcLocMode) {
			case 0:
				atrac3pDecodeGainLocsRaw(bands, &ch->gcCodedBands, buf, bitPos);
				break;
			case 1:
				for (b = 0; b < ch->gcCodedBands; b++) {
					if (bands[b].numPoints > 0) {
						int32_t d = atrac3pGetVlc(&kAt3pVlcGainLoc2, buf, bitPos);
						if (refBands[b].numPoints <= 0)
							bands[b].locCode[0] = d;
						else
							bands[b].locCode[0] = (refBands[b].locCode[0] + d) & 0x1F;
						for (p = 1; p < bands[b].numPoints; p++) {
							if (p >= refBands[b].numPoints) {
								const Atrac3pVlcDesc *vlc =
									(bands[b].levCode[p] - bands[b].levCode[p - 1] <= 0)
									? &kAt3pVlcGainLev0 : &kAt3pVlcGainLev2;
								bands[b].locCode[p] = bands[b].locCode[p - 1] + atrac3pGetVlc(vlc, buf, bitPos);
							} else if (bands[b].levCode[p] - bands[b].levCode[p - 1] <= 0) {
								bands[b].locCode[p] = (refBands[b].locCode[p]
										       + atrac3pGetVlc(&kAt3pVlcGainLoc2, buf, bitPos)) & 0x1F;
							} else {
								int32_t flag = 0;
								atrac3pGetBits(1, buf, bitPos, &flag);
								if (flag != 0)
									atrac3pReadGainLoc(&bands[b], buf, bitPos, p);
								else
									bands[b].locCode[p] = refBands[b].locCode[p];
							}
						}
					}
				}
				break;
			case 2:
				for (b = 0; b < ch->gcCodedBands; b++) {
					if (bands[b].numPoints <= 0)
						continue;
					if (bands[b].numPoints <= refBands[b].numPoints) {
						atrac3pGetBits(1, buf, bitPos, &ch->gcLocBandFlag[b]);
						if (ch->gcLocBandFlag[b] == 0) {
							for (p = 0; p < bands[b].numPoints; p++)
								bands[b].locCode[p] = refBands[b].locCode[p];
							continue;
						}
					}
					atrac3pDecodeGainLocHuff(&bands[b], buf, bitPos);
				}
				break;
			case 3:
				for (b = 0; b < ch->gcCodedBands; b++) {
					for (p = 0; p < bands[b].numPoints; p++) {
						if (p < refBands[b].numPoints)
							bands[b].locCode[p] = refBands[b].locCode[p];
						else
							atrac3pReadGainLoc(&bands[b], buf, bitPos, p);
					}
				}
				break;
			default:
				break;
			}
		}

		if (ch->gcExtendFlag != 0) {
			int32_t srcIdx = ch->gcCodedBands - 1;
			for (b = ch->gcCodedBands; b < ch->gcTotalBands; b++) {
				bands[b].numPoints = bands[srcIdx].numPoints;
				for (p = 0; p < bands[srcIdx].numPoints; p++) {
					bands[b].levCode[p] = bands[srcIdx].levCode[p];
					bands[b].locCode[p] = bands[srcIdx].locCode[p];
				}
			}
		}
	}

	/* Validate gain control parameters */
	for (c = 0; c < numCh; c++) {
		int32_t numSb = chs[c].unitCtx->numSubbands;
		Atrac3pGainBand *bands = chs[c].gainData->band;
		for (b = 0; b < numSb; b++) {
			int32_t nPts = bands[b].numPoints;
			if (nPts > 7) {
				ret = 0;
				chs[0].unit->errorCode = 261;
				break;
			}
			for (p = 0; p < nPts; p++) {
				if ((uint32_t)bands[b].levCode[p] > 0xFu || (uint32_t)bands[b].locCode[p] > 0x1Fu) {
					ret = 0;
					chs[0].unit->errorCode = 261;
					break;
				}
			}
			for (p = 1; p < nPts; p++) {
				if (bands[b].levCode[p - 1] == bands[b].levCode[p]
				    || bands[b].locCode[p] <= bands[b].locCode[p - 1]) {
					ret = 0;
					chs[0].unit->errorCode = 261;
					break;
				}
			}
		}
	}
	return ret;
}

/* --- GHA tonal waves decoding --- */

AT3P_HELPER void atrac3pDecodeGhaEnvelope(Atrac3pChannel *ch, const uint8_t *buf, int32_t *bitPos)
{
	Atrac3pWavesData *wd = ch->wavesInfo;
	int32_t b;

	for (b = 0; b < wd->header->numToneBands; b++) {
		if (ch->ghaBandActive[b] != 0) {
			Atrac3pWaveBand *band = &wd->bands[b];
			atrac3pGetBits(1, buf, bitPos, &band->hasStartPoint);
			if (band->hasStartPoint != 0)
				atrac3pGetBits(5, buf, bitPos, &band->startPoint);
			else
				band->startPoint = -1;
			atrac3pGetBits(1, buf, bitPos, &band->hasStopPoint);
			if (band->hasStopPoint != 0)
				atrac3pGetBits(5, buf, bitPos, &band->stopPoint);
			else
				band->stopPoint = 32;
		}
	}
}

AT3P_HELPER void atrac3pDecodeGhaFreqsRaw(Atrac3pChannel *ch, const uint8_t *buf, int32_t *bitPos)
{
	Atrac3pWavesData *wd = ch->wavesInfo;
	int32_t b;

	for (b = 0; b < wd->header->numToneBands; b++) {
		Atrac3pWaveBand *band = &wd->bands[b];
		int32_t nWaves = band->numWaves;
		int32_t i;

		if (ch->ghaBandActive[b] == 0)
			continue;

		if (nWaves <= 1) {
			ch->ghaFreqDir[b] = 0;
		} else {
			atrac3pGetBits(1, buf, bitPos, &ch->ghaFreqDir[b]);
		}

		if (ch->ghaFreqDir[b] == 0) {
			for (i = 0; i < nWaves; i++) {
				if (i == 0) {
					atrac3pGetBits(10, buf, bitPos, &band->waves[0].freqIndex);
				} else {
					int32_t prev = band->waves[i - 1].freqIndex;
					if (prev < 512) {
						atrac3pGetBits(10, buf, bitPos, &band->waves[i].freqIndex);
					} else if (prev < 768) {
						atrac3pGetBits(9, buf, bitPos, &band->waves[i].freqIndex);
						band->waves[i].freqIndex += 512;
					} else if (prev < 896) {
						atrac3pGetBits(8, buf, bitPos, &band->waves[i].freqIndex);
						band->waves[i].freqIndex += 768;
					} else if (prev < 960) {
						atrac3pGetBits(7, buf, bitPos, &band->waves[i].freqIndex);
						band->waves[i].freqIndex += 896;
					} else if (prev < 992) {
						atrac3pGetBits(6, buf, bitPos, &band->waves[i].freqIndex);
						band->waves[i].freqIndex += 960;
					} else if (prev < 1008) {
						atrac3pGetBits(5, buf, bitPos, &band->waves[i].freqIndex);
						band->waves[i].freqIndex += 992;
					} else if (prev < 1016) {
						atrac3pGetBits(4, buf, bitPos, &band->waves[i].freqIndex);
						band->waves[i].freqIndex += 1008;
					} else if (prev < 1020) {
						atrac3pGetBits(3, buf, bitPos, &band->waves[i].freqIndex);
						band->waves[i].freqIndex += 1016;
					} else if (prev > 1021) {
						atrac3pGetBits(1, buf, bitPos, &band->waves[i].freqIndex);
						band->waves[i].freqIndex += 1022;
					} else {
						atrac3pGetBits(2, buf, bitPos, &band->waves[i].freqIndex);
						band->waves[i].freqIndex += 1020;
					}
				}
			}
		} else if (ch->ghaFreqDir[b] == 1) {
			for (i = nWaves - 1; i >= 0; i--) {
				if (i == nWaves - 1) {
					atrac3pGetBits(10, buf, bitPos, &band->waves[i].freqIndex);
				} else {
					int32_t next = band->waves[i + 1].freqIndex;
					int32_t nbits;
					if (next <= 1)
						nbits = 1;
					else if (next <= 3)
						nbits = 2;
					else if (next <= 7)
						nbits = 3;
					else if (next <= 15)
						nbits = 4;
					else if (next <= 31)
						nbits = 5;
					else if (next <= 63)
						nbits = 6;
					else if (next <= 127)
						nbits = 7;
					else if (next <= 255)
						nbits = 8;
					else if (next < 512)
						nbits = 9;
					else
						nbits = 10;
					atrac3pGetBits(nbits, buf, bitPos, &band->waves[i].freqIndex);
				}
			}
		}
	}
}

AT3P_HELPER int32_t atrac3pDecodeGhaWaves(Atrac3pChannel *chs, const uint8_t *buf,
					  int32_t *bitPos, int32_t numCh)
{
	Atrac3pWavesHeader *hdr = chs[0].wavesInfo->header;
	int32_t c, b, i;
	int32_t tmpVal = 0;

	if (numCh == 2) {
		sceClibMemset(&hdr->toneSharing, 0, sizeof(Atrac3pBandFlags));
		sceClibMemset(&hdr->phaseShift, 0, sizeof(Atrac3pBandFlags));
		sceClibMemset(&hdr->toneMaster, 0, sizeof(Atrac3pBandFlags));
	}

	for (c = 0; c < numCh; c++) {
		Atrac3pWaveBand *bands = chs[c].wavesInfo->bands;
		sceClibMemset(bands, 0, sizeof(chs[c].wavesInfo->bands));
		for (b = 0; b < 16; b++) {
			bands[b].startPoint = 0;
			bands[b].stopPoint = 32;
		}
	}

	hdr->wavePoolNext = hdr->wavePool;
	atrac3pGetBits(1, buf, bitPos, &hdr->tonesPresent);
	if (hdr->tonesPresent == 0)
		return 1;

	atrac3pGetBits(1, buf, bitPos, &hdr->amplitudeMode);
	hdr->numToneBands = atrac3pGetVlc(&kAt3pVlcGhaNumBands, buf, bitPos) + 1;

	if (numCh == 2) {
		atrac3pDecodeBandFlags(&hdr->toneSharing, buf, bitPos, hdr->numToneBands);
		atrac3pDecodeBandFlags(&hdr->toneMaster, buf, bitPos, hdr->numToneBands);
		atrac3pDecodeBandFlags(&hdr->phaseShift, buf, bitPos, hdr->numToneBands);
	}

	for (c = 0; c < numCh; c++) {
		Atrac3pChannel *ch = &chs[c];
		Atrac3pWavesData *wd = ch->wavesInfo;
		Atrac3pWaveBand *bands = wd->bands;
		Atrac3pWaveBand *refBands = ch->ch0->wavesInfo->bands;
		int32_t nBands = wd->header->numToneBands;
		int32_t totalWaves = 0;

		atrac3pBuildGhaActiveMask(wd->header->toneSharing.val, ch->ghaBandActive, ch->chNum, nBands);

		/* Envelopes */
		if (ch->chNum != 0) {
			atrac3pGetBits(1, buf, bitPos, &tmpVal);
			ch->ghaEnvMode = tmpVal;
			if (tmpVal == 0) {
				atrac3pDecodeGhaEnvelope(ch, buf, bitPos);
			} else if (tmpVal == 1) {
				for (b = 0; b < nBands; b++) {
					if (ch->ghaBandActive[b] != 0) {
						bands[b].hasStartPoint = refBands[b].hasStartPoint;
						bands[b].hasStopPoint = refBands[b].hasStopPoint;
						bands[b].startPoint = refBands[b].startPoint;
						bands[b].stopPoint = refBands[b].stopPoint;
					}
				}
			}
		} else {
			atrac3pDecodeGhaEnvelope(ch, buf, bitPos);
		}

		/* Number of waves per band */
		atrac3pGetBits(kAt3pGhaModeBits[ch->chNum], buf, bitPos, &tmpVal);
		ch->ghaNumWavesMode = tmpVal;
		switch (tmpVal) {
		case 0:
			for (b = 0; b < nBands; b++) {
				if (ch->ghaBandActive[b] != 0)
					atrac3pGetBits(4, buf, bitPos, &bands[b].numWaves);
			}
			break;
		case 1:
			for (b = 0; b < nBands; b++) {
				if (ch->ghaBandActive[b] != 0)
					bands[b].numWaves = atrac3pGetVlc(&kAt3pVlcGhaNumWaves0, buf, bitPos);
			}
			break;
		case 2:
			for (b = 0; b < nBands; b++) {
				if (ch->ghaBandActive[b] != 0) {
					int32_t d = atrac3pGetVlc(&kAt3pVlcGhaNumWavesDelta, buf, bitPos);
					int32_t sd = (d & 4) ? (d | ~7) : (d & 7);
					bands[b].numWaves = (refBands[b].numWaves + sd) & 0xF;
				}
			}
			break;
		case 3:
			for (b = 0; b < nBands; b++) {
				if (ch->ghaBandActive[b] != 0)
					bands[b].numWaves = refBands[b].numWaves;
			}
			break;
		default:
			break;
		}

		for (b = 0; b < nBands; b++) {
			bands[b].waves = wd->header->wavePoolNext + totalWaves;
			totalWaves += bands[b].numWaves;
		}
		wd->header->wavePoolNext += totalWaves;
		if ((uintptr_t)wd->header->wavePoolNext - (uintptr_t)wd->header->wavePool >= 784u) {
			chs[0].unit->errorCode = 265;
			return 0;
		}

		/* Wave frequencies */
		if (ch->chNum != 0) {
			atrac3pGetBits(1, buf, bitPos, &tmpVal);
			ch->ghaFreqMode = tmpVal;
			if (tmpVal == 0) {
				atrac3pDecodeGhaFreqsRaw(ch, buf, bitPos);
			} else if (tmpVal == 1) {
				for (b = 0; b < nBands; b++) {
					if (ch->ghaBandActive[b] != 0 && bands[b].numWaves > 0) {
						int32_t nRef = refBands[b].numWaves;
						for (i = 0; i < bands[b].numWaves; i++) {
							int32_t d = atrac3pGetVlc(&kAt3pVlcGhaFreqDelta, buf, bitPos);
							int32_t sd = (d & 0x80) ? (d | ~0xFF) : (d & 0xFF);
							if (i < nRef)
								sd += refBands[b].waves[i].freqIndex;
							else if (nRef > 0)
								sd += refBands[b].waves[nRef - 1].freqIndex;
							bands[b].waves[i].freqIndex = sd & 0x3FF;
						}
					}
				}
			}
		} else {
			atrac3pDecodeGhaFreqsRaw(ch, buf, bitPos);
		}

		if (ch->chNum == 1) {
			atrac3pMatchGhaWaves(refBands, bands, ch->ghaBandActive, nBands,
					     ch->unitCtx->waveMatchIdx);
		}

		/* Wave amplitudes (ampSf) */
		atrac3pGetBits(kAt3pGhaModeBits[ch->chNum], buf, bitPos, &tmpVal);
		ch->ghaAmpMode = tmpVal;
		if (wd->header->amplitudeMode != 0) {
			switch (tmpVal) {
			case 0:
				for (b = 0; b < nBands; b++) {
					if (ch->ghaBandActive[b] != 0) {
						for (i = 0; i < bands[b].numWaves; i++)
							atrac3pGetBits(6, buf, bitPos, &bands[b].waves[i].ampSf);
					}
				}
				break;
			case 1:
				for (b = 0; b < nBands; b++) {
					if (ch->ghaBandActive[b] != 0) {
						for (i = 0; i < bands[b].numWaves; i++)
							bands[b].waves[i].ampSf = atrac3pGetVlc(&kAt3pVlcSfTable[0], buf, bitPos) + 20;
					}
				}
				break;
			case 2: {
				int32_t matchPos = 0;
				for (b = 0; b < nBands; b++) {
					if (ch->ghaBandActive[b] != 0) {
						for (i = 0; i < bands[b].numWaves; i++) {
							int32_t d = atrac3pGetVlc(&kAt3pVlcGhaAmpDelta, buf, bitPos);
							int32_t sd = (d & 0x10) ? (d | ~0x1F) : (d & 0x1F);
							int32_t m = ch->unitCtx->waveMatchIdx[matchPos++];
							int32_t pred = (m < 0) ? 34 : refBands[b].waves[m].ampSf;
							bands[b].waves[i].ampSf = (pred + sd) & 0x3F;
						}
					}
				}
				break;
			}
			case 3: {
				int32_t matchPos = 0;
				for (b = 0; b < nBands; b++) {
					if (ch->ghaBandActive[b] != 0) {
						for (i = 0; i < bands[b].numWaves; i++) {
							int32_t m = ch->unitCtx->waveMatchIdx[matchPos++];
							bands[b].waves[i].ampSf = (m < 0) ? 32 : refBands[b].waves[m].ampSf;
						}
					}
				}
				break;
			}
			default:
				break;
			}
		} else {
			switch (tmpVal) {
			case 0:
				for (b = 0; b < nBands; b++) {
					if (ch->ghaBandActive[b] != 0 && bands[b].numWaves > 0) {
						atrac3pGetBits(6, buf, bitPos, &bands[b].waves[0].ampSf);
						for (i = 1; i < bands[b].numWaves; i++)
							bands[b].waves[i].ampSf = bands[b].waves[0].ampSf;
					}
				}
				break;
			case 1:
				for (b = 0; b < nBands; b++) {
					if (ch->ghaBandActive[b] != 0 && bands[b].numWaves > 0) {
						int32_t v = atrac3pGetVlc(&kAt3pVlcGhaAmpMode0, buf, bitPos) + 24;
						for (i = 0; i < bands[b].numWaves; i++)
							bands[b].waves[i].ampSf = v;
					}
				}
				break;
			case 2:
				for (b = 0; b < nBands; b++) {
					if (ch->ghaBandActive[b] != 0 && bands[b].numWaves > 0) {
						int32_t d = atrac3pGetVlc(&kAt3pVlcGhaAmpDelta, buf, bitPos);
						int32_t sd = (d & 0x10) ? (d | ~0x1F) : (d & 0x1F);
						int32_t pred = (refBands[b].numWaves <= 0) ? 44 : refBands[b].waves[0].ampSf;
						int32_t v = (pred + sd) & 0x3F;
						for (i = 0; i < bands[b].numWaves; i++)
							bands[b].waves[i].ampSf = v;
					}
				}
				break;
			case 3:
				for (b = 0; b < nBands; b++) {
					if (ch->ghaBandActive[b] != 0 && bands[b].numWaves > 0) {
						int32_t v = (refBands[b].numWaves <= 0) ? 49 : refBands[b].waves[0].ampSf;
						for (i = 0; i < bands[b].numWaves; i++)
							bands[b].waves[i].ampSf = v;
					}
				}
				break;
			default:
				break;
			}

			/* Amplitude index (only when amplitudeMode == 0) */
			atrac3pGetBits(kAt3pGhaModeBits[ch->chNum], buf, bitPos, &tmpVal);
			ch->ghaAmpIdxMode = tmpVal;
			switch (tmpVal) {
			case 0:
				for (b = 0; b < nBands; b++) {
					if (ch->ghaBandActive[b] != 0) {
						for (i = 0; i < bands[b].numWaves; i++)
							atrac3pGetBits(4, buf, bitPos, &bands[b].waves[i].ampIndex);
					}
				}
				break;
			case 1:
				for (b = 0; b < nBands; b++) {
					if (ch->ghaBandActive[b] != 0) {
						if (bands[b].numWaves == 1) {
							bands[b].waves[0].ampIndex = atrac3pGetVlc(&kAt3pVlcGhaPhase[0], buf, bitPos);
						} else {
							for (i = 0; i < bands[b].numWaves; i++)
								bands[b].waves[i].ampIndex = atrac3pGetVlc(&kAt3pVlcGhaPhase[1], buf, bitPos);
						}
					}
				}
				break;
			case 2: {
				int32_t matchPos = 0;
				for (b = 0; b < nBands; b++) {
					if (ch->ghaBandActive[b] != 0) {
						for (i = 0; i < bands[b].numWaves; i++) {
							int32_t d = atrac3pGetVlc(&kAt3pVlcGhaPhaseDelta, buf, bitPos);
							int32_t sd = (d & 4) ? (d | ~7) : (d & 7);
							int32_t m = ch->unitCtx->waveMatchIdx[matchPos++];
							int32_t pred = (m < 0) ? 12 : refBands[b].waves[m].ampIndex;
							bands[b].waves[i].ampIndex = (pred + sd) & 0xF;
						}
					}
				}
				break;
			}
			case 3: {
				int32_t matchPos = 0;
				for (b = 0; b < nBands; b++) {
					if (ch->ghaBandActive[b] != 0) {
						for (i = 0; i < bands[b].numWaves; i++) {
							int32_t m = ch->unitCtx->waveMatchIdx[matchPos++];
							bands[b].waves[i].ampIndex = (m < 0) ? 14 : refBands[b].waves[m].ampIndex;
						}
					}
				}
				break;
			}
			default:
				break;
			}
		}

		/* Wave phase indices */
		for (b = 0; b < nBands; b++) {
			if (ch->ghaBandActive[b] != 0) {
				for (i = 0; i < bands[b].numWaves; i++)
					atrac3pGetBits(5, buf, bitPos, &bands[b].waves[i].phaseIndex);
			}
		}
	}

	if (numCh == 2) {
		Atrac3pWaveBand *prev0 = chs[0].wavesInfoPrev->bands;
		Atrac3pWaveBand *prev1 = chs[1].wavesInfoPrev->bands;
		Atrac3pWaveBand *cur0 = chs[0].wavesInfo->bands;
		Atrac3pWaveBand *cur1 = chs[1].wavesInfo->bands;

		for (b = 0; b < hdr->numToneBands; b++) {
			if (hdr->toneSharing.val[b] != 0) {
				cur1[b] = cur0[b];
				prev1[b].hasStartPoint = prev0[b].hasStartPoint;
				prev1[b].hasStopPoint = prev0[b].hasStopPoint;
				prev1[b].startPoint = prev0[b].startPoint;
				prev1[b].stopPoint = prev0[b].stopPoint;
			}
			if (hdr->toneMaster.val[b] != 0) {
				Atrac3pWaveBand tmp = cur1[b];
				cur1[b] = cur0[b];
				cur0[b] = tmp;
			}
		}
	}

	return 1;
}

AT3P_HELPER int32_t atrac3pDecodeChannelUnit(Atrac3pChannel *chs, const uint8_t *buf,
					     int32_t *bitPos, int32_t *outEndPos)
{
	int32_t numCh = chs[0].unitCtx->numChannels;
	int32_t res;

	if (atrac3pDecodeChannelUnitHeader(chs, buf, bitPos) == 0
	    || atrac3pDecodeWordlengths(chs, buf, bitPos, numCh) == 0
	    || atrac3pDecodeScalefactors(chs, buf, bitPos, numCh) == 0
	    || atrac3pDecodeCodeTable(chs, buf, bitPos, numCh) == 0
	    || atrac3pDecodeSpectrum(chs, buf, bitPos, numCh) == 0
	    || atrac3pDecodeWindowShape(chs, buf, bitPos, numCh) == 0
	    || atrac3pDecodeGainControl(chs, buf, bitPos, numCh) == 0
	    || atrac3pDecodeGhaWaves(chs, buf, bitPos, numCh) == 0) {
		*outEndPos = *bitPos;
		return 0;
	}
	res = atrac3pDecodeNoiseInfo(chs, buf, bitPos);
	*outEndPos = *bitPos;
	return res;
}

/* ============================================================================
 * Spectrum reconstruction, power compensation, IMDCT, GHA waves & IPQF
 * ============================================================================ */

AT3P_HELPER void atrac3pPowerCompensation(Atrac3pChannel *chs, float **specOut,
					  const int16_t *noiseOff, int32_t numCh, int32_t numSb)
{
	float pcGain[2][16];
	Atrac3pGainBand *prevGain[2][16];
	Atrac3pGainBand *curGain[2][16];
	float noiseBuf[16][128];
	Atrac3pChannelUnitCtx *ctx = chs[0].unitCtx;
	int32_t c, sb;

	for (c = 0; c < numCh; c++) {
		for (sb = 0; sb < numSb; sb++) {
			int32_t grp = kAt3pWlToCbGroup[sb];
			int32_t srcCh = (ctx->numChannels == 2 && ctx->swapChannels.val[sb] != 0) ? (1 - c) : c;
			prevGain[c][sb] = &chs[srcCh].gainDataPrev->band[sb];
			curGain[c][sb] = &chs[srcCh].gainData->band[sb];
			pcGain[c][sb] = kAt3pPowerCompTab[chs[srcCh].powerComp[grp]];
		}
	}

	if (numCh == 2) {
		for (sb = 0; sb < numSb; sb++) {
			if (pcGain[0][sb] > 0.0f || pcGain[1][sb] > 0.0f) {
				int32_t qu0 = kAt3pSubbandToQu[sb];
				sceClibMemcpy_safe(noiseBuf[sb], &kAt3pNoiseTableFloat[noiseOff[sb]],
						   (SceSize)(4 * kAt3pQuNumSpecs[qu0]));
			}
		}
	} else {
		for (sb = 0; sb < numSb; sb++) {
			if (pcGain[0][sb] > 0.0f) {
				int32_t qu0 = kAt3pSubbandToQu[sb];
				sceClibMemcpy_safe(noiseBuf[sb], &kAt3pNoiseTableFloat[noiseOff[sb]],
						   (SceSize)(4 * kAt3pQuNumSpecs[qu0]));
			}
		}
	}

	for (c = 0; c < numCh; c++) {
		for (sb = 0; sb < numSb; sb++) {
			float gain = pcGain[c][sb];
			if (gain > 0.0f) {
				Atrac3pGainBand *gCur = curGain[c][sb];
				Atrac3pGainBand *gPrev = prevGain[c][sb];
				int32_t nCur = gCur->numPoints;
				int32_t nPrev = gPrev->numPoints;
				int32_t baseLev = (nCur > 0) ? -kAt3pGainLevelOffset[gCur->levCode[0]] : 0;
				int32_t maxLev = 0;
				int32_t i, qu, quStart, quEnd;
				float scale;

				for (i = 0; i < nPrev; i++) {
					int32_t lv = (int8_t)(baseLev - kAt3pGainLevelOffset[gPrev->levCode[i]]);
					if (lv > maxLev)
						maxLev = lv;
				}
				for (i = 0; i < nCur; i++) {
					int32_t lv = (int8_t)(-kAt3pGainLevelOffset[gCur->levCode[i]]);
					if (lv > maxLev)
						maxLev = lv;
				}

				scale = gain / (float)(1 << maxLev);
				pcGain[c][sb] = scale;

				quStart = kAt3pSubbandToQuPower[sb];
				quEnd = kAt3pSubbandToQu[sb + 1];
				for (qu = quStart; qu < quEnd; qu++) {
					int32_t wl = (int16_t)chs[c].u.s.quWordlen[qu];
					if (wl > 0) {
						int32_t sf = (int16_t)chs[c].u.s.quSf[qu];
						int32_t posStart = kAt3pQuToSpecPos[qu];
						int32_t posEnd = kAt3pQuToSpecPos[qu + 1];
						float qScale = (kAt3pSfTab[sf] * kAt3pMantTab[wl] * scale) / (float)(1 << wl);
						float *dst = specOut[c];
						for (i = posStart; i < posEnd; i += 4) {
							const float *ns = &noiseBuf[sb][i - posStart];
							dst[i]     += qScale * ns[0];
							dst[i + 1] += qScale * ns[1];
							dst[i + 2] += qScale * ns[2];
							dst[i + 3] += qScale * ns[3];
						}
					}
				}
			}
		}
	}
}

AT3P_HELPER void atrac3pReconstructSpectrum(Atrac3pChannel *chs, float **specOut, int32_t numCh)
{
	Atrac3pChannelUnitCtx *ctx = chs[0].unitCtx;
	int16_t noiseOff[16];
	float swapTmp[128];
	int16_t seed = 0;
	int32_t c, i;

	for (c = 0; c < numCh; c++) {
		int32_t nCoded = chs[c].unitCtx->numCodedQus;
		for (i = 0; i < nCoded; i++)
			seed = (int16_t)(seed + chs[c].u.s.quSf[i]);
	}
	seed &= 0x3FC;

	for (i = 0; i < ctx->usedSubbands; i++) {
		noiseOff[i] = seed;
		seed = (int16_t)((seed + 128) & 0x3FC);
	}

	if (numCh == 2) {
		for (i = 0; i < ctx->numCodedQus; i++) {
			if (chs[1].u.s.quWordlen[i] == 0
			    && chs[0].u.s.quWordlen[i] > 0
			    && chs[1].u.s.quCodeTable[i] == 0) {
				int32_t pos = kAt3pQuToSpecPos[i];
				sceClibMemcpy_safe(&chs[1].u.s.quSpec[pos], &chs[0].u.s.quSpec[pos],
						   (SceSize)(2 * kAt3pQuNumSpecs[i]));
				chs[1].u.s.quWordlen[i] = chs[0].u.s.quWordlen[i];
			}
		}
	}

	for (c = 0; c < numCh; c++) {
		sceClibMemset(specOut[c], 0, 2048 * sizeof(float));
		for (i = 0; i < ctx->numCodedQus; i++) {
			int32_t wl = chs[c].u.s.quWordlen[i];
			if (wl > 0) {
				int32_t pos = kAt3pQuToSpecPos[i];
				int32_t cnt = kAt3pQuNumSpecs[i];
				float scale = kAt3pMantTab[wl] * kAt3pSfTab[chs[c].u.s.quSf[i]];
				const int16_t *src = &chs[c].u.s.quSpec[pos];
				float *dst = &specOut[c][pos];
				int32_t j;
				for (j = 0; j < cnt; j++)
					dst[j] = (float)src[j] * scale;
			}
		}
	}

	atrac3pPowerCompensation(chs, specOut, noiseOff, numCh, ctx->usedSubbands);

	if (numCh == 2) {
		for (i = 0; i < ctx->usedSubbands; i++) {
			if (ctx->swapChannels.val[i] == 1) {
				float *p0 = &specOut[0][128 * i];
				float *p1 = &specOut[1][128 * i];
				sceClibMemcpy_safe(swapTmp, p0, sizeof(swapTmp));
				sceClibMemcpy_safe(p0, p1, sizeof(swapTmp));
				sceClibMemcpy_safe(p1, swapTmp, sizeof(swapTmp));
			}
			if (ctx->negateCoeffs.val[i] == 1) {
				int32_t qu;
				for (qu = kAt3pSubbandToQu[i]; qu < kAt3pSubbandToQu[i + 1]; qu++) {
					int32_t j;
					for (j = kAt3pQuToSpecPos[qu]; j < kAt3pQuToSpecPos[qu + 1]; j++)
						specOut[1][j] = -specOut[1][j];
				}
			}
		}
	}

	if (ctx->muteFlag == 1) {
		for (c = 0; c < numCh; c++)
			sceClibMemset(specOut[c], 0, 2048 * sizeof(float));
	}
}

AT3P_HELPER void atrac3pSynthGhaBand(const Atrac3pWaveBand *band, float *dst, int32_t posOffset,
				     int32_t ampMode, int32_t phaseShift, int32_t chNum)
{
	float env[256];
	int32_t i, j;

	sceClibMemset(dst, 0, 128 * sizeof(float));

	if (ampMode != 0) {
		for (i = 0; i < band->numWaves; i++) {
			const Atrac3pWaveParam *w = &band->waves[i];
			float amp = kAt3pGainTab[w->ampSf];
			int16_t ph = (int16_t)atrac3pPhaseOffset(w->phaseIndex);
			int32_t freq = w->freqIndex;
			int32_t idx = (ph + (int16_t)(posOffset - 128) * (int16_t)freq) & 0x7FF;
			for (j = 0; j < 128; j++) {
				dst[j] += amp * kAt3pSineTable[idx];
				idx = (idx + freq) & 0x7FF;
			}
		}
	} else {
		for (i = 0; i < band->numWaves; i++) {
			const Atrac3pWaveParam *w = &band->waves[i];
			float amp = kAt3pNoiseLevelTab[w->ampIndex] * kAt3pGainTab[w->ampSf];
			int16_t ph = (int16_t)atrac3pPhaseOffset(w->phaseIndex);
			int32_t freq = w->freqIndex;
			int32_t idx = (ph + (int16_t)(posOffset - 128) * (int16_t)freq) & 0x7FF;
			for (j = 0; j < 128; j++) {
				dst[j] += amp * kAt3pSineTable[idx];
				idx = (idx + freq) & 0x7FF;
			}
		}
	}

	if (phaseShift != 0 && chNum == 1) {
		for (j = 0; j < 128; j++)
			dst[j] = -dst[j];
	}

	for (i = 0; i < 256; i++)
		env[i] = 1.0f;

	if (band->envStartFlag != 0) {
		int32_t sp = band->envStartPos;
		for (i = 0; i < sp; i++)
			env[i] = 0.0f;
		for (i = 0; i < 4; i++)
			env[sp + i] = kAt3pNoiseEnvTab[i];
	}

	if (band->envStopFlag != 0) {
		int32_t ep = band->envStopPos;
		for (i = 0; i < 4; i++)
			env[ep + i] = kAt3pNoiseEnvTab[3 - i];
		for (i = ep + 4; i < 256; i++)
			env[i] = 0.0f;
	}

	for (j = 0; j < 128; j++)
		dst[j] *= env[posOffset + j];
}

AT3P_HELPER void atrac3pSynthGhaWaves(float **subbandBuf, Atrac3pChannel *ch)
{
	Atrac3pWavesData *prev = ch->wavesInfoPrev;
	Atrac3pWavesData *cur = ch->wavesInfo;
	Atrac3pWavesHeader *prevHdr = prev->header;
	Atrac3pWavesHeader *curHdr = cur->header;
	float sumBuf[128];
	float prevOut[128];
	float curOut[128];
	int32_t b, i;

	if (prevHdr->tonesPresent == 0 && curHdr->tonesPresent == 0)
		return;

	for (b = 0; b < 16; b++) {
		Atrac3pWaveBand *cb = &cur->bands[b];
		Atrac3pWaveBand *pb = &prev->bands[b];

		if (cb->hasStartPoint != 0 && cb->startPoint < cb->stopPoint) {
			cb->envStartPos = 4 * (cb->startPoint + 32);
			cb->envStartFlag = 1;
		} else if (pb->hasStartPoint != 0) {
			cb->envStartPos = 4 * pb->startPoint;
			cb->envStartFlag = 1;
		} else {
			cb->envStartPos = 0;
			cb->envStartFlag = 0;
		}

		if (pb->hasStopPoint != 0 && cb->envStartPos <= 4 * pb->stopPoint) {
			cb->envStopPos = 4 * pb->stopPoint;
			cb->envStopFlag = 1;
		} else if (cb->hasStopPoint != 0) {
			cb->envStopFlag = 1;
			cb->envStopPos = 4 * (cb->stopPoint + 32);
		} else {
			cb->envStopFlag = 0;
			cb->envStopPos = 256;
		}

		if (cb->numWaves == 0 && pb->numWaves == 0)
			continue;

		atrac3pSynthGhaBand(pb, prevOut, 128, prevHdr->amplitudeMode,
				    prevHdr->phaseShift.val[b], ch->chNum);
		atrac3pSynthGhaBand(cb, curOut, 0, curHdr->amplitudeMode,
				    curHdr->phaseShift.val[b], ch->chNum);

		if (pb->numWaves > 0 && cb->numWaves > 0 && (pb->envStopPos - 123) > cb->envStartPos) {
			for (i = 0; i < 128; i++) {
				prevOut[i] *= kAt3pWaveEnv[128 + i];
				curOut[i] *= kAt3pWaveEnv[i];
			}
		} else {
			if (pb->numWaves > 0 && pb->envStopFlag == 0) {
				for (i = 0; i < 128; i++)
					prevOut[i] *= kAt3pWaveEnv[128 + i];
			}
			if (cb->numWaves > 0 && cb->envStartFlag == 0) {
				for (i = 0; i < 128; i++)
					curOut[i] *= kAt3pWaveEnv[i];
			}
		}

		for (i = 0; i < 128; i++)
			sumBuf[i] = curOut[i] + prevOut[i];
		for (i = 0; i < 128; i++)
			subbandBuf[b][i] += sumBuf[i];
	}
}

static inline __attribute__((always_inline)) float32x4_t atrac3pRev4(float32x4_t v)
{
	float32x4_t r = vrev64q_f32(v);
	return vextq_f32(r, r, 2);
}

AT3P_HELPER void atrac3pImdct128(float *spec, float *out, float *overlap,
				 int32_t winType, int32_t flip, float gain)
{
	float32x4_t *v = (float32x4_t *)spec;
	const float *win = (winType != 0) ? kAt3pImdctWindowSteep : kAt3pImdctWindowSine;
	float32x2x2_t c0 = vld2_f32(kAt3pImdctTwiddleA);
	float32x2_t c0a = c0.val[0];
	float c0b = vget_lane_f32(c0.val[1], 0);
	int32_t i;

	if (flip == 1) {
		for (i = 0; i < 4; i++) {
			float32x4_t a0 = atrac3pRev4(v[i]);
			float32x4_t a1 = atrac3pRev4(v[7 - i]);
			float32x4_t a2 = atrac3pRev4(v[8 + i]);
			float32x4_t a3 = atrac3pRev4(v[15 - i]);
			float32x4_t b0 = atrac3pRev4(vsubq_f32(v[23 - i], a2));
			float32x4_t b1 = atrac3pRev4(vsubq_f32(v[16 + i], a3));
			float32x4_t b2 = atrac3pRev4(vsubq_f32(v[24 + i], a1));
			float32x4_t b3 = atrac3pRev4(vsubq_f32(v[31 - i], a0));

			float32x4_t m0 = vmulq_lane_f32(a0, c0a, 0);
			float32x4_t m1 = vmulq_lane_f32(a1, c0a, 0);
			float32x4_t m2 = vmulq_lane_f32(a2, c0a, 0);
			float32x4_t m3 = vmulq_lane_f32(a3, c0a, 0);

			float32x4_t t0 = vsubq_f32(b0, m1);
			float32x4_t t1 = vaddq_f32(b0, m1);
			float32x4_t t2 = vsubq_f32(b1, m0);
			float32x4_t t3 = vaddq_f32(b1, m0);

			float32x4_t u0 = vsubq_f32(vsubq_f32(b2, m2), atrac3pRev4(t0));
			float32x4_t u1 = vsubq_f32(vaddq_f32(b2, m2), atrac3pRev4(t1));
			float32x4_t u2 = vsubq_f32(vsubq_f32(b3, m3), atrac3pRev4(t2));
			float32x4_t u3 = vsubq_f32(vaddq_f32(b3, m3), atrac3pRev4(t3));

			float32x4_t k0 = vmulq_lane_f32(t2, c0a, 1);
			float32x4_t k1 = vmulq_lane_f32(t0, c0a, 1);
			float32x4_t k2 = vmulq_n_f32(t3, c0b);
			float32x4_t k3 = vmulq_n_f32(t1, c0b);

			float32x4_t y0 = vsubq_f32(u0, k0);
			float32x4_t y1 = vaddq_f32(u0, k0);
			float32x4_t y2 = vsubq_f32(u1, k2);
			float32x4_t y3 = vaddq_f32(u1, k2);

			v[31 - i] = y0;
			v[24 + i] = vsubq_f32(vsubq_f32(u2, k1), atrac3pRev4(y0));
			v[23 - i] = y1;
			v[16 + i] = vsubq_f32(vaddq_f32(u2, k1), atrac3pRev4(y1));
			v[15 - i] = y2;
			v[8 + i]  = vsubq_f32(vsubq_f32(u3, k3), atrac3pRev4(y2));
			v[7 - i]  = y3;
			v[i]      = vsubq_f32(vaddq_f32(u3, k3), atrac3pRev4(y3));
		}
	} else {
		for (i = 0; i < 4; i++) {
			float32x4_t a0 = v[i];
			float32x4_t a1 = v[7 - i];
			float32x4_t a2 = v[8 + i];
			float32x4_t a3 = v[15 - i];
			float32x4_t r0 = v[31 - i];
			float32x4_t r1 = v[24 + i];
			float32x4_t r2 = v[23 - i];
			float32x4_t r3 = v[16 + i];

			float32x4_t b0 = vsubq_f32(a2, atrac3pRev4(r2));
			float32x4_t b1 = vsubq_f32(a3, atrac3pRev4(r3));
			float32x4_t b2 = vsubq_f32(a1, atrac3pRev4(r1));
			float32x4_t b3 = vsubq_f32(a0, atrac3pRev4(r0));

			float32x4_t m0 = vmulq_lane_f32(r0, c0a, 0);
			float32x4_t m1 = vmulq_lane_f32(r1, c0a, 0);
			float32x4_t m2 = vmulq_lane_f32(r2, c0a, 0);
			float32x4_t m3 = vmulq_lane_f32(r3, c0a, 0);

			float32x4_t t0 = vsubq_f32(b0, m1);
			float32x4_t t1 = vaddq_f32(b0, m1);
			float32x4_t t2 = vsubq_f32(b1, m0);
			float32x4_t t3 = vaddq_f32(b1, m0);

			float32x4_t u0 = vsubq_f32(vsubq_f32(b2, m2), atrac3pRev4(t0));
			float32x4_t u1 = vsubq_f32(vaddq_f32(b2, m2), atrac3pRev4(t1));
			float32x4_t u2 = vsubq_f32(vsubq_f32(b3, m3), atrac3pRev4(t2));
			float32x4_t u3 = vsubq_f32(vaddq_f32(b3, m3), atrac3pRev4(t3));

			float32x4_t k0 = vmulq_lane_f32(t2, c0a, 1);
			float32x4_t k1 = vmulq_lane_f32(t0, c0a, 1);
			float32x4_t k2 = vmulq_n_f32(t3, c0b);
			float32x4_t k3 = vmulq_n_f32(t1, c0b);

			float32x4_t y0 = vsubq_f32(u0, k0);
			float32x4_t y1 = vaddq_f32(u0, k0);
			float32x4_t y2 = vsubq_f32(u1, k2);
			float32x4_t y3 = vaddq_f32(u1, k2);

			v[31 - i] = y0;
			v[24 + i] = vsubq_f32(vsubq_f32(u2, k1), atrac3pRev4(y0));
			v[23 - i] = y1;
			v[16 + i] = vsubq_f32(vaddq_f32(u2, k1), atrac3pRev4(y1));
			v[15 - i] = y2;
			v[8 + i]  = vsubq_f32(vsubq_f32(u3, k3), atrac3pRev4(y2));
			v[7 - i]  = y3;
			v[i]      = vsubq_f32(vaddq_f32(u3, k3), atrac3pRev4(y3));
		}
	}

	/* Stage 2: 4 groups of 8 float32x4_t */
	for (i = 0; i < 4; i++) {
		float32x2x4_t tw = vld4_f32(&kAt3pImdctTwiddleA[3 + 7 * i]);
		float32x4_t *p = &v[8 * i];
		float c00 = vget_lane_f32(tw.val[0], 0);
		float32x4_t m0 = vmulq_n_f32(p[4], c00);
		float32x4_t m1 = vmulq_n_f32(p[5], c00);
		float32x4_t m2 = vmulq_n_f32(p[6], c00);
		float32x4_t m3 = vmulq_n_f32(p[7], c00);

		float32x4_t s0 = vsubq_f32(p[2], m2);
		float32x4_t a0 = vaddq_f32(p[2], m2);
		float32x4_t s1 = vsubq_f32(p[3], m3);
		float32x4_t a1 = vaddq_f32(p[3], m3);

		float32x4_t u0 = vsubq_f32(vsubq_f32(p[1], m1), atrac3pRev4(s0));
		float32x4_t u1 = vsubq_f32(vaddq_f32(p[1], m1), atrac3pRev4(a0));
		float32x4_t u2 = vsubq_f32(vsubq_f32(p[0], m0), atrac3pRev4(s1));
		float32x4_t u3 = vsubq_f32(vaddq_f32(p[0], m0), atrac3pRev4(a1));

		float32x4_t k0 = vmulq_n_f32(s1, vget_lane_f32(tw.val[2], 0));
		float32x4_t k1 = vmulq_n_f32(s0, vget_lane_f32(tw.val[2], 0));
		float32x4_t k2 = vmulq_n_f32(a1, vget_lane_f32(tw.val[1], 0));
		float32x4_t k3 = vmulq_n_f32(a0, vget_lane_f32(tw.val[1], 0));

		float32x4_t d0 = vsubq_f32(u0, k0);
		float32x4_t d1 = vaddq_f32(u0, k0);
		float32x4_t d2 = vsubq_f32(u1, k2);
		float32x4_t d3 = vaddq_f32(u1, k2);

		float32x4_t e0 = vsubq_f32(vsubq_f32(u2, k1), atrac3pRev4(d0));
		float32x4_t e1 = vsubq_f32(vaddq_f32(u2, k1), atrac3pRev4(d1));
		float32x4_t e2 = vsubq_f32(vsubq_f32(u3, k3), atrac3pRev4(d2));
		float32x4_t e3 = vsubq_f32(vaddq_f32(u3, k3), atrac3pRev4(d3));

		float32x4_t f0 = vmulq_lane_f32(d0, tw.val[2], 1);
		float32x4_t f1 = vmulq_lane_f32(d1, tw.val[1], 1);
		float32x4_t f2 = vmulq_lane_f32(d2, tw.val[0], 1);
		float32x4_t f3 = vmulq_n_f32(d3, vget_lane_f32(tw.val[3], 0));

		p[7] = vsubq_f32(e0, f0);
		p[6] = vaddq_f32(e0, f0);
		p[5] = vsubq_f32(e1, f1);
		p[4] = vaddq_f32(e1, f1);
		p[3] = vsubq_f32(e2, f2);
		p[2] = vaddq_f32(e2, f2);
		p[1] = vsubq_f32(e3, f3);
		p[0] = vaddq_f32(e3, f3);
	}

	/* Stage 3: 8 radix-4 blocks using kAt3pImdctTwiddleB */
	for (i = 0; i < 8; i++) {
		float32x4_t tw = vld1q_f32(&kAt3pImdctTwiddleB[4 * i]);
		float32x4x4_t x = vld4q_f32(&spec[16 * i]);
		float32x4_t d0 = vsubq_f32(x.val[0], x.val[3]);
		float32x4_t m2 = vmulq_f32(x.val[2], tw);
		float32x4_t d1 = vsubq_f32(x.val[1], x.val[2]);
		float32x4_t m3 = vmulq_f32(x.val[3], tw);
		x.val[0] = vaddq_f32(d0, m2);
		x.val[2] = vsubq_f32(d0, m2);
		x.val[1] = vaddq_f32(d1, m3);
		x.val[3] = vsubq_f32(d1, m3);
		vst4q_f32(&spec[16 * i], x);
	}

	/* Stage 4: Bit-reversed post-twiddle, windowing, and overlap-add */
	for (i = 0; i < 64; i++) {
		uint32_t idx = kAt3pImdctBitRev[i];
		float re = spec[2 * i];
		float im = spec[2 * i + 1];
		float diff = re - im;
		float prod = kAt3pImdctPostTwiddle[i] * im;
		float c0v = kAt3pImdctCosSin[idx];
		float c1v = kAt3pImdctCosSin[idx + 64];
		float w0 = win[63 - idx];
		float w1 = win[idx + 64];
		float prev = -overlap[idx];
		float vA = (gain * c1v) * (diff - prod);
		float vB = (diff + prod) * c0v;

		out[63 - idx] = w1 * prev + vA * w0;
		out[idx + 64] = w0 * prev - vA * w1;
		overlap[idx] = vB;
	}
}

/* --- 4 IPQF polyphase synthesis kernels --- */

static inline __attribute__((always_inline)) void atrac3pLoadProtoTriplet(const float *p, float32x4_t *t0, float32x4_t *t1, float32x4_t *t2)
{
	float32x2x3_t lo = vld3_f32(p);
	float32x2x3_t hi = vld3_f32(p + 6);
	*t0 = vcombine_f32(lo.val[0], hi.val[0]);
	*t1 = vcombine_f32(lo.val[1], hi.val[1]);
	*t2 = vcombine_f32(lo.val[2], hi.val[2]);
}

static inline __attribute__((always_inline)) void atrac3pIpqfFilterEvenCore(float32x4_t *state, const float32x4_t *inEven,
					     const float32x4_t *inOdd, float32x4_t *out,
					     const float *proto, int32_t revOut)
{
	float32x4_t tapHi[12], tapLo[12];
	float32x4_t s[12];
	int32_t k, t;

	for (k = 0; k < 4; k++) {
		atrac3pLoadProtoTriplet(proto + 12 * (7 - k), &tapHi[3 * k + 2], &tapHi[3 * k + 1], &tapHi[3 * k]);
		atrac3pLoadProtoTriplet(proto + 12 * (3 - k), &tapLo[3 * k + 2], &tapLo[3 * k + 1], &tapLo[3 * k]);
	}

	for (k = 0; k < 12; k++)
		s[k] = state[k];

	for (t = 0; t < 64; t++) {
		float32x4_t ev = inEven[2 * t];
		float32x4_t od = inOdd[2 * t];
		float32x4_t y = vmlaq_f32(s[0], tapHi[0], ev);

		if (revOut)
			y = atrac3pRev4(y);
		out[8 * t] = y;

		for (k = 0; k < 11; k++)
			s[k] = vmlaq_f32(vmlaq_f32(s[k + 1], tapHi[k + 1], ev), tapLo[k], od);
		s[11] = vmulq_f32(tapLo[11], od);
	}

	for (k = 0; k < 12; k++)
		state[k] = s[k];
}

static inline __attribute__((always_inline)) void atrac3pIpqfFilterOddCore(float32x4_t *state, const float32x4_t *inEven,
					    const float32x4_t *inOdd, float32x4_t *out,
					    const float *proto, int32_t revOut)
{
	float32x4_t tapHi[12], tapLo[12];
	float32x4_t s[11];
	int32_t k, t;

	for (k = 0; k < 4; k++) {
		atrac3pLoadProtoTriplet(proto + 12 * (7 - k), &tapHi[3 * k + 2], &tapHi[3 * k + 1], &tapHi[3 * k]);
		atrac3pLoadProtoTriplet(proto + 12 * (3 - k), &tapLo[3 * k + 2], &tapLo[3 * k + 1], &tapLo[3 * k]);
	}

	for (k = 0; k < 11; k++)
		s[k] = state[k];

	for (t = 0; t < 64; t++) {
		float32x4_t ev = inEven[2 * t];
		float32x4_t od = inOdd[2 * t];
		float32x4_t y = vmlaq_f32(vmlaq_f32(s[0], tapLo[0], od), tapHi[0], ev);

		if (revOut)
			y = atrac3pRev4(y);
		out[8 * t] = y;

		for (k = 0; k < 10; k++)
			s[k] = vmlaq_f32(vmlaq_f32(s[k + 1], tapLo[k + 1], od), tapHi[k + 1], ev);
		s[10] = vmlaq_f32(vmulq_f32(tapLo[11], od), tapHi[11], ev);
	}

	for (k = 0; k < 11; k++)
		state[k] = s[k];
}

AT3P_HELPER void atrac3pIpqfFilterEvenFwd(float32x4_t *state, const float32x4_t *inEven,
					  const float32x4_t *inOdd, float32x4_t *out,
					  const float *proto)
{
	atrac3pIpqfFilterEvenCore(state, inEven, inOdd, out, proto, 0);
}

AT3P_HELPER void atrac3pIpqfFilterOddFwd(float32x4_t *state, const float32x4_t *inEven,
					 const float32x4_t *inOdd, float32x4_t *out,
					 const float *proto)
{
	atrac3pIpqfFilterOddCore(state, inEven, inOdd, out, proto, 0);
}

AT3P_HELPER void atrac3pIpqfFilterEvenRev(float32x4_t *state, const float32x4_t *inEven,
					  const float32x4_t *inOdd, float32x4_t *out,
					  const float *proto)
{
	atrac3pIpqfFilterEvenCore(state, inEven, inOdd, out, proto, 1);
}

AT3P_HELPER void atrac3pIpqfFilterOddRev(float32x4_t *state, const float32x4_t *inEven,
					 const float32x4_t *inOdd, float32x4_t *out,
					 const float *proto)
{
	atrac3pIpqfFilterOddCore(state, inEven, inOdd, out, proto, 1);
}

static inline __attribute__((always_inline)) void atrac3pIpqfDctPass(const float *base, const float *dct,
				      float32x4_t *dstA, float32x4_t *dstB)
{
	int32_t t, b;

	for (t = 0; t < 128; t += 4) {
		float32x4x4_t m = vld4q_f32(dct);
		float32x2x2_t x0 = vld2_f32(base + t);
		float32x2x2_t x1 = vld2_f32(base + t + 128);

		float32x4_t a0 = vmlaq_lane_f32(vmulq_lane_f32(m.val[0], x0.val[0], 0), m.val[2], x1.val[0], 0);
		float32x4_t a1 = vmlaq_lane_f32(vmulq_lane_f32(m.val[0], x0.val[1], 0), m.val[2], x1.val[1], 0);
		float32x4_t a2 = vmlaq_lane_f32(vmulq_lane_f32(m.val[0], x0.val[0], 1), m.val[2], x1.val[0], 1);
		float32x4_t a3 = vmlaq_lane_f32(vmulq_lane_f32(m.val[0], x0.val[1], 1), m.val[2], x1.val[1], 1);

		float32x4_t b0 = vmlaq_lane_f32(vmulq_lane_f32(m.val[1], x0.val[0], 0), m.val[3], x1.val[0], 0);
		float32x4_t b1 = vmlaq_lane_f32(vmulq_lane_f32(m.val[1], x0.val[1], 0), m.val[3], x1.val[1], 0);
		float32x4_t b2 = vmlaq_lane_f32(vmulq_lane_f32(m.val[1], x0.val[0], 1), m.val[3], x1.val[0], 1);
		float32x4_t b3 = vmlaq_lane_f32(vmulq_lane_f32(m.val[1], x0.val[1], 1), m.val[3], x1.val[1], 1);

		for (b = 1; b < 8; b++) {
			m = vld4q_f32(dct + 16 * b);
			x0 = vld2_f32(base + t + 256 * b);
			x1 = vld2_f32(base + t + 256 * b + 128);

			a0 = vmlaq_lane_f32(vmlaq_lane_f32(a0, m.val[0], x0.val[0], 0), m.val[2], x1.val[0], 0);
			a1 = vmlaq_lane_f32(vmlaq_lane_f32(a1, m.val[0], x0.val[1], 0), m.val[2], x1.val[1], 0);
			a2 = vmlaq_lane_f32(vmlaq_lane_f32(a2, m.val[0], x0.val[0], 1), m.val[2], x1.val[0], 1);
			a3 = vmlaq_lane_f32(vmlaq_lane_f32(a3, m.val[0], x0.val[1], 1), m.val[2], x1.val[1], 1);

			b0 = vmlaq_lane_f32(vmlaq_lane_f32(b0, m.val[1], x0.val[0], 0), m.val[3], x1.val[0], 0);
			b1 = vmlaq_lane_f32(vmlaq_lane_f32(b1, m.val[1], x0.val[1], 0), m.val[3], x1.val[1], 0);
			b2 = vmlaq_lane_f32(vmlaq_lane_f32(b2, m.val[1], x0.val[0], 1), m.val[3], x1.val[0], 1);
			b3 = vmlaq_lane_f32(vmlaq_lane_f32(b3, m.val[1], x0.val[1], 1), m.val[3], x1.val[1], 1);
		}

		dstA[t]     = a0;
		dstA[t + 1] = a1;
		dstA[t + 2] = a2;
		dstA[t + 3] = a3;

		dstB[t]     = b0;
		dstB[t + 1] = b1;
		dstB[t + 2] = b2;
		dstB[t + 3] = b3;
	}
}

AT3P_HELPER void atrac3pIpqfSynthesis(float **subbands, float32x4_t *pcmOut, float32x4_t *buf)
{
	const float *base = subbands[0];
	float32x4_t *b0 = buf + 96;
	float32x4_t *b1 = buf + 224;
	float32x4_t *b2 = buf + 352;
	float32x4_t *b3 = buf + 480;

	atrac3pIpqfDctPass(base, kAt3pIpqfDctEven, b0, b1);
	atrac3pIpqfDctPass(base, kAt3pIpqfDctOdd, b2, b3);

	atrac3pIpqfFilterEvenFwd(buf + 0,  b0,     b1 + 1, pcmOut + 0, kAt3pIpqfProto0);
	atrac3pIpqfFilterEvenRev(buf + 12, b0,     b1 + 1, pcmOut + 3, kAt3pIpqfProto1);
	atrac3pIpqfFilterOddFwd (buf + 24, b0 + 1, b1,     pcmOut + 4, kAt3pIpqfProto0);
	atrac3pIpqfFilterOddRev (buf + 36, b0 + 1, b1,     pcmOut + 7, kAt3pIpqfProto1);
	atrac3pIpqfFilterEvenFwd(buf + 48, b2,     b3 + 1, pcmOut + 1, kAt3pIpqfProto2);
	atrac3pIpqfFilterEvenRev(buf + 60, b2,     b3 + 1, pcmOut + 2, kAt3pIpqfProto3);
	atrac3pIpqfFilterOddFwd (buf + 72, b2 + 1, b3,     pcmOut + 5, kAt3pIpqfProto2);
	atrac3pIpqfFilterOddRev (buf + 84, b2 + 1, b3,     pcmOut + 6, kAt3pIpqfProto3);
}

AT3P_HELPER void atrac3pSynthesizeChannels(float **specIn, float **pcmOut, Atrac3pChannel *chs,
					   int32_t numSb, int32_t numCh)
{
	float sbStorage[2048] __attribute__((aligned(64)));
	float *sbPtrs[2][16];
	int32_t c, sb;

	for (c = 0; c < numCh; c++) {
		Atrac3pChannel *ch = &chs[c];
		int32_t winSb = ch->unitCtx->numSubbands;
		Atrac3pGainBand *curGain = ch->gainData->band;
		Atrac3pGainBand *prevGain = ch->gainDataPrev->band;

		for (sb = 0; sb < 16; sb++)
			sbPtrs[c][sb] = &sbStorage[128 * sb];

		for (sb = 0; sb < winSb; sb++)
			curGain[sb].winShape = ch->gainData->winShape.val[sb];

		for (sb = 0; sb < numSb; sb++) {
			float gain = (curGain[sb].numPoints != 0)
				? atrac3pPow2(-kAt3pGainLevelOffset[curGain[sb].levCode[0]])
				: 1.0f;
			float *sbOut = sbPtrs[c][sb];

			atrac3pImdct128(&specIn[c][128 * sb], sbOut, ch->imdctOverlap[sb],
					prevGain[sb].winShape, kAt3pSubbandFlip[sb], gain);

			/* Apply gain control compensation from previous frame's gain curve */
			if (prevGain[sb].numPoints > 0) {
				int32_t nPts = prevGain[sb].numPoints;
				int32_t lev = kAt3pGainLevelOffset[prevGain[sb].levCode[0]];
				int32_t pos = 0;
				int32_t pt;
				float *p = sbOut;

				for (pt = 0; pt < nPts; pt++) {
					int32_t loc = prevGain[sb].locCode[pt];
					int32_t nextLev = (pt == nPts - 1) ? 0
						: kAt3pGainLevelOffset[prevGain[sb].levCode[pt + 1]];

					while (pos < loc) {
						float g = atrac3pPow2(-lev);
						p[0] *= g;
						p[1] *= g;
						p[2] *= g;
						p[3] *= g;
						p += 4;
						pos++;
					}

					if (nextLev > lev) {
						float g = atrac3pPow2(-lev);
						const float *interp = kAt3pGainInterp[nextLev - lev - 1];
						p[0] *= g;
						p[1] = (p[1] * interp[2]) * g;
						p[2] = (p[2] * interp[1]) * g;
						p[3] = (p[3] * interp[0]) * g;
					} else if (nextLev < lev) {
						float gNext = atrac3pPow2(-nextLev);
						float gCur = atrac3pPow2(-lev);
						const float *interp = kAt3pGainInterp[lev - nextLev - 1];
						p[0] *= gCur;
						p[1] = (p[1] * interp[0]) * gNext;
						p[2] = (p[2] * interp[1]) * gNext;
						p[3] = (p[3] * interp[2]) * gNext;
					} else {
						float g = atrac3pPow2(-nextLev);
						p[0] *= g;
						p[1] *= g;
						p[2] *= g;
						p[3] *= g;
					}
					p += 4;
					pos++;
					lev = nextLev;
				}
			}
		}

		if (numSb < 16) {
			sceClibMemset(ch->imdctOverlap[numSb], 0, (SceSize)((16 - numSb) * 64 * sizeof(float)));
			sceClibMemset(&sbStorage[128 * numSb], 0, (SceSize)((16 - numSb) * 128 * sizeof(float)));
		}

		atrac3pSynthGhaWaves(sbPtrs[c], ch);

		/* Add comfort noise if present */
		if (ch->unitCtx->noisePresent != 0) {
			int32_t tblIdx = ch->unitCtx->noiseTableIndex;
			float nScale = (float)(1 << ch->unitCtx->noiseLevelIndex) * 0.000030517578125f;
			for (sb = 0; sb < 16; sb++) {
				int32_t off = kAt3pNoiseSeedTab[tblIdx + sb];
				const int16_t *ns = &kAt3pNoiseTableInt16[off];
				float *dst = sbPtrs[c][sb];
				int32_t j;
				for (j = 0; j < 128; j += 4) {
					dst[j]     += (float)ns[j]     * nScale;
					dst[j + 1] += (float)ns[j + 1] * nScale;
					dst[j + 2] += (float)ns[j + 2] * nScale;
					dst[j + 3] += (float)ns[j + 3] * nScale;
				}
			}
			ch->unitCtx->noiseTableIndex = tblIdx + 16;
		}

		atrac3pIpqfSynthesis(sbPtrs[c], (float32x4_t *)pcmOut[c], (float32x4_t *)ch->u.ipqfBuf);
	}
}

AT3P_HELPER int32_t atrac3pSynthesizeUnit(const uint8_t *buf, float **pcmOut, Atrac3pChannelUnit *unit)
{
	float specBuf[2][2048];
	float *specPtrs[2];
	Atrac3pChannelUnitCtx *ctx = unit->channels[0].unitCtx;
	int32_t numCh = ctx->numChannels;
	int32_t c;

	(void)buf;

	if (numCh > 0) {
		specPtrs[0] = specBuf[0];
		if (numCh > 1)
			specPtrs[1] = specBuf[1];
	}

	atrac3pReconstructSpectrum(unit->channels, specPtrs, numCh);
	atrac3pSynthesizeChannels(specPtrs, pcmOut, unit->channels, ctx->numSubbands, numCh);

	for (c = 0; c < numCh; c++) {
		Atrac3pChannel *ch = &unit->channels[c];
		Atrac3pWavesData *tmpW = ch->wavesInfo;
		Atrac3pGainInfo *tmpG = ch->gainData;
		ch->wavesInfo = ch->wavesInfoPrev;
		ch->wavesInfoPrev = tmpW;
		ch->gainData = ch->gainDataPrev;
		ch->gainDataPrev = tmpG;
	}

	return 1;
}

/* ============================================================================
 * Frame decode & public SceAudiocodec ATRAC3plus entrypoints
 * ============================================================================ */

AT3P_HELPER int32_t atrac3pDecodeFrame(Atrac3pDecoder *dec, const uint8_t *inBuf, int16_t *pcmOut,
				       int32_t *outConsumed, int32_t *outSamples)
{
	float chBuf[2][2048];
	float *chPtrs[2];
	int32_t bitPos = 0;
	int32_t firstBit = 0;
	int32_t endPos = 0;
	int32_t extId = 0;
	int32_t unitsDecoded = 0;
	int32_t inCh, outCh, res;

	chPtrs[0] = chBuf[0];
	chPtrs[1] = chBuf[1];

	atrac3pGetBits(1, inBuf, &bitPos, &firstBit);
	if (firstBit != 0) {
		dec->errorCode = 520;
		return AT3P_ERR_FAIL;
	}

	while (1) {
		int32_t id = atrac3pReadUnitId(inBuf, &bitPos, &endPos);
		if (id == 3)
			break;
		if (bitPos > 0x4000) {
			dec->errorCode = 521;
			return AT3P_ERR_FAIL;
		}
		if (id == 2) {
			atrac3pSkipExtData(inBuf, &bitPos, &extId);
		} else {
			Atrac3pChannelUnitCtx *ctx;
			if (unitsDecoded != 0) {
				if (dec->chanConfig <= 2) {
					dec->errorCode = 524;
					return AT3P_ERR_FAIL;
				}
				break;
			}
			ctx = dec->unit.channels[0].unitCtx;
			if (id == 1) {
				if (ctx->numChannels == 1) {
					dec->errorCode = 522;
					return AT3P_ERR_FAIL;
				}
				ctx->unitType = 1;
				ctx->numChannels = 2;
			} else if (id == 0) {
				if (ctx->numChannels == 2) {
					dec->errorCode = 522;
					return AT3P_ERR_FAIL;
				}
				ctx->unitType = 0;
				ctx->numChannels = 1;
			} else {
				dec->errorCode = 522;
				return AT3P_ERR_FAIL;
			}
			if (atrac3pDecodeChannelUnit(dec->unit.channels, inBuf, &bitPos, &endPos) == 0) {
				dec->errorCode = 523;
				return AT3P_ERR_FAIL;
			}
			unitsDecoded = 1;
		}
	}

	*outConsumed = dec->frameBytes;
	if (unitsDecoded != dec->numUnits) {
		dec->errorCode = 525;
		return AT3P_ERR_FAIL;
	}

	if (unitsDecoded != 0)
		atrac3pSynthesizeUnit(inBuf, chPtrs, &dec->unit);

	inCh = atrac3pGetChannelsForConfig(dec->chanConfig);
	outCh = dec->outputChannels;
	if (inCh == 1 && outCh == 2) {
		sceClibMemcpy_safe(chPtrs[1], chPtrs[0], 2048 * sizeof(float));
	} else if (inCh == 2 && outCh == 1) {
		int32_t i;
		for (i = 0; i < 2048; i++)
			chPtrs[0][i] = (chPtrs[1][i] + chPtrs[0][i]) * 0.5f;
	}

	if (dec->skipFrames != 0) {
		*outSamples = 0;
		dec->skipFrames--;
	} else {
		*outSamples = 2048;
	}

	res = (dec->numUnits > 0 && (uint32_t)(dec->unit.channels[0].unit->errorCode - 1) <= 0xFEu) ? 1 : 0;

	if (outCh == 1) {
		int32x4_t vMax = vdupq_n_s32(0x7FFF);
		int32x4_t vMin = vdupq_n_s32(-0x8000);
		const float *src = chPtrs[0];
		int16_t *dst = pcmOut;
		int32_t i;
		for (i = 0; i < 2048; i += 4) {
			float32x4_t f = vld1q_f32(src + i);
			int32x4_t q = vmaxq_s32(vminq_s32(vcvtq_s32_f32(f), vMax), vMin);
			vst1_s16(dst + i, vqmovn_s32(q));
		}
	} else if (outCh == 2) {
		int32x4_t vMax = vdupq_n_s32(0x7FFF);
		int32x4_t vMin = vdupq_n_s32(-0x8000);
		const float *srcL = chPtrs[0];
		const float *srcR = chPtrs[1];
		int16_t *dst = pcmOut;
		int32_t i;
		for (i = 0; i < 2048; i += 4) {
			float32x4x2_t z = vzipq_f32(vld1q_f32(srcL + i), vld1q_f32(srcR + i));
			int32x4_t q0 = vmaxq_s32(vminq_s32(vcvtq_s32_f32(z.val[0]), vMax), vMin);
			int32x4_t q1 = vmaxq_s32(vminq_s32(vcvtq_s32_f32(z.val[1]), vMax), vMin);
			vst1_s16(dst + 2 * i, vqmovn_s32(q0));
			vst1_s16(dst + 2 * i + 4, vqmovn_s32(q1));
		}
	} else {
		int32_t i, c;
		int16_t *dst = pcmOut;
		for (i = 0; i < 2048; i++) {
			for (c = 0; c < outCh; c++) {
				float v = chPtrs[c][i];
				int32_t q = (int32_t)(v <= 0.0f ? (v - 0.5f) : (v + 0.5f));
				if (q < -32768)
					q = -32768;
				if (q >= 0x7FFF)
					q = 0x7FFF;
				*dst++ = (int16_t)q;
			}
		}
	}

	return res;
}

int32_t atrac3pCheckNeedMem(uint32_t *work)
{
	int32_t res;

	work[0] = 0;
	res = atrac3pParseFrameHeader((int32_t *)&work[11], (int32_t *)&work[13],
				      (int32_t *)&work[14], (const uint8_t *)&work[8]);
	if (res != 0) {
		work[0] = (uint32_t)res;
		return AUDIOCODEC_ERROR_DECODE;
	}

	if (work[13] == 1) {
		work[2] = 21260;
	} else if (work[13] == 2) {
		work[2] = 40072;
	} else {
		work[2] = 0;
		return AUDIOCODEC_ERROR_UNSUPPORTED;
	}
	return res;
}

int32_t atrac3pInit(uint32_t *work)
{
	Atrac3pDecoder *dec = (Atrac3pDecoder *)(uintptr_t)work[1];
	int32_t res;

	work[0] = 0;
	if (atrac3pParseFrameHeader((int32_t *)&work[11], (int32_t *)&work[13],
				    (int32_t *)&work[14], (const uint8_t *)&work[8]) != 0
	    || atrac3pInitParams(dec, (int32_t)work[11], (int32_t)work[13], (int32_t)work[14]) != 0
	    || atrac3pGetTotalChannels(dec, (int32_t *)&work[16]) != 0
	    || atrac3pCalcBitrate(dec, (int32_t *)&work[15]) != 0
	    || (res = atrac3pSetupChannels(dec, (int32_t)work[16], (uintptr_t)dec + 2448)) != 0) {
		atrac3pGetError(dec, (int32_t *)work);
		return AUDIOCODEC_ERROR_DECODE;
	}
	return res;
}

int32_t atrac3pDecode(uint32_t *work)
{
	Atrac3pDecoder *dec = (Atrac3pDecoder *)(uintptr_t)work[1];
	int32_t hdrSkip;
	int32_t res;

	if (work[12] != 0)
		return AUDIOCODEC_ERROR_UNSUPPORTED;

	hdrSkip = (int32_t)work[10];
	if (hdrSkip != 0) {
		if (atrac3pParseEa3Header(dec, (const uint8_t *)(uintptr_t)work[4],
					  (uint16_t *)&work[8], &work[9]) < 0
		    || atrac3pParseFrameHeader((int32_t *)&work[11], (int32_t *)&work[13],
					       (int32_t *)&work[14], (const uint8_t *)&work[8]) < 0
		    || atrac3pUpdateFrameParams(dec, (int32_t)work[11], (int32_t)work[13],
						(int32_t)work[14]) < 0) {
			atrac3pGetError(dec, (int32_t *)work);
			return AUDIOCODEC_ERROR_DECODE;
		}
		hdrSkip = 8;
	}

	res = atrac3pDecodeFrame(dec, (const uint8_t *)(uintptr_t)work[4] + hdrSkip,
				 (int16_t *)(uintptr_t)work[6],
				 (int32_t *)&work[5], (int32_t *)&work[7]);
	if (res == 0) {
		work[5] += (uint32_t)hdrSkip;
		work[7] = 2u * work[16] * work[7];
		return 0;
	}

	atrac3pGetError(dec, (int32_t *)work);
	return AUDIOCODEC_ERROR_DECODE;
}
