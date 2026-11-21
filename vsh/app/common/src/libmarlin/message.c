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
	MlnBuffer **entry;
	unsigned int size, entries, i;

	if (group == NULL)
		return 0;
	size = MlnBufferLength(group->first) + 32 + MlnBufferLength(group->second) +
	       MlnBufferLength(group->third);
	if (group->entryCount == 0 || group->entries == NULL) {
		entries = 0;
	} else {
		entries = group->entryCount * 8 + 4;
		for (i = 0; group->entryCount > i; i++) {
			entry = group->entries[i];
			if (entry != NULL)
				entries += MlnBufferLength(entry[0]) + 24 + MlnBufferLength(entry[1]) +
					   MlnBufferLength(entry[2]);
		}
	}
	return size + entries;
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
		if (rights->idCount != 0) {
			size += rights->idCount * 8 + 4;
			if (rights->ids != NULL) {
				for (i = 0; i != rights->idCount; i++)
					size += MlnBufferLength(rights->ids[i]);
			}
		}
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
