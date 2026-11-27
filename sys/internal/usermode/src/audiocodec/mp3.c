/*
 * SceAudiocodec (vs0:sys/external/audiocodec.suprx):
 * MPEG-1 / MPEG-2 / MPEG-2.5 Layer III (MP3) software decoder (ARM + NEON).
 */
#include <arm_neon.h>
#include "audiocodec_internal.h"
#include "mp3_tables.h"

#define MP3_MAGIC_0	4102U
#define MP3_MAGIC_1	22347776U

typedef struct Mp3BitStream {
	const uint8_t *buf;		/* 0x00 */
	int32_t bitPos;			/* 0x04 */
	int32_t bytePos;		/* 0x08 */
	int32_t bitsLeft;		/* 0x0C */
} Mp3BitStream;

typedef struct Mp3Header {
	int32_t version;		/* 0x00: 0=MPEG2, 1=MPEG1, 2=MPEG2.5 */
	int32_t layer;			/* 0x04: 3=Layer III */
	int32_t errorProtection;	/* 0x08: 1 if CRC present */
	int32_t bitrateIdx;		/* 0x0C */
	int32_t sampleRateIdx;		/* 0x10 */
	int32_t padding;		/* 0x14 */
	int32_t privateExt;		/* 0x18 */
	int32_t mode;			/* 0x1C: 0=stereo,1=joint,2=dual,3=mono */
	int32_t modeExt;		/* 0x20 */
	int32_t copyright;		/* 0x24 */
	int32_t original;		/* 0x28 */
	int32_t emphasis;		/* 0x2C */
} Mp3Header;

typedef struct Mp3FrameParams {
	Mp3Header *hdr;			/* 0x00 */
	int32_t channels;		/* 0x04 */
	int32_t jsBound;		/* 0x08 */
	int32_t sblimit;		/* 0x0C */
} Mp3FrameParams;

typedef struct Mp3GranuleInfo {
	int32_t part23Length;		/* 0x00 */
	int32_t bigValues;		/* 0x04 */
	int32_t globalGain;		/* 0x08 */
	int32_t scalefacCompress;	/* 0x0C */
	int32_t windowSwitching;	/* 0x10 */
	int32_t blockType;		/* 0x14 */
	int32_t mixedBlock;		/* 0x18 */
	int32_t tableSelect[3];		/* 0x1C..0x24 */
	int32_t subblockGain[3];	/* 0x28..0x30 */
	int32_t region0Count;		/* 0x34 */
	int32_t region1Count;		/* 0x38 */
	int32_t preflag;		/* 0x3C */
	int32_t scalefacScale;		/* 0x40 */
	int32_t count1TableSelect;	/* 0x44 */
} Mp3GranuleInfo;			/* 0x48 (72B) */

typedef struct Mp3ChannelSideInfo {
	int32_t scfsi[4];		/* 0x00..0x0C */
	Mp3GranuleInfo gr[2];		/* 0x10..0x9F */
} Mp3ChannelSideInfo;			/* 0xA0 (160B) */

typedef struct Mp3SideInfo {
	int32_t mainDataBegin;		/* 0x00 */
	int32_t privateBits;		/* 0x04 */
	Mp3ChannelSideInfo ch[2];	/* 0x08..0x147 */
} Mp3SideInfo;				/* 0x148 (328B) */

typedef struct Mp3ScaleFactors {
	int16_t l[23];			/* 0x00..0x2D */
	int16_t s[3][13];		/* 0x2E..0x7B */
} Mp3ScaleFactors;			/* 0x7C (124B) */

typedef struct Mp3State {
	int32_t mainBufErr;		/* 0x0000 */
	int32_t streamErr;		/* 0x0004 */
	Mp3BitStream inStream;		/* 0x0008 */
	Mp3FrameParams frameParams;	/* 0x0018 */
	Mp3Header header;		/* 0x0028 */
	float pcmSamples[2][576];	/* 0x0058 */
	float *pcmBufPtr;		/* 0x1258 */
	int32_t samplesPerFrame;	/* 0x125C */
	int32_t mainDataTop;		/* 0x1260 */
	float hybridOut[32][18];	/* 0x1264 */
	float dequantSpec[2][32][18];	/* 0x1B64 */
	float stereoSpec[2][32][18];	/* 0x2D64 */
	int32_t huffSpec[32][18];	/* 0x3F64 */
	uint32_t resWriteIdx;		/* 0x4864 */
	uint32_t resBitPos;		/* 0x4868 */
	uint32_t resReadIdx;		/* 0x486C */
	uint32_t resRing[4096];		/* 0x4870 */
	uint32_t resBitsLeft;		/* 0x8870 */
	int16_t synthBufOff[2];		/* 0x8874 */
	float synthRing[2][2048];	/* 0x8878 */
	float *synthRingPtr;		/* 0xC878 */
	float imdctPrev[2][32][18];	/* 0xC87C */
	int16_t isPos[576];		/* 0xDA7C */
	float isRatio[576];		/* 0xDEFC */
	float isK[2][576];		/* 0xE7FC */
	int16_t mpeg2SfTmp[54];		/* 0xF9FC */
	int32_t frameBytes;		/* 0xFA68 */
	int32_t debugFailPoint;		/* 0xFA6C */
} Mp3State;				/* 0xFA70 (64112B) */

typedef struct Mp3Decoder {
	int32_t outputFormat;		/* 0x00 */
	uint8_t forceStereo;		/* 0x04 */
	uint8_t reservedForceStereo[3];
	int32_t syncSearchBytes;	/* 0x08 */
	int32_t allowMpeg25;		/* 0x0C */
	uint8_t resetReservoirFlag;	/* 0x10 */
	uint8_t skipMainDataFlag;	/* 0x11 */
	uint8_t reservedFlags[2];
	uint32_t magic0;		/* 0x14 */
	uint32_t magic1;		/* 0x18 */
	uint32_t frameCountHi;		/* 0x1C */
	uint32_t frameCountLo;		/* 0x20 */
	uint32_t bitrateBps;		/* 0x24 */
	int32_t pcmBytesOut;		/* 0x28 */
	int32_t bitsConsumed;		/* 0x2C */
	int32_t lastError;		/* 0x30 */
	uint8_t headerChanged;		/* 0x34 */
	uint8_t headerValid;		/* 0x35 */
	uint8_t streamChannels;		/* 0x36 */
	uint8_t sampleRateCode;		/* 0x37 */
	uint8_t actualOutFmt;		/* 0x38 */
	uint8_t actualOutCh;		/* 0x39 */
	uint8_t isSingleChannel;	/* 0x3A */
	uint8_t reservedSingleCh;
	uint32_t rawHdrBits;		/* 0x3C */
	int32_t mainDataBegin;		/* 0x40 */
	int32_t reservoirAvail;		/* 0x44 */
	int32_t frameDataBytes;		/* 0x48 */
	uint8_t noCrcFlag;		/* 0x4C */
	uint8_t channelModeVal;		/* 0x4D */
	uint8_t modeExtVal;		/* 0x4E */
	uint8_t copyrightVal;		/* 0x4F */
	uint8_t originalVal;		/* 0x50 */
	uint8_t emphasisVal;		/* 0x51 */
	uint8_t crcMismatch;		/* 0x52 */
	uint8_t reservedCrc;
	Mp3State st;			/* 0x54..0xFAC3 */
} Mp3Decoder;				/* 0xFAC4 (64196B) */

typedef struct Mp3Config {
	uint32_t magic0;		/* 0x000 */
	uint32_t magic1;		/* 0x004 */
	uint8_t reservedHeader[940];
	uint8_t outFmt;			/* 0x3B4 (948) */
	uint8_t channelMode;		/* 0x3B5 (949) */
	uint8_t reservedMode[74];
	uint8_t resetReservoir;		/* 0x400 (1024) */
	uint8_t skipMainData;		/* 0x401 (1025) */
	uint8_t allowMpeg25;		/* 0x402 (1026) */
	uint8_t reservedTail[141];
} Mp3Config;				/* 0x490 (1168B) */

typedef struct Mp3WorkMem {
	Mp3Config cfg;			/* 0x0000..0x048F */
	Mp3Decoder dec;			/* 0x0490..0xFF53 */
} Mp3WorkMem;

static __attribute__((noinline, noclone)) uint32_t
mp3InitConfigMagic(Mp3Config *cfg)
{
	cfg->magic0 = MP3_MAGIC_0;
	cfg->magic1 = MP3_MAGIC_1;
	return cfg->magic1;
}

static __attribute__((noinline, noclone)) int32_t
mp3InitDecoderZero(Mp3Decoder *dec)
{
	sceClibMemset(dec, 0, sizeof(Mp3Decoder));
	dec->magic0 = MP3_MAGIC_0;
	dec->magic1 = MP3_MAGIC_1;
	return 0;
}

static __attribute__((noinline, noclone)) int32_t
mp3ResetState(Mp3Decoder *dec)
{
	sceClibMemset(&dec->st, 0, sizeof(Mp3State));
	dec->st.frameParams.hdr = &dec->st.header;
	dec->st.synthRingPtr = &dec->st.synthRing[0][0];
	dec->st.pcmBufPtr = &dec->st.pcmSamples[0][0];
	dec->st.resBitsLeft = 8U;
	dec->st.synthBufOff[0] = 64;
	dec->st.synthBufOff[1] = 64;
	dec->st.mainBufErr = 0;
	dec->st.inStream.buf = NULL;
	dec->st.inStream.bitPos = 0;
	dec->st.inStream.bytePos = 0;
	dec->st.inStream.bitsLeft = 0;
	dec->st.frameParams.channels = 0;
	dec->st.frameParams.jsBound = 0;
	dec->st.frameParams.sblimit = 0;
	sceClibMemset(&dec->st.header, 0, sizeof(Mp3Header));
	return 0;
}

static __attribute__((noinline, noclone)) int32_t
mp3InitDecoderFull(Mp3Decoder *dec)
{
	sceClibMemset(dec, 0, sizeof(Mp3Decoder));
	dec->magic0 = MP3_MAGIC_0;
	dec->magic1 = MP3_MAGIC_1;
	mp3ResetState(dec);
	return 0;
}

static __attribute__((noinline, noclone)) void
mp3CheckDebugFail(Mp3State *st, int32_t point)
{
	if (st->debugFailPoint == point) {
		st->frameBytes = 0;
		st->resWriteIdx = 0;
		st->resBitsLeft = 0;
	}
}

static __attribute__((noinline, noclone)) int32_t
mp3StreamReadBits(Mp3BitStream *bs, int32_t numBits, Mp3State *st)
{
	if (st->streamErr != 0)
		return 0;

	int32_t bytePos = bs->bytePos;
	int32_t limit = st->frameBytes;
	bs->bitPos += numBits;
	if (bytePos >= limit) {
		st->streamErr = -1;
		return 0;
	}

	int32_t bitsLeft = bs->bitsLeft;
	const uint8_t *p = bs->buf + bytePos;
	uint32_t cur = (uint32_t)(*p) & (0xFFU >> (8 - bitsLeft));
	if (numBits <= bitsLeft) {
		int32_t rem = bitsLeft - numBits;
		bs->bitsLeft = rem;
		return (int32_t)(cur >> rem);
	}

	uint32_t acc = 0;
	for (;;) {
		++bytePos;
		numBits -= bitsLeft;
		bitsLeft = 8;
		acc |= cur << numBits;
		if (bytePos == limit) {
			st->streamErr = -1;
			return 0;
		}
		cur = *++p;
		if (numBits <= 8) {
			int32_t rem = 8 - numBits;
			bs->bytePos = bytePos;
			bs->bitsLeft = rem;
			return (int32_t)(acc | (cur >> rem));
		}
	}
}

static __attribute__((noinline, noclone)) int32_t
mp3StreamReadBit(Mp3BitStream *bs, Mp3State *st)
{
	return mp3StreamReadBits(bs, 1, st);
}

static __attribute__((noinline, noclone)) int32_t
mp3FindSyncWord(Mp3BitStream *bs, int32_t maxSearchBytes, Mp3State *st)
{
	int32_t startBitPos = bs->bitPos;
	int32_t failPt = st->debugFailPoint;

	if (failPt == 100) {
		st->frameBytes = 0;
		st->resWriteIdx = 0;
		st->resBitsLeft = 0;
		mp3StreamReadBits(bs, 7, st);
		startBitPos = bs->bitPos;
	} else if ((startBitPos % 8) != 0) {
		mp3StreamReadBits(bs, 8 - (startBitPos % 8), st);
		startBitPos = bs->bitPos;
	}

	if (st->debugFailPoint == 101) {
		st->frameBytes = 0;
		st->resWriteIdx = 0;
		st->resBitsLeft = 0;
	}

	uint32_t sync = (uint32_t)mp3StreamReadBits(bs, 12, st);
	if (st->streamErr != 0)
		return 0;

	int32_t maxBits = 8 * maxSearchBytes;
	for (;;) {
		uint32_t low12 = sync & 0xFFFU;
		if (low12 == 0xFFFU)
			return 1;
		if (low12 == 0xFFEU)
			return 2;
		if (bs->bitPos - startBitPos >= maxBits)
			return 0;
		if (st->debugFailPoint == 102) {
			st->frameBytes = 0;
			st->resWriteIdx = 0;
			st->resBitsLeft = 0;
		}
		sync = (sync << 8) | (uint32_t)mp3StreamReadBits(bs, 8, st);
		if (st->streamErr != 0)
			return 0;
	}
}

