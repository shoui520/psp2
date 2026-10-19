/* Bluetooth SDP helpers; original source filename unavailable. */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "bt_internal.h"

extern const uint8_t gBtSdpBaseUuidSuffix[12];
extern const uint8_t gBtSdpServiceRecords[696];

uint8_t *BtReadSdpElement(uint8_t *data, int *length, uint8_t *type)
{
	int remaining = *length - 1;
	uint32_t size;
	uint8_t tag;

	if (remaining < 0)
		return NULL;
	tag = *data++;
	switch (tag) {
	case 0:
		size = 0;
		goto out;
	case 0x08:
	case 0x10:
	case 0x28:
		size = 1;
		break;
	case 0x09:
	case 0x11:
	case 0x19:
		size = 2;
		break;
	case 0x0a:
	case 0x12:
	case 0x1a:
		size = 4;
		break;
	case 0x0b:
	case 0x13:
		size = 8;
		break;
	case 0x0c:
	case 0x14:
	case 0x1c:
		size = 16;
		break;
	case 0x25:
	case 0x26:
	case 0x27:
	case 0x35:
	case 0x36:
	case 0x37:
	case 0x3d:
	case 0x3e:
	case 0x3f:
	case 0x45:
	case 0x46:
	case 0x47: {
		int width = 1 << ((tag & 7) - 5);
		uint8_t *end;

		size = 0;
		if (width <= 0)
			goto out;
		end = data + width;
		remaining--;
		if (*length != 1) {
			do {
				size = *data++ | (size << 8);
				if (data == end)
					break;
			} while (remaining-- != 0);
		}
		break;
	}
	default:
		return NULL;
	}
	if (remaining < (int32_t)size)
		return NULL;
out:
	*type = tag & 0xf8;
	*length = (int32_t)size;
	return data;
}

unsigned int BtFindSdpServiceRecords(const uint32_t *uuids, int uuidCount,
				     uint8_t *handles)
{
	const uint8_t *record = gBtSdpServiceRecords;
	unsigned int count = 0;

	for (;;) {
		const uint8_t *attribute = record + 1;
		uint32_t handle = 0;

		while (attribute < record + *record) {
			const uint8_t *attributeEnd;
			const uint8_t *element;
			int eligible;

			if (*attribute > 7 && attribute[1] == 0 &&
			    attribute[2] == 0 && attribute[3] == 0x0a) {
				handle = BtReadBe32(attribute + 4);
				attribute += *attribute;
				continue;
			}
			attributeEnd = attribute + *attribute;
			element = attribute + 3;
			eligible = handle != 0 && uuidCount > 0;
			while (element < attributeEnd) {
				switch (*element) {
				case 0x08:
				case 0x09:
				case 0x0a:
				case 0x0b:
				case 0x0c:
					element += (1 << (*element & 7)) + 1;
					break;
				case 0x19: {
					uint32_t uuid = BtReadBe16(element + 1);
					int index;
					unsigned int previous;

					if (eligible && count <= 31) {
						for (index = 0; index < uuidCount; index++) {
							if (uuids[index] == uuid)
								break;
						}
						if (index < uuidCount) {
							for (previous = 0; previous < count; previous++) {
								if (BtReadBe32(handles + 4 * previous) == handle)
									goto nextAttribute;
							}
							BtPack(handles + 4 * count++, 4, "W", handle);
						}
					}
					element += 3;
					attributeEnd = attribute + *attribute;
					break;
				}
				case 0x25:
					element += element[1] + 2;
					break;
				case 0x35:
					element += 2;
					break;
				default:
					goto nextAttribute;
				}
			}
nextAttribute:
			attribute = attributeEnd;
		}
		record += *record;
		if (record >= gBtSdpServiceRecords + sizeof(gBtSdpServiceRecords))
			return count;
	}
}

