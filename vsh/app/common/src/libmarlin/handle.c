/*
 * The core context and the handles that name it: each open reads the
 * store into a context, and the handle table maps handles to their
 * MlnCoreHandle (0x8104a560-0x8104d140). The file's name is ours.
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
	if (core->callbackRoutine != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, core->callbackRoutine);
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
	while (sessions-- > 0)
		MlnImpl_8104A558();
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
	ret = MlnImpl_8106273C(core->context, rec->buffer, &rec->contentKey);
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

/*
 * Evaluate a core handle's license for an action (kind 0..3): the rights
 * info it gives is checked and handed out in *out; 0x1d when the license
 * is missing or its rights carry what the library cannot act on.
 */
unsigned int MlnImpl_8104B2AC(int handle, int kind, int flags, MlnBlob *out)
{
	MlnBuffer *rights = NULL;
	MlnCoreHandle *core = NULL;
	MlnRightsInfo *info = NULL;
	MlnPlanktonParameterBlock *parameters = NULL;
	MlnHandleTable *table;
	MlnCoreContext *context;
	unsigned int ret;
	unsigned int r;
	int missing;

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
	if (!(core->flags & MLN_CORE_HANDLE_BUNDLE)) {
		ret = 0x1b;
		goto out;
	}
	context = core->context;
	if (context == NULL) {
		ret = 3;
		goto out;
	}
	if (core->flags & MLN_CORE_HANDLE_DONE) {
		ret = 0x1c;
		goto out;
	}
	if ((unsigned int)kind > 3) {
		ret = 4;
		goto out;
	}
	if (MlnImpl_8104A4B8() != 0) {
		ret = 7;
		goto out;
	}

	ret = MlnImpl_81082834(context, core);
	if (ret != 0)
		goto end;
	if (flags != 0) {
		ret = kind == 3 ? 6 : 4;
		goto end;
	}
	r = MlnImpl_810629C8(context, kind, parameters, &rights);
	if (r == 0) {
		missing = 0;
	} else if (r == 0x23020006) {
		missing = 1;
	} else {
		ret = r == 2 ? 2 : 0x17;
		goto end;
	}
	if (rights != NULL) {
		if (MlnRightsInfoParse(MLN_HEAP_GENERAL, rights->size, rights->data, &info) != 0) {
			ret = 0x42;
			goto end;
		}
		if (out != NULL) {
			out->buffer = rights;
			rights = NULL;
			out->size = out->buffer->size;
		}
		/* the rights info's flags carry the same unsupported bit */
		r = MlnEsbCheckGlobalFlags(MLN_HEAP_GENERAL, (const MlnEsb *)info);
		if (r != 0) {
			ret = r == MLN_ERROR_UNSUPPORTED ? 0x1d : 7;
			goto end;
		}
	}
	ret = missing ? 0x1d : 0;
end:
	MlnRightsInfoFree(MLN_HEAP_GENERAL, info);
	MlnImpl_8103CFC0(parameters);
	MlnBufferDestroy(MLN_HEAP_GENERAL, rights);
	MlnImpl_8104A558();
	return ret;
out:
	MlnRightsInfoFree(MLN_HEAP_GENERAL, info);
	MlnImpl_8103CFC0(parameters);
	MlnBufferDestroy(MLN_HEAP_GENERAL, rights);
	return ret;
}

#define MLN_CORE_CONTENT_KEYS	16	/* content keys MlnImpl_8104B4C4 keeps on its stack */

/*
 * The parameters of a transfer action: Sink, a value list of one
 * ParameterBlock holding Id (transfer[0]), and TransferMode (transfer[1]).
 */
