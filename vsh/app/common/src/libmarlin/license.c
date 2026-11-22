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

/* the errors of a service's SOAP faults: by fault code and, where given, detail name */
static const struct {
	MlnBuffer code;
	MlnBuffer detail;
	unsigned int error;
} MlnServiceFaultErrors[13] = {
#define MLN_FAULT_TEXT(s)	{ sizeof(s) - 1, (unsigned char *)(s) }
	{ MLN_FAULT_TEXT("wsse:InvalidSecurityToken"),
	  MLN_FAULT_TEXT("NemoTrustObjectDCSAssertionStaleException"), 0x30 },
	{ MLN_FAULT_TEXT("wsse:InvalidSecurityToken"),
	  MLN_FAULT_TEXT("NemoTrustObjectMarlinSpecVersionException"), 0x31 },
	{ MLN_FAULT_TEXT("wsse:InvalidSecurityToken"),
	  MLN_FAULT_TEXT("NemoTrustObjectClientAttributeMismatchException"), 0x32 },
	{ MLN_FAULT_TEXT("soap:Server"), MLN_FAULT_TEXT("MediaServiceException"), 0x33 },
	{ MLN_FAULT_TEXT("soap:Server"), { 0, NULL }, 0x34 },
	{ MLN_FAULT_TEXT("soap:Client"), { 0, NULL }, 0x35 },
	{ MLN_FAULT_TEXT("wsse:InvalidSecurity"), { 0, NULL }, 0x36 },
	{ MLN_FAULT_TEXT("wsse:InvalidSecurityToken"), { 0, NULL }, 0x37 },
	{ MLN_FAULT_TEXT("wsse:UnsupportedSecurityToken"), { 0, NULL }, 0x38 },
	{ MLN_FAULT_TEXT("wsse:FailedCheck"), { 0, NULL }, 0x39 },
	{ MLN_FAULT_TEXT("wsse:FailedAuthentication"), { 0, NULL }, 0x3a },
	{ MLN_FAULT_TEXT("wsu:MessageExpired"), { 0, NULL }, 0x3b },
	{ MLN_FAULT_TEXT("nemosec:UnsupportedSecureProtocol"), { 0, NULL }, 0x3c },
#undef MLN_FAULT_TEXT
};

/* the SOAP fault of a service handle's exchange, with its error, in the core's form */
unsigned int MlnImpl_8104F620(int handle, MlnCoreGroup *core)
{
	MlnCoreServiceHandle *service = NULL;
	MlnServiceFault *fault;
	MlnBufferGroup *group;
	MlnBuffer ***entries;
	MlnBuffer **entry;
	MlnBuffer *detail;
	MlnHandleTable *table;
	unsigned int error;
	unsigned int count;
	unsigned int ret;
	unsigned int i, j;

	table = MlnServiceHandleTable(MlnImpl_81006EAC());
	if (table == NULL)
		return 2;
	ret = MlnImpl_81069FE8(handle, table, &service);
	if (ret != 0)
		return ret;
	if (service == NULL || service->exchange == NULL)
		return 3;
	if (core == NULL)
		return 4;
	if (MlnImpl_8104A448() != 0)
		return 7;

	group = NULL;
	ret = MlnImpl_81068C4C(service->exchange, &fault);
	if (ret != 0) {
		ret = ret == 0x23000009 ? 0x2a : 7;
		goto end;
	}
	if (fault == NULL) {
		ret = 0x2a;
		goto end;
	}
	group = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*group));
	if (group == NULL)
		goto nomem;
	MlnMemset(group, 0, sizeof(*group));

	error = 0x2f;
	for (i = 0; i < 13; i++) {
		if (MlnBufferCompareKey(MLN_HEAP_GENERAL, fault->code,
					&MlnServiceFaultErrors[i].code) != 0)
			continue;
		if (MlnServiceFaultErrors[i].detail.size == 0 ||
		    MlnServiceFaultErrors[i].detail.data == NULL) {
			error = MlnServiceFaultErrors[i].error;
			break;
		}
		if (fault->details == NULL || fault->detailCount == 0)
			continue;
		for (j = 0; j < fault->detailCount; j++) {
			if (fault->details[j] == NULL)
				continue;
			detail = fault->details[j][0];
			if (detail == NULL)
				continue;
			if (MlnBufferCompareKey(MLN_HEAP_GENERAL, detail,
						&MlnServiceFaultErrors[i].detail) == 0) {
				error = MlnServiceFaultErrors[i].error;
				break;
			}
		}
	}
	group->tag = error;
	if (fault->code != NULL) {
		group->first = MlnBufferCreate(MLN_HEAP_GENERAL, fault->code->size,
					       fault->code->data);
		if (group->first == NULL)
			goto nomem;
	}
	if (fault->string != NULL) {
		group->second = MlnBufferCreate(MLN_HEAP_GENERAL, fault->string->size,
						fault->string->data);
		if (group->second == NULL)
			goto nomem;
	}
	if (fault->actor != NULL) {
		group->third = MlnBufferCreate(MLN_HEAP_GENERAL, fault->actor->size,
					       fault->actor->data);
		if (group->third == NULL)
			goto nomem;
	}
	if (fault->details != NULL && fault->detailCount != 0) {
		entries = MlnAllocate(MLN_HEAP_GENERAL, fault->detailCount * sizeof(*entries));
		if (entries == NULL)
			goto nomem;
		MlnMemset(entries, 0, fault->detailCount * sizeof(*entries));
		count = fault->detailCount;
		for (j = 0; j < count; j++) {
			entry = MlnAllocate(MLN_HEAP_GENERAL, 3 * sizeof(*entry));
			entries[j] = entry;
			if (entry == NULL)
				goto free;
			MlnMemset(entry, 0, 3 * sizeof(*entry));
			for (i = 0; i < 3; i++) {
				if (fault->details[j][i] == NULL)
					continue;
				entry[i] = MlnBufferCreate(MLN_HEAP_GENERAL,
							   fault->details[j][i]->size,
							   fault->details[j][i]->data);
				if (entries[j][i] == NULL)
					goto free;
			}
		}
		group->entries = entries;
		group->entryCount = count;
	}

	core->tag = group->tag;
	if (group->first != NULL) {
		core->firstSize = group->first->size;
		core->first = group->first;
		group->first = NULL;
	}
	if (group->second != NULL) {
		core->secondSize = group->second->size;
		core->second = group->second;
		group->second = NULL;
	}
	if (group->third != NULL) {
		core->thirdSize = group->third->size;
		core->third = group->third;
		group->third = NULL;
	}
	if (group->entries != NULL && group->entryCount != 0) {
		core->entries = MlnAllocate(MLN_HEAP_GENERAL,
					    group->entryCount * sizeof(*core->entries));
		if (core->entries == NULL)
			goto nomem;
		/* half the array: three words an entry */
		MlnMemset(core->entries, 0, group->entryCount * 3 * sizeof(MlnBuffer *));
		core->entryCount = group->entryCount;
		for (j = 0; j < core->entryCount; j++) {
			core->entries[j].first = NULL;
			core->entries[j].firstSize = 0;
			core->entries[j].second = NULL;
			core->entries[j].secondSize = 0;
			core->entries[j].third = NULL;
			core->entries[j].thirdSize = 0;
			entry = group->entries[j];
			if (entry[0] != NULL) {
				core->entries[j].firstSize = entry[0]->size;
				core->entries[j].first = group->entries[j][0];
				group->entries[j][0] = NULL;
			}
			if (group->entries[j][1] != NULL) {
				core->entries[j].secondSize = group->entries[j][1]->size;
				core->entries[j].second = group->entries[j][1];
				group->entries[j][1] = NULL;
			}
			if (group->entries[j][2] != NULL) {
				core->entries[j].thirdSize = group->entries[j][2]->size;
				core->entries[j].third = group->entries[j][2];
				group->entries[j][2] = NULL;
			}
		}
	}
	goto out;

