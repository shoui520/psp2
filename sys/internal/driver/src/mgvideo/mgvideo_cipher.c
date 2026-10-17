/* Descriptive source name: the original has no source-file debug records. */
#include "mgvideo_private.h"
#include "mgvideo_result.h"
#include <processmgr/processmgr_imports.h>
#include <usbstormg/usbstormg.h>
#include <psp2kern/kernel/sysclib.h>

int SceMagicGateForDriver_F2502A3B(void *output, const void *input,
				unsigned int size, int slot, const void *iv);
int SceMagicGateForDriver_2B026FFF(void *output, const void *input,
				unsigned int size, int slot, const void *iv);
void *SceProcessmgrForDriver_5468892A(SceSize size, void *opt);
int SceProcessmgrForDriver_FC2A424E(void *ptr);
int SceMagicGateForDriver_0B01FE3F(const void *tag,
		const struct MagicGateRecordHeader *record, unsigned int size, int slot);
int SceMagicGateForDriver_0EBA5958(void *output, unsigned int capacity);

int encryptUserVideoBlocks(void *output, const void *input, unsigned int size,
			  int slot, MgVideoCipherBlock *iv)
{
	MgVideoCipherBlock workspace[32] __attribute__((aligned(16)));
	uint8_t *destination = output;
	const uint8_t *source = input;
	unsigned int remaining = size & ~15U;
	unsigned int total = remaining, chunk;
	int result;

	while (remaining != 0) {
		chunk = remaining >= 512 ? 512 : remaining;
		ksceKernelCopyFromUser(workspace, source, chunk);
		result = SceMagicGateForDriver_F2502A3B(workspace, workspace,
						    chunk, slot, iv);
		if (result < 0) {
			TRANSLATE_MAGICGATE_RESULT(result);
			return result;
		}
		source += chunk;
		*iv = workspace[((chunk & ~15U) - 16) / 16];
		output = destination;
		destination += chunk;
		ksceKernelCopyToUser(output, workspace, chunk);
		remaining -= chunk;
	}
	return total;
}

int decryptUserVideoBlocks(void *output, const void *input, unsigned int size,
			  int slot, MgVideoCipherBlock *iv)
{
	/* The first block saves ciphertext; the remaining 32 hold one user chunk. */
	MgVideoCipherBlock workspace[33] __attribute__((aligned(16)));
	MgVideoCipherBlock *blocks = workspace + 1;
	uint8_t *destination = output;
	const uint8_t *source = input;
	unsigned int remaining = size, chunk = 0;
	int result;

	while (remaining != 0) {
		chunk = remaining > 512 ? 512 : remaining;
		ksceKernelCopyFromUser(blocks, source, chunk);
		workspace[0] = workspace[chunk / 16];
		result = SceMagicGateForDriver_2B026FFF(blocks, blocks,
						    chunk, slot, iv);
		if (result < 0) {
			TRANSLATE_MAGICGATE_RESULT(result);
			return result;
		}
		source += chunk;
		*iv = workspace[0];
		output = destination;
		destination += chunk;
		ksceKernelCopyToUser(output, blocks, chunk);
		remaining -= chunk;
	}
	return chunk ? ((uint8_t *)blocks)[chunk - 1] : 0;
}

int encryptPaddedUserVideo(void *output, const void *input, unsigned int size,
			  int slot, MgVideoCipherBlock *iv)
{
	uint8_t finalBlock[16] __attribute__((aligned(16)));
	unsigned int remaining;
	int result, complete;

	result = encryptUserVideoBlocks(output, input, size, slot, iv);
	if (result < 0)
		return result;
	remaining = size - result;
	complete = result;
	if (remaining != 0)
		ksceKernelCopyFromUser(finalBlock, (const uint8_t *)input + complete,
				       remaining);
	memset(finalBlock + remaining, 16 - remaining, 16 - remaining);
	result = SceMagicGateForDriver_F2502A3B(finalBlock, finalBlock, 16,
					    slot, iv);
	if (result >= 0) {
		ksceKernelCopyToUser((uint8_t *)output + complete, finalBlock, 16);
		return complete + 16;
	}
	TRANSLATE_MAGICGATE_RESULT(result);
	return result;
}


int verifyUserVideoRecord(MgVideoContext *context, int index,
			  const uint64_t *headerTag, const MgVideoUserBuffer *input,
			  int slot)
{
	struct MagicGateRecordHeader *record;
	int result;

	if (index < 0 || (result = timingsafe_memcmp(headerTag,
			&context->records.recordHeaderTags[index], 8)) != 0) {
		record = SceProcessmgrForDriver_5468892A(input->size, 0);
		if (record == 0)
			return (int)0x8050c004;
		ksceKernelCopyFromUser(record, input->data, input->size);
		result = SceMagicGateForDriver_0B01FE3F(&context->deviceIdentity,
						     record, input->size, slot);
		if (result >= 0) {
			SceProcessmgrForDriver_FC2A424E(record);
			return 0;
		}
		if (result == (int)0x80501004)
			result = (int)0x8050c002;
		else
			TRANSLATE_MAGICGATE_RESULT(result);
		SceProcessmgrForDriver_FC2A424E(record);
		return result;
	}
	return result;
}

int installUserVideoRecord(MgVideoContext *context, const MgVideoUserBuffer *input)
{
	MagicGateRecordHeader *record;
	int slot, result;

	if (context == 0)
		return (int)0x8050c002;
	slot = SceMagicGateForDriver_FEF11D84(0);
	if (slot < 0) {
		result = slot;
		goto translateResult;
	}
	record = SceProcessmgrForDriver_5468892A(input->size, 0);
	if (record == 0) {
		SceMagicGateForDriver_144BFA76(slot);
		return (int)0x8050c004;
	}
	ksceKernelCopyFromUser(record, input->data, input->size);
	result = context->callbacks.installRecord(slot, record, input->size);
	if (result >= 0) {
		SceProcessmgrForDriver_FC2A424E(record);
		return slot;
	}
	SceProcessmgrForDriver_FC2A424E(record);
	SceMagicGateForDriver_144BFA76(slot);
	if ((result == (int)0x80501004) | (result == (int)0x80501002))
		return (int)0x8050c031;
translateResult:
	switch (result) {
	case (int)0x80505001:
		return (int)0x8050c021;
	case (int)0x80503001:
		return (int)0x8050c032;
	case (int)0x80503002:
	case (int)0x80505005:
		return (int)0x8050c012;
	case (int)0x80502000 ... (int)0x80502002:
	case (int)0x80504000 ... (int)0x80504002:
		return (int)0x8050c005;
	case (int)0x80505002:
		return (int)0x8050c022;
	case (int)0x80505003:
		return (int)0x8050c024;
	case (int)0x80505004:
		return (int)0x8050c023;
	case (int)0x80505006:
		return (int)0x8050c025;
	case (int)0x80505007:
		return (int)0x8050c026;
	case (int)0x80505009:
		return (int)0x8050c027;
	default:
		break;
	}
	return result;
}

int readVideoEkb2Record(void *output, unsigned int capacity)
{
	/* A bounded output probe; no provider record fields are inspected here. */
	uint8_t probe[48] __attribute__((aligned(16)));
	int required, result;

	required = SceMagicGateForDriver_0EBA5958(probe, sizeof(probe));
	result = required;
	if (required >= 0) {
		if (required == 0)
			goto done;
		if (output == 0)
			goto done;
		result = SceMagicGateForDriver_0EBA5958(output, capacity);
		if (result >= 0) {
			result = required;
			goto done;
		}
	}
	switch (result) {
	case (int)0x80505001:
		result = (int)0x8050c021;
		break;
	case (int)0x80503001:
		result = (int)0x8050c032;
		break;
	case (int)0x80503002:
	case (int)0x80505005:
		result = (int)0x8050c012;
		break;
	case (int)0x80502000 ... (int)0x80502002:
	case (int)0x80504000 ... (int)0x80504002:
		result = (int)0x8050c005;
		break;
	case (int)0x80505002:
		result = (int)0x8050c022;
		break;
	case (int)0x80505003:
		result = (int)0x8050c024;
		break;
	case (int)0x80505004:
		result = (int)0x8050c023;
		break;
	case (int)0x80505006:
		result = (int)0x8050c025;
		break;
	case (int)0x80505007:
		result = (int)0x8050c026;
		break;
	case (int)0x80505009:
		result = (int)0x8050c027;
		break;
	default:
		break;
	}
done:
	return result;
}

int SceMagicGateForDriver_CFD943E0(void *output, const void *input,
				void *iv, unsigned int size);
int SceMagicGateForDriver_C9658A76(void *output, const void *input,
				const void *iv, unsigned int size, int slot);

int updateUserVideoRecord(void *argument)
{
	MgVideoRecordUpdateRequest *request = argument;
	MgVideoCipherBlock workspace[16] __attribute__((aligned(16)));
	MgVideoContext *context = request->context;
	int index = request->index;
	MgVideoCipherBlock *userBlocks = request->userBlocks;
	const int16_t *blockNumbers = request->blockNumbers;
	int blockCount = request->blockCount;
	const MgVideoCipherBlock *replacement = request->replacement;
	MgVideoCipherBlock *recordKey = request->recordKey;
	uint64_t *recordTag = request->recordTag;
	int slot = request->slot;
	uint64_t originalTag = *recordTag;
	int remaining = 15984, firstBlock = 0, changedTotal = 0;
	unsigned int chunk = 256;
	int changed, previousDifference, block, localBlock, position, result;

	for (;;) {
		ksceKernelCopyFromUser(workspace, userBlocks + firstBlock, chunk);
		previousDifference = timingsafe_memcmp(&originalTag, recordTag, 8);
		result = SceMagicGateForDriver_CFD943E0(&originalTag, workspace,
						      &originalTag, chunk);
		if (result < 0)
			goto error;
		changed = 0;
		position = 0;
		if (blockCount > 0) {
#define REPLACE_RECORD_BLOCK() do { \
	block = blockNumbers[position] - 1; \
	if ((unsigned int)block > 998) \
		return (int)0x8050c002; \
	localBlock = block - firstBlock; \
	if (localBlock >= 0 && localBlock < (int)(chunk >> 4)) { \
		workspace[localBlock] = *replacement; \
		changed++; \
	} \
} while (0)
			int prefix = (blockCount - 1) & 3;

			while (prefix-- != 0) {
				REPLACE_RECORD_BLOCK();
				position++;
			}
			for (;;) {
				REPLACE_RECORD_BLOCK();
				if (++position == blockCount)
					break;
				REPLACE_RECORD_BLOCK();
				position++;
				REPLACE_RECORD_BLOCK();
				position++;
				REPLACE_RECORD_BLOCK();
				position++;
			}
#undef REPLACE_RECORD_BLOCK
		}
		if (changed > 0 || previousDifference != 0) {
			result = SceMagicGateForDriver_CFD943E0(recordTag, workspace,
							      recordTag, chunk);
			if (result < 0)
				goto error;
		} else {
			*recordTag = originalTag;
		}
		if (timingsafe_memcmp(recordTag, &originalTag, 8) == 0)
			changed = 0;
		result = SceMagicGateForDriver_C9658A76(recordKey, workspace,
						      recordKey, chunk, slot);
		if (result < 0)
			goto error;
		remaining -= chunk;
		firstBlock += chunk >> 4;
		changedTotal += changed;
		if (remaining <= 0)
			break;
		chunk = remaining >= 256 ? 256 : remaining;
	}
	if (timingsafe_memcmp(&originalTag,
			     &context->records.recordTags[index], 8) != 0 &&
	    memcmp(&context->records.recordTags[index], &zeroVideoBlock, 16) != 0)
		return (int)0x8050c014;
	if (changedTotal != 0) {
		position = blockCount;
#define COPY_RECORD_BLOCK() do { \
	block = blockNumbers[--position] - 1; \
	ksceKernelCopyToUser(userBlocks + block, replacement, 16); \
} while (0)
		if ((position & 3) != 0 && position > 0) {
			int prefix = position & 3;

			COPY_RECORD_BLOCK();
			if (prefix != 1) {
				if (prefix != 2)
					COPY_RECORD_BLOCK();
				COPY_RECORD_BLOCK();
			}
		}
		while (position > 0) {
			COPY_RECORD_BLOCK();
			COPY_RECORD_BLOCK();
			COPY_RECORD_BLOCK();
			COPY_RECORD_BLOCK();
		}
