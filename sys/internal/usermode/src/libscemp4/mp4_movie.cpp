#include "mp4_internal.h"

static Mp4ListNode<Mp4Track *> *mp4TrackListGetAt(Mp4ListNode<Mp4Track *> *node, uint32_t curIdx, uint32_t targetIdx)
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

static Mp4ListNode<Mp4Track *> *mp4TrackListFindPrev(Mp4ListNode<Mp4Track *> *node, Mp4ListNode<Mp4Track *> *target)
{
	if (node == target)
		return node->prev;
	return mp4TrackListFindPrev(node->next, target);
}

static Mp4Track *mp4TrackListElem(const Mp4List<Mp4Track *> *list, uint32_t index)
{
	if (index >= list->count)
		return list->sentinel->value;
	if (index >= (list->count >> 1))
		return mp4TrackListGetAt(list->tail, list->count - 1, index)->value;
	return mp4TrackListGetAt(list->head, 0, index)->value;
}

static void *mp4TaggedAlloc(SceAvPlayerMemAllocator *allocator, int alignment, SceSize size)
{
	uint8_t *raw = (uint8_t *)mp4MspaceAllocZero(alignment, size + 1);
	if (raw != NULL) {
		raw[0] = MP4_TAG_MSPACE_DF;
		return raw + 1;
	}
	raw = (uint8_t *)allocator->allocate(allocator->objectPointer, (uint32_t)alignment, (uint32_t)(size + 1));
	if (raw != NULL) {
		raw[0] = MP4_TAG_EXT_ALLOC_D0;
		return raw + 1;
	}
	return NULL;
}

static inline __attribute__((always_inline)) void mp4TaggedFree(SceAvPlayerMemAllocator *allocator, void *ptr)
{
	if (ptr == NULL)
		return;
	uint8_t *raw = (uint8_t *)ptr - 1;
	if (raw[0] == MP4_TAG_MSPACE_DF)
		mp4MspaceFree(raw);
	else
		allocator->deallocate(allocator->objectPointer, raw);
}

static Mp4RefControl *mp4CreateRefControl()
{
	SceKernelLwMutexOptParam opt;
	Mp4RefControl *ctrl = (Mp4RefControl *)mp4MspaceAllocZero(1, sizeof(Mp4RefControl));
	if (ctrl != NULL) {
		ctrl->refCount = 1;
		sceClibMemset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		sceKernelCreateLwMutex(&ctrl->mutex, "CriticalSmart", 0, 0, &opt);
	}
	return ctrl;
}

template <typename T>
static void mp4ReleaseSmartPtr(T **objSlot, Mp4RefControl **ctrlSlot)
{
	Mp4RefControl *ctrl = *ctrlSlot;
	if (ctrl != NULL) {
		mp4AtomicDec32((volatile uint32_t *)&ctrl->refCount);
		if (ctrl->refCount <= 0) {
			sceKernelDeleteLwMutex(&ctrl->mutex);
			mp4MspaceFree(ctrl);
			T *obj = *objSlot;
			if (obj != NULL) {
				obj->~T();
				mp4MspaceFree(obj);
			}
		}
	}
	*objSlot = NULL;
	*ctrlSlot = NULL;
}

static inline __attribute__((always_inline)) void mp4ReleaseSampleSizeTable(Mp4SampleSizeTable **objSlot, Mp4RefControl **ctrlSlot)
{
	Mp4RefControl *ctrl = *ctrlSlot;
	if (ctrl != NULL) {
		mp4AtomicDec32((volatile uint32_t *)&ctrl->refCount);
		if (ctrl->refCount <= 0) {
			sceKernelDeleteLwMutex(&ctrl->mutex);
			mp4MspaceFree(ctrl);
			Mp4SampleSizeTable *tbl = *objSlot;
			if (tbl != NULL) {
				if (tbl->stcoMappedAtom != NULL)
					tbl->stcoMappedAtom->unmapBuffer();
				if (tbl->stscMappedAtom != NULL)
					tbl->stscMappedAtom->unmapBuffer();
				if (tbl->stszMappedAtom != NULL)
					tbl->stszMappedAtom->unmapBuffer();
				mp4MspaceFree(tbl);
			}
		}
	}
	*objSlot = NULL;
	*ctrlSlot = NULL;
}

static inline __attribute__((always_inline)) void mp4ReleaseTimeToSampleTable(Mp4TimeToSampleTable **objSlot, Mp4RefControl **ctrlSlot)
{
	Mp4RefControl *ctrl = *ctrlSlot;
	if (ctrl != NULL) {
		mp4AtomicDec32((volatile uint32_t *)&ctrl->refCount);
		if (ctrl->refCount <= 0) {
			sceKernelDeleteLwMutex(&ctrl->mutex);
			mp4MspaceFree(ctrl);
			Mp4TimeToSampleTable *tbl = *objSlot;
			if (tbl != NULL) {
				if (tbl->cttsMappedAtom != NULL)
					tbl->cttsMappedAtom->unmapBuffer();
				if (tbl->sttsMappedAtom != NULL)
					tbl->sttsMappedAtom->unmapBuffer();
				mp4MspaceFree(tbl);
			}
		}
	}
	*objSlot = NULL;
	*ctrlSlot = NULL;
}

