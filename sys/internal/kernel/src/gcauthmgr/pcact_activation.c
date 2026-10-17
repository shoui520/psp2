/* Descriptive source boundary preserves the original out-of-line helper calls. */
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/ssmgr.h>
#include <psp2kern/kernel/rtc.h>
#include <marlin_hci/rsa.h>
#include "pcact.h"
#include "drm_crypto.h"
#include <psp2kern/kernel/threadmgr/misc.h>
#include <sysmem/utils.h>

GcAuthPcactScratch *gcAuthPcactScratchAddress;
SceConsoleId gcAuthPcactConsoleId;
GcAuthPcactChallengeWorkspace gcAuthPcactChallengeWorkspace;
unsigned char gcAuthPcactRtcRandom[20];

const unsigned char gcAuthPcactChallengeSalt[16] = {
	0xfe, 0x09, 0x4a, 0x99, 0x06, 0x96, 0x1a, 0x57,
	0xeb, 0xe0, 0x2b, 0x1d, 0xb2, 0x98, 0x2d, 0x84
};
const unsigned char gcAuthPcactChallengeMask[16] = {
	0x90, 0xab, 0x41, 0xee, 0xea, 0x3e, 0xd1, 0xec,
	0xd8, 0xc4, 0x92, 0xfd, 0x66, 0x15, 0x5e, 0x6c
};
const unsigned char gcAuthPcactEncryptChallengeSalt[16] = {
	0xca, 0x0e, 0x20, 0x6e, 0xce, 0xe6, 0xfb, 0x66,
	0x68, 0x28, 0x65, 0x42, 0xef, 0x4f, 0xf8, 0x8f
};
const unsigned char gcAuthPcactRtcSalt[16] = {
	0x77, 0x25, 0x84, 0x03, 0xa9, 0x8e, 0x8f, 0x7c,
	0xf8, 0x16, 0x59, 0xd7, 0xda, 0x5f, 0xe9, 0x59
};
extern const unsigned char gcAuthPcactRtcAesKey[32];
extern const unsigned char gcAuthPcactRtcActivationAesKey[32];
extern unsigned int gcAuthPcactRtcRsaModulus[64];
extern unsigned int gcAuthPcactRtcRsaExponent[64];
extern const unsigned char gcAuthPsmAesIv[16];

int GcAuthDrmEncrypt(unsigned char *data, size_t size, int keyId);
int GcAuthGenerateRandom20(void *data);
int GcAuthExecuteKirkCommand(void *output, const void *input, int command,
			    size_t size, int keyId);
int GcAuthRsaVerify2048(const MarlinRsaData *signature,
		      const MarlinRsaData *digest,
		      const MarlinRsaExponentKey *key, int algorithm);

