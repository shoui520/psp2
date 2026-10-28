/* DrmBB: PSP license keys and PGD-protected data, on a kernel stack. */
#include <string.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/utils.h>
#include <intrmgr/syscall.h>
#include <gcauthmgr/sblgcauthmgrdrmbb.h>
#include "drm_bb.h"
#include "gcauth.h"

/* the MAC key of a PGD header */
static const unsigned char gcAuthPgdHeaderKey[16] = {
	0xed, 0xe2, 0x5d, 0x2d, 0xbb, 0xf8, 0x12, 0xe5,
	0x3c, 0x5c, 0x59, 0x32, 0xfa, 0xe3, 0xe2, 0x43,
};

/* a license's key encrypted under the key of its type (1..3) */
static const unsigned char gcAuthDrmLicenseTypeKeys[3][16] = {
	{
		0x07, 0x3d, 0x9e, 0x9d, 0xa8, 0xfd, 0x3b, 0x2f,
		0x63, 0x18, 0x93, 0x2e, 0xf8, 0x57, 0xa6, 0x64,
	},
	{
		0x37, 0x49, 0xb7, 0x01, 0xca, 0xe2, 0xe0, 0xc5,
		0x44, 0x2e, 0x06, 0xb6, 0x1e, 0xff, 0x84, 0xf2,
	},
	{
		0x9d, 0x31, 0xb8, 0x5a, 0xc8, 0xfa, 0x16, 0x80,
		0x73, 0x60, 0x18, 0x82, 0x18, 0x77, 0x91, 0x9d,
	},
};

/* the license key's final decryption */
static const unsigned char gcAuthDrmLicenseKey[16] = {
	0xba, 0x87, 0xe4, 0xab, 0x2c, 0x60, 0x5f, 0x59,
	0xb8, 0x3b, 0xdb, 0xa6, 0x82, 0xfd, 0xae, 0x14,
};

/* what a PGD of version 2 or 3 MACs to derive its key */
static const unsigned char gcAuthPgdVersionData[32] = {
	0x2a, 0x05, 0x54, 0x40, 0x62, 0xd9, 0x1f, 0xe3,
	0xf2, 0xd0, 0x2b, 0xc6, 0x21, 0xff, 0x20, 0x0e,
	0xb1, 0x44, 0x28, 0xdf, 0x0a, 0xcd, 0x14, 0x5b,
	0xc8, 0x19, 0x36, 0x90, 0xd1, 0x42, 0x99, 0x2f,
};

/* the MAC key of a license body */
static const unsigned char gcAuthDrmLicenseMacKey[16] = {
	0x38, 0x20, 0xd0, 0x11, 0x07, 0xa3, 0xff, 0x3e,
	0x0a, 0x4c, 0x20, 0x85, 0x39, 0x10, 0xb5, 0x54,
};

int SceSblGcAuthMgrDrmBBForDriver_812B2B5C(int type, unsigned int *value)
{
	unsigned char *bytes = (unsigned char *)value;

	bytes[0] = 0;
	bytes[1] = 0;
	bytes[2] = 0;
	bytes[3] = 0;
	return 0;
}

int error(void)
{
	return (int)0x808a040a;
}

int GcAuthGetCartSecretForUser(GcAuthCartSecretData *secret)
{
	GcAuthGetCartSecretRequest request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.secret = secret;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthGetCartSecret, &request);
	sceSyscallLeave(state);
	return result;
}

static inline __attribute__((always_inline)) void GcAuthDrmKeyClear(GcAuthDrmKey *key)
{
	key->words[0] = 0;
	key->words[1] = 0;
	key->words[2] = 0;
	key->words[3] = 0;
}

typedef union GcAuthDrmLicenseWork {
	GcAuthDrmBbMacContext mac;
	SceAesFullContext aes;
} GcAuthDrmLicenseWork;