static __attribute__((noinline, noclone)) uint32_t
mp3ResGetBitPos(Mp3State *st)
{
	return st->resBitPos;
}

static __attribute__((noinline, noclone)) uint32_t
mp3ResPeekBits(int32_t numBits, Mp3State *st)
{
	if (st->mainBufErr != 0)
		return 0;
	if (numBits <= 0)
		return 0;

	uint32_t bitsLeft = st->resBitsLeft;
	uint32_t readIdx = st->resReadIdx;
	uint32_t acc = 0;

	for (;;) {
		uint32_t cur = st->resRing[readIdx++ & 0xFFFU] & (0xFFU >> (8U - bitsLeft));
		if ((uint32_t)numBits <= bitsLeft)
			return acc | (cur >> (bitsLeft - (uint32_t)numBits));
		numBits -= (int32_t)bitsLeft;
		acc |= cur << numBits;
		bitsLeft = 8U;
		if (readIdx > st->resWriteIdx) {
			st->mainBufErr = -1;
			return 0;
		}
		if (numBits == 0)
			return acc;
	}
}

static __attribute__((noinline, noclone)) uint32_t
mp3ResReadBits(int32_t numBits, Mp3State *st)
{
	if (st->mainBufErr != 0)
		return 0;
	if (numBits <= 0)
		return 0;

	uint32_t readIdx = st->resReadIdx;
	uint32_t bitsLeft = st->resBitsLeft;
	st->resBitPos += (uint32_t)numBits;

	uint32_t cur = st->resRing[readIdx & 0xFFFU] & (0xFFU >> (8U - bitsLeft));
	if ((uint32_t)numBits <= bitsLeft) {
		uint32_t rem = bitsLeft - (uint32_t)numBits;
		st->resBitsLeft = rem;
		return cur >> rem;
	}

	uint32_t nextIdx = readIdx + 1U;
	uint32_t writeIdx = st->resWriteIdx;
	if (nextIdx + (((uint32_t)numBits - bitsLeft - 1U) >> 3) > writeIdx) {
		st->resReadIdx = writeIdx + 1U;
		st->mainBufErr = -1;
		st->resBitsLeft = 8U;
		return 0;
	}

	uint32_t acc = 0;
	uint32_t remBits = (uint32_t)numBits;
	for (;;) {
		remBits -= bitsLeft;
		acc |= cur << remBits;
		bitsLeft = 8U;
		cur = st->resRing[nextIdx & 0xFFFU] & 0xFFU;
		if (remBits <= 8U)
			break;
		++nextIdx;
	}
	uint32_t rem = 8U - remBits;
	st->resBitsLeft = rem;
	st->resReadIdx = nextIdx;
	return acc | (cur >> rem);
}

static __attribute__((noinline, noclone)) void
mp3ResSkipBits(uint32_t numBits, Mp3State *st)
{
	if (st->mainBufErr != 0)
		return;

	uint32_t bitsLeft = st->resBitsLeft;
	st->resBitPos += numBits;
	if (numBits <= bitsLeft) {
		st->resBitsLeft = bitsLeft - numBits;
		return;
	}

	uint32_t diff = numBits - bitsLeft;
	uint32_t wholeBytes = (diff - 1U) >> 3;
	uint32_t writeIdx = st->resWriteIdx;
	uint32_t nextIdx = st->resReadIdx + 1U + wholeBytes;
	st->resReadIdx = nextIdx;
	if (nextIdx > writeIdx) {
		st->resReadIdx = writeIdx + 1U;
		st->resBitsLeft = 8U;
		st->mainBufErr = -1;
	} else {
		st->resBitsLeft = 8U - (diff - 8U * wholeBytes);
	}
}

static __attribute__((noinline, noclone)) uint32_t
mp3ResReadBit(Mp3State *st)
{
	if (st->mainBufErr != 0)
		return 0;

	uint32_t bitsLeft = st->resBitsLeft;
	++st->resBitPos;
	if (bitsLeft != 0U) {
		uint32_t readIdx = st->resReadIdx;
		uint32_t rem = bitsLeft - 1U;
		uint32_t mask = 0xFFU >> (8U - bitsLeft);
		uint32_t val = st->resRing[readIdx & 0xFFFU];
		st->resBitsLeft = rem;
		return (val & mask) >> rem;
	}

	uint32_t nextIdx = st->resReadIdx + 1U;
	st->resBitsLeft = 8U;
	st->resReadIdx = nextIdx;
	if (nextIdx <= st->resWriteIdx) {
		uint32_t val = st->resRing[nextIdx & 0xFFFU];
		st->resBitsLeft = 7U;
		return (val & 0xFFU) >> 7;
	}
	st->mainBufErr = -1;
	return 0;
}

static __attribute__((noinline, noclone)) void
mp3ResPushByte(uint32_t byteVal, Mp3State *st)
{
	uint32_t writeIdx = st->resWriteIdx;
	st->resRing[writeIdx & 0xFFFU] = byteVal;
	st->resWriteIdx = writeIdx + 1U;
}

static __attribute__((noinline, noclone)) void
mp3ResRewindBits(int32_t numBits, Mp3State *st)
{
	uint32_t newBitsLeft = (uint32_t)numBits + st->resBitsLeft;
	st->resBitPos -= (uint32_t)numBits;
	st->resBitsLeft = newBitsLeft;
	if (newBitsLeft > 7U) {
		st->resBitsLeft = newBitsLeft & 7U;
		st->resReadIdx = st->resReadIdx - 1U - ((newBitsLeft - 8U) >> 3);
	}
}

static __attribute__((noinline, noclone)) void
mp3ResDiscardBytes(int32_t numBytes, Mp3State *st)
{
	st->resBitPos -= (uint32_t)(8 * numBytes);
	st->resReadIdx -= (uint32_t)numBytes;
	st->resWriteIdx -= (uint32_t)numBytes;
}

static __attribute__((noinline, noclone)) void
mp3CrcUpdate(uint32_t data, int32_t length, uint32_t *crc)
{
	uint32_t c = *crc;
	uint32_t mask = 1U << length;
	for (;;) {
		mask >>= 1;
		if (mask == 0U)
			break;
		int32_t topZero = (c & 0x8000U) == 0U;
		c <<= 1;
		if (topZero != ((data & mask) == 0U))
			c ^= 0x8005U;
		*crc = c;
	}
	*crc = c & 0xFFFFU;
}

static __attribute__((noinline, noclone)) int32_t
mp3InitFrameParams(Mp3FrameParams *fp)
{
	Mp3Header *hdr = fp->hdr;
	fp->sblimit = 32;
	fp->channels = (hdr->mode == 3) ? 1 : 2;
	if (hdr->mode == 1) {
		uint32_t ext = (uint32_t)hdr->modeExt;
		uint32_t lay = (uint32_t)(hdr->layer - 1);
		if (lay > 2U || ext > 3U)
			return -100;
		fp->jsBound = kMp3JsBound[lay][ext];
	} else {
		fp->jsBound = 32;
	}
	return 0;
}

static __attribute__((noinline, noclone)) void
mp3ReadCrc16(Mp3BitStream *bs, uint32_t *outCrc, Mp3State *st)
{
	*outCrc = (uint32_t)mp3StreamReadBits(bs, 16, st);
}

static __attribute__((noinline, noclone)) uint32_t
mp3CalcFrameBytes(const Mp3FrameParams *fp)
{
	const Mp3Header *hdr = fp->hdr;
	int32_t isMpeg2 = (hdr->version != 1);
	uint32_t bytes = (uint32_t)((float)(144U * kMp3BitrateKbps[hdr->version][2][hdr->bitrateIdx]) /
				    kMp3SampleRatesKhz[4 * hdr->version + hdr->sampleRateIdx]);
	if (isMpeg2)
		bytes >>= 1;
	if (hdr->padding != 0)
		++bytes;
	return bytes;
}

static __attribute__((noinline, noclone)) int32_t
mp3ParseFrameHeader(Mp3BitStream *bs, Mp3FrameParams *fp, int32_t isMpeg25, Mp3State *st)
{
	Mp3Header *hdr = fp->hdr;

	mp3CheckDebugFail(st, 200);
	int32_t idBit = mp3StreamReadBit(bs, st);
	hdr->version = (isMpeg25 != 0) ? 2 : idBit;

	mp3CheckDebugFail(st, 201);
	hdr->layer = 4 - mp3StreamReadBits(bs, 2, st);

	mp3CheckDebugFail(st, 202);
	hdr->errorProtection = (mp3StreamReadBit(bs, st) == 0) ? 1 : 0;

	mp3CheckDebugFail(st, 203);
	hdr->bitrateIdx = mp3StreamReadBits(bs, 4, st);

	mp3CheckDebugFail(st, 204);
	hdr->sampleRateIdx = mp3StreamReadBits(bs, 2, st);

	mp3CheckDebugFail(st, 205);
	hdr->padding = mp3StreamReadBit(bs, st);

	mp3CheckDebugFail(st, 206);
	hdr->privateExt = mp3StreamReadBit(bs, st);

	mp3CheckDebugFail(st, 207);
	hdr->mode = mp3StreamReadBits(bs, 2, st);

	mp3CheckDebugFail(st, 208);
	hdr->modeExt = mp3StreamReadBits(bs, 2, st);

	mp3CheckDebugFail(st, 209);
	hdr->copyright = mp3StreamReadBit(bs, st);

	mp3CheckDebugFail(st, 210);
	hdr->original = mp3StreamReadBit(bs, st);

	mp3CheckDebugFail(st, 211);
	uint32_t emph = (uint32_t)mp3StreamReadBits(bs, 2, st);
	hdr->emphasis = (int32_t)emph;

	if (st->streamErr != 0)
		return -1600;

	if (hdr->layer == 3 && (uint32_t)hdr->version <= 2U &&
	    (uint32_t)(hdr->bitrateIdx - 1) <= 13U && (uint32_t)hdr->sampleRateIdx <= 2U &&
	    emph <= 3U && (uint32_t)hdr->mode <= 3U && (uint32_t)hdr->modeExt <= 3U) {
		st->frameBytes = (int32_t)mp3CalcFrameBytes(fp);
		return 0;
	}
	return -200;
}

static __attribute__((noinline, noclone)) int32_t
mp3CalcMainDataBytes(const Mp3Header *hdr, int32_t channels)
{
	int32_t frameBytes =
		(int32_t)((float)(144U * kMp3BitrateKbps[hdr->version][2][hdr->bitrateIdx]) /
			  kMp3SampleRatesKhz[4 * hdr->version + hdr->sampleRateIdx]);
	int32_t dataBytes;
	if (hdr->version == 1) {
		dataBytes = (channels == 1) ? (frameBytes - 17) : (frameBytes - 32);
	} else {
		frameBytes /= 2;
		dataBytes = (channels == 1) ? (frameBytes - 9) : (frameBytes - 17);
	}
	if (hdr->padding != 0)
		++dataBytes;
	return (hdr->errorProtection != 0) ? (dataBytes - 6) : (dataBytes - 4);
}

