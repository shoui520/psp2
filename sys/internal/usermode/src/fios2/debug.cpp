/*
 * debug.cpp: the debug dumps (the SDK's fios2_debug.h): an error code, op, file or
 * directory handle, chunk, I/O filter or date as text. A dump given no buffer writes into
 * the next of four static ones.
 */
#include <sce_atomic.h>
#include "fios2_internal.h"

#define DUMP_BUFFER_COUNT	4
#define DUMP_BUFFER_SIZE	512

static volatile int32_t s_dumpBufferIndex;
static char s_opNameBuffer[8];
static char s_chunkNameBuffer[8];
static char s_dumpBuffers[DUMP_BUFFER_COUNT][DUMP_BUFFER_SIZE];

/* the next static buffer, for a dump given none */
static inline char *nextDumpBuffer(size_t *pBufferSize)
{
	uint32_t index = sceAtomicIncrement32Relaxed(&s_dumpBufferIndex);

	*pBufferSize = DUMP_BUFFER_SIZE;
	return s_dumpBuffers[index & (DUMP_BUFFER_COUNT - 1)];
}

#define ERROR_NAME(err)	case err: pName = #err; break

char *sceFiosDebugDumpError(int err, char *pBuffer, size_t bufferSize)
{
	char number[20];
	const char *pName = number;

	if (pBuffer == NULL)
		pBuffer = nextDumpBuffer(&bufferSize);
	else if (bufferSize == 0)
		return pBuffer;
	switch (err) {
	ERROR_NAME(SCE_FIOS_OK);
	ERROR_NAME(SCE_FIOS_IN_PROGRESS);
	ERROR_NAME(SCE_FIOS_ERROR_UNIMPLEMENTED);
	ERROR_NAME(SCE_FIOS_ERROR_CANT_ALLOCATE_OP);
	ERROR_NAME(SCE_FIOS_ERROR_CANT_ALLOCATE_FH);
	ERROR_NAME(SCE_FIOS_ERROR_CANT_ALLOCATE_DH);
	ERROR_NAME(SCE_FIOS_ERROR_CANT_ALLOCATE_CHUNK);
	ERROR_NAME(SCE_FIOS_ERROR_BAD_PATH);
	ERROR_NAME(SCE_FIOS_ERROR_BAD_PTR);
	ERROR_NAME(SCE_FIOS_ERROR_BAD_OFFSET);
	ERROR_NAME(SCE_FIOS_ERROR_BAD_SIZE);
	ERROR_NAME(SCE_FIOS_ERROR_BAD_IOVCNT);
	ERROR_NAME(SCE_FIOS_ERROR_BAD_OP);
	ERROR_NAME(SCE_FIOS_ERROR_BAD_FH);
	ERROR_NAME(SCE_FIOS_ERROR_BAD_DH);
	ERROR_NAME(SCE_FIOS_ERROR_BAD_ALIGNMENT);
	ERROR_NAME(SCE_FIOS_ERROR_NOT_A_FILE);
	ERROR_NAME(SCE_FIOS_ERROR_NOT_A_DIRECTORY);
	ERROR_NAME(SCE_FIOS_ERROR_EOF);
	ERROR_NAME(SCE_FIOS_ERROR_TIMEOUT);
	ERROR_NAME(SCE_FIOS_ERROR_CANCELLED);
	ERROR_NAME(SCE_FIOS_ERROR_ACCESS);
	ERROR_NAME(SCE_FIOS_ERROR_DECOMPRESSION);
	ERROR_NAME(SCE_FIOS_ERROR_READ_ONLY);
	ERROR_NAME(SCE_FIOS_ERROR_WRITE_ONLY);
	ERROR_NAME(SCE_FIOS_ERROR_MEDIA_GONE);
	ERROR_NAME(SCE_FIOS_ERROR_PATH_TOO_LONG);
	ERROR_NAME(SCE_FIOS_ERROR_TOO_MANY_OVERLAYS);
	ERROR_NAME(SCE_FIOS_ERROR_BAD_OVERLAY);
	ERROR_NAME(SCE_FIOS_ERROR_BAD_ORDER);
	ERROR_NAME(SCE_FIOS_ERROR_BAD_INDEX);
	ERROR_NAME(SCE_FIOS_ERROR_EVENT_NOT_HANDLED);
	ERROR_NAME(SCE_FIOS_ERROR_BUSY);
	ERROR_NAME(SCE_FIOS_ERROR_BAD_ARCHIVE);
	ERROR_NAME(SCE_KERNEL_ERROR_VA2PA_FAULT);
	ERROR_NAME(SCE_ERROR_ERRNO_ENOMEM);
	ERROR_NAME(SCE_ERROR_ERRNO_EFAULT);
	ERROR_NAME(SCE_ERROR_ERRNO_EEXIST);
	ERROR_NAME(SCE_ERROR_ERRNO_EINVAL);
	ERROR_NAME(SCE_ERROR_ERRNO_ENFILE);
	ERROR_NAME(SCE_ERROR_ERRNO_EMFILE);
	ERROR_NAME(SCE_ERROR_ERRNO_ENOSPC);
	ERROR_NAME(SCE_ERROR_ERRNO_EFORMAT);
	ERROR_NAME(SCE_ERROR_ERRNO_EUNSUP);
	ERROR_NAME(SCE_ERROR_ERRNO_ENOTEMPTY);
	default:
		fiosSnprintf(number, sizeof(number), "0x%08X", err);
		break;
	}
	fiosStrlcpy(pBuffer, pName, bufferSize);
	return pBuffer;
}

