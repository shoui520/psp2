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

__attribute__((noinline)) void MlnImpl_81026BFC(unsigned char *header, unsigned int command)
{
	MlnServicePutWord(header, command);
}

__attribute__((noinline)) void MlnImpl_81026C1C(unsigned char *header, unsigned int size)
{
	MlnServicePutWord(header + 4, size);
}

__attribute__((noinline)) void MlnImpl_81026C3C(unsigned char *header, unsigned int status)
{
	MlnServicePutWord(header + 8, status);
}

__attribute__((noinline)) void MlnImpl_81026C5C(unsigned char *header, unsigned int value)
{
	MlnServicePutWord(header + 12, value);
}

__attribute__((noinline)) void MlnImpl_81026C7C(unsigned char *header, unsigned int first)
{
	MlnServicePutWord(header, first);
}

__attribute__((noinline)) void MlnImpl_81026C9C(unsigned char *header, unsigned int second)
{
	MlnServicePutWord(header + 4, second);
}

__attribute__((noinline)) unsigned int MlnImpl_81026CBC(const unsigned char *header)
{
	return MLN_BE32(header);
}

__attribute__((noinline)) unsigned int MlnImpl_81026CE0(const unsigned char *header)
{
	return MLN_BE32(header + 4);
}

__attribute__((noinline)) int MlnImpl_81026D04(const unsigned char *header)
{
	return MLN_BE32(header + 8);
}

__attribute__((noinline)) unsigned int MlnImpl_81026D28(const unsigned char *header)
{
	return MLN_BE32(header + 12);
}

__attribute__((noinline)) unsigned int MlnImpl_81026D4C(const unsigned char *header)
{
	return MLN_BE32(header);
}

__attribute__((noinline)) unsigned int MlnImpl_81026D70(const unsigned char *header)
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

