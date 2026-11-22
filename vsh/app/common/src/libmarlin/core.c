/*
 * The core's entry points that the API layer calls: init and term, a buffer pair taken from
 * the core, the HTTP requests a transaction step asks the application to send, and the frees
 * of a step's values. The file's name is ours.
 */
#include "mln_internal.h"

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

/* the response and the result of a request to the core, as buffers */
__attribute__((noinline)) int MlnImpl_81012F48(const MlnBuffer *state, const MlnBuffer *request,
					       MlnBuffer **first, MlnBuffer **second)
{
	MlnBlob secondBlob = { NULL, 0 }, firstBlob = { NULL, 0 };
	MlnBuffer *one = NULL, *two = NULL;
	int ret;

	if (first == NULL || second == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		ret = MlnImpl_81013074(state, request, &firstBlob, &secondBlob);
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

__attribute__((noinline)) int MlnImpl_81013074(const MlnBuffer *state, const MlnBuffer *request,
					       MlnBlob *response, MlnBlob *result)
{
	return MlnImpl_810318D4(state, request, response, result);
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
	if (reply->items == NULL || reply->count != 1)
		goto done;
	ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &block);
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
	} else if (out == NULL || in == NULL) {
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
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &buffer);
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
 * (get) sent with command 0x408, and released.
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
	ret = MlnImpl_81027084(MLN_HEAP_SECURE, reply->items, &flag);
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
	ret = MlnServiceCall(context->session, 0x408, &args, &reply, &status);
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
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &buffer);
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
					       unsigned int *out)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (out == NULL || buffer == NULL) {
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
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026FE4(MLN_HEAP_SECURE, reply->items, out);
					if (ret != MLN_OK)
						ret = MlnImpl_810191C4(ret);
					else
						ret = status;
				}
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
		items[1].type = MLN_SERVICE_ARG_LIST;
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
					ret = MlnImpl_8102701C(MLN_HEAP_SECURE, reply->items, &id);
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
	} else if (out == NULL || name == NULL) {
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
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &buffer);
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
	} else if (out == NULL || in == NULL) {
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
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &buffer);
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
	} else if (out == NULL || name == NULL) {
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
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &buffer);
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
	} else if (outSecond == NULL || outFirst == NULL) {
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
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &first);
					if (ret == MLN_OK)
						ret = MlnImpl_81026F0C(MLN_HEAP_SECURE,
								       &reply->items[1], &second);
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
	} else if (out == NULL || name == NULL) {
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
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &buffer);
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
	} else if (out == NULL || in == NULL) {
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
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &buffer);
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
	} else if (out == NULL || name == NULL) {
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
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &buffer);
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
	} else if (out == NULL || in == NULL) {
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
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &buffer);
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
					ret = MlnImpl_81026ED0(MLN_HEAP_SECURE, reply->items, &group);
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
					ret = MlnImpl_8102701C(MLN_HEAP_SECURE, reply->items, &id);
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

