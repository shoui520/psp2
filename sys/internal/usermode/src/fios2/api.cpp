/*
 * api.cpp: the FIOS2 API (the SDK's fios2_api.h): initialization, the default op
 * attributes, suspension, the handle and op calls, each of which builds an op and hands it
 * to the scheduler.
 */
#include <alloca.h>
#include <algorithm>
#include <sce_atomic.h>
#include "fios2_internal.h"

SceFiosParams g_FiosParams = SCE_FIOS_PARAMS_INITIALIZER;

Wws::Platform::Rwlock s_defaultOpAttrLock;
static SceFiosOpAttr s_globalDefaultOpAttr;
static volatile int s_suspendCount;
static bool s_autoInitialized;
bool s_libraryInitialized;
static bool s_libraryInitializedByFios;
static bool s_hasGlobalDefaultOpAttr;
Wws::Platform::Mutex s_chunkMutex;
Wws::Platform::Rwlock s_ioFilterLock;
Wws::Platform::Mutex s_dhMutex;

void fiosLibraryInit()
{
	SceSize count;

	if (s_libraryInitialized)
		return;
	s_defaultOpAttrLock.Init("SceFiosDefaultOpAttr");
	s_opWaitMutex.Init(0, "SceFiosOpWait");
	s_opWaitCond.Init(&s_opWaitMutex, "SceFiosOpWait");
	s_chunkMutex.Init(1, "SceFiosChunk");
	s_ioFilterLock.Init("SceFiosIOFilter");
	s_dhMutex.Init(0, "SceFiosDirHandle");
	s_overlayCount = 0;
	if (fiosPlatformOverlayGetList(sceKernelGetProcessId(), 0, 127, NULL, 0, &count) == 0)
		s_overlayCount = count;
	s_libraryInitialized = true;
}

void fiosLibraryTerm()
{
	if (!s_libraryInitialized)
		return;
	s_dhMutex.Term();
	s_ioFilterLock.Term();
	s_chunkMutex.Term();
	s_opWaitCond.Term();
	s_opWaitMutex.Term();
	s_defaultOpAttrLock.Term();
	s_libraryInitialized = false;
	s_libraryInitializedByFios = false;
}

int sceFiosPrintf(const char *pFormat, ...)
{
	va_list ap;

	va_start(ap, pFormat);
	return sceFiosVprintf(pFormat, ap);
}

__attribute__((noinline)) void sceFiosUpdateParameters(const SceFiosParams *pParameters)
{
	if (pParameters == NULL || pParameters->paramsSize != sizeof(SceFiosParams) ||
	    !g_FiosParams.initialized)
		return;
	g_FiosParams.pVprintf = pParameters->pVprintf;
	g_FiosParams.profiling = pParameters->profiling;
	g_FiosParams.pProfileCallback = pParameters->pProfileCallback;
	g_FiosParams.maxChunk = pParameters->maxChunk;
	g_FiosParams.pMemcpy = pParameters->pMemcpy != NULL ? pParameters->pMemcpy : sceClibMemcpy;
	for (int i = 0; i < SCE_FIOS_THREAD_TYPES; i++) {
		g_FiosParams.threadPriority[i] = pParameters->threadPriority[i];
		g_FiosParams.threadAffinity[i] = pParameters->threadAffinity[i];
	}
}

int sceFiosInitialize(const SceFiosParams *pParameters)
{
	int ret;

	if (pParameters == NULL) {
		sceFiosPrintf("sceFiosInitialize: NULL parameters.\n");
		return SCE_FIOS_ERROR_BAD_PTR;
	}
	if (g_FiosParams.initialized) {
		sceFiosPrintf("sceFiosInitialize: WARNING: FIOS has already been %sinitialized. "
			      "Updating parameters with new values.\n",
			      s_autoInitialized ? "auto-" : "");
		sceFiosUpdateParameters(pParameters);
		return SCE_FIOS_OK;
	}
	if (pParameters->paramsSize != sizeof(SceFiosParams)) {
		sceFiosPrintf("sceFiosInitialize: Unsupported paramsSize (%d, most recent is %d).\n",
			      pParameters->paramsSize, sizeof(SceFiosParams));
		return SCE_FIOS_ERROR_BAD_SIZE;
	}

	g_FiosParams = *pParameters;
	if (g_FiosParams.pMemcpy == NULL)
		g_FiosParams.pMemcpy = sceClibMemcpy;
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosInitialize");
	uint16_t pathMax = g_FiosParams.pathMax;

	if (pathMax == 0)
		pathMax = SCE_FIOS_PATH_DEFAULT;
	else if (pathMax > SCE_FIOS_PATH_MAX)
		pathMax = SCE_FIOS_PATH_MAX;
	g_FiosParams.pathMax = pathMax;
	if (g_FiosParams.threadPriority[SCE_FIOS_IO_THREAD] == -1)
		g_FiosParams.threadPriority[SCE_FIOS_IO_THREAD] = SCE_FIOS_IO_THREAD_DEFAULT_PRIORITY;
	if (g_FiosParams.threadPriority[SCE_FIOS_CALLBACK_THREAD] == -1)
		g_FiosParams.threadPriority[SCE_FIOS_CALLBACK_THREAD] =
			SCE_FIOS_CALLBACK_THREAD_DEFAULT_PRIORITY;
	if (g_FiosParams.threadPriority[SCE_FIOS_DECOMPRESSOR_THREAD] == -1)
		g_FiosParams.threadPriority[SCE_FIOS_DECOMPRESSOR_THREAD] =
			SCE_FIOS_DECOMPRESSOR_THREAD_DEFAULT_PRIORITY;
	if ((uint32_t)g_FiosParams.threadStackSize[SCE_FIOS_IO_THREAD] <
	    SCE_FIOS_IO_THREAD_DEFAULT_STACKSIZE)
		g_FiosParams.threadStackSize[SCE_FIOS_IO_THREAD] = SCE_FIOS_IO_THREAD_DEFAULT_STACKSIZE;
	if ((uint32_t)g_FiosParams.threadStackSize[SCE_FIOS_DECOMPRESSOR_THREAD] <
	    SCE_FIOS_DECOMPRESSOR_THREAD_DEFAULT_STACKSIZE)
		g_FiosParams.threadStackSize[SCE_FIOS_DECOMPRESSOR_THREAD] =
			SCE_FIOS_DECOMPRESSOR_THREAD_DEFAULT_STACKSIZE;
	if ((uint32_t)g_FiosParams.threadStackSize[SCE_FIOS_CALLBACK_THREAD] <
	    SCE_FIOS_CALLBACK_THREAD_DEFAULT_STACKSIZE)
		g_FiosParams.threadStackSize[SCE_FIOS_CALLBACK_THREAD] =
			SCE_FIOS_CALLBACK_THREAD_DEFAULT_STACKSIZE;
	if (g_FiosParams.ioThreadCount == 0)
		g_FiosParams.ioThreadCount = SCE_FIOS_IO_THREAD_COUNT_MIN;
	else if (g_FiosParams.ioThreadCount > SCE_FIOS_IO_THREAD_COUNT_MAX)
		g_FiosParams.ioThreadCount = SCE_FIOS_IO_THREAD_COUNT_MAX;
	if (g_FiosParams.threadsPerScheduler == 0)
		g_FiosParams.threadsPerScheduler = SCE_FIOS_SCHEDULER_THREAD_COUNT_MIN;
	else if (g_FiosParams.threadsPerScheduler > SCE_FIOS_SCHEDULER_THREAD_COUNT_MAX)
		g_FiosParams.threadsPerScheduler = SCE_FIOS_SCHEDULER_THREAD_COUNT_MAX;
	g_FiosParams.maxChunk = g_FiosParams.maxChunk != 0 ? g_FiosParams.maxChunk : SCE_FIOS_CHUNK_DEFAULT;
	if (g_FiosParams.maxDecompressorThreadCount == 0)
		g_FiosParams.maxDecompressorThreadCount = SCE_FIOS_DECOMPRESSOR_THREAD_COUNT_DEFAULT;
	else if (g_FiosParams.maxDecompressorThreadCount > SCE_FIOS_DECOMPRESSOR_THREAD_COUNT_MAX)
		g_FiosParams.maxDecompressorThreadCount = SCE_FIOS_DECOMPRESSOR_THREAD_COUNT_MAX;
	if (g_FiosParams.reserved2 != 0 || g_FiosParams.reserved3 != 0 ||
	    g_FiosParams.reserved4 != 0 || g_FiosParams.reserved5 != 0 ||
	    g_FiosParams.extraFlags != 0) {
		sceFiosPrintf("FIOS2 ERROR: A reserved, padding, or extraFlags field is not zero.\n");
		return SCE_FIOS_ERROR_BAD_SIZE;
	}

	if (!s_libraryInitialized) {
		s_libraryInitializedByFios = true;
		fiosLibraryInit();
	}
	ret = s_pOpTable->Init(g_FiosParams.opStorage, 8,
			       SCE_FIOS_ALIGN_UP(offsetof(FiosOp, path.path) + g_FiosParams.pathMax, 8));
	if (ret == SCE_FIOS_OK) {
		ret = s_pFHTable->Init(g_FiosParams.fhStorage, 8,
				       SCE_FIOS_ALIGN_UP(offsetof(FiosFH, path.path) + g_FiosParams.pathMax, 8));
		if (ret == SCE_FIOS_OK) {
			ret = s_pDHTable->Init(g_FiosParams.dhStorage, 8,
					       SCE_FIOS_ALIGN_UP(offsetof(FiosDH, path.path) +
								 g_FiosParams.pathMax, 8));
			if (ret == SCE_FIOS_OK) {
				ret = fiosChunkInit(g_FiosParams.chunkStorage);
				if (ret == SCE_FIOS_OK) {
					ret = fiosSchedulersInit();
					if (ret == SCE_FIOS_OK) {
						fiosDearchiverSetThreadCount(
							g_FiosParams.maxDecompressorThreadCount);
						g_FiosParams.initialized = 1;
						return SCE_FIOS_OK;
					}
					fiosChunkTerm();
				}
				s_pDHTable->Term();
			}
			s_pFHTable->Term();
		}
		s_pOpTable->Term();
	}
	if (s_libraryInitializedByFios)
		fiosLibraryTerm();
	return ret;
}

