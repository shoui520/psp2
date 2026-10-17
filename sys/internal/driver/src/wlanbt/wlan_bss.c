/* BSS descriptor methods; this filename is inferred from the function family. */
#include "wlanbt_internal.h"
#include <string.h>

/* Sony's 4.00 BSS stack-buffer copies use the checked object-size interface. */
#if FW_VERSION >= 0x04000000
#include <sysmem/fortify.h>
#endif

static const uint8_t gWmmElementSelector[4] = { 0x00, 0x50, 0xf2, 0x02 };
const uint8_t gBssTypeMapping[3] = { 1, 2, 3 };

/* Marvell background-scan report followed by variable BSS records. */
typedef struct WlanBackgroundScanResponse {
	uint8_t reportCondition[4];
	uint8_t bssDescriptorSize[2];
	uint8_t bssCount;
	uint8_t records[];
} WlanBackgroundScanResponse;

_Static_assert(sizeof(WlanBackgroundScanResponse) == 7, "background scan response header");
_Static_assert(offsetof(WlanBackgroundScanResponse, bssDescriptorSize) == 4,
	       "background scan record bytes");
_Static_assert(offsetof(WlanBackgroundScanResponse, bssCount) == 6,
	       "background scan record count");
_Static_assert(offsetof(WlanBackgroundScanResponse, records) == 7,
	       "background scan variable records");

int wlanParseSecurityInformationElement(WlanSecurityInfo *security,
				       const WlanInformationElement *element);
int wlanCopyInformationElement(const void *buffer, unsigned int length,
			       unsigned int id, void *output,
			       unsigned int *outputSize, unsigned int *offset);

int wlanBssHasPrivacy(const WlanBssDesc *descriptor)
{
	return descriptor->capabilities & 0x10;
}

int wlanBssHasWpa(const WlanBssDesc *descriptor)
{
	return descriptor->wpa.enabled;
}

int wlanBssHasRsn(const WlanBssDesc *descriptor)
{
	return descriptor->rsn.enabled;
}

int wlanBssHasWmm(const WlanBssDesc *descriptor)
{
	uint8_t payload[255];
	unsigned int length = descriptor->informationElementsSize;
	const uint8_t *buffer = descriptor->informationElements;
	unsigned int offset = 0;
	unsigned int remaining, consumed, payloadSize;
	const uint8_t *start;
	const WlanInformationElement *element;

	for (;;) {
		start = buffer + offset;
		remaining = length - offset;
		consumed = 0;
		do {
			element = (const void *)(start + consumed);
			consumed += sizeof(*element);
			if (remaining < consumed)
				return 0;
			payloadSize = element->length;
			consumed += payloadSize;
			if (remaining < consumed)
				return 0;
		} while (element->id != 221);
		offset += consumed;
		if (!payloadSize)
			continue;
		memcpy(payload, element->data, payloadSize);
		if (payloadSize > 3 &&
		    memcmp(payload, gWmmElementSelector, sizeof(gWmmElementSelector)) == 0)
			return 1;
	}
}

int wlanBssSetInformationElements(WlanBssDesc *descriptor, const void *buffer,
				 unsigned int length, const uint32_t *mask)
{
	const uint8_t *next = buffer;
	const uint8_t *end = next + length;
	uint8_t *output = descriptor->informationElements;

	descriptor->informationElementsSize = 0;
	descriptor->wpa = (WlanSecurityInfo){0};
	descriptor->rsn = (WlanSecurityInfo){0};
	memset(&descriptor->ht, 0, sizeof(descriptor->ht));
	if (mask) {
		while ((uintptr_t)end >= (uintptr_t)next + sizeof(WlanInformationElement)) {
			const WlanInformationElement *element = (const void *)next;
			unsigned int size = element->length + sizeof(*element);
			unsigned int id;

			next += size;
			if ((uintptr_t)end < (uintptr_t)next)
				return 0;
			id = element->id;
			if (id == 48) {
				if (!descriptor->rsn.enabled)
					wlanParseSecurityInformationElement(&descriptor->rsn, element);
			} else if (id == 221) {
				if (!descriptor->wpa.enabled)
					wlanParseSecurityInformationElement(&descriptor->wpa, element);
			} else if (id == 45) {
				descriptor->ht.enabled = 1;
			}
			id = element->id;
			if (!(mask[id >> 5] & (1u << (id & 31))))
				continue;
			memcpy(output, element, size);
			output += size;
			descriptor->informationElementsSize += size;
		}
	} else {
		while ((uintptr_t)end >= (uintptr_t)next + sizeof(WlanInformationElement)) {
			const WlanInformationElement *element = (const void *)next;

			next += element->length + sizeof(*element);
			if ((uintptr_t)end < (uintptr_t)next)
				break;
			switch (element->id) {
			case 48:
				if (!descriptor->rsn.enabled)
					wlanParseSecurityInformationElement(&descriptor->rsn, element);
				break;
			case 221:
				if (!descriptor->wpa.enabled)
					wlanParseSecurityInformationElement(&descriptor->wpa, element);
				break;
			case 45:
				descriptor->ht.enabled = 1;
				break;
			}
		}
	}
	return 0;
}

