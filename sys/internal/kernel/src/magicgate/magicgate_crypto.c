/* Descriptive file name: the original has no source-file debug records. */
#include "magicgate_private.h"
#include <sysmem/aes.h>
#include <sysmem/des.h>
#include <psp2kern/kernel/utils.h>

/* The block's byte fields are numbered from the network-order least byte. */
typedef union MagicGateDesBlock {
	uint64_t value;
	struct {
		unsigned int leastSignificant : 8;
		unsigned int lowerMiddle : 8;
		unsigned int upperMiddle : 8;
		unsigned int mostSignificant : 8;
	} words[2];
} MagicGateDesBlock;

#define LOAD_DES_BLOCK(block, source) do { \
	(block).value = 0; \
	(block).words[1].mostSignificant = (source)[0]; \
	(block).words[0].mostSignificant = (source)[4]; \
	(block).words[1].upperMiddle = (source)[1]; \
	(block).words[0].upperMiddle = (source)[5]; \
	(block).words[1].lowerMiddle = (source)[2]; \
	(block).words[0].lowerMiddle = (source)[6]; \
	(block).words[1].leastSignificant = (source)[3]; \
	(block).words[0].leastSignificant = (source)[7]; \
} while (0)

#define STORE_DES_BLOCK(destination, block) do { \
	(destination)[0] = (block).words[1].mostSignificant; \
	(destination)[1] = (block).words[1].upperMiddle; \
	(destination)[2] = (block).words[1].lowerMiddle; \
	(destination)[3] = (block).words[1].leastSignificant; \
	(destination)[4] = (block).words[0].mostSignificant; \
	(destination)[5] = (block).words[0].upperMiddle; \
	(destination)[6] = (block).words[0].lowerMiddle; \
	(destination)[7] = (block).words[0].leastSignificant; \
} while (0)

int encryptRecordBlock(const MagicGateKey *key, const void *input, void *output)
{
	DesContext first;
	DesContext second;
	const uint8_t *source = input;
	uint8_t *destination = output;
	MagicGateDesBlock block;

	LOAD_DES_BLOCK(block, key->bytes);
	SceKernelUtilsForDriver_AE6C45FB(&first, block.value);
	LOAD_DES_BLOCK(block, key->bytes + 8);
	SceKernelUtilsForDriver_AE6C45FB(&second, block.value);
	LOAD_DES_BLOCK(block, source);
	block.value = SceKernelUtilsForDriver_A0D45D25(&first, block.value);
	block.value = SceKernelUtilsForDriver_801D0ED4(&second, block.value);
	block.value = SceKernelUtilsForDriver_A0D45D25(&first, block.value);
	STORE_DES_BLOCK(destination, block);
	return 0;
}

int decryptRecordBlock(const MagicGateKey *key, const void *input, void *output)
{
	DesContext first;
	DesContext second;
	const uint8_t *source = input;
	uint8_t *destination = output;
	MagicGateDesBlock block;

	LOAD_DES_BLOCK(block, key->bytes);
	SceKernelUtilsForDriver_AE6C45FB(&first, block.value);
	LOAD_DES_BLOCK(block, key->bytes + 8);
	SceKernelUtilsForDriver_AE6C45FB(&second, block.value);
	LOAD_DES_BLOCK(block, source);
	block.value = SceKernelUtilsForDriver_801D0ED4(&first, block.value);
	block.value = SceKernelUtilsForDriver_A0D45D25(&second, block.value);
	block.value = SceKernelUtilsForDriver_801D0ED4(&first, block.value);
	STORE_DES_BLOCK(destination, block);
	return 0;
}

int encryptDesEcb(const void *key, const void *input, void *output,
		  unsigned int size)
{
	DesContext context;
	MagicGateDesBlock block;
	const uint8_t *source = input;
	const uint8_t *nextSource;
	uint8_t *destination = output;
	uint8_t *nextDestination;
	uint8_t *end;

	LOAD_DES_BLOCK(block, (const uint8_t *)key);
	SceKernelUtilsForDriver_AE6C45FB(&context, block.value);
	if (size > 7) {
		nextDestination = destination + 8;
		end = nextDestination + ((size - 8) & ~7U);
		if ((size - 8) & 8) {
			LOAD_DES_BLOCK(block, source);
			source += 8;
			block.value = SceKernelUtilsForDriver_A0D45D25(&context,
							    block.value);
			STORE_DES_BLOCK(destination, block);
			destination += 8;
			nextDestination += 8;
		}
		for (;;) {
			LOAD_DES_BLOCK(block, source);
			nextSource = source + 8;
			block.value = SceKernelUtilsForDriver_A0D45D25(&context,
								    block.value);
			STORE_DES_BLOCK(destination, block);
			if (nextDestination == end)
				break;
			destination = nextDestination + 8;
			source += 16;
			LOAD_DES_BLOCK(block, nextSource);
			block.value = SceKernelUtilsForDriver_A0D45D25(&context,
								    block.value);
			STORE_DES_BLOCK(nextDestination, block);
			nextDestination += 16;
		}
	}
	return 0;
}