int MlnImpl_81026DE0(int heap, const MlnBuffer *response, MlnServiceValueVisitor visit,
		     void *context)
{
	if (response == NULL || response->size == 0 || response->data == NULL || visit == NULL)
		return 3;
	return MlnImpl_81027B50(heap, response, visit, context);
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

/* free the values of count arguments, then the arguments */
__attribute__((noinline)) void MlnImpl_81027230(int heap, unsigned int count, MlnServiceArg *items)
{
	unsigned int i;
	void *value;

	if (items == NULL)
		return;
	for (i = 0; i != count; i++) {
		value = items[i].value;
		if (value == NULL)
			continue;
		switch (items[i].type) {
		case MLN_SERVICE_ARG_HANDLE:
		case MLN_SERVICE_ARG_SIZE:
		case MLN_SERVICE_ARG_INTEGER:
		case MLN_SERVICE_ARG_ID:
		case MLN_SERVICE_ARG_BLOCK28:
		case MLN_SERVICE_ARG_OFFSET:
		case MLN_SERVICE_ARG_FLAG:
		case MLN_SERVICE_ARG_RECORD21:
			MlnFree(heap, value);
			break;
		case MLN_SERVICE_ARG_BUFFER:
			MlnBufferDestroy(heap, value);
			break;
		case MLN_SERVICE_ARG_OBJECT:
			MlnBufferDestroy(heap, ((MlnBuffer **)value)[1]);
			MlnFree(heap, value);
			break;
		case MLN_SERVICE_ARG_STRUCT:
			MlnImpl_8102913C(heap, value);
			break;
		case MLN_SERVICE_ARG_LIST:
			MlnImpl_81029234(heap, value);
			break;
		case MLN_SERVICE_ARG_GROUP:
			MlnImpl_810292C8(heap, value);
			break;
		case MLN_SERVICE_ARG_RECORD:
			MlnBufferDestroy(heap, *(MlnBuffer **)value);
			MlnFree(heap, value);
			break;
		default:
			break;
		}
	}
	MlnFree(heap, items);
}

void MlnImpl_81027390(int heap, MlnServiceReply *reply)
{
	if (reply == NULL)
		return;
	if (reply->items != NULL)
		MlnImpl_81027230(heap, reply->count, reply->items);
	MlnFree(heap, reply);
}

/* a request: the 16-byte header (command, size, value, 0), then each argument */
int MlnImpl_810273C4(int heap, int command, int value, const MlnServiceArgs *args,
		     MlnBuffer **request)
{
	MlnBuffer *buffer = NULL;
	unsigned int size = 0;
	MlnServiceWriter writer;
	unsigned char *header;
	int ret;

	MlnMemset(&writer, 0, sizeof(writer));
	if (request == NULL) {
		ret = 3;
		goto out;
	}
	if (args != NULL) {
		ret = MlnImpl_81028818(args, MlnImpl_81028880, &size);
		if (ret != 0)
			goto out;
	}
	if (size != 0) {
		writer.size = size;
		buffer = MlnBufferCreate(heap, size + 16, NULL);
		if (buffer == NULL) {
			ret = 2;
			goto out;
		}
		header = buffer->data;
		writer.data = header + 16;
		ret = MlnImpl_81028818(args, MlnImpl_810288D0, &writer);
		if (ret != 0)
			goto out;
		if (writer.written != size) {
			ret = 5;
			goto out;
		}
	} else {
		buffer = MlnBufferCreate(heap, 16, NULL);
		if (buffer == NULL) {
			ret = 2;
			goto out;
		}
		header = buffer->data;
	}
	MlnImpl_81026BFC(header, command);
	MlnImpl_81026C1C(header, size + 16);
	MlnImpl_81026C3C(header, value);
	MlnImpl_81026C5C(header, 0);
	*request = buffer;
	buffer = NULL;
	ret = 0;
out:
	MlnBufferDestroy(heap, buffer);
	return ret;
}

/* a reply: the response's values, counted, then read */
int MlnImpl_8102752C(int heap, const MlnBuffer *response, MlnServiceReply **reply)
{
	MlnServiceArg *items = NULL;
	unsigned int count = 0;
	MlnServiceReader reader;
	MlnServiceReply *result;
	int ret;

	if (response == NULL || response->size == 0 || response->data == NULL || reply == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81027B50(heap, response, MlnImpl_81027C00, &count);
	if (ret != 0)
		goto out;
	if (count != 0) {
		items = MlnAllocate(heap, count * sizeof(*items));
		if (items == NULL) {
			ret = 2;
			goto out;
		}
		MlnMemset(items, 0, count * sizeof(*items));
		reader.index = 0;
		reader.items = items;
		ret = MlnImpl_81027B50(heap, response, MlnImpl_81027C1C, &reader);
		if (ret != 0)
			goto out;
	}
	result = MlnAllocate(heap, sizeof(*result));
	if (result == NULL) {
		ret = 2;
		goto out;
	}
	result->items = items;
	items = NULL;
	result->count = count;
	*reply = result;
out:
	MlnImpl_81027230(heap, count, items);
	return ret;
}

/* a buffer's size on the wire: its length word and its bytes */
static inline unsigned int MlnServiceBufferSize(const MlnBuffer *buffer)
{
	return buffer != NULL ? buffer->size + 4 : 4;
}

/* an argument's size on the wire, without its 8-byte value header */
__attribute__((noinline)) int MlnImpl_8102766C(const MlnServiceArg *arg, unsigned int *size)
{
	const MlnServiceObject *object;
	const MlnServiceList *list;
	const MlnServiceListEntry *entry;
	const MlnBufferGroup *group;
	const MlnServiceRecordValue *record;
	MlnBuffer **triple;
	unsigned int n, i;

	if (size == NULL || arg == NULL)
		return 3;
	switch (arg->type) {
	case MLN_SERVICE_ARG_HANDLE:
	case MLN_SERVICE_ARG_SIZE:
	case MLN_SERVICE_ARG_INTEGER:
	case MLN_SERVICE_ARG_ID:
	case MLN_SERVICE_ARG_OFFSET:
	case MLN_SERVICE_ARG_FLAG:
		n = 4;
		break;
	case MLN_SERVICE_ARG_RECORD:
		record = arg->value;
		if (record == NULL)
			return 3;
		n = 4 + MlnServiceBufferSize(record->buffer);
		switch (record->kind) {
		case MLN_SERVICE_RECORD_PLAIN:
			break;
		case MLN_SERVICE_RECORD_LONG:
			n += 12;
			break;
		case MLN_SERVICE_RECORD_SHORT:
			n += 3;
			break;
		default:
			return 3;
		}
		break;
	case MLN_SERVICE_ARG_GROUP:
		group = arg->value;
		if (group == NULL)
			return 3;
		n = 8 + MlnServiceBufferSize(group->first) + MlnServiceBufferSize(group->second) +
		    MlnServiceBufferSize(group->third);
		if (group->entryCount == 0)
			break;
		if (group->entries == NULL)
			return 3;
		for (i = 0; i < group->entryCount; i++) {
			triple = &(*group->entries)[i * 3];
			n += MlnServiceBufferSize(triple[0]) + MlnServiceBufferSize(triple[1]) +
			     MlnServiceBufferSize(triple[2]);
		}
		break;
	case MLN_SERVICE_ARG_LIST:
		list = arg->value;
		n = 1;
		if (list == NULL || list->count == 0)
			break;
		if (list->entries == NULL)
			return 3;
		for (i = 0; i < list->count; i++) {
			entry = &(*list->entries)[i];
			n += MlnServiceBufferSize(entry->name) + 1 + MlnServiceBufferSize(entry->value);
		}
		break;
	case MLN_SERVICE_ARG_BLOCK28:
		n = 28;
		break;
	case MLN_SERVICE_ARG_BUFFER:
		n = MlnServiceBufferSize(arg->value);
		break;
	case MLN_SERVICE_ARG_RECORD21:
		n = 21;
		break;
	case MLN_SERVICE_ARG_OBJECT:
		object = arg->value;
		if (object == NULL)
			return 3;
		n = 8 + MlnServiceBufferSize(object->buffer);
		break;
	default:
		return 3;
	}
	*size = n;
	return 0;
}

/* take size bytes from the cursor */
int MlnImpl_810278C8(const unsigned char **cursor, unsigned int size, void *out,
		     unsigned int *remaining)
{
	if (cursor == NULL || out == NULL || *cursor == NULL || size > *remaining)
		return 3;
	*remaining -= size;
	MlnMemcpy(out, *cursor, size);
	*cursor += size;
	return 0;
}

/* put size bytes at the cursor */
static inline __attribute__((always_inline)) int MlnServiceWrite(unsigned char **cursor, const void *data, unsigned int size)
{
	if (cursor == NULL || *cursor == NULL || data == NULL)
		return 3;
	if (size != 0) {
		MlnMemcpy(*cursor, data, size);
		*cursor += size;
	}
	return 0;
}

/* put a buffer at the cursor: its length, then its bytes */
__attribute__((noinline)) int MlnImpl_81027934(unsigned char **cursor, const MlnBuffer *buffer)
{
	unsigned int length = MlnHtonl(0);

	if (buffer == NULL || buffer->data == NULL)
		return MlnServiceWrite(cursor, &length, sizeof(length));
	length = MlnHtonl(buffer->size);
	if (MlnServiceWrite(cursor, &length, sizeof(length)) != 0)
		return 3;
	return MlnServiceWrite(cursor, buffer->data, buffer->size);
}

/* take a buffer from the cursor: its length, then its bytes (none for length 0) */
__attribute__((noinline)) int MlnImpl_81027A1C(int heap, const unsigned char **cursor, MlnBuffer **out,
		     unsigned int *remaining)
{
	MlnBuffer *buffer = NULL;
	unsigned int length;
	int ret;

	if (out == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_810278C8(cursor, sizeof(length), &length, remaining);
	if (ret != 0)
		goto out;
	length = MlnHtonl(length);
	if (length == 0) {
		*out = NULL;
		goto out;
	}
	if (length > *remaining) {
		ret = 3;
		goto out;
	}
	buffer = MlnBufferCreate(heap, length, NULL);
	if (buffer == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_810278C8(cursor, length, buffer->data, remaining);
	if (ret != 0)
		goto out;
	*out = buffer;
	buffer = NULL;
out:
	MlnBufferDestroy(heap, buffer);
	return ret;
}

/* hand each of a response's values to visit */
__attribute__((noinline)) int MlnImpl_81027B50(int heap, const MlnBuffer *response,
					      MlnServiceValueVisitor visit,
		     void *context)
{
	const unsigned char *start, *value, *next;
	unsigned int size;
	int ret;

	if (response == NULL || response->size == 0 || response->data == NULL)
		return 3;
	if (visit == NULL || context == NULL)
		return 3;
	size = MlnImpl_81026CE0(response->data) - 16;
	start = response->data + 16;
	value = start;
	while ((unsigned int)(value - start) < size) {
		next = value + MlnImpl_81026D70(value) + 8;
		if ((unsigned int)(next - start) > size)
			return 3;
		ret = visit(heap, value, context);
		if (ret != 0)
			return ret;
		value = next;
	}
	return value == start + size ? 0 : 3;
}

/* count a value */
int MlnImpl_81027C00(int heap, const unsigned char *value, void *context)
{
	unsigned int *count = context;

	if (count == NULL)
		return 3;
	(*count)++;
	return 0;
}

/* read a value of the response into the next argument */
int MlnImpl_81027C1C(int heap, const unsigned char *value, void *context)
{
	MlnServiceReader *reader = context;
	MlnServiceArg *item;
	const unsigned char *cursor;
	unsigned int index, remaining, type, count, i;
	void *result = NULL;
	int ret = 0;

	if (reader == NULL || value == NULL)
		return 3;
	index = reader->index;
	item = &reader->items[index];
	if (item == NULL)
		return 3;
	reader->index = index + 1;
	cursor = value + 8;
	remaining = MlnImpl_81026D70(value);
	type = MlnImpl_81026D4C(value);
	switch (type) {
	case MLN_SERVICE_ARG_HANDLE:
	case MLN_SERVICE_ARG_INTEGER:
	case MLN_SERVICE_ARG_ID:
	case MLN_SERVICE_ARG_OFFSET:
	case MLN_SERVICE_ARG_FLAG: {
		unsigned int *word;

		if (remaining < 4)
			return 3;
		word = MlnAllocate(heap, sizeof(*word));
		result = word;
		if (word == NULL)
			return 2;
		/* Sony's leaks the word when the read fails */
		ret = MlnImpl_810278C8(&cursor, sizeof(*word), word, &remaining);
		if (ret != 0)
			return ret;
		*word = (*word >> 24) | ((*word >> 8) & 0xff00) | ((*word & 0xff00) << 8) |
			(*word << 24);
		break;
	}
	case MLN_SERVICE_ARG_SIZE: {
		unsigned int *word;

		if (remaining < 4)
			return 3;
		word = MlnAllocate(heap, sizeof(*word));
		result = word;
		if (word == NULL)
			return 2;
		/* the size is kept in the response's byte order */
		ret = MlnImpl_810278C8(&cursor, sizeof(*word), word, &remaining);
		if (ret != 0)
			return ret;
		break;
	}
	case MLN_SERVICE_ARG_BLOCK28: {
		MlnServiceBlock28 *block = NULL;
		unsigned int words[7];

		if (remaining < sizeof(*block)) {
			ret = 3;
			goto block_out;
		}
		block = MlnAllocate(heap, sizeof(*block));
		if (block == NULL) {
			ret = 2;
			goto block_out;
		}
		ret = MlnImpl_810278C8(&cursor, sizeof(words[0]), &words[0], &remaining);
		if (ret != 0)
			goto block_out;
		ret = MlnImpl_810278C8(&cursor, sizeof(words[1]), &words[1], &remaining);
		if (ret != 0)
			goto block_out;
		ret = MlnImpl_810278C8(&cursor, sizeof(words[2]), &words[2], &remaining);
		if (ret != 0)
			goto block_out;
		ret = MlnImpl_810278C8(&cursor, sizeof(words[3]), &words[3], &remaining);
		if (ret != 0)
			goto block_out;
		ret = MlnImpl_810278C8(&cursor, sizeof(words[4]), &words[4], &remaining);
		if (ret != 0)
			goto block_out;
		ret = MlnImpl_810278C8(&cursor, sizeof(words[5]), &words[5], &remaining);
		if (ret != 0)
			goto block_out;
		ret = MlnImpl_810278C8(&cursor, sizeof(words[6]), &words[6], &remaining);
		if (ret != 0)
			goto block_out;
		block->words[0] = MlnNtohl(words[0]);
		block->words[1] = MlnNtohl(words[1]);
		block->words[2] = MlnNtohl(words[2]);
		block->words[3] = MlnNtohl(words[3]);
		block->words[4] = MlnNtohl(words[4]);
		block->words[5] = MlnNtohl(words[5]);
		block->words[6] = MlnNtohl(words[6]);
		result = block;
		block = NULL;
block_out:
		MlnFree(heap, block);
		if (ret != 0)
			return ret;
		break;
	}
	case MLN_SERVICE_ARG_BUFFER:
		ret = MlnImpl_81027A1C(heap, &cursor, (MlnBuffer **)&result, &remaining);
		if (ret != 0)
			return ret;
		break;
	case MLN_SERVICE_ARG_RECORD21: {
		unsigned char *record = NULL;

		if (remaining < 4) {
			ret = 3;
			goto record21_out;
		}
		record = MlnAllocate(heap, sizeof(MlnServiceRecord));
		if (record == NULL) {
			ret = 2;
			goto record21_out;
		}
		MlnMemset(record, 0, sizeof(MlnServiceRecord));
		ret = MlnImpl_810278C8(&cursor, 1, record, &remaining);
		if (ret != 0)
			goto record21_out;
		ret = MlnImpl_810278C8(&cursor, sizeof(MlnServiceRecord) - 1, record + 1, &remaining);
		if (ret != 0)
			goto record21_out;
		result = record;
		record = NULL;
record21_out:
		MlnFree(heap, record);
		if (ret != 0)
			return ret;
		break;
	}
	case MLN_SERVICE_ARG_OBJECT: {
		MlnServiceObject *object = NULL;
		unsigned int first, second;

		if (remaining < 4) {
			ret = 3;
			goto object_out;
		}
		object = MlnAllocate(heap, sizeof(*object));
		if (object == NULL) {
			ret = 2;
			goto object_out;
		}
		MlnMemset(object, 0, sizeof(*object));
		ret = MlnImpl_810278C8(&cursor, sizeof(first), &first, &remaining);
		if (ret != 0)
			goto object_out;
		ret = MlnImpl_81027A1C(heap, &cursor, &object->buffer, &remaining);
		if (ret != 0)
			goto object_out;
		ret = MlnImpl_810278C8(&cursor, sizeof(second), &second, &remaining);
		if (ret != 0)
			goto object_out;
		object->first = MlnNtohl(first);
		object->second = MlnNtohl(second);
		result = object;
		object = NULL;
object_out:
		if (object != NULL)
			MlnBufferDestroy(heap, object->buffer);
		MlnFree(heap, object);
		if (ret != 0)
			return ret;
		break;
	}
	case MLN_SERVICE_ARG_LIST: {
		MlnServiceList *list = NULL;
		MlnServiceListEntry **entries;
		MlnServiceListEntry *entry;
		unsigned char number, flag;

		ret = MlnImpl_810278C8(&cursor, sizeof(number), &number, &remaining);
		if (ret != 0)
			goto list_out;
		if (number == 0)
			goto list_out;
		list = MlnAllocate(heap, sizeof(*list));
		if (list == NULL) {
			ret = 2;
			goto list_out;
		}
		MlnMemset(list, 0, sizeof(*list));
		list->count = number;
		if (number * sizeof(*entries) > remaining) {
			ret = 3;
			goto list_out;
		}
		entries = MlnAllocate(heap, number * sizeof(*entries));
		if (entries == NULL) {
			ret = 2;
			goto list_out;
		}
		MlnMemset(entries, 0, number * sizeof(*entries));
		list->entries = entries;
		for (i = 0; i < number; i++) {
			if (remaining < sizeof(*entry)) {
				ret = 3;
				goto list_out;
			}
			entry = MlnAllocate(heap, sizeof(*entry));
			entries[i] = entry;
			if (entry == NULL) {
				ret = 2;
				goto list_out;
			}
			MlnMemset(entry, 0, sizeof(*entry));
			ret = MlnImpl_81027A1C(heap, &cursor, &entry->name, &remaining);
			if (ret != 0)
				goto list_out;
			ret = MlnImpl_81027A1C(heap, &cursor, &entry->value, &remaining);
			if (ret != 0)
				goto list_out;
			ret = MlnImpl_810278C8(&cursor, sizeof(flag), &flag, &remaining);
			if (ret != 0)
				goto list_out;
			entries[i]->flag = flag;
		}
		result = list;
		list = NULL;
list_out:
		MlnImpl_81029234(heap, list);
		if (ret != 0)
			return ret;
		break;
	}
	case MLN_SERVICE_ARG_GROUP: {
		MlnBufferGroup *group = NULL;
		MlnBuffer ***entries;
		MlnBuffer **entry;
		unsigned int word;

		if (remaining < sizeof(*group)) {
			ret = 3;
			goto group_out;
		}
		group = MlnAllocate(heap, sizeof(*group));
		if (group == NULL) {
			ret = 2;
			goto group_out;
		}
		MlnMemset(group, 0, sizeof(*group));
		ret = MlnImpl_810278C8(&cursor, sizeof(word), &word, &remaining);
		if (ret != 0)
			goto group_out;
		group->tag = MlnHtonl(word);
		ret = MlnImpl_81027A1C(heap, &cursor, &group->first, &remaining);
		if (ret != 0)
			goto group_out;
		ret = MlnImpl_81027A1C(heap, &cursor, &group->second, &remaining);
		if (ret != 0)
			goto group_out;
		ret = MlnImpl_81027A1C(heap, &cursor, &group->third, &remaining);
		if (ret != 0)
			goto group_out;
		ret = MlnImpl_810278C8(&cursor, sizeof(count), &count, &remaining);
		if (ret != 0)
			goto group_out;
		count = MlnHtonl(count);
		group->entryCount = count;
		if (count != 0) {
			/* Sony's bounds the count by 24-byte entries, then takes 4 for each */
			if (count > 0xffffffff / 24 || count * sizeof(*entries) > remaining) {
				ret = 3;
				goto group_out;
			}
			entries = MlnAllocate(heap, count * sizeof(*entries));
			if (entries == NULL) {
				ret = 2;
				goto group_out;
			}
			MlnMemset(entries, 0, count * sizeof(*entries));
			group->entries = entries;
			for (i = 0; i < count; i++) {
				if (remaining < 3 * sizeof(*entry)) {
					ret = 3;
					goto group_out;
				}
				entry = MlnAllocate(heap, 3 * sizeof(*entry));
				entries[i] = entry;
				if (entry == NULL) {
					ret = 2;
					goto group_out;
				}
				MlnMemset(entry, 0, 3 * sizeof(*entry));
				ret = MlnImpl_81027A1C(heap, &cursor, &entries[i][0], &remaining);
				if (ret != 0)
					goto group_out;
				ret = MlnImpl_81027A1C(heap, &cursor, &entries[i][1], &remaining);
				if (ret != 0)
					goto group_out;
				ret = MlnImpl_81027A1C(heap, &cursor, &entries[i][2], &remaining);
				if (ret != 0)
					goto group_out;
			}
		}
		result = group;
		group = NULL;
group_out:
		MlnImpl_810292C8(heap, group);
		if (ret != 0)
			return ret;
		break;
	}
	case MLN_SERVICE_ARG_RECORD: {
		MlnServiceRecordValue *record;
		unsigned int kind;
		unsigned char byte;

		record = MlnAllocate(heap, sizeof(*record));
		if (record == NULL) {
			ret = 2;
			goto record_out;
		}
		MlnMemset(record, 0, sizeof(*record));
		ret = MlnImpl_81027A1C(heap, &cursor, &record->buffer, &remaining);
		if (ret != 0)
			goto record_out;
		ret = MlnImpl_810278C8(&cursor, sizeof(kind), &kind, &remaining);
		if (ret != 0)
			goto record_out;
		kind = MlnHtonl(kind);
		switch (kind) {
		case MLN_SERVICE_RECORD_PLAIN:
			MlnMemset(record->head, 0, sizeof(record->head) + sizeof(record->tail));
			break;
		case MLN_SERVICE_RECORD_LONG:
			ret = MlnImpl_810278C8(&cursor, sizeof(byte), &byte, &remaining);
			if (ret != 0)
				goto record_out;
			record->head[0] = byte;
			ret = MlnImpl_810278C8(&cursor, sizeof(byte), &byte, &remaining);
			if (ret != 0)
				goto record_out;
			record->head[1] = byte;
			ret = MlnImpl_810278C8(&cursor, sizeof(byte), &byte, &remaining);
			if (ret != 0)
				goto record_out;
			record->head[2] = byte;
			ret = MlnImpl_810278C8(&cursor, sizeof(byte), &byte, &remaining);
			if (ret != 0)
				goto record_out;
			record->head[3] = byte;
			ret = MlnImpl_810278C8(&cursor, sizeof(record->tail), record->tail, &remaining);
			if (ret != 0)
				goto record_out;
			break;
		case MLN_SERVICE_RECORD_SHORT:
			ret = MlnImpl_810278C8(&cursor, sizeof(byte), &byte, &remaining);
			if (ret != 0)
				goto record_out;
			record->head[0] = byte;
			ret = MlnImpl_810278C8(&cursor, sizeof(byte), &byte, &remaining);
			if (ret != 0)
				goto record_out;
			record->head[1] = byte;
			ret = MlnImpl_810278C8(&cursor, sizeof(byte), &byte, &remaining);
			if (ret != 0)
				goto record_out;
			record->head[2] = byte;
			break;
		default:
			ret = 3;
			goto record_out;
		}
		record->kind = kind;
		result = record;
		record = NULL;
record_out:
		if (record != NULL)
			MlnBufferDestroy(heap, record->buffer);
		MlnFree(heap, record);
		if (ret != 0)
			return ret;
		break;
	}
	default:
		return 3;
	}
	item->type = type;
	item->value = result;
	return remaining == 0 ? 0 : 3;
}

/* hand each argument to visit */
__attribute__((noinline)) int MlnImpl_81028818(const MlnServiceArgs *args, MlnServiceArgVisitor visit, void *context)
{
	unsigned int i;
	int ret;

	if (visit == NULL || args == NULL)
		return 3;
	for (i = 0; i < args->count; i++) {
		ret = visit(&args->items[i], context);
		if (ret != 0)
			return ret;
	}
	return 0;
}

/* add an argument's size, with its value header, to the total */
int MlnImpl_81028880(const MlnServiceArg *arg, void *context)
{
	unsigned int *total = context;
	unsigned int size = 0;
	int ret;

	if (total == NULL || arg == NULL)
		return 3;
	ret = MlnImpl_8102766C(arg, &size);
	if (ret == 0)
		*total += size + 8;
	return ret;
}

/* write an argument, its value header and its data, into the request */
int MlnImpl_810288D0(const MlnServiceArg *arg, void *context)
{
	MlnServiceWriter *writer = context;
	const MlnBuffer *buffer;
	const MlnServiceObject *object;
	const MlnServiceList *list;
	const MlnServiceListEntry *entry;
	const MlnBufferGroup *group;
	const MlnServiceRecordValue *record;
	const unsigned char *bytes;
	unsigned char *data, *header, *cursor;
	unsigned int written, size = 0, words[7], word, count, i;
	unsigned char none;
	int ret;

	if (writer == NULL || arg == NULL)
		return 3;
	data = writer->data;
	written = writer->written;
	ret = MlnImpl_8102766C(arg, &size);
	if (ret != 0)
		return ret;
	if (writer->written > 0xffffffff - 8)
		return 3;
	if (size > 0xffffffff - (writer->written + 8))
		return 3;
	if (writer->written + size + 8 > writer->size)
		return 5;
	header = data + written;
	if (header == NULL)
		return 3;
	cursor = header + 8;
	if (arg->value == NULL && arg->type != MLN_SERVICE_ARG_LIST &&
	    arg->type != MLN_SERVICE_ARG_BUFFER)
		return 3;
	MlnImpl_81026C7C(header, arg->type);
	MlnImpl_81026C9C(header, size);
	switch (arg->type) {
	case MLN_SERVICE_ARG_HANDLE:
	case MLN_SERVICE_ARG_INTEGER:
	case MLN_SERVICE_ARG_ID:
	case MLN_SERVICE_ARG_OFFSET:
	case MLN_SERVICE_ARG_FLAG:
		word = MlnNtohl(*(const unsigned int *)arg->value);
		ret = MlnServiceWrite(&cursor, &word, sizeof(word));
		break;
	case MLN_SERVICE_ARG_SIZE:
		/* the size goes in the caller's byte order */
		ret = MlnServiceWrite(&cursor, arg->value, sizeof(unsigned int));
		break;
	case MLN_SERVICE_ARG_BLOCK28:
		if (arg->value == NULL)
			return 3;
		words[0] = MlnNtohl(((const MlnServiceBlock28 *)arg->value)->words[0]);
		words[1] = MlnNtohl(((const MlnServiceBlock28 *)arg->value)->words[1]);
		words[2] = MlnNtohl(((const MlnServiceBlock28 *)arg->value)->words[2]);
		words[3] = MlnNtohl(((const MlnServiceBlock28 *)arg->value)->words[3]);
		words[4] = MlnNtohl(((const MlnServiceBlock28 *)arg->value)->words[4]);
		words[5] = MlnNtohl(((const MlnServiceBlock28 *)arg->value)->words[5]);
		words[6] = MlnNtohl(((const MlnServiceBlock28 *)arg->value)->words[6]);
		ret = MlnServiceWrite(&cursor, &words[0], sizeof(words[0]));
		if (ret != 0)
			return ret;
		ret = MlnServiceWrite(&cursor, &words[1], sizeof(words[1]));
		if (ret != 0)
			return ret;
		ret = MlnServiceWrite(&cursor, &words[2], sizeof(words[2]));
		if (ret != 0)
			return ret;
		ret = MlnServiceWrite(&cursor, &words[3], sizeof(words[3]));
		if (ret != 0)
			return ret;
		ret = MlnServiceWrite(&cursor, &words[4], sizeof(words[4]));
		if (ret != 0)
			return ret;
		ret = MlnServiceWrite(&cursor, &words[5], sizeof(words[5]));
		if (ret != 0)
			return ret;
		ret = MlnServiceWrite(&cursor, &words[6], sizeof(words[6]));
		break;
	case MLN_SERVICE_ARG_BUFFER:
		buffer = arg->value;
		if (buffer != NULL && buffer->size != 0 && buffer->data == NULL)
			return 3;
		ret = MlnImpl_81027934(&cursor, buffer);
		break;
	case MLN_SERVICE_ARG_RECORD21:
		bytes = arg->value;
		if (bytes == NULL)
			return 3;
		ret = MlnServiceWrite(&cursor, bytes, 1);
		if (ret != 0)
			return ret;
		ret = MlnServiceWrite(&cursor, bytes + 1, sizeof(MlnServiceRecord) - 1);
		break;
	case MLN_SERVICE_ARG_OBJECT:
		object = arg->value;
		if (object == NULL)
			return 3;
		words[0] = MlnNtohl(object->first);
		words[1] = MlnNtohl(object->second);
		ret = MlnServiceWrite(&cursor, &words[0], sizeof(words[0]));
		if (ret != 0)
			return ret;
		ret = MlnImpl_81027934(&cursor, object->buffer);
		if (ret != 0)
			return ret;
		ret = MlnServiceWrite(&cursor, &words[1], sizeof(words[1]));
		break;
	case MLN_SERVICE_ARG_LIST:
		list = arg->value;
		none = 0;
		if (list == NULL) {
			ret = MlnServiceWrite(&cursor, &none, sizeof(none));
			break;
		}
		if (list->count != 0 && list->entries == NULL)
			return 3;
		ret = MlnServiceWrite(&cursor, &list->count, sizeof(list->count));
		if (ret != 0)
			return ret;
		for (i = 0; i < list->count; i++) {
			entry = &(*list->entries)[i];
			if (entry == NULL)
				return 3;
			ret = MlnImpl_81027934(&cursor, entry->name);
			if (ret != 0)
				return ret;
			ret = MlnImpl_81027934(&cursor, entry->value);
			if (ret != 0)
				return ret;
			ret = MlnServiceWrite(&cursor, &entry->flag, sizeof(entry->flag));
			if (ret != 0)
				return ret;
		}
		break;
	case MLN_SERVICE_ARG_GROUP:
		group = arg->value;
		if (group == NULL || (group->entryCount != 0 && group->entries == NULL))
			return 3;
		words[0] = MlnHtonl(group->tag);
		count = MlnHtonl(group->entryCount);
		ret = MlnServiceWrite(&cursor, &words[0], sizeof(words[0]));
		if (ret != 0)
			return ret;
		ret = MlnImpl_81027934(&cursor, group->first);
		if (ret != 0)
			return ret;
		ret = MlnImpl_81027934(&cursor, group->second);
		if (ret != 0)
			return ret;
		ret = MlnImpl_81027934(&cursor, group->third);
		if (ret != 0)
			return ret;
		ret = MlnServiceWrite(&cursor, &count, sizeof(count));
		if (ret != 0)
			return ret;
		for (i = 0; i < group->entryCount; i++) {
			ret = MlnImpl_81027934(&cursor, (*group->entries)[i * 3 + 0]);
			if (ret != 0)
				return ret;
			ret = MlnImpl_81027934(&cursor, (*group->entries)[i * 3 + 1]);
			if (ret != 0)
				return ret;
			ret = MlnImpl_81027934(&cursor, (*group->entries)[i * 3 + 2]);
			if (ret != 0)
				return ret;
		}
		break;
	case MLN_SERVICE_ARG_RECORD:
		record = arg->value;
		if (record == NULL)
			return 3;
		ret = MlnImpl_81027934(&cursor, record->buffer);
		if (ret != 0)
			return ret;
		word = MlnHtonl(record->kind);
		ret = MlnServiceWrite(&cursor, &word, sizeof(word));
		if (ret != 0)
			return ret;
		switch (record->kind) {
		case MLN_SERVICE_RECORD_PLAIN:
			break;
		case MLN_SERVICE_RECORD_LONG:
			ret = MlnServiceWrite(&cursor, &record->head[0], 1);
			if (ret != 0)
				return ret;
			ret = MlnServiceWrite(&cursor, &record->head[1], 1);
			if (ret != 0)
				return ret;
			ret = MlnServiceWrite(&cursor, &record->head[2], 1);
			if (ret != 0)
				return ret;
			ret = MlnServiceWrite(&cursor, &record->head[3], 1);
			if (ret != 0)
				return ret;
			ret = MlnServiceWrite(&cursor, record->tail, sizeof(record->tail));
			break;
		case MLN_SERVICE_RECORD_SHORT:
			ret = MlnServiceWrite(&cursor, &record->head[0], 1);
			if (ret != 0)
				return ret;
			ret = MlnServiceWrite(&cursor, &record->head[1], 1);
			if (ret != 0)
				return ret;
			ret = MlnServiceWrite(&cursor, &record->head[2], 1);
			break;
		default:
			return 3;
		}
		break;
	default:
		return 3;
	}
	if (ret != 0)
		return ret;
	writer->written += size + 8;
	return 0;
}

__attribute__((noinline)) void MlnImpl_8102913C(int heap, MlnServiceStruct *value)
{
	MlnServiceStructEntry *entry;
	unsigned int i, j;

	if (value == NULL)
		return;
	if (value->entries != NULL) {
		for (i = 0; i < value->count; i++) {
			if (value->entries[i] == NULL)
				continue;
			MlnBufferDestroy(heap, value->entries[i]->name);
			entry = value->entries[i];
			if (entry->pairs != NULL && entry->pairCount != 0) {
				for (j = 0; j < value->entries[i]->pairCount; j++) {
					MlnBufferDestroy(heap, value->entries[i]->pairs[j].first);
					MlnBufferDestroy(heap, value->entries[i]->pairs[j].second);
				}
			}
			MlnFree(heap, value->entries[i]->pairs);
			MlnFree(heap, value->entries[i]);
		}
		MlnFree(heap, value->entries);
	}
	MlnFree(heap, value);
}

__attribute__((noinline)) void MlnImpl_81029234(int heap, MlnServiceList *list)
{
	unsigned int i;

	if (list == NULL)
		return;
	if (list->entries != NULL) {
		for (i = 0; i < list->count; i++) {
			if (list->entries[i] == NULL)
				continue;
			MlnBufferDestroy(heap, list->entries[i]->name);
			MlnBufferDestroy(heap, list->entries[i]->value);
			MlnFree(heap, list->entries[i]);
		}
		MlnFree(heap, list->entries);
	}
	MlnFree(heap, list);
}

__attribute__((noinline)) void MlnImpl_810292C8(int heap, MlnBufferGroup *group)
{
	unsigned int i;

	if (group == NULL)
		return;
	MlnBufferDestroy(heap, group->first);
	MlnBufferDestroy(heap, group->second);
	MlnBufferDestroy(heap, group->third);
	if (group->entries != NULL) {
		for (i = 0; i < group->entryCount; i++) {
			if (group->entries[i] == NULL)
				continue;
			MlnBufferDestroy(heap, group->entries[i][0]);
			MlnBufferDestroy(heap, group->entries[i][1]);
			MlnBufferDestroy(heap, group->entries[i][2]);
			MlnFree(heap, group->entries[i]);
		}
		MlnFree(heap, group->entries);
	}
	MlnFree(heap, group);
}
