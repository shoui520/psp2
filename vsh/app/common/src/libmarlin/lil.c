/*
 * A core handle's pending response: the "LIL" list of content IDs, each
 * with the minute of its first use, as the store keeps it; the handle table,
 * where a handle is the index of its object's slot; the cache's secret and
 * its HMAC (0x81069968-0x8106A15C). The file's name is ours.
 */
#include <libhmac.h>
#include "mln_internal.h"

#define MLN_LIL_VERSION		1
#define MLN_LIL_HEADER_SIZE	4	/* "LIL" and the version */

/* the algorithms of MlnImpl_8106A0AC */
#define MLN_HMAC_SHA1		1
#define MLN_HMAC_SHA256		2
#define MLN_SHA1_DIGEST_SIZE	20
#define MLN_SHA256_DIGEST_SIZE	32

/* an entry of the list: the header, then the ID, padded to a word */
typedef struct MlnLilEntry {
	unsigned short size;		/* the whole entry */
	unsigned short idSize;
	unsigned int minutes;		/* the first use, in minutes */
	unsigned char expired;		/* marked by MlnImpl_81069D90 */
	unsigned char reserved[3];
	unsigned char id[];
} MlnLilEntry;

#define MLN_LIL_NEXT(entry)	((MlnLilEntry *)((unsigned char *)(entry) + (entry)->size))

/*
 * Make a response over the store's list: data itself when keep is set, a copy
 * of it otherwise, and an empty list (to be written) when there is none.
 */
unsigned int MlnImpl_81069968(MlnBuffer *data, int keep, MlnCoreResponse **out)
{
	MlnCoreResponse *response;
	MlnBuffer *list = NULL;
	unsigned char *header = NULL;
	unsigned int ret;

	if (out == NULL) {
		ret = 3;
		goto out;
	}
	response = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*response));
	if (response == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(response, 0, sizeof(*response));

	if (data != NULL) {
		if (keep) {
			response->data = data;
		} else {
			list = MlnBufferCreate(MLN_HEAP_GENERAL, data->size, data->data);
			if (list == NULL)
				goto fail;
			response->data = list;
			list = NULL;
		}
	} else {
		list = MlnBufferCreate(MLN_HEAP_GENERAL, MLN_LIL_HEADER_SIZE, NULL);
		if (list == NULL) {
			MlnFree(MLN_HEAP_GENERAL, header);
			goto fail;
		}
		header = list->data;
		MlnMemcpy(header, "LIL", 3);
		header[3] = MLN_LIL_VERSION;
		response->data = list;
		response->pending = 1;
		list = NULL;
		header = NULL;
		MlnFree(MLN_HEAP_GENERAL, header);
	}
	*out = response;
	ret = 0;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, list);
	return ret;
fail:
	MlnBufferDestroy(MLN_HEAP_GENERAL, list);
	MlnImpl_81069A9C(response);
	return 2;
}

/* free a response and its buffers */
int MlnImpl_81069A9C(MlnCoreResponse *response)
{
	if (response != NULL) {
		if (response->request != NULL)
			MlnBufferDestroy(MLN_HEAP_GENERAL, response->request);
		if (response->data != NULL)
			MlnBufferDestroy(MLN_HEAP_GENERAL, response->data);
		MlnFree(MLN_HEAP_GENERAL, response);
	}
	return 0;
}

/*
 * Find a content ID in the list: the response then names its entry, its first
 * use and the ID; an ID the list does not have leaves them 0.
 */
