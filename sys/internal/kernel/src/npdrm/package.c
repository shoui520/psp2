/* Package implementation; the original source filename is unavailable. */
#include "package_internal.h"
#include <string.h>
#include <sysmem/utils.h>
#include <psp2kern/npdrm.h>
#include <sysmem/sblaimgr.h>
#include <acmgr/acmgr.h>
#include <sysmem/sysmem.h>
#include <intrmgr/syscall.h>
#include <intrmgr/syscall_copy.h>

#define NPDRM_BE32(value) \
	((((value) & 0xff000000U) >> 24) | (((value) & 0x00ff0000U) >> 8) | \
	 (((value) & 0x0000ff00U) << 8) | (((value) & 0x000000ffU) << 24))
#define NPDRM_BE64(value) \
	(((uint64_t)NPDRM_BE32((uint32_t)(value)) << 32) | \
	 NPDRM_BE32((uint32_t)((value) >> 32)))

enum NpDrmPackageRsaKey {
	NPDRM_RSA_PSP,
	NPDRM_RSA_VITA,
	NPDRM_RSA_LIVE_AREA,
	NPDRM_RSA_ALTERNATE,
};

static const unsigned int gNpDrmPackageRsaModuli[4][64] = {
	{ /* PSP */
		0xb8519dd5, 0x43e51bbf, 0xde1c1acc, 0x2567107c,
		0x45c7c4d0, 0x79f6b9d5, 0xaba75e88, 0xe16aade1,
		0xf1886c43, 0x4018b2cd, 0xaad4e6fb, 0xa659a662,
		0x96565457, 0x7bd3c888, 0xa09dc290, 0x5a7194ae,
		0x8d379ad6, 0x63faa3ad, 0x85dc8913, 0xad0f0d09,
		0x1137b3b2, 0x7fc7d832, 0xacbc234e, 0x8b909202,
		0xace8a719, 0x3d802bc5, 0x2b30a2e5, 0x0ebc22f4,
		0xc1612d1a, 0x31c8e468, 0xa9da2e70, 0xe4afccb9,
		0x99c31ff7, 0xea1a0514, 0x0699257c, 0x625f5336,
		0x4e5d43a1, 0x11718d4a, 0x72f9dd1a, 0xeffe5316,
		0xadcc7026, 0x75c060ad, 0xad97698e, 0x3f169b7e,
		0x01392905, 0xaedd64b4, 0x8bbea8f6, 0x8ed6bf17,
		0xca51d0fb, 0x635ebffb, 0x39963987, 0xff06f69f,
		0xa1f227b8, 0xc3720826, 0x5bfa8ef9, 0x476e7353,
		0xa5d6906f, 0x64c3ba43, 0x83e98f48, 0x395a2aad,
		0xc9899919, 0xd4708d5f, 0x2e3b51a6, 0xbbdb6aa3,
	},
	{ /* Vita */
		0x2813a6c3, 0xdecdd689, 0x6b88b869, 0x7c7eaeae,
		0xdba9a0b3, 0x3f882acf, 0x3a168aa2, 0xf2f557a7,
		0x79d7b4d6, 0x683ea1d0, 0xbbf3a023, 0x70d820c8,
		0x91eee462, 0xe1b13f83, 0x2bc73e2b, 0x18e62b16,
		0x2f63ddd1, 0x22d28d3c, 0x36da79f8, 0xc556f16a,
		0xf522afc9, 0x6664f6c4, 0xa88cf637, 0x49cb0065,
		0x1cd55e04, 0x7a5d4f9c, 0x350cdd8c, 0x139ee661,
		0x1c6b8ae1, 0xf3f51096, 0x0fa2882d, 0x4acef46c,
		0xe40b5f9e, 0xb44247d5, 0x00ae13a0, 0x012807c5,
		0xe657a795, 0x0dc0e4d3, 0xb88d41ee, 0xca4a88bc,
		0x29843914, 0xed9f1027, 0xac6cdcda, 0x890b953a,
		0xa4609ccf, 0x2ab1e680, 0x4550d6f7, 0xb4c77d20,
		0x10fa1ca1, 0xad3bcd46, 0x33eb12c7, 0x61d7571b,
		0x854e38ea, 0x19911288, 0xd5cc4311, 0xc4bbaf63,
		0xefb50784, 0x8b3bad9d, 0xe3bc68fe, 0x731cf624,
		0x2d30274a, 0xd2691b1d, 0xa67c9a04, 0x85d72f79,
	},
	{ /* LiveArea */
		0xe9fdc62d, 0x73895376, 0x8c30a334, 0x84eead72,
		0x7abe861e, 0x4eebeb91, 0x43d69215, 0xa964f3a3,
		0x20af13fb, 0xce387f38, 0x81dc9227, 0xce28806b,
		0xa684814b, 0x4b26af23, 0x26038ef4, 0xb8967c3f,
		0xfca24bd9, 0x1ea2856e, 0xae322e3a, 0x8c440c12,
		0x926b7cff, 0x2ff0d1cc, 0x6671b7cb, 0x0ef24487,
		0x46a70feb, 0xf76f68b6, 0x3a74612a, 0x8a85884d,
		0x15403780, 0x59b3a101, 0xc84ba685, 0x41d950b2,
		0x2106d6b5, 0x80f285ad, 0xb01a56e3, 0xbe152285,
		0xae364816, 0xa5265f93, 0x1756b94d, 0x23eba915,
		0xed1c14d7, 0xa0449ef6, 0xae25d06b, 0x3b840669,
		0x014ff15c, 0xd01a9a5e, 0x9c00c846, 0x944f5abc,
		0x817919b1, 0xa2de3e1f, 0xcdf37ae7, 0xc83517cc,
		0xc93cb2a1, 0x3e30ac7c, 0x7f6c0fe6, 0xb09990b6,
		0x67b37d05, 0xd015bb3e, 0x678ac454, 0x7376208f,
		0x09d740b2, 0x1328c250, 0xcecfc0ca, 0x8e5c3b05,
	},
	{ /* Alternate package signature */
		0x126a85eb, 0x14e4708d, 0x33d327c7, 0xfd8a1f5b,
		0xbe6e05c2, 0xa9193231, 0xd1bc6a9d, 0x4144b3df,
		0x720502b0, 0x581dbac1, 0xbfad7147, 0xbb981b06,
		0x78ce9662, 0x4548444a, 0xc9d8b043, 0x25970052,
		0xc8dd6ba2, 0xf55489af, 0xc2cfc428, 0x1dbdf335,
		0xb84c18ae, 0xe2def46f, 0x195eebdc, 0xaec5b194,
		0xeec3ac5d, 0x2f53671e, 0xcc88ef88, 0x72558134,
		0x57b92bd3, 0x5dcd1e19, 0xeafb0f8e, 0xd61c738f,
		0x163f86a5, 0x46b1ce03, 0xa7e35cd4, 0xd7652eae,
		0x563cc1c8, 0xa3db0087, 0xc3849277, 0xd778a626,
		0xc7ef4812, 0xbb05328e, 0x3692fd5f, 0x6015a806,
		0xe381391e, 0x863526f3, 0x9ab95bbc, 0xad1d6612,
		0xe5cf8ad1, 0x49820839, 0x4d647831, 0xc1a7211d,
		0xc6ac83dd, 0x1ad15db6, 0xb8433049, 0x537764a1,
		0x3653cc2e, 0x28f0ab60, 0x5d6f9e6e, 0x767d3adc,
		0x693d9770, 0xede7d051, 0x294cf882, 0x890ca1f1,
	},
};

char gNpDrmPackageContentId[48];
struct NpDrmPackageContext gNpDrmPackageContexts[6];
const struct NpDrmPackageRsaCrtMaterial *gNpDrmPackageUnwrapKey;
const struct NpDrmPackageRsaCrtMaterial *gNpDrmPackageSignKey;
SceAesCompactContext gNpDrmPackageAesWorkspaces[6];
int gNpDrmPackageGameExist;

int NpDrmPackageCheckDebugSignature(const unsigned char *signature,
	unsigned int size, const unsigned char *digest)
{
	const unsigned char *end;
	volatile unsigned int difference;

	if (size != 20)
		return 0x80871004;
	end = digest + 20;
	difference = 0;
	do {
		difference += (unsigned char)(signature[0] - digest[0]);
		difference += (unsigned char)(signature[1] - digest[1]);
		difference += (unsigned char)(signature[2] - digest[2]);
		difference += (unsigned char)(signature[3] - digest[3]);
		signature += 4;
		digest += 4;
	} while (digest != end);
	return difference ? 0x80871004 : 0;
}

