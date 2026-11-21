/*
 * The remote client's messages: each is a 16-byte header and its fields, big-endian words; a
 * buffer is a size and an offset from the end of its entry, its bytes after the entries. The
 * file's name is ours.
 */
#include "mln_internal.h"

static inline __attribute__((always_inline)) unsigned int MlnBufferLength(const MlnBuffer *buffer)
{
	return buffer != NULL ? buffer->size : 0;
}

/* a big-endian word */
static inline __attribute__((always_inline)) void MlnMessagePutWord(unsigned char *p,
								    unsigned int value)
{
	unsigned int word = MlnNtohl(value);

	MlnMemcpy(p, &word, sizeof(word));
}

/* a big-endian halfword */
static inline __attribute__((always_inline)) void MlnMessagePutHalf(unsigned char *p,
								    unsigned int value)
{
	unsigned short half = MlnHtons(value);

	MlnMemcpy(p, &half, sizeof(half));
}

/* the size of a buffer group's entry table: its count, an entry for each, each entry's buffers */
static inline __attribute__((always_inline)) unsigned int
MlnBufferGroupEntriesSize(const MlnBufferGroup *group)
{
	MlnBuffer **entry;
	unsigned int size, i;

	if (group->entryCount == 0 || group->entries == NULL)
		return 0;
	size = group->entryCount * 8 + 4;
	for (i = 0; group->entryCount > i; i++) {
		entry = group->entries[i];
		if (entry != NULL)
			size += MlnBufferLength(entry[0]) + 24 + MlnBufferLength(entry[1]) +
				MlnBufferLength(entry[2]);
	}
	return size;
}

/* the size of a rights summary's ID table */
static inline __attribute__((always_inline)) unsigned int
MlnRightsIdsSize(const MlnRightsSummary *rights)
{
	unsigned int size = rights->idCount * 8 + 4, i;

	if (rights->ids != NULL) {
		for (i = 0; i != rights->idCount; i++)
			size += MlnBufferLength(rights->ids[i]);
	}
	return size;
}

/* count buffers: their entries, then their bytes */
__attribute__((noinline)) void MlnImpl_81020470(unsigned char **cursor, unsigned int count,
						MlnBuffer **buffers)
{
	unsigned char *p = *cursor, *data;
	unsigned int offset, i;

	data = p;
	if (count != 0) {
		offset = count * 8;
		for (i = 0; i != count; i++) {
			offset -= 8;
			MlnMessagePutWord(p, MlnBufferLength(buffers[i]));
			MlnMessagePutWord(p + 4, MlnBufferLength(buffers[i]) != 0 ? offset : 0);
			p += 8;
			offset += MlnBufferLength(buffers[i]);
		}
		data += count * 8;
		for (i = 0; i != count; i++) {
			if (MlnBufferLength(buffers[i]) != 0)
				MlnMemcpy(data, buffers[i]->data, buffers[i]->size);
			data += MlnBufferLength(buffers[i]);
		}
	}
	*cursor = data;
}

/* a list of pairs: its count, an entry for each pair, then each pair's buffers */
__attribute__((noinline)) void MlnImpl_810205A4(unsigned char *p, MlnMessagePairs *pairs)
{
	MlnBuffer **buffers;
	unsigned int offset, size, i;

	MlnMessagePutWord(p, pairs->count);
	p += 4;
	offset = (pairs->count - 1) * 8;
	if (pairs->count == 0)
		return;
	i = 0;
	do {
		buffers = pairs->items[i++]->buffers;
		size = MlnBufferLength(buffers[0]) + 16 + MlnBufferLength(buffers[1]);
		MlnMessagePutWord(p, size);
		p += 4;
		MlnMessagePutWord(p, offset);
		offset += size - 8;
		p += 4;
	} while (pairs->count > i);
	for (i = 0; pairs->count > i; i++)
		MlnImpl_81020470(&p, pairs->items[i]->count, pairs->items[i]->buffers);
}

/* the encoded size of a buffer group */
__attribute__((noinline)) unsigned int MlnImpl_810206C0(const MlnBufferGroup *group)
{
	unsigned int size;

	if (group == NULL)
		return 0;
	size = MlnBufferLength(group->first) + 32 + MlnBufferLength(group->second) +
	       MlnBufferLength(group->third);
	return size + MlnBufferGroupEntriesSize(group);
}

