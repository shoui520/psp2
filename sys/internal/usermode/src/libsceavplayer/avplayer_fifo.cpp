#include "avplayer_internal.h"

/* AvAuFifo implementation */
int32_t AvAuFifo::init(uint32_t size, void *buffer, const char *name)
{
	if (buffer == NULL || size == 0)
		return -1;

	poolBase = (uint8_t *)buffer;
	poolSize = size;
	freeBytes = size;
	highWaterMark = size;
	lowWaterMark = 0;
	maxAuSize = 0;
	waitingFlag = 0;
	abortFlag = 0;
	fifoFlags = 0;
	isFull = 0;

	mspace = sceClibMspaceCreate(buffer, size);
	if (mspace == NULL) {
		avLogError(debugLevel, "AvAuFifo::Init", 56, "Failed to create AU mspace");
		return -1;
	}

	condMutex = sceKernelCreateMutex(name != NULL ? name : "FilterAu", 0x2000u, 0, NULL);
	if (condMutex < 0)
		return -1;

	condVar = sceKernelCreateCond(name != NULL ? name : "FilterAuCond", 0, condMutex, NULL);
	if (condVar < 0) {
		sceKernelDeleteMutex(condMutex);
		condMutex = -1;
		return -1;
	}
	return 0;
}

int32_t AvAuFifo::allocateAu(AvAccessUnit *au, uint32_t size)
{
	if (au == NULL || mspace == NULL)
		return -1;

	sceKernelLockMutex(condMutex, 1, NULL);
	uint8_t *mem = NULL;
	while (abortFlag == 0) {
		SceSize aligned = (size + 3u) & ~3u;
		if (aligned == 0)
			aligned = 4;
		mem = (uint8_t *)sceClibMspaceMalloc(mspace, aligned);
		if (mem != NULL) {
			isFull = 0;
			au->buffer = mem;
			au->capacity = aligned;
			if (freeBytes >= aligned)
				freeBytes -= aligned;
			else
				freeBytes = 0;
			break;
		}
		isFull = 1;
		waitingFlag = 1;
		sceKernelWaitCond(condVar, NULL);
		waitingFlag = 0;
	}
	sceKernelUnlockMutex(condMutex, 1);
	return (mem != NULL) ? 0 : -1;
}

int32_t AvAuFifo::pushAu(AvAccessUnit *au)
{
	if (au == NULL)
		return -1;
	sceKernelLockMutex(condMutex, 1, NULL);
	queue.pushBack(au);
	if (waitingFlag != 0)
		sceKernelSignalCond(condVar);
	sceKernelUnlockMutex(condMutex, 1);
	return 0;
}

AvAccessUnit *AvAuFifo::peekAu()
{
	sceKernelLockMutex(condMutex, 1, NULL);
	AvAccessUnit *front = NULL;
	if (queue.count > 0)
		front = queue.at(0);
	sceKernelUnlockMutex(condMutex, 1);
	return front;
}

int32_t AvAuFifo::popAndFreeAu(AvAccessUnit *au)
{
	sceKernelLockMutex(condMutex, 1, NULL);
	if (queue.count > 0) {
		AvAccessUnit *front = queue.at(0);
		queue.eraseAt(0);
		if (front != NULL && front->buffer != NULL && mspace != NULL) {
			sceClibMspaceFree(mspace, front->buffer);
			freeBytes += front->capacity;
			front->buffer = NULL;
		}
		isFull = 0;
		if (waitingFlag != 0)
			sceKernelSignalCond(condVar);
	} else if (au != NULL && au->buffer != NULL && mspace != NULL) {
		sceClibMspaceFree(mspace, au->buffer);
		au->buffer = NULL;
	}
	sceKernelUnlockMutex(condMutex, 1);
	return 0;
}

int32_t AvAuFifo::flush(void (*freeFunc)(void *))
{
	sceKernelLockMutex(condMutex, 1, NULL);
	while (queue.count > 0) {
		AvAccessUnit *front = queue.at(0);
		queue.eraseAt(0);
		if (front != NULL) {
			if (front->buffer != NULL && mspace != NULL) {
				sceClibMspaceFree(mspace, front->buffer);
				front->buffer = NULL;
			}
			if (freeFunc != NULL)
				freeFunc(front);
		}
	}
	freeBytes = poolSize;
	isFull = 0;
	if (waitingFlag != 0)
		sceKernelSignalCond(condVar);
	sceKernelUnlockMutex(condMutex, 1);
	return 0;
}

int32_t AvAuFifo::getCount()
{
	return (int32_t)queue.count;
}

int32_t AvAuFifo::destroy()
{
	signalAbort();
	flush(avMspaceFree);
	if (condVar >= 0) {
		sceKernelDeleteCond(condVar);
		condVar = -1;
	}
	if (condMutex >= 0) {
		sceKernelDeleteMutex(condMutex);
		condMutex = -1;
	}
	return 0;
}