static inline __attribute__((always_inline)) unsigned int
MlnCoreTransferParameters(MlnBuffer *const *transfer, MlnPlanktonParameterBlock **out)
{
	MlnPlanktonValueList *lists = NULL;
	MlnPlanktonValue *sink = NULL;
	MlnPlanktonValue *mode = NULL;
	MlnPlanktonValue *id = NULL;
	MlnPlanktonValue *idValue = NULL;
	MlnPlanktonValueList *idList = NULL;
	MlnPlanktonValueList *sinkList;
	MlnPlanktonParameterBlock *idBlock = NULL;
	MlnPlanktonParameterBlock *block = NULL;
	MlnBuffer *name;
	unsigned int ret = 0;
	unsigned int count = 2;
	unsigned int i;

	if (transfer[0] == NULL || transfer[1] == NULL) {
		ret = 4;
		goto out;
	}
	lists = MlnAllocate(MLN_HEAP_GENERAL, count * sizeof(*lists));
	if (lists == NULL)
		goto nomem;
	MlnMemset(lists, 0, count * sizeof(*lists));
	sink = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*sink));
	if (sink == NULL)
		goto nomem;
	MlnMemset(sink, 0, sizeof(*sink));
	mode = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*mode));
	if (mode == NULL)
		goto nomem;
	MlnMemset(mode, 0, sizeof(*mode));

	idList = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*idList));
	if (idList == NULL)
		goto nomem;
	MlnMemset(idList, 0, sizeof(*idList));
	id = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*id));
	if (id == NULL)
		goto nomem;
	MlnMemset(id, 0, sizeof(*id));
	id->type = MLN_PLANKTON_STRING;
	id->value.buffer = MlnBufferCreate(MLN_HEAP_GENERAL, transfer[0]->size, transfer[0]->data);
	if (id->value.buffer == NULL)
		goto nomem;
	name = MlnBufferCreate(MLN_HEAP_GENERAL, 2, "Id");
	if (name == NULL)
		goto nomem;
	idList->name = name;
	idList->count = 1;
	idList->values = id;
	id = NULL;

	idBlock = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*idBlock));
	if (idBlock == NULL)
		goto nomem;
	MlnMemset(idBlock, 0, sizeof(*idBlock));
	idBlock->parameters = idList;
	idBlock->count = 1;
	idList = NULL;
	idValue = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*idValue));
	if (idValue == NULL)
		goto nomem;
	MlnMemset(idValue, 0, sizeof(*idValue));
	idValue->value.parameters = idBlock;
	idValue->type = MLN_PLANKTON_PARAMETER;
	idBlock = NULL;
	sinkList = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*sinkList));
	if (sinkList == NULL)
		goto nomem;
	MlnMemset(sinkList, 0, sizeof(*sinkList));
	sinkList->values = idValue;
	sinkList->count = 1;
	idValue = NULL;
	sink->value.list = sinkList;
	sink->type = MLN_PLANKTON_VALUE_LIST;

	name = MlnBufferCreate(MLN_HEAP_GENERAL, 4, "Sink");
	if (name == NULL)
		goto nomem;
	lists[0].name = name;
	lists[0].count = 1;
	lists[0].values = sink;
	sink = NULL;
	name = MlnBufferCreate(MLN_HEAP_GENERAL, 12, "TransferMode");
	if (name == NULL)
		goto nomem;
	lists[1].name = name;
	lists[1].count = 1;
	mode->type = MLN_PLANKTON_STRING;
	mode->value.buffer = MlnBufferCreate(MLN_HEAP_GENERAL, transfer[1]->size, transfer[1]->data);
	if (mode->value.buffer == NULL)
		goto nomem;
	lists[1].values = mode;
	mode = NULL;

	block = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*block));
	if (block == NULL)
		goto nomem;
	MlnMemset(block, 0, sizeof(*block));
	block->count = count;
	block->parameters = lists;
	lists = NULL;
	*out = block;
	block = NULL;
	goto out;
nomem:
	ret = 2;
out:
	if (sink != NULL) {
		MlnImpl_8103D01C(sink);
		MlnFree(MLN_HEAP_GENERAL, sink);
	}
	if (mode != NULL) {
		MlnImpl_8103D01C(mode);
		MlnFree(MLN_HEAP_GENERAL, mode);
	}
	if (id != NULL) {
		MlnImpl_8103D01C(id);
		MlnFree(MLN_HEAP_GENERAL, id);
	}
	if (idValue != NULL) {
		MlnImpl_8103D01C(idValue);
		MlnFree(MLN_HEAP_GENERAL, idValue);
	}
	MlnImpl_8103CFC0(block);
	MlnImpl_8103CFC0(idBlock);
	if (idList != NULL) {
		MlnImpl_8103CEB0(idList);
		MlnFree(MLN_HEAP_GENERAL, idList);
	}
	if (lists != NULL) {
		for (i = 0; i < count; i++)
			MlnImpl_8103CEB0(&lists[i]);
		MlnFree(MLN_HEAP_GENERAL, lists);
	}
	return ret;
}

