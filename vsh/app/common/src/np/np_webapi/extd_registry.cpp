/*
 * SceNpWebApi: the service registry of the extended push event filters: the services, the names each
 * knows and the objects that point at them. The names are ours.
 */

#include "np_webapi_internal.h"

void *npWebApiExtdEntryClearService(void *service)
{
	sceClibMemset(service, 0, 65);
	return service;
}

int npWebApiExtdEntrySetService(char *dst, const char *src)
{
	if (sceClibStrlcpy(dst, src, 65) < 65)
		return 0;
	return NP_WEBAPI_ERROR_REGISTRY_SERVICE_TOO_LONG;
}

void *npWebApiExtdNameClear(void *name)
{
	sceClibMemset(name, 0, 33);
	return name;
}

int npWebApiExtdNameSet(char *dst, const char *src)
{
	if (sceClibStrlcpy(dst, src, 33) < 33)
		return 0;
	return NP_WEBAPI_ERROR_REGISTRY_NAME_TOO_LONG;
}

int npWebApiExtdNameEquals(const char *a, const char *b)
{
	return sceClibStrncmp(a, b, 33) == 0;
}

NpWebApiExtdName *npWebApiExtdNameDestruct(NpWebApiExtdName *name)
{
	name->vtable = g_baseVtable;
	return name;
}

int npWebApiExtdNameDelete(NpWebApiExtdName *name)
{
	name->vtable = g_baseVtable;
	return SceNpCommonPs4_BB5282C8(name);
}

NpWebApiExtdEntry *npWebApiExtdEntryDestruct(NpWebApiExtdEntry *entry)
{
	NpWebApiExtdName *name;
	NpWebApiExtdName *next;
	NpWebApiExtdName *prev;

	entry->vtable = g_extdEntryVtable;
	for (name = entry->head; name != NULL; name = entry->head) {
		next = name->next;
		prev = name->prev;
		if (next != NULL)
			next->prev = prev;
		else
			entry->tail = prev;
		if (name->prev != NULL)
			name->prev->next = name->next;
		else
			entry->head = name->next;
		name->next = NULL;
		name->prev = NULL;
		((void (*)(NpWebApiExtdName *))name->vtable[1])(name);
	}
	entry->vtable = g_baseVtable;
	return entry;
}

int npWebApiExtdEntryDelete(NpWebApiExtdEntry *entry)
{
	npWebApiExtdEntryDestruct(entry);
	return SceNpCommonPs4_BB5282C8(entry);
}

NpWebApiExtdService *npWebApiExtdServiceDestruct(NpWebApiExtdService *service)
{
	service->vtable = g_baseVtable;
	return service;
}

int npWebApiExtdServiceDelete(NpWebApiExtdService *service)
{
	service->vtable = g_baseVtable;
	return SceNpCommonPs4_BB5282C8(service);
}

int npWebApiExtdEntryListAdd(NpWebApiExtdEntryList *list, void *pool, const char *service, NpWebApiExtdEntry **pEntry)
{
	NpWebApiExtdEntry *entry;
	NpWebApiExtdEntry *memory;
	NpWebApiExtdEntry *tail;
	int result;

	entry = NULL;
	memory = (NpWebApiExtdEntry *)SceNpCommonPs4_D154D84F(92, pool);
	if (memory != NULL) {
		memory->pool = pool;
		memory->vtable = g_extdEntryVtable;
		npWebApiExtdEntryClearService(memory->service);
		memory->next = NULL;
		memory->prev = NULL;
		memory->head = NULL;
		memory->tail = NULL;
		entry = memory;
		result = npWebApiExtdEntrySetService(memory->service, service);
		if (result >= 0) {
			memory->next = NULL;
			tail = list->tail;
			memory->prev = tail;
			if (tail != NULL)
				list->tail->next = memory;
			if (list->head == NULL)
				list->head = memory;
			list->tail = memory;
			*pEntry = memory;
			return 0;
		}
	} else {
		result = ((int *)pool)[4];
	}
	if (entry != NULL)
		((void (*)(NpWebApiExtdEntry *))entry->vtable[1])(entry);
	return result;
}

int npWebApiExtdEntryListRemove(NpWebApiExtdEntryList *list, NpWebApiExtdEntry *entry)
{
	NpWebApiExtdEntry *found;
	NpWebApiExtdEntry *next;
	NpWebApiExtdEntry *prev;
	const void *const *vtable;

	found = list->head;
	if (found == NULL)
		return NP_WEBAPI_ERROR_REGISTRY_ENTRY_NOT_FOUND;
	while (1) {
		next = found->next;
		if (found == entry)
			break;
		found = found->next;
		if (next == NULL)
			return NP_WEBAPI_ERROR_REGISTRY_ENTRY_NOT_FOUND;
	}
	prev = found->prev;
	if (next != NULL)
		next->prev = prev;
	else
		list->tail = prev;
	if (found->prev != NULL)
		found->prev->next = found->next;
	else
		list->head = found->next;
	vtable = found->vtable;
	found->next = NULL;
	found->prev = NULL;
	((void (*)(NpWebApiExtdEntry *))vtable[1])(found);
	return 0;
}

