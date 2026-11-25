#include "mp4_internal.h"

static const int32_t MP4_FRAGMENT_SAMPLE_SENTINEL = (int32_t)0xDEADBEED;

static Mp4ListNode<Mp4TrackFragment *> *mp4FragListGetAt(Mp4ListNode<Mp4TrackFragment *> *node, uint32_t curIdx, uint32_t targetIdx)
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

static Mp4ListNode<Mp4TrackFragment *> *mp4FragListFindPrev(Mp4ListNode<Mp4TrackFragment *> *node, Mp4ListNode<Mp4TrackFragment *> *target)
{
	if (node == target)
		return node->prev;
	return mp4FragListFindPrev(node->next, target);
}

static Mp4ListNode<Mp4SidxEntry> *mp4SidxListGetAt(Mp4ListNode<Mp4SidxEntry> *node, uint32_t curIdx, uint32_t targetIdx)
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

static Mp4ListNode<Mp4SidxEntry> *mp4SidxListFindPrev(Mp4ListNode<Mp4SidxEntry> *node, Mp4ListNode<Mp4SidxEntry> *target)
{
	if (node == target)
		return node->prev;
	return mp4SidxListFindPrev(node->next, target);
}

static void mp4FreeBoxBuffer(Mp4BoxBuffer **slot)
{
	Mp4BoxBuffer *buf = *slot;
	if (buf != NULL) {
		mp4MspaceFree(buf->data);
		buf->data = NULL;
		buf->size = 0;
		buf->capacity = 0;
		mp4MspaceFree(buf);
		*slot = NULL;
	}
}

static inline __attribute__((always_inline)) void mp4CopyBoxToBuffer(Mp4BoxBuffer **slot, Mp4Atom *atom)
{
	uint8_t *payload = atom->mapBuffer();
	if (payload != NULL)
		payload += atom->getHeaderSize();
	uint32_t sz = (uint32_t)atom->getSize();

	Mp4BoxBuffer *buf = *slot;
	if (buf == NULL) {
		buf = (Mp4BoxBuffer *)mp4MspaceAllocZero(4, sizeof(Mp4BoxBuffer));
		if (buf == NULL)
			return;
		uint32_t cap = (sz + 597u) & 0xFFFFFDAAu;
		buf->capacity = cap;
		buf->data = (uint8_t *)mp4MspaceAllocZero(4, cap);
		if (buf->data != NULL) {
			buf->size = sz;
			sceClibMemset(buf->data, 0, buf->capacity);
			sceClibMemcpy_safe(buf->data, payload, buf->size);
		}
		*slot = buf;
		return;
	}

	if (sz > buf->capacity) {
		mp4MspaceFree(buf->data);
		buf->data = NULL;
		uint32_t cap = (sz + 597u) & 0xFFFFFDAAu;
		buf->capacity = cap;
		buf->data = (uint8_t *)mp4MspaceAllocZero(4, cap);
		if (buf->data == NULL)
			return;
	}
	buf->size = sz;
	sceClibMemset(buf->data, 0, buf->capacity);
	sceClibMemcpy_safe(buf->data, payload, buf->size);
}

Mp4TrackFragment::Mp4TrackFragment(const uint32_t *inParams, int32_t fragmentCount, int32_t inHint)
{
	this->trunBuf = NULL;
	this->sdtpBuf = NULL;
	this->subsBuf = NULL;
	this->enabled = 0;
	this->trackId = 0;
	this->baseDataOffset = 0;
	this->defaultSampleDuration = 0.0f;
	this->defaultSampleSize = 0;
	this->sampleCount = 0;
	this->currentSampleOffset = 0;
	this->currentDts = 0.0f;
	this->currentPts = 0.0f;
	this->currentSampleSize = 0;
	this->currentSampleIndex = 0;
	this->mediaTimescale = 1;
	this->trackDuration = 0;
	this->totalFragments = fragmentCount;
	this->timeScaleFactor = 1.0f;
	this->fragmentDuration = 0.0f;
	this->initParams[0] = inParams[0];
	this->initParams[1] = inParams[1];
	this->initParams[2] = inParams[2];
	this->trackHint = inHint;
}