__attribute__((noinline)) void sceFiosTerminate()
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosTerminate");
	if (!g_FiosParams.initialized) {
		sceFiosPrintf("sceFiosTerminate: WARNING: FIOS is not initialized.\n");
		return;
	}
	int suspendCount = s_suspendCount;

	if (suspendCount != 0) {
		do {
			sceAtomicCompareAndSwap32Relaxed(&s_suspendCount, suspendCount, 0);
			suspendCount = s_suspendCount;
		} while (suspendCount != 0);
		fiosSchedulersWake();
	}
	fiosSchedulersTerm();
	s_autoInitialized = false;
	g_FiosParams.initialized = 0;
	fiosIOFiltersTerm();
	fiosDearchiverTerm();
	fiosOverlaysTerm(sceKernelGetProcessId());
	fiosChunkTerm();

	uint32_t count = s_pDHTable->GetAll(NULL, 0);
	SceFiosDH *pDHs = (SceFiosDH *)alloca(count * sizeof(SceFiosDH));

	s_pDHTable->GetAll(pDHs, count);
	for (uint32_t i = 0; i < count; i++) {
		FiosDH *pDH = (FiosDH *)s_pDHTable->Lookup(pDHs[i]);

		if (pDH != NULL && pDH->nativeDH != SCE_FIOS_NATIVEDH_INVALID) {
			fiosPlatformDclose(pDH->nativeDH);
			s_pDHTable->Free(pDHs[i]);
		}
	}
	s_pDHTable->Term();

	count = s_pFHTable->GetAll(NULL, 0);
	SceFiosFH *pFHs = (SceFiosFH *)alloca(count * sizeof(SceFiosFH));

	s_pFHTable->GetAll(pFHs, count);
	for (uint32_t i = 0; i < count; i++) {
		FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(pFHs[i]);

		if (pFH != NULL && pFH->nativeFH != SCE_FIOS_NATIVEFH_INVALID) {
			fiosPlatformClose(pFH->nativeFH);
			s_pFHTable->Free(pFHs[i]);
		}
	}
	s_pFHTable->Term();
	s_pOpTable->Term();
	if (s_libraryInitializedByFios)
		fiosLibraryTerm();
	g_FiosParams.pVprintf = NULL;
	g_FiosParams.pMemcpy = NULL;
	g_FiosParams.pProfileCallback = NULL;
	g_FiosParams.profiling = 0;
}

bool sceFiosIsInitialized(SceFiosParams *pOutParameters)
{
	bool initialized = g_FiosParams.initialized;

	if (initialized && pOutParameters != NULL)
		*pOutParameters = g_FiosParams;
	return initialized;
}

void sceFiosSetGlobalDefaultOpAttr(const SceFiosOpAttr *pAttr)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosSetGlobalDefaultOpAttr");
	s_defaultOpAttrLock.WriteLock();

	SceFiosOpAttr none = SCE_FIOS_OPATTR_INITIALIZER;

	if (pAttr != NULL && sceClibMemcmp(pAttr, &none, sizeof(SceFiosOpAttr)) != 0) {
		s_globalDefaultOpAttr = *pAttr;
		s_hasGlobalDefaultOpAttr = true;
	} else {
		s_globalDefaultOpAttr = none;
		s_hasGlobalDefaultOpAttr = false;
	}
	s_defaultOpAttrLock.WriteUnlock();
}

bool sceFiosGetGlobalDefaultOpAttr(SceFiosOpAttr *pOutAttr)
{
	bool has = s_hasGlobalDefaultOpAttr;

	if (has) {
		s_defaultOpAttrLock.ReadLock();
		*pOutAttr = s_globalDefaultOpAttr;
		s_defaultOpAttrLock.ReadUnlock();
	} else {
		SceFiosOpAttr none = SCE_FIOS_OPATTR_INITIALIZER;

		*pOutAttr = none;
	}
	return has;
}

void sceFiosGetDefaultOpAttr(SceFiosOpAttr *pOutAttr)
{
	if (s_hasGlobalDefaultOpAttr) {
		s_defaultOpAttrLock.ReadLock();
		*pOutAttr = s_globalDefaultOpAttr;
		s_defaultOpAttrLock.ReadUnlock();
	} else {
		SceFiosOpAttr none = SCE_FIOS_OPATTR_INITIALIZER;

		*pOutAttr = none;
	}
}

void sceFiosSuspend()
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosSuspend");
	sceAtomicIncrement32Relaxed(&s_suspendCount);
}

uint32_t sceFiosGetSuspendCount()
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosGetSuspendCount");
	return s_suspendCount;
}

bool sceFiosIsSuspended()
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosIsSuspended");
	return s_suspendCount > 0;
}

void sceFiosResume()
{
	int count;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosResume");
	for (count = s_suspendCount; count > 0; count = s_suspendCount) {
		if (sceAtomicCompareAndSwap32Relaxed(&s_suspendCount, count, count - 1) == count) {
			if (s_suspendCount == 0)
				fiosSchedulersWake();
			return;
		}
	}
}

__attribute__((noinline)) void sceFiosOpCancel(SceFiosOp op)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOpCancel");
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	if (pOp != NULL && !pOp->done && op != *fiosGetCallbackOp()) {
		uint16_t chunk = pOp->rootChunk;

		pOp->err = SCE_FIOS_ERROR_CANCELLED;
		pOp->internalFlags |= FIOS_OP_CANCELLED;
		fiosChunkCancel(chunk, true);
	}
}