/*
 * Take an action (kind 0..3; 2 a transfer, with its parameters) under a
 * core handle's license. The rights it gives go to *out, the core's state
 * to *state; the rights' first callback of type 1 is kept in the handle.
 * 0x1d when the license is missing or its rights carry what the library
 * cannot act on.
 */
unsigned int MlnImpl_8104B4C4(int handle, int kind, MlnBuffer *const *transfer, MlnBlob *out,
			      MlnBlob *state)
{
	MlnOctopusContentKey *keys[MLN_CORE_CONTENT_KEYS];
	MlnOctopusContentKey **contentKeys = NULL;
	MlnPlanktonParameterBlock *parameters = NULL;
	MlnBuffer *rights = NULL;
	MlnRightsInfo *info = NULL;
	MlnBuffer *coreState = NULL;
	MlnArray *pending = NULL;
	MlnArray *pair = NULL;
	MlnCoreHandle *core = NULL;
	MlnOctopusSignedBundle *bundle;
	MlnCoreContext *context;
	MlnHandleTable *table;
	MlnCallback *callback;
	unsigned int ret;
	unsigned int r;
	unsigned short i;
	int sessions = 0;
	int count;
	int found = 0;
	int missing = 0;
	int outcome;

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
	if (!(core->flags & MLN_CORE_HANDLE_BUNDLE)) {
		ret = 0x1b;
		goto out;
	}
	context = core->context;
	if (context == NULL) {
		ret = 3;
		goto out;
	}
	if (!(core->flags & MLN_CORE_HANDLE_RECORD) && kind != 2) {
		ret = 0x1a;
		goto out;
	}
	if (core->flags & MLN_CORE_HANDLE_DONE) {
		ret = 0x1c;
		goto out;
	}
	if ((unsigned int)kind > 3) {
		ret = 4;
		goto out;
	}
	core->kind = kind;
	__builtin_memset(keys, 0, sizeof(keys));
	if (MlnImpl_8104A448() != 0) {
		ret = 7;
		goto out;
	}
	sessions = 1;

	r = MlnImpl_8104570C(&pair);
	if (r == 0)
		r = MlnImpl_81062450(context, pair);
	else if (r == 6) {
		ret = 0x44;
		goto out;
	}
	if (r != 0) {
		ret = r == 2 ? 2 : 7;
		goto out;
	}

	bundle = core->bundle;
	count = bundle->bundle->contentKeyCount;
	if (count > MLN_CORE_CONTENT_KEYS) {
		contentKeys = MlnAllocate(MLN_HEAP_GENERAL, count * sizeof(*contentKeys));
		if (contentKeys == NULL) {
			ret = 2;
			goto out;
		}
		MlnMemset(contentKeys, 0, count * sizeof(*contentKeys));
	} else {
		contentKeys = keys;
	}
	ret = MlnImpl_81082834(context, core);
	if (ret != 0)
		goto out;
	if (transfer != NULL) {
		if (kind != 2) {
			ret = kind == 3 ? 6 : 4;
			goto out;
		}
		ret = MlnCoreTransferParameters(transfer, &parameters);
		if (ret != 0)
			goto out;
	}

	r = MlnImpl_81062BE8(context, kind, parameters, contentKeys, &rights);
	if (r == 0) {
		r = MlnImpl_81041F48(bundle, contentKeys);
		if (r != 0) {
			ret = r == 2 ? 2 : 0x17;
			goto unload;
		}
		found = 1;
	} else if (r == 0x23020006) {
		missing = 1;
	} else {
		ret = r == 2 ? 2 : 0x17;
		goto out;
	}

	r = MlnImpl_8106249C(context, &pending);
	if (r == 0)
		r = MlnImpl_810456B0((const MlnStoreKeyPair *)pending);
	if (r == 0)
		r = MlnImpl_81062450(context, pending);
	if (r == 0)
		r = MlnImpl_81043A60(1, &coreState);
	if (r != 0) {
		ret = r == 2 ? 2 : 7;
		goto failed;
	}

	outcome = 1;
	if (found == 1 && rights != NULL) {
		if (MlnRightsInfoParse(MLN_HEAP_GENERAL, rights->size, rights->data, &info) != 0) {
			ret = 0x42;
			goto unload;
		}
		for (i = 0; i < info->callbackCount; i++) {
			if (info->callbacks[i].type == 1)
				break;
		}
		if (i < info->callbackCount) {
			callback = &info->callbacks[i];
			core->callbackCookie = callback->cookie;
			MlnBufferDestroy(MLN_HEAP_GENERAL, core->callbackRoutine);
			core->callbackRoutine = MlnBufferCreate(MLN_HEAP_GENERAL,
								info->callbacks[i].routine->size,
								info->callbacks[i].routine->data);
			if (core->callbackRoutine == NULL) {
				ret = 2;
				goto unload;
			}
			outcome = info->callbacks[i].flags != 1;
		}
	}
	if (rights != NULL && out != NULL) {
		out->buffer = rights;
		rights = NULL;
		out->size = out->buffer->size;
	}
	state->buffer = coreState;
	coreState = NULL;
	state->size = state->buffer->size;
	if (info != NULL) {
		r = MlnEsbCheckGlobalFlags(MLN_HEAP_GENERAL, (const MlnEsb *)info);
		if (r != 0) {
			ret = r == MLN_ERROR_UNSUPPORTED ? 0x1d : 7;
			goto failed;
		}
	}
	core->flags |= MLN_CORE_HANDLE_DONE;
	if (missing == 1) {
		ret = 0x1d;
		goto out;
	}
	core->outcome = outcome;
	ret = 0;
	goto out;

failed:
	if (!found)
		goto out;
unload:
	MlnImpl_81062EDC(context, count, contentKeys);
out:
	MlnImpl_8103CFC0(parameters);
	MlnArrayDestroy(MLN_HEAP_GENERAL, pending);
	MlnArrayDestroy(MLN_HEAP_GENERAL, pair);
	MlnBufferDestroy(MLN_HEAP_GENERAL, coreState);
	MlnBufferDestroy(MLN_HEAP_GENERAL, rights);
	if (info != NULL)
		MlnRightsInfoFree(MLN_HEAP_GENERAL, info);
	if (contentKeys != NULL && contentKeys != keys)
		MlnFree(MLN_HEAP_GENERAL, contentKeys);
	while (sessions-- > 0)
		MlnImpl_8104A558();
	return ret;
}

