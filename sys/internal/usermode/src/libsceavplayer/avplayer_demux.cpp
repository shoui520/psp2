#include "avplayer_internal.h"

void avLogError(int32_t level, const char *funcName, uint32_t line, const char *fmt, ...)
{
	if (level == SCE_AVPLAYER_DBG_WARNINGS || level == SCE_AVPLAYER_DBG_ALL) {
		char buffer[256];
		va_list args;
		va_start(args, fmt);
		int32_t prefixLen = sceClibSnprintf(buffer, sizeof(buffer), "E/ %s [%u]: ", funcName, line);
		if (prefixLen < 255)
			sceClibVsnprintf(&buffer[prefixLen], sizeof(buffer) - (SceSize)prefixLen, fmt, args);
		va_end(args);
		sceClibPrintf("%s\n", buffer);
	}
}

static void avDemuxThreadEntry(void *context)
{
	AvCoreDemux *demux = (AvCoreDemux *)context;
	if (demux != NULL)
		demux->demuxThread();
}

int32_t AvCoreDemux::init(AvStateMachine *sm, AvController *ctrl, int32_t priority, int32_t dbgLevel)
{
	stopFlag = 0;
	eventPendingFlag = 0;
	threadId = -1;
	sourceFilter = NULL;
	debugLevel = dbgLevel;
	basePriority = (priority != 0) ? priority : 140;
	stateMachine = sm;
	controller = ctrl;
	playbackMode = AV_PLAYMODE_NORMAL;
	sourceStatus = 0;
	eofReached = 0;
	loopEnabled = 0;

	eventQueue.init();
	auFifoList.init();
	trackEnableList.init();
	streamDescList.init();
	return 0;
}

int32_t AvCoreDemux::checkSource(const char *path, SceAvPlayerFileReplacement *fileOps, int32_t isRetry)
{
	(void)isRetry;
	if (path == NULL)
		return -1;

	const char *ext = sceClibStrrchr(path, '.');
	if (ext != NULL &&
	    sceClibStrncasecmp(ext, ".mp4", 4) != 0 &&
	    sceClibStrncasecmp(ext, ".m4v", 4) != 0 &&
	    sceClibStrncasecmp(ext, ".m4a", 4) != 0) {
		avLogWarn(debugLevel, "CheckSource", 164, "Non-standard file extension: %s", ext);
	}

	if (sourceFilter == NULL) {
		if (sceSysmoduleIsLoaded(SCE_SYSMODULE_MP4) != 0) {
			if (sceSysmoduleLoadModule(SCE_SYSMODULE_MP4) < 0) {
				avLogError(debugLevel, "CheckSource", 172, "Failed to load SceMp4 module");
				return -1;
			}
			g_avMp4Preloaded = 0;
		} else {
			g_avMp4Preloaded = 1;
		}

		sourceFilter = new AvMp4SourceFilter();
		if (sourceFilter == NULL) {
			avLogError(debugLevel, "CheckSource", 180, "Failed to allocate AvMp4SourceFilter");
			return -1;
		}
	}

	if (sourceFilter->init(path, fileOps, debugLevel) < 0) {
		avLogError(debugLevel, "CheckSource", 188, "Failed to init source filter");
		return -1;
	}

	if (sourceFilter->open() < 0) {
		avLogError(debugLevel, "CheckSource", 194, "Failed to open source");
		return -1;
	}

	int32_t trackCount = sourceFilter->getStreamCount();
	if (trackCount <= 0) {
		avLogError(debugLevel, "CheckSource", 202, "No valid streams found in source");
		return -1;
	}

	for (int32_t i = 0; i < trackCount; i++) {
		AvStreamDescriptor *desc = (AvStreamDescriptor *)avMspaceAlloc(8, sizeof(AvStreamDescriptor));
		if (desc == NULL)
			return -1;
		if (sourceFilter->getStreamInfo((uint32_t)i, desc) < 0) {
			avMspaceFree(desc);
			continue;
		}
		streamDescList.pushBack(desc);
		AvAuFifo *nullFifo = NULL;
		auFifoList.pushBack(nullFifo);
		int32_t disabled = 0;
		trackEnableList.pushBack(disabled);
	}

	sourceStatus = 1;
	if (controller != NULL)
		controller->changeFlowState(AV_STATE_READY);
	return 0;
}

