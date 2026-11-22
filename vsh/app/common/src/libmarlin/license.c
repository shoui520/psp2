/*
 * The core's license calls over its handles: open and close, a bundle
 * given, what the license gives read, and the first use of a bundle whose
 * rights limit how long it may be used (0x8104d144-). The file's name is
 * ours.
 */

#include "mln_internal.h"

/* open a core handle */
int MlnImpl_8104D144(int *handle)
{
	return MlnImpl_8104A8D8(handle);
}

/* close a core handle */
int MlnImpl_8104D150(int handle)
{
	return MlnImpl_8104A9CC(handle);
}

/* give a core handle its signed bundle */
int MlnImpl_8104D15C(int handle, unsigned int size, const unsigned char *data)
{
	return MlnImpl_8104AB18(handle, size, data, NULL);
}

/* what a core handle's license gives for an action, NULL when nothing */
static inline __attribute__((always_inline)) unsigned int
MlnLicenseRead(int handle, int kind, MlnBuffer **out)
{
	MlnBlob blob = { NULL, 0 };
	MlnBuffer *buffer = NULL;
	unsigned int ret;

	ret = MlnImpl_8104BFB0(handle, kind, 0, &blob);
	if (ret == 0) {
		buffer = MlnBufferCreate(MLN_HEAP_GENERAL, blob.size, NULL);
		ret = MlnImpl_8102AA38(&blob, buffer);
		if (ret != 0)
			goto out;
		if (buffer == NULL) {
			ret = 2;
			goto out;
		}
	} else if (ret != 0x1e) {
		ret = ret == 2 ? 2 : 0x17;
		goto out;
	}
	*out = buffer;
	buffer = NULL;
	ret = 0;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

unsigned int MlnImpl_8104D16C(int handle, int kind, MlnBuffer **out)
{
	return MlnLicenseRead(handle, kind, out);
}

/*
 * Note the first use of the bundle in token when its rights limit how
 * long it may be used (NotLongerThan): the stored response's entry for
 * the bundle's controller gets the minute of its first use, the rights'
 * NotAfter or now plus the limit. controller, when given, must be the
 * bundle's controller.
 */
unsigned int MlnImpl_8104D208(const MlnBuffer *controller, const MlnBuffer *token)
{
	MlnCoreResponse *response = NULL;
	MlnOctopusSignedBundle *bundle = NULL;
	MlnRightsInfo *info = NULL;
	MlnBuffer *rights = NULL;
	MlnBuffer *pending;
	MlnBuffer *id;
	unsigned int notAfter = 0;
	unsigned int minutes;
	unsigned int ret;
	unsigned int r;
	int longerThan;
	int handle;
	int now;
	int mode;

	if (token == NULL)
		return 7;

	r = MlnImpl_8104A8D8(&handle);
	if (r != 0) {
		ret = r == 2 ? 2 : 7;
		goto close;
	}
	r = MlnImpl_8104AB18(handle, token->size, token->data, NULL);
	if (r != 0) {
		ret = r == 2 ? 2 : 0x17;
		goto close;
	}
	r = MlnLicenseRead(handle, 0, &rights);
	if (r == 0x2302000C)
		goto none;
	if (r != 0) {
		ret = r == 2 ? 2 : 0x17;
		goto close;
	}
	if (rights == NULL)
		goto none;
	r = MlnRightsInfoParse(MLN_HEAP_GENERAL, rights->size, rights->data, &info);
	if (r != 0) {
		ret = r == 2 ? 2 : 0x42;
		goto close;
	}
	if ((info->constraintFlags & MLN_RIGHTS_NOT_AFTER) &&
	    MlnTimeToMinutes(info->notAfter[0], &notAfter) != 0) {
		ret = 7;
		goto close;
	}
	if (!(info->constraintFlags & MLN_RIGHTS_NOT_LONGER_THAN))
		goto none;
	longerThan = info->notLongerThan;
	MlnRightsInfoFree(MLN_HEAP_GENERAL, info);
	MlnBufferDestroy(MLN_HEAP_GENERAL, rights);
	MlnImpl_8104A9CC(handle);
	if (longerThan == 0) {
		ret = 0;
		goto out;
	}

	r = MlnImpl_810403F4(token, &bundle);
	if (r != 0) {
		ret = r == 2 ? 2 : 0x17;
		goto out;
	}
	if (controller != NULL &&
	    MlnBufferCompare(MLN_HEAP_GENERAL, controller, bundle->bundle->controller->uid) != 0) {
		ret = 4;
		goto out;
	}
	pending = NULL;
	r = MlnImpl_81045E74(&pending);
	if (r == 0)
		r = MlnImpl_81069968(pending, 1, &response);
	else if (r == 6) {
		ret = 0x44;
		goto out;
	}
	if (r != 0)
		goto fail;
	id = bundle->bundle->controller->uid;
	r = MlnImpl_81069AE4(response, id);
	if (r != 0)
		goto fail;
	if (response == NULL || response->entryOffset == 0) {
		minutes = notAfter;
		if (minutes == 0) {
			if (MlnImpl_81052D84(&now, &mode) != 0) {
				ret = 7;
				goto out;
			}
			minutes = (unsigned int)(longerThan + now) / 60;
		}
		r = MlnImpl_81069C38(response, bundle->bundle->controller->uid, minutes);
		if (r != 0)
			goto fail;
	} else if (MlnImpl_81069D28(response, notAfter) != 0) {
		ret = 7;
		goto out;
	}
	if (response != NULL && response->pending != 0) {
		r = MlnImpl_81069D90(response);
		if (r == 0)
			r = MlnImpl_81045EE0(response != NULL ? response->data : NULL);
		if (r != 0)
			goto fail;
	}
	ret = 0;
	goto out;

fail:
	ret = r == 2 ? 2 : 7;
	goto out;
none:
	ret = 0;
close:
	MlnRightsInfoFree(MLN_HEAP_GENERAL, info);
	MlnBufferDestroy(MLN_HEAP_GENERAL, rights);
	MlnImpl_8104A9CC(handle);
out:
	if (bundle != NULL)
		MlnImpl_81040368(bundle);
	if (response != NULL)
		MlnImpl_81069A9C(response);
	return ret;
}

/* a service list entry and its buffers */
static inline __attribute__((always_inline)) void MlnServiceEntryFree(MlnServiceListEntry *entry)
{
	MlnBufferDestroy(MLN_HEAP_GENERAL, entry->name);
	entry->name = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL, entry->value);
	entry->value = NULL;
	MlnFree(MLN_HEAP_GENERAL, entry);
}

