/* Memory Stick save data: the MAC and cipher of the PSP's savedata BB. */
#include <stddef.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <intrmgr/syscall.h>
#include "drm_crypto.h"
#include "ms_savebb.h"

int GcAuthAesDecryptZeroIv(unsigned char *data, size_t size, const void *key);
int GcAuthAesEncryptCbc(unsigned char *data, size_t size,
		unsigned char *iv, const void *key);
int GcAuthAesDecryptCbc(unsigned char *data, size_t size,
		unsigned char *iv, const void *key);



SceUID gcAuthMsSaveBbMacSemaphore;
SceUID gcAuthMsSaveBbCipherSemaphore;
unsigned char gcAuthMsSaveBbMacBuffer[2048];
unsigned char gcAuthMsSaveBbCipherBuffer[2048];

static const unsigned char gcAuthMsSaveBbSeedKey3[16] = {
	0xc8, 0x71, 0xfd, 0xb3, 0xbc, 0xc5, 0xd2, 0xf2,
	0xe2, 0xd7, 0x72, 0x9d, 0xdf, 0x82, 0x68, 0x82
};

static const unsigned char gcAuthMsSaveBbCipherKey12[16] = {
	0xaf, 0xfe, 0x8e, 0xb1, 0x3d, 0xd1, 0x7e, 0xd8,
	0x0a, 0x61, 0x24, 0x1c, 0x95, 0x92, 0x56, 0xb6
};

static const unsigned char gcAuthMsSaveBbMacMask56[16] = {
	0xcb, 0x15, 0xf4, 0x07, 0xf9, 0x6a, 0x52, 0x3c,
	0x04, 0xb9, 0xb2, 0xee, 0x5c, 0x53, 0xfa, 0x86
};

static const unsigned char gcAuthMsSaveBbSeedKey1[16] = {
	0x99, 0x24, 0x4c, 0xd2, 0x58, 0xf5, 0x1b, 0xcb,
	0xb0, 0x61, 0x9c, 0xa7, 0x38, 0x30, 0x07, 0x5f
};

static const unsigned char gcAuthMsSaveBbMacKey1[16] = {
	0x98, 0x02, 0xc4, 0xe6, 0xec, 0x9e, 0x9e, 0x2f,
	0xfc, 0x63, 0x4c, 0xe4, 0x2f, 0xbb, 0x46, 0x68
};

static const unsigned char gcAuthMsSaveBbMacKey4[16] = {
	0xb5, 0xb1, 0x6e, 0xde, 0x23, 0xa9, 0x7b, 0x0e,
	0xa1, 0x7c, 0xdb, 0xa2, 0xdc, 0xde, 0xc4, 0x6e
};

static const unsigned char gcAuthMsSaveBbMacKey2[16] = {
	0x02, 0x25, 0xd7, 0xba, 0x63, 0xec, 0xb9, 0x4a,
	0x9d, 0x23, 0x76, 0x01, 0xb3, 0xf6, 0xac, 0x17
};

static const unsigned char gcAuthMsSaveBbMacKey3[16] = {
	0x84, 0x85, 0xc8, 0x48, 0x75, 0x08, 0x43, 0xbc,
	0x9b, 0x9a, 0xec, 0xa7, 0x9c, 0x7f, 0x60, 0x18
};

static const unsigned char gcAuthMsSaveBbMacKey6[16] = {
	0x46, 0xf2, 0x5e, 0x8e, 0x4d, 0x2a, 0xa5, 0x40,
	0x73, 0x0b, 0xc4, 0x6e, 0x47, 0xee, 0x6f, 0x0a
};

static const unsigned char gcAuthMsSaveBbSeedKey[16] = {
	0x5d, 0xc7, 0x11, 0x39, 0xd0, 0x19, 0x38, 0xbc,
	0x02, 0x7f, 0xdd, 0xdc, 0xb0, 0x83, 0x7d, 0x9d
};

static const unsigned char gcAuthMsSaveBbCipherKey56[16] = {
	0x03, 0xb3, 0x02, 0xe8, 0x5f, 0xf3, 0x81, 0xb1,
	0x3b, 0x8d, 0xaa, 0x2a, 0x90, 0xff, 0x5e, 0x61
};

