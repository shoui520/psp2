/* Descriptive source name: the original has no source-file debug records. */
#include "mgvideo_private.h"
#include <rtc/rtc.h>
#include <sysmem/sysclib.h>

int findVideoTaggedRecords(const void *input, int size, const char *const labels[],
			  const MgVideoTaggedRecord *records[], int count)
{
	const uint8_t *end = (const uint8_t *)input + size;
	const MgVideoTaggedRecord *record = input;
	int index, found = 0, payloadSize;

	if (count <= 0)
		return 0;
	for (index = 0; index < count; index++)
		records[index] = 0;
	while (size > (int)sizeof(*record)) {
		payloadSize = 16 * ((record->descriptorCount[0] << 8) |
				    record->descriptorCount[1]);
		if (end >= (const uint8_t *)record + payloadSize) {
			for (index = 0; index < count; index++) {
				if (!records[index] && labels[index] &&
				    memcmp(record->label, labels[index], sizeof(record->label)) == 0) {
					records[index] = record;
					found++;
				}
			}
		}
		payloadSize += sizeof(*record);
		size -= payloadSize;
		record = (const MgVideoTaggedRecord *)((const uint8_t *)record + payloadSize);
	}
	return found;
}

int classifyUnsupportedVideoPolicy(const char *label)
{
	if (memcmp(label, "P_PlayPeriod1  ", 15) == 0 ||
	    memcmp(label, "P_PlayPeriod2  ", 15) == 0 ||
	    memcmp(label, "T_ClockType1   ", 15) == 0 ||
	    memcmp(label, "T_ClockType2   ", 15) == 0)
		return -3;
	if (memcmp(label, "T_Grouping     ", 15) == 0 ||
	    memcmp(label, "P_ShareChkSID  ", 15) == 0)
		return -9;
	if (memcmp(label, "P_BindCount    ", 15) == 0 ||
	    memcmp(label, "P_CopyCount    ", 15) == 0)
		return -6;
	if (label[0] != 'V')
		return -2;
	if (label[1] == '_')
		return -8;
	return -2;
}

#define FIND_VIDEO_PARAMETER(record, tag, output) do { \
	const MgVideoPlaybackParameter *element; \
	unsigned int count, parameterIndex; \
	(output) = 0; \
	if (record) { \
		count = ((record)->descriptorCount[0] << 8) | \
			(record)->descriptorCount[1]; \
		element = (const MgVideoPlaybackParameter *)(record)->payload; \
		if (element) { \
			for (parameterIndex = 0; parameterIndex < count; parameterIndex++) { \
				if (memcmp(element[parameterIndex].label, tag, 8) == 0) { \
					(output) = &element[parameterIndex]; \
					break; \
				} \
			} \
		} \
	} \
} while (0)

int validateVideoPlaybackTime(const MgVideoTaggedRecord *playParameters,
			     const MgVideoTaggedRecord *sessionParameters,
			     const MgVideoEncodedTime *currentTime)
{
	const MgVideoPlaybackParameter *parameter;
	const MgVideoPlaybackValue *period;
	const MgVideoEncodedTime *start, *end;
	unsigned int periodHours;

	FIND_VIDEO_PARAMETER(playParameters, "period  ", parameter);
	period = parameter ? &parameter->value : 0;
	if (!period)
		period = &emptyVideoPlaybackValue;
	periodHours = (period->period.hours[0] << 8) | period->period.hours[1];
	if (periodHours) {
		FIND_VIDEO_PARAMETER(sessionParameters, "end_t2  ", parameter);
		end = parameter ? &parameter->value.time : 0;
		if (!end || memcmp(&latestVideoTime, end, sizeof(*end)) == 0)
			return -15;
		FIND_VIDEO_PARAMETER(sessionParameters, "start_t2", parameter);
		start = parameter ? &parameter->value.time : 0;
		if (!start)
			start = &latestVideoTime;
		if (!currentTime)
			return -3;
		if (memcmp(currentTime, start, sizeof(*start)) < 0)
			return memcmp(start, end, sizeof(*start)) > 0 ? -5 : -4;
		if (memcmp(currentTime, end, sizeof(*end)) > 0)
			return -5;
	}
	FIND_VIDEO_PARAMETER(playParameters, "start_t ", parameter);
	start = parameter ? &parameter->value.time : 0;
	if (!start)
		start = &earliestVideoTime;
	FIND_VIDEO_PARAMETER(playParameters, "end_t   ", parameter);
	end = parameter ? &parameter->value.time : 0;
	if (!end)
		end = &latestVideoTime;
	if (currentTime) {
		if (memcmp(currentTime, start, sizeof(*start)) < 0)
			return memcmp(start, end, sizeof(*start)) > 0 ? -5 : -4;
		if (memcmp(currentTime, end, sizeof(*end)) <= 0)
			return 0;
		return -5;
	}
	if (memcmp(&earliestVideoTime, start, sizeof(*start)) == 0 &&
	    memcmp(&latestVideoTime, end, sizeof(*end)) == 0)
		return 0;
	return -3;
}

