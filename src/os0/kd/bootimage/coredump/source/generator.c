/* SceCoredump writer stack and dump generator. */

#include "writer_internal.h"
#include "seg_internal.h"
#include <sce/debug.h>
#include <sce/error.h>
#include <elf.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <string.h>
#include <stdio.h>

typedef struct CoredumpCancelContext {
	int requestId;
	int numCanceled;
} CoredumpCancelContext;

_Static_assert(sizeof(CoredumpCancelContext) == 8, "cancel context size");

Writer *currentWriter __attribute__((section(".data"))) = NULL;

SceUInt32 numWritableSegments;
int cafWriterInitialized;
CafWriter cafWriter;
int gzipWriterInitialized;
GzipWriter gzipWriter;
int fileWriterInitialized;
int activeRequestId;
SceKernelCoredumpStateUpdateCallback progressCallback;
int progressEnabled;
SceUID handlerThreadId;
const SegIfMethodPair *activeSegments;
FileWriter fileWriter;
int useAlternateCafSegment;
SceUInt32 numWrittenSegments;
SceUInt32 numCalculatedSegments;
SceUInt32 numWrittenProgramHeaders;
SceUInt32 reservedSegmentCount;
SceUID generatorMutexId;
int shutdownRequested;
int cancelRequested;
SceSize processedFileSize;
SceSize totalFileSize;
SceUInt32 numAvailableSegments;
SceUInt32 numProgramHeaders;
SceUInt64 programDataOffset;
SceUInt64 corefileSizeLimit;
CoredumpCdnPcAndLrInfo cdnLocation;
int plaintextDumpAllowed;
CoredumpEventQueue generatorEventQueue;
char corefilePath[1330];
int kernelDumpMemoryInitialized;
int kernelDumpCafInitialized;
int kernelDumpWritersInitialized;
SceSize kernelDumpPayloadSize;
SceSize kernelDumpWrittenSize;

extern int oalIsToolDipswEnabled(void);
int _getExitFlag(SceSize processedSize);
extern int oalFileOpen(const char *path, SceUID *fd);
extern void oalGetDestHostPathFromSettings(char *path);
extern unsigned int oalGetUnixTime(void);
extern int oalGetProcessPath(SceUID pid, char *path, SceSize *pathSize);
extern int oalGetProcessDumpName(SceUID pid, char *name);
extern int oalFileSetIoCacheSize(SceUID fd, SceSize cacheSize);
extern int oalIsRootProgram(SceUID pid);
extern int triggerGetNextRequestId(void);
extern const SegIfMethodPair kernelSegments[5];
extern int oalMutexLock(SceUID mutexId);
extern int oalMutexUnlock(SceUID mutexId);
extern int writeFrameBuffers(SceUID pid);
extern int oalResumeProcess(SceUID pid);
extern int oalSuspendProcess(SceUID pid, SceUInt32 *timeout);
extern int oalChangeThreadPriority(SceUID threadId, int priority);
extern int writerFlush(Writer *writer);
extern SceSize encryptedScreenshotSize;
extern const SegIfMethodPair registeredSegments[50];
extern int oalIsPlaintextDumpAllowed(SceUID pid);
extern int oalIsSystemProgram(SceUID pid);
extern int oalMutexFinal(SceUID mutexId);
extern int oalEventQueueReceive(CoredumpEventQueue *queue, CoredumpEvent *event);
extern int oalEventQueueSend(CoredumpEventQueue *queue, const CoredumpEvent *event);
extern int oalEventQueueEachElem(CoredumpEventQueue *queue,
	void (*callback)(CoredumpEvent *event, void *context), void *context);
extern int oalEventQueueFinal(CoredumpEventQueue *queue);
extern int oalExitDeleteThread(void);
extern int oalReleaseObject(SceUID objectId);
extern int oalFileRename(const char *from, const char *to);
extern int oalCafInit(void);
extern int oalCafFinal(void);
extern int memMgrInit(void);
extern int memMgrFinal(void);
extern int memMgrTerminate(void);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern int removeEncryptedScreenshot(void);
extern int coredumpMemoryInit(void);
extern int userDataFinal(void);
extern int userDataInit(void);
extern int memMgrBootstrap(void);
extern int oalEventQueueInit(CoredumpEventQueue *queue);
extern int oalMutexInit(SceUID *mutexId);
extern int oalCreateThread(SceUID *threadId, const char *name, int priority,
	SceSize stackSize, SceKernelThreadEntry entry);
extern int oalStartThread(SceUID threadId);
extern int oalDeleteThread(SceUID threadId);
extern void segSummarySetSegIfMethodPairTables(const SegIfMethodPair *pairs,
	SceUInt32 numSegments);