int GcAuthPcactGetAuthenticatedAesKey(const GcAuthPcactKeyRequest *request)
{
	const GcAuthPcactActivationBlock *activation = request->activation;
	const GcAuthPcactRandomRecord *random = request->random;
	unsigned char *outputKey = request->outputKey;
	const unsigned char *input;
	const unsigned char *comparison;
	unsigned char *cursor;
	unsigned int index;
	int result;
	int difference;

	if (ksceKernelWaitSema(gcAuthPcactSemaphore, 1, NULL) != 0)
		return (int)0x808a0206;
	if (random == NULL || activation == NULL || outputKey == NULL) {
		result = (int)0x808a0200;
		goto finish;
	}
	if (activation->format != 32 || activation->version != 2 ||
	    activation->mode != random->mode) {
		result = (int)0x808a0201;
		goto finish;
	}
	for (index = 0; index != 13; index++) {
		if (activation->reserved[index] != 0) {
			result = (int)0x808a0201;
			goto finish;
		}
	}
	for (index = 0; index != 16; index++)
		outputKey[index] = 0;
	gcAuthPcactScratchAddress = &gcAuthPcactScratch;
	gcAuthPcactScratch.hashInput.header.framing.size = 80;
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.hashInput.header.framing.salt[index] =
			gcAuthPcactActivationMask[index];
	input = (const unsigned char *)activation;
	for (index = 0; index != 64; index++)
		gcAuthPcactScratch.hashInput.serializedChallenge[index] = input[index];
	difference = SceKernelUtilsForDriver_87DC7F2F(gcAuthPcactScratch.hashInput.header.framing.salt,
			80, &gcAuthPcactScratch);
	if (difference != 0) {
		result = (int)0x808a0205;
		goto finish;
	}
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.hashInput.serializedChallenge[index] =
			gcAuthPcactScratch.hashInput.header.sha1[index];
	if (GcAuthDrmEncrypt(gcAuthPcactScratch.hashInput.serializedChallenge,
			16, 19) != 0) {
		result = (int)0x808a0204;
		goto finish;
	}
	input = activation->authenticated.mac;
	comparison = gcAuthPcactScratch.hashInput.serializedChallenge;
	do {
		unsigned int actual = *comparison++;
		unsigned int expected = *input++;

		difference |= actual ^ expected;
	} while (input != activation->authenticated.mac + 16);
	if (difference != 0) {
		result = (int)0x808a0207;
		goto finish;
	}
	for (index = 0; index != 48; index++)
		gcAuthPcactScratch.hashInput.serializedChallenge[index] =
			activation->authenticated.ciphertext[index];
	cursor = gcAuthPcactScratch.hashInput.serializedChallenge;
	for (index = 0; index != 48; index++)
		*cursor++ ^= gcAuthPcactActivationMask[index & 15];
	result = GcAuthPcactDecryptChallenge(48, 19);
	if (result != 0) {
		result = (int)0x808a0203;
		goto finish;
	}
	cursor = (unsigned char *)&gcAuthPcactScratch;
	for (index = 0; index != 48; index++)
		*cursor++ ^= gcAuthPcactActivationMask[index & 15];
	for (index = 0; index != 8; index++)
		gcAuthPcactScratch.decodedKey.comparisonKey[index] =
			gcAuthPcactScratch.decodedKey.nonceHalves[1][index];
	for (index = 0; index != 8; index++)
		gcAuthPcactScratch.decodedKey.comparisonKey[index + 8] =
			gcAuthPcactScratch.decodedKey.nonceHalves[0][index];
	input = random->derivedKey;
	comparison = gcAuthPcactScratch.decodedKey.comparisonKey;
	do {
		unsigned int actual = *comparison++;
		unsigned int expected = *input++;

		result |= actual ^ expected;
	} while (input != random->derivedKey + 16);
	if (result != 0) {
		result = (int)0x808a0208;
	} else {
		for (index = 0; index != 16; index++)
			outputKey[index] = random->randomKey[index];
	}
finish:
	cursor = (unsigned char *)&gcAuthPcactScratch;
	do {
		*cursor++ = 0;
	} while (cursor != (unsigned char *)&gcAuthPcactScratch + 184);
	if (ksceKernelSignalSema(gcAuthPcactSemaphore, 1) != 0)
		result = (int)0x808a0206;
	return result;
}

