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
