#include "mp4_internal.h"

void *operator new(size_t size)
{
	return mp4MspaceAllocZero(1, (SceSize)size);
}

static const uint32_t kAacSampleRates[16] = {
	96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
	16000, 12000, 11025,  8000,  7350,     0,     0,     0
};

/* MPEG-4 ESDS descriptor header */
struct Mp4EsdsDescriptor {
	uint32_t       tag;
	int32_t        headerLength;
	int32_t        payloadSize;
	const uint8_t *rawPtr;
};

static int mp4ParseEsdsDescriptor(Mp4EsdsDescriptor *outDesc, const uint8_t *buf, int32_t maxLen)
{
	outDesc->rawPtr = buf;
	outDesc->tag = buf[0];
	outDesc->payloadSize = 0;

	int32_t hdrLen = 1;
	int32_t sizeVal = 0;
	const uint8_t *cur = buf + 1;
	while (true) {
		uint8_t byteVal = *cur;
		sizeVal = (sizeVal << 7) | (byteVal & 0x7F);
		outDesc->payloadSize = sizeVal;
		if (hdrLen >= maxLen)
			break;
		cur++;
		hdrLen++;
		if ((byteVal & 0x80u) == 0)
			break;
	}

	outDesc->headerLength = hdrLen;
	if (hdrLen + sizeVal > maxLen) {
		outDesc->headerLength = 0;
		outDesc->payloadSize = 0;
		return 0;
	}
	return 1;
}

static inline __attribute__((always_inline)) int mp4ParseEsdsSubDescriptor(const Mp4EsdsDescriptor *parent, int32_t offset, Mp4EsdsDescriptor *outChild)
{
	const uint8_t *start = parent->rawPtr + parent->headerLength + offset;
	int32_t remaining = parent->payloadSize - offset;
	return mp4ParseEsdsDescriptor(outChild, start, remaining);
}

/* Sample buffer grow helper */
static int mp4EnsureSampleCapacity(Mp4DemuxSample *sample, int32_t requiredBytes)
{
	if (sample->capacity < requiredBytes + 64) {
		int32_t newCap = requiredBytes + 2112;
		if (sample->data != NULL)
			mp4MspaceFree(sample->data);
		sample->data = (uint8_t *)mp4MspaceAllocZero(1, (SceSize)newCap);
		if (sample->data == NULL)
			return 0;
		sample->capacity = newCap;
	}
	return 1;
}

/* Passthrough sample extractor (MPEG-4 video, timed text, raw) */
class Mp4PassthroughExtractor : public Mp4SampleExtractor {
public:
	virtual ~Mp4PassthroughExtractor() {}
	virtual int getOutputSize(int sampleSize) { return sampleSize; }
	virtual void reset() {}
	virtual int extractSample(Mp4DemuxSample *sample, Mp4Movie *movie, int64_t offset, int sampleSize, int *outSize);
};

int Mp4PassthroughExtractor::extractSample(Mp4DemuxSample *sample, Mp4Movie *movie, int64_t offset, int sampleSize, int *outSize)
{
	if (mp4EnsureSampleCapacity(sample, sampleSize) == 0) {
		*outSize = 0;
		return MP4_ERR_GENERIC;
	}
	int err = movie->readData(offset, (uint32_t)sampleSize, sample->data);
	if (err == MP4_OK) {
		sample->size = sampleSize;
		*outSize = sampleSize;
	}
	return err;
}

/* Audio sample extractor (minimum 8KB buffer) */
class Mp4AudioExtractor : public Mp4SampleExtractor {
public:
	virtual ~Mp4AudioExtractor() {}
	virtual int getOutputSize(int sampleSize) { return (sampleSize < 0x2000) ? 0x2000 : sampleSize; }
	virtual int extractSample(Mp4DemuxSample *sample, Mp4Movie *movie, int64_t offset, int sampleSize, int *outSize);
};

int Mp4AudioExtractor::extractSample(Mp4DemuxSample *sample, Mp4Movie *movie, int64_t offset, int sampleSize, int *outSize)
{
	if (mp4EnsureSampleCapacity(sample, sampleSize) == 0) {
		*outSize = 0;
		return MP4_ERR_GENERIC;
	}
	int err = movie->readData(offset, (uint32_t)sampleSize, sample->data);
	if (err == MP4_OK) {
		sample->size = sampleSize;
		*outSize = sampleSize;
	}
	return err;
}

/* H.264/AVC Annex-B sample extractor */
class Mp4AvcExtractor : public Mp4SampleExtractor {
public:
	Mp4AvcExtractor(const uint8_t *avcCData, int32_t avcCSize);
	virtual ~Mp4AvcExtractor();
	virtual int getOutputSize(int sampleSize);
	virtual void reset() { emitConfigNals = 1; }
	virtual int extractSample(Mp4DemuxSample *sample, Mp4Movie *movie, int64_t offset, int sampleSize, int *outSize);

	int appendNal4ByteStartCode(Mp4DemuxSample *sample, const uint8_t *nalLenPtr, int32_t *outConsumed);
	int appendNal3ByteStartCode(Mp4DemuxSample *sample, const uint8_t *nalLenPtr, int32_t *outConsumed);

	int32_t  nalLengthSize;
	uint8_t *avcCBuffer;
	int32_t  avcCLength;
	uint8_t  emitConfigNals;
	uint8_t  pad[3];
};

Mp4AvcExtractor::Mp4AvcExtractor(const uint8_t *avcCData, int32_t avcCSize)
{
	this->nalLengthSize = 0;
	this->avcCLength = avcCSize;
	this->emitConfigNals = 1;
	this->avcCBuffer = (uint8_t *)mp4MspaceAllocZero(4, (SceSize)avcCSize);
	if (this->avcCBuffer != NULL)
		sceClibMemcpy_safe(this->avcCBuffer, avcCData, (SceSize)avcCSize);
	if (avcCSize > 4)
		this->nalLengthSize = (avcCData[4] & 3) + 1;
}

Mp4AvcExtractor::~Mp4AvcExtractor()
{
	mp4MspaceFree(this->avcCBuffer);
	this->avcCBuffer = NULL;
}

int Mp4AvcExtractor::getOutputSize(int sampleSize)
{
	int32_t extra = (this->nalLengthSize < 4) ? (12 * (4 - this->nalLengthSize)) : 0;
	return sampleSize + extra + this->avcCLength;
}

int Mp4AvcExtractor::appendNal4ByteStartCode(Mp4DemuxSample *sample, const uint8_t *nalLenPtr, int32_t *outConsumed)
{
	SceSize nalLen = mp4ReadBe16(nalLenPtr);
	if (sample->capacity < sample->size + (int32_t)nalLen + 4)
		return 0;

	uint8_t *dst = sample->data + sample->size;
	dst[0] = 0;
	dst[1] = 0;
	dst[2] = 0;
	dst[3] = 1;
	sceClibMemcpy_safe(dst + 4, nalLenPtr + 2, nalLen);
	sample->size += (int32_t)nalLen + 4;
	*outConsumed = (int32_t)nalLen + 2;
	return 1;
}