#define ENUM_NAME(value)	case value: return #value

static __attribute__((noinline)) const char *opName(uint16_t what)
{
	switch (what) {
	ENUM_NAME(OP_DELETE);
	ENUM_NAME(OP_DH_CLOSE);
	ENUM_NAME(OP_DH_OPEN);
	ENUM_NAME(OP_DH_READ);
	ENUM_NAME(OP_DH_READ_BULK);
	ENUM_NAME(OP_DIRECTORY_CREATE);
	ENUM_NAME(OP_DIRECTORY_DELETE);
	ENUM_NAME(OP_DIRECTORY_EXISTS);
	ENUM_NAME(OP_EXISTS);
	ENUM_NAME(OP_FH_CLOSE);
	ENUM_NAME(OP_FH_OPEN);
	ENUM_NAME(OP_FH_PREADV);
	ENUM_NAME(OP_FH_PWRITEV);
	ENUM_NAME(OP_FH_READV);
	ENUM_NAME(OP_FH_STAT);
	ENUM_NAME(OP_FH_SYNC);
	ENUM_NAME(OP_FH_TRUNCATE);
	ENUM_NAME(OP_FH_WRITEV);
	ENUM_NAME(OP_FILE_DELETE);
	ENUM_NAME(OP_FILE_EXISTS);
	ENUM_NAME(OP_FILE_GET_SIZE);
	ENUM_NAME(OP_PATH_READ);
	ENUM_NAME(OP_PATH_WRITE);
	ENUM_NAME(OP_PREFETCH_FH);
	ENUM_NAME(OP_PREFETCH_FH_RANGE);
	ENUM_NAME(OP_PREFETCH_FILE);
	ENUM_NAME(OP_PREFETCH_FILE_RANGE);
	ENUM_NAME(OP_RENAME);
	ENUM_NAME(OP_RESOLVE);
	ENUM_NAME(OP_STAT);
	ENUM_NAME(OP_TRUNCATE);
	ENUM_NAME(OP_ARCHIVE_QUERY);
	ENUM_NAME(OP_ARCHIVE_MOUNT);
	ENUM_NAME(OP_ARCHIVE_UNMOUNT);
	ENUM_NAME(OP_CH_STAT);
	ENUM_NAME(OP_DEVCTL);
	ENUM_NAME(OP_IOCTL);
	ENUM_NAME(OP_SYNC);
	default:
		fiosSnprintf(s_opNameBuffer, sizeof(s_opNameBuffer), "%d", what);
		return s_opNameBuffer;
	}
}

