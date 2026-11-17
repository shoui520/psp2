/*
 * SceNpWebApi: the data a push event carries (Sony's np_push/ipc/np_push_ipc_data.cpp), and the names
 * of the extended data each service wants. The names are ours where the asserts give none.
 */

#include "np_webapi_internal.h"

#define NP_PUSH_EXDATA_MAX		10
#define NP_PUSH_EXDATA_NAME_SIZE	33

/* the data text of an event (a group of type 2 holding a 0x1001 header) */
int npPushExDataParseFirst(const char **first, char *src, unsigned int size, unsigned int *consumed)
{
	NpPushIpcTlv tlv;
	unsigned short type;
	unsigned short length;
	unsigned int headerSize;
	unsigned int remaining;
	unsigned int offset;
	unsigned int skipped;
	int result;

	result = npPushIpcReadHeader(2, &length, src, size, &headerSize);
	if (result >= 0) {
		remaining = length;
		offset = headerSize;
		if (length != 0) {
			while (1) {
				result = npPushIpcReadTypeLength(&type, &length, &src[offset], remaining, NULL);
				if (result < 0)
					return result;
				if (type == 0x1001) {
					npPushIpcTlvStringConstruct(&tlv, 0x1001);
					result = npPushIpcTlvStringUnserialize(&tlv, &src[offset], remaining, &headerSize);
					if (result < 0) {
						npPushIpcTlvStringDestruct(&tlv);
						return result;
					}
					skipped = headerSize;
					*first = tlv.data;
					npPushIpcTlvStringDestruct(&tlv);
				} else {
					result = npPushIpcSkip(&src[offset], size - offset, &headerSize);
					if (result < 0)
						return result;
					skipped = headerSize;
				}
				remaining -= skipped;
				offset += skipped;
				if (remaining == 0)
					break;
			}
		}
		result = 0;
		*consumed = offset;
	}
	return result;
}

/* an extended data slot (a group of type 3 holding the name, 0x1002, and the text, 0x1003) */
int npPushExDataParseNames(NpPushExDataSlot *slot, char *src, unsigned int size, unsigned int *consumed)
{
	NpPushIpcTlv name;
	NpPushIpcTlv text;
	unsigned short type;
	unsigned short length;
	unsigned int headerSize;
	unsigned int remaining;
	unsigned int offset;
	unsigned int skipped;
	int result;

	result = npPushIpcReadHeader(3, &length, src, size, &headerSize);
	if (result >= 0) {
		remaining = length;
		offset = headerSize;
		while (remaining != 0) {
			result = npPushIpcReadTypeLength(&type, &length, &src[offset], remaining, NULL);
			if (result < 0)
				return result;
			if (type > 0x1002) {
				if (type > 0x1003)
					goto skip;
				npPushIpcTlvStringConstruct(&text, 0x1003);
				result = npPushIpcTlvStringUnserialize(&text, &src[offset], remaining, &headerSize);
				if (result < 0) {
					npPushIpcTlvStringDestruct(&text);
					return result;
				}
				skipped = headerSize;
				slot->text = text.data;
				npPushIpcTlvStringDestruct(&text);
			} else {
				if (type <= 0x1001)
					goto skip;
				npPushIpcTlvStringConstruct(&name, 0x1002);
				result = npPushIpcTlvStringUnserialize(&name, &src[offset], remaining, &headerSize);
				if (result < 0) {
					npPushIpcTlvStringDestruct(&name);
					return result;
				}
				skipped = headerSize;
				slot->name = name.data;
				npPushIpcTlvStringDestruct(&name);
			}
			goto next;
skip:
			result = npPushIpcSkip(&src[offset], size - offset, &headerSize);
			if (result < 0)
				return result;
			skipped = headerSize;
next:
			remaining -= skipped;
			offset += skipped;
		}
		result = 0;
		*consumed = offset;
	}
	return result;
}

/* the whole data (a group of type 1): the first text and the slots */
int npPushExDataParse(NpPushExData *exData, char *src, unsigned int size, unsigned int *consumed)
{
	unsigned short type;
	unsigned short length;
	unsigned int headerSize;
	unsigned int remaining;
	unsigned int offset;
	unsigned int skipped;
	NpPushExDataSlot *slot;
	const char **first;
	int *entry;
	int index;
	int result;

	result = npPushIpcReadHeader(1, &length, src, size, &headerSize);
	if (result >= 0) {
		remaining = length;
		offset = headerSize;
		if (length != 0) {
			do {
				result = npPushIpcReadTypeLength(&type, &length, &src[offset], remaining, NULL);
				if (result < 0)
					return result;
				if (type > 2) {
					if (type > 3)
						goto skip;
					index = 0;
					entry = (int *)exData;
					do {
						if (entry[1] == 0) {
							slot = (NpPushExDataSlot *)(entry + 1);
							goto found;
						}
						index++;
						entry += 2;
					} while (index < NP_PUSH_EXDATA_MAX);
					slot = NULL;
found:
					if (slot == NULL)
						return NP_PUSH_IPC_ERROR_NO_SLOT;
					result = npPushExDataParseNames(slot, &src[offset], remaining, &headerSize);
					if (result < 0)
						return result;
					skipped = headerSize;
				} else {
					if (type <= 1)
						goto skip;
					first = NULL;
					if (exData->data == NULL)
						first = &exData->data;
					if (first == NULL)
						return NP_PUSH_IPC_ERROR_NO_FIRST;
					result = npPushExDataParseFirst(first, &src[offset], remaining, &headerSize);
					if (result < 0)
						return result;
					skipped = headerSize;
				}
				goto next;
skip:
				result = npPushIpcSkip(&src[offset], size - offset, &headerSize);
				if (result < 0)
					return result;
				skipped = headerSize;
next:
				remaining -= skipped;
				offset += skipped;
			} while (remaining != 0);
		}
		result = 0;
		*consumed = offset;
	}
	return result;
}