int Mp4TrackFragment::clear()
{
	mp4FreeBoxBuffer(&this->trunBuf);
	mp4FreeBoxBuffer(&this->sdtpBuf);
	mp4FreeBoxBuffer(&this->subsBuf);
	this->baseDataOffset = 0;
	this->defaultSampleDuration = 0.0f;
	this->defaultSampleSize = 0;
	this->sampleCount = 0;
	this->currentSampleSize = 0;
	this->currentSampleIndex = 0;
	this->currentSampleOffset = 0;
	return 0;
}

Mp4TrackFragment::~Mp4TrackFragment()
{
	this->clear();
}

int Mp4TrackFragment::advanceSampleInfo()
{
	float dur = this->defaultSampleDuration;
	if (this->fragmentDuration != 0.0f) {
		dur = this->fragmentDuration / (float)this->sampleCount;
		this->defaultSampleDuration = dur;
	} else if (dur == 0.0f && this->trackDuration > 0 && this->totalFragments != 0 && this->sampleCount != 0) {
		dur = ((float)this->trackDuration / (float)(uint32_t)this->totalFragments) / (float)this->sampleCount;
		this->defaultSampleDuration = dur;
	}

	const uint8_t *trun = this->trunBuf->data;
	uint32_t flags = mp4ReadBe32(trun);
	bool hasDataOffset = (flags & 0x001u) != 0;
	bool hasFirstFlags = (flags & 0x004u) != 0;
	bool hasDuration = (flags & 0x100u) != 0;
	bool hasSize = (flags & 0x200u) != 0;
	bool hasFlags = (flags & 0x400u) != 0;
	bool hasCto = (flags & 0x800u) != 0;

	uint32_t wordIdx = 2;
	if (hasDataOffset)
		wordIdx++;
	if (hasFirstFlags)
		wordIdx++;

	this->currentSampleOffset += (uint32_t)this->currentSampleSize;

	uint32_t entryStride = 0;
	if (hasDuration) entryStride += 4;
	if (hasSize)     entryStride += 4;
	if (hasFlags)    entryStride += 4;
	if (hasCto)      entryStride += 4;

	const uint8_t *entryPtr = trun + wordIdx * 4 + (uint32_t)this->currentSampleIndex * entryStride;
	uint32_t fieldOff = 0;

	float dts = this->currentDts;
	if (dur != 0.0f) {
		dts += dur;
		this->currentDts = dts;
		if (hasDuration)
			fieldOff += 4;
	} else if (hasDuration) {
		dts += (float)(int32_t)mp4ReadBe32(entryPtr + fieldOff);
		this->currentDts = dts;
		fieldOff += 4;
	}

	if (hasSize) {
		this->currentSampleSize = (int32_t)mp4ReadBe32(entryPtr + fieldOff);
		fieldOff += 4;
	} else if (this->defaultSampleSize != 0) {
		this->currentSampleSize = (int32_t)this->defaultSampleSize;
	}

	if (hasFlags)
		fieldOff += 4;

	this->currentPts = dts;
	if (hasCto)
		this->currentPts = dts + (float)(int32_t)mp4ReadBe32(entryPtr + fieldOff);

	return 0;
}

int Mp4TrackFragment::nextSample()
{
	if (this->currentSampleIndex == MP4_FRAGMENT_SAMPLE_SENTINEL)
		return MP4_ERR_END_OF_FRAGMENT;

	uint32_t nextIdx = (uint32_t)(this->currentSampleIndex + 1);
	if (nextIdx < this->sampleCount) {
		this->currentSampleIndex = (int32_t)nextIdx;
		this->advanceSampleInfo();
	} else {
		this->currentSampleIndex = MP4_FRAGMENT_SAMPLE_SENTINEL;
	}
	return 0;
}

int Mp4TrackFragment::prevSample()
{
	if (this->currentSampleIndex == MP4_FRAGMENT_SAMPLE_SENTINEL)
		return MP4_ERR_END_OF_FRAGMENT;

	if (this->currentSampleIndex != 0) {
		this->currentSampleIndex--;
		this->advanceSampleInfo();
	} else {
		this->currentSampleIndex = MP4_FRAGMENT_SAMPLE_SENTINEL;
	}
	return 0;
}

