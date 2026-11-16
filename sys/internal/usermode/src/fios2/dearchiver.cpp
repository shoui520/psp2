/*
 * dearchiver.cpp: the PSARC dearchiver (sceFiosIOFilterPsarcDearchiver) and the codecs it
 * decompresses with. The names are ours.
 */
#include <sce_atomic.h>
#include "fios2_internal.h"

static FiosCodec *volatile s_codecs[4];
static uint8_t s_threadCount;			/* the most decompressor threads */
static uint8_t s_decompressorThreadCount;

void fiosDearchiverSetThreadCount(uint8_t count)
{
	s_decompressorThreadCount = count;
	s_threadCount = count;
}

/* take the codec out of the registry */
static inline bool removeCodec(FiosCodec *pCodec)
{
	bool removed = false;
	int i;

	for (i = 0; i < 4 && !removed; i++)
		if ((FiosCodec *)sceAtomicCompareAndSwap32Relaxed((volatile int32_t *)&s_codecs[i],
								  (int32_t)pCodec, 0) == pCodec)
			removed = true;
	return removed;
}

__attribute__((noinline)) bool fiosDearchiverUnregisterCodec(FiosCodec *pCodec)
{
	bool removed = removeCodec(pCodec);

	if (removed && pCodec != NULL && pCodec->term != NULL)
		pCodec->term();
	return removed;
}

void fiosDearchiverTerm()
{
	int i;

	for (i = 0; i < 4; i++) {
		FiosCodec *pCodec = s_codecs[i];

		if (pCodec != NULL && fiosDearchiverUnregisterCodec(pCodec))
			sceFiosPrintf("WARNING: Unregistered codec during termination\n");
	}
	s_decompressorThreadCount = 0;
	s_threadCount = 0;
}

int fiosDearchiverRegisterCodec(FiosCodec *pCodec)
{
	bool added = false;
	int ret, i;

	for (i = 0; i < 4 && !added; i++)
		if (sceAtomicCompareAndSwap32Relaxed((volatile int32_t *)&s_codecs[i], 0,
						     (int32_t)pCodec) == 0)
			added = true;
	if (!added || pCodec == NULL || pCodec->init == NULL)
		return -1;
	ret = pCodec->init(s_threadCount);
	if (ret != 0)
		removeCodec(pCodec);
	return ret;
}

/* a codec of the kind that has the flags */
FiosCodec *fiosDearchiverFindCodec(uint32_t codec, uint16_t flags)
{
	int i;

	for (i = 0; i < 4; i++) {
		FiosCodec *pCodec = s_codecs[i];

		if (pCodec != NULL && pCodec->codec == codec && (pCodec->flags & flags) == flags)
			return pCodec;
	}
	return NULL;
}

uint8_t fiosDearchiverSetDecompressorThreadCount(uint8_t threadCount)
{
	int i;

	if (threadCount == 0)
		threadCount = 1;
	else if (threadCount > s_threadCount)
		threadCount = s_threadCount;
	for (i = 0; i < 4; i++) {
		FiosCodec *pCodec = s_codecs[i];

		if (pCodec != NULL && pCodec->setThreadCount != NULL)
			pCodec->setThreadCount(threadCount);
	}
	s_decompressorThreadCount = threadCount;
	return threadCount;
}

uint8_t fiosDearchiverGetDecompressorThreadCount()
{
	return s_decompressorThreadCount;
}
