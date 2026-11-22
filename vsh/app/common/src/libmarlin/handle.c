/*
 * The core context and the handles that name it: each open reads the
 * store into a context, and the handle table maps handles to their
 * MlnCoreHandle (0x8104a560-)
 */

#include "mln_internal.h"

/*
 * Open the core context: the root node, the SAML assertion, the stored
 * links and the key pair, each handed to the context. A missing node, an
 * assertion the store does not have and an empty link list are not errors;
 * a link the store cannot parse resets *state to the first kind and gives
 * 0xa.
 */
unsigned int MlnImpl_8104A560(MlnCoreContext **context, MlnBuffer **state)
{
	MlnArray *pair = NULL;
	MlnOctopusNode *node;
	MlnOctopusSamlAssertion *assertion;
	MlnStoreLink *links;
	MlnOctopusArray *array;
	MlnBuffer *entry;
	MlnOctopusLink *link;
	MlnCoreContext *ctx;
	unsigned int ret;
	unsigned int r;
	int count;
	int complete;
	int i;

	if (*context != NULL)
		return 0;

	if (MlnImpl_81062024(context) != 0) {
		ret = 2;
		goto error;
	}

	node = NULL;
	ctx = *context;
	ret = MlnImpl_81043B94(&node);
	if (ret == 0) {
		ret = MlnImpl_81062164(ctx, node);
		if (ret == 0)
			goto assertion;
		if (ret != 2)
			ret = 7;
	} else if (ret == 2) {
		ret = 2;
	} else if (ret == 0x230A0004) {
		ret = 9;
	} else if (ret == 6) {
		ret = 0x44;
	} else {
		ret = 0x10;
	}
	if (node != NULL)
		MlnImpl_8103B8F8(node);
	if (ret != 0x10)
		goto error;

assertion:
	ctx = *context;
	assertion = NULL;
	ret = MlnImpl_810644A0(&assertion);
	if (ret == 0) {
		ret = MlnImpl_810623F0(ctx, assertion);
		if (ret == 0) {
			assertion = NULL;
			MlnImpl_8103CD0C(assertion);
			goto links;
		}
	}
	if (ret != 0x230A0002 && ret != 0x230A0004 &&
	    ret != 0x230A0006 && ret != 0x230A0007) {
		if (ret != 2)
			ret = ret == 6 ? 0x44 : 7;
		MlnImpl_8103CD0C(assertion);
		goto error;
	}
	MlnImpl_8103CD0C(assertion);

links:
	array = NULL;
	ctx = *context;
	entry = NULL;
	link = NULL;
	links = NULL;
	ret = MlnImpl_8104411C(&count, &links);
	if (ret != 0) {
		if (ret == 2)
			goto nomem;
		ret = ret == 6 ? 0x44 : 0x12;
		goto out;
	}
	if (count == 0) {
		array = NULL;
		ret = 0x12;
		goto out;
	}
	array = MlnImpl_8103B758(count);
	if (array == NULL)
		goto nomem;
	if (count > 0) {
		complete = 1;
		for (i = 0; i < count; i++) {
			ret = MlnImpl_810442D4(&links, &entry);
			if (ret != 0) {
				if (ret != 2)
					ret = ret == 6 ? 0x44 : 7;
				goto out;
			}
			r = MlnImpl_810422E8(entry, &link);
			if (r == 0) {
				MlnBufferDestroy(MLN_HEAP_GENERAL, entry);
				entry = NULL;
				array->items[i] = link;
				link = NULL;
			} else if (r == 2) {
				ret = r;
				goto out;
			} else {
				complete = 0;
				MlnImpl_81043F1C(entry);
				MlnBufferDestroy(MLN_HEAP_GENERAL, entry);
				entry = NULL;
			}
		}
		if (!complete) {
			MlnImpl_81043A60(1, state);
			ret = 0xA;
			goto out;
		}
	}
	ret = MlnImpl_81062224(ctx, array);
	if (ret == 0)
		array = NULL;
	else
		ret = 7;
	goto out;
nomem:
	array = NULL;
	ret = 2;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, entry);
	if (link != NULL)
		MlnImpl_8103BBA0(link);
	if (array != NULL)
		MlnImpl_8103C57C(array, 5);
	MlnImpl_8104435C(links);
	if (ret != 0x12 && ret != 0)
		goto error;

	ret = MlnImpl_8104570C(&pair);
	if (ret == 0) {
		ret = MlnImpl_81062450(*context, pair);
		if (ret == 0)
			goto done;
		if (ret != 2)
			ret = 7;
	} else if (ret != 2) {
		ret = ret == 6 ? 0x44 : 7;
	}