static const unsigned char gcAuthMsSaveBbCipherKey34[16] = {
	0x1c, 0x9b, 0xc4, 0x90, 0xe3, 0x06, 0x64, 0x81,
	0xfa, 0x59, 0xfd, 0xb6, 0x00, 0xbb, 0x28, 0x70
};

static const unsigned char gcAuthMsSaveBbMacKey[16] = {
	0x32, 0x29, 0x5b, 0xd5, 0xea, 0xf7, 0xa3, 0x42,
	0x16, 0xc8, 0x8e, 0x48, 0xff, 0x50, 0xd3, 0x71
};

static const unsigned char gcAuthMsSaveBbSeedInputMask56[16] = {
	0x70, 0x44, 0xa3, 0xae, 0xef, 0x5d, 0xa5, 0xf2,
	0x85, 0x7f, 0xf2, 0xd6, 0x94, 0xf5, 0x36, 0x3b
};

static const unsigned char gcAuthMsSaveBbSeedOutputMask56[16] = {
	0xec, 0x6d, 0x29, 0x59, 0x26, 0x35, 0xa5, 0x7f,
	0x97, 0x2a, 0x0d, 0xbc, 0xa3, 0x26, 0x33, 0x00
};

/* the KIRK key slot of a MAC type */
static inline __attribute__((always_inline)) int GcAuthMsSaveBbMacKeyId(int type)
{
	if (type == 6)
		return 17;
	if (type == 4)
		return 13;
	if (type == 2)
		return 5;
	if (type == 1)
		return 3;
	if (type == 3)
		return 12;
	return 16;
}

/* the AES key of a MAC type, for data in kernel memory */
static inline __attribute__((always_inline)) const unsigned char *
GcAuthMsSaveBbMacKeyOf(int type)
{
	if (type == 6)
		return gcAuthMsSaveBbMacKey6;
	if (type == 4)
		return gcAuthMsSaveBbMacKey4;
	if (type == 2)
		return gcAuthMsSaveBbMacKey2;
	if (type == 1)
		return gcAuthMsSaveBbMacKey1;
	if (type == 3)
		return gcAuthMsSaveBbMacKey3;
	return gcAuthMsSaveBbMacKey;
}

static int GcAuthMsSaveBbMacUpdateUser(const GcAuthMsSaveBbMacUpdateRequest *request)
{
	GcAuthMsSaveBbMacContext *context = request->context;
	const unsigned char *data = request->data;
	SceSize size = request->size;
	const unsigned char *end;
	SceSize count;
	SceSize tail;
	SceSize index;
	int keyId;
	int result;

	if (sceKernelWaitSema(gcAuthMsSaveBbMacSemaphore, 1, NULL) != 0)
		return (int)0x808a0004;
	if (context == NULL || data == NULL) {
		result = (int)0x808a0003;
	} else if (context->padSize > 16) {
		result = (int)0x808a0007;
	} else if (size <= 16 && context->padSize + size <= 16) {
		result = sceKernelCopyFromUser(&context->pad[context->padSize],
			data, size);
		if (result >= 0) {
			result = 0;
			context->padSize += size;
		}
	} else {
		keyId = GcAuthMsSaveBbMacKeyId(context->type);
		count = context->padSize;
		for (index = 0; index < count; index++) {
			gcAuthMsSaveBbMacBuffer[index] = context->pad[index];
			count = context->padSize;
		}
		tail = (count + size) & 15;
		if (tail == 0)
			tail = 16;
		context->padSize = tail;
		end = data + size - tail;
		result = sceKernelCopyFromUser(context->pad, end, tail);
		if (result < 0)
			goto out;
		if (size - tail != 0) {
			do {
				if (count == 2048) {
					result = GcAuthDrmEncryptCbc(gcAuthMsSaveBbMacBuffer,
						2048, context->key, keyId);
					if (result != 0)
						goto out;
					count = 0;
				}
				result = sceKernelCopyFromUser(&gcAuthMsSaveBbMacBuffer[count++],
					data++, 1);
				if (result < 0)
					goto out;
			} while (end != data);
		}
		result = count;
		if (count != 0)
			result = GcAuthDrmEncryptCbc(gcAuthMsSaveBbMacBuffer, count,
				context->key, keyId);
	}
out:
	if (sceKernelSignalSema(gcAuthMsSaveBbMacSemaphore, 1) != 0)
		return (int)0x808a0004;
	return result;
}