int Mp4Track::buildSampleOffsets(Mp4Atom *stscAtom)
{
	uint8_t *buf = NULL;
	if (stscAtom != NULL) {
		buf = stscAtom->mapBuffer();
		if (buf != NULL)
			buf += stscAtom->getHeaderSize();
	}
	if (buf == NULL || this->chunkOffsets == NULL) {
		if (stscAtom != NULL)
			stscAtom->unmapBuffer();
		return 0;
	}

	uint32_t entryCount = mp4ReadBe32(buf + 4);
	uint32_t *firstChunkArr = (uint32_t *)mp4TaggedAlloc(this->allocator, 4, entryCount * 4);
	uint32_t *samplesPerChunkArr = (uint32_t *)mp4TaggedAlloc(this->allocator, 4, entryCount * 4);
	uint32_t *descIdxArr = (uint32_t *)mp4TaggedAlloc(this->allocator, 4, entryCount * 4);

	if (firstChunkArr == NULL || samplesPerChunkArr == NULL || descIdxArr == NULL) {
		mp4TaggedFree(this->allocator, firstChunkArr);
		mp4TaggedFree(this->allocator, samplesPerChunkArr);
		mp4TaggedFree(this->allocator, descIdxArr);
		if (stscAtom != NULL)
			stscAtom->unmapBuffer();
		return 0;
	}

	for (uint32_t i = 0; i < entryCount; i++) {
		firstChunkArr[i] = mp4ReadBe32(buf + 8 + i * 12);
		samplesPerChunkArr[i] = mp4ReadBe32(buf + 12 + i * 12);
		descIdxArr[i] = mp4ReadBe32(buf + 16 + i * 12);
	}

	this->sampleOffsets = (uint32_t *)mp4TaggedAlloc(this->allocator, 4, this->totalSampleCount * 4);
	if (this->sampleOffsets == NULL || this->sampleSizes == NULL) {
		mp4TaggedFree(this->allocator, firstChunkArr);
		mp4TaggedFree(this->allocator, samplesPerChunkArr);
		mp4TaggedFree(this->allocator, descIdxArr);
		if (stscAtom != NULL)
			stscAtom->unmapBuffer();
		return 0;
	}

	uint32_t sampleIdx = 0;
	uint32_t chunkIdx = 0;
	if (entryCount > 1) {
		for (uint32_t e = 0; e < entryCount - 1; e++) {
			int32_t chunksInRun = (int32_t)(firstChunkArr[e + 1] - firstChunkArr[e]);
			for (int32_t c = 0; c < chunksInRun; c++) {
				uint32_t offInChunk = 0;
				for (uint32_t s = 0; s < samplesPerChunkArr[e]; s++) {
					this->sampleOffsets[sampleIdx] = this->chunkOffsets[chunkIdx] + offInChunk;
					offInChunk += this->sampleSizes[sampleIdx];
					sampleIdx++;
				}
				chunkIdx++;
			}
		}
	}

	if (entryCount != 0 && this->totalChunkCount > chunkIdx) {
		uint32_t lastSpc = samplesPerChunkArr[entryCount - 1];
		for (uint32_t c = chunkIdx; c < this->totalChunkCount; c++) {
			uint32_t offInChunk = 0;
			for (uint32_t s = 0; s < lastSpc; s++) {
				this->sampleOffsets[sampleIdx] = this->chunkOffsets[c] + offInChunk;
				offInChunk += this->sampleSizes[sampleIdx];
				sampleIdx++;
			}
		}
	}

	mp4TaggedFree(this->allocator, firstChunkArr);
	mp4TaggedFree(this->allocator, samplesPerChunkArr);
	mp4TaggedFree(this->allocator, descIdxArr);
	mp4TaggedFree(this->allocator, this->sampleSizes);
	this->sampleSizes = NULL;
	this->allocator->deallocate(this->allocator->objectPointer, this->chunkOffsets);
	this->chunkOffsets = NULL;

	if (stscAtom != NULL)
		stscAtom->unmapBuffer();
	return 1;
}

int Mp4Track::loadChunkOffsets(Mp4Atom *stcoAtom)
{
	uint8_t *buf = NULL;
	if (stcoAtom != NULL) {
		buf = stcoAtom->mapBuffer();
		if (buf != NULL)
			buf += stcoAtom->getHeaderSize();
	}
	if (buf == NULL) {
		if (stcoAtom != NULL)
			stcoAtom->unmapBuffer();
		return 0;
	}

	uint32_t count = mp4ReadBe32(buf + 4);
	this->chunkOffsets = (uint32_t *)this->allocator->allocate(this->allocator->objectPointer, 4, count * 4);
	if (this->chunkOffsets == NULL) {
		if (stcoAtom != NULL)
			stcoAtom->unmapBuffer();
		return 0;
	}

	for (uint32_t i = 0; i < count; i++)
		this->chunkOffsets[i] = mp4ReadBe32(buf + 8 + i * 4);

	this->totalChunkCount = count;
	if (stcoAtom != NULL)
		stcoAtom->unmapBuffer();
	return 1;
}

