/* Registry data is adapted from pristine SSLeay; private lookup names are ours. */
#include "objects.h"
#include "../memory.h"
#include "../format.h"
#include "../CRYPTOCME/common/object.h"
#include "obj_dat_sce.inc"

static int sslCompareObjectNames(const char *left, const char *right,
				 SceUInt32 length)
{
	SceUInt32 i;
	char a, b;

	for (i = 0; i < length; i++) {
		a = left[i];
		b = right[i];
		if (a >= 'a' && a <= 'z')
			a = (char)(a - 32);
		if (b >= 'a' && b <= 'z')
			b = (char)(b - 32);
		if (a != b)
			return a - b;
	}
	return 0;
}

int sslGetObjectNid(const void *oid, SceUInt32 length, int *result)
{
	int status = 10008;
	int i;

	if (oid == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (length == 0)
		return 10022;
	for (i = 0; i < NUM_NID; i++) {
		if (length == (SceUInt32)nid_objs[i].length &&
		    sslCompareMemory(oid, nid_objs[i].data, length) == 0) {
			status = 0;
			*result = i;
			break;
		}
	}
	return status;
}

int sslGetObjectShortName(const void *oid, SceUInt32 length, const char **result)
{
	int status = 10008;
	int i;

	if (oid == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (length == 0)
		return 10022;
	for (i = 0; i < NUM_NID; i++) {
		if (length == (SceUInt32)nid_objs[i].length &&
		    sslCompareMemory(oid, nid_objs[i].data, length) == 0) {
			*result = nid_objs[i].sn;
			status = 0;
			break;
		}
	}
	return status;
}

int sslGetObjectLongName(const void *oid, SceUInt32 length, const char **result)
{
	int status = 10008;
	int i;

	if (oid == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (length == 0)
		return 10022;
	for (i = 0; i < NUM_NID; i++) {
		if (length == (SceUInt32)nid_objs[i].length &&
		    sslCompareMemory(oid, nid_objs[i].data, length) == 0) {
			status = 0;
			*result = nid_objs[i].ln;
			break;
		}
	}
	return status;
}

int sslFindObjectByShortName(const char *name, SceUInt32 length,
			     const unsigned char **oid, SceUInt32 *oidLength)
{
	int status = 10008;
	int i;

	if (name == NULL || oid == NULL || oidLength == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	for (i = 0; i < NUM_NID; i++) {
		if (nid_objs[i].sn != NULL &&
		    sslStringLength(nid_objs[i].sn) == length &&
		    sslCompareObjectNames(name, nid_objs[i].sn, length) == 0) {
			*oid = nid_objs[i].data;
			*oidLength = (SceUInt32)nid_objs[i].length;
			status = 0;
			break;
		}
	}
	return status;
}

int sslFindObjectByLongName(const char *name, SceUInt32 length,
			    const unsigned char **oid, SceUInt32 *oidLength)
{
	int status = 10008;
	int i;

	if (name == NULL || oid == NULL || oidLength == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	for (i = 0; i < NUM_NID; i++) {
		if (nid_objs[i].ln != NULL &&
		    sslStringLength(nid_objs[i].ln) == length &&
		    sslCompareObjectNames(name, nid_objs[i].ln, length) == 0) {
			*oid = nid_objs[i].data;
			*oidLength = (SceUInt32)nid_objs[i].length;
			status = 0;
			break;
		}
	}
	return status;
}

int sslGetObjectEncodingByNid(int nid, const unsigned char **oid,
			      SceUInt32 *oidLength)
{
	int status = 0;

	if (oid == NULL || oidLength == NULL) {
		status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	} else if (nid < 0 || nid >= NUM_NID) {
		status = 10018;
	} else {
		*oid = nid_objs[nid].data;
		*oidLength = (SceUInt32)nid_objs[nid].length;
	}
	return status;
}

int sslNewObjectIdentifier(void *unused, SslObjectIdentifier **result)
{
	unsigned char *bytes;
	unsigned int i;

	(void)unused;
	if (result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	*result = sslAllocate(sizeof(**result));
	if (*result == NULL)
		return 10005;
	bytes = (unsigned char *)*result;
	for (i = 0; i < sizeof(**result); i++)
		bytes[i] = 0;
	(*result)->flags |= 1u;
	return 0;
}

int sslFreeObjectIdentifier(SslObjectIdentifier *object)
{
	if (object != NULL) {
		if ((object->flags & 2u) != 0 && object->data != NULL)
			sslFree(object->data);
		if ((object->flags & 1u) != 0)
			sslFree(object);
	}
	return 0;
}

int SceSslInternal_4D80D3F7(void *oid, int mode,
			    const void *data, SceSize length)
{
	SslObjectIdentifier *object = oid;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (data == NULL) {
		object->data = NULL;
		object->length = 0;
		object->flags &= ~2u;
	} else if (mode == 0) {
		object->data = sslAllocate(length);
		if (object->data == NULL)
			return 10005;
		sslCopyMemory(object->data, data, length);
		object->length = length;
		object->flags |= 2u;
	} else if (mode == 1) {
		object->data = (void *)data;
		object->length = length;
		object->flags &= ~2u;
	} else if (mode == 2) {
		object->data = (void *)data;
		object->length = length;
		object->flags |= 2u;
	} else {
		return 10018;
	}
	return 0;
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

int SceSslInternal_3DE4F3DB(void *oid, int mode,
			    SceSize capacity, char *data)
{
	SslObjectIdentifier *object = oid;
	const char *name = NULL;
	int status;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	switch (mode) {
	case 0:
		status = sslGetObjectLongName(object->data, object->length, &name);
		break;
	case 1:
		status = sslGetObjectShortName(object->data, object->length, &name);
		break;
	case 2:
		return sslFormatObjectIdentifier(object->data, object->length,
						 capacity, data);
	default:
		return 10018;
	}
	if (status == 0) {
		if (sslStringLength(name) + 1 <= capacity)
			sslCopyString(data, name, capacity);
		else
			status = 10016;
	}
	return status;
}