error:
	if (*context != NULL) {
		MlnImpl_81062060(*context);
		*context = NULL;
	}
done:
	MlnArrayDestroy(MLN_HEAP_GENERAL, pair);
	return ret;
}

/* the handle table, made on first use */
static inline __attribute__((always_inline)) MlnHandleTable *MlnCoreHandleTable(MlnShared *shared)
{
	MlnHandleTable *table = shared->buffers[4];

	if (table == NULL) {
		table = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*table));
		if (table == NULL)
			return NULL;
		table->initialized = 0;
		MlnMemset(table->slots, 0, sizeof(table->slots));
		shared->buffers[4] = table;
	}
	return table;
}

/* open a core handle: *handle names a new MlnCoreHandle */
unsigned int MlnImpl_8104A8D8(int *handle)
{
	MlnCoreHandle *core = NULL;
	MlnHandleTable *table;
	unsigned int ret;
	int id;

	if (handle == NULL) {
		ret = 4;
		goto fail;
	}
	if (MlnImpl_8104A448() != 0) {
		ret = 7;
		goto fail;
	}

	core = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*core));
	if (core == NULL) {
		ret = 2;
		goto end;
	}
	MlnMemset(core, 0, sizeof(*core));
	core->kind = 4;
	table = MlnCoreHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto end;
	}
	ret = MlnImpl_81069F10(table);
	if (ret == 0) {
		ret = MlnImpl_81069F68(core, table, &id);
		if (ret == 0) {
			*handle = id;
			core = NULL;
		}
	}
end:
	MlnFree(MLN_HEAP_GENERAL, core);
	MlnImpl_8104A558();
	return ret;
fail:
	MlnFree(MLN_HEAP_GENERAL, core);
	return ret;
}

/* close a core handle of MlnImpl_8104A8D8, writing its pending response */
int MlnImpl_8104A9CC(int handle)
{
	MlnCoreHandle *core = NULL;
	MlnHandleTable *table;
	int ret;

	if (MlnImpl_8104A448() != 0)
		return 7;

	table = MlnCoreHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto end;
	}
	ret = MlnImpl_81069FE8(handle, table, &core);
	if (ret != 0 || core == NULL)
		goto end;

	if (core->response == NULL || core->response->pending == 0 ||
	    MlnImpl_81045EE0(core->response->data) == 0) {
		MlnImpl_81069A9C(core->response);
		core->response = NULL;
	}
	if (core->bundle != NULL)
		MlnImpl_81040368(core->bundle);
	if (core->memory != NULL)
		MlnFree(MLN_HEAP_GENERAL, core->memory);
	if (core->context != NULL)
		MlnImpl_81062060(core->context);
	if (core->state != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, core->state);
	MlnFree(MLN_HEAP_GENERAL, core);
	ret = MlnImpl_8106A048(handle, NULL, table) != 0 ? 7 : 0;
end:
	MlnImpl_8104A558();
	return ret;
}

/*
 * Give a core handle its signed bundle: the context is opened from the
 * store, the bundle parsed and handed to it, and the store's pending
 * response read. A failure to open the context hands its state back.
 */