int decryptDesEcb(const void *key, const void *input, void *output,
		  unsigned int size)
{
	DesContext context;
	MagicGateDesBlock block;
	const uint8_t *source = input;
	const uint8_t *nextSource;
	uint8_t *destination = output;
	uint8_t *nextDestination;
	uint8_t *end;

	LOAD_DES_BLOCK(block, (const uint8_t *)key);
	SceKernelUtilsForDriver_AE6C45FB(&context, block.value);
	if (size > 7) {
		nextDestination = destination + 8;
		end = nextDestination + ((size - 8) & ~7U);
		if ((size - 8) & 8) {
			LOAD_DES_BLOCK(block, source);
			source += 8;
			block.value = SceKernelUtilsForDriver_801D0ED4(&context,
							    block.value);
			STORE_DES_BLOCK(destination, block);
			destination += 8;
			nextDestination += 8;
		}
		for (;;) {
			LOAD_DES_BLOCK(block, source);
			nextSource = source + 8;
			block.value = SceKernelUtilsForDriver_801D0ED4(&context,
								    block.value);
			STORE_DES_BLOCK(destination, block);
			if (nextDestination == end)
				break;
			destination = nextDestination + 8;
			source += 16;
			LOAD_DES_BLOCK(block, nextSource);
			block.value = SceKernelUtilsForDriver_801D0ED4(&context,
								    block.value);
			STORE_DES_BLOCK(nextDestination, block);
			nextDestination += 16;
		}
	}
	return 0;
}

int encryptDesCbc(const void *key, const void *input, void *output,
		  unsigned int size, const void *iv)
{
	DesContext context;
	MagicGateDesBlock block;
	MagicGateDesBlock chain;
	const uint8_t *source = input;
	uint8_t *destination = output;

	LOAD_DES_BLOCK(block, (const uint8_t *)key);
	SceKernelUtilsForDriver_AE6C45FB(&context, block.value);
	if (iv) {
		LOAD_DES_BLOCK(chain, (const uint8_t *)iv);
	} else {
		chain.value = 0;
	}
	while (size > 7) {
		LOAD_DES_BLOCK(block, source);
		size -= 8;
		source += 8;
		chain.value = SceKernelUtilsForDriver_A0D45D25(&context,
						    chain.value ^ block.value);
		STORE_DES_BLOCK(destination, chain);
		destination += 8;
	}
	return 0;
}

int decryptDesCbc(const void *key, const void *input, void *output,
		  unsigned int size, const void *iv)
{
	DesContext context;
	MagicGateDesBlock block;
	MagicGateDesBlock chain;
	const uint8_t *source = input;
	uint8_t *destination = output;

	LOAD_DES_BLOCK(block, (const uint8_t *)key);
	SceKernelUtilsForDriver_AE6C45FB(&context, block.value);
	if (iv) {
		LOAD_DES_BLOCK(chain, (const uint8_t *)iv);
	} else {
		chain.value = 0;
	}
	while (size > 7) {
		LOAD_DES_BLOCK(block, source);
		size -= 8;
		block.value = SceKernelUtilsForDriver_801D0ED4(&context,
							    block.value);
		block.value ^= chain.value;
		STORE_DES_BLOCK(destination, block);
		destination += 8;
		LOAD_DES_BLOCK(chain, source);
		source += 8;
	}
	return 0;
}