int Mp4AvcExtractor::appendNal3ByteStartCode(Mp4DemuxSample *sample, const uint8_t *nalLenPtr, int32_t *outConsumed)
{
	SceSize nalLen = mp4ReadBe16(nalLenPtr);
	if (sample->capacity < sample->size + (int32_t)nalLen + 4)
		return 0;

	uint8_t *dst = sample->data + sample->size;
	dst[0] = 0;
	dst[1] = 0;
	dst[2] = 1;
	sceClibMemcpy_safe(dst + 3, nalLenPtr + 2, nalLen);
	sample->size += (int32_t)nalLen + 3;
	*outConsumed = (int32_t)nalLen + 2;
	return 1;
}

int Mp4AvcExtractor::extractSample(Mp4DemuxSample *sample, Mp4Movie *movie, int64_t offset, int sampleSize, int *outSize)
{
	if (this->nalLengthSize == 0 || this->nalLengthSize >= 5) {
		*outSize = 0;
		return MP4_ERR_INVALID_PARAM;
	}

	sample->size = 0;
	int32_t startCodeSlack = 4 * (4 - this->nalLengthSize);

	if (this->emitConfigNals != 0) {
		this->emitConfigNals = 0;
		if (mp4EnsureSampleCapacity(sample, startCodeSlack + this->avcCLength + sampleSize + 4) == 0) {
			*outSize = 0;
			return MP4_ERR_NO_MEMORY;
		}
		if (this->avcCBuffer == NULL) {
			*outSize = 0;
			return MP4_ERR_NO_MEMORY;
		}

		const uint8_t *cur = this->avcCBuffer + 6;
		int spsCount = this->avcCBuffer[5] & 0x1F;
		for (int i = 0; i < spsCount; i++) {
			int32_t consumed = 0;
			if (this->appendNal3ByteStartCode(sample, cur, &consumed) == 0) {
				*outSize = 0;
				return MP4_ERR_GENERIC;
			}
			cur += consumed;
		}

		int ppsCount = *cur++;
		for (int i = 0; i < ppsCount; i++) {
			int32_t consumed = 0;
			if (this->appendNal4ByteStartCode(sample, cur, &consumed) == 0) {
				*outSize = 0;
				return MP4_ERR_GENERIC;
			}
			cur += consumed;
		}
	} else {
		if (mp4EnsureSampleCapacity(sample, startCodeSlack + sampleSize) == 0) {
			*outSize = 0;
			return MP4_ERR_NO_MEMORY;
		}
	}

	int64_t curOffset = offset;
	int32_t remaining = sampleSize;

	while (remaining > this->nalLengthSize) {
		uint8_t lenBytes[4];
		int err = movie->readData(curOffset, (uint32_t)this->nalLengthSize, lenBytes);
		if (err != MP4_OK) {
			*outSize = 0;
			return err;
		}

		curOffset += this->nalLengthSize;
		remaining -= this->nalLengthSize;

		int32_t nalLen = 0;
		for (int i = 0; i < this->nalLengthSize; i++)
			nalLen = (nalLen << 8) | lenBytes[i];

		if (remaining < nalLen) {
			*outSize = 0;
			return MP4_ERR_GENERIC;
		}

		if (mp4EnsureSampleCapacity(sample, sample->size + nalLen + 4) == 0) {
			*outSize = 0;
			return MP4_ERR_NO_MEMORY;
		}

		uint8_t *dst = sample->data + sample->size;
		dst[0] = 0;
		dst[1] = 0;
		dst[2] = 0;
		dst[3] = 1;

		err = movie->readData(curOffset, (uint32_t)nalLen, dst + 4);
		if (err != MP4_OK) {
			*outSize = 0;
			return err;
		}

		sample->size += nalLen + 4;
		curOffset += nalLen;
		remaining -= nalLen;
	}

	*outSize = sample->size;
	return MP4_OK;
}

/* SPS list helpers */
static Mp4ListNode<Mp4AvcSpsInfo> *mp4SpsListGetAt(Mp4ListNode<Mp4AvcSpsInfo> *node, uint32_t curIdx, uint32_t targetIdx)
{
	while (curIdx != targetIdx) {
		if (curIdx > targetIdx) {
			node = node->prev;
			curIdx--;
		} else {
			node = node->next;
			curIdx++;
		}
	}
	return node;
}

static Mp4ListNode<Mp4AvcSpsInfo> *mp4SpsListFindPrev(Mp4ListNode<Mp4AvcSpsInfo> *node, Mp4ListNode<Mp4AvcSpsInfo> *target)
{
	if (node == target)
		return node->prev;
	return mp4SpsListFindPrev(node->next, target);
}

Mp4StreamFormat::Mp4StreamFormat(int32_t inFlags)
{
	SceKernelLwMutexOptParam opt;
	Mp4AvcSpsInfo zeroSps;

	this->format = MP4_FORMAT_UNKNOWN;
	this->codecConfig = NULL;
	this->codecConfigSize = 0;
	this->esdsDecoderConfig = NULL;
	this->esdsDecoderConfigSize = 0;
	this->videoWidth = 0;
	this->videoHeight = 0;
	this->avgFrameDuration = 0;
	this->rawFourcc = 0;
	this->rawBitDepth = 0;
	this->defaultSpsIndex = 0;
	this->extractor = NULL;

	this->spsList.head = NULL;
	this->spsList.tail = NULL;
	this->spsList.count = 0;
	this->spsList.totalBytes = 0;
	this->spsList.isShallowCopy = 0;
	this->spsList.beginIter = NULL;
	this->spsList.endIter = NULL;

	sceClibMemset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	sceKernelCreateLwMutex(&this->spsList.mutex, "0Demux_Stl_Handler_Mutex ", 0, 0, &opt);

	sceClibMemset(&zeroSps, 0, sizeof(zeroSps));
	Mp4ListNode<Mp4AvcSpsInfo> *sentinel = (Mp4ListNode<Mp4AvcSpsInfo> *)mp4MspaceAllocZero(8, sizeof(Mp4ListNode<Mp4AvcSpsInfo>));
	if (sentinel != NULL) {
		sentinel->prev = NULL;
		sentinel->next = NULL;
		sceClibMemcpy_safe(&sentinel->value, &zeroSps, sizeof(zeroSps));
	}
	this->spsList.sentinel = sentinel;
	this->streamFlags = inFlags;

	sceClibMemset(&this->selectedDesc, 0, sizeof(this->selectedDesc));
	sceClibMemset(&this->tx3gDisplayFlags, 0, 40);
	this->audioChannels = 0;
	this->audioBitsPerSample = 0;
	this->audioSampleRate = 0;
}