/* fmt's text appended to the string in pBuffer, when there is room for more than its end */
static void appendf(char *pBuffer, size_t bufferSize, const char *fmt, ...)
{
	size_t length = fiosStrlen(pBuffer);
	va_list ap;

	va_start(ap, fmt);
	if (length <= bufferSize && bufferSize - length > 1) {
		fiosVsnprintf(pBuffer + length, bufferSize - length, fmt, ap);
		pBuffer[bufferSize - 1] = '\0';
	}
	va_end(ap);
}

/* the ops with a file range: their offset and request size are always shown */
static inline bool hasRange(uint16_t what)
{
	switch (what) {
	case OP_FH_PREADV:
	case OP_FH_PWRITEV:
	case OP_FH_READV:
	case OP_FH_WRITEV:
	case OP_PATH_READ:
	case OP_PATH_WRITE:
	case OP_PREFETCH_FH_RANGE:
	case OP_PREFETCH_FILE_RANGE:
		return true;
	default:
		return false;
	}
}

/*
 * A handle's unit: the one it names, or the unit its index names when the handle is stale
 * (DELETED when that unit is free, REALLOCATED when another handle has it), or BOGUS.
 */
static inline const char *handleState(const void *pUnit, bool deleted)
{
	if (pUnit == NULL)
		return "BOGUS";
	return deleted ? "DELETED" : "REALLOCATED";
}

char *sceFiosDebugDumpOp(SceFiosOp op, char *pBuffer, size_t bufferSize)
{
	FiosOp *pOp;
	bool valid = false, bogus, deleted = false, reallocated;
	char err[48];
	const char *pPath;

	if (pBuffer == NULL)
		pBuffer = nextDumpBuffer(&bufferSize);
	else if (bufferSize == 0)
		return pBuffer;
	pOp = (FiosOp *)s_pOpTable->Lookup(op);
	if (pOp != NULL)
		valid = true;
	else
		pOp = (FiosOp *)s_pOpTable->GetUnit(op);
	bogus = pOp == NULL;
	if (!(valid | bogus))
		deleted = pOp->h == 0;
	reallocated = !(valid | bogus | deleted);
	*pBuffer = '\0';
	if (valid)
		appendf(pBuffer, bufferSize, "op 0x%08X:{", op);
	else
		appendf(pBuffer, bufferSize, "op 0x%08X:{ state:%s", op, handleState(pOp, deleted));
	if (!bogus && !reallocated) {
		uint16_t i;

		if (!valid)
			appendf(pBuffer, bufferSize, ",");
		appendf(pBuffer, bufferSize, " what:%s", opName(pOp->what));
		appendf(pBuffer, bufferSize, ", done:%d",
			pOp->done != 0 || op == *fiosGetCallbackOp());
		appendf(pBuffer, bufferSize, ", err:%s",
			sceFiosDebugDumpError(pOp->err, err, sizeof(err)));
		if (pOp->fh != 0)
			appendf(pBuffer, bufferSize, ", h:0x%08X", pOp->fh);
		pPath = fiosPathGet(&pOp->path);
		if (pPath != NULL && *pPath != '\0')
			appendf(pBuffer, bufferSize, ", path:\"%s\"", pPath);
		if (pOp->offset != 0 || hasRange(pOp->what))
			appendf(pBuffer, bufferSize, ", offset:%lld", pOp->offset);
		if (pOp->bufferCount != 0) {
			SceFiosSize reqCount = 0;

			for (i = 0; i < pOp->bufferCount; i++)
				reqCount += pOp->buffers[i].length;
			if (hasRange(pOp->what))
				appendf(pBuffer, bufferSize, ", reqCount:%lld, actCount:%lld", reqCount,
					pOp->actCount);
			appendf(pBuffer, bufferSize, ", buffers:[");
			for (i = 0; i < pOp->bufferCount; i++)
				appendf(pBuffer, bufferSize, "%s{ pPtr:0x%08llX, length:%lld }",
					i > 0 ? "," : "", (int64_t)(intptr_t)pOp->buffers[i].pPtr,
					(int64_t)pOp->buffers[i].length);
			appendf(pBuffer, bufferSize, "]");
		}
		if (pOp->attr.priority != 0 || pOp->attr.deadline != 0 || pOp->attr.opflags != 0 ||
		    pOp->attr.pCallback != NULL || pOp->attr.pCallbackContext != NULL ||
		    pOp->attr.userTag != 0 || pOp->attr.userPtr != NULL) {
			appendf(pBuffer, bufferSize, ", attr:{ priority:%d", pOp->attr.priority);
			if (pOp->attr.deadline != 0) {
				int64_t ns = sceFiosTimeIntervalToNanoseconds(pOp->attr.deadline);
				int64_t sec = ns / 1000000000;

				appendf(pBuffer, bufferSize, ", deadline:%lld.%06lld", sec,
					(ns - sec * 1000000000) / 1000);
			}
			if (pOp->attr.opflags != 0)
				appendf(pBuffer, bufferSize, ", opflags:0x%08X", pOp->attr.opflags);
			if (pOp->attr.pCallback != NULL)
				appendf(pBuffer, bufferSize, ", pCallback:0x%08llX",
					(int64_t)(intptr_t)pOp->attr.pCallback);
			if (pOp->attr.pCallbackContext != NULL)
				appendf(pBuffer, bufferSize, ", pCallbackContext:0x%08llX",
					(int64_t)(intptr_t)pOp->attr.pCallbackContext);
			if (pOp->attr.userTag != 0)
				appendf(pBuffer, bufferSize, ", userTag:0x%08llX",
					(int64_t)(int32_t)pOp->attr.userTag);
			if (pOp->attr.userPtr != NULL)
				appendf(pBuffer, bufferSize, ", userPtr:0x%08llX",
					(int64_t)(intptr_t)pOp->attr.userPtr);
			appendf(pBuffer, bufferSize, " }");
		}
		if (pOp->rootChunk != CHUNKID_INVALID)
			appendf(pBuffer, bufferSize, ", rootChunk:%d", pOp->rootChunk);
		if (pOp->internalFlags != 0)
			appendf(pBuffer, bufferSize, ", internalFlags:0x%08X", pOp->internalFlags);
	}
	appendf(pBuffer, bufferSize, " }");
	return pBuffer;
}