#define GET_VIDEO_PARAMETER_VALUE(tag, destination, member) do { \
	const MgVideoPlaybackParameter *match = 0; \
	if (elements) { \
		for (index = 0; index < count; index++) { \
			if (memcmp(elements[index].label, tag, 8) == 0) { \
				match = &elements[index]; \
				break; \
			} \
		} \
	} \
	*(destination) = match ? &match->member : 0; \
} while (0)

int getVideoPlaybackParameters(const MgVideoTaggedRecord *record,
			      const MgVideoPlaybackValue **period,
			      const MgVideoEncodedTime **start,
			      const MgVideoEncodedTime **end,
			      const MgVideoAvOut **avOut)
{
	const MgVideoPlaybackParameter *elements = 0;
	unsigned int count = 0, index;

	if (record) {
		elements = (const MgVideoPlaybackParameter *)record->payload;
		count = (record->descriptorCount[0] << 8) | record->descriptorCount[1];
	}
	if (period)
		GET_VIDEO_PARAMETER_VALUE("period  ", period, value);
	if (start)
		GET_VIDEO_PARAMETER_VALUE("start_t ", start, value.time);
	if (end)
		GET_VIDEO_PARAMETER_VALUE("end_t   ", end, value.time);
	if (avOut)
		GET_VIDEO_PARAMETER_VALUE("avout   ", avOut, value.avOut);
	return 0;
}

#define FIND_VIDEO_POLICY(found) do { \
	MgVideoPolicyList capabilities = *list; \
	int remaining = capabilities.count; \
	int low = 0, high = remaining - 1; \
	int half, middle, compared; \
	(found) = 0; \
	while (low <= high) { \
		half = remaining / 2; \
		middle = half + low; \
		compared = memcmp(item->label, capabilities.labels[middle], sizeof(item->label)); \
		remaining += ~half; \
		if (!compared) { \
			(found) = 1; \
			break; \
		} \
		if (compared < 0) { \
			high = middle - 1; \
			remaining = half; \
		} else { \
			low = middle + 1; \
		} \
	} \
} while (0)

int evaluateVideoPolicyList(const MgVideoTaggedRecord *record,
			   const MgVideoPolicyList *list)
{
	const MgVideoPolicyItem *item = (const MgVideoPolicyItem *)record->payload;
	const MgVideoPolicyItem *end = item +
		((record->descriptorCount[0] << 8) | record->descriptorCount[1]);
	unsigned int instruction;
	int found;

	if (item >= end)
		return -2;
	do {
		instruction = item->instruction;
		if (!(instruction & 0x80)) {
			if (!instruction)
				return -2;
			if (instruction == 1)
				return 0;
			FIND_VIDEO_POLICY(found);
			item += found ? 1 : instruction;
		} else {
			switch (instruction) {
			case 0x80:
				FIND_VIDEO_POLICY(found);
				if (found)
					return -2;
				break;
			case 0x81:
				FIND_VIDEO_POLICY(found);
				if (found)
					return 0;
				break;
			case 0x82:
				FIND_VIDEO_POLICY(found);
				if (!found)
					return classifyUnsupportedVideoPolicy(item->label);
				break;
			case 0x83:
				FIND_VIDEO_POLICY(found);
				if (!found)
					return 0;
				break;
			default:
				return -2;
			}
			item++;
		}
	} while (item < end);
	if (instruction == 0x81)
		return classifyUnsupportedVideoPolicy((item - 1)->label);
	return -2;
}