/*
 * Read what a core handle's license gives for an action (kind 0..3) into
 * *out; 0x1e when the license has nothing for it.
 */
unsigned int MlnImpl_8104BFB0(int handle, int kind, int flags, MlnBlob *out)
{
	MlnBuffer *data = NULL;
	MlnCoreHandle *core = NULL;
	MlnPlanktonParameterBlock *parameters = NULL;
	MlnHandleTable *table;
	MlnCoreContext *context;
	unsigned int ret;

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
	if (out == NULL) {
		ret = 4;
		goto out;
	}
	if (!(core->flags & MLN_CORE_HANDLE_BUNDLE)) {
		ret = 0x1b;
		goto out;
	}
	context = core->context;
	if (context == NULL) {
		ret = 3;
		goto out;
	}
	if ((unsigned int)kind > 3) {
		ret = 4;
		goto out;
	}
	if (MlnImpl_8104A4B8() != 0) {
		ret = 7;
		goto out;
	}

	if (flags != 0) {
		ret = kind == 3 ? 6 : 4;
		goto end;
	}
	ret = MlnImpl_81062F20(context, kind, parameters, &data);
	if (ret == 0) {
		out->buffer = data;
		data = NULL;
		out->size = out->buffer->size;
	} else if (ret != 2) {
		ret = ret == 0x2302000C ? 0x1e : 0x17;
	}
end:
	MlnImpl_8103CFC0(parameters);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnImpl_8104A558();
	return ret;
out:
	MlnImpl_8103CFC0(parameters);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	return ret;
}