int NpDrmPackageInitializeContext(struct NpDrmPackageContext *context,
	const struct NpDrmPackageHeader *header, unsigned int size)
{
	struct {
		unsigned char bytes[16];
	} key;
	unsigned char *destination;
	volatile unsigned char *wipe;
	const unsigned char *source;
	unsigned int keyId = (header->extended.keyId >> 24) & 7;

	switch (keyId) {
	case 1:
		key = (typeof(key)) { {
			0x07, 0xf2, 0xc6, 0x82, 0x90, 0xb5, 0x0d, 0x2c,
			0x33, 0x81, 0x8d, 0x70, 0x9b, 0x60, 0xe6, 0x2b,
		} };
		break;
	case 2:
		key = (typeof(key)) { {
			0xe3, 0x1a, 0x70, 0xc9, 0xce, 0x1d, 0xd7, 0x2b,
			0xf3, 0xc0, 0x62, 0x29, 0x63, 0xf2, 0xec, 0xcb,
		} };
		break;
	case 3:
		key = (typeof(key)) { {
			0x42, 0x3a, 0xca, 0x3a, 0x2b, 0xd5, 0x64, 0x9f,
			0x96, 0x86, 0xab, 0xad, 0x6f, 0xd8, 0x80, 0x1f,
		} };
		break;
	case 4:
		key = (typeof(key)) { {
			0xaf, 0x07, 0xfd, 0x59, 0x65, 0x25, 0x27, 0xba,
			0xf1, 0x33, 0x89, 0x66, 0x8b, 0x17, 0xd9, 0xea,
		} };
		break;
	}
	if (keyId == 2 || keyId == 3 || keyId == 4) {
		NpDrmPackageAesInitialize(context->aesContext, key.bytes, sizeof(key));
		NpDrmPackageAesEncrypt(context->aesContext, key.bytes,
			header->main.initialVector);
	}
	NpDrmPackageAesInitialize(context->aesContext, key.bytes, sizeof(key));
	for (destination = context->key, source = key.bytes;
	     destination < context->key + sizeof(context->key);)
		*destination++ = *source++;
	for (destination = (unsigned char *)context->counter,
	     source = header->main.initialVector;
	     destination < (unsigned char *)(context->counter + 2);)
		*destination++ = *source++;
	for (wipe = key.bytes; wipe < key.bytes + sizeof(key);)
		*wipe++ = 0xff;
	for (wipe = key.bytes; wipe < key.bytes + sizeof(key);)
		*wipe++ = 0;
	return 0;
}

int NpDrmPackageInitializeCounter(struct NpDrmPackageContext *context,
	const struct NpDrmPackageHeader *header, unsigned int size)
{
	unsigned char counter[16] = { 0 };
	unsigned char *destination;
	const unsigned char *source;
	unsigned int keyId;

	for (destination = counter, source = header->main.digest;
	     destination < counter + sizeof(counter);)
		*destination++ = *source++;
	keyId = (header->extended.keyId >> 24) & 7;
	if (keyId == 2 || keyId == 3) {
		NpDrmPackageAesInitialize(context->aesContext,
			header->main.digest, sizeof(counter));
		NpDrmPackageAesEncrypt(context->aesContext, counter, counter);
	}
	for (destination = (unsigned char *)context->counter, source = counter;
	     destination < (unsigned char *)(context->counter + 2);)
		*destination++ = *source++;
	return 0;
}

int NpDrmPackageCryptData(struct NpDrmPackageContext *context,
	void *buffer, unsigned int size, uint64_t offset, unsigned int flags)
{
	unsigned char *data = buffer;
	uint64_t counter[2];
	unsigned char stream[16];
	uint64_t base;
	unsigned int count;
	unsigned int index;
	int result;

	if (flags & 0x8000) {
		uint64_t end;

		counter[0] = __builtin_bswap64(context->counter[1]) + offset;
		counter[1] = __builtin_bswap64(context->counter[0]);
		end = counter[0] + size;
		if (end != 0 && end < counter[0]) {
			count = size - (unsigned int)end;
			result = NpDrmPackageAesCtr(data, data, count,
				context->key, 128, counter);
			if (result < 0)
				return result;
			data += count;
			size = end;
			counter[0] = 0;
			counter[1] = __builtin_bswap64(context->counter[0]);
		}
		return NpDrmPackageAesCtr(data, data, size,
			context->key, 128, counter);
	}
	for (index = 0; index < sizeof(counter); index++)
		((unsigned char *)counter)[index] =
			((const unsigned char *)context->counter)[index];
	base = __builtin_bswap64(counter[1]);
	while (size != 0) {
		counter[1] = __builtin_bswap64(base + offset);
		NpDrmPackageAesEncrypt(context->aesContext, stream, counter);
		count = size;
		if (count >= sizeof(stream))
			count = sizeof(stream);
		offset++;
		for (index = 0; index < count; index++)
			data[index] ^= stream[index];
		size -= count;
		data += count;
	}
	return 0;
}

int NpDrmPackageInitializePrivateContext(struct NpDrmPackageContext *context,
	const struct NpDrmPackageHeader *header, unsigned int size)
{
	const unsigned char *data = (const void *)header;
	const struct NpDrmPackageMetadataRecord *record;
	const struct NpDrmPackageEncryptedKeyMetadata *encryptedKey = NULL;
	unsigned char counter[16] = { 0 };
	unsigned char key[32] = { 0 };
	unsigned char decrypted[256];
	MarlinRsaPrivateKey privateKey;
	MarlinRsaData output, input;
	unsigned int count, metadataOffset, metadataEnd, recordEnd;
	unsigned int offset, extent, fullHeaderHmacOffset, i;

	for (i = 0; i < sizeof(counter); i++)
		counter[i] = header->main.digest[i];
	if (!(header->extended.keyId & 0x40))
		return 0x80871020;
	fullHeaderHmacOffset = NPDRM_BE32(header->extended.fullHeaderHmacOffset);
	if (!fullHeaderHmacOffset)
		return 0x80871020;
	count = NPDRM_BE32(header->main.metadataCount);
	if (!count)
		return 0x80871020;
	metadataOffset = NPDRM_BE32(header->main.metadataOffset);
	metadataEnd = metadataOffset + NPDRM_BE32(header->main.metadataSize) - 64;
	record = (const void *)(data + metadataOffset);
	for (;;) {
		recordEnd = (unsigned int)((const unsigned char *)record - data) +
			sizeof(*record) + NPDRM_BE32(record->size);
		if (metadataEnd < recordEnd)
			return 0x80871003;
		if (NPDRM_BE32(record->type) == 15)
			encryptedKey = (const void *)record->payload;
		if (--count == 0)
			break;
		record = (const void *)(data + recordEnd);
	}
	if (!encryptedKey)
		return 0x80871020;
	offset = NPDRM_BE32(encryptedKey->offset);
	extent = offset + NPDRM_BE32(encryptedKey->size);
	if (size < extent || extent > fullHeaderHmacOffset)
		return 0x80871020;
	privateKey.primeP = (unsigned int *)gNpDrmPackageEncryptedKeyRsaKey.primeP;
	privateKey.primeQ = (unsigned int *)gNpDrmPackageEncryptedKeyRsaKey.primeQ;
	privateKey.exponentP = (unsigned int *)gNpDrmPackageEncryptedKeyRsaKey.exponentP;
	privateKey.exponentQ = (unsigned int *)gNpDrmPackageEncryptedKeyRsaKey.exponentQ;
	privateKey.coefficient = (unsigned int *)gNpDrmPackageEncryptedKeyRsaKey.coefficient;
	output = (MarlinRsaData) { decrypted, 256 };
	input = (MarlinRsaData) { (unsigned char *)data + offset + 256, 256 };
	if (NpDrmRsaDecrypt2048(&output, &input, &privateKey) < 0 || output.size != 32)
		return 0x8087101f;
	for (i = 0; i < sizeof(key); i++)
		key[i] = output.data[i];
	NpDrmPackageAesInitialize(context->aesContext, key, 16);
	NpDrmPackageAesEncrypt(context->aesContext, counter, header->main.digest);
	NpDrmPackageAesInitialize(context->aesContext, counter, 16);
	for (i = 0; i < sizeof(context->key); i++)
		context->key[i] = counter[i];
	for (i = 0; i < sizeof(context->counter); i++)
		((unsigned char *)context->counter)[i] = counter[i];
	return 0;
}

int NpDrmPackageVerifyMainHeader(unsigned char *data, unsigned int size,
	const struct NpDrmPackageExtendedHeader *header)
{
	MarlinRsaExponentKey key;
	MarlinRsaData signatureData;
	MarlinRsaData digestData;
	unsigned int exponent[64];
	unsigned int digest[8];
	unsigned int offset = header->fullHeaderHmacOffset;
	int result = 1;

	offset = ((offset & 0x00ff00ff) << 8) |
		((offset & 0xff00ff00) >> 8);
	offset = (offset >> 16) | (offset << 16);
	if (offset + 384 <= size) {
		unsigned char *signature = data + offset;

		SceKernelUtilsForDriver_A773A6A8(data, offset, digest);
		memset(exponent, 0, sizeof(exponent));
		key.modulus = (unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_VITA];
		key.exponent = exponent;
		exponent[0] = 65537;
		signatureData = (MarlinRsaData) { signature, 256 };
		digestData = (MarlinRsaData) { (unsigned char *)digest, 20 };
		result = NpDrmRsaVerify2048(&signatureData, &digestData,
			&key, 5) >= 0;
	}
	return result;
}