static int GcAuthMsSaveBbMacUpdate(const GcAuthMsSaveBbMacUpdateRequest *request)
{
	GcAuthMsSaveBbMacContext *context = request->context;
	const unsigned char *data = request->data;
	SceSize size = request->size;
	const unsigned char *end;
	const unsigned char *key;
	SceSize count;
	SceSize tail;
	SceSize index;
	int result;

	if (sceKernelWaitSema(gcAuthMsSaveBbMacSemaphore, 1, NULL) != 0)
		return (int)0x808a0004;
	if (context == NULL || data == NULL) {
		result = (int)0x808a0003;
	} else if (context->padSize > 16) {
		result = (int)0x808a0007;
	} else if (size <= 16 && context->padSize + size <= 16) {
		count = context->padSize;
		for (index = 0; index != size; index++)
			context->pad[count + index] = data[index];
		context->padSize = count + size;
		result = 0;
	} else {
		key = GcAuthMsSaveBbMacKeyOf(context->type);
		count = context->padSize;
		for (index = 0; index < count; index++) {
			gcAuthMsSaveBbMacBuffer[index] = context->pad[index];
			count = context->padSize;
		}
		tail = (count + size) & 15;
		if (tail == 0)
			tail = 16;
		context->padSize = tail;
		end = data + size - tail;
		index = 0;
		do {
			context->pad[index] = end[index];
			index++;
		} while (tail > index);
		if (size - tail != 0) {
			do {
				if (count == 2048) {
					result = GcAuthAesEncryptCbc(gcAuthMsSaveBbMacBuffer,
						2048, context->key, key);
					if (result != 0)
						goto out;
					count = 0;
				}
				gcAuthMsSaveBbMacBuffer[count++] = *data++;
			} while (data != end);
		}
		result = count;
		if (count != 0)
			result = GcAuthAesEncryptCbc(gcAuthMsSaveBbMacBuffer, count,
				context->key, key);
	}
out:
	if (sceKernelSignalSema(gcAuthMsSaveBbMacSemaphore, 1) != 0)
		return (int)0x808a0004;
	return result;
}