__attribute__((noinline)) void sceFiosCancelAllOps()
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosCancelAllOps");

	uint32_t count = s_pOpTable->GetAll(NULL, 0);
	SceFiosOp *pOps = (SceFiosOp *)alloca(count * sizeof(SceFiosOp));

	s_pOpTable->GetAll(pOps, count);
	for (uint32_t i = 0; i < count; i++) {
		FiosOp *pOp = (FiosOp *)s_pOpTable->Lookup(pOps[i]);

		if (pOp != NULL && pOp->what != OP_FH_CLOSE && pOp->what != OP_DH_CLOSE)
			sceFiosOpCancel(pOps[i]);
	}
}

void sceFiosShutdownAndCancelOps()
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosShutdownAndCancelOps");
	sceFiosCancelAllOps();
	sceFiosTerminate();
}

void sceFiosCloseAllFiles()
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosCloseAllFiles");

	uint32_t count = s_pFHTable->GetAll(NULL, 0);
	SceFiosFH *pFHs = (SceFiosFH *)alloca(count * sizeof(SceFiosFH));

	s_pFHTable->GetAll(pFHs, count);
	for (uint32_t i = 0; i < count; i++)
		if (pFHs[i] != 0)
			sceFiosFHCloseSync(NULL, pFHs[i]);
}

bool sceFiosIsIdle()
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosIsIdle");
	return fiosSchedulersIsIdle();
}

/* each storage's use, as sceFiosStatisticsPrint shows it */
static inline void getStorageStatistics(FiosStorageStatistics *pStats, const Allocator *pAllocator)
{
	pStats->current = pAllocator->m_allocated;
	pStats->highWater = pAllocator->m_maxAllocated;
	pStats->maxAvailable = pAllocator->m_count;
	pStats->bytesPerUnit = pAllocator->m_unitSize;
}

void sceFiosStatisticsGet(FiosStatistics *pStats)
{
	if (pStats == NULL)
		return;
	sceClibMemset(pStats, 0, sizeof(*pStats));
	if (!g_FiosParams.initialized)
		return;
	getStorageStatistics(&pStats->op, &s_pOpTable->m_allocator);
	getStorageStatistics(&pStats->fh, &s_pFHTable->m_allocator);
	getStorageStatistics(&pStats->dh, &s_pDHTable->m_allocator);
	getStorageStatistics(&pStats->chunk, &s_chunkAllocator);
	fiosSchedulersGetStatistics(pStats);
}

void sceFiosStatisticsReset()
{
	if (!g_FiosParams.initialized)
		return;
	s_pOpTable->m_allocator.m_maxAllocated = s_pOpTable->m_allocator.m_allocated;
	s_pFHTable->m_allocator.m_maxAllocated = s_pFHTable->m_allocator.m_allocated;
	s_pDHTable->m_allocator.m_maxAllocated = s_pDHTable->m_allocator.m_allocated;
	s_chunkAllocator.m_maxAllocated = s_chunkAllocator.m_allocated;
	fiosSchedulersResetStatistics();
}

static inline void printStorageStatistics(const char *pName, const FiosStorageStatistics *pStats)
{
	sceFiosPrintf("  %18s    current   %5lld highWater  %7lld bytesPerUnit %4lld  max available %8lld\n",
		      pName, (uint64_t)pStats->current, (uint64_t)pStats->highWater,
		      (uint64_t)pStats->bytesPerUnit, (uint64_t)pStats->maxAvailable);
}

void sceFiosStatisticsPrint()
{
	FiosStatistics stats;
	char name[80];

	sceFiosStatisticsGet(&stats);
	sceFiosPrintf("Current FIOS statistics:\n");
	if (!g_FiosParams.initialized) {
		sceFiosPrintf("FIOS is not initialized\n");
		return;
	}
	printStorageStatistics("opStorage", &stats.op);
	printStorageStatistics("fhStorage", &stats.fh);
	printStorageStatistics("dhStorage", &stats.dh);
	printStorageStatistics("chunkStorage", &stats.chunk);
	for (uint32_t i = 0; i < g_FiosParams.ioThreadCount; i++) {
		if (i == 0)
			fiosSnprintf(name, sizeof(name), "hostfs IO queue");
		else if (i == g_FiosParams.ioThreadCount - 1)
			fiosSnprintf(name, sizeof(name), "All other IO queue");
		else
			fiosSnprintf(name, sizeof(name), "sched-fail-%d", i);
		printStorageStatistics(name, &stats.scheduler[i]);
	}
	printStorageStatistics("schedCallbackQueue", &stats.scheduler[2]);
}

size_t sceFiosGetAllFHs(SceFiosFH *pOutArray, size_t arraySize)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosGetAllFHs");
	return s_pFHTable->GetAll(pOutArray, arraySize);
}

size_t sceFiosGetAllDHs(SceFiosDH *pOutArray, size_t arraySize)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosGetAllDHs");
	return s_pDHTable->GetAll(pOutArray, arraySize);
}

size_t sceFiosGetAllOps(SceFiosOp *pOutArray, size_t arraySize)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosGetAllOps");
	return s_pOpTable->GetAll(pOutArray, arraySize);
}

int sceFiosPathcmp(const char *pA, const char *pB)
{
	return fiosStrcmp(pA, pB);
}

int sceFiosPathncmp(const char *pA, const char *pB, size_t n)
{
	return fiosStrncmp(pA, pB, n);
}

SceFiosOp sceFiosDHOpen(const SceFiosOpAttr *pAttr, SceFiosDH *pOutDH, const char *pPath,
			SceFiosBuffer buf)
{
	FiosOp *pOp;
	FiosDH *pDH;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDHOpen");
	pOp = fiosOpAllocate(pAttr, OP_DH_OPEN);
	if (pOp == NULL)
		return 0;
	if (pOutDH == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_PTR);
	*pOutDH = 0;
	if (buf.length != 0 && buf.pPtr == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_PTR);
	*pOutDH = pOp->fh = s_pDHTable->Allocate();
	if (pOp->fh == 0)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_CANT_ALLOCATE_DH);
	pDH = (FiosDH *)s_pDHTable->Lookup(pOp->fh);
	pDH->pBuf = buf.pPtr;
	pDH->nativeDH = SCE_FIOS_NATIVEDH_INVALID;
	pDH->bufSize = buf.length;
	pOp->internalFlags |= FIOS_OP_OWNS_HANDLE;
	return fiosOpSubmitPath(pOp, pPath, false);
}

SceFiosOp sceFiosDHRead(const SceFiosOpAttr *pAttr, SceFiosDH dh, SceFiosDirEntry *pOutEntry)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDHRead");
	pOp = fiosOpAllocate(pAttr, OP_DH_READ);
	if (pOp == NULL)
		return 0;
	if (s_pDHTable->Lookup(dh) == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_DH);
	if (pOutEntry == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_PTR);
	pOp->fh = dh;
	pOp->bufferCount = 1;
	pOp->buffers[0].pPtr = pOutEntry;
	pOp->buffers[0].length = sizeof(SceFiosDirEntry);
	return fiosOpSubmit(pOp);
}

SceFiosOp sceFiosDHClose(const SceFiosOpAttr *pAttr, SceFiosDH dh)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDHClose");
	pOp = fiosOpAllocate(pAttr, OP_DH_CLOSE);
	if (pOp == NULL)
		return 0;
	pOp->fh = dh;
	if (s_pDHTable->Lookup(dh) == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_DH);
	return fiosOpSubmit(pOp);
}

const char *sceFiosDHGetPath(SceFiosDH dh)
{
	FiosDH *pDH;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDHGetPath");
	pDH = (FiosDH *)s_pDHTable->Lookup(dh);
	if (pDH == NULL)
		return NULL;
	return fiosPathGet(&pDH->path);
}

