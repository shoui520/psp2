/* Authenticate the peer certificate and complete the 160-bit Adhoc exchange. */
#include <psp2kern/kernel/threadmgr.h>
#include "adhoc.h"

extern SceUID gcAuthAdhocSemaphore;
extern int gcAuthAdhocInitialized;
extern uint64_t gcAuthAdhocCounter;
extern unsigned char gcAuthAdhocRandom[32];
extern GcAuthAdhocWorkspace gcAuthAdhocWorkspace;
extern GcAuthAdhocHashScratch gcAuthAdhocHashScratch;
extern GcAuthAdhocCertificate gcAuthAdhocCertificate;
extern GcAuthAdhocSigningScratch gcAuthAdhocSigningScratch;
GcAuthAdhocMultiplyScratch gcAuthAdhocMultiplyScratch;

int GcAuthAdhocDecryptPacket(const unsigned char *data, unsigned int size);
int GcAuthAdhocReadCertificate160(void);
int GcAuthAdhocHashSpans(GcAuthAdhocHashMessage *message,
		const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize);
int GcAuthAdhocVerify160(const GcAuthAdhocPublicKey160 *publicKey,
		const unsigned char *digest, const GcAuthAdhocSignature160 *signature);
int GcAuthExecuteKirkCommand(void *output, const void *input,
		unsigned int command, unsigned int size, unsigned int keyId);
void GcAuthAdhocBuildMessage(unsigned char *output,
		const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize);
int GcAuthAdhocEncryptCbcAuthData(GcAuthAdhocWorkspace *scratch,
		unsigned char *data, unsigned int size, uint64_t *counter,
		const unsigned int *seed, GcAuthAdhocAuthTag *tag, int encrypt);
int GcAuthAdhocEncryptPacket(const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize,
		const unsigned char *fifth, unsigned int fifthSize,
		const unsigned char *sixth, unsigned int sixthSize);

