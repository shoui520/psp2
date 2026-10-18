/* Descriptive source name: the original library has no debug records. */
#include "cipher.h"

typedef int (*MarlinBlockTransform)(SceAesFullContext *key, const void *data,
				    void *output);

MarlinBlockTransform marlinBlockEncryptors[2] = { 0, MarlinEncryptBlock };
MarlinBlockTransform marlinBlockDecryptors[2] = { 0, MarlinDecryptBlock };

typedef int (*MarlinEncryptMode)(const MarlinCipherContext *context,
				 SceAesFullContext *key, const unsigned char *data,
				 unsigned int size, unsigned char *encrypted,
				 unsigned int *encryptedSize,
				 const unsigned char *paddingBlock,
				 unsigned int paddingSize);
typedef int (*MarlinDecryptMode)(const MarlinCipherContext *context,
				 SceAesFullContext *key, const unsigned char *encrypted,
				 unsigned int size, unsigned char *data,
				 unsigned int *dataSize);

MarlinEncryptMode marlinCipherEncryptors[4] = {
	0, MarlinCipherEncryptEcb, MarlinCipherEncryptCbc, 0
};
MarlinDecryptMode marlinCipherDecryptors[4] = {
	0, MarlinCipherDecryptEcb, MarlinCipherDecryptCbc, 0
};

int MarlinCipherValidate(const MarlinCipherContext *context)
{
	int result = 0;

	switch (context->algorithm) {
	case 1:
		break;
	default:
		result = 0x22030001;
		goto out;
	}
	switch (context->blockMode) {
	case 0x20000:
		break;
	case 0x30000:
		break;
	case 0x10000:
		break;
	default:
		result = 0x22030001;
		goto out;
	}
	switch (context->paddingMode) {
	case 0x2000000:
		break;
	case 0x1000000:
		break;
	case 0x3000000:
		break;
	case 0x4000000:
		break;
	default:
		result = 0x22030001;
		goto out;
	}
out:
	return result;
}


int MarlinCipherGetPaddingSize(const MarlinCipherContext *context,
			      const unsigned char *data, unsigned int *size)
{
	int result = 0;
	unsigned int padding;
	unsigned int blockSize;

	blockSize = context->blockSize;
	padding = 0;

	switch (context->paddingMode) {
	case 0x1000000:
		padding = 0;
		break;
	case 0x2000000:
		padding = data[blockSize - 1];
		if (padding == 0 || padding > blockSize) {
			result = 3;
			goto out;
		}
		break;
	case 0x3000000:
		for (padding = 0; padding < blockSize; padding++) {
			if (data[blockSize - padding - 1] != 0)
				break;
		}
		if (padding == blockSize) {
			result = 3;
			goto out;
		}
		break;
	case 0x4000000:
		for (padding = 0; padding < blockSize; padding++) {
			if (data[blockSize - padding - 1] != 32)
				break;
		}
		if (padding == blockSize) {
			result = 3;
			goto out;
		}
		break;
	}
	*size = padding;
out:
	return result;
}

int MarlinCipherPreparePadding(const MarlinCipherContext *context,
			       const unsigned char *data, unsigned int size,
			       unsigned char *block, unsigned int *paddingSize)
{
	unsigned int remaining;
	signed char value;
	int result;
	unsigned int blockSize;
	unsigned int blocks;

	result = 0;
	blockSize = context->blockSize;
	blocks = size / blockSize;
	remaining = size - blockSize * blocks;
	value = 0;
	if (remaining != 0)
		MarlinCopyMemory(block, data + blockSize * blocks, remaining);
	switch (context->paddingMode) {
	case 0x1000000:
		remaining = 0;
		*paddingSize = 0;
		break;
	case 0x2000000:
		*paddingSize = blockSize - remaining;
		value = *paddingSize;
		break;
	case 0x3000000:
		*paddingSize = remaining != 0 ? blockSize - remaining : 0;
		value = 0;
		break;
	case 0x4000000:
		*paddingSize = remaining != 0 ? blockSize - remaining : 0;
		value = 32;
		break;
	}
	if (*paddingSize != 0)
		MarlinFillMemory(block + remaining, value, *paddingSize);
	return result;
}