int NpDrmPackageVerifyRsaSignature(const unsigned char *signature,
	const unsigned char *digest, unsigned int *modulus)
{
	MarlinRsaExponentKey key;
	MarlinRsaData signatureData, digestData;
	unsigned int exponent[64];
	int signatureSize = sizeof(exponent);

	memset(exponent, 0, signatureSize);
	key = (MarlinRsaExponentKey) { modulus, exponent };
	exponent[0] = 65537;
	signatureData = (MarlinRsaData) { (unsigned char *)signature, signatureSize };
	digestData = (MarlinRsaData) { (unsigned char *)digest, 20 };
	return NpDrmRsaVerify2048(&signatureData, &digestData, &key, 5) >= 0;
}

int NpDrmPackageCheckRsaSignature(const unsigned char *signature,
	unsigned int size, const unsigned char *digest)
{
	if (size != 384)
		return 0x80871004;
	return NpDrmPackageVerifyRsaSignature(signature, digest,
		(unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_ALTERNATE]) ?
		0 : 0x80871004;
}

int NpDrmPackageVerifyLiveAreaHeader(unsigned char *data,
	unsigned int size, const struct NpDrmPackageExtendedHeader *header)
{
	unsigned int offset = header->fullHeaderHmacOffset;
	unsigned char digest[32];
	int result = 1;

	offset = ((offset & 0x00ff00ff) << 8) |
		((offset & 0xff00ff00) >> 8);
	offset = (offset >> 16) | (offset << 16);
	if (size >= offset + 384) {
		unsigned char *signature = data + offset;

		SceKernelUtilsForDriver_A773A6A8(data, offset, digest);
		result = NpDrmPackageVerifyRsaSignature(signature, digest,
			(unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_LIVE_AREA]);
	}
	return result;
}

int NpDrmPackageVerifyAlternateSignature(const unsigned char *signature,
	const unsigned char *digest)
{
	return NpDrmPackageVerifyRsaSignature(signature, digest,
		(unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_ALTERNATE]);
}

int NpDrmPackageCheckLiveAreaSignature(const unsigned char *signature,
	unsigned int size, const unsigned char *digest)
{
	if (size != 384)
		return 0x80871004;
	return NpDrmPackageVerifyRsaSignature(signature, digest,
		(unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_LIVE_AREA]) ?
		0 : 0x80871004;
}

int NpDrmPackageVerifyLiveAreaSignature(const unsigned char *signature,
	const unsigned char *digest)
{
	return NpDrmPackageVerifyRsaSignature(signature, digest,
		(unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_LIVE_AREA]);
}

int NpDrmPackageVerifyPspSignature(const unsigned char *signature,
	const unsigned char *digest)
{
	MarlinRsaExponentKey key;
	MarlinRsaData signatureData = { (unsigned char *)signature, 256 };
	MarlinRsaData digestData = { (unsigned char *)digest, 20 };
	unsigned int exponent[64];

	memset(exponent, 0, sizeof(exponent));
	key.modulus = (unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_PSP];
	key.exponent = exponent;
	exponent[0] = 65537;
	return NpDrmRsaVerify2048(&signatureData, &digestData, &key, 5) >= 0;
}

int NpDrmPackageVerifyPspSignatures(const unsigned char *signature,
	const unsigned char *digest)
{
	if (SceSblGcAuthMgrPkgForDriver_E459A9A8((const char *)digest,
		(const char *)(signature + 256)) != 0)
		return 0;
	return NpDrmPackageVerifyPspSignature(signature, digest);
}

int NpDrmPackageCheckPspSignatures(const unsigned char *signature,
	unsigned int size, const unsigned char *digest)
{
	if (size != 384)
		return 0x80871004;
	if (SceSblGcAuthMgrPkgForDriver_E459A9A8((const char *)digest,
		(const char *)(signature + 256)) != 0)
		return 0x80871004;
	return NpDrmPackageVerifyPspSignature(signature, digest) ?
		0 : 0x80871004;
}

int NpDrmPackageVerifyAlternateHeader(unsigned char *data,
	unsigned int size, const struct NpDrmPackageExtendedHeader *header)
{
	unsigned int offset = header->fullHeaderHmacOffset;
	unsigned char digest[32];
	int result = 1;

	offset = ((offset & 0x00ff00ff) << 8) |
		((offset & 0xff00ff00) >> 8);
	offset = (offset >> 16) | (offset << 16);
	if (size >= offset + 384) {
		unsigned char *signature = data + offset;

		SceKernelUtilsForDriver_A773A6A8(data, offset, digest);
		result = NpDrmPackageVerifyRsaSignature(signature, digest,
			(unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_ALTERNATE]);
	}
	return result;
}

int NpDrmPackageCryptDebugData(struct NpDrmPackageContext *context,
	void *buffer, unsigned int size, uint64_t offset, unsigned int flags)
{
	struct NpDrmPackageDebugSeed seed;
	unsigned char digest[20];
	unsigned char *data = buffer;
	unsigned char *cursor = (unsigned char *)&seed;
	unsigned int skip;

	(void)flags;
	while (cursor < (unsigned char *)(&seed + 1))
		*cursor++ = 0;
	seed.firstDigest[0] = context->counter[0];
	seed.firstDigest[1] = context->counter[0];
	seed.secondDigest[0] = context->counter[1];
	seed.secondDigest[1] = context->counter[1];
	skip = seed.hashSuffix[23] & 0x10;
	seed.blockCounter = __builtin_bswap64(offset);
	while (size) {
		unsigned int length = size < 16 ? size : 16;
		unsigned int i;

		SceKernelUtilsForDriver_87DC7F2F((unsigned char *)&seed + skip,
			sizeof(seed) - skip, digest);
		seed.blockCounter = __builtin_bswap64(
			__builtin_bswap64(seed.blockCounter) + 1);
		for (i = 0; i < length; i++)
			data[i] ^= digest[i];
		data += length;
		size -= length;
	}
	return 0;
}

int NpDrmPackageVerifyDebugSignature(const unsigned char *signature,
	const unsigned char *digest)
{
	uint64_t rotatedDigest[2];
	struct NpDrmPackageDebugSeed seed;
	unsigned char output[16];
	unsigned char hash[20];
	unsigned char *rotated = (unsigned char *)rotatedDigest;
	unsigned char *cursor = (unsigned char *)&seed;
	volatile unsigned int difference = 0;
	unsigned int skip;
	unsigned int i;

	for (i = 0; i < 13; i++)
		rotated[i] = digest[i + 4];
	for (i = 0; i < 3; i++)
		rotated[i + 13] = digest[i + 1];
	while (cursor < (unsigned char *)(&seed + 1))
		*cursor++ = 0;
	seed.firstDigest[0] = rotatedDigest[0];
	seed.firstDigest[1] = rotatedDigest[0];
	seed.secondDigest[0] = rotatedDigest[1];
	seed.secondDigest[1] = rotatedDigest[1];
	seed.blockCounter = 0;
	for (i = 0; i < sizeof(output); i++)
		output[i] = 0;
	skip = seed.hashSuffix[23] & 0x10;
	SceKernelUtilsForDriver_87DC7F2F((unsigned char *)&seed + skip,
		sizeof(seed) - skip, hash);
	seed.blockCounter = __builtin_bswap64(
		__builtin_bswap64(seed.blockCounter) + 1);
	for (i = 0; i < sizeof(output); i++)
		output[i] ^= hash[i];
	for (i = 0; i < sizeof(output); i++)
		difference += (unsigned char)(signature[i] - output[i]);
	return difference == 0;
}

int NpDrmPackageVerifyDebugHeader(unsigned char *data,
	unsigned int size, const struct NpDrmPackageExtendedHeader *header)
{
	unsigned int offset = header->fullHeaderHmacOffset;
	unsigned char digest[20];
	int result = 1;

	offset = ((offset & 0x00ff00ff) << 8) |
		((offset & 0xff00ff00) >> 8);
	offset = (offset >> 16) | (offset << 16);
	if (size >= offset + 384) {
		SceKernelUtilsForDriver_87DC7F2F(data, offset, digest);
		result = NpDrmPackageVerifyDebugSignature(data + offset, digest);
	}
	return result;
}

static const struct NpDrmPackageOperations gNpDrmPackageEncryptedDebugOperations = {
	NpDrmPackageVerifyDebugSignature, NpDrmPackageVerifyDebugHeader,
	NpDrmPackageCheckDebugSignature, NpDrmPackageInitializePrivateContext,
	NpDrmPackageCryptData
};
static const struct NpDrmPackageOperations gNpDrmPackageDebugOperations = {
	NpDrmPackageVerifyDebugSignature, NpDrmPackageVerifyDebugHeader,
	NpDrmPackageCheckDebugSignature, NpDrmPackageInitializeCounter,
	NpDrmPackageCryptDebugData
};
static const struct NpDrmPackageOperations gNpDrmPackagePspOperations = {
	NpDrmPackageVerifyPspSignatures, NpDrmPackageVerifyMainHeader,
	NpDrmPackageCheckPspSignatures, NpDrmPackageInitializeContext,
	NpDrmPackageCryptData
};
static const struct NpDrmPackageOperations gNpDrmPackageLiveAreaOperations = {
	NpDrmPackageVerifyLiveAreaSignature, NpDrmPackageVerifyLiveAreaHeader,
	NpDrmPackageCheckLiveAreaSignature, NpDrmPackageInitializeContext,
	NpDrmPackageCryptData
};
static const struct NpDrmPackageRsaCrtMaterial *const gNpDrmPackageRetailRsaKeys[] = {
	&gNpDrmPackageRetailRsaKey, &gNpDrmPackageSigningRsaKey
};
static const struct NpDrmPackageRsaCrtMaterial *const gNpDrmPackageDebugRsaKeys[] = {
	&gNpDrmPackageDebugRsaKey, &gNpDrmPackageSigningRsaKey
};
static const struct NpDrmPackageOperations gNpDrmPackageAlternateOperations = {
	NpDrmPackageVerifyAlternateSignature, NpDrmPackageVerifyAlternateHeader,
	NpDrmPackageCheckRsaSignature, NpDrmPackageInitializeContext,
	NpDrmPackageCryptData
};

