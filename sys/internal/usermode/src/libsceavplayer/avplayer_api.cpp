#include "avplayer_internal.h"

SceClibMspace g_avCoreMspace = NULL;
uint8_t g_avMp4Preloaded = 0;

static SceUID g_avFileIoMutex = 0;
static SceAvPlayerInstance *g_avActivePlayer = NULL;

void *avMspaceAlloc(uint32_t alignment, SceSize size)
{
	(void)alignment;
	if (g_avCoreMspace == NULL || size == 0)
		return NULL;
	SceSize alignedSize = (size + 3u) & ~3u;
	void *mem = sceClibMspaceMalloc(g_avCoreMspace, alignedSize);
	if (mem != NULL)
		sceClibMemset(mem, 0, alignedSize);
	return mem;
}

void avMspaceFree(void *ptr)
{
	if (g_avCoreMspace != NULL && ptr != NULL)
		sceClibMspaceFree(g_avCoreMspace, ptr);
}

void *operator new(size_t size)
{
	return avMspaceAlloc(8, (SceSize)size);
}

void *operator new(size_t size, void *placement)
{
	(void)size;
	return placement;
}

void operator delete(void *ptr) throw()
{
	avMspaceFree(ptr);
}

void avLogDebug(int32_t level, const char *funcName, uint32_t line, const char *fmt, ...)
{
	if (level == SCE_AVPLAYER_DBG_ALL) {
		char buffer[256];
		va_list args;
		va_start(args, fmt);
		int32_t prefixLen = sceClibSnprintf(buffer, sizeof(buffer), "D/ %s [%u]: ", funcName, line);
		if (prefixLen < 255)
			sceClibVsnprintf(&buffer[prefixLen], sizeof(buffer) - (SceSize)prefixLen, fmt, args);
		va_end(args);
		sceClibPrintf("%s\n", buffer);
	}
}

int32_t avThreadTrampoline(SceSize argSize, void *argBlock)
{
	(void)argSize;
	AvThreadStartArg *startArg = (AvThreadStartArg *)argBlock;
	startArg->entry(startArg->context);
	return 0;
}

static void avAutoPlayEventCallback(void *userPtr, int32_t eventId, int32_t sourceId, void *eventData)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)userPtr;
	if (player == NULL)
		return;

	if (eventId == SCE_AVPLAYER_STATE_READY && player->autoStart) {
		int32_t count = player->controller->getStreamCount();
		int32_t selectedVideo = -1;
		int32_t selectedAudio = -1;
		int32_t fallbackAudio = -1;

		for (int32_t i = 0; i < count; i++) {
			SceAvPlayerStreamInfo info;
			if (player->controller->getStreamInfo((uint32_t)i, &info) == 0) {
				if (info.type == SCE_AVPLAYER_VIDEO && selectedVideo < 0) {
					selectedVideo = i;
				} else if (info.type == SCE_AVPLAYER_AUDIO) {
					if (fallbackAudio < 0)
						fallbackAudio = i;
					if (player->defaultLanguage != NULL &&
					    sceClibStrncasecmp((const char *)info.details.audio.languageCode,
					                       player->defaultLanguage, 3) == 0) {
						selectedAudio = i;
					}
				}
			}
		}

		if (selectedVideo >= 0)
			player->controller->enableStream((uint32_t)selectedVideo);
		if (selectedAudio >= 0)
			player->controller->enableStream((uint32_t)selectedAudio);
		else if (fallbackAudio >= 0)
			player->controller->enableStream((uint32_t)fallbackAudio);

		player->controller->start();
	}

	if (player->userEventCb.eventCallback != NULL)
		player->userEventCb.eventCallback(player->userEventCb.objectPointer, eventId, sourceId, eventData);
}

static void *avDefaultAllocate(void *argP, uint32_t alignment, uint32_t size)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)argP;
	if (player != NULL && player->userMemAlloc.allocate != NULL)
		return player->userMemAlloc.allocate(player->userMemAlloc.objectPointer, alignment, size);
	return NULL;
}

static void avDefaultDeallocate(void *argP, void *mem)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)argP;
	if (player != NULL && player->userMemAlloc.deallocate != NULL)
		player->userMemAlloc.deallocate(player->userMemAlloc.objectPointer, mem);
}

