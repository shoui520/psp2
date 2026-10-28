/* 160-bit unicast packet encryption under the session's send counter. */
#include <threadmgr/threadmgr.h>
#include "adhoc.h"

extern SceUID gcAuthAdhocSemaphore;
extern int gcAuthAdhocInitialized;
extern GcAuthAdhocWorkspace gcAuthAdhocWorkspace;

static uint64_t GcAuthAdhocLoadCounter(const unsigned char *bytes)
{
	return (uint64_t)bytes[7] << 56 | (uint64_t)bytes[6] << 48 | bytes[0] |
		(uint64_t)bytes[5] << 40 | (uint64_t)bytes[4] << 32 |
		(uint64_t)bytes[3] << 24 | (uint64_t)bytes[2] << 16 |
		(uint64_t)bytes[1] << 8;
}

static void GcAuthAdhocStoreCounter(unsigned char *bytes, uint64_t counter)
{
	bytes[0] = (unsigned char)counter;
	bytes[4] = (unsigned char)(counter >> 32);
	bytes[7] = (unsigned char)(counter >> 56);
	bytes[5] = (unsigned char)(counter >> 40);
	bytes[6] = (unsigned char)(counter >> 48);
	bytes[1] = (unsigned char)(counter >> 8);
	bytes[2] = (unsigned char)(counter >> 16);
	bytes[3] = (unsigned char)(counter >> 24);
}

int GcAuthAdhocUniCastEncrypt160(GcAuthAdhocContext160 *context,
		unsigned char *data, unsigned int size, GcAuthAdhocAuthTag *tag, int encrypt)
{
	uint64_t counter;
	int result;

	if (sceKernelWaitSema(gcAuthAdhocSemaphore, 1, NULL) != 0)
		return (int)0x808a010a;
	if (context == NULL || data == NULL || tag == NULL)
		result = (int)0x808a0124;
	else if (!gcAuthAdhocInitialized)
		result = (int)0x808a010f;
	else if (size & 15)
		result = (int)0x808a011f;
	else if (size > 0x910)
		result = (int)0x808a0120;
	else {
		counter = GcAuthAdhocLoadCounter(context->ready.sendCounter);
		result = GcAuthAdhocEncryptCbcAuthData(&gcAuthAdhocWorkspace, data, size,
			&counter, (const unsigned int *)context->ready.seed, tag, encrypt);
		if (result == 0)
			GcAuthAdhocStoreCounter(context->ready.sendCounter, counter);
	}
	if (sceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		result = (int)0x808a010a;
	return result;
}

int GcAuthAdhocUniCastEncrypt160Stack(const GcAuthAdhocUniCastEncrypt160Request *request)
{
	return GcAuthAdhocUniCastEncrypt160(request->context, request->data,
		request->size, request->tag, request->encrypt);
}
