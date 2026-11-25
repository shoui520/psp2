#include "mp4_internal.h"

int g_mp4DebugLevel = 0;
SceClibMspace g_mp4Mspace = NULL;
SceKernelLwMutexWork g_mp4DemuxMutex;

/* Global mspace allocator helpers */
int mp4MspaceInit(SceSize size, void *memblock)
{
	if (memblock == NULL)
		return -1;
	g_mp4Mspace = sceClibMspaceCreate(memblock, size);
	return (g_mp4Mspace != NULL) ? 0 : -1;
}

int mp4MspaceDestroy(void)
{
	sceClibMspaceDestroy(g_mp4Mspace);
	g_mp4Mspace = NULL;
	return 0;
}

void operator delete(void *ptr) throw()
{
	sceClibMspaceFree(g_mp4Mspace, ptr);
}

void *mp4MspaceAllocZero(int alignment, SceSize size)
{
	(void)alignment;
	SceSize aligned = (size + 3u) & ~3u;
	void *mem = sceClibMspaceMalloc(g_mp4Mspace, aligned);
	if (mem != NULL)
		sceClibMemset(mem, 0, aligned);
	return mem;
}

/* Cursor list helpers */
static Mp4ListNode<Mp4TrackCursor> *mp4CursorListGetAt(Mp4ListNode<Mp4TrackCursor> *node, uint32_t curIdx, uint32_t targetIdx)
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

static Mp4ListNode<Mp4TrackCursor> *mp4CursorListFindPrev(Mp4ListNode<Mp4TrackCursor> *node, Mp4ListNode<Mp4TrackCursor> *target)
{
	if (node == target)
		return node->prev;
	return mp4CursorListFindPrev(node->next, target);
}

static Mp4TrackCursor *mp4CursorListElem(const Mp4List<Mp4TrackCursor> *list, uint32_t index)
{
	if (index >= list->count)
		return &list->sentinel->value;
	if (index >= (list->count >> 1))
		return &mp4CursorListGetAt(list->tail, list->count - 1, index)->value;
	return &mp4CursorListGetAt(list->head, 0, index)->value;
}

static Mp4ListNode<Mp4Track *> *mp4MovieTrackGetAt(Mp4ListNode<Mp4Track *> *node, uint32_t curIdx, uint32_t targetIdx)
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

static Mp4Track *mp4MovieTrackElem(const Mp4List<Mp4Track *> *list, uint32_t index)
{
	if (index >= list->count)
		return list->sentinel->value;
	if (index >= (list->count >> 1))
		return mp4MovieTrackGetAt(list->tail, list->count - 1, index)->value;
	return mp4MovieTrackGetAt(list->head, 0, index)->value;
}

static bool mp4CursorIsEof(const Mp4TrackCursor *cursor)
{
	if (cursor->track->fragmentTrack != NULL)
		return cursor->track->fragmentMgr->isEndOfFile();
	return cursor->currentSample >= cursor->track->sampleSizeTable->sampleCount;
}

static int64_t mp4CursorGetPts(const Mp4TrackCursor *cursor)
{
	if (cursor->track->fragmentTrack != NULL)
		return cursor->track->fragmentTrack->getCurrentPts();
	return cursor->track->timeToSampleTable->getSamplePts(cursor->currentSample);
}

static int64_t mp4CursorGetOffset(const Mp4TrackCursor *cursor)
{
	if (cursor->track->fragmentTrack != NULL)
		return cursor->track->fragmentTrack->getCurrentOffset();
	return cursor->track->sampleSizeTable->getSampleOffset(cursor->currentSample);
}

void mp4CursorSetEnabled(Mp4TrackCursor *cursor, bool enabled)
{
	uint8_t flag = enabled ? 1u : 0u;
	if (flag != cursor->enabled && flag != 0) {
		Mp4StreamDescription desc;
		cursor->track->getStreamDescription(&desc, 0, 0);
		cursor->track->selectStreamDescription(&desc);
	}
	cursor->enabled = flag;
	if (cursor->track->fragmentTrack != NULL)
		cursor->track->fragmentTrack->enabled = flag;
}

int mp4CursorAdvance(Mp4TrackCursor *cursor)
{
	if (cursor->track->fragmentTrack != NULL) {
		int err = cursor->track->fragmentTrack->nextSample();
		if (err == MP4_ERR_END_OF_FRAGMENT && cursor->track->fragmentMgr != NULL)
			return cursor->track->fragmentMgr->loadNextMoof();
		return err;
	}
	cursor->currentSample++;
	return cursor->currentSample;
}

int mp4CursorRewind(Mp4TrackCursor *cursor)
{
	if (cursor->track->fragmentTrack != NULL) {
		int err = cursor->track->fragmentTrack->prevSample();
		if (err == MP4_ERR_END_OF_FRAGMENT)
			return cursor->track->fragmentMgr->loadPrevMoof();
		return err;
	}
	cursor->currentSample--;
	return cursor->currentSample;
}