int MarlinCipherEncryptEcb(const MarlinCipherContext *context,
			   SceAesFullContext *key, const unsigned char *data,
			   unsigned int size, unsigned char *encrypted,
			   unsigned int *encryptedSize,
			   const unsigned char *paddingBlock,
			   unsigned int paddingSize)
{
	int result = 0;
	const unsigned char *input;
	unsigned char *output;
	unsigned int index;
	unsigned int blockSize;
	unsigned int blocks;
	unsigned int remaining;

	blockSize = context->blockSize;
	blocks = size / blockSize;
	remaining = size - blocks * blockSize;
	input = data;
	output = encrypted;
	if (*encryptedSize < blocks * blockSize + remaining + paddingSize) {
		result = 3;
		goto out;
	}
	for (index = 0; index < blocks; index++) {
		result = marlinBlockEncryptors[context->algorithm](key, input,
								output);
		if (result != 0) {
			result = 0x22030003;
			goto out;
		}
		input += blockSize;
		output += blockSize;
	}
	if (paddingSize != 0) {
		result = marlinBlockEncryptors[context->algorithm](key,
							paddingBlock, output);
		if (result != 0) {
			result = 0x22030003;
			goto out;
		}
	}
	*encryptedSize = blocks * blockSize + remaining + paddingSize;
out:
	return result;
}

int MarlinCipherDecryptEcb(const MarlinCipherContext *context,
			   SceAesFullContext *key, const unsigned char *encrypted,
			   unsigned int size, unsigned char *data,
			   unsigned int *dataSize)
{
	int result;
	const unsigned char *input;
	unsigned char *output;
	unsigned int index;
	unsigned int blockSize;
	unsigned int blocks;
	unsigned int finalSize;
	unsigned char finalBlock[16];
	unsigned int paddingSize;

	result = 0;
	blockSize = context->blockSize;
	blocks = size / blockSize;
	paddingSize = 0;
	finalSize = 0;
	input = encrypted;
	output = data;
	if (size % blockSize != 0) {
		result = 3;
		goto out;
	}
	if (*dataSize < (blocks - 1) * blockSize) {
		result = 3;
		goto out;
	}
	for (index = 0; index < blocks - 1; index++) {
		result = marlinBlockDecryptors[context->algorithm](key, input,
								output);
		if (result != 0) {
			result = 0x22030004;
			goto out;
		}
		input += blockSize;
		output += blockSize;
	}
	result = marlinBlockDecryptors[context->algorithm](key, input, finalBlock);
	if (result != 0) {
		result = 0x22030004;
		goto out;
	}
	result = MarlinCipherGetPaddingSize(context, finalBlock, &paddingSize);
	if (result != 0) {
		result = 0x22030004;
		goto out;
	}
	finalSize = blockSize - paddingSize;
	if (finalSize != 0) {
		if (*dataSize < (blocks - 1) * blockSize + finalSize) {
			result = 3;
			goto out;
		}
		MarlinCopyMemory(output, finalBlock, finalSize);
	}
	*dataSize = (blocks - 1) * blockSize + finalSize;
out:
	return result;
}

