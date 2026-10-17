/* Variable-length CBC counter stream. */
#include <psp2/types.h>
#include "adhoc.h"
#include "drm_crypto.h"
#include <psp2kern/kernel/utils.h>

extern const unsigned char gcAuthAdhocCounterAesKey[16];

int GcAuthAdhocGenerateCbcCounterStream(GcAuthAdhocCounterStream *stream,
		unsigned int size, uint64_t *counter, const unsigned int *seed)
{
	struct GcAuthAesCbcState state;
	unsigned char iv[16];
	unsigned char block[32];
	GcAuthAdhocCounterBlock *entry = stream->blocks;
	unsigned char *cursor = (unsigned char *)stream->blocks;
	unsigned char *output = (unsigned char *)stream;
	unsigned char *end;
	unsigned int limit = size + 20;
	unsigned int position;
	unsigned int index;
	unsigned int blockBytes;
	uint64_t value;
	const unsigned char *seedBytes;
	unsigned char *entrySeed;
	unsigned char previous;

	if (limit > 20) {
		value = *counter;
		position = 20;
		do {
			entry->counterLow = (unsigned int)*counter;
			entry->counterHigh = (unsigned int)(*counter >> 32);
			if (value & 1) {
				if ((uintptr_t)seed & 3) {
					seedBytes = (const unsigned char *)(seed + 2);
					entrySeed = (unsigned char *)&entry->seedLow;
					for (index = 0; index != 8; ++index)
						entrySeed[index] = seedBytes[index];
					value = *counter;
				} else {
					entry->seedLow = seed[2];
					entry->seedHigh = seed[3];
				}
			} else if ((uintptr_t)seed & 3) {
				seedBytes = (const unsigned char *)seed;
				entrySeed = (unsigned char *)&entry->seedLow;
				for (index = 0; index != 8; ++index)
					entrySeed[index] = seedBytes[index];
				value = *counter;
			} else {
				entry->seedLow = seed[0];
				entry->seedHigh = seed[1];
			}
			position += 16;
			*counter = ++value;
			++entry;
		} while (limit > position);
	}
	for (index = 0; index != 16; ++index)
		iv[index] = 0;
	if (ksceAesInit1(&state.aes, 128, 128, gcAuthAdhocCounterAesKey) >= 0) {
		blockBytes = state.aes.encryption.blockWords * 4;
		for (index = 0; index != blockBytes; ++index)
			state.chain[index] = iv[index];
	}
	end = cursor + size;
	while (cursor < end) {
		ksceAesDecrypt1(&state.aes, cursor, block);
		blockBytes = state.aes.encryption.blockWords * 4;
		for (index = 0; index != blockBytes; ++index) {
			previous = state.chain[index];
			state.chain[index] = cursor[index];
			output[index] = block[index] ^ previous;
		}
		cursor += 16;
		output += 16;
	}
	return 0;
}