/* a service list, its entries and their buffers */
static inline __attribute__((always_inline)) void MlnServiceValuesFree(MlnServiceList *list)
{
	MlnServiceListEntry *entry;
	unsigned int i;

	for (i = 0; i < list->count; i++) {
		entry = list->entries[i];
		if (entry != NULL) {
			MlnBufferDestroy(MLN_HEAP_GENERAL, entry->name);
			entry->name = NULL;
			MlnBufferDestroy(MLN_HEAP_GENERAL, entry->value);
			entry->value = NULL;
		}
		MlnFree(MLN_HEAP_GENERAL, entry);
	}
	MlnFree(MLN_HEAP_GENERAL, list->entries);
	list->entries = NULL;
	list->count = 0;
	MlnFree(MLN_HEAP_GENERAL, list);
}

/* the second handle table, made on first use */
static inline __attribute__((always_inline)) MlnHandleTable *MlnServiceHandleTable(MlnShared *shared)
{
	MlnHandleTable *table = shared->buffers[5];

	if (table == NULL) {
		table = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*table));
		if (table == NULL)
			return NULL;
		table->initialized = 0;
		MlnMemset(table->slots, 0, sizeof(table->slots));
		shared->buffers[5] = table;
	}
	return table;
}

/*
 * Open a service handle of a kind (2 to 8) with a copy of the values
 * given: *handle names it. Kinds 6 and 7 are not services of the core.
 */
