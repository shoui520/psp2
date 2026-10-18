/* Descriptive source name: the original module has no debug records. */
#include <stddef.h>
#include <string.h>
#include <psp2kern/types.h>
#include "drm_crypto.h"
#include <sysmem/utils.h>
#include <ss_mgr/ss_mgr.h>

int GcAuthExecuteKirkCommand(void *output, const void *input, int command,
		size_t size, int keyId);

int GcAuthAesCbc(unsigned char *data, size_t size, const void *key,
		const unsigned char *iv, int decrypt)
{
	struct GcAuthAesCbcState state;
	unsigned int zeroIv[4];
	unsigned char block[32];
	unsigned char *end;
	unsigned int blockBytes;
	unsigned int index;
	unsigned char previous;

	size &= ~15u;
	if (iv == NULL) {
		zeroIv[0] = 0;
		zeroIv[1] = 0;
		zeroIv[2] = 0;
		zeroIv[3] = 0;
		iv = (const unsigned char *)zeroIv;
	}
	if (SceKernelUtilsForDriver_F12B6451(&state.aes, 128, 128, key) >= 0) {
		blockBytes = state.aes.encryption.blockWords * 4;
		for (index = 0; index != blockBytes; index++)
			state.chain[index] = iv[index];
	}
	end = data + size;
	if (decrypt == 0) {
		if (data < end) {
			blockBytes = state.aes.encryption.blockWords * 4;
			do {
				for (index = 0; index != blockBytes; index++)
					block[index] = data[index] ^ state.chain[index];
				SceKernelUtilsForDriver_C2A61770(&state.aes, block, data);
				blockBytes = state.aes.encryption.blockWords * 4;
				for (index = 0; index != blockBytes; index++)
					state.chain[index] = data[index];
				data += 16;
			} while (data < end);
		}
	} else {
		while (data < end) {
			SceKernelUtilsForDriver_D8678061(&state.aes, data, block);
			blockBytes = state.aes.encryption.blockWords * 4;
			for (index = 0; index != blockBytes; index++) {
				previous = data[index];
				data[index] = state.chain[index] ^ block[index];
				state.chain[index] = previous;
			}
			data += 16;
		}
	}
	memset(&state, 0, sizeof(state));
	return 0;
}

int GcAuthDrmEncrypt(unsigned char *data, size_t size, int keyId)
{
	if (GcAuthExecuteKirkCommand(data, data, 4, size, keyId) != 0)
		return (int)0x808a0001;
	return 0;
}

int GcAuthDrmDecrypt(unsigned char *data, size_t size, int keyId)
{
	if (GcAuthExecuteKirkCommand(data, data, 7, size, keyId) != 0)
		return (int)0x808a0001;
	return 0;
}

int GcAuthDrmEncryptPrivate(unsigned char *data, size_t size)
{
	if (GcAuthExecuteKirkCommand(data, data, 4, size, 256) != 0)
		return (int)0x808a0001;
	return 0;
}

int GcAuthDrmDecryptPrivate(unsigned char *data, size_t size)
{
	if (GcAuthExecuteKirkCommand(data, data, 7, size, 256) != 0)
		return (int)0x808a0001;
	return 0;
}

int GcAuthDrmEncryptCbc(unsigned char *data, size_t size,
		unsigned char *iv, int keyId)
{
	unsigned char *end;
	int index;

	for (index = 0; index != 16; index++)
		data[index] ^= iv[index];
	if (GcAuthExecuteKirkCommand(data, data, 4, size, keyId) != 0)
		return (int)0x808a0001;
	end = data + size;
	for (index = 0; index != 16; index++)
		iv[index] = end[index - 16];
	return 0;
}

int GcAuthDrmDecryptCbc(unsigned char *data, size_t size,
		unsigned char *iv, int keyId)
{
	unsigned char finalBlock[16];
	int index;

	for (index = 0; index != 16; index++)
		finalBlock[index] = data[size - 16 + index];
	if (GcAuthExecuteKirkCommand(data, data, 7, size, keyId) != 0)
		return (int)0x808a0001;
	for (index = 0; index != 16; index++)
		data[index] ^= iv[index];
	for (index = 0; index != 16; index++)
		iv[index] = finalBlock[index];
	return 0;
}

