/* Answer a peer challenge with a generated 160-bit key pair. */
#include <threadmgr/threadmgr.h>
#include <sysmem/utils.h>
#include "adhoc.h"

extern SceUID gcAuthAdhocSemaphore;
extern int gcAuthAdhocInitialized;
extern GcAuthAdhocWorkspace gcAuthAdhocWorkspace;
extern GcAuthAdhocHashScratch gcAuthAdhocHashScratch;
extern GcAuthAdhocCertificate gcAuthAdhocCertificate;
extern GcAuthAdhocSigningScratch gcAuthAdhocSigningScratch;
extern GcAuthAdhocMultiplyScratch gcAuthAdhocMultiplyScratch;
extern GcAuthAdhocGeneratedKeyPair gcAuthAdhocGeneratedKeyPair;
int GcAuthExecuteKirkCommand(void *output, const void *input,
		unsigned int command, unsigned int size, unsigned int keyId);

int GcAuthAdhocAnswerChallenge160(const GcAuthAdhocAnswerChallenge160Request *request)
{
	GcAuthAdhocContext160 *context = request->context;
	const unsigned char *packet = request->packet;
	unsigned char *output = request->output;
	GcAuthAdhocChallenge160 *challenge = &gcAuthAdhocWorkspace.challenge160.payload;
	GcAuthAdhocSignedMessage160 message;
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
	result = GcAuthAdhocDecryptPacket(packet, 160);
	if (result != 0)
		goto finish;
	result = SceKernelUtilsForDriver_87DC7F2F(&challenge->signedCertificate.certificate, 56,
		gcAuthAdhocHashScratch.digest160);
	if (result != 0)
		goto finish;
	result = GcAuthAdhocReadCertificate160();
	if (result != 0)
		goto finish;
	if (GcAuthAdhocVerify160(&gcAuthAdhocCertificate.certificate160.fields.publicKey,
			gcAuthAdhocHashScratch.digest160,
			&challenge->signedCertificate.signature) != 0) {
		result = 0x808A0111;
		goto finish;
	}
	if (GcAuthExecuteKirkCommand(&gcAuthAdhocGeneratedKeyPair,
			NULL, 12, 0, 0) != 0) {
		result = 0x808A0101;
		goto finish;
	}
	for (index = 0; index != 20; index++)
		gcAuthAdhocMultiplyScratch.input160.privateKey[index] =
			gcAuthAdhocGeneratedKeyPair.key160.privateKey[index];
	cursor = gcAuthAdhocMultiplyScratch.input160.publicKey.serialized;
	source = challenge->publicKey.serialized;
	end = cursor + 40;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	if (GcAuthExecuteKirkCommand(&gcAuthAdhocMultiplyScratch,
			&gcAuthAdhocMultiplyScratch, 13, 60, 0) != 0) {
		result = 0x808A0104;
		goto finish;
	}
	cursor = message.localPublicKey.serialized;
	source = gcAuthAdhocGeneratedKeyPair.key160.publicKey.serialized;
	end = cursor + 40;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	cursor = message.remotePublicKey.serialized;
	source = challenge->publicKey.serialized;
	end = cursor + 40;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	cursor = message.certificatePrefix;
	source = challenge->signedCertificate.certificate.certificatePrefix;
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
		challenge->signedCertificate.certificate.publicKey.serialized, 40,
		(const unsigned char *)&message, 80,
		gcAuthAdhocCertificate.certificate160.serialized, 16,
		gcAuthAdhocMultiplyScratch.output160.point.x, 20);
	result = GcAuthAdhocEncryptPacket((const unsigned char *)&message, 96,
		(const unsigned char *)&gcAuthAdhocSigningScratch.signature160, 40,
		gcAuthAdhocHashScratch.digest160, 20,
		gcAuthAdhocCertificate.certificate160.serialized, 96,
		NULL, 0, NULL, 0);
	if (result != 0)
		goto finish;
	source = (const unsigned char *)gcAuthAdhocWorkspace.counterStream.blocks;
	end = output + 272;
	do {
		*output++ = *source++;
	} while (output != end);
finish:
	cursor = (unsigned char *)&message;
	end = cursor + sizeof(message);
	do {
		*cursor++ = 0;
	} while (cursor != end);
	if (sceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		result = 0x808A010A;
	return result;
}