extern SceUID openCorefile(void);
extern int segInit(SegIf *segment);
extern int segCalcSize(SegIf *segment);
extern int utilsElf32MakeEhdr(Elf32_Ehdr *header, SceUInt16 numPhdr);
extern int oalGetCdnPcAndLrInfo(SceUID pid,
	const CoredumpThreadStopEvent *event, CoredumpCdnPcAndLrInfo *location);
extern int writerWritePaddingData(Writer *writer, SceSize size);
extern int cafWriterEndSegment(CafWriter *writer);
extern int cafWriterGetNumSegments(CafWriter *writer);
extern int oalFileRemove(const char *path);
extern void oalGetCdnAccessParam(SceUInt32 appVersion, SceUInt32 stopReason,
	CoredumpCdnAccessParam *param, const CoredumpCdnPcAndLrInfo *location);

int initWriters(FileWriterOpenCallback openFile, int useGzip, int useCaf,
	SceUInt32 maxSegments)
{
	Writer *destination;
	SceUInt32 segmentType;
	int result;

	fileWriterInitialized = 0;
	cafWriterInitialized = 0;
	gzipWriterInitialized = 0;
	if (oalIsToolDipswEnabled() != 0)
		result = fileWriterInit(&fileWriter, openFile, 0x100000, 0);
	else
		result = fileWriterInit(&fileWriter, openFile, 0x10000, 0);
	if (result != 0)
		goto error;
	destination = &fileWriter.writer;
	currentWriter = destination;
	fileWriterInitialized = 1;
	if (useCaf != 0) {
		segmentType = useAlternateCafSegment != 0 ? 2 : 1;
		result = cafWriterInit(&cafWriter, destination, segmentType,
			1024, 1024, maxSegments);
		if (result != 0)
			goto error;
		result = cafWriterBeginSegment(&cafWriter, segmentType);
		if (result != 0)
			goto error;
		destination = &cafWriter.writer;
		currentWriter = destination;
		cafWriterInitialized = 1;
	}
	if (useGzip == 0)
		return result;
	result = gzipWriterInit(&gzipWriter, destination, 1024, 1024,
		_getExitFlag);
	if (result == 0) {
		currentWriter = &gzipWriter.writer;
		gzipWriterInitialized = 1;
		return result;
	}
error:
	if (gzipWriterInitialized != 0) {
		gzipWriterFinal(&gzipWriter);
		gzipWriterInitialized = 0;
	}
	if (cafWriterInitialized != 0) {
		cafWriterFinal(&cafWriter);
		cafWriterInitialized = 0;
	}
	if (fileWriterInitialized != 0) {
		fileWriterFinal(&fileWriter);
		fileWriterInitialized = 0;
	}
	return result;
}

int writeSegments(void)
{
	SceUInt32 index = 0;
	SceUInt32 timeout;
	int result;

	for (;;) {
		if (numWritableSegments <= index)
			break;
		result = segWriteSegs(activeSegments[index].segment);
		if (result != 0)
			return result;
		index++;
		numWrittenSegments++;
		if (index == 14)
			break;
	}
	if (index == numWritableSegments)
		return 0;
	writeFrameBuffers(coredumpRun.pid);
	if (progressCallback != NULL)
		progressCallback(activeRequestId, coredumpRun.pid, 0);
	timeout = 10000000;
	if (coredumpRun.triggerReason == -2) {
		result = oalResumeProcess(coredumpRun.pid);
		if (result != 0)
			return result;
	} else if (coredumpRun.triggerReason != -1) {
		result = oalSuspendProcess(coredumpRun.pid, &timeout);
		if (result != 0)
			return result;
	}
	oalChangeThreadPriority(handlerThreadId, 77);
	progressEnabled = 1;
	result = writerFlush(&fileWriter.writer);
	if (result != 0)
		return result;
	while (index < numWritableSegments) {
		result = segWriteSegs(activeSegments[index].segment);
		if (result != 0)
			return result;
		index++;
		numWrittenSegments++;
	}
	return 0;
}

int openCrashReportFile(void)
{
	SceUID fd;
	int result;

	result = oalFileOpen("ux0:data/crash_report.caf", &fd);
	if (result == 0)
		result = fd;
	return result;
}

void cancelQueuedRequest(CoredumpEvent *event, void *argp)
{
	CoredumpCancelContext *context = argp;

	if (event->kind <= 1 && context->requestId == event->requestId) {
		event->canceled = 1;
		context->numCanceled++;
	}
}