int validateVideoReferencePolicy(const MgVideoPolicyList *list,
				 const MgVideoTaggedRecord *referenceRecord,
				 const MgVideoTaggedRecord *policyRecord)
{
	MgVideoReferenceId reference;
	int requirement = 0, result;

	if (referenceRecord) {
		if (((referenceRecord->descriptorCount[0] << 8) |
		     referenceRecord->descriptorCount[1]) * 16 <= 15)
			return -2;
		reference = *(const MgVideoReferenceId *)referenceRecord->payload;
		if (reference.format == 2) {
			if (reference.bindingMode > 3)
				return -2;
			requirement = videoPolicyRecordRequirement[reference.bindingMode];
			if (requirement < 0)
				return requirement;
			if (requirement && policyRecord)
				return -1;
		} else if (reference.format || reference.encodedReferenceMetadata ||
			   reference.bindingMode || reference.encodedUsageMetadata) {
			return -2;
		}
	}
	if (!requirement && !policyRecord)
		return -2;
	if (policyRecord) {
		result = evaluateVideoPolicyList(policyRecord, list);
		return result & (result >> 31);
	}
	return 0;
}

int validateVideoBindingPolicy(int dialect, const void *input, unsigned int size)
{
	const MgVideoTaggedRecord *records[2];
	int result;

	if (dialect != 1)
		return -1;
	result = findVideoTaggedRecords(input, size, videoBindingRecordLabels, records, 2);
	if (result >= 0) {
		result = validateVideoReferencePolicy(&videoPlaybackPolicies,
						      records[0], records[1]);
		result &= result >> 31;
	}
	return result;
}

int getCurrentVideoTime(MgVideoEncodedTime *output, int secure);

int parseVideoProtectionInfo(int dialect, MgVideoKeySystemId *keySystemId,
			     const void *input, unsigned int size)
{
	const MgVideoTaggedRecord *records[5];
	MgVideoEncodedTime currentTime;
	MgVideoReferenceId reference;
	int result, clockResult;

	if (dialect != 1)
		return -1;
	result = findVideoTaggedRecords(input, size, videoProtectionRecordLabels,
				       records, 5);
	if (result < 0)
		return result;
	if (keySystemId) {
		if (!records[0] || ((records[0]->descriptorCount[0] << 8) |
				    records[0]->descriptorCount[1]) * 16 <= 31)
			return -2;
		*keySystemId = *(const MgVideoKeySystemId *)records[0]->payload;
	}
	if (records[1]) {
		if (((records[1]->descriptorCount[0] << 8) |
		     records[1]->descriptorCount[1]) * 16 <= 15)
			return -2;
		reference = *(const MgVideoReferenceId *)records[1]->payload;
		if (reference.format == 2) {
			if (reference.encodedReferenceMetadata)
				return -2;
		} else if (reference.format || reference.encodedReferenceMetadata ||
			   reference.bindingMode || reference.encodedUsageMetadata) {
			return -2;
		}
	}
	if (!records[2])
		return -2;
	result = evaluateVideoPolicyList(records[2], &videoPlaybackClockPolicies);
	if (result < 0)
		return result;
	clockResult = getCurrentVideoTime(&currentTime, 1);
	result = validateVideoPlaybackTime(records[3], records[4],
					 clockResult < 0 ? 0 : &currentTime);
	if (result >= 0)
		return 0;
	if (evaluateVideoPolicyList(records[2], &videoPlaybackPolicies) >= 0) {
		clockResult = getCurrentVideoTime(&currentTime, 0);
		result = validateVideoPlaybackTime(records[3], records[4],
						 clockResult < 0 ? 0 : &currentTime);
		result &= result >> 31;
	}
	return result;
}