/* the core's errors of a keyed operation as the library's */
static inline __attribute__((always_inline)) unsigned int MlnCoreKeyError(unsigned int ret)
{
	switch (ret) {
	case 0:
	case 2:
		return ret;
	case 3:
		return 4;
	case 4:
		return 3;
	case 6:
		return 0x44;
	case 0x23020007:
	case 0x23040002:
	case 0x23040005:
	case 0x23040006:
		return 0x20;
	default:
		return 7;
	}
}

/*
 * Run a core handle's keyed operation with an RSA public key given as
 * DER: input to output, *outputSize its size.
 */
unsigned int MlnImpl_8104C130(int handle, const MlnBuffer *publicKey, const void *input,
			      unsigned int inputSize, void *output, unsigned int *outputSize)
{
	MlnCoreHandle *core = NULL;
	void *rsa = NULL;
	MlnBuffer *modulus;
	MlnBuffer *exponent;
	MlnHandleTable *table;
	MlnCoreContext *context;
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
	context = core->context;
	if (publicKey == NULL || inputSize == 0 || output == NULL || outputSize == NULL) {
		ret = 4;
		goto out;
	}

	modulus = NULL;
	exponent = NULL;
	ret = MlnImpl_81039000(publicKey, &modulus, &exponent);
	if (ret == 0) {
		ret = MlnImpl_81055B3C(&rsa, modulus, exponent);
		if (ret == 0) {
			modulus = NULL;
			exponent = NULL;
		}
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, modulus);
	MlnBufferDestroy(MLN_HEAP_GENERAL, exponent);
	if (ret == 0)
		ret = MlnImpl_81063F38(context, rsa, input, inputSize, output, outputSize);
	ret = MlnCoreKeyError(ret);
out:
	if (rsa != NULL)
		MlnImpl_81055CA8(rsa);
	return ret;
}

#define MLN_CORE_MOVE_CALLBACK	"OnAgentCompletion.Move.Callback"

/*
 * Run a callback of a core handle's license (without a record handle only
 * OnAgentCompletion.Move.Callback): the rights it gives go to *first, the
 * core's state to *second. Whether the callback is the one the handle
 * kept becomes the handle's outcome.
 */