char *sceFiosDebugDumpFH(SceFiosFH fh, char *pBuffer, size_t bufferSize)
{
	FiosFH *pFH;
	bool valid = false, bogus, deleted = false, reallocated;
	const char *pPath;

	if (pBuffer == NULL)
		pBuffer = nextDumpBuffer(&bufferSize);
	else if (bufferSize == 0)
		return pBuffer;
	pFH = (FiosFH *)s_pFHTable->Lookup(fh);
	if (pFH != NULL)
		valid = true;
	else
		pFH = (FiosFH *)s_pFHTable->GetUnit(fh);
	bogus = pFH == NULL;
	if (!(valid | bogus))
		deleted = pFH->h == 0;
	reallocated = !(valid | bogus | deleted);
	*pBuffer = '\0';
	if (valid)
		appendf(pBuffer, bufferSize, "fh 0x%08X:{", fh);
	else
		appendf(pBuffer, bufferSize, "fh 0x%08X:{ state:%s", fh, handleState(pFH, deleted));
	if (!bogus && !reallocated) {
		if (!valid)
			appendf(pBuffer, bufferSize, ",");
		appendf(pBuffer, bufferSize, " offset:%lld", pFH->offset);
		appendf(pBuffer, bufferSize, ", size:%lld", pFH->size);
		appendf(pBuffer, bufferSize, ", nativeFH:%d", pFH->nativeFH);
		pPath = fiosPathGet(&pFH->path);
		if (pPath != NULL && *pPath != '\0')
			appendf(pBuffer, bufferSize, ", path:\"%s\"", pPath);
	}
	appendf(pBuffer, bufferSize, " }");
	return pBuffer;
}