int GcAuthPcactCreateChallenge(const GcAuthPcactChallengeRequest *request)
{
	const unsigned char *entropy = request->entropy;
	const unsigned char *rtcSeconds = request->rtcSeconds;
	GcAuthPcactChallenge *output = request->output;
	GcAuthPcactRandomRecord *random = request->random;
	unsigned char consoleId[16];
	unsigned char *cursor;
	const unsigned char *input;
	unsigned int index;
	int permission;
	int result;

	if (ksceKernelWaitSema(gcAuthPcactSemaphore, 1, NULL) != 0)
		return (int)0x808a0206;
	permission = ksceKernelSetPermission(128);
	if (permission < 0) {
		result = (int)0x808a0300;
		goto signal;
	}
	if (rtcSeconds == NULL || entropy == NULL || random == NULL || output == NULL) {
		result = (int)0x808a0200;
		goto finish;
	}
	cursor = (unsigned char *)output;
	for (index = 0; index != sizeof(*output); index++)
		cursor[index] = 0;
	cursor = (unsigned char *)random;
	for (index = 0; index != sizeof(*random); index++)
		cursor[index] = 0;
	if (ksceSblAimgrGetConsoleId(&gcAuthPcactConsoleId) < 0) {
		result = (int)0x808a0300;
		goto finish;
	}
	input = (const unsigned char *)&gcAuthPcactConsoleId;
	for (index = 0; index != 16; index++)
		consoleId[index] = input[index];
	if (GcAuthGenerateRandom20(&gcAuthPcactScratch) != 0) {
		result = (int)0x808a0202;
		goto finish;
	}
	for (index = 0; index != 16; index++)
		random->randomKey[index] = gcAuthPcactScratch.random[index];
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.hashInput.serializedChallenge[index] = random->randomKey[index];
	if (GcAuthPcactDecryptChallenge(16, 104) != 0) {
		result = (int)0x808a0203;
		goto finish;
	}
	for (index = 0; index != 16; index++)
		random->derivedKey[index] = gcAuthPcactScratch.random[index];
	random->mode = 0x80;
	for (index = 0; index != 16; index++)
		gcAuthPcactChallengeWorkspace.plaintext.derivedKey[index] =
			gcAuthPcactScratch.random[index];
	for (index = 0; index != 16; index++)
		gcAuthPcactChallengeWorkspace.plaintext.salt[index] =
			gcAuthPcactChallengeSalt[index];
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.hashInput.serializedChallenge[index] = consoleId[index];
	result = GcAuthPcactDecryptChallenge(16, 104);
	if (result != 0) {
		result = (int)0x808a0203;
		goto finish;
	}
	for (index = 0; index != 16; index++)
		gcAuthPcactChallengeWorkspace.plaintext.consoleDerivedKey[index] =
			gcAuthPcactScratch.random[index];
	for (index = 0; index != 8; index++)
		gcAuthPcactChallengeWorkspace.plaintext.rtcSeconds[index] = rtcSeconds[index];
	for (index = 0; index != 32; index++)
		gcAuthPcactChallengeWorkspace.plaintext.entropy[index] = entropy[index];
	gcAuthPcactChallengeWorkspace.plaintext.zeroFill[0] = 0;
	gcAuthPcactChallengeWorkspace.plaintext.zeroFill[1] = 0;
	gcAuthPcactChallengeWorkspace.plaintext.zeroFill[2] = 0;
	gcAuthPcactChallengeWorkspace.plaintext.zeroFill[3] = 0;
	gcAuthPcactChallengeWorkspace.plaintext.zeroFill[4] = 0;
	gcAuthPcactChallengeWorkspace.plaintext.zeroFill[5] = 0;
	gcAuthPcactChallengeWorkspace.plaintext.zeroFill[6] = 0;
	gcAuthPcactChallengeWorkspace.plaintext.zeroFill[7] = 0;
	input = (const unsigned char *)&gcAuthPcactChallengeWorkspace.plaintext;
	for (index = 0; index != sizeof(GcAuthPcactChallengeBody); index++)
		gcAuthPcactScratch.hashInput.serializedChallenge[index] = input[index];
	cursor = gcAuthPcactScratch.hashInput.serializedChallenge;
	for (index = 0; index != 96; index++)
		*cursor++ ^= gcAuthPcactChallengeMask[index & 15];
	result = GcAuthDrmEncrypt(gcAuthPcactScratch.hashInput.serializedChallenge, 96, 19);
	if (result != 0) {
		result = (int)0x808a0204;
		goto finish;
	}
	cursor = gcAuthPcactScratch.hashInput.serializedChallenge;
	for (index = 0; index != 96; index++)
		*cursor++ ^= gcAuthPcactChallengeMask[index & 15];
	gcAuthPcactChallengeWorkspace.encrypted.version = 1;
	gcAuthPcactChallengeWorkspace.encrypted.format = 32;
	gcAuthPcactChallengeWorkspace.encrypted.mode = 0x80;
	for (index = 0; index != 9; index++)
		gcAuthPcactChallengeWorkspace.encrypted.reserved[index] = 0;
	gcAuthPcactChallengeWorkspace.encrypted.reserved[9] = 0;
	gcAuthPcactChallengeWorkspace.encrypted.reserved[10] = 0;
	gcAuthPcactChallengeWorkspace.encrypted.reserved[11] = 0;
	gcAuthPcactChallengeWorkspace.encrypted.reserved[12] = 0;
	for (index = 0; index != 96; index++)
		gcAuthPcactChallengeWorkspace.encrypted.ciphertext[index] =
			gcAuthPcactScratch.hashInput.serializedChallenge[index];
	gcAuthPcactScratch.hashInput.header.framing.size = 128;
	gcAuthPcactScratchAddress = &gcAuthPcactScratch;
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.hashInput.header.framing.salt[index] =
			gcAuthPcactChallengeMask[index];
	input = (const unsigned char *)&gcAuthPcactChallengeWorkspace.encrypted;
	for (index = 0; index != 112; index++)
		gcAuthPcactScratch.hashInput.serializedChallenge[index] = input[index];
	result = SceKernelUtilsForDriver_87DC7F2F(gcAuthPcactScratch.hashInput.header.framing.salt,
			128, &gcAuthPcactScratch);
	if (result != 0) {
		result = (int)0x808a0205;
		goto finish;
	}
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.hashInput.serializedChallenge[index] =
			gcAuthPcactScratch.hashInput.header.sha1[index];
	if (GcAuthDrmEncrypt(gcAuthPcactScratch.hashInput.serializedChallenge, 16, 19) != 0) {
		result = (int)0x808a0204;
		goto finish;
	}
	cursor = (unsigned char *)&output->packet;
	for (index = 0; index != 112; index++)
		cursor[index] = input[index];
	for (index = 0; index != 16; index++)
		output->mac[index] = gcAuthPcactScratch.hashInput.serializedChallenge[index];
finish:
	cursor = (unsigned char *)&gcAuthPcactChallengeWorkspace;
	for (index = 0; index != sizeof(gcAuthPcactChallengeWorkspace); index++)
		cursor[index] = 0;
	cursor = (unsigned char *)&gcAuthPcactScratch;
	for (index = 0; index != 184; index++)
		cursor[index] = 0;
	ksceKernelSetPermission(permission);
signal:
	if (ksceKernelSignalSema(gcAuthPcactSemaphore, 1) != 0)
		result = (int)0x808a0206;
	return result;
}

