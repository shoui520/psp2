/*
 * SceRazorCapture: program cache, buffered file writer, host tool comms,
 * mapped range list, and user marker string buffers.
 */

#include "razor_capture_internal.h"

/* Host tool communication buffers */
typedef struct RazorHostTriggerBuf {
	uint32_t pending;
	uint32_t reserved;
	char filename[512];
} RazorHostTriggerBuf;

typedef struct RazorHostInfo {
	uint32_t version;
	uint32_t size;
	void (*markerFunc)(void);
	void (*hookStart)(void);
	void (*hookComplete)(void);
	RazorHostTriggerBuf *triggerBuf;
	uint32_t triggerBufSize;
	uint64_t *statusBuf;
	uint32_t statusBufSize;
} RazorHostInfo;

static uint64_t s_hostStatus;
static RazorHostTriggerBuf s_hostTrigger;

RazorHostInfo g_razorCaptureHostInfo = {
	0,
	sizeof(RazorHostInfo),
	razorCaptureHostMarker,
	razorCaptureHostHookStart,
	razorCaptureHostHookComplete,
	&s_hostTrigger,
	sizeof(s_hostTrigger),
	&s_hostStatus,
	sizeof(s_hostStatus),
};

/* User marker buffers (2 contexts, 20 KiB each) */
typedef struct RazorMarkerBuf {
	uint32_t contextId;
	char *start;
	char *cur;
	char *end;
} RazorMarkerBuf;

static RazorMarkerBuf s_markerBufs[2];
static uint8_t s_markerInitialized;
static char s_markerStorage[2][0x5000];

/* --- 32-entry ring set for deduplicating GXM shader programs --- */

RazorProgramCache *razorProgramCacheInit(RazorProgramCache *cache)
{
	cache->nextIdx = 0;
	cache->count = 0;
	return cache;
}

void razorProgramCacheNoop(void)
{
}

RazorProgramCache *razorProgramCacheReset(RazorProgramCache *cache)
{
	cache->nextIdx = 0;
	cache->count = 0;
	return cache;
}

RazorProgramCache *razorProgramCacheInsert(RazorProgramCache *cache, uint32_t ptr, uint8_t *alreadyPresent)
{
	int32_t i;
	int32_t count;

	*alreadyPresent = 0;
	count = cache->count;
	for (i = 0; i < count; i++) {
		if (cache->entries[i] == ptr) {
			*alreadyPresent = 1;
			return cache;
		}
	}

	cache->entries[cache->nextIdx] = ptr;
	cache->nextIdx = (cache->nextIdx + 1) % 32;
	if (count < 32)
		cache->count = count + 1;
	return cache;
}

/* --- Buffered sceIo file writer --- */

RazorFileWriter *razorFileWriterInit(RazorFileWriter *writer)
{
	writer->fd = -1;
	writer->used = 0;
	return writer;
}

int razorFileWriterOpen(RazorFileWriter *writer, const char *path)
{
	SceUID fd;

	writer->used = 0;
	fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0600);
	writer->fd = fd;
	if (fd < 0) {
		sceClibPrintf("File open error code: 0x%x\n", fd);
		return 1;
	}
	return 0;
}

int razorFileWriterClose(RazorFileWriter *writer)
{
	int res;

	if (writer->fd >= 0) {
		if (writer->used == 0 || sceIoWrite(writer->fd, writer->buffer, writer->used) >= 0)
			writer->used = 0;
	}
	res = sceIoClose(writer->fd);
	writer->fd = -1;
	return res;
}

int razorFileWriterFlush(RazorFileWriter *writer)
{
	if (writer->fd < 0)
		return 1;
	if (writer->used != 0) {
		if (sceIoWrite(writer->fd, writer->buffer, writer->used) < 0)
			return 1;
	}
	writer->used = 0;
	return 0;
}