char *sceFiosDebugDumpDH(SceFiosDH dh, char *pBuffer, size_t bufferSize)
{
	FiosDH *pDH;
	bool valid = false, bogus, deleted = false, reallocated;
	const char *pPath;

	if (pBuffer == NULL)
		pBuffer = nextDumpBuffer(&bufferSize);
	else if (bufferSize == 0)
		return pBuffer;
	pDH = (FiosDH *)s_pDHTable->Lookup(dh);
	if (pDH != NULL)
		valid = true;
	else
		pDH = (FiosDH *)s_pDHTable->GetUnit(dh);
	bogus = pDH == NULL;
	if (!(valid | bogus))
		deleted = pDH->h == 0;
	reallocated = !(valid | bogus | deleted);
	*pBuffer = '\0';
	if (valid)
		appendf(pBuffer, bufferSize, "dh 0x%08X:{", dh);
	else
		appendf(pBuffer, bufferSize, "dh 0x%08X:{ state:%s", dh, handleState(pDH, deleted));
	if (!bogus && !reallocated) {
		if (!valid)
			appendf(pBuffer, bufferSize, ",");
		appendf(pBuffer, bufferSize, " nativeDH:%d", pDH->nativeDH);
		pPath = fiosPathGet(&pDH->path);
		if (pPath != NULL && *pPath != '\0')
			appendf(pBuffer, bufferSize, ", path:\"%s\"", pPath);
	}
	appendf(pBuffer, bufferSize, " }");
	return pBuffer;
}

static __attribute__((noinline)) const char *chunkName(uint16_t type)
{
	switch (type) {
	ENUM_NAME(CHUNK_NULL);
	ENUM_NAME(CHUNK_ROOT);
	ENUM_NAME(CHUNK_PARENT);
	ENUM_NAME(CHUNK_CHILD);
	ENUM_NAME(CHUNK_CALLBACK);
	ENUM_NAME(CHUNK_BUFFER);
	ENUM_NAME(CHUNK_FH_OPEN);
	ENUM_NAME(CHUNK_FH_READ);
	ENUM_NAME(CHUNK_FH_WRITE);
	ENUM_NAME(CHUNK_FH_BARRIER);
	ENUM_NAME(CHUNK_FH_SYNC);
	ENUM_NAME(CHUNK_FH_CLOSE);
	ENUM_NAME(CHUNK_DH_OPEN);
	ENUM_NAME(CHUNK_DH_READ);
	ENUM_NAME(CHUNK_DH_READ_BULK);
	ENUM_NAME(CHUNK_DH_CLOSE);
	ENUM_NAME(CHUNK_DIR_EXISTS);
	ENUM_NAME(CHUNK_DIR_CREATE);
	ENUM_NAME(CHUNK_DIR_DELETE);
	ENUM_NAME(CHUNK_FILE_EXISTS);
	ENUM_NAME(CHUNK_FILE_DELETE);
	ENUM_NAME(CHUNK_FILE_SIZE);
	ENUM_NAME(CHUNK_DELETE);
	ENUM_NAME(CHUNK_EXISTS);
	ENUM_NAME(CHUNK_STAT);
	ENUM_NAME(CHUNK_TRUNCATE);
	ENUM_NAME(CHUNK_PREFETCH);
	ENUM_NAME(CHUNK_RESOLVE);
	ENUM_NAME(CHUNK_RENAME);
	ENUM_NAME(CHUNK_ARCHIVE_QUERY);
	ENUM_NAME(CHUNK_ARCHIVE_MOUNT);
	ENUM_NAME(CHUNK_ARCHIVE_UNMOUNT);
	ENUM_NAME(CHUNK_CHSTAT);
	ENUM_NAME(CHUNK_DEVCTL);
	ENUM_NAME(CHUNK_IOCTL);
	ENUM_NAME(CHUNK_SYNC);
	default:
		fiosSnprintf(s_chunkNameBuffer, sizeof(s_chunkNameBuffer), "%d", type);
		return s_chunkNameBuffer;
	}
}

/* "{a,b,...}" or "none": a chunk's parents or children */
static inline void dumpChunkList(char *pBuffer, size_t bufferSize, uint16_t chunkID,
				 uint16_t (*pGet)(uint16_t chunkID, int index))
{
	uint16_t id = pGet(chunkID, 0);
	int i;

	if (id == CHUNKID_INVALID) {
		appendf(pBuffer, bufferSize, "none");
		return;
	}
	appendf(pBuffer, bufferSize, "{%d", id);
	for (i = 1; (id = pGet(chunkID, i)) != CHUNKID_INVALID; i++)
		appendf(pBuffer, bufferSize, ",%d", id);
	appendf(pBuffer, bufferSize, "}");
}