int32_t AvAuFifo::isBufferFull()
{
	return (int32_t)isFull;
}

void AvAuFifo::signalAbort()
{
	sceKernelLockMutex(condMutex, 1, NULL);
	abortFlag = 1;
	sceKernelSignalCond(condVar);
	sceKernelUnlockMutex(condMutex, 1);
}

void AvAuFifo::clearAbort()
{
	sceKernelLockMutex(condMutex, 1, NULL);
	abortFlag = 0;
	sceKernelUnlockMutex(condMutex, 1);
}

/* AvBaseClock implementation */
void AvBaseClock::initBaseClock()
{
	audioPts = 0;
	videoPts = 0;
	systemStartUs = 0;
	baseMediaTime = 0;
	lastMediaTime = 0;
	pauseOffset = 0;
	playbackSpeed = SCE_AVPLAYER_SPEED_NORMAL;
	clockFlags = 0;
	hasAudioClock = 0;
	hasVideoClock = 0;
	isRunning = 0;
	isPaused = 0;
	debugLevel = SCE_AVPLAYER_DBG_WARNINGS;
}

void AvBaseClock::updateVideoPts(uint64_t pts)
{
	videoPts = pts;
	hasVideoClock = 1;
	if (!isRunning) {
		systemStartUs = sceKernelGetProcessTimeWide();
		baseMediaTime = pts;
		isRunning = 1;
	}
	lastMediaTime = pts;
}

void AvBaseClock::updateAudioPts(uint64_t pts)
{
	audioPts = pts;
	hasAudioClock = 1;
	if (!isRunning) {
		systemStartUs = sceKernelGetProcessTimeWide();
		baseMediaTime = pts;
		isRunning = 1;
	}
	lastMediaTime = pts;
}

uint64_t AvBaseClock::getVideoPts()
{
	return videoPts;
}

uint64_t AvBaseClock::getAudioPts()
{
	return audioPts;
}

int32_t AvBaseClock::evaluateVideoSync(uint64_t framePts)
{
	if (!isRunning)
		return AV_CLOCK_SHOW_NOW;

	uint64_t refTime = getCurrentMediaTime90k();
	int64_t diff = (int64_t)framePts - (int64_t)refTime;

	if (diff > 3000)
		return AV_CLOCK_WAIT;
	if (diff < -4500)
		return AV_CLOCK_DROP_LATE;
	if (hasAudioClock && (int64_t)audioPts - (int64_t)framePts > 9000)
		return AV_CLOCK_AUDIO_AHEAD;
	if (!hasAudioClock)
		return AV_CLOCK_NO_AUDIO;
	return AV_CLOCK_SHOW_NOW;
}

void AvBaseClock::setPlaybackSpeed(int32_t speed)
{
	baseMediaTime = getCurrentMediaTime90k();
	systemStartUs = sceKernelGetProcessTimeWide();
	playbackSpeed = speed;
}

void AvBaseClock::resumeClock()
{
	systemStartUs = sceKernelGetProcessTimeWide();
	baseMediaTime = lastMediaTime;
	isPaused = 0;
	isRunning = 1;
}

void AvBaseClock::resetClock()
{
	audioPts = 0;
	videoPts = 0;
	systemStartUs = 0;
	baseMediaTime = 0;
	lastMediaTime = 0;
	pauseOffset = 0;
	hasAudioClock = 0;
	hasVideoClock = 0;
	isRunning = 0;
	isPaused = 0;
}

uint64_t AvBaseClock::getCurrentMediaTime90k()
{
	if (!isRunning)
		return baseMediaTime;
	uint64_t nowUs = sceKernelGetProcessTimeWide();
	uint64_t elapsedUs = (nowUs > systemStartUs) ? (nowUs - systemStartUs) : 0;
	int64_t speed = (playbackSpeed != 0) ? (int64_t)playbackSpeed : 100;
	int64_t delta90k = ((int64_t)elapsedUs * 9 * speed) / 10000;
	int64_t mediaNow = (int64_t)baseMediaTime + delta90k;
	if (mediaNow < 0)
		mediaNow = 0;
	return (uint64_t)mediaNow;
}

int32_t AvBaseClock::getPlaybackSpeed()
{
	return playbackSpeed;
}

uint64_t AvBaseClock::getCurrentMediaTimeMs()
{
	return getCurrentMediaTime90k() / 90u;
}