int razorFileWriterWrite(RazorFileWriter *writer, const void *buf, SceSize size)
{
	if (writer->fd < 0)
		return 1;

	if (writer->used + size <= sizeof(writer->buffer)) {
		sceClibMemcpy_safe(writer->buffer + writer->used, buf, size);
		writer->used += size;
		return 0;
	}

	if (writer->used != 0) {
		if (sceIoWrite(writer->fd, writer->buffer, writer->used) < 0)
			return 1;
	}
	writer->used = 0;

	if (sceIoWrite(writer->fd, buf, size) < 0) {
		sceClibPrintf("razor_capture: ERROR: failure to write capture data\n");
		return 1;
	}
	return 0;
}

int razorFileWriterWriteMem(RazorFileWriter *writer, const void *buf, SceSize size)
{
	SceSSize written;
	const char *cur;
	const char *end;
	int status = 0;

	if (writer->fd < 0)
		return 1;

	if (writer->used != 0) {
		if (sceIoWrite(writer->fd, writer->buffer, writer->used) < 0)
			return 1;
	}
	writer->used = 0;

	written = sceIoWrite(writer->fd, buf, size);
	if (written >= 0)
		return 0;

	if (written == (SceSSize)0x8001000E || written == (SceSSize)0x80020001) {
		cur = (const char *)buf;
		end = cur + size;
		sceClibMemset(writer->buffer, 0xCD, 0x1000);
		while (cur < end) {
			SceKernelMemBlockInfo info;
			uintptr_t nextPage = (((uintptr_t)(cur + 0xFFF)) & ~0xFFFU) + 0x1000U;
			const char *chunkEnd = (nextPage < (uintptr_t)end) ? (const char *)nextPage : end;
			SceSize chunkLen = (SceSize)(chunkEnd - cur);

			sceClibMemset(&info, 0, sizeof(info));
			info.size = sizeof(info);
			if (sceKernelGetMemBlockInfoByAddr((void *)cur, &info) != 0) {
				sceClibPrintf("razor_capture: WARNING: dummy data for 0x%x since it is not mapped memory\n",
					      cur);
				if (chunkLen > sizeof(writer->buffer) ||
				    sceIoWrite(writer->fd, writer->buffer, chunkLen) < 0)
					return 1;
				status = 2;
			} else {
				if (sceIoWrite(writer->fd, cur, chunkLen) < 0)
					return 1;
			}
			cur = chunkEnd;
		}
		return status;
	}

	return 0;
}

/* --- Host tool communication --- */

void razorHostNotifyStart(void)
{
	razorCaptureHostHookStart();
}

int razorHostNotifyComplete(uint64_t errorFlags)
{
	s_hostStatus = errorFlags;
	s_hostTrigger.pending = 0;
	razorCaptureHostHookComplete();
	return sceClibPrintf("Host Tool completed.\n");
}

int razorHostPollTrigger(char *outFilename, int maxLen)
{
	int pending;

	g_razorCaptureHostInfo.version = 2;
	pending = (s_hostTrigger.pending != 0) ? 1 : 0;
	if (pending && outFilename != NULL) {
		sceClibStrncpy(outFilename, s_hostTrigger.filename, (SceSize)(maxLen - 1));
		s_hostTrigger.filename[maxLen - 1] = '\0';
	}
	return pending;
}

/* --- Buffer range list --- */

int razorRangeListAdd(RazorRangeList *list, uint32_t start, uint32_t size)
{
	uint32_t count = list->count;

	if (count >= 1024)
		return 0;

	list->entries[count].start = start;
	list->entries[count].end = start + size;
	list->count = count + 1;
	return 1;
}

int razorRangeListRemove(RazorRangeList *list, uint32_t start)
{
	uint32_t count = list->count;
	uint32_t i;

	if (count == 0)
		return 0;

	for (i = count; i > 0; i--) {
		if (list->entries[i - 1].start == start) {
			uint32_t j;
			for (j = i - 1; j + 1 < count; j++)
				list->entries[j] = list->entries[j + 1];
			list->count = count - 1;
			return 1;
		}
	}
	return 0;
}