SceFiosOp fiosOpSubmitPathWithMode(const SceFiosOpAttr *pAttr, const char *pPath, uint16_t what,
				   bool resolve, int32_t nativeMode)
{
	FiosOp *pOp = fiosOpAllocate(pAttr, what);

	if (pOp == NULL)
		return 0;
	pOp->nativeMode = nativeMode;
	return fiosOpSubmitPath(pOp, pPath, resolve);
}

SceFiosOp sceFiosDirectoryCreateWithMode(const SceFiosOpAttr *pAttr, const char *pPath,
					 int32_t nativeMode)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDirectoryCreateWithMode");
	return fiosOpSubmitPathWithMode(pAttr, pPath, OP_DIRECTORY_CREATE, true, nativeMode);
}

SceFiosOp sceFiosDirectoryCreate(const SceFiosOpAttr *pAttr, const char *pPath)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDirectoryCreate");
	return sceFiosDirectoryCreateWithMode(pAttr, pPath, -1);
}

SceFiosOp sceFiosDirectoryDelete(const SceFiosOpAttr *pAttr, const char *pPath)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDirectoryDelete");
	return fiosOpSubmitPathWithMode(pAttr, pPath, OP_DIRECTORY_DELETE, true, -1);
}

SceFiosOp fiosOpSubmitPathExists(const SceFiosOpAttr *pAttr, const char *pPath, bool *pOutExists,
				 uint16_t what)
{
	FiosOp *pOp = fiosOpAllocate(pAttr, what);

	if (pOp == NULL)
		return 0;
	if (pOutExists == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_PTR);
	pOp->buffers[0].pPtr = pOutExists;
	pOp->bufferCount = 1;
	pOp->buffers[0].length = sizeof(bool);
	return fiosOpSubmitPath(pOp, pPath, false);
}

SceFiosOp sceFiosDirectoryExists(const SceFiosOpAttr *pAttr, const char *pPath, bool *pOutExists)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDirectoryExists");
	return fiosOpSubmitPathExists(pAttr, pPath, pOutExists, OP_DIRECTORY_EXISTS);
}

SceFiosOp sceFiosFHOpenWithMode(const SceFiosOpAttr *pAttr, SceFiosFH *pOutFH, const char *pPath,
				const SceFiosOpenParams *pOpenParams, int32_t nativeMode)
{
	FiosOp *pOp;
	FiosFH *pFH;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHOpenWithMode");
	pOp = fiosOpAllocate(pAttr, OP_FH_OPEN);
	if (pOp == NULL)
		return 0;
	if (pOutFH == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_PTR);
	*pOutFH = 0;

	SceFiosOpenParams defaultParams = SCE_FIOS_OPENPARAMS_INITIALIZER;

	if (pOpenParams == NULL)
		pOpenParams = &defaultParams;
	if (pOpenParams->openFlags & ~(SCE_FIOS_O_RDWR | SCE_FIOS_O_APPEND | SCE_FIOS_O_CREAT |
				       SCE_FIOS_O_TRUNC | 0x1f00))
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_FLAGS);
	*pOutFH = pOp->fh = s_pFHTable->Allocate();
	if (pOp->fh == 0)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_CANT_ALLOCATE_FH);
	pFH = (FiosFH *)s_pFHTable->Lookup(pOp->fh);
	pFH->nativeFH = SCE_FIOS_NATIVEFH_INVALID;
	pFH->openParams = *pOpenParams;
	if ((pFH->openParams.openFlags & SCE_FIOS_O_RDWR) == 0)
		pFH->openParams.openFlags |= SCE_FIOS_O_READ;
	if (pAttr != NULL)
		pFH->openParams.opFlags = pAttr->opflags;
	pOp->nativeMode = nativeMode;
	pOp->internalFlags |= FIOS_OP_OWNS_HANDLE;
	return fiosOpSubmitPath(pOp, pPath, (pFH->openParams.openFlags & SCE_FIOS_O_CREAT) != 0);
}

SceFiosOp sceFiosFHOpen(const SceFiosOpAttr *pAttr, SceFiosFH *pOutFH, const char *pPath,
			const SceFiosOpenParams *pOpenParams)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHOpen");
	return sceFiosFHOpenWithMode(pAttr, pOutFH, pPath, pOpenParams, -1);
}

SceFiosOp sceFiosFHStat(const SceFiosOpAttr *pAttr, SceFiosFH fh, SceFiosStat *pOutStatus)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHStat");
	pOp = fiosOpAllocate(pAttr, OP_FH_STAT);
	if (pOp == NULL)
		return 0;
	pOp->fh = fh;
	pOp->bufferCount = 1;
	pOp->buffers[0].pPtr = pOutStatus;
	pOp->buffers[0].length = sizeof(SceFiosStat);
	if (pOutStatus == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_PTR);
	if (s_pFHTable->Lookup(fh) == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_FH);
	return fiosOpSubmit(pOp);
}

SceFiosOp sceFiosFHTruncate(const SceFiosOpAttr *pAttr, SceFiosFH fh, SceFiosSize length)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHTruncate");
	pOp = fiosOpAllocate(pAttr, OP_FH_TRUNCATE);
	if (pOp == NULL)
		return 0;
	pOp->fh = fh;
	pOp->length = length;
	if (s_pFHTable->Lookup(fh) == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_FH);
	return fiosOpSubmit(pOp);
}

SceFiosOp fiosOpSubmitFH(const SceFiosOpAttr *pAttr, SceFiosFH fh, uint16_t what)
{
	FiosOp *pOp = fiosOpAllocate(pAttr, what);

	if (pOp == NULL)
		return 0;
	pOp->fh = fh;
	if (s_pFHTable->Lookup(fh) == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_FH);
	return fiosOpSubmit(pOp);
}

SceFiosOp sceFiosFHSync(const SceFiosOpAttr *pAttr, SceFiosFH fh)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHSync");
	return fiosOpSubmitFH(pAttr, fh, OP_FH_SYNC);
}

SceFiosOp fiosOpSubmitFHIO(const SceFiosOpAttr *pAttr, SceFiosFH fh, const SceFiosBuffer *pIov,
			   int iovcnt, SceFiosOffset offset, FiosOpCommand what)
{
	FiosOp *pOp;
	FiosFH *pFH;

	pOp = fiosOpAllocate(pAttr, what);
	if (pOp == NULL)
		return 0;
	pFH = (FiosFH *)s_pFHTable->Lookup(fh);
	if (pFH == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_FH);
	if (iovcnt <= 0 || iovcnt > 8)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_IOVCNT);
	if (offset < 0)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_OFFSET);
	pOp->fh = fh;
	pOp->bufferCount = iovcnt;
	sceClibMemcpy(pOp->buffers, pIov, iovcnt * sizeof(SceFiosBuffer));
	pOp->offset = offset;
	if (what == OP_FH_READV || what == OP_FH_WRITEV || what == OP_FH_PWRITEV)
		pOp->internalFlags |= FIOS_OP_UPDATES_FH;
	if ((pOp->what == OP_PATH_WRITE || pOp->what == OP_FH_WRITEV ||
	     pOp->what == OP_FH_PWRITEV) && !(pFH->openParams.openFlags & SCE_FIOS_O_WRITE))
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_READ_ONLY);
	if ((pOp->what == OP_PATH_READ || pOp->what == OP_FH_READV ||
	     pOp->what == OP_FH_PREADV) && !(pFH->openParams.openFlags & SCE_FIOS_O_READ))
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_WRITE_ONLY);

	size_t total = 0;
	bool badPtr = false;

	for (int i = 0; i < iovcnt; i++) {
		total += pIov[i].length;
		badPtr |= pIov[i].pPtr == NULL;
	}
	if (total == 0)
		return fiosOpComplete(pOp);
	if (badPtr)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_PTR);
	return fiosOpSubmit(pOp);
}

SceFiosOffset fiosFHTell(SceFiosFH fh)
{
	FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(fh);

	return pFH != NULL ? pFH->offset : -1;
}

