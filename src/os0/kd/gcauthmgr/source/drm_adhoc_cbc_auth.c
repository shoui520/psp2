/* CBC counter-stream authentication. */
#include "adhoc.h"

int GcAuthAdhocGenerateCbcCounterStream(GcAuthAdhocCounterStream *stream,
		unsigned int size, uint64_t *counter, const unsigned int *seed);
int GcAuthAdhocHashSpans(GcAuthAdhocHashMessage *message,
		const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize);

int GcAuthAdhocEncryptCbcAuthData(GcAuthAdhocWorkspace *scratch,
		unsigned char *data, unsigned int size, uint64_t *counter,
		const unsigned int *seed, GcAuthAdhocAuthTag *tag, int encrypt)
{
	unsigned char *keystream = (unsigned char *)scratch;
	unsigned int *dataWords = (unsigned int *)data;
	unsigned int *keystreamWords = (unsigned int *)scratch;
	unsigned int index;
	int result;

	tag->counter[0] = (unsigned char)*counter;
	tag->counter[1] = (unsigned char)(*counter >> 8);
	tag->counter[2] = (unsigned char)(*counter >> 16);
	tag->counter[3] = (unsigned char)(*counter >> 24);
	tag->counter[4] = (unsigned char)(*counter >> 32);
	tag->counter[5] = (unsigned char)(*counter >> 40);
	tag->counter[6] = (unsigned char)(*counter >> 48);
	tag->counter[7] = (unsigned char)(*counter >> 56);
	for (index = 0; index != 8; ++index)
		tag->zero[index] = 0;
	if (encrypt != 0) {
		if (size == 0)
			return (int)0x808a011f;
		result = GcAuthAdhocGenerateCbcCounterStream(&scratch->counterStream,
				size, counter, seed);
		if (result != 0)
			return result;
		if ((uintptr_t)data & 3) {
			for (index = 0; index != size; ++index)
				data[index] ^= keystream[index];
		} else {
			for (index = 0; index != (size >> 2); ++index)
				dataWords[index] ^= keystreamWords[index];
		}
	}
	result = GcAuthAdhocHashSpans(&scratch->hashMessage,
			(const unsigned char *)seed, 20,
			(const unsigned char *)tag, 16, data, size);
	if (result == 0) {
		for (index = 0; index != 16; ++index)
			tag->hash[index] = keystream[index];
	}
	return result;
}

int GcAuthAdhocDecryptCbcAuthData(GcAuthAdhocWorkspace *scratch,
		unsigned char *data, unsigned int size, uint64_t *counter,
		const uint64_t *minimumCounter, const unsigned int *seed,
		const GcAuthAdhocAuthTag *tag, int decrypt, int checkCounter)
{
	unsigned char *keystream = (unsigned char *)scratch;
	unsigned int *dataWords = (unsigned int *)data;
	unsigned int *keystreamWords = (unsigned int *)scratch;
	unsigned int index;
	int result;
	int mismatch;

	result = GcAuthAdhocHashSpans(&scratch->hashMessage,
			(const unsigned char *)seed, 20,
			(const unsigned char *)tag, 16, data, size);
	if (result != 0)
		return result;
	mismatch = result;
	for (index = 0; index != 16; ++index)
		mismatch |= keystream[index] ^ tag->hash[index];
	if (mismatch != 0)
		return (int)0x808a0121;
	if (checkCounter != 0 && *counter < *minimumCounter)
		return (int)0x808a0123;
	if (decrypt == 0)
		return result;
	if (size == 0)
		return (int)0x808a011f;
	result = GcAuthAdhocGenerateCbcCounterStream(&scratch->counterStream,
			size, counter, seed);
	if (result == 0) {
		if ((uintptr_t)data & 3) {
			for (index = 0; index != size; ++index)
				data[index] ^= keystream[index];
		} else {
			for (index = 0; index != (size >> 2); ++index)
				dataWords[index] ^= keystreamWords[index];
		}
		return mismatch;
	}
	return result;
}