int NpDrmPackageProcessHeader(unsigned char *data, unsigned int size,
	const unsigned char *digest, unsigned int flags)
{
	const struct NpDrmPackageHeader *header = (const void *)data;
	const struct NpDrmPackageMainHeader *main;
	const struct NpDrmPackageExtendedHeader *extended;
	struct NpDrmPackageContext *context = NULL;
	const struct NpDrmPackageOperations *operations;
	unsigned char hash[20];
	unsigned int version, revision, metadataOffset, metadataSize;
	unsigned int headerSize, dataSize, hmacOffset, keyId, offset, index;
	int finalize = (flags & 0x200) != 0 || digest != NULL;

	if (!data)
		return 0x80871800;
	main = &header->main;
	extended = &header->extended;
	if ((flags & 0x100) || finalize) {
		index = (unsigned char)flags;
		if (index > 5)
			return 0x8087101c;
		context = &gNpDrmPackageContexts[index];
		if (finalize) {
			int result;

			if (!digest)
				return 0x8087101e;
			if (!context->operations)
				return 0x80871800;
			result = context->operations->checkSignature(data, size, digest);
			memset(context->aesContext, 0, context->aesContextSize);
			memset(context, 0, sizeof(*context));
			return result;
		}
	}
	if (size < 0x8000)
		return 0x80871014;
	if (NPDRM_BE32(main->magic) != 0x7f504b47)
		return 0x80871003;
	revision = NPDRM_BE32((unsigned int)main->revision |
		((unsigned int)main->type << 16));
	version = revision & 0x3fffffff;
	if (!version || version > 2 || (revision & 0xc0000000) == 0xc0000000)
		return 0x80871003;
	metadataOffset = NPDRM_BE32(main->metadataOffset);
	if (metadataOffset <= 191)
		return 0x80871003;
	metadataSize = NPDRM_BE32(main->metadataSize);
	if (metadataSize - 64 > 0x8000 ||
		metadataSize - 64 < 8 * NPDRM_BE32(main->metadataCount))
		return 0x80871003;
	if (version == 1) {
		uint64_t dataOffset = NPDRM_BE64(main->dataOffset);
		unsigned int metadataEnd = metadataOffset + metadataSize;

		if (dataOffset == metadataEnd)
			return NPDRM_BE64(main->totalSize) >= metadataEnd + 96ULL +
				NPDRM_BE64(main->dataSize) ? 0x80871007 : 0x80871003;
		if (metadataEnd >= dataOffset)
			return 0x80871007;
	}
	if (NPDRM_BE32(extended->magic) != 0x7f657874)
		return 0x80871003;
	dataSize = NPDRM_BE32(extended->dataSize);
	if (dataSize < 384 || dataSize > 0x8000)
		return 0x80871003;
	headerSize = NPDRM_BE32(extended->headerSize);
	if (headerSize > 0x8000)
		return 0x80871003;
	hmacOffset = NPDRM_BE32(extended->mainHeaderHmacOffset);
	if (hmacOffset > 0x8000 || hmacOffset != headerSize + 192 ||
		metadataOffset < hmacOffset + dataSize ||
		metadataOffset + metadataSize != NPDRM_BE32(extended->metadataHmacOffset) ||
		NPDRM_BE64(main->dataOffset) < metadataOffset + metadataSize + dataSize ||
		NPDRM_BE64(main->totalSize) < dataSize + 32ULL + NPDRM_BE64(extended->tailOffset) ||
		hmacOffset + 384 > 0x8000)
		return 0x80871003;
	if (!NpDrmPackageIsHardwareAllowed(revision >> 31))
		return 0x80871007;
	keyId = NPDRM_BE32(extended->keyId);
	operations = (int)keyId < 0 ? &gNpDrmPackageEncryptedDebugOperations :
		&gNpDrmPackageDebugOperations;
	if (revision & 0x80000000) {
		operations = &gNpDrmPackagePspOperations;
		if (keyId & 0x10000000)
			operations = &gNpDrmPackageLiveAreaOperations;
		if (keyId & 0x08000000)
			operations = &gNpDrmPackageAlternateOperations;
		if (((keyId & 0x10000000) != 0) != ((keyId & 7) == 3) ||
			((keyId & 0x08000000) != 0) != ((keyId & 7) == 4))
			return 0x80871003;
	}
	SceKernelUtilsForDriver_87DC7F2F(data, hmacOffset, hash);
	if (!operations->verifySignature(data + hmacOffset, hash))
		return 0x80871004;
	if ((keyId & 7) - 1 > 3)
		return 0x80871006;
	offset = NPDRM_BE32(extended->fullHeaderHmacOffset);
	if (offset) {
		if (offset + dataSize > 0x8000)
			return 0x80871003;
		if (!operations->verifyHeader(data, size, extended))
			return 0x80871004;
	} else {
		offset = NPDRM_BE32(extended->metadataHmacOffset);
		if (offset + dataSize > 0x8000)
			return 0x80871003;
		SceKernelUtilsForDriver_87DC7F2F(data + metadataOffset, metadataSize - 64, hash);
		if (!operations->verifySignature(data + offset, hash))
			return 0x80871004;
	}
	if (flags & 0x1000) {
		const struct NpDrmPackageRsaCrtMaterial *const *keys =
			(revision & 0x80000000) ? gNpDrmPackageRetailRsaKeys :
			gNpDrmPackageDebugRsaKeys;
		unsigned int i;

		gNpDrmPackageUnwrapKey = keys[0];
		gNpDrmPackageSignKey = keys[1];
		for (i = 0; i < sizeof(gNpDrmPackageContentId); i++)
			gNpDrmPackageContentId[i] = main->contentId[i];
	}
	if (context) {
		context->operations = operations;
		operations->initializeContext(context, header, size);
	}
	return 0;
}

unsigned int NpDrmPackageTransformData(unsigned char *data, unsigned int size,
	uint64_t offset, unsigned int flags)
{
	const struct NpDrmPackageRsaCrtMaterial *material;
	MarlinRsaPrivateKey privateKey;
	MarlinRsaData output, input;
	unsigned char hash[32];
	unsigned char decrypted[256];
	struct NpDrmPackageUnwrappedKey unwrapped;
	SceNpDrmLicense license;
	unsigned int exponent[64];
	MarlinRsaExponentKey publicKey;
	struct NpDrmPackageContext *context;
	unsigned int index, i;
	volatile unsigned int difference;
	int result;

	if (data == NULL)
		return 0x80871800;
	if (flags & 0x4000) {
		material = gNpDrmPackageUnwrapKey;
		if (!material)
			return 0x80871800;
		gNpDrmPackageUnwrapKey = NULL;
		if (size != 256)
			return 0x80871800;
		privateKey.primeP = (unsigned int *)material->primeP;
		privateKey.primeQ = (unsigned int *)material->primeQ;
		privateKey.exponentP = (unsigned int *)material->exponentP;
		privateKey.exponentQ = (unsigned int *)material->exponentQ;
		privateKey.coefficient = (unsigned int *)material->coefficient;
		output = (MarlinRsaData) { decrypted, 256 };
		input = (MarlinRsaData) { data, 256 };
		if (NpDrmRsaDecrypt2048(&output, &input, &privateKey) < 0 ||
			output.size != sizeof(unwrapped))
			return 0x8087101f;
		for (i = 0; i < sizeof(unwrapped); i++)
			((unsigned char *)&unwrapped)[i] = output.data[i];
		difference = 0;
		for (i = 0; i < sizeof(unwrapped.contentId); i++)
			difference += (unsigned char)(gNpDrmPackageContentId[i] -
				unwrapped.contentId[i]);
		if (difference) {
			memset(&unwrapped, 0, sizeof(unwrapped));
			return 0x8087101d;
		}
		memcpy(data, unwrapped.keyMaterial, sizeof(unwrapped.keyMaterial));
		memset(&unwrapped, 0, sizeof(unwrapped));
		return 0;
	}
	if (flags & 0x2000) {
		material = gNpDrmPackageSignKey;
		if (!material)
			return 0x80871800;
		gNpDrmPackageSignKey = NULL;
		if (size != sizeof(license))
			return 0x80871800;
		memcpy(&license, data, sizeof(license));
		license.version = -1;
		license.license_type = 256;
		license.version_flags = 256;
		license.license_flags = 0;
		license.account_id = 0;
		memcpy(license.content_id, gNpDrmPackageContentId,
			sizeof(license.content_id));
		memset(license.key_table, 0, 32);
		memset(license.ecdsa_signature, 0, sizeof(license.ecdsa_signature));
		memset(license.key2, 0, 32);
		memset(license.open_psid, 0, 60);
		SceKernelUtilsForDriver_A773A6A8(&license, 256, hash);
		privateKey.primeP = (unsigned int *)material->primeP;
		privateKey.primeQ = (unsigned int *)material->primeQ;
		privateKey.exponentP = (unsigned int *)material->exponentP;
		privateKey.exponentQ = (unsigned int *)material->exponentQ;
		privateKey.coefficient = (unsigned int *)material->coefficient;
		output = (MarlinRsaData) { (unsigned char *)license.rsa_signature, 256 };
		input = (MarlinRsaData) { hash, 32 };
		if (NpDrmRsaSign2048(&output, &input, &privateKey, 11) < 0) {
			memset(&license, 0, sizeof(license));
			return 0x8087101e;
		}
		memcpy(data, &license, sizeof(license));
		memset(&license, 0, sizeof(license));
		return 0;
	}
	if (flags & 0x20000) {
		if (size < sizeof(license))
			return 0x80871014;
		SceKernelUtilsForDriver_A773A6A8(data, 256, hash);
		memset(exponent, 0, sizeof(exponent));
		exponent[0] = 65537;
		publicKey.modulus = (unsigned int *)gNpDrmPackageRsaModuli[NPDRM_RSA_LIVE_AREA];
		publicKey.exponent = exponent;
		output = (MarlinRsaData) { data + 256, 256 };
		input = (MarlinRsaData) { hash, 20 };
		result = NpDrmRsaVerify2048(&output, &input, &publicKey, 5);
		return (result >> 31) & 0x8087101e;
	}
	index = flags & 0xff;
	if (index > 5)
		return 0x8087101c;
	context = &gNpDrmPackageContexts[index];
	if (!context->operations)
		return 0x80871800;
	return context->operations->cryptData(context, data, size, offset >> 4, flags);
}