int wlanCollectScanRecords(const WlanScanRecord *record, unsigned int capacity,
			  const void *end, const WlanScanRecord **records)
{
	unsigned int count = 0;
	unsigned int last, i, j;

	if (!capacity)
		return 0;
	while ((uintptr_t)record + sizeof(*record) <= (uintptr_t)end) {
		unsigned int length = record->ieLength[0] | record->ieLength[1] << 8;
		unsigned int size = sizeof(*record);

		if (length >= sizeof(*record) - sizeof(record->ieLength) &&
		    (uintptr_t)record + length + sizeof(record->ieLength) > (uintptr_t)end)
			break;
		records[count++] = record;
		length = record->ieLength[0] | record->ieLength[1] << 8;
		if (length >= sizeof(*record) - sizeof(record->ieLength))
			size = length + sizeof(record->ieLength);
		record = (const void *)((const uint8_t *)record + size);
		if (count == capacity)
			break;
	}
#if FW_VERSION >= 0x04000000
	if (!count)
		return 0;
#endif
	last = count - 1;
	if (!last)
		return count;
	for (i = 0; i < last; ++i) {
		for (j = last; j > i; --j) {
			const WlanScanRecord *left = records[j - 1];
			const WlanScanRecord *right = records[j];

			if (left->rssi > right->rssi) {
				records[j - 1] = right;
				records[j] = left;
			}
		}
	}
	return count;
}

int wlanBuildScanResult(const WlanScanRecord *record, unsigned int recordCount,
		       const void *recordEnd, WlanScanResult *result,
		       unsigned int capacity, const uint32_t *mask)
{
	const WlanScanRecord *records[16];
	WlanBssDesc *descriptor;
	uintptr_t end;
	unsigned int count, completed = 0;
	unsigned int length, outputSize, offset;
	uint8_t channel;
	int status = 0;

	if (!result)
		return 0;
	memset(records, 0, sizeof(records));
	end = (uintptr_t)result + capacity;
	if (recordCount >= 16)
		count = 16;
	else
		count = recordCount;
	count = wlanCollectScanRecords(record, count,
				       recordEnd, records);
	result->bssNum = 0;
	descriptor = (void *)result->bssDesc;
	while (completed < count && end >= (uintptr_t)descriptor + sizeof(*descriptor)) {
		memset(descriptor, 0, sizeof(*descriptor));
		record = records[completed];
		length = record->ieLength[0] | record->ieLength[1] << 8;
		length = length >= 19 ? length - 19 : 0;
		outputSize = sizeof(descriptor->ssid);
		offset = 0;
		if (wlanCopyInformationElement(record->informationElements, length, 0,
					      descriptor->ssid, &outputSize, &offset) == 0)
			descriptor->ssidSize = outputSize;
		descriptor->bssid = records[completed]->bssid;
		record = records[completed];
		length = record->ieLength[0] | record->ieLength[1] << 8;
		length = length >= 19 ? length - 19 : 0;
		channel = 0;
		outputSize = sizeof(channel);
		offset = 0;
		wlanCopyInformationElement(record->informationElements, length, 3,
					   &channel, &outputSize, &offset);
		descriptor->channel = channel;
		record = records[completed];
		descriptor->capabilities = record->capabilities[0] | record->capabilities[1] << 8;
		descriptor->bssMode = record->capabilities[0] & 2 ? 2 : 1;
		descriptor->beaconInterval = record->beaconInterval[0] | record->beaconInterval[1] << 8;
		descriptor->rssi = record->rssi;
		if (record->rssi > 99)
			descriptor->signalQuality = 1;
		else if (record->rssi > 50)
			descriptor->signalQuality = 2 * (100 - record->rssi);
		else
			descriptor->signalQuality = 100;
		length = record->ieLength[0] | record->ieLength[1] << 8;
		length = length >= 19 ? length - 19 : 0;
		outputSize = sizeof(descriptor->supportedRates);
		offset = 0;
		status = wlanCopyInformationElement(record->informationElements, length, 1,
						   descriptor->supportedRates, &outputSize, &offset);
		if (status < 0)
			return status;
		record = records[completed];
		length = record->ieLength[0] | record->ieLength[1] << 8;
		length = length >= 19 ? length - 19 : 0;
		outputSize = sizeof(descriptor->extendedRates);
		offset = 0;
		wlanCopyInformationElement(record->informationElements, length, 50,
					   descriptor->extendedRates, &outputSize, &offset);
		record = records[completed];
		length = record->ieLength[0] | record->ieLength[1] << 8;
		length = length >= 19 ? length - 19 : 0;
		if (mask && end < (uintptr_t)descriptor + sizeof(*descriptor) + length)
			break;
		status = wlanBssSetInformationElements(descriptor, record->informationElements,
						      length, mask);
		if (status < 0)
			return status;
		length = (descriptor->informationElementsSize + 3u) & ~3u;
		descriptor->size = sizeof(*descriptor) + length;
		descriptor = (void *)((uint8_t *)descriptor + sizeof(*descriptor) + length);
		++completed;
	}
	result->bssNum = completed;
	return status;
}