int Mp4Track::loadSampleSizes(Mp4Atom *stszAtom)
{
	uint8_t *buf = NULL;
	if (stszAtom != NULL) {
		buf = stszAtom->mapBuffer();
		if (buf != NULL)
			buf += stszAtom->getHeaderSize();
	}
	if (buf == NULL) {
		if (stszAtom != NULL)
			stszAtom->unmapBuffer();
		return 0;
	}

	uint32_t count = mp4ReadBe32(buf + 8);
	this->maxSampleSize = 0;
	this->maxSampleSizeIndex = 0;
	this->sampleSizes = (uint32_t *)mp4TaggedAlloc(this->allocator, 4, count * 4);

	uint32_t totalBytes = 0;
	if (this->sampleSizes != NULL) {
		for (uint32_t i = 0; i < count; i++) {
			uint32_t sz = mp4ReadBe32(buf + 12 + i * 4);
			this->sampleSizes[i] = sz;
			totalBytes += sz;
			if (this->maxSampleSize < sz) {
				this->maxSampleSize = sz;
				this->maxSampleSizeIndex = i;
			}
		}
	} else {
		count = 0;
	}

	int64_t durationSec = this->trackDuration / (int64_t)this->mediaTimescale;
	uint32_t avgFps = (uint32_t)(int32_t)((double)count / (double)durationSec);
	this->totalSampleCount = count;
	this->totalDataBytes = totalBytes;
	this->peakBytesPerSec = (float)(this->maxSampleSize * avgFps);

	if (stszAtom != NULL)
		stszAtom->unmapBuffer();
	return 1;
}

int Mp4Track::loadTimeToSample(Mp4Atom *sttsAtom, bool isVideoTrack)
{
	uint8_t *buf = NULL;
	if (sttsAtom != NULL) {
		buf = sttsAtom->mapBuffer();
		if (buf != NULL)
			buf += sttsAtom->getHeaderSize();
	}
	if (buf == NULL) {
		if (sttsAtom != NULL)
			sttsAtom->unmapBuffer();
		return 0;
	}

	uint32_t entryCount = mp4ReadBe32(buf + 4);
	if (entryCount == 0) {
		if (sttsAtom != NULL)
			sttsAtom->unmapBuffer();
		return 0;
	}

	this->sttsSampleCounts = (uint32_t *)mp4TaggedAlloc(this->allocator, 4, entryCount * 16);
	if (this->sttsSampleCounts == NULL) {
		if (sttsAtom != NULL)
			sttsAtom->unmapBuffer();
		return 0;
	}
	this->sttsSampleDeltas = (uint32_t *)mp4TaggedAlloc(this->allocator, 4, entryCount * 16);
	if (this->sttsSampleDeltas == NULL) {
		mp4TaggedFree(this->allocator, this->sttsSampleCounts);
		this->sttsSampleCounts = NULL;
		if (sttsAtom != NULL)
			sttsAtom->unmapBuffer();
		return 0;
	}

	sceClibMemset(this->sttsSampleCounts, 0, entryCount * 4);
	sceClibMemset(this->sttsSampleDeltas, 0, entryCount * 4);

	uint32_t totalSamples = 0;
	for (uint32_t i = 0; i < entryCount; i++) {
		this->sttsSampleCounts[i] = mp4ReadBe32(buf + 8 + i * 8);
		this->sttsSampleDeltas[i] = mp4ReadBe32(buf + 12 + i * 8);
		totalSamples += this->sttsSampleCounts[i];
	}

	this->sttsEntryCount = entryCount;
	this->sttsTotalSamples = totalSamples;

	uint32_t tableLen = (!isVideoTrack && entryCount == 1) ? 1u : totalSamples;
	if (this->sampleDts90k == NULL) {
		this->sampleDts90k = (int64_t *)this->allocator->allocate(this->allocator->objectPointer, 4, tableLen * 8);
		if (this->sampleDts90k == NULL) {
			mp4TaggedFree(this->allocator, this->sttsSampleCounts);
			mp4TaggedFree(this->allocator, this->sttsSampleDeltas);
			this->sttsSampleCounts = NULL;
			this->sttsSampleDeltas = NULL;
			if (sttsAtom != NULL)
				sttsAtom->unmapBuffer();
			return 0;
		}
		sceClibMemset(this->sampleDts90k, 0, tableLen * 8);
	}

	if (!isVideoTrack && entryCount == 1) {
		this->sampleDts90k[0] += (90000LL * (int64_t)this->sttsSampleDeltas[0]) / (int64_t)this->mediaTimescale;
	} else {
		int64_t accumMedia = 0;
		uint32_t dstIdx = 1;
		for (uint32_t i = 0; i < entryCount; i++) {
			if (i != 0) {
				accumMedia += this->sttsSampleDeltas[i - 1];
				this->sampleDts90k[dstIdx++] += (90000LL * accumMedia) / (int64_t)this->mediaTimescale;
			}
			for (uint32_t s = 1; s < this->sttsSampleCounts[i]; s++) {
				accumMedia += this->sttsSampleDeltas[i];
				this->sampleDts90k[dstIdx++] += (90000LL * accumMedia) / (int64_t)this->mediaTimescale;
			}
		}
	}

	mp4TaggedFree(this->allocator, this->sttsSampleCounts);
	mp4TaggedFree(this->allocator, this->sttsSampleDeltas);
	this->sttsSampleCounts = NULL;
	this->sttsSampleDeltas = NULL;

	if (sttsAtom != NULL)
		sttsAtom->unmapBuffer();
	return 1;
}

