/* Descriptive source name: the original module has no debug records. */
#include <stddef.h>
#include <string.h>
#include <threadmgr/threadmgr.h>
#include "drm_bb.h"

SceUID gcAuthDrmBbSemaphore;
unsigned char gcAuthDrmWorkBuffer[2048];

const unsigned char gcAuthDrmMacMask[16] = {
	0xe3, 0x50, 0xed, 0x1d, 0x91, 0x0a, 0x1f, 0xd0,
	0x29, 0xbb, 0x1c, 0x3e, 0xf3, 0x40, 0x77, 0xfb
};

const unsigned char gcAuthDrmDecryptOutputMask[16] = {
	0x13, 0x5f, 0xa4, 0x7c, 0xab, 0x39, 0x5b, 0xa4,
	0x76, 0xb8, 0xcc, 0xa9, 0x8f, 0x3a, 0x04, 0x45
};

const unsigned char gcAuthDrmDecryptInputMask[16] = {
	0x67, 0x8d, 0x7f, 0xa3, 0x2a, 0x9c, 0xa0, 0xd1,
	0x50, 0x8a, 0xd8, 0x38, 0x5e, 0x4b, 0x01, 0x7e
};

int GcAuthDrmDecrypt(unsigned char *data, size_t size, int keyId);
int GcAuthDrmDecryptPrivate(unsigned char *data, size_t size);
int GcAuthDrmDecryptCbc(unsigned char *data, size_t size,
			unsigned char *iv, int keyId);
int GcAuthDrmEncryptCbc(unsigned char *data, size_t size,
			unsigned char *iv, int keyId);
int GcAuthDrmEncrypt(unsigned char *data, size_t size, int keyId);
int GcAuthDrmEncryptPrivate(unsigned char *data, size_t size);
int GcAuthGenerateRandom20(unsigned char *data);

int GcAuthDrmDecryptCounterBlocks(unsigned char *work, unsigned char *data,
		size_t size, const unsigned char *key, unsigned int *counter,
		int mode)
{
	unsigned char iv[16];
	unsigned char seed[16];
	unsigned char *cursor;
	unsigned int offset;
	unsigned int previous;
	int index;
	int result;
	int decryptResult;

	index = 0;
	do {
		work[index] = key[index];
		index++;
	} while (index != 16);
	if (mode == 2) {
		for (index = 0; index != 16; index++)
			work[index] ^= gcAuthDrmDecryptInputMask[index];
		result = GcAuthDrmDecryptPrivate(work, 16);
		for (index = 0; index != 16; index++)
			work[index] ^= gcAuthDrmDecryptOutputMask[index];
	} else {
		for (index = 0; index != 16; index++)
			work[index] ^= gcAuthDrmDecryptInputMask[index];
		result = GcAuthDrmDecrypt(work, 16, 57);
		for (index = 0; index != 16; index++)
			work[index] ^= gcAuthDrmDecryptOutputMask[index];
	}
	if (result == 0) {
		for (index = 0; index != 16; index++)
			seed[index] = work[index];
		if (*counter == 1) {
			for (index = 0; index != 16; index++)
				iv[index] = 0;
		} else {
			for (index = 0; index != 12; index++)
				iv[index] = seed[index];
			previous = *counter - 1;
			iv[12] = previous;
			iv[13] = previous >> 8;
			iv[14] = previous >> 16;
			iv[15] = previous >> 24;
		}
		cursor = work;
		offset = 0;
		if (size != 0) {
			do {
				for (index = 0; index != 12; index++)
					cursor[index] = seed[index];
				cursor[12] = *counter;
				cursor[13] = *counter >> 8;
				cursor[14] = *counter >> 16;
				cursor[15] = *counter >> 24;
				++*counter;
				offset += 16;
				cursor += 16;
			} while (size > offset);
			decryptResult = GcAuthDrmDecryptCbc(work, size, iv, 99);
			if (decryptResult != 0) {
				result = decryptResult;
			} else {
				offset = 0;
				do {
					data[offset] ^= work[offset];
					offset++;
				} while (offset != size);
			}
		} else {
			result = GcAuthDrmDecryptCbc(work, size, iv, 99);
		}
	}
	return result;
}