int Mp4TrackFragment::parseTfhd(Mp4Atom *tfhdAtom)
{
	uint8_t *buf = NULL;
	if (tfhdAtom != NULL) {
		buf = tfhdAtom->mapBuffer();
		if (buf != NULL)
			buf += tfhdAtom->getHeaderSize();
	}

	uint32_t flags = mp4ReadBe32(buf);
	this->trackId = mp4ReadBe32(buf + 4);
	this->baseDataOffset = 0;

	uint32_t idx = 2;
	if ((flags & 0x01u) != 0) {
		this->baseDataOffset = (int64_t)mp4ReadBe64(buf + idx * 4);
		idx += 2;
	}
	if ((flags & 0x02u) != 0)
		idx++;
	if ((flags & 0x08u) != 0) {
		this->defaultSampleDuration = (float)(int32_t)mp4ReadBe32(buf + idx * 4);
		idx++;
	}
	if ((flags & 0x10u) != 0)
		this->defaultSampleSize = (int32_t)mp4ReadBe32(buf + idx * 4);

	if (tfhdAtom != NULL)
		tfhdAtom->unmapBuffer();
	return 0;
}

int Mp4TrackFragment::parseTrafBoxes(Mp4Atom *trafAtom, int64_t moofOffset)
{
	if (trafAtom == NULL)
		return MP4_ERR_MISSING_ATOM;

	if (this->baseDataOffset == 0)
		this->baseDataOffset = moofOffset;

	Mp4Atom *trunAtom = trafAtom->findChildByType(FOURCC_TRUN, 0);
	if (trunAtom != NULL)
		mp4CopyBoxToBuffer(&this->trunBuf, trunAtom);

	Mp4Atom *sdtpAtom = trafAtom->findChildByType(FOURCC_SDTP, 0);
	if (sdtpAtom != NULL)
		mp4CopyBoxToBuffer(&this->sdtpBuf, sdtpAtom);

	Mp4Atom *subsAtom = trafAtom->findChildByType(FOURCC_SUBS, 0);
	if (subsAtom != NULL)
		mp4CopyBoxToBuffer(&this->subsBuf, subsAtom);

	return 0;
}

int Mp4TrackFragment::prepareRun(float startTime, float runDuration)
{
	if (this->trunBuf == NULL)
		return MP4_ERR_MISSING_ATOM;

	if (runDuration != 0.0f)
		this->fragmentDuration = runDuration;
	if (startTime != 0.0f)
		this->currentDts = startTime;

	const uint8_t *trun = this->trunBuf->data;
	uint8_t flagsLow = trun[3];
	this->sampleCount = mp4ReadBe32(trun + 4);
	int64_t baseOff = this->baseDataOffset;
	if ((flagsLow & 0x01u) != 0) {
		baseOff += (int32_t)mp4ReadBe32(trun + 8);
		this->baseDataOffset = baseOff;
	}
	this->currentSampleOffset = baseOff;
	this->advanceSampleInfo();
	return 0;
}

int64_t Mp4TrackFragment::getCurrentPts()
{
	if (this->currentSampleIndex == MP4_FRAGMENT_SAMPLE_SENTINEL)
		return 0x7FFFFFFFFFFFFFFFLL;
	return (int64_t)(this->currentPts * this->timeScaleFactor);
}

int64_t Mp4TrackFragment::getCurrentDts()
{
	if (this->currentSampleIndex == MP4_FRAGMENT_SAMPLE_SENTINEL)
		return 0x7FFFFFFFFFFFFFFFLL;
	return (int64_t)(this->currentDts * this->timeScaleFactor);
}

bool Mp4TrackFragment::isAtEnd()
{
	if (this->currentSampleIndex == MP4_FRAGMENT_SAMPLE_SENTINEL)
		return true;
	return (uint32_t)this->currentSampleIndex == this->sampleCount;
}