const char *npPushExDataGetData(NpPushExData *exData)
{
	return exData->data;
}

SceSize npPushExDataGetDataLength(NpPushExData *exData)
{
	if (exData->data != NULL)
		return sceClibStrnlen(exData->data, 0xFFFFFFFF);
	return 0;
}

static int npPushExDataCount(NpPushExData *exData)
{
	NpPushExDataSlot *slot;
	int count;
	int i;

	count = 0;
	slot = exData->slots;
	for (i = 0; i < NP_PUSH_EXDATA_MAX; i++) {
		if (slot->name != NULL)
			count++;
		slot++;
	}
	return count;
}

#define NP_PUSH_EXDATA_ASSERT(line, what)							\
	SceNpCommonPs4_F50BD96A(sceClibPrintf("Assertion in %s(%s:%d): %s(%s)\n", what,		\
		"W:\\Build\\J01509658\\vsh\\app\\common\\src\\np\\np_push\\ipc\\np_push_ipc_data.cpp", line,	\
		"ASSERT", "idx < ExDataNum()"))

/* the names and texts of the slots, for the callbacks */
int npPushExDataGather(NpPushExData *exData, NpPushExDataArgument *dst, int max, int *count)
{
	NpPushExDataSlot *slot;
	const char *text;
	SceSize length;
	int used;
	int i;

	used = npPushExDataCount(exData);
	if (max < used)
		return NP_PUSH_IPC_ERROR_NO_ROOM;
	sceClibMemset(dst, 0, 12 * max);
	slot = exData->slots;
	for (i = 0; i < used; i++) {
		if (i >= npPushExDataCount(exData))
			NP_PUSH_EXDATA_ASSERT(318, "ExDataName");
		dst->name = slot->name;
		if (i >= npPushExDataCount(exData))
			NP_PUSH_EXDATA_ASSERT(325, "ExDataJsonStr");
		text = slot->text;
		dst->text = text;
		if (i >= npPushExDataCount(exData)) {
			NP_PUSH_EXDATA_ASSERT(332, "ExDataJsonLen");
			text = slot->text;
		}
		length = 0;
		if (text != NULL)
			length = sceClibStrnlen(text, 0xFFFFFFFF);
		dst->length = length;
		dst++;
		slot++;
	}
	*count = used;
	return 0;
}

/* the names a service wants: ten of 33 bytes, made when the first is added */
int npPushExDataNamesClear(NpPushExDataNames *names)
{
	sceClibMemset(names->names, 0, NP_PUSH_EXDATA_MAX * NP_PUSH_EXDATA_NAME_SIZE + 0);
	names->count = 0;
	return 0;
}

int npPushExDataNamesContain(NpPushExDataNames *names, const char *name)
{
	char (*base)[NP_PUSH_EXDATA_NAME_SIZE];
	int offset;
	int i;

	base = names->names;
	if (base == NULL)
		return 0;
	i = 0;
	if (names->count <= 0)
		return 0;
	for (offset = 0; sceClibStrncmp((const char *)base + offset, name, NP_PUSH_EXDATA_NAME_SIZE) != 0;
		offset += NP_PUSH_EXDATA_NAME_SIZE) {
		i++;
		base = names->names;
		if (i >= names->count)
			return 0;
	}
	return 1;
}

int npPushExDataNamesAdd(NpPushExDataNames *names, const char *name)
{
	char (*array)[NP_PUSH_EXDATA_NAME_SIZE];

	array = names->names;
	if (array == NULL) {
		array = (char (*)[NP_PUSH_EXDATA_NAME_SIZE])SceNpCommonPs4_342A935D(330);
		names->names = array;
		if (array == NULL)
			return -2141871612;
		sceClibMemset(array, 0, 330);
		array = names->names;
		names->count = 0;
	}
	if (names->count >= NP_PUSH_EXDATA_MAX)
		return NP_PUSH_IPC_ERROR_NO_SLOT;
	if (sceClibStrlcpy((char *)array + NP_PUSH_EXDATA_NAME_SIZE * names->count, name, NP_PUSH_EXDATA_NAME_SIZE)
		>= 0x21)
		return NP_PUSH_IPC_ERROR_NAME_TOO_LONG;
	names->count++;
	return 0;
}

int npPushServiceNameEquals(const char *a, const char *b)
{
	return sceClibStrncmp(a, b, 65) == 0;
}

/* a service name is compared up to the length of the one matched against when exact is given */
int npPushServiceNameMatches(const char *a, const char *b, char exact)
{
	SceSize length;

	if (exact != 0) {
		length = sceClibStrnlen(a, 0xFFFFFFFF);
		return sceClibStrncmp(b, a, length) == 0;
	}
	return sceClibStrncmp(b, a, 65) == 0;
}

int npPushServiceListIsEmpty(NpWebApiExtdEntryList *list)
{
	return list->head == NULL;
}