unsigned int MlnImpl_8104C32C(int handle, const MlnServiceObject *object, unsigned int size,
			      const unsigned char *data, MlnBlob *first, MlnBlob *second)
{
	MlnBuffer *rights = NULL;
	MlnBuffer *state = NULL;
	MlnRightsInfo *info = NULL;
	MlnArray *pending = NULL;
	MlnArray *pair = NULL;
	MlnCoreHandle *core = NULL;
	MlnHandleTable *table;
	MlnCoreContext *context;
	MlnCallback *callback;
	unsigned int ret;
	unsigned int r;
	unsigned int count;
	unsigned int i;
	int sessions = 0;
	int missing;
	int match;

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
	if (!(core->flags & MLN_CORE_HANDLE_BUNDLE)) {
		ret = 0x1b;
		goto out;
	}
	context = core->context;
	if (context == NULL) {
		ret = 3;
		goto out;
	}
	if (second == NULL) {
		ret = 4;
		goto out;
	}
	if (!(core->flags & MLN_CORE_HANDLE_RECORD) &&
	    MlnStrncmp((const char *)object->buffer->data, MLN_CORE_MOVE_CALLBACK,
		       MlnStrlen(MLN_CORE_MOVE_CALLBACK)) != 0) {
		ret = 0x1a;
		goto out;
	}
	if (MlnImpl_8104A448() != 0) {
		ret = 7;
		goto out;
	}
	sessions = 1;

	r = MlnImpl_8104570C(&pair);
	if (r == 0)
		r = MlnImpl_81062450(context, pair);
	else if (r == 6) {
		ret = 0x44;
		goto out;
	}
	if (r != 0) {
		ret = r == 2 ? 2 : 7;
		goto out;
	}

	r = MlnImpl_81063CE4(context, object, size, data, &rights);
	if (r == 0x23020006) {
		match = 0;
		missing = 1;
		goto pending;
	}
	if (r != 0) {
		ret = r == 2 ? 2 : 0x17;
		goto out;
	}
	if (core->callbackRoutine == NULL)
		match = 1;
	else if (object->second != core->callbackCookie)
		match = 0;
	else
		match = MlnBufferCompare(MLN_HEAP_GENERAL, core->callbackRoutine, object->buffer) == 0;
	missing = 0;
	if (rights == NULL)
		goto pending;
	if (MlnRightsInfoParse(MLN_HEAP_GENERAL, rights->size, rights->data, &info) != 0) {
		ret = 0x42;
		goto out;
	}
	/* the rights info's flags carry the same unsupported bit */
	r = MlnEsbCheckGlobalFlags(MLN_HEAP_GENERAL, (const MlnEsb *)info);
	if (r != 0) {
		ret = r == MLN_ERROR_UNSUPPORTED ? 0x1d : 7;
		goto out;
	}
	count = info->callbackCount;
	for (i = 0; i < count; i++) {
		if (info->callbacks[i].type == 1)
			break;
	}
	if (i < count) {
		callback = &info->callbacks[i];
		core->callbackCookie = callback->cookie;
		MlnBufferDestroy(MLN_HEAP_GENERAL, core->callbackRoutine);
		core->callbackRoutine = MlnBufferCreate(MLN_HEAP_GENERAL,
							info->callbacks[i].routine->size,
							info->callbacks[i].routine->data);
		if (core->callbackRoutine == NULL) {
			ret = 2;
			goto out;
		}
		if (info->callbacks[i].flags == 1)
			match = 0;
	}

pending:
	r = MlnImpl_8106249C(context, &pending);
	if (r == 0)
		r = MlnImpl_810456B0((const MlnStoreKeyPair *)pending);
	if (r == 0)
		r = MlnImpl_81062450(context, pending);
	if (r == 0)
		r = MlnImpl_81043A60(1, &state);
	if (r != 0) {
		ret = r == 2 ? 2 : 7;
		goto out;
	}
	second->buffer = state;
	state = NULL;
	second->size = second->buffer->size;
	ret = missing == 1 ? 0x1d : 0;
	if (core->outcome != 1)
		core->outcome = match;

out:
	if (rights != NULL) {
		if ((ret == 0 || ret == 0x1d) && first != NULL) {
			first->buffer = rights;
			rights = NULL;
			first->size = first->buffer->size;
		} else {
			MlnBufferDestroy(MLN_HEAP_GENERAL, rights);
		}
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, state);
	MlnArrayDestroy(MLN_HEAP_GENERAL, pending);
	MlnArrayDestroy(MLN_HEAP_GENERAL, pair);
	MlnRightsInfoFree(MLN_HEAP_GENERAL, info);
	while (sessions-- > 0)
		MlnImpl_8104A558();
	return ret;
}

/*
 * Run the control of the stored link whose digest is data: the context is
 * opened from the store (its state to *second on failure) and the
 * control's result goes to *first.
 */
unsigned int MlnImpl_8104C7B4(int handle, const MlnBuffer *data, MlnBlob *first, MlnBlob *second)
{
	MlnCoreHandle *core = NULL;
	MlnBuffer *entry = NULL;
	MlnOctopusLink *link = NULL;
	MlnBuffer *result = NULL;
	MlnBuffer *state = NULL;
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
	if (first == NULL || second == NULL) {
		ret = 4;
		goto out;
	}
	if (MlnImpl_8104A4B8() != 0) {
		ret = 7;
		goto out;
	}
	sessions = 1;

	ret = MlnImpl_8104A560(&core->context, &state);
	if (ret != 0) {
		if (state != NULL) {
			second->buffer = state;
			state = NULL;
			second->size = second->buffer->size;
		}
		goto out;
	}
	r = MlnImpl_8104436C(data, &entry);
	if (r != 0) {
		if (r == 3)
			ret = 4;
		else if (r == 2)
			ret = 2;
		else if (r == 6)
			ret = 0x44;
		else
			ret = r == 0x230A0003 ? 0x12 : 7;
		goto out;
	}
	r = MlnImpl_810422E8(entry, &link);
	if (r != 0) {
		if (r == 2) {
			ret = 2;
		} else {
			MlnImpl_81043F1C(entry);
			ret = 0xA;
		}
		goto out;
	}
	r = MlnImpl_8106314C(core->context, link->control, &result);
	if (r != 0) {
		if (r == 2)
			ret = 2;
		else
			ret = r == 0x2302000C ? 0x1e : 0xA;
		goto out;
	}
	first->buffer = result;
	result = NULL;
	first->size = first->buffer->size;
	ret = 0;
out:
	MlnImpl_8103BBA0(link);
	MlnBufferDestroy(MLN_HEAP_GENERAL, entry);
	MlnBufferDestroy(MLN_HEAP_GENERAL, state);
	if (result != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, result);
	while (sessions-- > 0)
		MlnImpl_8104A558();
	return ret;
}

