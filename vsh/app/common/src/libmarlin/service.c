/*
 * The service messages: their big-endian header words, the checks of a reply's
 * arguments and the accessors of each argument's value. The file's name is ours.
 */
#include "mln_internal.h"

/* a big-endian word into p, its lowest byte first */
static inline __attribute__((always_inline)) void MlnServicePutWord(unsigned char *p, unsigned int value)
{
	p[3] = value;
	p[0] = value >> 24;
	p[1] = value >> 16;
	p[2] = value >> 8;
}

void MlnImpl_81026BFC(unsigned char *header, unsigned int command)
{
	MlnServicePutWord(header, command);
}

void MlnImpl_81026C1C(unsigned char *header, unsigned int size)
{
	MlnServicePutWord(header + 4, size);
}

void MlnImpl_81026C3C(unsigned char *header, unsigned int status)
{
	MlnServicePutWord(header + 8, status);
}

void MlnImpl_81026C5C(unsigned char *header, unsigned int value)
{
	MlnServicePutWord(header + 12, value);
}

void MlnImpl_81026C7C(unsigned char *header, unsigned int first)
{
	MlnServicePutWord(header, first);
}

void MlnImpl_81026C9C(unsigned char *header, unsigned int second)
{
	MlnServicePutWord(header + 4, second);
}

unsigned int MlnImpl_81026CBC(const unsigned char *header)
{
	return MLN_BE32(header);
}

unsigned int MlnImpl_81026CE0(const unsigned char *header)
{
	return MLN_BE32(header + 4);
}

int MlnImpl_81026D04(const unsigned char *header)
{
	return MLN_BE32(header + 8);
}

unsigned int MlnImpl_81026D28(const unsigned char *header)
{
	return MLN_BE32(header + 12);
}

unsigned int MlnImpl_81026D4C(const unsigned char *header)
{
	return MLN_BE32(header);
}

unsigned int MlnImpl_81026D70(const unsigned char *header)
{
	return MLN_BE32(header + 4);
}

/* 3 unless the reply carries count values (with none, it may be missing) */
int MlnImpl_81026D94(const MlnServiceReply *reply, unsigned int count)
{
	if (count != 0) {
		if (reply == NULL || reply->count != count || reply->items == NULL)
			return 3;
	} else if (reply != NULL && reply->count != 0) {
		return 3;
	}
	return 0;
}

int MlnImpl_81026DE0(int heap, const MlnServiceArgs *args, MlnBuffer **out)
{
	if (args == NULL || args->count == 0 || args->items == NULL || out == NULL)
		return 3;
	return MlnImpl_81027B50(heap, args, out);
}