bool Mp4Track::parseStsd(int64_t avgFrameDuration, Mp4Atom *stsdAtom)
{
	uint8_t header[8];
	if (stsdAtom->readData(stsdAtom->getHeaderSize(), 8, header) != MP4_OK)
		return false;
	if (header[0] != 0 || mp4ReadBe32(header + 4) != 1)
		return false;

	stsdAtom->parseChildren(8);
	Mp4Atom *entryAtom = stsdAtom->getChildAt(0);
	if (entryAtom == NULL)
		return false;

	Mp4StreamFormat *fmt = new Mp4StreamFormat(this->streamFlags);

	mp4ReleaseSmartPtr(&this->streamFormat, &this->streamFormatRef);
	this->streamFormat = fmt;
	this->streamFormatRef = mp4CreateRefControl();

	return (this->streamFormat != NULL) && (this->streamFormat->parseSampleDescription(avgFrameDuration, entryAtom) != 0);
}

int Mp4Track::parseMedia(Mp4Atom *mdiaAtom, int64_t editOffset)
{
	if (mdiaAtom == NULL)
		return 0;

	Mp4Atom *mdhdAtom = mdiaAtom->findChildByType(FOURCC_MDHD, 0);
	if (mdhdAtom == NULL)
		return 0;

	uint8_t *mdhdBuf = mdhdAtom->mapBuffer();
	if (mdhdBuf != NULL)
		mdhdBuf += mdhdAtom->getHeaderSize();
	if (mdhdBuf == NULL) {
		mdhdAtom->unmapBuffer();
		return 0;
	}

	this->mdhdVersion1 = (mdhdBuf[0] == 1) ? 1u : 0u;
	uint16_t packedLang;
	if (mdhdBuf[0] == 1) {
		this->mediaTimescale = (int32_t)mp4ReadBe32(mdhdBuf + 20);
		if (this->trackDuration == 0)
			this->trackDuration = (int32_t)mp4ReadBe32(mdhdBuf + 24);
		packedLang = mp4ReadBe16(mdhdBuf + 28);
	} else {
		this->mediaTimescale = (int32_t)mp4ReadBe32(mdhdBuf + 12);
		if (this->trackDuration == 0)
			this->trackDuration = (int32_t)mp4ReadBe32(mdhdBuf + 16);
		packedLang = mp4ReadBe16(mdhdBuf + 20);
	}

	this->languageCode[0] = (uint8_t)(((packedLang >> 10) & 0x1Fu) + 96);
	this->languageCode[1] = (uint8_t)(((packedLang >> 5) & 0x1Fu) + 96);
	this->languageCode[2] = (uint8_t)((packedLang & 0x1Fu) + 96);

	Mp4Atom *minfAtom = mdiaAtom->findChildByType(FOURCC_MINF, 0);
	if (minfAtom == NULL) {
		mdhdAtom->unmapBuffer();
		return 0;
	}

	this->stblAtom = minfAtom->findChildByType(FOURCC_STBL, 0);
	if (this->stblAtom == NULL) {
		mdhdAtom->unmapBuffer();
		return 0;
	}

	Mp4SampleSizeTable *szTbl = new Mp4SampleSizeTable();
	mp4ReleaseSampleSizeTable(&this->sampleSizeTable, &this->sampleSizeTableRef);
	this->sampleSizeTable = szTbl;
	this->sampleSizeTableRef = mp4CreateRefControl();

	if (this->sampleSizeTable == NULL || this->sampleSizeTable->init(this->stblAtom) != MP4_OK) {
		mdhdAtom->unmapBuffer();
		return 0;
	}

	Mp4SyncSampleTable *syncTbl = new Mp4SyncSampleTable();
	mp4ReleaseSmartPtr(&this->syncSampleTable, &this->syncSampleTableRef);
	this->syncSampleTable = syncTbl;
	this->syncSampleTableRef = mp4CreateRefControl();
	if (this->syncSampleTable == NULL) {
		mdhdAtom->unmapBuffer();
		return 0;
	}
	this->syncSampleTable->init(this->stblAtom);

	Mp4TimeToSampleTable *tsTbl = new Mp4TimeToSampleTable();
	mp4ReleaseTimeToSampleTable(&this->timeToSampleTable, &this->timeToSampleTableRef);
	this->timeToSampleTable = tsTbl;
	this->timeToSampleTableRef = mp4CreateRefControl();
	if (this->timeToSampleTable == NULL) {
		mdhdAtom->unmapBuffer();
		return 0;
	}

	int tsErr = this->timeToSampleTable->init(this->mediaTimescale, editOffset, this->stblAtom);
	if (tsErr != MP4_OK && tsErr != MP4_ERR_GENERIC) {
		mdhdAtom->unmapBuffer();
		return 0;
	}

	int64_t avgDur = 0;
	if (this->sampleSizeTable->sampleCount > 0)
		avgDur = this->ownerMovie->movieDuration100us / this->sampleSizeTable->sampleCount;

	Mp4Atom *stsdAtom = this->stblAtom->findChildByType(FOURCC_STSD, 0);
	if (stsdAtom == NULL || !this->parseStsd(avgDur, stsdAtom)) {
		mdhdAtom->unmapBuffer();
		return 0;
	}

	bool vid = this->streamFormat->isVideo();
	Mp4Atom *sttsAtom = this->stblAtom->findChildByType(FOURCC_STTS, 0);
	if (sttsAtom != NULL)
		this->loadTimeToSample(sttsAtom, vid);

	Mp4Atom *stszAtom = this->stblAtom->findChildByType(FOURCC_STSZ, 0);
	if (stszAtom != NULL)
		this->loadSampleSizes(stszAtom);

	Mp4Atom *stcoAtom = this->stblAtom->findChildByType(FOURCC_STCO, 0);
	if (stcoAtom != NULL)
		this->loadChunkOffsets(stcoAtom);

	Mp4Atom *stscAtom = this->stblAtom->findChildByType(FOURCC_STSC, 0);
	if (stscAtom != NULL && stcoAtom != NULL)
		this->buildSampleOffsets(stscAtom);

	mdhdAtom->unmapBuffer();
	return 1;
}