free:
	for (j = 0; j < count; j++)
		MlnImpl_81006CD4(MLN_HEAP_GENERAL, entries[j]);
	MlnFree(MLN_HEAP_GENERAL, entries);
nomem:
	ret = 2;
	if (core->entries != NULL) {
		MlnFree(MLN_HEAP_GENERAL, core->entries);
		core->entries = NULL;
		core->entryCount = 0;
	}
out:
	MlnImpl_81006D14(MLN_HEAP_GENERAL, group);
end:
	MlnImpl_8104A558();
	return ret;
}

/* free a certificate chain of MlnImpl_81059600 */
static inline void MlnServiceChainFree(MlnBuffer **certificates, int count)
{
	unsigned short i;

	if (certificates == NULL)
		return;
	for (i = 0; i < count; i++) {
		if (certificates[i] != NULL)
			MlnBufferDestroy(MLN_HEAP_GENERAL, certificates[i]);
	}
	MlnFree(MLN_HEAP_GENERAL, certificates);
}

/*
 * Give a service handle's exchange the two certificate chains of its peers, once each chain is
 * checked for the peer's role and the leaf certificates' subjects match the NameIdentifier of
 * the SAML assertion given.
 */
unsigned int MlnImpl_8104FC70(int handle, unsigned int firstSize, const unsigned char *first,
			      unsigned int secondSize, const unsigned char *second,
			      unsigned int thirdSize, const unsigned char *third)
{
	MlnCoreServiceHandle *service = NULL;
	MlnBuffer **firstChain = NULL;
	MlnBuffer **secondChain = NULL;
	MlnBuffer *nameIdentifier = NULL;
	MlnBuffer *subject;
	MlnX509 *x509;
	MlnBuffer firstBuffer = { firstSize, (unsigned char *)first };
	MlnBuffer secondBuffer = { secondSize, (unsigned char *)second };
	MlnBuffer assertion;
	MlnBuffer role;
	MlnHandleTable *table;
	unsigned int ret;
	unsigned int r;
	int firstCount = 0;
	int secondCount = 0;
	int firstKind, secondKind;
	int type;
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
	if (firstSize == 0 || first == NULL || secondSize == 0 || second == NULL ||
	    thirdSize == 0 || third == NULL) {
		ret = 4;
		goto out;
	}
	if (MlnImpl_8104A448() != 0) {
		ret = 7;
		goto out;
	}
	sessions = 1;

	assertion.size = thirdSize;
	assertion.data = (unsigned char *)third;
	switch (service->state >> 8) {
	case 2:
	case 3:
	case 5:
		role.size = 27;
		role.data = (unsigned char *)"urn:marlin:core:role:device";
		firstKind = 0xc;
		secondKind = 0xb;
		type = 0;
		break;
	case 4:
		role.size = 41;
		role.data = (unsigned char *)"urn:marlin:broadband:role:license-service";
		firstKind = 0xa;
		secondKind = 9;
		type = 2;
		break;
	case 8:
		role.size = 46;
		role.data = (unsigned char *)"urn:marlin:broadband:role:registration-service";
		firstKind = 0xa;
		secondKind = 9;
		type = 2;
		break;
	default:
		ret = 4;
		goto out;
	}
	r = MlnImpl_81042B58(&assertion, type, &role, &nameIdentifier);
	switch (r) {
	case 0:
		break;
	case 1:
		ret = 0xc;
		goto out;
	case 2:
		ret = 2;
		goto out;
	case 3:
		ret = 7;
		goto out;
	case 4:
		ret = 0xc;
		goto out;
	case 5:
		ret = 7;
		goto out;
	default:
		ret = 0xc;
		goto out;
	}
	r = MlnImpl_81059600(&firstBuffer, &firstChain, &firstCount);
	if (r == 0)
		r = MlnImpl_81059600(&secondBuffer, &secondChain, &secondCount);
	if (r != 0) {
		ret = r == 2 ? 2 : 0x2b;
		goto out;
	}

	x509 = NULL;
	subject = NULL;
	r = MlnImpl_81082A60(secondChain, secondCount, secondKind);
	if (r == 0)
		r = MlnImpl_81082A60(firstChain, firstCount, firstKind);
	if (r == 0)
		r = MlnImpl_81038EF4(firstChain[0], &x509);
	if (r == 0)
		r = MlnImpl_81039298(x509, &subject);
	if (r != 0)
		goto check;
	if (MlnBufferCompare(MLN_HEAP_GENERAL, nameIdentifier, subject) != 0) {
		r = 5;
		goto check;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, subject);
	subject = NULL;
	MlnImpl_81038E90(x509);
	x509 = NULL;
	r = MlnImpl_81038EF4(secondChain[0], &x509);
	if (r == 0)
		r = MlnImpl_81039298(x509, &subject);
	if (r != 0)
		goto check;
	if (MlnBufferCompare(MLN_HEAP_GENERAL, nameIdentifier, subject) != 0) {
		r = 5;
		goto check;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, subject);
	subject = NULL;
	MlnImpl_81038E90(x509);
	x509 = NULL;
	r = MlnImpl_81068A8C(service->exchange, firstChain[0], secondChain[0]);
check:
	MlnBufferDestroy(MLN_HEAP_GENERAL, subject);
	if (x509 != NULL)
		MlnImpl_81038E90(x509);
	if (r != 0)
		ret = r == 2 ? 2 : 0x2b;
	else
		ret = 0;
out:
	MlnServiceChainFree(secondChain, secondCount);
	MlnServiceChainFree(firstChain, firstCount);
	MlnBufferDestroy(MLN_HEAP_GENERAL, nameIdentifier);
	while (sessions-- > 0)
		MlnImpl_8104A558();
	return ret;
}