int mp4CursorReadSample(Mp4TrackCursor *cursor, Mp4Movie *movie, Mp4DemuxSample *outSample)
{
	if (outSample == NULL || movie == NULL)
		return MP4_ERR_INVALID_PARAM;

	outSample->size = 0;
	Mp4Track *trk = cursor->track;
	if (cursor->currentSample == 0) {
		Mp4SampleExtractor *ext = trk->getExtractor();
		if (ext != NULL)
			ext->reset();
	}

	int64_t sampleOffset;
	int32_t sampleSize;
	if (trk->fragmentTrack != NULL) {
		sampleOffset = trk->fragmentTrack->getCurrentOffset();
		sampleSize = trk->fragmentTrack->getCurrentSampleSize();
	} else {
		sampleOffset = trk->sampleSizeTable->getSampleOffset(cursor->currentSample);
		sampleSize = (int32_t)trk->sampleSizeTable->getSampleSize(cursor->currentSample);
	}

	int extractedSize = 0;
	Mp4SampleExtractor *ext = trk->getExtractor();
	int err = ext->extractSample(outSample, movie, sampleOffset, sampleSize, &extractedSize);
	if (err != MP4_OK)
		return err;

	outSample->duration = trk->timeToSampleTable->getSampleDuration(cursor->currentSample);
	int32_t prevSync = trk->syncSampleTable->findPrevSyncSample(cursor->currentSample);
	outSample->isKeyFrame = (prevSync == cursor->currentSample) ? 1 : 0;

	if (trk->fragmentTrack != NULL) {
		outSample->pts = trk->fragmentTrack->getCurrentPts();
		outSample->dts = trk->fragmentTrack->getCurrentDts();
	} else {
		outSample->pts = trk->timeToSampleTable->getSamplePts(cursor->currentSample);
		outSample->dts = trk->timeToSampleTable->getSampleDts(cursor->currentSample);
	}

	outSample->isVideo = trk->isVideo() ? 1u : 0u;
	mp4CursorAdvance(cursor);
	return MP4_OK;
}

int mp4CursorSkipFragment(Mp4TrackCursor *cursor, Mp4DemuxSample *outSample)
{
	if (cursor->track->fragmentTrack == NULL)
		return MP4_ERR_GENERIC;

	if (cursor->track->fragmentTrack->nextSample() == MP4_ERR_END_OF_FRAGMENT &&
	    cursor->track->fragmentMgr != NULL) {
		cursor->track->fragmentMgr->loadNextMoof();
	}

	if (mp4CursorIsEof(cursor))
		return MP4_ERR_EOF;

	outSample->trackIndex = 0xFFFF;
	outSample->capacity = 0;
	return MP4_OK;
}

int mp4CursorSeekFragment(Mp4TrackCursor *cursor, int32_t unused, uint64_t targetTicks, int64_t *outActual)
{
	if (cursor->track->fragmentTrack == NULL)
		return -1;
	return (cursor->track->fragmentMgr->seekToTime(unused, targetTicks, outActual) == 0) ? 0 : -1;
}

int Mp4Demuxer::openFile(const char *filename, SceAvPlayerFileReplacement *fileOps, SceAvPlayerMemAllocator *inAllocator, int32_t basePriority, int32_t inStreamFlags)
{
	const char *dot = sceClibStrrchr(filename, '.');
	this->streamFlags = inStreamFlags;
	this->allocator = inAllocator;

	if (dot != NULL) {
		if (sceClibStrncasecmp(dot, ".mp4", 4) == 0 ||
		    sceClibStrncasecmp(dot, ".m4v", 4) == 0 ||
		    sceClibStrncasecmp(dot, ".m4a", 4) == 0 ||
		    sceClibStrncasecmp(dot, ".m3d", 4) == 0 ||
		    sceClibStrncasecmp(dot, ".mov", 4) == 0) {
			if (sceClibStrncasecmp(dot, ".m3d", 4) == 0)
				this->isM3d = 1;
		} else if (sceClibStrncasecmp(dot, ".mpd", 4) == 0) {
			return MP4_ERR_OPEN_FAILED;
		}
	}

	Mp4FileStreamDataReader *rdr = new Mp4FileStreamDataReader(inStreamFlags);
	this->dataReader = rdr;

	if (rdr == NULL || rdr->streamReader == NULL)
		return MP4_ERR_NO_MEMORY;

	return rdr->streamReader->openAndStart(filename, 0, fileOps, basePriority);
}