#undef COPY_RECORD_BLOCK
	}
	return changedTotal;
error:
	TRANSLATE_MAGICGATE_RESULT(result);
	return result;
}

int SceMagicGateForDriver_9CEF4965(void *output, const void *input,
				void *iv, unsigned int size, const void *key);
int SceMagicGateForDriver_05912BF5(void *output, const void *input,
				const void *iv, unsigned int size, int slot);

int openUserVideoKeyring(void *argument)
{
	MgVideoKeyringRequest *request = argument;
	struct MgVideoKeyringWorkspace {
		MgVideoCipherBlock headerBlock;
		MgVideoCipherBlock calculatedTag;
		MgVideoCipherBlock selectedBlock;
		MgVideoKeyringEnvelope envelope;
		uint8_t payload[4656];
	} workspace __attribute__((aligned(16)));
	MgVideoContext *context = request->context;
	const MgVideoUserRecord *record = request->record;
	const MgVideoUserBuffer *keyring = request->keyring;
	MgVideoKeyringHeader *header = request->header;
	uint64_t tag = 0;
	void *copy;
	unsigned int size, descriptorCount, keyDataSize, keyCount, payloadOffset;
	int selected = request->blockNumber - 1;
	int result, installed, slot;

	if ((context->validRecordMask & (1U << request->index)) == 0)
		return (int)0x8050c013;
	ksceKernelCopyFromUser(&workspace.selectedBlock, &record->record.blocks[selected], 16);
	ksceKernelCopyFromUser(&workspace.headerBlock, &record->record.header.authenticatedData.fileInfo, 16);
	if (context->contentKeySlot < 0)
		result = SceMagicGateForDriver_CFD943E0(&tag, &workspace.headerBlock, &tag, 16);
	else
		result = SceMagicGateForDriver_9CEF4965(&tag, &workspace.headerBlock, &tag,
						      16, &zeroVideoBlock);
	if (result < 0)
		goto error;
	copy = SceProcessmgrForDriver_5468892A(16048, 0);
	if (copy == 0)
		return (int)0x8050c004;
	size = 16 * selected;
	ksceKernelCopyFromUser(copy, record->record.blocks, size);
	if (context->contentKeySlot < 0)
		result = SceMagicGateForDriver_CFD943E0(&tag, copy, &tag, size);
	else
		result = SceMagicGateForDriver_9CEF4965(&tag, copy, &tag,
						      size, &zeroVideoBlock);
	if (result < 0)
		goto recordError;
	if (context->contentKeySlot < 0)
		result = SceMagicGateForDriver_CFD943E0(&tag, &workspace.selectedBlock, &tag, 16);
	else
		result = SceMagicGateForDriver_9CEF4965(&tag, &workspace.selectedBlock, &tag,
						      16, &zeroVideoBlock);
	if (result < 0)
		goto recordError;
	size = 16 * (998 - selected);
	if (size != 0) {
		ksceKernelCopyFromUser(copy, &record->record.blocks[selected + 1], size);
		if (context->contentKeySlot < 0)
			result = SceMagicGateForDriver_CFD943E0(&tag, copy, &tag, size);
		else
			result = SceMagicGateForDriver_9CEF4965(&tag, copy, &tag,
							      size, &zeroVideoBlock);
		if (result < 0)
			goto recordError;
	}
	result = timingsafe_memcmp(&tag, &context->records.recordTags[request->index], 8)
		 ? (int)0x8050c014 : 0;
	SceProcessmgrForDriver_FC2A424E(copy);
	if (result != 0)
		return result;
	if (memcmp(&workspace.selectedBlock, &zeroVideoBlock, 16) == 0)
		return (int)0x8050c048;
	ksceKernelCopyFromUser(&workspace.envelope, keyring->data, sizeof(workspace.envelope));
	if (memcmp(workspace.envelope.header.label, keyringLabel, 12) != 0)
		return (int)0x8050c003;
	if ((workspace.envelope.header.format[0] << 8 | workspace.envelope.header.format[1]) != 3)
		return (int)0x8050c033;
	descriptorCount = workspace.envelope.header.descriptorCount[0] << 8 |
			  workspace.envelope.header.descriptorCount[1];
	keyDataSize = workspace.envelope.header.keyDataSize[0] << 8 | workspace.envelope.header.keyDataSize[1];
	keyCount = workspace.envelope.header.keyCount[0] << 8 | workspace.envelope.header.keyCount[1];
	if (descriptorCount <= 3 || keyDataSize <= 63 ||
	    keyDataSize > 16 * descriptorCount || keyCount > 32 ||
	    keyDataSize - 32 < 16 * keyCount)
		return (int)0x8050c003;
	memcpy(header, &workspace.envelope.header, sizeof(*header));
	payloadOffset = 16 * (descriptorCount + 1);
	installed = installUserVideoRecord(context, request->input);
	if (installed < 0)
		return installed;
	slot = SceMagicGateForDriver_FEF11D84(4);
	if (slot < 0) {
		SceMagicGateForDriver_144BFA76(installed);
		result = slot;
		goto error;
	}
	result = context->callbacks.unwrapDriverKey(slot, &header->wrappedDriverKey,
						    installed);
	SceMagicGateForDriver_144BFA76(installed);
	if (result < 0)
		goto slotError;
	size = keyring->size - 16;
	if (size > sizeof(workspace.payload)) {
		result = (int)0x8050c003;
		goto releaseSlot;
	}
	ksceKernelCopyFromUser(workspace.payload, (const uint8_t *)keyring->data + 16, size);
	result = SceMagicGateForDriver_05912BF5(&workspace.calculatedTag, workspace.payload, 0, size, slot);
	if (result < 0)
		goto slotError;
	if (timingsafe_memcmp(&workspace.calculatedTag, &workspace.selectedBlock, 16) != 0) {
		result = (int)0x8050c014;
		goto releaseSlot;
	}
	result = parseVideoProtectionInfo(context->parserDialect, request->keySystemId,
					workspace.payload + payloadOffset, size - payloadOffset);
	if (result >= 0)
		return slot;
	switch (result) {
	case -15: result = (int)0x8050c04e; break;
	case -8: result = (int)0x8050c047; break;
	case -7: result = (int)0x8050c046; break;
	case -6: result = (int)0x8050c045; break;
	case -5: result = (int)0x8050c044; break;
	case -4: result = (int)0x8050c043; break;
	case -3: result = (int)0x8050c042; break;
	case -2: result = (int)0x8050c041; break;
	case -1: result = (int)0x8050c003; break;
	}
	goto releaseSlot;
recordError:
	TRANSLATE_MAGICGATE_RESULT(result);
	SceProcessmgrForDriver_FC2A424E(copy);
	return result;
slotError:
	TRANSLATE_MAGICGATE_RESULT(result);
releaseSlot:
	SceMagicGateForDriver_144BFA76(slot);
	return result;
error:
	TRANSLATE_MAGICGATE_RESULT(result);
	return result;
}

int SceMagicGateForDriver_6A240950(void *output, const void *input,
				void *iv, unsigned int size, int slot);

int unwrapVideoRecordHeader(int (**unwrapKey)(int, const void *, int),
			    MgVideoCipherBlock *recordKey, uint64_t *recordTag,
			    uint64_t *headerTag,
			    const MgVideoWrappedRecordHeader *header,
			    int sourceSlot, int keySlot)
{
	int slot, result;

	slot = SceMagicGateForDriver_FEF11D84(0);
	if (slot >= 0) {
		result = (*unwrapKey)(slot, header, sourceSlot);
		if (result >= 0)
			result = SceMagicGateForDriver_6A240950(headerTag,
						&header->authenticatedData, 0, 32, slot);
		if (result >= 0)
			result = SceMagicGateForDriver_C9658A76(recordKey,
						&header->authenticatedData, 0, 32, slot);
		if (result >= 0) {
			if (keySlot < 0)
				result = SceMagicGateForDriver_CFD943E0(recordTag,
						&header->authenticatedData, 0, 16);
			else
				result = SceMagicGateForDriver_9CEF4965(recordTag,
						&header->authenticatedData, 0, 16,
						&zeroVideoBlock);
			if (result >= 0)
				return slot;
			switch (result) {
			case (int)0x80502000 ... (int)0x80502002:
			case (int)0x80504000 ... (int)0x80504002:
				result = 0x8050c005;
				break;
			case (int)0x80503001:
				result = 0x8050c032;
				break;
			case (int)0x80503002:
			case (int)0x80505005:
				result = 0x8050c012;
				break;
			case (int)0x80505001:
				result = 0x8050c021;
				break;
			case (int)0x80505002:
				result = 0x8050c022;
				break;
			case (int)0x80505003:
				result = 0x8050c024;
				break;
			case (int)0x80505004:
				result = 0x8050c023;
				break;
			case (int)0x80505006:
				result = 0x8050c025;
				break;
			case (int)0x80505007:
				result = 0x8050c026;
				break;
			case (int)0x80505009:
				result = 0x8050c027;
				break;
			}
		}
		SceMagicGateForDriver_144BFA76(slot);
		slot = result;
	}
	switch (slot) {
	case (int)0x80502000 ... (int)0x80502002:
	case (int)0x80504000 ... (int)0x80504002:
		return 0x8050c005;
	case (int)0x80503001:
		return 0x8050c032;
	case (int)0x80503002:
	case (int)0x80505005:
		return 0x8050c012;
	case (int)0x80505001:
		return 0x8050c021;
	case (int)0x80505002:
		return 0x8050c022;
	case (int)0x80505003:
		return 0x8050c024;
	case (int)0x80505004:
		return 0x8050c023;
	case (int)0x80505006:
		return 0x8050c025;
	case (int)0x80505007:
		return 0x8050c026;
	case (int)0x80505009:
		return 0x8050c027;
	}
	return slot;
}

