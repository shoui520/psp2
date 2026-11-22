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

/* send a named message through a service handle of kind 2, once */
unsigned int MlnImpl_8104DC4C(int handle, unsigned int size, const unsigned char *data,
			      const char *name, MlnBlob *out)
{
	MlnCoreServiceHandle *service = NULL;
	MlnBufferGroups *groups = NULL;
	MlnBuffer *response = NULL;
	MlnBuffer nameBuffer;
	MlnBuffer message;
	MlnHandleTable *table;
	void *unused = NULL;
	unsigned int ret;
	int sessions = 0;

	table = MlnServiceHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069FE8(handle, table, &service);
	if (ret != 0)
		goto out;
	if (service == NULL || service->state != 2 << 8) {
		ret = 3;
		goto out;
	}
	if ((size != 0 && data == NULL) || name == NULL || out == NULL) {
		ret = 4;
		goto out;
	}
	if (MlnImpl_8104A448() != 0) {
		ret = 7;
		goto out;
	}
	sessions = 1;
	ret = MlnImpl_8108299C(&groups);
	if (ret != 0)
		goto out;
	nameBuffer.size = MlnStrlen(name);
	nameBuffer.data = (unsigned char *)name;
	message.size = size;
	message.data = (unsigned char *)data;
	ret = MlnImpl_81082DC0(MlnImpl_81066CE0(service->exchange, &groups, NULL, NULL,
						&nameBuffer, &message, &response));
	if (ret != 0)
		goto out;
	out->buffer = response;
	out->size = response->size;
	response = NULL;
	service->state++;
out:
	MlnImpl_81042A88(groups);
	MlnFree(MLN_HEAP_GENERAL, unused);
	MlnBufferDestroy(MLN_HEAP_GENERAL, response);
	while (sessions-- > 0)
		MlnImpl_8104A558();
	return ret;
}

/*
 * Take the reply to a service handle's message: the uid of its bundle's node and the core's
 * state.
 */
unsigned int MlnImpl_8104DE10(int handle, const MlnBuffer *data, MlnBlob *first, MlnBlob *second)
{
	MlnCoreServiceHandle *service = NULL;
	MlnBuffer *response = NULL;
	MlnBuffer *uid = NULL;
	MlnBuffer *state = NULL;
	MlnXmlNode *document = NULL;
	MlnXmlNode *node = NULL;
	MlnHandleTable *table;
	unsigned int ret;
	unsigned int r;
	int sessions = 0;

	table = MlnServiceHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069FE8(handle, table, &service);
	if (ret != 0)
		goto out;
	if (service == NULL || service->state != (2 << 8 | 1)) {
		ret = 3;
		goto out;
	}
	if (data == NULL || first == NULL || second == NULL) {
		ret = 4;
		goto out;
	}
	if (data->size == 0 || data->data == NULL) {
		ret = 0x24;
		goto out;
	}
	if (MlnImpl_8104A448() != 0) {
		ret = 7;
		goto out;
	}
	sessions = 1;

	r = MlnImpl_81066F74(service->exchange, data, &response);
	switch (r) {
	case 0:
		break;
	case 2:
		ret = 2;
		goto out;
	case 0x23000001:
	case 0x23000002:
	case 0x2300000b:
		ret = 0x24;
		goto out;
	case 0x23000007:
		ret = 0x29;
		goto out;
	default:
		ret = 7;
		goto out;
	}
	r = MlnImpl_81082B04(service->exchange);
	if (r != 0) {
		ret = r == 2 ? 2 : 7;
		goto out;
	}
	r = MlnXmlParse(MLN_HEAP_GENERAL, response->size, response->data, &document);
	if (r == 0)
		r = MlnXmlFindNode(MLN_HEAP_GENERAL, document, "/Bundle/Node", MLN_OCTOPUS_NAMESPACE,
				   &node);
	if (r == 0)
		r = MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "uid", &uid);
	if (r != 0) {
		ret = r == 2 ? 2 : 0xb;
		goto out;
	}
	switch (MlnImpl_810457D4(uid, response)) {
	case 0:
		break;
	case 2:
		ret = 2;
		goto out;
	case 6:
		ret = 0x44;
		goto out;
	case 0x230a0012:
		ret = 0x16;
		goto out;
	default:
		ret = 7;
		goto out;
	}
	ret = MlnImpl_81043A60(1, &state);
	if (ret != 0) {
		if (ret != 2)
			ret = 7;
		goto out;
	}
	first->buffer = uid;
	first->size = uid->size;
	uid = NULL;
	second->buffer = state;
	second->size = state->size;
	state = NULL;
	service->state++;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, uid);
	MlnBufferDestroy(MLN_HEAP_GENERAL, response);
	MlnBufferDestroy(MLN_HEAP_GENERAL, state);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, node);
	while (sessions-- > 0)
		MlnImpl_8104A558();
	return ret;
}