int NpDrmPackageHashData(const void *data, unsigned int size,
	SceSblDmac5HashTransformContext *context, unsigned int flags)
{
	uint32_t digest[8];
	int result;

	if (data == NULL)
		return 0x80871800;
	digest[0] = digest[1] = digest[2] = digest[3] = 0;
	digest[4] = digest[5] = digest[6] = digest[7] = 0;
	if (flags & 0x10000) {
		result = NpDrmPackageHashTransform(data, digest, size, context, flags);
		context->state[0] = digest[0];
		context->state[1] = digest[1];
		context->state[2] = digest[2];
		context->state[3] = digest[3];
		context->state[4] = digest[4];
		context->state[5] = digest[5];
		context->state[6] = digest[6];
		context->state[7] = digest[7];
	} else {
		result = NpDrmPackageHashTransform(data, NULL, size, context, flags);
	}
	return result;
}

__attribute__((noinline)) int NpDrmPackageSetAesWorkspace(void *workspace,
	unsigned int stride,
	unsigned int count, unsigned int flags)
{
	unsigned int index;
	int result = workspace == NULL;

	if (count <= 5)
		result |= 1;
	if (result)
		return 0x80871800;
	index = flags & 0xff;
	if (index > 5)
		return 0x8087101c;
	gNpDrmPackageContexts[index].aesContext =
		(SceAesCompactContext *)((unsigned char *)workspace + stride * index);
	gNpDrmPackageContexts[index].aesContextSize = stride;
	return result;
}

#define NPDRM_ACT_INFO_BODY(_call) \
	struct NpDrmCheckActDataArgs args; \
	int *kernelActType = actType, *kernelVersionFlag = versionFlag; \
	uint64_t *kernelAccountId = accountId; \
	uint64_t *kernelStartTime, *kernelExpirationTime; \
	int ret, result; \
 \
	ARGS_IN(args, userArgs); \
	if (actType) { \
		kernelActType = __builtin_alloca(sizeof(*kernelActType)); \
		*kernelActType = 0; \
	} \
	if (versionFlag) { \
		kernelVersionFlag = __builtin_alloca(sizeof(*kernelVersionFlag)); \
		*kernelVersionFlag = 0; \
	} \
	if (accountId) { \
		kernelAccountId = __builtin_alloca(sizeof(*kernelAccountId)); \
		*kernelAccountId = 0; \
	} \
	kernelStartTime = args.startTime; \
	if (args.startTime) { \
		kernelStartTime = __builtin_alloca(sizeof(*kernelStartTime)); \
		*kernelStartTime = 0; \
	} \
	kernelExpirationTime = args.expirationTime; \
	if (args.expirationTime) { \
		kernelExpirationTime = __builtin_alloca(sizeof(*kernelExpirationTime)); \
		*kernelExpirationTime = 0; \
	} \
	result = _call(kernelActType, kernelVersionFlag, kernelAccountId, \
		kernelStartTime, kernelExpirationTime); \
	if (actType) { \
		ret = SceSysmemForDriver_6D88EF8A(actType, kernelActType, sizeof(*actType)); \
		if (ret < 0) \
			return ret; \
	} \
	if (versionFlag) { \
		ret = SceSysmemForDriver_6D88EF8A(versionFlag, kernelVersionFlag, sizeof(*versionFlag)); \
		if (ret < 0) \
			return ret; \
	} \
	if (accountId) { \
		ret = SceSysmemForDriver_6D88EF8A(accountId, kernelAccountId, sizeof(*accountId)); \
		if (ret < 0) \
			return ret; \
	} \
	if (args.startTime) { \
		ret = SceSysmemForDriver_6D88EF8A(args.startTime, kernelStartTime, \
			sizeof(*args.startTime)); \
		if (ret < 0) \
			return ret; \
	} \
	if (args.expirationTime) \
		return copyResult(result, SceSysmemForDriver_6D88EF8A(args.expirationTime, \
			kernelExpirationTime, sizeof(*args.expirationTime))); \
	return result; \

int _sceNpDrmCheckActData(int *actType, int *versionFlag, uint64_t *accountId,
	const struct NpDrmCheckActDataArgs *userArgs)
{
	NPDRM_ACT_INFO_BODY(NpDrmCheckActData);
}

int _sceNpDrmRemoveActData(uint64_t *accountId)
{
	int ret;

	BLOCK_OUT(accountId, sizeof(*accountId), NpDrmRemoveActData(NULL),
		NpDrmRemoveActData((uint64_t *)k));
}

int _sceNpDrmGetRifInfo(const SceNpDrmLicense *license, unsigned int license_size,
	int check_sign, const struct NpDrmGetRifInfoArgs *userArgs)
{
	struct NpDrmGetRifInfoArgs args, kernel;
	SceNpDrmLicense *kernelLicense = NULL;
	int ret, result;

	ARGS_IN(args, userArgs);
	BLOCK_IN(kernelLicense, license, sizeof(*kernelLicense));
	kernel.content_id = args.content_id;
	if (args.content_id) {
		kernel.content_id = __builtin_alloca(48);
		__builtin_memset(kernel.content_id, 0, 48);
	}
	kernel.account_id = args.account_id;
	if (args.account_id) {
		kernel.account_id = __builtin_alloca(8);
		*kernel.account_id = 0;
	}
	kernel.license_version = args.license_version;
	if (args.license_version) {
		kernel.license_version = __builtin_alloca(4);
		*kernel.license_version = 0;
	}
	kernel.license_flags = args.license_flags;
	if (args.license_flags) {
		kernel.license_flags = __builtin_alloca(4);
		*kernel.license_flags = 0;
	}
	kernel.flags = args.flags;
	if (args.flags) {
		kernel.flags = __builtin_alloca(4);
		*kernel.flags = 0;
	}
	kernel.sku_flags = args.sku_flags;
	if (args.sku_flags) {
		kernel.sku_flags = __builtin_alloca(4);
		*kernel.sku_flags = 0;
	}
	kernel.lic_start_time = args.lic_start_time;
	if (args.lic_start_time) {
		kernel.lic_start_time = __builtin_alloca(8);
		*kernel.lic_start_time = 0;
	}
	kernel.lic_exp_time = args.lic_exp_time;
	if (args.lic_exp_time) {
		kernel.lic_exp_time = __builtin_alloca(8);
		*kernel.lic_exp_time = 0;
	}
	kernel.flags2 = args.flags2;
	if (args.flags2) {
		kernel.flags2 = __builtin_alloca(8);
		*kernel.flags2 = 0;
	}
	result = NpDrmGetRifInfo(kernelLicense, license_size, check_sign,
		kernel.content_id, kernel.account_id, kernel.license_version,
		kernel.license_flags, kernel.flags, kernel.sku_flags,
		kernel.lic_start_time, kernel.lic_exp_time, kernel.flags2);
	if (args.content_id) {
		ret = SceSysmemForDriver_6D88EF8A(args.content_id, kernel.content_id, 48);
		if (ret < 0)
			return ret;
	}
	if (args.account_id) {
		ret = SceSysmemForDriver_6D88EF8A(args.account_id, kernel.account_id, 8);
		if (ret < 0)
			return ret;
	}
	if (args.license_version) {
		ret = SceSysmemForDriver_6D88EF8A(args.license_version, kernel.license_version, 4);
		if (ret < 0)
			return ret;
	}
	if (args.license_flags) {
		ret = SceSysmemForDriver_6D88EF8A(args.license_flags, kernel.license_flags, 4);
		if (ret < 0)
			return ret;
	}
	if (args.flags) {
		ret = SceSysmemForDriver_6D88EF8A(args.flags, kernel.flags, 4);
		if (ret < 0)
			return ret;
	}
	if (args.sku_flags) {
		ret = SceSysmemForDriver_6D88EF8A(args.sku_flags, kernel.sku_flags, 4);
		if (ret < 0)
			return ret;
	}
	if (args.lic_start_time) {
		ret = SceSysmemForDriver_6D88EF8A(args.lic_start_time, kernel.lic_start_time, 8);
		if (ret < 0)
			return ret;
	}
	if (args.lic_exp_time) {
		ret = SceSysmemForDriver_6D88EF8A(args.lic_exp_time, kernel.lic_exp_time, 8);
		if (ret < 0)
			return ret;
	}
	if (args.flags2)
		return copyResult(result, SceSysmemForDriver_6D88EF8A(args.flags2,
			kernel.flags2, 8));
	return result;
}