unsigned int MlnImpl_8104AB18(int handle, unsigned int size, const unsigned char *data,
			      MlnBlob *out)
{
	MlnOctopusSignedBundle *bundle = NULL;
	MlnCoreHandle *core = NULL;
	MlnBuffer *state = NULL;
	MlnBuffer *token = NULL;
	MlnCoreResponse *response;
	MlnBuffer *pending;
	MlnHandleTable *table;
	unsigned int ret;
	unsigned int r;
	int sessions = 0;

	table = MlnCoreHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069FE8(handle, table, &core);
	if (ret != 0)
		goto out;
	if (core == NULL) {
		ret = 3;
		goto out;
	}
	if (size == 0 || data == NULL) {
		ret = 4;
		goto out;
	}
	if (core->flags & MLN_CORE_HANDLE_BUNDLE) {
		ret = 0x19;
		goto out;
	}
	if (MlnImpl_8104A4B8() != 0) {
		ret = 7;
		goto out;
	}
	sessions = 1;

	ret = MlnImpl_8104A560(&core->context, &state);
	if (ret != 0) {
		if (out != NULL && state != NULL) {
			out->buffer = state;
			state = NULL;
			out->size = out->buffer->size;
		}
		goto out;
	}

	token = MlnBufferCreate(MLN_HEAP_GENERAL, size, data);
	if (token == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_810403F4(token, &bundle);
	if (ret != 0) {
		if (ret != 2)
			ret = 0x17;
		goto out;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, token);
	token = NULL;
	ret = MlnImpl_81062524(core->context, bundle->bundle);
	if (ret != 0) {
		if (ret != 2)
			ret = 0x17;
		goto out;
	}

	pending = NULL;
	response = NULL;
	r = MlnImpl_81045E74(&pending);
	if (r == 0) {
		if (pending != NULL) {
			r = MlnImpl_81069968(pending, 1, &response);
			if (r == 0)
				pending = NULL;
			else if (r == 2)
				goto nomem;
			else
				goto fail;
		}
		core->response = response;
		response = NULL;
		MlnImpl_81069A9C(response);
		MlnBufferDestroy(MLN_HEAP_GENERAL, pending);
		core->bundle = bundle;
		bundle = NULL;
		core->flags |= MLN_CORE_HANDLE_BUNDLE;
		goto out;
	}
	if (r == 2) {
nomem:
		ret = 2;
	} else if (r == 6) {
		ret = 0x44;
	} else {
fail:
		ret = 7;
	}
	MlnImpl_81069A9C(response);
	MlnBufferDestroy(MLN_HEAP_GENERAL, pending);

out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, token);
	MlnBufferDestroy(MLN_HEAP_GENERAL, state);
	if (bundle != NULL)
		MlnImpl_81040368(bundle);
	while (sessions > 0) {
		sessions--;
		MlnImpl_8104A558();
	}
	return ret;
}

/* set the attributes of a core handle's bundle from node's AttributeList */
unsigned int MlnImpl_8104AE1C(int handle, MlnXmlNode *node)
{
	MlnCoreHandle *core = NULL;
	MlnOctopusArray *attributes = NULL;
	MlnHandleTable *table;
	unsigned int ret;

	table = MlnCoreHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069FE8(handle, table, &core);
	if (ret != 0)
		goto out;
	if (core == NULL || core->context == NULL) {
		ret = 3;
		goto out;
	}
	if (node == NULL) {
		ret = 4;
		goto out;
	}
	ret = MlnImpl_81042D10(node, &attributes);
	if (ret != 0) {
		if (ret != 2)
			ret = 9;
		goto out;
	}
	if (core->bundle == NULL || core->bundle->bundle == NULL) {
		ret = 0x17;
		goto out;
	}
	core->bundle->bundle->attributes = attributes;
	attributes = NULL;
out:
	MlnImpl_8103C57C(attributes, MLN_OCTOPUS_ATTRIBUTE);
	return ret;
}