unsigned int MlnImpl_81069AE4(MlnCoreResponse *response, const MlnBuffer *id)
{
	MlnBuffer *request = NULL;
	const MlnBuffer *list;
	const unsigned char *data;
	MlnLilEntry *entry;
	unsigned int offset;
	unsigned int minutes;
	unsigned int ret;
	int remaining;
	int size;

	if (response == NULL || id == NULL)
		return 3;
	if (response->data == NULL)
		return 5;

	MlnBufferDestroy(MLN_HEAP_GENERAL, response->request);
	list = response->data;
	response->request = NULL;
	size = list->size;
	data = list->data;
	if (size < MLN_LIL_HEADER_SIZE || MlnMemcmp(data, "LIL", 3) != 0 ||
	    data[3] != MLN_LIL_VERSION) {
		ret = 3;
		goto out;
	}

	offset = 0;
	minutes = 0;
	remaining = size - MLN_LIL_HEADER_SIZE;
	if (remaining > 0) {
		entry = (MlnLilEntry *)(data + MLN_LIL_HEADER_SIZE);
		if (entry->size > remaining) {
			ret = 3;
			goto out;
		}
		for (;;) {
			if (entry->idSize == id->size &&
			    MlnMemcmp(entry->id, id->data, entry->idSize) == 0) {
				minutes = entry->minutes;
				offset = (unsigned char *)entry - response->data->data;
				request = MlnBufferCreate(MLN_HEAP_GENERAL, id->size, id->data);
				if (request == NULL) {
					ret = 2;
					goto out;
				}
				break;
			}
			remaining -= entry->size;
			if (remaining <= 0)
				break;
			entry = MLN_LIL_NEXT(entry);
			if (entry->size > remaining) {
				ret = 3;
				goto out;
			}
		}
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, response->request);
	ret = 0;
	response->request = request;
	response->entryOffset = offset;
	response->minutes = minutes;
	request = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, request);
	return ret;
}

/* add a content ID, first used at minutes, to the end of the list */
unsigned int MlnImpl_81069C38(MlnCoreResponse *response, const MlnBuffer *id, unsigned int minutes)
{
	MlnBuffer *grown = NULL;
	const MlnBuffer *list;
	MlnLilEntry *entry;
	unsigned char *data;
	unsigned int size;
	unsigned int ret;

	if (response == NULL || id == NULL)
		return 3;
	list = response->data;
	if (list == NULL)
		return 5;

	size = (id->size + sizeof(MlnLilEntry) + 3) & ~3;
	grown = MlnBufferCreate(MLN_HEAP_GENERAL, size + list->size, NULL);
	if (grown == NULL) {
		ret = 2;
	} else {
		MlnMemset(grown->data, 0, grown->size);
		data = grown->data;
		MlnMemcpy(data, list->data, list->size);
		entry = (MlnLilEntry *)(data + list->size);
		entry->size = size;
		entry->idSize = id->size;
		entry->minutes = minutes;
		MlnMemcpy(entry->id, id->data, id->size);
		if (response->data != NULL)
			MlnBufferDestroy(MLN_HEAP_GENERAL, response->data);
		response->data = grown;
		response->pending = 1;
		grown = NULL;
		ret = 0;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, grown);
	return ret;
}

/* move the found entry's first use to minutes, when that is later */
unsigned int MlnImpl_81069D28(MlnCoreResponse *response, unsigned int minutes)
{
	MlnLilEntry *entry;

	if (response == NULL)
		return 3;
	if (response->data == NULL || response->entryOffset == 0)
		return 5;
	if (response->minutes < minutes) {
		entry = (MlnLilEntry *)(response->data->data + response->entryOffset);
		entry->minutes = minutes;
		response->minutes = minutes;
		response->pending = 1;
	}
	return 0;
}

/* drop the entries first used before the current minute */
unsigned int MlnImpl_81069D90(MlnCoreResponse *response)
{
	const MlnBuffer *list;
	MlnBuffer *kept;
	unsigned char *data;
	unsigned char *end;
	unsigned char *to;
	MlnLilEntry *entry;
	unsigned int removed;
	unsigned int now;
	int mode;
	unsigned int ret;

	if (response == NULL)
		return 3;
	if (response->data == NULL)
		return 0;
	ret = MlnImpl_81052D84((int *)&now, &mode);
	if (ret != 0)
		return ret;

	list = response->data;
	data = list->data;
	now /= 60;
	end = data + list->size;
	removed = 0;
	for (entry = (MlnLilEntry *)(data + MLN_LIL_HEADER_SIZE); (unsigned char *)entry < end;
	     entry = MLN_LIL_NEXT(entry)) {
		if (now > entry->minutes) {
			entry->expired = 1;
			removed += entry->size;
		}
	}
	if (removed == 0)
		return 0;

	kept = MlnBufferCreate(MLN_HEAP_GENERAL, response->data->size - removed, NULL);
	if (kept == NULL)
		return 2;
	data = kept->data;
	to = data + MLN_LIL_HEADER_SIZE;
	end = data + kept->size;
	entry = (MlnLilEntry *)(response->data->data + MLN_LIL_HEADER_SIZE);
	MlnMemcpy(data, "LIL", 3);
	data[3] = MLN_LIL_VERSION;
	while (to < end) {
		if (entry->expired) {
			entry = MLN_LIL_NEXT(entry);
		} else {
			MlnMemcpy(to, entry, entry->size);
			to += entry->size;
			entry = MLN_LIL_NEXT(entry);
		}
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, response->data);
	response->data = kept;
	response->entryOffset = 0;
	response->pending = 1;
	response->minutes = 0;
	MlnBufferDestroy(MLN_HEAP_GENERAL, response->request);
	response->request = NULL;
	return ret;
}