int _sceNpDrmGetRifName(char *rif_name, uint64_t aid)
{
	int ret;

	BLOCK_OUT(rif_name, 48, NpDrmGetRifName(NULL, aid),
		NpDrmGetRifName((char *)k, aid));
}

int _sceNpDrmGetFixedRifName(char *rif_name, uint64_t aid)
{
	int ret;

	BLOCK_OUT(rif_name, 48, NpDrmGetFixedRifName(NULL, aid),
		NpDrmGetFixedRifName((char *)k, aid));
}

int _sceNpDrmGetRifNameForInstall(char *rif_name,
	const SceNpDrmLicense *license, unsigned int is_fixed)
{
	char *kernelName = rif_name;
	SceNpDrmLicense *kernelLicense = NULL;
	int ret;

	if (rif_name) {
		kernelName = __builtin_alloca(48);
		__builtin_memset(kernelName, 0, 48);
	}
	BLOCK_IN(kernelLicense, license, sizeof(*kernelLicense));
	ret = NpDrmGetRifNameForInstall(kernelName, kernelLicense, is_fixed);
	if (rif_name)
		return copyResult(ret, SceSysmemForDriver_6D88EF8A(rif_name, kernelName, 48));
	return ret;
}

unsigned int _sceNpDrmCheckDrmReset(const void *in_addr, unsigned int size,
	const SceNpDrmCheckDrmResetOpt *pOpt)
{
	SceNpDrmCheckDrmResetOpt args;
	void *kernelInput = (void *)in_addr;
	uint64_t account_id;
	int *pReset;
	int ret;

	ARGS_IN(args, pOpt);
	account_id = args.account_id;
	pReset = args.pReset;
	BLOCK_IN_SIZED(kernelInput, in_addr, args.in_size, 64);
	BLOCK_OUT(pReset, 4,
		NpDrmCheckDrmReset(kernelInput, size, account_id, NULL),
		NpDrmCheckDrmReset(kernelInput, size, account_id, (int *)k));
}

int _sceNpDrmPresetRifProvisionalFlag(SceNpDrmLicense *license)
{
	SceNpDrmLicense *kernelLicense;
	int ret;

	if (!license)
		return NpDrmPresetRifProvisionalFlag(NULL);
	kernelLicense = __builtin_alloca(sizeof(*kernelLicense));
	memset(kernelLicense, 0, sizeof(*kernelLicense));
	ret = SceSysmemForDriver_BC996A7A(kernelLicense, license, sizeof(*kernelLicense));
	if (ret < 0)
		return ret;
	ret = NpDrmPresetRifProvisionalFlag(kernelLicense);
	return copyResult(ret, SceSysmemForDriver_6D88EF8A(license, kernelLicense,
		sizeof(*kernelLicense)));
}

__attribute__((noinline)) int NpDrmRemoveActData(uint64_t *accountId)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = ksceNpDrmRemoveActData(accountId);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmCheckActData(int *actType, int *versionFlag,
	uint64_t *accountId, uint64_t *startTime, uint64_t *expirationTime)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = ksceNpDrmCheckActData(actType, versionFlag, accountId,
		startTime, expirationTime);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmGetRifName(char *name, uint64_t aid)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = ksceNpDrmGetRifName(name, aid);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmGetFixedRifName(char *name, uint64_t aid)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = ksceNpDrmGetFixedRifName(name, aid);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmGetRifNameForInstall(char *rif_name,
	const SceNpDrmLicense *license, unsigned int is_fixed)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = ksceNpDrmGetRifNameForInstall(rif_name, license, is_fixed);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmGetRifInfo(const SceNpDrmLicense *license,
	unsigned int license_size, int check_sign, char *content_id,
	uint64_t *account_id, int *license_version, int *license_flags, int *flags,
	int *sku_flags, int64_t *lic_start_time, int64_t *lic_exp_time,
	uint64_t *flags2)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = ksceNpDrmGetRifInfo(license, license_size, check_sign, content_id,
		account_id, license_version, license_flags, flags, sku_flags,
		lic_start_time, lic_exp_time, flags2);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmCheckDrmReset(const void *in_addr,
	unsigned int size, uint64_t account_id, int *pReset)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = NpDrmCheckDrmResetForDriver(in_addr, size, account_id, pReset);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmPresetRifProvisionalFlag(SceNpDrmLicense *license)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = ksceNpDrmPresetRifProvisionalFlag(license, 1);

	sceSyscallLeave(syscall);
	return ret;
}

int scePsmDrmRemoveActData(uint64_t *accountId)
{
	int ret;

	BLOCK_OUT(accountId, 8, PsmDrmRemoveActData(NULL),
		PsmDrmRemoveActData((uint64_t *)k));
}

int scePsmDrmGetActInfo(int *actType, int *versionFlag, uint64_t *accountId,
	const struct NpDrmCheckActDataArgs *userArgs)
{
	NPDRM_ACT_INFO_BODY(PsmDrmGetActInfo);
}

int scePsmDrmGetRifInfo(const ScePsmDrmLicense *license, char *contentId,
	uint64_t *accountId, const struct NpDrmCheckActDataArgs *userArgs)
{
	struct NpDrmCheckActDataArgs args;
	ScePsmDrmLicense *kernelLicense = NULL;
	char *kernelContentId = contentId;
	uint64_t *kernelAccountId = accountId;
	uint64_t *kernelStartTime, *kernelExpirationTime;
	int ret, result;

	ARGS_IN(args, userArgs);
	kernelExpirationTime = args.expirationTime;
	kernelStartTime = args.startTime;
	BLOCK_IN(kernelLicense, license, sizeof(*kernelLicense));
	if (contentId) {
		kernelContentId = __builtin_alloca(48);
		__builtin_memset(kernelContentId, 0, 48);
	}
	if (accountId) {
		kernelAccountId = __builtin_alloca(sizeof(*kernelAccountId));
		*kernelAccountId = 0;
	}
	if (kernelStartTime) {
		kernelStartTime = __builtin_alloca(sizeof(*kernelStartTime));
		*kernelStartTime = 0;
	}
	if (kernelExpirationTime) {
		kernelExpirationTime = __builtin_alloca(sizeof(*kernelExpirationTime));
		*kernelExpirationTime = 0;
	}
	result = PsmDrmGetRifInfo(kernelLicense, kernelContentId, kernelAccountId,
		kernelStartTime, kernelExpirationTime);
	if (contentId) {
		ret = SceSysmemForDriver_6D88EF8A(contentId, kernelContentId, 48);
		if (ret < 0)
			return ret;
	}
	if (accountId) {
		ret = SceSysmemForDriver_6D88EF8A(accountId, kernelAccountId, sizeof(*accountId));
		if (ret < 0)
			return ret;
	}
	if (args.startTime) {
		ret = SceSysmemForDriver_6D88EF8A(args.startTime, kernelStartTime,
			sizeof(*args.startTime));
		if (ret < 0)
			return ret;
	}
	if (args.expirationTime)
		return copyResult(result, SceSysmemForDriver_6D88EF8A(args.expirationTime,
			kernelExpirationTime, sizeof(*args.expirationTime)));
	return result;
}

int scePsmDrmGetRifName(char *name)
{
	int ret;

	BLOCK_OUT(name, 48, PsmDrmGetRifName(NULL),
		PsmDrmGetRifName((char *)k));
}

int scePsmDrmGetRifNameForInstall(char *name, const ScePsmDrmLicense *license)
{
	char *kernelName = name;
	ScePsmDrmLicense *kernelLicense = NULL;
	int ret;

	if (name) {
		kernelName = __builtin_alloca(48);
		__builtin_memset(kernelName, 0, 48);
	}
	BLOCK_IN(kernelLicense, license, sizeof(*kernelLicense));
	ret = PsmDrmGetRifNameForInstall(kernelName, kernelLicense);
	if (name)
		return copyResult(ret, SceSysmemForDriver_6D88EF8A(name, kernelName, 48));
	return ret;
}

