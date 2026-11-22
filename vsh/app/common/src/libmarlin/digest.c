/*
 * SHA-1, SHA-256 and a Mersenne Twister seeded from the RTC, over the library's crypto
 * routines (0x81057024-). The file's name is ours.
 */
#include <libmt19937.h>
#include <libsha1.h>
#include <libsha256.h>
#include <rtc.h>
#include "mln_internal.h"

#define MLN_SHA1_CONTEXT_SIZE	0x78
#define MLN_SHA256_CONTEXT_SIZE	0x90
#define MLN_RANDOM_CONTEXT_SIZE	0x9C4

int MlnImpl_81057024(void **context)
{
	void *sha;

	*context = NULL;
	sha = MlnAllocate(MLN_HEAP_GENERAL, MLN_SHA1_CONTEXT_SIZE);
	if (sha == NULL)
		goto fail;
	sceSha1BlockInit(sha);
	*context = sha;
	return 0;
fail:
	/* Sony's test is inverted: only a context that is not there is freed */
	if (*context == NULL)
		MlnFree(MLN_HEAP_GENERAL, *context);
	return 2;
}

int MlnImpl_81057078(void *context, unsigned char *digest)
{
	sceSha1BlockResult(context, digest);
	MlnFree(MLN_HEAP_GENERAL, context);
	return 0;
}

int MlnImpl_81057098(void *context, const void *data, unsigned int size)
{
	sceSha1BlockUpdate(context, data, size);
	return 0;
}

int MlnImpl_810570A8(const void *data, unsigned int size, unsigned char *digest)
{
	sceSha1Digest(data, size, digest);
	return 0;
}

int MlnImpl_810570B8(void **context)
{
	void *sha;

	*context = NULL;
	sha = MlnAllocate(MLN_HEAP_GENERAL, MLN_SHA256_CONTEXT_SIZE);
	if (sha == NULL)
		goto fail;
	sceSha256BlockInit(sha);
	*context = sha;
	return 0;
fail:
	if (*context == NULL)
		MlnFree(MLN_HEAP_GENERAL, *context);
	return 2;
}

int MlnImpl_8105710C(void *context, unsigned char *digest)
{
	sceSha256BlockResult(context, digest);
	MlnFree(MLN_HEAP_GENERAL, context);
	return 0;
}

int MlnImpl_8105712C(void *context, const void *data, unsigned int size)
{
	sceSha256BlockUpdate(context, data, size);
	return 0;
}

int MlnImpl_8105713C(const void *data, unsigned int size, unsigned char *digest)
{
	sceSha256Digest(data, size, digest);
	return 0;
}

int MlnImpl_8105714C(void **context)
{
	SceRtcTick tick;
	void *random;

	*context = NULL;
	random = MlnAllocate(MLN_HEAP_GENERAL, MLN_RANDOM_CONTEXT_SIZE);
	if (random == NULL)
		goto fail;
	sceRtcGetCurrentTick(&tick);
	sceMt19937Init(random, (unsigned int)tick.tick);
	*context = random;
	return 0;
fail:
	if (*context == NULL)
		MlnFree(MLN_HEAP_GENERAL, *context);
	return 2;
}

int MlnImpl_810571B4(void)
{
	return 0;
}

int MlnImpl_810571BC(void)
{
	return 0;
}

/* random bytes, a 32-bit draw for each four, least significant byte first */
int MlnImpl_810571C4(void *context, unsigned int size, void *out)
{
	unsigned char *p = out;
	unsigned int value;

	if (context == NULL || out == NULL)
		return 3;
	for (; size != 0; size -= 4, p += 4) {
		value = sceMt19937UInt(context);
		p[0] = value;
		if (size == 1)
			break;
		p[1] = value >> 8;
		if (size == 2)
			break;
		p[2] = value >> 16;
		if (size == 3)
			break;
		p[3] = value >> 24;
	}
	return 0;
}

int MlnImpl_81057244(void *context)
{
	MlnFree(MLN_HEAP_GENERAL, context);
	return 0;
}
