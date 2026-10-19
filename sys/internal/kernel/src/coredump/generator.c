/* SceCoredump writer stack and dump generator. */

#include "writer_internal.h"
#include "seg_internal.h"
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <coredump/elf.h>
#include <threadmgr/threadmgr.h>
#include <string.h>
#include <sysmem/sysclib.h>
#include <stdio.h>

typedef struct CoredumpCancelContext {
	int requestId;
	int numCanceled;
} CoredumpCancelContext;

_Static_assert(sizeof(CoredumpCancelContext) == 8, "cancel context size");

typedef struct CoredumpCorefileLayout {
	SceUInt32 numProgramHeaders;
	SceSize totalFileSize;
	SceUInt64 programDataOffset;
	SceSize processedFileSize;
	SceUInt32 reservedSegmentCount;
} CoredumpCorefileLayout;

_Static_assert(sizeof(CoredumpCorefileLayout) == 24, "corefile layout size");

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
SceUID generatorMutexId;
int shutdownRequested;
int cancelRequested;
SceUInt32 numAvailableSegments;
CoredumpCorefileLayout corefileLayout;
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

int _getExitFlag(SceSize processedSize);
extern SceSize encryptedScreenshotSize;
extern SceUID openCorefile(void);

/* the segments of a kernel dump and of a process dump, in writing order */
extern SegIf corefileSegment;
extern SegIf systemSegment;
extern SegIf hardwareSegment;
extern SegIf buildVersionSegment;
extern SegIf crashdumpSegment;
extern SegIf threadSegment;
extern SegIf semaphoreSegment;
extern SegIf eventFlagSegment;
extern SegIf mutexSegment;
extern SegIf lwMutexSegment;
extern SegIf msgPipeSegment;
extern SegIf callbackSegment;
extern SegIf timerSegment;
extern SegIf rwLockSegment;
extern SegIf condSegment;
extern SegIf lwCondSegment;
extern SegIf simpleEventSegment;
extern SegIf threadRegSegment;
extern ProcessSegment processSegment;
extern SegIf appSegment;
extern SegIf stackSegment;
extern SegIf moduleSegment;
extern SegIf libSegment;
extern SegIf metaDataSegment;
extern SegIf procMemMiniSegment;
extern SegIf procMemFullSegment;
extern SegIf memBlockSegment;
extern FileSegIf fileSegment;
extern SegIf fiberSegment;
extern SegIf ultSegment;
extern SegIf extnlProcSegment;
extern SegIf budgetSegment;
extern SegIf appListSegment;
extern SegIf deviceSegment;
extern SegIf systemDeviceSegment;
extern SegIf userDataSegment;
extern SegIf ultSemaSegment;
extern SegIf ultMutexSegment;
extern SegIf ultCondSegment;
extern SegIf ultRwlockSegment;
extern SegIf ultQueueSegment;
extern SegIf ultQueueDataPoolSegment;
extern SegIf ultWaitingQueuePoolSegment;
extern SegIf ttySegment;
extern SegIf screenshotSegment;
extern SegIf eventLogSegment;
extern SegIf encryptedScreenshotSegment;
extern GpuSegIf gpuSegment;
extern SegIf gpuActivitySegment;
extern SegIf tty2Segment;
extern SummarySegIf summarySegment;
extern const SegIfMethod corefileMethods;
extern const SegIfMethod systemMethods;
extern const SegIfMethod hardwareMethods;
extern const SegIfMethod buildVersionMethods;
extern const SegIfMethod crashdumpMethods;
extern const SegIfMethod threadMethods;
extern const SegIfMethod semaphoreMethods;
extern const SegIfMethod eventFlagMethods;
extern const SegIfMethod mutexMethods;
extern const SegIfMethod lwMutexMethods;
extern const SegIfMethod msgPipeMethods;
extern const SegIfMethod callbackMethods;
extern const SegIfMethod timerMethods;
extern const SegIfMethod rwLockMethods;
extern const SegIfMethod condMethods;
extern const SegIfMethod lwCondMethods;
extern const SegIfMethod simpleEventMethods;
extern const SegIfMethod threadRegMethods;
extern const SegIfMethod processMethods;
extern const SegIfMethod appMethods;
extern const SegIfMethod stackMethods;
extern const SegIfMethod moduleMethods;
extern const SegIfMethod libMethods;
extern const SegIfMethod metaDataMethods;
extern const SegIfMethod procMemMiniMethods;
extern const SegIfMethod procMemFullMethods;
extern const SegIfMethod memBlockMethods;
extern const SegIfMethod fileMethods;
extern const SegIfMethod fiberMethods;
extern const SegIfMethod ultMethods;
extern const SegIfMethod extnlProcMethods;
extern const SegIfMethod budgetMethods;
extern const SegIfMethod appListMethods;
extern const SegIfMethod deviceMethods;
extern const SegIfMethod systemDeviceMethods;
extern const SegIfMethod userDataMethods;
extern const SegIfMethod ultSemaMethods;
extern const SegIfMethod ultMutexMethods;
extern const SegIfMethod ultCondMethods;
extern const SegIfMethod ultRwlockMethods;
extern const SegIfMethod ultQueueMethods;
extern const SegIfMethod ultQueueDataPoolMethods;
extern const SegIfMethod ultWaitingQueuePoolMethods;
extern const SegIfMethod ttyMethods;
extern const SegIfMethod screenshotMethods;
extern const SegIfMethod eventLogMethods;
extern const SegIfMethod encryptedScreenshotMethods;
extern const SegIfMethod gpuMethods;
extern const SegIfMethod gpuActivityMethods;
extern const SegIfMethod tty2Methods;
extern const SegIfMethod summaryMethods;