static __attribute__((noinline, noclone)) void
mp3ComputeSideInfoCrc(const Mp3FrameParams *fp, const Mp3SideInfo *si, uint32_t *crc)
{
	const Mp3Header *hdr = fp->hdr;
	int32_t channels = fp->channels;

	*crc = 0xFFFFU;
	mp3CrcUpdate((uint32_t)hdr->bitrateIdx, 4, crc);
	mp3CrcUpdate((uint32_t)hdr->sampleRateIdx, 2, crc);
	mp3CrcUpdate((uint32_t)hdr->padding, 1, crc);
	mp3CrcUpdate((uint32_t)hdr->privateExt, 1, crc);
	mp3CrcUpdate((uint32_t)hdr->mode, 2, crc);
	mp3CrcUpdate((uint32_t)hdr->modeExt, 2, crc);
	mp3CrcUpdate((uint32_t)hdr->copyright, 1, crc);
	mp3CrcUpdate((uint32_t)hdr->original, 1, crc);
	mp3CrcUpdate((uint32_t)hdr->emphasis, 2, crc);

	if (hdr->version == 1) {
		mp3CrcUpdate((uint32_t)si->mainDataBegin, 9, crc);
		mp3CrcUpdate((uint32_t)si->privateBits, (channels == 1) ? 5 : 3, crc);
		for (int32_t ch = 0; ch < channels; ++ch) {
			for (int32_t band = 0; band < 4; ++band)
				mp3CrcUpdate((uint32_t)si->ch[ch].scfsi[band], 1, crc);
		}
		for (int32_t gr = 0; gr < 2; ++gr) {
			for (int32_t ch = 0; ch < channels; ++ch) {
				const Mp3GranuleInfo *gi = &si->ch[ch].gr[gr];
				mp3CrcUpdate((uint32_t)gi->part23Length, 12, crc);
				mp3CrcUpdate((uint32_t)gi->bigValues, 9, crc);
				mp3CrcUpdate((uint32_t)gi->globalGain, 8, crc);
				mp3CrcUpdate((uint32_t)gi->scalefacCompress, 4, crc);
				mp3CrcUpdate((uint32_t)gi->windowSwitching, 1, crc);
				if (gi->windowSwitching != 0) {
					mp3CrcUpdate((uint32_t)gi->blockType, 2, crc);
					mp3CrcUpdate((uint32_t)gi->mixedBlock, 1, crc);
					mp3CrcUpdate((uint32_t)gi->tableSelect[0], 5, crc);
					mp3CrcUpdate((uint32_t)gi->tableSelect[1], 5, crc);
					for (int32_t w = 0; w < 3; ++w)
						mp3CrcUpdate((uint32_t)gi->subblockGain[w], 3, crc);
				} else {
					for (int32_t r = 0; r < 3; ++r)
						mp3CrcUpdate((uint32_t)gi->tableSelect[r], 5, crc);
					mp3CrcUpdate((uint32_t)gi->region0Count, 4, crc);
					mp3CrcUpdate((uint32_t)gi->region1Count, 3, crc);
				}
				mp3CrcUpdate((uint32_t)gi->preflag, 1, crc);
				mp3CrcUpdate((uint32_t)gi->scalefacScale, 1, crc);
				mp3CrcUpdate((uint32_t)gi->count1TableSelect, 1, crc);
			}
		}
	} else {
		mp3CrcUpdate((uint32_t)si->mainDataBegin, 8, crc);
		mp3CrcUpdate((uint32_t)si->privateBits, (channels == 1) ? 1 : 2, crc);
		for (int32_t ch = 0; ch < channels; ++ch) {
			const Mp3GranuleInfo *gi = &si->ch[ch].gr[0];
			mp3CrcUpdate((uint32_t)gi->part23Length, 12, crc);
			mp3CrcUpdate((uint32_t)gi->bigValues, 9, crc);
			mp3CrcUpdate((uint32_t)gi->globalGain, 8, crc);
			mp3CrcUpdate((uint32_t)gi->scalefacCompress, 9, crc);
			mp3CrcUpdate((uint32_t)gi->windowSwitching, 1, crc);
			if (gi->windowSwitching != 0) {
				mp3CrcUpdate((uint32_t)gi->blockType, 2, crc);
				mp3CrcUpdate((uint32_t)gi->mixedBlock, 1, crc);
				mp3CrcUpdate((uint32_t)gi->tableSelect[0], 5, crc);
				mp3CrcUpdate((uint32_t)gi->tableSelect[1], 5, crc);
				for (int32_t w = 0; w < 3; ++w)
					mp3CrcUpdate((uint32_t)gi->subblockGain[w], 3, crc);
			} else {
				for (int32_t r = 0; r < 3; ++r)
					mp3CrcUpdate((uint32_t)gi->tableSelect[r], 5, crc);
				mp3CrcUpdate((uint32_t)gi->region0Count, 4, crc);
				mp3CrcUpdate((uint32_t)gi->region1Count, 3, crc);
			}
			mp3CrcUpdate((uint32_t)gi->scalefacScale, 1, crc);
			mp3CrcUpdate((uint32_t)gi->count1TableSelect, 1, crc);
		}
	}
}

static __attribute__((noinline, noclone)) int32_t
mp3ReadSideInfo(Mp3BitStream *bs, Mp3SideInfo *si, const Mp3FrameParams *fp, Mp3State *st)
{
	int32_t channels = fp->channels;

	if (fp->hdr->version != 1) {
		mp3CheckDebugFail(st, 231);
		si->mainDataBegin = mp3StreamReadBits(bs, 8, st);
		if (channels == 1) {
			mp3CheckDebugFail(st, 232);
			si->privateBits = mp3StreamReadBits(bs, 1, st);
		} else {
			mp3CheckDebugFail(st, 233);
			si->privateBits = mp3StreamReadBits(bs, 2, st);
		}
		for (int32_t ch = 0; ch < channels; ++ch) {
			Mp3GranuleInfo *gi = &si->ch[ch].gr[0];
			mp3CheckDebugFail(st, 234);
			gi->part23Length = mp3StreamReadBits(bs, 12, st);
			mp3CheckDebugFail(st, 235);
			gi->bigValues = mp3StreamReadBits(bs, 9, st);
			mp3CheckDebugFail(st, 236);
			gi->globalGain = mp3StreamReadBits(bs, 8, st);
			mp3CheckDebugFail(st, 237);
			gi->scalefacCompress = mp3StreamReadBits(bs, 9, st);
			mp3CheckDebugFail(st, 238);
			gi->windowSwitching = mp3StreamReadBit(bs, st);
			if (gi->windowSwitching != 0) {
				mp3CheckDebugFail(st, 239);
				gi->blockType = mp3StreamReadBits(bs, 2, st);
				mp3CheckDebugFail(st, 240);
				gi->mixedBlock = mp3StreamReadBit(bs, st);
				mp3CheckDebugFail(st, 241);
				gi->tableSelect[0] = mp3StreamReadBits(bs, 5, st);
				mp3CheckDebugFail(st, 241);
				gi->tableSelect[1] = mp3StreamReadBits(bs, 5, st);
				for (int32_t w = 0; w < 3; ++w) {
					mp3CheckDebugFail(st, 242);
					gi->subblockGain[w] = mp3StreamReadBits(bs, 3, st);
				}
				if (st->streamErr != 0)
					return -1600;
				if (gi->blockType == 0)
					return -300;
				if (gi->blockType == 2 && gi->mixedBlock == 0) {
					gi->region0Count = 8;
					gi->region1Count = 12;
				} else {
					gi->region0Count = 7;
					gi->region1Count = 13;
				}
			} else {
				for (int32_t r = 0; r < 3; ++r) {
					mp3CheckDebugFail(st, 243);
					gi->tableSelect[r] = mp3StreamReadBits(bs, 5, st);
				}
				mp3CheckDebugFail(st, 244);
				gi->region0Count = mp3StreamReadBits(bs, 4, st);
				mp3CheckDebugFail(st, 245);
				gi->region1Count = mp3StreamReadBits(bs, 3, st);
				gi->blockType = 0;
			}
			mp3CheckDebugFail(st, 246);
			gi->scalefacScale = mp3StreamReadBit(bs, st);
			mp3CheckDebugFail(st, 247);
			gi->count1TableSelect = mp3StreamReadBit(bs, st);
		}
	} else {
		mp3CheckDebugFail(st, 212);
		si->mainDataBegin = mp3StreamReadBits(bs, 9, st);
		if (channels == 1) {
			mp3CheckDebugFail(st, 213);
			si->privateBits = mp3StreamReadBits(bs, 5, st);
		} else {
			mp3CheckDebugFail(st, 214);
			si->privateBits = mp3StreamReadBits(bs, 3, st);
		}
		for (int32_t ch = 0; ch < channels; ++ch) {
			for (int32_t b = 0; b < 4; ++b) {
				mp3CheckDebugFail(st, 215);
				si->ch[ch].scfsi[b] = mp3StreamReadBit(bs, st);
			}
		}
		for (int32_t gr = 0; gr < 2; ++gr) {
			for (int32_t ch = 0; ch < channels; ++ch) {
				Mp3GranuleInfo *gi = &si->ch[ch].gr[gr];
				mp3CheckDebugFail(st, 216);
				gi->part23Length = mp3StreamReadBits(bs, 12, st);
				mp3CheckDebugFail(st, 217);
				gi->bigValues = mp3StreamReadBits(bs, 9, st);
				mp3CheckDebugFail(st, 218);
				gi->globalGain = mp3StreamReadBits(bs, 8, st);
				mp3CheckDebugFail(st, 219);
				gi->scalefacCompress = mp3StreamReadBits(bs, 4, st);
				mp3CheckDebugFail(st, 220);
				gi->windowSwitching = mp3StreamReadBit(bs, st);
				if (gi->windowSwitching != 0) {
					mp3CheckDebugFail(st, 221);
					gi->blockType = mp3StreamReadBits(bs, 2, st);
					mp3CheckDebugFail(st, 222);
					gi->mixedBlock = mp3StreamReadBit(bs, st);
					mp3CheckDebugFail(st, 223);
					gi->tableSelect[0] = mp3StreamReadBits(bs, 5, st);
					mp3CheckDebugFail(st, 223);
					gi->tableSelect[1] = mp3StreamReadBits(bs, 5, st);
					for (int32_t w = 0; w < 3; ++w) {
						mp3CheckDebugFail(st, 224);
						gi->subblockGain[w] = mp3StreamReadBits(bs, 3, st);
					}
					if (st->streamErr != 0)
						return -1600;
					if (gi->blockType == 0)
						return -300;
					if (gi->blockType == 2 && gi->mixedBlock == 0) {
						gi->region0Count = 8;
						gi->region1Count = 12;
					} else {
						gi->region0Count = 7;
						gi->region1Count = 13;
					}
				} else {
					for (int32_t r = 0; r < 3; ++r) {
						mp3CheckDebugFail(st, 225);
						gi->tableSelect[r] = mp3StreamReadBits(bs, 5, st);
					}
					mp3CheckDebugFail(st, 226);
					gi->region0Count = mp3StreamReadBits(bs, 4, st);
					mp3CheckDebugFail(st, 227);
					gi->region1Count = mp3StreamReadBits(bs, 3, st);
					gi->blockType = 0;
				}
				mp3CheckDebugFail(st, 228);
				gi->preflag = mp3StreamReadBit(bs, st);
				mp3CheckDebugFail(st, 229);
				gi->scalefacScale = mp3StreamReadBit(bs, st);
				mp3CheckDebugFail(st, 230);
				gi->count1TableSelect = mp3StreamReadBit(bs, st);
			}
		}
	}

	if (st->streamErr != 0)
		return -1600;
	return 0;
}

static __attribute__((noinline, noclone)) int32_t
mp3ReadScaleFactorsMpeg1(Mp3ScaleFactors *sf, const Mp3SideInfo *si, int32_t gr, int32_t ch,
			 Mp3State *st)
{
	const Mp3GranuleInfo *gi = &si->ch[ch].gr[gr];
	uint32_t sfc = (uint32_t)gi->scalefacCompress;
	if (sfc > 15U)
		return -400;

	if (gi->windowSwitching != 0 && gi->blockType == 2) {
		if (gi->mixedBlock != 0) {
			for (int32_t sfb = 0; sfb < 8; ++sfb) {
				int32_t bits = (st->debugFailPoint == 248) ? 4 : kMp3Slen[0][sfc];
				mp3CheckDebugFail(st, 248);
				sf[ch].l[sfb] = (int16_t)mp3ResReadBits(bits, st);
			}
			for (int32_t sfb = 3; sfb < 6; ++sfb) {
				for (int32_t w = 0; w < 3; ++w) {
					int32_t bits =
						(st->debugFailPoint == 249) ? 4 : kMp3Slen[0][sfc];
					mp3CheckDebugFail(st, 249);
					sf[ch].s[w][sfb] = (int16_t)mp3ResReadBits(bits, st);
				}
			}
			for (int32_t sfb = 6; sfb < 12; ++sfb) {
				for (int32_t w = 0; w < 3; ++w) {
					int32_t bits =
						(st->debugFailPoint == 250) ? 4 : kMp3Slen[1][sfc];
					mp3CheckDebugFail(st, 250);
					sf[ch].s[w][sfb] = (int16_t)mp3ResReadBits(bits, st);
				}
			}
		} else {
			for (int32_t part = 0; part < 2; ++part) {
				int32_t start = kMp3ShortSfBand[part];
				int32_t end = kMp3ShortSfBand[part + 1];
				for (int32_t sfb = start; sfb < end; ++sfb) {
					for (int32_t w = 0; w < 3; ++w) {
						int32_t bits = (st->debugFailPoint == 251)
								       ? 4
								       : kMp3Slen[part][sfc];
						mp3CheckDebugFail(st, 251);
						sf[ch].s[w][sfb] =
							(int16_t)mp3ResReadBits(bits, st);
					}
				}
			}
		}
		sf[ch].s[0][12] = 0;
		sf[ch].s[1][12] = 0;
		sf[ch].s[2][12] = 0;
	} else {
		for (int32_t band = 0; band < 4; ++band) {
			if (si->ch[ch].scfsi[band] == 0 || gr == 0) {
				int32_t start = kMp3ScfsiBand[band];
				int32_t end = kMp3ScfsiBand[band + 1];
				int32_t slenRow = (band > 1) ? 1 : 0;
				for (int32_t sfb = start; sfb < end; ++sfb) {
					int32_t bits = (st->debugFailPoint == 252)
							       ? 4
							       : kMp3Slen[slenRow][sfc];
					mp3CheckDebugFail(st, 252);
					sf[ch].l[sfb] = (int16_t)mp3ResReadBits(bits, st);
				}
			}
		}
		sf[ch].l[21] = 0;
	}

	return (st->mainBufErr != 0) ? -1500 : 0;
}

