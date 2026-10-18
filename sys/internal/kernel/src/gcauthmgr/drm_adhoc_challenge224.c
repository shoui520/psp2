/* Answer a peer challenge with a generated 224-bit key pair. */
#include <psp2kern/kernel/threadmgr.h>
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
int GcAuthAdhocDecryptPacket(const unsigned char *data, unsigned int size);
int GcAuthAdhocReadCertificate224(void);
int GcAuthAdhocHash224Spans(const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize);
int GcAuthAdhocVerify224(const GcAuthAdhocPublicKey224 *publicKey,
		const unsigned char *digest, const GcAuthAdhocSignature224 *signature);
int GcAuthExecuteKirkCommand(void *output, const void *input,
		unsigned int command, unsigned int size, unsigned int keyId);
void GcAuthAdhocBuildExtendedMessage(unsigned char *output,
		const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize);
int GcAuthAdhocEncryptPacket(const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize,
		const unsigned char *fifth, unsigned int fifthSize,
		const unsigned char *sixth, unsigned int sixthSize);

int GcAuthAdhocAnswerChallenge224(const GcAuthAdhocAnswerChallenge224Request *request)
{
	GcAuthAdhocContext224 *context = request->context;
	const unsigned char *packet = request->packet;
	unsigned char *output = request->output;
	GcAuthAdhocChallenge224 *challenge = &gcAuthAdhocWorkspace.challenge224.payload;
	GcAuthAdhocSignedMessage224 message;
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
	result = GcAuthAdhocDecryptPacket(packet, 208);
	if (result != 0)
		goto finish;
	result = SceKernelUtilsForDriver_9EA9D4DC(&challenge->signedCertificate.certificate, 72,
		gcAuthAdhocHashScratch.digest224);
	if (result != 0)
		goto finish;
	result = GcAuthAdhocReadCertificate224();
	if (result != 0)
		goto finish;
	if (GcAuthAdhocVerify224(&gcAuthAdhocCertificate.certificate224.fields.publicKey,
			gcAuthAdhocHashScratch.digest224,
			&challenge->signedCertificate.signature) != 0) {
		result = 0x808A0111;
		goto finish;
	}
	if (GcAuthExecuteKirkCommand(&gcAuthAdhocGeneratedKeyPair,
			NULL, 20, 0, 0) != 0) {
		result = 0x808A0101;
		goto finish;
	}
	for (index = 0; index != 28; index++)
		gcAuthAdhocMultiplyScratch.input224.privateKey[index] =
			gcAuthAdhocGeneratedKeyPair.key224.privateKey[index];
	cursor = gcAuthAdhocMultiplyScratch.input224.publicKey.serialized;
	source = challenge->publicKey.serialized;
	end = cursor + 56;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	if (GcAuthExecuteKirkCommand(&gcAuthAdhocMultiplyScratch,
			&gcAuthAdhocMultiplyScratch, 21, 84, 0) != 0) {
		result = 0x808A0104;
		goto finish;
	}
	cursor = message.localPublicKey.serialized;
	source = gcAuthAdhocGeneratedKeyPair.key224.publicKey.serialized;
	end = cursor + 56;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	cursor = message.remotePublicKey.serialized;
	source = challenge->publicKey.serialized;
	end = cursor + 56;
	do {
		*cursor++ = *source++;
	} while (cursor != end);
	cursor = message.certificatePrefix;
	source = challenge->signedCertificate.certificate.certificatePrefix;
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
		challenge->signedCertificate.certificate.publicKey.serialized, 56,
		(const unsigned char *)&message, 112,
		gcAuthAdhocCertificate.certificate224.serialized, 16,
		gcAuthAdhocMultiplyScratch.output224.point.x, 28);
	result = GcAuthAdhocEncryptPacket((const unsigned char *)&message, 128,
		(const unsigned char *)&gcAuthAdhocSigningScratch.signature224, 56,
		gcAuthAdhocHashScratch.digest224, 28,
		gcAuthAdhocCertificate.certificate224.serialized, 128,
		NULL, 0, NULL, 0);
	if (result != 0)
		goto finish;
	source = (const unsigned char *)gcAuthAdhocWorkspace.counterStream.blocks;
	end = output + 368;
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