/* the encoded size of a message, by its type; 6 for a type it cannot send */
__attribute__((noinline)) int MlnImpl_81020784(const MlnMessage *message, unsigned int *out)
{
	MlnBuffer **buffers = message->payload;
	MlnMessageResult *result;
	MlnRightsSummary *rights;
	MlnMessagePairs *pairs;
	MlnBuffer **pair;
	unsigned int size, i;

	switch (message->type) {
	case 0x400:
	case MLN_MESSAGE_HTTP_POST:
		size = MlnBufferLength(buffers[1]) + 44 + MlnBufferLength(buffers[2]) +
		       MlnBufferLength(buffers[3]);
		break;
	case 0x401:
	case MLN_MESSAGE_STATUS:
	case 0x10a0:
		size = 32;
		break;
	case MLN_MESSAGE_HTTP_GET:
	case MLN_MESSAGE_HTTP_BODY:
	case 0x500:
	case 0x601:
	case 0x10b0:
	case MLN_MESSAGE_TOKEN:
		size = MlnBufferLength(buffers[0]) + 24;
		break;
	case MLN_MESSAGE_VALUE:
		size = MlnBufferLength(buffers[1]) + 28;
		break;
	case MLN_MESSAGE_RIGHTS:
		rights = message->payload;
		size = 52;
		if (rights->idCount != 0)
			size += MlnRightsIdsSize(rights);
		break;
	case 0x502:
	case 0x603:
	case 0x10a6:
		size = 16;
		break;
	case MLN_MESSAGE_LIST:
	case MLN_MESSAGE_PAIRS:
		pairs = message->payload;
		size = 20;
		for (i = 0; i != pairs->count; i++) {
			if (pairs->items[i]->count == 0) {
				size += 8;
				continue;
			}
			pair = pairs->items[i]->buffers;
			if (pair[0] != NULL && pair[0]->size != 0 && pair[0]->data == NULL)
				return MLN_CORE_ERROR_INVALID;
			if (pair[1] != NULL && pair[1]->size != 0 && pair[1]->data == NULL)
				return MLN_CORE_ERROR_INVALID;
			size += MlnBufferLength(pair[0]) + 24 + MlnBufferLength(pair[1]);
		}
		break;
	case 0x504:
	case 0x506:
	case 0x10a4:
		size = MlnBufferLength(buffers[0]) + 32 + MlnBufferLength(buffers[1]);
		break;
	case MLN_MESSAGE_PAIR:
		size = 24;
		break;
	case MLN_MESSAGE_VERSION:
		size = MlnBufferLength(((MlnApiVersion *)message->payload)->name) + 40 +
		       MlnBufferLength(((MlnApiVersion *)message->payload)->id);
		break;
	case MLN_MESSAGE_RESULT:
		result = message->payload;
		if (result->status == MLN_RESULT_VALUE)
			size = MlnImpl_810206C0(result->data) + 28;
		else if (result->status == MLN_RESULT_NONE)
			size = 32;
		else
			size = 28;
		break;
	case MLN_MESSAGE_HANDLE:
	case 0x10a2:
	case MLN_MESSAGE_FOUND:
		size = MlnBufferLength(buffers[0]) + 20;
		break;
	case MLN_MESSAGE_STATE:
	case 0x10ae:
		size = 20;
		break;
	default:
		return 6;
	}
	*out = size;
	return MLN_OK;
}

