/*
 * zlib_codec.cpp: the PSARC dearchiver's zlib codec. Decompression jobs queue up for a pool
 * of up to SCE_FIOS_DECOMPRESSOR_THREAD_COUNT_MAX threads ("SceFiosLibdeflt<n>"), each
 * inflating with libSceDeflt and, where the caller asked for a window of the output in
 * another buffer, copying that window out.
 */
#include <algorithm>
#include <libdeflt.h>
#include "fios2_internal.h"

using namespace Wws::Platform;

static int zlibInit(uint8_t threadCount);
static void zlibTerm();
static void zlibSetThreadCount(uint8_t threadCount);
static FiosZlibJob *zlibSubmit(FiosZlibJob *job);
static bool zlibIsDone(FiosZlibJob *job);
static int zlibWait(FiosZlibJob *job, uint32_t *pSize);
static void zlibThread(uintptr_t index);

FiosCodec g_fiosZlibCodec = {
	zlibInit,
	zlibTerm,
	zlibSetThreadCount,
	zlibSubmit,
	zlibIsDone,
	zlibWait,
	FIOS_CODEC_ZLIB,
	0x7fffffff,
	0x7fffffff,
	0,
	0,
	0x40000,
};

static Mutex s_mutex = { {}, -1, 0 };
static Thread s_threads[SCE_FIOS_DECOMPRESSOR_THREAD_COUNT_MAX] = {
	{ -1, NULL, 0 }, { -1, NULL, 0 }, { -1, NULL, 0 } };
static Cond s_workerCond[SCE_FIOS_DECOMPRESSOR_THREAD_COUNT_MAX];
static Cond s_doneCond;
static FiosZlibJob *s_queue;

static uint8_t s_maxThreadCount;
static uint8_t s_threadCount;
static int s_initCount;
static uint8_t s_busy[SCE_FIOS_DECOMPRESSOR_THREAD_COUNT_MAX];
static uint8_t s_shutdown;

static int zlibInit(uint8_t threadCount)
{
	int err = 0;
	int i, j;

	if (s_initCount++ == 0) {
		ThreadParams params = { 0, 0, 0, 0, "" };

		params.stackSize = g_FiosParams.threadStackSize[SCE_FIOS_DECOMPRESSOR_THREAD];
		params.priority = g_FiosParams.threadPriority[SCE_FIOS_DECOMPRESSOR_THREAD];
		params.cpuAffinityMask = g_FiosParams.threadAffinity[SCE_FIOS_DECOMPRESSOR_THREAD];
		if (threadCount <= 0 || threadCount > SCE_FIOS_DECOMPRESSOR_THREAD_COUNT_MAX)
			threadCount = SCE_FIOS_DECOMPRESSOR_THREAD_COUNT_MAX;
		s_threadCount = threadCount;
		s_maxThreadCount = threadCount;
		s_shutdown = 0;
		s_mutex.Init(0, "SceFiosZlib");
		s_doneCond.Init(&s_mutex, "SceFiosZlibWaiter");
		for (i = 0; i < s_maxThreadCount; i++) {
			fiosSnprintf(params.name, sizeof(params.name), "SceFiosLibdeflt%d", i);
			s_workerCond[i].Init(&s_mutex, params.name);
			err = s_threads[i].Create(&params, zlibThread, i);
			if (err != 0) {
				s_initCount--;
				s_mutex.Lock();
				s_shutdown = 1;
				for (j = i - 1; j >= 0; j--)
					s_workerCond[j].Broadcast();
				s_mutex.Unlock();
				for (j = i - 1; j >= 0; j--)
					s_threads[j].Join();
				for (j = i; j >= 0; j--)
					s_workerCond[j].Term();
				s_doneCond.Term();
				s_mutex.Term();
				break;
			}
		}
	}
	return err;
}

static void zlibTerm()
{
	unsigned int i;

	if (--s_initCount != 0)
		return;
	s_mutex.Lock();
	s_shutdown = 1;
	for (i = 0; i < s_maxThreadCount; i++)
		s_workerCond[i].Broadcast();
	s_mutex.Unlock();
	for (i = 0; i < s_maxThreadCount; i++) {
		s_threads[i].Join();
		s_workerCond[i].Term();
	}
	s_doneCond.Term();
	s_mutex.Term();
}

static void zlibSetThreadCount(uint8_t threadCount)
{
	if (threadCount <= 0 || threadCount > s_maxThreadCount)
		threadCount = s_maxThreadCount;
	s_threadCount = threadCount;
}

static FiosZlibJob *zlibSubmit(FiosZlibJob *job)
{
	FiosZlibJob **pp;
	unsigned int i;

	if (!(job->flags & FIOS_ZLIB_JOB_VALID))
		return NULL;
	job->flags &= ~FIOS_ZLIB_JOB_DONE;
	job->result = -1;
	s_mutex.Lock();
	for (pp = &s_queue; *pp != NULL; pp = &(*pp)->next)
		;
	*pp = job;
	for (i = 0; i < s_threadCount; i++) {
		if (!s_busy[i]) {
			s_workerCond[i].Signal();
			s_busy[i] = 1;
			break;
		}
	}
	s_mutex.Unlock();
	return job;
}

static bool zlibIsDone(FiosZlibJob *job)
{
	return job == NULL || (job->flags & FIOS_ZLIB_JOB_DONE);
}

static int zlibWait(FiosZlibJob *job, uint32_t *pSize)
{
	if (!zlibIsDone(job)) {
		s_mutex.Lock();
		while (!zlibIsDone(job))
			s_doneCond.Wait();
		s_mutex.Unlock();
	}
	if (job == NULL)
		return SCE_DEFLATE_ERROR_INVALID_POINTER;
	if (pSize != NULL)
		*pSize = job->size;
	return job->result;
}

static void zlibThread(uintptr_t index)
{
	FiosZlibJob *job;
	unsigned int adler;
	uint32_t start, end;

	s_mutex.Lock();
	for (;;) {
		while (((uintptr_t)s_queue | s_shutdown) == 0) {
			s_busy[index] = 0;
			s_workerCond[index].Wait();
		}
		s_busy[index] = 1;
		job = s_queue;
		if (job == NULL) {
			if (s_shutdown)
				break;
			continue;
		}
		s_queue = job->next;
		s_mutex.Unlock();

		if (job->codec == FIOS_CODEC_ZLIB) {
			adler = 0;
			job->result = sceZlibDecompress(job->pDst, job->size, job->pSrc, &adler);
			if (job->result >= 0) {
				job->size = job->result;
				job->result = 0;
				if (job->pCopyDst != NULL && job->pDst != job->pCopyDst) {
					start = std::min(job->copyOffset, job->size);
					end = job->copyOffset + job->copyLength;
					end = std::min(end, job->size);
					sceClibMemcpy(job->pCopyDst, (uint8_t *)job->pDst + start, end - start);
				}
			}
		} else {
			job->result = SCE_DEFLATE_ERROR_NOT_SUPPORTED;
		}

		s_mutex.Lock();
		job->flags |= FIOS_ZLIB_JOB_DONE;
		if (job->pCallback != NULL)
			job->pCallback(job);
		else
			s_doneCond.Broadcast();
	}
	s_mutex.Unlock();
}