int decodeVideoTime(SceRtcTick *output, const MgVideoEncodedTime *input);

int parseVideoPlaybackInfo(int dialect, int mode, MgVideoPlaybackQueryWorkspace *output,
			   const void *input, unsigned int size)
{
	const MgVideoTaggedRecord *records[4];
	const MgVideoPlaybackParameter *parameter;
	const MgVideoPlaybackValue *period;
	const MgVideoEncodedTime *start, *end, *periodStart, *periodEnd;
	const MgVideoAvOut *avOut;
	MgVideoReferenceId reference;
	int result;

	if (dialect != 1)
		return -1;
	if (mode != 1) {
		if (mode < 1 || mode > 4)
			return -1;
		result = findVideoTaggedRecords(input, size, videoReferenceBindingRecordLabels,
					       records, 2);
		if (result < 0)
			return result;
		if (mode != 2)
			return -1;
		result = validateVideoReferencePolicy(&videoPlaybackPolicies,
						      records[0], records[1]);
		if (result >= 0 || result == -7) {
			output->query.resultMarker = -1;
			output->query.zeroWords[0] = 0;
			output->query.zeroWords[1] = 0;
			result = 0;
		}
		return result;
	}
	result = findVideoTaggedRecords(input, size, videoPlaybackRecordLabels, records, 4);
	if (result < 0)
		return result;
	if (records[0]) {
		if (((records[0]->descriptorCount[0] << 8) |
		     records[0]->descriptorCount[1]) * 16 <= 15)
			return -2;
		reference = *(const MgVideoReferenceId *)records[0]->payload;
		if (reference.format == 2) {
			if (reference.encodedReferenceMetadata)
				return -2;
		} else if (reference.format || reference.encodedReferenceMetadata ||
			   reference.bindingMode || reference.encodedUsageMetadata) {
			return -2;
		}
	}
	if (!records[1])
		return -2;
	result = evaluateVideoPolicyList(records[1], &videoPlaybackClockPolicies);
	if (result < 0)
		return result;
	if (evaluateVideoPolicyList(records[1], &videoPlaybackPolicies) < 0)
		result = SceRtcForDriver_401C0954(&output->period.currentTick);
	else
		result = SceRtcForDriver_DEC408D4(&output->period.currentTick);
	if (result < 0)
		output->period.currentTick.tick = 0;
	result = getVideoPlaybackParameters(records[2], &period, &start, &end, &avOut);
	if (result < 0)
		return result;
	FIND_VIDEO_PARAMETER(records[3], "start_t2", parameter);
	periodStart = parameter ? &parameter->value.time : 0;
	FIND_VIDEO_PARAMETER(records[3], "end_t2  ", parameter);
	periodEnd = parameter ? &parameter->value.time : 0;
	if (decodeVideoTime(&output->period.startTick, start) < 0 ||
	    decodeVideoTime(&output->period.endTick, end) < 0)
		return -1;
	if (!avOut)
		avOut = &defaultVideoAvOut;
	output->period.encodedAvOut = *avOut;
	if (!period)
		period = &emptyVideoPlaybackValue;
	output->period.periodSeconds = 3600 * ((period->period.hours[0] << 8) |
					     period->period.hours[1]);
	if (decodeVideoTime(&output->period.periodStartTick, periodStart) < 0)
		return -1;
	if (decodeVideoTime(&output->period.periodEndTick, periodEnd) < 0)
		return -1;
	return 0;
}

int addVideoPeriod(MgVideoEncodedTime *time, unsigned int hours);