/* clear the table's slots on its first use */
unsigned int MlnImpl_81069F10(MlnHandleTable *table)
{
	MlnSlotArray slots;

	if (table == NULL)
		return 4;
	slots.items = table->slots;
	slots.count = MLN_HANDLE_TABLE_SLOTS;
	if (table->initialized)
		return 0;
	if (MlnImpl_8107A3D0(&slots) != 0)
		return 7;
	table->initialized = 1;
	return 0;
}

/* put object into a free slot: *handle names it */
unsigned int MlnImpl_81069F68(void *object, MlnHandleTable *table, int *handle)
{
	MlnSlotArray slots;
	unsigned int index;

	if (object == NULL || table == NULL || handle == NULL)
		return 4;
	slots.items = table->slots;
	slots.count = MLN_HANDLE_TABLE_SLOTS;
	if (MlnImpl_8107A428(&slots, &index) != 0)
		return 7;
	if (MlnImpl_8107A48C(&slots, index, object) != 0)
		return 7;
	*handle = index;
	return 0;
}

/* the object a handle names */
unsigned int MlnImpl_81069FE8(int handle, MlnHandleTable *table, void *out)
{
	MlnSlotArray slots;
	void *object = NULL;

	if (table == NULL || out == NULL)
		return 4;
	slots.items = table->slots;
	slots.count = MLN_HANDLE_TABLE_SLOTS;
	if (MlnImpl_8107A4C0(&slots, handle, &object) != 0)
		return 3;
	*(void **)out = object;
	return 0;
}

/* set the object a handle names (NULL frees the handle) */
unsigned int MlnImpl_8106A048(int handle, void *object, MlnHandleTable *table)
{
	MlnSlotArray slots;

	if (table == NULL)
		return 4;
	slots.items = table->slots;
	slots.count = MLN_HANDLE_TABLE_SLOTS;
	if (MlnImpl_8107A48C(&slots, handle, object) != 0)
		return 7;
	return 0;
}

/* the cache's secret: "美容と健康に食後に一杯の紅茶を。" in Shift-JIS */
static const char MlnSecretText[] =
	"\x94\xfc\x97\x65\x82\xc6\x8c\x92\x8d\x4e\x82\xc9\x90\x48\x8c\xe3"
	"\x82\xc9\x88\xea\x94\x74\x82\xcc\x8d\x67\x92\x83\x82\xf0\x81\x42";

/* a copy of the cache's secret */
MlnBuffer *MlnImpl_8106A08C(int heap)
{
	return MlnBufferCreate(heap, sizeof(MlnSecretText) - 1, MlnSecretText);
}

int MlnImpl_8106A0A4(void)
{
	return 0;
}

/* the HMAC of message under key: algorithm 1 is HMAC-SHA1, 2 HMAC-SHA256 */
int MlnImpl_8106A0AC(int algorithm, const MlnBuffer *key, const MlnBuffer *message,
		     MlnBuffer **out)
{
	MlnBuffer *digest;

	if (algorithm == MLN_HMAC_SHA1) {
		digest = MlnBufferCreate(MLN_HEAP_GENERAL, MLN_SHA1_DIGEST_SIZE, NULL);
		if (digest == NULL)
			return 2;
		sceHmacSha1Digest(key->data, key->size, message->data, message->size,
				 digest->data);
	} else if (algorithm == MLN_HMAC_SHA256) {
		digest = MlnBufferCreate(MLN_HEAP_GENERAL, MLN_SHA256_DIGEST_SIZE, NULL);
		if (digest == NULL)
			return 2;
		sceHmacSha256Digest(key->data, key->size, message->data, message->size,
				 digest->data);
	} else {
		return 3;
	}
	*out = digest;
	return 0;
}