int GcAuthDrmDecryptCounterWords(unsigned char *work, unsigned int *data,
		size_t size, const unsigned char *key, unsigned int *counter,
		int mode)
{
	unsigned int seed[4];
	unsigned char iv[16];
	unsigned char *seedBytes = (unsigned char *)seed;
	unsigned int *cursor;
	unsigned int *words;
	unsigned int offset;
	unsigned int wordCount;
	unsigned int previous;
	int index;
	int result;

	for (index = 0; index != 16; index++)
		work[index] = key[index];
	if (mode == 2) {
		for (index = 0; index != 16; index++)
			work[index] ^= gcAuthDrmDecryptInputMask[index];
		result = GcAuthDrmDecryptPrivate(work, 16);
		for (index = 0; index != 16; index++)
			work[index] ^= gcAuthDrmDecryptOutputMask[index];
	} else {
		for (index = 0; index != 16; index++)
			work[index] ^= gcAuthDrmDecryptInputMask[index];
		result = GcAuthDrmDecrypt(work, 16, 57);
		for (index = 0; index != 16; index++)
			work[index] ^= gcAuthDrmDecryptOutputMask[index];
	}
	if (result == 0) {
		for (index = 0; index != 16; index++)
			seedBytes[index] = work[index];
		if (*counter == 1) {
			for (index = 0; index != 16; index++)
				iv[index] = 0;
		} else {
			for (index = 0; index != 12; index++)
				iv[index] = seedBytes[index];
			previous = *counter - 1;
			iv[12] = previous;
			iv[13] = previous >> 8;
			iv[14] = previous >> 16;
			iv[15] = previous >> 24;
		}
		if (size != 0) {
			cursor = (unsigned int *)work;
			offset = 0;
			do {
				cursor[0] = seed[0];
				cursor[1] = seed[1];
				cursor[2] = seed[2];
				((unsigned char *)cursor)[12] = *counter;
				((unsigned char *)cursor)[13] = *counter >> 8;
				((unsigned char *)cursor)[14] = *counter >> 16;
				((unsigned char *)cursor)[15] = *counter >> 24;
				++*counter;
				offset += 16;
				cursor += 4;
			} while (size > offset);
		}
		result = GcAuthDrmDecryptCbc(work, size, iv, 99);
		if (result == 0) {
			wordCount = size >> 2;
			if (wordCount != 0) {
				words = (unsigned int *)work;
				offset = 0;
				do {
					data[0] ^= words[0];
					data[1] ^= words[1];
					data[2] ^= words[2];
					data[3] ^= words[3];
					data += 4;
					words += 4;
					offset += 4;
				} while (offset < wordCount);
			}
		}
	}
	return result;
}

int GcAuthInitializeDrmBb(void)
{
	SceUID semaphore;

	semaphore = sceKernelCreateSema("SceDrmBB", 0, 1, 1, NULL);
	gcAuthDrmBbSemaphore = semaphore;
	return semaphore <= 0;
}

int SceSblGcAuthMgrDrmBBForDriver_48D7784E(GcAuthDrmBbMacContext *context,
				      unsigned int mode)
{
	int index;

	if (context == NULL)
		return (int)0x808a040a;
	context->mode = mode;
	for (index = 0; index != 16; index++)
		context->chainingValue[index] = 0;
	for (index = 0; index != 16; index++)
		context->tail[index] = 0;
	context->tailSize = 0;
	return 0;
}