int64_t Mp4Demuxer::parseMovie(int32_t *outTrackCount)
{
	int64_t fileLength = 0;
	uint32_t waitCount = 0;
	do {
		fileLength = this->dataReader->getLength();
		waitCount++;
		if (waitCount > 960) {
			if (this->dataReader != NULL) {
				delete this->dataReader;
				this->dataReader = NULL;
			}
			return -1;
		}
		if (fileLength <= 0)
			sceKernelDelayThread(900);
	} while (fileLength <= 0);

	Mp4Atom *rootAtom = new Mp4Atom(this->dataReader, 0, fileLength, 0, 0, this->allocator);
	if (rootAtom == NULL)
		return -1;

	Mp4Movie *mov = new Mp4Movie(rootAtom, this->allocator, this->streamFlags);
	this->movie = mov;
	if (mov == NULL)
		return -1;

	if (mov->tracks.count == 0) {
		if (this->dataReader != NULL) {
			delete this->dataReader;
			this->dataReader = NULL;
		}
		mov->~Mp4Movie();
		mp4MspaceFree(mov);
		this->movie = NULL;
		return -1;
	}

	for (uint32_t i = 0; i < mov->tracks.count; i++) {
		Mp4Track *trk = mp4MovieTrackElem(&mov->tracks, i);
		Mp4TrackCursor cursor;
		cursor.enabled = 0;
		cursor.track = trk;
		cursor.currentSample = 0;
		cursor.streamFlags = this->streamFlags;

		sceKernelLockLwMutex(&this->cursors.mutex, 1, NULL);
		Mp4ListNode<Mp4TrackCursor> *node = (Mp4ListNode<Mp4TrackCursor> *)mp4MspaceAllocZero(8, sizeof(Mp4ListNode<Mp4TrackCursor>));
		if (node != NULL) {
			node->prev = NULL;
			node->next = NULL;
			sceClibMemcpy_safe(&node->value, &cursor, sizeof(cursor));
		}
		if (this->cursors.tail != NULL) {
			this->cursors.tail->next = node;
			node->prev = this->cursors.tail;
		} else {
			this->cursors.head = node;
		}
		this->cursors.tail = node;
		this->cursors.count++;
		this->cursors.totalBytes += sizeof(Mp4ListNode<Mp4TrackCursor>);
		sceKernelUnlockLwMutex(&this->cursors.mutex, 1);
	}

	*outTrackCount = (int32_t)mov->tracks.count;
	return mov->movieDuration100us;
}

int Mp4Demuxer::closeMovie()
{
	int res = 0;

	while (this->cursors.count != 0) {
		sceKernelLockLwMutex(&this->cursors.mutex, 1, NULL);
		if (this->cursors.count != 0 && this->cursors.tail != NULL) {
			Mp4ListNode<Mp4TrackCursor> *tail = this->cursors.tail;
			Mp4ListNode<Mp4TrackCursor> *prev = mp4CursorListFindPrev(tail, tail);
			if (prev != NULL)
				prev->next = NULL;
			mp4MspaceFree(tail);
			this->cursors.tail = prev;
			this->cursors.count--;
			this->cursors.totalBytes -= sizeof(Mp4ListNode<Mp4TrackCursor>);
		}
		sceKernelUnlockLwMutex(&this->cursors.mutex, 1);
	}

	if (this->dataReader != NULL) {
		if (this->dataReader->streamReader == NULL)
			res = -1;
		delete this->dataReader;
		this->dataReader = NULL;
	}

	if (this->movie != NULL) {
		this->movie->~Mp4Movie();
		mp4MspaceFree(this->movie);
		this->movie = NULL;
	}
	return res;
}

int Mp4Demuxer::resetCursors()
{
	for (uint32_t i = 0; i < this->cursors.count; i++) {
		Mp4TrackCursor *cursor = mp4CursorListElem(&this->cursors, i);
		if (cursor->enabled != 0)
			cursor->currentSample = 0;
	}
	return 0;
}

int Mp4Demuxer::resumeStreaming()
{
	if (this->cursors.count != 0 && this->dataReader != NULL && this->dataReader->streamReader != NULL)
		return this->dataReader->streamReader->resumeStreaming();
	return 0;
}

int Mp4Demuxer::suspendStreaming()
{
	if (this->cursors.count != 0 && this->dataReader != NULL && this->dataReader->streamReader != NULL)
		return this->dataReader->streamReader->suspendStreaming();
	return 0;
}

int Mp4Demuxer::abortStreaming()
{
	if (this->dataReader == NULL)
		return 0;
	if (this->dataReader->streamReader == NULL)
		return -1;
	this->dataReader->streamReader->abortStreaming();
	return 0;
}

static int64_t mp4TrackSampleTimestamp(const Mp4Track *trk, uint32_t sampleIdx)
{
	if (trk->sampleDts90k == NULL)
		return 0;
	if (trk->isVideo() || trk->sttsEntryCount != 1)
		return trk->sampleDts90k[sampleIdx];
	return trk->sampleDts90k[0] * (int64_t)sampleIdx;
}

static uint32_t mp4TrackSampleFileOffset(const Mp4Track *trk, uint32_t sampleIdx)
{
	if (trk->sampleOffsets == NULL)
		return 0;
	return trk->sampleOffsets[sampleIdx];
}

