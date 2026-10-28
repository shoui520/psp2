/* Descriptive source name: the original module has no debug records. */
#include <threadmgr/threadmgr.h>
#include <marlin_hci/rsa.h>
#include "pcact.h"
#include <sysmem/utils.h>
#include <idstorage/idstorage.h>
#include "drm_crypto.h"

SceUID gcAuthPcactSemaphore;
GcAuthPcactScratch gcAuthPcactScratch;
const unsigned char gcAuthPcactActivationMask[16] = {
	0x53, 0xcb, 0xcc, 0xad, 0xb7, 0x6c, 0xba, 0x3a,
	0x60, 0x9c, 0xf2, 0x9f, 0x31, 0x8d, 0x92, 0xe4
};
const unsigned char gcAuthPsmAesIv[16] = { 0 };

unsigned char gcAuthPsmHmacKey[32] = {
	0x13, 0x9f, 0x95, 0x55, 0x80, 0x27, 0x5b, 0x85,
	0xab, 0xae, 0x51, 0x17, 0x46, 0x4b, 0xd0, 0xe9,
	0x24, 0x73, 0x12, 0x59, 0xe8, 0x74, 0x70, 0x5a,
	0xcb, 0x92, 0xb4, 0xc7, 0x86, 0x97, 0x8e, 0xbf,
};

unsigned char gcAuthPsmAesRecordKey[32] = {
	0x98, 0x60, 0xde, 0xd8, 0xb9, 0x5e, 0xbd, 0x06,
	0x90, 0x04, 0x64, 0xbb, 0x25, 0xd9, 0x05, 0x09,
	0xd0, 0xf1, 0x49, 0xfb, 0x73, 0xc3, 0x5a, 0x90,
	0x88, 0xd8, 0xc7, 0xd5, 0x95, 0x03, 0x23, 0x27,
};

unsigned int gcAuthPsmRsaModulus[64] = {
	0x460611a7, 0x2de7a844, 0x30fba335, 0xb37fa8e2,
	0x39c05086, 0x7207c120, 0x4dc0faa3, 0x61b1c677,
	0xdb68b9cc, 0x94db09d5, 0x6f92bb2c, 0x73ba8d68,
	0x7451d3ac, 0x077d9554, 0x6f26f7ca, 0x77c60fea,
	0x61c21bee, 0x7ac9061b, 0x3ffcb5ce, 0xf57e294d,
	0x9c354bfa, 0x3ef0377d, 0xac448829, 0xb3d330ef,
	0xddc7fc26, 0x4e2702f1, 0xe451ffad, 0x8fd13c59,
	0x24eaec7c, 0x39a99251, 0xea119aba, 0x494af892,
	0xde8e2bd2, 0x7e2f1369, 0x3b492433, 0x7318e80b,
	0x176f3404, 0x5fddb5ac, 0xb899adf1, 0xeb031990,
	0x028af4d3, 0xc94017e8, 0x869de23a, 0xe6bbf94f,
	0xbe77e2d8, 0x89627f0f, 0xff7a429c, 0xb37d2a04,
	0xe158f8b9, 0x0fe6ab19, 0x5b347f24, 0x350a6c11,
	0x84a58586, 0x8ec1d3be, 0xd76f9390, 0x2b2b5258,
	0xf13394c5, 0x5fb63351, 0xc1bc1615, 0xae3d56fb,
	0x9fe7e3eb, 0x548cabd9, 0x0d951fea, 0xcaad97b2,
};

unsigned int gcAuthPsmRsaPrimeP[32] = {
	0x1585dcab, 0x3b20f15b, 0x6c1d1813, 0x5da34e43,
	0xc8963753, 0xc1ea544b, 0x978a7980, 0xa0302e04,
	0x14c950e2, 0xc67e7e30, 0xe6834d91, 0x376c04c8,
	0xff65ea55, 0x3b95d346, 0x7446e7e7, 0x15009f54,
	0xb4d56152, 0x99106a91, 0xcdc6570c, 0x0ccc1130,
	0x9d530720, 0x9164d35b, 0xe01deb3f, 0x9aa6acfb,
	0x993ff763, 0xcf1b1f62, 0x74c69310, 0x716e60be,
	0xb942bf62, 0xa461cb9f, 0x681b4d37, 0xe5be7162,
};

unsigned int gcAuthPsmRsaExponentQ[32] = {
	0x9d1e2ddb, 0xdfa7d964, 0x014c8c26, 0xb7ce63b1,
	0x8580c0ac, 0xc31e3af9, 0xc33ae898, 0xd2b06255,
	0x4f0f0b6b, 0x88dc35c5, 0x6fb425f3, 0x5d4f56c7,
	0xafb7ec6e, 0xe468e568, 0x1539f9d3, 0xb62e0fa6,
	0xf8ec1ff9, 0x3bdfddcb, 0xbffd39e1, 0x64ec4dec,
	0xb4827c45, 0xcd9fee2c, 0x5a5b0772, 0x968b1eec,
	0x7299d5b6, 0xf8772f46, 0x1904651d, 0x98022174,
	0xe342af40, 0xe28b7ccf, 0x518ff2da, 0x1d03ad06,
};