Mp4StreamFormat::~Mp4StreamFormat()
{
	if (this->esdsDecoderConfig != NULL) {
		mp4MspaceFree(this->esdsDecoderConfig);
		this->esdsDecoderConfig = NULL;
	}
	if (this->codecConfig != NULL) {
		mp4MspaceFree(this->codecConfig);
		this->codecConfig = NULL;
	}
	if (this->extractor != NULL) {
		delete this->extractor;
		this->extractor = NULL;
	}
	if (this->tx3gFonts != NULL) {
		for (uint16_t i = 0; i < this->tx3gFontCount; i++) {
			if (this->tx3gFonts[i].fontName != NULL) {
				mp4MspaceFree(this->tx3gFonts[i].fontName);
				this->tx3gFonts[i].fontName = NULL;
			}
		}
		mp4MspaceFree(this->tx3gFonts);
		this->tx3gFonts = NULL;
	}

	while (this->spsList.count != 0) {
		sceKernelLockLwMutex(&this->spsList.mutex, 1, NULL);
		if (this->spsList.count != 0 && this->spsList.tail != NULL) {
			Mp4ListNode<Mp4AvcSpsInfo> *tail = this->spsList.tail;
			Mp4ListNode<Mp4AvcSpsInfo> *prev = mp4SpsListFindPrev(tail, tail);
			if (prev != NULL)
				prev->next = NULL;
			mp4MspaceFree(tail);
			this->spsList.tail = prev;
			this->spsList.count--;
			this->spsList.totalBytes -= sizeof(Mp4ListNode<Mp4AvcSpsInfo>);
		}
		sceKernelUnlockLwMutex(&this->spsList.mutex, 1);
	}

	if (this->spsList.isShallowCopy == 0) {
		if (this->spsList.beginIter != NULL)
			delete this->spsList.beginIter;
		if (this->spsList.endIter != NULL)
			delete this->spsList.endIter;
		if (this->spsList.sentinel != NULL)
			mp4MspaceFree(this->spsList.sentinel);
	}
	sceKernelDeleteLwMutex(&this->spsList.mutex);
}

int Mp4StreamFormat::parseEsdsAtom(Mp4Atom *esdsAtom)
{
	uint8_t *buf = NULL;
	if (esdsAtom != NULL) {
		buf = esdsAtom->mapBuffer();
		if (buf != NULL)
			buf += esdsAtom->getHeaderSize();
	}
	if (buf == NULL || buf[0] != 0) {
		if (esdsAtom != NULL)
			esdsAtom->unmapBuffer();
		return 0;
	}

	int32_t payloadSize = (int32_t)esdsAtom->getSize() - esdsAtom->getHeaderSize();
	Mp4EsdsDescriptor esDesc;
	sceClibMemset(&esDesc, 0, sizeof(esDesc));
	if (mp4ParseEsdsDescriptor(&esDesc, buf + 4, payloadSize - 4) == 0 || esDesc.tag != 3) {
		esdsAtom->unmapBuffer();
		return 0;
	}

	uint8_t esFlags = buf[esDesc.headerLength + 6];
	int32_t subOff = 3;
	if ((esFlags & 0x80u) != 0)
		subOff += 2;
	if ((esFlags & 0x40u) != 0)
		subOff += buf[esDesc.headerLength + 4 + subOff] + 1;
	if ((esFlags & 0x20u) != 0)
		subOff += 2;

	Mp4EsdsDescriptor decCfgDesc;
	sceClibMemset(&decCfgDesc, 0, sizeof(decCfgDesc));
	if (mp4ParseEsdsSubDescriptor(&esDesc, subOff, &decCfgDesc) == 0) {
		esdsAtom->unmapBuffer();
		return 0;
	}

	this->esdsDecoderConfigSize = decCfgDesc.payloadSize;
	this->esdsDecoderConfig = (uint8_t *)mp4MspaceAllocZero(4, (SceSize)decCfgDesc.payloadSize);
	if (this->esdsDecoderConfig == NULL) {
		esdsAtom->unmapBuffer();
		return 0;
	}
	sceClibMemcpy_safe(this->esdsDecoderConfig, decCfgDesc.rawPtr + decCfgDesc.headerLength, (SceSize)this->esdsDecoderConfigSize);

	Mp4EsdsDescriptor decSpecInfo;
	sceClibMemset(&decSpecInfo, 0, sizeof(decSpecInfo));
	if (mp4ParseEsdsSubDescriptor(&decCfgDesc, 13, &decSpecInfo) == 0) {
		esdsAtom->unmapBuffer();
		return 0;
	}

	this->codecConfigSize = decSpecInfo.payloadSize;
	this->codecConfig = (uint8_t *)mp4MspaceAllocZero(4, (SceSize)decSpecInfo.payloadSize);
	if (this->codecConfig == NULL) {
		esdsAtom->unmapBuffer();
		return 0;
	}
	sceClibMemcpy_safe(this->codecConfig, decSpecInfo.rawPtr + decSpecInfo.headerLength, (SceSize)this->codecConfigSize);

	esdsAtom->unmapBuffer();
	return 1;
}

Mp4AvcSpsInfo *Mp4StreamFormat::getSpsInfo(uint32_t index)
{
	uint32_t count = this->spsList.count;
	uint32_t idx = (index == 0 || index > count) ? this->defaultSpsIndex : index;
	if (idx >= count)
		idx = 0;
	if (idx >= count)
		return &this->spsList.sentinel->value;
	if (idx >= (count >> 1))
		return &mp4SpsListGetAt(this->spsList.tail, count - 1, idx)->value;
	return &mp4SpsListGetAt(this->spsList.head, 0, idx)->value;
}

uint32_t Mp4StreamFormat::bumpAvcLevel(Mp4AvcSpsInfo *sps)
{
	uint32_t level = sps->levelIdc;
	if (level >= 10 && level <= 12) {
		level++;
		sps->levelIdc = (uint8_t)level;
		return level;
	}
	switch (level) {
	case 13:
		sps->levelIdc = 20;
		return 20;
	case 20:
	case 21:
		level++;
		sps->levelIdc = (uint8_t)level;
		return level;
	case 22:
		sps->levelIdc = 30;
		return 30;
	case 30:
	case 31:
		sps->levelIdc = 32;
		return 32;
	case 32:
		sps->levelIdc = 40;
		return 40;
	default:
		return level;
	}
}

int Mp4StreamFormat::readExpGolomb(uint8_t *bitOffset, int32_t *outVal, const uint8_t *data, uint32_t maxVal)
{
	uint32_t maxBits = 0;
	if (maxVal > 1) {
		do {
			maxBits++;
		} while ((1ULL << maxBits) < (uint64_t)maxVal);
	}

	int32_t bitPos = *bitOffset;
	uint32_t leadingZeros = 0;
	uint8_t curByte = (uint8_t)(data[0] << bitPos);

	while (true) {
		if ((bitPos & 7) == 0)
			curByte = data[bitPos >> 3];
		uint8_t bit = (uint8_t)(curByte >> 7);
		curByte = (uint8_t)((curByte & 0x7Fu) << 1);
		bitPos++;
		if (bit != 0)
			break;
		leadingZeros++;
	}

	if (leadingZeros > maxBits)
		return -1;

	int32_t suffix = 0;
	for (uint32_t i = 0; i < leadingZeros; i++) {
		if ((bitPos & 7) == 0)
			curByte = data[bitPos >> 3];
		suffix = (suffix << 1) | ((curByte & 0x80u) != 0 ? 1 : 0);
		curByte = (uint8_t)((curByte & 0x7Fu) << 1);
		bitPos++;
	}

	*bitOffset = (uint8_t)(*bitOffset + 2 * leadingZeros + 1);
	*outVal = (leadingZeros != 0) ? ((1 << leadingZeros) + suffix - 1) : 0;
	return 0;
}