int scePsmDrmGetRifKey(const ScePsmDrmLicense *license_buf,
	ScePsmDrmKeySet *keydata, int *flags, const ScePsmDrmExpireTime *exp_time)
{
	ScePsmDrmExpireTime args;
	ScePsmDrmLicense *kernelLicense = NULL;
	ScePsmDrmKeySet *kernelKeydata = keydata;
	int *kernelFlags = flags;
	SceRtcTick *kernelStartTime, *kernelExpirationTime;
	int ret, result;

	ARGS_IN(args, exp_time);
	kernelStartTime = args.start_date;
	kernelExpirationTime = args.end_date;
	BLOCK_IN(kernelLicense, license_buf, sizeof(*kernelLicense));
	if (keydata) {
		kernelKeydata = __builtin_alloca(sizeof(*kernelKeydata));
		memset(kernelKeydata, 0, sizeof(*kernelKeydata));
	}
	if (flags) {
		kernelFlags = __builtin_alloca(sizeof(*kernelFlags));
		*kernelFlags = 0;
	}
	if (kernelStartTime) {
		kernelStartTime = __builtin_alloca(sizeof(*kernelStartTime));
		kernelStartTime->tick = 0;
	}
	if (kernelExpirationTime) {
		kernelExpirationTime = __builtin_alloca(sizeof(*kernelExpirationTime));
		kernelExpirationTime->tick = 0;
	}
	result = PsmDrmGetRifKey(kernelLicense, kernelKeydata, kernelFlags,
		kernelStartTime, kernelExpirationTime);
	if (keydata) {
		ret = SceSysmemForDriver_6D88EF8A(keydata, kernelKeydata, sizeof(*keydata));
		if (ret < 0)
			return ret;
	}
	if (flags) {
		ret = SceSysmemForDriver_6D88EF8A(flags, kernelFlags, sizeof(*flags));
		if (ret < 0)
			return ret;
	}
	if (args.start_date) {
		ret = SceSysmemForDriver_6D88EF8A(args.start_date, kernelStartTime,
			sizeof(*args.start_date));
		if (ret < 0)
			return ret;
	}
	if (args.end_date)
		return copyResult(result, SceSysmemForDriver_6D88EF8A(args.end_date,
			kernelExpirationTime, sizeof(*args.end_date)));
	return result;
}

__attribute__((noinline)) int PsmDrmRemoveActData(uint64_t *accountId)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = kscePsmDrmRemoveActData(accountId);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int PsmDrmGetActInfo(int *actType, int *versionFlag,
	uint64_t *accountId, uint64_t *startTime, uint64_t *expirationTime)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = kscePsmDrmGetActInfo(actType, versionFlag, accountId,
		startTime, expirationTime);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int PsmDrmGetRifInfo(const ScePsmDrmLicense *license,
	char *contentId, uint64_t *accountId, uint64_t *startTime,
	uint64_t *expirationTime)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = kscePsmDrmGetRifInfo(license, contentId, accountId,
		startTime, expirationTime);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int PsmDrmGetRifName(char *name)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = PsmDrmGetRifNameForDriver(name);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int PsmDrmGetRifNameForInstall(char *name,
	const ScePsmDrmLicense *license)
{
	unsigned int syscall = sceSyscallEnter();
	int ret = PsmDrmGetRifNameForInstallForDriver(name, license);

	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int PsmDrmGetRifKey(const ScePsmDrmLicense *license,
	ScePsmDrmKeySet *keydata, int *flags, SceRtcTick *startTime,
	SceRtcTick *expirationTime)
{
	unsigned int syscall = sceSyscallEnter();
	int ret;

	if (!SceSblACMgrForDriver_091F7247(0)) {
		sceSyscallLeave(syscall);
		return 0x80010058;
	}
	ret = kscePsmDrmGetRifKey(license, keydata, flags, startTime,
		expirationTime);
	sceSyscallLeave(syscall);
	return ret;
}

int _sceNpDrmPackageCheck(const void *buffer, unsigned int size,
	const unsigned char *digest, unsigned int identifier)
{
	unsigned char *kernelDigest = NULL;
	int ret;

	BLOCK_IN(kernelDigest, digest, 32);
	return NpDrmPackageCheckForUser(buffer, size, kernelDigest, identifier);
}

int _sceNpDrmPackageDecrypt(void *buffer, unsigned int size,
	const _sceNpDrmPackageDecrypt_opt *opt)
{
	_sceNpDrmPackageDecrypt_opt args;
	int ret;

	ARGS_IN(args, opt);
	return NpDrmPackageDecryptForUser(buffer, size, args.offset, args.identifier);
}

int _sceNpDrmPackageTransform(void *buffer, unsigned int size,
	SceSblDmac5HashTransformContext *context, unsigned int flags)
{
	SceSblDmac5HashTransformContext *kernelContext;
	int ret, result;

	if (context) {
		kernelContext = __builtin_alloca(sizeof(*kernelContext));
		*kernelContext = (SceSblDmac5HashTransformContext){ 0 };
		ret = SceSysmemForDriver_BC996A7A(kernelContext, context,
			sizeof(*kernelContext));
		if (ret < 0)
			return ret;
		result = NpDrmPackageTransformForUser(buffer, size, kernelContext,
			flags);
		return copyResult(result, SceSysmemForDriver_6D88EF8A(context,
			kernelContext, sizeof(*kernelContext)));
	}
	return NpDrmPackageTransformForUser(buffer, size, NULL, flags);
}

#define NPDRM_PACKAGE_STARTED_BODY(_call) \
	struct NpDrmPackageInstallStartedArgs args; \
	char *kernelMsg = NULL; \
	int ret, len; \
	ARGS_IN(args, opt); \
	len = args.len; \
	BLOCK_IN_SIZED(kernelMsg, msg, args.size, 64); \
	return _call(kind, total, kernelMsg, len)

int _sceNpDrmPackageInstallStarted(int kind, unsigned int total,
	const char *msg, const struct NpDrmPackageInstallStartedArgs *opt)
{
	NPDRM_PACKAGE_STARTED_BODY(NpDrmPackageInstallStarted);
}

#define NPDRM_PACKAGE_FINISHED_BODY(_call) \
	SyscallSizeArgs args; \
	char *kernelMsg = NULL; \
	int ret; \
	ARGS_IN(args, opt); \
	BLOCK_IN_SIZED(kernelMsg, msg, args.size, 64); \
	return _call(result, kernelMsg, len)

int _sceNpDrmPackageInstallFinished(unsigned int result, const char *msg,
	int len, const SyscallSizeArgs *opt)
{
	NPDRM_PACKAGE_FINISHED_BODY(NpDrmPackageInstallFinished);
}

int _sceNpDrmPackageUninstallStarted(int kind, unsigned int total,
	const char *msg, const struct NpDrmPackageInstallStartedArgs *opt)
{
	NPDRM_PACKAGE_STARTED_BODY(NpDrmPackageUninstallStarted);
}

int _sceNpDrmPackageUninstallFinished(unsigned int result, const char *msg,
	int len, const SyscallSizeArgs *opt)
{
	NPDRM_PACKAGE_FINISHED_BODY(NpDrmPackageUninstallFinished);
}

int _sceNpDrmSaveDataInstallStarted(int kind, unsigned int total,
	const char *msg, const struct NpDrmPackageInstallStartedArgs *opt)
{
	NPDRM_PACKAGE_STARTED_BODY(NpDrmSaveDataInstallStarted);
}

int _sceNpDrmSaveDataInstallFinished(unsigned int result, const char *msg,
	int len, const SyscallSizeArgs *opt)
{
	NPDRM_PACKAGE_FINISHED_BODY(NpDrmSaveDataInstallFinished);
}

int _sceNpDrmSaveDataFormatStarted(int kind, unsigned int total,
	const char *msg, const struct NpDrmPackageInstallStartedArgs *opt)
{
	NPDRM_PACKAGE_STARTED_BODY(NpDrmSaveDataFormatStarted);
}

int _sceNpDrmSaveDataFormatFinished(unsigned int result, const char *msg,
	int len, const SyscallSizeArgs *opt)
{
	NPDRM_PACKAGE_FINISHED_BODY(NpDrmSaveDataFormatFinished);
}

int NpDrmPackageCheckOnStack(void *argp)
{
	const struct NpDrmPackageCheckArgs *args = argp;

	return NpDrmPackageProcessHeader(args->data, args->size, args->digest,
		args->flags);
}

int NpDrmPackageDecryptOnStack(void *argp)
{
	const struct NpDrmPackageDecryptArgs *args = argp;

	return NpDrmPackageTransformData(args->data, args->size, args->offset,
		args->flags);
}

__attribute__((noinline)) int NpDrmPackageAesInitialize(
	SceAesCompactContext *context, const void *key, unsigned int keySize)
{
	return SceKernelUtilsForDriver_EDA97D6D((void *)context, 128, keySize * 8, key);
}

__attribute__((noinline)) int NpDrmPackageAesEncrypt(
	SceAesCompactContext *context, void *output, const void *input)
{
	return SceKernelUtilsForDriver_302947B6((void *)context, input, output);
}

__attribute__((noinline)) int NpDrmPackageAesCtr(const void *input,
	void *output, unsigned int size, const void *key, unsigned int keyBits,
	void *counter)
{
	return ksceSblDmac5AesCtrEnc(input, output, size, key, keyBits, counter, 1);
}

__attribute__((noinline)) int NpDrmPackageHashTransform(const void *input,
	void *digest, unsigned int size, SceSblDmac5HashTransformContext *context,
	unsigned int flags)
{
	unsigned int command = (int)(flags << 1) < 0 ? 3 : 0x13;
	unsigned int mode;

	if (context->length == 0)
		mode = digest ? 0 : 0x400;
	else
		mode = digest ? 0x800 : 0xc00;
	return ksceSblSsMgrExecuteDmac5HashCommand(input, digest, size, context,
		1, command, mode);
}

__attribute__((noinline)) int NpDrmPackageIsHardwareAllowed(unsigned int retail)
{
	if (retail) {
		if (SceSblAIMgrForDriver_D78B04A2())
			return 1;
		if (ksceSblUtMgrHasStoreFlag())
			return 1;
		return SceQafMgrForDriver_AE033133() != 0;
	}
	if (SceSblAIMgrForDriver_F4B98F66())
		return 1;
	return SceSblAIMgrForDriver_274663A0() != 0;
}

__attribute__((noinline)) int NpDrmPackageCheckForUser(const void *buffer,
	unsigned int size, const unsigned char *digest, unsigned int identifier)
{
	unsigned int syscall = sceSyscallEnter();
	uint64_t authId = 0;
	struct NpDrmPackageCheckArgs args;
	void *kernelPage;
	SceSize kernelSize;
	unsigned int kernelOffset;
	SceUID mapId;
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = 0x80010058;
		goto out;
	}
	ret = sceSblACMgrGetPaid(0, &authId);
	if (ret < 0)
		goto out;
	if (authId != UINT64_C(0x2800000000000001) &&
		authId - UINT64_C(0x280000000000002d) >= 2 &&
		authId != UINT64_C(0x2800000000000039)) {
		ret = 0x8001000d;
		goto out;
	}
	if (!size) {
		ret = 0x80010016;
		goto out;
	}
	ret = SceSysmemForDriver_9F6E45E3(0, 0x10, buffer, size);
	if (ret < 0)
		goto out;
	mapId = SceSysmemForDriver_278BC201("SceNpDrmPackageChk", 1, buffer, size,
		&kernelPage, &kernelSize, &kernelOffset);
	if (mapId < 0) {
		ret = mapId;
		goto out;
	}
	args.data = (unsigned char *)kernelPage + kernelOffset;
	ret = NpDrmPackageSetAesWorkspace(gNpDrmPackageAesWorkspaces,
		sizeof(gNpDrmPackageAesWorkspaces[0]), 6, identifier);
	if (!ret) {
		args.size = size;
		args.digest = digest;
		args.flags = identifier;
		ret = SceThreadmgrForDriver_E54FD746(0x2000, NpDrmPackageCheckOnStack, &args);
	}
	sceKernelUserUnmap(mapId, buffer, size);
out:
	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmPackageDecryptForUser(void *buffer,
	unsigned int size, int64_t offset, unsigned int identifier)
{
	unsigned int syscall = sceSyscallEnter();
	uint64_t authId = 0;
	struct NpDrmPackageDecryptArgs args;
	void *kernelPage;
	SceSize kernelSize;
	unsigned int kernelOffset;
	SceUID mapId;
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = 0x80010058;
		goto out;
	}
	ret = sceSblACMgrGetPaid(0, &authId);
	if (ret < 0)
		goto out;
	if (authId != UINT64_C(0x2800000000000001) &&
		authId - UINT64_C(0x280000000000002d) >= 2 &&
		authId != UINT64_C(0x2800000000000039)) {
		ret = 0x8001000d;
		goto out;
	}
	if (!size) {
		ret = 0x80010016;
		goto out;
	}
	ret = SceSysmemForDriver_9F6E45E3(0, 0x30, buffer, size);
	if (ret < 0)
		goto out;
	mapId = SceSysmemForDriver_278BC201("SceNpDrmPackageDec", 3, buffer, size,
		&kernelPage, &kernelSize, &kernelOffset);
	if (mapId < 0) {
		ret = mapId;
		goto out;
	}
	args.data = (unsigned char *)kernelPage + kernelOffset;
	args.size = size;
	args.offset = offset;
	args.flags = identifier;
	args.reserved = 0;
	ret = SceThreadmgrForDriver_E54FD746(0x2000, NpDrmPackageDecryptOnStack, &args);
	sceKernelUserUnmap(mapId, buffer, size);
out:
	sceSyscallLeave(syscall);
	return ret;
}

