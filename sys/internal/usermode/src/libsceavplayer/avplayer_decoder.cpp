#include "avplayer_internal.h"

void avLogWarn(int32_t level, const char *funcName, uint32_t line, const char *fmt, ...)
{
	if (level == SCE_AVPLAYER_DBG_WARNINGS || level == SCE_AVPLAYER_DBG_ALL) {
		char buffer[256];
		va_list args;
		va_start(args, fmt);
		int32_t prefixLen = sceClibSnprintf(buffer, sizeof(buffer), "W/ %s [%u]: ", funcName, line);
		if (prefixLen < 255)
			sceClibVsnprintf(&buffer[prefixLen], sizeof(buffer) - (SceSize)prefixLen, fmt, args);
		va_end(args);
		sceClibPrintf("%s\n", buffer);
	}
}

static void avDecodeThreadEntry(void *context)
{
	AvCoreDecoder *core = (AvCoreDecoder *)context;
	if (core != NULL)
		core->decodeThread();
}

void AvCoreDecoder::initCore()
{
	stopFlag = 0;
	eventPendingFlag = 0;
	decodeMode = 0;
	reservedAlign = 0;
	discardUntilPts = 0xFFFFFFFFFFFFFFFFULL;
	inputFifo = NULL;
	outputFifo = NULL;
	threadId = -1;
	playbackMode = AV_PLAYMODE_NORMAL;
	lockMutex = sceKernelCreateMutex("CoreDecoder", 0x2000u, 0, NULL);
	if (lockMutex < 0)
		lockMutex = 0;
	stateMachine = NULL;
	decoder = NULL;
	debugLevel = SCE_AVPLAYER_DBG_WARNINGS;
	eventQueue.init();
	highFrameRateFlag = 0;
}

void AvCoreDecoder::setInputAndDecoder(AvAuFifo *fifo, AvDecoder *dec)
{
	inputFifo = fifo;
	decoder = dec;
}

int32_t AvCoreDecoder::initDecoder(AvStreamDescriptor *desc, SceAvPlayerMemAllocator *alloc, int32_t dbgLevel)
{
	debugLevel = dbgLevel;
	if (decoder == NULL)
		return -1;
	return decoder->init(desc, alloc, dbgLevel);
}

int32_t AvCoreDecoder::startVideo(int32_t priority, const char *threadName, uint8_t highFps)
{
	avAtomicStore32(&stopFlag, 0);
	highFrameRateFlag = highFps;

	threadId = sceKernelCreateThread(threadName != NULL ? threadName : "avPlayer VideoDec",
	                                 (SceKernelThreadEntry)avThreadTrampoline,
	                                 priority,
	                                 0x4000u,
	                                 0,
	                                 0,
	                                 NULL);
	if (threadId < 0)
		return -1;

	AvThreadStartArg startArg;
	startArg.entry = avDecodeThreadEntry;
	startArg.context = this;
	if (sceKernelStartThread(threadId, sizeof(startArg), &startArg) < 0) {
		sceKernelDeleteThread(threadId);
		threadId = -1;
		return -1;
	}
	return 0;
}

int32_t AvCoreDecoder::startAudio(int32_t priority, const char *threadName)
{
	avAtomicStore32(&stopFlag, 0);

	threadId = sceKernelCreateThread(threadName != NULL ? threadName : "avPlayer AudioDec",
	                                 (SceKernelThreadEntry)avThreadTrampoline,
	                                 priority,
	                                 0x4000u,
	                                 0,
	                                 0,
	                                 NULL);
	if (threadId < 0)
		return -1;

	AvThreadStartArg startArg;
	startArg.entry = avDecodeThreadEntry;
	startArg.context = this;
	if (sceKernelStartThread(threadId, sizeof(startArg), &startArg) < 0) {
		sceKernelDeleteThread(threadId);
		threadId = -1;
		return -1;
	}
	return 0;
}

int32_t AvCoreDecoder::addNewEvent(int32_t eventType, int64_t payload)
{
	AvEvent ev;
	ev.type = eventType;
	ev.reserved = 0;
	ev.payload = payload;
	eventQueue.pushBack(ev);
	return 0;
}