int npWebApiExtdEntryAddName(NpWebApiExtdEntry *entry, const char *name)
{
	NpWebApiExtdName *node;
	NpWebApiExtdName *memory;
	NpWebApiExtdName *tail;
	int result;

	for (node = entry->head; node != NULL; node = node->next) {
		if (npWebApiExtdNameEquals(node->name, name) != 0)
			return NP_WEBAPI_ERROR_REGISTRY_NAME_EXISTS;
	}
	node = NULL;
	memory = (NpWebApiExtdName *)SceNpCommonPs4_D154D84F(48, entry->pool);
	if (memory != NULL) {
		memory->vtable = g_extdNameVtable;
		npWebApiExtdNameClear(memory->name);
		memory->next = NULL;
		memory->prev = NULL;
		node = memory;
		result = npWebApiExtdNameSet(memory->name, name);
		if (result >= 0) {
			memory->next = NULL;
			tail = entry->tail;
			memory->prev = tail;
			if (tail != NULL)
				entry->tail->next = memory;
			if (entry->head == NULL)
				entry->head = memory;
			entry->tail = memory;
			return 0;
		}
	} else {
		result = ((int *)entry->pool)[4];
	}
	if (node != NULL)
		((void (*)(NpWebApiExtdName *))node->vtable[1])(node);
	return result;
}

int npWebApiExtdRegistryAddEntry(NpPushLibContext *registry, void *pool, const char *service,
	NpWebApiExtdEntry **pEntry)
{
	int result;

	SceNpCommonPs4_A2A8493B(registry->mutex);
	result = npWebApiExtdEntryListAdd(&registry->services, pool, service, pEntry);
	SceNpCommonPs4_6BA5A164(registry->mutex);
	if (result >= 0)
		return 0;
	return result;
}

int npWebApiExtdRegistryRemoveEntry(NpPushLibContext *registry, NpWebApiExtdEntry *entry)
{
	int result;

	SceNpCommonPs4_A2A8493B(registry->mutex);
	result = npWebApiExtdEntryListRemove(&registry->services, entry);
	SceNpCommonPs4_6BA5A164(registry->mutex);
	if (result >= 0)
		return 0;
	return result;
}

int npWebApiExtdRegistryAddName(NpPushLibContext *registry, NpWebApiExtdEntry *entry, const char *name)
{
	int result;

	SceNpCommonPs4_A2A8493B(registry->mutex);
	result = npWebApiExtdEntryAddName(entry, name);
	SceNpCommonPs4_6BA5A164(registry->mutex);
	if (result >= 0)
		return 0;
	return result;
}

int npWebApiExtdServiceCreate(void *pool, const char *service, NpWebApiExtdService **pService)
{
	NpPushLibContext *registry;
	NpWebApiExtdService *object;
	NpWebApiExtdService *memory;
	NpWebApiExtdEntry *entry;
	int result;

	registry = npPushLibContextGet();
	if (registry == NULL)
		return NP_WEBAPI_ERROR_REGISTRY_NOT_FOUND;
	object = NULL;
	memory = (NpWebApiExtdService *)SceNpCommonPs4_D154D84F(8, pool);
	if (memory != NULL) {
		memory->vtable = g_extdServiceVtable;
		object = memory;
		result = npWebApiExtdRegistryAddEntry(registry, pool, service, &entry);
		if (result >= 0) {
			memory->entry = entry;
			*pService = memory;
			return 0;
		}
	} else {
		result = ((int *)pool)[4];
	}
	if (object != NULL)
		((void (*)(NpWebApiExtdService *))object->vtable[1])(object);
	return result;
}

int npWebApiExtdServiceAddName(NpWebApiExtdService *service, const char *name)
{
	NpPushLibContext *registry;
	int result;

	registry = npPushLibContextGet();
	if (registry == NULL)
		return NP_WEBAPI_ERROR_REGISTRY_NOT_FOUND;
	if (sceClibStrnlen(name, 0xFFFFFFFF) > 0x20)
		return NP_WEBAPI_ERROR_REGISTRY_NAME_TOO_LONG;
	result = npWebApiExtdRegistryAddName(registry, service->entry, name);
	if (result >= 0)
		return 0;
	return result;
}

int npWebApiExtdServiceDestroy(NpWebApiExtdService *service)
{
	NpPushLibContext *registry;
	int result;

	registry = npPushLibContextGet();
	if (registry == NULL)
		return NP_WEBAPI_ERROR_REGISTRY_NOT_FOUND;
	result = npWebApiExtdRegistryRemoveEntry(registry, service->entry);
	if (result >= 0) {
		if (service != NULL)
			((void (*)(NpWebApiExtdService *))service->vtable[1])(service);
		return 0;
	}
	return result;
}

const void *g_extdNameVtable[] = {		/* 0x8100f0e0 */
	(const void *)npWebApiExtdNameDestruct,
	(const void *)npWebApiExtdNameDelete,
	NULL,
};

const void *g_extdEntryVtable[] = {		/* 0x8100f0f0 */
	(const void *)npWebApiExtdEntryDestruct,
	(const void *)npWebApiExtdEntryDelete,
	NULL,
};

const void *g_extdServiceVtable[] = {		/* 0x8100f088 */
	(const void *)npWebApiExtdServiceDestruct,
	(const void *)npWebApiExtdServiceDelete,
	NULL,
};