int SceSblGcAuthMgrDrmBBForDriver_500F5157(GcAuthDrmBbMacContext *context,
				      const unsigned char *data, size_t size)
{
	const unsigned char *cursor;
	const unsigned char *end;
	unsigned int used;
	unsigned int tailSize;
	unsigned int prefixSize;
	unsigned int index;
	int keyId;
	int result;

	if (sceKernelWaitSema(gcAuthDrmBbSemaphore, 1, NULL) != 0)
		return (int)0x808a0008;
	if (data == NULL || context == NULL) {
		result = (int)0x808a040a;
		goto out;
	}
	used = context->tailSize;
	if (used > 16) {
		result = (int)0x808a0407;
		goto out;
	}
	if (size <= 16 && size + used <= 16) {
		for (index = 0; index != size; index++)
			context->tail[used + index] = data[index];
		context->tailSize = size + used;
		result = 0;
		goto out;
	}
	keyId = context->mode == 2 ? 58 : 56;
	if (used != 0) {
		for (index = 0; index < used; index++) {
			gcAuthDrmWorkBuffer[index] = context->tail[index];
			used = context->tailSize;
		}
	}
	tailSize = (used + size) & 15;
	if (tailSize == 0)
		tailSize = 16;
	prefixSize = size - tailSize;
	context->tailSize = tailSize;
	end = data + prefixSize;
	index = 0;
	do {
		context->tail[index] = end[index];
		index++;
	} while (tailSize > index);
	if (prefixSize != 0) {
		cursor = data;
		do {
			if (used == 2048) {
				result = GcAuthDrmEncryptCbc(gcAuthDrmWorkBuffer,
					used, context->chainingValue, keyId);
				if (result != 0)
					goto out;
				used = 0;
			}
			gcAuthDrmWorkBuffer[used++] = *cursor++;
		} while (cursor != end);
	}
	result = 0;
	if (used != 0)
		result = GcAuthDrmEncryptCbc(gcAuthDrmWorkBuffer, used,
					context->chainingValue, keyId);
out:
	if (sceKernelSignalSema(gcAuthDrmBbSemaphore, 1) != 0)
		return (int)0x808a0008;
	return result;
}


