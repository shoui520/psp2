#include "mp4_internal.h"

Mp4SampleSizeTable::Mp4SampleSizeTable()
{
	this->isCo64 = 0;
	this->stcoData = NULL;
	this->cachedFirstSample = 0;
	this->cachedEndSample = 0;
	this->cachedStscIndex = 0;
	this->stcoAtom = NULL;
	this->stcoMappedAtom = NULL;
	this->stscMappedAtom = NULL;
	this->stscData = NULL;
	this->stscAtom = NULL;
	this->stscEntryCount = 0;
	this->chunkCount = 0;
	this->maxSampleSize = 0;
	this->uniformSampleSize = 0;
	this->stszData = NULL;
	this->sampleCount = 0;
	this->stszAtom = NULL;
	this->stszMappedAtom = NULL;
}

int Mp4SampleSizeTable::init(Mp4Atom *stblAtom)
{
	this->stszAtom = stblAtom->findChildByType(FOURCC_STSZ, 0);
	if (this->stszAtom == NULL)
		return MP4_ERR_GENERIC;

	if (this->stszMappedAtom != NULL)
		this->stszMappedAtom->unmapBuffer();
	this->stszMappedAtom = this->stszAtom;

	uint8_t *buf = this->stszAtom->mapBuffer();
	if (buf != NULL)
		buf += this->stszAtom->getHeaderSize();
	this->stszData = buf;
	if (buf == NULL)
		return MP4_ERR_NO_MEMORY;
	if (buf[0] != 0)
		return MP4_ERR_GENERIC;

	int32_t uniformSize = (int32_t)mp4ReadBe32(buf + 4);
	int32_t count = (int32_t)mp4ReadBe32(buf + 8);
	this->uniformSampleSize = uniformSize;
	this->maxSampleSize = uniformSize;
	this->sampleCount = count;

	if (uniformSize == 0 && count > 0) {
		int32_t peak = 0;
		for (int32_t i = 0; i < count; i++) {
			int32_t sz = (int32_t)mp4ReadBe32(buf + 12 + i * 4);
			if (sz > peak) {
				peak = sz;
				this->maxSampleSize = sz;
			}
		}
	}

	this->stscAtom = stblAtom->findChildByType(FOURCC_STSC, 0);
	if (this->stscAtom == NULL)
		return MP4_ERR_GENERIC;

	if (this->stscMappedAtom != NULL)
		this->stscMappedAtom->unmapBuffer();
	this->stscMappedAtom = this->stscAtom;

	buf = this->stscAtom->mapBuffer();
	if (buf != NULL)
		buf += this->stscAtom->getHeaderSize();
	this->stscData = buf;
	this->stscEntryCount = (int32_t)mp4ReadBe32(buf + 4);

	this->stcoAtom = stblAtom->findChildByType(FOURCC_STCO, 0);
	if (this->stcoAtom != NULL) {
		this->isCo64 = 0;
	} else {
		this->stcoAtom = stblAtom->findChildByType(FOURCC_CO64, 0);
		if (this->stcoAtom == NULL)
			return MP4_ERR_GENERIC;
		this->isCo64 = 1;
	}

	if (this->stcoMappedAtom != NULL)
		this->stcoMappedAtom->unmapBuffer();
	this->stcoMappedAtom = this->stcoAtom;

	buf = this->stcoAtom->mapBuffer();
	if (buf != NULL)
		buf += this->stcoAtom->getHeaderSize();
	this->stcoData = buf;
	if (buf == NULL)
		return MP4_ERR_NO_MEMORY;

	this->chunkCount = (int32_t)mp4ReadBe32(buf + 4);
	return MP4_OK;
}

uint32_t Mp4SampleSizeTable::getSampleSize(int sampleIndex)
{
	if (this->uniformSampleSize != 0)
		return (uint32_t)this->uniformSampleSize;
	return mp4ReadBe32(this->stszData + 12 + sampleIndex * 4);
}