int _getExitFlag(SceSize processedSize)
{
	int result, exitFlag;
	SceKernelCoredumpStateUpdateCallback callback;
	SceSize total, processed;
	SceUInt32 oldPercent, newPercent;
	int notify;

	SCE_DBG_ASSERT_FUNC(1, 2297, "_getExitFlag");
	if (oalMutexLock(generatorMutexId) != 0)
		return (int)0x8001008c;
	exitFlag = shutdownRequested;
	if (oalMutexUnlock(generatorMutexId) != 0 || exitFlag != 0)
		return (int)0x8001008c;
	SCE_DBG_ASSERT_FUNC(1, 2257, "_getCancelFlag");
	result = oalMutexLock(generatorMutexId);
	if (result != 0)
		return result;
	exitFlag = cancelRequested;
	result = oalMutexUnlock(generatorMutexId);
	if (result != 0)
		return result;
	if (exitFlag != 0)
		return (int)0x8001008c;
	callback = progressCallback;
	if (callback != NULL && cancelRequested != 1 && progressEnabled != 0) {
		total = totalFileSize;
		processed = processedFileSize + processedSize;
		oldPercent = (SceUInt64)100 * processedFileSize / total;
		processedFileSize = processed;
		newPercent = 5 * ((SceUInt32)((SceUInt64)100 * processed / total) / 5);
		notify = newPercent <= 99;
		if (oldPercent - oldPercent % 5 == newPercent)
			notify = 0;
		if (notify)
			callback(activeRequestId, coredumpRun.pid, newPercent);
	}
	return 0;
}

int accumulateEncryptedScreenshotSize(SceSize size)
{
	encryptedScreenshotSize += size;
	return 0;
}

int calculateCorefileSize(void)
{
	SceUInt32 index, numPhdr;
	SceSize dataSize, nextSize, phdrSize;

	numProgramHeaders = 0;
	processedFileSize = 0;
	totalFileSize = 52;
	programDataOffset = 52;
	reservedSegmentCount = 0;
	numWritableSegments = 0;
	corefileSizeLimit = ~(SceUInt64)0;
	for (index = 0; index < numAvailableSegments; ++index) {
		segGetSize(activeSegments[index].segment, &numPhdr, &dataSize);
		phdrSize = 32 * numPhdr;
		nextSize = totalFileSize + dataSize + phdrSize;
		if (corefileSizeLimit < (SceUInt64)nextSize)
			break;
		totalFileSize = nextSize;
		numProgramHeaders += numPhdr;
		programDataOffset += phdrSize;
		numWritableSegments++;
	}
	return 0;
}

int writeCrashReport(void)
{
	CoredumpCdnAccessParam access;
	SceSize pathSize;
	SceUInt32 type;
	int result, payloadResult, finalResult;

	fileWriterInitialized = 0;
	cafWriterInitialized = 0;
	gzipWriterInitialized = 0;
	if (oalIsToolDipswEnabled())
		result = fileWriterInit(&fileWriter, openCrashReportFile, 0x100000, 0);
	else
		result = fileWriterInit(&fileWriter, openCrashReportFile, 0x10000, 0);
	if (result != 0)
		goto error;
	type = useAlternateCafSegment ? 2 : 1;
	currentWriter = &fileWriter.writer;
	fileWriterInitialized = 1;
	result = cafWriterInit(&cafWriter, &fileWriter.writer, type, 1024, 1024, 1);
	if (result != 0)
		goto error;
	result = cafWriterBeginSegment(&cafWriter, type);
	if (result != 0)
		goto error;
	currentWriter = &cafWriter.writer;
	cafWriterInitialized = 1;
	pathSize = strnlen(corefilePath, 1329) + 1;
	payloadResult = writerWrite(&cafWriter.writer, corefilePath, pathSize);
	if (payloadResult == 0) {
		payloadResult = writerWrite(currentWriter, coredumpRun.titleid,
			coredumpRun.titleid_len);
		if (payloadResult == 0)
			payloadResult = writerWritePaddingData(currentWriter,
				16 - ((coredumpRun.titleid_len + pathSize) & 15));
	}
	if (gzipWriterInitialized != 0) {
		result = gzipWriterFinal(&gzipWriter);
		gzipWriterInitialized = 0;
	}
	if (cafWriterInitialized != 0) {
		finalResult = cafWriterEndSegment(&cafWriter);
		if (finalResult != 0)
			result = finalResult;
		if (cafWriterGetNumSegments(&cafWriter) == 2) {
			finalResult = cafWriterBeginSegment(&cafWriter,
				useAlternateCafSegment ? 4 : 3);
			if (finalResult == 0) {
				oalGetCdnAccessParam(coredumpRun.appVersion,
					coredumpRun.stopReason, &access, &cdnLocation);
				finalResult = writerWrite(&cafWriter.writer, &access,
					sizeof(access));
				if (finalResult == 0)
					finalResult = cafWriterEndSegment(&cafWriter);
			}
			if (finalResult != 0)
				result = finalResult;
		}
		finalResult = cafWriterFinal(&cafWriter);
		if (finalResult != 0)
			result = finalResult;
		cafWriterInitialized = 0;
	}
	if (fileWriterInitialized != 0) {
		finalResult = fileWriterFinal(&fileWriter);
		if (finalResult < 0)
			result = finalResult;
		fileWriterInitialized = 0;
	}
	currentWriter = NULL;
	if (result != 0)
		goto remove;
	if (payloadResult != 0) {
		result = payloadResult;
		goto remove;
	}
	return result;
error:
	if (gzipWriterInitialized != 0) {
		gzipWriterFinal(&gzipWriter);
		gzipWriterInitialized = 0;
	}
	if (cafWriterInitialized != 0) {
		cafWriterFinal(&cafWriter);
		cafWriterInitialized = 0;
	}
	if (fileWriterInitialized != 0) {
		fileWriterFinal(&fileWriter);
		fileWriterInitialized = 0;
	}
remove:
	oalFileRemove("ux0:data/crash_report.caf");
	return result;
}