SceFiosOp sceFiosFHRead(const SceFiosOpAttr *pAttr, SceFiosFH fh, void *pBuf, SceFiosSize length)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHRead");

	SceFiosBuffer buf = { pBuf, (size_t)length };

	if (length >= 0 && length <= 0xFFFFFFFF)
		return fiosOpSubmitFHIO(pAttr, fh, &buf, 1, fiosFHTell(fh), OP_FH_READV);

	FiosOp *pOp = fiosOpAllocate(pAttr, OP_FH_READV);

	return pOp != NULL ? fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_SIZE) : 0;
}

SceFiosOp sceFiosFHWrite(const SceFiosOpAttr *pAttr, SceFiosFH fh, const void *pBuf,
			 SceFiosSize length)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHWrite");

	SceFiosBuffer buf = { (void *)pBuf, (size_t)length };

	if (length >= 0 && length <= 0xFFFFFFFF)
		return fiosOpSubmitFHIO(pAttr, fh, &buf, 1, fiosFHTell(fh), OP_FH_WRITEV);

	FiosOp *pOp = fiosOpAllocate(pAttr, OP_FH_WRITEV);

	return pOp != NULL ? fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_SIZE) : 0;
}

SceFiosOp sceFiosFHReadv(const SceFiosOpAttr *pAttr, SceFiosFH fh, const SceFiosBuffer iov[],
			 int iovcnt)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHReadv");
	return fiosOpSubmitFHIO(pAttr, fh, iov, iovcnt, fiosFHTell(fh), OP_FH_READV);
}

SceFiosOp sceFiosFHWritev(const SceFiosOpAttr *pAttr, SceFiosFH fh, const SceFiosBuffer iov[],
			  int iovcnt)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHWritev");
	return fiosOpSubmitFHIO(pAttr, fh, iov, iovcnt, fiosFHTell(fh), OP_FH_WRITEV);
}

SceFiosOp sceFiosFHPread(const SceFiosOpAttr *pAttr, SceFiosFH fh, void *pBuf, SceFiosSize length,
			 SceFiosOffset offset)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHPread");

	SceFiosBuffer buf = { pBuf, (size_t)length };

	if (length >= 0 && length <= 0xFFFFFFFF)
		return fiosOpSubmitFHIO(pAttr, fh, &buf, 1, offset, OP_FH_PREADV);

	FiosOp *pOp = fiosOpAllocate(pAttr, OP_FH_PREADV);

	return pOp != NULL ? fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_SIZE) : 0;
}

SceFiosOp sceFiosFHPwrite(const SceFiosOpAttr *pAttr, SceFiosFH fh, const void *pBuf,
			  SceFiosSize length, SceFiosOffset offset)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHPwrite");

	SceFiosBuffer buf = { (void *)pBuf, (size_t)length };

	if (length >= 0 && length <= 0xFFFFFFFF)
		return fiosOpSubmitFHIO(pAttr, fh, &buf, 1, offset, OP_FH_PWRITEV);

	FiosOp *pOp = fiosOpAllocate(pAttr, OP_FH_PWRITEV);

	return pOp != NULL ? fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_SIZE) : 0;
}

SceFiosOp sceFiosFHPreadv(const SceFiosOpAttr *pAttr, SceFiosFH fh, const SceFiosBuffer iov[],
			  int iovcnt, SceFiosOffset offset)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHPreadv");
	return fiosOpSubmitFHIO(pAttr, fh, iov, iovcnt, offset, OP_FH_PREADV);
}

SceFiosOp sceFiosFHPwritev(const SceFiosOpAttr *pAttr, SceFiosFH fh, const SceFiosBuffer iov[],
			   int iovcnt, SceFiosOffset offset)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHPwritev");
	return fiosOpSubmitFHIO(pAttr, fh, iov, iovcnt, offset, OP_FH_PWRITEV);
}

SceFiosOp sceFiosFHClose(const SceFiosOpAttr *pAttr, SceFiosFH fh)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHClose");
	return fiosOpSubmitFH(pAttr, fh, OP_FH_CLOSE);
}

const char *sceFiosFHGetPath(SceFiosFH fh)
{
	FiosFH *pFH;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHGetPath");
	pFH = (FiosFH *)s_pFHTable->Lookup(fh);
	if (pFH == NULL)
		return NULL;
	return fiosPathGet(&pFH->path);
}

SceFiosSize sceFiosFHGetSize(SceFiosFH fh)
{
	FiosFH *pFH;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHGetSize");
	pFH = (FiosFH *)s_pFHTable->Lookup(fh);
	return pFH != NULL ? pFH->size : -1;
}

SceFiosOpenParams *sceFiosFHGetOpenParams(SceFiosFH fh)
{
	FiosFH *pFH;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHGetOpenParams");
	pFH = (FiosFH *)s_pFHTable->Lookup(fh);
	return pFH != NULL ? &pFH->openParams : NULL;
}

SceFiosOp sceFiosFileExists(const SceFiosOpAttr *pAttr, const char *pPath, bool *pOutExists)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFileExists");
	return fiosOpSubmitPathExists(pAttr, pPath, pOutExists, OP_FILE_EXISTS);
}

SceFiosOp sceFiosFileGetSize(const SceFiosOpAttr *pAttr, const char *pPath, SceFiosSize *pOutSize)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFileGetSize");
	pOp = fiosOpAllocate(pAttr, OP_FILE_GET_SIZE);
	if (pOp == NULL)
		return 0;
	if (pOutSize == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_PTR);
	pOp->buffers[0].pPtr = pOutSize;
	pOp->bufferCount = 1;
	pOp->buffers[0].length = sizeof(SceFiosSize);
	return fiosOpSubmitPath(pOp, pPath, false);
}

SceFiosOp sceFiosFileDelete(const SceFiosOpAttr *pAttr, const char *pPath)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFileDelete");
	return fiosOpSubmitPathWithMode(pAttr, pPath, OP_FILE_DELETE, true, -1);
}

SceFiosOp sceFiosExists(const SceFiosOpAttr *pAttr, const char *pPath, bool *pOutExists)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosExists");
	return fiosOpSubmitPathExists(pAttr, pPath, pOutExists, OP_EXISTS);
}

SceFiosOp sceFiosStat(const SceFiosOpAttr *pAttr, const char *pPath, SceFiosStat *pOutStatus)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosStat");
	pOp = fiosOpAllocate(pAttr, OP_STAT);
	if (pOp == NULL)
		return 0;
	pOp->buffers[0].pPtr = pOutStatus;
	pOp->bufferCount = 1;
	pOp->buffers[0].length = sizeof(SceFiosStat);
	if (pOutStatus == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_PTR);
	return fiosOpSubmitPath(pOp, pPath, false);
}

SceFiosOp sceFiosDelete(const SceFiosOpAttr *pAttr, const char *pPath)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDelete");
	return fiosOpSubmitPathWithMode(pAttr, pPath, OP_DELETE, true, -1);
}

SceFiosOp sceFiosResolve(const SceFiosOpAttr *pAttr, const SceFiosTuple *pInTuple,
			 SceFiosTuple *pOutTuple)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosResolve");
	pOp = fiosOpAllocate(pAttr, OP_RESOLVE);
	if (pOp == NULL)
		return 0;
	if (pInTuple == NULL || pOutTuple == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_PTR);
	if (pOutTuple != pInTuple) {
		pOutTuple->offset = pInTuple->offset;
		pOutTuple->size = pInTuple->size;
		fiosStrlcpy(pOutTuple->path, pInTuple->path, g_FiosParams.pathMax);
	}
	pOp->offset = pInTuple->offset;
	pOp->length = pInTuple->size;
	pOp->bufferCount = 1;
	pOp->buffers[0].pPtr = pOutTuple;
	pOp->buffers[0].length = sizeof(SceFiosTuple);
	return fiosOpSubmitPath(pOp, pInTuple->path, false);
}