uint64_t Mp4Track::parseEditList(Mp4Atom *edtsAtom)
{
	if (edtsAtom == NULL)
		return 0;

	Mp4Atom *elstAtom = edtsAtom->findChildByType(FOURCC_ELST, 0);
	if (elstAtom == NULL)
		return 0;

	uint8_t *buf = elstAtom->mapBuffer();
	if (buf != NULL)
		buf += elstAtom->getHeaderSize();
	if (buf == NULL) {
		elstAtom->unmapBuffer();
		return 0;
	}

	uint64_t result = 0;
	if (mp4ReadBe32(buf + 4) == 2) {
		uint64_t segDur = 0;
		int64_t firstMediaTime = 0;
		int64_t secondMediaTime = 0;
		if (buf[0] == 0) {
			segDur = (int32_t)mp4ReadBe32(buf + 8);
			firstMediaTime = (int32_t)mp4ReadBe32(buf + 12);
			secondMediaTime = (int32_t)mp4ReadBe32(buf + 24);
		} else if (buf[0] == 1) {
			segDur = mp4ReadBe64(buf + 8);
			firstMediaTime = (int64_t)mp4ReadBe64(buf + 16);
			secondMediaTime = (int64_t)mp4ReadBe64(buf + 36);
		} else {
			elstAtom->unmapBuffer();
			return 0;
		}
		if (firstMediaTime == -1 && secondMediaTime == 0)
			result = segDur;
	}

	elstAtom->unmapBuffer();
	return result;
}

Mp4Track::Mp4Track(Mp4Atom *inTrakAtom, Mp4Movie *inMovie, int32_t inTrackIndex, SceAvPlayerMemAllocator *inAllocator, int32_t inStreamFlags)
{
	sceClibMemset(this, 0, sizeof(*this));
	this->allocator = inAllocator;
	this->streamFlags = inStreamFlags;
	this->ownerMovie = inMovie;
	this->trackIndex = inTrackIndex;
	this->mediaTimescale = 1;

	if (inTrakAtom == NULL)
		return;

	Mp4Atom *tkhdAtom = inTrakAtom->findChildByType(FOURCC_TKHD, 0);
	if (tkhdAtom == NULL)
		return;

	uint8_t flagsBytes[4];
	if (tkhdAtom->readData(tkhdAtom->getHeaderSize(), 4, flagsBytes) != MP4_OK)
		return;
	if ((flagsBytes[3] & 1u) == 0)
		return;

	uint8_t *tkhdBuf = tkhdAtom->mapBuffer();
	if (tkhdBuf != NULL)
		tkhdBuf += tkhdAtom->getHeaderSize();
	if (tkhdBuf == NULL) {
		tkhdAtom->unmapBuffer();
		return;
	}

	uint32_t matrixOff;
	if (tkhdBuf[0] == 1) {
		this->trackDuration = (int64_t)mp4ReadBe64(tkhdBuf + 28);
		matrixOff = 52;
	} else {
		this->trackDuration = (int32_t)mp4ReadBe32(tkhdBuf + 20);
		matrixOff = 40;
	}

	for (int i = 0; i < 9; i++)
		this->tkhdMatrix[i] = mp4ReadBe32(tkhdBuf + matrixOff + i * 4);

	int64_t editOffset = 0;
	Mp4Atom *edtsAtom = inTrakAtom->findChildByType(FOURCC_EDTS, 0);
	if (edtsAtom != NULL) {
		uint64_t editDur = this->parseEditList(edtsAtom);
		int64_t movieTs = (inMovie->movieTimescale != 0) ? inMovie->movieTimescale : 1;
		editOffset = (90000LL * (int64_t)editDur) / movieTs;
	}

	Mp4Atom *mdiaAtom = inTrakAtom->findChildByType(FOURCC_MDIA, 0);
	if (mdiaAtom != NULL && this->parseMedia(mdiaAtom, editOffset) != 0)
		this->trakAtom = inTrakAtom;

	tkhdAtom->unmapBuffer();
}

