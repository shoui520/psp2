#include "property.h"
#include "../../memory.h"

SslProperty *sslNewProperty(void)
{
	SslProperty *property = sslAllocate(sizeof(*property));

	if (property == NULL)
		return NULL;
	sslFillMemory(property, 0, sizeof(*property));
	property->flags = SSL_PROPERTY_HEAP_OBJECT;
	return property;
}

int sslSetProperty(SslProperty *property, int type, int id, const void *data,
		   SceUInt32 length, unsigned int flags)
{
	SceUInt32 oldFlags;

	if ((flags & SSL_PROPERTY_COPY_DATA) && data != NULL && length != 0) {
		void *copy = sslAllocate(length);

		if (copy == NULL)
			return SSL_PROPERTY_FAILURE;
		sslCopyMemory(copy, data, length);
		oldFlags = property->flags;
		if (property->data != NULL && (oldFlags & SSL_PROPERTY_OWNS_DATA)) {
			if ((oldFlags & SSL_PROPERTY_WIPE_DATA) ||
			    (flags & SSL_PROPERTY_WIPE_COPY))
				sslFillMemory(property->data, 0, property->length);
			sslFree(property->data);
			oldFlags = property->flags;
		}
		property->data = copy;
		property->flags = oldFlags | SSL_PROPERTY_OWNS_DATA;
		if ((oldFlags & SSL_PROPERTY_WIPE_DATA) ||
		    (flags & SSL_PROPERTY_WIPE_COPY))
			property->flags = oldFlags | SSL_PROPERTY_OWNS_DATA |
				SSL_PROPERTY_WIPE_DATA;
	} else {
		oldFlags = property->flags;
		if (property->data != NULL && (oldFlags & SSL_PROPERTY_OWNS_DATA)) {
			if ((oldFlags & SSL_PROPERTY_WIPE_DATA) ||
			    (flags & SSL_PROPERTY_WIPE_COPY))
				sslFillMemory(property->data, 0, property->length);
			sslFree(property->data);
			oldFlags = property->flags;
		}
		property->data = (void *)data;
		if (flags & SSL_PROPERTY_TAKE_DATA)
			property->flags = oldFlags | SSL_PROPERTY_OWNS_DATA;
		else
			property->flags = oldFlags & ~SSL_PROPERTY_OWNS_DATA;
	}
	property->type = type;
	property->id = id;
	property->length = length;
	property->value = 0;
	return 0;
}

int sslCopyProperty(SslProperty *destination, const SslProperty *source,
		    unsigned int flags)
{
	unsigned int copyFlags;
	int status;

	if ((source->flags | flags) & SSL_PROPERTY_COPY_DATA) {
		copyFlags = SSL_PROPERTY_COPY_DATA;
		if ((source->flags & SSL_PROPERTY_WIPE_DATA) ||
		    (flags & SSL_PROPERTY_WIPE_COPY))
			copyFlags |= SSL_PROPERTY_WIPE_COPY;
	} else {
		copyFlags = flags & SSL_PROPERTY_TAKE_DATA;
	}
	status = sslSetProperty(destination, source->type, source->id,
				source->data, source->length, copyFlags);
	destination->value = source->value;
	return status;
}

void sslFreeProperty(SslProperty *property)
{
	SceUInt32 flags = property->flags;

	if ((flags & SSL_PROPERTY_OWNS_DATA) && property->data != NULL) {
		if (flags & SSL_PROPERTY_WIPE_DATA)
			sslFillMemory(property->data, 0, property->length);
		sslFree(property->data);
		flags = property->flags & ~SSL_PROPERTY_OWNS_DATA;
		property->data = NULL;
		property->length = 0;
		property->flags = flags;
	}
	if (flags & SSL_PROPERTY_HEAP_OBJECT)
		sslFree(property);
}

SslProperty *sslCloneProperty(SslProperty *destination,
			      const SslProperty *source, unsigned int flags)
{
	SslProperty *property = destination;
	int status;

	if (property == NULL) {
		property = sslNewProperty();
		if (property == NULL)
			return NULL;
	}
	status = sslCopyProperty(property, source, flags | SSL_PROPERTY_COPY_DATA);
	if (status == 0 || destination != NULL)
		return property;
	sslFreeProperty(property);
	return NULL;
}