int GcAuthAdhocAuthenticateResponse160(
		const GcAuthAdhocAuthenticateResponse160Request *request)
{
	GcAuthAdhocContext160 *context = request->context;
	const unsigned char *packet = request->packet;
	unsigned char *output = request->output;
	GcAuthAdhocAuthenticatedResponse160 *response =
		&gcAuthAdhocWorkspace.authenticatedResponse160.payload;
	uint64_t counter = 0;
	GcAuthAdhocAuthData data;
	unsigned char serializedCounter[8];
	GcAuthAdhocAuthTag tag;
	GcAuthAdhocSignedMessage160 message;
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
	result = GcAuthAdhocDecryptPacket(packet, 272);
	if (result != 0)
		goto finish;
	result = GcAuthAdhocHashSpans(&gcAuthAdhocHashScratch.hashMessage,
		(const unsigned char *)&response->certificate, 56, NULL, 0, NULL, 0);
	if (result != 0)
		goto finish;
	result = GcAuthAdhocReadCertificate160();
	if (result != 0)
		goto finish;
	if (GcAuthAdhocVerify160(&gcAuthAdhocCertificate.certificate160.fields.publicKey,
			gcAuthAdhocHashScratch.digest160,
			&response->certificateSignature) != 0) {
		result = 0x808A0111;
		goto finish;
	}
	result = GcAuthAdhocHashSpans(&gcAuthAdhocHashScratch.hashMessage,
		(const unsigned char *)&response->message, 96, NULL, 0, NULL, 0);
	if (result != 0)
		goto finish;
	if (GcAuthAdhocVerify160(&response->certificate.publicKey,
			gcAuthAdhocHashScratch.digest160, &response->signature) != 0) {
		result = 0x808A0113;
		goto finish;
	}
	cursor = context->initialized.keyPair.publicKey.serialized;
	source = response->message.remotePublicKey.serialized;
	end = cursor + 40;
	do {
		result |= *cursor++ ^ *source++;
	} while (cursor != end);
	if (result != 0) {
		result = 0x808A0118;
		goto finish;
	}
	cursor = context->initialized.certificatePrefix;
	source = response->message.certificatePrefix;
	end = cursor + 16;
	do {
		result |= *cursor++ ^ *source++;
	} while (cursor != end);
	if (result != 0) {
		result = 0x808A0114;
		goto finish;
	}
	for (index = 0; index != 20; index++)
		gcAuthAdhocMultiplyScratch.input160.privateKey[index] =
			context->initialized.keyPair.privateKey[index];
	cursor = gcAuthAdhocMultiplyScratch.input160.publicKey.serialized;
	source = response->message.localPublicKey.serialized;
	end = cursor + 40;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	if (GcAuthExecuteKirkCommand(&gcAuthAdhocMultiplyScratch,
			&gcAuthAdhocMultiplyScratch, 13, 60, 0) != 0) {
		result = 0x808A0106;
		goto finish;
	}
	result = GcAuthAdhocHashSpans(&gcAuthAdhocHashScratch.hashMessage,
		gcAuthAdhocMultiplyScratch.output160.point.x, 20,
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
		result = 0x808A0115;
		goto finish;
	}
	cursor = message.localPublicKey.serialized;
	source = response->message.remotePublicKey.serialized;
	end = cursor + 40;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	cursor = message.remotePublicKey.serialized;
	source = response->message.localPublicKey.serialized;
	end = cursor + 40;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	cursor = message.certificatePrefix;
	source = response->certificate.certificatePrefix;
	end = cursor + 16;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	result = GcAuthAdhocHashSpans(&gcAuthAdhocHashScratch.hashMessage,
		(const unsigned char *)&message, 96, NULL, 0, NULL, 0);
	if (result != 0)
		goto finish;
	result = GcAuthAdhocReadCertificate160();
	if (result != 0)
		goto finish;
	cursor = gcAuthAdhocSigningScratch.input160.encryptedPrivateKey;
	source = gcAuthAdhocCertificate.certificate160.fields.encryptedPrivateKey;
	end = cursor + 32;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	cursor = gcAuthAdhocSigningScratch.input160.digest;
	source = gcAuthAdhocHashScratch.digest160;
	end = cursor + 20;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	if (GcAuthExecuteKirkCommand(&gcAuthAdhocSigningScratch,
			&gcAuthAdhocSigningScratch, 16, 52, 0) != 0) {
		result = 0x808A0105;
		goto finish;
	}
	result = GcAuthAdhocHashSpans(&gcAuthAdhocHashScratch.hashMessage,
		gcAuthAdhocMultiplyScratch.output160.point.x, 20,
		(const unsigned char *)&message, 96, NULL, 0);
	if (result != 0)
		goto finish;
	GcAuthAdhocBuildMessage((unsigned char *)context,
		gcAuthAdhocMultiplyScratch.output160.point.x, 20,
		NULL, 0, NULL, 0, NULL, 0);
	for (index = 0; index != 32; index++)
		data.bytes[index] = gcAuthAdhocRandom[index];
	result = GcAuthAdhocEncryptCbcAuthData(&gcAuthAdhocWorkspace,
		data.bytes, 32, &counter, (const unsigned int *)context->ready.seed,
		&tag, 1);
	if (result != 0)
		goto finish;
	outgoingCounter = gcAuthAdhocCounter;
	serializedCounter[0] = (unsigned char)outgoingCounter;
	serializedCounter[7] = (unsigned char)(outgoingCounter >> 56);
	serializedCounter[4] = (unsigned char)(outgoingCounter >> 32);
	serializedCounter[5] = (unsigned char)(outgoingCounter >> 40);
	serializedCounter[6] = (unsigned char)(outgoingCounter >> 48);
	serializedCounter[1] = (unsigned char)(outgoingCounter >> 8);
	serializedCounter[2] = (unsigned char)(outgoingCounter >> 16);
	serializedCounter[3] = (unsigned char)(outgoingCounter >> 24);
	result = GcAuthAdhocEncryptPacket((const unsigned char *)&message, 96,
		(const unsigned char *)&gcAuthAdhocSigningScratch.signature160, 40,
		gcAuthAdhocHashScratch.digest160, 20, data.bytes, 32,
		serializedCounter, 8, (const unsigned char *)&tag, 32);
	if (result != 0)
		goto finish;
	source = (const unsigned char *)gcAuthAdhocWorkspace.counterStream.blocks;
	end = output + 256;
	do {
		*output++ = *source++;
	} while (output != end);
finish:
	cursor = (unsigned char *)&message;
	end = cursor + sizeof(message);
	do {
		*cursor++ = 0;
	} while (cursor != end);
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
