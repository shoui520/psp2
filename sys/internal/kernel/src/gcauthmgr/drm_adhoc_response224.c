/* Verify the 224-bit Adhoc response and return the final packet. */
#include <psp2kern/kernel/threadmgr.h>
#include "adhoc.h"

extern SceUID gcAuthAdhocSemaphore;
extern int gcAuthAdhocInitialized;
extern GcAuthAdhocWorkspace gcAuthAdhocWorkspace;
extern unsigned char gcAuthAdhocRandom[32];
extern uint64_t gcAuthAdhocCounter;
extern GcAuthAdhocHashScratch gcAuthAdhocHashScratch;
int GcAuthAdhocDecryptPacket(const unsigned char *data, unsigned int size);
int GcAuthAdhocHash224Spans(const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize);
int GcAuthAdhocVerify224(const GcAuthAdhocPublicKey224 *publicKey,
		const unsigned char *digest, const GcAuthAdhocSignature224 *signature);
void GcAuthAdhocBuildExtendedMessage(unsigned char *output,
		const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize);
int GcAuthAdhocDecryptAuthData(GcAuthAdhocAuthScratch *scratch,
		GcAuthAdhocAuthData *data, uint64_t *counter, int unused,
		const unsigned int *seed, const GcAuthAdhocAuthTag *tag);
int GcAuthAdhocEncryptAuthData(GcAuthAdhocAuthScratch *scratch,
		GcAuthAdhocAuthData *data, uint64_t *counter,
		const unsigned int *seed, GcAuthAdhocAuthTag *tag);
int GcAuthAdhocEncryptPacket(const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize,
		const unsigned char *fifth, unsigned int fifthSize,
		const unsigned char *sixth, unsigned int sixthSize);

int GcAuthAdhocVerifyResponse224(const GcAuthAdhocVerifyResponse224Request *request)
{
	GcAuthAdhocContext224 *context = request->context;
	const unsigned char *packet = request->packet;
	unsigned char *output = request->output;
	GcAuthAdhocResponse224 *response = &gcAuthAdhocWorkspace.response224.payload;
	uint64_t counter = 0;
	uint64_t minimumCounter = 0;
	GcAuthAdhocAuthData data;
	unsigned char serializedCounter[8];
	GcAuthAdhocAuthTag tag;
	uint64_t outgoingCounter;
	unsigned char *cursor;
	unsigned char *end;
	const unsigned char *source;
	unsigned int index;
	int result;

	if (ksceKernelWaitSema(gcAuthAdhocSemaphore, 1, NULL) != 0)
		return 0x808A010A;
	if (packet == NULL || context == NULL || output == NULL) {
		result = 0x808A0124;
		goto finish;
	}
	result = 0x808A010F;
	if (!gcAuthAdhocInitialized)
		goto finish;
	result = GcAuthAdhocDecryptPacket(packet, 304);
	if (result != 0)
		goto finish;
	result = GcAuthAdhocHash224Spans((const unsigned char *)&response->message,
		128, NULL, 0);
	if (result != 0)
		goto finish;
	if (GcAuthAdhocVerify224(&context->pending.verificationKey,
			gcAuthAdhocHashScratch.digest224, &response->signature) != 0) {
		result = 0x808A0117;
		goto finish;
	}
	cursor = context->pending.localPublicKey.serialized;
	source = response->message.remotePublicKey.serialized;
	end = cursor + 56;
	do {
		result |= *cursor++ ^ *source++;
	} while (cursor != end);
	if (result != 0) {
		result = 0x808A011D;
		goto finish;
	}
	cursor = context->pending.remotePublicKey.serialized;
	source = response->message.localPublicKey.serialized;
	end = cursor + 56;
	do {
		result |= *cursor++ ^ *source++;
	} while (cursor != end);
	if (result != 0) {
		result = 0x808A0118;
		goto finish;
	}
	cursor = context->pending.certificatePrefix;
	source = response->message.certificatePrefix;
	end = cursor + 16;
	do {
		result |= *cursor++ ^ *source++;
	} while (cursor != end);
	if (result != 0) {
		result = 0x808A011E;
		goto finish;
	}
	result = GcAuthAdhocHash224Spans(context->pending.sharedSecret, 28,
		(const unsigned char *)&response->message, 128);
	if (result != 0)
		goto finish;
	cursor = gcAuthAdhocHashScratch.digest224;
	source = response->digest;
	end = cursor + 28;
	do {
		result |= *cursor++ ^ *source++;
	} while (cursor != end);
	if (result != 0) {
		result = 0x808A0119;
		goto finish;
	}
	GcAuthAdhocBuildExtendedMessage((unsigned char *)context,
		context->pending.sharedSecret, 28, response->data.bytes, 32,
		response->counter, 8, NULL, 0);
	result = GcAuthAdhocDecryptAuthData(&gcAuthAdhocWorkspace.auth,
		&context->ready.data, &counter, (int)(uintptr_t)&minimumCounter,
		(const unsigned int *)context->ready.seed, &response->tag);
	if (result != 0)
		goto finish;
	counter = 0;
	for (index = 0; index != 32; index++)
		data.bytes[index] = gcAuthAdhocRandom[index];
	result = GcAuthAdhocEncryptAuthData(&gcAuthAdhocWorkspace.auth,
		&data, &counter, (const unsigned int *)context->ready.seed, &tag);
	if (result != 0)
		goto finish;
	outgoingCounter = gcAuthAdhocCounter;
	serializedCounter[0] = (unsigned char)outgoingCounter;
	serializedCounter[5] = (unsigned char)(outgoingCounter >> 40);
	serializedCounter[4] = (unsigned char)(outgoingCounter >> 32);
	serializedCounter[6] = (unsigned char)(outgoingCounter >> 48);
	serializedCounter[7] = (unsigned char)(outgoingCounter >> 56);
	serializedCounter[1] = (unsigned char)(outgoingCounter >> 8);
	serializedCounter[2] = (unsigned char)(outgoingCounter >> 16);
	serializedCounter[3] = (unsigned char)(outgoingCounter >> 24);
	result = GcAuthAdhocEncryptPacket(data.bytes, 32, serializedCounter, 8,
		(const unsigned char *)&tag, 32, NULL, 0, NULL, 0, NULL, 0);
	if (result != 0)
		goto finish;
	source = (const unsigned char *)gcAuthAdhocWorkspace.counterStream.blocks;
	end = output + 96;
	do {
		*output++ = *source++;
	} while (output != end);
finish:
	cursor = serializedCounter;
	end = cursor + sizeof(serializedCounter);
	do {
		*cursor++ = 0;
	} while (cursor != end);
	cursor = (unsigned char *)&tag;
	end = cursor + sizeof(tag);
	do {
		*cursor++ = 0;
	} while (cursor != end);
	cursor = data.bytes;
	end = cursor + sizeof(data);
	do {
		*cursor++ = 0;
	} while (cursor != end);
	if (ksceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		result = 0x808A010A;
	return result;
}