int32_t AvCoreDemux::start()
{
	avAtomicStore32(&stopFlag, 0);
	eofReached = 0;

	threadId = sceKernelCreateThread("avPlayer Demux",
	                                 (SceKernelThreadEntry)avThreadTrampoline,
	                                 basePriority + 7,
	                                 0x4000u,
	                                 0,
	                                 0,
	                                 NULL);
	if (threadId < 0) {
		avLogError(debugLevel, "Start", 242, "Failed to create Demux thread");
		return -1;
	}

	AvThreadStartArg startArg;
	startArg.entry = avDemuxThreadEntry;
	startArg.context = this;
	if (sceKernelStartThread(threadId, sizeof(startArg), &startArg) < 0) {
		sceKernelDeleteThread(threadId);
		threadId = -1;
		return -1;
	}
	return 0;
}

int32_t AvCoreDemux::addNewEvent(int32_t eventType, int64_t payload)
{
	AvEvent ev;
	ev.type = eventType;
	ev.reserved = 0;
	ev.payload = payload;
	eventQueue.pushBack(ev);
	avAtomicStore32(&eventPendingFlag, 1);
	return 0;
}

int32_t AvCoreDemux::processEvent()
{
	if (eventQueue.count == 0) {
		avAtomicStore32(&eventPendingFlag, 0);
		return 0;
	}

	AvEvent ev = eventQueue.at(0);
	eventQueue.eraseAt(0);
	if (eventQueue.count == 0)
		avAtomicStore32(&eventPendingFlag, 0);

	if (ev.type == AV_EVENT_JUMP) {
		flushAllAUs();
		int32_t seekResult = 0;
		uint64_t timeMs = (uint64_t)ev.payload / 90u;
		for (uint32_t i = 0; i < streamDescList.count; i++) {
			if (trackEnableList.at(i) != 0 && sourceFilter != NULL) {
				if (sourceFilter->seekToTime(i, timeMs, (uint32_t)playbackMode) < 0)
					seekResult = -1;
			}
		}
		eofReached = 0;
		if (stateMachine != NULL)
			stateMachine->postAckEvent(AV_SENDER_DEMUX, AV_EVENT_JUMP, ev.payload, seekResult);
		return seekResult;
	}

	if (ev.type == AV_EVENT_TRICKMODE) {
		playbackMode = (int32_t)ev.payload;
		if (stateMachine != NULL)
			stateMachine->postAckEvent(AV_SENDER_DEMUX, AV_EVENT_TRICKMODE, ev.payload, 0);
		return 0;
	}

	return 0;
}

void AvCoreDemux::demuxThread()
{
	avLogDebug(debugLevel, "DemuxThread", 310, "Demux thread started");

	while (stopFlag == 0) {
		if (eventPendingFlag != 0 || eventQueue.count > 0) {
			processEvent();
			continue;
		}

		if (sourceFilter == NULL || eofReached != 0) {
			sceKernelDelayThread(5000);
			continue;
		}

		int32_t anyActive = 0;
		int32_t allEof = 1;

		for (uint32_t i = 0; i < auFifoList.count && stopFlag == 0; i++) {
			if (trackEnableList.at(i) == 0)
				continue;
			AvAuFifo *fifo = auFifoList.at(i);
			if (fifo == NULL)
				continue;

			anyActive = 1;
			if (fifo->isBufferFull()) {
				allEof = 0;
				continue;
			}

			AvAccessUnit *au = (AvAccessUnit *)avMspaceAlloc(8, sizeof(AvAccessUnit));
			if (au == NULL) {
				allEof = 0;
				continue;
			}
			au->trackIndex = i;

			int32_t ret = sourceFilter->readAccessUnit(i, au);
			if (ret < 0) {
				avMspaceFree(au);
				continue;
			}

			if (au->isEof != 0) {
				avMspaceFree(au);
				continue;
			}

			allEof = 0;
			if (au->size > 0 && fifo->allocateAu(au, au->size) == 0) {
				fifo->pushAu(au);
			} else {
				avMspaceFree(au);
			}
		}

		if (anyActive && allEof) {
			if (controller != NULL && controller->loopingEnabled != 0) {
				for (uint32_t i = 0; i < streamDescList.count; i++) {
					if (trackEnableList.at(i) != 0 && sourceFilter != NULL)
						sourceFilter->seekToTime(i, 0, AV_PLAYMODE_NORMAL);
				}
			} else {
				eofReached = 1;
				sourceStatus = 5;
				if (controller != NULL)
					controller->changeFlowState(AV_STATE_EOF);
			}
		} else {
			sceKernelDelayThread(2000);
		}
	}

	avLogDebug(debugLevel, "DemuxThread", 390, "Demux thread exiting");
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
}