int64_t Mp4SampleSizeTable::getSampleOffset(int sampleIndex)
{
	int32_t firstSample = this->cachedFirstSample;
	int32_t entryIdx = this->cachedStscIndex;
	if (sampleIndex < firstSample || sampleIndex > this->cachedEndSample) {
		entryIdx = 0;
		firstSample = 0;
		this->cachedFirstSample = 0;
		this->cachedEndSample = 0;
		this->cachedStscIndex = 0;
	}

	uint32_t samplesPerChunk = 1;
	uint32_t firstChunkZeroBased = 0;

	while (entryIdx < this->stscEntryCount) {
		const uint8_t *entry = this->stscData + 8 + entryIdx * 12;
		firstChunkZeroBased = mp4ReadBe32(entry) - 1;
		samplesPerChunk = mp4ReadBe32(entry + 4);
		uint32_t nextFirstChunk = mp4ReadBe32(entry + 12);
		int32_t endSample = firstSample + (int32_t)((nextFirstChunk - 1 - firstChunkZeroBased) * samplesPerChunk);
		this->cachedEndSample = endSample;
		if (entryIdx == this->stscEntryCount - 1 || sampleIndex < endSample) {
			this->cachedStscIndex = entryIdx;
			break;
		}
		firstSample = endSample;
		entryIdx++;
	}

	this->cachedFirstSample = firstSample;
	int32_t relSample = sampleIndex - firstSample;
	int32_t chunkOffsetInRun = relSample / (int32_t)samplesPerChunk;
	int32_t sampleInChunk = relSample - chunkOffsetInRun * (int32_t)samplesPerChunk;
	int32_t chunkIndex = (int32_t)firstChunkZeroBased + chunkOffsetInRun;

	int64_t byteOffsetInChunk = 0;
	if (this->uniformSampleSize != 0) {
		byteOffsetInChunk = (int64_t)sampleInChunk * this->uniformSampleSize;
	} else {
		for (int32_t s = sampleIndex - sampleInChunk; s < sampleIndex; s++)
			byteOffsetInChunk += (int32_t)mp4ReadBe32(this->stszData + 12 + s * 4);
	}

	if (chunkIndex >= this->chunkCount)
		return 0;

	uint64_t chunkFileOffset;
	if (this->isCo64 != 0)
		chunkFileOffset = mp4ReadBe64(this->stcoData + 8 + chunkIndex * 8);
	else
		chunkFileOffset = mp4ReadBe32(this->stcoData + 8 + chunkIndex * 4);

	return (int64_t)chunkFileOffset + byteOffsetInChunk;
}

Mp4SyncSampleTable::~Mp4SyncSampleTable()
{
	if (this->stssAtom != NULL)
		this->stssAtom->unmapBuffer();
}

int Mp4SyncSampleTable::init(Mp4Atom *stblAtom)
{
	this->stssAtom = stblAtom->findChildByType(FOURCC_STSS, 0);
	if (this->stssAtom == NULL)
		return 1;

	uint8_t *buf = this->stssAtom->mapBuffer();
	if (buf == NULL)
		return 0;
	buf += this->stssAtom->getHeaderSize();
	this->stssData = buf;
	if (buf[0] != 0)
		return 0;
	this->entryCount = (int32_t)mp4ReadBe32(buf + 4);
	return 1;
}

int32_t Mp4SyncSampleTable::findPrevSyncSample(int32_t sampleIndex)
{
	if (this->stssAtom == NULL || this->entryCount == 0)
		return sampleIndex;

	int32_t best = 0;
	for (int32_t i = 0; i < this->entryCount; i++) {
		int32_t syncIdx = (int32_t)mp4ReadBe32(this->stssData + 8 + i * 4) - 1;
		if (sampleIndex < syncIdx)
			break;
		best = syncIdx;
	}
	return best;
}

int32_t Mp4SyncSampleTable::findNextSyncSample(int32_t sampleIndex)
{
	if (this->stssAtom == NULL || this->entryCount == 0)
		return sampleIndex + 1;

	for (int32_t i = 0; i < this->entryCount; i++) {
		int32_t syncIdx = (int32_t)mp4ReadBe32(this->stssData + 8 + i * 4) - 1;
		if (sampleIndex < syncIdx)
			return syncIdx;
	}
	return 0;
}

Mp4SyncSampleTable::Mp4SyncSampleTable()
{
	this->stssAtom = NULL;
	this->stssData = NULL;
	this->entryCount = 0;
}