int _coredumpHandlerEntry(SceSize argSize, void *argp)
{
	CoredumpEvent event;
	union {
		CoredumpEvent event;
		Elf32_Ehdr elfHeader;
		CoredumpCdnAccessParam cdnParam;
	} buffer;
	CoredumpThreadStopEvent stopEvent;
	SceUInt32 index, numPhdr;
	SceSize dataSize, pathSize;
	SceOff fileOffset;
	char *finalPath;
	int memoryFailed = 0;
	int memoryInitialized, cafInitialized;
	int result, payloadResult, finalResult;

	(void)argSize;
	(void)argp;
	memset(&event, 0, sizeof(event));
	for (;;) {
		do {
			memset(&coredumpRun, 0, sizeof(coredumpRun));
			progressCallback = NULL;
			SCE_DBG_ASSERT_FUNC(1, 633, "_receiveEvent");
		} while (oalEventQueueReceive(&generatorEventQueue, &buffer.event) != 0);
		memcpy(&event, &buffer.event, sizeof(event));
		if (event.trigger.titleid != NULL && event.trigger.titleid_len != 0)
			SCE_PRINTF_WITH_INFO(15, 773, "[coredump] start ID=%s\n",
				event.trigger.titleid);
		else
			SCE_PRINTF_WITH_INFO(15, 775, "[coredump] start ID=Unknown\n");
		if (event.kind == 2)
			break;
		memoryInitialized = 0;
		cafInitialized = 0;
		if (event.kind > 1) {
			result = (int)0x80010016;
			goto restorePriority;
		}
		if (memoryFailed != 0) {
			result = (int)0x8001000c;
			goto restorePriority;
		}
		coredumpRun.pid = event.pid;
		memcpy(&coredumpRun.triggerParamSize, &event.trigger, sizeof(event.trigger));
		progressCallback = event.progressCallback;
		plaintextDumpAllowed = oalIsPlaintextDumpAllowed(event.pid);
		useAlternateCafSegment = oalIsSystemProgram(coredumpRun.pid);
		memset(corefilePath, 0, sizeof(corefilePath));
		numCalculatedSegments = 0;
		numWrittenProgramHeaders = 0;
		numWrittenSegments = 0;
		progressEnabled = 0;
		result = oalMutexLock(generatorMutexId);
		if (result != 0)
			goto restorePriority;
		cancelRequested = 0;
		activeRequestId = event.requestId;
		result = oalMutexUnlock(generatorMutexId);
		if (result != 0)
			goto restorePriority;
		if (event.canceled != 0) {
			result = (int)0x8001008c;
			goto restorePriority;
		}
		result = memMgrInit();
		if (result != 0)
			goto restorePriority;
		memoryInitialized = 1;
		stopEvent.threadId = coredumpRun.crash_thid;
		stopEvent.reason = coredumpRun.triggerReason;
		oalGetCdnPcAndLrInfo(coredumpRun.pid, &stopEvent, &cdnLocation);
		if (plaintextDumpAllowed == 0) {
			result = oalCafInit();
			if (result != 0)
				goto restorePriority;
			cafInitialized = 1;
		}
		if (plaintextDumpAllowed != 0)
			result = initWriters(openCorefile, 1, 0, 0);
		else
			result = initWriters(openCorefile, 1, 1, 2);
		if (result != 0)
			goto restorePriority;
		activeSegments = registeredSegments;
		numProgramHeaders = 0;
		totalFileSize = 0;
		programDataOffset = 0;
		processedFileSize = 0;
		reservedSegmentCount = 0;
		numWritableSegments = 0;
		numAvailableSegments = 50;
		for (index = 0; index < numAvailableSegments; ++index) {
			payloadResult = segInit(activeSegments[index].segment);
			if (payloadResult != 0)
				goto finalWriters;
		}
		for (index = 0; index < numAvailableSegments; ++index) {
			payloadResult = segCalcSize(activeSegments[index].segment);
			if (payloadResult != 0)
				goto finalWriters;
			numCalculatedSegments++;
		}
		calculateCorefileSize();
		utilsElf32MakeEhdr(&buffer.elfHeader, numProgramHeaders);
		payloadResult = writerWrite(currentWriter, &buffer.elfHeader,
			sizeof(buffer.elfHeader));
		if (payloadResult == 0) {
			fileOffset = programDataOffset;
			for (index = 0; index < numWritableSegments; ++index) {
				segGetSize(activeSegments[index].segment, &numPhdr, &dataSize);
				payloadResult = segWritePhdr(activeSegments[index].segment,
					fileOffset);
				if (payloadResult != 0)
					goto finalWriters;
				numWrittenProgramHeaders++;
				fileOffset += dataSize;
			}
			payloadResult = writeSegments();
		}
finalWriters:
		oalChangeThreadPriority(handlerThreadId, 30);
		if (gzipWriterInitialized != 0) {
			result = gzipWriterFinal(&gzipWriter);
			gzipWriterInitialized = 0;
		}
		if (cafWriterInitialized != 0) {
			finalResult = cafWriterEndSegment(&cafWriter);
			if (finalResult != 0)
				result = finalResult;
			if (cafWriterGetNumSegments(&cafWriter) == 2) {
				finalResult = cafWriterBeginSegment(&cafWriter,
					useAlternateCafSegment ? 4 : 3);
				if (finalResult == 0) {
					oalGetCdnAccessParam(coredumpRun.appVersion,
						coredumpRun.stopReason, &buffer.cdnParam, &cdnLocation);
					finalResult = writerWrite(&cafWriter.writer, &buffer.cdnParam,
						sizeof(buffer.cdnParam));
					if (finalResult == 0)
						finalResult = cafWriterEndSegment(&cafWriter);
				}
				if (finalResult != 0)
					result = finalResult;
			}
			finalResult = cafWriterFinal(&cafWriter);
			if (finalResult != 0)
				result = finalResult;
			cafWriterInitialized = 0;
		}
		if (fileWriterInitialized != 0) {
			finalResult = fileWriterFinal(&fileWriter);
			if (finalResult < 0)
				result = finalResult;
			fileWriterInitialized = 0;
		}
		currentWriter = NULL;
		if (result != 0)
			goto finalMemory;
		if (payloadResult != 0) {
			result = payloadResult;
			goto finalMemory;
		}
		pathSize = strnlen(corefilePath, sizeof(corefilePath));
		if (pathSize <= 3) {
			result = (int)0x80010016;
			goto finalMemory;
		}
		finalPath = memMgrLocalAlloc(pathSize - 3);
		if (finalPath == NULL) {
			result = (int)0x8001000c;
			goto finalMemory;
		}
		memset(finalPath, 0, pathSize - 3);
		strncpy(finalPath, corefilePath, pathSize - 4);
		oalFileRemove(finalPath);
		result = oalFileRename(corefilePath, finalPath);
		corefilePath[pathSize - 4] = 0;
		corefilePath[pathSize - 3] = 0;
		corefilePath[pathSize - 2] = 0;
		corefilePath[pathSize - 1] = 0;
		memMgrLocalFree(finalPath);
		if (result == 0 && plaintextDumpAllowed == 0)
			writeCrashReport();
		goto finalMemory;
restorePriority:
		oalChangeThreadPriority(handlerThreadId, 30);
finalMemory:
		if (plaintextDumpAllowed == 0 && cafInitialized != 0) {
			finalResult = oalCafFinal();
			if (finalResult != 0)
				result = finalResult;
		}
		if (memoryInitialized != 0) {
			finalResult = memMgrFinal();
			if (finalResult != 0) {
				result = finalResult;
				memoryFailed = 1;
				goto finishEvent;
			}
		}
		if (progressCallback != NULL && result == 0 && cancelRequested == 0)
			progressCallback(activeRequestId, coredumpRun.pid, 100);
finishEvent:
		finalResult = oalMutexLock(generatorMutexId);
		if (finalResult == 0) {
			cancelRequested = 0;
			finalResult = oalMutexUnlock(generatorMutexId);
			if (finalResult == 0)
				finalResult = result;
		}
		memset(&cdnLocation, 0, sizeof(cdnLocation));
		removeEncryptedScreenshot();
		coredumpMemoryInit();
		if (finalResult == (int)0x8001008c)
			SCE_PRINTF_WITH_INFO(15, 1017, "[coredump] canceled\n");
		else if (finalResult == 0)
			SCE_PRINTF_WITH_INFO(15, 1019, "[coredump] done\n");
		else
			SCE_PRINTF_WITH_INFO(15, 1015, "[coredump] error:0x%08x\n",
				finalResult);
		event.completionCallback(activeRequestId, coredumpRun.pid, finalResult,
			corefilePath, strnlen(corefilePath, 1329) + 1,
			plaintextDumpAllowed == 0, &event.completionContext);
		if (oalMutexLock(generatorMutexId) == 0) {
			cancelRequested = 0;
			activeRequestId = -1;
			oalMutexUnlock(generatorMutexId);
		}
		oalReleaseObject(event.pid);
	}
	oalMutexFinal(generatorMutexId);
	oalEventQueueFinal(&generatorEventQueue);
	memMgrTerminate();
	result = userDataFinal();
	if (event.kind == 2)
		event.shutdownCallback(result, &event.shutdownContext);
	oalExitDeleteThread();
	return 0;
}