int SceSblGcAuthMgrDrmBBForDriver_6EF3C9DB(GcAuthDrmBbMacContext *context,
				      unsigned char *mac,
				      const unsigned char *key)
{
	unsigned char subkey[16];
	unsigned char value[16];
	unsigned int index;
	unsigned int tailSize;
	unsigned char carry;
	int keyId;
	int result;

	if (sceKernelWaitSema(gcAuthDrmBbSemaphore, 1, NULL) != 0)
		return (int)0x808a0008;
	if (mac == NULL || context == NULL) {
		result = (int)0x808a040a;
		goto out;
	}
	if (context->tailSize > 16) {
		result = (int)0x808a0407;
		goto out;
	}
	keyId = context->mode == 2 ? 58 : 56;
	for (index = 0; index != 16; index++)
		gcAuthDrmWorkBuffer[index] = 0;
	result = GcAuthDrmEncrypt(gcAuthDrmWorkBuffer, 16, keyId);
	if (result != 0)
		goto out;
	for (index = 0; index != 16; index++)
		subkey[index] = gcAuthDrmWorkBuffer[index];
	carry = (signed char)subkey[0] < 0 ? 0x87 : 0;
	for (index = 0; index != 15; index++)
		subkey[index] = (subkey[index] * 2) | (subkey[index + 1] >> 7);
	subkey[15] = (subkey[15] * 2) ^ carry;
	tailSize = context->tailSize;
	if (tailSize != 16) {
		carry = (signed char)subkey[0] < 0 ? 0x87 : 0;
		for (index = 0; index != 15; index++)
			subkey[index] = (subkey[index] * 2) | (subkey[index + 1] >> 7);
		subkey[15] = (subkey[15] * 2) ^ carry;
		context->tail[tailSize] = 0x80;
		for (index = tailSize + 1; index < 16; index++)
			context->tail[index] = 0;
	}
	for (index = 0; index != 16; index++)
		context->tail[index] ^= subkey[index];
	for (index = 0; index != 16; index++)
		gcAuthDrmWorkBuffer[index] = context->tail[index];
	for (index = 0; index != 16; index++)
		value[index] = context->chainingValue[index];
	result = GcAuthDrmEncryptCbc(gcAuthDrmWorkBuffer, 16, value, keyId);
	if (result != 0)
		goto out;
	for (index = 0; index != 16; index++)
		value[index] ^= gcAuthDrmMacMask[index];
	if (context->mode == 2) {
		for (index = 0; index != 16; index++)
			gcAuthDrmWorkBuffer[index] = value[index];
		result = GcAuthDrmEncryptPrivate(gcAuthDrmWorkBuffer, 16);
		if (result != 0)
			goto out;
		result = GcAuthDrmEncrypt(gcAuthDrmWorkBuffer, 16, keyId);
		if (result != 0)
			goto out;
		for (index = 0; index != 16; index++)
			value[index] = gcAuthDrmWorkBuffer[index];
	}
	if (key != NULL) {
		for (index = 0; index != 16; index++)
			value[index] ^= key[index];
		for (index = 0; index != 16; index++)
			gcAuthDrmWorkBuffer[index] = value[index];
		result = GcAuthDrmEncrypt(gcAuthDrmWorkBuffer, 16, keyId);
		if (result != 0)
			goto out;
		for (index = 0; index != 16; index++)
			value[index] = gcAuthDrmWorkBuffer[index];
	}
	for (index = 0; index != 16; index++)
		mac[index] = value[index];
	for (index = 0; index != 16; index++)
		context->chainingValue[index] = 0;
	for (index = 0; index != 16; index++)
		context->tail[index] = 0;
	context->mode = 0;
	context->tailSize = 0;
out:
	if (sceKernelSignalSema(gcAuthDrmBbSemaphore, 1) != 0)
		return (int)0x808a0008;
	return result;
}

int SceSblGcAuthMgrDrmBBForDriver_3C25F9FA(GcAuthDrmBbMacContext *context,
				      const unsigned char *mac,
				      const unsigned char *key)
{
	unsigned char calculated[16];
	const unsigned char *first;
	const unsigned char *second;
	unsigned int mode = context->mode;
	int index;
	int result;

	result = SceSblGcAuthMgrDrmBBForDriver_6EF3C9DB(context, calculated, key);
	if (result != 0)
		return result;
	if (sceKernelWaitSema(gcAuthDrmBbSemaphore, 1, NULL) != 0)
		return (int)0x808a0008;
	if (mode == 3) {
		for (index = 0; index != 16; index++)
			gcAuthDrmWorkBuffer[index] = mac[index];
		result = GcAuthDrmDecrypt(gcAuthDrmWorkBuffer, 16, 99);
		if (result != 0)
			goto out;
	} else {
		for (index = 0; index != 16; index++)
			gcAuthDrmWorkBuffer[index] = mac[index];
	}
	first = calculated;
	second = gcAuthDrmWorkBuffer;
	do {
		result |= *first++ ^ *second++;
	} while (first != calculated + 16);
	result = result != 0 ? (int)0x808a0408 : 0;
out:
	if (sceKernelSignalSema(gcAuthDrmBbSemaphore, 1) != 0)
		return (int)0x808a0008;
	return result;
}