int robinWlanReadBackgroundScanResponse(SceWlanBtRobinWlanContext *context,
				      SceWlanBtRobinWlanCommand *command,
				      WlanScanResult *result, unsigned int capacity)
{
	const uint32_t *mask = context->tcm.informationElementMask;
	const WlanBackgroundScanResponse *response;
	const WlanScanRecord *record;
	const uint8_t *recordEnd, *responseEnd;
	unsigned int size, lengthLow, lengthHigh, count;

	if (!context->tcm.informationElementMaskEnabled)
		mask = NULL;
	size = command->responseSize;
	if (size < sizeof(*response))
		return 0x80418001;
	response = command->responsePayload;
	lengthLow = response->bssDescriptorSize[0];
	record = (const void *)response->records;
	lengthHigh = response->bssDescriptorSize[1];
	responseEnd = (const uint8_t *)response + size;
	count = response->bssCount;
	recordEnd = (const uint8_t *)record + (lengthLow | lengthHigh << 8);
	if ((uintptr_t)recordEnd > (uintptr_t)responseEnd)
		return 0x80418001;
	return wlanBuildScanResult(record, count, recordEnd, result, capacity, mask);
}

SceWlanBtRobinWlanCommand *robinWlanAllocateCommand(SceWlanBtRobinWlanContext *context,
						 unsigned int size);
int robinWlanSubmitCommand(SceWlanBtRobinWlanContext *context,
	SceWlanBtRobinWlanCommand *command, unsigned int commandId, unsigned int size);
int robinWlanWaitCommand(SceWlanBtRobinWlanContext *context,
			SceWlanBtRobinWlanCommand *command);
int robinWlanReleaseCommand(SceWlanBtRobinWlanContext *context,
			   SceWlanBtRobinWlanCommand *command);

int robinWlanReadBackgroundScan(SceWlanBtRobinWlanContext *context,
			       WlanScanResult *result, unsigned int capacity)
{
	SceWlanBtRobinWlanCommand *command = robinWlanAllocateCommand(context, 1);
	SceWlanBtRobinWlanBackgroundScanQuery *request;
	const WlanBackgroundScanResponse *response;
	const uint32_t *mask;
	const uint8_t *recordEnd;
	unsigned int size;
	int status;

	if (!command)
		return 0x80418005;
	request = command->requestPayload;
	command->callback = NULL;
	request->flush = 1;
	status = robinWlanSubmitCommand(context, command, 0x6c, sizeof(*request));
	if (status < 0)
		goto out;
	status = robinWlanWaitCommand(context, command);
	if (status < 0)
		goto release;
	mask = context->tcm.informationElementMask;
	if (!context->tcm.informationElementMaskEnabled)
		mask = NULL;
	size = command->responseSize;
	if (size < sizeof(*response))
		goto invalid;
	response = command->responsePayload;
	recordEnd = response->records + (response->bssDescriptorSize[0] |
		response->bssDescriptorSize[1] << 8);
	if ((uintptr_t)recordEnd > (uintptr_t)response + size)
		goto invalid;
	status = wlanBuildScanResult((const WlanScanRecord *)response->records,
		response->bssCount, recordEnd, result, capacity, mask);
	goto release;
invalid:
	status = 0x80418001;
release:
	robinWlanReleaseCommand(context, command);
out:
	return status;
}

int wlanMapBssType(uint8_t bssType)
{
	unsigned int index = (uint8_t)(bssType - 1);

	if (index > 2)
		return 0;
	return gBssTypeMapping[index];
}