Mp4FragmentManager::Mp4FragmentManager(Mp4Atom *inRoot, int32_t primaryVideoTimescale, int32_t moofCount, const uint32_t *inParams, int32_t inHint)
{
	SceKernelLwMutexOptParam opt;
	Mp4TrackFragment *nullFrag = NULL;
	Mp4SidxEntry zeroSidx;

	this->rootAtom = inRoot;
	this->sequenceNumber = 0;
	this->currentMoofIndex = 0;
	this->primaryTimescale = primaryVideoTimescale;
	this->totalMoofCount = moofCount;
	this->reachedEof = 0;

	this->trackFragments.head = NULL;
	this->trackFragments.tail = NULL;
	this->trackFragments.count = 0;
	this->trackFragments.totalBytes = 0;
	this->trackFragments.isShallowCopy = 0;
	this->trackFragments.beginIter = NULL;
	this->trackFragments.endIter = NULL;
	sceClibMemset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	sceKernelCreateLwMutex(&this->trackFragments.mutex, "0Demux_Stl_Handler_Mutex ", 0, 0, &opt);

	Mp4ListNode<Mp4TrackFragment *> *fragSent = (Mp4ListNode<Mp4TrackFragment *> *)mp4MspaceAllocZero(8, sizeof(Mp4ListNode<Mp4TrackFragment *>));
	if (fragSent != NULL) {
		fragSent->prev = NULL;
		fragSent->next = NULL;
		sceClibMemcpy_safe(&fragSent->value, &nullFrag, sizeof(nullFrag));
	}
	this->trackFragments.sentinel = fragSent;

	this->sidxEntries.head = NULL;
	this->sidxEntries.tail = NULL;
	this->sidxEntries.count = 0;
	this->sidxEntries.totalBytes = 0;
	this->sidxEntries.isShallowCopy = 0;
	this->sidxEntries.beginIter = NULL;
	this->sidxEntries.endIter = NULL;
	sceClibMemset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	sceKernelCreateLwMutex(&this->sidxEntries.mutex, "0Demux_Stl_Handler_Mutex ", 0, 0, &opt);

	sceClibMemset(&zeroSidx, 0, sizeof(zeroSidx));
	Mp4ListNode<Mp4SidxEntry> *sidxSent = (Mp4ListNode<Mp4SidxEntry> *)mp4MspaceAllocZero(8, sizeof(Mp4ListNode<Mp4SidxEntry>));
	if (sidxSent != NULL) {
		sidxSent->prev = NULL;
		sidxSent->next = NULL;
		sceClibMemcpy_safe(&sidxSent->value, &zeroSidx, sizeof(zeroSidx));
	}
	this->sidxEntries.sentinel = sidxSent;

	this->initParams[0] = inParams[0];
	this->initParams[1] = inParams[1];
	this->initParams[2] = inParams[2];
	this->trackHint = inHint;
}

void Mp4FragmentManager::clear()
{
	for (uint32_t i = 0; i < this->trackFragments.count; i++) {
		Mp4TrackFragment *frag = this->getTrackFragment(i);
		if (frag != NULL) {
			frag->clear();
			mp4MspaceFree(frag);
		}
	}

	while (this->trackFragments.count != 0) {
		sceKernelLockLwMutex(&this->trackFragments.mutex, 1, NULL);
		if (this->trackFragments.count != 0 && this->trackFragments.tail != NULL) {
			Mp4ListNode<Mp4TrackFragment *> *tail = this->trackFragments.tail;
			Mp4ListNode<Mp4TrackFragment *> *prev = mp4FragListFindPrev(tail, tail);
			if (prev != NULL)
				prev->next = NULL;
			mp4MspaceFree(tail);
			this->trackFragments.tail = prev;
			this->trackFragments.count--;
			this->trackFragments.totalBytes -= sizeof(Mp4ListNode<Mp4TrackFragment *>);
		}
		sceKernelUnlockLwMutex(&this->trackFragments.mutex, 1);
	}

	while (this->sidxEntries.count != 0) {
		sceKernelLockLwMutex(&this->sidxEntries.mutex, 1, NULL);
		if (this->sidxEntries.count != 0 && this->sidxEntries.tail != NULL) {
			Mp4ListNode<Mp4SidxEntry> *tail = this->sidxEntries.tail;
			Mp4ListNode<Mp4SidxEntry> *prev = mp4SidxListFindPrev(tail, tail);
			if (prev != NULL)
				prev->next = NULL;
			mp4MspaceFree(tail);
			this->sidxEntries.tail = prev;
			this->sidxEntries.count--;
			this->sidxEntries.totalBytes -= sizeof(Mp4ListNode<Mp4SidxEntry>);
		}
		sceKernelUnlockLwMutex(&this->sidxEntries.mutex, 1);
	}
}