SceFiosOp sceFiosRename(const SceFiosOpAttr *pAttr, const char *pOldPath, const char *pNewPath)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosRename");
	pOp = fiosOpAllocate(pAttr, OP_RENAME);
	if (pOp == NULL)
		return 0;
	if (pOldPath == NULL || pNewPath == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_PATH);
	if (fiosStrcmp(pOldPath, pNewPath) == 0)
		return fiosOpComplete(pOp);
	pOp->pPath2 = pNewPath;
	pOp->bufferCount = 1;
	return fiosOpSubmitPath(pOp, pOldPath, false);
}

/* a path op's private file handle, opened with these flags */
static inline SceFiosOp submitPathIO(FiosOp *pOp, const char *pPath, uint16_t openFlags,
				     bool resolve)
{
	SceFiosFH fh = s_pFHTable->Allocate();
	FiosFH *pFH;

	pOp->fh = fh;
	if (fh == 0)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_CANT_ALLOCATE_FH);
	pOp->internalFlags |= FIOS_OP_PRIVATE_FH;
	pFH = (FiosFH *)s_pFHTable->Lookup(fh);

	SceFiosOpenParams params = { openFlags, 0, 0, SCE_FIOS_BUFFER_INITIALIZER };

	pFH->openParams = params;
	pFH->nativeFH = SCE_FIOS_NATIVEFH_INVALID;
	pOp->internalFlags |= FIOS_OP_OWNS_HANDLE;
	return fiosOpSubmitPath(pOp, pPath, resolve);
}

SceFiosOp sceFiosFileRead(const SceFiosOpAttr *pAttr, const char *pPath, void *pBuf,
			  SceFiosSize length, SceFiosOffset offset)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFileRead");
	pOp = fiosOpAllocate(pAttr, OP_PATH_READ);
	if (pOp == NULL)
		return 0;
	if (length < 0 || length > 0xFFFFFFFF)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_SIZE);
	if (offset < 0)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_OFFSET);
	if (pBuf == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_PTR);
	pOp->bufferCount = 1;
	pOp->buffers[0].pPtr = pBuf;
	pOp->offset = offset;
	pOp->buffers[0].length = std::max<SceFiosSize>(std::min<SceFiosSize>(length, 0xFFFFFFFF), 0);
	return submitPathIO(pOp, pPath, SCE_FIOS_O_READ, false);
}

SceFiosOp sceFiosFileWrite(const SceFiosOpAttr *pAttr, const char *pPath, const void *pBuf,
			   SceFiosSize length, SceFiosOffset offset)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFileWrite");
	pOp = fiosOpAllocate(pAttr, OP_PATH_WRITE);
	if (pOp == NULL)
		return 0;
	if (length < 0 || length > 0xFFFFFFFF)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_SIZE);
	if (offset < 0)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_OFFSET);
	if (pBuf == NULL)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_PTR);
	pOp->buffers[0].pPtr = (void *)pBuf;
	pOp->bufferCount = 1;
	pOp->offset = offset;
	pOp->buffers[0].length = std::max<SceFiosSize>(std::min<SceFiosSize>(length, 0xFFFFFFFF), 0);
	return submitPathIO(pOp, pPath, SCE_FIOS_O_WRITE | SCE_FIOS_O_CREAT, true);
}

SceFiosOp sceFiosFileTruncate(const SceFiosOpAttr *pAttr, const char *pPath, SceFiosSize length)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFileTruncate");
	pOp = fiosOpAllocate(pAttr, OP_TRUNCATE);
	if (pOp == NULL)
		return 0;
	pOp->length = length;
	return fiosOpSubmitPath(pOp, pPath, true);
}

SceFiosOp sceFiosCachePrefetchFile(const SceFiosOpAttr *pAttr, const char *pPath)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosCachePrefetchFile");
	pOp = fiosOpAllocate(pAttr, OP_PREFETCH_FILE);
	if (pOp == NULL)
		return 0;
	pOp->fh = s_pFHTable->Allocate();
	if (pOp->fh == 0)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_CANT_ALLOCATE_FH);
	pOp->internalFlags |= FIOS_OP_PRIVATE_FH;
	return fiosOpSubmitPath(pOp, pPath, false);
}

SceFiosOp sceFiosCachePrefetchFileRange(const SceFiosOpAttr *pAttr, const char *pPath,
					SceFiosOffset offset, SceFiosSize length)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosCachePrefetchFileRange");
	pOp = fiosOpAllocate(pAttr, OP_PREFETCH_FILE_RANGE);
	if (pOp == NULL)
		return 0;
	pOp->offset = offset;
	pOp->length = length;
	pOp->fh = s_pFHTable->Allocate();
	if (pOp->fh == 0)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_CANT_ALLOCATE_FH);
	pOp->internalFlags |= FIOS_OP_PRIVATE_FH;
	return fiosOpSubmitPath(pOp, pPath, false);
}

SceFiosOp sceFiosCachePrefetchFH(const SceFiosOpAttr *pAttr, SceFiosFH fh)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosCachePrefetchFH");
	return fiosOpSubmitFH(pAttr, fh, OP_PREFETCH_FH);
}

SceFiosOp sceFiosCachePrefetchFHRange(const SceFiosOpAttr *pAttr, SceFiosFH fh,
				      SceFiosOffset offset, SceFiosSize length)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosCachePrefetchFHRange");
	pOp = fiosOpAllocate(pAttr, OP_PREFETCH_FH_RANGE);
	if (pOp == NULL)
		return 0;
	pOp->fh = fh;
	pOp->offset = offset;
	pOp->length = length;
	return fiosOpSubmit(pOp);
}

void sceFiosCacheFlushSync()
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosCacheFlushSync");
}

void sceFiosCacheFlushFileRangeSync(const char *pPath, SceFiosOffset offset, SceFiosSize length)
{
	FiosCacheRange range;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosCacheFlushFileRangeSync");
	range.pPath = pPath;
	range.offset = offset;
	range.length = length;
	fiosIOFiltersCall(FIOS_FILTER_CACHE_FLUSH, &range);
}

void sceFiosCacheFlushFileSync(const char *pPath)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosCacheFlushFileSync");
	sceFiosCacheFlushFileRangeSync(pPath, 0, SCE_FIOS_OFFSET_MAX);
}

int sceFiosCacheContainsFileRangeSync(const char *pPath, SceFiosOffset offset, SceFiosSize length)
{
	FiosCacheRange range;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosCacheContainsFileRangeSync");
	range.pPath = pPath;
	range.offset = offset;
	range.length = length;
	return fiosIOFiltersCall(FIOS_FILTER_CACHE_CONTAINS, &range);
}

int sceFiosCacheContainsFileSync(const char *pPath, SceFiosSize *pOutSize)
{
	SceFiosSize size;
	int ret;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosCacheContainsFileSync");
	if (pOutSize != NULL)
		*pOutSize = 0;
	size = sceFiosFileGetSizeSync(NULL, pPath);
	if (size < 0)
		return size;
	ret = sceFiosCacheContainsFileRangeSync(pPath, 0, size);
	if (ret >= 0 && pOutSize != NULL)
		*pOutSize = size;
	return ret;
}

/* an op is done once its callback has run, or while its own callback runs */
static inline bool isDone(SceFiosOp op, const FiosOp *pOp)
{
	return pOp->done || op == *fiosGetCallbackOp();
}

bool sceFiosOpIsDone(SceFiosOp op)
{
	FiosOp *pOp = (FiosOp *)s_pOpTable->Lookup(op);

	return pOp == NULL || isDone(op, pOp);
}

int sceFiosOpWait(SceFiosOp op)
{
	int err = SCE_FIOS_ERROR_BAD_OP;
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOpWait");
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	if (pOp != NULL) {
		if (isDone(op, pOp)) {
			__dmb();
			err = pOp->err;
		} else {
			err = fiosOpWait(pOp);
		}
	}
	return err;
}

