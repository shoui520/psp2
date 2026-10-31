/* Descriptive file name: the original has no source-file debug records. */
#include "magicgate_private.h"
#include <string.h>
#include <sysmem/sysclib.h>

#define INSTALL_NEXT_BLOCK(key, block, temporary) do { \
	decryptRecordBlock((key), &(block)->bytes[0], &(temporary)); \
	decryptRecordBlock((key), &(block)->bytes[8], &(key)->halves[1]); \
	(key)->halves[0] = (temporary); \
	++(block); \
} while (0)

MagicGateKey slotKeys[41];
MagicGateInstalledRecord installedRecord;

/* The caller continues record installation when this check returns an error. */
int checkCachedRecord(int slot, const MagicGateRecordHeader *record)
{
	(void)slot;
	(void)record;
	return (int)0x80500001;
}

/* The original installation notification has an empty body. */
void notifyRecordInstalled(int slot, const MagicGateRecordIdentity *identity)
{
	(void)slot;
	(void)identity;
}

int verifyRecordTag(const void *tag, const uint32_t *identifier,
		    const void *data, unsigned int size, int slot)
{
	uint8_t workspaceStorage[0x50];
	struct Workspace {
		MagicGateKey iv;
		MagicGateKey finalBlock;
		MagicGateKey key;
		MagicGateKey message;
	} *workspace = (struct Workspace *)(workspaceStorage +
			  (-(SceUInt32)(SceUIntPtr)workspaceStorage & 0xf));
	const uint8_t *input = data;
	const uint8_t *tail;
	const uint32_t *words;
	unsigned int remaining;
	int result;

	result = getDriverKey(&workspace->key, 0);
	if (result < 0)
		return result;
	result = getDriverKey(&workspace->finalBlock, 1);
	if (result < 0)
		return result;
	workspace->iv.words[0] = 0;
	workspace->iv.words[1] = 0;
	workspace->iv.words[2] = 0;
	workspace->iv.words[3] = 0;
	workspace->message.words[0] = *identifier;
	workspace->key.halves[0] ^= slotKeys[slot].halves[0];
	workspace->key.halves[1] ^= slotKeys[slot].halves[1];
	if (size <= 11) {
		memcpy(&workspace->message.bytes[4], input, size);
		tail = workspace->message.bytes;
		remaining = size + 4;
	} else {
		words = (const uint32_t *)input;
		workspace->message.words[1] = words[0];
		workspace->message.words[2] = words[1];
		workspace->message.words[3] = words[2];
		result = encryptRecordCbc(&workspace->key, &workspace->message,
					  &workspace->iv, 16, &workspace->iv);
		if (result < 0)
			return result;
		remaining = size - 12;
		tail = input + 12;
		while (remaining > 16) {
			words = (const uint32_t *)tail;
			workspace->message.words[0] = words[0];
			workspace->message.words[1] = words[1];
			workspace->message.words[2] = words[2];
			workspace->message.words[3] = words[3];
			result = encryptRecordCbc(&workspace->key, &workspace->message,
						  &workspace->iv, 16, &workspace->iv);
			if (result < 0)
				return result;
			remaining -= 16;
			tail += 16;
		}
	}
	if (remaining) {
		memmove(&workspace->message, tail, remaining);
		memset(&workspace->message.bytes[remaining], 0, 16 - remaining);
		result = encryptRecordCbc(&workspace->key, &workspace->message,
					  &workspace->iv, 16, &workspace->iv);
		if (result < 0)
			return result;
	}
	result = encryptRecordCbc(&workspace->key, &workspace->finalBlock,
				  &workspace->message, 16, &workspace->iv);
	if (result < 0)
		return result;
	if (timingsafe_memcmp(tag, &workspace->message, 16))
		return (int)0x80501004;
	return 0;
}

int installRecordKey(int slot, int type,
		     const MagicGateInstalledRecord *record, const uint64_t *tag)
{
	MagicGateKey key __attribute__((aligned(16)));
	MagicGateKey *destination = &slotKeys[slot];
	const MagicGateBlock *block = record->blocks;
	unsigned int depth;
	unsigned int count;
	uint64_t firstHalf;
	uint64_t verification;
	int last;
	int remaining;
	int result;

	depth = (unsigned int)record->depth[0] << 24 |
		(unsigned int)record->depth[1] << 16 |
		(unsigned int)record->depth[2] << 8 | record->depth[3];
	count = (unsigned int)record->count[0] << 24 |
		(unsigned int)record->count[1] << 16 |
		(unsigned int)record->count[2] << 8 | record->count[3];
	result = getTreeKey(type, &key, depth);
	if (result < 0)
		return result;
	destination->words[0] = key.words[0];
	destination->words[1] = key.words[1];
	destination->words[2] = key.words[2];
	destination->words[3] = key.words[3];
	last = (int)(count - 1);
	if (last >= 0) {
		INSTALL_NEXT_BLOCK(destination, block, firstHalf);
		remaining = last - 1;
		switch (last & 3) {
		case 3:
			INSTALL_NEXT_BLOCK(destination, block, firstHalf);
			--remaining;
			/* Fall through. */
		case 2:
			INSTALL_NEXT_BLOCK(destination, block, firstHalf);
			--remaining;
			/* Fall through. */
		case 1:
			INSTALL_NEXT_BLOCK(destination, block, firstHalf);
			--remaining;
		}
		while (remaining != -1) {
			INSTALL_NEXT_BLOCK(destination, block, firstHalf);
			INSTALL_NEXT_BLOCK(destination, block, firstHalf);
			INSTALL_NEXT_BLOCK(destination, block, firstHalf);
			INSTALL_NEXT_BLOCK(destination, block, firstHalf);
			remaining -= 4;
		}
	}
	if (tag && *tag) {
		decryptRecordBlock(destination, tag, &verification);
		if (record->identity.identifier != (uint32_t)verification)
			return (int)0x80501004;
	}
	return 0;
}
