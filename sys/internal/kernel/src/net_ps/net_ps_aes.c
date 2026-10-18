/* SCE SDK-backed cipher adapters; original source filename is unavailable. */

#include <sysmem/utils.h>
#include <string.h>
#if FW_VERSION >= 0x04000000
#include <sysmem/fortify.h>
#endif

extern void *(*gNetPsCryptoAllocate)(SceSize size);
extern void (*gNetPsCryptoRelease)(void *ptr);

/* SCE: CBC full blocks, then an encrypted chaining block for the byte tail. */
int sceNetPsAesEncrypt(unsigned char *destination, const unsigned char *source,
		       int length, const void *key, unsigned int keyBits,
		       const unsigned char *initialVector)
{
	int fullLength = length & ~15;
	int tailLength = length & 15;
	SceAesFullContext *context;
	unsigned char *output;
	const unsigned char *input, *chain = initialVector;
	int result, i;

	context = gNetPsCryptoAllocate(sizeof(*context));
	if (context == NULL)
		return -1;
	result = SceKernelUtilsForDriver_F12B6451(context, 128, keyBits, key);
	if (result < 0)
		return result;
	if (fullLength > 0) {
		unsigned char *block;

		output = destination;
		input = source;
		for (;;) {
			block = output;
			if (destination != source)
				memcpy(block, input, 16);
			for (i = 0; i < 16; i++)
				block[i] ^= chain[i];
			output += 16;
			SceKernelUtilsForDriver_C2A61770(context, block, block);
			input += 16;
			if (output == destination + fullLength)
				break;
			chain = block;
		}
#if FW_VERSION >= 0x04000000
		chain = destination + fullLength - 16;
#else
		chain = block;
#endif
	}
	if (tailLength != 0) {
#if FW_VERSION >= 0x04000000
		unsigned char stream[16] = { 0 };
#else
		unsigned char stream[16];

		memset(stream, 0, sizeof(stream));
#endif
		SceKernelUtilsForDriver_C2A61770(context, chain, stream);
		output = destination + fullLength;
		input = source + fullLength;
		for (i = 0; i < tailLength; i++)
			output[i] = input[i] ^ stream[i];
	}
	gNetPsCryptoRelease(context);
	return 0;
}

/* SCE: decode backwards so the preceding ciphertext survives in-place use. */
int sceNetPsAesDecrypt(unsigned char *destination, const unsigned char *source,
		       int length, const void *key, unsigned int keyBits,
		       const unsigned char *initialVector)
{
	int tailLength = length & 15;
	int fullLength = length & ~15;
	SceAesFullContext *context;
	const unsigned char *chain;
	unsigned char *output;
	int result, i;

	context = gNetPsCryptoAllocate(sizeof(*context));
	if (context == NULL)
		return -1;
	result = SceKernelUtilsForDriver_F12B6451(context, 128, keyBits, key);
	if (result < 0)
		return result;
	if (tailLength != 0) {
#if FW_VERSION >= 0x04000000
		unsigned char stream[16] = { 0 };
#else
		unsigned char stream[16];

		memset(stream, 0, sizeof(stream));
#endif
		chain = fullLength != 0 ? source + fullLength - 16 : initialVector;
		SceKernelUtilsForDriver_C2A61770(context, chain, stream);
		output = destination + fullLength;
		for (i = 0; i < tailLength; i++)
			output[i] = source[fullLength + i] ^ stream[i];
	}
	while (fullLength > 15) {
		chain = fullLength > 31 ? source + fullLength - 32 : initialVector;
		fullLength -= 16;
		output = destination + fullLength;
		SceKernelUtilsForDriver_D8678061(context, source + fullLength, output);
		for (i = 0; i < 16; i++)
			output[i] ^= chain[i];
	}
	gNetPsCryptoRelease(context);
	return 0;
}