/* open a record handle of a core handle: *opened names it */
unsigned int MlnImpl_8104AF48(int handle, const MlnServiceRecordValue *record, int *opened)
{
	MlnCoreHandle *core = NULL;
	MlnCoreRecordHandle *rec;
	MlnHandleTable *table;
	unsigned char data[11];
	void *object;
	unsigned int ret;
	int id;

	table = MlnCoreHandleTable(MlnImpl_81006EAC());
	if (table == NULL)
		return 2;
	ret = MlnImpl_81069FE8(handle, table, &core);
	if (ret != 0)
		return ret;
	if (core == NULL)
		return 3;
	if (!(core->flags & MLN_CORE_HANDLE_BUNDLE))
		return 0x1b;
	if (core->context == NULL)
		return 3;
	if (core->flags & MLN_CORE_HANDLE_DONE)
		return 0x1c;
	if (record == NULL || record->buffer == NULL || opened == NULL)
		return 4;
	if (MlnImpl_8104A4B8() != 0)
		return 7;

	rec = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*rec));
	if (rec == NULL) {
		ret = 2;
		goto end;
	}
	MlnMemset(rec, 0, sizeof(*rec));
	if (MlnImpl_81061BBC(&object) != 0) {
		ret = 2;
		goto fail;
	}
	rec->object = object;
	rec->core = core;
	core->outcome = 0;
	MlnMemcpy(&rec->kind, &record->kind,
		  sizeof(*record) - __builtin_offsetof(MlnServiceRecordValue, kind));
	rec->buffer = MlnBufferCreate(MLN_HEAP_GENERAL, record->buffer->size, record->buffer->data);
	if (rec->buffer == NULL) {
		ret = 2;
		goto fail;
	}
	ret = MlnImpl_8106273C(core->context, rec->buffer, &rec->entry);
	if (ret != 0) {
		if (ret != 2)
			ret = 4;
		goto fail;
	}
	core->flags |= MLN_CORE_HANDLE_RECORD;

	if (rec->kind == MLN_SERVICE_RECORD_LONG) {
		MlnMemcpy(data, rec->tail, sizeof(rec->tail));
		data[8] = rec->head[2];
		data[9] = rec->head[0];
		data[10] = rec->head[1];
		if (MlnImpl_81061C1C(object, 2, data) != 0) {
			ret = 7;
			goto fail;
		}
	} else if (rec->kind == MLN_SERVICE_RECORD_SHORT) {
		data[0] = rec->head[0];
		data[1] = rec->head[1];
		data[2] = rec->head[2];
		if (MlnImpl_81061C1C(object, 3, data) != 0) {
			ret = 7;
			goto fail;
		}
	}
	ret = MlnImpl_81069F68(rec, table, &id);
	if (ret != 0)
		goto fail;
	*opened = id;
	goto end;
fail:
	if (rec->object != NULL)
		MlnImpl_81061C00(rec->object);
	MlnBufferDestroy(MLN_HEAP_GENERAL, rec->buffer);
	MlnFree(MLN_HEAP_GENERAL, rec);
end:
	MlnImpl_8104A558();
	return ret;
}

/* close a record handle of MlnImpl_8104AF48 */
int MlnImpl_8104B1C8(int handle)
{
	MlnCoreRecordHandle *rec = NULL;
	MlnHandleTable *table;
	int ret;

	if (MlnImpl_8104A4B8() != 0)
		return 7;

	table = MlnCoreHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto end;
	}
	ret = MlnImpl_81069FE8(handle, table, &rec);
	if (ret != 0 || rec == NULL)
		goto end;
	if (rec->object != NULL)
		MlnImpl_81061C00(rec->object);
	MlnBufferDestroy(MLN_HEAP_GENERAL, rec->buffer);
	MlnFree(MLN_HEAP_GENERAL, rec);
	ret = MlnImpl_8106A048(handle, NULL, table) != 0 ? 7 : 0;
end:
	MlnImpl_8104A558();
	return ret;
}