/* keep two more messages (each when given and not empty) in a service handle */
unsigned int MlnImpl_81050110(int handle, const MlnBuffer *first, const MlnBuffer *second)
{
	MlnCoreServiceHandle *service = NULL;
	MlnHandleTable *table;
	MlnBuffer *copy;
	unsigned int ret;

	table = MlnServiceHandleTable(MlnImpl_81006EAC());
	if (table == NULL)
		return 2;
	ret = MlnImpl_81069FE8(handle, table, &service);
	if (ret != 0)
		return ret;
	if (service == NULL)
		return 3;
	if (first != NULL && first->size != 0 && first->data != NULL) {
		copy = MlnBufferCreate(MLN_HEAP_GENERAL, first->size, first->data);
		service->messages[2] = copy;
		if (copy == NULL)
			return 2;
	}
	if (second != NULL && second->size != 0 && second->data != NULL) {
		copy = MlnBufferCreate(MLN_HEAP_GENERAL, second->size, second->data);
		service->messages[3] = copy;
		if (copy == NULL)
			return 2;
		return 0;
	}
	return ret;
}

/* send a request of a kind through a service handle of kind 8 */
unsigned int MlnImpl_81050210(int handle, int kind, MlnXmlNode **document,
			      const MlnBuffer *extra, MlnBlob *out)
{
	MlnCoreServiceHandle *service = NULL;
	MlnBufferGroups *groups = NULL;
	MlnBuffer *response = NULL;
	MlnHandleTable *table;
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
	if (service == NULL || service->state != 8 << 8 || document == NULL || out == NULL) {
		ret = 3;
		goto out;
	}
	if (extra == NULL && kind == 1) {
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
	ret = MlnImpl_81082DC0(MlnImpl_81067CA4(service->exchange, kind, &groups, extra, document,
						&response));
	if (ret != 0)
		goto out;
	out->buffer = response;
	out->size = response->size;
	response = NULL;
out:
	MlnImpl_81042A88(groups);
	MlnBufferDestroy(MLN_HEAP_GENERAL, response);
	while (sessions-- > 0)
		MlnImpl_8104A558();
	return ret;
}

/* take the reply to a request of a kind through a service handle of kind 8 */
unsigned int MlnImpl_81050378(int handle, int kind, const char *name,
			      const MlnBuffer *message, MlnXmlNode **document)
{
	MlnCoreServiceHandle *service = NULL;
	MlnHandleTable *table;
	unsigned int ret;

	table = MlnServiceHandleTable(MlnImpl_81006EAC());
	if (table == NULL)
		return 2;
	ret = MlnImpl_81069FE8(handle, table, &service);
	if (ret != 0)
		return ret;
	if (service == NULL || service->state != 8 << 8)
		return 3;
	if (message == NULL || document == NULL)
		return 4;
	if (message->size == 0 || message->data == NULL)
		return 0x24;
	if (MlnImpl_8104A448() != 0)
		return 7;

	ret = MlnImpl_81068060(service->exchange, kind, name, message, document);
	switch (ret) {
	case 0:
	case 2:
		break;
	case 6:
		ret = 0x44;
		break;
	case 0x23000001:
	case 0x23000002:
	case 0x2300000b:
		ret = 0x24;
		break;
	case 0x23000007:
		ret = 0x29;
		break;
	default:
		ret = 7;
		break;
	}
	MlnImpl_8104A558();
	return ret;
}

/* map an error of the store or the core, others to fallback */
unsigned int MlnImpl_810504EC(unsigned int r, unsigned int fallback)
{
	switch (r) {
	case 0:
		return 0;
	case 2:
		return 2;
	case 3:
		return 4;
	case 4:
		return 3;
	case 6:
		return 0x44;
	case 0x230a0002:
		return 0x10;
	case 0x230a0004:
		return 9;
	case 0x23100005:
		return 0x17;
	case 0x23100006:
		return 9;
	case 0x23110004:
		return 9;
	case 0x23110005:
		return 0x10;
	default:
		return fallback;
	}
}

/* the registration session handle table, made on first use */
static inline __attribute__((always_inline)) MlnHandleTable *MlnRegistrationHandleTable(MlnShared *shared)
{
	MlnHandleTable *table = shared->buffers[7];

	if (table == NULL) {
		table = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*table));
		if (table == NULL)
			return NULL;
		table->initialized = 0;
		MlnMemset(table->slots, 0, sizeof(table->slots));
		shared->buffers[7] = table;
	}
	return table;
}