int SceSblGcAuthMgrDrmBBForDriver_E950BE32(GcAuthDrmBbMacContext *context,
				      const unsigned char *data, size_t size)
{
	unsigned char *destination;
	unsigned int used;
	unsigned int tailSize;
	unsigned int prefixSize;
	unsigned int total;
	unsigned int copied;
	unsigned int index;
	int keyId;
	int result;

	if (sceKernelWaitSema(gcAuthDrmBbSemaphore, 1, NULL) != 0)
		return (int)0x808a0008;
	if (data == NULL || context == NULL) {
		result = (int)0x808a040a;
		goto out;
	}
	used = context->tailSize;
	if (used > 16) {
		result = (int)0x808a0407;
		goto out;
	}
	if (size <= 16 && size + used <= 16) {
		for (index = 0; index != size; index++)
			context->tail[used + index] = data[index];
		context->tailSize = size + used;
		result = 0;
		goto out;
	}
	keyId = context->mode == 2 ? 58 : 56;
	if (used != 0) {
		for (index = 0; index < used; index++) {
			gcAuthDrmWorkBuffer[index] = context->tail[index];
			used = context->tailSize;
		}
	}
	tailSize = (used + size) & 15;
	if (tailSize == 0)
		tailSize = 16;
	prefixSize = size - tailSize;
	context->tailSize = tailSize;
	index = 0;
	do {
		context->tail[index] = data[prefixSize + index];
		index++;
	} while (tailSize > index);
	total = prefixSize + used;
	if (total <= 2048) {
		destination = gcAuthDrmWorkBuffer + used;
		copied = 0;
	} else {
		copied = 2048 - used;
		memmove(gcAuthDrmWorkBuffer + used, data, copied);
		result = GcAuthDrmEncryptCbc(gcAuthDrmWorkBuffer, 2048,
					context->chainingValue, keyId);
		if (result != 0)
			goto out;
		while (prefixSize > copied + 2048) {
			memmove(gcAuthDrmWorkBuffer, data + copied, 2048);
			copied += 2048;
			result = GcAuthDrmEncryptCbc(gcAuthDrmWorkBuffer, 2048,
					context->chainingValue, keyId);
			if (result != 0)
				goto out;
		}
		destination = gcAuthDrmWorkBuffer;
		total = prefixSize;
	}
	memmove(destination, data + copied, prefixSize - copied);
	result = GcAuthDrmEncryptCbc(gcAuthDrmWorkBuffer, total - copied,
				context->chainingValue, keyId);
out:
	if (sceKernelSignalSema(gcAuthDrmBbSemaphore, 1) != 0)
		return (int)0x808a0008;
	return result;
}

int SceSblGcAuthMgrDrmBBForDriver_7F98E4E2(
		GcAuthDrmBbCipherContext *context, int mode, int generationMode,
		unsigned char *seed, const unsigned char *key,
		unsigned int counter)
{
	int index;
	int result;

	if (sceKernelWaitSema(gcAuthDrmBbSemaphore, 1, NULL) != 0)
		return (int)0x808a0008;
	if (seed == NULL || context == NULL) {
		result = (int)0x808a040a;
		goto out;
	}
	context->mode = mode;
	if (generationMode == 2) {
		context->counter = counter + 1;
		for (index = 0; index != 16; index++)
			context->seed[index] = seed[index];
		if (key != NULL) {
			for (index = 0; index != 16; index++)
				context->seed[index] ^= key[index];
		}
	} else if (generationMode == 1) {
		context->counter = 1;
		result = GcAuthGenerateRandom20(gcAuthDrmWorkBuffer);
		if (result != 0)
			goto out;
		gcAuthDrmWorkBuffer[12] = 0;
		gcAuthDrmWorkBuffer[13] = 0;
		gcAuthDrmWorkBuffer[14] = 0;
		gcAuthDrmWorkBuffer[15] = 0;
		if (context->mode == 2) {
			for (index = 0; index != 16; index++)
				gcAuthDrmWorkBuffer[index] ^=
					gcAuthDrmDecryptOutputMask[index];
			result = GcAuthDrmEncryptPrivate(gcAuthDrmWorkBuffer, 16);
			for (index = 0; index != 16; index++)
				gcAuthDrmWorkBuffer[index] ^=
					gcAuthDrmDecryptInputMask[index];
		} else {
			for (index = 0; index != 16; index++)
				gcAuthDrmWorkBuffer[index] ^=
					gcAuthDrmDecryptOutputMask[index];
			result = GcAuthDrmEncrypt(gcAuthDrmWorkBuffer, 16, 57);
			for (index = 0; index != 16; index++)
				gcAuthDrmWorkBuffer[index] ^=
					gcAuthDrmDecryptInputMask[index];
		}
		if (result != 0)
			goto out;
		for (index = 0; index != 16; index++)
			context->seed[index] = gcAuthDrmWorkBuffer[index];
		for (index = 0; index != 16; index++)
			seed[index] = gcAuthDrmWorkBuffer[index];
		if (key != NULL) {
			for (index = 0; index != 16; index++)
				context->seed[index] ^= key[index];
		}
	}
	result = 0;
out:
	if (sceKernelSignalSema(gcAuthDrmBbSemaphore, 1) != 0)
		return (int)0x808a0008;
	return result;
}