static int GcAuthMsSaveBbMacFinal(const GcAuthMsSaveBbMacFinalRequest *request)
{
	GcAuthMsSaveBbMacContext *context = request->context;
	unsigned char *mac = request->mac;
	const unsigned char *key = request->key;
	unsigned char subkey[16];
	unsigned char iv[16];
	unsigned char carry;
	SceSize padSize;
	int index;
	int keyId;
	int type;
	int result;

	if (sceKernelWaitSema(gcAuthMsSaveBbMacSemaphore, 1, NULL) != 0)
		return (int)0x808a0004;
	if (context == NULL || mac == NULL) {
		result = (int)0x808a0003;
		goto out;
	}
	if (context->padSize > 16) {
		result = (int)0x808a0007;
		goto out;
	}
	keyId = GcAuthMsSaveBbMacKeyId(context->type);
	for (index = 0; index != 16; index++)
		gcAuthMsSaveBbMacBuffer[index] = 0;
	result = GcAuthDrmEncrypt(gcAuthMsSaveBbMacBuffer, 16, keyId);
	if (result != 0)
		goto out;
	for (index = 0; index != 16; index++)
		subkey[index] = gcAuthMsSaveBbMacBuffer[index];

	carry = (signed char)subkey[0] < 0 ? 0x87 : 0;
	for (index = 0; index != 15; index++)
		subkey[index] = (subkey[index] << 1) | (subkey[index + 1] >> 7);
	subkey[15] = carry ^ (subkey[15] << 1);
	padSize = context->padSize;
	if (padSize != 16) {
		carry = (signed char)subkey[0] < 0 ? 0x87 : 0;
		for (index = 0; index != 15; index++)
			subkey[index] = (subkey[index] << 1) | (subkey[index + 1] >> 7);
		subkey[15] = carry ^ (subkey[15] << 1);
		context->pad[padSize] = 0x80;
		for (index = padSize + 1; index <= 15; index++)
			context->pad[index] = 0;
	}
	for (index = 0; index != 16; index++)
		context->pad[index] ^= subkey[index];

	for (index = 0; index != 16; index++)
		gcAuthMsSaveBbMacBuffer[index] = context->pad[index];
	for (index = 0; index != 16; index++)
		iv[index] = context->key[index];
	result = GcAuthDrmEncryptCbc(gcAuthMsSaveBbMacBuffer, 16, iv, keyId);
	if (result != 0)
		goto out;

	type = context->type;
	if ((unsigned int)(type - 3) <= 1) {
		for (index = 0; index != 16; index++)
			iv[index] ^= gcAuthMsSaveBbMacMask[index];
	} else if ((unsigned int)(type - 5) <= 1) {
		for (index = 0; index != 16; index++)
			iv[index] ^= gcAuthMsSaveBbMacMask56[index];
	}
	if (type == 4 || (type & ~4) == 2) {
		for (index = 0; index != 16; index++)
			gcAuthMsSaveBbMacBuffer[index] = iv[index];
		result = GcAuthDrmEncryptPrivate(gcAuthMsSaveBbMacBuffer, 16);
		if (result != 0)
			goto out;
		result = GcAuthDrmEncrypt(gcAuthMsSaveBbMacBuffer, 16, keyId);
		if (result != 0)
			goto out;
		for (index = 0; index != 16; index++)
			iv[index] = gcAuthMsSaveBbMacBuffer[index];
	}
	if (key != NULL) {
		for (index = 0; index != 16; index++)
			iv[index] ^= key[index];
		for (index = 0; index != 16; index++)
			gcAuthMsSaveBbMacBuffer[index] = iv[index];
		result = GcAuthDrmEncrypt(gcAuthMsSaveBbMacBuffer, 16, keyId);
		if (result != 0)
			goto out;
		for (index = 0; index != 16; index++)
			iv[index] = gcAuthMsSaveBbMacBuffer[index];
	}
	for (index = 0; index != 16; index++)
		mac[index] = iv[index];
	for (index = 0; index != 16; index++)
		context->key[index] = 0;
	for (index = 0; index != 16; index++)
		context->pad[index] = 0;
	context->type = 0;
	context->padSize = 0;
out:
	if (sceKernelSignalSema(gcAuthMsSaveBbMacSemaphore, 1) != 0)
		result = (int)0x808a0004;
	return result;
}

static inline __attribute__((always_inline)) void
GcAuthMsSaveBbXorSeed(const unsigned char *mask)
{
	int index;

	for (index = 0; index != 16; index++)
		gcAuthMsSaveBbCipherBuffer[index] ^= mask[index];
}