/* close a session of MlnImpl_81050778 */
int MlnImpl_810505B8(int handle)
{
	MlnRegistrationSession *session = NULL;
	MlnHandleTable *table;
	unsigned int r;
	int ret;

	table = MlnRegistrationHandleTable(MlnImpl_81006EAC());
	if (table == NULL)
		return 2;
	ret = MlnImpl_81069FE8(handle, table, &session);
	if (ret != 0 || session == NULL)
		return ret;
	MlnBufferDestroy(MLN_HEAP_GENERAL, session->buffer);
	MlnBufferDestroy(MLN_HEAP_GENERAL, session->token);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, session->node);
	MlnRightsInfoFree(MLN_HEAP_GENERAL, session->rights);
	ret = MlnImpl_8104DAA4(session->service);
	r = MlnImpl_8104A9CC(session->core);
	if (r != 0)
		ret = r;
	r = MlnImpl_81064DD4(session->context);
	if (r != 0)
		ret = MlnImpl_810504EC(r, 7);
	MlnFree(MLN_HEAP_GENERAL, session);
	if (MlnImpl_8106A048(handle, NULL, table) != 0)
		ret = 7;
	return ret;
}

/*
 * Open a session with a registration service. Without a token, the device's own certificates
 * and token are given back (the leaf certificates, the token and the other buffer of the core's
 * groups); with the caller's (which the session takes), the service is given the leaf
 * certificates of the two chains given, checked against the token.
 */
unsigned int MlnImpl_81050778(int *handle, MlnServiceList *list, unsigned int *firstCount,
			      MlnBuffer ***firsts, unsigned int *secondCount, MlnBuffer ***seconds,
			      MlnBuffer **third, MlnBuffer **fourth)
{
	MlnRegistrationSession *session = NULL;
	MlnRegistrationSession *opened;
	MlnBufferGroups *info = NULL;
	MlnBuffer *firstLeaf = NULL;
	MlnBuffer *secondLeaf = NULL;
	MlnBuffer **chain;
	MlnBuffer *encoded;
	MlnBuffer *firstCopy, *secondCopy;
	MlnX509Certificates certificates;
	MlnHandleTable *table;
	MlnBuffer **items;
	MlnBuffer *token;
	unsigned int count;
	unsigned int ret;
	unsigned int r;
	unsigned int i;
	int chainCount;
	int id;
	int k;

	if (handle == NULL || list == NULL || firstCount == NULL || fourth == NULL ||
	    firsts == NULL || seconds == NULL || secondCount == NULL || third == NULL) {
		ret = 4;
		goto fail;
	}
	session = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*session));
	if (session == NULL) {
		ret = 2;
		goto fail;
	}
	MlnMemset(session, 0, sizeof(*session));
	table = MlnRegistrationHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto fail;
	}
	ret = MlnImpl_81069F10(table);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_81069F68(session, table, &id);
	if (ret != 0)
		goto fail;
	opened = session;

	ret = MlnImpl_8104D5E0(8, list, &opened->service);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8104A8D8(&opened->core);
	if (ret != 0)
		goto out;
	r = MlnImpl_81064D8C(&opened->context);
	if (r != 0) {
		ret = MlnImpl_810504EC(r, 7);
		goto out;
	}

	token = *fourth;
	if (token == NULL) {
		r = MlnImpl_81064E18(&info);
		if (r != 0) {
			ret = r == 0x230a0002 ? 0xf : MlnImpl_810504EC(r, 7);
			goto out;
		}
		*firstCount = info->lists[1].count;
		*firsts = MlnAllocate(MLN_HEAP_GENERAL, *firstCount * sizeof(**firsts));
		if (*firsts == NULL)
			goto nomem;
		MlnMemset(*firsts, 0, *firstCount * sizeof(**firsts));
		for (i = 0; i < *firstCount; i++) {
			items = *firsts;
			items[i] = MlnBufferCreate(MLN_HEAP_GENERAL, info->lists[1].items[i]->size,
						   info->lists[1].items[i]->data);
			if ((*firsts)[i] == NULL)
				goto nomem;
		}
		/* the second list's count is the first's, as Sony's code reads it */
		*secondCount = info->lists[1].count;
		*seconds = MlnAllocate(MLN_HEAP_GENERAL, *secondCount * sizeof(**seconds));
		if (*seconds == NULL)
			goto nomem;
		MlnMemset(*seconds, 0, *secondCount * sizeof(**seconds));
		for (i = 0; i < *secondCount; i++) {
			items = *seconds;
			items[i] = MlnBufferCreate(MLN_HEAP_GENERAL, info->lists[0].items[i]->size,
						   info->lists[0].items[i]->data);
			if ((*seconds)[i] == NULL)
				goto nomem;
		}
		*third = MlnBufferCreate(MLN_HEAP_GENERAL, info->last->size, info->last->data);
		if (*third == NULL)
			goto nomem;
		*fourth = MlnBufferCreate(MLN_HEAP_GENERAL, info->first->size, info->first->data);
		if (*fourth == NULL)
			goto nomem;
		goto done;
	}

	if (*firsts == NULL || *seconds == NULL || *third == NULL) {
		ret = 4;
		goto out;
	}
	opened->token = token;
	*fourth = NULL;
	encoded = NULL;
	firstCopy = NULL;
	secondCopy = NULL;
	chain = NULL;
	chainCount = 0;
	certificates.count = 0;
	certificates.items = NULL;
	count = *firstCount;
	r = MlnImpl_81059600((*firsts)[0], &chain, &chainCount);
	if (r == 0) {
		if (count != 1 || *secondCount != 1) {
			ret = 4;
			goto check;
		}
		firstCopy = MlnBufferCreate(MLN_HEAP_GENERAL, (*firsts)[0]->size, (*firsts)[0]->data);
		secondCopy = MlnBufferCreate(MLN_HEAP_GENERAL, (*seconds)[0]->size,
					     (*seconds)[0]->data);
		if (firstCopy == NULL || secondCopy == NULL) {
			ret = 2;
			goto check;
		}
	} else if ((r & 0xffff0000) == 0x22000000) {
		/* not one certificate: chains, encoded */
		if (count == 0) {
			ret = 4;
			goto check;
		}
		ret = MlnImpl_81082E44(count, *firsts, &certificates);
		if (ret != 0)
			goto check;
		r = MlnImpl_8103A888(&certificates, &encoded);
		if (r == 0)
			r = MlnImpl_81026814(MLN_HEAP_GENERAL, encoded, 0, 0, &firstCopy);
		if (r != 0) {
			ret = MlnImpl_810504EC(r, 7);
			goto check;
		}
		MlnBufferDestroy(MLN_HEAP_GENERAL, encoded);
		MlnFree(MLN_HEAP_GENERAL, certificates.items);
		encoded = NULL;
		certificates.count = 0;
		certificates.items = NULL;
		if (*secondCount == 0 || *seconds == NULL) {
			ret = 4;
			goto check;
		}
		ret = MlnImpl_81082E44(*secondCount, *seconds, &certificates);
		if (ret != 0)
			goto check;
		r = MlnImpl_8103A888(&certificates, &encoded);
		if (r == 0)
			r = MlnImpl_81026814(MLN_HEAP_GENERAL, encoded, 0, 0, &secondCopy);
		if (r != 0) {
			ret = MlnImpl_810504EC(r, 7);
			goto check;
		}
	} else {
		ret = MlnImpl_810504EC(r, 7);
		if (ret != 0)
			goto check;
	}
	ret = 0;
	firstLeaf = firstCopy;
	secondLeaf = secondCopy;
	firstCopy = NULL;
	secondCopy = NULL;