int sslFindProperty(SslPropertyList *list, int type, int id, int *cursor,
		    SslProperty **result, unsigned int flags)
{
	SslProperty **entries;
	int count;
	int start = 0;
	int index;

	if (list == NULL || list->count == 0 || list->entries == NULL) {
		*result = NULL;
		return SSL_PROPERTY_FAILURE;
	}
	count = list->count;
	entries = list->entries;
	if (cursor != NULL)
		start = *cursor;
	if (start > count)
		start = count - 1;
	if (start < 0)
		start = 0;
	index = start;
	if (type != 0 && id != 0) {
		do {
			if (index >= count) {
				if (!(flags & SSL_PROPERTY_FIND_WRAP))
					goto missing;
				index = 0;
				flags &= ~SSL_PROPERTY_FIND_WRAP;
			}
			if (entries[index]->type == type && entries[index]->id == id)
				goto found;
			index++;
		} while (index != start);
	} else if (type == 0 && id == 0) {
		goto found;
	}
missing:
	*result = NULL;
	return SSL_PROPERTY_FAILURE;
found:
	*result = entries[index];
	if (cursor != NULL)
		*cursor = index + 1;
	return 0;
}

void sslFreePropertyList(SslPropertyList *list)
{
	SslProperty *storageInfo = NULL;
	int i;

	if (list->storage != NULL) {
		if (sslFindProperty(list, 1, 32, NULL, &storageInfo, 0) == 0 &&
		    storageInfo != NULL && storageInfo->length != 0)
			sslFillMemory(list->storage, 0, storageInfo->length);
		sslFree(list->storage);
	}
	if (list->entries != NULL) {
		for (i = 0; i < list->count; i++)
			sslFreeProperty(list->entries[i]);
		sslFree(list->entries);
		list->entries = NULL;
	}
	if (list->flags & SSL_PROPERTY_HEAP_OBJECT)
		sslFree(list);
}

int sslDeleteProperty(SslPropertyList *list, int type, int id)
{
	SslProperty *property;
	int cursor = 0;
	int count;
	int i;
	int status = sslFindProperty(list, type, id, &cursor, &property, 0);

	if (status != 0)
		return status;
	sslFreeProperty(property);
	count = (int)((SceUInt32)list->count - 1);
	for (i = cursor - 1; i < count; i++)
		list->entries[i] = list->entries[i + 1];
	list->count = count;
	return 0;
}

int sslAddProperty(SslPropertyList *list, SslProperty *property,
		   unsigned int flags)
{
	SslProperty *entry = property;
	int count;
	int index;

	if ((flags & SSL_PROPERTY_REPLACE) && !(flags & SSL_PROPERTY_TAKE_DATA))
		sslDeleteProperty(list, property->type, property->id);
	count = list->count;
	if (list->capacity <= count) {
		int growth = (count >> 1) + 20;
		int capacity;

		if (growth > 2000)
			growth = 2000;
		capacity = (int)((SceUInt32)count + (SceUInt32)growth);
		if (list->capacity < capacity && capacity > 0) {
			SslProperty **entries;

			if (list->entries != NULL)
				entries = sslReallocate(list->entries,
					(SceUInt32)capacity * sizeof(*entries),
					(SceUInt32)list->capacity * sizeof(*entries));
			else
				entries = sslAllocate((SceUInt32)capacity * sizeof(*entries));
			if (entries == NULL)
				return SSL_PROPERTY_FAILURE;
			list->capacity = capacity;
			list->entries = entries;
		}
	}
	if (!(flags & SSL_PROPERTY_BORROW_ENTRY)) {
		int status;

		entry = sslNewProperty();
		if (entry == NULL)
			return SSL_PROPERTY_FAILURE;
		status = sslCopyProperty(entry, property, flags);
		if (status != 0) {
			sslFreeProperty(entry);
			return status;
		}
	}
	index = list->count;
	list->count = (int)((SceUInt32)index + 1);
	list->entries[index] = entry;
	return 0;
}

SslPropertyList *sslNewPropertyList(void)
{
	SslPropertyList *list = sslAllocate(sizeof(*list));

	if (list == NULL)
		return NULL;
	sslFillMemory(list, 0, sizeof(*list));
	list->flags = SSL_PROPERTY_HEAP_OBJECT;
	return list;
}