/* a buffer group as one buffer: its three buffers and its entry table, each entry encoded first */
static inline __attribute__((always_inline)) int MlnBufferGroupEncode(const MlnBufferGroup *group,
								      MlnBuffer **out)
{
	MlnBuffer *buffers[4], *entry[3], *encoded = NULL, *entries = NULL, **tables = NULL;
	unsigned char *cursor;
	unsigned int i;
	int error = MLN_OK;

	*out = NULL;
	if (group == NULL)
		goto out;
	buffers[0] = group->first;
	buffers[1] = group->second;
	buffers[2] = group->third;
	buffers[3] = NULL;
	if (group->entryCount != 0) {
		tables = MlnAllocate(1, group->entryCount * sizeof(*tables));
		if (tables == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnMemset(tables, 0, group->entryCount * sizeof(*tables));
		for (i = 0; i < group->entryCount; i++) {
			entry[0] = group->entries[i][0];
			entry[1] = group->entries[i][1];
			entry[2] = group->entries[i][2];
			tables[i] = MlnBufferCreate(1, group->entries[i] != NULL ?
						    MlnBufferLength(group->entries[i][0]) + 24 +
						    MlnBufferLength(group->entries[i][1]) +
						    MlnBufferLength(group->entries[i][2]) : 0, NULL);
			if (tables[i] == NULL) {
				error = MLN_ERROR_NO_MEMORY;
				goto out;
			}
			cursor = tables[i]->data;
			MlnImpl_81020470(&cursor, 3, entry);
		}
		entries = MlnBufferCreate(1, MlnBufferGroupEntriesSize(group), NULL);
		if (entries == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		cursor = entries->data;
		MlnMessagePutWord(cursor, group->entryCount);
		cursor += 4;
		MlnImpl_81020470(&cursor, group->entryCount, tables);
		buffers[3] = entries;
	}
	encoded = MlnBufferCreate(1, MlnImpl_810206C0(group), NULL);
	if (encoded == NULL) {
		error = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	cursor = encoded->data;
	MlnImpl_81020470(&cursor, 4, buffers);
	*out = encoded;
	encoded = NULL;
out:
	MlnBufferDestroy(1, encoded);
	MlnBufferDestroy(1, entries);
	if (tables != NULL) {
		for (i = 0; i < group->entryCount; i++)
			MlnBufferDestroy(1, tables[i]);
		MlnFree(1, tables);
	}
	return error;
}

/* a message's fields after its header; 7 for a payload where the type has none or the reverse */
__attribute__((noinline)) int MlnImpl_81020B20(unsigned char *p, const MlnMessage *message)
{
	void *payload = message->payload;
	MlnBuffer **fields = payload, *buffers[4], *value;
	const unsigned short *halves = payload;
	const int *words = payload;
	const MlnMessagePost *post;
	const MlnMessageValue *data;
	const MlnApiVersion *version;
	const MlnMessageResult *result;
	const MlnRightsSummary *rights;
	unsigned short flags;
	unsigned char *cursor = p;
	unsigned int word;
	int error;

	if (message->type == 0x603 || message->type == 0x10a6 || message->type == 0x502) {
		if (payload != NULL)
			return MLN_CORE_ERROR_INVALID;
	} else if (payload == NULL) {
		return MLN_CORE_ERROR_INVALID;
	}
	switch (message->type) {
	case 0x400:			/* MlnMessageAccount, the same layout */
	case MLN_MESSAGE_HTTP_POST:
		post = payload;
		buffers[0] = post->url;
		buffers[1] = post->body;
		buffers[2] = post->headers;
		MlnMessagePutHalf(cursor, post->method);
		cursor += 2;
		MlnMessagePutHalf(cursor, 0);
		cursor += 2;
		MlnImpl_81020470(&cursor, 3, buffers);
		return MLN_OK;
	case 0x401:
		MlnMessagePutHalf(p, halves[0]);
		return MLN_OK;
	case MLN_MESSAGE_HTTP_GET:
	case MLN_MESSAGE_HTTP_BODY:
	case 0x500:
	case 0x601:
	case 0x10b0:
	case MLN_MESSAGE_TOKEN:
		buffers[0] = fields[0];
		MlnImpl_81020470(&cursor, 1, buffers);
		return MLN_OK;
	case MLN_MESSAGE_VALUE:
		data = payload;
		MlnMessagePutHalf(cursor, data->kind);
		cursor += 2;
		MlnMessagePutHalf(cursor, 0);
		cursor += 2;
		buffers[0] = data->data;
		MlnImpl_81020470(&cursor, 1, buffers);
		return MLN_OK;
	case MLN_MESSAGE_STATUS:
		MlnMemcpy(p, payload, 4);
		return MLN_OK;
	case MLN_MESSAGE_RIGHTS:
		rights = payload;
		MlnMessagePutWord(p, 1);
		cursor = p + 4;
		MlnMessagePutWord(cursor, rights->idCount != 0 ? MlnRightsIdsSize(rights) + 24 : 24);
		cursor += 4;
		MlnMessagePutWord(cursor, 0);
		cursor += 4;
		flags = rights->notBefore != -1 ? 0x8000 : 0;
		if (rights->notAfter != -1)
			flags |= 0x4000;
		if (rights->notMoreThan != -1)
			flags |= 0x2000;
		if (rights->notLongerThan != -1)
			flags |= 0x1000;
		MlnMessagePutHalf(cursor, rights->state);
		cursor += 2;
		MlnMessagePutHalf(cursor, flags);
		cursor += 2;
		MlnMessagePutWord(cursor, rights->notBefore);
		cursor += 4;
		MlnMessagePutWord(cursor, rights->notAfter);
		cursor += 4;
		MlnMessagePutWord(cursor, rights->notMoreThan);
		cursor += 4;
		MlnMessagePutWord(cursor, rights->notLongerThan);
		cursor += 4;
		if (rights->idCount != 0) {
			MlnMessagePutWord(cursor, MlnRightsIdsSize(rights));
			cursor += 4;
		}
		MlnMessagePutWord(cursor, rights->idCount);
		cursor += 4;
		MlnImpl_81020470(&cursor, rights->idCount, rights->ids);
		return MLN_OK;
	case 0x502:
	case 0x603:
	case 0x10a6:
		return MLN_OK;
	case MLN_MESSAGE_LIST:
	case MLN_MESSAGE_PAIRS:
		MlnImpl_810205A4(p, payload);
		return MLN_OK;
	case 0x504:
	case 0x506:
	case 0x10a4:
		buffers[0] = fields[0];
		buffers[1] = fields[1];
		MlnImpl_81020470(&cursor, 2, buffers);
		return MLN_OK;
	case MLN_MESSAGE_PAIR:
		MlnMessagePutWord(p, words[0]);
		MlnMessagePutWord(p + 4, words[1]);
		return MLN_OK;
	case MLN_MESSAGE_VERSION:
		version = payload;
		buffers[0] = version->name;
		buffers[1] = version->id;
		MlnMessagePutHalf(cursor, version->major);
		cursor += 2;
		MlnMessagePutHalf(cursor, version->minor);
		cursor += 2;
		MlnMessagePutHalf(cursor, version->build);
		cursor += 2;
		MlnMessagePutHalf(cursor, 0);
		cursor += 2;
		MlnImpl_81020470(&cursor, 2, buffers);
		return MLN_OK;
	case MLN_MESSAGE_RESULT:
		result = payload;
		MlnMessagePutWord(p, result->status);
		cursor = p + 4;
		if (result->status == MLN_RESULT_VALUE) {
			error = MlnBufferGroupEncode(result->data, &value);
			if (error != MLN_OK)
				goto out;
		} else if (result->status == MLN_RESULT_NONE) {
			word = MlnHtonl(result->error);
			value = MlnBufferCreate(1, sizeof(word), &word);
			if (value == NULL) {
				error = MLN_ERROR_NO_MEMORY;
				goto out;
			}
		} else {
			value = NULL;
		}
		buffers[0] = value;
		MlnImpl_81020470(&cursor, 1, buffers);
		error = MLN_OK;
out:
		MlnBufferDestroy(1, value);
		return error;
	case 0x10a0:
		MlnMessagePutHalf(p, halves[0]);
		MlnMessagePutHalf(p + 2, halves[1]);
		MlnMemset(p + 4, 0, 12);
		return MLN_OK;
	case MLN_MESSAGE_HANDLE:
	case 0x10a2:
	case MLN_MESSAGE_FOUND:
		value = fields[0];
		MlnMessagePutWord(p, MlnBufferLength(value));
		MlnMemcpy(p + 4, value->data, value->size);
		return MLN_OK;
	case MLN_MESSAGE_STATE:
	case 0x10ae:
		MlnMessagePutWord(p, words[0]);
		return MLN_OK;
	default:
		return 6;
	}
}

/* a message as one buffer: its header (version 1, type, size) and its fields */
int MlnImpl_81021640(const MlnMessage *message, MlnBuffer **out)
{
	MlnBuffer *buffer = NULL;
	unsigned char *p;
	unsigned int size = 0, type;
	int error;

	error = MlnImpl_81020784(message, &size);
	if (error != MLN_OK)
		goto out;
	buffer = MlnBufferCreate(1, size, NULL);
	if (buffer == NULL) {
		error = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(buffer->data, 0, buffer->size);
	p = buffer->data;
	type = message->type;
	MlnMessagePutHalf(p, 1);
	MlnMessagePutWord(p + 2, type);
	MlnMessagePutWord(p + 6, size);
	error = MlnImpl_81020B20(buffer->data + 16, message);
	if (error == MLN_OK) {
		*out = buffer;
		buffer = NULL;
	}
out:
	MlnBufferDestroy(1, buffer);
	return error;
}

/* the client's account: a token (noToken 0), or the account and its secret (noToken 1) */
__attribute__((noinline)) int MlnImpl_81021734(int noToken, const MlnBuffer *token,
					       MlnBuffer *account, MlnBuffer *secret,
					       MlnBuffer **request)
{
	MlnBuffer *encoded = NULL;
	MlnMessageAccount fields;
	MlnMessage message;
	int error;

	fields.noToken = noToken;
	fields.token = token;
	fields.account = account;
	fields.secret = secret;
	message.type = 0x400;
	message.payload = &fields;
	if (request == NULL ||
	    (noToken == 0 ? token == NULL : noToken != 1 || token != NULL)) {
		error = MLN_CORE_ERROR_INVALID;
	} else {
		error = MlnImpl_81021640(&message, &encoded);
		if (error == MLN_OK) {
			*request = encoded;
			encoded = NULL;
		}
	}
	MlnBufferDestroy(1, encoded);
	return error;
}

/* type 0x401: a halfword, 2 only */
__attribute__((noinline)) int MlnImpl_810217E4(int type, MlnBuffer **request)
{
	MlnBuffer *encoded = NULL;
	unsigned short field = type;
	MlnMessage message;
	int error;

	message.type = 0x401;
	message.payload = &field;
	if (request == NULL) {
		error = MLN_CORE_ERROR_INVALID;
	} else if (type == 2) {
		error = MlnImpl_81021640(&message, &encoded);
		if (error == MLN_OK) {
			*request = encoded;
			encoded = NULL;
		}
	} else {
		error = MLN_CORE_ERROR_INVALID;
	}
	MlnBufferDestroy(1, encoded);
	return error;
}

/* the service asks for a URL to be fetched */
__attribute__((noinline)) int MlnImpl_81021864(const char *url, MlnBuffer **request)
{
	MlnBuffer *encoded = NULL, *buffer = NULL;
	MlnMessageGet fields = { NULL };
	MlnMessage message;
	int error;

	message.type = MLN_MESSAGE_HTTP_GET;
	message.payload = &fields;
	if (request == NULL || url == NULL) {
		error = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	buffer = MlnBufferCreate(1, MlnStrlen(url), url);
	if (buffer == NULL) {
		error = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	fields.url = buffer;
	error = MlnImpl_81021640(&message, &encoded);
	if (error == MLN_OK) {
		*request = encoded;
		encoded = NULL;
	}
out:
	MlnBufferDestroy(1, buffer);
	MlnBufferDestroy(1, encoded);
	return error;
}

/* the request's headers as one buffer, a "name:value" line with CR LF for each; none when empty */
static inline __attribute__((always_inline)) int MlnHttpHeadersEncode(const MlnHttpHeaders *headers,
								      MlnBuffer **out)
{
	MlnBuffer *buffer = NULL;
	unsigned int size = 0, i;
	int error = MLN_OK;

	*out = NULL;
	if (headers->count == 0)
		goto out;
	if (headers->items == NULL) {
		error = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	for (i = 0; i < headers->count; i++) {
		if (headers->items[i] == NULL) {
			error = MLN_CORE_ERROR_INVALID;
			goto out;
		}
		size += MlnStrlen(headers->items[i]->name) + MlnStrlen(headers->items[i]->value) + 3;
	}
	buffer = MlnBufferCreate(1, size + 1, NULL);
	if (buffer == NULL) {
		error = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(buffer->data, 0, size + 1);
	for (i = 0; i < headers->count; i++) {
		const char *name = headers->items[i]->name, *value = headers->items[i]->value;

		MlnStrncat((char *)buffer->data, name, MlnStrlen(name));
		MlnStrncat((char *)buffer->data, ":", 2);
		MlnStrncat((char *)buffer->data, value, MlnStrlen(value));
		MlnStrncat((char *)buffer->data, "\r\n", 2);
	}
	buffer->size--;			/* without the terminator */
	*out = buffer;
	buffer = NULL;
out:
	MlnBufferDestroy(1, buffer);
	return error;
}

/* the service asks for a request posted: method 0, the URL, its body and its headers */
__attribute__((noinline)) int MlnImpl_8102190C(int method, const char *url, MlnBuffer *body,
					       MlnHttpHeaders *headers, MlnBuffer **request)
{
	MlnBuffer *encoded = NULL, *address = NULL, *lines = NULL;
	MlnMessagePost fields;
	MlnMessage message;
	int error;

	fields.method = method;
	fields.url = NULL;
	fields.body = body;
	fields.headers = NULL;
	message.type = MLN_MESSAGE_HTTP_POST;
	message.payload = &fields;
	if (request == NULL || url == NULL) {
		error = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	if (method != 0 || body == NULL) {
		error = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	address = MlnBufferCreate(1, MlnStrlen(url), url);
	if (address == NULL) {
		error = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	fields.url = address;
	if (headers != NULL) {
		error = MlnHttpHeadersEncode(headers, &lines);
		if (error != MLN_OK)
			goto out;
		fields.headers = lines;
	}
	error = MlnImpl_81021640(&message, &encoded);
	if (error == MLN_OK) {
		*request = encoded;
		encoded = NULL;
	}
out:
	MlnBufferDestroy(1, address);
	MlnBufferDestroy(1, lines);
	MlnBufferDestroy(1, encoded);
	return error;
}

/* the body of a response the client fetched */
__attribute__((noinline)) int MlnImpl_81021BAC(MlnBuffer *response, MlnBuffer **request)
{
	MlnBuffer *encoded = NULL;
	MlnBuffer *field = response;
	MlnMessage message;
	int error;

	message.type = MLN_MESSAGE_HTTP_BODY;
	message.payload = &field;
	if (request == NULL) {
		error = MLN_CORE_ERROR_INVALID;
	} else {
		error = MlnImpl_81021640(&message, &encoded);
		if (error == MLN_OK) {
			*request = encoded;
			encoded = NULL;
		}
	}
	MlnBufferDestroy(1, encoded);
	return error;
}

/* a value for the client, kinds 1 to 5 */
__attribute__((noinline)) int MlnImpl_81021CD4(int kind, void *data, MlnBuffer **message)
{
	MlnBuffer *encoded = NULL;
	MlnMessageValue fields;
	MlnMessage request;
	int error;

	fields.kind = kind;
	fields.data = data;
	request.type = MLN_MESSAGE_VALUE;
	request.payload = &fields;
	if (message == NULL || data == NULL) {
		error = MLN_CORE_ERROR_INVALID;
	} else if ((unsigned int)(kind - 1) <= 4) {
		error = MlnImpl_81021640(&request, &encoded);
		if (error == MLN_OK) {
			*message = encoded;
			encoded = NULL;
		}
	} else {
		error = MLN_CORE_ERROR_INVALID;
	}
	MlnBufferDestroy(1, encoded);
	return error;
}

/* type 0x500: a license */
__attribute__((noinline)) int MlnImpl_81021D70(MlnBuffer *license, MlnBuffer **request)
{
	MlnBuffer *encoded = NULL;
	MlnBuffer *field = license;
	MlnMessage message;
	int error;

	message.type = 0x500;
	message.payload = &field;
	if (license == NULL || request == NULL) {
		error = MLN_CORE_ERROR_INVALID;
	} else {
		error = MlnImpl_81021640(&message, &encoded);
		if (error == MLN_OK) {
			*request = encoded;
			encoded = NULL;
		}
	}
	MlnBufferDestroy(1, encoded);
	return error;
}

/* a rights summary, with its own copy of the ID table */
__attribute__((noinline)) int MlnImpl_81021DE8(MlnApiReportData *data, MlnBuffer **message)
{
	MlnBuffer *encoded = NULL;
	MlnRightsSummary fields = { 0 };
	const MlnRightsSummary *rights = &data->rights;
	MlnMessage request;
	unsigned int i;
	int error;

	request.type = MLN_MESSAGE_RIGHTS;
	request.payload = &fields;
	if (data == NULL || message == NULL) {
		error = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	if (rights->idCount == 0) {
		fields.state = rights->state;
		fields.notBefore = rights->notBefore;
		fields.notAfter = rights->notAfter;
		fields.notMoreThan = rights->notMoreThan;
		fields.notLongerThan = rights->notLongerThan;
	} else {
		if (rights->ids == NULL) {
			error = MLN_CORE_ERROR_INVALID;
			goto out;
		}
		fields.state = rights->state;
		fields.notBefore = rights->notBefore;
		fields.notAfter = rights->notAfter;
		fields.notMoreThan = rights->notMoreThan;
		fields.notLongerThan = rights->notLongerThan;
		fields.idCount = rights->idCount;
		fields.ids = MlnAllocate(1, fields.idCount * sizeof(*fields.ids));
		if (fields.ids == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnMemset(fields.ids, 0, fields.idCount * sizeof(*fields.ids));
		for (i = 0; i < fields.idCount; i++) {
			if (rights->ids[i] == NULL) {
				error = MLN_CORE_ERROR_INVALID;
				goto out;
			}
			fields.ids[i] = rights->ids[i];
		}
	}
	error = MlnImpl_81021640(&request, &encoded);
	if (error == MLN_OK) {
		*message = encoded;
		encoded = NULL;
	}
out:
	if (fields.idCount != 0)
		MlnFree(1, fields.ids);
	MlnBufferDestroy(1, encoded);
	return error;
}

/* a list of buffers, sent as pairs: each two items one pair */
__attribute__((noinline)) int MlnImpl_81021F74(MlnApiReportData *data, MlnBuffer **message)
{
	MlnBuffer *encoded = NULL;
	MlnMessagePairs fields = { 0, NULL };
	MlnMessageList *list;
	MlnMessage request;
	unsigned int count, i;
	MlnBuffer **items;
	int error;

	request.type = MLN_MESSAGE_LIST;
	request.payload = &fields;
	if (data == NULL || message == NULL) {
		error = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	count = data->items.count;
	items = data->items.items;
	if ((count == 0 && items != NULL) || (count != 0 && items == NULL) || (count & 1) != 0) {
		error = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	if (count != 0) {
		fields.count = count / 2;
		fields.items = MlnAllocate(1, fields.count * sizeof(*fields.items));
		if (fields.items == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnMemset(fields.items, 0, fields.count * sizeof(*fields.items));
		for (i = 0; i < count; i += 2) {
			list = MlnAllocate(1, sizeof(*list));
			if (list == NULL) {
				error = MLN_ERROR_NO_MEMORY;
				goto out;
			}
			MlnMemset(list, 0, sizeof(*list));
			list->count = 2;
			list->buffers = MlnAllocate(1, 2 * sizeof(*list->buffers));
			if (list->buffers == NULL) {
				MlnFree(1, list->buffers);
				MlnFree(1, list);
				error = MLN_ERROR_NO_MEMORY;
				goto out;
			}
			MlnMemset(list->buffers, 0, 2 * sizeof(*list->buffers));
			list->buffers[0] = items[i];
			list->buffers[1] = items[i + 1];
			fields.items[i / 2] = list;
		}
	}
	error = MlnImpl_81021640(&request, &encoded);
	if (error == MLN_OK) {
		*message = encoded;
		encoded = NULL;
	}
out:
	if (fields.items != NULL) {
		for (i = 0; i < fields.count; i++) {
			if (fields.items[i] != NULL)
				MlnFree(1, fields.items[i]->buffers);
			MlnFree(1, fields.items[i]);
		}
		MlnFree(1, fields.items);
	}
	MlnBufferDestroy(1, encoded);
	return error;
}

/* type 0x502, no fields */
__attribute__((noinline)) int MlnImpl_81022180(MlnBuffer **request)
{
	MlnBuffer *encoded = NULL;
	MlnMessage message;
	int error;

	message.type = 0x502;
	message.payload = NULL;
	if (request == NULL) {
		error = MLN_CORE_ERROR_INVALID;
	} else {
		error = MlnImpl_81021640(&message, &encoded);
		if (error == MLN_OK) {
			*request = encoded;
			encoded = NULL;
		}
	}
	MlnBufferDestroy(1, encoded);
	return error;
}

/* type 0x601: the engine's address */
__attribute__((noinline)) int MlnImpl_810222A0(const MlnBuffer *address, MlnBuffer **request)
{
	MlnBuffer *encoded = NULL;
	const MlnBuffer *field = address;
	MlnMessage message;
	int error;

	message.type = 0x601;
	message.payload = &field;
	if (request == NULL) {
		error = MLN_CORE_ERROR_INVALID;
	} else {
		error = MlnImpl_81021640(&message, &encoded);
		if (error == MLN_OK) {
			*request = encoded;
			encoded = NULL;
		}
	}
	MlnBufferDestroy(1, encoded);
	return error;
}

/* the session's version: 1.0, its build, its name and ID */
__attribute__((noinline)) int MlnImpl_810223C8(int major, int minor, int build, MlnBuffer *name,
					       MlnBuffer *id, MlnBuffer **message)
{
	MlnBuffer *encoded = NULL;
	MlnApiVersion fields;
	MlnMessage request;
	int error;

	fields.major = major;
	fields.minor = minor;
	fields.build = build;
	fields.name = name;
	fields.id = id;
	request.type = MLN_MESSAGE_VERSION;
	request.payload = &fields;
	if (message == NULL || major != 1) {
		error = MLN_CORE_ERROR_INVALID;
	} else if (minor != 0) {
		error = MLN_CORE_ERROR_INVALID;
	} else {
		error = MlnImpl_81021640(&request, &encoded);
		if (error == MLN_OK) {
			*message = encoded;
			encoded = NULL;
		}
	}
	MlnBufferDestroy(1, encoded);
	return error;
}

/* type 0x504: two buffers, the second required */
__attribute__((noinline)) int MlnImpl_81022478(MlnBuffer *first, MlnBuffer *second,
					       MlnBuffer **request)
{
	MlnBuffer *encoded = NULL;
	MlnBuffer *fields[2] = { first, second };
	MlnMessage message;
	int error;

	message.type = 0x504;
	message.payload = fields;
	if (second == NULL || request == NULL) {
		error = MLN_CORE_ERROR_INVALID;
	} else {
		error = MlnImpl_81021640(&message, &encoded);
		if (error == MLN_OK) {
			*request = encoded;
			encoded = NULL;
		}
	}
	MlnBufferDestroy(1, encoded);
	return error;
}

/* a pair of times */
__attribute__((noinline)) int MlnImpl_81022500(MlnApiReportData *data, MlnBuffer **message)
{
	MlnBuffer *encoded = NULL;
	MlnMessage request;
	int error;

	request.type = MLN_MESSAGE_PAIR;
	request.payload = data;
	if (data == NULL || message == NULL) {
		error = MLN_CORE_ERROR_INVALID;
	} else {
		error = MlnImpl_81021640(&request, &encoded);
		if (error == MLN_OK) {
			*message = encoded;
			encoded = NULL;
		}
	}
	MlnBufferDestroy(1, encoded);
	return error;
}

/* type 0x506: two buffers, both required */
__attribute__((noinline)) int MlnImpl_81022570(MlnBuffer *first, MlnBuffer *second,
					       MlnBuffer **request)
{
	MlnBuffer *encoded = NULL;
	MlnBuffer *fields[2] = { first, second };
	MlnMessage message;
	int error;

	message.type = 0x506;
	message.payload = fields;
	if (first == NULL || second == NULL) {
		error = MLN_CORE_ERROR_INVALID;
	} else if (request == NULL) {
		error = MLN_CORE_ERROR_INVALID;
	} else {
		error = MlnImpl_81021640(&message, &encoded);
		if (error == MLN_OK) {
			*request = encoded;
			encoded = NULL;
		}
	}
	MlnBufferDestroy(1, encoded);
	return error;
}