int GcAuthPcactEncryptChallenge(const GcAuthPcactEncryptChallengeRequest *request)
{
	unsigned char mode = request->mode;
	const GcAuthPcactChallengeInput *input = request->input;
	const unsigned char *rtcSeconds = request->rtcSeconds;
	GcAuthPcactEncryptedChallenge *output = request->output;
	GcAuthPcactRandomRecord *random = request->random;
	unsigned char *cursor;
	const unsigned char *source;
	unsigned int index;
	int result;

	if (ksceKernelWaitSema(gcAuthPcactSemaphore, 1, NULL) != 0)
		return (int)0x808a0306;
	if (rtcSeconds == NULL || input == NULL || random == NULL || output == NULL) {
		result = (int)0x808a0300;
		goto finish;
	}
	if (input->format != 16 || input->version != 0) {
		result = (int)0x808a0301;
		goto finish;
	}
	for (index = 0; index != 14; index++) {
		if (input->reserved[index] != 0) {
			result = (int)0x808a0301;
			goto finish;
		}
	}
	for (index = 0; index != 128; index++)
		output->serialized[index] = 0;
	cursor = (unsigned char *)random;
	for (index = 0; index != 64; index++)
		cursor[index] = 0;
	if (GcAuthGenerateRandom20(&gcAuthPcactScratch) != 0) {
		result = (int)0x808a0302;
		goto finish;
	}
	for (index = 0; index != 16; index++)
		random->randomKey[index] = gcAuthPcactScratch.random[index];
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.hashInput.serializedChallenge[index] = random->randomKey[index];
	result = GcAuthPcactDecryptChallenge(16, 104);
	if (result != 0) {
		result = (int)0x808a0303;
		goto finish;
	}
	for (index = 0; index != 16; index++)
		random->derivedKey[index] = gcAuthPcactScratch.random[index];
	random->mode = mode;
	for (index = 0; index != 16; index++)
		gcAuthPcactChallengeWorkspace.plaintext.derivedKey[index] =
			gcAuthPcactScratch.random[index];
	for (index = 0; index != 16; index++)
		gcAuthPcactChallengeWorkspace.plaintext.salt[index] =
			gcAuthPcactEncryptChallengeSalt[index];
	for (index = 0; index != 8; index++)
		gcAuthPcactChallengeWorkspace.plaintext.rtcSeconds[index] = rtcSeconds[index];
	for (index = 0; index != 32; index++)
		gcAuthPcactChallengeWorkspace.plaintext.entropy[index] = input->entropy[index];
	cursor = (unsigned char *)&gcAuthPcactScratch.kirkChallenge;
	for (index = 0; index != 16; index++)
		cursor[index] = 0;
	gcAuthPcactScratch.kirkChallenge.mode = mode;
	source = (const unsigned char *)&gcAuthPcactChallengeWorkspace.plaintext;
	cursor = (unsigned char *)&gcAuthPcactScratch.kirkChallenge.plaintext;
	for (index = 0; index != 96; index++)
		cursor[index] = source[index];
	if (GcAuthExecuteKirkCommand(&gcAuthPcactScratch, &gcAuthPcactScratch,
			36, 128, 0) != 0) {
		result = (int)0x808a0304;
	} else {
		for (index = 0; index != 128; index++)
			output->serialized[index] =
				gcAuthPcactScratch.encryptedChallenge.serialized[index];
	}
finish:
	cursor = (unsigned char *)&gcAuthPcactChallengeWorkspace;
	for (index = 0; index != sizeof(gcAuthPcactChallengeWorkspace); index++)
		cursor[index] = 0;
	cursor = (unsigned char *)&gcAuthPcactScratch;
	for (index = 0; index != 184; index++)
		cursor[index] = 0;
	if (ksceKernelSignalSema(gcAuthPcactSemaphore, 1) != 0)
		result = (int)0x808a0306;
	return result;
}