/* count empty arguments */
int MlnImpl_81026E20(int heap, unsigned int count, MlnServiceArgs **out)
{
	MlnServiceArg *items = NULL;
	MlnServiceArgs *args = NULL;
	int ret;

	if (count == 0 || out == NULL) {
		ret = 3;
		goto out;
	}
	if (count > 0xffffffff / sizeof(*items)) {
		ret = 3;
		goto out;
	}
	items = MlnAllocate(heap, count * sizeof(*items));
	if (items == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(items, 0, count * sizeof(*items));
	args = MlnAllocate(heap, sizeof(*args));
	if (args == NULL) {
		ret = 2;
		goto out;
	}
	args->items = items;
	items = NULL;
	args->count = count;
	ret = 0;
	*out = args;
	args = NULL;
out:
	MlnFree(heap, items);
	MlnFree(heap, args);
	return ret;
}

/* the accessors: 3 unless the value is of the type; values held by pointer are handed over */
int MlnImpl_81026ED0(int heap, MlnServiceArg *value, MlnBufferGroup **out)
{
	if (value == NULL || out == NULL || value->type != MLN_SERVICE_ARG_GROUP)
		return 3;
	*out = value->value;
	value->value = NULL;
	return 0;
}

int MlnImpl_81026F0C(int heap, MlnServiceArg *value, MlnBuffer **out)
{
	if (value == NULL || out == NULL || value->type != MLN_SERVICE_ARG_BUFFER)
		return 3;
	*out = value->value;
	value->value = NULL;
	return 0;
}

int MlnImpl_81026F48(int heap, const MlnServiceArg *value, MlnServiceRecord *out)
{
	if (value == NULL || out == NULL)
		return 3;
	if (value->type != MLN_SERVICE_ARG_RECORD21)
		return 3;
	*out = *(const MlnServiceRecord *)value->value;
	return 0;
}

int MlnImpl_81026FAC(int heap, const MlnServiceArg *value, int *out)
{
	if (value == NULL || out == NULL)
		return 3;
	if (value->type != MLN_SERVICE_ARG_INTEGER)
		return 3;
	*out = *(const int *)value->value;
	return 0;
}

int MlnImpl_81026FE4(int heap, const MlnServiceArg *value, unsigned int *out)
{
	if (value == NULL || out == NULL)
		return 3;
	if (value->type != MLN_SERVICE_ARG_ID)
		return 3;
	*out = *(const unsigned int *)value->value;
	return 0;
}

int MlnImpl_8102701C(int heap, const MlnServiceArg *value, int *out)
{
	if (value == NULL || out == NULL)
		return 3;
	if (value->type != MLN_SERVICE_ARG_HANDLE)
		return 3;
	*out = *(const int *)value->value;
	return 0;
}

int MlnImpl_8102704C(int heap, const MlnServiceArg *value, unsigned int *out)
{
	if (value == NULL || out == NULL)
		return 3;
	if (value->type != MLN_SERVICE_ARG_SIZE)
		return 3;
	*out = *(const unsigned int *)value->value;
	return 0;
}

int MlnImpl_81027084(int heap, const MlnServiceArg *value, int *out)
{
	if (value == NULL || out == NULL)
		return 3;
	if (value->type != MLN_SERVICE_ARG_FLAG)
		return 3;
	*out = *(const int *)value->value;
	return 0;
}

int MlnImpl_810270BC(int heap, const MlnServiceArg *value, unsigned int *out)
{
	if (value == NULL || out == NULL)
		return 3;
	if (value->type != MLN_SERVICE_ARG_OFFSET)
		return 3;
	*out = *(const unsigned int *)value->value;
	return 0;
}

typedef struct MlnServiceBlock28 {
	unsigned int words[7];
} MlnServiceBlock28;

int MlnImpl_810270F4(int heap, const MlnServiceArg *value, void *out)
{
	if (value == NULL || out == NULL)
		return 3;
	if (value->type != MLN_SERVICE_ARG_BLOCK28)
		return 3;
	*(MlnServiceBlock28 *)out = *(const MlnServiceBlock28 *)value->value;
	return 0;
}

int MlnImpl_81027140(int heap, MlnServiceArg *value, void **out)
{
	if (value == NULL || out == NULL || value->type != MLN_SERVICE_ARG_OBJECT)
		return 3;
	*out = value->value;
	value->value = NULL;
	return 0;
}

int MlnImpl_8102717C(int heap, MlnServiceArg *value, void **out)
{
	if (value == NULL || out == NULL || value->type != MLN_SERVICE_ARG_STRUCT)
		return 3;
	*out = value->value;
	value->value = NULL;
	return 0;
}

int MlnImpl_810271B8(int heap, MlnServiceArg *value, void **out)
{
	if (value == NULL || out == NULL || value->type != MLN_SERVICE_ARG_LIST)
		return 3;
	*out = value->value;
	value->value = NULL;
	return 0;
}

int MlnImpl_810271F4(int heap, MlnServiceArg *value, void **out)
{
	if (value == NULL || out == NULL || value->type != MLN_SERVICE_ARG_RECORD)
		return 3;
	*out = value->value;
	value->value = NULL;
	return 0;
}