static void *avDefaultAllocateTexture(void *argP, uint32_t alignment, uint32_t size)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)argP;
	if (player != NULL && player->userMemAlloc.allocateTexture != NULL)
		return player->userMemAlloc.allocateTexture(player->userMemAlloc.objectPointer, alignment, size);
	return NULL;
}

static void avDefaultDeallocateTexture(void *argP, void *mem)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)argP;
	if (player != NULL && player->userMemAlloc.deallocateTexture != NULL)
		player->userMemAlloc.deallocateTexture(player->userMemAlloc.objectPointer, mem);
}

extern "C" {

int32_t module_start(SceSize args, const void *argp)
{
	(void)args;
	(void)argp;
	g_avFileIoMutex = sceKernelCreateMutex("FileIOLock", 0x2000u, 0, NULL);
	return 0;
}

SceAvPlayerHandle sceAvPlayerInit(SceAvPlayerInitData *pInit)
{
	if (pInit == NULL)
		return NULL;

	if (pInit->memoryReplacement.allocate == NULL || pInit->memoryReplacement.deallocate == NULL)
		return NULL;

	const SceSize coreHeapSize = 0x180000u;
	void *coreHeap = pInit->memoryReplacement.allocate(pInit->memoryReplacement.objectPointer, 32, coreHeapSize);
	if (coreHeap == NULL)
		return NULL;

	sceClibMemset(coreHeap, 0, coreHeapSize);
	g_avCoreMspace = sceClibMspaceCreate(coreHeap, coreHeapSize);
	if (g_avCoreMspace == NULL) {
		pInit->memoryReplacement.deallocate(pInit->memoryReplacement.objectPointer, coreHeap);
		return NULL;
	}

	SceAvPlayerInstance *player = (SceAvPlayerInstance *)avMspaceAlloc(8, sizeof(SceAvPlayerInstance));
	if (player == NULL) {
		sceClibMspaceDestroy(g_avCoreMspace);
		g_avCoreMspace = NULL;
		pInit->memoryReplacement.deallocate(pInit->memoryReplacement.objectPointer, coreHeap);
		return NULL;
	}

	player->coreHeapMem = coreHeap;
	player->userMemAlloc = pInit->memoryReplacement;
	player->userFileOps = pInit->fileReplacement;
	player->userEventCb = pInit->eventReplacement;
	player->debugLevel = pInit->debugLevel;
	player->basePriority = pInit->basePriority;
	player->numOutputVideoFrameBuffers = pInit->numOutputVideoFrameBuffers;
	player->autoStart = pInit->autoStart;
	player->defaultLanguage = pInit->defaultLanguage;
	player->apiMutex = sceKernelCreateMutex("SceAvPlayerMutex", 0x2000u, 0, NULL);
	player->sourceCount = 0;
	player->activeFlag = 1;

	SceAvPlayerInitData controllerInit;
	sceClibMemcpy_safe(&controllerInit, pInit, sizeof(controllerInit));
	controllerInit.memoryReplacement.objectPointer = player;
	controllerInit.memoryReplacement.allocate = avDefaultAllocate;
	controllerInit.memoryReplacement.deallocate = avDefaultDeallocate;
	controllerInit.memoryReplacement.allocateTexture = avDefaultAllocateTexture;
	controllerInit.memoryReplacement.deallocateTexture = avDefaultDeallocateTexture;
	controllerInit.eventReplacement.objectPointer = player;
	controllerInit.eventReplacement.eventCallback = avAutoPlayEventCallback;

	AvController *controller = (AvController *)avMspaceAlloc(8, sizeof(AvController));
	if (controller == NULL || controller->init(&controllerInit, player) < 0) {
		if (controller != NULL)
			avMspaceFree(controller);
		if (player->apiMutex >= 0)
			sceKernelDeleteMutex(player->apiMutex);
		avMspaceFree(player);
		sceClibMspaceDestroy(g_avCoreMspace);
		g_avCoreMspace = NULL;
		pInit->memoryReplacement.deallocate(pInit->memoryReplacement.objectPointer, coreHeap);
		return NULL;
	}

	player->controller = controller;
	g_avActivePlayer = player;
	return (SceAvPlayerHandle)player;
}

int32_t sceAvPlayerPostInit(SceAvPlayerHandle h, SceAvPlayerPostInitData *pPostInit)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL || pPostInit == NULL)
		return SCE_AVPLAYER_ERR_INVALID_PARAMS;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	int32_t ret = player->controller->setDemuxVideoBufferSize(pPostInit->demuxVideoBufferSize);
	sceKernelUnlockMutex(player->apiMutex, 1);
	return (ret < 0) ? SCE_AVPLAYER_ERR_OPERATION_FAILED : SCE_AVPLAYER_NO_ERR;
}

