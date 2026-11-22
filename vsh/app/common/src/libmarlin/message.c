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
	MlnBuffer **buffers;
	MlnMessageResult *result;
	MlnRightsSummary *rights;
	MlnMessagePairs *pairs;
	MlnBuffer **pair;
	unsigned int size, i;

	switch (message->type) {
	case 0x400:
	case MLN_MESSAGE_HTTP_POST:
		buffers = message->payload;
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
		buffers = message->payload;
		size = MlnBufferLength(buffers[0]) + 24;
		break;
	case MLN_MESSAGE_VALUE:
		buffers = message->payload;
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
		buffers = message->payload;
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
		buffers = message->payload;
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

/* a message's header: version 1, its type and its size */
static inline __attribute__((always_inline)) void MlnMessagePutHeader(unsigned char *p,
								      unsigned int type,
								      unsigned int size)
{
	MlnMessagePutHalf(p, 1);
	MlnMessagePutWord(p + 2, type);
	MlnMessagePutWord(p + 6, size);
}

/* a message as one buffer: its header (version 1, type, size) and its fields */
int MlnImpl_81021640(const MlnMessage *message, MlnBuffer **out)
{
	MlnBuffer *buffer = NULL;
	unsigned int size = 0;
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
	MlnMessagePutHeader(buffer->data, message->type, size);
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

/* a list of buffers, sent as a list of pairs: each two items one pair */
__attribute__((noinline)) int MlnImpl_81022608(MlnApiReportData *data, MlnBuffer **message)
{
	MlnBuffer *encoded = NULL;
	MlnMessagePairs fields = { 0, NULL };
	MlnMessageList *list;
	MlnMessage request;
	unsigned int count, i;
	MlnBuffer **items;
	int error;

	request.type = MLN_MESSAGE_PAIRS;
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
		MlnMemset(fields.items, 0, fields.count * sizeof(*fields.items));
		for (i = 0; i < fields.count; i++) {
			list = MlnAllocate(1, sizeof(*list));
			if (list == NULL) {
				error = MLN_ERROR_NO_MEMORY;
				goto out;
			}
			MlnMemset(list, 0, sizeof(*list));
			list->count = 2;
			list->buffers = NULL;
			list->buffers = MlnAllocate(1, 2 * sizeof(*list->buffers));
			if (list->buffers == NULL) {
				error = MLN_ERROR_NO_MEMORY;
				MlnBufferDestroy(1, encoded);
				MlnFree(1, list->buffers);
				MlnFree(1, list);
				goto release;
			}
			MlnMemset(list->buffers, 0, 2 * sizeof(*list->buffers));
			list->buffers[0] = items[2 * i];
			list->buffers[1] = items[2 * i + 1];
			fields.items[i] = list;
		}
	}
	error = MlnImpl_81021640(&request, &encoded);
	if (error == MLN_OK) {
		*message = encoded;
		encoded = NULL;
	}
out:
	MlnBufferDestroy(1, encoded);
release:
	if (fields.items != NULL) {
		for (i = 0; i < fields.count; i++) {
			if (fields.items[i] != NULL)
				MlnFree(1, fields.items[i]->buffers);
			MlnFree(1, fields.items[i]);
		}
		MlnFree(1, fields.items);
	}
	return error;
}

/* the state, 0 or 0x100100 */
__attribute__((noinline)) int MlnImpl_8102285C(MlnApiReportData *data, MlnBuffer **message)
{
	MlnBuffer *encoded = NULL;
	int field = 0;
	MlnMessage request;
	int error;

	request.type = MLN_MESSAGE_STATE;
	request.payload = &field;
	if (data == NULL || message == NULL) {
		error = MLN_CORE_ERROR_INVALID;
	} else if (data->state == 0x100100 || data->state == 0) {
		field = data->state;
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

/* type 0x10a6, no fields */
__attribute__((noinline)) int MlnImpl_810228FC(MlnBuffer **request)
{
	MlnBuffer *encoded = NULL;
	MlnMessage message;
	int error;

	message.type = 0x10a6;
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

/* type 0x603, no fields */
__attribute__((noinline)) int MlnImpl_81022A1C(MlnBuffer **request)
{
	MlnBuffer *encoded = NULL;
	MlnMessage message;
	int error;

	message.type = 0x603;
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

/* a result: a value with its data, or none (the error) or busy without */
__attribute__((noinline)) int MlnImpl_81022B3C(int status, int error, void *data, MlnBuffer **reply)
{
	MlnBuffer *encoded = NULL;
	MlnMessageResult fields;
	MlnMessage message;
	int ret;

	fields.status = status;
	fields.error = error;
	fields.data = data;
	message.type = MLN_MESSAGE_RESULT;
	message.payload = &fields;
	if (reply == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	if (status == MLN_RESULT_VALUE) {
		if (data == NULL) {
			ret = MLN_CORE_ERROR_INVALID;
			goto out;
		}
	} else {
		if (status != MLN_RESULT_NONE && status != MLN_RESULT_BUSY) {
			ret = MLN_CORE_ERROR_INVALID;
			goto out;
		}
		if (data != NULL) {
			ret = MLN_CORE_ERROR_INVALID;
			goto out;
		}
	}
	ret = MlnImpl_81021640(&message, &encoded);
	if (ret == MLN_OK) {
		*reply = encoded;
		encoded = NULL;
	}
out:
	MlnBufferDestroy(1, encoded);
	return ret;
}

/* type 0x10a4: two buffers, the second required */
__attribute__((noinline)) int MlnImpl_81022C10(MlnBuffer *first, MlnBuffer *second,
					       MlnBuffer **request)
{
	MlnBuffer *encoded = NULL;
	MlnBuffer *fields[2] = { first, second };
	MlnMessage message;
	int error;

	message.type = 0x10a4;
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

/* the status: 0, 0x10 to 0x13 or 0x80, and whether a result is to be fetched */
__attribute__((noinline)) int MlnImpl_81022C98(int status, int result, MlnBuffer **reply)
{
	MlnMessageStatus fields;
	MlnBuffer *encoded = NULL;
	MlnMessage message;
	int error;

	message.type = MLN_MESSAGE_STATUS;
	message.payload = &fields;
	if (reply == NULL) {
		error = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	if ((unsigned int)status <= 0x13) {
		if ((unsigned int)status < 0x10 && status != 0) {
			error = MLN_CORE_ERROR_INVALID;
			goto out;
		}
	} else if (status != 0x80) {
		error = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	MlnMemset(&fields, 0, 4);
	fields.failed = status;
	if (result == 1)
		fields.result = 1;
	error = MlnImpl_81021640(&message, &encoded);
	if (error == MLN_OK) {
		*reply = encoded;
		encoded = NULL;
	}
out:
	MlnBufferDestroy(1, encoded);
	return error;
}

/* type 0x10ae: a word */
__attribute__((noinline)) int MlnImpl_81022D54(void *arg, MlnBuffer **request)
{
	MlnBuffer *encoded = NULL;
	void *field = arg;
	MlnMessage message;
	int error;

	message.type = 0x10ae;
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

/* a buffer the client found */
__attribute__((noinline)) int MlnImpl_81022E7C(MlnApiReportData *arg, MlnBuffer **message)
{
	MlnBuffer *encoded = NULL;
	MlnBuffer *field = NULL;
	MlnMessage request;
	int error;

	request.type = MLN_MESSAGE_FOUND;
	request.payload = &field;
	if (arg == NULL || message == NULL) {
		error = MLN_CORE_ERROR_INVALID;
	} else {
		field = arg->buffer;
		error = MlnImpl_81021640(&request, &encoded);
		if (error == MLN_OK) {
			*message = encoded;
			encoded = NULL;
		}
	}
	MlnBufferDestroy(1, encoded);
	return error;
}

/* type 0x10a0: version 1.0 */
__attribute__((noinline)) int MlnImpl_81022EFC(MlnBuffer **request)
{
	MlnBuffer *encoded = NULL;
	unsigned short fields[2] = { 1, 0 };
	MlnMessage message;
	int error;

	message.type = 0x10a0;
	message.payload = fields;
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

/* a handle */
__attribute__((noinline)) int MlnImpl_8102302C(MlnBuffer *arg, MlnBuffer **message)
{
	MlnBuffer *encoded = NULL;
	MlnBuffer *field = NULL;
	MlnMessage request;
	int error;

	request.type = MLN_MESSAGE_HANDLE;
	request.payload = &field;
	if (arg == NULL || message == NULL) {
		error = MLN_CORE_ERROR_INVALID;
	} else {
		field = arg;
		error = MlnImpl_81021640(&request, &encoded);
		if (error == MLN_OK) {
			*message = encoded;
			encoded = NULL;
		}
	}
	MlnBufferDestroy(1, encoded);
	return error;
}

/* type 0x10a2: one buffer */
__attribute__((noinline)) int MlnImpl_810230AC(MlnBuffer *arg, MlnBuffer **request)
{
	MlnBuffer *encoded = NULL;
	MlnBuffer *field = arg;
	MlnMessage message;
	int error;

	message.type = 0x10a2;
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

/* type 0x10b0: an action token */
__attribute__((noinline)) int MlnImpl_810231D4(MlnBuffer *arg, MlnBuffer **request)
{
	MlnBuffer *encoded = NULL;
	MlnBuffer *field = arg;
	MlnMessage message;
	int error;

	message.type = 0x10b0;
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

/* a token */
__attribute__((noinline)) int MlnImpl_810232FC(MlnApiReportData *arg, MlnBuffer **message)
{
	MlnBuffer *encoded = NULL;
	MlnBuffer *field = NULL;
	MlnMessage request;
	int error;

	request.type = MLN_MESSAGE_TOKEN;
	request.payload = &field;
	if (arg == NULL || message == NULL) {
		error = MLN_CORE_ERROR_INVALID;
	} else {
		field = arg->buffer;
		error = MlnImpl_81021640(&request, &encoded);
		if (error == MLN_OK) {
			*message = encoded;
			encoded = NULL;
		}
	}
	MlnBufferDestroy(1, encoded);
	return error;
}

/* a buffer list's buffers and the list */
void MlnImpl_8102337C(unsigned int count, MlnBuffer **buffers)
{
	unsigned int i;

	if (buffers == NULL)
		return;
	for (i = 0; i != count; i++)
		MlnBufferDestroy(1, buffers[i]);
	MlnFree(1, buffers);
}

/* a decoded message's fields; the message itself stays the caller's */
__attribute__((noinline)) void MlnImpl_810233C4(MlnMessage *message)
{
	MlnMessagePairs *pairs;
	MlnRightsSummary *rights;
	unsigned int i;
	void **fields;

	if (message == NULL || message->payload == NULL)
		return;
	fields = message->payload;
	switch (message->type) {
	case 0x400:
		MlnBufferDestroy(1, fields[1]);
		MlnBufferDestroy(1, fields[2]);
		MlnBufferDestroy(1, fields[3]);
		break;
	case MLN_MESSAGE_HTTP_POST:
		MlnFree(1, fields[1]);		/* the URL, a string */
		MlnBufferDestroy(1, fields[2]);
		MlnBufferDestroy(1, fields[3]);
		break;
	case MLN_MESSAGE_HTTP_GET:
		MlnFree(1, fields[0]);
		break;
	case MLN_MESSAGE_HTTP_BODY:
	case 0x500:
	case 0x601:
	case 0x10a0:
	case MLN_MESSAGE_HANDLE:
	case 0x10a2:
	case MLN_MESSAGE_FOUND:
	case 0x10b0:
	case MLN_MESSAGE_TOKEN:
		MlnBufferDestroy(1, fields[0]);
		break;
	case MLN_MESSAGE_VALUE:
		MlnBufferDestroy(1, ((MlnMessageValue *)message->payload)->data);
		break;
	case MLN_MESSAGE_RIGHTS:
		rights = message->payload;
		MlnImpl_8102337C(rights->idCount, rights->ids);
		break;
	case MLN_MESSAGE_LIST:
	case MLN_MESSAGE_PAIRS:
		pairs = message->payload;
		if (pairs->items == NULL)
			break;
		for (i = 0; i != pairs->count; i++) {
			if (pairs->items[i] != NULL) {
				MlnImpl_8102337C(pairs->items[i]->count, pairs->items[i]->buffers);
				MlnFree(1, pairs->items[i]);
			}
		}
		MlnFree(1, pairs->items);
		break;
	case 0x504:
	case 0x506:
	case 0x10a4:
		MlnBufferDestroy(1, fields[0]);
		MlnBufferDestroy(1, fields[1]);
		break;
	case MLN_MESSAGE_VERSION:
		MlnBufferDestroy(1, ((MlnApiVersion *)message->payload)->name);
		MlnBufferDestroy(1, ((MlnApiVersion *)message->payload)->id);
		break;
	case MLN_MESSAGE_RESULT:
		MlnImpl_81006D14(1, ((MlnMessageResult *)message->payload)->data);
		break;
	}
	MlnFree(1, message->payload);
}


/* a big-endian word and halfword of a received message */
static inline __attribute__((always_inline)) unsigned int MlnMessageGetWord(const unsigned char *p)
{
	return __builtin_bswap32(*(const unsigned int *)p);
}

static inline __attribute__((always_inline)) unsigned short MlnMessageGetHalf(const unsigned char *p)
{
	unsigned short half = *(const unsigned short *)p;

	return (unsigned short)(half << 8 | half >> 8);
}

/* a buffer entry, left bytes from it to the message's end: its size, then its offset */
static inline __attribute__((always_inline)) int MlnMessageGetBuffer(const unsigned char *p,
								      unsigned int left,
								      MlnBuffer **buffer)
{
	unsigned int size = MlnMessageGetWord(p), offset;

	*buffer = NULL;
	if (left <= 3)
		return MLN_MESSAGE_ERROR_FORMAT;
	offset = MlnMessageGetWord(p + 4);
	if (left - 4 <= 3 || size > left - 8)
		return MLN_MESSAGE_ERROR_FORMAT;
	if (size == 0)
		return MLN_OK;
	*buffer = MlnBufferCreate(1, size, p + 8 + offset);
	return *buffer != NULL ? MLN_OK : MLN_ERROR_NO_MEMORY;
}

/* a buffer given in place: its size, then its bytes */
static inline __attribute__((always_inline)) int MlnMessageGetInline(const unsigned char *p,
								      unsigned int left,
								      MlnBuffer **buffer)
{
	unsigned int size = MlnMessageGetWord(p);

	*buffer = NULL;
	if (left <= 3 || size > left - 4)
		return MLN_MESSAGE_ERROR_FORMAT;
	if (size == 0)
		return MLN_OK;
	*buffer = MlnBufferCreate(1, size, p + 4);
	return *buffer != NULL ? MLN_OK : MLN_ERROR_NO_MEMORY;
}

/*
 * A buffer group: its three buffers and its entry table, each entry three buffers. The bounds
 * are counted on from the message's, as if the group were where the value's entry is.
 */
static inline __attribute__((always_inline)) int MlnBufferGroupDecode(const unsigned char *p,
								       unsigned int left,
								       MlnBufferGroup **out)
{
	MlnBuffer *first = NULL, *second = NULL, *third = NULL, *table = NULL;
	MlnBuffer *entry = NULL, *buffer[3] = { NULL, NULL, NULL };
	MlnBuffer ***entries = NULL;
	MlnBufferGroup *group = NULL;
	const unsigned char *q;
	unsigned int count = 0, rest, i;
	int error;

	error = MlnMessageGetBuffer(p, left, &first);
	if (error != MLN_OK)
		goto out;
	error = MlnMessageGetBuffer(p + 8, left - 8, &second);
	if (error != MLN_OK)
		goto out;
	error = MlnMessageGetBuffer(p + 16, left - 16, &third);
	if (error != MLN_OK)
		goto out;
	error = MlnMessageGetBuffer(p + 24, left - 24, &table);
	if (error != MLN_OK)
		goto out;
	if (table != NULL) {
		q = table->data;
		count = MlnMessageGetWord(q);
		if (left - 32 <= 3 || 0xffffffffU / count <= 3) {
			error = MLN_MESSAGE_ERROR_FORMAT;
			goto out;
		}
		entries = MlnAllocate(1, count * sizeof(*entries));
		if (entries == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnMemset(entries, 0, count * sizeof(*entries));
		q += 4;
		rest = left - 36;
		for (i = 0; i != count; i++) {
			error = MlnMessageGetBuffer(q, rest, &entry);
			if (error != MLN_OK)
				goto out;
			if (entry == NULL) {
				error = MLN_CORE_ERROR_INVALID;
				goto out;
			}
			error = MlnMessageGetBuffer(entry->data, rest - 8, &buffer[0]);
			if (error != MLN_OK)
				goto out;
			error = MlnMessageGetBuffer(entry->data + 8, rest - 16, &buffer[1]);
			if (error != MLN_OK)
				goto out;
			error = MlnMessageGetBuffer(entry->data + 16, rest - 24, &buffer[2]);
			if (error != MLN_OK)
				goto out;
			entries[i] = MlnAllocate(1, 3 * sizeof(MlnBuffer *));
			if (entries[i] == NULL) {
				error = MLN_ERROR_NO_MEMORY;
				goto out;
			}
			MlnMemset(entries[i], 0, 3 * sizeof(MlnBuffer *));
			entries[i][0] = buffer[0];
			entries[i][1] = buffer[1];
			entries[i][2] = buffer[2];
			buffer[0] = buffer[1] = buffer[2] = NULL;
			MlnBufferDestroy(1, entry);
			entry = NULL;
			q += 8;
			rest -= 32;
		}
	}
	group = MlnAllocate(1, sizeof(*group));
	if (group == NULL) {
		error = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(group, 0, sizeof(*group));
	group->entries = entries;
	entries = NULL;
	group->third = third;
	group->first = first;
	group->second = second;
	group->entryCount = count;
	first = second = third = NULL;
	*out = group;
	group = NULL;
out:
	MlnBufferDestroy(1, first);
	MlnBufferDestroy(1, second);
	MlnBufferDestroy(1, third);
	MlnBufferDestroy(1, table);
	MlnBufferDestroy(1, entry);
	MlnBufferDestroy(1, buffer[0]);
	MlnBufferDestroy(1, buffer[1]);
	MlnBufferDestroy(1, buffer[2]);
	MlnImpl_81006D14(1, group);
	if (entries != NULL) {
		for (i = 0; i != count; i++) {
			if (entries[i] != NULL) {
				MlnBufferDestroy(1, entries[i][0]);
				MlnBufferDestroy(1, entries[i][1]);
				MlnBufferDestroy(1, entries[i][2]);
			}
		}
		MlnFree(1, entries);
	}
	return error;
}

/* a result: its status, then a value (a buffer group), the error, or nothing */
static inline __attribute__((always_inline)) int MlnMessageResultDecode(const unsigned char *p,
									 unsigned int left,
									 void **payload)
{
	MlnMessageResult *result;
	MlnBufferGroup *group = NULL;
	MlnBuffer *value;
	unsigned int status = MlnMessageGetWord(p);
	int error, code = 0;

	if (left <= 3) {
		value = NULL;
		error = MLN_MESSAGE_ERROR_FORMAT;
		goto out;
	}
	error = MlnMessageGetBuffer(p + 4, left - 4, &value);
	if (error != MLN_OK)
		goto out;
	if (value != NULL) {
		if (status == MLN_RESULT_VALUE) {
			error = MlnBufferGroupDecode(value->data, left - 12, &group);
			if (error != MLN_OK)
				goto out;
		} else if (status == MLN_RESULT_NONE) {
			code = MlnMessageGetWord(value->data);
		} else {
			error = 6;
			goto out;
		}
	}
	result = MlnAllocate(1, sizeof(*result));
	if (result == NULL) {
		error = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(result, 0, sizeof(*result));
	result->data = group;
	result->status = status;
	result->error = code;
	*payload = result;
	group = NULL;
out:
	MlnBufferDestroy(1, value);
	MlnImpl_81006D14(1, group);
	return error;
}

/* a rights summary; one with version 0 stays empty */
static inline __attribute__((always_inline)) int MlnRightsDecode(const unsigned char *p,
								  unsigned int left,
								  MlnRightsSummary *rights)
{
	MlnBuffer **ids = NULL, **list = NULL;
	unsigned int count = 0, size, i, rest;
	int notBefore, notAfter, notMoreThan, notLongerThan;
	unsigned short state, flags;
	const unsigned char *q;
	int error;

	if (left <= 3)
		return MLN_MESSAGE_ERROR_FORMAT;
	if (MlnMessageGetWord(p) == 0)
		return MLN_OK;
	if (left - 4 <= 3 || left - 8 <= 3)
		return MLN_MESSAGE_ERROR_FORMAT;
	state = MlnMessageGetHalf(p + 12);
	if (left - 12 <= 1)
		return MLN_MESSAGE_ERROR_FORMAT;
	flags = MlnMessageGetHalf(p + 14);
	if (left - 14 <= 1)
		return MLN_MESSAGE_ERROR_FORMAT;
	notBefore = MlnMessageGetWord(p + 16);
	if (left - 16 <= 3)
		return MLN_MESSAGE_ERROR_FORMAT;
	notAfter = MlnMessageGetWord(p + 20);
	if (left - 20 <= 3)
		return MLN_MESSAGE_ERROR_FORMAT;
	notMoreThan = MlnMessageGetWord(p + 24);
	if (left - 24 <= 3)
		return MLN_MESSAGE_ERROR_FORMAT;
	notLongerThan = MlnMessageGetWord(p + 28);
	if (left - 28 <= 3)
		return MLN_MESSAGE_ERROR_FORMAT;
	size = MlnMessageGetWord(p + 32);
	if (left - 32 <= 3)
		return MLN_MESSAGE_ERROR_FORMAT;
	if (size != 0) {
		count = MlnMessageGetWord(p + 36);
		if (left - 36 <= 3 || 0xffffffffU / count <= 3) {
			error = MLN_MESSAGE_ERROR_FORMAT;
			goto out;
		}
		ids = MlnAllocate(1, count * sizeof(*ids));
		if (ids == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnMemset(ids, 0, count * sizeof(*ids));
		q = p + 40;
		rest = left - 40;
		for (i = 0; i != count; i++) {
			error = MlnMessageGetBuffer(q, rest, &ids[i]);
			if (error != MLN_OK)
				goto out;
			q += 8;
			rest -= 8;
		}
		list = ids;
		ids = NULL;
out:
		MlnImpl_8102337C(count, ids);
		if (error != MLN_OK)
			return error;
	}
	rights->idCount = count;
	rights->ids = list;
	rights->state = state;
	rights->notBefore = (flags & 0x8000) != 0 ? notBefore : -1;
	rights->notAfter = (flags & 0x4000) != 0 ? notAfter : -1;
	rights->notMoreThan = (flags & 0x2000) != 0 ? notMoreThan : -1;
	rights->notLongerThan = (flags & 0x1000) != 0 ? notLongerThan : -1;
	return MLN_OK;
}

/*
 * A received message: its header (version 1, type, size, six zero bytes) and its fields, as the
 * type's payload. 84 for a message that does not parse, 6 for a result it cannot hold.
 */
__attribute__((noinline)) int MlnImpl_8102371C(const MlnBuffer *reply, MlnMessage *message)
{
	unsigned char zero[12];
	MlnMessage decoded;
	MlnBuffer *buffer[3] = { NULL, NULL, NULL };
	const unsigned char *data, *p;
	unsigned int size, left;
	unsigned int pair[2];
	unsigned short half[3];
	unsigned short *halves;
	MlnBuffer **fields;
	unsigned int *words;
	void *payload;
	int error;

	MlnMemset(&decoded, 0, sizeof(decoded));
	if (reply == NULL || message == NULL) {
		error = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	data = reply->data;
	size = reply->size;
	if (data == NULL || size <= 15 || data[10] != 0 || data[11] != 0 || data[12] != 0 ||
	    data[13] != 0 || data[14] != 0 || data[15] != 0 || size != MlnMessageGetWord(data + 6) ||
	    MlnMessageGetHalf(data) != 1) {
		error = MLN_MESSAGE_ERROR_FORMAT;
		goto out;
	}
	p = data + 16;
	left = size - 16;
	decoded.type = MlnMessageGetWord(data + 2);
	switch (decoded.type) {
	case 0x400:
	case MLN_MESSAGE_HTTP_POST:
		half[0] = MlnMessageGetHalf(p);
		if (left <= 1 || left - 2 <= 1) {
			error = MLN_MESSAGE_ERROR_FORMAT;
			goto three;
		}
		error = MlnMessageGetBuffer(p + 4, left - 4, &buffer[0]);
		if (error != MLN_OK)
			goto three;
		error = MlnMessageGetBuffer(p + 12, left - 12, &buffer[1]);
		if (error != MLN_OK)
			goto three;
		error = MlnMessageGetBuffer(p + 20, left - 20, &buffer[2]);
		if (error != MLN_OK)
			goto three;
		halves = MlnAllocate(1, sizeof(MlnMessagePost));
		if (halves == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto three;
		}
		MlnMemset(halves, 0, sizeof(MlnMessagePost));
		((MlnMessagePost *)halves)->method = half[0];
		((MlnMessagePost *)halves)->url = buffer[0];
		((MlnMessagePost *)halves)->body = buffer[1];
		((MlnMessagePost *)halves)->headers = buffer[2];
		decoded.payload = halves;
		buffer[0] = buffer[1] = buffer[2] = NULL;
three:
		MlnBufferDestroy(1, buffer[0]);
		MlnBufferDestroy(1, buffer[1]);
		MlnBufferDestroy(1, buffer[2]);
		break;
	case 0x401:
		half[0] = MlnMessageGetHalf(p);
		if (left <= 1) {
			error = MLN_MESSAGE_ERROR_FORMAT;
			goto out;
		}
		halves = MlnAllocate(1, sizeof(*halves));
		if (halves == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnMemset(halves, 0, sizeof(*halves));
		*halves = half[0];
		decoded.payload = halves;
		error = MLN_OK;
		break;
	case MLN_MESSAGE_HTTP_GET:
	case MLN_MESSAGE_HTTP_BODY:
	case 0x500:
	case 0x601:
	case 0x10b0:
	case MLN_MESSAGE_TOKEN:
		error = MlnMessageGetBuffer(p, left, &buffer[0]);
		if (error != MLN_OK)
			goto one;
		goto single;
	case MLN_MESSAGE_HANDLE:
	case 0x10a2:
	case MLN_MESSAGE_FOUND:
		error = MlnMessageGetInline(p, left, &buffer[0]);
		if (error != MLN_OK)
			goto one;
single:
		fields = MlnAllocate(1, sizeof(*fields));
		if (fields == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto one;
		}
		MlnMemset(fields, 0, sizeof(*fields));
		fields[0] = buffer[0];
		decoded.payload = fields;
		buffer[0] = NULL;
one:
		MlnBufferDestroy(1, buffer[0]);
		break;
	case MLN_MESSAGE_VALUE:
		half[0] = MlnMessageGetHalf(p);
		if (left <= 1 || left - 2 <= 1) {
			error = MLN_MESSAGE_ERROR_FORMAT;
			goto one;
		}
		error = MlnMessageGetBuffer(p + 4, left - 4, &buffer[0]);
		if (error != MLN_OK)
			goto one;
		halves = MlnAllocate(1, sizeof(MlnMessageValue));
		if (halves == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto one;
		}
		MlnMemset(halves, 0, sizeof(MlnMessageValue));
		((MlnMessageValue *)halves)->kind = half[0];
		((MlnMessageValue *)halves)->data = buffer[0];
		decoded.payload = halves;
		buffer[0] = NULL;
		goto one;
	case MLN_MESSAGE_STATUS:
		MlnMemset(zero, 0, sizeof(zero));
		if (size != 32 || MlnMemcmp(p + 4, zero, sizeof(zero)) != 0) {
			error = MLN_MESSAGE_ERROR_FORMAT;
			goto out;
		}
		payload = MlnAllocate(1, 4);
		if (payload == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnMemset(payload, 0, 4);
		MlnMemcpy(payload, p, 4);
		decoded.payload = payload;
		error = MLN_OK;
		break;
	case MLN_MESSAGE_RIGHTS:
		payload = MlnAllocate(1, sizeof(MlnRightsSummary));
		if (payload == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnMemset(payload, 0, sizeof(MlnRightsSummary));
		error = MlnRightsDecode(p, left, payload);
		if (error != MLN_OK) {
			MlnFree(1, payload);
			goto out;
		}
		decoded.payload = payload;
		break;
	case 0x502:
	case 0x603:
	case 0x10a6:
		error = MLN_OK;
		break;
	case MLN_MESSAGE_LIST:
	case MLN_MESSAGE_PAIRS:
		error = MlnImpl_8108150C(p, left, &decoded.payload);
		break;
	case 0x504:
	case 0x10a4:
		error = MlnMessageGetBuffer(p, left, &buffer[0]);
		if (error != MLN_OK)
			goto two;
		error = MlnMessageGetBuffer(p + 8, left - 8, &buffer[1]);
		if (error != MLN_OK)
			goto two;
		fields = MlnAllocate(1, 2 * sizeof(*fields));
		if (fields == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto two;
		}
		MlnMemset(fields, 0, 2 * sizeof(*fields));
		fields[0] = buffer[0];
		fields[1] = buffer[1];
		decoded.payload = fields;
		buffer[0] = buffer[1] = NULL;
two:
		MlnBufferDestroy(1, buffer[0]);
		MlnBufferDestroy(1, buffer[1]);
		break;
	case 0x506:
		error = MlnMessageGetBuffer(p, left, &buffer[0]);
		if (error != MLN_OK)
			goto pair;
		error = MlnMessageGetBuffer(p + 8, left - 8, &buffer[1]);
		if (error != MLN_OK)
			goto pair;
		fields = MlnAllocate(1, 2 * sizeof(*fields));
		if (fields == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto pair;
		}
		MlnMemset(fields, 0, 2 * sizeof(*fields));
		fields[0] = buffer[0];
		fields[1] = buffer[1];
		decoded.payload = fields;
		buffer[0] = buffer[1] = NULL;
pair:
		MlnBufferDestroy(1, buffer[1]);
		MlnBufferDestroy(1, buffer[0]);
		break;
	case MLN_MESSAGE_PAIR:
		pair[0] = MlnMessageGetWord(p);
		pair[1] = MlnMessageGetWord(p + 4);
		if (left <= 3 || left - 4 <= 3) {
			error = MLN_MESSAGE_ERROR_FORMAT;
			goto out;
		}
		words = MlnAllocate(1, 2 * sizeof(*words));
		if (words == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnMemset(words, 0, 2 * sizeof(*words));
		words[0] = pair[0];
		words[1] = pair[1];
		decoded.payload = words;
		error = MLN_OK;
		break;
	case MLN_MESSAGE_VERSION:
		half[0] = MlnMessageGetHalf(p);
		half[1] = MlnMessageGetHalf(p + 2);
		half[2] = MlnMessageGetHalf(p + 4);
		if (left <= 1 || left - 2 <= 1 || left - 4 <= 1 || left - 6 <= 1) {
			error = MLN_MESSAGE_ERROR_FORMAT;
			goto two;
		}
		error = MlnMessageGetBuffer(p + 8, left - 8, &buffer[0]);
		if (error != MLN_OK)
			goto two;
		error = MlnMessageGetBuffer(p + 16, left - 16, &buffer[1]);
		if (error != MLN_OK)
			goto two;
		halves = MlnAllocate(1, sizeof(MlnApiVersion));
		if (halves == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto two;
		}
		MlnMemset(halves, 0, sizeof(MlnApiVersion));
		((MlnApiVersion *)halves)->major = half[0];
		((MlnApiVersion *)halves)->minor = half[1];
		((MlnApiVersion *)halves)->build = half[2];
		((MlnApiVersion *)halves)->name = buffer[0];
		((MlnApiVersion *)halves)->id = buffer[1];
		decoded.payload = halves;
		buffer[0] = buffer[1] = NULL;
		goto two;
	case MLN_MESSAGE_RESULT:
		error = MlnMessageResultDecode(p, left, &decoded.payload);
		break;
	case 0x10a0:
		if (left <= 1) {
			error = MLN_MESSAGE_ERROR_FORMAT;
			goto out;
		}
		half[0] = MlnMessageGetHalf(p + 2);
		if (left - 2 <= 1) {
			error = MLN_MESSAGE_ERROR_FORMAT;
			goto out;
		}
		halves = MlnAllocate(1, 2 * sizeof(*halves));
		if (halves == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnMemset(halves, 0, 2 * sizeof(*halves));
		halves[0] = half[0];
		decoded.payload = halves;
		error = MLN_OK;
		break;
	case MLN_MESSAGE_STATE:
	case 0x10ae:
		pair[0] = MlnMessageGetWord(p);
		if (left <= 3) {
			error = MLN_MESSAGE_ERROR_FORMAT;
			goto out;
		}
		words = MlnAllocate(1, sizeof(*words));
		if (words == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnMemset(words, 0, sizeof(*words));
		*words = pair[0];
		decoded.payload = words;
		error = MLN_OK;
		break;
	default:
		error = MLN_MESSAGE_ERROR_FORMAT;
		goto out;
	}
	if (error == MLN_OK) {
		message->type = decoded.type;
		message->payload = decoded.payload;
		decoded.payload = NULL;
	}
out:
	MlnImpl_810233C4(&decoded);
	return error;
}
