/* PSF serialization and buffer I/O; the source-family filename is inferred. */

#include "psf_internal.h"
#include <stdint.h>

#define WRITE_PADDING(writer, buffer, size) do { \
	int paddingSize = (size); \
	if (paddingSize > 0) { \
		int blocks = paddingSize >> 5; \
		int tail = paddingSize & 31; \
		appmgrPsfSetMemory((buffer), 0, 32); \
		while (blocks != 0) { \
			--blocks; \
			if ((writer)->write((writer)->argument, (buffer), 32) != 32) \
				return (int)0x80120002; \
		} \
		if (tail != 0 && \
		    (writer)->write((writer)->argument, (buffer), tail) != tail) \
			return (int)0x80120002; \
	} \
} while (0)

int appmgrPsfBufferWrite(void *argument, const void *data, SceSize size)
{
	AppmgrPsfBufferWriter *writer = argument;
	SceSize copied, remaining;

	if (writer->remaining == 0)
		return -1;
	copied = size < writer->remaining ? size : writer->remaining;
	appmgrPsfCopyMemory(writer->buffer + writer->offset, data, copied);
	remaining = writer->remaining - copied;
	writer->offset += copied;
	writer->remaining = remaining;
	return copied;
}

int appmgrPsfSerialize(AppmgrPsfWriter *writer, AppmgrPsfWriteContext *psf)
{
	SceSize index, count, keyBytes = 0, paddedKeyBytes = 0, valueOffset = 0;
	unsigned char padding[32], bigEndian[4];
	uint32_t integer;
	AppmgrPsfHeader *header;
	AppmgrPsfWriter headerWriter;

	if (!psf || (psf->context.flags & 2))
		return (int)0x80120003;
	appmgrPsfSortEntries(psf);
	count = psf->context.header->entryCount;
	if (count != 0) {
		index = 0;
		do {
			AppmgrPsfEntry *entry = &psf->entries[index];
			AppmgrPsfDiskEntry *disk = entry->diskEntry;
			SceSize capacity = (disk->capacity + 3) & ~3U;

			disk->valueOffset = valueOffset;
			disk->keyOffset = keyBytes;
			disk->capacity = capacity;
			valueOffset += capacity;
			++index;
			if (index < count) {
				unsigned int alignment = (unsigned char)entry[1].diskEntry->format;

				valueOffset = alignment * ((valueOffset + alignment - 1) / alignment);
			} else {
				valueOffset = (valueOffset + 3) & ~3U;
			}
			keyBytes += appmgrPsfStringLength((const signed char *)entry->key) + 1;
			count = psf->context.header->entryCount;
		} while (index < count);
		paddedKeyBytes = (keyBytes + 3) & ~3U;
	}
	appmgrPsfSetMemory(padding, 0, sizeof(padding));
	header = psf->context.header;
	headerWriter = *writer;
	header->keyTableOffset = sizeof(*header) + header->entryCount * sizeof(AppmgrPsfDiskEntry);
	header->valueTableOffset = paddedKeyBytes + header->keyTableOffset;
	if (headerWriter.write(headerWriter.argument, header, sizeof(*header)) != sizeof(*header))
		return (int)0x80120002;
	for (index = 0; index < psf->context.header->entryCount; ++index) {
		if (writer->write(writer->argument, psf->entries[index].diskEntry,
				  sizeof(AppmgrPsfDiskEntry)) != sizeof(AppmgrPsfDiskEntry))
			return (int)0x80120002;
	}
	for (index = 0; index < psf->context.header->entryCount; ++index) {
		AppmgrPsfEntry *entry = &psf->entries[index];
		SceSize keySize = appmgrPsfStringLength((const signed char *)entry->key) + 1;

		if (writer->write(writer->argument, entry->key, keySize) != keySize)
			return (int)0x80120002;
	}
	WRITE_PADDING(writer, padding, paddedKeyBytes - keyBytes);
	valueOffset = 0;
	for (index = 0; index < psf->context.header->entryCount; ++index) {
		AppmgrPsfEntry *entries = psf->entries;
		AppmgrPsfEntry *entry = &entries[index];
		AppmgrPsfDiskEntry *disk = entry->diskEntry;
		SceSize size;
		int result;

		if (disk->valueOffset > valueOffset) {
			WRITE_PADDING(writer, padding, disk->valueOffset - valueOffset);
			disk = entries[index].diskEntry;
			valueOffset = disk->valueOffset;
		}
		size = disk->length;
		if (entry->value) {
			if (disk->format == 0x404) {
				AppmgrPsfWriter integerWriter = *writer;

				integer = *(uint32_t *)entry->value;
				result = integerWriter.write(integerWriter.argument, &integer, size);
			} else if (disk->format == 0x504) {
				AppmgrPsfWriter integerWriter = *writer;
				const unsigned char *bytes = (const unsigned char *)&integer;

				integer = *(uint32_t *)entry->value;
				bigEndian[0] = bytes[3];
				bigEndian[1] = bytes[2];
				bigEndian[2] = bytes[1];
				bigEndian[3] = bytes[0];
				result = integerWriter.write(integerWriter.argument, bigEndian, size);
			} else {
				result = writer->write(writer->argument, entry->value, size);
			}
			if (result != size)
				return (int)0x80120002;
		} else {
			size = 0;
		}
		valueOffset += size;
	}
	if (psf->context.header->entryCount != 0) {
		AppmgrPsfDiskEntry *last = psf->entries[psf->context.header->entryCount - 1].diskEntry;
		SceSize end = (last->capacity + last->valueOffset + 3) & ~3U;

		if (end > valueOffset)
			WRITE_PADDING(writer, padding, end - valueOffset);
	}
	return 0;
}