int authenticateUserVideoRecord(void *argument)
{
	MgVideoAuthenticationRequest *request = argument;
	struct MgVideoAuthenticationWorkspace {
		MagicGateFileInfo fileInfo;
		MgVideoWrappedRecordHeader header;
		MgVideoCipherBlock blocks[16];
	} workspace __attribute__((aligned(16)));
	MgVideoContext *context = request->context;
	MgVideoCipherBlock *recordKey = request->recordKey;
	uint64_t *recordTag = request->recordTag;
	uint64_t *headerTag = request->headerTag;
	int sourceSlot = request->sourceSlot;
	const MgVideoCipherBlock *source = request->record->blocks;
	int remaining = 15984;
	unsigned int chunk = 256;
	int slot, result;

	ksceKernelCopyFromUser(&workspace.header, &request->record->header, 48);
	slot = unwrapVideoRecordHeader(&context->callbacks.unwrapKey, recordKey,
				       recordTag, headerTag, &workspace.header,
				       sourceSlot, context->contentKeySlot);
	if (slot < 0)
		return slot;
	result = context->callbacks.getFileInfo(context->fileHandle, &workspace.fileInfo);
	if (result < 0)
		goto error;
	if (timingsafe_memcmp(&workspace.header.authenticatedData.fileInfo,
			     &workspace.fileInfo, 16) != 0) {
		result = (int)0x8050c003;
		goto release;
	}
	for (;;) {
		ksceKernelCopyFromUser(workspace.blocks, source, chunk);
		result = SceMagicGateForDriver_C9658A76(recordKey, workspace.blocks,
						      recordKey, chunk, slot);
		if (result < 0)
			goto error;
		if (context->contentKeySlot < 0)
			result = SceMagicGateForDriver_CFD943E0(recordTag, workspace.blocks,
							      recordTag, chunk);
		else
			result = SceMagicGateForDriver_9CEF4965(recordTag, workspace.blocks,
							      recordTag, chunk, &zeroVideoBlock);
		if (result < 0)
			goto error;
		remaining -= chunk;
		source += chunk >> 4;
		if (remaining <= 0)
			break;
		chunk = remaining >= 256 ? 256 : remaining;
	}
	result = 0;
	goto release;
error:
	TRANSLATE_MAGICGATE_RESULT(result);
release:
	SceMagicGateForDriver_144BFA76(slot);
	return result;
}

int ksceKernelRunWithStack(unsigned int size, int (*func)(void *), void *arg);

int replaceUserVideoRecordBlock(MgVideoContext *context, int index,
			       int blockNumber, const MgVideoCipherBlock *replacement,
			       MgVideoUserRecord *record, const MgVideoUserBuffer *input)
{
	struct MgVideoRecordReplacementWorkspace {
		MagicGateFileInfo fileInfo;
		MgVideoCipherBlock recordKey;
		MgVideoWrappedRecordHeader header;
		MgVideoUserRecordHeader initialHeader;
	} workspace __attribute__((aligned(16)));
	MgVideoRecordUpdateRequest update;
	uint64_t headerTag, recordTag;
	int16_t selectedBlock = blockNumber;
	MgVideoCipherBlock *blocks;
	int installed, slot, result;

	if (memcmp(context->records.contentKeys[index], &zeroVideoBlock, 16) == 0) {
		memset(&workspace.initialHeader, 0, sizeof(workspace.initialHeader));
		ksceKernelCopyToUser(record->record.blocks, &workspace.initialHeader, 64);
		blocks = &record->record.blocks[4];
		do {
			ksceKernelCopyToUser(blocks,
					    &workspace.initialHeader, 64);
			ksceKernelCopyToUser(blocks + 4,
					    &workspace.initialHeader, 64);
			ksceKernelCopyToUser(blocks + 8,
					    &workspace.initialHeader, 64);
			ksceKernelCopyToUser(blocks + 12,
					    &workspace.initialHeader, 64);
			blocks += 16;
		} while (blocks != &record->record.blocks[996]);
		ksceKernelCopyToUser(blocks,
				    &workspace.initialHeader, 48);
		memcpy(&workspace.initialHeader.prefix.magic, videoRecordMagic, 4);
		result = SceMagicGateForDriver_E0888D78(
			&workspace.initialHeader.record.wrappedKey[0], &videoRandomChallenge);
		if (result < 0)
			goto error;
		result = SceMagicGateForDriver_E0888D78(
			&workspace.initialHeader.record.wrappedKey[1], &videoRandomChallenge);
		if (result < 0)
			goto error;
		if (memcmp(workspace.initialHeader.record.wrappedKey, &zeroVideoBlock, 16) == 0) {
			result = SceMagicGateForDriver_E0888D78(
				&workspace.initialHeader.record.wrappedKey[0], &videoRandomChallenge);
			if (result < 0)
				goto error;
		}
		result = context->callbacks.getFileInfo(context->fileHandle,
					&workspace.initialHeader.record.authenticatedData.fileInfo);
		if (result < 0)
			goto error;
		ksceKernelCopyToUser(record, &workspace.initialHeader, 64);
	}
	installed = installUserVideoRecord(context, input);
	if (installed < 0)
		return installed;
	ksceKernelCopyFromUser(&workspace.header, &record->record.header, 48);
	ksceKernelCopyFromUser(&workspace.header.authenticatedData.sequence, input->data, 4);
	slot = unwrapVideoRecordHeader(&context->callbacks.unwrapKey, &workspace.recordKey,
				       &recordTag, &headerTag, &workspace.header,
				       installed, context->contentKeySlot);
	if (slot < 0) {
		if (installed != -1)
			SceMagicGateForDriver_144BFA76(installed);
		return slot;
	}
	result = context->callbacks.getFileInfo(context->fileHandle, &workspace.fileInfo);
	if (result < 0) {
		TRANSLATE_MAGICGATE_RESULT(result);
		goto release;
	}
	if (timingsafe_memcmp(&workspace.header.authenticatedData.fileInfo,
			     &workspace.fileInfo, 16) != 0) {
		result = (int)0x8050c003;
		goto release;
	}
	result = verifyUserVideoRecord(context, index, &headerTag, input, installed);
	if (result < 0)
		goto release;
	update.context = context;
	update.index = index;
	update.userBlocks = record->record.blocks;
	update.blockNumbers = &selectedBlock;
	update.blockCount = 1;
	update.replacement = replacement;
	update.recordKey = &workspace.recordKey;
	update.recordTag = &recordTag;
	update.slot = slot;
	result = ksceKernelRunWithStack(1280, updateUserVideoRecord, &update);
	if (result >= 0) {
		result = 0;
		ksceKernelCopyToUser(&record->record.header.authenticatedData.sequence,
				    &workspace.header.authenticatedData.sequence, 4);
		memcpy(context->records.contentKeys[index], &workspace.recordKey, 16);
		context->records.recordHeaderTags[index] = headerTag;
		context->records.recordTags[index] = recordTag;
		ksceKernelCopyToUser(&record->encodedPrefix.stateMarker,
				    &videoRecordStateMarker, 2);
	}
release:
	SceMagicGateForDriver_144BFA76(slot);
	SceMagicGateForDriver_144BFA76(installed);
	return result;
error:
	TRANSLATE_MAGICGATE_RESULT(result);
	return result;
}

int SceMagicGateForDriver_A855C045(int slot, const void *input);

int generateVideoRecordKey(uint64_t output[2], const MgVideoKeySystemId *keySystemId,
			   unsigned int selector)
{
	MgVideoKeyGenerationInput input __attribute__((aligned(16)));
	int slot, result;

	if (keySystemId == 0) {
		result = SceMagicGateForDriver_E0888D78(&output[0], &videoRandomChallenge);
		if (result < 0)
			goto error;
		result = SceMagicGateForDriver_E0888D78(&output[1], &videoRandomChallenge);
		if (result < 0)
			goto error;
		return 0;
	}
	input.keySystemId = *keySystemId;
	input.zeroWord = 0;
	input.selector = selector;
	slot = SceMagicGateForDriver_FEF11D84(4);
	if (slot < 0) {
		result = slot;
		goto error;
	}
	result = SceMagicGateForDriver_A855C045(slot, &zeroVideoBlock);
	if (result >= 0) {
		result = SceMagicGateForDriver_05912BF5(output, &input, 0, sizeof(input), slot);
		if (result >= 0) {
			SceMagicGateForDriver_144BFA76(slot);
			return 0;
		}
	}
	SceMagicGateForDriver_144BFA76(slot);
error:
	TRANSLATE_MAGICGATE_RESULT(result);
	return result;
}

int SceMagicGateForDriver_ACA0F529(int slot, const void *input, int sourceSlot);

typedef struct MgVideoCounterBlock {
	uint64_t prefix;
	uint32_t zeroWord;
	uint32_t encodedIndex;
} MgVideoCounterBlock;

typedef struct MgVideoStreamCipherWorkspace {
	MagicGateFileInfo fileInfo;
	MgVideoCipherBlock iv;
	MgVideoCounterBlock counter;
} MgVideoStreamCipherWorkspace;

_Static_assert(sizeof(MgVideoCounterBlock) == 16, "counter block size");
_Static_assert(sizeof(MgVideoStreamCipherWorkspace) == 48, "stream cipher workspace size");

