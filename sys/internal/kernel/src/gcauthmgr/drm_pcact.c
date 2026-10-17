/* Descriptive source name: the original module has no debug records. */
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <marlin_hci/rsa.h>
#include "pcact.h"

SceUID gcAuthPcactSemaphore;
GcAuthPcactScratch gcAuthPcactScratch;
const unsigned char gcAuthPcactActivationMask[16] = {
	0x53, 0xcb, 0xcc, 0xad, 0xb7, 0x6c, 0xba, 0x3a,
	0x60, 0x9c, 0xf2, 0x9f, 0x31, 0x8d, 0x92, 0xe4
};
const unsigned char gcAuthPsmAesIv[16] = { 0 };

extern unsigned int gcAuthPsmRsaModulus[64];
extern unsigned int gcAuthPsmRsaPublicExponent[64];
extern unsigned int gcAuthPsmRsaPrimeP[32];
extern unsigned int gcAuthPsmRsaPrimeQ[32];
extern unsigned int gcAuthPsmRsaExponentP[32];
extern unsigned int gcAuthPsmRsaExponentQ[32];
extern unsigned int gcAuthPsmRsaCoefficient[32];
extern unsigned char gcAuthPsmAesRecordKey[16];
extern unsigned char gcAuthPsmHmacKey[32];

int GcAuthExecuteKirkCommand(void *output, const void *input, int command,
		size_t size, int keyId);
int ksceIdStorageLookup(unsigned int leaf, unsigned int offset, void *output,
		unsigned int size);
int GcAuthGenerateRandom20(unsigned char *data);
int GcAuthAesEncrypt(unsigned char *data, size_t size, const void *key,
		const unsigned char *iv);
int GcAuthFillRandomBytes(unsigned char *data, int size);
int MarlinRsaSetRandomGenerator(int (*generator)(unsigned char *, int));
int MarlinRsaEncrypt2048(MarlinRsaData *output, const MarlinRsaData *input,
		const MarlinRsaExponentKey *key);
unsigned int GcAuthRsaSign2048(MarlinRsaData *output,
		const MarlinRsaData *digest, const MarlinRsaPrivateKey *key,
		int algorithm);
int GcAuthDrmDecrypt(unsigned char *data, size_t size, int keyId);
int ksceSha256Digest(const void *input, unsigned int size, void *digest);
int ksceHmacSha256Digest(const void *key, unsigned int keySize,
		const void *input, unsigned int inputSize, void *digest);
int GcAuthAesDecrypt(unsigned char *data, size_t size, const void *key,
		const unsigned char *iv);