const SegIfMethodPair kernelSegments[5] = {
	{ &corefileSegment, &corefileMethods, 0x1000 },
	{ &systemSegment, &systemMethods, 0x1001 },
	{ &hardwareSegment, &hardwareMethods, 0x101a },
	{ &buildVersionSegment, &buildVersionMethods, 0x101d },
	{ &crashdumpSegment, &crashdumpMethods, 0x3000 },
};

const SegIfMethodPair registeredSegments[50] = {
	{ &corefileSegment, &corefileMethods, 0x1000 },
	{ &threadSegment, &threadMethods, 0x1003 },
	{ &semaphoreSegment, &semaphoreMethods, 0x100a },
	{ &eventFlagSegment, &eventFlagMethods, 0x100b },
	{ &mutexSegment, &mutexMethods, 0x100c },
	{ &lwMutexSegment, &lwMutexMethods, 0x100d },
	{ &msgPipeSegment, &msgPipeMethods, 0x1010 },
	{ &callbackSegment, &callbackMethods, 0x1011 },
	{ &timerSegment, &timerMethods, 0x1013 },
	{ &rwLockSegment, &rwLockMethods, 0x1014 },
	{ &condSegment, &condMethods, 0x1015 },
	{ &lwCondSegment, &lwCondMethods, 0x1016 },
	{ &simpleEventSegment, &simpleEventMethods, 0x1030 },
	{ &threadRegSegment, &threadRegMethods, 0x1004 },
	{ &processSegment.segment, &processMethods, 0x1002 },
	{ &systemSegment, &systemMethods, 0x1001 },
	{ &appSegment, &appMethods, 0x101c },
	{ &hardwareSegment, &hardwareMethods, 0x101a },
	{ &buildVersionSegment, &buildVersionMethods, 0x101d },
	{ &stackSegment, &stackMethods, 0x101b },
	{ &moduleSegment, &moduleMethods, 0x1005 },
	{ &libSegment, &libMethods, 0x1006 },
	{ &metaDataSegment, &metaDataMethods, 0x1019 },
	{ &procMemMiniSegment, &procMemMiniMethods, 0x4002 },
	{ &procMemFullSegment, &procMemFullMethods, 0x4003 },
	{ &memBlockSegment, &memBlockMethods, 0x1007 },
	{ &fileSegment.base, &fileMethods, 0x1009 },
	{ &fiberSegment, &fiberMethods, 0x1017 },
	{ &ultSegment, &ultMethods, 0x1018 },
	{ &extnlProcSegment, &extnlProcMethods, 0x101e },
	{ &budgetSegment, &budgetMethods, 0x101f },
	{ &appListSegment, &appListMethods, 0x1020 },
	{ &deviceSegment, &deviceMethods, 0x1021 },
	{ &systemDeviceSegment, &systemDeviceMethods, 0x102f },
	{ &userDataSegment, &userDataMethods, 0x1022 },
	{ &ultSemaSegment, &ultSemaMethods, 0x1023 },
	{ &ultMutexSegment, &ultMutexMethods, 0x1024 },
	{ &ultCondSegment, &ultCondMethods, 0x1029 },
	{ &ultRwlockSegment, &ultRwlockMethods, 0x1028 },
	{ &ultQueueSegment, &ultQueueMethods, 0x1027 },
	{ &ultQueueDataPoolSegment, &ultQueueDataPoolMethods, 0x1025 },
	{ &ultWaitingQueuePoolSegment, &ultWaitingQueuePoolMethods, 0x1026 },
	{ &ttySegment, &ttyMethods, 0x102a },
	{ &screenshotSegment, &screenshotMethods, 0x102b },
	{ &eventLogSegment, &eventLogMethods, 0x102c },
	{ &encryptedScreenshotSegment, &encryptedScreenshotMethods, 0x102d },
	{ &gpuSegment.base, &gpuMethods, 0x2000 },
	{ &gpuActivitySegment, &gpuActivityMethods, 0x2001 },
	{ &tty2Segment, &tty2Methods, 0x1031 },
	{ &summarySegment.base, &summaryMethods, 0x102e },
};

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
	if (coredumpRun.trigger.triggerReason == -2) {
		result = oalResumeProcess(coredumpRun.pid);
		if (result != 0)
			return result;
	} else if (coredumpRun.trigger.triggerReason != -1) {
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
		total = corefileLayout.totalFileSize;
		processed = corefileLayout.processedFileSize + processedSize;
		oldPercent = (SceUInt64)100 * corefileLayout.processedFileSize / total;
		corefileLayout.processedFileSize = processed;
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

	corefileLayout.numProgramHeaders = 0;
	corefileLayout.processedFileSize = 0;
	corefileLayout.totalFileSize = 52;
	corefileLayout.programDataOffset = 52;
	corefileLayout.reservedSegmentCount = 0;
	numWritableSegments = 0;
	corefileSizeLimit = ~(SceUInt64)0;
	for (index = 0; index < numAvailableSegments; ++index) {
		segGetSize(activeSegments[index].segment, &numPhdr, &dataSize);
		phdrSize = 32 * numPhdr;
		nextSize = corefileLayout.totalFileSize + dataSize + phdrSize;
		if (corefileSizeLimit < (SceUInt64)nextSize)
			break;
		corefileLayout.totalFileSize = nextSize;
		corefileLayout.numProgramHeaders += numPhdr;
		corefileLayout.programDataOffset += phdrSize;
		numWritableSegments++;
	}
	return 0;
}