static __attribute__((noinline, noclone)) int32_t
mp3ReadScaleFactorsMpeg2(Mp3ScaleFactors *sf, Mp3SideInfo *si, int32_t gr, int32_t ch, Mp3State *st)
{
	Mp3GranuleInfo *gi = &si->ch[ch].gr[gr];
	uint32_t sfc = (uint32_t)gi->scalefacCompress;
	uint32_t slen[4] = { 0, 0, 0, 0 };
	int32_t blockIdx = 0;
	if (gi->blockType == 2)
		blockIdx = (gi->mixedBlock != 0) ? 2 : 1;

	int32_t isIntCh = ((st->frameParams.hdr->modeExt & 1) != 0 && ch == 1);
	int32_t nrRow;

	if (!isIntCh) {
		if (sfc < 400U) {
			slen[0] = (sfc >> 4) / 5U;
			slen[1] = (sfc >> 4) % 5U;
			slen[2] = (sfc >> 2) & 3U;
			slen[3] = sfc & 3U;
			gi->preflag = 0;
			nrRow = 0;
		} else if (sfc < 500U) {
			slen[0] = (sfc - 400U) / 20U;
			slen[1] = ((sfc - 400U) >> 2) % 5U;
			slen[2] = sfc & 3U;
			slen[3] = 0U;
			gi->preflag = 0;
			nrRow = 1;
		} else if (sfc < 512U) {
			slen[0] = (sfc - 500U) / 3U;
			slen[1] = (sfc - 500U) % 3U;
			slen[2] = 0U;
			slen[3] = 0U;
			gi->preflag = 1;
			nrRow = 2;
		} else {
			return -500;
		}
	} else {
		uint32_t intensity = sfc >> 1;
		if (intensity < 180U) {
			slen[0] = intensity / 36U;
			slen[1] = (intensity % 36U) / 6U;
			slen[2] = (intensity % 36U) % 6U;
			slen[3] = 0U;
			gi->preflag = 0;
			nrRow = 3;
		} else if (intensity < 244U) {
			slen[0] = (intensity - 180U) >> 4;
			slen[1] = ((intensity - 180U) >> 2) & 3U;
			slen[2] = intensity & 3U;
			slen[3] = 0U;
			gi->preflag = 0;
			nrRow = 4;
		} else if (intensity < 256U) {
			slen[0] = (intensity - 244U) / 3U;
			slen[1] = (intensity - 244U) % 3U;
			slen[2] = 0U;
			slen[3] = 0U;
			gi->preflag = 0;
			nrRow = 5;
		} else {
			return -500;
		}
	}

	for (int32_t i = 0; i < 54; ++i)
		st->mpeg2SfTmp[i] = 0;

	int32_t pos = 0;
	for (int32_t part = 0; part < 4; ++part) {
		int32_t count = kMp3Mpeg2SfNrTab[nrRow][blockIdx][part];
		for (int32_t i = 0; i < count; ++i) {
			if (slen[part] != 0U) {
				mp3CheckDebugFail(st, 253);
				st->mpeg2SfTmp[pos] = (int16_t)mp3ResReadBits((int32_t)slen[part], st);
			} else {
				st->mpeg2SfTmp[pos] = 0;
			}
			++pos;
		}
	}

	if (st->mainBufErr != 0)
		return -1500;

	if (gi->windowSwitching != 0 && gi->blockType == 2) {
		if (gi->mixedBlock != 0) {
			for (int32_t sfb = 0; sfb < 8; ++sfb)
				sf[ch].l[sfb] = st->mpeg2SfTmp[sfb];
			int32_t idx = 8;
			for (int32_t sfb = 3; sfb < 12; ++sfb) {
				sf[ch].s[0][sfb] = st->mpeg2SfTmp[idx++];
				sf[ch].s[1][sfb] = st->mpeg2SfTmp[idx++];
				sf[ch].s[2][sfb] = st->mpeg2SfTmp[idx++];
			}
		} else {
			int32_t idx = 0;
			for (int32_t sfb = 0; sfb < 12; ++sfb) {
				sf[ch].s[0][sfb] = st->mpeg2SfTmp[idx++];
				sf[ch].s[1][sfb] = st->mpeg2SfTmp[idx++];
				sf[ch].s[2][sfb] = st->mpeg2SfTmp[idx++];
			}
		}
		sf[ch].s[0][12] = 0;
		sf[ch].s[1][12] = 0;
		sf[ch].s[2][12] = 0;
	} else {
		for (int32_t sfb = 0; sfb < 21; ++sfb)
			sf[ch].l[sfb] = st->mpeg2SfTmp[sfb];
		sf[ch].l[21] = 0;
	}
	return 0;
}

static __attribute__((noinline, noclone)) int32_t
mp3HuffDecodeBigValues(int32_t *out, int32_t count, int32_t tableIdx, Mp3State *st)
{
	if (count <= 0)
		return 0;

	int32_t pairs = count >> 1;
	const Mp3HuffTableDesc *desc = &kMp3HuffDesc[tableIdx];
	const uint32_t *baseTbl = desc->table;
	uint32_t linbits = desc->linbits;
	uint32_t tblSz = kMp3HuffTableSize[tableIdx];

	switch (desc->mode) {
	case 0U:
		for (int32_t i = 0; i < pairs; ++i) {
			out[0] = 0;
			out[1] = 0;
			out += 2;
		}
		break;
	case 1U:
		for (int32_t i = 0; i < pairs; ++i) {
			uint32_t peekBits = baseTbl[0] & 0xFFU;
			mp3CheckDebugFail(st, 300);
			uint32_t code = mp3ResPeekBits((int32_t)peekBits, st);
			if (code + 1U >= tblSz)
				return -900;
			uint32_t entry = baseTbl[code + 1U];
			mp3CheckDebugFail(st, 301);
			mp3ResSkipBits(entry >> 24, st);
			int32_t x = (int32_t)((entry >> 8) & 0xFFU);
			int32_t y = (int32_t)((entry >> 16) & 0xFFU);
			if (x != 0) {
				mp3CheckDebugFail(st, 302);
				if (mp3ResReadBit(st) != 0U)
					x = -x;
			}
			if (y != 0) {
				mp3CheckDebugFail(st, 303);
				if (mp3ResReadBit(st) != 0U)
					y = -y;
			}
			out[0] = x;
			out[1] = y;
			out += 2;
		}
		break;
	case 2U: {
		int32_t err = 0;
		for (int32_t i = 0; i < pairs; ++i) {
			const uint32_t *node = baseTbl;
			uint32_t entry;
			for (;;) {
				uint32_t peekBits = node[0] & 0xFFU;
				mp3CheckDebugFail(st, 304);
				uint32_t idx = mp3ResPeekBits((int32_t)peekBits, st) + 1U;
				entry = node[idx];
				if ((uint32_t)((node - baseTbl) + (int32_t)idx) >= tblSz) {
					err = -900;
					break;
				}
				if ((entry >> 24) != 0U)
					break;
				node += entry;
				mp3CheckDebugFail(st, 305);
				mp3ResSkipBits(peekBits, st);
			}
			mp3CheckDebugFail(st, 306);
			mp3ResSkipBits(entry >> 24, st);
			int32_t x = (int32_t)((entry >> 8) & 0xFFU);
			int32_t y = (int32_t)((entry >> 16) & 0xFFU);
			if (x != 0) {
				mp3CheckDebugFail(st, 307);
				if (mp3ResReadBit(st) != 0U)
					x = -x;
			}
			if (y != 0) {
				mp3CheckDebugFail(st, 308);
				if (mp3ResReadBit(st) != 0U)
					y = -y;
			}
			out[0] = x;
			out[1] = y;
			out += 2;
		}
		if (err != 0)
			return err;
		break;
	}
	case 3U: {
		int32_t err = 0;
		for (int32_t i = 0; i < pairs; ++i) {
			const uint32_t *node = baseTbl;
			uint32_t entry;
			for (;;) {
				uint32_t peekBits = node[0] & 0xFFU;
				uint32_t idx = mp3ResPeekBits((int32_t)peekBits, st) + 1U;
				entry = node[idx];
				if ((uint32_t)((node - baseTbl) + (int32_t)idx) >= tblSz) {
					err = -900;
					break;
				}
				if ((entry >> 24) != 0U)
					break;
				node += entry;
				mp3CheckDebugFail(st, 310);
				mp3ResSkipBits(peekBits, st);
			}
			mp3CheckDebugFail(st, 311);
			mp3ResSkipBits(entry >> 24, st);
			int32_t x = (int32_t)((entry >> 8) & 0xFFU);
			int32_t y = (int32_t)((entry >> 16) & 0xFFU);
			if (x == 15) {
				mp3CheckDebugFail(st, 312);
				x = (int32_t)mp3ResReadBits((int32_t)linbits, st) + 15;
			}
			if (x != 0) {
				mp3CheckDebugFail(st, 313);
				if (mp3ResReadBit(st) != 0U)
					x = -x;
			}
			if (y == 15) {
				mp3CheckDebugFail(st, 314);
				y = (int32_t)mp3ResReadBits((int32_t)linbits, st) + 15;
			}
			if (y != 0) {
				mp3CheckDebugFail(st, 315);
				if (mp3ResReadBit(st) != 0U)
					y = -y;
			}
			out[0] = x;
			out[1] = y;
			out += 2;
		}
		if (err != 0)
			return err;
		break;
	}
	default:
		return -900;
	}

	return (st->mainBufErr != 0) ? -1500 : 0;
}

static __attribute__((noinline, noclone)) int32_t
mp3HuffDecodeCount1(int32_t *out, int32_t maxSamples, int32_t availBits, int32_t tableSel,
		    Mp3State *st)
{
	int32_t maxQuads = maxSamples >> 2;
	int32_t quadsDone = 0;
	int32_t lastNonZeroQuad = -1;
	uint32_t lastMask = 15U;
	int32_t remBits = availBits;

	if (tableSel == 0) {
		for (int32_t q = 0; q < maxQuads && remBits > 0; ++q) {
			mp3CheckDebugFail(st, 321);
			uint32_t code = mp3ResPeekBits(6, st);
			uint32_t bits = kMp3Count1HuffA[code][0];
			uint32_t mask = kMp3Count1HuffA[code][1];
			mp3CheckDebugFail(st, 322);
			mp3ResSkipBits(bits, st);
			remBits -= (int32_t)bits;

			int32_t v = 0, w = 0, x = 0, y = 0;
			if (mask != 0U) {
				if ((mask & 8U) != 0U) {
					--remBits;
					mp3CheckDebugFail(st, 323);
					v = (mp3ResReadBit(st) != 0U) ? -1 : 1;
				}
				if ((mask & 4U) != 0U) {
					--remBits;
					mp3CheckDebugFail(st, 324);
					w = (mp3ResReadBit(st) != 0U) ? -1 : 1;
				}
				if ((mask & 2U) != 0U) {
					--remBits;
					mp3CheckDebugFail(st, 325);
					x = (mp3ResReadBit(st) != 0U) ? -1 : 1;
				}
				lastMask = mask;
				lastNonZeroQuad = q;
				if ((mask & 1U) != 0U) {
					--remBits;
					mp3CheckDebugFail(st, 326);
					y = (mp3ResReadBit(st) != 0U) ? -1 : 1;
				}
			}
			out[q * 4 + 0] = v;
			out[q * 4 + 1] = w;
			out[q * 4 + 2] = x;
			out[q * 4 + 3] = y;
			quadsDone = q + 1;
		}
		if (st->mainBufErr != 0)
			return -1;
		if (remBits < 0) {
			int32_t prev = quadsDone - 1;
			out[prev * 4 + 0] = 0;
			out[prev * 4 + 1] = 0;
			out[prev * 4 + 2] = 0;
			out[prev * 4 + 3] = 0;
		}
	} else {
		for (int32_t q = 0; q < maxQuads && remBits > 3; ++q) {
			mp3CheckDebugFail(st, 316);
			uint32_t mask = mp3ResReadBits(4, st) ^ 0x0FU;
			remBits -= 4;

			int32_t v = 0, w = 0, x = 0, y = 0;
			if (mask != 0U) {
				if ((mask & 8U) != 0U) {
					--remBits;
					mp3CheckDebugFail(st, 317);
					v = (mp3ResReadBit(st) != 0U) ? -1 : 1;
				}
				if ((mask & 4U) != 0U) {
					--remBits;
					mp3CheckDebugFail(st, 318);
					w = (mp3ResReadBit(st) != 0U) ? -1 : 1;
				}
				if ((mask & 2U) != 0U) {
					--remBits;
					mp3CheckDebugFail(st, 319);
					x = (mp3ResReadBit(st) != 0U) ? -1 : 1;
				}
				lastMask = mask;
				lastNonZeroQuad = q;
				if ((mask & 1U) != 0U) {
					--remBits;
					mp3CheckDebugFail(st, 320);
					y = (mp3ResReadBit(st) != 0U) ? -1 : 1;
				}
			}
			out[q * 4 + 0] = v;
			out[q * 4 + 1] = w;
			out[q * 4 + 2] = x;
			out[q * 4 + 3] = y;
			quadsDone = q + 1;
		}
		if (remBits < 0) {
			int32_t prev = quadsDone - 1;
			out[prev * 4 + 0] = 0;
			out[prev * 4 + 1] = 0;
			out[prev * 4 + 2] = 0;
			out[prev * 4 + 3] = 0;
		}
		if (st->mainBufErr != 0)
			return -1;
	}

	int32_t total = 4 * (lastNonZeroQuad + 1);
	if ((lastMask & 3U) == 0U)
		total -= 2;
	return total;
}