/* service command 0x42a on the handle and a record: the integer of its type 1 reply */
__attribute__((noinline)) int MlnImpl_8101617C(MlnServiceContext *context, void *record, int *out)
{
	MlnServiceArg items[2];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_RECORD;
		items[1].value = record;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x42a, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_8102701C(MLN_HEAP_SECURE, reply->items, out);
					if (ret != MLN_OK)
						ret = MlnImpl_810191C4(ret);
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x42b on another handle */
__attribute__((noinline)) int MlnImpl_81016270(MlnServiceContext *context, int id)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else {
		item.type = MLN_SERVICE_ARG_HANDLE;
		item.value = &id;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x42b, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* the rights info of command 0x42c */
__attribute__((noinline)) int MlnImpl_810162F4(MlnServiceContext *context, int value,
					       MlnRightsInfo **rights)
{
	if (context == NULL)
		return MLN_ERROR_INVALID_ARGUMENT;
	return MlnImpl_81014574(context, 0x42c, value, rights, NULL);
}

/* the rights info of command 0x42d */
__attribute__((noinline)) int MlnImpl_8101632C(MlnServiceContext *context, int value,
					       MlnRightsInfo **rights)
{
	if (context == NULL)
		return MLN_ERROR_INVALID_ARGUMENT;
	return MlnImpl_81014574(context, 0x42d, value, rights, NULL);
}

/* the rights info of command 0x42e */
__attribute__((noinline)) int MlnImpl_81016364(MlnServiceContext *context, int value,
					       MlnRightsInfo **rights)
{
	if (context == NULL)
		return MLN_ERROR_INVALID_ARGUMENT;
	if (rights == NULL)
		return MLN_CORE_ERROR_INVALID;
	return MlnImpl_81014574(context, 0x42e, value, rights, NULL);
}

/* the extended status block of command 0x42e */
__attribute__((noinline)) int MlnImpl_810163A8(MlnServiceContext *context, MlnBuffer **esb)
{
	if (context == NULL)
		return MLN_ERROR_INVALID_ARGUMENT;
	if (esb == NULL)
		return MLN_CORE_ERROR_INVALID;
	return MlnImpl_81014574(context, 0x42e, 0, NULL, esb);
}

/* stores a word big-endian */
static inline __attribute__((always_inline)) void MlnPutWord(unsigned char *p, unsigned int v)
{
	p[0] = (unsigned char)(v >> 24);
	p[1] = (unsigned char)(v >> 16);
	p[2] = (unsigned char)(v >> 8);
	p[3] = (unsigned char)v;
}

/*
 * Evaluates an object's rights (command 0x42f) under a limit sent as one or two big-endian
 * words; a partial reply still gives the rights info.
 */
__attribute__((noinline)) int MlnImpl_810163E4(MlnServiceContext *context, void *object,
					       const MlnEvaluationLimit *limit,
					       MlnRightsInfo **rights)
{
	MlnServiceArg items[3];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer *data = NULL;
	MlnBuffer *block = NULL;
	MlnRightsInfo *info = NULL;
	int status, partial;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	if (limit != NULL) {
		switch (limit->kind) {
		case 1:
		case 2:
		case 3:
			break;
		case MLN_LIMIT_KIND_WORD:
			data = MlnBufferCreate(MLN_HEAP_SECURE, 4, NULL);
			if (data == NULL)
				goto nomem;
			MlnPutWord(data->data, limit->value);
			/* fall through */
		case MLN_LIMIT_KIND_COUNT:
			if (limit->value == 0)
				goto invalid;
			if (limit->value <= 3)
				break;
			if (limit->value != 4)
				goto invalid;
			data = MlnBufferCreate(MLN_HEAP_SECURE, 8, NULL);
			if (data == NULL)
				goto nomem;
			MlnPutWord(data->data, limit->first);
			MlnPutWord((unsigned char *)data->data + 4, limit->second);
			break;
		default:
			goto invalid;
		}
	}
	/* Sony's frees a buffer here that is never set */
	MlnBufferDestroy(MLN_HEAP_SECURE, NULL);

	items[0].type = MLN_SERVICE_ARG_HANDLE;
	items[0].value = context;
	items[1].type = MLN_SERVICE_ARG_OBJECT;
	items[1].value = object;
	items[2].type = MLN_SERVICE_ARG_BUFFER;
	items[2].value = data;
	args.count = 3;
	args.items = items;
	ret = MlnServiceCall(context->session, 0x42f, &args, &reply, &status);
	if (ret != MLN_OK)
		goto out;
	ret = status;
	if (ret == MLN_OK) {
		partial = 0;
	} else {
		if (ret != MLN_SERVICE_STATUS_PARTIAL)
			goto out;
		partial = 1;
	}
	ret = MlnImpl_81026D94(reply, 1);
	if (ret != MLN_OK) {
		ret = MLN_SERVICE_ERROR_REPLY;
		goto out;
	}
	if (rights != NULL) {
		ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &block);
		if (ret == MLN_OK && block != NULL)
			ret = MlnRightsInfoParse(MLN_HEAP_SECURE, block->size, block->data, &info);
		if (ret != MLN_OK) {
			ret = MlnImpl_810191C4(ret);
			goto out;
		}
		*rights = info;
		info = NULL;
	}
	if (partial == 1)
		ret = MLN_SERVICE_STATUS_PARTIAL;
	goto out;
nomem:
	ret = MLN_ERROR_NO_MEMORY;
	goto fail;
invalid:
	ret = MLN_CORE_ERROR_INVALID;
fail:
	MlnBufferDestroy(MLN_HEAP_SECURE, data);
	data = NULL;
out:
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	MlnBufferDestroy(MLN_HEAP_SECURE, data);
	MlnBufferDestroy(MLN_HEAP_SECURE, block);
	MlnRightsInfoFree(MLN_HEAP_SECURE, info);
	return ret;
}

/* the rights info in the reply to command 0x430 on the handle and a buffer */
__attribute__((noinline)) int MlnImpl_810166AC(MlnServiceContext *context, MlnBuffer *in,
					       MlnRightsInfo **rights)
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
	} else if (rights == NULL || in == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = in;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x430, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &block);
					if (ret != MLN_OK) {
						ret = MlnImpl_810191C4(ret);
					} else if (block != NULL &&
						   (ret = MlnRightsInfoParse(MLN_HEAP_SECURE, block->size,
									     block->data, &info)) != MLN_OK) {
						/* a parse error as the service's: 10 for a bad block */
						if (ret != MLN_ERROR_NO_MEMORY)
							ret = ret == MLN_ERROR_FORMAT ||
							      ret == MLN_ERROR_UNSUPPORTED ? 10 : 7;
					} else {
						*rights = info;
						info = NULL;
					}
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	if (block != NULL)
		MlnBufferDestroy(MLN_HEAP_SECURE, block);
	if (info != NULL)
		MlnRightsInfoFree(MLN_HEAP_SECURE, info);
	return ret;
}