int MarlinCipherEncryptCbc(const MarlinCipherContext *context,
			   SceAesFullContext *key, const unsigned char *data,
			   unsigned int size, unsigned char *encrypted,
			   unsigned int *encryptedSize,
			   const unsigned char *paddingBlock,
			   unsigned int paddingSize)
{
	int result = 0;
	const unsigned char *input;
	unsigned char *output;
	unsigned int index;
	unsigned int byte;
	unsigned int blockSize;
	unsigned int blocks;
	unsigned int remaining;
	unsigned char block[16];

	blockSize = context->blockSize;
	blocks = size / blockSize;
	remaining = size - blocks * blockSize;
	input = data;
	output = encrypted;
	if (*encryptedSize < blocks * blockSize + remaining + paddingSize) {
		result = 3;
		goto out;
	}
	MarlinCopyMemory(block, context->iv, blockSize);
	for (index = 0; index < blocks; index++) {
		for (byte = 0; byte < blockSize; byte++)
			block[byte] ^= input[byte];
		result = marlinBlockEncryptors[context->algorithm](key, block,
								output);
		if (result != 0) {
			result = 0x22030003;
			goto out;
		}
		MarlinCopyMemory(block, output, blockSize);
		input += blockSize;
		output += blockSize;
	}
	if (paddingSize != 0) {
		for (byte = 0; byte < blockSize; byte++)
			block[byte] ^= paddingBlock[byte];
		result = marlinBlockEncryptors[context->algorithm](key, block,
								output);
		if (result != 0) {
			result = 0x22030003;
			goto out;
		}
	}
	*encryptedSize = blocks * blockSize + remaining + paddingSize;
out:
	return result;
}

int MarlinCipherDecryptCbc(const MarlinCipherContext *context,
			   SceAesFullContext *key, const unsigned char *encrypted,
			   unsigned int size, unsigned char *data,
			   unsigned int *dataSize)
{
	int result;
	const unsigned char *input;
	unsigned char *output;
	unsigned int index;
	unsigned int byte;
	unsigned int blockSize;
	unsigned int blocks;
	unsigned int finalSize;
	unsigned char finalBlock[16];
	unsigned char chain[16];
	unsigned int paddingSize;

	result = 0;
	blockSize = context->blockSize;
	blocks = size / blockSize;
	paddingSize = 0;
	finalSize = 0;
	input = encrypted;
	output = data;
	if (size % blockSize != 0) {
		result = 3;
		goto out;
	}
	if (*dataSize < (blocks - 1) * blockSize) {
		result = 3;
		goto out;
	}
	MarlinCopyMemory(chain, context->iv, blockSize);
	for (index = 0; index < blocks - 1; index++) {
		result = marlinBlockDecryptors[context->algorithm](key, input,
								output);
		if (result != 0) {
			result = 0x22030004;
			goto out;
		}
		for (byte = 0; byte < blockSize; byte++)
			output[byte] ^= chain[byte];
		MarlinCopyMemory(chain, input, blockSize);
		input += blockSize;
		output += blockSize;
	}
	result = marlinBlockDecryptors[context->algorithm](key, input, finalBlock);
	if (result != 0) {
		result = 0x22030004;
		goto out;
	}
	for (byte = 0; byte < blockSize; byte++)
		finalBlock[byte] ^= chain[byte];
	result = MarlinCipherGetPaddingSize(context, finalBlock, &paddingSize);
	if (result != 0) {
		result = 0x22030004;
		goto out;
	}
	finalSize = blockSize - paddingSize;
	if (finalSize != 0) {
		if (*dataSize < (blocks - 1) * blockSize + finalSize) {
			result = 3;
			goto out;
		}
		MarlinCopyMemory(output, finalBlock, finalSize);
	}
	*dataSize = (blocks - 1) * blockSize + finalSize;
out:
	return result;
}

unsigned int MarlinCipherSetKey(MarlinCipherContext *context,
			       const void *key, unsigned int keySize)
{
	int result = 0;
	int operation;
	SceAesFullContext *keySchedule = 0;

	if (context == 0) {
		result = 3;
		goto out;
	}
	switch (context->algorithm) {
	case 1:
		if (context->keyOperation == 0x100 ||
		    context->keyOperation == 0x300)
			operation = 1;
		else
			operation = 2;
		result = MarlinKeyScheduleCreate(&keySchedule, operation,
						keySize, key);
		if (result != 0)
			result = (((unsigned int)result >> 16) << 16) == 0 ?
				result : 0x22030005;
		else
			context->keySchedule = keySchedule;
		break;
	default:
		result = 0x22030001;
		goto out;
	}
out:
	if (result != 0 && keySchedule != 0) {
		switch (context->algorithm) {
		case 1:
			MarlinKeyScheduleDestroy(keySchedule);
			break;
		default:
			;
		}
	}
	return result;
}