/*
 * The validity of the stored link from from to to: its control's rights
 * give the minutes of their NotBefore and NotAfter (0 when not given). The
 * context is opened in a core handle of its own; its state goes to *state
 * when that fails.
 */
int MlnImpl_8104CA50(const MlnBuffer *from, const MlnBuffer *to, unsigned int *notBefore,
		     unsigned int *notAfter, MlnBlob *state)
{
	MlnCoreHandle *core = NULL;
	MlnBuffer *entry = NULL;
	MlnOctopusLink *link = NULL;
	MlnBuffer *rights = NULL;
	MlnRightsInfo *info = NULL;
	unsigned int before = 0;
	unsigned int after = 0;
	MlnBuffer *coreState = NULL;
	MlnHandleTable *table;
	unsigned int ret;
	unsigned int r;
	int handle;

	if (to == NULL || notBefore == NULL || notAfter == NULL) {
		ret = 4;
		goto out;
	}
	r = MlnImpl_8104D144(&handle);
	if (r != 0) {
		ret = r == 2 ? 2 : 7;
		goto out;
	}
	table = MlnCoreHandleTable(MlnImpl_81006EAC());
	if (table == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81069FE8(handle, table, &core);
	if (ret != 0)
		goto out;
	if (core == NULL || MlnImpl_8104A4B8() != 0) {
		ret = 7;
		goto out;
	}

	ret = MlnImpl_8104A560(&core->context, &coreState);
	if (ret != 0) {
		if (coreState != NULL) {
			state->buffer = coreState;
			coreState = NULL;
			state->size = state->buffer->size;
		}
		goto end;
	}
	r = MlnImpl_81044824(from, to, &entry);
	if (r != 0) {
		if (r == 3)
			ret = 4;
		else if (r == 2)
			ret = 2;
		else if (r == 6)
			ret = 0x44;
		else if (r == 0x230A0011)
			ret = 0x11;
		else
			ret = r == 0x230A0003 ? 0x12 : 7;
		goto end;
	}
	ret = MlnImpl_810422E8(entry, &link);
	if (ret != 0) {
		if (ret != 2) {
			ret = 0xA;
			MlnImpl_81043F1C(entry);
		}
		goto end;
	}
	r = MlnImpl_8106314C(core->context, link->control, &rights);
	if (r == 0x2302000C)
		goto done;
	if (r != 0) {
		ret = r == 2 ? r : 0xA;
		goto end;
	}
	r = MlnRightsInfoParse(MLN_HEAP_GENERAL, rights->size, rights->data, &info);
	if (r != 0) {
		ret = r == 2 ? 2 : 0x42;
		goto end;
	}
	if ((info->constraintFlags & MLN_RIGHTS_NOT_BEFORE) &&
	    MlnTimeToMinutes(info->notBefore[0], &before) != 0) {
		ret = 7;
		goto end;
	}
	if ((info->constraintFlags & MLN_RIGHTS_NOT_AFTER) &&
	    MlnTimeToMinutes(info->notAfter[0], &after) != 0) {
		ret = 7;
		goto end;
	}
done:
	*notBefore = before;
	*notAfter = after;
end:
	MlnRightsInfoFree(MLN_HEAP_GENERAL, info);
	MlnBufferDestroy(MLN_HEAP_GENERAL, rights);
	MlnImpl_8103BBA0(link);
	MlnBufferDestroy(MLN_HEAP_GENERAL, entry);
	MlnBufferDestroy(MLN_HEAP_GENERAL, coreState);
	MlnImpl_8104A558();
	MlnImpl_8104D150(handle);
	return ret;
out:
	MlnRightsInfoFree(MLN_HEAP_GENERAL, info);
	MlnBufferDestroy(MLN_HEAP_GENERAL, rights);
	MlnImpl_8103BBA0(link);
	MlnBufferDestroy(MLN_HEAP_GENERAL, entry);
	MlnBufferDestroy(MLN_HEAP_GENERAL, coreState);
	MlnImpl_8104D150(handle);
	return ret;
}

/*
 * Move a record handle's position within the record by its content key:
 * offset and size of what is wanted, limit of what may be read.
 */
unsigned int MlnImpl_8104CDA0(int handle, unsigned int offset, unsigned int size,
			      unsigned int *position, unsigned int limit)
{
	MlnCoreRecordHandle *rec = NULL;
	MlnHandleTable *table;
	MlnCoreHandle *core;
	const MlnOctopusKey *key;
	unsigned int ret;

	table = MlnCoreHandleTable(MlnImpl_81006EAC());
	if (table == NULL)
		return 2;
	ret = MlnImpl_81069FE8(handle, table, &rec);
	if (ret != 0)
		return ret;
	if (rec == NULL || rec->object == NULL || rec->core == NULL)
		return 3;
	core = rec->core;
	if (offset == 0 || size == 0 || position == NULL || limit == 0)
		return 4;
	if (core->outcome == 0 || core->kind != 0)
		return 0x50;

	key = rec->kind != MLN_SERVICE_RECORD_PLAIN ? rec->contentKey->secretKey : NULL;
	ret = MlnImpl_81061C64(rec->object, size, offset, key, limit, position);
	switch (ret) {
	case 0:
	case 2:
		return ret;
	case 3:
		return 4;
	case 0x23010004:
		return 0x20;
	case 0x23010003:
		return 0x21;
	default:
		return 0x22;
	}
}

/* check a record handle's 32-byte HMAC (second) of first with its content key */
unsigned int MlnImpl_8104CF00(int handle, int kind, unsigned int firstSize,
			      const unsigned char *first, unsigned int secondSize,
			      const unsigned char *second)
{
	MlnCoreRecordHandle *rec = NULL;
	MlnHandleTable *table;
	MlnBuffer message;
	MlnBuffer signature;
	unsigned int ret;

	table = MlnCoreHandleTable(MlnImpl_81006EAC());
	if (table == NULL)
		return 2;
	ret = MlnImpl_81069FE8(handle, table, &rec);
	if (ret != 0)
		return ret;
	if (rec == NULL)
		return 3;
	if (kind != 1 || firstSize == 0 || first == NULL || second == NULL || secondSize != 32)
		return 4;
	if (rec->kind == MLN_SERVICE_RECORD_PLAIN)
		return 0x20;

	message.size = firstSize;
	message.data = (unsigned char *)first;
	signature.size = secondSize;
	signature.data = (unsigned char *)second;
	if (MlnImpl_81061898(2, rec->contentKey->secretKey, &message, &signature) != 0)
		return 0x22;
	return 0;
}

/* decrypt data with a record handle's content key into *out */
unsigned int MlnImpl_8104D018(int handle, const MlnBuffer *data, MlnBuffer **out)
{
	MlnCoreRecordHandle *rec = NULL;
	MlnHandleTable *table;
	MlnCoreHandle *core;
	const MlnOctopusKey *key;
	unsigned int ret;

	table = MlnCoreHandleTable(MlnImpl_81006EAC());
	if (table == NULL)
		return 2;
	ret = MlnImpl_81069FE8(handle, table, &rec);
	if (ret != 0)
		return ret;
	if (rec == NULL || rec->core == NULL || rec->contentKey == NULL ||
	    rec->contentKey->secretKey == NULL)
		return 3;
	core = rec->core;
	key = rec->contentKey->secretKey;
	if (core->outcome == 0 || core->kind != 0)
		return 0x50;

	ret = MlnImpl_81061B2C(key, data, out);
	switch (ret) {
	case 0:
	case 2:
		return ret;
	case 3:
		return 4;
	case 4:
		return 3;
	case 6:
		return 0x44;
	default:
		return 0x20;
	}
}