int appmgrPsfWrite(void *buffer, SceSize capacity, void *psf)
{
	AppmgrPsfWriter writer;
	AppmgrPsfBufferWriter output;

	output.buffer = buffer;
	output.remaining = capacity;
	writer.write = appmgrPsfBufferWrite;
	writer.argument = &output;
	output.offset = 0;
	return appmgrPsfSerialize(&writer, psf);
}

int appmgrPsfGetSize(void *object, SceSize *size)
{
	AppmgrPsfWriteContext *psf = object;
	SceSize valueOffset = 0, keyBytes = 0, keyWords = 0, index, count;
	AppmgrPsfDiskEntry *last;

	if (!psf)
		return (int)0x80120003;
	if (psf->context.flags & 2)
		return (int)0x80120003;
	appmgrPsfSortEntries(psf);
	count = psf->context.header->entryCount;
	if (count != 0) {
		index = 0;
		do {
			AppmgrPsfEntry *entry = &psf->entries[index];
			AppmgrPsfDiskEntry *disk = entry->diskEntry;
			SceSize capacity = (disk->capacity + 3) & ~3U;
			SceSize end;

			disk->valueOffset = valueOffset;
			disk->keyOffset = keyBytes;
			disk->capacity = capacity;
			end = capacity + valueOffset;
			valueOffset = (end + 3) & ~3U;
			++index;
			if (index < count) {
				unsigned int alignment = (unsigned char)entry[1].diskEntry->format;

				valueOffset = ((end + alignment - 1) / alignment) * alignment;
			}
			keyBytes += appmgrPsfStringLength((const signed char *)entry->key) + 1;
			count = psf->context.header->entryCount;
		} while (index < count);
		keyWords = (keyBytes + 3) >> 2;
	}
	last = (psf->entries + count)[-1].diskEntry;
	*size = count * sizeof(AppmgrPsfDiskEntry) + sizeof(AppmgrPsfHeader) +
		4 * (keyWords + ((last->valueOffset + last->capacity + 3) >> 2));
	return 0;
}