static __attribute__((noinline, noclone)) int32_t
mp3DecodeHuffmanGranule(const Mp3SideInfo *si, int32_t ch, int32_t gr, uint32_t part2Start,
			Mp3State *st)
{
	const Mp3GranuleInfo *gi = &si->ch[ch].gr[gr];
	int32_t sfIdx = 3 * st->frameParams.hdr->version + st->frameParams.hdr->sampleRateIdx;
	int32_t reg1Start, reg2Start;

	if (gi->windowSwitching != 0 && gi->blockType == 2) {
		reg1Start = 3 * kMp3SfBandIndex[sfIdx][26];
		reg2Start = 576;
	} else {
		uint32_t r0 = (uint32_t)gi->region0Count;
		uint32_t r1 = r0 + (uint32_t)gi->region1Count + 2U;
		if (r1 > 22U)
			return -600;
		reg1Start = kMp3SfBandIndex[sfIdx][r0 + 1U];
		reg2Start = kMp3SfBandIndex[sfIdx][r1];
	}

	int32_t bigEnd = 2 * gi->bigValues;
	if (bigEnd > 576)
		return -600;
	if (reg1Start > bigEnd)
		reg1Start = bigEnd;
	if (reg2Start > bigEnd)
		reg2Start = bigEnd;

	int32_t *spec = &st->huffSpec[0][0];
	int32_t err = mp3HuffDecodeBigValues(spec, reg1Start, gi->tableSelect[0], st);
	if (err != 0)
		return err;
	err = mp3HuffDecodeBigValues(spec + reg1Start, reg2Start - reg1Start, gi->tableSelect[1], st);
	if (err != 0)
		return err;
	err = mp3HuffDecodeBigValues(spec + reg2Start, bigEnd - reg2Start, gi->tableSelect[2], st);
	if (err != 0)
		return err;

	int32_t remBits = (int32_t)(part2Start + (uint32_t)gi->part23Length - mp3ResGetBitPos(st));
	int32_t count1 = mp3HuffDecodeCount1(spec + bigEnd, 576 - bigEnd, remBits,
					     gi->count1TableSelect, st);
	if (st->mainBufErr != 0)
		return -1500;

	int32_t nonZeroEnd = bigEnd + count1;
	uint32_t targetEnd = part2Start + (uint32_t)gi->part23Length;
	if (mp3ResGetBitPos(st) > targetEnd) {
		nonZeroEnd -= 4;
		mp3ResRewindBits((int32_t)(mp3ResGetBitPos(st) - targetEnd), st);
	}
	if (mp3ResGetBitPos(st) < targetEnd) {
		mp3CheckDebugFail(st, 254);
		mp3ResSkipBits(targetEnd - mp3ResGetBitPos(st), st);
		if (st->mainBufErr != 0)
			return -1500;
	}

	if (nonZeroEnd > 576)
		return -600;
	sceClibMemset(spec + nonZeroEnd, 0, (size_t)(576 - nonZeroEnd) * sizeof(int32_t));
	return nonZeroEnd;
}

static __attribute__((noinline, noclone)) int32_t
mp3DequantizeGranule(const Mp3ScaleFactors *sf, const Mp3GranuleInfo *gi, int32_t ch, Mp3State *st,
		     int32_t maxNonZero)
{
	int32_t sfIdx = 3 * st->frameParams.hdr->version + st->frameParams.hdr->sampleRateIdx;
	const uint16_t *longBands = &kMp3SfBandIndex[sfIdx][0];
	const uint16_t *shortBands = &kMp3SfBandIndex[sfIdx][23];

	int32_t isShort = (gi->windowSwitching != 0 && gi->blockType == 2);
	int32_t isMixed = gi->mixedBlock;
	int32_t winLen = 0;
	int32_t nextSfbLine;
	if (isShort && isMixed == 0) {
		winLen = shortBands[1];
		nextSfbLine = 3 * winLen;
	} else {
		nextSfbLine = longBands[1];
	}

	int32_t sfb = 0;
	int32_t shortStart = 0;
	int32_t maxSb = maxNonZero / 18;
	for (int32_t sb = 0; sb <= maxSb; ++sb) {
		for (int32_t s = 0; s < 18; ++s) {
			int32_t line = sb * 18 + s;
			if (line == nextSfbLine) {
				if (isShort) {
					if (isMixed != 0) {
						if (line == longBands[8]) {
							sfb = 3;
							shortStart = 3 * shortBands[3];
							winLen = shortBands[4] - shortBands[3];
							nextSfbLine = 3 * shortBands[4];
						} else {
							++sfb;
							if (line < longBands[8]) {
								nextSfbLine = longBands[sfb + 1];
							} else {
								shortStart = 3 * shortBands[sfb];
								winLen = shortBands[sfb + 1] -
									 shortBands[sfb];
								nextSfbLine =
									3 * shortBands[sfb + 1];
							}
						}
					} else {
						++sfb;
						shortStart = 3 * shortBands[sfb];
						winLen = shortBands[sfb + 1] - shortBands[sfb];
						nextSfbLine = 3 * shortBands[sfb + 1];
					}
				} else {
					++sfb;
					nextSfbLine = longBands[sfb + 1];
				}
			}

			float gain = kMp3GlobalGainTable[gi->globalGain];
			float scale;
			int32_t q = st->huffSpec[sb][s];
			if (isShort && (isMixed == 0 || sb > 1)) {
				if (winLen == 0)
					return -700;
				int32_t win = (line - shortStart) / winLen;
				scale = (gain * kMp3SubblockGainTable[gi->subblockGain[win]]) *
					kMp3ShortScaleFactorTable[32 * gi->scalefacScale +
								  sf[ch].s[win][sfb]];
			} else {
				scale = gain *
					kMp3LongScaleFactorTable[128 * gi->scalefacScale +
								 4 * sf[ch].l[sfb] +
								 gi->preflag * kMp3Pretab[sfb]];
			}

			if (q < 0) {
				if (-q > 8206)
					return -700;
				st->dequantSpec[ch][sb][s] = -(kAudioPow43Table[-q] * scale);
			} else {
				if (q > 8206)
					return -700;
				st->dequantSpec[ch][sb][s] = kAudioPow43Table[q] * scale;
			}
		}
	}

	if (maxNonZero != 575 && 576 - maxNonZero > 0) {
		float *flat = &st->dequantSpec[ch][0][0];
		sceClibMemset(flat + maxNonZero, 0, (size_t)(576 - maxNonZero) * sizeof(float));
	}
	return 0;
}

static __attribute__((noinline, noclone)) void
mp3ReorderShortBlocks(int32_t ch, const Mp3GranuleInfo *gi, Mp3State *st)
{
	int32_t sfIdx = 3 * st->frameParams.hdr->version + st->frameParams.hdr->sampleRateIdx;
	const uint16_t *shortBands = &kMp3SfBandIndex[sfIdx][23];
	const float *src = &st->stereoSpec[ch][0][0];
	float *dst = &st->hybridOut[0][0];

	int32_t sfb = 0;
	if (gi->mixedBlock != 0) {
		sceClibMemcpy_safe(dst, src, 36U * sizeof(float));
		sfb = 3;
	}

	for (; sfb < 13; ++sfb) {
		int32_t start = shortBands[sfb];
		int32_t width = shortBands[sfb + 1] - start;
		int32_t base = 3 * start;
		for (int32_t i = 0; i < width; ++i) {
			dst[base + i * 3 + 0] = src[base + i];
			dst[base + i * 3 + 1] = src[base + width + i];
			dst[base + i * 3 + 2] = src[base + 2 * width + i];
		}
	}
}

static inline void mp3SetIntensityPos(Mp3State *st, int32_t line, int16_t isVal, int32_t isMpeg2,
				      int32_t scfScale)
{
	st->isPos[line] = isVal;
	if (isVal == 7)
		return;
	if (!isMpeg2) {
		st->isRatio[line] = kMp3IsRatioMpeg1[isVal];
	} else if (isVal == 0) {
		st->isK[0][line] = 1.0f;
		st->isK[1][line] = 1.0f;
	} else if ((isVal & 1) != 0) {
		st->isK[0][line] = kMp3IsRatioMpeg2[32 * scfScale + isVal + 1];
		st->isK[1][line] = 1.0f;
	} else {
		st->isK[0][line] = 1.0f;
		st->isK[1][line] = kMp3IsRatioMpeg2[32 * scfScale + isVal];
	}
}

static __attribute__((noinline, noclone)) int32_t
mp3ProcessStereo(const Mp3ScaleFactors *sf, const Mp3GranuleInfo *gi, Mp3State *st)
{
	const Mp3Header *hdr = st->frameParams.hdr;
	int32_t isStereo = (st->frameParams.channels == 2);
	int32_t msStereo = (hdr->mode == 1) && ((hdr->modeExt & 2) != 0);
	int32_t iStereo = (hdr->mode == 1) && ((hdr->modeExt & 1) != 0);
	int32_t isMpeg2 = (hdr->version != 1);
	int32_t sfIdx = 3 * hdr->version + hdr->sampleRateIdx;
	const uint16_t *longBands = &kMp3SfBandIndex[sfIdx][0];
	const uint16_t *shortBands = &kMp3SfBandIndex[sfIdx][23];

	sceClibMemset(st->isPos, 0, sizeof(st->isPos));
	sceClibMemset(st->isRatio, 0, sizeof(st->isRatio));
	sceClibMemset(st->isK, 0, sizeof(st->isK));
	for (int32_t i = 0; i < 576; ++i)
		st->isPos[i] = 7;

	if (isStereo && iStereo) {
		int32_t scfScale = gi->scalefacCompress & 1;
		const float *ch1Flat = &st->dequantSpec[1][0][0];

		if (gi->windowSwitching != 0 && gi->blockType == 2) {
			int32_t minSfb = (gi->mixedBlock != 0) ? 3 : 0;
			int32_t maxZeroSfb = 0;
			for (int32_t w = 0; w < 3; ++w) {
				int32_t sfb = 12;
				int32_t found = 0;
				for (; sfb >= minSfb; --sfb) {
					int32_t width = shortBands[sfb + 1] - shortBands[sfb];
					int32_t end = 3 * shortBands[sfb] + (w + 1) * width;
					for (int32_t k = end - 1; k >= end - width; --k) {
						if (ch1Flat[k] != 0.0f) {
							found = 1;
							break;
						}
					}
					if (found)
						break;
				}
				int32_t startSfb = sfb + 1;
				if (startSfb > maxZeroSfb)
					maxZeroSfb = startSfb;
				for (int32_t b = startSfb; b < 12; ++b) {
					int32_t width = shortBands[b + 1] - shortBands[b];
					int32_t base = 3 * shortBands[b] + w * width;
					int16_t isVal = sf[1].s[w][b];
					for (int32_t k = 0; k < width; ++k)
						mp3SetIntensityPos(st, base + k, isVal, isMpeg2,
								   scfScale);
				}
				int32_t prevStart = 3 * shortBands[11] +
						    w * (shortBands[12] - shortBands[11]);
				int32_t lastWidth = shortBands[13] - shortBands[12];
				int32_t lastStart = 3 * shortBands[12] + w * lastWidth;
				for (int32_t k = 0; k < lastWidth; ++k) {
					st->isPos[lastStart + k] = st->isPos[prevStart];
					st->isRatio[lastStart + k] = st->isRatio[prevStart];
					st->isK[0][lastStart + k] = st->isK[0][prevStart];
					st->isK[1][lastStart + k] = st->isK[1][prevStart];
				}
			}

			if (gi->mixedBlock != 0 && maxZeroSfb == 3) {
				int32_t lastNz = 0;
				for (int32_t k = 53; k >= 0; --k) {
					if (ch1Flat[k] != 0.0f) {
						lastNz = k;
						break;
					}
				}
				int32_t sfb = 0;
				while (sfb < 8 && longBands[sfb] <= lastNz)
					++sfb;
				for (; sfb < 8; ++sfb) {
					int32_t start = longBands[sfb];
					int32_t end = longBands[sfb + 1];
					if (start >= 576)
						return -800;
					int16_t isVal = sf[1].l[sfb];
					for (int32_t k = start; k < end; ++k) {
						if (k >= 576)
							return -800;
						mp3SetIntensityPos(st, k, isVal, isMpeg2, scfScale);
					}
				}
			}
		} else {
			int32_t lastNz = 0;
			for (int32_t k = 575; k >= 0; --k) {
				if (ch1Flat[k] != 0.0f) {
					lastNz = k;
					break;
				}
			}
			int32_t sfb = 0;
			while (sfb < 21 && longBands[sfb] <= lastNz)
				++sfb;
			for (; sfb < 21; ++sfb) {
				int32_t start = longBands[sfb];
				int32_t end = longBands[sfb + 1];
				if (start >= 576)
					return -800;
				int16_t isVal = sf[1].l[sfb];
				for (int32_t k = start; k < end; ++k) {
					if (k >= 576)
						return -800;
					mp3SetIntensityPos(st, k, isVal, isMpeg2, scfScale);
				}
			}
			int32_t prevStart = longBands[20];
			int32_t tailStart = longBands[21];
			int32_t tailLen = 576 - tailStart;
			if (tailStart + tailLen > 576)
				return -800;
			for (int32_t k = 0; k < tailLen; ++k) {
				st->isPos[tailStart + k] = st->isPos[prevStart];
				st->isRatio[tailStart + k] = st->isRatio[prevStart];
				st->isK[0][tailStart + k] = st->isK[0][prevStart];
				st->isK[1][tailStart + k] = st->isK[1][prevStart];
			}
		}
	}

	sceClibMemset(st->stereoSpec, 0, sizeof(st->stereoSpec));

	if (isStereo) {
		for (int32_t sb = 0; sb < 32; ++sb) {
			for (int32_t s = 0; s < 18; ++s) {
				int32_t idx = sb * 18 + s;
				float lVal = st->dequantSpec[0][sb][s];
				float rVal = st->dequantSpec[1][sb][s];
				if (st->isPos[idx] == 7) {
					if (msStereo) {
						st->stereoSpec[0][sb][s] =
							(lVal + rVal) * 0.70710677f;
						st->stereoSpec[1][sb][s] =
							(lVal - rVal) * 0.70710677f;
					} else {
						st->stereoSpec[0][sb][s] = lVal;
						st->stereoSpec[1][sb][s] = rVal;
					}
				} else {
					if (!iStereo)
						return -800;
					float k0, k1;
					if (isMpeg2) {
						k0 = st->isK[0][idx];
						k1 = st->isK[1][idx];
					} else {
						float r = st->isRatio[idx];
						float denom = r + 1.0f;
						k0 = r / denom;
						k1 = 1.0f / denom;
					}
					st->stereoSpec[0][sb][s] = k0 * lVal;
					st->stereoSpec[1][sb][s] = k1 * lVal;
				}
			}
		}
	} else {
		sceClibMemcpy_safe(st->stereoSpec[0], st->dequantSpec[0],
				   sizeof(st->dequantSpec[0]));
	}
	return 0;
}