int sceFiosOpWaitUntil(SceFiosOp op, SceFiosTime deadline)
{
	int err = SCE_FIOS_ERROR_BAD_OP;
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOpWaitUntil");
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	if (pOp != NULL) {
		if (isDone(op, pOp)) {
			__dmb();
			err = pOp->err;
		} else {
			err = fiosOpWaitUntil(pOp, deadline);
		}
	}
	return err;
}

void fiosOpDelete(SceFiosOp op, FiosOp *pOp)
{
	if (pOp != NULL && !pOp->deleted) {
		pOp->deleted = true;
		fiosOpCallback(pOp, SCE_FIOS_OPEVENT_DELETE);
		s_pOpTable->Free(op);
	}
}

void sceFiosOpDelete(SceFiosOp op)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOpDelete");
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	if (pOp == NULL) {
		sceFiosPrintf("sceFiosOpDelete: called with an invalid op 0x%08X.\n", op);
		return;
	}
	if (!isDone(op, pOp))
		fiosOpWait(pOp);
	fiosOpDelete(op, pOp);
}

int sceFiosOpSyncWait(SceFiosOp op)
{
	int err;
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOpSyncWait");
	if (op == 0)
		return SCE_FIOS_ERROR_CANT_ALLOCATE_OP;
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	if (pOp == NULL)
		return SCE_FIOS_ERROR_BAD_OP;
	if (isDone(op, pOp)) {
		__dmb();
		err = pOp->err;
	} else {
		err = fiosOpWait(pOp);
	}
	fiosOpDelete(op, pOp);
	return err;
}

SceFiosSize sceFiosOpSyncWaitForIO(SceFiosOp op)
{
	FiosOp *pOp;
	int err = SCE_FIOS_ERROR_BAD_OP;
	SceFiosSize actCount;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOpSyncWaitForIO");
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	if (pOp != NULL) {
		if (isDone(op, pOp)) {
			__dmb();
			err = pOp->err;
		} else {
			err = fiosOpWait(pOp);
		}
		actCount = pOp->actCount;
		fiosOpDelete(op, pOp);
		if (err == SCE_FIOS_OK)
			return actCount;
	}
	if (err == SCE_FIOS_ERROR_EOF)
		return actCount;
	return err;
}

int sceFiosOpGetError(SceFiosOp op)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOpGetError");
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	if (pOp == NULL)
		return SCE_FIOS_ERROR_BAD_OP;
	if (isDone(op, pOp))
		return pOp->err;
	return SCE_FIOS_IN_PROGRESS;
}

bool sceFiosOpIsCancelled(SceFiosOp op)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOpIsCancelled");
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	return pOp != NULL && (pOp->internalFlags & FIOS_OP_CANCELLED) != 0;
}

const SceFiosOpAttr *sceFiosOpGetAttr(SceFiosOp op)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOpGetAttr");
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	return pOp != NULL ? &pOp->attr : NULL;
}

const char *sceFiosOpGetPath(SceFiosOp op)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOpGetPath");
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	return pOp != NULL ? fiosPathGet(&pOp->path) : NULL;
}

void *sceFiosOpGetBuffer(SceFiosOp op)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOpGetBuffer");
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	return pOp != NULL ? pOp->buffers : NULL;
}

SceFiosOffset sceFiosOpGetOffset(SceFiosOp op)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOpGetOffset");
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	return pOp != NULL ? pOp->offset : 0;
}

SceFiosSize sceFiosOpGetRequestCount(SceFiosOp op)
{
	FiosOp *pOp;
	SceFiosSize count = 0;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOpGetRequestCount");
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	if (pOp == NULL)
		return 0;
	for (int i = 0; i < 8; i++)
		count += pOp->buffers[i].length;
	return count;
}

SceFiosSize sceFiosOpGetActualCount(SceFiosOp op)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOpGetActualCount");
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	return pOp != NULL ? pOp->actCount : 0;
}

void sceFiosOpRescheduleWithPriority(SceFiosOp op, SceFiosTime newDeadline,
				     SceFiosPriority newPriority)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOpRescheduleWithPriority");
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	if (pOp != NULL && !isDone(op, pOp) && pOp->attr.deadline != newDeadline) {
		fiosChunkCancel(pOp->rootChunk, false);
		fiosChunkReschedule(pOp->rootChunk, newDeadline, newPriority);
		fiosScheduleTree(pOp->rootChunk, 0, false);
	}
}

void sceFiosOpReschedule(SceFiosOp op, SceFiosTime newDeadline)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOpReschedule");
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	if (pOp != NULL)
		sceFiosOpRescheduleWithPriority(op, newDeadline, pOp->attr.priority);
}

int sceFiosOverlayAdd(const SceFiosOverlay *pOverlay, SceFiosOverlayID *pOutID)
{
	SceFiosOverlay overlay;
	int ret;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOverlayAdd");
	ret = fiosOverlayCopy(pOverlay, &overlay);
	if (ret == SCE_FIOS_OK) {
		ret = fiosPlatformOverlayAdd(sceKernelGetProcessId(), &overlay, pOutID);
		if (ret == SCE_FIOS_OK)
			sceAtomicIncrement32Relaxed(&s_overlayCount);
	}
	return ret;
}

/* an overlay this process added itself, at a user order */
static inline bool isOwnOverlay(const FiosProcessOverlay *pOverlay)
{
	SceUID pid = sceKernelGetProcessId();

	return pOverlay->order <= SCE_FIOS_OVERLAY_ORDER_USER_LAST &&
	       pid == sceKernelGetProcessId() && pOverlay->pid == pid;
}

int sceFiosOverlayRemove(SceFiosOverlayID id)
{
	FiosProcessOverlay overlay;
	int ret;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOverlayRemove");
	ret = fiosPlatformOverlayGetInfo(sceKernelGetProcessId(), id, (SceFiosOverlay *)&overlay);
	if (ret != SCE_FIOS_OK)
		return ret;
	if (!isOwnOverlay(&overlay))
		return SCE_FIOS_ERROR_BAD_OVERLAY;
	ret = fiosPlatformOverlayRemove(sceKernelGetProcessId(), id);
	if (ret == SCE_FIOS_OK)
		sceAtomicDecrement32Relaxed(&s_overlayCount);
	return ret;
}

int sceFiosOverlayGetInfo(SceFiosOverlayID id, SceFiosOverlay *pOutOverlay)
{
	FiosProcessOverlay overlay;
	int ret;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOverlayGetInfo");
	if (pOutOverlay == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	ret = fiosPlatformOverlayGetInfo(sceKernelGetProcessId(), id, (SceFiosOverlay *)&overlay);
	if (ret != SCE_FIOS_OK)
		return ret;
	if (!isOwnOverlay(&overlay))
		return SCE_FIOS_ERROR_BAD_OVERLAY;
	sceClibMemset(pOutOverlay, 0, sizeof(SceFiosOverlay));
	pOutOverlay->type = overlay.type;
	pOutOverlay->order = overlay.order;
	pOutOverlay->id = overlay.id;
	sceClibMemcpy(pOutOverlay->dst, overlay.dst, overlay.dstLength);
	sceClibMemcpy(pOutOverlay->src, overlay.src, overlay.srcLength);
	return ret;
}

int sceFiosOverlayModify(SceFiosOverlayID id, const SceFiosOverlay *pNewValue)
{
	FiosProcessOverlay overlay;
	int ret;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOverlayModify");
	if (pNewValue == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	ret = fiosPlatformOverlayGetInfo(sceKernelGetProcessId(), id, (SceFiosOverlay *)&overlay);
	if (ret != SCE_FIOS_OK)
		return ret;
	if (!isOwnOverlay(&overlay))
		return SCE_FIOS_ERROR_BAD_OVERLAY;
	ret = fiosOverlayCopy(pNewValue, (SceFiosOverlay *)&overlay);
	if (ret != SCE_FIOS_OK)
		return ret;
	return fiosPlatformOverlayModify(sceKernelGetProcessId(), id, (SceFiosOverlay *)&overlay);
}

int sceFiosOverlayGetList(SceFiosOverlayID *pOutIDs, size_t maxIDs, size_t *pActualIDs)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOverlayGetList");
	return fiosPlatformOverlayGetList(sceKernelGetProcessId(), 0, SCE_FIOS_OVERLAY_ORDER_USER_LAST,
					  pOutIDs, maxIDs, pActualIDs);
}

int sceFiosOverlayResolveSync(int resolveFlag, const char *pInPath, char *pOutPath, size_t maxPath)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosOverlayResolveSync");
	return fiosPlatformOverlayResolve(sceKernelGetProcessId(), resolveFlag, pInPath, pOutPath,
					  maxPath, 0, SCE_FIOS_OVERLAY_ORDER_USER_LAST);
}