int Mp4Track::getStreamDescription(Mp4StreamDescription *outDesc, uint32_t index, uint32_t spsIndex)
{
	int ok = this->streamFormat->getStreamDescription(outDesc, index, spsIndex);
	outDesc->languageCode[0] = this->languageCode[0];
	outDesc->languageCode[1] = this->languageCode[1];
	outDesc->languageCode[2] = this->languageCode[2];
	outDesc->languageCode[3] = 0;
	return ok;
}

int Mp4Track::findSampleForSeek(int forward, uint64_t targetTicks, int32_t *outSampleIndex, int64_t *outTimestamp)
{
	(void)forward;
	bool vid = this->streamFormat->isVideo();
	int64_t *dtsTable = this->sampleDts90k;
	if (vid) {
		if (dtsTable == NULL || this->syncSampleIndices == NULL)
			return 0;
	} else {
		if (dtsTable == NULL)
			return 0;
		if (this->sttsEntryCount == 1) {
			int64_t step = dtsTable[0];
			for (uint32_t i = 1; i < this->sttsTotalSamples; i++) {
				if ((uint64_t)(step * (i - 1)) <= targetTicks && (uint64_t)(step * i) > targetTicks) {
					*outSampleIndex = (int32_t)i;
					*outTimestamp = step * i;
					return 1;
				}
			}
			return 0;
		}
	}

	uint32_t chosenSample = 0;
	if (targetTicks >= (uint64_t)dtsTable[1]) {
		bool found = false;
		if (this->cttsOffsets90k != NULL) {
			for (uint32_t i = 1; i < this->sttsTotalSamples; i++) {
				if ((uint64_t)(dtsTable[i - 1] + this->cttsOffsets90k[i - 1]) <= targetTicks &&
				    (uint64_t)(dtsTable[i] + this->cttsOffsets90k[i]) > targetTicks) {
					chosenSample = i - 1;
					found = true;
					break;
				}
			}
		} else {
			for (uint32_t i = 1; i < this->sttsTotalSamples; i++) {
				if ((uint64_t)dtsTable[i - 1] <= targetTicks && (uint64_t)dtsTable[i] > targetTicks) {
					chosenSample = i - 1;
					found = true;
					break;
				}
			}
		}
		if (!found) {
			if ((uint64_t)dtsTable[this->sttsTotalSamples - 1] > targetTicks ||
			    targetTicks > (uint64_t)(this->trackDuration + 2 * this->mediaTimescale))
				return 0;
			chosenSample = this->sttsTotalSamples - 1;
		}
	}

	if (chosenSample == 0) {
		*outSampleIndex = 0;
		*outTimestamp = dtsTable[0];
		return 1;
	}

	uint32_t finalSample = chosenSample;
	if (this->syncSampleIndices != NULL && this->syncSampleCount > 1) {
		for (uint32_t i = 1; i < this->syncSampleCount; i++) {
			uint32_t prevKey = this->syncSampleIndices[i - 1];
			uint32_t nextKey = this->syncSampleIndices[i];
			if (prevKey <= chosenSample && nextKey > chosenSample) {
				finalSample = (chosenSample - prevKey < nextKey - chosenSample) ? prevKey : nextKey;
				break;
			}
		}
	}

	*outSampleIndex = (int32_t)finalSample;
	*outTimestamp = dtsTable[finalSample] + ((this->cttsOffsets90k != NULL) ? this->cttsOffsets90k[finalSample] : 0);
	return 1;
}

bool Mp4Track::findKeyFrame(bool forward, uint32_t currentSample, uint32_t *outSample, int64_t *outTimestamp)
{
	if (!this->streamFormat->isVideo() || this->sampleDts90k == NULL || this->syncSampleIndices == NULL)
		return false;

	for (uint32_t i = 1; i < this->syncSampleCount; i++) {
		uint32_t prevKey = this->syncSampleIndices[i - 1];
		uint32_t nextKey = this->syncSampleIndices[i];
		if (forward) {
			if (prevKey <= currentSample && nextKey > currentSample) {
				*outSample = nextKey;
				*outTimestamp = this->sampleDts90k[nextKey] + ((this->cttsOffsets90k != NULL) ? this->cttsOffsets90k[nextKey] : 0);
				return true;
			}
		} else {
			if (prevKey < currentSample && nextKey >= currentSample) {
				*outSample = prevKey;
				*outTimestamp = this->sampleDts90k[prevKey] + ((this->cttsOffsets90k != NULL) ? this->cttsOffsets90k[prevKey] : 0);
				return true;
			}
		}
	}

	uint32_t edgeKey = forward ? this->syncSampleIndices[this->syncSampleCount - 1] : this->syncSampleIndices[0];
	*outSample = edgeKey;
	*outTimestamp = this->sampleDts90k[edgeKey] + ((this->cttsOffsets90k != NULL) ? this->cttsOffsets90k[edgeKey] : 0);
	return !forward;
}

int Mp4Track::readSampleData(int32_t sampleIndex, uint32_t length, void *dst)
{
	int64_t off = this->sampleSizeTable->getSampleOffset(sampleIndex);
	return this->ownerMovie->readData(off, length, dst);
}