static __attribute__((noinline, noclone)) void
mp3AntiAlias(float (*inSpec)[18], float (*outSpec)[18], const Mp3GranuleInfo *gi)
{
	for (int32_t sb = 0; sb < 32; ++sb) {
		for (int32_t s = 0; s < 18; ++s)
			outSpec[sb][s] = inSpec[sb][s];
	}

	int32_t numSb;
	if (gi->windowSwitching != 0 && gi->blockType == 2) {
		if (gi->mixedBlock == 0)
			return;
		numSb = 1;
	} else {
		numSb = 31;
	}

	float32x4_t cs0 = vld1q_f32(&kMp3AliasCs[0]);
	float32x4_t ca0 = vld1q_f32(&kMp3AliasCa[0]);
	float32x4_t cs1 = vld1q_f32(&kMp3AliasCs[4]);
	float32x4_t ca1 = vld1q_f32(&kMp3AliasCa[4]);

	for (int32_t sb = 0; sb < numSb; ++sb) {
		const float *lo = inSpec[sb];
		const float *hi = inSpec[sb + 1];
		float *dstLo = outSpec[sb];
		float *dstHi = outSpec[sb + 1];

		float32x4_t bu0 = { lo[17], lo[16], lo[15], lo[14] };
		float32x4_t bd0 = vld1q_f32(&hi[0]);
		float32x4_t outLo0 = vmlsq_f32(vmulq_f32(bu0, cs0), bd0, ca0);
		float32x4_t outHi0 = vmlaq_f32(vmulq_f32(bd0, cs0), bu0, ca0);
		dstLo[17] = vgetq_lane_f32(outLo0, 0);
		dstLo[16] = vgetq_lane_f32(outLo0, 1);
		dstLo[15] = vgetq_lane_f32(outLo0, 2);
		dstLo[14] = vgetq_lane_f32(outLo0, 3);
		vst1q_f32(&dstHi[0], outHi0);

		float32x4_t bu1 = { lo[13], lo[12], lo[11], lo[10] };
		float32x4_t bd1 = vld1q_f32(&hi[4]);
		float32x4_t outLo1 = vmlsq_f32(vmulq_f32(bu1, cs1), bd1, ca1);
		float32x4_t outHi1 = vmlaq_f32(vmulq_f32(bd1, cs1), bu1, ca1);
		dstLo[13] = vgetq_lane_f32(outLo1, 0);
		dstLo[12] = vgetq_lane_f32(outLo1, 1);
		dstLo[11] = vgetq_lane_f32(outLo1, 2);
		dstLo[10] = vgetq_lane_f32(outLo1, 3);
		vst1q_f32(&dstHi[4], outHi1);
	}
}

static __attribute__((noinline, noclone)) void
mp3ImdctSubband(int32_t sb, int32_t ch, const Mp3GranuleInfo *gi, Mp3State *st)
{
	float rawOut[36];
	float *in = st->hybridOut[sb];
	float *prev = st->imdctPrev[ch][sb];
	int32_t winType = (gi->windowSwitching != 0 && gi->mixedBlock != 0 && sb < 2)
				  ? 0
				  : gi->blockType;
	const float *win = &kMp3ImdctWindow[36 * winType];

	if (winType == 2) {
		sceClibMemset(rawOut, 0, sizeof(rawOut));
		for (int32_t w = 0; w < 3; ++w) {
			float sub[6];
			for (int32_t k = 0; k < 6; ++k)
				sub[k] = in[k * 3 + w];
			for (int32_t p = 0; p < 12; ++p) {
				const float *cosRow = &kMp3Imdct12Cos[p * 6];
				float acc = 0.0f;
				for (int32_t k = 0; k < 6; ++k)
					acc += sub[k] * cosRow[k];
				rawOut[6 * w + p + 6] += acc * win[p];
			}
		}
	} else {
		for (int32_t p = 0; p < 36; ++p) {
			const float *cosRow = &kMp3Imdct36Cos[p * 18];
			float acc = 0.0f;
			for (int32_t k = 0; k < 18; ++k)
				acc += in[k] * cosRow[k];
			rawOut[p] = acc * win[p];
		}
	}

	for (int32_t i = 0; i < 18; ++i) {
		in[i] = rawOut[i] + prev[i];
		prev[i] = rawOut[i + 18];
	}
}

static __attribute__((noinline, noclone)) void
mp3ImdctZeroSubband(int32_t sb, int32_t ch, Mp3State *st)
{
	float *dst = st->hybridOut[sb];
	float *prev = st->imdctPrev[ch][sb];
	for (int32_t i = 0; i < 18; ++i) {
		dst[i] = prev[i];
		prev[i] = 0.0f;
	}
}

static __attribute__((noinline, noclone)) void
mp3SynthDct8(float *vBuf, float32x4_t x0, float32x4_t x1)
{
	for (int32_t i = 0; i < 4; i += 2) {
		int32_t idx = 4 * i + 2;
		const float *c0 = &kMp3SynthCos[idx * 32];
		const float *c1 = &kMp3SynthCos[(idx + 4) * 32];
		float32x4_t p0 = vmlaq_f32(vmulq_f32(x0, vld1q_f32(c0)), x1, vld1q_f32(c0 + 4));
		float32x4_t p1 = vmlaq_f32(vmulq_f32(x0, vld1q_f32(c1)), x1, vld1q_f32(c1 + 4));
		float32x2_t s0 = vpadd_f32(vpadd_f32(vget_low_f32(p0), vget_high_f32(p0)),
					   vpadd_f32(vget_low_f32(p1), vget_high_f32(p1)));
		vBuf[idx] = vget_lane_f32(s0, 0);
		vBuf[idx + 4] = vget_lane_f32(s0, 1);

		const float *c2 = &kMp3SynthCos[(idx + 32) * 32];
		const float *c3 = &kMp3SynthCos[(idx + 36) * 32];
		float32x4_t q0 = vmlaq_f32(vmulq_f32(x0, vld1q_f32(c2)), x1, vld1q_f32(c2 + 4));
		float32x4_t q1 = vmlaq_f32(vmulq_f32(x0, vld1q_f32(c3)), x1, vld1q_f32(c3 + 4));
		float32x2_t s1 = vpadd_f32(vpadd_f32(vget_low_f32(q0), vget_high_f32(q0)),
					   vpadd_f32(vget_low_f32(q1), vget_high_f32(q1)));
		vBuf[idx + 32] = vget_lane_f32(s1, 0);
		vBuf[idx + 36] = vget_lane_f32(s1, 1);
	}
}

static __attribute__((noinline, noclone)) void
mp3SynthDct16(float *vBuf, float32x4_t d0, float32x4_t d1, float32x4_t d2, float32x4_t d3)
{
	for (int32_t i = 0; i < 8; i += 2) {
		int32_t idx = 2 * i + 1;
		const float *c0 = &kMp3SynthCos[idx * 32];
		const float *c1 = &kMp3SynthCos[(idx + 2) * 32];
		float32x4_t p0 = vaddq_f32(
			vmlaq_f32(vmulq_f32(d0, vld1q_f32(c0)), d2, vld1q_f32(c0 + 8)),
			vmlaq_f32(vmulq_f32(d1, vld1q_f32(c0 + 4)), d3, vld1q_f32(c0 + 12)));
		float32x4_t p1 = vaddq_f32(
			vmlaq_f32(vmulq_f32(d0, vld1q_f32(c1)), d2, vld1q_f32(c1 + 8)),
			vmlaq_f32(vmulq_f32(d1, vld1q_f32(c1 + 4)), d3, vld1q_f32(c1 + 12)));
		float32x2_t s0 = vpadd_f32(vpadd_f32(vget_low_f32(p0), vget_high_f32(p0)),
					   vpadd_f32(vget_low_f32(p1), vget_high_f32(p1)));
		vBuf[idx] = vget_lane_f32(s0, 0);
		vBuf[idx + 2] = vget_lane_f32(s0, 1);

		const float *c2 = &kMp3SynthCos[(idx + 32) * 32];
		const float *c3 = &kMp3SynthCos[(idx + 34) * 32];
		float32x4_t q0 = vaddq_f32(
			vmlaq_f32(vmulq_f32(d0, vld1q_f32(c2)), d2, vld1q_f32(c2 + 8)),
			vmlaq_f32(vmulq_f32(d1, vld1q_f32(c2 + 4)), d3, vld1q_f32(c2 + 12)));
		float32x4_t q1 = vaddq_f32(
			vmlaq_f32(vmulq_f32(d0, vld1q_f32(c3)), d2, vld1q_f32(c3 + 8)),
			vmlaq_f32(vmulq_f32(d1, vld1q_f32(c3 + 4)), d3, vld1q_f32(c3 + 12)));
		float32x2_t s1 = vpadd_f32(vpadd_f32(vget_low_f32(q0), vget_high_f32(q0)),
					   vpadd_f32(vget_low_f32(q1), vget_high_f32(q1)));
		vBuf[idx + 32] = vget_lane_f32(s1, 0);
		vBuf[idx + 34] = vget_lane_f32(s1, 1);
	}
}

static __attribute__((noinline, noclone)) void
mp3SynthMirror(float *vBuf)
{
	for (int32_t i = 0; i < 16; ++i)
		vBuf[32 - i] = -vBuf[i];
	for (int32_t i = 1; i <= 16; ++i)
		vBuf[48 + i - 1] = vBuf[48 - i];
	for (int32_t i = 0; i < 64; ++i)
		vBuf[1024 + i] = vBuf[i];
}

static __attribute__((noinline, noclone)) void
mp3SynthWindow(const float *vBuf, float *pcmOut)
{
	for (int32_t j = 0; j < 32; ++j) {
		float sum = 0.0f;
		const float *w = &kMp3SynthWindow[j * 16];
		for (int32_t k = 0; k < 8; ++k) {
			sum += vBuf[j + k * 128] * w[2 * k];
			sum += vBuf[j + k * 128 + 96] * w[2 * k + 1];
		}
		pcmOut[j] = sum;
	}
}

static __attribute__((noinline, noclone)) int32_t
mp3SubbandSynthesis(int32_t ch, Mp3State *st)
{
	int32_t bufOff = st->synthBufOff[ch];

	for (int32_t sample = 0; sample < 18; ++sample) {
		bufOff = (bufOff - 64) & 0x3FF;
		float x[32];
		for (int32_t sb = 0; sb < 32; ++sb)
			x[sb] = st->hybridOut[sb][sample];

		float s[16], d[16];
		for (int32_t k = 0; k < 16; ++k) {
			s[k] = x[k] + x[31 - k];
			d[k] = x[k] - x[31 - k];
		}

		float s2[8], d2[8];
		for (int32_t k = 0; k < 8; ++k) {
			s2[k] = s[k] + s[15 - k];
			d2[k] = s[k] - s[15 - k];
		}

		float s4[4], d4[4];
		for (int32_t k = 0; k < 4; ++k) {
			s4[k] = s2[k] + s2[7 - k];
			d4[k] = s2[k] - s2[7 - k];
		}

		float *vBuf = st->synthRingPtr + ch * 2048 + bufOff;
		vBuf[0] = ((s4[0] - s4[1]) - s4[2] + s4[3]) * 0.70710677f;
		vBuf[16] = 0.0f;
		vBuf[1040] = 0.0f;
		vBuf[48] = -(s4[0] + s4[1] + s4[2] + s4[3]);

		for (int32_t i = 0; i < 2; ++i) {
			int32_t idx = 8 * i + 4;
			const float *c = &kMp3SynthCos[idx * 32];
			vBuf[idx] = d4[0] * c[0] + d4[1] * c[1] + d4[2] * c[2] + d4[3] * c[3];
		}
		for (int32_t i = 0; i < 2; ++i) {
			int32_t idx = 8 * i + 36;
			const float *c = &kMp3SynthCos[idx * 32];
			vBuf[idx] = d4[0] * c[0] + d4[1] * c[1] + d4[2] * c[2] + d4[3] * c[3];
		}

		float t0 = s4[0] - s4[3];
		float t1 = s4[1] - s4[2];
		vBuf[8] = t1 * -0.9238795f + t0 * 0.38268343f;
		vBuf[40] = t1 * -0.382683f + t0 * -0.9238795f;

		float32x4_t qd2_0 = vld1q_f32(&d2[0]);
		float32x4_t qd2_1 = vld1q_f32(&d2[4]);
		mp3SynthDct8(vBuf, qd2_0, qd2_1);

		float32x4_t qd_0 = vld1q_f32(&d[0]);
		float32x4_t qd_1 = vld1q_f32(&d[4]);
		float32x4_t qd_2 = vld1q_f32(&d[8]);
		float32x4_t qd_3 = vld1q_f32(&d[12]);
		mp3SynthDct16(vBuf, qd_0, qd_1, qd_2, qd_3);

		mp3SynthMirror(vBuf);
		mp3SynthWindow(vBuf, st->pcmBufPtr + ch * 576 + sample * 32);
	}

	st->synthBufOff[ch] = (int16_t)bufOff;
	return 0;
}