int sceFiosIOFilterAdd(int index, SceFiosIOFilterCallback pFilterCallback, void *pFilterContext)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosIOFilterAdd");
	return fiosIOFilterAdd(index, pFilterCallback, pFilterContext);
}

int sceFiosIOFilterGetInfo(int index, SceFiosIOFilterCallback *pOutFilterCallback,
			   void **pOutFilterContext)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosIOFilterGetInfo");
	return fiosIOFilterGetInfo(index, pOutFilterCallback, pOutFilterContext);
}

int sceFiosIOFilterRemove(int index)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosIOFilterRemove");
	return fiosIOFilterRemove(index);
}

SceFiosOp sceFiosArchiveGetMountBufferSize(const SceFiosOpAttr *pAttr, const char *pArchivePath,
					   const SceFiosOpenParams *pOpenParams)
{
	FiosOp *pOp;
	FiosFH *pFH;
	SceFiosFH fh;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosArchiveGetMountBufferSize");
	pOp = fiosOpAllocate(pAttr, OP_ARCHIVE_QUERY);
	if (pOp == NULL)
		return 0;
	if (pOpenParams != NULL && (pOpenParams->openFlags & SCE_FIOS_O_WRITE))
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_READ_ONLY);
	pOp->fh = fh = s_pFHTable->Allocate();
	if (fh == 0)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_CANT_ALLOCATE_FH);
	pOp->internalFlags |= FIOS_OP_OWNS_HANDLE;
	pFH = (FiosFH *)s_pFHTable->Lookup(fh);
	pFH->nativeFH = SCE_FIOS_NATIVEFH_INVALID;

	SceFiosOpenParams defaultParams = SCE_FIOS_OPENPARAMS_INITIALIZER;

	if (pOpenParams == NULL)
		pOpenParams = &defaultParams;
	pFH->openParams = *pOpenParams;
	if ((pFH->openParams.openFlags & SCE_FIOS_O_RDWR) == 0)
		pFH->openParams.openFlags |= SCE_FIOS_O_READ;
	return fiosOpSubmitPath(pOp, pArchivePath, false);
}

SceFiosOp sceFiosArchiveMount(const SceFiosOpAttr *pAttr, SceFiosFH *pOutFH,
			      const char *pArchivePath, const char *pMountPoint,
			      SceFiosBuffer mountBuffer, const SceFiosOpenParams *pOpenParams)
{
	FiosOp *pOp;
	FiosFH *pFH;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosArchiveMount");
	pOp = fiosOpAllocate(pAttr, OP_ARCHIVE_MOUNT);
	if (pOp == NULL)
		return 0;
	if (pOutFH == NULL || pMountPoint == NULL ||
	    (mountBuffer.length != 0 && mountBuffer.pPtr == NULL))
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_BAD_PTR);
	if (pOpenParams != NULL && (pOpenParams->openFlags & SCE_FIOS_O_WRITE))
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_READ_ONLY);
	*pOutFH = pOp->fh = s_pFHTable->Allocate();
	if (pOp->fh == 0)
		return fiosOpCompleteWithError(pOp, SCE_FIOS_ERROR_CANT_ALLOCATE_FH);
	pOp->internalFlags |= FIOS_OP_OWNS_HANDLE;
	pFH = (FiosFH *)s_pFHTable->Lookup(pOp->fh);
	pFH->nativeFH = SCE_FIOS_NATIVEFH_INVALID;

	SceFiosOpenParams defaultParams = SCE_FIOS_OPENPARAMS_INITIALIZER;

	if (pOpenParams == NULL)
		pOpenParams = &defaultParams;
	pFH->openParams = *pOpenParams;
	if ((pFH->openParams.openFlags & SCE_FIOS_O_RDWR) == 0)
		pFH->openParams.openFlags |= SCE_FIOS_O_READ;
	pOp->buffers[0].pPtr = mountBuffer.pPtr;
	pOp->bufferCount = 1;
	pOp->buffers[0].length = mountBuffer.length;
	pOp->pPath2 = pMountPoint;
	return fiosOpSubmitPath(pOp, pArchivePath, false);
}

SceFiosOp sceFiosArchiveUnmount(const SceFiosOpAttr *pAttr, SceFiosFH fh)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosArchiveUnmount");
	return fiosOpSubmitFH(pAttr, fh, OP_ARCHIVE_UNMOUNT);
}

uint8_t sceFiosArchiveSetDecompressorThreadCount(uint8_t threadCount)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosArchiveSetDecompressorThreadCount");
	return fiosDearchiverSetDecompressorThreadCount(threadCount);
}

uint8_t sceFiosArchiveGetDecompressorThreadCount()
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosArchiveGetDecompressorThreadCount");
	return fiosDearchiverGetDecompressorThreadCount();
}

SceFiosOp sceFiosChangeStat(const SceFiosOpAttr *pAttr, const char *pPath, SceFiosStat *pStatus,
			    int cbit)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosChangeStat");
	pOp = fiosOpAllocate(pAttr, OP_CH_STAT);
	if (pOp == NULL)
		return 0;
	pOp->cbit = cbit;
	pOp->bufferCount = 1;
	pOp->buffers[0].pPtr = pStatus;
	pOp->buffers[0].length = sizeof(SceFiosStat);
	return fiosOpSubmitPath(pOp, pPath, true);
}

SceFiosOp sceFiosDevctl(const SceFiosOpAttr *pAttr, const char *pPath, int cmd, void *pArg,
			size_t arglen, void *pBuffer, size_t buflen)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosDevctl");
	pOp = fiosOpAllocate(pAttr, OP_DEVCTL);
	if (pOp == NULL)
		return 0;
	pOp->cmd = cmd;
	pOp->bufferCount = 2;
	pOp->buffers[0].pPtr = pArg;
	pOp->buffers[0].length = arglen;
	pOp->buffers[1].pPtr = pBuffer;
	pOp->buffers[1].length = buflen;
	return fiosOpSubmitPath(pOp, pPath, true);
}

SceFiosOp sceFiosFHIoctl(const SceFiosOpAttr *pAttr, const SceFiosFH fh, int cmd, void *pArg,
			 size_t arglen, void *pBuffer, size_t buflen)
{
	FiosOp *pOp;

	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosFHIoctl");
	pOp = fiosOpAllocate(pAttr, OP_IOCTL);
	if (pOp == NULL)
		return 0;
	pOp->fh = fh;
	pOp->cmd = cmd;
	pOp->bufferCount = 2;
	pOp->buffers[0].pPtr = pArg;
	pOp->buffers[0].length = arglen;
	pOp->buffers[1].pPtr = pBuffer;
	pOp->buffers[1].length = buflen;
	return fiosOpSubmit(pOp);
}

SceFiosOp sceFiosSync(const SceFiosOpAttr *pAttr, const char *pPath, int flag)
{
	FIOS_TRACE(SCE_FIOS_PROFILE_API, "api", "%-15s ", "sceFiosSync");
	return fiosOpSubmitPathWithMode(pAttr, pPath, OP_SYNC, true, flag);
}