int razorRangeListFindEnd(const RazorRangeList *list, uint32_t addr, uint32_t *outEnd)
{
	uint32_t count = list->count;
	uint32_t best = 0xFFFFFFFFU;
	uint32_t i;

	for (i = count; i > 0; i--) {
		const RazorRangeEntry *e = &list->entries[i - 1];
		if (e->start <= addr && addr <= e->end && e->end < best)
			best = e->end;
	}
	*outEnd = best;
	return best != 0xFFFFFFFFU;
}

/* --- User marker string buffers --- */

static inline void razorMarkerEnsureInit(void)
{
	if (!s_markerInitialized) {
		s_markerBufs[0].contextId = 0;
		s_markerBufs[0].start = s_markerStorage[0];
		s_markerBufs[0].cur = s_markerStorage[0];
		s_markerBufs[0].end = s_markerStorage[0] + sizeof(s_markerStorage[0]);
		s_markerBufs[1].contextId = 0;
		s_markerBufs[1].start = s_markerStorage[1];
		s_markerBufs[1].cur = s_markerStorage[1];
		s_markerBufs[1].end = s_markerStorage[1] + sizeof(s_markerStorage[1]);
		s_markerInitialized = 1;
	}
}

int razorMarkerInit(void)
{
	int wasInit = s_markerInitialized;
	razorMarkerEnsureInit();
	return wasInit;
}

void *razorMarkerTerm(void)
{
	if (s_markerInitialized) {
		if (s_markerBufs[0].start != NULL)
			s_markerBufs[0].start = NULL;
		s_markerBufs[0].contextId = 0;
		s_markerBufs[0].cur = NULL;
		s_markerBufs[0].end = NULL;
		if (s_markerBufs[1].start != NULL)
			s_markerBufs[1].start = NULL;
		s_markerBufs[1].contextId = 0;
		s_markerBufs[1].cur = NULL;
		s_markerBufs[1].end = NULL;
		s_markerInitialized = 0;
		return s_markerBufs;
	}
	return NULL;
}

int razorMarkerReset(uint32_t contextId)
{
	RazorMarkerBuf *buf;

	razorMarkerEnsureInit();
	if (contextId == 0)
		return 0;

	if (contextId == s_markerBufs[0].contextId)
		buf = &s_markerBufs[0];
	else if (contextId == s_markerBufs[1].contextId)
		buf = &s_markerBufs[1];
	else
		return (int)contextId;

	buf->contextId = 0;
	buf->cur = buf->start;
	return (int)contextId;
}

int razorMarkerPush(uint32_t contextId, const char *name)
{
	RazorMarkerBuf *buf;
	int i;

	razorMarkerEnsureInit();
	if (contextId == 0)
		return 0;

	if (contextId == s_markerBufs[0].contextId) {
		buf = &s_markerBufs[0];
	} else if (contextId == s_markerBufs[1].contextId) {
		buf = &s_markerBufs[1];
	} else if (s_markerBufs[0].contextId == 0) {
		buf = &s_markerBufs[0];
		buf->contextId = contextId;
		buf->cur = buf->start;
	} else if (s_markerBufs[1].contextId == 0) {
		buf = &s_markerBufs[1];
		buf->contextId = contextId;
		buf->cur = buf->start;
	} else {
		return 0;
	}

	for (i = 0; i < 64 && name[i] != '\0'; i++) {
		if (buf->cur == buf->end)
			return 0;
		*buf->cur++ = name[i];
	}
	if (buf->cur == buf->end)
		return 0;
	*buf->cur++ = '\0';
	return 1;
}

int razorMarkerGetRange(uint32_t contextId, uint32_t *outStart, uint32_t *outEnd)
{
	RazorMarkerBuf *buf;

	razorMarkerEnsureInit();
	if (contextId == 0) {
		*outStart = 0;
		*outEnd = 0;
		return 0;
	}

	if (contextId == s_markerBufs[0].contextId) {
		buf = &s_markerBufs[0];
	} else if (contextId == s_markerBufs[1].contextId) {
		buf = &s_markerBufs[1];
	} else {
		*outStart = 0;
		*outEnd = 0;
		return 0;
	}

	*outStart = (uint32_t)(uintptr_t)buf->start;
	*outEnd = (uint32_t)(uintptr_t)buf->end;
	return 1;
}