void Mp4Demuxer::analyzeInterleave()
{
	Mp4Track *trk0 = mp4MovieTrackElem(&this->movie->tracks, 0);
	Mp4Track *trk1 = mp4MovieTrackElem(&this->movie->tracks, 1);
	uint32_t count0 = trk0->sttsTotalSamples;
	uint32_t count1 = trk1->sttsTotalSamples;

	uint32_t firstIndex = 0;
	uint32_t secondIndex = 0;
	int64_t maxGap = 0;

	while (firstIndex < count0 && secondIndex < count1) {
		int64_t ts0 = mp4TrackSampleTimestamp(trk0, firstIndex);
		int64_t ts1 = mp4TrackSampleTimestamp(trk1, secondIndex);
		int64_t diff = 0;

		if (ts0 == ts1) {
			uint32_t firstOffset = mp4TrackSampleFileOffset(trk0, firstIndex);
			uint32_t secondOffset = mp4TrackSampleFileOffset(trk1, secondIndex);
			firstIndex++;
			secondIndex++;
			diff = (int64_t)firstOffset - (int64_t)secondOffset;
		} else {
			bool advanceTrk1 = (ts0 >= ts1);
			Mp4Track *leadTrk = advanceTrk1 ? trk0 : trk1;
			Mp4Track *lagTrk = advanceTrk1 ? trk1 : trk0;
			uint32_t *leadIdx = advanceTrk1 ? &firstIndex : &secondIndex;
			uint32_t *lagIdx = advanceTrk1 ? &secondIndex : &firstIndex;
			uint32_t lagCount = advanceTrk1 ? count1 : count0;
			int64_t leadTs = advanceTrk1 ? ts0 : ts1;
			int64_t prevLagTs = advanceTrk1 ? ts1 : ts0;

			uint32_t leadOff = mp4TrackSampleFileOffset(leadTrk, *leadIdx);
			(*leadIdx)++;

			uint32_t scan = *lagIdx + 1;
			bool matched = false;
			uint32_t chosenLagOff = 0;
			for (uint32_t step = 1; step < 40 && scan < lagCount; step++, scan++) {
				int64_t curLagTs = mp4TrackSampleTimestamp(lagTrk, scan);
				if (curLagTs > leadTs) {
					uint32_t pickIdx = ((curLagTs - leadTs) >= (leadTs - prevLagTs)) ? (scan - 1) : scan;
					chosenLagOff = mp4TrackSampleFileOffset(lagTrk, pickIdx);
					matched = true;
					break;
				}
				prevLagTs = curLagTs;
			}
			*lagIdx = scan;
			diff = matched ? ((int64_t)chosenLagOff - (int64_t)leadOff) : 1572864LL;
		}

		if (diff < 0)
			diff = -diff;
		if (diff > maxGap)
			maxGap = diff;
	}

	int nonInterleaved = 0;
	if (this->dataReader != NULL && this->dataReader->streamReader != NULL)
		nonInterleaved = this->dataReader->streamReader->configureCacheLimit(maxGap);

	this->isNonInterleaved = (uint8_t)nonInterleaved;
	this->maxInterleaveGap = (nonInterleaved != 0) ? 0x40000LL : maxGap;
}

static void mp4PrepareMovieCacheAndSeekTables(Mp4Demuxer *demux)
{
	Mp4Movie *mov = demux->movie;
	uint32_t trackCount = mov->tracks.count;

	if (mov->fragmentMgr != NULL) {
		int flag = 0;
		if (demux->dataReader != NULL && demux->dataReader->streamReader != NULL)
			flag = demux->dataReader->streamReader->configureCacheLimit(-1);
		demux->isNonInterleaved = (uint8_t)flag;
		demux->maxInterleaveGap = 0x100000;
	} else if (trackCount == 1) {
		int flag = 0;
		if (demux->dataReader != NULL && demux->dataReader->streamReader != NULL)
			flag = demux->dataReader->streamReader->configureCacheLimit(0x100000);
		demux->isNonInterleaved = (uint8_t)flag;
		demux->maxInterleaveGap = 0x100000;
	} else if (trackCount == 2) {
		demux->analyzeInterleave();
	} else if (trackCount > 2) {
		int flag = 0;
		if (demux->dataReader != NULL && demux->dataReader->streamReader != NULL)
			flag = demux->dataReader->streamReader->configureCacheLimit(10485760);
		demux->isNonInterleaved = (uint8_t)flag;
		demux->maxInterleaveGap = 0x100000;
	}

	for (uint32_t i = 0; i < mov->tracks.count; i++) {
		Mp4Track *trk = mp4MovieTrackElem(&mov->tracks, i);
		trk->loadSeekTables();
	}

	for (uint32_t i = 0; i < mov->tracks.count; i++) {
		Mp4Track *trk = mp4MovieTrackElem(&mov->tracks, i);
		if (trk->sampleSizes != NULL) {
			uint8_t *raw = (uint8_t *)trk->sampleSizes - 1;
			if (raw[0] == MP4_TAG_MSPACE_DF)
				mp4MspaceFree(raw);
			else
				trk->allocator->deallocate(trk->allocator->objectPointer, raw);
			trk->sampleSizes = NULL;
		}
		if (trk->chunkOffsets != NULL) {
			uint8_t *raw = (uint8_t *)trk->chunkOffsets - 1;
			if (raw[0] == MP4_TAG_MSPACE_DF)
				mp4MspaceFree(raw);
			else
				trk->allocator->deallocate(trk->allocator->objectPointer, raw);
			trk->chunkOffsets = NULL;
		}
		if (trk->sampleOffsets != NULL) {
			uint8_t *raw = (uint8_t *)trk->sampleOffsets - 1;
			if (raw[0] == MP4_TAG_MSPACE_DF)
				mp4MspaceFree(raw);
			else
				trk->allocator->deallocate(trk->allocator->objectPointer, raw);
			trk->sampleOffsets = NULL;
		}
	}

	if (demux->maxInterleaveGap < 4096)
		demux->maxInterleaveGap = 0x100000;
}