/*
 * Send a named message with two documents (the first optional) through a service handle of
 * kind 3, once; the documents are kept in the handle for its reply.
 */
unsigned int MlnImpl_8104E18C(int handle, unsigned int size, const unsigned char *data,
			      const char *name, const MlnBuffer *third, const MlnBuffer *fourth,
			      MlnBlob *first, MlnBlob *second)
{
	MlnCoreServiceHandle *service = NULL;
	MlnBufferGroups *groups = NULL;
	MlnXmlNode *header = NULL;
	MlnXmlNode *thirdNode = NULL;
	MlnXmlNode *fourthNode = NULL;
	MlnBuffer *response = NULL;
	MlnBuffer *copy;
	MlnBuffer nameBuffer;
	MlnBuffer message;
	MlnHandleTable *table;
	void *unused = NULL;
	unsigned int ret;
	int sessions = 0;

	table = MlnServiceHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069FE8(handle, table, &service);
	if (ret != 0)
		goto out;
	if (service == NULL || service->state != 3 << 8) {
		ret = 3;
		goto out;
	}
	if ((size != 0 && data == NULL) || name == NULL || fourth == NULL || first == NULL) {
		ret = 4;
		goto out;
	}
	if (MlnImpl_8104A448() != 0) {
		ret = 7;
		goto out;
	}
	sessions = 1;
	ret = MlnImpl_8108299C(&groups);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81082BC4(&header);
	if (ret != 0)
		goto out;
	if (third != NULL) {
		ret = MlnImpl_81082CB4(third, &thirdNode, second);
		if (ret != 0)
			goto out;
	}
	ret = MlnImpl_81082CB4(fourth, &fourthNode, second);
	if (ret != 0)
		goto out;
	nameBuffer.size = MlnStrlen(name);
	nameBuffer.data = (unsigned char *)name;
	message.size = size;
	message.data = (unsigned char *)data;
	ret = MlnImpl_81082DC0(MlnImpl_810670A0(service->exchange, &groups, NULL, NULL, header,
						thirdNode, fourthNode, &nameBuffer, &message,
						&response));
	if (ret != 0)
		goto out;
	copy = MlnBufferCreate(MLN_HEAP_GENERAL, fourth->size, fourth->data);
	service->messages[0] = copy;
	if (copy == NULL) {
		ret = 2;
		goto out;
	}
	if (third != NULL) {
		copy = MlnBufferCreate(MLN_HEAP_GENERAL, third->size, third->data);
		service->messages[1] = copy;
		if (copy == NULL) {
			ret = 2;
			goto out;
		}
	}
	first->buffer = response;
	first->size = response->size;
	response = NULL;
	service->state++;
out:
	MlnImpl_81042A88(groups);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, thirdNode);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, fourthNode);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, header);
	MlnFree(MLN_HEAP_GENERAL, unused);
	MlnBufferDestroy(MLN_HEAP_GENERAL, response);
	while (sessions-- > 0)
		MlnImpl_8104A558();
	return ret;
}

/* map the errors of storing a link */
static inline unsigned int MlnServiceLinkError(unsigned int r)
{
	switch (r) {
	case 2:
		return 2;
	case 6:
		return 0x44;
	case 0x230a0001:
		return 0xe;
	case 0x230a0005:
		return 0xa;
	case 0x230a0012:
		return 0x16;
	default:
		return 7;
	}
}

/*
 * Take the reply to a service handle's message of kind 3: a link between the two documents
 * sent, which is stored; returns its uid, the second reply and the core's state. The
 * documents kept in the handle are dropped.
 */