Mp4FragmentManager::~Mp4FragmentManager()
{
	this->clear();

	if (this->sidxEntries.isShallowCopy == 0) {
		if (this->sidxEntries.beginIter != NULL)
			delete this->sidxEntries.beginIter;
		if (this->sidxEntries.endIter != NULL)
			delete this->sidxEntries.endIter;
		if (this->sidxEntries.sentinel != NULL)
			mp4MspaceFree(this->sidxEntries.sentinel);
	}
	sceKernelDeleteLwMutex(&this->sidxEntries.mutex);

	if (this->trackFragments.isShallowCopy == 0) {
		if (this->trackFragments.beginIter != NULL)
			delete this->trackFragments.beginIter;
		if (this->trackFragments.endIter != NULL)
			delete this->trackFragments.endIter;
		if (this->trackFragments.sentinel != NULL)
			mp4MspaceFree(this->trackFragments.sentinel);
	}
	sceKernelDeleteLwMutex(&this->trackFragments.mutex);
}

Mp4SidxEntry *Mp4FragmentManager::getSidxEntry(uint32_t index)
{
	if (index >= this->sidxEntries.count)
		return NULL;
	if (index >= (this->sidxEntries.count >> 1))
		return &mp4SidxListGetAt(this->sidxEntries.tail, this->sidxEntries.count - 1, index)->value;
	return &mp4SidxListGetAt(this->sidxEntries.head, 0, index)->value;
}

Mp4TrackFragment *Mp4FragmentManager::getTrackFragment(uint32_t index)
{
	if (index >= this->trackFragments.count)
		return NULL;
	if (index >= (this->trackFragments.count >> 1))
		return mp4FragListGetAt(this->trackFragments.tail, this->trackFragments.count - 1, index)->value;
	return mp4FragListGetAt(this->trackFragments.head, 0, index)->value;
}

int Mp4FragmentManager::parseSidx()
{
	uint32_t sidxCount = (uint32_t)this->rootAtom->countChildrenByType(FOURCC_SIDX);
	uint32_t existing = this->sidxEntries.count;
	if (sidxCount < existing)
		return 0;

	float runningStart = 0.0f;
	if (existing != 0) {
		Mp4SidxEntry *last = this->getSidxEntry(existing - 1);
		if (last != NULL)
			runningStart = last->startTimeTicks + last->durationTicks;
	}

	for (uint32_t idx = existing; idx < sidxCount; idx++) {
		Mp4Atom *sidxAtom = this->rootAtom->findChildByType(FOURCC_SIDX, (int)idx);
		if (sidxAtom == NULL)
			continue;

		uint8_t *buf = sidxAtom->mapBuffer();
		if (buf != NULL)
			buf += sidxAtom->getHeaderSize();

		uint8_t version = buf[0];
		uint32_t refCountOff = (version == 0) ? 20u : 28u;
		uint16_t extraCount = mp4ReadBe16(buf + refCountOff);
		uint16_t entryCount = mp4ReadBe16(buf + refCountOff + 2);
		uint32_t cursor = refCountOff + 4 + extraCount * ((version == 0) ? 4u : 8u);

		for (uint16_t e = 0; e < entryCount; e++) {
			const uint8_t *rec = buf + cursor + e * 12;
			uint32_t typeAndSize = mp4ReadBe32(rec);
			uint32_t subsegDur = mp4ReadBe32(rec + 4);
			uint32_t sapWord = mp4ReadBe32(rec + 8);
			bool isSubsegment = (typeAndSize >> 31) != 0;
			if (!isSubsegment) {
				Mp4SidxEntry entry;
				entry.referencedSize = typeAndSize & 0x7FFFFFFFu;
				entry.durationTicks = ((float)subsegDur / (float)this->primaryTimescale) * 90000.0f;
				entry.startTimeTicks = runningStart;
				entry.startsWithSap = (uint8_t)(sapWord >> 31);
				entry.sapDeltaTime = (entry.startsWithSap != 0) ? (sapWord & 0x7FFFFFFFu) : 0;

				sceKernelLockLwMutex(&this->sidxEntries.mutex, 1, NULL);
				Mp4ListNode<Mp4SidxEntry> *node = (Mp4ListNode<Mp4SidxEntry> *)mp4MspaceAllocZero(8, sizeof(Mp4ListNode<Mp4SidxEntry>));
				if (node != NULL) {
					node->prev = NULL;
					node->next = NULL;
					sceClibMemcpy_safe(&node->value, &entry, sizeof(entry));
				}
				if (this->sidxEntries.tail != NULL) {
					this->sidxEntries.tail->next = node;
					node->prev = this->sidxEntries.tail;
				} else {
					this->sidxEntries.head = node;
				}
				this->sidxEntries.tail = node;
				this->sidxEntries.count++;
				this->sidxEntries.totalBytes += sizeof(Mp4ListNode<Mp4SidxEntry>);
				sceKernelUnlockLwMutex(&this->sidxEntries.mutex, 1);

				runningStart += entry.durationTicks;
			}
		}

		sidxAtom->unmapBuffer();
	}
	return 0;
}