int Mp4Demuxer::getExternalStreamInfo(uint32_t trackIndex, Mp4ExternalStreamInfo *outInfo)
{
	if ((int32_t)trackIndex < 0 || this->movie == NULL || trackIndex >= this->movie->tracks.count)
		return -1;

	mp4PrepareMovieCacheAndSeekTables(this);

	Mp4TrackCursor *cursor = mp4CursorListElem(&this->cursors, trackIndex);
	Mp4StreamDescription desc;
	if (cursor->track->getStreamDescription(&desc, 0, 0) == 0)
		return -1;

	outInfo->streamType = desc.streamType;
	outInfo->codecType = desc.codecType;
	if (desc.streamType != 0) {
		outInfo->audioSampleRate = desc.widthOrSampleRate;
		outInfo->audioBitsPerSample = desc.heightOrBitsPerSample;
		outInfo->audioBitrateMax = desc.profileOrBitrateMax;
		outInfo->audioBitrateAvg = desc.levelOrBitrateAvg;
		outInfo->audioBufferSize = desc.refFramesOrBufferSize;
		outInfo->audioChannels = desc.maxReorderOrChannels;
	} else {
		outInfo->videoWidth = desc.widthOrSampleRate;
		outInfo->videoHeight = desc.heightOrBitsPerSample;
		outInfo->videoProfile = desc.profileOrBitrateMax;
		outInfo->videoLevel = desc.levelOrBitrateAvg;
		outInfo->videoRefFrames = desc.refFramesOrBufferSize;
		outInfo->videoMaxReorder = desc.maxReorderOrChannels;
		outInfo->videoFrameDuration = desc.frameDuration;
		outInfo->videoAspectRatio = desc.aspectRatioFixed;
		outInfo->videoMaxBufferSize = (uint32_t)this->maxInterleaveGap;
		if (this->isM3d != 0)
			outInfo->videoLevel += 100;
	}

	uint8_t cfgLen = (uint8_t)desc.configSize;
	outInfo->configSize = cfgLen;
	sceClibMemcpy_safe(outInfo->configData, desc.configData, cfgLen);
	outInfo->languageCode[0] = desc.languageCode[0];
	outInfo->languageCode[1] = desc.languageCode[1];
	outInfo->languageCode[2] = desc.languageCode[2];
	outInfo->languageCode[3] = desc.languageCode[3];
	return 0;
}

int Mp4Demuxer::getStreamDescription(int32_t trackIndex, Mp4StreamDescription *outDesc)
{
	if (trackIndex < 0 || this->movie == NULL || (uint32_t)trackIndex >= this->movie->tracks.count)
		return -1;

	mp4PrepareMovieCacheAndSeekTables(this);

	Mp4TrackCursor *cursor = mp4CursorListElem(&this->cursors, (uint32_t)trackIndex);
	if (cursor->track->getStreamDescription(outDesc, 0, 0) == 0)
		return -1;

	if (outDesc->streamType == 0) {
		outDesc->maxBufferSize = (uint32_t)this->maxInterleaveGap;
		if (this->isM3d != 0)
			outDesc->levelOrBitrateAvg += 100;
	}
	return 0;
}

int Mp4Demuxer::setTrackEnabled(int32_t trackIndex, bool enabled)
{
	if (trackIndex < 0 || (uint32_t)trackIndex >= this->cursors.count)
		return -1;

	Mp4TrackCursor *cursor = mp4CursorListElem(&this->cursors, (uint32_t)trackIndex);
	mp4CursorSetEnabled(cursor, enabled);
	return 0;
}

int Mp4Demuxer::findEnabledVideoTrack()
{
	int found = -1;
	for (uint32_t i = 0; i < this->cursors.count; i++) {
		Mp4TrackCursor *cursor = mp4CursorListElem(&this->cursors, i);
		if (cursor->enabled != 0 && cursor->track->isVideo())
			found = (int)i;
	}
	return found;
}

int Mp4Demuxer::findEnabledNonVideoTrack()
{
	int found = -1;
	for (uint32_t i = 0; i < this->cursors.count; i++) {
		Mp4TrackCursor *cursor = mp4CursorListElem(&this->cursors, i);
		if (cursor->enabled != 0 && !mp4CursorIsEof(cursor) && !cursor->track->isVideo())
			found = (int)i;
	}
	return found;
}