static int GcAuthMsSaveBbCipherInit(const GcAuthMsSaveBbCipherInitRequest *request)
{
	GcAuthMsSaveBbCipherContext *context = request->context;
	int type = request->type;
	int mode = request->mode;
	unsigned char *key = request->key;
	const unsigned char *seed = request->seed;
	int index;
	int result;

	if (sceKernelWaitSema(gcAuthMsSaveBbCipherSemaphore, 1, NULL) != 0)
		return (int)0x808a0004;
	if (context == NULL || key == NULL) {
		result = (int)0x808a0003;
		goto out;
	}
	context->type = type;
	context->counter = 1;
	if (mode == 2) {
		for (index = 0; index != 16; index++)
			context->key[index] = key[index];
		if (seed != NULL) {
			for (index = 0; index != 16; index++)
				context->key[index] ^= seed[index];
		}
	} else if (mode == 1) {
		result = GcAuthGenerateRandom20(gcAuthMsSaveBbCipherBuffer);
		if (result != 0)
			goto out;
		gcAuthMsSaveBbCipherBuffer[12] = 0;
		gcAuthMsSaveBbCipherBuffer[13] = 0;
		gcAuthMsSaveBbCipherBuffer[14] = 0;
		gcAuthMsSaveBbCipherBuffer[15] = 0;
		type = context->type;
		if (type == 6) {
			GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedInputMask56);
			result = GcAuthDrmEncryptPrivate(gcAuthMsSaveBbCipherBuffer, 16);
			GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedOutputMask56);
		} else if (type == 4) {
			GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedInputMask);
			result = GcAuthDrmEncryptPrivate(gcAuthMsSaveBbCipherBuffer, 16);
			GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedOutputMask);
		} else if (type == 2) {
			result = GcAuthDrmEncryptPrivate(gcAuthMsSaveBbCipherBuffer, 16);
		} else if (type == 1) {
			result = GcAuthDrmEncrypt(gcAuthMsSaveBbCipherBuffer, 16, 4);
		} else if (type == 3) {
			GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedInputMask);
			result = GcAuthDrmEncrypt(gcAuthMsSaveBbCipherBuffer, 16, 14);
			GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedOutputMask);
		} else {
			GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedInputMask56);
			result = GcAuthDrmEncrypt(gcAuthMsSaveBbCipherBuffer, 16, 18);
			GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedOutputMask56);
		}
		if (result != 0)
			goto out;
		for (index = 0; index != 16; index++)
			context->key[index] = gcAuthMsSaveBbCipherBuffer[index];
		for (index = 0; index != 16; index++)
			key[index] = gcAuthMsSaveBbCipherBuffer[index];
		if (seed != NULL) {
			for (index = 0; index != 16; index++)
				context->key[index] ^= seed[index];
		}
	}
	result = 0;
out:
	if (sceKernelSignalSema(gcAuthMsSaveBbCipherSemaphore, 1) != 0)
		return (int)0x808a0004;
	return result;
}

/* the counter blocks of a cipher run: the seed's first 12 bytes, then the counter */
static inline __attribute__((always_inline)) void
GcAuthMsSaveBbFillCounterBlocks(const unsigned char *seed, SceSize size,
		unsigned int *counter)
{
	unsigned char *block;
	SceSize offset;
	int index;

	block = gcAuthMsSaveBbCipherBuffer;
	offset = 0;
	do {
		for (index = 0; index != 12; index++)
			block[index] = seed[index];
		offset += 16;
		block += 16;
		block[-4] = *counter;
		block[-3] = *counter >> 8;
		block[-2] = *counter >> 16;
		block[-1] = *counter >> 24;
		(*counter)++;
	} while (offset < size);
}

/* the CBC IV of a cipher run: zero for the first, else the previous counter block */
static inline __attribute__((always_inline)) void
GcAuthMsSaveBbFirstIv(unsigned char *iv, const unsigned char *seed,
		const unsigned int *counter)
{
	unsigned int previous;
	int index;

	previous = *counter;
	if (previous == 1) {
		for (index = 0; index != 16; index++)
			iv[index] = 0;
	} else {
		for (index = 0; index != 12; index++)
			iv[index] = seed[index];
		previous--;
		iv[12] = previous;
		iv[13] = previous >> 8;
		iv[14] = previous >> 16;
		iv[15] = previous >> 24;
	}
}