/* AvOutputFifo implementation */
int32_t AvOutputFifo::init(uint32_t totalSize, uint32_t frameSize, uint32_t maxSlots, void *buffer, const char *name, int32_t dbgLevel)
{
	if (buffer == NULL || totalSize == 0 || frameSize == 0)
		return -1;

	poolBase = (uint8_t *)buffer;
	poolSize = totalSize;
	slotSize = frameSize;
	slotCount = totalSize / frameSize;
	maxHoldCount = maxSlots;
	allocatedCount = 0;
	waitingFlag = 0;
	abortFlag = 0;
	isFull = 0;
	debugLevel = dbgLevel;

	condMutex = sceKernelCreateMutex(name != NULL ? name : "decbuff", 0x2000u, 0, NULL);
	if (condMutex < 0)
		return -1;

	condVar = sceKernelCreateCond(name != NULL ? name : "decbuffCond", 0, condMutex, NULL);
	if (condVar < 0) {
		sceKernelDeleteMutex(condMutex);
		condMutex = -1;
		return -1;
	}
	return 0;
}

uint8_t *AvOutputFifo::acquireBuffer(uint32_t *outIndex)
{
	sceKernelLockMutex(condMutex, 1, NULL);
	uint8_t *result = NULL;
	while (abortFlag == 0) {
		if (readyQueue.count + freeQueue.count < slotCount) {
			uint32_t idx = allocatedCount % slotCount;
			allocatedCount++;
			if (outIndex != NULL)
				*outIndex = idx;
			result = poolBase + (idx * slotSize);
			isFull = 0;
			break;
		}
		isFull = 1;
		waitingFlag = 1;
		sceKernelWaitCond(condVar, NULL);
		waitingFlag = 0;
	}
	sceKernelUnlockMutex(condMutex, 1);
	return result;
}

int32_t AvOutputFifo::pushFrame(AvDecodedFrame *frame)
{
	if (frame == NULL)
		return -1;
	sceKernelLockMutex(condMutex, 1, NULL);
	readyQueue.pushBack(frame);
	if (waitingFlag != 0)
		sceKernelSignalCond(condVar);
	sceKernelUnlockMutex(condMutex, 1);
	return 0;
}

AvDecodedFrame *AvOutputFifo::peekOrPopReady(uint32_t popFlag)
{
	sceKernelLockMutex(condMutex, 1, NULL);
	AvDecodedFrame *frame = NULL;
	if (readyQueue.count > 0) {
		frame = readyQueue.at(0);
		if (popFlag != 0) {
			readyQueue.eraseAt(0);
			freeQueue.pushBack(frame);
		}
	}
	sceKernelUnlockMutex(condMutex, 1);
	return frame;
}

AvDecodedFrame *AvOutputFifo::popReadyNoWait()
{
	return peekOrPopReady(1);
}

AvDecodedFrame *AvOutputFifo::popReady()
{
	sceKernelLockMutex(condMutex, 1, NULL);
	AvDecodedFrame *frame = NULL;
	if (readyQueue.count > 0) {
		frame = readyQueue.at(0);
		readyQueue.eraseAt(0);
		isFull = 0;
		if (waitingFlag != 0)
			sceKernelSignalCond(condVar);
	}
	sceKernelUnlockMutex(condMutex, 1);
	return frame;
}

AvDecodedFrame *AvOutputFifo::releaseOldest(uint32_t forceFlag)
{
	sceKernelLockMutex(condMutex, 1, NULL);
	AvDecodedFrame *released = NULL;
	if (freeQueue.count > maxHoldCount || (forceFlag != 0 && freeQueue.count > 0)) {
		released = freeQueue.at(0);
		freeQueue.eraseAt(0);
		isFull = 0;
		if (waitingFlag != 0)
			sceKernelSignalCond(condVar);
	}
	sceKernelUnlockMutex(condMutex, 1);
	return released;
}

int32_t AvOutputFifo::getReadyCount()
{
	return (int32_t)readyQueue.count;
}

int32_t AvOutputFifo::destroy()
{
	signalAbort();
	sceKernelLockMutex(condMutex, 1, NULL);
	while (readyQueue.count > 0) {
		AvDecodedFrame *frame = readyQueue.at(0);
		readyQueue.eraseAt(0);
		if (frame != NULL)
			avMspaceFree(frame);
	}
	while (freeQueue.count > 0) {
		AvDecodedFrame *frame = freeQueue.at(0);
		freeQueue.eraseAt(0);
		if (frame != NULL)
			avMspaceFree(frame);
	}
	sceKernelUnlockMutex(condMutex, 1);

	if (condVar >= 0) {
		sceKernelDeleteCond(condVar);
		condVar = -1;
	}
	if (condMutex >= 0) {
		sceKernelDeleteMutex(condMutex);
		condMutex = -1;
	}
	return 0;
}

void AvOutputFifo::signalAbort()
{
	sceKernelLockMutex(condMutex, 1, NULL);
	abortFlag = 1;
	sceKernelSignalCond(condVar);
	sceKernelUnlockMutex(condMutex, 1);
}

void AvOutputFifo::clearAbort()
{
	sceKernelLockMutex(condMutex, 1, NULL);
	abortFlag = 0;
	sceKernelUnlockMutex(condMutex, 1);
}

int32_t AvOutputFifo::isBufferFull()
{
	return (int32_t)isFull;
}