int cipherUserVideoStream(unsigned int handle, void *output, const void *input,
			 unsigned int size, unsigned int totalSize,
			 unsigned int *processed, const uint32_t *blockIndex)
{
	MgVideoStreamCipherWorkspace workspace __attribute__((aligned(16)));
	MgVideoRecordStream *stream = 0;
	MgVideoContext *context;
	MgVideoCipherChannel *channel;
	const MgVideoCipherBlock *iv;
	const void *wrappedKey;
	unsigned int consumed, counterIndex, decodedIndex;
	int encrypt, finalChunk, slot, result;

	if (videoReferences <= 0) {
		result = (int)0x8050c011;
		goto error;
	}
	if ((handle & 0x80000000U) || !output || !input || size > totalSize) {
		result = (int)0x8050c002;
		goto error;
	}
	encrypt = blockIndex != 0;
	if (blockIndex)
		counterIndex = *blockIndex;
	consumed = processed ? *processed : 0;
	finalChunk = totalSize == consumed + size;
	if (consumed == 0 && size <= 16) {
		result = (int)0x8050c002;
		goto error;
	}
	if (!(handle & 0x40000) || ((handle >> 8) & 255) > 1) {
		result = (int)0x8050c002;
		goto error;
	}
	stream = &recordStreams[(handle >> 8) & 255];
	if (!stream->active || !stream->context) {
		stream = 0;
		result = (int)0x8050c002;
		goto error;
	}
	if (stream->context->state <= 1) {
		stream = 0;
		result = (int)0x8050c012;
		goto error;
	}
	if ((handle & 255) > 3) {
		stream = 0;
		result = (int)0x8050c002;
		goto error;
	}
	result = stream->contentKeySlot;
	if (result < 0)
		goto error;
	channel = &stream->channels[handle & 255];
	wrappedKey = channel->key;
	if ((uintptr_t)wrappedKey - 1 > 0xfffffffdU) {
		result = (int)0x8050c002;
		goto error;
	}
	if (channel->counterKey) {
		workspace.counter.prefix = channel->counterKey->counterPrefix;
		workspace.counter.zeroWord = 0;
		if (encrypt) {
			if (consumed == 0) {
				iv = &channel->initialIv;
				channel->currentBlock = counterIndex;
			} else {
				if ((consumed & 15) || channel->currentBlock != counterIndex) {
					result = (int)0x8050c002;
					goto error;
				}
				iv = &channel->currentIv;
			}
			workspace.counter.encodedIndex = __builtin_bswap32(counterIndex);
		} else {
			if (consumed) {
				if (consumed <= 3 || (consumed & 15) != 4) {
					result = (int)0x8050c002;
					goto error;
				}
				counterIndex = channel->currentBlock;
				iv = &channel->currentIv;
			} else {
				ksceKernelCopyFromUser(&counterIndex, input, 4);
				input = (const uint8_t *)input + 4;
				size -= 4;
				consumed = 4;
				iv = &channel->initialIv;
			}
			workspace.counter.encodedIndex = counterIndex;
		}
		decodedIndex = __builtin_bswap32(workspace.counter.encodedIndex);
		if (decodedIndex < channel->firstBlock || decodedIndex >= channel->endBlock) {
			result = (int)0x8050c003;
			goto error;
		}
		wrappedKey = &workspace.counter;
	} else {
		if (encrypt) {
			result = (int)0x8050c033;
			goto error;
		}
		iv = &channel->currentIv;
		if (consumed) {
			if (consumed & 15) {
				result = (int)0x8050c002;
				goto error;
			}
		} else {
			ksceKernelCopyFromUser(&channel->currentIv, input, 16);
			input = (const uint8_t *)input + 16;
			size -= 16;
			consumed = 16;
			wrappedKey = channel->key;
		}
	}
	slot = SceMagicGateForDriver_FEF11D84(5);
	if (slot < 0) {
		result = slot;
		TRANSLATE_MAGICGATE_RESULT(result);
		goto error;
	}
	SceKernelSuspendForDriver_4DF40893(0);
	context = stream->context;
	if (!(context->modeFlags & 32)) {
		result = context->callbacks.getFileInfo(context->fileHandle, &workspace.fileInfo);
		if (result < 0) {
			TRANSLATE_MAGICGATE_RESULT(result);
			goto unlock;
		}
	}
	result = SceMagicGateForDriver_ACA0F529(slot, wrappedKey, stream->contentKeySlot);
	if (result < 0) {
		SceKernelSuspendForDriver_2BB92967(0);
		TRANSLATE_MAGICGATE_RESULT(result);
		goto release;
	}
	if (!finalChunk)
		size &= ~15U;
	workspace.iv = *iv;
	if (encrypt) {
		if (finalChunk) {
			result = encryptPaddedUserVideo(output, input, size, slot, &workspace.iv);
			if (result < 0)
				goto unlock;
			goto final;
		}
		result = encryptUserVideoBlocks(output, input, size, slot, &workspace.iv);
	} else {
		if (finalChunk) {
			result = decryptUserVideoBlocks(output, input, size, slot, &workspace.iv);
			if (result < 0)
				goto unlock;
			if (result > 16) {
				result = (int)0x8050c003;
				goto unlock;
			}
			result = size - result;
			if (result < 0)
				goto unlock;
			goto final;
		}
		result = decryptUserVideoBlocks(output, input, size, slot, &workspace.iv);
	}
	if (result < 0)
		goto unlock;
	result = size;
	SceKernelSuspendForDriver_2BB92967(0);
	channel->currentIv = workspace.iv;
	/* The original also stores this indeterminate word in non-counter mode. */
	channel->currentBlock = counterIndex;
	goto processed;
final:
	SceKernelSuspendForDriver_2BB92967(0);
processed:
	if (processed)
		*processed = size + consumed;
	goto release;
unlock:
	SceKernelSuspendForDriver_2BB92967(0);
release:
	SceMagicGateForDriver_144BFA76(slot);
error:
	if ((unsigned int)result - 0x8050c021U <= 1 && stream && stream->context)
		stream->context->state = 1;
	return result;
}

int cipherUserVideoBuffer(unsigned int handle, void *output, const void *input,
			 unsigned int size, const uint32_t *blockIndex)
{
	MgVideoStreamCipherWorkspace workspace __attribute__((aligned(16)));
	MgVideoRecordStream *stream = 0;
	MgVideoCipherChannel *channel;
	MgVideoContext *context;
	const void *wrappedKey;
	unsigned int decodedIndex, counterIndex;
	int encrypt, slot, result;

	if (videoReferences <= 0) {
		result = (int)0x8050c011;
		goto error;
	}
	if ((handle & 0x80000000U) || !output || !input ||
	    !(handle & 0x40000) || ((handle >> 8) & 255) > 1) {
		result = (int)0x8050c002;
		goto error;
	}
	stream = &recordStreams[(handle >> 8) & 255];
	if (!stream->active || !stream->context) {
		result = (int)0x8050c002;
		stream = 0;
		goto error;
	}
	if (stream->context->state <= 1) {
		result = (int)0x8050c012;
		stream = 0;
		goto error;
	}
	if ((handle & 255) > 3) {
		result = (int)0x8050c002;
		stream = 0;
		goto error;
	}
	result = stream->contentKeySlot;
	if (result < 0)
		goto error;
	channel = &stream->channels[handle & 255];
	wrappedKey = channel->key;
	if ((uintptr_t)wrappedKey - 1 > 0xfffffffdU) {
		result = (int)0x8050c002;
		goto error;
	}
	encrypt = blockIndex != 0;
	if (blockIndex)
		counterIndex = *blockIndex;
	if (channel->counterKey) {
		workspace.counter.prefix = channel->counterKey->counterPrefix;
		workspace.counter.zeroWord = 0;
		if (encrypt) {
			workspace.counter.encodedIndex = __builtin_bswap32(counterIndex);
		} else {
			if (size <= 19) {
				result = (int)0x8050c002;
				goto error;
			}
			ksceKernelCopyFromUser(&workspace.counter.encodedIndex, input, 4);
			input = (const uint8_t *)input + 4;
			size -= 4;
		}
		decodedIndex = __builtin_bswap32(workspace.counter.encodedIndex);
		if (decodedIndex < channel->firstBlock || decodedIndex >= channel->endBlock) {
			result = (int)0x8050c003;
			goto error;
		}
		wrappedKey = &workspace.counter;
		workspace.iv = channel->initialIv;
	} else {
		if (encrypt) {
			result = (int)0x8050c033;
			goto error;
		}
		if (size <= 31) {
			result = (int)0x8050c002;
			goto error;
		}
		ksceKernelCopyFromUser(&workspace.iv, input, 16);
		input = (const uint8_t *)input + 16;
		size -= 16;
	}
	if (!encrypt && (size & 15)) {
		result = (int)0x8050c002;
		goto error;
	}
	slot = SceMagicGateForDriver_FEF11D84(5);
	if (slot < 0) {
		result = slot;
		TRANSLATE_MAGICGATE_RESULT(result);
		goto error;
	}
	SceKernelSuspendForDriver_4DF40893(0);
	context = stream->context;
	if (!(context->modeFlags & 32)) {
		result = context->callbacks.getFileInfo(context->fileHandle, &workspace.fileInfo);
		if (result < 0) {
			TRANSLATE_MAGICGATE_RESULT(result);
			goto unlock;
		}
	}
	result = SceMagicGateForDriver_ACA0F529(slot, wrappedKey, stream->contentKeySlot);
	if (result < 0) {
		SceKernelSuspendForDriver_2BB92967(0);
		TRANSLATE_MAGICGATE_RESULT(result);
		goto release;
	}
	if (encrypt) {
		result = encryptPaddedUserVideo(output, input, size, slot, &workspace.iv);
	} else {
		result = decryptUserVideoBlocks(output, input, size, slot, &workspace.iv);
		if (result >= 0) {
			if (result <= 16)
				result = size - result;
			else
				result = (int)0x8050c003;
		}
	}
unlock:
	SceKernelSuspendForDriver_2BB92967(0);
release:
	SceMagicGateForDriver_144BFA76(slot);
error:
	if ((unsigned int)result - 0x8050c021U <= 1 && stream && stream->context)
		stream->context->state = 1;
	return result;
}

int authorizeUserVideoRecord(int handle, unsigned int index,
			    const MgVideoUserBuffer *recordInput,
			    const MgVideoUserBuffer *input)
{
	MgVideoCipherBlock recordKey __attribute__((aligned(16)));
	MgVideoAuthenticationRequest request;
	MgVideoUserBuffer body;
	MgVideoContext *context;
	const MgVideoUserRecord *record;
	uint64_t recordTag, headerTag;
	uint32_t magic, recordIdentity;
	int slot, result;

	if (videoReferences <= 0)
		return (int)0x8050c011;
	if (handle < 0 || !(handle & 0x10000))
		return (int)0x8050c002;
	context = videoContexts[(uint16_t)handle];
	if ((uint16_t)handle > 1 || context->references <= 0)
		return (int)0x8050c002;
	if (context->state <= 1)
		return (int)0x8050c012;
	if (context->recordErrorFlags & 1)
		return (int)0x8050c016;
	if (index > 31)
		return (int)0x8050c002;
	record = recordInput ? recordInput->data : 0;
	if (!record) {
		recordKey = (MgVideoCipherBlock) { { 0, 0, 0, 0 } };
		slot = -1;
		goto compare;
	}
	if (recordInput->size != sizeof(*record))
		return (int)0x8050c003;
	if (!input)
		return (int)0x8050c002;
	body = *input;
	if (!body.data)
		return (int)0x8050c002;
	if (body.size <= 79)
		return (int)0x8050c031;
	if (ksceKernelCopyFromUser(&magic, body.data, sizeof(magic)) < 0)
		return (int)0x8050c002;
	if (magic != 0x32424b45U)
		return (int)0x8050c031;
	body.data = (const uint8_t *)body.data +
		    offsetof(MagicGateBundledEkb2Record, header);
	body.size -= offsetof(MagicGateBundledEkb2Record, header);
	ksceKernelCopyFromUser(&recordIdentity,
			      &record->record.header.authenticatedData.fileInfo,
			      sizeof(recordIdentity));
	magic = 0;
	ksceKernelCopyFromUser(&magic, body.data, sizeof(magic));
	if (__builtin_bswap32(magic) != __builtin_bswap32(recordIdentity))
		return (int)0x8050c002;
	slot = installUserVideoRecord(context, &body);
	if (slot < 0) {
		result = slot;
		goto reset;
	}
	request.context = context;
	request.recordKey = &recordKey;
	request.recordTag = &recordTag;
	request.headerTag = &headerTag;
	request.record = &record->record;
	request.sourceSlot = slot;
	result = ksceKernelRunWithStack(1280, authenticateUserVideoRecord, &request);
	if (result < 0)
		goto release;
compare:
	if (timingsafe_memcmp(&recordKey, context->records.contentKeys[index], 16) != 0) {
		result = (int)0x8050c014;
	} else {
		if (record) {
			context->records.recordHeaderTags[index] = headerTag;
			context->records.recordTags[index] = recordTag;
		} else {
			context->records.recordHeaderTags[index] = 0;
			context->records.recordTags[index] = 0;
		}
		context->state = 3;
		context->validRecordMask |= 1U << index;
		result = 0;
	}
	if (slot == -1)
		return result;
release:
	SceMagicGateForDriver_144BFA76(slot);
reset:
	if ((unsigned int)(result - (int)0x8050c021) <= 1 && context)
		context->state = 1;
	return result;
}

