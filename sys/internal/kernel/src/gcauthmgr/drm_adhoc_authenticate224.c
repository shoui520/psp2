/* Authenticate the peer certificate and complete the 224-bit Adhoc exchange. */
#include <threadmgr/threadmgr.h>
#include "adhoc.h"

extern SceUID gcAuthAdhocSemaphore;
extern int gcAuthAdhocInitialized;
extern uint64_t gcAuthAdhocCounter;
extern unsigned char gcAuthAdhocRandom[32];
extern GcAuthAdhocWorkspace gcAuthAdhocWorkspace;
extern GcAuthAdhocHashScratch gcAuthAdhocHashScratch;
extern GcAuthAdhocCertificate gcAuthAdhocCertificate;
extern GcAuthAdhocSigningScratch gcAuthAdhocSigningScratch;
extern GcAuthAdhocMultiplyScratch gcAuthAdhocMultiplyScratch;
int GcAuthExecuteKirkCommand(void *output, const void *input,
		unsigned int command, unsigned int size, unsigned int keyId);

int GcAuthAdhocAuthenticateResponse224(
		const GcAuthAdhocAuthenticateResponse224Request *request)
{
	GcAuthAdhocContext224 *context = request->context;
	const unsigned char *packet = request->packet;
	unsigned char *output = request->output;
	GcAuthAdhocAuthenticatedResponse224 *response =
		&gcAuthAdhocWorkspace.authenticatedResponse224.payload;
	uint64_t counter = 0;
	GcAuthAdhocAuthData data;
	unsigned char serializedCounter[8];
	GcAuthAdhocAuthTag tag;
	GcAuthAdhocSignedMessage224 message;
	uint64_t outgoingCounter;
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
	result = GcAuthAdhocDecryptPacket(packet, 368);
	if (result != 0)
		goto finish;
	result = GcAuthAdhocHash224Spans((const unsigned char *)&response->certificate,
		72, NULL, 0);
	if (result != 0)
		goto finish;
	result = GcAuthAdhocReadCertificate224();
	if (result != 0)
		goto finish;
	if (GcAuthAdhocVerify224(&gcAuthAdhocCertificate.certificate224.fields.publicKey,
			gcAuthAdhocHashScratch.digest224,
			&response->certificateSignature) != 0) {
		result = 0x808A0111;
		goto finish;
	}
	result = GcAuthAdhocHash224Spans((const unsigned char *)&response->message,
		128, NULL, 0);
	if (result != 0)
		goto finish;
	if (GcAuthAdhocVerify224(&response->certificate.publicKey,
			gcAuthAdhocHashScratch.digest224, &response->signature) != 0) {
		result = 0x808A0113;
		goto finish;
	}
	cursor = context->initialized.keyPair.publicKey.serialized;
	source = response->message.remotePublicKey.serialized;
	end = cursor + 56;
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
	for (index = 0; index != 28; index++)
		gcAuthAdhocMultiplyScratch.input224.privateKey[index] =
			context->initialized.keyPair.privateKey[index];
	cursor = gcAuthAdhocMultiplyScratch.input224.publicKey.serialized;
	source = response->message.localPublicKey.serialized;
	end = cursor + 56;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	if (GcAuthExecuteKirkCommand(&gcAuthAdhocMultiplyScratch,
			&gcAuthAdhocMultiplyScratch, 21, 84, 0) != 0) {
		result = 0x808A0106;
		goto finish;
	}
	result = GcAuthAdhocHash224Spans(gcAuthAdhocMultiplyScratch.output224.point.x,
		28, (const unsigned char *)&response->message, 128);
	if (result != 0)
		goto finish;
	cursor = gcAuthAdhocHashScratch.digest224;
	source = response->digest;
	end = cursor + 28;
	do {
		result |= *cursor++ ^ *source++;
	} while (cursor != end);
	if (result != 0) {
		result = 0x808A0115;
		goto finish;
	}
	cursor = message.localPublicKey.serialized;
	source = response->message.remotePublicKey.serialized;
	end = cursor + 56;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	cursor = message.remotePublicKey.serialized;
	source = response->message.localPublicKey.serialized;
	end = cursor + 56;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	cursor = message.certificatePrefix;
	source = response->certificate.certificatePrefix;
	end = cursor + 16;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	result = GcAuthAdhocHash224Spans((const unsigned char *)&message,
		128, NULL, 0);
	if (result != 0)
		goto finish;
	result = GcAuthAdhocReadCertificate224();
	if (result != 0)
		goto finish;
	cursor = gcAuthAdhocSigningScratch.input224.encryptedPrivateKey;
	source = gcAuthAdhocCertificate.certificate224.fields.encryptedPrivateKey;
	end = cursor + 32;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	cursor = gcAuthAdhocSigningScratch.input224.digest;
	source = gcAuthAdhocHashScratch.digest224;
	end = cursor + 28;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	if (GcAuthExecuteKirkCommand(&gcAuthAdhocSigningScratch,
			&gcAuthAdhocSigningScratch, 23, 60, 0) != 0) {
		result = 0x808A0105;
		goto finish;
	}
	result = GcAuthAdhocHash224Spans(gcAuthAdhocMultiplyScratch.output224.point.x,
		28, (const unsigned char *)&message, 128);
	if (result != 0)
		goto finish;
	GcAuthAdhocBuildExtendedMessage((unsigned char *)context,
		gcAuthAdhocMultiplyScratch.output224.point.x, 28,
		NULL, 0, NULL, 0, NULL, 0);
	for (index = 0; index != 32; index++)
		data.bytes[index] = gcAuthAdhocRandom[index];
	result = GcAuthAdhocEncryptAuthData(&gcAuthAdhocWorkspace.auth,
		&data, &counter, (const unsigned int *)context->ready.seed, &tag);
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
	result = GcAuthAdhocEncryptPacket((const unsigned char *)&message, 128,
		(const unsigned char *)&gcAuthAdhocSigningScratch.signature224, 56,
		gcAuthAdhocHashScratch.digest224, 28, data.bytes, 32,
		serializedCounter, 8, (const unsigned char *)&tag, 32);
	if (result != 0)
		goto finish;
	source = (const unsigned char *)gcAuthAdhocWorkspace.counterStream.blocks;
	end = output + 304;
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
	if (sceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		result = 0x808A010A;
	return result;
}