int GcAuthPcactCreateRtcChallenge(const GcAuthPcactRtcChallengeRequest *request)
{
	SceSblGcAuthMgrSclkData1 *output = request->output;
	struct GcAuthAesCbcState state;
	unsigned char block[32];
	unsigned char *cursor;
	unsigned int index;
	unsigned int blockBytes;
	int result;

	if (ksceKernelWaitSema(gcAuthPcactSemaphore, 1, NULL) != 0)
		return (int)0x808a0605;
	if (output == NULL) {
		result = (int)0x808a0600;
		goto finish;
	}
	cursor = (unsigned char *)output;
	for (index = 0; index != sizeof(*output); index++)
		cursor[index] = 0;
	result = GcAuthGenerateRandom20(gcAuthPcactRtcRandom);
	if (result != 0) {
		result = (int)0x808a0602;
		goto finish;
	}
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.rtcPlaintext.random[index] = gcAuthPcactRtcRandom[index];
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.rtcPlaintext.salt[index] = gcAuthPcactRtcSalt[index];
	if (SceKernelUtilsForDriver_F12B6451(&state.aes, 128, 256, gcAuthPcactRtcAesKey) >= 0) {
		blockBytes = state.aes.encryption.blockWords * 4;
		for (index = 0; index != blockBytes; index++)
			state.chain[index] = gcAuthPsmAesIv[index];
	}
	blockBytes = state.aes.encryption.blockWords * 4;
	cursor = (unsigned char *)&gcAuthPcactScratch.rtcPlaintext;
	for (index = 0; index != blockBytes; index++)
		block[index] = cursor[index] ^ state.chain[index];
	SceKernelUtilsForDriver_C2A61770(&state.aes, block, cursor);
	blockBytes = state.aes.encryption.blockWords * 4;
	for (index = 0; index != blockBytes; index++)
		state.chain[index] = cursor[index];
	for (index = 0; index != blockBytes; index++)
		block[index] = state.chain[index] ^ cursor[index + 16];
	SceKernelUtilsForDriver_C2A61770(&state.aes, block, cursor + 16);
	blockBytes = state.aes.encryption.blockWords * 4;
	for (index = 0; index != blockBytes; index++)
		state.chain[index] = cursor[index + 16];
	output->reservedPrefix[0] = 0;
	output->reservedPrefix[1] = 0;
	output->reservedPrefix[2] = 0;
	output->format = 1;
	output->version = 1;
	for (index = 0; index != 11; index++)
		output->reserved[index] = 0;
	for (index = 0; index != 32; index++)
		output->ciphertext[index] = cursor[index];
finish:
	cursor = (unsigned char *)&gcAuthPcactScratch;
	for (index = 0; index != 184; index++)
		cursor[index] = 0;
	if (ksceKernelSignalSema(gcAuthPcactSemaphore, 1) != 0)
		result = (int)0x808a0605;
	return result;
}