unsigned int gcAuthPsmRsaPublicExponent[64] = {
	0x00010001, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

unsigned int gcAuthPsmRsaExponentP[32] = {
	0xdd4d5ee5, 0x8bcad37a, 0xc7b72c87, 0x960ac8a9,
	0x8001ab5c, 0x4fac20ae, 0xa64e0f8d, 0x31480a09,
	0x0df54aa2, 0x5df79316, 0x5e9904af, 0x235e3f8a,
	0x3c7c71dc, 0xe6f98983, 0xa9ea003d, 0x2cd6ecc8,
	0xf1cfa6d6, 0x00adae72, 0x8dc6e75b, 0xd878df45,
	0x21edc5f5, 0x9857c4a0, 0xec1c44a6, 0xc7f2ef4d,
	0x7f1b0340, 0x98a69c15, 0x3694337a, 0x9678e6a2,
	0xb89bbad0, 0x6c00ce0c, 0x4dbb8c66, 0x3922353c,
};

unsigned int gcAuthPsmRsaPrimeQ[32] = {
	0x83d5302b, 0x1c2253f6, 0x95a939e9, 0xe05d8dcc,
	0x9706e80d, 0x133d46d3, 0x67c6c10b, 0xc36b5f37,
	0x413a5c47, 0x9747b075, 0xad2c7948, 0xc617d7cf,
	0xbe4357cf, 0x1af88e40, 0xc80408e6, 0xf4f82341,
	0x0b5aff20, 0xf5aa8c1c, 0x6e0f2c32, 0xff557036,
	0xefb1a187, 0xb61c6018, 0xe058019b, 0xa33aa102,
	0x0753edba, 0x82708083, 0xa2985dff, 0xa7528766,
	0x1341b5f2, 0xc0ad047e, 0x91879d9d, 0xdde7fb47,
};

unsigned int gcAuthPsmRsaCoefficient[32] = {
	0x57f953a8, 0xad0c201a, 0x4c87b061, 0xd622483f,
	0x6c996858, 0x1e061aae, 0xfc5f5ee0, 0x7330d683,
	0xee1c6cc6, 0x0e7d9350, 0x9415ab00, 0x6fa6b958,
	0xa8a5289e, 0x5fa0ab10, 0x16ecf587, 0x5fa0b9fa,
	0x4689b555, 0x3a05f7ba, 0x4a095163, 0x0e6d63da,
	0x3111c617, 0x7dc6701d, 0x4e6779c0, 0xe73f992f,
	0x04a5ada9, 0x7ede8f7b, 0x21867dea, 0xfa50a0c9,
	0x3ccb1958, 0xb02b0cea, 0x109e4c45, 0x22d06ad8,
};

int GcAuthExecuteKirkCommand(void *output, const void *input, int command,
		size_t size, int keyId);

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

	if (sceKernelWaitSema(gcAuthPcactSemaphore, 1, NULL) != 0)
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
	if (sceKernelSignalSema(gcAuthPcactSemaphore, 1) != 0)
		result = (int)0x808a0306;
	return result;
}

int GcAuthPcactReadCertificate(GcAuthPcactCertificate *certificate)
{
	int permission;
	int result;

	permission = sceKernelSetThreadAccessLevel(128);
	if (permission < 0)
		return permission;
	if (sceIdStorageLookup(0x46, 64, certificate, 232) < 0 ||
	    sceIdStorageLookup(0x47, 0, certificate + 1, 0) < 0) {
		sceKernelSetThreadAccessLevel(permission);
		return (int)0x808a080b;
	}
	result = (int)0x808a080c;
	if (GcAuthExecuteKirkCommand(NULL, certificate, 25, 232, 0) == 0)
		result = 0;
	sceKernelSetThreadAccessLevel(permission);
	return result;
}

int GcAuthPcactGetCertificate(const GcAuthPcactCertificateRequest *request)
{
	unsigned char *output = request->output;
	unsigned char *cursor;
	unsigned int index;
	int result;

	if (sceKernelWaitSema(gcAuthPcactSemaphore, 1, NULL) != 0)
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
	if (sceKernelSignalSema(gcAuthPcactSemaphore, 1) != 0)
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

	if (sceKernelWaitSema(gcAuthPcactSemaphore, 1, NULL) != 0)
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
	if (sceSha256Digest(output, 768, gcAuthPcactScratch.sha256) != 0) {
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
	if (sceKernelSignalSema(gcAuthPcactSemaphore, 1) != 0)
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

	if (sceKernelWaitSema(gcAuthPcactSemaphore, 1, NULL) != 0)
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
	if (sceHmacSha256Digest(gcAuthPsmHmacKey, 32,
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
	if (sceHmacSha256Digest(masterKey, 32, &gcAuthPcactScratch.derivation,
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
	if (sceHmacSha256Digest(masterKey, 32, &gcAuthPcactScratch.derivation,
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
	if (sceHmacSha256Digest(masterKey, 32, &gcAuthPcactScratch.derivation,
			37, authenticationKey) != 0 ||
	    sceHmacSha256Digest(authenticationKey, 32, activation, 480, masterKey) != 0)
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
	if (sceKernelSignalSema(gcAuthPcactSemaphore, 1) != 0)
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