int GcAuthPcactGetAesDecryptKey(const GcAuthPcactKeyRequest *request)
{
	const GcAuthPcactActivationBlock *activation = request->activation;
	const GcAuthPcactRandomRecord *random = request->random;
	unsigned char *outputKey = request->outputKey;
	const unsigned char *input;
	const unsigned char *comparison;
	const unsigned char *end;
	unsigned char *cursor;
	unsigned int index;
	int result;

	if (ksceKernelWaitSema(gcAuthPcactSemaphore, 1, NULL) != 0)
		return (int)0x808a0306;
	if (random == NULL || activation == NULL || outputKey == NULL) {
		result = (int)0x808a0300;
		goto finish;
	}
	if (activation->format != 17 || activation->version != 2 ||
	    activation->mode != random->mode) {
		result = (int)0x808a0301;
		goto finish;
	}
	for (index = 0; index != 13; index++) {
		if (activation->reserved[index] != 0) {
			result = (int)0x808a0301;
			goto finish;
		}
	}
	for (index = 0; index != 16; index++)
		outputKey[index] = 0;
	input = (const unsigned char *)activation;
	cursor = (unsigned char *)&gcAuthPcactScratch.activation;
	for (index = 0; index != sizeof(*activation); index++)
		cursor[index] = input[index];
	result = (int)0x808a0303;
	if (GcAuthExecuteKirkCommand(&gcAuthPcactScratch,
			&gcAuthPcactScratch, 37, 80, 0) != 0)
		goto finish;
	for (index = 0; index != 8; index++)
		gcAuthPcactScratch.decodedKey.comparisonKey[index] =
			gcAuthPcactScratch.decodedKey.nonceHalves[1][index];
	for (index = 0; index != 8; index++)
		gcAuthPcactScratch.decodedKey.comparisonKey[index + 8] =
			gcAuthPcactScratch.decodedKey.nonceHalves[0][index];
	comparison = gcAuthPcactScratch.decodedKey.comparisonKey;
	input = random->derivedKey;
	end = input + 16;
	result = 0;
	do {
		unsigned int actual = *comparison++;
		unsigned int expected = *input++;

		result |= actual ^ expected;
	} while (input != end);
	if (result != 0) {
		result = (int)0x808a0308;
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
		result = (int)0x808a0306;
	return result;
}

int GcAuthPcactReadCertificate(GcAuthPcactCertificate *certificate)
{
	int permission;
	int result;

	permission = ksceKernelSetPermission(128);
	if (permission < 0)
		return permission;
	if (ksceIdStorageLookup(0x46, 64, certificate, 232) < 0 ||
	    ksceIdStorageLookup(0x47, 0, certificate + 1, 0) < 0) {
		ksceKernelSetPermission(permission);
		return (int)0x808a080b;
	}
	result = (int)0x808a080c;
	if (GcAuthExecuteKirkCommand(NULL, certificate, 25, 232, 0) == 0)
		result = 0;
	ksceKernelSetPermission(permission);
	return result;
}

int GcAuthPcactGetCertificate(const GcAuthPcactCertificateRequest *request)
{
	unsigned char *output = request->output;
	unsigned char *cursor;
	unsigned int index;
	int result;

	if (ksceKernelWaitSema(gcAuthPcactSemaphore, 1, NULL) != 0)
		return (int)0x808a0801;
	if (output != NULL &&
	    GcAuthPcactReadCertificate(&gcAuthPcactScratch.certificate) >= 0) {
		for (index = 0; index != 128; index++)
			output[index] = gcAuthPcactScratch.certificate.serialized[index];
		result = 0;
	} else {
		result = (int)0x808a0800;
	}
	cursor = (unsigned char *)&gcAuthPcactScratch;
	do {
		*cursor++ = 0;
	} while (cursor != (unsigned char *)&gcAuthPcactScratch + 184);
	if (ksceKernelSignalSema(gcAuthPcactSemaphore, 1) != 0)
		result = (int)0x808a0801;
	return result;
}

int GcAuthPsmactCreateChallenge(const GcAuthPsmactChallengeRequest *request)
{
	unsigned char mode = request->mode;
	const unsigned char *rtcSeconds = request->rtcSeconds;
	GcAuthPsmactChallenge *output = request->output;
	GcAuthPsmactRandomRecord *random = request->random;
	MarlinRsaExponentKey publicKey;
	MarlinRsaPrivateKey privateKey;
	MarlinRsaData input;
	MarlinRsaData encrypted;
	unsigned char aesKey[16];
	unsigned char *cursor;
	unsigned int index;
	int result;

	if (ksceKernelWaitSema(gcAuthPcactSemaphore, 1, NULL) != 0)
		return (int)0x808a0801;
	if (output == NULL || rtcSeconds == NULL || random == NULL) {
		result = (int)0x808a0800;
		goto finish;
	}
	cursor = (unsigned char *)output;
	for (index = 0; index != sizeof(*output); index++)
		cursor[index] = 0;
	cursor = (unsigned char *)random;
	for (index = 0; index != sizeof(*random); index++)
		cursor[index] = 0;
	if (GcAuthPcactReadCertificate(&output->payload.certificateInput.certificate) < 0) {
		result = (int)0x808a0800;
		goto finish;
	}
	for (index = 0; index != 8; index++)
		output->payload.challenge.rtcSeconds[index] = rtcSeconds[index];
	for (index = 0; index != 8; index++)
		random->rtcSeconds[index] = rtcSeconds[index];
	if (GcAuthGenerateRandom20(gcAuthPcactScratch.random) != 0)
		goto randomFailure;
	for (index = 0; index != 16; index++)
		output->payload.challenge.challenge[index] = gcAuthPcactScratch.random[index];
	for (index = 0; index != 16; index++)
		random->challenge[index] = gcAuthPcactScratch.random[index];
	if (GcAuthGenerateRandom20(gcAuthPcactScratch.random) != 0)
		goto randomFailure;
	for (index = 0; index != 16; index++)
		output->payload.challenge.derivationSeed[index] = gcAuthPcactScratch.random[index];
	for (index = 0; index != 16; index++)
		random->derivationSeed[index] = gcAuthPcactScratch.random[index];
	if (GcAuthGenerateRandom20(gcAuthPcactScratch.random) != 0)
		goto randomFailure;
	for (index = 0; index != 16; index++)
		aesKey[index] = gcAuthPcactScratch.random[index];
	GcAuthAesEncrypt((unsigned char *)&output->payload, 496, aesKey,
			gcAuthPsmAesIv);
	for (index = 0; index != 8; index++)
		gcAuthPcactScratch.keyEnvelope.rtcSeconds[index] = rtcSeconds[index];
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.keyEnvelope.aesKey[index] = aesKey[index];
	publicKey.modulus = gcAuthPsmRsaModulus;
	publicKey.exponent = gcAuthPsmRsaPublicExponent;
	encrypted.data = output->encryptedKey;
	encrypted.size = 0;
	input.data = (unsigned char *)&gcAuthPcactScratch.keyEnvelope;
	input.size = 24;
	MarlinRsaSetRandomGenerator(GcAuthFillRandomBytes);
	if (MarlinRsaEncrypt2048(&encrypted, &input, &publicKey) != 0) {
		result = (int)0x808a0803;
		goto finish;
	}
	output->header.magic[0] = 'P';
	output->header.magic[1] = 'S';
	output->header.magic[2] = 'M';
	output->header.magic[3] = 'C';
	output->header.mode = mode;
	output->header.reserved[0] = 0;
	output->header.reserved[1] = 0;
	output->header.reserved[2] = 0;
	output->header.reserved[3] = 0;
	output->header.reserved[4] = 0;
	output->header.reserved[5] = 0;
	output->header.reserved[6] = 0;
	output->header.packetType = 1;
	output->header.trailerTag = 3;
	output->header.format = 48;
	output->header.packetBlocks = 4;
	if (ksceSha256Digest(output, 768, gcAuthPcactScratch.sha256) != 0) {
		result = (int)0x808a080d;
		goto finish;
	}
	encrypted.data = output->signature;
	encrypted.size = 0;
	input.data = gcAuthPcactScratch.sha256;
	input.size = 32;
	privateKey.primeP = gcAuthPsmRsaPrimeP;
	privateKey.exponentP = gcAuthPsmRsaExponentP;
	privateKey.exponentQ = gcAuthPsmRsaExponentQ;
	privateKey.primeQ = gcAuthPsmRsaPrimeQ;
	privateKey.coefficient = gcAuthPsmRsaCoefficient;
	if (GcAuthRsaSign2048(&encrypted, &input, &privateKey, 11) != 0) {
		result = (int)0x808a0806;
		goto finish;
	}
	result = GcAuthDrmDecrypt(output->signature, 256, 192);
	if (result != 0) {
		result = (int)0x808a0804;
		goto finish;
	}
	random->header.packetType = 4;
	random->header.format = 48;
	random->header.flags = 0;
	random->header.mode = mode;
	random->header.magic[0] = 'P';
	random->header.magic[1] = 'S';
	random->header.magic[2] = 'M';
	random->header.magic[3] = 'C';
	random->header.recordSize = 64;
	for (index = 0; index != 7; index++)
		random->header.reserved[index] = 0;
	GcAuthAesEncrypt(random->rtcSeconds, 48, gcAuthPsmAesRecordKey,
			gcAuthPsmAesIv);
	goto finish;
randomFailure:
	result = (int)0x808a0802;
finish:
	cursor = (unsigned char *)&gcAuthPcactScratch;
	do {
		*cursor++ = 0;
	} while (cursor != (unsigned char *)&gcAuthPcactScratch + 184);
	if (ksceKernelSignalSema(gcAuthPcactSemaphore, 1) != 0)
		result = (int)0x808a0801;
	return result;
}

int GcAuthPsmactVerifyActivation(const GcAuthPsmactVerifyRequest *request)
{
	const GcAuthPsmactActivation *activation = request->activation;
	const GcAuthPsmactRandomRecord *record = request->random;
	GcAuthPsmactDerivedKeys *output = request->output;
	GcAuthPsmactRandomRecord random;
	unsigned char masterKey[32];
	unsigned char authenticationKey[32];
	const unsigned char *input;
	unsigned char *cursor;
	const unsigned char *magic;
	unsigned int index;
	int result;

	if (ksceKernelWaitSema(gcAuthPcactSemaphore, 1, NULL) != 0)
		return (int)0x808a0801;
	if (record == NULL || activation == NULL || output == NULL) {
		result = (int)0x808a0800;
		goto finish;
	}
	input = (const unsigned char *)record;
	cursor = (unsigned char *)&random;
	for (index = 0; index != sizeof(random); index++)
		cursor[index] = input[index];
	GcAuthAesDecrypt(random.rtcSeconds, 48, gcAuthPsmAesRecordKey,
			gcAuthPsmAesIv);
	result = (int)0x808a0807;
	if (activation->header.format != 48 || activation->header.packetType != 2 ||
	    activation->header.mode != random.header.mode)
		goto finish;
	magic = (const unsigned char *)"PSMC";
	result = 0;
	for (index = 0; index != 4; index++)
		result |= activation->header.magic[index] ^ magic[index];
	if (result != 0 || activation->header.flags != 2 ||
	    activation->header.recordSize != 0)
		goto invalidRecord;
	for (index = 0; index != 7; index++) {
		if (activation->header.reserved[index] != 0)
			goto invalidRecord;
	}
	if (random.header.format != 48 || random.header.packetType != 4)
		goto invalidRecord;
	for (index = 0; index != 4; index++)
		result |= random.header.magic[index] ^ magic[index];
	if (result != 0 || random.header.flags != 0 || random.header.recordSize != 64)
		goto invalidRecord;
	for (index = 0; index != 7; index++) {
		if (random.header.reserved[index] != 0)
			goto invalidRecord;
	}
	cursor = (unsigned char *)output;
	for (index = 0; index != sizeof(*output); index++)
		cursor[index] = 0;
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.derivation.recordSeed[index] = random.derivationSeed[index];
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.derivation.activationSeed[index] = activation->derivationSeed[index];
	gcAuthPcactScratch.derivation.stage = 0;
	gcAuthPcactScratch.derivation.label[0] = 'P';
	gcAuthPcactScratch.derivation.label[1] = 'S';
	gcAuthPcactScratch.derivation.label[2] = 'M';
	gcAuthPcactScratch.derivation.label[3] = '0';
	if (ksceHmacSha256Digest(gcAuthPsmHmacKey, 32,
			&gcAuthPcactScratch.derivation, 37, masterKey) != 0)
		goto hmacFailure;
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.derivation.recordSeed[index] = random.derivationSeed[index];
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.derivation.activationSeed[index] = activation->derivationSeed[index];
	gcAuthPcactScratch.derivation.stage = 1;
	gcAuthPcactScratch.derivation.label[0] = 'P';
	gcAuthPcactScratch.derivation.label[1] = 'S';
	gcAuthPcactScratch.derivation.label[2] = 'M';
	gcAuthPcactScratch.derivation.label[3] = '1';
	if (ksceHmacSha256Digest(masterKey, 32, &gcAuthPcactScratch.derivation,
			37, output->stageOneKey) != 0)
		goto hmacFailure;
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.derivation.recordSeed[index] = random.derivationSeed[index];
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.derivation.activationSeed[index] = activation->derivationSeed[index];
	gcAuthPcactScratch.derivation.stage = 2;
	gcAuthPcactScratch.derivation.label[0] = 'P';
	gcAuthPcactScratch.derivation.label[1] = 'S';
	gcAuthPcactScratch.derivation.label[2] = 'M';
	gcAuthPcactScratch.derivation.label[3] = '2';
	if (ksceHmacSha256Digest(masterKey, 32, &gcAuthPcactScratch.derivation,
			37, output->stageTwoKey) != 0)
		goto hmacFailure;
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.derivation.recordSeed[index] = random.derivationSeed[index];
	for (index = 0; index != 16; index++)
		gcAuthPcactScratch.derivation.activationSeed[index] = activation->derivationSeed[index];
	gcAuthPcactScratch.derivation.stage = 3;
	gcAuthPcactScratch.derivation.label[0] = 'P';
	gcAuthPcactScratch.derivation.label[1] = 'S';
	gcAuthPcactScratch.derivation.label[2] = 'M';
	gcAuthPcactScratch.derivation.label[3] = '3';
	if (ksceHmacSha256Digest(masterKey, 32, &gcAuthPcactScratch.derivation,
			37, authenticationKey) != 0 ||
	    ksceHmacSha256Digest(authenticationKey, 32, activation, 480, masterKey) != 0)
		goto hmacFailure;
	for (index = 0; index != 32; index++)
		result |= masterKey[index] ^ activation->mac[index];
	if (result != 0) {
		result = (int)0x808a0809;
		goto finish;
	}
	for (index = 0; index != 16; index++)
		result |= random.challenge[index] ^ activation->challenge[index];
	if (result != 0 || activation->header.mode != random.header.mode)
		result = (int)0x808a080a;
	goto finish;
invalidRecord:
	result = (int)0x808a0807;
	goto finish;
hmacFailure:
	result = (int)0x808a0808;
finish:
	cursor = (unsigned char *)&gcAuthPcactScratch;
	do {
		*cursor++ = 0;
	} while (cursor != (unsigned char *)&gcAuthPcactScratch + 184);
	if (ksceKernelSignalSema(gcAuthPcactSemaphore, 1) != 0)
		result = (int)0x808a0801;
	return result;
}

int GcAuthPcactDecryptChallenge(size_t size, int keyId)
{
	unsigned char *source;
	unsigned char *output;
	unsigned char *end;
	int result;

	result = GcAuthDrmDecrypt(gcAuthPcactScratch.hashInput.serializedChallenge,
			size, keyId);
	if (result == 0 && size != 0) {
		source = gcAuthPcactScratch.hashInput.serializedChallenge;
		output = (unsigned char *)&gcAuthPcactScratch;
		end = source + size;
		do {
			*output++ = *source++;
		} while (source != end);
	}
	return result;
}
