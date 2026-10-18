/* Complete the 160-bit Adhoc key exchange. */
#include <psp2kern/kernel/threadmgr.h>
#include "adhoc.h"

extern SceUID gcAuthAdhocSemaphore;
extern int gcAuthAdhocInitialized;
extern GcAuthAdhocWorkspace gcAuthAdhocWorkspace;
int GcAuthAdhocDecryptPacket(const unsigned char *data, unsigned int size);
void GcAuthAdhocBuildMessage(unsigned char *output,
		const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize);
int GcAuthAdhocDecryptCbcAuthData(GcAuthAdhocWorkspace *scratch,
		unsigned char *data, unsigned int size, uint64_t *counter,
		const uint64_t *minimumCounter, const unsigned int *seed,
		const GcAuthAdhocAuthTag *tag, int decrypt, int checkCounter);

int GcAuthAdhocFinishContext160(const GcAuthAdhocFinishContext160Request *request)
{
	GcAuthAdhocContext160 *context = request->context;
	const GcAuthAdhocFinalPacket *packet = request->packet;
	uint64_t counter = 0;
	uint64_t minimumCounter = 0;
	GcAuthAdhocFinalPacket temporary;
	unsigned char *cursor;
	unsigned char *end;
	const unsigned char *source;
	unsigned int index;
	int result;

	if (sceKernelWaitSema(gcAuthAdhocSemaphore, 1, NULL) != 0)
		return 0x808A010A;
	if (context == NULL || packet == NULL) {
		result = 0x808A0124;
		goto finish;
	}
	result = 0x808A010F;
	if (!gcAuthAdhocInitialized)
		goto finish;
	for (index = 0; index != sizeof(temporary); index++)
		temporary.ciphertext[index] = packet->ciphertext[index];
	result = GcAuthAdhocDecryptPacket(temporary.ciphertext, sizeof(temporary));
	if (result != 0)
		goto finish;
	source = gcAuthAdhocWorkspace.decrypted.payload;
	cursor = (unsigned char *)&temporary;
	end = cursor + sizeof(temporary);
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	GcAuthAdhocBuildMessage((unsigned char *)context, context->ready.seed, 20,
		temporary.decoded.data.bytes, 32, temporary.decoded.counter, 8,
		NULL, 0);
	result = GcAuthAdhocDecryptCbcAuthData(&gcAuthAdhocWorkspace,
		context->ready.data.bytes, 32, &counter, &minimumCounter,
		(const unsigned int *)context->ready.seed, &temporary.decoded.tag, 1, 0);
finish:
	cursor = (unsigned char *)&temporary;
	end = cursor + sizeof(temporary);
	do {
		*cursor++ = 0;
	} while (cursor != end);
	if (sceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		result = 0x808A010A;
	return result;
}