int Mp4Track::loadCompositionOffsets(Mp4Atom *cttsAtom)
{
	uint8_t *buf = NULL;
	if (cttsAtom != NULL) {
		buf = cttsAtom->mapBuffer();
		if (buf != NULL)
			buf += cttsAtom->getHeaderSize();
	}
	if (buf == NULL) {
		if (cttsAtom != NULL)
			cttsAtom->unmapBuffer();
		return 0;
	}

	uint32_t entryCount = mp4ReadBe32(buf + 4);
	uint32_t *counts = (uint32_t *)mp4TaggedAlloc(this->allocator, 4, entryCount * 16);
	uint32_t *offsets = (uint32_t *)mp4TaggedAlloc(this->allocator, 4, entryCount * 16);
	if (counts == NULL || offsets == NULL) {
		mp4TaggedFree(this->allocator, counts);
		mp4TaggedFree(this->allocator, offsets);
		if (cttsAtom != NULL)
			cttsAtom->unmapBuffer();
		return 0;
	}

	sceClibMemset(counts, 0, entryCount * 4);
	sceClibMemset(offsets, 0, entryCount * 4);

	uint32_t total = 0;
	for (uint32_t i = 0; i < entryCount; i++) {
		counts[i] = mp4ReadBe32(buf + 8 + i * 8);
		offsets[i] = mp4ReadBe32(buf + 12 + i * 8);
		total += counts[i];
	}

	this->cttsTotalSamples = total;
	if (total != 0) {
		if (this->cttsOffsets90k == NULL) {
			this->cttsOffsets90k = (int64_t *)this->allocator->allocate(this->allocator->objectPointer, 8, total * 8);
			if (this->cttsOffsets90k == NULL) {
				mp4TaggedFree(this->allocator, counts);
				mp4TaggedFree(this->allocator, offsets);
				if (cttsAtom != NULL)
					cttsAtom->unmapBuffer();
				return 0;
			}
		}

		uint32_t dstIdx = 0;
		for (uint32_t i = 0; i < entryCount; i++) {
			int64_t scaled = (90000LL * (int32_t)offsets[i]) / (int64_t)this->mediaTimescale;
			for (uint32_t s = 0; s < counts[i]; s++)
				this->cttsOffsets90k[dstIdx++] = scaled;
		}
	}

	mp4TaggedFree(this->allocator, counts);
	mp4TaggedFree(this->allocator, offsets);
	if (cttsAtom != NULL)
		cttsAtom->unmapBuffer();
	return 1;
}

int Mp4Track::loadSyncSamples(Mp4Atom *stssAtom)
{
	if (stssAtom == NULL)
		return 0;

	uint8_t *buf = stssAtom->mapBuffer();
	if (buf != NULL)
		buf += stssAtom->getHeaderSize();
	if (buf == NULL) {
		stssAtom->unmapBuffer();
		return 0;
	}

	uint32_t count = mp4ReadBe32(buf + 4);
	this->syncSampleCount = count;
	this->syncSampleIndices = (uint32_t *)mp4TaggedAlloc(this->allocator, 4, count * 4);
	if (this->syncSampleIndices == NULL) {
		stssAtom->unmapBuffer();
		return 0;
	}

	for (uint32_t i = 0; i < count; i++)
		this->syncSampleIndices[i] = mp4ReadBe32(buf + 8 + i * 4) - 1;

	stssAtom->unmapBuffer();
	return 1;
}

int Mp4Track::loadSeekTables()
{
	if (this->streamFormat == NULL)
		return 0;

	if (this->streamFormat->isVideo()) {
		if (this->stblAtom == NULL)
			return 0;
		Mp4Atom *stssAtom = this->stblAtom->findChildByType(FOURCC_STSS, 0);
		if (stssAtom != NULL)
			this->loadSyncSamples(stssAtom);
		Mp4Atom *cttsAtom = this->stblAtom->findChildByType(FOURCC_CTTS, 0);
		if (cttsAtom != NULL)
			this->loadCompositionOffsets(cttsAtom);
	}
	return 1;
}

int Mp4Movie::readData(int64_t offset, uint32_t length, void *dst)
{
	if (this->rootAtom == NULL)
		return MP4_ERR_NO_MEMORY;
	return this->rootAtom->readData(offset, length, dst);
}