check:
	MlnBufferDestroy(MLN_HEAP_GENERAL, encoded);
	MlnBufferDestroy(MLN_HEAP_GENERAL, firstCopy);
	MlnBufferDestroy(MLN_HEAP_GENERAL, secondCopy);
	MlnFree(MLN_HEAP_GENERAL, certificates.items);
	if (chain != NULL) {
		for (k = 0; k < chainCount; k++)
			MlnBufferDestroy(MLN_HEAP_GENERAL, chain[k]);
		MlnFree(MLN_HEAP_GENERAL, chain);
	}
	if (ret != 0)
		goto out;
	ret = MlnImpl_8104FC70(opened->service, firstLeaf->size, firstLeaf->data, secondLeaf->size,
			       secondLeaf->data, (*third)->size, (*third)->data);
	if (ret != 0)
		goto out;
done:
	*handle = id;
	ret = 0;
	goto out;

nomem:
	ret = 2;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, firstLeaf);
	MlnBufferDestroy(MLN_HEAP_GENERAL, secondLeaf);
	if (ret != 0)
		MlnImpl_810505B8(id);
	MlnImpl_81042A88(info);
	return ret;

fail:
	MlnBufferDestroy(MLN_HEAP_GENERAL, NULL);
	MlnBufferDestroy(MLN_HEAP_GENERAL, NULL);
	MlnFree(MLN_HEAP_GENERAL, session);
	/* Sony's code closes the session even when none was opened */
	MlnImpl_810505B8(id);
	MlnImpl_81042A88(info);
	return ret;
}

/*
 * Make a registration session's request from the data given and send it; the request is kept
 * in the session.
 */
unsigned int MlnImpl_81050E58(int handle, const MlnBuffer *data, MlnBlob *out)
{
	MlnRegistrationSession *session = NULL;
	MlnXmlNode *document = NULL;
	MlnBuffer *request = NULL;
	MlnHandleTable *table;
	unsigned int ret;
	unsigned int r;

	if (out == NULL || data == NULL || data->size == 0 || data->data == NULL) {
		ret = 4;
		goto out;
	}
	table = MlnRegistrationHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069FE8(handle, table, &session);
	if (ret != 0)
		goto out;
	if (session == NULL) {
		ret = 3;
		goto out;
	}
	r = MlnImpl_81064E68(&request);
	if (r == 0)
		r = MlnImpl_81064EC0(session->context, request, data, &document);
	if (r != 0) {
		ret = MlnImpl_810504EC(r, 7);
		goto out;
	}
	ret = MlnImpl_81050210(session->service, 1, &document, session->token, out);
	if (ret != 0)
		goto out;
	if (session->buffer != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, session->buffer);
	session->buffer = request;
	request = NULL;
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
	MlnBufferDestroy(MLN_HEAP_GENERAL, request);
	return ret;
}