unsigned int MlnImpl_8104D5E0(int kind, MlnServiceList *list, int *handle)
{
	MlnCoreServiceHandle *service;
	MlnServiceListEntry *source;
	MlnServiceListEntry *entry;
	MlnServiceList *values;
	MlnHandleTable *table;
	unsigned int ret;
	unsigned int i;
	int protocol;
	int sessions = 0;
	int id;

	if (handle == NULL)
		return 4;
	switch (kind) {
	case 2:
	case 3:
	case 5:
		protocol = 2;
		break;
	case 4:
		protocol = 3;
		break;
	case 6:
	case 7:
		return 6;
	case 8:
		protocol = 6;
		break;
	default:
		return 4;
	}

	service = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*service));
	if (service == NULL)
		return 2;
	MlnMemset(service, 0, sizeof(*service));
	service->state = kind << 8;
	service->values = NULL;

	values = NULL;
	if (list != NULL && list->count != 0) {
		values = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*values));
		if (values == NULL) {
			ret = 2;
			goto out;
		}
		values->count = 0;
		values->entries = MlnAllocate(MLN_HEAP_GENERAL, list->count * sizeof(*values->entries));
		if (values->entries == NULL) {
			ret = 2;
			goto free;
		}
		for (i = 0; i < list->count; i++) {
			values->entries[i] = NULL;
			source = list->entries[i];
			entry = NULL;
			ret = 0;
			if (source != NULL) {
				if (source->name == NULL || source->name->data == NULL ||
				    source->name->size == 0) {
					ret = 4;
					goto next;
				}
				entry = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*entry));
				if (entry == NULL) {
					ret = 2;
					goto next;
				}
				entry->value = NULL;
				entry->name = MlnBufferCreate(MLN_HEAP_GENERAL, source->name->size,
							      source->name->data);
				if (entry->name == NULL) {
					ret = 2;
					goto bad;
				}
				if (source->value != NULL && source->value->size != 0) {
					if (source->value->data == NULL) {
						ret = 4;
						goto bad;
					}
					entry->value = MlnBufferCreate(MLN_HEAP_GENERAL,
								       source->value->size,
								       source->value->data);
					if (entry->value == NULL) {
						ret = 2;
						goto bad;
					}
				}
				entry->flag = source->flag;
			}
			values->entries[i] = entry;
			entry = NULL;
			goto next;
bad:
			MlnServiceEntryFree(entry);
			entry = NULL;
next:
			MlnFree(MLN_HEAP_GENERAL, entry);
			if (ret != 0)
				goto free;
			values->count++;
		}
	}
	service->values = values;

	ret = MlnImpl_81068844(protocol, &service->exchange);
	if (ret != 0) {
		ret = 2;
		goto out;
	}
	if (MlnImpl_8104A448() != 0) {
		ret = 7;
		goto out;
	}
	sessions = 1;
	table = MlnServiceHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069F10(table);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81069F68(service, table, &id);
	if (ret != 0)
		goto out;
	*handle = id;
	goto end;

free:
	MlnServiceValuesFree(values);
out:
	values = service->values;
	if (values != NULL)
		MlnServiceValuesFree(values);
	MlnFree(MLN_HEAP_GENERAL, service->exchange);
	MlnFree(MLN_HEAP_GENERAL, service);
end:
	while (sessions-- > 0)
		MlnImpl_8104A558();
	return ret;
}

/* close a service handle of MlnImpl_8104D5E0 */
int MlnImpl_8104DAA4(int handle)
{
	MlnCoreServiceHandle *service = NULL;
	MlnHandleTable *table;
	int ret;

	if (MlnImpl_8104A448() != 0)
		return 7;

	table = MlnServiceHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto end;
	}
	ret = MlnImpl_81069FE8(handle, table, &service);
	if (ret != 0 || service == NULL)
		goto end;
	ret = MlnImpl_81068888(service->exchange) != 0 ? 7 : 0;
	MlnBufferDestroy(MLN_HEAP_GENERAL, service->messages[0]);
	service->messages[0] = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL, service->messages[1]);
	service->messages[1] = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL, service->messages[2]);
	service->messages[2] = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL, service->messages[3]);
	service->messages[3] = NULL;
	if (service->values != NULL)
		MlnServiceValuesFree(service->values);
	MlnFree(MLN_HEAP_GENERAL, service);
	if (MlnImpl_8106A048(handle, NULL, table) != 0)
		ret = 7;
end:
	MlnImpl_8104A558();
	return ret;
}