int Mp4Demuxer::seekToTime(int32_t unused, int64_t targetTicks90k)
{
	if (this->cursors.count == 0)
		return -1;

	int32_t primaryIdx = this->findEnabledVideoTrack();
	if (primaryIdx == -1) {
		for (uint32_t i = 0; i < this->cursors.count; i++) {
			Mp4TrackCursor *cursor = mp4CursorListElem(&this->cursors, i);
			if (cursor->enabled != 0 && !mp4CursorIsEof(cursor)) {
				primaryIdx = (int32_t)i;
				break;
			}
		}
	} else {
		Mp4TrackCursor *primary = mp4CursorListElem(&this->cursors, (uint32_t)primaryIdx);
		if (mp4CursorIsEof(primary)) {
			for (uint32_t i = 0; i < this->cursors.count; i++) {
				Mp4TrackCursor *cursor = mp4CursorListElem(&this->cursors, i);
				if (cursor->enabled != 0)
					cursor->currentSample = 0;
			}
		}
	}

	if (primaryIdx < 0)
		return -3;

	int64_t actualTs = 0;
	Mp4TrackCursor *primary = mp4CursorListElem(&this->cursors, (uint32_t)primaryIdx);

	if (this->movie->fragmentMgr != NULL) {
		if (primary->track->fragmentTrack == NULL)
			return -1;
		return (primary->track->fragmentMgr->seekToTime(unused, (uint64_t)targetTicks90k, &actualTs) == 0) ? 0 : -1;
	}

	int64_t curPts = mp4CursorGetPts(primary);
	int forward = (curPts <= targetTicks90k) ? 1 : 0;
	int32_t foundSample = 0;
	if (primary->track->findSampleForSeek(forward, (uint64_t)targetTicks90k, &foundSample, &actualTs) == 0)
		return -2;

	if (foundSample == 0) {
		while (primary->currentSample != 0)
			mp4CursorRewind(primary);
		for (uint32_t i = 0; i < this->cursors.count; i++) {
			if ((int32_t)i == primaryIdx)
				continue;
			Mp4TrackCursor *other = mp4CursorListElem(&this->cursors, i);
			while (other->currentSample != 0)
				mp4CursorRewind(other);
		}
		return 0;
	}

	primary->currentSample = foundSample;
	if (primary->track->fragmentTrack != NULL)
		primary->track->fragmentTrack->isAtFirstSample();
	else
		primary->track->syncSampleTable->findPrevSyncSample(primary->currentSample);

	Mp4SampleExtractor *ext = primary->track->getExtractor();
	if (ext != NULL)
		ext->reset();

	actualTs = mp4CursorGetPts(primary);
	for (uint32_t i = 0; i < this->cursors.count; i++) {
		if ((int32_t)i == primaryIdx)
			continue;
		Mp4TrackCursor *other = mp4CursorListElem(&this->cursors, i);
		int64_t otherPts = mp4CursorGetPts(other);
		if (actualTs > otherPts || actualTs < otherPts) {
			int32_t alignedSample = other->track->timeToSampleTable->findSampleByTimestamp(actualTs);
			if (alignedSample >= 0)
				other->currentSample = alignedSample;
		}
	}

	int64_t minOff = -1;
	for (uint32_t i = 0; i < this->cursors.count; i++) {
		Mp4TrackCursor *cursor = mp4CursorListElem(&this->cursors, i);
		int64_t off = mp4CursorGetOffset(cursor);
		if (minOff < 0 || off < minOff)
			minOff = off;
	}
	return 0;
}

int Mp4Demuxer::stepKeyFrameForward(Mp4DemuxSample *outSample)
{
	if (this->cursors.count == 0)
		return -1;

	int32_t videoIdx = this->findEnabledVideoTrack();
	if (videoIdx == -1)
		return -1;

	Mp4TrackCursor *videoCursor = mp4CursorListElem(&this->cursors, (uint32_t)videoIdx);
	uint32_t nextKey = 0;
	int64_t keyTs = 0;
	if (!videoCursor->track->findKeyFrame(true, (uint32_t)videoCursor->currentSample, &nextKey, &keyTs))
		return 1;

	videoCursor->currentSample = (int32_t)nextKey;
	for (uint32_t i = 0; i < this->cursors.count; i++) {
		if ((int32_t)i == videoIdx)
			continue;
		Mp4TrackCursor *other = mp4CursorListElem(&this->cursors, i);
		int32_t aligned = other->track->timeToSampleTable->findSampleByTimestamp(keyTs);
		if (aligned >= 0)
			other->currentSample = aligned;
	}

	if (videoCursor->track->fragmentTrack != NULL)
		videoCursor->track->fragmentTrack->isAtFirstSample();
	else
		videoCursor->track->syncSampleTable->findPrevSyncSample(videoCursor->currentSample);

	outSample->trackIndex = videoIdx;
	if (mp4CursorReadSample(videoCursor, this->movie, outSample) != 0)
		return -1;

	this->lastSample = outSample;
	outSample->isKeyFrame = 1;
	mp4CursorRewind(videoCursor);
	return 0;
}

int Mp4Demuxer::stepKeyFrameBackward(Mp4DemuxSample *outSample)
{
	if (this->cursors.count == 0)
		return -1;

	int32_t videoIdx = this->findEnabledVideoTrack();
	if (videoIdx == -1)
		return -1;

	Mp4TrackCursor *videoCursor = mp4CursorListElem(&this->cursors, (uint32_t)videoIdx);
	uint32_t prevKey = 0;
	int64_t keyTs = 0;
	if (!videoCursor->track->findKeyFrame(false, (uint32_t)videoCursor->currentSample, &prevKey, &keyTs))
		return 1;

	videoCursor->currentSample = (int32_t)prevKey;
	for (uint32_t i = 0; i < this->cursors.count; i++) {
		if ((int32_t)i == videoIdx)
			continue;
		Mp4TrackCursor *other = mp4CursorListElem(&this->cursors, i);
		int32_t aligned = other->track->timeToSampleTable->findSampleByTimestamp(keyTs);
		if (aligned >= 0)
			other->currentSample = aligned;
	}

	outSample->trackIndex = videoIdx;
	if (mp4CursorReadSample(videoCursor, this->movie, outSample) != 0)
		return -1;

	this->lastSample = outSample;
	outSample->isKeyFrame = 1;
	mp4CursorRewind(videoCursor);
	return 0;
}