int Mp4StreamFormat::readBits(const uint8_t **ptr, uint8_t *bitOffset, uint32_t numBits, int32_t *outVal, uint32_t maxVal, bool isExpGolomb)
{
	if (isExpGolomb) {
		if (this->readExpGolomb(bitOffset, outVal, *ptr, maxVal) < 0)
			return 0;
		*ptr += (*bitOffset >> 3);
		*bitOffset &= 7u;
		return 1;
	}

	if (numBits == 0)
		return 0;

	if (numBits == 1) {
		*outVal = (((**ptr << *bitOffset) & 0x80u) != 0) ? 1 : 0;
		uint8_t nextBit = (uint8_t)(*bitOffset + 1);
		*ptr += (nextBit >> 3);
		*bitOffset = nextBit & 7u;
		return 1;
	}

	if (numBits >= 8) {
		uint32_t numBytes = (uint8_t)(numBits >> 3);
		int32_t accum = 0;
		const uint8_t *cur = *ptr;
		for (uint32_t i = 0; i < numBytes; i++) {
			uint8_t hi = (uint8_t)(cur[i] << *bitOffset);
			uint8_t lo = (uint8_t)(cur[i + 1] >> (8 - *bitOffset));
			accum = (accum << 8) | (uint8_t)(hi | lo);
		}
		*outVal = accum;
		*ptr = cur + numBytes + (*bitOffset >> 3);
		*bitOffset &= 7u;
		return 1;
	}

	int32_t accum = 0;
	for (uint32_t i = 0; i < numBits; i++) {
		accum = (accum << 1) | ((((**ptr << *bitOffset) & 0x80u) != 0) ? 1 : 0);
		uint8_t nextBit = (uint8_t)(*bitOffset + 1);
		*ptr += (nextBit >> 3);
		*bitOffset = nextBit & 7u;
	}
	*outVal = accum;
	return 1;
}

bool Mp4StreamFormat::skipHrdParameters(const uint8_t **ptr, uint8_t *bitOffset)
{
	int32_t cpbCntMinus1 = 0;
	int32_t discarded = 0;

	if (this->readBits(ptr, bitOffset, 0, &cpbCntMinus1, 32, true) == 0)
		return false;
	if (this->readBits(ptr, bitOffset, 4, &discarded, 0, false) == 0)
		return false;
	if (this->readBits(ptr, bitOffset, 4, &discarded, 0, false) == 0)
		return false;

	if (cpbCntMinus1 != -1) {
		for (uint32_t i = 0; i <= (uint32_t)cpbCntMinus1; i++) {
			if (this->readBits(ptr, bitOffset, 0, &discarded, 0xFFFFFFFDu, true) == 0)
				return false;
			if (this->readBits(ptr, bitOffset, 0, &discarded, 0xFFFFFFFDu, true) == 0)
				return false;
			uint8_t nextBit = (uint8_t)(*bitOffset + 1);
			*ptr += (nextBit >> 3);
			*bitOffset = nextBit & 7u;
		}
	}

	if (this->readBits(ptr, bitOffset, 5, &discarded, 0, false) == 0)
		return false;
	if (this->readBits(ptr, bitOffset, 5, &discarded, 0, false) == 0)
		return false;
	if (this->readBits(ptr, bitOffset, 5, &discarded, 0, false) == 0)
		return false;
	if (this->readBits(ptr, bitOffset, 5, &discarded, 0, false) == 0)
		return false;
	return true;
}

static inline uint8_t mp4ReadBit(const uint8_t **ptr, uint8_t *bitOff)
{
	uint8_t bit = (((**ptr << *bitOff) & 0x80u) != 0) ? 1u : 0u;
	uint8_t next = (uint8_t)(*bitOff + 1);
	*ptr += (next >> 3);
	*bitOff = next & 7u;
	return bit;
}

static inline uint8_t mp4ReadAlignedOrUnalignedByte(const uint8_t **ptr, uint8_t bitOff)
{
	uint8_t val = (bitOff != 0) ? (uint8_t)(((*ptr)[0] << bitOff) | ((*ptr)[1] >> (8 - bitOff))) : (*ptr)[0];
	(*ptr)++;
	return val;
}

void Mp4StreamFormat::parseVuiParameters(Mp4AvcSpsInfo *sps, const uint8_t *data, int startBitOff)
{
	const uint8_t *ptr = data;
	uint8_t bitOff = (uint8_t)startBitOff;
	int32_t val = 0;

	if (mp4ReadBit(&ptr, &bitOff) != 0) {
		uint8_t aspectIdc = mp4ReadAlignedOrUnalignedByte(&ptr, bitOff);
		sps->aspectRatioIdc = aspectIdc;
		switch (aspectIdc) {
		case 0:
		case 1: case 2: case 4: case 5: case 7:
		case 10: case 11: case 12: case 13: case 14: case 15:
			sps->sarRatio = 1.0f;
			break;
		case 3:
			sps->sarRatio = 0.0f;
			break;
		case 6: case 8: case 9: case 16:
			sps->sarRatio = 2.0f;
			break;
		case 255: {
			uint16_t sarW = (uint16_t)((mp4ReadAlignedOrUnalignedByte(&ptr, bitOff) << 8) | mp4ReadAlignedOrUnalignedByte(&ptr, bitOff));
			uint16_t sarH = (uint16_t)((mp4ReadAlignedOrUnalignedByte(&ptr, bitOff) << 8) | mp4ReadAlignedOrUnalignedByte(&ptr, bitOff));
			sps->sarWidth = sarW;
			sps->sarHeight = sarH;
			if (sarH != 0)
				sps->sarRatio = (float)sarW / (float)sarH;
			break;
		}
		default:
			break;
		}
	}

	if (mp4ReadBit(&ptr, &bitOff) != 0)
		mp4ReadBit(&ptr, &bitOff);

	if (mp4ReadBit(&ptr, &bitOff) != 0) {
		if (this->readBits(&ptr, &bitOff, 3, (int32_t *)&sps->videoFormat, 8, false) == 0)
			return;
		mp4ReadBit(&ptr, &bitOff);
		if (mp4ReadBit(&ptr, &bitOff) != 0) {
			mp4ReadAlignedOrUnalignedByte(&ptr, bitOff);
			mp4ReadAlignedOrUnalignedByte(&ptr, bitOff);
			mp4ReadAlignedOrUnalignedByte(&ptr, bitOff);
		}
	}

	if (mp4ReadBit(&ptr, &bitOff) != 0) {
		if (this->readBits(&ptr, &bitOff, 0, &val, 255, true) == 0)
			return;
		if (this->readBits(&ptr, &bitOff, 0, &val, 255, true) == 0)
			return;
	}

	if (mp4ReadBit(&ptr, &bitOff) != 0) {
		uint32_t fieldBits = 32;
		if (sps->aspectRatioIdc != 0) {
			ptr += 8;
			fieldBits = 8;
		}
		if (this->readBits(&ptr, &bitOff, fieldBits, (int32_t *)&sps->numUnitsInTick, 0, false) == 0)
			return;
		if (this->readBits(&ptr, &bitOff, fieldBits, (int32_t *)&sps->timeScale, 0, false) == 0)
			return;
		mp4ReadBit(&ptr, &bitOff);
	}

	uint8_t nalHrd = mp4ReadBit(&ptr, &bitOff);
	if (nalHrd != 0) {
		if (!this->skipHrdParameters(&ptr, &bitOff))
			return;
	}
	uint8_t vclHrd = mp4ReadBit(&ptr, &bitOff);
	if (vclHrd != 0) {
		if (!this->skipHrdParameters(&ptr, &bitOff))
			return;
	}
	if (nalHrd != 0 || vclHrd != 0)
		mp4ReadBit(&ptr, &bitOff);

	mp4ReadBit(&ptr, &bitOff);

	if (mp4ReadBit(&ptr, &bitOff) != 0) {
		mp4ReadBit(&ptr, &bitOff);
		if (this->readBits(&ptr, &bitOff, 0, &val, 255, true) == 0) return;
		if (this->readBits(&ptr, &bitOff, 0, &val, 255, true) == 0) return;
		if (this->readBits(&ptr, &bitOff, 0, &val, 255, true) == 0) return;
		if (this->readBits(&ptr, &bitOff, 0, &val, 255, true) == 0) return;
		if (this->readBits(&ptr, &bitOff, 0, &val, 255, true) == 0) return;
		sps->maxNumReorderFrames = (uint32_t)val;
		if (this->readBits(&ptr, &bitOff, 0, &val, 255, true) != 0)
			sps->maxDecFrameBuffering = (uint32_t)val;
	}
}