uint8_t *BtReadSdpUuids(uint8_t *data, uint8_t *end, uint32_t *uuids, int *count)
{
	uint32_t size = 0;
	uint8_t *limit;
	unsigned int tag;
	int index = 0;

	if (end - data <= 3)
		return NULL;
	tag = *data++;
	if (tag == 0x36)
		goto readHigh;
	if (tag == 0x37) {
		size = (uint32_t)data[0] << 24 | (uint32_t)data[1] << 16;
		data += 2;
		goto readHigh;
	}
	if (tag != 0x35)
		return NULL;
	goto readLow;
readHigh:
	size |= (uint32_t)*data++ << 8;
readLow:
	size |= *data++;
	if ((int32_t)size > end - data)
		return NULL;
	limit = (uint8_t *)((uintptr_t)data + size);
	if (!data)
		return NULL;
	if ((uintptr_t)data < (uintptr_t)limit) {
		do {
			uint8_t *value = data + 1;

			tag = *data;
			switch (tag) {
			case 0x19:
				data += 3;
				if ((uintptr_t)data > (uintptr_t)limit)
					return NULL;
				uuids[index++] = BtReadBe16(value);
				break;
			case 0x1a:
				data += 5;
				if ((uintptr_t)data > (uintptr_t)limit)
					return NULL;
				uuids[index++] = BtReadBe32(value);
				break;
			case 0x1c: {
				uint8_t *next = data + 17;
				int different;

				if ((uintptr_t)next > (uintptr_t)limit)
					return NULL;
				different = memcmp(gBtSdpBaseUuidSuffix, data + 5, 12);
				data = next;
				if (!different)
					uuids[index++] = BtReadBe32(value);
				break;
			}
			default:
				return NULL;
			}
		} while ((uintptr_t)data < (uintptr_t)limit && index <= 15);
	}
	*count = index;
	return data;
}

uint8_t *BtReadSdpAttributeRanges(uint8_t *data, uint8_t *end,
				uint16_t *lower, uint16_t *upper, int *count)
{
	uint32_t size = 0;
	uint8_t *limit;
	unsigned int tag;
	int index = 0;

	if (end - data <= 3)
		return NULL;
	tag = *data++;
	if (tag == 0x36)
		goto readHigh;
	if (tag == 0x37) {
		size = (uint32_t)data[0] << 24 | (uint32_t)data[1] << 16;
		data += 2;
		goto readHigh;
	}
	if (tag != 0x35)
		return NULL;
	goto readLow;
readHigh:
	size |= (uint32_t)*data++ << 8;
readLow:
	size |= *data++;
	if ((int32_t)size > end - data)
		return NULL;
	limit = (uint8_t *)((uintptr_t)data + size);
	if (!data)
		return NULL;
	if ((uintptr_t)data < (uintptr_t)limit) {
		do {
			uint8_t *value = data + 1;

			tag = *data;
			if (tag == 9) {
				uint16_t attribute;

				data += 3;
				if ((uintptr_t)data > (uintptr_t)limit)
					return NULL;
				attribute = BtReadBe16(value);
				lower[index] = attribute;
				upper[index] = attribute;
			} else {
				uint8_t *next = data + 5;

				if (tag != 10 || (uintptr_t)next > (uintptr_t)limit)
					return NULL;
				lower[index] = BtReadBe16(value);
				value = data + 3;
				data = next;
				upper[index] = BtReadBe16(value);
			}
			index++;
		} while ((uintptr_t)data < (uintptr_t)limit && index != 16);
	}
	*count = index;
	return data;
}

int BtGetLocalServiceClasses(uint8_t *output)
{
	const uint8_t *record = gBtSdpServiceRecords;
	uint8_t *start = output;

	do {
		const uint8_t *attribute = record + 1;
		const uint8_t *end = record + *record;

		while (attribute < end) {
			const uint8_t *attributeEnd;

			if (attribute[1] != 0 || attribute[2] != 1) {
				attribute += *attribute;
				continue;
			}
			attributeEnd = attribute + *attribute;
			if (attribute[3] == 0x35 && attribute + 5 < attributeEnd &&
			    attribute[5] == 0x19) {
				const uint8_t *uuid = attribute + 6;
				const uint8_t *next;

				do {
					unsigned int serviceClass = BtReadBe16(uuid);

					output[0] = serviceClass;
					output[1] = serviceClass >> 8;
					output += 2;
					next = uuid + 2;
					uuid += 3;
				} while (attribute + *attribute > next && next[0] == 0x19);
			}
			attribute = attributeEnd;
		}
		record = end;
	} while (record < gBtSdpServiceRecords + sizeof(gBtSdpServiceRecords));
	return output - start;
}