unsigned int MlnImpl_8104E45C(int handle, const MlnBuffer *data, MlnBlob *first, MlnBlob *second,
			      MlnBlob *third)
{
	MlnCoreServiceHandle *service = NULL;
	MlnXmlNode *document = NULL;
	MlnXmlNode *node = NULL;
	MlnBuffer *response = NULL;
	MlnBuffer *reply = NULL;
	MlnBuffer *state = NULL;
	MlnBuffer *uid = NULL;
	MlnOctopusLink *link;
	MlnBuffer *unused;
	MlnBuffer *to, *from, *value1, *value2;
	MlnHandleTable *table;
	unsigned int ret;
	unsigned int r;
	int sessions = 0;

	table = MlnServiceHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069FE8(handle, table, &service);
	if (ret != 0)
		goto out;
	if (service == NULL) {
		ret = 3;
		goto out;
	}
	if (service->state != (3 << 8 | 1)) {
		ret = 3;
		goto clear;
	}
	if (data == NULL || first == NULL || second == NULL || third == NULL) {
		ret = 4;
		goto clear;
	}
	if (data->size == 0 || data->data == NULL) {
		ret = 0x24;
		goto clear;
	}
	if (MlnImpl_8104A448() != 0) {
		ret = 7;
		goto clear;
	}
	sessions = 1;

	r = MlnImpl_81067494(service->exchange, data, &response, &document);
	switch (r) {
	case 0:
		break;
	case 2:
		ret = 2;
		goto clear;
	case 0x23000001:
	case 0x23000002:
	case 0x2300000b:
		ret = 0x24;
		goto clear;
	case 0x23000007:
		ret = 0x29;
		goto clear;
	default:
		ret = 7;
		goto clear;
	}
	r = MlnImpl_81082B04(service->exchange);
	if (r != 0) {
		ret = r == 2 ? 2 : 7;
		goto clear;
	}
	if (document != NULL) {
		r = MlnImpl_81082B44(document, &node);
		if (r != 0) {
			ret = r == 2 ? 2 : r == 6 ? 0x44 : 7;
			goto clear;
		}
	}

	link = NULL;
	unused = NULL;
	to = service->messages[0];
	from = service->messages[1];
	value1 = service->messages[2];
	value2 = service->messages[3];
	r = MlnImpl_810422E8(response, &link);
	if (r != 0) {
		ret = r >> 16 != 0 ? 0xa : MlnServiceLinkError(r);
		goto unlink;
	}
	if (link->to == NULL || to == NULL ||
	    MlnBufferCompare(MLN_HEAP_GENERAL, link->to, to) != 0) {
		ret = 7;
		goto unlink;
	}
	if (from != NULL &&
	    (link->from == NULL || MlnBufferCompare(MLN_HEAP_GENERAL, link->from, from) != 0)) {
		ret = 7;
		goto unlink;
	}
	r = MlnImpl_81043DE0(to, from, response, value1, value2);
	if (r != 0) {
		ret = MlnServiceLinkError(r);
		goto unlink;
	}
	uid = MlnBufferCreate(MLN_HEAP_GENERAL, link->uid->size, link->uid->data);
	if (uid == NULL) {
		ret = 2;
		goto unlink;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, unused);
	MlnImpl_8103BBA0(link);

	ret = MlnImpl_81082DC0(MlnImpl_81067618(service->exchange, node, &reply));
	if (ret != 0)
		goto clear;
	ret = MlnImpl_81043A60(1, &state);
	if (ret != 0) {
		if (ret != 2)
			ret = 7;
		goto clear;
	}
	first->buffer = uid;
	first->size = uid->size;
	uid = NULL;
	second->buffer = reply;
	second->size = reply->size;
	reply = NULL;
	third->buffer = state;
	third->size = state->size;
	state = NULL;
	service->state++;
	goto clear;

unlink:
	MlnBufferDestroy(MLN_HEAP_GENERAL, unused);
	MlnImpl_8103BBA0(link);
clear:
	MlnBufferDestroy(MLN_HEAP_GENERAL, service->messages[1]);
	service->messages[1] = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL, service->messages[0]);
	service->messages[0] = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, response);
	MlnBufferDestroy(MLN_HEAP_GENERAL, uid);
	MlnBufferDestroy(MLN_HEAP_GENERAL, reply);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, node);
	MlnBufferDestroy(MLN_HEAP_GENERAL, state);
	while (sessions-- > 0)
		MlnImpl_8104A558();
	return ret;
}

/*
 * Send a named message with a document, or an empty one, through a service handle of kind 4,
 * once.
 */