uint32_t Mp4StreamFormat::computeMaxDpbFrames(const Mp4AvcSpsInfo *sps)
{
	if (sps->picWidthMbs == 0 || sps->picHeightMbs == 0)
		return 0;

	uint32_t maxDpbMbs = 0;
	switch (sps->levelIdc) {
	case 10: maxDpbMbs = 396; break;
	case 11: maxDpbMbs = 900; break;
	case 12:
	case 13:
	case 20: maxDpbMbs = 2376; break;
	case 21: maxDpbMbs = 4752; break;
	case 22:
	case 30: maxDpbMbs = 8100; break;
	case 31: maxDpbMbs = 18000; break;
	case 32: maxDpbMbs = 20480; break;
	case 40:
	case 41: maxDpbMbs = 32768; break;
	case 42: maxDpbMbs = 34816; break;
	case 50: maxDpbMbs = 110400; break;
	case 51: maxDpbMbs = 184320; break;
	default: maxDpbMbs = 0; break;
	}

	return maxDpbMbs / (uint32_t)(sps->picWidthMbs * sps->picHeightMbs);
}

void Mp4StreamFormat::parseAvcSpsList()
{
	const uint8_t *avcC = this->codecConfig;
	uint32_t spsCount = avcC[5] & 0x1Fu;
	if (spsCount == 0)
		return;

	if (spsCount == 1)
		this->defaultSpsIndex = 0;

	const uint8_t *cur = avcC + 6;
	for (uint32_t i = 0; i < spsCount; i++) {
		uint16_t nalSize = mp4ReadBe16(cur);
		cur += 2;

		Mp4AvcSpsInfo sps;
		sceClibMemset(&sps, 0, sizeof(sps));
		sps.nalSize = nalSize;
		sps.profileIdc = cur[1];
		sps.levelIdc = cur[3];

		const uint8_t *bitPtr = cur + 4;
		uint8_t bitOff = 0;
		int32_t val = 0;

		if (this->readExpGolomb(&bitOff, &val, bitPtr, 31) == -1)
			break;
		sps.spsId = (uint32_t)val;
		bitPtr += (bitOff >> 3);
		bitOff &= 7u;

		if (sps.profileIdc == 100 || sps.profileIdc == 110 || sps.profileIdc == 122 ||
		    sps.profileIdc == 244 || sps.profileIdc == 44 || sps.profileIdc == 83 || sps.profileIdc == 86) {
			int32_t chromaFmt = 0;
			if (this->readExpGolomb(&bitOff, &chromaFmt, bitPtr, 3) == -1)
				break;
			bitPtr += (bitOff >> 3);
			bitOff &= 7u;
			if (chromaFmt == 3)
				mp4ReadBit(&bitPtr, &bitOff);
			if (this->readExpGolomb(&bitOff, &val, bitPtr, 3) == -1)
				break;
			bitPtr += (bitOff >> 3);
			bitOff &= 7u;
			if (this->readExpGolomb(&bitOff, &val, bitPtr, 3) == -1)
				break;
			bitPtr += (bitOff >> 3);
			bitOff &= 7u;
			mp4ReadBit(&bitPtr, &bitOff);
			if (mp4ReadBit(&bitPtr, &bitOff) != 0) {
				int listCount = (chromaFmt == 3) ? 12 : 8;
				for (int j = 0; j < listCount; j++)
					mp4ReadBit(&bitPtr, &bitOff);
			}
		}

		if (this->readExpGolomb(&bitOff, &val, bitPtr, 28) == -1)
			break;
		sps.maxFrameNum = 1u << (val + 4);
		bitPtr += (bitOff >> 3);
		bitOff &= 7u;

		if (this->readExpGolomb(&bitOff, &val, bitPtr, 2) == -1)
			break;
		sps.picOrderCntType = val;
		bitPtr += (bitOff >> 3);
		bitOff &= 7u;

		if (sps.picOrderCntType == 0) {
			if (this->readExpGolomb(&bitOff, &val, bitPtr, 28) == -1)
				break;
			bitPtr += (bitOff >> 3);
			bitOff &= 7u;
		} else if (sps.picOrderCntType == 1) {
			mp4ReadBit(&bitPtr, &bitOff);
			if (this->readExpGolomb(&bitOff, &val, bitPtr, 31) == -1)
				break;
			bitPtr += (bitOff >> 3);
			bitOff &= 7u;
			if (this->readExpGolomb(&bitOff, &val, bitPtr, 31) == -1)
				break;
			bitPtr += (bitOff >> 3);
			bitOff &= 7u;
			int32_t numRefInCycle = 0;
			if (this->readExpGolomb(&bitOff, &numRefInCycle, bitPtr, 255) == -1)
				break;
			bitPtr += (bitOff >> 3);
			bitOff &= 7u;
			bool failed = false;
			for (int32_t j = 0; j < numRefInCycle; j++) {
				if (this->readExpGolomb(&bitOff, &val, bitPtr, 31) == -1) {
					failed = true;
					break;
				}
				bitPtr += (bitOff >> 3);
				bitOff &= 7u;
			}
			if (failed)
				break;
		}

		if (this->readExpGolomb(&bitOff, &val, bitPtr, 16) == -1)
			break;
		sps.maxNumRefFrames = (uint32_t)val;
		bitPtr += (bitOff >> 3);
		bitOff &= 7u;
		mp4ReadBit(&bitPtr, &bitOff);

		if (this->readExpGolomb(&bitOff, &val, bitPtr, 1280) == -1)
			break;
		sps.picWidthMbs = val + 1;
		bitPtr += (bitOff >> 3);
		bitOff &= 7u;

		if (this->readExpGolomb(&bitOff, &val, bitPtr, 1280) == -1)
			break;
		sps.picHeightMbs = val + 1;
		bitPtr += (bitOff >> 3);
		bitOff &= 7u;

		sps.frameMbsOnlyFlag = mp4ReadBit(&bitPtr, &bitOff);
		if (sps.frameMbsOnlyFlag == 0)
			mp4ReadBit(&bitPtr, &bitOff);
		mp4ReadBit(&bitPtr, &bitOff);

		sps.frameCroppingFlag = mp4ReadBit(&bitPtr, &bitOff);
		if (sps.frameCroppingFlag != 0) {
			bool failed = false;
			for (int j = 0; j < 4; j++) {
				if (this->readExpGolomb(&bitOff, &val, bitPtr, 1280) == -1) {
					failed = true;
					break;
				}
				bitPtr += (bitOff >> 3);
				bitOff &= 7u;
			}
			if (failed)
				break;
		}

		uint32_t maxDpb = this->computeMaxDpbFrames(&sps);
		if (maxDpb > 15)
			maxDpb = 15;

		sps.vuiPresentFlag = mp4ReadBit(&bitPtr, &bitOff);
		if (sps.vuiPresentFlag != 0)
			this->parseVuiParameters(&sps, bitPtr, bitOff);

		if (sps.maxDecFrameBuffering != 0) {
			if (sps.maxDecFrameBuffering > maxDpb)
				sps.maxDecFrameBuffering = maxDpb;
			else if (sps.maxDecFrameBuffering < maxDpb)
				this->bumpAvcLevel(&sps);
		} else {
			sps.maxDecFrameBuffering = maxDpb;
			if (maxDpb < sps.maxNumRefFrames) {
				sps.maxDecFrameBuffering = sps.maxNumRefFrames;
				this->bumpAvcLevel(&sps);
			}
		}

		if (sps.maxDecFrameBuffering == 1)
			this->bumpAvcLevel(&sps);

		sps.valid = 1;

		sceKernelLockLwMutex(&this->spsList.mutex, 1, NULL);
		Mp4ListNode<Mp4AvcSpsInfo> *node = (Mp4ListNode<Mp4AvcSpsInfo> *)mp4MspaceAllocZero(8, sizeof(Mp4ListNode<Mp4AvcSpsInfo>));
		if (node != NULL) {
			node->prev = NULL;
			node->next = NULL;
			sceClibMemcpy_safe(&node->value, &sps, sizeof(sps));
		}
		if (this->spsList.tail != NULL) {
			this->spsList.tail->next = node;
			node->prev = this->spsList.tail;
		} else {
			this->spsList.head = node;
		}
		this->spsList.tail = node;
		this->spsList.count++;
		this->spsList.totalBytes += sizeof(Mp4ListNode<Mp4AvcSpsInfo>);
		sceKernelUnlockLwMutex(&this->spsList.mutex, 1);

		cur += nalSize;
	}
}