SslPropertyList *sslCopyPropertyList(SslPropertyList *destination,
				    SslPropertyList *source, unsigned int flags)
{
	SslPropertyList *list = destination;
	int i;

	if (list == NULL) {
		list = sslNewPropertyList();
		if (list == NULL)
			return NULL;
	}
	for (i = 0; i < source->count; i++) {
		SslProperty *property = source->entries[i];

		if (property == NULL)
			continue;
		if (sslAddProperty(list, property, flags) != 0) {
			if (list != destination)
				sslFreePropertyList(list);
			return NULL;
		}
	}
	return list;
}

SslPropertyList *sslInitPropertyList(SslPropertyList *list)
{
	list->reserved = 0;
	list->count = 0;
	list->capacity = 0;
	list->entries = NULL;
	list->storage = NULL;
	list->flags = 0;
	return list;
}

int sslAddPropertyValue(SslPropertyList *list, int type, int id, SceUInt32 value,
			const void *data, SceUInt32 length, unsigned int flags)
{
	SslProperty property;

	property.type = type;
	property.id = id;
	property.value = value;
	property.data = (void *)data;
	property.length = length;
	property.flags = flags & ~SSL_PROPERTY_BORROW_ENTRY;
	return sslAddProperty(list, &property, flags & ~SSL_PROPERTY_BORROW_ENTRY);
}

int sslNextProperty(SslPropertyList *list, int *cursor, SslProperty **result)
{
	if (cursor != NULL && *cursor >= 0 && *cursor < list->count) {
		*result = list->entries[*cursor];
		*cursor += 1;
		return 0;
	}
	*result = NULL;
	return SSL_PROPERTY_FAILURE;
}

int sslPackProperties(SslPropertyList *list, unsigned int flags)
{
	SslProperty *storageInfo = NULL;
	SslProperty **entries;
	SslProperty *property = NULL;
	unsigned char *storage;
	unsigned char *cursor;
	SceUInt32 length = 0;
	int count;
	int i;

	if (sslFindProperty(list, 1, 32, NULL, &storageInfo, 0) != 0) {
		SslProperty info = { 1, 32, 0, NULL, 0, SSL_PROPERTY_REPLACE };

		if (sslAddProperty(list, &info, SSL_PROPERTY_REPLACE) != 0)
			return SSL_PROPERTY_STORAGE_ERROR;
	}
	count = list->count;
	entries = list->entries;
	if (count <= 0)
		return 0;
	for (i = 0; i < count; i++) {
		if (entries[i]->data != NULL)
			length += entries[i]->length;
	}
	if (length == 0)
		return 0;
	storage = sslAllocate(length);
	if (storage == NULL)
		return SSL_PROPERTY_STORAGE_ERROR;
	cursor = storage;
	for (i = 0; i < list->count; i++) {
		SceUInt32 propertyFlags;

		property = entries[i];
		if (property->data == NULL || property->length == 0)
			continue;
		sslCopyMemory(cursor, property->data, property->length);
		propertyFlags = property->flags;
		if (propertyFlags & SSL_PROPERTY_OWNS_DATA) {
			if ((propertyFlags & SSL_PROPERTY_WIPE_DATA) ||
			    (flags & SSL_PROPERTY_WIPE_COPY))
				sslFillMemory(property->data, 0, property->length);
			sslFree(property->data);
			propertyFlags = property->flags;
		}
		property->data = cursor;
		cursor += property->length;
		property->flags = propertyFlags & ~SSL_PROPERTY_OWNS_DATA;
	}
	if (list->storage != NULL) {
		if (storageInfo != NULL && storageInfo->length != 0)
			sslFillMemory(list->storage, 0, storageInfo->length);
		sslFree(list->storage);
	}
	list->storage = storage;
	if ((property->flags & SSL_PROPERTY_WIPE_DATA) ||
	    (flags & SSL_PROPERTY_WIPE_COPY)) {
		SslProperty info = { 1, 32, 0, NULL, length, SSL_PROPERTY_REPLACE };

		if (sslAddProperty(list, &info, SSL_PROPERTY_REPLACE) != 0)
			return SSL_PROPERTY_STORAGE_ERROR;
	}
	return 0;
}
