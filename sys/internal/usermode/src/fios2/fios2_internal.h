/*
 * SceLibFios2: the FIOS2 runtime (the SDK's fios2.h interface): its scheduler, chunks,
 * I/O filters (the PSARC dearchiver and the cache), handle tables and the platform layer
 * over SceIofilemgr and SceThreadmgr. Sony built it with SNC in C++. Sony's names come from
 * the SDK headers and the library's messages; the rest are ours.
 */
#ifndef FIOS2_INTERNAL_H
#define FIOS2_INTERNAL_H

#define SCE_FIOS_LINKAGE
#include <scetypes.h>
#include <stdarg.h>
#include <time.h>
#include <rtc.h>
#include <kernel.h>
#include <fios2.h>
#include "wws_platform.h"

/*
 * The library's own string and memory routines: it defines sceClibMemcpy and sceClibMemset
 * itself (kernel/libkernel.h's prototypes) rather than importing SceLibKernel's, and the SDK's
 * deflate.o, which calls sceClibMemcpy, binds to its copy. SNC does not expand them inline.
 */
int fiosVsnprintf(char *dst, size_t size, const char *fmt, va_list ap);
int fiosSnprintf(char *dst, size_t size, const char *fmt, ...);

/* the parameters sceFiosInitialize was given */
extern SceFiosParams g_fiosParams;

/* a decompression job of the dearchiver's codecs */
#define FIOS_ZLIB_JOB_DONE	0x1
#define FIOS_ZLIB_JOB_VALID	0x4

typedef struct FiosZlibJob {
	struct FiosZlibJob *next;	/* in the codec's queue */
	uint32_t codec;			/* FIOS_CODEC_ZLIB */
	uint16_t flags;
	uint16_t reserved0;
	uint32_t reserved1;
	const void *pSrc;
	void *pDst;
	void *pCopyDst;			/* where the window of the output goes, or NULL */
	void (*pCallback)(struct FiosZlibJob *job);	/* NULL: the waiters are woken */
	uint32_t reserved2;
	uint32_t size;			/* the output buffer's size, then the output's */
	uint32_t copyOffset;		/* the window copied to pCopyDst */
	uint32_t copyLength;
	int result;
} FiosZlibJob;

/* a dearchiver codec */
#define FIOS_CODEC_ZLIB		0x7a6c6962	/* "zlib" */

typedef struct FiosCodec {
	int (*init)(uint8_t threadCount);
	void (*term)();
	void (*setThreadCount)(uint8_t threadCount);
	FiosZlibJob *(*submit)(FiosZlibJob *job);
	bool (*isDone)(FiosZlibJob *job);
	int (*wait)(FiosZlibJob *job, uint32_t *pSize);
	uint32_t codec;
	uint32_t maxInput;
	uint32_t maxOutput;
	uint32_t reserved0;
	uint32_t reserved1;
	uint32_t blockSize;
} FiosCodec;

extern FiosCodec g_fiosZlibCodec;

#endif