int extendVideoPlaybackPeriod(int dialect, void *input, unsigned int size,
			     unsigned int capacity)
{
	const MgVideoTaggedRecord *records[4];
	const MgVideoPlaybackParameter *parameter;
	const MgVideoPlaybackValue *period;
	MgVideoTaggedRecord *session;
	MgVideoPlaybackParameter *elements, *insertion;
	MgVideoEncodedTime currentTime, *start, *end;
	MgVideoReferenceId reference;
	unsigned int hours, count, newSize;
	int result, secure;

	if (dialect != 1 || capacity < size)
		return -1;
	result = findVideoTaggedRecords(input, size, videoPlaybackPeriodRecordLabels,
				       records, 4);
	if (result < 0)
		return result;
	result = getVideoPlaybackParameters(records[2], &period, 0, 0, 0);
	if (result < 0)
		return result;
	if (!period)
		period = &emptyVideoPlaybackValue;
	hours = (period->period.hours[0] << 8) | period->period.hours[1];
	if (!hours)
		return 0;
	if (records[0]) {
		if (((records[0]->descriptorCount[0] << 8) |
		     records[0]->descriptorCount[1]) * 16 <= 15)
			return -2;
		reference = *(const MgVideoReferenceId *)records[0]->payload;
		if (reference.format == 2) {
			if (reference.encodedReferenceMetadata)
				return -2;
		} else if (reference.format || reference.encodedReferenceMetadata ||
			   reference.bindingMode || reference.encodedUsageMetadata) {
			return -2;
		}
	}
	if (!records[1])
		return -2;
	result = evaluateVideoPolicyList(records[1], &videoPlaybackClockPolicies);
	if (result < 0)
		return result;
	secure = (unsigned int)evaluateVideoPolicyList(records[1],
						      &videoPlaybackPolicies) >> 31;
	if (getCurrentVideoTime(&currentTime, secure) < 0)
		return -3;
	session = (MgVideoTaggedRecord *)records[3];
	FIND_VIDEO_PARAMETER(session, "start_t2", parameter);
	start = parameter ? (MgVideoEncodedTime *)&parameter->value.time : 0;
	FIND_VIDEO_PARAMETER(session, "end_t2  ", parameter);
	end = parameter ? (MgVideoEncodedTime *)&parameter->value.time : 0;
	if (end && memcmp(&latestVideoTime, end, sizeof(*end)) != 0)
		return 0;
	if (!session) {
		newSize = size + sizeof(*session) + 2 * sizeof(*elements);
		if (capacity < newSize)
			return -1;
		session = (MgVideoTaggedRecord *)((uint8_t *)input + size);
		memcpy(session->label, "PLAY_S      ", sizeof(session->label));
		session->descriptorSize[0] = 0;
		session->descriptorSize[1] = 8;
		session->descriptorCount[0] = 0;
		session->descriptorCount[1] = 2;
		elements = (MgVideoPlaybackParameter *)session->payload;
		memcpy(elements[0].label, "start_t2", sizeof(elements[0].label));
		memcpy(elements[1].label, "end_t2  ", sizeof(elements[1].label));
		start = &elements[0].value.time;
		end = &elements[1].value.time;
	} else if (!start && !end) {
		newSize = size + 2 * sizeof(*elements);
		if (capacity < newSize)
			return -1;
		elements = (MgVideoPlaybackParameter *)session->payload;
		memmove(elements + 2, elements, (uint8_t *)input + size -
						(uint8_t *)elements);
		count = ((session->descriptorCount[0] << 8) |
			 session->descriptorCount[1]) + 2;
		memcpy(elements[0].label, "start_t2", sizeof(elements[0].label));
		session->descriptorCount[1] = count;
		session->descriptorCount[0] = count >> 8;
		memcpy(elements[1].label, "end_t2  ", sizeof(elements[1].label));
		start = &elements[0].value.time;
		end = &elements[1].value.time;
	} else if (!start || !end) {
		newSize = size + sizeof(*elements);
		if (capacity < newSize)
			return -1;
		if (start)
			insertion = (MgVideoPlaybackParameter *)(start + 1);
		else
			insertion = (MgVideoPlaybackParameter *)((uint8_t *)end -
					 offsetof(MgVideoPlaybackParameter, value));
		memmove(insertion + 1, insertion, (uint8_t *)input + size -
						  (uint8_t *)insertion);
		count = ((session->descriptorCount[0] << 8) |
			 session->descriptorCount[1]) + 1;
		session->descriptorCount[1] = count;
		session->descriptorCount[0] = count >> 8;
		if (start) {
			memcpy(insertion->label, "end_t2  ", sizeof(insertion->label));
			end = &insertion->value.time;
		} else {
			memcpy(insertion->label, "start_t2", sizeof(insertion->label));
			start = &insertion->value.time;
			end = (MgVideoEncodedTime *)((uint8_t *)end + sizeof(*insertion));
		}
	} else {
		newSize = size;
	}
	if ((int)newSize >= 0) {
		*start = currentTime;
		addVideoPeriod(&currentTime, hours);
		*end = currentTime;
	}
	return newSize;
}

