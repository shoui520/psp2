/* PSF entry management, parsing and serialization. */

#include "psf_internal.h"

int appmgrPsfCompareEntries(const void *leftObject, const void *rightObject)
{
	const AppmgrPsfEntry *left = leftObject;
	const AppmgrPsfEntry *right = rightObject;
	const unsigned char *leftKey = left->key;
	const unsigned char *rightKey = right->key;

	if (leftKey == NULL)
		return 1;
	if (rightKey != NULL)
		return appmgrPsfCompareString(leftKey, rightKey);
	return -1;
}

int appmgrPsfCompareDiskKey(const void *keyObject, const void *entryObject)
{
	const AppmgrPsfSearchKey *search = keyObject;
	const AppmgrPsfDiskEntry *entry = entryObject;

	return appmgrPsfCompareString(search->key, search->keyTable + entry->keyOffset);
}

int appmgrPsfCompareEntryKey(const void *keyObject, const void *entryObject)
{
	const unsigned char *key = keyObject;
	const AppmgrPsfEntry *entry = entryObject;

	return appmgrPsfCompareString(key, entry->key);
}

void appmgrPsfSortEntries(void *object)
{
	AppmgrPsfContext *context = object;
	AppmgrPsfWriteContext *writeContext = object;
	SceUInt32 flags;
	AppmgrPsfHeader *header;
	SceSize count;

	if ((context->flags & 6) == 0) {
		appmgrPsfShellSort(writeContext->entries, writeContext->entryHighWater,
			sizeof(AppmgrPsfEntry), appmgrPsfCompareEntries);
		flags = context->flags;
		header = context->header;
		count = header->entryCount;
		context->flags = flags | 4;
		writeContext->entryHighWater = count;
	}
}

int appmgrPsfSetEntryLength(void *object, void *entryObject, SceSize length)
{
	AppmgrPsfContext *context = object;
	AppmgrPsfEntry *entry = entryObject;
	AppmgrPsfDiskEntry *diskEntry = entryObject;

	if (context == NULL)
		return (int)0x80120003;
	if (entryObject == NULL)
		return (int)0x80120003;
	if ((context->flags & 2) != 0) {
		diskEntry->length = length;
		return 0;
	}
	if (entry->diskEntry == NULL)
		return (int)0x80120003;
	entry->diskEntry->length = length;
	return 0;
}

int appmgrPsfGetEntryData(void *object, void *entryObject, void **data)
{
	AppmgrPsfContext *context = object;
	AppmgrPsfEntry *entry = entryObject;
	AppmgrPsfDiskEntry *diskEntry = entryObject;
	SceUInt32 flags;
	void *value;

	if (context == NULL)
		return (int)0x80120003;
	if (entryObject == NULL)
		return (int)0x80120003;
	flags = context->flags;
	*data = NULL;
	if ((flags & 2) != 0) {
		*data = context->storage.valueTable + diskEntry->valueOffset;
		return 0;
	}
	value = entry->value;
	if (value == NULL)
		return (int)0x80120003;
	*data = value;
	return 0;
}

int appmgrPsfFindEntry(void *object, const char *key, void **entry)
{
	AppmgrPsfContext *context = object;
	AppmgrPsfWriteContext *writeContext = object;
	AppmgrPsfSearchKey search;
	AppmgrPsfHeader *header;
	SceUInt32 flags;
	SceSize count;
	void *found;

	*entry = NULL;
	if (context == NULL)
		return (int)0x80120003;
	if ((context->flags & 2) != 0) {
		search.key = (const unsigned char *)key;
		search.keyTable = context->keyTable;
		found = appmgrPsfBinarySearch(&search, context->diskEntries,
			context->header->entryCount, sizeof(AppmgrPsfDiskEntry),
			appmgrPsfCompareDiskKey);
	} else {
		if ((context->flags & 4) == 0) {
			appmgrPsfShellSort(writeContext->entries, writeContext->entryHighWater,
				sizeof(AppmgrPsfEntry), appmgrPsfCompareEntries);
			flags = context->flags;
			header = context->header;
			count = header->entryCount;
			context->flags = flags | 4;
			writeContext->entryHighWater = count;
		}
		found = appmgrPsfBinarySearch(key, writeContext->entries,
			context->header->entryCount, sizeof(AppmgrPsfEntry),
			appmgrPsfCompareEntryKey);
	}
	if (found == NULL)
		return (int)0x80120005;
	*entry = found;
	return 0;
}

