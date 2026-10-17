/* Descriptive file name: no source-file debug records exist in the original. */
#include "mgvideo_private.h"
#include "mgvideo_result.h"
#include <processmgr/processmgr_imports.h>
#include <psp2kern/kernel/sysclib.h>
#include <magicgate/magicgate.h>

unsigned int copyFileGroupToUser(void *destination, unsigned int size,
				 const MagicGateFileGroup *group, int status)
{
	MgVideoFileGroupInfo info = {
		.identity.words = {
			group->identity.words[0], group->identity.words[1],
			group->identity.words[2], group->identity.words[3],
		},
	};
	unsigned int copied;

	if (status >= 0) {
		info.creationTick = group->creationTick;
		if (info.creationTick == 0)
			info.creationTick = 1;
		info.modificationTick = group->modificationTick;
		if (info.modificationTick == 0)
			info.modificationTick = 1;
	}
	if (size >= sizeof(info))
		copied = sizeof(info);
	else
		copied = size;
	SceSysmemForDriver_6D88EF8A(destination, &info, copied);
	return size - copied;
}

int initializeVideoProtectionInfo(MgVideoProtectionInfoBox *output,
				 const MgVideoKeySystemId *keySystemId, int index,
				 unsigned int algorithm,
				 const MgVideoKeySystemId *derivationId)
{
	MgVideoGeneratedKey generated __attribute__((aligned(16)));
	uint32_t first, second, third, fourth, fifth, sixth;
	int result;

	memcpy(output, &protectionInfoTemplate, sizeof(*output));
	if (algorithm == 8) {
		result = generateVideoRecordKey(generated.halves, derivationId, index + 5);
		if (result < 0)
			return result;
		output->information.encryption.initializationVector = generated.block;
	} else if (algorithm != 9 && algorithm != 0) {
		return (int)0x8050c003;
	}
	first = keySystemId->words[0];
	second = keySystemId->words[1];
	third = keySystemId->words[2];
	fourth = keySystemId->words[3];
	output->information.keySystem.keySystemId.words[0] = first;
	output->information.keySystem.keySystemId.words[1] = second;
	output->information.keySystem.keySystemId.words[3] = fourth;
	output->information.keySystem.keySystemId.words[2] = third;
	fifth = keySystemId->words[4];
	sixth = keySystemId->words[5];
	output->information.encryption.algorithm[0] = algorithm >> 24;
	output->information.keySystem.keySystemId.words[4] = fifth;
	output->information.keySystem.keySystemId.words[5] = sixth;
	output->information.encryption.algorithm[1] = algorithm >> 16;
	output->information.encryption.algorithm[2] = algorithm >> 8;
	output->information.encryption.algorithm[3] = algorithm;
	output->information.key.keyId[1] = index;
	return 0;
}

int enumerateVideoFileGroups(int handle, MgVideoFileGroupInfo *output,
			    unsigned int size)
{
	MagicGateFileGroup group __attribute__((aligned(16)));
	MgVideoContext *context;
	MgVideoFileGroupInfo *next;
	uintptr_t end;
	int currentGroup, index, result;

	if (videoReferences <= 0)
		return (int)0x8050c011;
	if (handle < 0 || !(handle & 0x10000))
		return (int)0x8050c002;
	context = videoContexts[(uint16_t)handle];
	if ((uint16_t)handle > 1 || context->references <= 0)
		return (int)0x8050c002;
	if (!context->state)
		return (int)0x8050c011;
	result = SceMagicGateForDriver_E62B8A67(context->fileHandle, &group, -1);
	if (result < 0)
		goto error;
	currentGroup = result - 1;
	end = (uintptr_t)output + size;
	next = output + 1;
	size = copyFileGroupToUser(output, size, &group, currentGroup);
	result = 1;
	if (end <= (uintptr_t)next)
		return result;
	index = 0;
	for (;;) {
		result = SceMagicGateForDriver_E62B8A67(context->fileHandle, &group, index);
		if (result < 0)
			goto error;
		if (index != currentGroup &&
		    memcmp(&group.identity, &zeroVideoBlock, sizeof(group.identity)) != 0) {
			size = copyFileGroupToUser(next, size, &group, index);
			next++;
		}
		index++;
		if ((uintptr_t)next >= end || result <= index)
			break;
	}
	if (end > (uintptr_t)next) {
		group.identity.words[0] = 0;
		group.identity.words[1] = 0;
		group.identity.words[2] = 0;
		group.identity.words[3] = 0;
		group.creationTick = 0;
		group.modificationTick = 0;
		do {
			size = copyFileGroupToUser(next, size, &group, -1);
			next++;
		} while (end > (uintptr_t)next);
	}
	return result;
error:
	TRANSLATE_MAGICGATE_RESULT(result);
	return result;
}

int selectVideoFileGroup(int handle, const MagicGateFileIdentity *input)
{
	MagicGateFileGroup group __attribute__((aligned(16)));
	MagicGateFileIdentity identity;
	MgVideoContext *context;
	unsigned int validMask, bit;
	int index, current, result;

	if (videoReferences <= 0)
		return (int)0x8050c011;
	if (handle < 0 || !(handle & 0x10000))
		return (int)0x8050c002;
	context = videoContexts[(uint16_t)handle];
	if ((uint16_t)handle > 1 || context->references <= 0)
		return (int)0x8050c002;
	if (!context->state || !context->driverType)
		return (int)0x8050c011;
	if (!input)
		return (int)0x8050c002;
	SceSysmemForDriver_BC996A7A(&identity, input, sizeof(identity));
	result = SceMagicGateForDriver_E62B8A67(context->fileHandle, &group, -1);
	if (result < 0)
		goto error;
	if (memcmp(&identity, &group.identity, sizeof(identity)) != 0) {
		index = 0;
		current = 0;
	} else {
		index = result - 1;
		if (!result)
			return 0;
		current = 1;
	}
	for (;;) {
		result = SceMagicGateForDriver_E62B8A67(context->fileHandle, &group, index);
		if (result < 0)
			goto error;
		if (memcmp(&identity, &group.identity, sizeof(identity)) == 0)
			break;
		if (result <= ++index)
			return (int)0x8050c002;
	}
	result = SceMagicGateForDriver_8E87BA88(context->fileHandle, index);
	if (result < 0)
		goto error;
	if (!current)
		return 0;
	result = loadVideoKeyring(context);
	if (result < 0)
		goto reset;
	validMask = context->validRecordMask;
	bit = 0;
	do {
		validMask |= (1U << bit) | (1U << (bit + 1)) |
			     (1U << (bit + 2)) | (1U << (bit + 3));
		bit += 4;
	} while (bit != 32);
	context->validRecordMask = validMask;
	context->state = 3;
	return 0;
error:
	TRANSLATE_MAGICGATE_RESULT(result);
reset:
	if ((unsigned int)(result - (int)0x8050c021) <= 1 && context)
		context->state = 1;
	return result;
}