int generatorInit(void)
{
	const SegIfMethodPair *pair;
	int result;

	shutdownRequested = 0;
	activeRequestId = -1;
	for (pair = registeredSegments; pair != registeredSegments + 50; ++pair) {
		pair->segment->methods = pair->methods;
		pair->segment->segmentType = pair->segmentType;
	}
	result = memMgrBootstrap();
	if (result != 0) {
		oalMutexFinal(generatorMutexId);
		return result;
	}
	result = oalEventQueueInit(&generatorEventQueue);
	if (result != 0) {
		oalMutexFinal(generatorMutexId);
		goto terminateMemory;
	}
	result = oalMutexInit(&generatorMutexId);
	if (result != 0) {
		oalMutexFinal(generatorMutexId);
		oalEventQueueFinal(&generatorEventQueue);
		goto terminateMemory;
	}
	result = userDataInit();
	if (result != 0)
		return result;
	result = oalCreateThread(&handlerThreadId, "SceCoredumpHandler", 30,
		0x4000, _coredumpHandlerEntry);
	if (result != 0)
		return result;
	result = oalStartThread(handlerThreadId);
	if (result != 0) {
		oalDeleteThread(handlerThreadId);
		return result;
	}
	segSummarySetSegIfMethodPairTables(registeredSegments, 50);
	cdnLocation.pc = 0;
	cdnLocation.lr = 0;
	cdnLocation.pcModuleFingerprint = 0;
	cdnLocation.lrModuleFingerprint = 0;
	return result;
terminateMemory:
	memMgrTerminate();
	return result;
}