Mp4Movie::Mp4Movie(Mp4Atom *inRootAtom, SceAvPlayerMemAllocator *inAllocator, int32_t inStreamFlags)
{
	SceKernelLwMutexOptParam opt;
	Mp4Track *nullTrack = NULL;

	this->rootAtom = inRootAtom;
	this->rootAtomRef = mp4CreateRefControl();
	this->tracks.head = NULL;
	this->tracks.tail = NULL;
	this->tracks.count = 0;
	this->tracks.totalBytes = 0;
	this->tracks.isShallowCopy = 0;
	this->tracks.beginIter = NULL;
	this->tracks.endIter = NULL;

	sceClibMemset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	sceKernelCreateLwMutex(&this->tracks.mutex, "0Demux_Stl_Handler_Mutex ", 0, 0, &opt);

	Mp4ListNode<Mp4Track *> *sentinel = (Mp4ListNode<Mp4Track *> *)mp4MspaceAllocZero(8, sizeof(Mp4ListNode<Mp4Track *>));
	if (sentinel != NULL) {
		sentinel->prev = NULL;
		sentinel->next = NULL;
		sceClibMemcpy_safe(&sentinel->value, &nullTrack, sizeof(nullTrack));
	}
	this->tracks.sentinel = sentinel;

	this->movieTimescale = 0;
	this->movieDurationRaw = 0;
	this->movieDuration100us = 0;
	this->streamFlags = inStreamFlags;
	this->isFragmented = 0;
	this->fragmentMgr = NULL;

	int32_t firstVideoTimescale = -1;
	if (this->rootAtom != NULL) {
		Mp4Atom *moovAtom = this->rootAtom->findChildByType(FOURCC_MOOV, 0);
		if (moovAtom != NULL) {
			Mp4Atom *mvhdAtom = moovAtom->findChildByType(FOURCC_MVHD, 0);
			if (mvhdAtom != NULL) {
				uint8_t *mvhdBuf = mvhdAtom->mapBuffer();
				if (mvhdBuf != NULL)
					mvhdBuf += mvhdAtom->getHeaderSize();
				if (mvhdBuf != NULL) {
					if (mvhdBuf[0] == 1) {
						this->movieTimescale = (int32_t)mp4ReadBe32(mvhdBuf + 20);
						this->movieDurationRaw = mp4ReadBe64(mvhdBuf + 24);
					} else {
						this->movieTimescale = (int32_t)mp4ReadBe32(mvhdBuf + 12);
						this->movieDurationRaw = (int32_t)mp4ReadBe32(mvhdBuf + 16);
					}
				} else {
					this->movieTimescale = 1;
					this->movieDurationRaw = 0;
				}
				if (this->movieTimescale == 0)
					this->movieTimescale = 1;
				this->movieDuration100us = (int64_t)(10000ULL * this->movieDurationRaw) / this->movieTimescale;
				mvhdAtom->unmapBuffer();
			}

			int childCount = moovAtom->getChildCount();
			int32_t trackNum = 0;
			for (int i = 0; i < childCount; i++) {
				Mp4Atom *child = moovAtom->getChildAt(i);
				if (child->getFourCC() != FOURCC_TRAK)
					continue;

				Mp4Track *trk = new Mp4Track(child, this, trackNum++, inAllocator, inStreamFlags);
				if (trk == NULL)
					continue;

				if (trk->trakAtom != NULL) {
					sceKernelLockLwMutex(&this->tracks.mutex, 1, NULL);
					Mp4ListNode<Mp4Track *> *node = (Mp4ListNode<Mp4Track *> *)mp4MspaceAllocZero(8, sizeof(Mp4ListNode<Mp4Track *>));
					if (node != NULL) {
						node->prev = NULL;
						node->next = NULL;
						sceClibMemcpy_safe(&node->value, &trk, sizeof(trk));
					}
					if (this->tracks.tail != NULL) {
						this->tracks.tail->next = node;
						node->prev = this->tracks.tail;
					} else {
						this->tracks.head = node;
					}
					this->tracks.tail = node;
					this->tracks.count++;
					this->tracks.totalBytes += sizeof(Mp4ListNode<Mp4Track *>);
					sceKernelUnlockLwMutex(&this->tracks.mutex, 1);

					if (firstVideoTimescale == -1 && trk->isVideo())
						firstVideoTimescale = trk->mediaTimescale;
				} else {
					trk->~Mp4Track();
					mp4MspaceFree(trk);
				}
			}
		}
	}

	int moofCount = this->rootAtom->countChildrenByType(FOURCC_MOOF);
	if (moofCount != 0) {
		this->isFragmented = 1;
		Mp4FragmentManager *mgr = new Mp4FragmentManager(this->rootAtom, firstVideoTimescale, moofCount, (const uint32_t *)inAllocator, inStreamFlags);
		this->fragmentMgr = mgr;

		if (mgr != NULL) {
			mgr->loadNextMoof();
			for (uint32_t i = 0; i < this->tracks.count; i++) {
				Mp4TrackFragment *frag = mgr->getTrackFragment(i);
				if (frag != NULL) {
					Mp4Track *trk = mp4TrackListElem(&this->tracks, i);
					trk->fragmentTrack = frag;
					trk->fragmentMgr = mgr;
					frag->mediaTimescale = (uint32_t)trk->mediaTimescale;
					frag->trackDuration = trk->trackDuration;
					frag->timeScaleFactor = 1.0f;
				}
			}
		}
	}
}

bool Mp4Track::isVideo() const
{
	return this->streamFormat->isVideo();
}

bool Mp4Track::isAudio() const
{
	return this->streamFormat->isAudio();
}

bool Mp4Track::isTimedText() const
{
	return this->streamFormat->isTimedText();
}

bool Mp4Track::selectStreamDescription(const Mp4StreamDescription *desc)
{
	return this->streamFormat->selectStreamDescription(desc);
}

Mp4SampleExtractor *Mp4Track::getExtractor()
{
	return this->streamFormat->extractor;
}