/*
 * Take a registration session's license transfer reply: the license is added to the session's
 * core handle and its transfer action run; when the license's first obligation carries a
 * ValidityAfterFirstUse parameter, the service is told of it. Errors 9, 0xf and 0x44 also
 * reset the store (MlnImpl_810485B4).
 */
unsigned int MlnImpl_81051084(int handle, const MlnBuffer *data, MlnBlob *first, MlnBlob *second)
{
	MlnRegistrationSession *session = NULL;
	MlnXmlNode *document = NULL;
	MlnXmlNode *transferNode = NULL;
	MlnXmlNode *parameterNode = NULL;
	MlnXmlNode *reply = NULL;
	MlnBuffer *request = NULL;
	MlnBuffer *token = NULL;
	MlnBuffer *pair[2] = { NULL, NULL };
	MlnBuffer *transfer[2] = { NULL, NULL };
	MlnBlob response = { NULL, 0 };
	MlnRightsInfo *info = NULL;
	MlnBuffer *rights = NULL;
	MlnObligation *obligation;
	MlnParameter *parameter;
	MlnValueList *parameters;
	MlnHandleTable *table;
	void *agent = NULL;
	void *context = NULL;
	unsigned int ret;
	unsigned int r;
	int failed = 1;
	int count;
	int i;

	if (data == NULL || data->size == 0 || data->data == NULL ||
	    first == NULL || second == NULL) {
		ret = 4;
		goto out;
	}
	table = MlnRegistrationHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069FE8(handle, table, &session);
	if (ret != 0)
		goto fail;
	if (session == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81050378(session->service, 1, "LicenseTransferRequestPayload", data, &document);
	if (ret != 0)
		goto fail;
	r = MlnImpl_810651F4(document, &request, &token);
	if (r != 0)
		goto map;
	ret = MlnImpl_8104AB18(session->core, token->size, token->data, second);
	if (ret != 0)
		goto fail;
	r = MlnImpl_8106632C(session->context, token);
	if (r != 0)
		goto map;
	r = MlnImpl_81066B84(request, pair, &transferNode);
	if (r != 0)
		goto map;
	transfer[0] = pair[0];
	transfer[1] = pair[1];
	if (transferNode != NULL) {
		ret = MlnImpl_8104AE1C(session->core, transferNode);
		if (ret != 0)
			goto fail;
	}
	ret = MlnImpl_8104B4C4(session->core, 2, transfer, &response, second);
	if (ret != 0)
		goto fail;
	rights = response.buffer;
	response.buffer = NULL;
	if (rights == NULL) {
		ret = 7;
		goto out;
	}
	r = MlnRightsInfoParse(MLN_HEAP_GENERAL, rights->size, rights->data, &info);
	if (r == 0)
		r = MlnEsbCheckGlobalFlags(MLN_HEAP_GENERAL, (const MlnEsb *)info);
	if (r != 0)
		goto map;
	if (info == NULL) {
		ret = 4;
		goto fail;
	}

	obligation = &info->obligations[0];
	parameters = obligation->parameters;
	count = parameters->count;
	for (i = 0; i != count; i++) {
		parameter = parameters->items[i]->data.parameter;
		if (MlnStrncmp((const char *)parameter->name->data, "ValidityAfterFirstUse",
			       21) == 0)
			break;
	}
	if (i == count || count <= i) {
		ret = 0x42;
		goto fail;
	}
	parameter = obligation->parameters->items[i]->data.parameter;
	r = MlnImpl_8106668C(parameter->name, parameter->value->data.buffer, &parameterNode);
	if (r == 0)
		r = MlnImpl_81066548(session->context, &agent, &context);
	if (r == 0)
		r = MlnImpl_81065430(obligation->data.agentId, obligation->contextId,
				     obligation->controlId, parameterNode, agent, context, &reply);
	if (r != 0)
		goto map;
	ret = MlnImpl_81050210(session->service, 2, &reply, session->token, first);
	if (ret != 0)
		goto fail;
	session->buffer = request;
	request = NULL;
	session->rights = info;
	info = NULL;
	failed = 0;
	goto out;

map:
	ret = MlnImpl_810504EC(r, 7);
fail:
	failed = ret != 0x1d && ret != 0;
	if (ret == 9 || ret == 0x44 || ret == 0xf)
		MlnImpl_810485B4();
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
	MlnBufferDestroy(MLN_HEAP_GENERAL, token);
	MlnBufferDestroy(MLN_HEAP_GENERAL, request);
	MlnBufferDestroy(MLN_HEAP_GENERAL, pair[0]);
	MlnBufferDestroy(MLN_HEAP_GENERAL, pair[1]);
	if (response.buffer != NULL) {
		rights = response.buffer;
		response.buffer = NULL;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, rights);
	if (failed && second->buffer != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, second->buffer);
		second->buffer = NULL;
	}
	MlnRightsInfoFree(MLN_HEAP_GENERAL, info);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, reply);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, parameterNode);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, transferNode);
	return ret;
}

/*
 * Take a registration session's license transfer response: the keys it carries are stored and
 * the node it gives kept in the session; the service is told, and the core's state returned.
 */