int generatorSendEvent(const CoredumpEvent *event)
{
	CoredumpEvent copy;

	SCE_DBG_ASSERT(event != NULL, 622);
	memset(&copy, 0, sizeof(copy));
	memcpy(&copy, event, sizeof(copy));
	return oalEventQueueSend(&generatorEventQueue, &copy);
}

int generatorCancel(int requestId)
{
	CoredumpCancelContext context;
	int result, unlockResult;

	result = oalMutexLock(generatorMutexId);
	if (result != 0)
		return result;
	if (activeRequestId == requestId) {
		cancelRequested = 1;
	} else {
		context.requestId = requestId;
		context.numCanceled = 0;
		result = oalEventQueueEachElem(&generatorEventQueue,
			cancelQueuedRequest, &context);
		if (result == 0 && context.numCanceled == 0)
			result = (int)0x80010016;
	}
	unlockResult = oalMutexUnlock(generatorMutexId);
	if (unlockResult != 0)
		return unlockResult;
	return result;
}

int generatorBeginShutdown(void)
{
	int result;

	result = oalMutexLock(generatorMutexId);
	if (result == 0) {
		shutdownRequested = 1;
		result = oalMutexUnlock(generatorMutexId);
	}
	return result;
}

int openDumpFileAtDestination(SceUID *fd, const char *filename, char *path)
{
	char *hostPath, *destination;
	SceSize triggerDataSize;

	switch (coredumpRun.triggerDataType) {
	case 0:
		hostPath = memMgrLocalAlloc(1024);
		if (hostPath != NULL) {
			oalGetDestHostPathFromSettings(hostPath);
			if (strnlen(hostPath, 1024) != 0)
				snprintf(path, 1330, "%s%s/%s", "host0:", hostPath,
					filename);
			else
				snprintf(path, 1330, "%s%s", "host0:", filename);
			memMgrLocalFree(hostPath);
			if (oalFileOpen(path, fd) == 0)
				return 0;
		}
		snprintf(path, 1330, "%s%s", "sd0:", filename);
		if (oalFileOpen(path, fd) == 0)
			return 0;
		/* Fall through to the internal storage destination. */
	case 2:
		snprintf(path, 1330, "%s%s", "ux0:data/", filename);
		return oalFileOpen(path, fd);
	case 1:
		snprintf(path, 1330, "%s%s", "host0:", filename);
		return oalFileOpen(path, fd);
	case 10:
		triggerDataSize = coredumpRun.triggerDataSize;
		destination = path + 6;
		memcpy(path, "host0:", 6);
		if (triggerDataSize != 0) {
			memcpy(destination, coredumpRun.triggerData, triggerDataSize);
			snprintf(destination + triggerDataSize - 1,
				1324 - triggerDataSize, "/%s", filename);
		} else {
			snprintf(destination, 1324, "%s", filename);
		}
		return oalFileOpen(path, fd);
	default:
		return (int)0x80010016;
	}
}

