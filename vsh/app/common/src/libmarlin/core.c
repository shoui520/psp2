/*
 * The core's entry points that the API layer calls: init and term, a buffer pair taken from
 * the core, the HTTP requests a transaction step asks the application to send, and the frees
 * of a step's values. The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_CORE_ERROR_INVALID	4

__attribute__((noinline)) int MlnCoreInit(void *memory, unsigned int size)
{
	return MlnImpl_8102A394(memory, size);
}

__attribute__((noinline)) int MlnCoreTerm(void)
{
	return MlnImpl_8102A418();
}

/* a blob the core filled, moved into a new buffer of the secure heap */
static inline __attribute__((always_inline)) int MlnBlobToBuffer(MlnBlob *blob, MlnBuffer **out)
{
	MlnBuffer *buffer = MlnBufferCreate(MLN_HEAP_SECURE, blob->size, NULL);
	int ret;

	ret = MlnImpl_8102AA38(blob, buffer);
	if (ret == MLN_OK) {
		if (buffer != NULL) {
			*out = buffer;
			buffer = NULL;
		} else {
			ret = MLN_ERROR_NO_MEMORY;
		}
	}
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	return ret;
}

/* the two buffers the core gives for a1 */
__attribute__((noinline)) int MlnImpl_81012F48(void *a0, const MlnBuffer *a1, MlnBuffer **first,
					       MlnBuffer **second)
{
	MlnBlob secondBlob = { NULL, 0 }, firstBlob = { NULL, 0 };
	MlnBuffer *one = NULL, *two = NULL;
	int ret;

	if (first == NULL || second == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		ret = MlnImpl_81013074(a0, a1, &firstBlob, &secondBlob);
		if (ret == MLN_OK) {
			ret = MlnBlobToBuffer(&firstBlob, &one);
			if (ret == MLN_OK) {
				ret = MlnBlobToBuffer(&secondBlob, &two);
				if (ret == MLN_OK) {
					*first = one;
					*second = two;
					one = NULL;
					two = NULL;
				}
			}
		}
	}
	MlnBufferDestroy(MLN_HEAP_SECURE, one);
	MlnBufferDestroy(MLN_HEAP_SECURE, two);
	return ret;
}

__attribute__((noinline)) int MlnImpl_81013074(void *a0, const MlnBuffer *a1, MlnBlob *first,
					       MlnBlob *second)
{
	return MlnImpl_810318D4(a0, a1, first, second);
}

static inline __attribute__((always_inline)) void MlnHttpHeaderArrayFree(MlnHttpHeader **items,
									 unsigned int count)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		if (items[i] != NULL) {
			MlnFree(MLN_HEAP_SECURE, items[i]->name);
			MlnFree(MLN_HEAP_SECURE, items[i]->value);
			MlnFree(MLN_HEAP_SECURE, items[i]);
		}
	}
	MlnFree(MLN_HEAP_SECURE, items);
}