int MarlinCipherCreate(MarlinCipherContext **context, unsigned int selector,
		       const void *iv, unsigned int ivSize, const void *counter,
		       const void *key, unsigned int keySize)
{
	unsigned int result = 0;
	MarlinCipherContext *cipher = 0;
	int algorithm;
	int operation;
	int blockMode;
	int paddingMode;

	*context = 0;
	cipher = MarlinAllocateMemory(0, sizeof(*cipher));
	if (cipher == 0) {
		result = 2;
		goto out;
	}
	MarlinFillMemory(cipher, 0, sizeof(*cipher));
	algorithm = (unsigned char)selector;
	switch (algorithm) {
	case 1:
		cipher->algorithm = algorithm;
		cipher->blockSize = 16;
		break;
	default:
		result = 0x22030001;
		goto out;
	}
	operation = selector & 0xff00;
	switch (operation) {
	case 0x100:
		cipher->keyOperation = operation;
		break;
	case 0x200:
		cipher->keyOperation = operation;
		break;
	case 0x300:
		cipher->keyOperation = operation;
		break;
	default:
		result = 0x22030001;
		goto out;
	}
	blockMode = selector & 0xff0000;
	switch (blockMode) {
	case 0x10000:
		cipher->blockMode = blockMode;
		break;
	case 0x20000:
		cipher->blockMode = blockMode;
		break;
	case 0x30000:
		cipher->blockMode = blockMode;
		if (cipher->keyOperation == 0x300)
			break;
		result = 0x22030001;
		goto out;
	default:
		result = 0x22030001;
		goto out;
	}
	paddingMode = selector & 0xff000000;
	switch ((int)paddingMode) {
	case 0x1000000:
		cipher->paddingMode = paddingMode;
		break;
	case 0x2000000:
		cipher->paddingMode = paddingMode;
		break;
	case 0x3000000:
		cipher->paddingMode = paddingMode;
		break;
	case 0x4000000:
		cipher->paddingMode = paddingMode;
		break;
	default:
		result = 0x22030001;
		goto out;
	}
	if (iv != 0) {
		if (ivSize > 16) {
			result = 3;
			goto out;
		}
		MarlinCopyMemory(cipher->iv, iv, ivSize);
		cipher->ivSize = ivSize;
	} else {
		MarlinFillMemory(cipher->iv, 0, 16);
		cipher->ivSize = 0;
	}
	if (counter != 0)
		MarlinCopyMemory(cipher->counter, counter, 8);
	else
		MarlinFillMemory(cipher->counter, 0, 8);
	result = MarlinCipherSetKey(cipher, key, keySize);
	if (result == 0)
		*context = cipher;
out:
	if (*context == 0)
		MarlinFreeMemory(0, cipher);
	return result;
}

int MarlinCipherDestroy(MarlinCipherContext *context)
{
	int result = 0;
	MarlinCipherContext *cipher = context;

	if (cipher != 0) {
		switch (cipher->algorithm) {
		case 1:
			MarlinKeyScheduleDestroy(cipher->keySchedule);
			break;
		}
		MarlinFreeMemory(0, cipher);
	}
	return result;
}

int MarlinCipherEncrypt(MarlinCipherContext *context, const void *data,
			unsigned int size, void *encrypted,
			unsigned int *encryptedSize)
{
	int result = 0;
	MarlinCipherContext *cipher = context;
	SceAesFullContext *keySchedule;
	unsigned char paddingBlock[16];
	unsigned int paddingSize;

	paddingSize = 0;
	keySchedule = 0;
	keySchedule = cipher->keySchedule;
	result = MarlinCipherValidate(cipher);
	if (result == 0) {
		result = MarlinCipherPreparePadding(cipher, data, size,
						    paddingBlock, &paddingSize);
		if (result == 0)
			result = marlinCipherEncryptors[cipher->blockMode >> 16](
				cipher, keySchedule, data, size, encrypted,
				encryptedSize, paddingBlock, paddingSize);
	}
	return result;
}