static inline int finalWriters(void)
{
	CoredumpCdnAccessParam access;
	int result, finalResult;

	result = 0;
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
				oalGetCdnAccessParam(coredumpRun.trigger.appVersion,
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
	return result;
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
		payloadResult = writerWrite(currentWriter, coredumpRun.trigger.titleid,
			coredumpRun.trigger.titleid_len);
		if (payloadResult == 0)
			payloadResult = writerWritePaddingData(currentWriter,
				16 - ((coredumpRun.trigger.titleid_len + pathSize) & 15));
	}
	result = finalWriters();
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
	CoredumpCompletionCallback completionCallback;
	uint32_t kind;
	int requestId;
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
		kind = event.kind;
		if (event.trigger.titleid != NULL && event.trigger.titleid_len != 0)
			SCE_PRINTF_WITH_INFO(15, 773, "[coredump] start ID=%s\n",
				event.trigger.titleid);
		else
			SCE_PRINTF_WITH_INFO(15, 775, "[coredump] start ID=Unknown\n");
		memoryInitialized = 0;
		cafInitialized = 0;
		if (kind > 1) {
			if (kind == 2)
				break;
			result = (int)0x80010016;
			goto restorePriority;
		}
		if (memoryFailed != 0) {
			result = (int)0x8001000c;
			goto restorePriority;
		}
		coredumpRun.pid = event.pid;
		coredumpRun.trigger = event.trigger;
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
		stopEvent.threadId = coredumpRun.trigger.crash_thid;
		stopEvent.reason = coredumpRun.trigger.triggerReason;
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
		corefileLayout.numProgramHeaders = 0;
		corefileLayout.totalFileSize = 0;
		corefileLayout.programDataOffset = 0;
		corefileLayout.processedFileSize = 0;
		corefileLayout.reservedSegmentCount = 0;
		numWritableSegments = 0;
		numAvailableSegments = 50;
		for (index = 0; index < numAvailableSegments; ++index) {
			payloadResult = segInit(activeSegments[index].segment);
			if (payloadResult != 0)
				goto endDump;
		}
		for (index = 0; index < numAvailableSegments; ++index) {
			payloadResult = segCalcSize(activeSegments[index].segment);
			if (payloadResult != 0)
				goto endDump;
			numCalculatedSegments++;
		}
		calculateCorefileSize();
		utilsElf32MakeEhdr(&buffer.elfHeader, corefileLayout.numProgramHeaders);
		payloadResult = writerWrite(currentWriter, &buffer.elfHeader,
			sizeof(buffer.elfHeader));
		if (payloadResult == 0) {
			fileOffset = corefileLayout.programDataOffset;
			for (index = 0; index < numWritableSegments; ++index) {
				segGetSize(activeSegments[index].segment, &numPhdr, &dataSize);
				payloadResult = segWritePhdr(activeSegments[index].segment,
					fileOffset);
				if (payloadResult != 0)
					goto endDump;
				numWrittenProgramHeaders++;
				fileOffset += dataSize;
			}
			payloadResult = writeSegments();
		}
endDump:
		oalChangeThreadPriority(handlerThreadId, 30);
		result = finalWriters();
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
		__builtin_memset(&cdnLocation, 0, sizeof(cdnLocation));
		removeEncryptedScreenshot();
		coredumpMemoryInit();
		completionCallback = event.completionCallback;
		if (finalResult == (int)0x8001008c)
			SCE_PRINTF_WITH_INFO(15, 1017, "[coredump] canceled\n");
		else if (finalResult == 0)
			SCE_PRINTF_WITH_INFO(15, 1019, "[coredump] done\n");
		else
			SCE_PRINTF_WITH_INFO(15, 1015, "[coredump] error:0x%08x\n",
				finalResult);
		requestId = activeRequestId;
		pathSize = strnlen(corefilePath, 1329) + 1;
		completionCallback(requestId, coredumpRun.pid, finalResult,
			corefilePath, pathSize,
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
	const void *triggerData;
	SceSize triggerDataSize;
	SceUInt32 triggerDataType;

	triggerDataType = coredumpRun.trigger.triggerDataType;
	if (triggerDataType == 0) {
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
	} else if (triggerDataType == 1) {
		snprintf(path, 1330, "%s%s", "host0:", filename);
		return oalFileOpen(path, fd);
	} else if (triggerDataType != 2) {
		if (triggerDataType != 10)
			return (int)0x80010016;
		triggerDataSize = coredumpRun.trigger.triggerDataSize;
		triggerData = coredumpRun.trigger.triggerData;
		destination = path + 6;
		__builtin_memcpy(path, "host0:", 6);
		if (triggerDataSize != 0) {
			memcpy(destination, triggerData, triggerDataSize);
			snprintf(destination + triggerDataSize - 1,
				1324 - triggerDataSize, "/%s", filename);
		} else {
			snprintf(destination, 1324, "%s", filename);
		}
		return oalFileOpen(path, fd);
	}
	/* type 2, and type 0 when neither host0: nor sd0: opens: internal storage */
	snprintf(path, 1330, "%s%s", "ux0:data/", filename);
	return oalFileOpen(path, fd);
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
		if (coredumpRun.trigger.triggerFlags != 0 && coredumpRun.trigger.triggerReason == 1) {
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
	} else if (coredumpRun.trigger.triggerFlags != 0 && coredumpRun.trigger.triggerReason == 1) {
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
	coredumpRun.trigger = param;
	plaintextDumpAllowed = oalIsPlaintextDumpAllowed(coredumpRun.pid);
	useAlternateCafSegment = oalIsSystemProgram(coredumpRun.pid) != 0 ||
		oalIsRootProgram(coredumpRun.pid) != 0;
	memset(corefilePath, 0, sizeof(corefilePath));
	result = oalMutexLock(generatorMutexId);
	if (result != 0)
		return result;
	activeRequestId = requestId;
	cancelRequested = 0;
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
	utilsElf32MakeEhdr(&buffer.elf, corefileLayout.numProgramHeaders);
	writerWrite(currentWriter, &buffer.elf, sizeof(buffer.elf));
	fileOffset = corefileLayout.programDataOffset;
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
	SceSize pathSize, finalPathSize, index;
	char *finalPath;
	int paddingResult, result, writerResult, renameResult;

	paddingResult = writerWritePaddingData(currentWriter,
		((kernelDumpWrittenSize + 3) & ~3u) - kernelDumpWrittenSize);
	if (kernelDumpWritersInitialized != 0) {
		writerResult = finalWriters();
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
	for (index = 0; index < 4; ++index)
		corefilePath[finalPathSize + index] = 0;
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