/* service command 0x431 on another handle, an offset, a size and a count: the new position */
__attribute__((noinline)) int MlnImpl_81016844(MlnServiceContext *context, int id,
					       unsigned int offset, unsigned int size,
					       unsigned int *position, unsigned int count)
{
	MlnServiceArg items[5];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (position == NULL || size == 0) {
		ret = MLN_CORE_ERROR_INVALID;
	} else if (count == 0) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = &id;
		items[1].type = MLN_SERVICE_ARG_OFFSET;
		items[1].value = &offset;
		items[2].type = MLN_SERVICE_ARG_SIZE;
		items[2].value = &size;
		items[3].type = MLN_SERVICE_ARG_OFFSET;
		items[3].value = position;
		items[4].type = MLN_SERVICE_ARG_SIZE;
		items[4].value = &count;
		args.count = 5;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x431, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_810270BC(MLN_HEAP_SECURE, reply->items, position);
					if (ret != MLN_OK)
						ret = MlnImpl_810191C4(ret);
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x432 on another handle, an integer and two buffers */
__attribute__((noinline)) int MlnImpl_8101698C(MlnServiceContext *context, int id, int value,
					       unsigned int firstSize, void *firstData,
					       unsigned int secondSize, void *secondData)
{
	MlnBuffer first;
	MlnBuffer second;
	MlnServiceArg items[4];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	first.size = firstSize;
	first.data = firstData;
	second.size = secondSize;
	second.data = secondData;
	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = &id;
		items[1].type = MLN_SERVICE_ARG_INTEGER;
		items[1].value = &value;
		items[2].type = MLN_SERVICE_ARG_BUFFER;
		items[2].value = &first;
		items[3].type = MLN_SERVICE_ARG_BUFFER;
		items[3].value = &second;
		args.count = 4;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x432, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x459 on an identifier: the buffer of its type 1 reply into out */
__attribute__((noinline)) int MlnImpl_81016A5C(MlnServiceContext *context, int id, MlnBuffer **out)
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
	if (out == NULL)
		goto invalid;
	item.type = MLN_SERVICE_ARG_ID;
	item.value = &id;
	args.count = 1;
	args.items = &item;
	ret = MlnServiceCall(context->session, 0x459, &args, &reply, &status);
	if (ret != MLN_OK)
		goto out;
	ret = status;
	if (ret != MLN_OK)
		goto out;
	if (MlnImpl_81026D94(reply, 1) != 0) {
		ret = MLN_SERVICE_ERROR_REPLY;
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, out);
	if (ret == MLN_OK)
		goto out;
invalid:
	ret = MLN_CORE_ERROR_INVALID;
out:
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/*
 * Service command 0x451 on another handle and a value: the buffer of its type 1 reply.
 * Sony's passes the value's address as a buffer, so the service reads the value as the
 * size and the handle beside it as the data.
 */
__attribute__((noinline)) int MlnImpl_81016B4C(MlnServiceContext *context, int id,
					       unsigned int value, MlnBuffer **out)
{
	MlnServiceArg items[2];
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
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = &id;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = &value;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x451, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &buffer);
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
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* opens the context's handle (command 0x413) */
__attribute__((noinline)) int MlnImpl_81016C70(MlnServiceContext *context)
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
		ret = MlnServiceCall(context->session, 0x413, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_8102701C(MLN_HEAP_SECURE, reply->items, &id);
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

/* closes the context's handle (command 0x414) */
__attribute__((noinline)) int MlnImpl_81016D68(MlnServiceContext *context)
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
		ret = MlnServiceCall(context->session, 0x414, &args, &reply, &status);
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

/* service command 0x415 on the handle: the buffer of its type 1 reply */
__attribute__((noinline)) int MlnImpl_81016E1C(MlnServiceContext *context, MlnBuffer **out)
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
		item.type = MLN_SERVICE_ARG_HANDLE;
		item.value = context;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x415, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &buffer);
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

/* service command 0x416 on the handle and a buffer: the buffer of its type 1 reply */
__attribute__((noinline)) int MlnImpl_81016F20(MlnServiceContext *context, MlnBuffer *in,
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
	} else if (out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = in;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x416, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &buffer);
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

/* service command 0x417 on the handle and a buffer */
__attribute__((noinline)) int MlnImpl_81017038(MlnServiceContext *context, MlnBuffer *in)
{
	MlnServiceArg items[2];
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
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x417, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x437 with a buffer given as its size and data (the context is not checked) */
__attribute__((noinline)) int MlnImpl_810170C4(MlnServiceContext *context, unsigned int size,
					       void *data)
{
	MlnBuffer buffer;
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	buffer.size = size;
	buffer.data = data;
	if (size == 0 || data == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = &buffer;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x437, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x438 with a buffer given as its size and data (the context is not checked) */
__attribute__((noinline)) int MlnImpl_81017158(MlnServiceContext *context, unsigned int size,
					       void *data)
{
	MlnBuffer buffer;
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	buffer.size = size;
	buffer.data = data;
	if (size == 0 || data == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = &buffer;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x438, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x439 with a buffer given as its size and data (the context is not checked) */
__attribute__((noinline)) int MlnImpl_810171EC(MlnServiceContext *context, unsigned int size,
					       void *data)
{
	MlnBuffer buffer;
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	buffer.size = size;
	buffer.data = data;
	if (size == 0 || data == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = &buffer;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x439, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x43a with a buffer given as its size and data (the context is not checked) */
__attribute__((noinline)) int MlnImpl_81017280(MlnServiceContext *context, unsigned int size,
					       void *data)
{
	MlnBuffer buffer;
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	buffer.size = size;
	buffer.data = data;
	if (size == 0 || data == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = &buffer;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x43a, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x43b with a buffer given as its size and data (the context is not checked) */
__attribute__((noinline)) int MlnImpl_81017314(MlnServiceContext *context, unsigned int size,
					       void *data)
{
	MlnBuffer buffer;
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	buffer.size = size;
	buffer.data = data;
	if (size == 0 || data == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = &buffer;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x43b, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x43c with a buffer given as its size and data (the context is not checked) */
__attribute__((noinline)) int MlnImpl_810173A8(MlnServiceContext *context, unsigned int size,
					       void *data)
{
	MlnBuffer buffer;
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	buffer.size = size;
	buffer.data = data;
	if (size == 0 || data == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = &buffer;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x43c, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/*
 * Service command 0x43d with a buffer given as its size and data (the context is not
 * checked): the buffer of its type 1 reply, when out is given.
 */
__attribute__((noinline)) int MlnImpl_8101743C(MlnServiceContext *context, unsigned int size,
					       void *data, MlnBuffer **out)
{
	MlnBuffer buffer;
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer *result = NULL;
	int status;
	int ret;

	buffer.size = size;
	buffer.data = data;
	if (size == 0 || data == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = &buffer;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x43d, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK && out != NULL) {
				if (MlnImpl_81026D94(reply, 1) != 0)
					ret = MLN_SERVICE_ERROR_REPLY;
				else if (MlnImpl_81026F0C(MLN_HEAP_SECURE, reply->items, &result) != 0)
					ret = MLN_CORE_ERROR_INVALID;
				else
					*out = result;
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x43e with two buffers given as their sizes and data */
__attribute__((noinline)) int MlnImpl_8101753C(MlnServiceContext *context,
					       unsigned int firstSize, void *firstData,
					       unsigned int secondSize, void *secondData)
{
	MlnBuffer first;
	MlnBuffer second;
	MlnServiceArg items[2];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	first.size = firstSize;
	first.data = firstData;
	second.size = secondSize;
	second.data = secondData;
	if (firstData == NULL || secondSize == 0 || secondData == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_BUFFER;
		items[0].value = &first;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = &second;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x43e, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x43f with a buffer given as its size and data (the context is not checked) */
__attribute__((noinline)) int MlnImpl_810175FC(MlnServiceContext *context, unsigned int size,
					       void *data)
{
	MlnBuffer buffer;
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	buffer.size = size;
	buffer.data = data;
	if (size == 0 || data == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = &buffer;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x43f, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x440 with a 28-byte block */
__attribute__((noinline)) int MlnImpl_81017690(MlnServiceContext *context, void *block)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (block == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BLOCK28;
		item.value = block;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x440, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x441 */
__attribute__((noinline)) int MlnImpl_81017710(MlnServiceContext *context)
{
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	ret = MlnServiceCall(context->session, 0x441, &args, &reply, &status);
	if (ret == MLN_OK)
		ret = status;
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* the record in the reply to service command 0x40a (zero when there is none) */
__attribute__((noinline)) MlnServiceRecord MlnImpl_81017764(MlnServiceContext *context)
{
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnServiceRecord record;
	int status;

	if (context != NULL) {
		MlnMemset(&record, 0, sizeof(record));
		if (MlnServiceCall(context->session, 0x40a, &args, &reply, &status) == MLN_OK &&
		    MlnImpl_81026D94(reply, 1) == 0)
			MlnImpl_81026F48(MLN_HEAP_SECURE, reply->items, &record);
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return record;
}

/* service command 0x409, its result dropped */
__attribute__((noinline)) void MlnImpl_8101782C(MlnServiceContext *context)
{
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;

	if (context != NULL)
		MlnServiceCall(context->session, 0x409, &args, &reply, &status);
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
}

/* the number of values in a reply (none without values) */
#define MLN_REPLY_COUNT(reply)	((reply)->items != NULL ? (reply)->count : 0)

/*
 * Opens the context's handle with command 0x442 on a list. Its reply holds the handle, a
 * count and that many buffers, a second count and buffers, and two more buffers; the
 * buffer arrays are allocated here.
 */
__attribute__((noinline)) int MlnImpl_81017880(MlnServiceContext *context, void *list,
					       unsigned int *firstCount, MlnBuffer ***first,
					       unsigned int *secondCount, MlnBuffer ***second,
					       MlnBuffer **third, MlnBuffer **fourth)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer **firsts = NULL;
	MlnBuffer **seconds = NULL;
	MlnBuffer *thirdBuffer = NULL;
	MlnBuffer *fourthBuffer = NULL;
	MlnBuffer *buffer = NULL;
	unsigned int nFirst, nSecond, i, n;
	int id = -1;
	int status;
	int ret;

	if (context == NULL || context->id != -1 || context->session == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	if (list == NULL || firstCount == NULL || secondCount == NULL ||
	    second == NULL || third == NULL || fourth == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	item.type = MLN_SERVICE_ARG_LIST;
	item.value = list;
	args.count = 1;
	args.items = &item;
	ret = MlnServiceCall(context->session, 0x442, &args, &reply, &status);
	if (ret != MLN_OK)
		goto out;
	ret = status;
	if (ret != MLN_OK)
		goto out;
	if (reply->items == NULL || reply->count <= 4)
		goto badreply;
	ret = MlnImpl_8102701C(MLN_HEAP_SECURE, &reply->items[0], &id);
	if (ret != MLN_OK)
		goto converted;
	if (id < 0) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	context->id = id;
	id = -1;
	ret = MlnImpl_81026FE4(MLN_HEAP_SECURE, &reply->items[1], &nFirst);
	if (ret != MLN_OK)
		goto converted;
	n = MLN_REPLY_COUNT(reply) - 5;
	if (nFirst > n || nFirst == 0)
		goto badreply;

	firsts = MlnAllocate(MLN_HEAP_SECURE, nFirst * sizeof(*firsts));
	if (firsts == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(firsts, 0, nFirst * sizeof(*firsts));
	for (i = 0; i < nFirst; i++) {
		ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, &reply->items[i + 2], &buffer);
		if (ret != MLN_OK)
			goto failed;
		firsts[i] = buffer;
	}
	ret = MlnImpl_81026FE4(MLN_HEAP_SECURE, &reply->items[i + 2], &nSecond);
	if (ret != MLN_OK)
		goto failed;
	n = MLN_REPLY_COUNT(reply) - 5;
	if (n - nFirst < nSecond || nSecond == 0) {
		ret = MLN_SERVICE_ERROR_REPLY;
		goto fail;
	}

	seconds = MlnAllocate(MLN_HEAP_SECURE, nSecond * sizeof(*seconds));
	if (seconds == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto fail;
	}
	MlnMemset(seconds, 0, nSecond * sizeof(*seconds));
	for (i = 0; i < nSecond; i++) {
		ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, &reply->items[nFirst + 3 + i], &buffer);
		if (ret != MLN_OK)
			goto failed;
		seconds[i] = buffer;
	}
	i += nFirst + 3;
	ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, &reply->items[i], &thirdBuffer);
	if (ret == MLN_OK)
		ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, &reply->items[i + 1], &fourthBuffer);
	if (ret != MLN_OK)
		goto failed;
	if (MlnImpl_81026D94(reply, i + 2) != 0) {
		ret = MLN_SERVICE_ERROR_REPLY;
		goto fail;
	}
	*firstCount = nFirst;
	*secondCount = nSecond;
	*first = firsts;
	*second = seconds;
	*third = thirdBuffer;
	thirdBuffer = NULL;
	*fourth = fourthBuffer;
	fourthBuffer = NULL;
	ret = MLN_OK;
	goto handed;

badreply:
	ret = MLN_SERVICE_ERROR_REPLY;
	goto out;
converted:
	ret = MlnImpl_810191C4(ret);
	goto out;
failed:
	ret = MlnImpl_810191C4(ret);
fail:
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	for (i = 0; i < nFirst; i++)
		MlnBufferDestroy(MLN_HEAP_SECURE, firsts[i]);
	if (seconds != NULL)
		for (i = 0; i < nSecond; i++)
			MlnBufferDestroy(MLN_HEAP_SECURE, seconds[i]);
	goto freed;
handed:
	firsts = NULL;
	seconds = NULL;
out:
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
freed:
	MlnFree(MLN_HEAP_SECURE, firsts);
	MlnFree(MLN_HEAP_SECURE, seconds);
	MlnBufferDestroy(MLN_HEAP_SECURE, thirdBuffer);
	MlnBufferDestroy(MLN_HEAP_SECURE, fourthBuffer);
	return ret;
}

/*
 * Opens the context's handle with command 0x442 on a list, two counted buffer lists and
 * copies of two buffers, all of them handed to the arguments.
 */
__attribute__((noinline)) int MlnImpl_81017D1C(MlnServiceContext *context, void **list,
					       unsigned int firstCount, MlnBuffer **first,
					       unsigned int secondCount, MlnBuffer **second,
					       const MlnBuffer *third, const MlnBuffer *fourth)
{
	MlnServiceArgs *args = NULL;
	MlnServiceReply *reply = NULL;
	unsigned int *firstTotal = NULL;
	unsigned int *secondTotal = NULL;
	MlnBuffer *thirdCopy = NULL;
	MlnBuffer *fourthCopy = NULL;
	unsigned int i;
	int id = -1;
	int status;
	int ret;

	if (context == NULL || context->id != -1 || context->session == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	if (list == NULL || first == NULL || second == NULL ||
	    third == NULL || third->size == 0 || third->data == NULL ||
	    fourth == NULL || fourth->size == 0 || fourth->data == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	ret = MLN_ERROR_NO_MEMORY;
	firstTotal = MlnAllocate(MLN_HEAP_SECURE, sizeof(*firstTotal));
	if (firstTotal == NULL)
		goto out;
	*firstTotal = firstCount;
	secondTotal = MlnAllocate(MLN_HEAP_SECURE, sizeof(*secondTotal));
	if (secondTotal == NULL)
		goto out;
	*secondTotal = secondCount;
	thirdCopy = MlnBufferCreate(MLN_HEAP_SECURE, third->size, third->data);
	if (thirdCopy == NULL)
		goto out;
	fourthCopy = MlnBufferCreate(MLN_HEAP_SECURE, fourth->size, fourth->data);
	if (fourthCopy == NULL)
		goto out;
	if (MlnImpl_81026E20(MLN_HEAP_SECURE, firstCount + 5 + secondCount, &args) != 0)
		goto out;

	/*
	 * Sony's frees what it handed to the arguments a second time when a buffer of the
	 * lists is empty.
	 */
	args->items[0].type = MLN_SERVICE_ARG_LIST;
	args->items[0].value = *list;
	*list = NULL;
	args->items[1].type = MLN_SERVICE_ARG_ID;
	args->items[1].value = firstTotal;
	for (i = 0; i < *firstTotal; i++) {
		if (first[i] == NULL || first[i]->size == 0 || first[i]->data == NULL) {
			ret = MLN_CORE_ERROR_INVALID;
			goto out;
		}
		args->items[i + 2].type = MLN_SERVICE_ARG_BUFFER;
		args->items[i + 2].value = first[i];
		first[i] = NULL;
	}
	args->items[i + 2].type = MLN_SERVICE_ARG_ID;
	args->items[i + 2].value = secondTotal;
	for (i = 0; i < *secondTotal; i++) {
		if (second[i] == NULL || second[i]->size == 0 || second[i]->data == NULL) {
			ret = MLN_CORE_ERROR_INVALID;
			goto out;
		}
		args->items[i + *firstTotal + 3].type = MLN_SERVICE_ARG_BUFFER;
		args->items[i + *firstTotal + 3].value = second[i];
		second[i] = NULL;
	}
	args->items[*secondTotal + *firstTotal + 3].type = MLN_SERVICE_ARG_BUFFER;
	args->items[*firstTotal + *secondTotal + 3].value = thirdCopy;
	args->items[*firstTotal + *secondTotal + 4].type = MLN_SERVICE_ARG_BUFFER;
	args->items[*firstTotal + *secondTotal + 4].value = fourthCopy;
	ret = MlnServiceCall(context->session, 0x442, args, &reply, &status);
	firstTotal = NULL;
	secondTotal = NULL;
	thirdCopy = NULL;
	fourthCopy = NULL;
	if (ret != MLN_OK)
		goto out;
	ret = status;
	if (ret != MLN_OK)
		goto out;
	if (MlnImpl_81026D94(reply, 1) != 0) {
		ret = MLN_SERVICE_ERROR_REPLY;
		goto out;
	}
	ret = MlnImpl_8102701C(MLN_HEAP_SECURE, &reply->items[0], &id);
	if (ret != MLN_OK) {
		ret = MlnImpl_810191C4(ret);
	} else if (id < 0) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else {
		context->id = id;
		id = -1;
	}
out:
	MlnImpl_81027390(MLN_HEAP_SECURE, args);
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	MlnFree(MLN_HEAP_SECURE, firstTotal);
	MlnFree(MLN_HEAP_SECURE, secondTotal);
	MlnBufferDestroy(MLN_HEAP_SECURE, thirdCopy);
	MlnBufferDestroy(MLN_HEAP_SECURE, fourthCopy);
	return ret;
}

/* a buffer the service takes: given, with a size and data */
#define MLN_BUFFER_VALID(b)	((b) != NULL && (b)->size != 0 && (b)->data != NULL)

/* closes the context's handle (command 0x443); a missing context is no error */
__attribute__((noinline)) int MlnImpl_81018220(MlnServiceContext *context)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret = MLN_OK;

	if (context != NULL && context->id != -1) {
		item.type = MLN_SERVICE_ARG_HANDLE;
		item.value = context;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x443, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK)
				context->id = -1;
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x444 on the handle and a buffer: the buffer of its type 1 reply */
__attribute__((noinline)) int MlnImpl_810182D0(MlnServiceContext *context, MlnBuffer *in,
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
	} else if (!MLN_BUFFER_VALID(in) || out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = in;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x444, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, &reply->items[0], &buffer);
					if (ret != MLN_OK)
						ret = MlnImpl_810191C4(ret);
					else
						*out = buffer;
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x445 on the handle and a buffer: the buffer of its type 1 reply */
__attribute__((noinline)) int MlnImpl_81018400(MlnServiceContext *context, MlnBuffer *in,
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
	} else if (!MLN_BUFFER_VALID(in) || out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = in;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x445, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, &reply->items[0], &buffer);
					if (ret != MLN_OK)
						ret = MlnImpl_810191C4(ret);
					else
						*out = buffer;
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x446 on the handle and a buffer: the buffer of its type 1 reply */
__attribute__((noinline)) int MlnImpl_81018528(MlnServiceContext *context, MlnBuffer *in,
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
	} else if (!MLN_BUFFER_VALID(in) || out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = in;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x446, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, &reply->items[0], &buffer);
					if (ret != MLN_OK)
						ret = MlnImpl_810191C4(ret);
					else
						*out = buffer;
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x447 on the handle and a buffer: the buffer of its type 1 reply */
__attribute__((noinline)) int MlnImpl_81018650(MlnServiceContext *context, MlnBuffer *in,
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
	} else if (!MLN_BUFFER_VALID(in) || out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = in;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x447, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, &reply->items[0], &buffer);
					if (ret != MLN_OK)
						ret = MlnImpl_810191C4(ret);
					else
						*out = buffer;
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	MlnBufferDestroy(MLN_HEAP_SECURE, NULL);	/* Sony's frees a buffer it never sets */
	return ret;
}

/* service command 0x448 on the handle and two buffers: the buffer of its type 1 reply */
__attribute__((noinline)) int MlnImpl_81018784(MlnServiceContext *context, MlnBuffer *in,
					       MlnBuffer *other, MlnBuffer **out)
{
	MlnServiceArg items[3];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer *buffer = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (!MLN_BUFFER_VALID(in) || !MLN_BUFFER_VALID(other) || out == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_HANDLE;
		items[0].value = context;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = in;
		items[2].type = MLN_SERVICE_ARG_BUFFER;
		items[2].value = other;
		args.count = 3;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x448, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, &reply->items[0], &buffer);
					if (ret != MLN_OK)
						ret = MlnImpl_810191C4(ret);
					else
						*out = buffer;
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* the two sizes in the reply to command 0x454 on two buffers (the context is not checked) */
__attribute__((noinline)) int MlnImpl_810188E8(MlnServiceContext *context, MlnBuffer *other,
					       MlnBuffer *in, int *first, int *second)
{
	MlnServiceArg items[2];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (!MLN_BUFFER_VALID(in) || first == NULL || second == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_BUFFER;
		items[0].value = other;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = in;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x454, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 2) != 0)
					ret = MLN_SERVICE_ERROR_REPLY;
				else if (MlnImpl_81026FE4(MLN_HEAP_SECURE, &reply->items[0],
							  (unsigned int *)first) != 0)
					ret = 7;
				else if (MlnImpl_81026FE4(MLN_HEAP_SECURE, &reply->items[1],
							  (unsigned int *)second) != 0)
					ret = 7;
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* the buffers in the reply to command 0x452, an even number of them (the context is unchecked) */
__attribute__((noinline)) int MlnImpl_81018A2C(MlnServiceContext *context, unsigned int *count,
					       MlnBuffer ***out)
{
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer **buffers = NULL;
	unsigned int n, i;
	int status;
	int ret;

	ret = MlnServiceCall(context->session, 0x452, &args, &reply, &status);
	if (ret != MLN_OK)
		goto out;
	ret = status;
	if (ret != MLN_OK)
		goto out;
	n = 0;
	if (reply->items != NULL) {
		n = reply->count;
		if (n & 1) {
			ret = MLN_SERVICE_ERROR_REPLY;
			goto out;
		}
		if (n != 0) {
			buffers = MlnAllocate(MLN_HEAP_SECURE, n * sizeof(*buffers));
			if (buffers == NULL) {
				ret = MLN_ERROR_NO_MEMORY;
				goto out;
			}
			MlnMemset(buffers, 0, n * sizeof(*buffers));
			for (i = 0; i < n; i++) {
				if (MlnImpl_81026F0C(MLN_HEAP_SECURE, &reply->items[i],
						     &buffers[i]) != 0) {
					for (i = 0; i < n; i++)
						MlnBufferDestroy(MLN_HEAP_SECURE, buffers[i]);
					MlnFree(MLN_HEAP_SECURE, buffers);
					ret = 7;
					goto out;
				}
			}
		}
	}
	*count = n;
	*out = buffers;
out:
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* the buffers in the reply to command 0x456 on three buffers (the context is not checked) */
__attribute__((noinline)) int MlnImpl_81018B8C(MlnServiceContext *context,
					       MlnBuffer *serviceProvider, MlnBuffer *token,
					       MlnBuffer *ids, unsigned int *count,
					       MlnBuffer ***out)
{
	MlnServiceArg items[3];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer **buffers = NULL;
	unsigned int n, i;
	int status;
	int ret;

	items[0].type = MLN_SERVICE_ARG_BUFFER;
	items[0].value = serviceProvider;
	items[1].type = MLN_SERVICE_ARG_BUFFER;
	items[1].value = token;
	items[2].type = MLN_SERVICE_ARG_BUFFER;
	items[2].value = ids;
	args.count = 3;
	args.items = items;
	ret = MlnServiceCall(context->session, 0x456, &args, &reply, &status);
	if (ret != MLN_OK)
		goto out;
	ret = status;
	if (ret != MLN_OK)
		goto out;
	n = 0;
	if (reply->items != NULL) {
		n = reply->count;
		if (n != 0) {
			buffers = MlnAllocate(MLN_HEAP_SECURE, n * sizeof(*buffers));
			if (buffers == NULL) {
				ret = MLN_ERROR_NO_MEMORY;
				goto out;
			}
			MlnMemset(buffers, 0, n * sizeof(*buffers));
			for (i = 0; i < n; i++) {
				if (MlnImpl_81026F0C(MLN_HEAP_SECURE, &reply->items[i],
						     &buffers[i]) != 0) {
					for (i = 0; i < n; i++)
						MlnBufferDestroy(MLN_HEAP_SECURE, buffers[i]);
					MlnFree(MLN_HEAP_SECURE, buffers);
					ret = 7;
					goto out;
				}
			}
		}
	}
	*count = n;
	*out = buffers;
out:
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* the buffer in the reply to service command 0x457 (the context is not checked) */
__attribute__((noinline)) int MlnImpl_81018D00(MlnServiceContext *context, void **out)
{
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	ret = MlnServiceCall(context->session, 0x457, &args, &reply, &status);
	if (ret == MLN_OK) {
		ret = status;
		if (ret == MLN_OK) {
			if (MlnImpl_81026D94(reply, 1) != 0)
				ret = MLN_SERVICE_ERROR_REPLY;
			else if (MlnImpl_81026F0C(MLN_HEAP_SECURE, &reply->items[0],
						  (MlnBuffer **)out) != 0)
				ret = 7;
			else
				ret = MLN_OK;
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x458 with a buffer (the context is not checked) */
__attribute__((noinline)) int MlnImpl_81018DA0(MlnServiceContext *context, MlnBuffer *in)
{
	MlnServiceArg item;
	MlnServiceArgs args;
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	item.type = MLN_SERVICE_ARG_BUFFER;
	item.value = in;
	args.count = 1;
	args.items = &item;
	ret = MlnServiceCall(context->session, 0x458, &args, &reply, &status);
	if (ret == MLN_OK)
		ret = status;
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x453 with two buffers (the context is not checked) */
__attribute__((noinline)) int MlnImpl_81018E08(MlnServiceContext *context, MlnBuffer *other,
					       MlnBuffer *in)
{
	MlnServiceArg items[2];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (!MLN_BUFFER_VALID(in)) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_BUFFER;
		items[0].value = other;
		items[1].type = MLN_SERVICE_ARG_BUFFER;
		items[1].value = in;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x453, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x449, whose reply has no values */
__attribute__((noinline)) int MlnImpl_81018EC4(MlnServiceContext *context)
{
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else {
		ret = MlnServiceCall(context->session, 0x449, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK && MlnImpl_81026D94(reply, 0) != 0)
				ret = MLN_SERVICE_ERROR_REPLY;
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* service command 0x44a, which closes the context's handle */
__attribute__((noinline)) int MlnImpl_81018F50(MlnServiceContext *context)
{
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else {
		ret = MlnServiceCall(context->session, 0x44a, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK)
				context->id = -1;
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/*
 * Service command 0x44b on a size and a value: the reply's buffer copied to data, unbounded,
 * and its size into size.
 */
__attribute__((noinline)) int MlnImpl_81018FD0(MlnServiceContext *context, unsigned int *size,
					       void *data, int value)
{
	MlnServiceArg items[2];
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	MlnBuffer *buffer = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (data == NULL || size == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		items[0].type = MLN_SERVICE_ARG_ID;
		items[0].value = size;
		items[1].type = MLN_SERVICE_ARG_ID;
		items[1].value = &value;
		args.count = 2;
		args.items = items;
		ret = MlnServiceCall(context->session, 0x44b, &args, &reply, &status);
		if (ret == MLN_OK) {
			ret = status;
			if (ret == MLN_OK) {
				if (MlnImpl_81026D94(reply, 1) != 0) {
					ret = MLN_SERVICE_ERROR_REPLY;
				} else {
					ret = MlnImpl_81026F0C(MLN_HEAP_SECURE, &reply->items[0], &buffer);
					if (ret != MLN_OK) {
						ret = MLN_CORE_ERROR_INVALID;
					} else {
						MlnMemcpy(data, buffer->data, buffer->size);
						*size = buffer->size;
					}
				}
			}
		}
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	MlnBufferDestroy(MLN_HEAP_SECURE, buffer);
	return ret;
}

/* service command 0x44c with a buffer */
__attribute__((noinline)) int MlnImpl_81019108(MlnServiceContext *context, MlnBuffer *in)
{
	MlnServiceArg item;
	MlnServiceArgs args = { 0, NULL };
	MlnServiceReply *reply = NULL;
	int status;
	int ret;

	if (context == NULL) {
		ret = MLN_ERROR_INVALID_ARGUMENT;
	} else if (!MLN_BUFFER_VALID(in)) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		item.type = MLN_SERVICE_ARG_BUFFER;
		item.value = in;
		args.count = 1;
		args.items = &item;
		ret = MlnServiceCall(context->session, 0x44c, &args, &reply, &status);
		if (ret == MLN_OK)
			ret = status;
	}
	MlnImpl_81027390(MLN_HEAP_SECURE, reply);
	return ret;
}

/* a value decoder's or encoder's error as the service's */
static inline __attribute__((always_inline)) int MlnServiceError(int ret)
{
	switch ((unsigned int)ret) {
	case MLN_OK:
	case MLN_ERROR_NO_MEMORY:
		return ret;
	case MLN_ERROR_INVALID_ARGUMENT:
	case 0x10010003:
	case 0x10010005:
	case 0x10010008:
	case 0x1001000a:
		return MLN_CORE_ERROR_INVALID;
	case 6:
		return MLN_SERVICE_STATUS_MORE;
	case MLN_ERROR_FORMAT:
	case MLN_ERROR_UNSUPPORTED:
		return 66;
	}
	if (((unsigned int)ret & 0xffff0000) == 0x11060000)
		return MLN_ERROR_INVALID;
	return 7;
}

__attribute__((noinline)) int MlnImpl_810191C4(int ret)
{
	return MlnServiceError(ret);
}

/* commands 0x42a to 0x451 a remote session refuses: the error they give (none for the others) */
static const unsigned short s_remoteCommandErrors[40] = {
	[0x42a - 0x42a] = MLN_SERVICE_ERROR_UNAVAILABLE,
	[0x42b - 0x42a] = MLN_SERVICE_ERROR_UNAVAILABLE,
	[0x42d - 0x42a] = MLN_SERVICE_ERROR_UNAVAILABLE,
	[0x431 - 0x42a] = MLN_SERVICE_ERROR_UNAVAILABLE,
	[0x44f - 0x42a] = MLN_SERVICE_ERROR_UNAVAILABLE,
	[0x450 - 0x42a] = MLN_SERVICE_ERROR_UNAVAILABLE,
	[0x451 - 0x42a] = MLN_SERVICE_ERROR_UNAVAILABLE,
};

__attribute__((noinline)) unsigned short MlnImpl_81019274(int command)
{
	unsigned int index = command - 0x42a;

	if (index >= sizeof(s_remoteCommandErrors) / sizeof(s_remoteCommandErrors[0]))
		return 0;
	return s_remoteCommandErrors[index];
}

/* a transport's failure as the service's */
static inline __attribute__((always_inline)) int MlnTransportError(unsigned int result)
{
	switch (result) {
	case MLN_TRANSPORT_OK:
		return MLN_OK;
	case MLN_TRANSPORT_INVALID:
		return MLN_CORE_ERROR_INVALID;
	case MLN_TRANSPORT_NO_MEMORY:
		return MLN_ERROR_NO_MEMORY;
	default:
		return MLN_SERVICE_ERROR_TRANSPORT;
	}
}

/*
 * Sends a command with its arguments on a session and gives back the service's reply and
 * status. The result is the transport's: the command's own outcome is the status.
 */
__attribute__((noinline)) int MlnServiceCall(MlnServiceSession *session, int command,
					     const MlnServiceArgs *args, MlnServiceReply **reply,
					     int *status)
{
	MlnBuffer *request = NULL;
	MlnBuffer *response = NULL;
	MlnServiceReply *decoded = NULL;
	const unsigned char *header;
	unsigned int result;
	int ret;

	if (session == NULL || args == NULL || reply == NULL || status == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	ret = MlnImpl_81019860(session, command);
	if (ret != MLN_OK)
		goto out;
	ret = MlnImpl_810273C4(MLN_HEAP_SECURE, command, 0, args, &request);
	if (ret != MLN_OK) {
		ret = MlnServiceError(ret);
		goto out;
	}
	result = session->transact(session->cookie, request, &response);
	if (result != MLN_TRANSPORT_OK) {
		ret = MlnTransportError(result);
	} else if (response == NULL) {
		ret = MLN_SERVICE_ERROR_UNAVAILABLE;
	} else if ((header = response->data) == NULL) {
		ret = MLN_SERVICE_ERROR_REPLY;
	} else if (MlnImpl_81026CBC(header) != (unsigned int)command) {
		ret = MLN_SERVICE_ERROR_COMMAND;
	} else {
		ret = MlnImpl_8102752C(MLN_HEAP_SECURE, response, &decoded);
		if (ret != MLN_OK) {
			ret = MlnImpl_810191C4(ret);
		} else {
			*status = MlnImpl_81026D04(header);
			*reply = decoded;
			decoded = NULL;
		}
	}
out:
	if (session != NULL && session->release != NULL)
		session->release(response);
	MlnImpl_81027390(MLN_HEAP_SECURE, decoded);
	MlnBufferDestroy(MLN_HEAP_SECURE, request);
	return ret;
}

/* the local transport's release */
__attribute__((noinline)) int MlnImpl_810194B4(MlnBuffer *response)
{
	MlnBufferDestroy(MLN_HEAP_SECURE, response);
	return MLN_OK;
}

/* a 16-byte response that gives the request's command the error */
static inline __attribute__((always_inline)) int MlnServiceErrorResponse(const MlnBuffer *request,
									 int error,
									 MlnBuffer **response)
{
	MlnBuffer *header = MlnBufferCreate(MLN_HEAP_SECURE, 16, NULL);

	if (header == NULL)
		return MLN_ERROR_NO_MEMORY;
	MlnImpl_81026BFC(header->data, MlnImpl_81026CBC(request->data));
	MlnImpl_81026C1C(header->data, 16);
	MlnImpl_81026C3C(header->data, error);
	MlnImpl_81026C5C(header->data, 0);
	*response = header;
	return MLN_OK;
}

/*
 * Runs a request in this process and gives its response; a request that fails gets a
 * response with the error. The state the command leaves behind is restored when its
 * status says so.
 */
__attribute__((noinline)) int MlnImpl_810194CC(const MlnBuffer *request, MlnBuffer **response)
{
	MlnBuffer *state = NULL;
	MlnBuffer *out = NULL;
	int status;
	int ret;

	if (request == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto done;
	}
	if (request->size == 0 || request->data == NULL || request->size < 16) {
		ret = MLN_CORE_ERROR_INVALID;
		if (response != NULL)
			goto fail;
		goto done;
	}
	if (response == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto done;
	}
	MlnImpl_81026CBC(request->data);
	ret = MlnImpl_81012F48(NULL, request, &out, &state);
	if (ret != MLN_OK)
		goto fail;
	status = MlnImpl_81026D04(out->data);
	if (state != NULL) {
		switch (status) {
		case MLN_OK:
		case 9:
		case 10:
		case 11:
		case 15:
		case 16:
		case MLN_SERVICE_STATUS_PARTIAL:
		case MLN_SERVICE_STATUS_MORE:
			ret = MlnStateRestore(state);
			if (ret == MLN_OK)
				break;
			ret = MlnImpl_810191C4(ret);
			if (ret != MLN_OK)
				goto fail;
			/* Sony's drops the response when the error converts to none */
			goto done;
		}
	}
	*response = out;
	out = NULL;
	goto done;
fail:
	ret = MlnServiceErrorResponse(request, ret, response);
done:
	MlnBufferDestroy(MLN_HEAP_SECURE, state);
	/* Sony's frees a buffer here that is never set */
	MlnBufferDestroy(MLN_HEAP_SECURE, NULL);
	MlnBufferDestroy(MLN_HEAP_SECURE, out);
	return ret;
}

/* the local transport: runs the request in this process */
__attribute__((noinline)) int MlnImpl_81019760(void *cookie, const MlnBuffer *request,
					       MlnBuffer **response)
{
	switch (MlnImpl_810194CC(request, response)) {
	case MLN_OK:
		return MLN_TRANSPORT_OK;
	case MLN_ERROR_NO_MEMORY:
		return MLN_TRANSPORT_NO_MEMORY;
	case MLN_CORE_ERROR_INVALID:
		return MLN_TRANSPORT_INVALID;
	default:
		return MLN_TRANSPORT_FAILED;
	}
}

__attribute__((noinline)) int MlnPlatformInit(void)
{
	return MLN_OK;
}

__attribute__((noinline)) int MlnPlatformTerm(void)
{
	return MLN_OK;
}

/*
 * Runs a request a remote session sent: the commands such a session may not send get
 * their error, as does a request that fails.
 */
__attribute__((noinline)) int MlnImpl_81019798(const MlnBuffer *request, MlnBuffer **response)
{
	int ret;

	if (request == NULL)
		return MLN_CORE_ERROR_INVALID;
	if (request->size == 0 || request->data == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
	} else {
		ret = MlnImpl_81019274(MlnImpl_81026CBC(request->data));
		if (ret == MLN_OK) {
			ret = MlnImpl_810194CC(request, response);
			if (ret == MLN_OK)
				return ret;
		}
	}
	if (response == NULL)
		return ret;
	return MlnServiceErrorResponse(request, ret, response);
}

/*
 * Checks a session before a command: a session with no transport gets the local one; a
 * remote session may not send every command.
 */
__attribute__((noinline)) int MlnImpl_81019860(MlnServiceSession *session, int command)
{
	if (session == NULL)
		return MLN_CORE_ERROR_INVALID;
	if (session->transact == NULL) {
		if (session->release != NULL)
			return MLN_SERVICE_ERROR_UNAVAILABLE;
		session->transact = MlnImpl_81019760;
		session->release = MlnImpl_810194B4;
		return MLN_OK;
	}
	if (session->release == NULL)
		return MLN_SERVICE_ERROR_UNAVAILABLE;
	if (session->transact == MlnImpl_81019760)
		return MLN_OK;
	return MlnImpl_81019274(command);
}