static __attribute__((noinline, noclone)) void
mp3WritePcmOutput(uint8_t *outBuf, int32_t *outBytes, Mp3Decoder *dec)
{
	int32_t streamCh = dec->st.frameParams.channels;
	int32_t outCh = (dec->forceStereo != 0U) ? 2 : streamCh;
	int32_t chMap[2];
	int32_t fmt = dec->outputFormat;

	if (streamCh == 1) {
		chMap[0] = 0;
		chMap[1] = 0;
	} else if (fmt == 5) {
		chMap[0] = 1;
		chMap[1] = 0;
	} else {
		chMap[0] = 0;
		chMap[1] = 1;
	}
	dec->actualOutCh = (uint8_t)outCh;

	int32_t stride;
	if (fmt == 3 || fmt == 5) {
		dec->actualOutFmt = (uint8_t)fmt;
		stride = 2 * outCh;
		if (outCh == 2) {
			float32x4_t halfPos = vdupq_n_f32(0.5f);
			float32x4_t halfNeg = vdupq_n_f32(-0.5f);
			float32x4_t scale = vdupq_n_f32(32768.0f);
			int32x4_t maxVal = vdupq_n_s32(0x7FFF);
			int32x4_t minVal = vdupq_n_s32(-0x8000);
			int32x4_t maskLo = vdupq_n_s32(0xFFFF);
			const float *src0 = dec->st.pcmBufPtr + 576 * chMap[0];
			const float *src1 = dec->st.pcmBufPtr + 576 * chMap[1];
			int32_t *dst = (int32_t *)outBuf;

			for (int32_t i = 0; i < 576; i += 4) {
				float32x4_t f0 = vld1q_f32(&src0[i]);
				float32x4_t f1 = vld1q_f32(&src1[i]);
				uint32x4_t m0 = vcgtq_f32(f0, vdupq_n_f32(0.0f));
				uint32x4_t m1 = vcgtq_f32(f1, vdupq_n_f32(0.0f));
				float32x4_t r0 = vmlaq_f32(vbslq_f32(m0, halfPos, halfNeg), f0, scale);
				float32x4_t r1 = vmlaq_f32(vbslq_f32(m1, halfPos, halfNeg), f1, scale);
				int32x4_t i0 = vmaxq_s32(vminq_s32(vcvtq_s32_f32(r0), maxVal), minVal);
				int32x4_t i1 = vmaxq_s32(vminq_s32(vcvtq_s32_f32(r1), maxVal), minVal);
				int32x4_t packed = vorrq_s32(vandq_s32(i0, maskLo), vshlq_n_s32(i1, 16));
				vst1q_s32(&dst[i], packed);
			}
		} else {
			for (int32_t ch = 0; ch < outCh; ++ch) {
				const float *src = dec->st.pcmBufPtr + 576 * chMap[ch];
				uint8_t *dst = outBuf + 2 * ch;
				for (int32_t i = 0; i < 576; ++i) {
					float v = src[i];
					float scaled = (v <= 0.0f) ? (v * 32768.0f - 0.5f)
								   : (v * 32768.0f + 0.5f);
					int32_t iv = (int32_t)scaled;
					if (iv < -32768)
						iv = -32768;
					if (iv > 32767)
						iv = 32767;
					*(int16_t *)dst = (int16_t)iv;
					dst += stride;
				}
			}
		}
	} else if (fmt == 4) {
		dec->actualOutFmt = 4U;
		stride = 4 * outCh;
		for (int32_t ch = 0; ch < outCh; ++ch) {
			const float *src = dec->st.pcmBufPtr + 576 * chMap[ch];
			uint8_t *dst = outBuf + 4 * ch;
			for (int32_t i = 0; i < 576; ++i) {
				*(float *)dst = src[i];
				dst += stride;
			}
		}
	} else {
		uint32_t mask;
		if (fmt == 1) {
			mask = 0xFFFFF0U;
			dec->actualOutFmt = 1U;
		} else if (fmt == 2) {
			mask = 0xFFFFC0U;
			dec->actualOutFmt = 2U;
		} else {
			mask = 0xFFFFFFU;
			dec->actualOutFmt = 0U;
		}
		stride = 4 * outCh;
		for (int32_t ch = 0; ch < outCh; ++ch) {
			const float *src = dec->st.pcmBufPtr + 576 * chMap[ch];
			uint8_t *dst = outBuf + 4 * ch;
			for (int32_t i = 0; i < 576; ++i) {
				float v = src[i];
				float scaled = (v <= 0.0f) ? (v * 8388608.0f - 0.5f)
							   : (v * 8388608.0f + 0.5f);
				int32_t iv = (int32_t)scaled;
				uint8_t b1, b2, b3;
				if (iv > 0x7FFFFF) {
					b1 = 0x7FU;
					b2 = 0xFFU;
					b3 = (uint8_t)mask;
				} else if (iv < -0x800000) {
					b1 = 0x80U;
					b2 = 0U;
					b3 = 0U;
				} else {
					uint32_t m = (uint32_t)iv & mask;
					b1 = (uint8_t)(m >> 16);
					b2 = (uint8_t)(m >> 8);
					b3 = (uint8_t)m;
				}
				dst[0] = 0U;
				dst[1] = b1;
				dst[2] = b2;
				dst[3] = b3;
				dst += stride;
			}
		}
	}

	*outBytes = 576 * stride;
}

static __attribute__((noinline, noclone)) int32_t
mp3DecodeLayer3(int32_t numGranules, uint8_t **outBufPtr, Mp3Decoder *dec, uint32_t expectedCrc)
{
	Mp3State *st = &dec->st;
	Mp3ScaleFactors sf[2];
	Mp3SideInfo si;
	int32_t pcmBytes = 0;
	uint32_t calcCrc = 0;
	int32_t channels = st->frameParams.channels;
	int32_t hasCrc = st->header.errorProtection;

	sceClibMemset(sf, 0, sizeof(sf));
	sceClibMemset(&si, 0, sizeof(si));
	st->samplesPerFrame = (st->header.version == 1) ? 1152 : 576;

	int32_t err = mp3ReadSideInfo(&st->inStream, &si, &st->frameParams, st);
	if (err != 0) {
		dec->lastError = err;
		return -104;
	}

	if (hasCrc != 0) {
		mp3ComputeSideInfoCrc(&st->frameParams, &si, &calcCrc);
		dec->crcMismatch = (calcCrc != expectedCrc) ? 1U : 0U;
	}

	int32_t resBytesRead = (int32_t)(mp3ResGetBitPos(st) >> 3);
	uint32_t remBits = mp3ResGetBitPos(st) & 7U;
	if (st->debugFailPoint == 402 || remBits != 0U) {
		uint32_t skip = (st->debugFailPoint == 402) ? 7U : (8U - remBits);
		mp3CheckDebugFail(st, 402);
		mp3ResSkipBits(skip, st);
		if (st->mainBufErr != 0) {
			dec->lastError = -1500;
			return -104;
		}
		++resBytesRead;
	}

	int32_t flushBytes = st->mainDataTop - resBytesRead - si.mainDataBegin;
	if (dec->resetReservoirFlag == 1U && flushBytes >= 0) {
		st->resBitPos = (uint32_t)(8 * (st->mainDataTop - si.mainDataBegin));
		uint32_t curByte = mp3ResGetBitPos(st) >> 3;
		resBytesRead = (int32_t)curByte;
		flushBytes = st->mainDataTop - resBytesRead - si.mainDataBegin;
		st->resReadIdx = (curByte != 0U) ? (curByte - 1U) : curByte;
		if (si.mainDataBegin != 0)
			st->resBitsLeft = 0U;
	}

	int32_t frameDataBytes = mp3CalcMainDataBytes(&st->header, channels);
	dec->mainDataBegin = si.mainDataBegin;
	dec->reservoirAvail = st->mainDataTop - resBytesRead;
	dec->frameDataBytes = frameDataBytes;
	st->frameBytes = (int32_t)mp3CalcFrameBytes(&st->frameParams);

	for (int32_t i = 0; i < frameDataBytes; ++i) {
		mp3CheckDebugFail(st, 403);
		uint32_t b = (uint32_t)mp3StreamReadBits(&st->inStream, 8, st);
		mp3ResPushByte(b, st);
	}
	if (st->streamErr != 0) {
		dec->lastError = -1600;
		return -104;
	}

	if (resBytesRead > 4096) {
		st->mainDataTop -= 4096;
		mp3ResDiscardBytes(4096, st);
	}
	st->mainDataTop += dec->frameDataBytes;

	if (flushBytes < 0 || (dec->resetReservoirFlag == 1U && dec->skipMainDataFlag == 1U))
		return 0;

	for (int32_t i = 0; i < flushBytes; ++i) {
		mp3CheckDebugFail(st, 404);
		mp3ResSkipBits(8U, st);
	}
	if (st->mainBufErr != 0) {
		dec->lastError = -1500;
		return -104;
	}

	for (int32_t gr = 0; gr < numGranules; ++gr) {
		for (int32_t ch = 0; ch < channels; ++ch) {
			uint32_t part2Start = mp3ResGetBitPos(st);
			if (st->header.version == 1)
				err = mp3ReadScaleFactorsMpeg1(sf, &si, gr, ch, st);
			else
				err = mp3ReadScaleFactorsMpeg2(sf, &si, gr, ch, st);
			if (err != 0) {
				dec->lastError = err;
				return -104;
			}

			int32_t nonZero = mp3DecodeHuffmanGranule(&si, ch, gr, part2Start, st);
			if (nonZero < 0) {
				dec->lastError = nonZero;
				return -104;
			}
			if (nonZero > 575)
				nonZero = 575;

			err = mp3DequantizeGranule(sf, &si.ch[ch].gr[gr], ch, st, nonZero);
			if (err != 0) {
				dec->lastError = err;
				return -104;
			}
		}

		err = mp3ProcessStereo(sf, &si.ch[0].gr[gr], st);
		if (err != 0) {
			dec->lastError = err;
			return -104;
		}

		for (int32_t ch = 0; ch < channels; ++ch) {
			const Mp3GranuleInfo *gi = &si.ch[ch].gr[gr];
			if (gi->windowSwitching != 0 && gi->blockType == 2) {
				mp3ReorderShortBlocks(ch, gi, st);
				mp3AntiAlias(st->hybridOut, st->hybridOut, gi);
			} else {
				mp3AntiAlias(st->stereoSpec[ch], st->hybridOut, gi);
			}

			int32_t lastSb = 31;
			for (; lastSb >= 0; --lastSb) {
				int32_t nonZeroFound = 0;
				for (int32_t s = 17; s >= 0; --s) {
					if (st->hybridOut[lastSb][s] != 0.0f) {
						nonZeroFound = 1;
						break;
					}
				}
				if (nonZeroFound)
					break;
			}
			int32_t activeSb = lastSb + 1;
			for (int32_t sb = 0; sb < activeSb; ++sb)
				mp3ImdctSubband(sb, ch, gi, st);
			for (int32_t sb = activeSb; sb < 32; ++sb)
				mp3ImdctZeroSubband(sb, ch, st);

			for (int32_t sb = 1; sb < 32; sb += 2) {
				for (int32_t s = 1; s < 18; s += 2)
					st->hybridOut[sb][s] = -st->hybridOut[sb][s];
			}

			mp3SubbandSynthesis(ch, st);
		}

		mp3WritePcmOutput(*outBufPtr, &pcmBytes, dec);
		*outBufPtr += pcmBytes;
	}

	return 0;
}