int calculateDesMac(const void *key, const void *input, void *output,
		    unsigned int size, const void *iv)
{
	DesContext context;
	MagicGateDesBlock block;
	MagicGateDesBlock chain;
	const uint8_t *source = input;
	uint8_t *destination = output;
	unsigned int remainder;

	chain.value = 0;
	LOAD_DES_BLOCK(block, (const uint8_t *)key);
	SceKernelUtilsForDriver_AE6C45FB(&context, block.value);
	if (iv) {
		LOAD_DES_BLOCK(chain, (const uint8_t *)iv);
	}
	if (size > 7) {
		size -= 8;
		remainder = size & 8;
		LOAD_DES_BLOCK(block, source);
		source += 8;
		chain.value = SceKernelUtilsForDriver_A0D45D25(&context,
						    chain.value ^ block.value);
		if (size > 7) {
			if (remainder) {
				size -= 8;
				LOAD_DES_BLOCK(block, source);
				source += 8;
				chain.value = SceKernelUtilsForDriver_A0D45D25(
					&context, chain.value ^ block.value);
			}
			while (size > 7) {
				size -= 16;
				LOAD_DES_BLOCK(block, source);
				chain.value = SceKernelUtilsForDriver_A0D45D25(
					&context, chain.value ^ block.value);
				LOAD_DES_BLOCK(block, source + 8);
				source += 16;
				chain.value = SceKernelUtilsForDriver_A0D45D25(
					&context, chain.value ^ block.value);
			}
		}
	}
	STORE_DES_BLOCK(destination, chain);
	return 0;
}

int calculateRetailMac(const MagicGateKey *key, const void *input, void *output,
		       unsigned int size, void *iv)
{
	DesContext first;
	DesContext second;
	MagicGateDesBlock block;
	MagicGateDesBlock chain;
	const uint8_t *source = input;
	uint8_t *destination = output;
	unsigned int remainder;

	LOAD_DES_BLOCK(block, key->bytes);
	SceKernelUtilsForDriver_AE6C45FB(&first, block.value);
	if (iv) {
		LOAD_DES_BLOCK(chain, (const uint8_t *)iv);
	} else {
		chain.value = 0;
	}
	if (size > 7) {
		size -= 8;
		remainder = size & 8;
		LOAD_DES_BLOCK(block, source);
		source += 8;
		chain.value = SceKernelUtilsForDriver_A0D45D25(&first,
						    chain.value ^ block.value);
		if (size > 7) {
			if (remainder) {
				size -= 8;
				LOAD_DES_BLOCK(block, source);
				source += 8;
				chain.value = SceKernelUtilsForDriver_A0D45D25(
					&first, chain.value ^ block.value);
			}
			while (size > 7) {
				size -= 16;
				LOAD_DES_BLOCK(block, source);
				chain.value = SceKernelUtilsForDriver_A0D45D25(
					&first, chain.value ^ block.value);
				LOAD_DES_BLOCK(block, source + 8);
				source += 16;
				chain.value = SceKernelUtilsForDriver_A0D45D25(
					&first, chain.value ^ block.value);
			}
		}
	}
	if (iv) {
		STORE_DES_BLOCK((uint8_t *)iv, chain);
	}
	LOAD_DES_BLOCK(block, key->bytes + 8);
	SceKernelUtilsForDriver_AE6C45FB(&second, block.value);
	block.value = SceKernelUtilsForDriver_801D0ED4(&second, chain.value);
	block.value = SceKernelUtilsForDriver_A0D45D25(&first, block.value);
	STORE_DES_BLOCK(destination, block);
	return 0;
}

int encryptRecordEcb(const MagicGateKey *key, const void *input, void *output,
		     unsigned int size)
{
	SceAesFullContext context;
	const uint8_t *source = input;
	uint8_t *destination = output;
	const uint8_t *next;
	const uint8_t *end;
	unsigned int remainder;

	ksceAesInit1(&context, 128, 128, key);
	if (size > 15) {
		next = source + 16;
		remainder = ((size - 16) >> 4) & 3;
		end = next + ((size - 16) & ~15U);
		if (remainder) {
			ksceAesEncrypt1(&context, source, destination);
			destination += 16;
			source += 16;
			next += 16;
			if (remainder != 1) {
				if (remainder != 2) {
					ksceAesEncrypt1(&context, source, destination);
					source = next;
					destination += 16;
					next += 16;
				}
				ksceAesEncrypt1(&context, source, destination);
				source = next;
				destination += 16;
				next += 16;
			}
		}
		for (;;) {
			ksceAesEncrypt1(&context, source, destination);
			if (next == end)
				break;
			ksceAesEncrypt1(&context, next, destination + 16);
			source = next + 48;
			ksceAesEncrypt1(&context, next + 16, destination + 32);
			ksceAesEncrypt1(&context, next + 32, destination + 48);
			next += 64;
			destination += 64;
		}
	}
	return 0;
}