unsigned int measureVideoTaggedData(const void *input, unsigned int size)
{
	const uint8_t *start = input;
	const uint8_t *end = start + (size & ~16U);
	const MgVideoTaggedRecord *record = input;
	unsigned int recordSize;

	if (start >= end)
		return 0;
	do {
		recordSize = sizeof(*record) * (((record->descriptorCount[0] << 8) |
					       record->descriptorCount[1]) + 1);
		if (recordSize == sizeof(*record) && !record->label[0] && !record->label[1] &&
		    !record->label[2] && !record->label[3] && !record->label[4] &&
		    !record->label[5] && !record->label[6] && !record->label[7] &&
		    !record->label[8] && !record->label[9] && !record->label[10] &&
		    !record->label[11] && !record->descriptorSize[0] &&
		    !record->descriptorSize[1])
			break;
		record = (const MgVideoTaggedRecord *)((const uint8_t *)record + recordSize);
	} while (end > (const uint8_t *)record);
	return (const uint8_t *)record - start;
}

int serializeVideoPlaybackPolicy(void *output, unsigned int capacity,
				 const MgVideoKeySystemId *keySystemId,
				 const MgVideoAvOut *avOut, int policyMode,
				 int bindOption, const char *comment)
{
	MgVideoTaggedRecord *record = output;
	MgVideoKeySystemPayload *identity;
	MgVideoReferenceId *reference;
	MgVideoPolicyItem *first, *item;
	MgVideoPlaybackParameter *parameter;
	unsigned int includeAvOut, count;

	if (capacity <= 223)
		return -1;
	includeAvOut = avOut->fields.controlFlags & 8;
	if (includeAvOut) {
		includeAvOut = 1;
		bindOption = 0;
	}
	if (comment) {
		char (*text)[16] = (char (*)[16])record->payload;

		record->descriptorSize[1] = 16;
		__builtin_memcpy(record->label, "!COMMENT    ", sizeof(record->label));
		record->descriptorSize[0] = 0;
		record->descriptorCount[0] = 0;
		record->descriptorCount[1] = 1;
		memset(*text, 0, sizeof(*text));
		strncpy(*text, comment, sizeof(*text) - 1);
		record = (MgVideoTaggedRecord *)((uint8_t *)record +
			 sizeof(*record) * (record->descriptorCount[1] + 1));
	}
	record->descriptorSize[1] = 16;
	__builtin_memcpy(record->label, "!CID        ", sizeof(record->label));
	record->descriptorSize[0] = 0;
	record->descriptorCount[1] = 2;
	record->descriptorCount[0] = 0;
	identity = (MgVideoKeySystemPayload *)record->payload;
	identity->keySystemId = *keySystemId;
	identity->zeroTrailer[0] = 0;
	identity->zeroTrailer[1] = 0;
	identity->zeroTrailer[2] = 0;
	identity->zeroTrailer[3] = 0;
	identity->zeroTrailer[4] = 0;
	identity->zeroTrailer[5] = 0;
	identity->zeroTrailer[6] = 0;
	identity->zeroTrailer[7] = 0;
	record = (MgVideoTaggedRecord *)(identity + 1);
	__builtin_memcpy(record->label, "!REFID      ", sizeof(record->label));
	record->descriptorSize[0] = 0;
	record->descriptorCount[1] = 1;
	record->descriptorSize[1] = 16;
	record->descriptorCount[0] = 0;
	reference = (MgVideoReferenceId *)record->payload;
	memset(reference, 0, sizeof(*reference));
	reference->format = 2;
	reference->encodedReferenceMetadata = 0;
	reference->bindingMode = 0;
	reference->encodedUsageMetadata = 0;
	reference->encodedMetadata[0] = 0;
	switch (policyMode) {
	case 1:
		reference->encodedUsageMetadata = 3;
		reference->encodedMetadata[0] = 3;
		reference->bindingMode = bindOption ? 2 : 3;
		break;
	case 2:
		reference->encodedMetadata[0] = 3;
		reference->bindingMode = bindOption ? 2 : 3;
		reference->encodedUsageMetadata = policyMode;
		break;
	case 0:
		reference->bindingMode = 2;
		reference->encodedUsageMetadata = 3;
		reference->encodedMetadata[0] = 3;
		break;
	default:
		return -1;
	}
	record = (MgVideoTaggedRecord *)(reference + 1);
	__builtin_memcpy(record->label, "PLAY_L      ", sizeof(record->label));
	record->descriptorSize[0] = 0;
	record->descriptorSize[1] = 16;
	record->descriptorCount[0] = 0;
	first = (MgVideoPolicyItem *)record->payload;
	item = first;
	if (!bindOption) {
		__builtin_memcpy(item->label, "T_SVR          ", sizeof(item->label));
		item->instruction = 0x82;
		item++;
	}
	if (includeAvOut) {
		item->instruction = 2;
		__builtin_memcpy(item->label, "P_PlayAVOUT    ", sizeof(item->label));
		__builtin_memcpy(item[1].label, "P_PlayAVOUT2   ", sizeof(item[1].label));
		item[1].instruction = 0x82;
		item += 2;
	}
	count = (uint8_t)(item + 1 - first);
	__builtin_memcpy(item->label, "T_MagicGateV   ", sizeof(item->label));
	item->instruction = 0x81;
	record->descriptorCount[1] = count;
	record = (MgVideoTaggedRecord *)(first + count);
	__builtin_memcpy(record->label, "PLAY_P      ", sizeof(record->label));
	record->descriptorSize[0] = 0;
	record->descriptorCount[1] = 1;
	record->descriptorSize[1] = 8;
	parameter = (MgVideoPlaybackParameter *)record->payload;
	__builtin_memcpy(parameter->label, "avout   ", sizeof(parameter->label));
	record->descriptorCount[0] = 0;
	parameter->value.avOut = *avOut;
	return (uint8_t *)(parameter + 1) - (uint8_t *)output;
}