int MarlinCipherProcess(MarlinCipherContext *context, const void *input,
			unsigned int size, void *output,
			unsigned int *outputSize)
{
	int result = 0;
	MarlinCipherContext *cipher = context;
	SceAesFullContext *keySchedule = 0;

	keySchedule = cipher->keySchedule;
	result = MarlinCipherValidate(cipher);
	if (result == 0)
		result = marlinCipherDecryptors[cipher->blockMode >> 16](cipher,
				keySchedule, input, size, output, outputSize);
	return result;
}

int MarlinEncryptData(const void *iv, const void *key, const void *data,
		      unsigned int size, void *encrypted,
		      unsigned int *encryptedSize)
{
	int result;
	MarlinCipherContext *context = 0;

	result = MarlinCipherCreate(&context, 0x2020101, iv, 16, 0, key, 16);
	if (result == 0)
		result = MarlinCipherEncrypt(context, data, size, encrypted,
					     encryptedSize);
	MarlinCipherDestroy(context);
	return result;
}

int MarlinDecryptData(const void *iv, const void *key, const void *encrypted,
		      unsigned int size, void *data, unsigned int *dataSize)
{
	int result;
	MarlinCipherContext *context = 0;

	result = MarlinCipherCreate(&context, 0x2020201, iv, 16, 0, key, 16);
	if (result == 0)
		result = MarlinCipherProcess(context, encrypted, size, data, dataSize);
	MarlinCipherDestroy(context);
	return result;
}

int MarlinCipherTransformBlock(const void *key, const void *input, void *output)
{
	int result;
	MarlinCipherContext *context = 0;
	unsigned int outputSize = 16;

	result = MarlinCipherCreate(&context, 0x1010201, 0, 16, 0, key, 16);
	if (result == 0)
		result = MarlinCipherProcess(context, input, 16, output, &outputSize);
	MarlinCipherDestroy(context);
	return result;
}

int MarlinKeyScheduleCreate(SceAesFullContext **context, int operation,
			    unsigned int keySize, const void *key)
{
	unsigned int result = 0;
	SceAesFullContext *schedule = 0;
	unsigned int keyBits = 0;
	unsigned int blockBits = 0;

	*context = 0;
	schedule = MarlinAllocateMemory(0, sizeof(*schedule));
	if (schedule == 0) {
		result = 2;
		goto out;
	}
	if (keySize == 16 || keySize == 24 || keySize == 32) {
		blockBits = keySize * 8;
		keyBits = blockBits;
		result = SceKernelUtilsForDriver_F12B6451(schedule, blockBits, keyBits, key);
		*context = schedule;
	} else {
		result = 0x22020002;
	}
out:
	if (*context == 0)
		MarlinFreeMemory(0, schedule);
	return result;
}

int MarlinKeyScheduleDestroy(SceAesFullContext *context)
{
	int result = 0;
	SceAesFullContext *schedule = context;

	MarlinFreeMemory(0, schedule);
	return result;
}

int MarlinEncryptBlock(SceAesFullContext *key, const void *data, void *encrypted)
{
	int result = 0;
	SceAesFullContext *schedule = key;

	if (schedule == 0) {
		result = 0x22020004;
		goto out;
	}
	if (data == 0 || encrypted == 0) {
		result = 3;
		goto out;
	}
	SceKernelUtilsForDriver_C2A61770(schedule, data, encrypted);
out:
	return result;
}

int MarlinDecryptBlock(SceAesFullContext *key, const void *encrypted, void *data)
{
	int result = 0;
	SceAesFullContext *schedule = key;

	if (schedule == 0) {
		result = 0x22020004;
		goto out;
	}
	if (encrypted == 0 || data == 0) {
		result = 3;
		goto out;
	}
	SceKernelUtilsForDriver_D8678061(schedule, encrypted, data);
out:
	return result;
}