int appmgrPsfSetEntry(void *object, const char *key, const void *value,
	SceUInt16 format, SceSize length, SceSize capacity)
{
	AppmgrPsfContext *context = object;
	AppmgrPsfWriteContext *writeContext = object;
	AppmgrPsfHeader *header;
	AppmgrPsfEntry *entry, *candidate;
	AppmgrPsfDiskEntry *diskEntry;
	signed char (*keySlots)[32];
	signed char *keySlot;
	SceUInt32 flags;
	SceSize count, limit, highWater, index, keyLength;

	if (context == NULL || (context->flags & 2) != 0 ||
	    appmgrPsfStringLength((const signed char *)key) - 1 > 30)
		return (int)0x80120003;
	if ((context->flags & 6) == 0) {
		appmgrPsfShellSort(writeContext->entries, writeContext->entryHighWater,
			sizeof(AppmgrPsfEntry), appmgrPsfCompareEntries);
		flags = context->flags;
		header = context->header;
		count = header->entryCount;
		context->flags = flags | 4;
		writeContext->entryHighWater = count;
	}
	entry = appmgrPsfBinarySearch(key, writeContext->entries,
		context->header->entryCount, sizeof(AppmgrPsfEntry),
		appmgrPsfCompareEntryKey);
	if (entry != NULL)
		return (int)0x80120006;
	limit = context->storage.entryCapacity;
	if (context->header->entryCount + 1 > limit)
		return (int)0x80120001;
	highWater = writeContext->entryHighWater;
	if (limit > highWater) {
		writeContext->entryHighWater = highWater + 1;
		entry = writeContext->entries + highWater;
	} else {
		candidate = writeContext->entries;
		if (limit == 0)
			return (int)0x80120001;
		for (index = 0; index < limit; index++, candidate++) {
			if (candidate->diskEntry == NULL) {
				entry = candidate;
				break;
			}
		}
	}
	highWater = writeContext->keyHighWater;
	keySlots = (signed char (*)[32])writeContext->keys;
	if (limit > highWater) {
		keySlot = keySlots[highWater];
		writeContext->keyHighWater = highWater + 1;
		if (keySlot == NULL)
			return (int)0x80120001;
	} else {
		if (limit == 0)
			return (int)0x80120001;
		keySlot = keySlots[0];
		for (index = 0; *keySlot != 0; index++, keySlot += 32) {
			if (index + 1 == limit)
				return (int)0x80120001;
		}
	}
	highWater = writeContext->diskEntryHighWater;
	if (limit > highWater) {
		writeContext->diskEntryHighWater = highWater + 1;
		diskEntry = context->diskEntries + highWater;
		if (diskEntry == NULL) {
			*keySlot = 0;
			return (int)0x80120001;
		}
	} else {
		diskEntry = context->diskEntries;
		if (limit == 0) {
			*keySlot = 0;
			return (int)0x80120001;
		}
		for (index = 0; diskEntry->keyOffset != 0xffff; index++, diskEntry++) {
			if (index + 1 == limit) {
				*keySlot = 0;
				return (int)0x80120001;
			}
		}
	}
	keyLength = appmgrPsfStringLength((const signed char *)key);
	appmgrPsfCopyMemory(keySlot, key, keyLength);
	appmgrPsfSetMemory(diskEntry, 0, sizeof(*diskEntry));
	flags = context->flags;
	header = context->header;
	entry->flags = 3;
	entry->diskEntry = diskEntry;
	entry->key = (unsigned char *)keySlot;
	if ((flags & 4) != 0)
		flags ^= 4;
	header->entryCount++;
	context->flags = flags | 0x40;
	diskEntry->format = format;
	diskEntry->length = length;
	diskEntry->capacity = (capacity + 3) & ~3U;
	diskEntry->keyOffset = 0;
	diskEntry->valueOffset = 0;
	entry->value = (void *)value;
	return 0;
}

void appmgrPsfRemoveEntry(void *object, AppmgrPsfEntry *entry)
{
	AppmgrPsfContext *context = object;

	if (entry == NULL || context == NULL)
		return;
	if ((context->flags & 2) != 0)
		return;
	if (entry->diskEntry != NULL && (entry->flags & 2) != 0) {
		if (entry->key != NULL) {
			*entry->key = 0;
			entry->key = NULL;
		}
		entry->diskEntry->keyOffset = 0xffff;
		entry->diskEntry = NULL;
	}
	entry->value = NULL;
	entry->flags = 0;
	context->header->entryCount--;
	if ((context->flags & 4) != 0)
		context->flags ^= 4;
	context->flags |= 0x40;
}

int appmgrPsfValidateEntries(const void *data, SceSize entryOffset,
	const AppmgrPsfHeader *header, SceSize size)
{
	const AppmgrPsfDiskEntry *entry =
		(const AppmgrPsfDiskEntry *)((const unsigned char *)data + entryOffset);
	SceSize count = header->entryCount;
	SceUInt32 keyTableOffset, valueTableOffset, valueEnd, format;
	SceSize index = 0;

	if (count == 0)
		return 0;
	keyTableOffset = header->keyTableOffset;
	valueTableOffset = header->valueTableOffset;
	while (keyTableOffset + entry->keyOffset < valueTableOffset) {
		format = entry->format;
		if (format != 0x104) {
			if (format <= 0x104) {
				if (format != 8) {
					if (format <= 8) {
						if (format != 0 && format != 4)
							return (int)0x80120003;
					} else if (format != 32 && format != 256 && format != 16) {
						return (int)0x80120003;
					}
				}
			} else if (format != 0x204) {
				if (format > 0x204) {
					if (format != 0x404 && format != 0x504 && format != 0x304)
						return (int)0x80120003;
				} else if (format != 0x110 && format != 0x120 && format != 0x108) {
					return (int)0x80120003;
				}
			}
		}
		if (entry->capacity < entry->length)
			return (int)0x80120003;
		valueEnd = entry->capacity + entry->valueOffset;
		if (valueEnd > size)
			return (int)0x80120003;
		if (index == count - 1) {
			if (valueEnd + valueTableOffset != size)
				return (int)0x80120003;
		} else if (valueEnd != entry[1].valueOffset) {
			return (int)0x80120003;
		}
		if (++index == count)
			return 0;
		__builtin_prefetch(entry + 8);
		entry++;
	}
	return (int)0x80120003;
}