int GcAuthPcactApplyRtcActivation(const GcAuthPcactRtcActivationRequest *request)
{
	const SceSblGcAuthMgrSclkData2 *activation = request->activation;
	struct GcAuthAesCbcState state;
	MarlinRsaExponentKey key;
	MarlinRsaData signature;
	MarlinRsaData digest;
	SceRtcTick tick;
	SceRtcTick currentTick;
	SceDateTime dateTime;
	unsigned char block[32];
	unsigned char *cursor;
	unsigned char previous;
	unsigned int blockBytes;
	unsigned int index;
	unsigned long long seconds;
	int result;

	if (ksceKernelWaitSema(gcAuthPcactSemaphore, 1, NULL) != 0)
		return (int)0x808a0605;
	if (activation == NULL) {
		result = (int)0x808a0600;
		goto finish;
	}
	result = 0;
	if (activation->challenge.format != 1) {
		result = (int)0x808a0601;
		goto finish;
	}
	for (index = 0; index != 3; index++)
		result |= activation->challenge.reservedPrefix[index];
	result |= activation->challenge.version ^ 1;
	if (result != 0) {
		result = (int)0x808a0601;
		goto finish;
	}
	for (index = 0; index != 11; index++) {
		if (activation->challenge.reserved[index] != 0) {
			result = (int)0x808a0601;
			goto finish;
		}
	}
	key.modulus = gcAuthPcactRtcRsaModulus;
	key.exponent = gcAuthPcactRtcRsaExponent;
	if (SceKernelUtilsForDriver_A773A6A8(activation, 48, &gcAuthPcactScratch) != 0) {
		result = (int)0x808a0604;
		goto finish;
	}
	digest.data = (unsigned char *)&gcAuthPcactScratch;
	digest.size = 32;
	signature.data = (unsigned char *)activation->signature;
	signature.size = 256;
	if (GcAuthRsaVerify2048(&signature, &digest, &key, 11) != 0) {
		result = (int)0x808a0606;
		goto finish;
	}
	cursor = (unsigned char *)&gcAuthPcactScratch.rtcDecoded;
	for (index = 0; index != 32; index++)
		cursor[index] = activation->challenge.ciphertext[index];
	if (SceKernelUtilsForDriver_F12B6451(&state.aes, 128, 256, gcAuthPcactRtcActivationAesKey) >= 0) {
		blockBytes = state.aes.encryption.blockWords * 4;
		for (index = 0; index != blockBytes; index++)
			state.chain[index] = gcAuthPsmAesIv[index];
	}
	SceKernelUtilsForDriver_D8678061(&state.aes, cursor, block);
	blockBytes = state.aes.encryption.blockWords * 4;
	for (index = 0; index != blockBytes; index++) {
		previous = state.chain[index];
		state.chain[index] = cursor[index];
		cursor[index] = previous ^ block[index];
	}
	SceKernelUtilsForDriver_D8678061(&state.aes, cursor + 16, block);
	blockBytes = state.aes.encryption.blockWords * 4;
	for (index = 0; index != blockBytes; index++) {
		unsigned char decrypted = block[index];

		previous = cursor[index + 16];
		cursor[index + 16] = state.chain[index] ^ decrypted;
		state.chain[index] = previous;
	}
	for (index = 0; index != 8; index++)
		gcAuthPcactScratch.rtcDecoded.comparisonKey[index] =
			gcAuthPcactScratch.rtcDecoded.nonceHalves[1][index];
	for (index = 0; index != 8; index++)
		gcAuthPcactScratch.rtcDecoded.comparisonKey[index + 8] =
			gcAuthPcactScratch.rtcDecoded.nonceHalves[0][index];
	for (index = 0; index != 16; index++)
		result |= gcAuthPcactScratch.rtcDecoded.comparisonKey[index] ^
			gcAuthPcactRtcRandom[index];
	if (result != 0) {
		result = (int)0x808a0607;
		goto finish;
	}
	seconds = (unsigned long long)gcAuthPcactScratch.rtcDecoded.seconds[0] |
		((unsigned long long)gcAuthPcactScratch.rtcDecoded.seconds[1] << 8) |
		((unsigned long long)gcAuthPcactScratch.rtcDecoded.seconds[2] << 16) |
		((unsigned long long)gcAuthPcactScratch.rtcDecoded.seconds[3] << 24) |
		((unsigned long long)gcAuthPcactScratch.rtcDecoded.seconds[4] << 32) |
		((unsigned long long)gcAuthPcactScratch.rtcDecoded.seconds[5] << 40) |
		((unsigned long long)gcAuthPcactScratch.rtcDecoded.seconds[6] << 48) |
		((unsigned long long)gcAuthPcactScratch.rtcDecoded.seconds[7] << 56);
	tick.tick = seconds * 1000 + 62135596800000000ULL;
	if (ksceRtcSetCurrentSecureTick(&tick) != 0) {
		result = (int)0x808a0603;
	} else {
		ksceRtcGetCurrentSecureTick(&currentTick);
		ksceRtcConvertTickToDateTime(&dateTime, &currentTick);
	}
finish:
	cursor = (unsigned char *)&gcAuthPcactScratch;
	for (index = 0; index != 184; index++)
		cursor[index] = 0;
	if (ksceKernelSignalSema(gcAuthPcactSemaphore, 1) != 0)
		result = (int)0x808a0605;
	return result;
}

int GcAuthInitializePcact(void)
{
	unsigned char *cursor = (unsigned char *)&gcAuthPcactScratch;
	SceUID semaphore;

	do {
		*cursor++ = 0;
	} while (cursor != (unsigned char *)&gcAuthPcactScratch +
		 sizeof(gcAuthPcactScratch));
	semaphore = ksceKernelCreateSema("ScePcact", 0, 1, 1, NULL);
	gcAuthPcactSemaphore = semaphore;
	return semaphore <= 0;
}
