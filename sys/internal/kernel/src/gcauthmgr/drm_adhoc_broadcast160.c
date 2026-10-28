/* 160-bit broadcast packet encryption under the group's random and counter. */
#include <threadmgr/threadmgr.h>
#include "adhoc.h"

extern SceUID gcAuthAdhocSemaphore;
extern int gcAuthAdhocInitialized;
extern GcAuthAdhocWorkspace gcAuthAdhocWorkspace;
extern uint64_t gcAuthAdhocCounter;
extern unsigned char gcAuthAdhocRandom[32];

int GcAuthAdhocBroadCastEncrypt160(unsigned char *data, unsigned int size,
		GcAuthAdhocAuthTag *tag, int encrypt)
{
	uint64_t counter;
	int result;

	if (sceKernelWaitSema(gcAuthAdhocSemaphore, 1, NULL) != 0)
		return (int)0x808a010a;
	if (data == NULL || tag == NULL)
		result = (int)0x808a0124;
	else if (!gcAuthAdhocInitialized)
		result = (int)0x808a010f;
	else if (size & 15)
		result = (int)0x808a011f;
	else if (size > 0x910)
		result = (int)0x808a0120;
	else {
		counter = gcAuthAdhocCounter;
		result = GcAuthAdhocEncryptCbcAuthData(&gcAuthAdhocWorkspace, data, size,
			&counter, (const unsigned int *)gcAuthAdhocRandom, tag, encrypt);
		if (result == 0)
			gcAuthAdhocCounter = counter;
	}
	if (sceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		result = (int)0x808a010a;
	return result;
}

int GcAuthAdhocBroadCastEncrypt160Stack(const GcAuthAdhocBroadCastEncrypt160Request *request)
{
	return GcAuthAdhocBroadCastEncrypt160(request->data, request->size,
		request->tag, request->encrypt);
}

int GcAuthAdhocBroadCastDecrypt160(GcAuthAdhocContext160 *context,
		unsigned char *data, unsigned int size, const GcAuthAdhocAuthTag *tag,
		int decrypt, int checkCounter)
{
	uint64_t counter;
	uint64_t minimumCounter;
	unsigned char *bytes;
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
		counter = GcAuthAdhocLoadCounter(tag->counter);
		minimumCounter = GcAuthAdhocLoadCounter(context->ready.peerCounter);
		result = GcAuthAdhocDecryptCbcAuthData(&gcAuthAdhocWorkspace, data, size,
			&counter, &minimumCounter, context->ready.data.words, tag, decrypt,
			checkCounter);
		if (result == 0 && checkCounter) {
			bytes = context->ready.peerCounter;
			bytes[0] = (unsigned char)counter;
			bytes[4] = (unsigned char)(counter >> 32);
			bytes[5] = (unsigned char)(counter >> 40);
			bytes[6] = (unsigned char)(counter >> 48);
			bytes[7] = (unsigned char)(counter >> 56);
			bytes[1] = (unsigned char)(counter >> 8);
			bytes[2] = (unsigned char)(counter >> 16);
			bytes[3] = (unsigned char)(counter >> 24);
		}
	}
	if (sceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		result = (int)0x808a010a;
	return result;
}

int GcAuthAdhocBroadCastDecrypt160Stack(const GcAuthAdhocUniCastDecrypt160Request *request)
{
	return GcAuthAdhocBroadCastDecrypt160(request->context, request->data,
		request->size, request->tag, request->decrypt, request->checkCounter);
}
