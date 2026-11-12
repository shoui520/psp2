/* Sony's strings name MES/common/err/err.c. Private identifiers here are ours. */
#include "err.h"
#include "../memory.h"
#include "../runtime.h"

#define SSL_ERROR_CAPACITY 16
#define SSL_ERROR_LOCK_INDEX 1

typedef struct SslErrorEntry {
	SceUInt32 threadId;
	SceUInt32 code;
} SslErrorEntry;

_Static_assert(sizeof(SslErrorEntry) == 8, "SslErrorEntry");

static int sslErrorCount;
static SslErrorEntry sslErrors[SSL_ERROR_CAPACITY];
static const char sslErrorFile[] = "S:\\work\\MES\\common\\err\\err.c";

void *sslClearErrors(void)
{
	sslErrorCount = 0;
	return sslFillMemory(sslErrors, 0, sizeof(sslErrors));
}

int sslPushError(int library, int function, int reason, const char *file, int line)
{
	SceUInt32 threadId = sslThreadId();
	SceUInt32 code;
	int count;

	/* Native callers supply these arguments; the queue does not record them. */
	(void)file;
	(void)line;
	sslLock(SSL_LOCK_ERROR_QUEUE, SSL_ERROR_LOCK_INDEX, sslErrorFile, 494);
	code = (reason & 0xfff) | ((function & 0xfff) << 12) |
	       ((SceUInt32)library << 24);
	count = sslErrorCount;
	if (count >= SSL_ERROR_CAPACITY) {
		sslMoveMemory(sslErrors, sslErrors + 1,
			      (SSL_ERROR_CAPACITY - 1) * sizeof(*sslErrors));
		sslErrors[SSL_ERROR_CAPACITY - 1].threadId = 0;
		sslErrors[SSL_ERROR_CAPACITY - 1].code = 0;
		count = SSL_ERROR_CAPACITY - 1;
	}
	sslErrors[count].threadId = threadId;
	sslErrors[count].code = code;
	sslErrorCount = count + 1;
	return sslLock(SSL_UNLOCK_ERROR_QUEUE, SSL_ERROR_LOCK_INDEX, sslErrorFile, 498);
}

int sslRemoveThreadErrors(SceUInt32 threadId)
{
	int removed;

	if (!threadId)
		threadId = sslThreadId();
	sslLock(SSL_LOCK_ERROR_QUEUE, SSL_ERROR_LOCK_INDEX, sslErrorFile, 1037);
	for (removed = 0; removed < SSL_ERROR_CAPACITY; ++removed) {
		int count = sslErrorCount;
		SceUInt32 code = 0;
		int i;

		for (i = 0; i < count; ++i) {
			SslErrorEntry *entry = &sslErrors[i];

			if (entry->threadId != threadId)
				continue;
			code = entry->code;
			sslErrorCount = count - 1;
			entry->threadId = 0;
			entry->code = 0;
			sslMoveMemory(entry, entry + 1, (count - 1 - i) * sizeof(*entry));
			break;
		}
		if (!code)
			break;
	}
	return sslLock(SSL_UNLOCK_ERROR_QUEUE, SSL_ERROR_LOCK_INDEX, sslErrorFile, 1044);
}

int sslClearThreadErrors(void)
{
	return sslRemoveThreadErrors(0);
}

SceUInt32 sslReadError(int remove)
{
	SceUInt32 threadId = sslThreadId();
	SceUInt32 code = 0;
	int count, i;

	sslLock(SSL_LOCK_ERROR_QUEUE, SSL_ERROR_LOCK_INDEX, sslErrorFile, 686);
	count = sslErrorCount;
	for (i = 0; i < count; ++i) {
		SslErrorEntry *entry = &sslErrors[i];

		if (entry->threadId != threadId)
			continue;
		code = entry->code;
		if (remove) {
			sslErrorCount = count - 1;
			entry->threadId = 0;
			entry->code = 0;
			sslMoveMemory(entry, entry + 1, (count - 1 - i) * sizeof(*entry));
		}
		break;
	}
	sslLock(SSL_UNLOCK_ERROR_QUEUE, SSL_ERROR_LOCK_INDEX, sslErrorFile, 690);
	return code;
}

SceUInt32 sslGetError(void)
{
	return sslReadError(1);
}

SceUInt32 sslPeekError(void)
{
	return sslReadError(0);
}

/* SSL-C's error trace hook; this build discards its message. */
void sslErrorTrace(const char *format, ...)
{
}
