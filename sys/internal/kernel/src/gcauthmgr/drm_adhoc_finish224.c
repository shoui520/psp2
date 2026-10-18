/* Complete the 224-bit Adhoc key exchange. */
#include <threadmgr/threadmgr.h>
#include "adhoc.h"

extern SceUID gcAuthAdhocSemaphore;
extern int gcAuthAdhocInitialized;
extern GcAuthAdhocWorkspace gcAuthAdhocWorkspace;
int GcAuthAdhocDecryptPacket(const unsigned char *data, unsigned int size);
void GcAuthAdhocBuildExtendedMessage(unsigned char *output,
		const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize);
int GcAuthAdhocDecryptAuthData(GcAuthAdhocAuthScratch *scratch,
		GcAuthAdhocAuthData *data, uint64_t *counter, int unused,
		const unsigned int *seed, const GcAuthAdhocAuthTag *tag);

int GcAuthAdhocFinishContext224(const GcAuthAdhocFinishContext224Request *request)
{
	GcAuthAdhocContext224 *context = request->context;
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
	GcAuthAdhocBuildExtendedMessage((unsigned char *)context,
		context->ready.seed, 28, temporary.decoded.data.bytes, 32,
		temporary.decoded.counter, 8, NULL, 0);
	result = GcAuthAdhocDecryptAuthData(&gcAuthAdhocWorkspace.auth,
		&context->ready.data, &counter, (int)(uintptr_t)&minimumCounter,
		(const unsigned int *)context->ready.seed, &temporary.decoded.tag);
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