unsigned int MlnImpl_8104E9B8(int handle, unsigned int size, const unsigned char *data,
			      const char *name, const MlnBuffer *third, MlnBlob *first,
			      MlnBlob *second)
{
	MlnCoreServiceHandle *service = NULL;
	MlnBufferGroups *groups = NULL;
	MlnXmlNode *node = NULL;
	MlnBuffer *response = NULL;
	MlnBuffer nameBuffer;
	MlnBuffer message;
	MlnHandleTable *table;
	void *unused = NULL;
	unsigned int ret;
	int sessions = 0;

	table = MlnServiceHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069FE8(handle, table, &service);
	if (ret != 0)
		goto out;
	if (service == NULL || service->state != 4 << 8) {
		ret = 3;
		goto out;
	}
	if ((size != 0 && data == NULL) || name == NULL || first == NULL) {
		ret = 4;
		goto out;
	}
	if (MlnImpl_8104A448() != 0) {
		ret = 7;
		goto out;
	}
	sessions = 1;
	ret = MlnImpl_8108299C(&groups);
	if (ret != 0)
		goto out;
	if (third != NULL)
		ret = MlnImpl_81082CB4(third, &node, second);
	else
		ret = MlnImpl_81082BC4(&node);
	if (ret != 0)
		goto out;
	nameBuffer.size = MlnStrlen(name);
	nameBuffer.data = (unsigned char *)name;
	message.size = size;
	message.data = (unsigned char *)data;
	ret = MlnImpl_81082DC0(MlnImpl_810681E4(service->exchange, &groups, NULL, NULL, node,
						&nameBuffer, &message, &response));
	if (ret != 0)
		goto out;
	first->buffer = response;
	first->size = response->size;
	response = NULL;
	service->state++;
out:
	MlnImpl_81042A88(groups);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, node);
	MlnFree(MLN_HEAP_GENERAL, unused);
	MlnBufferDestroy(MLN_HEAP_GENERAL, response);
	while (sessions-- > 0)
		MlnImpl_8104A558();
	return ret;
}

/*
 * Record the first use of a bundle's license when its rights limit how long it may be used:
 * until NotAfter, or NotLongerThan from now.
 */
static inline unsigned int MlnServiceRecordFirstUse(const MlnBuffer *token)
{
	MlnOctopusSignedBundle *bundle = NULL;
	MlnCoreContext *context = NULL;
	MlnCoreResponse *response = NULL;
	MlnRightsInfo *info = NULL;
	MlnBuffer *rights = NULL;
	MlnBuffer *pending = NULL;
	unsigned int minutes;
	unsigned int ret = 0;
	unsigned int r;
	int now;
	int mode;

	if (MlnImpl_81062024(&context) != 0) {
		ret = 2;
		goto out;
	}
	r = MlnImpl_810403F4(token, &bundle);
	if (r == 0)
		r = MlnImpl_81062524(context, bundle->bundle);
	if (r != 0) {
		ret = r == 2 ? 2 : 0x17;
		goto out;
	}
	r = MlnImpl_81062F20(context, 0, NULL, &rights);
	if (r != 0) {
		ret = r == 2 ? 2 : r == 0x2302000C ? 0x1e : 0x17;
		goto out;
	}
	if (rights == NULL)
		goto out;
	r = MlnRightsInfoParse(MLN_HEAP_GENERAL, rights->size, rights->data, &info);
	if (r != 0) {
		ret = r == 2 ? 2 : 0x42;
		goto out;
	}
	if (!(info->constraintFlags & MLN_RIGHTS_NOT_LONGER_THAN))
		goto out;
	r = MlnImpl_81045E74(&pending);
	if (r == 0)
		r = MlnImpl_81069968(pending, 0, &response);
	if (r == 0)
		r = MlnImpl_81069D90(response);
	if (r != 0)
		goto fail;
	if (info->constraintFlags & MLN_RIGHTS_NOT_AFTER) {
		if (MlnTimeToMinutes(info->notAfter[0], &minutes) != 0) {
			ret = 7;
			goto out;
		}
	} else {
		if (MlnImpl_81052D84(&now, &mode) != 0) {
			ret = 7;
			goto out;
		}
		minutes = (unsigned int)(info->notLongerThan + now) / 60;
	}
	r = MlnImpl_81069C38(response, bundle->bundle->controller->uid, minutes);
	if (r != 0)
		goto fail;
	if (response != NULL && response->pending != 0) {
		r = MlnImpl_81045EE0(response->data);
		if (r != 0)
			goto fail;
	}
	goto out;

fail:
	ret = r == 2 ? 2 : 7;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, pending);
	MlnImpl_81069A9C(response);
	MlnBufferDestroy(MLN_HEAP_GENERAL, rights);
	MlnRightsInfoFree(MLN_HEAP_GENERAL, info);
	MlnImpl_81040368(bundle);
	MlnImpl_810628E4(context);
	MlnImpl_81062060(context);
	return ret;
}

