/* Authentication message assembly. */
#include <psp2kern/kernel/threadmgr.h>
#include <sysmem/utils.h>
#include "adhoc.h"

extern GcAuthAdhocWorkspace gcAuthAdhocWorkspace;
extern SceUID gcAuthAdhocSemaphore;
extern GcAuthAdhocSigningScratch gcAuthAdhocSigningScratch;
int GcAuthExecuteKirkCommand(void *output, const void *input, int command,
		size_t size, int keyId);
int GcAuthGenerateRandom20(unsigned char *data);

int GcAuthAdhocSign160(const GcAuthAdhocSign160Request *request)
{
	unsigned char *cursor;
	unsigned int index;
	int result;

	if (request->keyId != 0)
		return (int)0x808a040a;
	if (ksceKernelWaitSema(gcAuthAdhocSemaphore, 1, NULL) != 0)
		return (int)0x808a0008;
	cursor = gcAuthAdhocSigningScratch.input160.zero;
	do {
		*cursor++ = 0;
	} while (cursor != gcAuthAdhocSigningScratch.input160.zero + 32);
	cursor = gcAuthAdhocSigningScratch.input160.digest;
	for (index = 0; index != 20; ++index)
		*cursor++ = request->digest[index];
	result = (int)0x808a040c;
	if (GcAuthExecuteKirkCommand(&gcAuthAdhocSigningScratch,
		&gcAuthAdhocSigningScratch, 33,
		sizeof(gcAuthAdhocSigningScratch.input160), 0) == 0) {
		for (index = 0; index != sizeof(*request->signature); ++index)
			((unsigned char *)request->signature)[index] =
				((unsigned char *)&gcAuthAdhocSigningScratch.signature160)[index];
		result = 0;
	}
	if (ksceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		return (int)0x808a0008;
	return result;
}

int GcAuthAdhocSign224(const GcAuthAdhocSign224Request *request)
{
	unsigned char *cursor;
	unsigned int index;
	int result;

	if (ksceKernelWaitSema(gcAuthAdhocSemaphore, 1, NULL) != 0)
		return (int)0x808a0008;
	cursor = gcAuthAdhocSigningScratch.input224.zero;
	do {
		*cursor++ = 0;
	} while (cursor != gcAuthAdhocSigningScratch.input224.zero + 32);
	cursor = gcAuthAdhocSigningScratch.input224.digest;
	for (index = 0; index != 28; ++index)
		*cursor++ = request->digest[index];
	result = (int)0x808a040c;
	if (GcAuthExecuteKirkCommand(&gcAuthAdhocSigningScratch,
		&gcAuthAdhocSigningScratch, 34,
		sizeof(gcAuthAdhocSigningScratch.input224), request->keyId) == 0) {
		for (index = 0; index != sizeof(*request->signature); ++index)
			((unsigned char *)request->signature)[index] =
				((unsigned char *)&gcAuthAdhocSigningScratch.signature224)[index];
		result = 0;
	}
	if (ksceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		return (int)0x808a0008;
	return result;
}

int GcAuthAdhocHashSpans(GcAuthAdhocHashMessage *message,
		const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize)
{
	unsigned int index;
	unsigned int size = secondSize + thirdSize + firstSize;
	unsigned char *cursor;

	message->size = size;
	for (index = 0; index != firstSize; ++index)
		message->payload[index] = first[index];
	cursor = message->payload + firstSize;
	for (index = 0; index != secondSize; ++index)
		cursor[index] = second[index];
	cursor += secondSize;
	for (index = 0; index != thirdSize; ++index)
		cursor[index] = third[index];
	if (SceKernelUtilsForDriver_87DC7F2F(message->payload, size, message) != 0)
		return (int)0x808a0103;
	return 0;
}

int GcAuthAdhocGenerateKeystream(GcAuthAdhocCounterScratch *scratch,
		uint64_t *counter, const unsigned int *seed)
{
	uint64_t initial = *counter;
	uint64_t current = initial;
	GcAuthAdhocCounterBlock *block = scratch->input.blocks;
	unsigned int index = 0;

	do {
		block->counterLow = (unsigned int)*counter;
		block->counterHigh = (unsigned int)(*counter >> 32);
		if (current & 1) {
			block->seedLow = seed[2];
			block->seedHigh = seed[3];
		} else {
			block->seedLow = seed[0];
			block->seedHigh = seed[1];
		}
		++current;
		++block;
		++index;
	} while (index != 2);
	*counter = initial + 2;
	scratch->input.mode = 5;
	scratch->input.zero[0] = 0;
	scratch->input.zero[1] = 0;
	scratch->input.keyId = 68;
	scratch->input.size = 32;
	if (GcAuthExecuteKirkCommand(scratch, scratch->input.blocks,
		7, 32, 68) != 0)
		return (int)0x808a010b;
	return 0;
}

int GcAuthAdhocEncryptAuthData(GcAuthAdhocAuthScratch *scratch,
		GcAuthAdhocAuthData *data, uint64_t *counter,
		const unsigned int *seed, GcAuthAdhocAuthTag *tag)
{
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
	for (index = 0; index != sizeof(tag->zero); ++index)
		tag->zero[index] = 0;
	result = GcAuthAdhocGenerateKeystream(&scratch->counter, counter, seed);
	if (result != 0)
		return result;
	for (index = 0; index != 8; ++index)
		data->words[index] ^= scratch->counter.keystream[index];
	result = GcAuthAdhocHashSpans(&scratch->hashMessage,
		(const unsigned char *)seed, 20, (const unsigned char *)tag,
		16, data->bytes, sizeof(data->bytes));
	if (result != 0)
		return result;
	for (index = 0; index != sizeof(tag->hash); ++index)
		tag->hash[index] = scratch->digest[index];
	return result;
}

int GcAuthAdhocDecryptAuthData(GcAuthAdhocAuthScratch *scratch,
		GcAuthAdhocAuthData *data, uint64_t *counter, int unused,
		const unsigned int *seed, const GcAuthAdhocAuthTag *tag)
{
	unsigned int index;
	int result;

	result = GcAuthAdhocHashSpans(&scratch->hashMessage,
		(const unsigned char *)seed, 20, (const unsigned char *)tag,
		16, data->bytes, sizeof(data->bytes));
	if (result != 0)
		return result;
	for (index = 0; index != sizeof(tag->hash); ++index)
		result |= tag->hash[index] ^ scratch->digest[index];
	if (result != 0)
		return (int)0x808a0121;
	result = GcAuthAdhocGenerateKeystream(&scratch->counter, counter, seed);
	if (result != 0)
		return result;
	for (index = 0; index != 8; ++index)
		data->words[index] ^= scratch->counter.keystream[index];
	return result;
}

int GcAuthAdhocEncryptPacket(const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize,
		const unsigned char *fifth, unsigned int fifthSize,
		const unsigned char *sixth, unsigned int sixthSize)
{
	GcAuthAdhocCipherInput *packet = &gcAuthAdhocWorkspace.cipher;
	unsigned char *cursor;
	unsigned int index;
	unsigned int size = (sixthSize + 31 + fifthSize + fourthSize + thirdSize +
		secondSize + firstSize) & ~15U;

	if (GcAuthGenerateRandom20((unsigned char *)&packet->prefix) != 0)
		return (int)0x808a0101;
	for (index = 0; index != 8; ++index)
		packet->prefix.fields.entropy[index] = packet->prefix.words[index];
	packet->prefix.fields.zeroFill[0] = 0;
	packet->prefix.fields.size = size;
	packet->prefix.fields.zeroFill[1] = 0;
	packet->prefix.fields.zeroFill[2] = 0;
	packet->prefix.fields.zeroFill[3] = 0;
	packet->prefix.fields.zeroFill[4] = 0;
	packet->prefix.fields.zeroFill[5] = 0;
	packet->prefix.fields.zeroFill[6] = 0;
	packet->prefix.fields.zeroFill[7] = 0;
	packet->prefix.fields.zero[0] = 0;
	packet->prefix.fields.zero[1] = 0;
	packet->prefix.fields.mode = 4;
	packet->prefix.fields.keyId = 2;
	cursor = packet->payload;
	for (index = 0; index != firstSize; ++index)
		cursor[index] = first[index];
	cursor += firstSize;
	for (index = 0; index != secondSize; ++index)
		cursor[index] = second[index];
	cursor += secondSize;
	for (index = 0; index != thirdSize; ++index)
		cursor[index] = third[index];
	cursor += thirdSize;
	for (index = 0; index != fourthSize; ++index)
		cursor[index] = fourth[index];
	cursor += fourthSize;
	for (index = 0; index != fifthSize; ++index)
		cursor[index] = fifth[index];
	cursor += fifthSize;
	for (index = 0; index != sixthSize; ++index)
		cursor[index] = sixth[index];
	if (GcAuthExecuteKirkCommand(packet->prefix.fields.entropy,
		packet->prefix.fields.entropy, 4, size, 2) != 0)
		return (int)0x808a010b;
	return 0;
}