#define ENCODE_VIDEO_BCD(value) (((value) % 10) | (((value) / 10) << 4))

MgVideoEncodedTime *encodeVideoTime(MgVideoEncodedTime *output, const SceDateTime *time)
{
	uint8_t component;

	component = (uint8_t)(time->year / 100U);
	output->yearHigh = ENCODE_VIDEO_BCD(component);
	component = (uint8_t)(time->year % 100U);
	output->yearLow = ENCODE_VIDEO_BCD(component);
	component = (uint8_t)time->month;
	output->month = ENCODE_VIDEO_BCD(component);
	component = (uint8_t)time->day;
	output->day = ENCODE_VIDEO_BCD(component);
	component = (uint8_t)time->hour;
	output->hour = ENCODE_VIDEO_BCD(component);
	component = (uint8_t)time->minute;
	output->minute = ENCODE_VIDEO_BCD(component);
	component = (uint8_t)time->second;
	output->second = ENCODE_VIDEO_BCD(component);
	component = (uint8_t)((int32_t)time->microsecond / 10000);
	output->hundredth = ENCODE_VIDEO_BCD(component);
	return output;
}

#define DECODE_VIDEO_COMPONENT(field, minimum, maximum) do { \
	units = input->field & 15; \
	if (units > 9) \
		goto error; \
	tens = input->field >> 4; \
	if (tens > 9) \
		goto error; \
	component = units + 10 * tens; \
	if (component - (minimum) > (maximum) - (minimum)) \
		goto error; \
	time.field = component; \
} while (0)