/* a chunk field not shown */
#define DUMP_NO_VALUE	(-1024)

char *fiosDebugDumpChunk(uint16_t chunkID, char *pBuffer, size_t bufferSize)
{
	FiosChunk *pChunk;
	bool invalid, bogus = false, deleted = false;

	if (pBuffer == NULL)
		pBuffer = nextDumpBuffer(&bufferSize);
	else if (bufferSize == 0)
		return pBuffer;
	pChunk = (FiosChunk *)((uint8_t *)g_FiosParams.chunkStorage.pPtr + chunkID * 8);
	invalid = chunkID == CHUNKID_INVALID;
	if (!invalid)
		bogus = chunkID > s_chunkAllocator.m_count;
	if (!(invalid | bogus))
		deleted = !(s_chunkAllocator.IsAllocatedIndex(chunkID, 1) &&
			    pChunk->magic == FIOS_CHUNK_MAGIC);
	*pBuffer = '\0';
	if (invalid) {
		appendf(pBuffer, bufferSize, "CHUNKID_INVALID");
		return pBuffer;
	}
	if (bogus | deleted)
		appendf(pBuffer, bufferSize, "chunk %d:{ state:%s", chunkID,
			bogus ? "BOGUS" : "DELETED");
	else
		appendf(pBuffer, bufferSize, "chunk %d:{", chunkID);
	if (!(bogus | deleted)) {
		SceFiosHandle h = 0;
		SceFiosOffset offset = DUMP_NO_VALUE;
		SceFiosSize reqCount = DUMP_NO_VALUE, actCount = DUMP_NO_VALUE;
		FiosPath *pPath = NULL;
		size_t length;

		appendf(pBuffer, bufferSize, " type:%s", chunkName(pChunk->type));
		appendf(pBuffer, bufferSize, ", scheduled:%d", pChunk->scheduled);
		appendf(pBuffer, bufferSize, ", completed:%d", pChunk->completed != 0);
		appendf(pBuffer, bufferSize, ", executing:%d", pChunk->executing != 0);
		appendf(pBuffer, bufferSize, ", prepared:%d", pChunk->prepared != 0);
		if (pChunk->cancelled)
			appendf(pBuffer, bufferSize, ", cancelled:%d", pChunk->cancelled != 0);
		appendf(pBuffer, bufferSize, ", parents:");
		dumpChunkList(pBuffer, bufferSize, chunkID, fiosChunkGetParent);
		appendf(pBuffer, bufferSize, ", children:");
		dumpChunkList(pBuffer, bufferSize, chunkID, fiosChunkGetChild);
		switch (pChunk->type) {
		case CHUNK_ROOT:
			if (pChunk->op != 0) {
				appendf(pBuffer, bufferSize, ", ");
				length = fiosStrlen(pBuffer);
				sceFiosDebugDumpOp(pChunk->op, pBuffer + length, bufferSize - length);
			}
			break;
		case CHUNK_FH_READ:
		case CHUNK_FH_WRITE:
		case CHUNK_DH_READ:
		case CHUNK_PREFETCH:
			h = pChunk->h;
			offset = pChunk->offset;
			reqCount = pChunk->reqCount;
			if (pChunk->pActCount != NULL)
				actCount = *pChunk->pActCount;
			goto handle;
		case CHUNK_FH_OPEN:
		case CHUNK_FH_CLOSE:
		case CHUNK_DH_OPEN:
		case CHUNK_DH_CLOSE:
			h = pChunk->h;
			/* fall through */
		case CHUNK_PARENT:
		case CHUNK_CHILD:
		case CHUNK_CALLBACK:
		case CHUNK_BUFFER:
		case CHUNK_FH_BARRIER:
		case CHUNK_FH_SYNC:
		case CHUNK_DH_READ_BULK:
		case CHUNK_DIR_EXISTS:
		case CHUNK_DIR_CREATE:
		case CHUNK_DIR_DELETE:
		case CHUNK_FILE_EXISTS:
		case CHUNK_FILE_DELETE:
		case CHUNK_FILE_SIZE:
		case CHUNK_DELETE:
		case CHUNK_EXISTS:
		case CHUNK_STAT:
		case CHUNK_TRUNCATE:
		handle:
			if (h != 0) {
				FiosFH *pFH = (FiosFH *)s_pFHTable->Lookup(h);
				FiosDH *pDH;

				if (pFH != NULL)
					pPath = &pFH->path;
				pDH = (FiosDH *)s_pDHTable->Lookup(h);
				if (pDH != NULL)
					pPath = &pDH->path;
				appendf(pBuffer, bufferSize, ", h:0x%08X", h);
			}
			if (pPath != NULL)
				appendf(pBuffer, bufferSize, ", path:\"%s\"", pPath->GetPath());
			if (offset != DUMP_NO_VALUE)
				appendf(pBuffer, bufferSize, ", offset:%lld", offset);
			if (reqCount != DUMP_NO_VALUE)
				appendf(pBuffer, bufferSize, ", reqCount:%lld", reqCount);
			if (actCount != DUMP_NO_VALUE)
				appendf(pBuffer, bufferSize, ", actCount:%lld", actCount);
			break;
		default:
			break;
		}
	}
	appendf(pBuffer, bufferSize, " }");
	return pBuffer;
}

