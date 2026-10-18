/* Verify the 160-bit Adhoc response and return the final packet. */
#include <threadmgr/threadmgr.h>
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

extern unsigned char gcAuthAdhocRandom[32];
extern uint64_t gcAuthAdhocCounter;
extern GcAuthAdhocHashScratch gcAuthAdhocHashScratch;
int GcAuthAdhocHashSpans(GcAuthAdhocHashMessage *message,
		const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize);
int GcAuthAdhocVerify160(const GcAuthAdhocPublicKey160 *publicKey,
		const unsigned char *digest, const GcAuthAdhocSignature160 *signature);
int GcAuthAdhocEncryptCbcAuthData(GcAuthAdhocWorkspace *scratch,
		unsigned char *data, unsigned int size, uint64_t *counter,
		const unsigned int *seed, GcAuthAdhocAuthTag *tag, int encrypt);
int GcAuthAdhocEncryptPacket(const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize,
		const unsigned char *fifth, unsigned int fifthSize,
		const unsigned char *sixth, unsigned int sixthSize);

int GcAuthAdhocVerifyResponse160(const GcAuthAdhocVerifyResponse160Request *request)
{
	GcAuthAdhocContext160 *context = request->context;
	const unsigned char *packet = request->packet;
	unsigned char *output = request->output;
	GcAuthAdhocResponse160 *response = &gcAuthAdhocWorkspace.response160.payload;
	uint64_t counter = 0;
	uint64_t minimumCounter = 0;
	uint64_t outgoingCounter;
	GcAuthAdhocAuthData data;
	unsigned char serializedCounter[8];
	GcAuthAdhocAuthTag tag;
	unsigned char *cursor;
	unsigned char *end;
	const unsigned char *source;
	unsigned int index;
	int result;

	if (sceKernelWaitSema(gcAuthAdhocSemaphore, 1, NULL) != 0)
		return 0x808A010A;
	if (packet == NULL || context == NULL || output == NULL) {
		result = 0x808A0124;
		goto finish;
	}
	result = 0x808A010F;
	if (!gcAuthAdhocInitialized)
		goto finish;
	result = GcAuthAdhocDecryptPacket(packet, 256);
	if (result != 0)
		goto finish;
	result = GcAuthAdhocHashSpans(&gcAuthAdhocHashScratch.hashMessage,
		(const unsigned char *)&response->message, 96, NULL, 0, NULL, 0);
	if (result != 0)
		goto finish;
	if (GcAuthAdhocVerify160(&context->pending.verificationKey,
			gcAuthAdhocHashScratch.digest160, &response->signature) != 0) {
		result = 0x808A0117;
		goto finish;
	}
	cursor = context->pending.localPublicKey.serialized;
	source = response->message.remotePublicKey.serialized;
	end = cursor + 40;
	do {
		result |= *cursor++ ^ *source++;
	} while (cursor != end);
	if (result != 0) {
		result = 0x808A011D;
		goto finish;
	}
	cursor = context->pending.remotePublicKey.serialized;
	source = response->message.localPublicKey.serialized;
	end = cursor + 40;
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
	result = GcAuthAdhocHashSpans(&gcAuthAdhocHashScratch.hashMessage,
		context->pending.sharedSecret, 20,
		(const unsigned char *)&response->message, 96, NULL, 0);
	if (result != 0)
		goto finish;
	cursor = gcAuthAdhocHashScratch.digest160;
	source = response->digest;
	end = cursor + 20;
	do {
		result |= *cursor++ ^ *source++;
	} while (cursor != end);
	if (result != 0) {
		result = 0x808A0119;
		goto finish;
	}
	GcAuthAdhocBuildMessage((unsigned char *)context,
		context->pending.sharedSecret, 20, response->data.bytes, 32,
		response->counter, 8, NULL, 0);
	result = GcAuthAdhocDecryptCbcAuthData(&gcAuthAdhocWorkspace,
		context->ready.data.bytes, 32, &counter, &minimumCounter,
		(const unsigned int *)context->ready.seed, &response->tag, 1, 0);
	if (result != 0)
		goto finish;
	counter = 0;
	for (index = 0; index != 32; index++)
		data.bytes[index] = gcAuthAdhocRandom[index];
	result = GcAuthAdhocEncryptCbcAuthData(&gcAuthAdhocWorkspace,
		data.bytes, 32, &counter, (const unsigned int *)context->ready.seed,
		&tag, 1);
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
	if (sceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		result = 0x808A010A;
	return result;
}
