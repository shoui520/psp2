/* Descriptive file name: no source-file debug records exist in the original. */
#include "mgvideo_private.h"
#include "mgvideo_result.h"
#include <sysmem/sysclib.h>
#include <usbstormg/usbstormg.h>

int loadVideoKeyring(MgVideoContext *context)
{
	const uint64_t *word;
	int result;

	context->validRecordMask = 0;
	memset(context->records.recordTags, 0,
	       sizeof(context->records.recordTags));
	memset(context->records.recordHeaderTags, 0,
	       sizeof(context->records.recordHeaderTags));
	result = context->callbacks.readRecord(context, 1, &keyringRecord);
	if (result >= 0)
		goto copyKeys;
	if (result != (int)0x8050c023)
		return result;
	if (context->modeFlags & 32) {
		context->modeFlags |= 8;
	} else if (((keyringRecord.payloadSize[0] << 8) |
		    keyringRecord.payloadSize[1]) == 0 &&
		   (keyringRecord.flags & 0xe0) == 0 &&
		   timingsafe_memcmp(&keyringRecord.encodedKeyWords[0],
				     &emptyKeyringEntry, sizeof(uint64_t)) == 0) {
		word = &keyringRecord.encodedKeyWords[1];
		while (timingsafe_memcmp(word, &emptyKeyringEntry,
					sizeof(uint64_t)) == 0) {
			if (timingsafe_memcmp(word + 1, &emptyKeyringEntry,
					     sizeof(uint64_t)) != 0)
				break;
			if (timingsafe_memcmp(word + 2, &emptyKeyringEntry,
					     sizeof(uint64_t)) != 0)
				break;
			if (timingsafe_memcmp(word + 3, &emptyKeyringEntry,
					     sizeof(uint64_t)) != 0)
				break;
			if (timingsafe_memcmp(word + 4, &emptyKeyringEntry,
					     sizeof(uint64_t)) != 0)
				break;
			if (timingsafe_memcmp(word + 5, &emptyKeyringEntry,
					     sizeof(uint64_t)) != 0)
				break;
			word += 6;
			if (word == keyringRecord.encodedKeyWords + 127) {
				context->modeFlags |= 8;
				break;
			}
		}
	}
	memset(&keyringRecord, 0, sizeof(keyringRecord));
copyKeys:
	memcpy(context->records.contentKeys, keyringRecord.encodedKeyWords,
	       sizeof(context->records.contentKeys));
	return 0;
}

int readVideoKeyringRecord(MgVideoContext *context, int index,
			   MgVideoKeyringRecord *record)
{
	int result;

	result = SceMagicGateForDriver_1BCE34AD(context->fileHandle, index,
					    record, sizeof(*record));
	if (result >= 0) {
		context->recordErrorFlags &= ~1U;
		if (record->index == index) {
			if ((record->payloadSize[1] | (record->payloadSize[0] << 8)) != 512)
				return (int)0x8050c023;
			if ((record->flags & 0xe0) == 0xa0)
				return 0;
			return (int)0x8050c023;
		}
		return (int)0x8050c023;
	}
	if (result == (int)0x80504004) {
		context->recordErrorFlags = 1;
		return (int)0x8050c016;
	}
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

int writeVideoKeyringRecord(MgVideoContext *context, unsigned int index,
			    MgVideoKeyringRecord *record)
{
	uint32_t sequence, encodedSequence;
	int result;

	sequence = ((uint32_t)record->sequence.bytes[0] << 24) |
		   ((uint32_t)record->sequence.bytes[1] << 16) |
		   ((uint32_t)record->sequence.bytes[2] << 8) |
		   record->sequence.bytes[3];
	record->index = index;
	record->flags = 0xa0;
	record->payloadSize[1] = 0;
	record->payloadSize[0] = 2;
	sequence++;
	encodedSequence = ((uint32_t)(uint8_t)(sequence >> 8) << 16) |
			  (sequence << 24) | (sequence >> 24) |
			  ((uint32_t)(uint8_t)(sequence >> 16) << 8);
	record->sequence.encoded = encodedSequence;
	result = SceMagicGateForDriver_75B445DA(context->fileHandle, index,
					    record, sizeof(*record));
	if (result < 0) {
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
			goto readBack;
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
		if (result != (int)0x8050c005)
			return result;
	}
readBack:
	result = SceMagicGateForDriver_1BCE34AD(context->fileHandle, index,
					    record, sizeof(*record));
	if (result < 0) {
		if (result == (int)0x80504004) {
			result = (int)0x8050c016;
			context->recordErrorFlags = 1;
		} else {
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
		}
	} else {
		context->recordErrorFlags &= ~1U;
		if (record->index == index &&
		    (record->payloadSize[1] | (record->payloadSize[0] << 8)) == 512 &&
		    (record->flags & 0xe0) == 0xa0)
			goto compareSequence;
		result = (int)0x8050c023;
	}
	if (result != (int)0x8050c023)
		return result;
compareSequence:
	return record->sequence.encoded == encodedSequence ? 0 : (int)0x8050c005;
}

int saveVideoKeyring(int handle)
{
	MgVideoContext *context;
	int result;

	if (videoReferences <= 0)
		return (int)0x8050c011;
	if (handle < 0 || !(handle & 0x10000))
		return (int)0x8050c002;
	context = videoContexts[(uint16_t)handle];
	if ((uint16_t)handle > 1 || context->references <= 0)
		return (int)0x8050c002;
	if (context->state <= 2)
		return (int)0x8050c013;
	result = context->callbacks.readRecord(context, 1, &keyringRecord);
	if (result >= 0 || (result == (int)0x8050c023 && (context->modeFlags & 8))) {
		memcpy(keyringRecord.encodedKeyWords, context->records.contentKeys, 512);
		result = context->callbacks.writeRecord(context, 1, &keyringRecord);
	}
	if ((unsigned int)result - 0x8050c021U <= 1 && context)
		context->state = 1;
	return result;
}

int resetVideoKeyring(int handle)
{
	MgVideoContext *context;
	unsigned int mask, index;
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
	if (context->recordErrorFlags & 1)
		return (int)0x8050c016;
	if (context->state <= 1) {
		result = context->callbacks.initialize(context, &videoRandomChallenge);
		if (result < 0)
			goto error;
	}
	context->validRecordMask = 0;
	result = context->callbacks.resetRecord(context, 1, &keyringRecord);
	if (result < 0)
		goto error;
	result = loadVideoKeyring(context);
	if (result < 0)
		goto error;
	mask = context->validRecordMask;
	for (index = 0; index != 32; index += 4) {
		mask |= 1U << index;
		mask |= 1U << (index + 1);
		mask |= 1U << (index + 2);
		mask |= 1U << (index + 3);
	}
	context->validRecordMask = mask;
	context->state = 3;
	return 0;
error:
	if ((unsigned int)result - 0x8050c021U <= 1 && context)
		context->state = 1;
	return result;
}