char *sceFiosDebugDumpDate(SceFiosDate date, char *pBuffer, size_t bufferSize)
{
	struct tm components;
	int64_t ns;

	if (pBuffer == NULL)
		pBuffer = nextDumpBuffer(&bufferSize);
	else if (bufferSize == 0)
		return pBuffer;
	sceFiosDateToComponents(date, &components);
	ns = date % 1000000000;
	*pBuffer = '\0';
	appendf(pBuffer, bufferSize, "%d-%02d-%02d %02d:%02d:%02d", components.tm_year + 1900,
		components.tm_mon + 1, components.tm_mday, components.tm_hour, components.tm_min,
		components.tm_sec);
	if (ns != 0)
		appendf(pBuffer, bufferSize, ".%09d", (int)ns);
	appendf(pBuffer, bufferSize, " UTC");
	return pBuffer;
}

char *fiosDebugDumpIOFilter(SceFiosIOFilterCallback pFilterCallback, void *pFilterContext,
			    char *pBuffer, size_t bufferSize)
{
	char desc[256];
	int err;

	if (pBuffer == NULL)
		pBuffer = nextDumpBuffer(&bufferSize);
	else if (bufferSize == 0)
		return pBuffer;
	*pBuffer = '\0';
	appendf(pBuffer, bufferSize, "iofilter 0x%08llX:{ pFilterContext:0x%08llX",
		(int64_t)(intptr_t)pFilterCallback, (int64_t)(intptr_t)pFilterContext);
	if (pFilterCallback != NULL) {
		err = ((int (*)(void *, int, char *))pFilterCallback)(pFilterContext, 2, desc);
		if (err != 0)
			appendf(pBuffer, bufferSize, ", descErr:%s",
				sceFiosDebugDumpError(err, desc, sizeof(desc)));
		else
			appendf(pBuffer, bufferSize, ", desc:\"%s\"", desc);
	}
	appendf(pBuffer, bufferSize, " }");
	return pBuffer;
}

/* past the end of the string at p, which is not empty */
static inline char *stringEnd(char *p)
{
	while (*++p != '\0')
		;
	return p;
}

/* bufferSize, not what is left of it, goes to each part, as in Sony's */
char *fiosDebugDumpCacheEntry(FiosCacheEntry entry, char *pBuffer, size_t bufferSize)
{
	char *p;

	if (pBuffer == NULL)
		pBuffer = nextDumpBuffer(&bufferSize);
	else if (bufferSize == 0)
		return pBuffer;
	*pBuffer = '\0';
	appendf(pBuffer, bufferSize, "accessDate: ");
	p = stringEnd(pBuffer);
	sceFiosDebugDumpDate(entry.accessDate, p, bufferSize);
	p = stringEnd(p);
	appendf(p, bufferSize, " modificationDate: ");
	p = stringEnd(p);
	sceFiosDebugDumpDate(entry.modificationDate, p, bufferSize);
	p = stringEnd(p);
	appendf(p, bufferSize, " offset: %lld - %lld", entry.offset,
		entry.offset + entry.length);
	return pBuffer;
}