static int GcAuthMsSaveBbCipherBlocks(unsigned char *data, SceSize size,
		const unsigned char *key, unsigned int *counter, int type)
{
	const unsigned char *cbcKey;
	unsigned char iv[16];
	unsigned char seed[16];
	SceSize index;
	int result;

	for (index = 0; index != 16; index++)
		gcAuthMsSaveBbCipherBuffer[index] = key[index];
	if (type == 6) {
		GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedOutputMask56);
		result = GcAuthDrmDecryptPrivate(gcAuthMsSaveBbCipherBuffer, 16);
		GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedInputMask56);
		cbcKey = gcAuthMsSaveBbCipherKey56;
	} else if (type == 4) {
		GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedOutputMask);
		result = GcAuthDrmDecryptPrivate(gcAuthMsSaveBbCipherBuffer, 16);
		GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedInputMask);
		cbcKey = gcAuthMsSaveBbCipherKey34;
	} else if (type == 2) {
		result = GcAuthDrmDecryptPrivate(gcAuthMsSaveBbCipherBuffer, 16);
		cbcKey = gcAuthMsSaveBbCipherKey12;
	} else if (type == 1) {
		result = GcAuthAesDecryptZeroIv(gcAuthMsSaveBbCipherBuffer, 16,
			gcAuthMsSaveBbSeedKey1);
		cbcKey = gcAuthMsSaveBbCipherKey12;
	} else if (type == 3) {
		GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedOutputMask);
		result = GcAuthAesDecryptZeroIv(gcAuthMsSaveBbCipherBuffer, 16,
			gcAuthMsSaveBbSeedKey3);
		GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedInputMask);
		cbcKey = gcAuthMsSaveBbCipherKey34;
	} else {
		GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedOutputMask56);
		result = GcAuthAesDecryptZeroIv(gcAuthMsSaveBbCipherBuffer, 16,
			gcAuthMsSaveBbSeedKey);
		GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedInputMask56);
		cbcKey = gcAuthMsSaveBbCipherKey56;
	}
	if (result != 0)
		return result;
	for (index = 0; index != 16; index++)
		seed[index] = gcAuthMsSaveBbCipherBuffer[index];
	GcAuthMsSaveBbFirstIv(iv, seed, counter);
	if (size == 0)
		return GcAuthAesDecryptCbc(gcAuthMsSaveBbCipherBuffer, 0, iv, cbcKey);
	GcAuthMsSaveBbFillCounterBlocks(seed, size, counter);
	result = GcAuthAesDecryptCbc(gcAuthMsSaveBbCipherBuffer, size, iv, cbcKey);
	if (result != 0)
		return result;
	index = 0;
	do {
		data[index] ^= gcAuthMsSaveBbCipherBuffer[index];
		index++;
	} while (index != size);
	return 0;
}

static int GcAuthMsSaveBbCipherUpdate(const GcAuthMsSaveBbCipherUpdateRequest *request)
{
	GcAuthMsSaveBbCipherContext *context = request->context;
	unsigned char *data = request->data;
	SceSize size = request->size;
	SceSize offset;
	int result;

	if (sceKernelWaitSema(gcAuthMsSaveBbCipherSemaphore, 1, NULL) != 0)
		return (int)0x808a0004;
	if (context == NULL || data == NULL) {
		result = (int)0x808a0003;
	} else if (size == 0) {
		result = 0;
	} else if ((offset = size & 15) != 0) {
		result = (int)0x808a0006;
	} else {
		result = 0;
		while (size >= 0x800) {
			result = GcAuthMsSaveBbCipherBlocks(data + offset, 0x800,
				context->key, &context->counter, context->type);
			offset += 0x800;
			size -= 0x800;
			if (result != 0)
				goto out;
		}
		if (size != 0)
			result = GcAuthMsSaveBbCipherBlocks(data + offset, size,
				context->key, &context->counter, context->type);
	}
out:
	if (sceKernelSignalSema(gcAuthMsSaveBbCipherSemaphore, 1) != 0)
		return (int)0x808a0004;
	return result;
}