Mp4TimeToSampleTable::Mp4TimeToSampleTable()
{
	this->mediaTimescale = 0;
	this->editTimeOffset = 0;
	this->sttsAtom = NULL;
	this->cttsAtom = NULL;
	this->sttsMappedAtom = NULL;
	this->sttsData = NULL;
	this->cttsMappedAtom = NULL;
	this->cttsData = NULL;
	this->sttsEntryCount = 0;
	this->cttsEntryCount = 0;
	this->cachedSampleIndex = 0;
	this->cachedSttsEntry = 0;
	this->cachedMediaTime = 0;
	this->cachedCttsEntry = 0;
	this->cachedCttsSample = 0;
	this->ticksPerMediaUnit = 0.0f;
}

int Mp4TimeToSampleTable::init(int32_t timescale, int64_t editOffset, Mp4Atom *stblAtom)
{
	this->mediaTimescale = timescale;
	this->editTimeOffset = editOffset;

	this->sttsAtom = stblAtom->findChildByType(FOURCC_STTS, 0);
	if (this->sttsAtom == NULL)
		return MP4_ERR_GENERIC;

	if (this->sttsMappedAtom != NULL)
		this->sttsMappedAtom->unmapBuffer();
	this->sttsMappedAtom = this->sttsAtom;

	uint8_t *buf = this->sttsAtom->mapBuffer();
	if (buf != NULL)
		buf += this->sttsAtom->getHeaderSize();
	this->sttsData = buf;
	if (buf == NULL)
		return MP4_ERR_NO_MEMORY;

	this->sttsEntryCount = mp4ReadBe32(buf + 4);

	this->cttsAtom = stblAtom->findChildByType(FOURCC_CTTS, 0);
	if (this->cttsAtom != NULL) {
		if (this->cttsMappedAtom != NULL)
			this->cttsMappedAtom->unmapBuffer();
		this->cttsMappedAtom = this->cttsAtom;
		buf = this->cttsAtom->mapBuffer();
		if (buf != NULL)
			buf += this->cttsAtom->getHeaderSize();
		this->cttsData = buf;
		if (buf == NULL)
			return MP4_ERR_NO_MEMORY;
		this->cttsEntryCount = mp4ReadBe32(buf + 4);
	} else {
		if (this->cttsMappedAtom != NULL)
			this->cttsMappedAtom->unmapBuffer();
		this->cttsMappedAtom = NULL;
		this->cttsEntryCount = 0;
	}

	this->cachedSampleIndex = 0;
	this->cachedSttsEntry = 0;
	this->cachedMediaTime = this->editTimeOffset;
	this->ticksPerMediaUnit = 90000.0f / (float)this->mediaTimescale;
	return MP4_OK;
}

int64_t Mp4TimeToSampleTable::ticksToMediaTime(int64_t ticks90k)
{
	return (int64_t)((float)ticks90k / this->ticksPerMediaUnit);
}

int64_t Mp4TimeToSampleTable::mediaTimeToTicks(int64_t mediaTime)
{
	return (int64_t)((float)mediaTime * this->ticksPerMediaUnit);
}

int64_t Mp4TimeToSampleTable::getCompositionOffset(int32_t sampleIndex)
{
	if (this->cttsEntryCount == 0 || this->cttsData == NULL)
		return 0;

	int32_t curSample = this->cachedCttsSample;
	uint32_t idx = this->cachedCttsEntry;
	if (sampleIndex < curSample) {
		idx = 0;
		curSample = 0;
		this->cachedCttsEntry = 0;
		this->cachedCttsSample = 0;
	}

	while (idx < this->cttsEntryCount) {
		const uint8_t *entry = this->cttsData + 8 + idx * 8;
		curSample += (int32_t)mp4ReadBe32(entry);
		if (sampleIndex < curSample) {
			int32_t offset = (int32_t)mp4ReadBe32(entry + 4);
			return this->mediaTimeToTicks(offset);
		}
		idx++;
		this->cachedCttsSample = curSample;
		this->cachedCttsEntry = idx;
	}
	return 0;
}