int GcAuthDrmGetLicenseKey(const GcAuthDrmLicenseKeyRequest *request)
{
	const GcAuthDrmLicense *license = request->license;
	const GcAuthDrmKey *key = request->key;
	const unsigned char *secret = request->secret;
	GcAuthDrmKey *output = request->output;
	GcAuthDrmBbMacContext context;
	GcAuthDrmLicenseWork work;
	GcAuthDrmKey licenseKey;
	unsigned int type;
	int result;
	int index;

	if (license == NULL || output == NULL)
		return (int)0x808a040a;
	result = license->version;
	type = license->type & 0xffffff;
	if (result != 0 || type > 3)
		return (int)0x808a040b;
	if ((int)license->type < 0) {
		memset(output, 0xaa, 16);
		return result;
	}
	if (!(license->type & 0x1000000)) {
		if (key == NULL)
			return (int)0x808a040a;
		licenseKey = *key;
	} else {
		result = SceSblGcAuthMgrDrmBBForDriver_48D7784E(&work.mac, 1);
		if (result != 0)
			return result;
		result = SceSblGcAuthMgrDrmBBForDriver_500F5157(&work.mac, license->body,
			sizeof(license->body));
		if (result != 0)
			return result;
		result = SceSblGcAuthMgrDrmBBForDriver_6EF3C9DB(&work.mac, licenseKey.bytes,
			gcAuthDrmLicenseMacKey);
		if (result != 0)
			return result;
	}
	if (type - 1 <= 2) {
		SceKernelUtilsForDriver_F12B6451(&work.aes, 128, 128,
			gcAuthDrmLicenseTypeKeys[type - 1]);
		SceKernelUtilsForDriver_C2A61770(&work.aes, &licenseKey, &licenseKey);
		memset(&work.aes, 0, sizeof(work.aes));
	}
	if (license->flags & 1) {
		if (secret == NULL) {
			GcAuthDrmKeyClear(&licenseKey);
			return (int)0x808a040a;
		}
		result = SceSblGcAuthMgrDrmBBForDriver_48D7784E(&context, 3);
		if (result != 0)
			return result;
		result = SceSblGcAuthMgrDrmBBForDriver_500F5157(&context,
			(const unsigned char *)license, offsetof(GcAuthDrmLicense, mac));
		if (result != 0)
			return result;
		if (SceSblGcAuthMgrDrmBBForDriver_3C25F9FA(&context, license->mac,
				secret) != 0) {
			GcAuthDrmKeyClear(&licenseKey);
			return (int)0x808a040b;
		}
		for (index = 0; index != 16; index++)
			licenseKey.bytes[index] ^= secret[index];
	}
	if (license->flags & 2) {
		for (index = 0; index != 16; index++)
			licenseKey.bytes[index] ^= license->keyMask[index];
	}
	SceKernelUtilsForDriver_F12B6451(&work.aes, 128, 128, gcAuthDrmLicenseKey);
	SceKernelUtilsForDriver_D8678061(&work.aes, &licenseKey, &licenseKey);
	memset(&work.aes, 0, sizeof(work.aes));
	*output = licenseKey;
	GcAuthDrmKeyClear(&licenseKey);
	return 0;
}

int SceSblGcAuthMgrDrmBBForDriver_4C5DE1AA(const void *license, const void *key,
		const void *secret, void *output)
{
	GcAuthDrmLicenseKeyRequest request;

	request.license = license;
	request.key = key;
	request.secret = secret;
	request.output = output;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthDrmGetLicenseKey, &request);
}

int SceSblGcAuthMgrDrmBBForDriver_4FE89ADB(const void *key, const void *pgd,
		unsigned int pgdSize, void *output, unsigned int outputSize)
{
	GcAuthPgdDecryptRequest request;

	request.key = key;
	request.pgd = pgd;
	request.pgdSize = pgdSize;
	request.output = output;
	request.outputSize = outputSize;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthPgdDecryptData, &request);
}

static int GcAuthPgdCheckMac(const unsigned char *data, size_t size,
		const GcAuthDrmKey *key, const unsigned char *mac, unsigned int mode,
		unsigned int counter)
{
	GcAuthDrmBbMacContext context __attribute__((aligned(8)));
	GcAuthDrmKey macKey __attribute__((aligned(8)));
	int result;

	macKey = *key;
	if (counter != 0)
		macKey.words[0] ^= counter;
	result = SceSblGcAuthMgrDrmBBForDriver_48D7784E(&context, mode);
	if (result >= 0) {
		result = SceSblGcAuthMgrDrmBBForDriver_500F5157(&context, data, size);
		if (result >= 0)
			result = SceSblGcAuthMgrDrmBBForDriver_3C25F9FA(&context, mac,
				macKey.bytes);
	}
	if (result > 0)
		result = 0;
	__builtin_memset(&context, 0, sizeof(context));
	__builtin_memset(&macKey, 0, sizeof(macKey));
	return result;
}

static int GcAuthPgdDecrypt(unsigned char *data, size_t size, unsigned int counter,
		const unsigned char *key, const GcAuthDrmKey *seed, int mode)
{
	GcAuthDrmBbCipherContext context __attribute__((aligned(8)));
	GcAuthDrmKey cipherSeed __attribute__((aligned(8)));
	int result;

	cipherSeed = *seed;
	result = SceSblGcAuthMgrDrmBBForDriver_7F98E4E2(&context, mode, 2,
		cipherSeed.bytes, key, counter);
	if (result >= 0) {
		result = SceSblGcAuthMgrDrmBBForDriver_C0F37F18(&context, data, size);
		if (result >= 0)
			result = SceSblGcAuthMgrDrmBBForDriver_6E6D2B89(&context);
	}
	if (result > 0)
		result = 0;
	__builtin_memset(&cipherSeed, 0, sizeof(cipherSeed));
	__builtin_memset(&context, 0, sizeof(context));
	return result;
}

#define GCAUTH_PGD_MAGIC	0x44475000	/* "\0PGD" */
#define GCAUTH_PGD_BLOCK	1024