int openVideoRecordStream(int handle, unsigned int index, int blockNumber,
			  const MgVideoUserBuffer *keyring,
			  const MgVideoUserBuffer *recordInput,
			  const MgVideoUserBuffer *input)
{
	MgVideoKeyringHeader header __attribute__((aligned(16)));
	MgVideoKeySystemId keySystemId;
	MgVideoKeyringRequest request;
	MgVideoUserBuffer keyringBuffer, inputBuffer;
	MgVideoContext *context;
	MgVideoRecordStream *stream;
	const MgVideoUserRecord *record;
	uint32_t magic;
	int processId, intr, owner, result;

	if (videoReferences <= 0)
		return (int)0x8050c011;
	if (handle < 0 || !(handle & 0x10000))
		return (int)0x8050c002;
	context = videoContexts[(uint16_t)handle];
	if ((uint16_t)handle > 1 || context->references <= 0)
		return (int)0x8050c002;
	if (context->state <= 1)
		return (int)0x8050c012;
	if (context->state == 2)
		return (int)0x8050c013;
	if (index > 31 || (unsigned int)blockNumber - 1 > 998)
		return (int)0x8050c002;
	if (!keyring)
		return (int)0x8050c002;
	keyringBuffer = *keyring;
	if (!keyringBuffer.data)
		return (int)0x8050c002;
	if (keyringBuffer.size <= 79)
		return (int)0x8050c003;
	if (!recordInput)
		return (int)0x8050c002;
	record = recordInput->data;
	if (!record)
		return (int)0x8050c002;
	if (recordInput->size != sizeof(*record))
		return (int)0x8050c003;
	if (!input)
		return (int)0x8050c002;
	inputBuffer = *input;
	if (!inputBuffer.data)
		return (int)0x8050c002;
	if (inputBuffer.size <= 79)
		return (int)0x8050c031;
	if (ksceKernelCopyFromUser(&magic, inputBuffer.data, sizeof(magic)) < 0)
		return (int)0x8050c002;
	if (magic != 0x32424b45U)
		return (int)0x8050c031;
	inputBuffer.data = (const uint8_t *)inputBuffer.data +
			   offsetof(MagicGateBundledEkb2Record, header);
	inputBuffer.size -= offsetof(MagicGateBundledEkb2Record, header);
	processId = sceKernelGetProcessId();
	intr = ksceKernelCpuSuspendIntr();
	if (!recordStreams[0].active) {
		owner = 0;
	} else if (!recordStreams[1].active) {
		owner = 1;
	} else {
		ksceKernelCpuResumeIntr(intr);
		return (int)0x8050c015;
	}
	recordStreams[owner].processId = processId & ~(processId >> 31);
	recordStreams[owner].active = 1;
	recordStreams[owner].context = context;
	ksceKernelCpuResumeIntr(intr);
	stream = recordStreams[owner].active && recordStreams[owner].context ?
		 &recordStreams[owner] : 0;
	request.context = context;
	request.header = &header;
	request.keySystemId = &keySystemId;
	request.index = index;
	request.blockNumber = blockNumber;
	request.keyring = &keyringBuffer;
	request.record = record;
	request.input = &inputBuffer;
	result = ksceKernelRunWithStack(5680, openUserVideoKeyring, &request);
	if (result < 0) {
		if (recordStreams[owner].contentKeySlot >= 0) {
			SceMagicGateForDriver_144BFA76(recordStreams[owner].contentKeySlot);
			recordStreams[owner].contentKeySlot = -1;
		}
		recordStreams[owner].processId = -1;
		recordStreams[owner].active = 0;
		if ((unsigned int)result - 0x8050c021U <= 1 && context)
			context->state = 1;
		return result;
	}
	stream->keys[0] = header.keys[0];
	stream->contentKeySlot = result;
	stream->keyCount = (unsigned int)header.keyCount[0] << 8 | header.keyCount[1];
	stream->keys[1] = header.keys[1];
	stream->keySystemId = keySystemId;
	return 0x20000 | owner;
}

int closeVideoRecordStream(int handle)
{
	MgVideoRecordStream *stream;
	unsigned int owner;

	if (videoReferences <= 0 || !(handle & 0x20000))
		return 0;
	owner = (uint16_t)handle;
	if (owner > 1)
		return 0;
	stream = &recordStreams[owner];
	if (!stream->active || !stream->context)
		return 0;
	if (stream->contentKeySlot >= 0) {
		SceMagicGateForDriver_144BFA76(stream->contentKeySlot);
		stream->contentKeySlot = -1;
	}
	recordStreams[owner].active = 0;
	recordStreams[owner].processId = -1;
	return 0;
}

#define VIDEO_BOX_SIZE(box) ((uint32_t)(box)->size[0] << 24 | \
	(uint32_t)(box)->size[1] << 16 | (uint32_t)(box)->size[2] << 8 | (box)->size[3])