unsigned int MlnImpl_8105152C(int handle, const MlnBuffer *data, MlnBlob *first, MlnBlob *second)
{
	MlnRegistrationSession *session = NULL;
	MlnXmlNode *document = NULL;
	MlnXmlNode *payload = NULL;
	MlnXmlNode *reply = NULL;
	MlnArray *keys = NULL;
	MlnArray *pending = NULL;
	MlnBuffer *state = NULL;
	MlnHandleTable *table;
	unsigned int ret;
	unsigned int r;

	if (data == NULL || data->size == 0 || data->data == NULL || first == NULL) {
		ret = 4;
		goto out;
	}
	table = MlnRegistrationHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069FE8(handle, table, &session);
	if (ret != 0)
		goto out;
	if (session == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81050378(session->service, 2, "LicenseTransferResponsePayload", data,
			       &document);
	if (ret != 0)
		goto out;
	r = MlnImpl_81065848(document, &payload);
	if (r == 0)
		r = MlnImpl_8104570C(&keys);
	if (r != 0)
		goto map;
	r = MlnImpl_81068C90(payload, keys, 0xf, &session->node, &pending);
	if (r != 0)
		goto map;
	if (session->node == NULL) {
		ret = 7;
		goto out;
	}
	r = MlnImpl_810456B0((const MlnStoreKeyPair *)pending);
	if (r == 0)
		r = MlnImpl_81043A60(1, &state);
	if (r == 0)
		r = MlnImpl_81066888(session->context, session->node, &reply);
	if (r != 0)
		goto map;
	ret = MlnImpl_81050210(session->service, 1, &reply, session->token, first);
	if (ret != 0)
		goto out;
	second->buffer = state;
	second->size = state->size;
	state = NULL;
	goto out;

map:
	ret = MlnImpl_810504EC(r, 7);
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, payload);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, reply);
	MlnArrayDestroy(MLN_HEAP_GENERAL, keys);
	MlnArrayDestroy(MLN_HEAP_GENERAL, pending);
	MlnBufferDestroy(MLN_HEAP_GENERAL, state);
	return ret;
}

/* free an array of count buffers */
static inline void MlnServiceBuffersFree(MlnBuffer **buffers, int count)
{
	int i;

	for (i = 0; i < count; i++)
		MlnBufferDestroy(MLN_HEAP_GENERAL, buffers[i]);
	MlnFree(MLN_HEAP_GENERAL, buffers);
}

/*
 * Complete a registration session's license transfer: the payload of the service's request
 * (a big-endian number before it) is given to the license's callback; with the answer and
 * the keyed operation's buffers, made with the public key in the ScubaKeys of the session's
 * token, the reply is sent to the service.
 */
unsigned int MlnImpl_8105183C(int handle, const MlnBuffer *data, MlnBlob *first, MlnBlob *second)
{
	MlnRegistrationSession *session = NULL;
	MlnXmlNode *document = NULL;
	MlnXmlNode *reply = NULL;
	MlnXmlNode *unused = NULL;
	MlnBuffer *payload = NULL;
	MlnBuffer *answer = NULL;
	MlnBuffer *publicKey = NULL;
	MlnBuffer *pair = NULL;
	MlnBuffer **firsts = NULL;
	MlnBuffer **seconds = NULL;
	MlnBuffer **thirds = NULL;
	MlnBlob response = { NULL, 0 };
	MlnOctopusNode *node;
	MlnOctopusExtension *extension;
	MlnOctopusArray *extensions;
	MlnOctopusArray *publicKeys;
	MlnOctopusKeyPair *keyPair;
	MlnOctopusKey key;
	MlnRightsInfo *info;
	MlnHandleTable *table;
	const MlnBuffer *token;
	unsigned char *blob = NULL;
	unsigned int size;
	unsigned int ret;
	unsigned int r;
	int value = 0;
	int count = 0;
	int i;

	if (data == NULL || data->size == 0 || data->data == NULL) {
		ret = 4;
		goto out;
	}
	table = MlnRegistrationHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069FE8(handle, table, &session);
	if (ret != 0)
		goto out;
	if (session == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81050378(session->service, 1, "LicenseTransferRequestPayload", data, &document);
	if (ret != 0)
		goto out;
	r = MlnImpl_81066A28(document, &value, &payload);
	if (r != 0) {
		ret = MlnImpl_810504EC(r, 7);
		goto out;
	}
	if (payload == NULL || payload->size == 0 || payload->data == NULL) {
		ret = 4;
		goto out;
	}
	size = payload->size + 8;
	blob = MlnAllocate(MLN_HEAP_GENERAL, size);
	if (blob == NULL) {
		ret = 2;
		goto out;
	}
	blob[0] = 0;
	blob[1] = 0;
	blob[2] = 0;
	blob[3] = 0;
	blob[4] = value >> 24;
	blob[5] = value >> 16;
	blob[6] = value >> 8;
	blob[7] = value;
	MlnMemcpy(blob + 8, payload->data, payload->size);
	info = session->rights;
	if (info == NULL) {
		ret = 4;
		goto out;
	}
	if (info->callbackCount != 1) {
		ret = 0x42;
		goto out;
	}
	ret = MlnImpl_8104C32C(session->core,
			       (const MlnServiceObject *)&info->callbacks[0].routineType, size, blob,
			       &response, second);
	answer = response.buffer;
	response.buffer = NULL;
	if (ret == 0x1d)
		goto send;
	if (ret != 0)
		goto out;

	node = NULL;
	token = session->buffer;
	if (token == NULL || token->size == 0 || token->data == NULL) {
		ret = 4;
		goto unload;
	}
	r = MlnImpl_810429F8(token, 1, &node);
	if (r != 0) {
		ret = MlnImpl_810504EC(r, 9);
		MlnImpl_8103B8F8(node);
		if (ret != 0)
			goto out;
		goto keyed;
	}
	if (node == NULL || node->extensions == NULL || node->extensions->count <= 0) {
		ret = 7;
		goto unload;
	}
	extensions = node->extensions;
	for (i = 0;; i++) {
		extension = extensions->items[i];
		if (extension == NULL) {
			ret = 7;
			goto unload;
		}
		if (extension->type == MLN_OCTOPUS_EXTENSION_SCUBA)
			break;
		if (i + 1 == extensions->count) {
			ret = 7;
			goto unload;
		}
	}
	publicKeys = extension->scubaKeys->publicKeys;
	if (publicKeys == NULL) {
		ret = 7;
		goto unload;
	}
	if (publicKeys->count != 1) {
		ret = 4;
		goto unload;
	}
	keyPair = publicKeys->items[0];
	if (keyPair == NULL) {
		ret = 7;
		goto unload;
	}
	key = keyPair->key;
	publicKey = MlnBufferCreate(MLN_HEAP_GENERAL, key.data->size, key.data->data);
	if (publicKey == NULL) {
		ret = 2;
		goto unload;
	}
	pair = MlnBufferCreate(MLN_HEAP_GENERAL, keyPair->pair->size, keyPair->pair->data);
	if (pair == NULL) {
		ret = 2;
		goto unload;
	}
	MlnImpl_8103B8F8(node);
keyed:
	ret = MlnImpl_8104C130(session->core, publicKey, &count, &firsts, &seconds, &thirds);
	if (ret != 0)
		goto out;
send:
	r = MlnImpl_81065920(session->context, count, firsts, seconds, thirds, pair, answer,
			     &reply);
	if (r != 0) {
		ret = MlnImpl_810504EC(r, 7);
		goto out;
	}
	ret = MlnImpl_81050210(session->service, 2, &reply, session->token, first);
	goto out;

unload:
	MlnImpl_8103B8F8(node);
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, unused);
	MlnBufferDestroy(MLN_HEAP_GENERAL, payload);
	MlnFree(MLN_HEAP_GENERAL, blob);
	MlnBufferDestroy(MLN_HEAP_GENERAL, answer);
	if (ret != 0 && second->buffer != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, second->buffer);
		second->buffer = NULL;
	}
	MlnXmlNodeFree(MLN_HEAP_GENERAL, reply);
	MlnBufferDestroy(MLN_HEAP_GENERAL, publicKey);
	MlnBufferDestroy(MLN_HEAP_GENERAL, pair);
	MlnServiceBuffersFree(firsts, count);
	MlnServiceBuffersFree(seconds, count);
	MlnServiceBuffersFree(thirds, count);
	return ret;
}