int decodeVideoTime(SceRtcTick *output, const MgVideoEncodedTime *input)
{
	SceDateTime time;
	unsigned int units, tens, yearLow, yearHigh, component, encodedYear;
	unsigned int lowUnits, lowTens;

	if (!input) {
		output->tick = 0;
		return 0;
	}
	encodedYear = input->yearLow | (input->yearHigh << 8);
	lowUnits = (uint8_t)encodedYear & 15;
	if (lowUnits > 9)
		goto error;
	lowTens = (uint8_t)encodedYear >> 4;
	if (lowTens > 9)
		goto error;
	units = (encodedYear >> 8) & 15;
	if (units > 9)
		goto error;
	tens = (encodedYear >> 8) >> 4;
	if (tens > 9)
		goto error;
	yearHigh = units + 10 * tens;
	yearLow = lowUnits + 10 * lowTens;
	component = yearLow + 100 * yearHigh;
	if (component - 1 > 9998)
		goto error;
	time.year = component;
	DECODE_VIDEO_COMPONENT(month, 1, 12);
	DECODE_VIDEO_COMPONENT(day, 1, 31);
	DECODE_VIDEO_COMPONENT(hour, 0, 23);
	DECODE_VIDEO_COMPONENT(minute, 0, 59);
	DECODE_VIDEO_COMPONENT(second, 0, 61);
	units = input->hundredth & 15;
	if (units > 9)
		goto error;
	tens = input->hundredth >> 4;
	if (tens > 9)
		goto error;
	time.microsecond = 10000 * (units + 10 * tens);
	if (SceRtcForDriver_F139E359(&time, output) >= 0)
		return 0;
error:
	return -1;
}

typedef struct MgVideoClockWorkspace {
	SceDateTime date;
	SceRtcTick tick;
} MgVideoClockWorkspace;

_Static_assert(sizeof(MgVideoClockWorkspace) == 24, "clock workspace size");
_Static_assert(offsetof(MgVideoClockWorkspace, tick) == 16, "clock workspace tick offset");

int addVideoPeriod(MgVideoEncodedTime *time, unsigned int hours)
{
	MgVideoClockWorkspace workspace;
	int result;

	if (decodeVideoTime(&workspace.tick, time) >= 0) {
		result = SceRtcForDriver_32E50C36(&workspace.tick, &workspace.tick, hours);
		if (result >= 0) {
			result = SceRtcForDriver_42A0DFCB(&workspace.date, &workspace.tick);
			if (result >= 0) {
				encodeVideoTime(time, &workspace.date);
				result = 0;
			}
		}
	} else {
		result = -2;
	}
	return result;
}

int getCurrentVideoTime(MgVideoEncodedTime *output, int secure)
{
	MgVideoClockWorkspace workspace;
	int result;

	if (secure)
		result = SceRtcForDriver_401C0954(&workspace.tick);
	else
		result = SceRtcForDriver_DEC408D4(&workspace.tick);
	if (result >= 0) {
		result = SceRtcForDriver_42A0DFCB(&workspace.date, &workspace.tick);
		if (result >= 0) {
			encodeVideoTime(output, &workspace.date);
			result = 0;
		}
	} else {
		result = -3;
	}
	return result;
}