int _createCorefile(SceUID *fd)
{
	char filename[300], processPath[256];
	const char *basename;
	SceSize pathSize;
	unsigned int timestamp, index, basenameIndex;
	char character;

	SCE_DBG_ASSERT(fd != NULL, 1244);
	SCE_DBG_ASSERT_FUNC(1, 1258, "_createCorefileName");
	memset(filename, 0, sizeof(filename));
	if (plaintextDumpAllowed != 0) {
		timestamp = oalGetUnixTime();
		if (coredumpRun.triggerFlags != 0 && coredumpRun.triggerReason == 1) {
			snprintf(filename, sizeof(filename),
				"psp2core-%010u-GPUCRASH.psp2dmp.tmp", timestamp);
		} else {
			oalGetProcessPath(coredumpRun.pid, processPath, &pathSize);
			basenameIndex = 0;
			index = 1;
			character = processPath[0];
			while (character != '\0') {
				if (character == '/' || character == ':' || character == '\\')
					basenameIndex = index;
				character = processPath[index++];
			}
			basename = processPath + basenameIndex;
			snprintf(filename, sizeof(filename),
				"psp2core-%010u-0x%010x-%s.psp2dmp.tmp", timestamp,
				coredumpRun.pid, basename);
		}
	} else if (coredumpRun.triggerFlags != 0 && coredumpRun.triggerReason == 1) {
		strncpy(filename, "psp2core-GPUCRASH.spsp2dmp.tmp", sizeof(filename));
	} else {
		oalGetProcessDumpName(coredumpRun.pid, processPath);
		snprintf(filename, sizeof(filename), "psp2core-%s.spsp2dmp.tmp",
			processPath);
	}
	return openDumpFileAtDestination(fd, filename, corefilePath);
}

SceUID openCorefile(void)
{
	SceUID fd;
	int result;

	result = _createCorefile(&fd);
	if (result == 0) {
		if (strncmp("host0:", corefilePath, 6) == 0)
			oalFileSetIoCacheSize(fd, 0x8000);
		result = fd;
	}
	return result;
}

int generatorBeginKernelDump(SceCoredumpTriggerParam param, SceSize payloadSize)
{
	union {
		Elf32_Ehdr elf;
		struct {
			CoredumpNoteHeader header;
			SceUInt32 reserved;
			SceSize payloadSize;
		} kernelNote;
	} buffer;
	SceUInt32 index, numPhdr;
	SceSize dataSize;
	SceOff fileOffset;
	int requestId, result;

	kernelDumpMemoryInitialized = 0;
	kernelDumpCafInitialized = 0;
	kernelDumpWritersInitialized = 0;
	kernelDumpPayloadSize = payloadSize;
	requestId = triggerGetNextRequestId();
	memset(&coredumpRun, 0, sizeof(coredumpRun));
	coredumpRun.pid = 65541;
	memcpy(&coredumpRun.triggerParamSize, &param, sizeof(param));
	plaintextDumpAllowed = oalIsPlaintextDumpAllowed(coredumpRun.pid);
	useAlternateCafSegment = oalIsSystemProgram(coredumpRun.pid) != 0 ||
		oalIsRootProgram(coredumpRun.pid) != 0;
	memset(corefilePath, 0, sizeof(corefilePath));
	result = oalMutexLock(generatorMutexId);
	if (result != 0)
		return result;
	cancelRequested = 0;
	activeRequestId = requestId;
	result = oalMutexUnlock(generatorMutexId);
	if (result != 0)
		return result;
	result = memMgrInit();
	if (result != 0)
		return result;
	kernelDumpMemoryInitialized = 1;
	if (plaintextDumpAllowed == 0) {
		result = oalCafInit();
		if (result != 0)
			return result;
		kernelDumpCafInitialized = 1;
	}
	if (plaintextDumpAllowed != 0)
		result = initWriters(openCorefile, 1, 0, 0);
	else
		result = initWriters(openCorefile, 1, 1, 2);
	if (result != 0)
		return result;
	for (index = 0; index < 5; ++index)
		kernelSegments[index].segment->methods = kernelSegments[index].methods;
	numAvailableSegments = 5;
	activeSegments = kernelSegments;
	kernelDumpWritersInitialized = 1;
	index = 0;
	do {
		if (segInit(activeSegments[index].segment) != 0)
			break;
		++index;
	} while (index < numAvailableSegments);
	for (index = 0; index < numAvailableSegments; ++index) {
		if (segCalcSize(activeSegments[index].segment) != 0)
			break;
		++numCalculatedSegments;
	}
	calculateCorefileSize();
	utilsElf32MakeEhdr(&buffer.elf, numProgramHeaders);
	writerWrite(currentWriter, &buffer.elf, sizeof(buffer.elf));
	fileOffset = programDataOffset;
	for (index = 0; index < numWritableSegments; ++index) {
		segGetSize(activeSegments[index].segment, &numPhdr, &dataSize);
		if (segWritePhdr(activeSegments[index].segment, fileOffset) != 0)
			break;
		fileOffset += dataSize;
		++numWrittenProgramHeaders;
	}
	writeSegments();
	kernelDumpWrittenSize = 0;
	utilsMakeNoteHeader(&buffer.kernelNote.header, "KERNEL_INFO", 0x3000,
		8, (kernelDumpPayloadSize + 43) & ~3u);
	buffer.kernelNote.reserved = 0;
	buffer.kernelNote.payloadSize = kernelDumpPayloadSize;
	result = writerWrite(currentWriter, &buffer.kernelNote,
		sizeof(buffer.kernelNote));
	if (result == 0) {
		kernelDumpWrittenSize += sizeof(buffer.kernelNote);
		result = requestId;
	}
	return result;
}