/*
 * Take a registration session's license transfer teardown: the ContentKeys of the bundle given
 * are replaced by the payload's, and the first use of the new bundle is recorded; returns the
 * new bundle and the core's state.
 */
unsigned int MlnImpl_81051F2C(int handle, const MlnBuffer *data, const MlnBuffer *extra,
			      MlnBlob *first, MlnBlob *second)
{
	MlnRegistrationSession *session;
	MlnXmlNode *bundle = NULL;
	MlnXmlNode *node = NULL;
	MlnXmlNode *document = NULL;
	MlnXmlNodeList *keys = NULL;
	MlnXmlNodeList *replacements = NULL;
	MlnBuffer *text = NULL;
	MlnBuffer *state = NULL;
	MlnHandleTable *table;
	unsigned int ret;
	unsigned int r;
	int refused;
	int i;

	if (first == NULL || data == NULL || data->size == 0 || data->data == NULL ||
	    extra == NULL || extra->size == 0 || extra->data == NULL) {
		ret = 4;
		goto out;
	}
	r = MlnXmlParse(MLN_HEAP_GENERAL, data->size, data->data, &bundle);
	if (r == 0)
		r = MlnXmlFindNode(MLN_HEAP_GENERAL, bundle, "Bundle", NULL, &node);
	if (r == 0)
		r = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "ContentKey", NULL, &keys);
	if (r != 0) {
		ret = MlnImpl_810504EC(r, 0x17);
		goto out;
	}

	session = NULL;
	table = MlnRegistrationHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069FE8(handle, table, &session);
	if (ret != 0)
		goto out;
	if (session == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81050378(session->service, 2, "LicenseTransferTeardownPayload", extra,
			       &document);
	if (ret != 0)
		goto out;
	r = MlnImpl_81066588(session->context, document, &refused);
	if (r != 0)
		goto map;
	if (refused != 0) {
		ret = 0x42;
		goto out;
	}
	r = MlnXmlFindNodes(MLN_HEAP_GENERAL, document,
			    "descendant-or-self::LicenseTransferTeardownPayload/ContentKeyList/"
			    "ContentKey", NULL, &replacements);
	if (r != 0)
		goto map;
	if (keys->count <= 0 || keys->count != replacements->count) {
		ret = 0x17;
		goto out;
	}
	for (i = 0; i < keys->count; i++) {
		r = MlnXmlNodeRemoveChild(MLN_HEAP_GENERAL, node, keys->nodes[i]);
		if (r == 0)
			r = MlnXmlNodeAppendChild(MLN_HEAP_GENERAL, node, replacements->nodes[i], 0);
		if (r != 0)
			goto map;
	}
	r = MlnXmlNodeToText(MLN_HEAP_GENERAL, bundle, &text);
	if (r != 0)
		goto map;
	ret = MlnImpl_8104D208(NULL, text);
	if (ret != 0)
		goto out;
	r = MlnImpl_81043A60(1, &state);
	if (r != 0)
		goto map;
	first->buffer = text;
	first->size = text->size;
	text = NULL;
	second->buffer = state;
	second->size = state->size;
	state = NULL;
	goto out;

map:
	ret = MlnImpl_810504EC(r, 7);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, state);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, bundle);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, node);
	MlnXmlListFree(MLN_HEAP_GENERAL, keys);
	MlnXmlListFree(MLN_HEAP_GENERAL, replacements);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	return ret;
}