int Mp4StreamFormat::parseTimedTextConfig()
{
	uint32_t len = (uint32_t)this->codecConfigSize;
	const uint8_t *buf = this->codecConfig;
	if (len == 0 || buf == NULL)
		return 0;

	if (len >= 12) {
		this->tx3gDisplayFlags = mp4ReadBe32(buf + 8);
		if (len >= 18) {
			this->tx3gHorizJust = buf[12];
			this->tx3gVertJust = buf[13];
			this->tx3gBgColor[0] = buf[14];
			this->tx3gBgColor[1] = buf[15];
			this->tx3gBgColor[2] = buf[16];
			this->tx3gBgColor[3] = buf[17];
			if (len >= 26) {
				this->tx3gBoxTop = (int16_t)mp4ReadBe16(buf + 18);
				this->tx3gBoxLeft = (int16_t)mp4ReadBe16(buf + 20);
				this->tx3gBoxBottom = (int16_t)mp4ReadBe16(buf + 22);
				this->tx3gBoxRight = (int16_t)mp4ReadBe16(buf + 24);
				if (len >= 38) {
					this->tx3gStartChar = mp4ReadBe16(buf + 26);
					this->tx3gEndChar = mp4ReadBe16(buf + 28);
					this->tx3gFontId = mp4ReadBe16(buf + 30);
					this->tx3gFaceStyle = buf[32];
					this->tx3gFontSize = buf[33];
					this->tx3gTextColor[0] = buf[34];
					this->tx3gTextColor[1] = buf[35];
					this->tx3gTextColor[2] = buf[36];
					this->tx3gTextColor[3] = buf[37];
					if (len > 46 && mp4ReadBe32(buf + 38) != 0 &&
					    buf[42] == 'f' && buf[43] == 't' && buf[44] == 'a' && buf[45] == 'b') {
						uint16_t fontCount = mp4ReadBe16(buf + 46);
						this->tx3gFontCount = fontCount;
						if (fontCount != 0) {
							this->tx3gFonts = (Mp4FontRecord *)mp4MspaceAllocZero(4, fontCount * sizeof(Mp4FontRecord));
							if (this->tx3gFonts != NULL) {
								uint32_t pos = 48;
								for (uint16_t i = 0; i < fontCount; i++) {
									this->tx3gFonts[i].fontId = mp4ReadBe16(buf + pos);
									this->tx3gFonts[i].nameLength = buf[pos + 2];
									if (this->tx3gFonts[i].nameLength != 0) {
										char *nameBuf = (char *)mp4MspaceAllocZero(1, this->tx3gFonts[i].nameLength);
										this->tx3gFonts[i].fontName = nameBuf;
										if (nameBuf == NULL)
											return 1;
										for (uint8_t j = 0; j < this->tx3gFonts[i].nameLength; j++)
											nameBuf[j] = (char)buf[pos + 3 + j];
										pos += this->tx3gFonts[i].nameLength + 3;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return 1;
}

int Mp4StreamFormat::parseAudioSampleEntry(Mp4Atom *sampleEntryAtom)
{
	uint8_t *buf = NULL;
	if (sampleEntryAtom != NULL) {
		buf = sampleEntryAtom->mapBuffer();
		if (buf != NULL)
			buf += sampleEntryAtom->getHeaderSize();
	}
	if (this->format == MP4_FORMAT_AAC && buf != NULL) {
		this->audioChannels = mp4ReadBe16(buf + 16);
		this->audioBitsPerSample = mp4ReadBe16(buf + 18);
		this->audioSampleRate = mp4ReadBe16(buf + 24);
	}
	if (sampleEntryAtom != NULL)
		sampleEntryAtom->unmapBuffer();
	return 1;
}

int Mp4StreamFormat::parseSampleDescription(int64_t frameDur, Mp4Atom *sampleEntryAtom)
{
	if (sampleEntryAtom == NULL)
		return 0;

	this->avgFrameDuration = frameDur;
	uint8_t *buf = sampleEntryAtom->mapBuffer();
	if (buf != NULL)
		buf += sampleEntryAtom->getHeaderSize();
	if (buf == NULL) {
		sampleEntryAtom->unmapBuffer();
		return 0;
	}

	uint32_t fourcc = sampleEntryAtom->getFourCC();
	bool parseChildrenGeneric = true;
	int64_t childScanOffset = 0;

	if (fourcc == FOURCC_MP4V) {
		this->format = MP4_FORMAT_MPEG4_VID;
		this->videoWidth = mp4ReadBe16(buf + 24);
		this->videoHeight = mp4ReadBe16(buf + 26);
		childScanOffset = 78;
	} else if (fourcc == FOURCC_JVT1) {
		this->format = MP4_FORMAT_AVC;
		this->videoWidth = mp4ReadBe16(buf + 24);
		this->videoHeight = mp4ReadBe16(buf + 26);
		childScanOffset = 78;
	} else if (fourcc == FOURCC_AVC1) {
		this->format = MP4_FORMAT_AVC;
		this->videoWidth = mp4ReadBe16(buf + 24);
		this->videoHeight = mp4ReadBe16(buf + 26);
		childScanOffset = 78;
		parseChildrenGeneric = false;
	} else if (fourcc == FOURCC_RAW || fourcc == FOURCC_I420 || fourcc == FOURCC_YUY2) {
		this->format = MP4_FORMAT_RAW_VID;
		this->rawFourcc = mp4Bswap32(fourcc);
		this->videoWidth = mp4ReadBe16(buf + 24);
		this->videoHeight = mp4ReadBe16(buf + 26);
		this->rawBitDepth = mp4ReadBe16(buf + 74);
		sampleEntryAtom->unmapBuffer();
		return 1;
	} else if (fourcc == FOURCC_MP4A) {
		this->format = MP4_FORMAT_AAC;
		int version = mp4ReadBe16(buf + 8);
		int hdrOff = 28;
		if (version == 1)
			hdrOff = 44;
		else if (version == 2)
			hdrOff = 64;
		this->parseAudioSampleEntry(sampleEntryAtom);
		childScanOffset = hdrOff;
		if (mp4ReadBe32(buf + hdrOff + 4) == FOURCC_WAVE && version > 0) {
			int64_t atomSz = sampleEntryAtom->getSize();
			for (int64_t scan = 28; scan < atomSz - 8; scan++) {
				if (mp4ReadBe32(buf + scan + 4) == FOURCC_WAVE) {
					this->format = MP4_FORMAT_PCM_AUDIO;
					break;
				}
			}
		}
	} else if (fourcc == FOURCC_LPCM || fourcc == FOURCC_ALAW || fourcc == FOURCC_ULAW ||
	           fourcc == FOURCC_SAMR || fourcc == FOURCC_SAWB) {
		this->format = MP4_FORMAT_PCM_AUDIO;
		childScanOffset = 28;
	} else if (fourcc == FOURCC_TX3G) {
		this->format = MP4_FORMAT_TIMEDTEXT;
		this->rawFourcc = mp4Bswap32(fourcc);
		int32_t cfgLen = (int32_t)sampleEntryAtom->getSize() - sampleEntryAtom->getHeaderSize();
		this->codecConfigSize = cfgLen;
		this->codecConfig = (uint8_t *)mp4MspaceAllocZero(4, (SceSize)cfgLen);
		if (this->codecConfig != NULL) {
			sceClibMemcpy_safe(this->codecConfig, buf, (SceSize)cfgLen);
			this->parseTimedTextConfig();
			sampleEntryAtom->unmapBuffer();
			return 1;
		}
		sampleEntryAtom->unmapBuffer();
		return 0;
	} else {
		sampleEntryAtom->unmapBuffer();
		return 0;
	}

	sampleEntryAtom->parseChildren(childScanOffset);
	int childCount = sampleEntryAtom->getChildCount();

	for (int i = 0; i < childCount; i++) {
		Mp4Atom *child = sampleEntryAtom->getChildAt(i);
		if (child == NULL) {
			sampleEntryAtom->unmapBuffer();
			return 0;
		}

		uint8_t *childBuf = child->mapBuffer();
		if (childBuf != NULL)
			childBuf += child->getHeaderSize();
		if (childBuf == NULL) {
			child->unmapBuffer();
			sampleEntryAtom->unmapBuffer();
			return 0;
		}

		int32_t childLen = (int32_t)child->getSize() - child->getHeaderSize();
		uint32_t childFourcc = child->getFourCC();

		if (this->format == MP4_FORMAT_AVC && !parseChildrenGeneric) {
			if (childFourcc == FOURCC_AVCC) {
				this->codecConfigSize = childLen;
				this->codecConfig = (uint8_t *)mp4MspaceAllocZero(4, (SceSize)childLen);
				if (this->codecConfig == NULL) {
					child->unmapBuffer();
					sampleEntryAtom->unmapBuffer();
					return 0;
				}
				sceClibMemcpy_safe(this->codecConfig, childBuf, (SceSize)childLen);
				this->parseAvcSpsList();
				this->getSpsInfo(0);
				child->unmapBuffer();
				break;
			}
			child->unmapBuffer();
			continue;
		}

		if (childFourcc == FOURCC_D263 || childFourcc == FOURCC_DAMR) {
			child->unmapBuffer();
			break;
		}
		if (childFourcc == FOURCC_ESDS) {
			if (this->parseEsdsAtom(child) == 0) {
				child->unmapBuffer();
				sampleEntryAtom->unmapBuffer();
				return 0;
			}
			child->unmapBuffer();
			break;
		}
		if (childFourcc == FOURCC_WAVE) {
			int waveCount = child->getChildCount();
			for (int j = 0; j < waveCount; j++) {
				Mp4Atom *waveChild = child->getChildAt(j);
				if (waveChild->getFourCC() == FOURCC_ESDS) {
					if (this->parseEsdsAtom(waveChild) == 0) {
						child->unmapBuffer();
						sampleEntryAtom->unmapBuffer();
						return 0;
					}
					break;
				}
			}
			if (this->codecConfigSize > 0) {
				child->unmapBuffer();
				break;
			}
		}

		child->unmapBuffer();
	}

	if (this->format == MP4_FORMAT_AAC || this->format == MP4_FORMAT_PCM_AUDIO ||
	    this->format == MP4_FORMAT_TIMEDTEXT || this->codecConfigSize > 0) {
		sampleEntryAtom->unmapBuffer();
		return 1;
	}

	sampleEntryAtom->unmapBuffer();
	return 0;
}

int Mp4StreamFormat::fillTimedTextDesc(Mp4StreamDescription *outDesc)
{
	sceClibMemset(outDesc, 0, sizeof(*outDesc));
	outDesc->streamType = 2;
	outDesc->codecType = 5;
	*((uint8_t *)&outDesc->heightOrBitsPerSample) = this->tx3gFontSize;
	return 1;
}

int Mp4StreamFormat::fillAudioDesc(Mp4StreamDescription *outDesc)
{
	outDesc->streamType = 1;
	outDesc->codecType = 2;

	if (this->audioChannels != 0 && this->audioSampleRate != 0 && this->audioBitsPerSample != 0) {
		outDesc->maxReorderOrChannels = this->audioChannels;
		outDesc->heightOrBitsPerSample = this->audioBitsPerSample;
		outDesc->widthOrSampleRate = this->audioSampleRate;
	} else if (this->codecConfigSize == 0) {
		outDesc->maxReorderOrChannels = 2;
		outDesc->widthOrSampleRate = 48000;
		outDesc->heightOrBitsPerSample = 16;
		return 1;
	} else {
		uint8_t byte0 = this->codecConfig[0];
		uint8_t byte1 = this->codecConfig[1];
		uint32_t srIdx = (((byte0 & 7u) << 1) | ((byte1 >> 7) & 1u));
		outDesc->heightOrBitsPerSample = 16;
		outDesc->maxReorderOrChannels = (byte1 >> 3) & 0xFu;
		outDesc->widthOrSampleRate = kAacSampleRates[srIdx];
	}

	const uint8_t *dec = this->esdsDecoderConfig;
	outDesc->refFramesOrBufferSize = ((uint32_t)dec[2] << 16) | ((uint32_t)dec[3] << 8) | dec[4];
	outDesc->profileOrBitrateMax = mp4ReadBe32(dec + 5);
	outDesc->levelOrBitrateAvg = mp4ReadBe32(dec + 9);

	if (this->codecConfigSize < 256) {
		outDesc->configSize = (uint16_t)this->codecConfigSize;
		sceClibMemcpy_safe(outDesc->configData, this->codecConfig, outDesc->configSize);
	}
	return 1;
}

void Mp4StreamFormat::applySpsToVideoDesc(Mp4StreamDescription *outDesc, uint32_t spsIndex)
{
	Mp4AvcSpsInfo *sps = this->getSpsInfo(spsIndex);
	if (sps->valid == 0)
		return;

	uint32_t codedW = (uint32_t)(16 * sps->picWidthMbs);
	if (outDesc->widthOrSampleRate < codedW)
		outDesc->widthOrSampleRate = codedW;
	uint32_t codedH = (uint32_t)(16 * sps->picHeightMbs);
	if (outDesc->heightOrBitsPerSample < codedH)
		outDesc->heightOrBitsPerSample = codedH;

	outDesc->profileOrBitrateMax = sps->profileIdc;
	outDesc->levelOrBitrateAvg = sps->levelIdc;
	outDesc->refFramesOrBufferSize = sps->maxNumRefFrames;
	outDesc->maxReorderOrChannels = sps->maxDecFrameBuffering;

	if (sps->vuiPresentFlag != 0)
		outDesc->aspectRatioFixed = (int64_t)(sps->sarRatio * 1000000.0f);
}

int Mp4StreamFormat::fillVideoDesc(Mp4StreamDescription *outDesc, uint32_t spsIndex)
{
	outDesc->streamType = 0;
	outDesc->codecType = 1;
	this->applySpsToVideoDesc(outDesc, spsIndex);
	outDesc->widthOrSampleRate = (uint32_t)this->videoWidth;
	outDesc->heightOrBitsPerSample = (uint32_t)this->videoHeight;
	outDesc->frameDuration = this->avgFrameDuration;
	if (this->codecConfigSize < 256) {
		outDesc->configSize = (uint16_t)this->codecConfigSize;
		sceClibMemcpy_safe(outDesc->configData, this->codecConfig, outDesc->configSize);
	}
	return 1;
}

int Mp4StreamFormat::getStreamDescription(Mp4StreamDescription *outDesc, uint32_t index, uint32_t spsIndex)
{
	if (outDesc == NULL)
		return 0;

	switch (this->format) {
	case MP4_FORMAT_AVC:
		if (index != 0)
			return 0;
		return this->fillVideoDesc(outDesc, spsIndex);
	case MP4_FORMAT_AAC:
		if (index != 0)
			return 0;
		return this->fillAudioDesc(outDesc);
	case MP4_FORMAT_TIMEDTEXT:
		return this->fillTimedTextDesc(outDesc);
	default:
		return 0;
	}
}

bool Mp4StreamFormat::selectStreamDescription(const Mp4StreamDescription *desc)
{
	if (desc == NULL)
		return false;

	if (this->selectedDesc.streamType == desc->streamType &&
	    this->selectedDesc.codecType == desc->codecType &&
	    this->selectedDesc.configSize == desc->configSize &&
	    sceClibMemcmp(this->selectedDesc.configData, desc->configData, desc->configSize) == 0) {
		if (this->selectedDesc.streamType == 0) {
			if (this->selectedDesc.frameDuration == desc->frameDuration &&
			    this->selectedDesc.heightOrBitsPerSample == desc->heightOrBitsPerSample &&
			    this->selectedDesc.widthOrSampleRate == desc->widthOrSampleRate)
				return true;
		} else if (this->selectedDesc.streamType == 1) {
			if (this->selectedDesc.maxReorderOrChannels == desc->maxReorderOrChannels &&
			    this->selectedDesc.heightOrBitsPerSample == desc->heightOrBitsPerSample &&
			    this->selectedDesc.widthOrSampleRate == desc->widthOrSampleRate)
				return true;
		} else {
			return true;
		}
	}

	if (this->extractor != NULL) {
		delete this->extractor;
		this->extractor = NULL;
	}

	this->selectedDesc = *desc;

	Mp4StreamDescription candidate;
	sceClibMemset(&candidate, 0, sizeof(candidate));
	bool matched = false;
	for (uint32_t i = 0; ; i++) {
		if (this->getStreamDescription(&candidate, i, 0) == 0)
			return false;
		if (candidate.streamType != desc->streamType ||
		    candidate.codecType != desc->codecType ||
		    candidate.configSize != desc->configSize ||
		    sceClibMemcmp(candidate.configData, desc->configData, candidate.configSize) != 0)
			continue;
		if (candidate.streamType == 0) {
			if (candidate.frameDuration == desc->frameDuration &&
			    candidate.heightOrBitsPerSample == desc->heightOrBitsPerSample &&
			    candidate.widthOrSampleRate == desc->widthOrSampleRate) {
				matched = true;
				break;
			}
		} else if (candidate.streamType == 1) {
			if (candidate.maxReorderOrChannels == desc->maxReorderOrChannels &&
			    candidate.heightOrBitsPerSample == desc->heightOrBitsPerSample &&
			    candidate.widthOrSampleRate == desc->widthOrSampleRate) {
				matched = true;
				break;
			}
		} else {
			matched = true;
			break;
		}
	}

	if (!matched)
		return false;

	if (this->format == MP4_FORMAT_AAC) {
		this->extractor = new Mp4AudioExtractor();
	} else if (this->format == MP4_FORMAT_AVC) {
		this->extractor = new Mp4AvcExtractor(this->codecConfig, this->codecConfigSize);
	} else {
		this->extractor = new Mp4PassthroughExtractor();
	}

	return this->extractor != NULL;
}

bool Mp4StreamFormat::isVideo() const
{
	return this->format > 0 && this->format < 5;
}

bool Mp4StreamFormat::isAudio() const
{
	return this->format >= 5 && this->format < 7;
}

bool Mp4StreamFormat::isTimedText() const
{
	return this->format == 7;
}