int Mp4Demuxer::readNextSample(Mp4DemuxSample *outSample)
{
	if (this->cursors.count == 0)
		return MP4_ERR_INVALID_PARAM;

	int32_t chosenIdx = -1;
	int64_t bestKey = 0;

	if (this->isNonInterleaved != 0) {
		for (uint32_t i = 0; i < this->cursors.count; i++) {
			Mp4TrackCursor *cursor = mp4CursorListElem(&this->cursors, i);
			if (cursor->enabled == 0 || mp4CursorIsEof(cursor))
				continue;
			int64_t pts = mp4CursorGetPts(cursor);
			if (chosenIdx < 0 || pts < bestKey) {
				chosenIdx = (int32_t)i;
				bestKey = pts;
			}
			if (bestKey == 0x7FFFFFFFFFFFFFFFLL)
				return mp4CursorSkipFragment(mp4CursorListElem(&this->cursors, (uint32_t)chosenIdx), outSample);
		}
	} else {
		for (uint32_t i = 0; i < this->cursors.count; i++) {
			Mp4TrackCursor *cursor = mp4CursorListElem(&this->cursors, i);
			if (cursor->enabled == 0 || mp4CursorIsEof(cursor))
				continue;
			int64_t off = mp4CursorGetOffset(cursor);
			if (chosenIdx < 0 || off < bestKey) {
				chosenIdx = (int32_t)i;
				bestKey = off;
			}
		}
	}

	if (chosenIdx < 0)
		return MP4_ERR_EOF;

	outSample->trackIndex = chosenIdx;
	Mp4TrackCursor *chosen = mp4CursorListElem(&this->cursors, (uint32_t)chosenIdx);
	int err = mp4CursorReadSample(chosen, this->movie, outSample);
	if (err == MP4_ERR_BUFFER_UNDERRUN) {
		outSample->trackIndex = 4095;
		return MP4_OK;
	}
	if (err == MP4_OK) {
		this->lastSample = outSample;
		chosen->track->isTimedText();
	}
	return err;
}

int Mp4Demuxer::copyLastSampleData(void *dst)
{
	if (this->lastSample == NULL || this->lastSample->data == NULL || this->lastSample->size == 0)
		return -1;
	sceClibMemcpy_safe(dst, this->lastSample->data, (SceSize)this->lastSample->size);
	return 0;
}

int Mp4Demuxer::readTrackSample(uint32_t trackIndex, Mp4DemuxSample *outSample)
{
	if ((int32_t)trackIndex > (int32_t)this->cursors.count)
		return -1;

	Mp4TrackCursor *cursor = mp4CursorListElem(&this->cursors, trackIndex);
	if (cursor->track == NULL || cursor->track->getExtractor() == NULL)
		return -1;

	outSample->trackIndex = (int32_t)trackIndex;
	int err = mp4CursorReadSample(cursor, this->movie, outSample);
	if (err >= 0)
		this->lastSample = outSample;
	return err;
}

static void mp4DestroyDemuxerObject(Mp4Demuxer *demux)
{
	demux->closeMovie();
	if (demux->cursors.isShallowCopy == 0) {
		if (demux->cursors.beginIter != NULL)
			delete demux->cursors.beginIter;
		if (demux->cursors.endIter != NULL)
			delete demux->cursors.endIter;
		if (demux->cursors.sentinel != NULL)
			mp4MspaceFree(demux->cursors.sentinel);
	}
	sceKernelDeleteLwMutex(&demux->cursors.mutex);
	mp4MspaceFree(demux);
}

/* =========================================================================
 * Exported SceMp4 API (21 functions)
 * ========================================================================= */
