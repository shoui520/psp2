/*
 * SHA-1, SHA-256 and a Mersenne Twister seeded from the RTC, over the library's crypto
 * routines (0x81057024-). The file's name is ours.
 */
#include <rtc.h>
#include "mln_internal.h"

#define MLN_SHA1_CONTEXT_SIZE	0x78
#define MLN_SHA256_CONTEXT_SIZE	0x90
#define MLN_RANDOM_CONTEXT_SIZE	0x9C4

void MlnImpl_8107FE2C(void *context);
void MlnImpl_8107FE80(void *context, const void *data, unsigned int size);
void MlnImpl_8107FF70(void *context, unsigned char *digest);
void MlnImpl_81080094(const void *data, unsigned int size, unsigned char *digest);
void MlnImpl_81080118(void *context);
void MlnImpl_81080190(void *context, const void *data, unsigned int size);
void MlnImpl_81080280(void *context, unsigned char *digest);
void MlnImpl_810803A8(const void *data, unsigned int size, unsigned char *digest);
void MlnImpl_81080450(void *context, unsigned int seed);
unsigned int MlnImpl_81087DF8(void *context);

int MlnImpl_81057024(void **context)
{
	void *sha;

	*context = NULL;
	sha = MlnAllocate(MLN_HEAP_GENERAL, MLN_SHA1_CONTEXT_SIZE);
	if (sha == NULL)
		goto fail;
	MlnImpl_8107FE2C(sha);
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
	MlnImpl_8107FF70(context, digest);
	MlnFree(MLN_HEAP_GENERAL, context);
	return 0;
}

int MlnImpl_81057098(void *context, const void *data, unsigned int size)
{
	MlnImpl_8107FE80(context, data, size);
	return 0;
}

int MlnImpl_810570A8(const void *data, unsigned int size, unsigned char *digest)
{
	MlnImpl_81080094(data, size, digest);
	return 0;
}

int MlnImpl_810570B8(void **context)
{
	void *sha;

	*context = NULL;
	sha = MlnAllocate(MLN_HEAP_GENERAL, MLN_SHA256_CONTEXT_SIZE);
	if (sha == NULL)
		goto fail;
	MlnImpl_81080118(sha);
	*context = sha;
	return 0;
fail:
	if (*context == NULL)
		MlnFree(MLN_HEAP_GENERAL, *context);
	return 2;
}

int MlnImpl_8105710C(void *context, unsigned char *digest)
{
	MlnImpl_81080280(context, digest);
	MlnFree(MLN_HEAP_GENERAL, context);
	return 0;
}

int MlnImpl_8105712C(void *context, const void *data, unsigned int size)
{
	MlnImpl_81080190(context, data, size);
	return 0;
}

int MlnImpl_8105713C(const void *data, unsigned int size, unsigned char *digest)
{
	MlnImpl_810803A8(data, size, digest);
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
	MlnImpl_81080450(random, (unsigned int)tick.tick);
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
		value = MlnImpl_81087DF8(context);
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