int Mp4FragmentManager::loadMoof(Mp4Atom *moofAtom)
{
	int64_t moofOffset = moofAtom->getOffset();
	Mp4Atom *mfhdAtom = moofAtom->findChildByType(FOURCC_MFHD, 0);
	if (mfhdAtom == NULL)
		return MP4_ERR_MISSING_ATOM;

	uint8_t *mfhdBuf = mfhdAtom->mapBuffer();
	if (mfhdBuf != NULL)
		mfhdBuf += mfhdAtom->getHeaderSize();
	this->sequenceNumber = mp4ReadBe32(mfhdBuf + 4);

	float sidxStart = 0.0f;
	float sidxDur = 0.0f;
	Mp4SidxEntry *sidx = this->getSidxEntry(this->currentMoofIndex);
	if (sidx != NULL) {
		sidxStart = sidx->startTimeTicks;
		sidxDur = sidx->durationTicks;
	}

	for (uint32_t i = 0; ; i++) {
		Mp4Atom *trafAtom = moofAtom->findChildByType(FOURCC_TRAF, (int)i);
		if (trafAtom == NULL)
			break;

		Mp4TrackFragment *frag;
		if (i < this->trackFragments.count) {
			frag = this->getTrackFragment(i);
			frag->clear();
		} else {
			frag = new Mp4TrackFragment(this->initParams, this->totalMoofCount, this->trackHint);
			if (frag == NULL) {
				mfhdAtom->unmapBuffer();
				return MP4_ERR_NO_MEMORY;
			}

			sceKernelLockLwMutex(&this->trackFragments.mutex, 1, NULL);
			Mp4ListNode<Mp4TrackFragment *> *node = (Mp4ListNode<Mp4TrackFragment *> *)mp4MspaceAllocZero(8, sizeof(Mp4ListNode<Mp4TrackFragment *>));
			if (node != NULL) {
				node->prev = NULL;
				node->next = NULL;
				sceClibMemcpy_safe(&node->value, &frag, sizeof(frag));
			}
			if (this->trackFragments.tail != NULL) {
				this->trackFragments.tail->next = node;
				node->prev = this->trackFragments.tail;
			} else {
				this->trackFragments.head = node;
			}
			this->trackFragments.tail = node;
			this->trackFragments.count++;
			this->trackFragments.totalBytes += sizeof(Mp4ListNode<Mp4TrackFragment *>);
			sceKernelUnlockLwMutex(&this->trackFragments.mutex, 1);
		}

		Mp4Atom *tfhdAtom = trafAtom->findChildByType(FOURCC_TFHD, 0);
		if (tfhdAtom == NULL) {
			mfhdAtom->unmapBuffer();
			return MP4_ERR_MISSING_ATOM;
		}

		int err = frag->parseTfhd(tfhdAtom);
		if (err != 0) {
			mfhdAtom->unmapBuffer();
			return err;
		}
		err = frag->parseTrafBoxes(trafAtom, moofOffset);
		if (err != 0) {
			mfhdAtom->unmapBuffer();
			return err;
		}
		err = frag->prepareRun(sidxStart, sidxDur);
		if (err != 0) {
			mfhdAtom->unmapBuffer();
			return err;
		}
	}

	mfhdAtom->unmapBuffer();
	return 0;
}