static int GcAuthMsSaveBbCipherBlocksUser(unsigned char *data, SceSize size,
		const unsigned char *key, unsigned int *counter, int type)
{
	unsigned char byte;
	unsigned char iv[16];
	unsigned char seed[16];
	SceSize index;
	int keyId;
	int result;

	for (index = 0; index != 16; index++)
		gcAuthMsSaveBbCipherBuffer[index] = key[index];
	if (type == 6) {
		GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedOutputMask56);
		result = GcAuthDrmDecryptPrivate(gcAuthMsSaveBbCipherBuffer, 16);
		GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedInputMask56);
		keyId = 100;
	} else if (type == 4) {
		GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedOutputMask);
		result = GcAuthDrmDecryptPrivate(gcAuthMsSaveBbCipherBuffer, 16);
		GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedInputMask);
		keyId = 87;
	} else if (type == 2) {
		result = GcAuthDrmDecryptPrivate(gcAuthMsSaveBbCipherBuffer, 16);
		keyId = 83;
	} else if (type == 1) {
		result = GcAuthDrmDecrypt(gcAuthMsSaveBbCipherBuffer, 16, 4);
		keyId = 83;
	} else if (type == 3) {
		GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedOutputMask);
		result = GcAuthDrmDecrypt(gcAuthMsSaveBbCipherBuffer, 16, 14);
		GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedInputMask);
		keyId = 87;
	} else {
		GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedOutputMask56);
		result = GcAuthDrmDecrypt(gcAuthMsSaveBbCipherBuffer, 16, 18);
		GcAuthMsSaveBbXorSeed(gcAuthMsSaveBbSeedInputMask56);
		keyId = 100;
	}
	if (result != 0)
		return result;
	for (index = 0; index != 16; index++)
		seed[index] = gcAuthMsSaveBbCipherBuffer[index];
	GcAuthMsSaveBbFirstIv(iv, seed, counter);
	if (size == 0)
		return GcAuthDrmDecryptCbc(gcAuthMsSaveBbCipherBuffer, 0, iv, keyId);
	GcAuthMsSaveBbFillCounterBlocks(seed, size, counter);
	result = GcAuthDrmDecryptCbc(gcAuthMsSaveBbCipherBuffer, size, iv, keyId);
	if (result != 0)
		return result;
	index = 0;
	do {
		result = sceKernelCopyFromUser(&byte, data + index, 1);
		if (result < 0)
			return result;
		byte ^= gcAuthMsSaveBbCipherBuffer[index];
		result = sceKernelCopyToUser(data + index, &byte, 1);
		if (result < 0)
			return result;
	} while (++index != size);
	return 0;
}

static int GcAuthMsSaveBbCipherUpdateUser(const GcAuthMsSaveBbCipherUpdateRequest *request)
{
	GcAuthMsSaveBbCipherContext *context = request->context;
	unsigned char *data = request->data;
	SceSize size = request->size;
	SceSize offset;
	int result;

	if (sceKernelWaitSema(gcAuthMsSaveBbCipherSemaphore, 1, NULL) != 0)
		return (int)0x808a0004;
	if (context == NULL || data == NULL) {
		result = (int)0x808a0003;
	} else if (size == 0) {
		result = 0;
	} else if ((offset = size & 15) != 0) {
		result = (int)0x808a0006;
	} else {
		result = 0;
		while (size >= 0x800) {
			result = GcAuthMsSaveBbCipherBlocksUser(data + offset, 0x800,
				context->key, &context->counter, context->type);
			offset += 0x800;
			size -= 0x800;
			if (result != 0)
				goto out;
		}
		if (size != 0)
			result = GcAuthMsSaveBbCipherBlocksUser(data + offset, size,
				context->key, &context->counter, context->type);
	}
out:
	if (sceKernelSignalSema(gcAuthMsSaveBbCipherSemaphore, 1) != 0)
		return (int)0x808a0004;
	return result;
}

int GcAuthInitializeMsSaveBb(void)
{
	gcAuthMsSaveBbMacSemaphore = sceKernelCreateSema("SceMSSaveBB1", 0, 1, 1, NULL);
	if (gcAuthMsSaveBbMacSemaphore <= 0)
		return 1;
	gcAuthMsSaveBbCipherSemaphore = sceKernelCreateSema("SceMSSaveBB2", 0, 1, 1,
		NULL);
	if (gcAuthMsSaveBbCipherSemaphore > 0)
		return 0;
	sceKernelDeleteSema(gcAuthMsSaveBbMacSemaphore);
	return 1;
}

static inline __attribute__((always_inline)) int
GcAuthMsSaveBbMacInit(GcAuthMsSaveBbMacContext *context, int type)
{
	int index;

	if (context == NULL)
		return (int)0x808a0003;
	context->type = type;
	for (index = 0; index != 16; index++)
		context->key[index] = 0;
	for (index = 0; index != 16; index++)
		context->pad[index] = 0;
	context->padSize = 0;
	return 0;
}

int GcAuthMsSaveBbMacInitForUser(GcAuthMsSaveBbMacContext *context, int type)
{
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	result = GcAuthMsSaveBbMacInit(context, type);
	sceSyscallLeave(state);
	return result;
}