/*
 * Take the reply to a service handle's message of kind 4: a license token, whose first use is
 * recorded; returns it and the core's state.
 */
unsigned int MlnImpl_8104EBD8(int handle, const MlnBuffer *data, MlnBlob *first, MlnBlob *second)
{
	MlnCoreServiceHandle *service = NULL;
	MlnBuffer *token = NULL;
	MlnBuffer *state = NULL;
	MlnHandleTable *table;
	unsigned int ret;
	unsigned int r;
	int sessions = 0;

	table = MlnServiceHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069FE8(handle, table, &service);
	if (ret != 0)
		goto out;
	if (service == NULL || service->exchange == NULL || service->state != (4 << 8 | 1)) {
		ret = 3;
		goto out;
	}
	if (data == NULL || first == NULL || second == NULL) {
		ret = 4;
		goto out;
	}
	if (data->size == 0 || data->data == NULL) {
		ret = 0x24;
		goto out;
	}
	if (MlnImpl_8104A448() != 0) {
		ret = 7;
		goto out;
	}
	sessions = 1;

	r = MlnImpl_810684CC(service->exchange, data, &token);
	switch (r) {
	case 0:
		break;
	case 2:
		ret = 2;
		goto out;
	case 0x23000001:
	case 0x23000002:
	case 0x2300000b:
		ret = 0x24;
		goto out;
	case 0x23000007:
		ret = 0x29;
		goto out;
	default:
		ret = 7;
		goto out;
	}
	ret = MlnImpl_81082B04(service->exchange);
	if (ret == 0) {
		ret = MlnServiceRecordFirstUse(token);
		if (ret != 0)
			goto out;
		ret = MlnImpl_81043A60(1, &state);
	}
	if (ret != 0) {
		if (ret != 2)
			ret = 7;
		goto out;
	}
	first->buffer = token;
	first->size = token->size;
	token = NULL;
	second->buffer = state;
	second->size = state->size;
	state = NULL;
	service->state++;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, state);
	MlnBufferDestroy(MLN_HEAP_GENERAL, token);
	while (sessions-- > 0)
		MlnImpl_8104A558();
	return ret;
}

/*
 * Send a named message with two documents (the first optional; an empty one in its place)
 * through a service handle of kind 5, once; the documents are kept in the handle for its
 * reply. The third argument is not used.
 */
unsigned int MlnImpl_8104F044(int handle, unsigned int size, const unsigned char *data,
			      const char *name, const MlnBuffer *third, const MlnBuffer *fourth,
			      const MlnBuffer *fifth, MlnBlob *first, MlnBlob *second)
{
	MlnCoreServiceHandle *service = NULL;
	MlnBufferGroups *groups = NULL;
	MlnXmlNode *fourthNode = NULL;
	MlnXmlNode *fifthNode = NULL;
	MlnBuffer *response = NULL;
	MlnBuffer *copy;
	MlnBuffer nameBuffer;
	MlnBuffer message;
	MlnHandleTable *table;
	void *unused = NULL;
	unsigned int ret;
	int sessions = 0;

	(void)third;
	table = MlnServiceHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069FE8(handle, table, &service);
	if (ret != 0)
		goto out;
	if (service == NULL || service->state != 5 << 8) {
		ret = 3;
		goto out;
	}
	if ((size != 0 && data == NULL) || name == NULL || fifth == NULL || first == NULL) {
		ret = 4;
		goto out;
	}
	if (MlnImpl_8104A448() != 0) {
		ret = 7;
		goto out;
	}
	sessions = 1;
	ret = MlnImpl_8108299C(&groups);
	if (ret != 0)
		goto out;
	if (fourth != NULL)
		ret = MlnImpl_81082CB4(fourth, &fourthNode, second);
	else
		ret = MlnImpl_81082BC4(&fourthNode);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81082CB4(fifth, &fifthNode, second);
	if (ret != 0)
		goto out;
	nameBuffer.size = MlnStrlen(name);
	nameBuffer.data = (unsigned char *)name;
	message.size = size;
	message.data = (unsigned char *)data;
	ret = MlnImpl_81082DC0(MlnImpl_81067814(service->exchange, &groups, NULL, NULL, fourthNode,
						fifthNode, &nameBuffer, &message, &response));
	if (ret != 0)
		goto out;
	copy = MlnBufferCreate(MLN_HEAP_GENERAL, fifth->size, fifth->data);
	service->messages[0] = copy;
	if (copy == NULL) {
		ret = 2;
		goto out;
	}
	if (fourth != NULL) {
		copy = MlnBufferCreate(MLN_HEAP_GENERAL, fourth->size, fourth->data);
		service->messages[1] = copy;
		if (copy == NULL) {
			ret = 2;
			goto out;
		}
	}
	service->state++;
	first->buffer = response;
	first->size = response->size;
	response = NULL;
out:
	MlnImpl_81042A88(groups);
	MlnFree(MLN_HEAP_GENERAL, unused);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, fourthNode);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, fifthNode);
	MlnBufferDestroy(MLN_HEAP_GENERAL, response);
	while (sessions-- > 0)
		MlnImpl_8104A558();
	return ret;
}

