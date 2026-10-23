/* PSF handle lifecycle; the source-family filename is inferred. */

#include "psf_internal.h"
#include <stdint.h>

const signed char gPsfSignature[4] = { 0, 'P', 'S', 'F' };
const signed char gPsfDefaultSignature[4] = { 0, 'P', 'S', 'F' };

int appmgrPsfParse(const void *data, SceSize size, void **object)
{
	const AppmgrPsfHeader *header = data;
	const unsigned char *key;
	AppmgrPsfContext *context;
	SceSize keyOffset, valueOffset, count, terminators = 0, index;
	int result;

	if (!data)
		return (int)0x80120003;
	if (appmgrPsfCompareMemory(data, gPsfSignature, sizeof(gPsfSignature)) != 0)
		return (int)0x80120004;
	if (header->version != 0x101)
		return (int)0x80120007;
	keyOffset = header->keyTableOffset;
	if (size < keyOffset)
		return (int)0x80120003;
	valueOffset = header->valueTableOffset;
	if (size < valueOffset)
		return (int)0x80120003;
	count = header->entryCount;
	if (size < count * sizeof(AppmgrPsfDiskEntry) + sizeof(*header) ||
	    keyOffset > valueOffset)
		return (int)0x80120003;
	key = (const unsigned char *)data + keyOffset;
	for (;;) {
		unsigned int character = *key++;

		if (character != 0) {
			if (character - '!' > 0x5d || character == '#')
				return (int)0x80120003;
		} else {
			++terminators;
		}
		if (terminators == count)
			break;
		if (size <= (SceSize)(key - (const unsigned char *)data))
			return (int)0x80120003;
	}
	if ((((uintptr_t)key + 3U) & ~3U) != (uintptr_t)data + valueOffset)
		return (int)0x80120003;
	result = appmgrPsfValidateEntries(data, sizeof(*header), header, size);
	if (result != 0)
		return result;
	if (!object)
		return (int)0x80120003;
	*object = NULL;
	context = appmgrPsfAllocMemory(sizeof(*context));
	if (!context)
		return (int)0x80120001;
	appmgrPsfSetMemory(context, 0, sizeof(*context));
	context->header = (AppmgrPsfHeader *)data;
	context->flags |= 6;
	result = appmgrPsfCompareMemory(data, gPsfSignature, sizeof(gPsfSignature));
	if (result != 0) {
		result = (int)0x80120004;
		goto release;
	}
	header = context->header;
	if (header->version != 0x101) {
		result = (int)0x80120007;
		goto release;
	}
	keyOffset = header->keyTableOffset;
	valueOffset = header->valueTableOffset;
	if (valueOffset < keyOffset) {
		result = (int)0x80120009;
		goto release;
	}
	count = header->entryCount;
	context->diskEntries = (AppmgrPsfDiskEntry *)((unsigned char *)data + sizeof(*header));
	context->storage.valueTable = (unsigned char *)header + valueOffset;
	context->keyTable = (unsigned char *)(context->diskEntries + count);
	if (count != 0 && !(context->flags & 2)) {
		AppmgrPsfWriteContext *mutable = (void *)context;
		AppmgrPsfEntry *entries = mutable->entries;
		unsigned char (*keys)[32] = mutable->keys;

		for (index = 0; index < count; ++index) {
			entries[index].key = keys[index];
			entries[index].diskEntry = &context->diskEntries[index];
		}
	}
	*object = context;
	return result;

release:
	index = context->flags & 2;
	if (index == 0 && context->storage.entryCapacity != 0) {
		AppmgrPsfWriteContext *mutable = (void *)context;

		do {
			AppmgrPsfEntry *entry = &mutable->entries[index++];

			appmgrPsfRemoveEntry(context, entry);
		} while (index < context->storage.entryCapacity);
	}
	appmgrPsfFreeMemory(context);
	return result;
}

int appmgrPsfDestroy(void *object)
{
	AppmgrPsfContext *context = object;
	SceSize index;

	if (!context)
		return (int)0x80120003;
	index = context->flags & 2;
	if (index == 0 && context->storage.entryCapacity != 0) {
		AppmgrPsfWriteContext *mutable = object;

		do {
			AppmgrPsfEntry *entry = &mutable->entries[index++];

			appmgrPsfRemoveEntry(context, entry);
		} while (context->storage.entryCapacity > index);
	}
	appmgrPsfFreeMemory(context);
	return 0;
}

int appmgrPsfCreate(void **object)
{
	AppmgrPsfWriteContext *psf;
	AppmgrPsfDiskEntry *disk;
	SceSize index;

	if (!object)
		return (int)0x80120003;
	psf = appmgrPsfAllocMemory(sizeof(*psf));
	if (!psf)
		return (int)0x80120001;
	appmgrPsfSetMemory(psf, 0, sizeof(*psf));
	psf->context.header = &psf->embeddedHeader;
	psf->context.storage.entryCapacity = 128;
	psf->entries = psf->entryStorage;
	disk = psf->diskStorage;
	psf->context.diskEntries = disk;
	for (index = 0; index < 128; ++index)
		disk[index].keyOffset = 0xffff;
	psf->keys = psf->keyStorage;
	psf->context.keyTable = (unsigned char *)UINTPTR_MAX;
	psf->context.flags |= 0x40;
	appmgrPsfInitializeHeader(psf, NULL, 1, 1);
	*object = psf;
	return 0;
}

int appmgrPsfGetEntryInfo(void *object, void *entryObject, const char **key,
	SceSize *length, SceSize *capacity)
{
	AppmgrPsfContext *context = object;
	AppmgrPsfEntry *entry = entryObject;
	AppmgrPsfDiskEntry *diskEntry = entryObject;

	if (context == NULL || entryObject == NULL)
		return (int)0x80120003;
	if ((context->flags & 2) != 0) {
		*key = (const char *)context->keyTable + diskEntry->keyOffset;
	} else {
		diskEntry = entry->diskEntry;
		if (diskEntry == NULL)
			return (int)0x80120003;
		*key = (const char *)entry->key;
	}
	*length = diskEntry->length;
	*capacity = diskEntry->capacity;
	return 0;
}

int appmgrPsfInitializeHeader(void *object, const void *magic,
	unsigned int majorVersion, unsigned int minorVersion)
{
	AppmgrPsfContext *context = object;
	AppmgrPsfHeader *header;

	if (!context)
		return (int)0x80120003;
	header = context->header;
	if (!header || (context->flags & 2))
		return (int)0x80120003;
	if (magic)
		appmgrPsfCopyMemory(header, magic, 4);
	else
		appmgrPsfCopyMemory(header, gPsfDefaultSignature, 4);
	context->header->version = (SceUInt8)minorVersion | (SceUInt16)(majorVersion << 8);
	return 0;
}