int32_t Mp4TimeToSampleTable::findSampleByTimestamp(int64_t ticks90k)
{
	int64_t targetMedia = this->ticksToMediaTime(ticks90k);

	if (targetMedia < this->cachedMediaTime) {
		while (this->cachedSttsEntry != 0) {
			uint32_t prevEntry = this->cachedSttsEntry - 1;
			const uint8_t *entry = this->sttsData + 8 + prevEntry * 8;
			int32_t count = (int32_t)mp4ReadBe32(entry);
			int32_t delta = (int32_t)mp4ReadBe32(entry + 4);
			int64_t cttsOff = this->getCompositionOffset(this->cachedSampleIndex - count);
			int64_t prevTime = this->cachedMediaTime - (int64_t)count * delta;
			if (targetMedia >= prevTime + cttsOff)
				return this->cachedSampleIndex - (int32_t)((targetMedia - this->cachedMediaTime) / delta);
			this->cachedMediaTime = prevTime;
			this->cachedSampleIndex -= count;
			this->cachedSttsEntry = prevEntry;
		}
		return -1;
	}

	while (this->cachedSttsEntry < this->sttsEntryCount) {
		const uint8_t *entry = this->sttsData + 8 + this->cachedSttsEntry * 8;
		int32_t count = (int32_t)mp4ReadBe32(entry);
		int32_t delta = (int32_t)mp4ReadBe32(entry + 4);
		int64_t cttsOff = this->getCompositionOffset(this->cachedSampleIndex + count);
		int64_t nextTime = this->cachedMediaTime + (int64_t)count * delta;
		if (targetMedia <= nextTime + cttsOff)
			return this->cachedSampleIndex + (int32_t)((targetMedia - this->cachedMediaTime) / delta);
		this->cachedMediaTime = nextTime;
		this->cachedSampleIndex += count;
		this->cachedSttsEntry++;
	}
	return -1;
}

int64_t Mp4TimeToSampleTable::getSampleDts(int32_t sampleIndex)
{
	if (sampleIndex < this->cachedSampleIndex) {
		while (this->cachedSttsEntry != 0) {
			const uint8_t *entry = this->sttsData + 8 + this->cachedSttsEntry * 8;
			int32_t count = (int32_t)mp4ReadBe32(entry - 8);
			int32_t delta = (int32_t)mp4ReadBe32(entry - 4);
			if (sampleIndex > this->cachedSampleIndex - count) {
				int64_t mediaTime = this->cachedMediaTime - (int64_t)(sampleIndex - this->cachedSampleIndex) * delta;
				return this->mediaTimeToTicks(mediaTime);
			}
			this->cachedSttsEntry--;
			this->cachedMediaTime -= (int64_t)count * delta;
			this->cachedSampleIndex -= count;
		}
		return 0;
	}

	while (this->cachedSttsEntry < this->sttsEntryCount) {
		const uint8_t *entry = this->sttsData + 8 + this->cachedSttsEntry * 8;
		int32_t count = (int32_t)mp4ReadBe32(entry);
		int32_t delta = (int32_t)mp4ReadBe32(entry + 4);
		if (sampleIndex < this->cachedSampleIndex + count) {
			int64_t mediaTime = this->cachedMediaTime + (int64_t)(sampleIndex - this->cachedSampleIndex) * delta;
			return this->mediaTimeToTicks(mediaTime);
		}
		this->cachedMediaTime += (int64_t)count * delta;
		this->cachedSampleIndex += count;
		this->cachedSttsEntry++;
	}
	return 0;
}

int64_t Mp4TimeToSampleTable::getSamplePts(int32_t sampleIndex)
{
	return this->getSampleDts(sampleIndex) + this->getCompositionOffset(sampleIndex);
}

int64_t Mp4TimeToSampleTable::getSampleDuration(int32_t sampleIndex)
{
	if (sampleIndex < this->cachedSampleIndex) {
		while (this->cachedSttsEntry != 0) {
			const uint8_t *entry = this->sttsData + 8 + this->cachedSttsEntry * 8;
			int32_t count = (int32_t)mp4ReadBe32(entry - 8);
			int32_t delta = (int32_t)mp4ReadBe32(entry - 4);
			if (sampleIndex > this->cachedSampleIndex - count)
				return this->mediaTimeToTicks(delta);
			this->cachedSttsEntry--;
			this->cachedMediaTime -= (int64_t)count * delta;
			this->cachedSampleIndex -= count;
		}
		return 0;
	}

	while (this->cachedSttsEntry < this->sttsEntryCount) {
		const uint8_t *entry = this->sttsData + 8 + this->cachedSttsEntry * 8;
		int32_t count = (int32_t)mp4ReadBe32(entry);
		int32_t delta = (int32_t)mp4ReadBe32(entry + 4);
		if (sampleIndex < this->cachedSampleIndex + count)
			return this->mediaTimeToTicks(delta);
		this->cachedMediaTime += (int64_t)count * delta;
		this->cachedSampleIndex += count;
		this->cachedSttsEntry++;
	}
	return 0;
}