int GcAuthAesEncryptZeroIv(unsigned char *data, size_t size, const void *key)
{
	struct GcAuthAesCbcState state;
	unsigned char zeroIv[16];
	unsigned char block[32];
	unsigned char *end;
	unsigned int blockBytes;
	unsigned int index;

	for (index = 0; index != 16; index++)
		zeroIv[index] = 0;
	if (SceKernelUtilsForDriver_F12B6451(&state.aes, 128, 128, key) < 0)
		return (int)0x808a0001;
	blockBytes = state.aes.encryption.blockWords * 4;
	for (index = 0; index != blockBytes; index++)
		state.chain[index] = zeroIv[index];
	end = data + size;
	while (data < end) {
		for (index = 0; index != blockBytes; index++)
			block[index] = data[index] ^ state.chain[index];
		SceKernelUtilsForDriver_C2A61770(&state.aes, block, data);
		blockBytes = state.aes.encryption.blockWords * 4;
		for (index = 0; index != blockBytes; index++)
			state.chain[index] = data[index];
		data += 16;
	}
	return 0;
}

int GcAuthAesDecryptZeroIv(unsigned char *data, size_t size, const void *key)
{
	struct GcAuthAesCbcState state;
	unsigned char zeroIv[16];
	unsigned char block[32];
	unsigned char *end;
	unsigned int blockBytes;
	unsigned int index;
	unsigned char previous;

	for (index = 0; index != 16; index++)
		zeroIv[index] = 0;
	if (SceKernelUtilsForDriver_F12B6451(&state.aes, 128, 128, key) < 0)
		return (int)0x808a0001;
	blockBytes = state.aes.encryption.blockWords * 4;
	for (index = 0; index != blockBytes; index++)
		state.chain[index] = zeroIv[index];
	end = data + size;
	while (data < end) {
		SceKernelUtilsForDriver_D8678061(&state.aes, data, block);
		blockBytes = state.aes.encryption.blockWords * 4;
		for (index = 0; index != blockBytes; index++) {
			previous = data[index];
			data[index] = state.chain[index] ^ block[index];
			state.chain[index] = previous;
		}
		data += 16;
	}
	return 0;
}

int GcAuthAesEncryptCbc(unsigned char *data, size_t size,
		unsigned char *iv, const void *key)
{
	unsigned char *end;
	int index;
	int result;

	for (index = 0; index != 16; index++)
		data[index] ^= iv[index];
	result = GcAuthAesEncryptZeroIv(data, size, key);
	if (result == 0) {
		end = data + size;
		for (index = 0; index != 16; index++)
			iv[index] = end[index - 16];
	}
	return result;
}

int GcAuthAesDecryptCbc(unsigned char *data, size_t size,
		unsigned char *iv, const void *key)
{
	unsigned char finalBlock[16];
	unsigned char *end;
	int index;
	int result;

	end = data + size;
	for (index = 0; index != 16; index++)
		finalBlock[index] = end[index - 16];
	result = GcAuthAesDecryptZeroIv(data, size, key);
	if (result == 0) {
		for (index = 0; index != 16; index++)
			data[index] ^= iv[index];
		for (index = 0; index != 16; index++)
			iv[index] = finalBlock[index];
	}
	return result;
}

int GcAuthGenerateRandom20(unsigned char *data)
{
	unsigned int random[16];
	unsigned int *output;
	int result;

	result = SceSblSsMgrForDriver_AC57F4F0(random);
	if (result != 0)
		return (int)0x808a0005;
	output = (unsigned int *)data;
	output[0] = random[0];
	output[1] = random[1];
	output[2] = random[2];
	output[4] = random[4];
	output[3] = random[3];
	return 0;
}