int SceSblGcAuthMgrMsSaveBBForDriver_4A249828(GcAuthMsSaveBbMacContext *context,
		int type)
{
	return GcAuthMsSaveBbMacInit(context, type);
}

int GcAuthMsSaveBbMacUpdateForUser(GcAuthMsSaveBbMacContext *context,
		const unsigned char *data, SceSize size)
{
	GcAuthMsSaveBbMacUpdateRequest request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.data = data;
	request.size = size;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthMsSaveBbMacUpdateUser, &request);
	sceSyscallLeave(state);
	return result;
}

int SceSblGcAuthMgrMsSaveBBForDriver_79A9BE44(GcAuthMsSaveBbMacContext *context,
		const unsigned char *data, SceSize size)
{
	GcAuthMsSaveBbMacUpdateRequest request;

	request.context = context;
	request.data = data;
	request.size = size;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthMsSaveBbMacUpdate, &request);
}

int GcAuthMsSaveBbMacFinalForUser(GcAuthMsSaveBbMacContext *context,
		unsigned char *mac, const unsigned char *key)
{
	GcAuthMsSaveBbMacFinalRequest request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.mac = mac;
	request.key = key;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthMsSaveBbMacFinal, &request);
	sceSyscallLeave(state);
	return result;
}

int SceSblGcAuthMgrMsSaveBBForDriver_8E6626A0(GcAuthMsSaveBbMacContext *context,
		unsigned char *mac, const unsigned char *key)
{
	GcAuthMsSaveBbMacFinalRequest request;

	request.context = context;
	request.mac = mac;
	request.key = key;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthMsSaveBbMacFinal, &request);
}

int GcAuthMsSaveBbCipherInitForUser(GcAuthMsSaveBbCipherContext *context,
		int type, int mode, unsigned char *key, const unsigned char *seed)
{
	GcAuthMsSaveBbCipherInitRequest request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.type = type;
	request.mode = mode;
	request.key = key;
	request.seed = seed;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthMsSaveBbCipherInit, &request);
	sceSyscallLeave(state);
	return result;
}

int SceSblGcAuthMgrMsSaveBBForDriver_3C185A67(GcAuthMsSaveBbCipherContext *context,
		int type, int mode, unsigned char *key, const unsigned char *seed)
{
	GcAuthMsSaveBbCipherInitRequest request;

	request.context = context;
	request.type = type;
	request.mode = mode;
	request.key = key;
	request.seed = seed;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthMsSaveBbCipherInit, &request);
}

int GcAuthMsSaveBbCipherUpdateForUser(GcAuthMsSaveBbCipherContext *context,
		unsigned char *data, SceSize size)
{
	GcAuthMsSaveBbCipherUpdateRequest request;
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	request.context = context;
	request.data = data;
	request.size = size;
	result = SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthMsSaveBbCipherUpdateUser, &request);
	sceSyscallLeave(state);
	return result;
}

int SceSblGcAuthMgrMsSaveBBForDriver_F66D5F76(GcAuthMsSaveBbCipherContext *context,
		unsigned char *data, SceSize size)
{
	GcAuthMsSaveBbCipherUpdateRequest request;

	request.context = context;
	request.data = data;
	request.size = size;
	return SceThreadmgrForDriver_E54FD746(8192,
		(int (*)(void *))GcAuthMsSaveBbCipherUpdate, &request);
}

static inline __attribute__((always_inline)) int
GcAuthMsSaveBbCipherFinal(GcAuthMsSaveBbCipherContext *context)
{
	int index;

	if (context == NULL)
		return (int)0x808a0003;
	for (index = 0; index != 16; index++)
		context->key[index] = 0;
	context->type = 0;
	context->counter = 0;
	return 0;
}

int GcAuthMsSaveBbCipherFinalForUser(GcAuthMsSaveBbCipherContext *context)
{
	unsigned int state;
	int result;

	state = sceSyscallEnter();
	result = GcAuthMsSaveBbCipherFinal(context);
	sceSyscallLeave(state);
	return result;
}

int SceSblGcAuthMgrMsSaveBBForDriver_805C860B(GcAuthMsSaveBbCipherContext *context)
{
	return GcAuthMsSaveBbCipherFinal(context);
}