int decryptRecordEcb(const MagicGateKey *key, const void *input, void *output,
		     unsigned int size)
{
	SceAesFullContext context;
	const uint8_t *source = input;
	uint8_t *destination = output;
	const uint8_t *next;
	const uint8_t *end;
	unsigned int remainder;

	ksceAesInit1(&context, 128, 128, key);
	if (size > 15) {
		next = source + 16;
		remainder = ((size - 16) >> 4) & 3;
		end = next + ((size - 16) & ~15U);
		if (remainder) {
			ksceAesDecrypt1(&context, source, destination);
			destination += 16;
			source += 16;
			next += 16;
			if (remainder != 1) {
				if (remainder != 2) {
					ksceAesDecrypt1(&context, source, destination);
					source = next;
					destination += 16;
					next += 16;
				}
				ksceAesDecrypt1(&context, source, destination);
				source = next;
				destination += 16;
				next += 16;
			}
		}
		for (;;) {
			ksceAesDecrypt1(&context, source, destination);
			if (next == end)
				break;
			ksceAesDecrypt1(&context, next, destination + 16);
			source = next + 48;
			ksceAesDecrypt1(&context, next + 16, destination + 32);
			ksceAesDecrypt1(&context, next + 32, destination + 48);
			next += 64;
			destination += 64;
		}
	}
	return 0;
}

int encryptRecordCbc(const void *key, const void *input, void *output,
		     unsigned int size, const void *iv)
{
	SceAesFullContext context;
	uint64_t chainStorage[4];
	uint64_t blockStorage[4];
	uint8_t *chain = (uint8_t *)chainStorage;
	uint8_t *block = (uint8_t *)blockStorage;
	const uint8_t *initial = iv;
	const uint8_t *source = input;
	uint8_t *destination = output;
	unsigned int blockSize;
	unsigned int index;

	if (ksceAesInit1(&context, 128, 128, key) >= 0) {
		blockSize = context.encryption.blockWords * 4;
		for (index = 0; index < blockSize; index++)
			chain[index] = initial[index];
	}
	while (size > 15) {
		blockSize = context.encryption.blockWords * 4;
		for (index = 0; index < blockSize; index++)
			block[index] = chain[index] ^ source[index];
		ksceAesEncrypt1(&context, block, destination);
		blockSize = context.encryption.blockWords * 4;
		for (index = 0; index < blockSize; index++)
			chain[index] = destination[index];
		source += 16;
		destination += 16;
		size -= 16;
	}
	return 0;
}

int decryptRecordCbc(const MagicGateKey *key, const void *input, void *output,
		     unsigned int size, const void *iv)
{
	SceAesFullContext context;
	uint64_t chainStorage[4];
	uint64_t blockStorage[4];
	uint8_t *chain = (uint8_t *)chainStorage;
	uint8_t *block = (uint8_t *)blockStorage;
	const uint8_t *initial = iv;
	const uint8_t *source = input;
	uint8_t *destination = output;
	unsigned int blockSize;
	unsigned int index;
	uint8_t decrypted;

	if (ksceAesInit1(&context, 128, 128, key) >= 0) {
		blockSize = context.encryption.blockWords * 4;
		for (index = 0; index < blockSize; index++)
			chain[index] = initial[index];
	}
	while (size > 15) {
		ksceAesDecrypt1(&context, source, block);
		blockSize = context.encryption.blockWords * 4;
		for (index = 0; index < blockSize; index++) {
			decrypted = chain[index] ^ block[index];
			chain[index] = source[index];
			destination[index] = decrypted;
		}
		source += 16;
		destination += 16;
		size -= 16;
	}
	return 0;
}

int calculateRecordMac(const MagicGateKey *key, const void *input, void *output,
		       unsigned int size, const void *iv)
{
	SceAesFullContext context;
	uint64_t chainStorage[4];
	uint8_t *chain = (uint8_t *)chainStorage;
	const uint8_t *initial = iv;
	const uint8_t *source = input;
	unsigned int blockSize;
	unsigned int index;

	if (ksceAesInit1(&context, 128, 128, key) >= 0) {
		blockSize = context.encryption.blockWords * 4;
		for (index = 0; index < blockSize; index++)
			chain[index] = initial[index];
	}
	while (size > 15) {
		blockSize = context.encryption.blockWords * 4;
		for (index = 0; index < blockSize; index++)
			chain[index] ^= source[index];
		ksceAesEncrypt1(&context, chain, chain);
		source += 16;
		size -= 16;
	}
	for (index = 0; index < 16; index++)
		((uint8_t *)output)[index] = chain[index];
	return 0;
}