extern "C" {

Mp4Demuxer *SceMp4_05474AF0(const char *filename, SceAvPlayerFileReplacement *fileOps, SceAvPlayerMemAllocator *allocator,
                           void *memblock, SceSize size, int32_t basePriority, int32_t debugLevel)
{
	SceKernelLwMutexOptParam opt;
	Mp4TrackCursor zeroCursor;

	g_mp4DebugLevel = debugLevel;
	if (mp4MspaceInit(size, memblock) < 0)
		return NULL;

	sceClibMemset(&g_mp4DemuxMutex, 0, sizeof(g_mp4DemuxMutex));

	Mp4Demuxer *demux = (Mp4Demuxer *)mp4MspaceAllocZero(1, sizeof(Mp4Demuxer));
	if (demux == NULL)
		return NULL;

	sceClibMemset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	sceKernelCreateLwMutex(&demux->cursors.mutex, "0Demux_Stl_Handler_Mutex ", 0, 0, &opt);

	sceClibMemset(&zeroCursor, 0, sizeof(zeroCursor));
	Mp4ListNode<Mp4TrackCursor> *sentinel = (Mp4ListNode<Mp4TrackCursor> *)mp4MspaceAllocZero(8, sizeof(Mp4ListNode<Mp4TrackCursor>));
	if (sentinel != NULL) {
		sentinel->prev = NULL;
		sentinel->next = NULL;
		sceClibMemcpy_safe(&sentinel->value, &zeroCursor, sizeof(zeroCursor));
	}
	demux->cursors.sentinel = sentinel;
	demux->maxInterleaveGap = 0x100000;
	demux->defaultBufferSize = 0x100000;
	demux->streamFlags = 1;
	demux->isNonInterleaved = 0;

	if (demux->openFile(filename, fileOps, allocator, basePriority, 1) == 0) {
		sceClibMemset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		sceKernelCreateLwMutex(&g_mp4DemuxMutex, "Mp4Demux Critical ", 0, 0, &opt);
		return demux;
	}

	mp4DestroyDemuxerObject(demux);
	return NULL;
}

int SceMp4_30E49E4D(Mp4Demuxer *demux, int32_t *outTrackCount, int64_t *outDuration100us)
{
	sceKernelLockLwMutex(&g_mp4DemuxMutex, 1, NULL);
	*outDuration100us = demux->parseMovie(outTrackCount);
	sceKernelUnlockLwMutex(&g_mp4DemuxMutex, 1);
	return (*outDuration100us >= 0) ? 0 : -1;
}

int SceMp4_920623C8(Mp4Demuxer *demux)
{
	sceKernelLockLwMutex(&g_mp4DemuxMutex, 1, NULL);
	int res = demux->closeMovie();
	if (demux != NULL)
		mp4DestroyDemuxerObject(demux);
	sceKernelUnlockLwMutex(&g_mp4DemuxMutex, 1);
	sceKernelDeleteLwMutex(&g_mp4DemuxMutex);
	sceClibMemset(&g_mp4DemuxMutex, 0, sizeof(g_mp4DemuxMutex));
	mp4MspaceDestroy();
	return res;
}

int SceMp4_609E57AD(Mp4Demuxer *demux, int32_t trackIndex, uint8_t enabled)
{
	sceKernelLockLwMutex(&g_mp4DemuxMutex, 1, NULL);
	int res = demux->setTrackEnabled(trackIndex, enabled != 0);
	sceKernelUnlockLwMutex(&g_mp4DemuxMutex, 1);
	return res;
}

int SceMp4_D5B26179(Mp4Demuxer *demux, int32_t trackIndex, Mp4StreamDescription *outDesc)
{
	return demux->getStreamDescription(trackIndex, outDesc);
}

int SceMp4_8BE0E3D3(Mp4Demuxer *demux, uint32_t trackIndex, Mp4ExternalStreamInfo *outInfo)
{
	return demux->getExternalStreamInfo(trackIndex, outInfo);
}

int SceMp4_20A01112(Mp4Demuxer *demux, Mp4DemuxSample *outSample)
{
	sceKernelLockLwMutex(&g_mp4DemuxMutex, 1, NULL);
	int err = demux->readNextSample(outSample);
	sceKernelUnlockLwMutex(&g_mp4DemuxMutex, 1);
	if (err == MP4_OK)
		return 0;
	if (err == MP4_ERR_EOF || err == MP4_ERR_END_OF_STREAM)
		return 1;
	return -1;
}

int SceMp4_32A15788(Mp4Demuxer *demux, Mp4DemuxSample *outSample)
{
	return demux->stepKeyFrameForward(outSample);
}

int SceMp4_40C64DC0(Mp4Demuxer *demux, Mp4DemuxSample *outSample)
{
	return demux->stepKeyFrameBackward(outSample);
}

int SceMp4_96676BA0(Mp4Demuxer *demux, int32_t unused, int64_t targetTicks90k)
{
	sceKernelLockLwMutex(&g_mp4DemuxMutex, 1, NULL);
	int res = demux->seekToTime(unused, targetTicks90k);
	sceKernelUnlockLwMutex(&g_mp4DemuxMutex, 1);
	return res;
}

int SceMp4_D092A066(Mp4Demuxer *demux, void *dst)
{
	return demux->copyLastSampleData(dst);
}

int SceMp4_40351E1A(Mp4Demuxer *demux)
{
	sceKernelLockLwMutex(&g_mp4DemuxMutex, 1, NULL);
	int res = demux->resetCursors();
	sceKernelUnlockLwMutex(&g_mp4DemuxMutex, 1);
	return res;
}

int SceMp4_7B4832FE(Mp4Demuxer *demux, uint32_t *outStats)
{
	(void)demux;
	outStats[0] = 0;
	outStats[1] = 0;
	outStats[2] = 0;
	return 0;
}

uint64_t SceMp4_A7C09746(uint64_t ticks90k)
{
	return ticks90k / 90000ULL;
}

uint64_t SceMp4_6B69F900(uint64_t time100us)
{
	return (time100us * 90000ULL) / 10000ULL;
}

int SceMp4_94E1305D(void)
{
	return -1;
}

int SceMp4_EFE299C9(void)
{
	return -1;
}

int SceMp4_A870587F(void)
{
	return -1;
}

int SceMp4_CB91CEBC(Mp4Demuxer *demux)
{
	return demux->resumeStreaming();
}

int SceMp4_D2871F1C(Mp4Demuxer *demux)
{
	return demux->suspendStreaming();
}

int SceMp4_C05DFF01(Mp4Demuxer *demux)
{
	if (demux != NULL)
		return demux->abortStreaming();
	return 0;
}

} /* extern "C" */
