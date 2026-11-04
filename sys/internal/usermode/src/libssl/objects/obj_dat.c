/* Registry data is adapted from pristine SSLeay; private lookup name is ours. */
#include "objects.h"
#include "../memory.h"
#include "../CRYPTOCME/common/object.h"
#include "obj_dat_sce.inc"

int sslGetObjectShortName(const void *oid, SceUInt32 length, const char **result)
{
	int i;

	if (oid == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (length == 0)
		return 10022;
	for (i = 0; i < NUM_NID; i++) {
		if (length == (SceUInt32)nid_objs[i].length &&
		    sslCompareMemory(oid, nid_objs[i].data, length) == 0) {
			*result = nid_objs[i].sn;
			return 0;
		}
	}
	return 10008;
}

int sslAppendObjectArc(SceUInt32 value, SceUInt32 capacity,
		       SceUInt32 *position, char *data)
{
	SceUInt32 start = *position;
	SceUInt32 tail = capacity - 2;
	SceUInt32 count;

	if (tail >= start) {
		do {
			data[tail] = value % 10 + '0';
			value /= 10;
			start = *position;
			if (value == 0)
				break;
			tail--;
		} while (tail >= start);
	}
	if (tail < start)
		return 10016;
	count = capacity - tail - 1;
	sslMoveMemory(data + start, data + tail, count);
	*position += count;
	data[*position] = '\0';
	return 0;
}

int sslFormatObjectIdentifier(const void *oid, SceUInt32 length,
			      SceUInt32 capacity, char *data)
{
	const SceUInt8 *encoding = oid;
	SceUInt32 index = 0, value = 0, position = 0, first, start;
	SceUInt8 byte;
	int status;

	do {
		if (index >= length)
			return 10022;
		byte = encoding[index++];
		value = (value << 7) | (byte & 127);
	} while (byte & 128);
	if (value >= 80) {
		first = 2;
		value -= 80;
	} else {
		first = value / 40;
		value %= 40;
	}
	status = sslAppendObjectArc(first, capacity, &position, data);
	if (status != 0)
		return status;
	index = 1;
	for (;;) {
		start = position;
		if (position + 1 >= capacity)
			return 10016;
		position++;
		data[start] = '.';
		status = sslAppendObjectArc(value, capacity, &position, data);
		if (status != 0 || index >= length)
			return status;
		value = 0;
		do {
			if (index >= length)
				return 10022;
			byte = encoding[index++];
			value = (value << 7) | (byte & 127);
		} while (byte & 128);
	}
}