int32_t AvCoreDecoder::processEvent()
{
	if (eventQueue.count == 0) {
		avAtomicStore32(&eventPendingFlag, 0);
		return 0;
	}

	AvEvent ev = eventQueue.at(0);
	eventQueue.eraseAt(0);
	if (eventQueue.count == 0)
		avAtomicStore32(&eventPendingFlag, 0);

	if (ev.type == AV_EVENT_RESET || ev.type == AV_EVENT_JUMP) {
		if (decoder != NULL)
			decoder->reset();
		if (outputFifo != NULL) {
			while (outputFifo->getReadyCount() > 0) {
				outputFifo->peekOrPopReady(1);
				AvDecodedFrame *oldFrame = outputFifo->releaseOldest(1);
				if (oldFrame != NULL)
					avMspaceFree(oldFrame);
			}
			for (;;) {
				AvDecodedFrame *leftover = outputFifo->popReady();
				if (leftover == NULL)
					break;
				avMspaceFree(leftover);
			}
			outputFifo->clearAbort();
		}
		if (ev.type == AV_EVENT_JUMP) {
			flushTillNextSyncSample();
			setDiscardTarget((uint64_t)ev.payload);
		}
		if (stateMachine != NULL)
			stateMachine->postAckEvent(AV_SENDER_DECODER, ev.type, ev.payload, 0);
	}
	return 0;
}

void AvCoreDecoder::decodeThread()
{
	avLogDebug(debugLevel, "DecodeThread", 320, "Decode thread started");

	while (stopFlag == 0) {
		if (eventPendingFlag != 0 || eventQueue.count > 0) {
			processEvent();
			continue;
		}

		if (inputFifo == NULL || outputFifo == NULL || decoder == NULL) {
			sceKernelDelayThread(5000);
			continue;
		}

		if (inputFifo->getCount() <= 0) {
			sceKernelDelayThread(2000);
			continue;
		}

		AvAccessUnit *au = inputFifo->peekAu();
		if (au == NULL) {
			sceKernelDelayThread(2000);
			continue;
		}

		if (playbackMode != AV_PLAYMODE_NORMAL && au->isSyncSample == 0) {
			inputFifo->popAndFreeAu(au);
			avMspaceFree(au);
			continue;
		}

		uint32_t bufIndex = 0xFFFFFFFFu;
		uint8_t *outBuf = outputFifo->acquireBuffer(&bufIndex);
		if (outBuf == NULL) {
			sceKernelDelayThread(2000);
			continue;
		}

		AvDecodedFrame *frame = (AvDecodedFrame *)avMspaceAlloc(8, sizeof(AvDecodedFrame));
		if (frame == NULL) {
			sceKernelDelayThread(2000);
			continue;
		}
		frame->buffer = outBuf;
		frame->bufferIndex = bufIndex;

		int32_t gotOutput = 0;
		int32_t decRet = decoder->decode(au, frame, &gotOutput);

		inputFifo->popAndFreeAu(au);
		avMspaceFree(au);

		if (decRet >= 0 && gotOutput != 0) {
			if (getDiscardInformation((int64_t)frame->pts) == 0) {
				if (stateMachine != NULL && *stateMachine->getStatePtr() == AV_STATE_JUMP_PAUSE)
					frame->resetFlag = 2;
				outputFifo->pushFrame(frame);
			} else {
				avMspaceFree(frame);
			}
		} else {
			avMspaceFree(frame);
		}
	}

	avLogDebug(debugLevel, "DecodeThread", 440, "Decode thread exiting");
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
}

int32_t AvCoreDecoder::reset()
{
	avAtomicStore32(&eventPendingFlag, 1);
	if (outputFifo != NULL)
		outputFifo->signalAbort();
	avLogDebug(debugLevel, "Reset", 554, "Event added to decoder Q to process");
	if (addNewEvent(AV_EVENT_RESET, 0) < 0)
		return -1;
	avLogDebug(debugLevel, "Reset", 600, "Decoder Reset ");
	return 0;
}