int32_t sceAvPlayerClose(SceAvPlayerHandle h)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL)
		return SCE_AVPLAYER_ERR_INVALID_PARAMS;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	if (player->controller != NULL) {
		player->controller->finalize();
		avMspaceFree(player->controller);
		player->controller = NULL;
	}
	SceAvPlayerMemAllocator allocCopy = player->userMemAlloc;
	void *heapCopy = player->coreHeapMem;
	SceUID mutexId = player->apiMutex;
	player->activeFlag = 0;
	sceKernelUnlockMutex(mutexId, 1);
	sceKernelDeleteMutex(mutexId);

	avMspaceFree(player);
	if (g_avCoreMspace != NULL) {
		sceClibMspaceDestroy(g_avCoreMspace);
		g_avCoreMspace = NULL;
	}
	if (heapCopy != NULL && allocCopy.deallocate != NULL)
		allocCopy.deallocate(allocCopy.objectPointer, heapCopy);

	if (g_avActivePlayer == player)
		g_avActivePlayer = NULL;
	return SCE_AVPLAYER_NO_ERR;
}

int32_t sceAvPlayerAddSource(SceAvPlayerHandle h, const char *argFilename)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL || argFilename == NULL)
		return SCE_AVPLAYER_ERR_INVALID_PARAMS;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	int32_t ret = player->controller->addSource(argFilename);
	if (ret >= 0)
		player->sourceCount++;
	sceKernelUnlockMutex(player->apiMutex, 1);
	return (ret < 0) ? SCE_AVPLAYER_ERR_OPERATION_FAILED : SCE_AVPLAYER_NO_ERR;
}

int32_t sceAvPlayerStreamCount(SceAvPlayerHandle h)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL)
		return SCE_AVPLAYER_ERR_INVALID_PARAMS;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	int32_t count = player->controller->getStreamCount();
	sceKernelUnlockMutex(player->apiMutex, 1);
	return count;
}

int32_t sceAvPlayerGetStreamInfo(SceAvPlayerHandle h, uint32_t argStreamID, SceAvPlayerStreamInfo *argInfo)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL || argInfo == NULL)
		return SCE_AVPLAYER_ERR_INVALID_PARAMS;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	int32_t ret = player->controller->getStreamInfo(argStreamID, argInfo);
	sceKernelUnlockMutex(player->apiMutex, 1);
	return (ret < 0) ? SCE_AVPLAYER_ERR_OPERATION_FAILED : SCE_AVPLAYER_NO_ERR;
}

int32_t sceAvPlayerEnableStream(SceAvPlayerHandle h, uint32_t argStreamID)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL)
		return SCE_AVPLAYER_ERR_INVALID_PARAMS;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	int32_t ret = player->controller->enableStream(argStreamID);
	sceKernelUnlockMutex(player->apiMutex, 1);
	return (ret < 0) ? SCE_AVPLAYER_ERR_OPERATION_FAILED : SCE_AVPLAYER_NO_ERR;
}

int32_t sceAvPlayerDisableStream(SceAvPlayerHandle h, uint32_t argStreamID)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL)
		return SCE_AVPLAYER_ERR_INVALID_PARAMS;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	int32_t ret = player->controller->disableStream(argStreamID);
	sceKernelUnlockMutex(player->apiMutex, 1);
	return (ret < 0) ? SCE_AVPLAYER_ERR_OPERATION_FAILED : SCE_AVPLAYER_NO_ERR;
}

int32_t sceAvPlayerStart(SceAvPlayerHandle h)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL)
		return SCE_AVPLAYER_ERR_INVALID_PARAMS;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	int32_t ret = player->controller->start();
	sceKernelUnlockMutex(player->apiMutex, 1);
	return (ret < 0) ? SCE_AVPLAYER_ERR_OPERATION_FAILED : SCE_AVPLAYER_NO_ERR;
}