int openVideoCipherChannel(int handle, const MgVideoBoxHeader *input, int size)
{
	MgVideoKeySystemId keySystemId;
	MgVideoRecordStream *stream;
	MgVideoCipherChannel *channel;
	const MgVideoBoxHeader *box, *next, *information = 0;
	const MgVideoSchemeTypeBox *scheme = 0;
	const MgVideoKeySystemIdBox *keySystem = 0;
	const MgVideoKeyIdBox *key = 0;
	const MgVideoEncryptionAlgorithmBox *encryption = 0;
	uintptr_t end;
	unsigned int boxSize, algorithm = 0, keyIndex, owner;
	int channelIndex = -1, intr, result;

	if (videoReferences <= 0) {
		result = (int)0x8050c011;
		goto error;
	}
	owner = (uint16_t)handle;
	if (!(handle & 0x20000) || owner > 1) {
		result = (int)0x8050c002;
		goto error;
	}
	stream = &recordStreams[owner];
	if (!stream->active || !stream->context) {
		result = (int)0x8050c002;
		goto error;
	}
	if (stream->context->state <= 1) {
		result = (int)0x8050c012;
		goto error;
	}
	if (!input) {
		result = (int)0x8050c002;
		goto error;
	}
	boxSize = VIDEO_BOX_SIZE(input);
	if (size < (int)boxSize)
		return 0;
	end = (uintptr_t)input + boxSize;
	box = input + 1;
	if (end <= (uintptr_t)box)
		goto malformed;
	boxSize = VIDEO_BOX_SIZE(box);
	next = (const MgVideoBoxHeader *)((const uint8_t *)box + boxSize);
	if ((uintptr_t)next > end || !boxSize)
		goto malformed;
	for (;;) {
		if (boxSize > 11 && memcmp(box->type, "schm", 4) == 0 &&
		    !((const MgVideoSchemeTypeBox *)box)->version) {
			if (scheme || boxSize <= 17)
				goto malformed;
			scheme = (const MgVideoSchemeTypeBox *)box;
		} else if (boxSize > 7 && memcmp(box->type, "schi", 4) == 0) {
			information = box;
			break;
		}
		if (end <= (uintptr_t)next)
			break;
		boxSize = VIDEO_BOX_SIZE(next);
		if (end < (uintptr_t)next + boxSize || !boxSize)
			break;
		box = next;
		next = (const MgVideoBoxHeader *)((const uint8_t *)next + boxSize);
	}
	if (!scheme || !information)
		goto malformed;
	if (memcmp(scheme->schemeType, "prot", 4) != 0 ||
	    ((unsigned int)scheme->schemeVersion[0] << 8 | scheme->schemeVersion[1]) > 256) {
		result = (int)0x8050c033;
		goto error;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (!stream->channels[0].key)
		channelIndex = 0;
	else if (!stream->channels[1].key)
		channelIndex = 1;
	else if (!stream->channels[2].key)
		channelIndex = 2;
	else if (!stream->channels[3].key)
		channelIndex = 3;
	else {
		ksceKernelCpuResumeIntr(intr);
		result = (int)0x8050c015;
		goto error;
	}
	channel = &stream->channels[channelIndex];
	channel->key = (MgVideoStreamKey *)-1;
	ksceKernelCpuResumeIntr(intr);
	end = (uintptr_t)information + VIDEO_BOX_SIZE(information);
	box = information + 1;
	if (end <= (uintptr_t)box)
		goto malformed;
	boxSize = VIDEO_BOX_SIZE(box);
	next = (const MgVideoBoxHeader *)((const uint8_t *)box + boxSize);
	if (end < (uintptr_t)next || !boxSize)
		goto malformed;
	for (;;) {
		if (!keySystem) {
			const MgVideoKeySystemIdBox *candidate = (const MgVideoKeySystemIdBox *)box;

			if (boxSize > 11 && memcmp(box->type, "ksid", 4) == 0 && !candidate->version) {
				if (boxSize <= 35)
					goto malformed;
				keySystem = candidate;
			} else {
				goto next_box;
			}
		}
		if (!key) {
			const MgVideoKeyIdBox *candidate = (const MgVideoKeyIdBox *)box;

			if (boxSize > 11 && memcmp(box->type, "kyid", 4) == 0 && !candidate->version) {
				if (boxSize <= 13)
					goto malformed;
				key = candidate;
			} else {
				goto next_box;
			}
		}
		if (!encryption) {
			const MgVideoEncryptionAlgorithmBox *candidate =
				(const MgVideoEncryptionAlgorithmBox *)box;

			if (boxSize > 11 && memcmp(box->type, "ealg", 4) == 0 && !candidate->version) {
				if (boxSize <= 15)
					goto malformed;
				algorithm = (uint32_t)candidate->algorithm[0] << 24 |
					    (uint32_t)candidate->algorithm[1] << 16 |
					    (uint32_t)candidate->algorithm[2] << 8 |
					    candidate->algorithm[3];
				if (algorithm == 8 && boxSize - 16 <= 15)
					goto malformed;
				encryption = candidate;
			}
		}
next_box:
		if (end <= (uintptr_t)next)
			break;
		boxSize = VIDEO_BOX_SIZE(next);
		if (end < (uintptr_t)next + boxSize || !boxSize)
			break;
		box = next;
		next = (const MgVideoBoxHeader *)((const uint8_t *)next + boxSize);
	}
	if (!keySystem || !key || !encryption)
		goto malformed;
	keySystemId = keySystem->keySystemId;
	channel->initialIv = encryption->initializationVector;
	keyIndex = (unsigned int)key->keyId[0] << 8 | key->keyId[1];
	if (memcmp(&keySystemId, &stream->keySystemId, sizeof(keySystemId)) != 0 ||
	    (int16_t)stream->keyCount <= (int)keyIndex)
		goto malformed;
	if (keyIndex > 1) {
		result = (int)0x8050c033;
		goto error;
	}
	channel->keySelection = keyIndex == 1 ? 2 : 1;
	if (algorithm == 8) {
		channel->counterKey = &stream->keys[keyIndex];
		channel->firstBlock = (uint32_t)channel->counterKey->firstBlock[0] << 24 |
			(uint32_t)channel->counterKey->firstBlock[1] << 16 |
			(uint32_t)channel->counterKey->firstBlock[2] << 8 |
			channel->counterKey->firstBlock[3];
		channel->endBlock = (uint32_t)channel->counterKey->endBlock[0] << 24 |
			(uint32_t)channel->counterKey->endBlock[1] << 16 |
			(uint32_t)channel->counterKey->endBlock[2] << 8 |
			channel->counterKey->endBlock[3];
	} else if (algorithm == 9) {
		channel->counterKey = 0;
	} else {
		result = (int)0x8050c033;
		goto error;
	}
	channel->key = &stream->keys[keyIndex];
	return 0x40000 | owner << 8 | channelIndex;
malformed:
	result = (int)0x8050c003;
error:
	if (handle >= 0 && (handle & 0x20000) && (uint16_t)handle <= 1 &&
	    recordStreams[(uint16_t)handle].active &&
	    recordStreams[(uint16_t)handle].context && channelIndex >= 0) {
		channel = &recordStreams[(uint16_t)handle].channels[channelIndex];
		if (channel->key)
			channel->key = 0;
	}
	return result;
}

#undef VIDEO_BOX_SIZE

int closeVideoCipherChannel(int handle)
{
	MgVideoRecordStream *stream;
	MgVideoCipherChannel *channel;
	unsigned int owner;

	if (videoReferences <= 0 || !(handle & 0x40000))
		return 0;
	owner = (handle >> 8) & 255;
	if (owner > 1)
		return 0;
	stream = &recordStreams[owner];
	if (!stream->active || !stream->context || (handle & 255) > 3)
		return 0;
	channel = &stream->channels[handle & 255];
	if (channel->key)
		channel->key = 0;
	return 0;
}

int isVideoCipherKeySelected(int handle, unsigned int selection)
{
	MgVideoRecordStream *stream;
	MgVideoContext *context;
	unsigned int owner, channel;
	int result;

	if (videoReferences <= 0)
		return (int)0x8050c011;
	if (handle < 0 || !(handle & 0x40000))
		return (int)0x8050c002;
	owner = (handle >> 8) & 255;
	if (owner > 1)
		return (int)0x8050c002;
	stream = &recordStreams[owner];
	if (!stream->active || !stream->context)
		return (int)0x8050c002;
	context = stream->context;
	if (!context)
		return (int)0x8050c002;
	if (context->state <= 1)
		return (int)0x8050c012;
	channel = handle & 255;
	if (channel > 3)
		return (int)0x8050c002;
	result = stream->contentKeySlot;
	if (result >= 0)
		return stream->channels[channel].keySelection == selection;
	return result;
}

int decryptVideoCipherBuffer(unsigned int handle, void *output,
			    const void *input, unsigned int size)
{
	return cipherUserVideoBuffer(handle, output, input, size, 0);
}

int decryptVideoCipherStream(unsigned int handle, void *output,
			    const void *input, unsigned int size,
			    unsigned int totalSize, unsigned int *processed)
{
	return cipherUserVideoStream(handle, output, input, size, totalSize, processed, 0);
}

int encryptVideoCipherBuffer(unsigned int handle, void *output,
			    const void *input, unsigned int size,
			    uint32_t blockIndex)
{
	return cipherUserVideoBuffer(handle, output, input, size, &blockIndex);
}

int encryptVideoCipherStream(unsigned int handle, void *output,
			    const void *input, unsigned int size,
			    unsigned int totalSize, unsigned int *processed,
			    uint32_t blockIndex)
{
	return cipherUserVideoStream(handle, output, input, size, totalSize,
				    processed, &blockIndex);
}

int removeUserVideoRecordBlocks(int handle, unsigned int index,
			       const MgVideoUserBuffer *recordInput,
			       const int16_t *blockNumbers, int blockCount,
			       const MgVideoUserBuffer *input)
{
	struct MgVideoRemoveWorkspace {
		MagicGateFileInfo fileInfo;
		MgVideoCipherBlock recordKey;
		MgVideoWrappedRecordHeader header;
	} workspace __attribute__((aligned(16)));
	MgVideoRecordUpdateRequest request;
	MgVideoUserBuffer body;
	MgVideoContext *context;
	MgVideoUserRecord *record;
	uint64_t recordTag, headerTag;
	uint32_t magic;
	int result, installed, slot;

	if (videoReferences <= 0)
		return (int)0x8050c011;
	if (handle < 0 || !(handle & 0x10000))
		return (int)0x8050c002;
	context = videoContexts[(uint16_t)handle];
	if ((uint16_t)handle > 1 || context->references <= 0)
		return (int)0x8050c002;
	if (!context->state)
		return (int)0x8050c011;
	if (index > 31)
		return (int)0x8050c002;
	if (blockCount == -1) {
		context->records.recordHeaderTags[index] = 0;
		context->records.recordTags[index] = 0;
		memset(context->records.contentKeys[index], 0, 16);
		context->state = 3;
		context->validRecordMask |= 1U << index;
		return 0;
	}
	if (blockCount < 0)
		return (int)0x8050c002;
	if (context->state <= 2 || !(context->validRecordMask & (1U << index)))
		return (int)0x8050c013;
	if (!recordInput)
		return (int)0x8050c002;
	record = (MgVideoUserRecord *)recordInput->data;
	if (!record)
		return (int)0x8050c002;
	if (recordInput->size != sizeof(*record))
		return (int)0x8050c003;
	if (blockCount && !blockNumbers)
		return (int)0x8050c002;
	if (!input) {
		body = (MgVideoUserBuffer) { 0, 0 };
		return (int)0x8050c002;
	}
	body = *input;
	if (!body.data)
		return (int)0x8050c002;
	if (body.size <= 79)
		return (int)0x8050c031;
	if (ksceKernelCopyFromUser(&magic, body.data, sizeof(magic)) < 0)
		return (int)0x8050c002;
	if (magic != 0x32424b45U)
		return (int)0x8050c031;
	body.data = (const uint8_t *)body.data +
		    offsetof(MagicGateBundledEkb2Record, header);
	body.size -= offsetof(MagicGateBundledEkb2Record, header);
	installed = installUserVideoRecord(context, &body);
	if (installed < 0) {
		result = installed;
		goto reset;
	}
	ksceKernelCopyFromUser(&workspace.header, &record->record.header, 48);
	ksceKernelCopyFromUser(&workspace.header.authenticatedData.sequence, body.data, 4);
	slot = unwrapVideoRecordHeader(&context->callbacks.unwrapKey, &workspace.recordKey,
				       &recordTag, &headerTag, &workspace.header,
				       installed, context->contentKeySlot);
	if (slot < 0) {
		result = slot;
		if (installed == -1)
			goto reset;
		goto release_installed;
	}
	result = context->callbacks.getFileInfo(context->fileHandle, &workspace.fileInfo);
	if (result < 0) {
		TRANSLATE_MAGICGATE_RESULT(result);
		goto release;
	}
	if (timingsafe_memcmp(&workspace.header.authenticatedData.fileInfo,
			     &workspace.fileInfo, 16) != 0) {
		result = (int)0x8050c003;
		goto release;
	}
	result = verifyUserVideoRecord(context, index, &headerTag, &body, installed);
	if (result < 0)
		goto release;
	request.context = context;
	request.index = index;
	request.userBlocks = record->record.blocks;
	request.blockNumbers = blockNumbers;
	request.blockCount = blockCount;
	request.replacement = &zeroVideoBlock;
	request.recordKey = &workspace.recordKey;
	request.recordTag = &recordTag;
	request.slot = slot;
	result = ksceKernelRunWithStack(1280, updateUserVideoRecord, &request);
	if (result >= 0) {
		ksceKernelCopyToUser(&record->record.header.authenticatedData.sequence,
				    &workspace.header.authenticatedData.sequence, 4);
		context->records.recordHeaderTags[index] = headerTag;
		memcpy(context->records.contentKeys[index], &workspace.recordKey, 16);
		context->records.recordTags[index] = recordTag;
		ksceKernelCopyToUser(&record->encodedPrefix.stateMarker,
				    &videoRecordStateMarker, 2);
		result = 0;
	}
release:
	SceMagicGateForDriver_144BFA76(slot);
release_installed:
	SceMagicGateForDriver_144BFA76(installed);
reset:
	if ((unsigned int)result - 0x8050c021U <= 1 && context)
		context->state = 1;
	return result;
}

int getUserVideoPlaybackInfo(int handle, void *output, unsigned int outputSize,
			    const MgVideoUserBuffer *input, int mode)
{
	MgVideoPlaybackQueryWorkspace workspace __attribute__((aligned(16)));
	MgVideoContext *context;
	void *copy;
	unsigned int size;
	int result;

	if (videoReferences <= 0)
		return (int)0x8050c011;
	if (handle < 0 || !(handle & 0x10000))
		return (int)0x8050c002;
	context = videoContexts[(uint16_t)handle];
	if ((uint16_t)handle > 1 || context->references <= 0)
		return (int)0x8050c002;
	if (!context->state)
		return (int)0x8050c011;
	if (!output || !input || !input->data)
		return (int)0x8050c002;
	if (input->size <= 79)
		return (int)0x8050c003;
	size = input->size - sizeof(MgVideoRecordPrefix);
	if (size > 21056)
		return (int)0x8050c051;
	copy = SceProcessmgrForDriver_5468892A(size, 0);
	if (!copy)
		return (int)0x8050c004;
	ksceKernelCopyFromUser(copy, (const uint8_t *)input->data +
			      sizeof(MgVideoRecordPrefix), size);
	if (mode == 1 && outputSize > 63) {
		result = parseVideoPlaybackInfo(context->parserDialect, 1, &workspace, copy, size);
		if (result >= 0) {
			MgVideoPlaybackPeriodInfo *period = output;

			memset(output, 0, outputSize);
			period->currentTick = workspace.period.currentTick;
			period->encodedAvOut = workspace.period.encodedAvOut;
			period->startTick = workspace.period.startTick;
			period->periodSeconds = workspace.period.periodSeconds;
			period->endTick = workspace.period.endTick;
			period->periodStartTick = workspace.period.periodStartTick;
			period->periodEndTick = workspace.period.periodEndTick;
			result = 0;
		} else {
			goto translate;
		}
	} else if (mode == 2 && outputSize > 19) {
		result = parseVideoPlaybackInfo(context->parserDialect, 2, &workspace, copy, size);
		if (result >= 0) {
			*(MgVideoPlaybackQueryResult *)output = workspace.query;
			result = 0;
		} else {
			goto translate;
		}
	} else {
		result = (int)0x8050c002;
	}
	goto release;
translate:
	switch (result) {
	case -15:
		result = (int)0x8050c04e;
		break;
	case -8:
		result = (int)0x8050c047;
		break;
	case -7:
		result = (int)0x8050c046;
		break;
	case -6:
		result = (int)0x8050c045;
		break;
	case -5:
		result = (int)0x8050c044;
		break;
	case -4:
		result = (int)0x8050c043;
		break;
	case -3:
		result = (int)0x8050c042;
		break;
	case -2:
		result = (int)0x8050c041;
		break;
	case -1:
		result = (int)0x8050c003;
		break;
	default:
		break;
	}
release:
	SceProcessmgrForDriver_FC2A424E(copy);
	return result;
}

int copyVideoEkb2RecordToUser(int handle, const MgVideoUserBuffer *output)
{
	MgVideoContext *context;
	void *destination, *copy = 0, *buffer;
	unsigned int size;
	int result;

	if (videoReferences <= 0)
		return (int)0x8050c011;
	if (handle < 0 || !(handle & 0x10000))
		return (int)0x8050c002;
	context = videoContexts[(uint16_t)handle];
	if ((uint16_t)handle > 1 || context->references <= 0)
		return (int)0x8050c002;
	if (!context->state)
		return (int)0x8050c011;
	if (output) {
		destination = (void *)output->data;
		size = output->size;
		if (!destination)
			size = 0;
	} else {
		destination = 0;
		size = 0;
	}
	buffer = destination;
	if (size) {
		copy = SceProcessmgrForDriver_5468892A(size, 0);
		if (!copy)
			return (int)0x8050c004;
		buffer = copy;
	}
	result = context->callbacks.getEkb2Record(buffer, size);
	if (result >= 0 && size) {
		ksceKernelCopyToUser(destination, copy, size);
		if (size > offsetof(MgVideoRecordPrefix, stateMarker) + 1)
			ksceKernelCopyToUser((uint8_t *)destination +
					offsetof(MgVideoRecordPrefix, stateMarker),
					&videoRecordStateMarker, 2);
		else if (size == offsetof(MgVideoRecordPrefix, stateMarker) + 1)
			ksceKernelCopyToUser((uint8_t *)destination +
					offsetof(MgVideoRecordPrefix, stateMarker),
					&videoRecordStateMarker, 1);
	}
	if (copy)
		SceProcessmgrForDriver_FC2A424E(copy);
	return result;
}

int generateUserVideoKeyring(int handle, const MgVideoUserBuffer *output,
			    MgVideoGeneratedProtectionInfo *protection,
			    const MgVideoAvOut *avOut,
			    const MgVideoKeyGenerationParameters *parameters,
			    int mode)
{
	struct {
		MgVideoGeneratedKey identityKey;
		MgVideoCipherBlock tag;
		MgVideoGeneratedKey authenticationKey;
	} scratch __attribute__((aligned(16)));
	MgVideoKeySystemId keySystemId;
	const MgVideoKeySystemId *derivationId;
	MgVideoKeyringBuffer *buffer;
	MgVideoContext *context;
	unsigned int flags, selection, algorithm, payloadOffset, dataSize;
	int policyMode, index, result;

	if (videoReferences <= 0)
		return (int)0x8050c011;
	if (handle < 0 || !(handle & 0x10000))
		return (int)0x8050c002;
	context = videoContexts[(uint16_t)handle];
	if ((uint16_t)handle > 1 || context->references <= 0)
		return (int)0x8050c002;
	if (!context->state)
		return (int)0x8050c011;
	if (!output || !output->data || output->size < sizeof(*buffer))
		return (int)0x8050c002;
	if ((unsigned int)mode - 4U > 2)
		return (int)0x8050c033;
	buffer = SceProcessmgrForDriver_5468892A(sizeof(*buffer), 0);
	if (!buffer)
		return (int)0x8050c004;
	flags = avOut->fields.controlFlags;
	selection = (flags >> 1) & 3;
	if (!selection) {
		policyMode = !(flags & 1);
	} else if (selection == 2 && (flags & 1)) {
		policyMode = 2;
	} else {
		result = (int)0x8050c002;
		goto release;
	}
	derivationId = parameters ? &parameters->keySystemId : 0;
	result = generateVideoRecordKey(scratch.authenticationKey.halves,
					derivationId, 0);
	if (result < 0)
		goto release;
	result = generateVideoRecordKey(scratch.identityKey.halves, derivationId, 1);
	if (result < 0)
		goto release;
	/* The original initializes only these two words of this serialized ID. */
	keySystemId.words[0] = 0x01000000;
	keySystemId.words[1] = 0x04000001;
	memset(protection, 0, sizeof(*protection));
	algorithm = mode == 5 ? 9 : mode == 6 ? 8 : 0;
	result = initializeVideoProtectionInfo(&protection->protection[0],
				&keySystemId, 0, algorithm, derivationId);
	if (result < 0)
		goto release;
	result = initializeVideoProtectionInfo(&protection->protection[1],
				&keySystemId, 1, algorithm, derivationId);
	if (result < 0)
		goto release;
	result = initializeVideoProtectionInfo(&protection->protection[2],
				&keySystemId, 2, algorithm, derivationId);
	if (result < 0)
		goto release;
	protection->keySystemId = keySystemId;
	protection->authenticationKey = scratch.authenticationKey.block;
	memcpy(buffer->header.label, keyringLabel, sizeof(buffer->header.label));
	buffer->header.descriptorSize[1] = 16;
	buffer->header.descriptorCount[1] = 4;
	buffer->header.keyDataSize[1] = 64;
	buffer->header.format[1] = 3;
	buffer->header.descriptorSize[0] = 0;
	buffer->header.descriptorCount[0] = 0;
	buffer->header.wrappedDriverKey = scratch.authenticationKey.block;
	buffer->header.keyDataSize[0] = 0;
	buffer->header.format[0] = 0;
	buffer->header.keyCount[0] = 0;
	buffer->header.encodedDescriptorMetadata[0] = 0;
	memset(&buffer->header.encodedDescriptorMetadata[2], 0, 8);
	buffer->header.keyCount[1] = 2;
	buffer->header.encodedDescriptorMetadata[1] = 16;
	index = ((buffer->header.keyCount[0] << 8) |
		 buffer->header.keyCount[1]) - 1;
	for (; index >= 0; index--) {
		MgVideoStreamKey *key = &buffer->header.keys[index];

		result = generateVideoRecordKey(key->generatedWords, derivationId,
						index + 2);
		if (result < 0)
			goto release;
		if (mode == 6) {
			if (parameters && (unsigned int)index < 3) {
				memcpy(key->firstBlock, &parameters->ranges[index],
				       sizeof(parameters->ranges[index]));
			} else {
				memset(key->firstBlock, 0, sizeof(key->firstBlock));
				memset(key->endBlock, 0xff, sizeof(key->endBlock));
			}
		}
	}
	payloadOffset = sizeof(buffer->prefix) + 16 *
		(((buffer->header.descriptorCount[0] << 8) |
		  buffer->header.descriptorCount[1]) + 1);
	result = serializeVideoPlaybackPolicy((uint8_t *)buffer + payloadOffset,
			output->size, &keySystemId, avOut, policyMode, 0, 0);
	if (result < 0) {
		result = (int)0x8050c002;
		goto release;
	}
	dataSize = payloadOffset + result;
	result = dataSize + sizeof(scratch.tag);
	buffer->prefix.capacity = 508;
	buffer->prefix.magic = 0x3350474d;
	buffer->prefix.size = result;
	memset(buffer->prefix.encodedMetadata, 0,
	       sizeof(buffer->prefix.encodedMetadata));
	scratch.tag = buffer->header.wrappedDriverKey;
	ksceKernelCopyToUser((void *)output->data, buffer, dataSize);
	ksceKernelCopyToUser((uint8_t *)output->data + dataSize,
			    &scratch.tag, sizeof(scratch.tag));
release:
	SceProcessmgrForDriver_FC2A424E(buffer);
	return result;
}

int validateVideoBindingPolicy(int dialect, const void *input, unsigned int size);
int copyVideoDataFromUser(void *destination, const void *source, unsigned int size);
int copyVideoDataToUser(void *destination, const void *source, unsigned int size);

int replaceUserVideoKeyring(int handle, unsigned int index, int blockNumber,
			   const MgVideoUserBuffer *keyring,
			   const MgVideoUserBuffer *input,
			   const MgVideoUserBuffer *recordInput,
			   const MgVideoUserBuffer *replacementInput)
{
	struct {
		MgVideoCipherBlock recordKey;
		MgVideoRecordPrefix prefix;
	} scratch __attribute__((aligned(16)));
	MgVideoContext *context = 0;
	MgVideoKeyringBuffer *buffer = 0;
	MgVideoUserBuffer body, replacement;
	MgVideoUserRecord *record;
	const void *keyringData;
	unsigned int keyringSize, allocationSize, totalSize, bodySize, policyOffset;
	uint32_t magic;
	MgVideoCipherBlock *tag;
	int installed, slot, result;

	if (videoReferences <= 0)
		return (int)0x8050c011;
	if (handle < 0 || !(handle & 0x10000))
		return (int)0x8050c002;
	context = videoContexts[(uint16_t)handle];
	if ((uint16_t)handle > 1 || context->references <= 0)
		return (int)0x8050c002;
	if (context->state <= 2)
		return (int)0x8050c013;
	if (index > 31 || (unsigned int)blockNumber - 1U > 998)
		return (int)0x8050c002;
	keyringData = keyring ? keyring->data : 0;
	keyringSize = keyring ? keyring->size : 0;
	if (!recordInput || !recordInput->data)
		return (int)0x8050c002;
	record = (MgVideoUserRecord *)recordInput->data;
	if (recordInput->size != sizeof(*record))
		return (int)0x8050c003;
	if (!input)
		return (int)0x8050c002;
	body = *input;
	if (!body.data)
		return (int)0x8050c002;
	if (body.size <= 79)
		return (int)0x8050c031;
	if (ksceKernelCopyFromUser(&magic, body.data, sizeof(magic)) < 0)
		return (int)0x8050c002;
	if (magic != 0x32424b45)
		return (int)0x8050c031;
	body.data = (const uint8_t *)body.data +
		    offsetof(MagicGateBundledEkb2Record, header);
	body.size -= offsetof(MagicGateBundledEkb2Record, header);
	if (!replacementInput)
		return (int)0x8050c002;
	replacement = *replacementInput;
	if (!replacement.data)
		return (int)0x8050c002;
	if (replacement.size <= 79)
		return (int)0x8050c031;
	if (ksceKernelCopyFromUser(&magic, replacement.data, sizeof(magic)) < 0)
		return (int)0x8050c002;
	if (magic != 0x32424b45)
		return (int)0x8050c031;
	replacement.data = (const uint8_t *)replacement.data +
			   offsetof(MagicGateBundledEkb2Record, header);
	replacement.size -= offsetof(MagicGateBundledEkb2Record, header);
	if (!(context->validRecordMask & (1U << index)))
		return (int)0x8050c013;
	allocationSize = keyringSize < sizeof(*buffer) ? keyringSize : sizeof(*buffer);
	buffer = SceProcessmgrForDriver_5468892A(allocationSize, 0);
	if (!buffer)
		return (int)0x8050c004;
	ksceKernelCopyFromUser(buffer, keyringData, allocationSize);
	result = (int)0x8050c003;
	if (buffer->prefix.magic != 0x3350474d)
		goto release;
	totalSize = buffer->prefix.size;
	if ((int)totalSize > (int)keyringSize || buffer->prefix.capacity != 508 ||
	    totalSize <= 95 || totalSize > sizeof(*buffer))
		goto release;
	installed = installUserVideoRecord(context, &body);
	if (installed < 0) {
		result = installed;
		goto release;
	}
	slot = SceMagicGateForDriver_FEF11D84(4);
	if (slot < 0) {
		result = slot;
		TRANSLATE_MAGICGATE_RESULT(result);
		goto releaseInstalled;
	}
	result = (int)0x8050c003;
	if (buffer->prefix.capacity != 508)
		goto releaseSlot;
	tag = (MgVideoCipherBlock *)((uint8_t *)buffer +
		((buffer->prefix.size - sizeof(*tag)) & ~15U));
	result = context->callbacks.unwrapDriverKey(slot, tag, installed);
	if (result < 0)
		goto error;
	result = SceMagicGateForDriver_05912BF5(&buffer->header.wrappedDriverKey,
					      tag, 0, sizeof(*tag), slot);
	if (result < 0)
		goto error;
	result = context->callbacks.unwrapDriverKey(slot,
				&buffer->header.wrappedDriverKey, installed);
	if (result < 0)
		goto error;
	totalSize -= sizeof(buffer->prefix);
	bodySize = totalSize - sizeof(*tag);
	policyOffset = ((buffer->header.descriptorSize[0] << 8) |
			buffer->header.descriptorSize[1]) *
		       (((buffer->header.descriptorCount[0] << 8) |
			 buffer->header.descriptorCount[1]) + 1);
	result = validateVideoBindingPolicy(context->parserDialect,
			(uint8_t *)&buffer->header + policyOffset,
			bodySize - policyOffset);
	if (result < 0) {
		switch (result) {
		case -15:
			result = (int)0x8050c04e;
			break;
		case -8:
			result = (int)0x8050c047;
			break;
		case -7:
			result = (int)0x8050c046;
			break;
		case -6:
			result = (int)0x8050c045;
			break;
		case -5:
			result = (int)0x8050c044;
			break;
		case -4:
			result = (int)0x8050c043;
			break;
		case -3:
			result = (int)0x8050c042;
			break;
		case -2:
			result = (int)0x8050c041;
			break;
		case -1:
			result = (int)0x8050c003;
			break;
		default:
			break;
		}
		goto releaseSlot;
	}
	result = context->callbacks.getWrappedKey(&buffer->header.wrappedDriverKey,
						 slot, installed);
	if (result < 0)
		goto releaseSlot;
	result = SceMagicGateForDriver_05912BF5(&scratch.recordKey,
					      &buffer->header, 0, bodySize, slot);
	if (result < 0)
		goto error;
	SceMagicGateForDriver_144BFA76(slot);
	result = replaceUserVideoRecordBlock(context, index, blockNumber,
					    &scratch.recordKey, record, &replacement);
	if (result >= 0) {
		scratch.prefix.stateMarker = videoRecordStateMarker;
		scratch.prefix.magic.encoded = 0x5343494c;
		scratch.prefix.encodedRecordMetadata = 0;
		copyVideoDataFromUser(scratch.prefix.encodedPrefixMetadata,
				     body.data, sizeof(uint32_t));
		memset(&scratch.prefix.encodedPrefixMetadata[4], 0, sizeof(uint32_t));
		copyVideoDataToUser((void *)keyringData, &scratch.prefix,
				    sizeof(scratch.prefix));
		copyVideoDataToUser((uint8_t *)keyringData + sizeof(scratch.prefix),
				    &buffer->header, bodySize);
		result = totalSize;
	}
	goto releaseInstalled;
error:
	TRANSLATE_MAGICGATE_RESULT(result);
releaseSlot:
	SceMagicGateForDriver_144BFA76(slot);
releaseInstalled:
	if (installed != -1)
		SceMagicGateForDriver_144BFA76(installed);
release:
	SceProcessmgrForDriver_FC2A424E(buffer);
	if ((unsigned int)result + 2142191583U <= 1 && context)
		context->state = 1;
	return result;
}

unsigned int measureVideoTaggedData(const void *input, unsigned int size);
int extendVideoPlaybackPeriod(int dialect, void *input, unsigned int size,
			     unsigned int capacity);

int updateUserVideoPlaybackPeriod(int handle, unsigned int index, int blockNumber,
				 const MgVideoUserBuffer *keyring,
				 const MgVideoUserBuffer *input,
				 const MgVideoUserBuffer *recordInput,
				 const MgVideoUserBuffer *replacementInput,
				 const MgVideoUserBuffer *optionInput,
				 const void *extensionInput)
{
	struct {
		MgVideoCipherBlock recordKey;
		MgVideoRecordPrefix prefix;
		MgVideoKeyringHeader header;
	} scratch __attribute__((aligned(16)));
	MgVideoKeyringRequest request;
	MgVideoUserBuffer keyringCopy, body, replacement;
	MgVideoContext *context;
	MgVideoUserRecord *record;
	MgVideoKeyringBuffer *buffer;
	unsigned int allocationSize, encodedSize, policyOffset, policySize;
	uint8_t *policy;
	uint32_t magic;
	int installed, slot, result;

	if (videoReferences <= 0)
		return (int)0x8050c011;
	if (handle < 0 || !(handle & 0x10000))
		return (int)0x8050c002;
	context = videoContexts[(uint16_t)handle];
	if ((uint16_t)handle > 1 || context->references <= 0)
		return (int)0x8050c002;
	if (context->state <= 2)
		return (int)0x8050c013;
	if (index > 31 || (unsigned int)blockNumber - 1U > 998 || !keyring)
		return (int)0x8050c002;
	keyringCopy = *keyring;
	if (!keyringCopy.data)
		return (int)0x8050c002;
	if (keyringCopy.size <= 79)
		return (int)0x8050c003;
	if (!recordInput || !recordInput->data)
		return (int)0x8050c002;
	record = (MgVideoUserRecord *)recordInput->data;
	if (recordInput->size != sizeof(*record))
		return (int)0x8050c003;
	if (!input)
		return (int)0x8050c002;
	body = *input;
	if (!body.data)
		return (int)0x8050c002;
	if (body.size <= 79)
		return (int)0x8050c031;
	if (ksceKernelCopyFromUser(&magic, body.data, sizeof(magic)) < 0)
		return (int)0x8050c002;
	if (magic != 0x32424b45)
		return (int)0x8050c031;
	body.data = (const uint8_t *)body.data +
		    offsetof(MagicGateBundledEkb2Record, header);
	body.size -= offsetof(MagicGateBundledEkb2Record, header);
	if (!replacementInput)
		return (int)0x8050c002;
	replacement = *replacementInput;
	if (!replacement.data)
		return (int)0x8050c002;
	if (replacement.size <= 79)
		return (int)0x8050c031;
	if (ksceKernelCopyFromUser(&magic, replacement.data, sizeof(magic)) < 0)
		return (int)0x8050c002;
	if (magic != 0x32424b45)
		return (int)0x8050c031;
	replacement.data = (const uint8_t *)replacement.data +
			   offsetof(MagicGateBundledEkb2Record, header);
	replacement.size -= offsetof(MagicGateBundledEkb2Record, header);
	if (optionInput)
		return (int)0x8050c002;
	if (!(context->validRecordMask & (1U << index)))
		return (int)0x8050c013;
	if (keyringCopy.size > sizeof(*buffer))
		allocationSize = sizeof(*buffer) - sizeof(buffer->prefix);
	else
		allocationSize = keyringCopy.size - sizeof(buffer->prefix);
	if (allocationSize > sizeof(*buffer))
		return (int)0x8050c051;
	buffer = SceProcessmgrForDriver_5468892A(allocationSize, 0);
	if (!buffer)
		return (int)0x8050c004;
	ksceKernelCopyFromUser(buffer, (const uint8_t *)keyringCopy.data +
			      sizeof(buffer->prefix), allocationSize);
	encodedSize = measureVideoTaggedData(buffer, allocationSize);
	keyringCopy.size = encodedSize + sizeof(buffer->prefix);
	ksceKernelCopyFromUser(buffer, keyringCopy.data, keyringCopy.size);
	keyringCopy.data = buffer;
	request.context = context;
	request.header = &scratch.header;
	request.keySystemId = 0;
	request.index = index;
	request.blockNumber = blockNumber;
	request.keyring = &keyringCopy;
	request.record = record;
	request.input = &body;
	result = ksceKernelRunWithStack(5680, openUserVideoKeyring, &request);
	if (result >= 0) {
		SceMagicGateForDriver_144BFA76(result);
		result = 0;
		goto release;
	}
	if (result != (int)0x8050c04e)
		goto release;
	policyOffset = ((scratch.header.descriptorSize[0] << 8) |
			scratch.header.descriptorSize[1]) *
		       (((scratch.header.descriptorCount[0] << 8) |
			 scratch.header.descriptorCount[1]) + 1);
	policy = (uint8_t *)&buffer->header + policyOffset;
	policySize = encodedSize - policyOffset;
	result = validateVideoBindingPolicy(context->parserDialect, policy, policySize);
	if (result < 0)
		goto parserError;
	result = extendVideoPlaybackPeriod(context->parserDialect, policy,
					  policySize, allocationSize - policyOffset);
	if (result < 0)
		goto parserError;
	if (!result)
		goto release;
	policySize = result;
	result = parseVideoProtectionInfo(context->parserDialect, 0, policy, policySize);
	if (result < 0)
		goto parserError;
	installed = installUserVideoRecord(context, &body);
	if (installed < 0) {
		result = installed;
		goto release;
	}
	slot = SceMagicGateForDriver_FEF11D84(4);
	if (slot < 0) {
		result = slot;
		SceMagicGateForDriver_144BFA76(installed);
		goto error;
	}
	result = context->callbacks.unwrapDriverKey(slot,
					 &scratch.header.wrappedDriverKey, installed);
	SceMagicGateForDriver_144BFA76(installed);
	if (result < 0) {
		SceMagicGateForDriver_144BFA76(slot);
		goto error;
	}
	encodedSize = policyOffset + policySize;
	result = SceMagicGateForDriver_05912BF5(&scratch.recordKey,
					      &buffer->header, 0, encodedSize, slot);
	if (result < 0) {
		TRANSLATE_MAGICGATE_RESULT(result);
		goto releaseSlot;
	}
	result = replaceUserVideoRecordBlock(context, index, blockNumber,
					    &scratch.recordKey, record, &replacement);
	if (result >= 0) {
		scratch.prefix.encodedRecordMetadata = 0;
		scratch.prefix.magic.encoded = 0x5343494c;
		scratch.prefix.stateMarker = videoRecordStateMarker;
		copyVideoDataFromUser(scratch.prefix.encodedPrefixMetadata,
				     body.data, sizeof(uint32_t));
		memset(&scratch.prefix.encodedPrefixMetadata[4], 0, sizeof(uint32_t));
		copyVideoDataToUser((void *)keyring->data, &scratch.prefix,
				    sizeof(scratch.prefix));
		copyVideoDataToUser((uint8_t *)keyring->data + sizeof(scratch.prefix),
				    &buffer->header, encodedSize);
		result = encodedSize + sizeof(scratch.prefix);
	}
releaseSlot:
	SceMagicGateForDriver_144BFA76(slot);
	goto release;
parserError:
	result = translateParserError(result);
	goto release;
error:
	TRANSLATE_MAGICGATE_RESULT(result);
release:
	SceProcessmgrForDriver_FC2A424E(buffer);
	if ((unsigned int)result + 2142191583U <= 1)
		context->state = 1;
	return result;
}