int32_t AvCoreDecoder::stop()
{
	if (stopFlag == 1)
		return 0;

	avAtomicStore32(&stopFlag, 1);
	avLogDebug(debugLevel, "Stop", 633, " Decoder Stop called ");
	if (outputFifo != NULL)
		outputFifo->signalAbort();

	if (threadId != 0) {
		if (threadId < 0 ||
		    sceKernelWaitThreadEnd(threadId, NULL, NULL) < 0 ||
		    sceKernelDeleteThread(threadId) < 0) {
			avLogError(debugLevel, "Stop", 642, " Cannot Join Thread ");
		}
		threadId = 0;
	}

	if (decoder != NULL)
		decoder->stop();

	if (outputFifo != NULL) {
		while (outputFifo->getReadyCount() > 0) {
			outputFifo->peekOrPopReady(1);
			AvDecodedFrame *oldFrame = outputFifo->releaseOldest(1);
			if (oldFrame != NULL)
				avMspaceFree(oldFrame);
		}
		for (;;) {
			AvDecodedFrame *leftover = outputFifo->popReady();
			if (leftover == NULL)
				break;
			avMspaceFree(leftover);
		}
	}

	eventQueue.clear();

	AvDecodedFrame *flushFrame = (AvDecodedFrame *)avMspaceAlloc(1, sizeof(AvDecodedFrame));
	if (flushFrame != NULL) {
		uint32_t flushSlotIndex = 0xFFFFFFFFu;
		if (outputFifo != NULL)
			flushFrame->buffer = outputFifo->acquireBuffer(&flushSlotIndex);
		if (decoder != NULL && decoder->flush(flushFrame) < 0)
			avLogWarn(debugLevel, "Stop", 685, " Output frame flush failure");
		avMspaceFree(flushFrame);
	}

	if (outputFifo != NULL)
		outputFifo->clearAbort();
	return 0;
}

int32_t AvCoreDecoder::shutdown()
{
	avLogDebug(debugLevel, "Shutdown", 699, " Decoder Shutdown called ");
	if (decoder == NULL)
		return -1;
	stop();
	if (decoder->shutdown() < 0)
		avLogError(debugLevel, "Shutdown", 708, " Cannot ShutDown decoder ");
	if (outputFifo != NULL && outputFifo->destroy() < 0)
		avLogError(debugLevel, "Shutdown", 713, " Failed to Finalize Target(Output) buffers ");
	return 0;
}

int32_t AvCoreDecoder::jump(int64_t targetPts)
{
	avAtomicStore32(&eventPendingFlag, 1);
	if (outputFifo != NULL)
		outputFifo->signalAbort();
	avLogDebug(debugLevel, "Jump", 612, "Event added to decoder Q to process");
	return (addNewEvent(AV_EVENT_JUMP, targetPts) >= 0) ? 0 : -1;
}

int32_t AvCoreDecoder::setPlaybackMode(int32_t mode)
{
	playbackMode = mode;
	return 0;
}

int32_t AvCoreDecoder::getDiscardInformation(int64_t pts)
{
	int64_t target = (int64_t)discardUntilPts;
	if (target == -1LL)
		return 0;
	int64_t delta = target - pts;
	if (delta < 0)
		delta = pts - target;
	if (delta > 1000)
		return 1;
	avLogDebug(debugLevel, "GetDiscardInformation", 204, " --------->  New PTS start:  %lld (%lld) ", pts, pts / 90LL);
	avAtomicStore64(&discardUntilPts, 0xFFFFFFFFFFFFFFFFULL);
	return 0;
}

int32_t AvCoreDecoder::setDiscardTarget(uint64_t targetPts)
{
	if ((int64_t)discardUntilPts != -1LL)
		return 0;
	avAtomicStore64(&discardUntilPts, targetPts);
	return 1;
}

int32_t AvCoreDecoder::flushTillNextSyncSample()
{
	avLogDebug(debugLevel, "FlushTillNextSyncSample", 527, "Flushing AUs till next sync Sample");
	if (inputFifo == NULL)
		return 0;
	while (inputFifo->getCount() > 0) {
		AvAccessUnit *au = inputFifo->peekAu();
		if (au == NULL)
			break;
		if (au->isSyncSample == 1) {
			avLogWarn(debugLevel, "FlushTillNextSyncSample", 534, "Found a Sync sample to refresh");
			return 0;
		}
		inputFifo->popAndFreeAu(au);
		avMspaceFree(au);
	}
	return 0;
}