int32_t AvCoreDemux::jump(int64_t targetPts)
{
	return addNewEvent(AV_EVENT_JUMP, targetPts);
}

int32_t AvCoreDemux::stop()
{
	if (stopFlag == 1)
		return 0;

	avAtomicStore32(&stopFlag, 1);
	for (uint32_t i = 0; i < auFifoList.count; i++) {
		AvAuFifo *fifo = auFifoList.at(i);
		if (fifo != NULL)
			fifo->signalAbort();
	}

	if (threadId >= 0) {
		sceKernelWaitThreadEnd(threadId, NULL, NULL);
		sceKernelDeleteThread(threadId);
		threadId = -1;
	}

	eventQueue.clear();
	for (uint32_t i = 0; i < auFifoList.count; i++) {
		AvAuFifo *fifo = auFifoList.at(i);
		if (fifo != NULL)
			fifo->clearAbort();
	}
	return 0;
}

int32_t AvCoreDemux::shutdown()
{
	stop();
	if (sourceFilter != NULL) {
		sourceFilter->shutdown();
		delete sourceFilter;
		sourceFilter = NULL;
	}
	return 0;
}

int32_t AvCoreDemux::setStreamFifo(uint32_t trackIndex, AvAuFifo *fifo)
{
	if (trackIndex >= auFifoList.count)
		return -1;
	auFifoList.at(trackIndex) = fifo;
	trackEnableList.at(trackIndex) = (fifo != NULL) ? 1 : 0;
	if (sourceFilter != NULL)
		sourceFilter->enableStream(trackIndex, (fifo != NULL) ? 1 : 0);
	return 0;
}

int32_t AvCoreDemux::disableStream(uint32_t trackIndex)
{
	if (trackIndex >= trackEnableList.count)
		return -1;
	trackEnableList.at(trackIndex) = 0;
	if (sourceFilter != NULL)
		sourceFilter->enableStream(trackIndex, 0);
	return 0;
}

int32_t AvCoreDemux::setPlaybackMode(int32_t mode)
{
	return addNewEvent(AV_EVENT_TRICKMODE, (int64_t)mode);
}

int32_t AvCoreDemux::getTrackOffset(uint32_t trackIndex)
{
	uint32_t offset = 0;
	if (sourceFilter != NULL && sourceFilter->getTrackOffset(trackIndex, &offset) >= 0)
		return (int32_t)offset;
	return 0;
}

int32_t AvCoreDemux::getSourceType()
{
	if (sourceFilter == NULL)
		return -1;
	return sourceFilter->getSourceType();
}

int32_t AvCoreDemux::discardAu(uint32_t trackIndex)
{
	if (trackIndex >= auFifoList.count)
		return -1;
	AvAuFifo *fifo = auFifoList.at(trackIndex);
	if (fifo == NULL)
		return -1;
	AvAccessUnit *au = fifo->peekAu();
	if (au != NULL) {
		fifo->popAndFreeAu(au);
		avMspaceFree(au);
	}
	return 0;
}

int32_t AvCoreDemux::flushAllAUs()
{
	for (uint32_t i = 0; i < auFifoList.count; i++) {
		AvAuFifo *fifo = auFifoList.at(i);
		if (fifo != NULL)
			fifo->flush(avMspaceFree);
	}
	return 0;
}