static __attribute__((noinline, noclone)) int32_t
mp3DecodeFrameInternal(const uint8_t *inBuf, uint8_t *outBuf, Mp3Decoder *dec)
{
	Mp3State *st = &dec->st;
	uint32_t crc16 = 0;
	uint8_t *outPtr = outBuf;

	dec->lastError = 0;
	st->mainBufErr = 0;
	st->streamErr = 0;
	if (++dec->frameCountLo == 0U)
		++dec->frameCountHi;

	sceClibMemset(st->hybridOut, 0, sizeof(st->hybridOut));
	sceClibMemset(st->dequantSpec, 0, sizeof(st->dequantSpec));
	sceClibMemset(st->stereoSpec, 0, sizeof(st->stereoSpec));
	sceClibMemset(st->huffSpec, 0, sizeof(st->huffSpec));

	st->inStream.buf = inBuf;
	st->inStream.bitPos = 0;
	st->inStream.bytePos = 0;
	st->inStream.bitsLeft = 8;
	dec->crcMismatch = 0U;

	mp3CheckDebugFail(st, 103);
	int32_t syncType = mp3FindSyncWord(&st->inStream, dec->syncSearchBytes, st);
	if (st->streamErr != 0) {
		dec->lastError = -1600;
		return -104;
	}

	int32_t isMpeg25 = 0;
	if (syncType != 1) {
		if (syncType != 2 || dec->allowMpeg25 != 1) {
			dec->lastError = -1000;
			return -104;
		}
		isMpeg25 = 1;
	}

	int32_t err = mp3ParseFrameHeader(&st->inStream, &st->frameParams, isMpeg25, st);
	if (err == 0) {
		dec->isSingleChannel =
			(st->header.mode == 3 && dec->forceStereo == 0U) ? 1U : 0U;
		err = mp3InitFrameParams(&st->frameParams);
	}
	if (err != 0) {
		dec->lastError = err;
		return -104;
	}

	int32_t numGranules = (st->header.version == 1) ? 2 : 1;
	if (st->header.errorProtection != 0) {
		mp3CheckDebugFail(st, 400);
		mp3ReadCrc16(&st->inStream, &crc16, st);
	}
	if (st->streamErr != 0) {
		dec->lastError = -1600;
		return -104;
	}
	if (st->header.layer != 3) {
		dec->lastError = -1000;
		return -104;
	}

	if (mp3DecodeLayer3(numGranules, &outPtr, dec, crc16) != 0)
		return -104;

	if (st->header.bitrateIdx > 0) {
		int32_t totalBytes =
			(int32_t)(((float)st->samplesPerFrame /
				   kMp3SampleRatesKhz[4 * st->header.version +
						      st->header.sampleRateIdx]) *
				  (float)kMp3BitrateKbps[st->header.version][st->header.layer - 1]
							[st->header.bitrateIdx] *
				  0.125f);
		if (st->header.padding != 0)
			++totalBytes;

		int32_t padBits = (st->debugFailPoint == 401)
					  ? 1
					  : (8 * totalBytes - st->inStream.bitPos);
		for (int32_t i = 0; i < padBits; ++i) {
			mp3CheckDebugFail(st, 401);
			mp3StreamReadBit(&st->inStream, st);
		}
	}

	if (st->streamErr != 0) {
		dec->lastError = -1600;
		return -104;
	}
	if (st->mainBufErr != 0) {
		dec->lastError = -1500;
		return -104;
	}

	dec->bitsConsumed = st->inStream.bitPos;
	dec->pcmBytesOut = (int32_t)(outPtr - outBuf);
	return 0;
}

static __attribute__((noinline, noclone)) int32_t
mp3ApplyOutputConfig(const Mp3Config *cfg, Mp3Decoder *dec)
{
	switch (cfg->outFmt) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		dec->outputFormat = cfg->outFmt;
		dec->syncSearchBytes = 0;
		dec->allowMpeg25 = (cfg->allowMpeg25 == 1U) ? 1 : 0;
		return 0;
	default:
		dec->lastError = -1300;
		return -104;
	}
}

static __attribute__((noinline, noclone)) int32_t
mp3ApplyDecodeConfig(const Mp3Config *cfg, Mp3Decoder *dec)
{
	dec->resetReservoirFlag = (cfg->resetReservoir == 1U) ? 1U : 0U;
	dec->skipMainDataFlag = (cfg->skipMainData == 1U) ? 1U : 0U;

	int32_t ret = 0;
	if (cfg->channelMode == 0U) {
		dec->forceStereo = 0U;
	} else if (cfg->channelMode == 2U) {
		dec->forceStereo = 2U;
	} else {
		dec->lastError = -1400;
		ret = -104;
	}
	dec->st.frameBytes = 4;
	return ret;
}

static __attribute__((noinline, noclone)) int32_t
mp3UpdateHeaderInfo(const Mp3Header *hdr, uint32_t rawBits, Mp3Decoder *dec)
{
	dec->headerValid = 1U;
	uint8_t modeVal;
	switch (hdr->mode) {
	case 0:
	case 1:
	case 2:
		modeVal = (uint8_t)hdr->mode;
		dec->streamChannels = 2U;
		break;
	case 3:
		modeVal = 3U;
		dec->streamChannels = 1U;
		break;
	default:
		dec->lastError = -1100;
		return -1;
	}

	uint8_t changed = 0U;
	if (dec->channelModeVal != modeVal) {
		dec->channelModeVal = modeVal;
		changed = 1U;
	}

	if ((uint32_t)hdr->emphasis > 3U) {
		dec->lastError = -1100;
		return -1;
	}
	if (dec->emphasisVal != (uint8_t)hdr->emphasis) {
		dec->emphasisVal = (uint8_t)hdr->emphasis;
		changed = 1U;
	}

	if ((uint32_t)hdr->copyright > 1U) {
		dec->lastError = -1100;
		return -1;
	}
	if (dec->copyrightVal != (uint8_t)hdr->copyright) {
		dec->copyrightVal = (uint8_t)hdr->copyright;
		changed = 1U;
	}

	if ((uint32_t)hdr->original > 1U) {
		dec->lastError = -1100;
		return -1;
	}
	if (dec->originalVal != (uint8_t)hdr->original) {
		dec->originalVal = (uint8_t)hdr->original;
		changed = 1U;
	}

	uint32_t brIdx = (uint32_t)hdr->bitrateIdx & 0x0FU;
	if (brIdx == 15U) {
		dec->lastError = -1100;
		return -1;
	}
	uint32_t bitrate = (hdr->version == 1) ? kMp3BitrateBpsMpeg1[brIdx] : kMp3BitrateBpsMpeg2[brIdx];
	if (dec->bitrateBps != bitrate) {
		dec->bitrateBps = bitrate;
		changed = 1U;
	}

	uint32_t maskedRaw = rawBits & 0xFFFFFU;
	if (dec->rawHdrBits != maskedRaw) {
		uint32_t prevRaw = dec->rawHdrBits;
		dec->rawHdrBits = maskedRaw;
		if (((maskedRaw ^ prevRaw) & 0xFFFFFDFFU) != 0U)
			changed = 1U;
	}

	uint8_t srCode;
	if (hdr->version == 1) {
		if (hdr->sampleRateIdx == 0)
			srCode = 4U;
		else if (hdr->sampleRateIdx == 1)
			srCode = 3U;
		else if (hdr->sampleRateIdx == 2)
			srCode = 5U;
		else {
			dec->lastError = -1100;
			return -1;
		}
	} else if (hdr->version == 0) {
		if (hdr->sampleRateIdx == 0)
			srCode = 7U;
		else if (hdr->sampleRateIdx == 1)
			srCode = 6U;
		else if (hdr->sampleRateIdx == 2)
			srCode = 8U;
		else {
			dec->lastError = -1100;
			return -1;
		}
	} else if (hdr->version == 2) {
		if (hdr->sampleRateIdx == 0)
			srCode = 10U;
		else if (hdr->sampleRateIdx == 1)
			srCode = 9U;
		else if (hdr->sampleRateIdx == 2)
			srCode = 11U;
		else {
			dec->lastError = -1100;
			return -1;
		}
	} else {
		dec->lastError = -1100;
		return -1;
	}

	if (dec->sampleRateCode != srCode) {
		dec->sampleRateCode = srCode;
		changed = 1U;
	}
	dec->headerChanged = changed;
	dec->noCrcFlag = (hdr->errorProtection == 0) ? 1U : 0U;

	if ((uint32_t)hdr->modeExt > 3U) {
		dec->lastError = -1100;
		return -1;
	}
	dec->modeExtVal = (uint8_t)hdr->modeExt;
	return 0;
}

static __attribute__((noinline, noclone)) int32_t
mp3ProbeFrameHeader(const uint8_t *inBuf, Mp3Decoder *dec)
{
	Mp3State *st = &dec->st;
	Mp3BitStream bs;
	Mp3Header hdr;
	Mp3FrameParams fp;
	Mp3SideInfo si;
	uint32_t crc16 = 0;

	bs.buf = inBuf;
	bs.bitPos = 0;
	bs.bytePos = 0;
	bs.bitsLeft = 8;
	sceClibMemset(&hdr, 0, sizeof(hdr));
	fp.hdr = &hdr;
	fp.channels = 0;
	fp.jsBound = 0;
	fp.sblimit = 0;

	int32_t syncType = mp3FindSyncWord(&bs, dec->syncSearchBytes, st);
	if (st->streamErr != 0) {
		dec->lastError = -1600;
		return -104;
	}

	int32_t isMpeg25 = 0;
	if (syncType != 1) {
		if (syncType != 2 || dec->allowMpeg25 != 1) {
			dec->lastError = -1200;
			return -104;
		}
		isMpeg25 = 1;
	}

	uint32_t rawBits =
		((uint32_t)inBuf[bs.bytePos + 2] +
		 (((uint32_t)inBuf[bs.bytePos + 1] +
		   ((uint32_t)((uint8_t)(inBuf[bs.bytePos] << bs.bitsLeft)) << (8 - bs.bitsLeft)))
		  << 8)) >>
		(4 - bs.bitsLeft);

	int32_t err = mp3ParseFrameHeader(&bs, &fp, isMpeg25, st);
	if (err == 0)
		err = mp3InitFrameParams(&fp);
	if (err != 0) {
		dec->lastError = err;
		return -104;
	}

	if (hdr.errorProtection != 0) {
		mp3CheckDebugFail(st, 500);
		mp3ReadCrc16(&bs, &crc16, st);
	}
	if (st->streamErr != 0) {
		dec->lastError = -1600;
		return -104;
	}

	if (hdr.layer == 3) {
		err = mp3ReadSideInfo(&bs, &si, &fp, st);
		if (err != 0) {
			dec->lastError = err;
			return -104;
		}
		if (hdr.errorProtection != 0)
			mp3ComputeSideInfoCrc(&fp, &si, &crc16);
		uint32_t resBytes = mp3ResGetBitPos(st) >> 3;
		uint32_t remBits = mp3ResGetBitPos(st) & 7U;
		if (remBits != 0U) {
			mp3CheckDebugFail(st, 501);
			mp3ResSkipBits(8U - remBits, st);
			if (st->mainBufErr != 0) {
				dec->lastError = -1500;
				return -104;
			}
			++resBytes;
		}
		dec->mainDataBegin = si.mainDataBegin;
		dec->reservoirAvail = st->mainDataTop - (int32_t)resBytes;
		dec->frameDataBytes = mp3CalcMainDataBytes(&hdr, fp.channels);
	}

	return (mp3UpdateHeaderInfo(&hdr, rawBits, dec) != 0) ? -104 : 0;
}

int32_t mp3CheckNeedMem(uint32_t *work)
{
	work[0] = 0U;
	work[1] = 0U;
	work[2] = 75604U;
	return 0;
}

int32_t mp3Init(uint32_t *work)
{
	Mp3WorkMem *wm = (Mp3WorkMem *)(uintptr_t)work[1];
	mp3InitConfigMagic(&wm->cfg);
	mp3InitDecoderFull(&wm->dec);
	mp3InitDecoderZero(&wm->dec);
	mp3ResetState(&wm->dec);
	wm->cfg.resetReservoir = 0U;
	wm->cfg.skipMainData = 1U;
	wm->cfg.channelMode = 0U;
	if (mp3ApplyDecodeConfig(&wm->cfg, &wm->dec) != 0)
		return AUDIOCODEC_ERROR_DECODE;
	wm->cfg.outFmt = 3U;
	wm->cfg.allowMpeg25 = 1U;
	if (mp3ApplyOutputConfig(&wm->cfg, &wm->dec) != 0)
		return AUDIOCODEC_ERROR_DECODE;
	return 0;
}

int32_t mp3Decode(uint32_t *work)
{
	Mp3WorkMem *wm = (Mp3WorkMem *)(uintptr_t)work[1];
	if (mp3ProbeFrameHeader((const uint8_t *)(uintptr_t)work[4], &wm->dec) != 0)
		return AUDIOCODEC_ERROR_DECODE;
	if (mp3DecodeFrameInternal((const uint8_t *)(uintptr_t)work[4],
				   (uint8_t *)(uintptr_t)work[6], &wm->dec) != 0) {
		mp3ResetState(&wm->dec);
		return AUDIOCODEC_ERROR_DECODE;
	}

	const Mp3Header *hdr = &wm->dec.st.header;
	work[9] = (uint32_t)hdr->version;
	work[10] = (uint32_t)hdr->layer;
	work[11] = (uint32_t)hdr->errorProtection;
	work[12] = (uint32_t)hdr->bitrateIdx;
	work[13] = (uint32_t)hdr->sampleRateIdx;
	work[14] = (uint32_t)hdr->padding;
	work[15] = (uint32_t)hdr->privateExt;
	work[16] = (uint32_t)hdr->mode;
	work[17] = (uint32_t)hdr->modeExt;
	work[18] = (uint32_t)hdr->copyright;
	work[20] = (uint32_t)hdr->emphasis;
	work[5] = (uint32_t)wm->dec.st.frameBytes;
	work[7] = (uint32_t)wm->dec.pcmBytesOut;
	return 0;
}