int32_t sceAvPlayerStop(SceAvPlayerHandle h)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL)
		return SCE_AVPLAYER_ERR_INVALID_PARAMS;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	int32_t ret = player->controller->stop();
	sceKernelUnlockMutex(player->apiMutex, 1);
	return (ret < 0) ? SCE_AVPLAYER_ERR_OPERATION_FAILED : SCE_AVPLAYER_NO_ERR;
}

int32_t sceAvPlayerPause(SceAvPlayerHandle h)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL)
		return SCE_AVPLAYER_ERR_INVALID_PARAMS;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	int32_t ret = player->controller->pause();
	sceKernelUnlockMutex(player->apiMutex, 1);
	return (ret < 0) ? SCE_AVPLAYER_ERR_OPERATION_FAILED : SCE_AVPLAYER_NO_ERR;
}

int32_t sceAvPlayerResume(SceAvPlayerHandle h)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL)
		return SCE_AVPLAYER_ERR_INVALID_PARAMS;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	int32_t ret = player->controller->resume();
	sceKernelUnlockMutex(player->apiMutex, 1);
	return (ret < 0) ? SCE_AVPLAYER_ERR_OPERATION_FAILED : SCE_AVPLAYER_NO_ERR;
}

int32_t sceAvPlayerSetLooping(SceAvPlayerHandle h, bool loopFlag)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL)
		return SCE_AVPLAYER_ERR_INVALID_PARAMS;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	player->controller->setLooping(loopFlag ? 1u : 0u);
	sceKernelUnlockMutex(player->apiMutex, 1);
	return SCE_AVPLAYER_NO_ERR;
}

bool sceAvPlayerIsActive(SceAvPlayerHandle h)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL)
		return false;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	int32_t active = player->controller->isActive();
	sceKernelUnlockMutex(player->apiMutex, 1);
	return (active != 0);
}

bool sceAvPlayerGetAudioData(SceAvPlayerHandle h, SceAvPlayerFrameInfo *audioInfo)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL || audioInfo == NULL)
		return false;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	int32_t got = player->controller->getAudioData(audioInfo);
	sceKernelUnlockMutex(player->apiMutex, 1);
	return (got != 0);
}

bool sceAvPlayerGetVideoData(SceAvPlayerHandle h, SceAvPlayerFrameInfo *videoInfo)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL || videoInfo == NULL)
		return false;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	int32_t got = player->controller->getVideoData(videoInfo);
	sceKernelUnlockMutex(player->apiMutex, 1);
	return (got != 0);
}

bool sceAvPlayerGetVideoDataEx(SceAvPlayerHandle h, SceAvPlayerFrameInfoEx *videoInfo)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL || videoInfo == NULL)
		return false;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	int32_t got = player->controller->getVideoDataEx(videoInfo);
	sceKernelUnlockMutex(player->apiMutex, 1);
	return (got != 0);
}

uint64_t sceAvPlayerCurrentTime(SceAvPlayerHandle h)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL)
		return 0;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	uint64_t nowMs = player->controller->getCurrentTime();
	sceKernelUnlockMutex(player->apiMutex, 1);
	return nowMs;
}

int32_t sceAvPlayerJumpToTime(SceAvPlayerHandle h, uint64_t argJumpTimeMsec)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL)
		return SCE_AVPLAYER_ERR_INVALID_PARAMS;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	int32_t ret = player->controller->jumpToTime((int64_t)argJumpTimeMsec);
	sceKernelUnlockMutex(player->apiMutex, 1);
	return (ret < 0) ? SCE_AVPLAYER_ERR_OPERATION_FAILED : SCE_AVPLAYER_NO_ERR;
}

int32_t sceAvPlayerSetTrickSpeed(SceAvPlayerHandle h, int32_t argTrickSpeed)
{
	SceAvPlayerInstance *player = (SceAvPlayerInstance *)h;
	if (player == NULL || player->controller == NULL)
		return SCE_AVPLAYER_ERR_INVALID_PARAMS;

	sceKernelLockMutex(player->apiMutex, 1, NULL);
	int32_t ret = player->controller->setTrickSpeed(argTrickSpeed);
	sceKernelUnlockMutex(player->apiMutex, 1);
	return (ret < 0) ? SCE_AVPLAYER_ERR_OPERATION_FAILED : SCE_AVPLAYER_NO_ERR;
}

} /* extern "C" */