int SceSblGcAuthMgrDrmBBForDriver_C0F37F18(
		GcAuthDrmBbCipherContext *context, unsigned char *data, size_t size)
{
	unsigned int offset;
	int result;

	if (sceKernelWaitSema(gcAuthDrmBbSemaphore, 1, NULL) != 0)
		return (int)0x808a0008;
	if (data == NULL || context == NULL) {
		result = (int)0x808a040a;
		goto out;
	}
	if (size == 0) {
		result = 0;
		goto out;
	}
	offset = size & 15;
	if (offset != 0) {
		result = (int)0x808a0406;
		goto out;
	}
	while (size >= 2048) {
		result = GcAuthDrmDecryptCounterBlocks(gcAuthDrmWorkBuffer,
				data + offset, 2048, context->seed,
				&context->counter, context->mode);
		size -= 2048;
		if (result != 0)
			goto out;
		offset += 2048;
	}
	if (size != 0)
		result = GcAuthDrmDecryptCounterBlocks(gcAuthDrmWorkBuffer,
				data + offset, size, context->seed,
				&context->counter, context->mode);
	else
		result = 0;
out:
	if (sceKernelSignalSema(gcAuthDrmBbSemaphore, 1) != 0)
		return (int)0x808a0008;
	return result;
}

int SceSblGcAuthMgrDrmBBForDriver_6E6D2B89(GcAuthDrmBbCipherContext *context)
{
	int index;

	if (context == NULL)
		return (int)0x808a040a;
	for (index = 0; index != 16; index++)
		context->seed[index] = 0;
	context->mode = 0;
	context->counter = 0;
	return 0;
}

int SceSblGcAuthMgrDrmBBForDriver_B13577E2(
		GcAuthDrmBbCipherContext *context, unsigned int *data, size_t size)
{
	unsigned int offset;
	int result;

	if (sceKernelWaitSema(gcAuthDrmBbSemaphore, 1, NULL) != 0)
		return (int)0x808a0008;
	if (data == NULL || context == NULL) {
		result = (int)0x808a040a;
		goto out;
	}
	if (size == 0) {
		result = 0;
		goto out;
	}
	offset = size & 15;
	if (offset != 0) {
		result = (int)0x808a0406;
		goto out;
	}
	while (size >= 2048) {
		result = GcAuthDrmDecryptCounterWords(gcAuthDrmWorkBuffer,
				(unsigned int *)((unsigned char *)data + offset),
				2048, context->seed, &context->counter,
				context->mode);
		size -= 2048;
		if (result != 0)
			goto out;
		offset += 2048;
	}
	if (size != 0)
		result = GcAuthDrmDecryptCounterWords(gcAuthDrmWorkBuffer,
				(unsigned int *)((unsigned char *)data + offset),
				size, context->seed, &context->counter, context->mode);
	else
		result = 0;
out:
	if (sceKernelSignalSema(gcAuthDrmBbSemaphore, 1) != 0)
		return (int)0x808a0008;
	return result;
}