/*
 * Take the reply to a service handle's message of kind 5: the link between the two documents
 * sent is removed; returns the second reply and the core's state. The first document kept in
 * the handle is dropped.
 */
unsigned int MlnImpl_8104F2F4(int handle, const MlnBuffer *data, MlnBlob *first, MlnBlob *second)
{
	MlnCoreServiceHandle *service = NULL;
	MlnXmlNode *document = NULL;
	MlnXmlNode *node = NULL;
	MlnBuffer *state = NULL;
	MlnBuffer *reply = NULL;
	MlnHandleTable *table;
	unsigned int ret;
	unsigned int r;
	int sessions = 0;

	table = MlnServiceHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069FE8(handle, table, &service);
	if (ret != 0)
		goto out;
	if (service == NULL) {
		ret = 3;
		goto out;
	}
	if (service->state != (5 << 8 | 1)) {
		ret = 3;
		goto clear;
	}
	if (data == NULL || first == NULL || second == NULL) {
		ret = 4;
		goto clear;
	}
	if (MlnImpl_8104A448() != 0) {
		ret = 7;
		goto clear;
	}
	sessions = 1;

	r = MlnImpl_81067BA4(service->exchange, data, &document);
	switch (r) {
	case 0:
		break;
	case 2:
		ret = 2;
		goto clear;
	case 0x23000001:
	case 0x23000002:
	case 0x2300000b:
		ret = 0x24;
		goto clear;
	case 0x23000007:
		ret = 0x29;
		goto clear;
	default:
		ret = 7;
		goto clear;
	}
	r = MlnImpl_81082B04(service->exchange);
	if (r != 0) {
		ret = r == 2 ? 2 : 7;
		goto clear;
	}
	if (document != NULL) {
		r = MlnImpl_81082B44(document, &node);
		if (r != 0)
			goto fail;
	}
	r = MlnImpl_810458B0(service->messages[1], service->messages[0]);
	switch (r) {
	case 0:
		break;
	case 2:
		ret = 2;
		goto clear;
	case 6:
		ret = 0x44;
		goto clear;
	case 0x230a0003:
		ret = 0x12;
		goto clear;
	default:
		ret = 7;
		goto clear;
	}
	r = MlnImpl_81067618(service->exchange, node, &reply);
	if (r != 0)
		goto fail;
	ret = MlnImpl_81043A60(1, &state);
	if (ret != 0) {
		if (ret != 2)
			ret = 7;
		goto clear;
	}
	first->buffer = reply;
	first->size = reply->size;
	reply = NULL;
	second->buffer = state;
	second->size = state->size;
	state = NULL;
	service->state++;
	goto clear;

fail:
	ret = r == 2 ? 2 : r == 6 ? 0x44 : 7;
clear:
	MlnBufferDestroy(MLN_HEAP_GENERAL, service->messages[0]);
	service->messages[0] = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, state);
	MlnBufferDestroy(MLN_HEAP_GENERAL, reply);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, node);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
	while (sessions-- > 0)
		MlnImpl_8104A558();
	return ret;
}