int GcAuthPgdDecryptData(const GcAuthPgdDecryptRequest *request)
{
	const GcAuthDrmKey *key = request->key;
	const unsigned char *pgd = request->pgd;
	unsigned int pgdSize = request->pgdSize;
	unsigned char *output = request->output;
	unsigned int outputSize = request->outputSize;
	GcAuthDrmBbMacContext context;
	GcAuthPgdHeader header;
	GcAuthDrmKey licenseKey;
	GcAuthDrmKey tail;
	const unsigned char *data;
	const unsigned char *blockMac;
	unsigned int macMode;
	int cipherMode;
	unsigned int limit;
	unsigned int blocks;
	unsigned int block;
	unsigned int offset;
	unsigned int length;
	unsigned int copied;
	unsigned int aligned;
	unsigned int done;
	unsigned int keyIndex;
	int result;

	if (pgd == NULL || key == NULL || output == NULL)
		return (int)0x808a040a;
	if (pgdSize < sizeof(header))
		return (int)0x808a040b;
	memcpy(&header, pgd, sizeof(header));
	if (header.magic != GCAUTH_PGD_MAGIC)
		return (int)0x808a040b;
	keyIndex = header.keyIndex;
	if (keyIndex > 3)
		return (int)0x808a040b;
	if (keyIndex == 1) {
		licenseKey = *key;
	} else {
		result = SceSblGcAuthMgrDrmBBForDriver_48D7784E(&context, 1);
		if (result < 0)
			return result;
		result = SceSblGcAuthMgrDrmBBForDriver_500F5157(&context,
			gcAuthPgdVersionData, 16 * (keyIndex - 1));
		if (result < 0)
			return result;
		result = SceSblGcAuthMgrDrmBBForDriver_6EF3C9DB(&context, licenseKey.bytes,
			key->bytes);
		__builtin_memset(&context, 0, sizeof(context));
		if (result < 0)
			return result;
	}
	if (header.drmType == 0) {
		macMode = 2;
		cipherMode = 2;
	} else if (header.drmType != 1) {
		return (int)0x808a040b;
	} else if (header.keyIndex > 1) {
		cipherMode = 1;
		macMode = 3;
	} else {
		macMode = 1;
		cipherMode = 1;
	}
	result = GcAuthPgdCheckMac((const unsigned char *)&header,
		offsetof(GcAuthPgdHeader, headerMac),
		(const GcAuthDrmKey *)gcAuthPgdHeaderKey, header.headerMac, macMode, 0);
	if (result < 0)
		return result;
	result = GcAuthPgdCheckMac((const unsigned char *)&header,
		offsetof(GcAuthPgdHeader, licenseMac), &licenseKey, header.licenseMac,
		macMode, 0);
	if (result < 0)
		return result;
	result = GcAuthPgdDecrypt((unsigned char *)&header.dataSeed, 48, 0,
		licenseKey.bytes, &header.seed, cipherMode);
	if (result < 0)
		return result;
	limit = header.dataSize < outputSize ? header.dataSize : outputSize;
	if (header.blockSize != GCAUTH_PGD_BLOCK)
		return (int)0x808a040b;
	offset = ((header.dataSize + 15) & ~15u) + sizeof(header);
	blocks = (header.dataSize + GCAUTH_PGD_BLOCK - 1) >> 10;
	blockMac = pgd + offset;
	data = pgd + sizeof(header);
	if (pgdSize < offset + 16 * ((limit + GCAUTH_PGD_BLOCK - 1) >> 10))
		return (int)0x808a040b;
	if (blocks == 0)
		return blocks;
	if (limit == 0)
		return 0;
	done = 0;
	block = 0;
	offset = 0;
	for (;;) {
		if (block == blocks - 1 && (header.dataSize & (GCAUTH_PGD_BLOCK - 1)))
			length = ((header.dataSize & (GCAUTH_PGD_BLOCK - 1)) + 15) & 0x7f0;
		else
			length = GCAUTH_PGD_BLOCK;
		result = GcAuthPgdCheckMac(data + offset, length, &licenseKey, blockMac,
			macMode, header.keyIndex <= 2 ? 0 : offset);
		if (result < 0)
			return result;
		copied = limit - done;
		if (length < copied)
			copied = length;
		aligned = copied & ~15u;
		if (aligned != 0) {
			memcpy(output + done, data + offset, aligned);
			result = GcAuthPgdDecrypt(output + done, aligned, offset >> 4,
				licenseKey.bytes, &header.dataSeed, cipherMode);
			if (result < 0)
				return result;
		}
		if (aligned < copied) {
			tail = *(const GcAuthDrmKey *)(data + offset + aligned);
			result = GcAuthPgdDecrypt(tail.bytes, 16, (offset + aligned) >> 4,
				licenseKey.bytes, &header.dataSeed, cipherMode);
			if (result < 0)
				return result;
			memcpy(output + done + aligned, &tail, copied - aligned);
			GcAuthDrmKeyClear(&tail);
		}
		block++;
		done += copied;
		if (block == blocks)
			break;
		offset = block << 10;
		blockMac += 16;
		if (offset >= limit)
			break;
	}
	return done;
}