int Mp4FragmentManager::dispatchBox(Mp4Atom *atom, int mode)
{
	if (mode == 1)
		return this->loadMoof(atom);
	if (mode == 2)
		return this->parseSidx();
	return MP4_ERR_GENERIC;
}

int Mp4FragmentManager::loadNextMoof()
{
	if (this->currentMoofIndex != 0) {
		Mp4Atom *prevMoof = this->rootAtom->findChildByType(FOURCC_MOOF, (int)(this->currentMoofIndex - 1));
		if (prevMoof != NULL) {
			if (prevMoof->mapBuffer() != NULL)
				prevMoof->getHeaderSize();
			prevMoof->unmapBuffer();
			prevMoof->unmapBuffer();
		}
	}

	Mp4Atom *nextMoof = this->rootAtom->findOrParseChild(FOURCC_MOOF, (int)this->currentMoofIndex);
	if (nextMoof == NULL) {
		this->reachedEof = 1;
		return MP4_ERR_EOF;
	}

	this->parseSidx();
	int err = this->loadMoof(nextMoof);
	if (err == 0)
		this->currentMoofIndex++;
	return err;
}

int Mp4FragmentManager::loadPrevMoof()
{
	if (this->currentMoofIndex == 0) {
		this->reachedEof = 1;
		return MP4_ERR_EOF;
	}

	Mp4Atom *prevMoof = this->rootAtom->findChildByType(FOURCC_MOOF, (int)(this->currentMoofIndex - 1));
	if (prevMoof == NULL)
		return MP4_ERR_EOF;

	int err = this->loadMoof(prevMoof);
	if (err == 0)
		this->currentMoofIndex--;
	return err;
}

int Mp4FragmentManager::seekToTime(int32_t unused, uint64_t targetTicks90k, int64_t *outActualTicks)
{
	(void)unused;
	uint32_t count = this->sidxEntries.count;
	if (count == 0)
		return MP4_ERR_SEEK_OUT_OF_RANGE;

	float target = (float)targetTicks90k;
	Mp4SidxEntry *last = this->getSidxEntry(count - 1);
	if (last == NULL || last->startTimeTicks + last->durationTicks < target)
		return MP4_ERR_SEEK_OUT_OF_RANGE;

	Mp4SidxEntry *first = this->getSidxEntry(0);
	if (first->startTimeTicks == target) {
		this->currentMoofIndex = 0;
		this->loadNextMoof();
		*outActualTicks = (int64_t)target;
		return 0;
	}

	if (last->startTimeTicks < target) {
		this->currentMoofIndex = count - 1;
		this->loadNextMoof();
		*outActualTicks = (int64_t)last->startTimeTicks;
		return 0;
	}

	Mp4SidxEntry *chosen = first;
	uint32_t chosenIdx = 0;
	for (uint32_t i = 1; i < count; i++) {
		Mp4SidxEntry *cur = this->getSidxEntry(i);
		if (cur->startsWithSap == 0)
			continue;
		if (cur->startTimeTicks >= target && target > chosen->startTimeTicks) {
			if (cur->startTimeTicks - target < target - chosen->startTimeTicks) {
				chosen = cur;
				chosenIdx = i;
			}
			break;
		}
		chosen = cur;
		chosenIdx = i;
	}

	*outActualTicks = (int64_t)(chosen->startTimeTicks * 90000.0f);
	Mp4Atom *curMoof = this->rootAtom->findOrParseChild(FOURCC_MOOF, (int)this->currentMoofIndex);
	if (curMoof != NULL) {
		if (curMoof->mapBuffer() != NULL)
			curMoof->getHeaderSize();
		curMoof->unmapBuffer();
		curMoof->unmapBuffer();
	}
	this->currentMoofIndex = chosenIdx;
	return this->loadNextMoof();
}

int64_t Mp4TrackFragment::getCurrentOffset()
{
	return this->currentSampleOffset;
}

int32_t Mp4TrackFragment::getCurrentSampleSize()
{
	return this->currentSampleSize;
}

bool Mp4TrackFragment::isAtFirstSample()
{
	return this->currentSampleIndex == 0;
}

bool Mp4FragmentManager::isEndOfFile()
{
	return this->reachedEof != 0;
}