__attribute__((noinline)) int NpDrmPackageTransformForUser(void *buffer,
	unsigned int size, SceSblDmac5HashTransformContext *context,
	unsigned int flags)
{
	unsigned int syscall = sceSyscallEnter();
	uint64_t authId = 0;
	void *kernelPage;
	SceSize kernelSize;
	unsigned int kernelOffset;
	SceUID mapId;
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = 0x80010058;
		goto out;
	}
	ret = sceSblACMgrGetPaid(0, &authId);
	if (ret < 0)
		goto out;
	if (authId != UINT64_C(0x2800000000000001) &&
		authId - UINT64_C(0x280000000000002d) >= 2 &&
		authId != UINT64_C(0x2800000000000039)) {
		ret = 0x8001000d;
		goto out;
	}
	if (!size) {
		ret = 0x80010016;
		goto out;
	}
	ret = SceSysmemForDriver_9F6E45E3(0, 0x10, buffer, size);
	if (ret < 0)
		goto out;
	mapId = SceSysmemForDriver_278BC201("SceNpDrmPackageTrans", 1, buffer, size,
		&kernelPage, &kernelSize, &kernelOffset);
	if (mapId < 0) {
		ret = mapId;
		goto out;
	}
	ret = NpDrmPackageHashData((unsigned char *)kernelPage + kernelOffset,
		size, context, flags);
	sceKernelUserUnmap(mapId, buffer, size);
out:
	sceSyscallLeave(syscall);
	return ret;
}

int ksceNpDrmPackageSetGameExist(int value)
{
	gNpDrmPackageGameExist = value;
	return 0;
}

int sceNpDrmPackageIsGameExist(void)
{
	return gNpDrmPackageGameExist;
}

#define NPDRM_PACKAGE_PROGRESS_BODY(_call) \
	unsigned int syscall = sceSyscallEnter(); \
	uint64_t authId = 0; \
	int ret; \
	if (!sceSblACMgrIsSystemProgram(0)) { \
		ret = 0x80010058; \
		goto out; \
	} \
	ret = sceSblACMgrGetPaid(0, &authId); \
	if (ret < 0) \
		goto out; \
	if (authId == UINT64_C(0x2800000000000001)) \
		goto notify; \
	if (authId - UINT64_C(0x280000000000002d) < 2) \
		goto notify; \
	if (authId != UINT64_C(0x2800000000000039)) { \
		ret = 0x8001000d; \
		goto out; \
	} \
notify: \
	ret = (_call); \
out: \
	sceSyscallLeave(syscall); \
	return ret

__attribute__((noinline)) int NpDrmPackageInstallStarted(int kind,
	unsigned int total, const char *msg, int len)
{
	NPDRM_PACKAGE_PROGRESS_BODY(
		SceSysrootForDriver_70A60BBA(kind, total, msg, len));
}

int sceNpDrmPackageInstallOngoing(int kind, unsigned int percent)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_79458ACB(kind, percent));
}

__attribute__((noinline)) int NpDrmPackageInstallFinished(unsigned int result,
	const char *msg, int len)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_D03741B8(result, msg, len));
}

__attribute__((noinline)) int NpDrmPackageUninstallStarted(int kind,
	unsigned int total, const char *msg, int len)
{
	NPDRM_PACKAGE_PROGRESS_BODY(
		SceSysrootForDriver_56C4C949(kind, total, msg, len));
}

int sceNpDrmPackageUninstallOngoing(int kind, unsigned int percent)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_7998F5B5(kind, percent));
}

__attribute__((noinline)) int NpDrmPackageUninstallFinished(unsigned int result,
	const char *msg, int len)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_0F59B369(result, msg, len));
}

__attribute__((noinline)) int NpDrmSaveDataInstallStarted(int kind,
	unsigned int total, const char *msg, int len)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_97E2FB77(kind, total, msg, len));
}

int sceNpDrmSaveDataInstallOngoing(int kind, unsigned int percent)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_F0C91CD3(kind, percent));
}

__attribute__((noinline)) int NpDrmSaveDataInstallFinished(unsigned int result,
	const char *msg, int len)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_E384DD93(result, msg, len));
}

__attribute__((noinline)) int NpDrmSaveDataFormatStarted(int kind,
	unsigned int total, const char *msg, int len)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_E8EBF179(kind, total, msg, len));
}

int sceNpDrmSaveDataFormatOngoing(int kind, unsigned int percent)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_1BA0C6C1(kind, percent));
}

__attribute__((noinline)) int NpDrmSaveDataFormatFinished(unsigned int result,
	const char *msg, int len)
{
	NPDRM_PACKAGE_PROGRESS_BODY(SceSysrootForDriver_D04C7723(result, msg, len));
}