int generatorWriteKernelDump(const void *data, SceSize size)
{
	int result;

	result = writerWrite(currentWriter, data, size);
	if (result == 0)
		kernelDumpWrittenSize += size;
	return result;
}

int generatorEndKernelDump(void)
{
	CoredumpCdnAccessParam access;
	SceSize pathSize, finalPathSize;
	char *finalPath;
	int paddingResult, result, writerResult, renameResult;

	paddingResult = writerWritePaddingData(currentWriter,
		((kernelDumpWrittenSize + 3) & ~3u) - kernelDumpWrittenSize);
	if (kernelDumpWritersInitialized != 0) {
		writerResult = gzipWriterInitialized;
		if (gzipWriterInitialized != 0) {
			writerResult = gzipWriterFinal(&gzipWriter);
			gzipWriterInitialized = 0;
		}
		if (cafWriterInitialized != 0) {
			result = cafWriterEndSegment(&cafWriter);
			if (result != 0)
				writerResult = result;
			if (cafWriterGetNumSegments(&cafWriter) == 2) {
				result = cafWriterBeginSegment(&cafWriter,
					useAlternateCafSegment != 0 ? 4 : 3);
				if (result == 0) {
					oalGetCdnAccessParam(coredumpRun.appVersion,
						coredumpRun.stopReason, &access, &cdnLocation);
					result = writerWrite(&cafWriter.writer, &access,
						sizeof(access));
					if (result == 0)
						result = cafWriterEndSegment(&cafWriter);
				}
				if (result != 0)
					writerResult = result;
			}
			result = cafWriterFinal(&cafWriter);
			if (result != 0)
				writerResult = result;
			cafWriterInitialized = 0;
		}
		if (fileWriterInitialized != 0) {
			result = fileWriterFinal(&fileWriter);
			if (result < 0)
				writerResult = result;
			fileWriterInitialized = 0;
		}
		currentWriter = NULL;
		if (writerResult != 0)
			goto cleanup;
	}
	if (paddingResult != 0)
		goto cleanup;
	pathSize = strnlen(corefilePath, sizeof(corefilePath));
	if (pathSize <= 3)
		goto cleanup;
	finalPathSize = pathSize - 4;
	finalPath = memMgrLocalAlloc(pathSize - 3);
	if (finalPath == NULL)
		goto cleanup;
	memset(finalPath, 0, pathSize - 3);
	strncpy(finalPath, corefilePath, finalPathSize);
	oalFileRemove(finalPath);
	renameResult = oalFileRename(corefilePath, finalPath);
	corefilePath[finalPathSize] = 0;
	corefilePath[finalPathSize + 1] = 0;
	corefilePath[finalPathSize + 2] = 0;
	corefilePath[finalPathSize + 3] = 0;
	memMgrLocalFree(finalPath);
	if (renameResult == 0 && plaintextDumpAllowed == 0)
		writeCrashReport();
cleanup:
	if (plaintextDumpAllowed == 0 && kernelDumpCafInitialized != 0)
		oalCafFinal();
	if (kernelDumpMemoryInitialized != 0)
		memMgrFinal();
	return 0;
}