/* copies of count headers, each name and value duplicated */
__attribute__((noinline)) int MlnHttpHeadersCreate(unsigned int count,
						   MlnHttpHeader *const *headers,
						   MlnHttpHeaders *out)
{
	MlnHttpHeader **items = NULL, *header = NULL;
	char *name = NULL, *value = NULL;
	unsigned int i;
	int ret;

	if (out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	if (count != 0) {
		if (headers == NULL) {
			ret = MLN_CORE_ERROR_INVALID;
			goto out;
		}
		items = MlnAllocate(MLN_HEAP_SECURE, count * sizeof(items[0]));
		if (items == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnMemset(items, 0, count * sizeof(items[0]));
		for (i = 0; i < count; i++) {
			if (headers[i] == NULL) {
				ret = MLN_CORE_ERROR_INVALID;
				goto fail;
			}
			name = MlnStrdup(MLN_HEAP_SECURE, headers[i]->name);
			value = MlnStrdup(MLN_HEAP_SECURE, headers[i]->value);
			if (name == NULL || value == NULL)
				goto no_memory;
			header = MlnAllocate(MLN_HEAP_SECURE, sizeof(*header));
			if (header == NULL)
				goto no_memory;
			header->name = name;
			header->value = value;
			items[i] = header;
			name = NULL;
			value = NULL;
			header = NULL;
		}
	}
	out->count = count;
	out->items = items;
	ret = MLN_OK;
out:
	MlnFree(MLN_HEAP_SECURE, name);
	MlnFree(MLN_HEAP_SECURE, value);
	MlnFree(MLN_HEAP_SECURE, header);
	return ret;

no_memory:
	ret = MLN_ERROR_NO_MEMORY;
fail:
	MlnFree(MLN_HEAP_SECURE, name);
	MlnFree(MLN_HEAP_SECURE, value);
	MlnFree(MLN_HEAP_SECURE, header);
	MlnHttpHeaderArrayFree(items, count);
	return ret;
}

static inline __attribute__((always_inline)) void MlnHttpHeadersFreeInline(MlnHttpHeaders *headers)
{
	unsigned int count = headers->count;
	MlnHttpHeader **items = headers->items;

	if (items != NULL)
		MlnHttpHeaderArrayFree(items, count);
}

__attribute__((noinline)) void MlnHttpHeadersFree(MlnHttpHeaders *headers)
{
	if (headers != NULL)
		MlnHttpHeadersFreeInline(headers);
}

static inline __attribute__((always_inline)) void MlnHttpRequestFreeInline(MlnHttpRequest *request)
{
	MlnBufferDestroy(MLN_HEAP_SECURE, request->body);
	MlnFree(MLN_HEAP_SECURE, request->url);
	MlnHttpHeadersFreeInline(&request->headers);
	MlnFree(MLN_HEAP_SECURE, request);
}

__attribute__((noinline)) void MlnHttpRequestFree(MlnHttpRequest *request)
{
	if (request != NULL)
		MlnHttpRequestFreeInline(request);
}

/* a value of a step: its two buffers, and those its type adds */
__attribute__((noinline)) void MlnApiValueFree(MlnApiValue *value)
{
	if (value == NULL)
		return;
	MlnBufferDestroy(MLN_HEAP_SECURE, value->data[0]);
	MlnBufferDestroy(MLN_HEAP_SECURE, value->data[1]);
	switch (value->type) {
	case 1:
	case 4:
		MlnBufferDestroy(MLN_HEAP_SECURE, value->data[2]);
		break;
	case 2:
	case 3:
		MlnBufferDestroy(MLN_HEAP_SECURE, value->data[2]);
		MlnBufferDestroy(MLN_HEAP_SECURE, value->data[3]);
		break;
	case 5:
	case 6:
		MlnBufferDestroy(MLN_HEAP_SECURE, value->data[3]);
		break;
	case 16:
		MlnImpl_81006D14(MLN_HEAP_SECURE, value->data[2]);
		break;
	}
	MlnFree(MLN_HEAP_SECURE, value);
}

__attribute__((noinline)) void MlnApiResultFree(MlnApiResult *result)
{
	if (result == NULL)
		return;
	MlnApiValueFree(result->value);
	if (result->request != NULL)
		MlnHttpRequestFreeInline(result->request);
	MlnFree(MLN_HEAP_SECURE, result);
}

/* a request's buffers, by its type, then the request cleared */
__attribute__((noinline)) void MlnApiRequestClear(MlnApiRequest *request)
{
	if (request == NULL)
		return;
	switch (request->type) {
	case 0:
	case 2:
		MlnBufferDestroy(MLN_HEAP_SECURE, request->arg[0]);
		MlnBufferDestroy(MLN_HEAP_SECURE, request->arg[1]);
		MlnBufferDestroy(MLN_HEAP_SECURE, request->arg[2]);
		break;
	case 3:
	case 4:
	case 5:
	case 13:
	case 14:
		MlnBufferDestroy(MLN_HEAP_SECURE, request->arg[0]);
		break;
	case 6:
	case 7:
	case 11:
		MlnBufferDestroy(MLN_HEAP_SECURE, request->arg[0]);
		MlnBufferDestroy(MLN_HEAP_SECURE, request->arg[1]);
		break;
	}
	MlnMemset(request, 0, sizeof(*request));
}

/* a report's data, by its type, then the report cleared */
__attribute__((noinline)) void MlnApiReportClear(MlnApiReport *report)
{
	MlnBuffer **items;
	unsigned int count, i;

	if (report == NULL)
		return;
	switch (report->type) {
	case 0:
	case 5:
	case 7:
	case 9:
		MlnBufferDestroy(MLN_HEAP_SECURE, report->data.buffer);
		break;
	case 2:
	case 6:
		count = report->data.items.count;
		items = report->data.items.items;
		if (items != NULL) {
			for (i = 0; i < count; i++)
				MlnBufferDestroy(MLN_HEAP_SECURE, items[i]);
			MlnFree(MLN_HEAP_SECURE, items);
		}
		break;
	case 3:
		count = report->data.rights.idCount;
		items = report->data.rights.ids;
		if (count != 0) {
			for (i = 0; i < count; i++) {
				if (items[i] != NULL)
					MlnFree(MLN_HEAP_SECURE, items[i]);
			}
			MlnFree(MLN_HEAP_SECURE, items);
		}
		break;
	}
	MlnMemset(report, 0, sizeof(*report));
}

__attribute__((noinline)) void MlnBufferPairFree(MlnBufferPair *pair)
{
	if (pair == NULL)
		return;
	MlnBufferDestroy(MLN_HEAP_SECURE, pair->first);
	MlnBufferDestroy(MLN_HEAP_SECURE, pair->second);
	MlnFree(MLN_HEAP_SECURE, pair);
}

/* the first buffer is required; the second has both a size and data, or neither */
__attribute__((noinline)) int MlnBufferPairCreate(unsigned int firstSize, const void *firstData,
						  unsigned int secondSize, const void *secondData,
						  unsigned char flag, MlnBufferPair **out)
{
	MlnBufferPair *pair;
	int hasSecond;

	if (firstSize == 0 || firstData == NULL || out == NULL)
		return MLN_CORE_ERROR_INVALID;
	if (secondSize != 0 && secondData == NULL)
		return MLN_CORE_ERROR_INVALID;
	hasSecond = secondData != NULL;
	if (secondSize == 0 && hasSecond)
		return MLN_CORE_ERROR_INVALID;
	pair = MlnAllocate(MLN_HEAP_SECURE, sizeof(*pair));
	if (pair == NULL)
		return MLN_ERROR_NO_MEMORY;
	MlnMemset(pair, 0, sizeof(*pair));
	pair->flag = flag;
	pair->second = NULL;
	pair->first = MlnBufferCreate(MLN_HEAP_SECURE, firstSize, firstData);
	if (pair->first != NULL) {
		if (!hasSecond)
			goto done;
		pair->second = MlnBufferCreate(MLN_HEAP_SECURE, secondSize, secondData);
		if (pair->second != NULL)
			goto done;
	}
	MlnBufferDestroy(MLN_HEAP_SECURE, pair->first);
	MlnBufferDestroy(MLN_HEAP_SECURE, pair->second);
	MlnFree(MLN_HEAP_SECURE, pair);
	return MLN_ERROR_NO_MEMORY;
done:
	*out = pair;
	return MLN_OK;
}

/*
 * A 20-byte ID: the first 10 bytes of the entitlement ID's SHA-1, the account ID (at most
 * 8 bytes, not all zero) right-aligned at the end. Either part may be left out, not both.
 */
__attribute__((noinline)) int MlnCompositeIdCreate(const MlnBuffer *accountId,
						   const MlnBuffer *entitlementId,
						   MlnBuffer **out)
{
	unsigned char zero[20], digest[20];
	MlnSha1Context context;
	MlnBuffer *id = NULL;
	int ret;

	MlnMemset(zero, 0, sizeof(zero));
	if (accountId == NULL && entitlementId == NULL)
		goto invalid;
	if (accountId != NULL && (accountId->data == NULL || accountId->size > 8 ||
				  MlnMemcmp(accountId->data, zero, accountId->size) == 0))
		goto invalid;
	if (entitlementId != NULL && (entitlementId->size == 0 || entitlementId->data == NULL))
		goto invalid;
	if (out == NULL)
		goto invalid;
	id = MlnBufferCreateFilled(MLN_HEAP_SECURE, 20, 0);
	if (id == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	if (accountId != NULL)
		MlnMemcpy(id->data + 20 - accountId->size, accountId->data, accountId->size);
	if (entitlementId != NULL) {
		MlnImpl_81033774(&context);
		MlnImpl_810337C4(&context, entitlementId->data, entitlementId->size);
		MlnImpl_81033864(&context, digest);
		if (MlnMemcmp(digest, zero, 10) == 0) {
			ret = MLN_CORE_ERROR_INVALID;
			goto out;
		}
		MlnMemcpy(id->data, digest, 10);
	}
	*out = id;
	id = NULL;
	ret = MLN_OK;
	goto out;
invalid:
	ret = MLN_CORE_ERROR_INVALID;
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, id);
	return ret;
}

/* the file layer's errors */
#define MLN_FILE_ERROR_SPACE	2
#define MLN_FILE_ERROR_INVALID	5
#define MLN_FILE_ERROR_NOT_FOUND	0x11060002

#define MLN_FILE_AREA		1
#define MLN_PATH_MAX		256

/*
 * A state slot's file: <root>/sdata/<kind>0000000/<kind>000000<index>.DAT, the kind a letter
 * for the slot's range and the index its place in it; create makes the slot's folder.
 */
__attribute__((noinline)) int MlnStateFilePath(unsigned int id, int create, char *path)
{
	unsigned int size = MLN_PATH_MAX;
	unsigned int index;
	char name[13];
	char kind;
	int ret;

	ret = MlnImpl_810262B4(MLN_FILE_AREA, &size, path);
	if (ret != MLN_OK)
		return ret;
	size = MLN_PATH_MAX;
	ret = MlnImpl_810256EC(&size, path, "sdata");
	if (ret != MLN_OK)
		return ret;
	if (id <= 15) {
		kind = 'P';
		index = id;
	} else if (id - 128 <= 7) {
		kind = 'L';
		index = id - 128;
	} else if (id - 152 <= 7) {
		kind = 'N';
		index = id - 152;
	} else if (id - 136 <= 7) {
		kind = 'D';
		index = id - 136;
	} else if (id - 144 <= 7) {
		kind = 'R';
		index = id - 144;
	} else if (id == 32) {
		kind = 'S';
		index = 0;
	} else if (id == 33) {
		kind = 'T';
		index = 0;
	} else if (id - 160 <= 7) {
		kind = 'C';
		index = id - 160;
	} else if (id == 34) {
		kind = 'V';
		index = 0;
	} else if (id == 35) {
		kind = 'I';
		index = 0;
	} else {
		return MLN_FILE_ERROR_INVALID;
	}
	size = MLN_PATH_MAX;
	name[0] = kind;
	name[1] = '0';
	name[2] = '0';
	name[3] = '0';
	name[4] = '0';
	name[5] = '0';
	name[6] = '0';
	name[7] = '0';
	name[8] = '\0';
	ret = MlnImpl_810256EC(&size, path, name);
	if (ret != MLN_OK)
		return ret;
	if (create)
		MlnImpl_8102619C(MLN_FILE_AREA, path);
	size = MLN_PATH_MAX;
	name[0] = kind;
	name[1] = '0';
	name[2] = '0';
	name[3] = '0';
	name[4] = '0';
	name[5] = '0';
	name[6] = '0';
	name[7] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"[index];
	name[8] = '.';
	name[9] = 'D';
	name[10] = 'A';
	name[11] = 'T';
	name[12] = '\0';
	return MlnImpl_810256EC(&size, path, name);
}

/*
 * A state slot as a record: the slot ID and the data's size (big-endian words), then the
 * file's data. Without data, the size such a record needs.
 */
__attribute__((noinline)) int MlnStateFileRead(unsigned int id, unsigned char *data,
					       unsigned int capacity, unsigned int *size)
{
	char path[MLN_PATH_MAX];
	MlnFileInfo info;
	unsigned int length;
	unsigned char *p;
	void *file = NULL;
	int ret;

	if (size == NULL) {
		ret = MLN_FILE_ERROR_INVALID;
		goto out;
	}
	ret = MlnStateFilePath(id, 0, path);
	if (ret != MLN_OK)
		goto out;
	if (data == NULL) {
		ret = MlnImpl_81026324(MLN_FILE_AREA, path, &info);
		if (ret == MLN_OK)
			*size = info.size + 8;
		goto out;
	}
	ret = MlnImpl_810259F8(MLN_FILE_AREA, path, 1, &file);
	if (ret != MLN_OK)
		goto out;
	ret = MlnImpl_810260E4(MLN_FILE_AREA, file, &length);
	if (ret != MLN_OK)
		goto out;
	if (length + 8 > capacity) {
		ret = MLN_FILE_ERROR_SPACE;
		goto out;
	}
	data[0] = id >> 24;
	data[1] = id >> 16;
	data[2] = id >> 8;
	data[3] = id;
	data[4] = length >> 24;
	data[5] = length >> 16;
	data[6] = length >> 8;
	data[7] = length;
	p = data + 8;
	if (length != 0) {
		ret = MlnImpl_81025C1C(MLN_FILE_AREA, file, length, &length, p);
		if (ret != MLN_OK)
			goto out;
		p += length;
	}
	*size = p - data;
out:
	if (file != NULL)
		MlnImpl_81025BE8(MLN_FILE_AREA, file);
	return ret;
}

/* the records of a group's eight slots, those with no file left out */
__attribute__((noinline)) int MlnStateGroupRead(int group, unsigned char *end, unsigned char *data,
						unsigned int *size)
{
	unsigned int length = 0, total;
	int first, last, id;
	int ret;

	switch (group) {
	case 0:
		first = 128;
		last = 135;
		break;
	case 1:
		first = 136;
		last = 143;
		break;
	case 2:
		first = 152;
		last = 159;
		break;
	default:
		return MLN_FILE_ERROR_INVALID;
	}
	total = 0;
	for (id = first; id <= last; id++) {
		ret = MlnStateFileRead(id, data, end - data, &length);
		if (ret != MLN_OK && ret != MLN_FILE_ERROR_NOT_FOUND)
			return ret;
		if (ret == MLN_OK) {
			total += length;
			if (data != NULL)
				data += length;
		}
	}
	*size = total;
	return MLN_OK;
}

/* one slot's record at p, a missing file skipped */
#define MLN_STATE_READ(id)							\
	do {									\
		ret = MlnStateFileRead((id), p, remaining, &length);		\
		if (ret != MLN_OK && ret != MLN_FILE_ERROR_NOT_FOUND)		\
			return ret;						\
		if (ret == MLN_OK) {						\
			total += length;					\
			if (p != NULL)						\
				p += length;					\
			remaining = end - p;					\
		}								\
	} while (0)

/* one group's records at p */
#define MLN_STATE_GROUP(group)							\
	do {									\
		ret = MlnStateGroupRead((group), end, p, &length);		\
		if (ret != MLN_OK)						\
			return ret;						\
		total += length;						\
		if (p != NULL)							\
			p += length;						\
	} while (0)

/*
 * The records of every state slot, in Sony's order, into the buffer; without one, the size
 * they need.
 */
__attribute__((noinline)) int MlnStateCollect(unsigned int *size, const MlnBuffer *buffer)
{
	unsigned char *p, *end;
	unsigned int remaining, length = 0, total = 0;
	int ret;

	if (size == NULL)
		return MLN_FILE_ERROR_INVALID;
	if (buffer != NULL) {
		p = buffer->data;
		remaining = buffer->size;
		end = p + remaining;
	} else {
		p = NULL;
		remaining = 0;
		end = NULL;
	}
	MLN_STATE_READ(3);
	MLN_STATE_READ(4);
	MLN_STATE_READ(5);
	MLN_STATE_READ(0);
	MLN_STATE_READ(1);
	MLN_STATE_GROUP(0);
	MLN_STATE_GROUP(2);
	MLN_STATE_GROUP(1);
	remaining = end - p;
	MLN_STATE_READ(144);
	MLN_STATE_READ(32);
	MLN_STATE_READ(33);
	MLN_STATE_READ(34);
	MLN_STATE_READ(35);
	MLN_STATE_READ(161);
	MLN_STATE_READ(162);
	*size = total;
	return MLN_OK;
}

/* a slot's file written with the data, or removed for no data */
__attribute__((noinline)) int MlnStateFileWrite(unsigned int id, const void *data, unsigned int size)
{
	char path[MLN_PATH_MAX];
	void *file = NULL;
	int ret;

	ret = MlnStateFilePath(id, 1, path);
	if (ret != MLN_OK)
		goto out;
	if (size == 0) {
		ret = MlnImpl_8102590C(MLN_FILE_AREA, path);
		ret = ret == MLN_FILE_ERROR_NOT_FOUND ? MLN_OK : ret;
		goto out;
	}
	ret = MlnImpl_810259F8(MLN_FILE_AREA, path, 2, &file);
	if (ret != MLN_OK) {
		MlnImpl_8102590C(MLN_FILE_AREA, path);
		goto out;
	}
	ret = MlnImpl_81025F5C(MLN_FILE_AREA, file, size, data);
	if (ret != MLN_OK) {
		MlnImpl_81025BE8(MLN_FILE_AREA, file);
		file = NULL;
		MlnImpl_8102590C(MLN_FILE_AREA, path);
	}
out:
	if (file != NULL)
		MlnImpl_81025BE8(MLN_FILE_AREA, file);
	return ret;
}

/* every slot's record in one new buffer */
__attribute__((noinline)) int MlnStateSerialize(MlnBuffer **state)
{
	MlnBuffer *buffer = NULL;
	unsigned int size;
	int ret;

	if (state == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
		goto fail;
	}
	ret = MlnStateCollect(&size, NULL);
	if (ret != MLN_OK)
		goto fail;
	buffer = MlnBufferCreate(MLN_HEAP_SECURE, size, NULL);
	if (buffer == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto fail;
	}
	ret = MlnStateCollect(&size, buffer);
	if (ret == MLN_OK) {
		*state = buffer;
		return MLN_OK;
	}
fail:
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	return ret;
}

/* each record of a serialized state written back to its slot's file */
__attribute__((noinline)) int MlnStateRestore(const MlnBuffer *state)
{
	const unsigned char *p, *end, *next;
	unsigned int id, size;
	int ret;

	if (state == NULL)
		return MLN_ERROR_INVALID_ARGUMENT;
	p = state->data;
	end = p + state->size;
	while (p + 8 <= end) {
		id = (unsigned int)p[0] << 24 | (unsigned int)p[1] << 16 |
		     (unsigned int)p[2] << 8 | p[3];
		size = (unsigned int)p[4] << 24 | (unsigned int)p[5] << 16 |
		       (unsigned int)p[6] << 8 | p[7];
		next = p + 8 + size;
		if (next > end)
			return MLN_FILE_ERROR_INVALID;
		ret = MlnStateFileWrite(id, p + 8, size);
		if (ret != MLN_OK)
			return ret;
		p = next;
	}
	return p == end ? MLN_OK : MLN_FILE_ERROR_INVALID;
}

/*
 * A service command on the context's handle and one integer: its status (29, partial, kept),
 * and from a type 1 reply the status block and, parsed from it, the rights it grants.
 */
__attribute__((noinline)) int MlnImpl_81014574(MlnServiceContext *context, int command, int value,
					       MlnRightsInfo **rights, MlnBuffer **esb)
{
	MlnServiceArg items[2];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer *block = NULL;
	MlnRightsInfo *info = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	items[0].type = MLN_SERVICE_ARG_HANDLE;
	items[0].value = context;
	items[1].type = MLN_SERVICE_ARG_INTEGER;
	items[1].value = &value;
	args.count = 2;
	args.items = items;
	ret = MlnServiceCall(context->session, command, &args, &reply, &status);
	if (ret != MLN_OK)
		goto out;
	ret = status;
	if (ret != MLN_OK) {
		if (ret != MLN_SERVICE_STATUS_PARTIAL)
			goto out;
		ret = 1;
	}
	if (reply->data == NULL || reply->type != 1)
		goto done;
	ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->data, &block);
	if (ret != MLN_OK)
		goto failed;
	if (block == NULL)
		goto done;
	if (esb != NULL) {
		*esb = block;
		block = NULL;
	}
	if (rights == NULL)
		goto done;
	/* Sony's reads the block it may just have handed over */
	ret = MlnRightsInfoParse(MLN_HEAP_SECURE, block->size, block->data, &info);
	if (ret != MLN_OK)
		goto failed;
	*rights = info;
	info = NULL;
done:
	ret = ret == 1 ? MLN_SERVICE_STATUS_PARTIAL : MLN_OK;
	goto out;
failed:
	ret = MlnImpl_810191C4(ret);
out:
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	MlnBufferDestroy(MLN_HEAP_SECURE, block);
	MlnRightsInfoFree(MLN_HEAP_SECURE, info);
	return ret;
}

__attribute__((noinline)) int MlnServiceSetState(MlnServiceContext *context, MlnBuffer *state)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (state == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = state;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, MLN_SERVICE_SET_STATE, &args, &reply,
				     &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

__attribute__((noinline)) int MlnServiceClearState(MlnServiceContext *context)
{
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else {
		ret = MlnServiceCall(context->session, MLN_SERVICE_CLEAR_STATE, &args, &reply,
				     &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x402 with one integer */
__attribute__((noinline)) int MlnImpl_810147D8(MlnServiceContext *context, int value)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else {
		item.type = MLN_SERVICE_ARG_INTEGER;
		item.value = &value;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x402, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x404 with two buffers, the first given as its size and data */
__attribute__((noinline)) int MlnImpl_81014860(MlnServiceContext *context, unsigned int size,
					       void *data, MlnBuffer *buffer)
{
	MlnBuffer first;
	MlnServiceArg items[2];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	first.size = size;
	first.data = data;
	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else {
		items[0].type = MLN_SERVICE_ARG_BUFFER;
		items[0].value = &first;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = buffer;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x404, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x407 on a buffer: the buffer of its type 1 reply */
__attribute__((noinline)) int MlnImpl_810148F8(MlnServiceContext *context, MlnBuffer *in,
					       MlnBuffer **out)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer *buffer = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (in == NULL || out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = in;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x407, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->data, &buffer);
					if (ret != MLN_OK) {
						ret = MlnImpl_810191C4(ret);
					} else {
						*out = buffer;
						buffer = NULL;
					}
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	return ret;
}

/*
 * Service command 0x405; when its type 1 reply's flag is clear, the application's data
 * (get) sent with command 0x406, and released.
 */
__attribute__((noinline)) int MlnImpl_81014A10(MlnServiceContext *context, MlnServiceDataGet get,
					       MlnServiceDataRelease release, void *arg,
					       void *cookie)
{
	MlnBuffer buffer;
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	void *data = NULL;
	unsigned int size = 0;
	int status, flag;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	ret = MlnServiceCall(context->session, 0x405, &args, &reply, &status);
	if (ret != MLN_OK)
		goto out;
	ret = status;
	if (ret != MLN_OK)
		goto out;
	if (MlnImpl_81026D94(reply, 1) != 0) {
		ret = MLN_SERVICE_ERROR_REPLY;
		goto out;
	}
	ret = MlnImpl_81027084(MLN_HEAP_SECURE, reply->data, &flag);
	if (ret != MLN_OK) {
		ret = MlnImpl_810191C4(ret);
		goto out;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	reply = NULL;
	if (flag != 0)
		goto out;
	if (get(cookie, arg, &data, &size) != 0) {
		ret = 7;
		goto out;
	}
	buffer.size = size;
	buffer.data = data;
	item.type = MLN_SERVICE_ARG_BUFFER;
	item.value = &buffer;
	args.count = 1;
	args.items = &item;
	ret = MlnServiceCall(context->session, 0x406, &args, &reply, &status);
	if (ret == MLN_OK)
		ret = status;
out:
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	release(cookie, data);
	return ret;
}

/* service command 0x408 with a buffer */
__attribute__((noinline)) int MlnImpl_81014B8C(MlnServiceContext *context, MlnBuffer *buffer)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (buffer == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = buffer;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x408, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x40b on a buffer (or none): the buffer of its type 1 reply */
__attribute__((noinline)) int MlnImpl_81014C20(MlnServiceContext *context, MlnBuffer *in,
					       MlnBuffer **out)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer *buffer = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = in;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x40b, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->data, &buffer);
					if (ret != MLN_OK) {
						ret = MlnImpl_810191C4(ret);
					} else {
						*out = buffer;
						buffer = NULL;
					}
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	return ret;
}

/* service command 0x40e with a buffer */
__attribute__((noinline)) int MlnImpl_81014D30(MlnServiceContext *context, MlnBuffer *buffer)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (buffer == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = buffer;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x40e, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x40f */
__attribute__((noinline)) int MlnImpl_81014DC4(MlnServiceContext *context)
{
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else {
		ret = MlnServiceCall(context->session, 0x40f, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x406 on a buffer: its type 1 reply (also a partial one) into out */
__attribute__((noinline)) int MlnImpl_81014E28(MlnServiceContext *context, MlnBuffer *buffer,
					       void *out)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (buffer == NULL || out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = buffer;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x406, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (status == MLN_OK || status == MLN_SERVICE_STATUS_MORE) {
				if (MlnImpl_81026D94(reply, 1) != 0)
					ret = MLN_SERVICE_ERROR_REPLY;
				else if (MlnImpl_81026FE4(MLN_HEAP_SECURE, reply->data, out) != 0)
					ret = MlnImpl_810191C4(ret);
				else
					ret = status;
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x40d with a buffer (or none) */
__attribute__((noinline)) int MlnImpl_81014F24(MlnServiceContext *context, MlnBuffer *buffer)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = buffer;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x40d, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x410 with a buffer */
__attribute__((noinline)) int MlnImpl_81014FA8(MlnServiceContext *context, MlnBuffer *buffer)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (buffer == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = buffer;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x410, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* opens the context's service handle (command 0x41a) */
__attribute__((noinline)) int MlnImpl_8101503C(MlnServiceContext *context, int value, void *data)
{
	MlnServiceArg items[2];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int id = -1;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (context->id != -1) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else {
		items[0].type = MLN_SERVICE_ARG_INTEGER;
		items[0].value = &value;
		items[1].type = MLN_SERVICE_ARG_DATA;
		items[1].value = data;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x41a, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_8102701C(MLN_HEAP_SECURE, reply->data, &id);
					if (ret != MLN_OK) {
						ret = MlnImpl_810191C4(ret);
					} else if (id < 0) {
						ret = MLN_ERROR_INVALID_ARGUMENT;
					} else {
						context->id = id;
						id = -1;
					}
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* closes the context's service handle (command 0x41b) */
__attribute__((noinline)) int MlnImpl_81015168(MlnServiceContext *context)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	if (context->id != -1) {
		item.type = MLN_SERVICE_ARG_HANDLE;
		item.value = context;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x41b, &args, &reply, &status);
		if (ret != MLN_OK)
			goto out;
		ret = status;
		if (ret != MLN_OK)
			goto out;
	}
	ret = MLN_OK;
	context->id = -1;
out:
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x41c on the handle, a buffer and a name: the buffer of its type 1 reply */
__attribute__((noinline)) int MlnImpl_8101521C(MlnServiceContext *context, unsigned int size,
					       void *data, const char *name, MlnBuffer **out)
{
	MlnBuffer first;
	MlnBuffer text;
	MlnServiceArg items[3];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer *buffer = NULL;
	int status;
	int ret;

	first.size = size;
	first.data = data;
	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (name == NULL || out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		text.size = MlnStrlen(name);
		text.data = (void *)name;
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = &first;
		items[2].type = MLN_SERVICE_ARG_BUFFER;
		items[2].value = &text;
		args.count = 3;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x41c, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->data, &buffer);
					if (ret != MLN_OK) {
						ret = MlnImpl_810191C4(ret);
					} else {
						*out = buffer;
						buffer = NULL;
					}
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	return ret;
}

/* service command 0x41d on the handle and a buffer: the buffer of its type 1 reply */
__attribute__((noinline)) int MlnImpl_81015374(MlnServiceContext *context, MlnBuffer *in,
					       MlnBuffer **out)
{
	MlnServiceArg items[2];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer *buffer = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (in == NULL || out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = in;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x41d, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->data, &buffer);
					if (ret != MLN_OK) {
						ret = MlnImpl_810191C4(ret);
					} else {
						*out = buffer;
						buffer = NULL;
					}
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	return ret;
}

/* service command 0x41e on the handle, a buffer, a name and two buffers: its reply's buffer */
__attribute__((noinline)) int MlnImpl_8101549C(MlnServiceContext *context, unsigned int size,
					       void *data, const char *name, MlnBuffer *other,
					       MlnBuffer *in, MlnBuffer **out)
{
	MlnBuffer first;
	MlnBuffer text;
	MlnServiceArg items[5];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer *buffer = NULL;
	int status;
	int ret;

	first.size = size;
	first.data = data;
	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (name == NULL || out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else if (in == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		text.size = MlnStrlen(name);
		text.data = (void *)name;
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = &first;
		items[2].type = MLN_SERVICE_ARG_BUFFER;
		items[2].value = &text;
		items[3].type = MLN_SERVICE_ARG_BUFFER;
		items[3].value = other;
		items[4].type = MLN_SERVICE_ARG_BUFFER;
		items[4].value = in;
		args.count = 5;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x41e, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->data, &buffer);
					if (ret != MLN_OK) {
						ret = MlnImpl_810191C4(ret);
					} else {
						*out = buffer;
						buffer = NULL;
					}
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	return ret;
}

/* service command 0x41f on the handle and a buffer: the two buffers of its type 2 reply */
__attribute__((noinline)) int MlnImpl_81015620(MlnServiceContext *context, MlnBuffer *in,
					       MlnBuffer **outFirst, MlnBuffer **outSecond)
{
	MlnServiceArg items[2];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer *first = NULL;
	MlnBuffer *second = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (outFirst == NULL || outSecond == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else if (in == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = in;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x41f, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 2) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->data, &first);
					if (ret == MLN_OK)
						ret = MlnImpl_81026F0C(MLN_HEAP_SECURE,
								       (MlnServiceArg *)reply->data + 1,
								       &second);
					if (ret != MLN_OK) {
						ret = MlnImpl_810191C4(ret);
					} else {
						*outFirst = first;
						first = NULL;
						*outSecond = second;
						second = NULL;
					}
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	MlnBufferDestroy(MLN_HEAP_SECURE, first);
	MlnBufferDestroy(MLN_HEAP_SECURE, second);
	return ret;
}

/* service command 0x420 on the handle, a buffer, a name and a buffer: the buffer of its reply */
__attribute__((noinline)) int MlnImpl_81015798(MlnServiceContext *context, unsigned int size,
					       void *data, const char *name, MlnBuffer *in,
					       MlnBuffer **out)
{
	MlnBuffer first;
	MlnBuffer text;
	MlnServiceArg items[4];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer *buffer = NULL;
	int status;
	int ret;

	first.size = size;
	first.data = data;
	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (name == NULL || out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		text.size = MlnStrlen(name);
		text.data = (void *)name;
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = &first;
		items[2].type = MLN_SERVICE_ARG_BUFFER;
		items[2].value = &text;
		items[3].type = MLN_SERVICE_ARG_BUFFER;
		items[3].value = in;
		args.count = 4;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x420, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->data, &buffer);
					if (ret != MLN_OK) {
						ret = MlnImpl_810191C4(ret);
					} else {
						*out = buffer;
						buffer = NULL;
					}
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	return ret;
}

/* service command 0x421 on the handle and a buffer: the buffer of its type 1 reply */
__attribute__((noinline)) int MlnImpl_810158FC(MlnServiceContext *context, MlnBuffer *in,
					       MlnBuffer **out)
{
	MlnServiceArg items[2];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer *buffer = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (in == NULL || out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = in;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x421, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->data, &buffer);
					if (ret != MLN_OK) {
						ret = MlnImpl_810191C4(ret);
					} else {
						*out = buffer;
						buffer = NULL;
					}
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	return ret;
}

/* service command 0x422 on the handle, a buffer, a name and three buffers: its reply's buffer */
__attribute__((noinline)) int MlnImpl_81015A24(MlnServiceContext *context, unsigned int size,
					       void *data, const char *name, MlnBuffer *other,
					       MlnBuffer *extra, MlnBuffer *in, MlnBuffer **out)
{
	MlnBuffer first;
	MlnBuffer text;
	MlnServiceArg items[6];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer *buffer = NULL;
	int status;
	int ret;

	first.size = size;
	first.data = data;
	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (name == NULL || out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else if (in == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		text.size = MlnStrlen(name);
		text.data = (void *)name;
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = &first;
		items[2].type = MLN_SERVICE_ARG_BUFFER;
		items[2].value = &text;
		items[3].type = MLN_SERVICE_ARG_BUFFER;
		items[3].value = other;
		items[4].type = MLN_SERVICE_ARG_BUFFER;
		items[4].value = extra;
		items[5].type = MLN_SERVICE_ARG_BUFFER;
		items[5].value = in;
		args.count = 6;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x422, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->data, &buffer);
					if (ret != MLN_OK) {
						ret = MlnImpl_810191C4(ret);
					} else {
						*out = buffer;
						buffer = NULL;
					}
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	return ret;
}

/* service command 0x423 on the handle and a buffer: the buffer of its type 1 reply */
__attribute__((noinline)) int MlnImpl_81015BB0(MlnServiceContext *context, MlnBuffer *in,
					       MlnBuffer **out)
{
	MlnServiceArg items[2];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer *buffer = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (in == NULL || out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = in;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x423, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->data, &buffer);
					if (ret != MLN_OK) {
						ret = MlnImpl_810191C4(ret);
					} else {
						*out = buffer;
						buffer = NULL;
					}
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	return ret;
}

/* service command 0x426 on the handle: the buffer group of its type 1 reply */
__attribute__((noinline)) int MlnImpl_81015CD8(MlnServiceContext *context, MlnBufferGroup **out)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBufferGroup *group = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_HANDLE;
		item.value = context;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x426, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026ED0(MLN_HEAP_SECURE, reply->data, &group);
					if (ret != MLN_OK) {
						ret = MlnImpl_810191C4(ret);
					} else {
						*out = group;
						group = NULL;
					}
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	MlnImpl_81006D14(MLN_HEAP_SECURE, group);
	return ret;
}

/* service command 0x425 on the handle and three buffers */
__attribute__((noinline)) int MlnImpl_81015DDC(MlnServiceContext *context, unsigned int firstSize,
					       void *firstData, unsigned int secondSize,
					       void *secondData, unsigned int thirdSize,
					       void *thirdData)
{
	MlnBuffer first;
	MlnBuffer second;
	MlnBuffer third;
	MlnServiceArg items[4];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	first.size = firstSize;
	first.data = firstData;
	second.size = secondSize;
	second.data = secondData;
	third.size = thirdSize;
	third.data = thirdData;
	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = &first;
		items[2].type = MLN_SERVICE_ARG_BUFFER;
		items[2].value = &second;
		items[3].type = MLN_SERVICE_ARG_BUFFER;
		items[3].value = &third;
		args.count = 4;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x425, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x455 on the handle and two buffers */
__attribute__((noinline)) int MlnImpl_81015EA4(MlnServiceContext *context, MlnBuffer *in,
					       MlnBuffer *other)
{
	MlnServiceArg items[3];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = in;
		items[2].type = MLN_SERVICE_ARG_BUFFER;
		items[2].value = other;
		args.count = 3;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x455, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* opens the context's handle (command 0x400) */
__attribute__((noinline)) int MlnImpl_81015F3C(MlnServiceContext *context)
{
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int id = -1;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (context->id != -1) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else {
		ret = MlnServiceCall(context->session, 0x400, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_8102701C(MLN_HEAP_SECURE, reply->data, &id);
					if (ret != MLN_OK) {
						ret = MlnImpl_810191C4(ret);
					} else if (id < 0) {
						ret = MLN_ERROR_INVALID_ARGUMENT;
					} else {
						context->id = id;
						id = -1;
					}
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* closes the context's handle (command 0x401) */
__attribute__((noinline)) int MlnImpl_81016034(MlnServiceContext *context)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	if (context->id != -1) {
		item.type = MLN_SERVICE_ARG_HANDLE;
		item.value = context;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x401, &args, &reply, &status);
		if (ret != MLN_OK)
			goto out;
		ret = status;
		if (ret != MLN_OK)
			goto out;
	}
	ret = MLN_OK;
	context->id = -1;
out:
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x429 on the handle and a buffer */
__attribute__((noinline)) int MlnImpl_810160E8(MlnServiceContext *context, unsigned int size,
					       void *data)
{
	MlnBuffer first;
	MlnServiceArg items[2];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	first.size = size;
	first.data = data;
	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = &first;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x429, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}
