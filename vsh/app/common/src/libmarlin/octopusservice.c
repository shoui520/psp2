/*
 * The DRM core's service handlers: each reads a request's arguments, calls into the core
 * and builds the reply's arguments, with the service layer's errors as the library's.
 * The file's name is ours.
 */
#include "mln_internal.h"
/* the service layer's errors as the library's */
int MlnImpl_8102C64C(unsigned int error)
{
	switch (error) {
	case 0:
		return 0;
	case 2:
		return 2;
	case 3:
		return 4;
	case 6:
		return 0x44;
	default:
		return 7;
	}
}

/* a reply of one argument: a handle the core opens, or the core's error in status */
int MlnImpl_8102C664(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	int *value = NULL;
	int handle;
	int err, ret;

	ret = MlnImpl_81026D94(reply, 0);
	if (ret != 0) {
		ret = 0x200;
		goto out;
	}
	err = MlnImpl_8104A8D8(&handle);
	if (err == 0) {
		value = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*value));
		if (value == NULL) {
			ret = 2;
			goto fail;
		}
		*value = handle;
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto fail;
		}
		args->items[0].type = MLN_SERVICE_ARG_HANDLE;
		args->items[0].value = value;
		value = NULL;
	}
	*out = args;
	args = NULL;
	*status = err;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	goto free;
fail:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	if (ret != 0)
		MlnImpl_8104A9CC(handle);
free:
	MlnFree(MLN_HEAP_GENERAL, value);
	return ret;
}

/* the handle in the reply's one argument, closed */
int MlnImpl_8102C77C(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	int handle;
	int ret;

	if (MlnImpl_81026D94(reply, 1) != 0)
		return 0x200;
	ret = MlnImpl_8102701C(MLN_HEAP_GENERAL, reply->items, &handle);
	if (ret != 0)
		return MlnImpl_8102C64C(ret);
	if (handle < 0)
		return 3;
	*status = MlnImpl_8104A9CC(handle);
	*out = NULL;
	return 0;
}

/* the mode in the reply's one argument, set */
int MlnImpl_8102C7FC(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	int mode;
	int ret;

	if (MlnImpl_81026D94(reply, 1) != 0)
		return 0x200;
	ret = MlnImpl_81026FAC(MLN_HEAP_GENERAL, reply->items, &mode);
	if (ret != 0)
		return MlnImpl_8102C64C(ret);
	*status = MlnImpl_8102A484(mode);
	*out = NULL;
	return 0;
}

/* a reply of one flag: whether the core is loaded */
int MlnImpl_8102C870(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	int *loaded = NULL;
	int err, ret;

	ret = MlnImpl_81026D94(reply, 0);
	if (ret != 0) {
		ret = 0x200;
		goto out;
	}
	loaded = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*loaded));
	if (loaded == NULL) {
		ret = 2;
		goto out;
	}
	err = MlnImpl_8102AAE8(loaded);
	if (err == 0) {
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		args->items[0].type = MLN_SERVICE_ARG_FLAG;
		args->items[0].value = loaded;
		loaded = NULL;
	}
	*out = args;
	args = NULL;
	*status = err;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnFree(MLN_HEAP_GENERAL, loaded);
	return ret;
}

/* a reply of the flags the core keeps for the identifier in the reply's one argument */
int MlnImpl_8102C95C(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *id = NULL;
	unsigned int *flags = NULL;
	int err, ret;

	if (MlnImpl_81026D94(reply, 1) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, reply->items, &id);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	flags = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*flags));
	if (flags == NULL) {
		ret = 2;
		goto out;
	}
	err = MlnImpl_8102B404(id, flags);
	if (err == 0 || err == 0x44) {
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		args->items[0].type = MLN_SERVICE_ARG_ID;
		args->items[0].value = flags;
		flags = NULL;
	}
	*out = args;
	args = NULL;
	*status = err;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	MlnFree(MLN_HEAP_GENERAL, flags);
	return ret;
}

/* a reply of the core's object for the identifier in the reply's one argument */
int MlnImpl_8102CAAC(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *id = NULL;
	MlnBuffer *object = NULL;
	MlnBlob blob = { NULL, 0 };
	int err, ret;

	if (MlnImpl_81026D94(reply, 1) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, reply->items, &id);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (id == NULL) {
		err = 4;
	} else {
		err = MlnImpl_8102A5B0(id, &blob);
		if (err == 0) {
			object = blob.buffer;
			blob.buffer = NULL;
			ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
			if (ret != 0) {
				ret = MlnImpl_8102C64C(ret);
				goto out;
			}
			args->items[0].type = MLN_SERVICE_ARG_BUFFER;
			args->items[0].value = object;
		}
	}
	*out = args;
	args = NULL;
	object = NULL;
	*status = err;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	return ret;
}

/* the state in the reply's one argument, given to the core */
int MlnImpl_8102CBF8(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnBuffer *state = NULL;
	int err, ret;

	if (MlnImpl_81026D94(reply, 1) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, reply->items, &state);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	err = state != NULL ? MlnImpl_8102AE84(state) : 4;
	*out = NULL;
	*status = err;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, state);
	return ret;
}

/* a reply of the 21 bytes the core reports */
int MlnImpl_8102CCA4(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	MlnCoreInfo *info = NULL;
	int ret;

	if (MlnImpl_81026D94(reply, 0) != 0) {
		ret = 0x200;
		goto out;
	}
	info = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*info));
	if (info == NULL) {
		ret = 2;
		goto out;
	}
	*info = MlnImpl_8102AFBC();
	ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	args->items[0].type = MLN_SERVICE_ARG_RECORD21;
	args->items[0].value = info;
	info = NULL;
	*out = args;
	args = NULL;
	*status = 0;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnFree(MLN_HEAP_GENERAL, info);
	return ret;
}

/* a reply of the core's buffer for the identifier in the reply's one argument */
int MlnImpl_8102CDB4(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *id = NULL;
	MlnBuffer *object = NULL;
	MlnBlob blob = { NULL, 0 };
	int err, ret;

	if (MlnImpl_81026D94(reply, 1) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, reply->items, &id);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	err = MlnImpl_8102B064(id, &blob);
	if (err == 0) {
		object = blob.buffer;
		blob.buffer = NULL;
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		args->items[0].type = MLN_SERVICE_ARG_BUFFER;
		args->items[0].value = object;
	}
	*out = args;
	args = NULL;
	object = NULL;
	*status = err;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	return ret;
}

/* the core's buffer for the identifier in the reply's one argument, handed to the caller */
int MlnImpl_8102CEEC(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnBuffer *id = NULL;
	MlnBlob blob = { NULL, 0 };
	int err, ret;

	if (result == NULL) {
		ret = 4;
		goto out;
	}
	if (MlnImpl_81026D94(reply, 1) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, reply->items, &id);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	err = MlnImpl_8102B590(id, &blob);
	if (err == 0) {
		*result = blob.buffer;
		blob.buffer = NULL;
	}
	*out = NULL;
	*status = err;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	return ret;
}

/* the identifier in the reply's one argument, given to the core */
int MlnImpl_8102CFB0(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnBuffer *id = NULL;
	int err, ret;

	if (MlnImpl_81026D94(reply, 1) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, reply->items, &id);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	err = id != NULL ? MlnImpl_8102B198(id) : 4;
	*out = NULL;
	*status = err;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	return ret;
}

/* the core's second buffer for the identifier in the reply's one argument */
int MlnImpl_8102D05C(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnBuffer *id = NULL;
	MlnBlob blob = { NULL, 0 };
	int err, ret;

	if (result == NULL) {
		ret = 4;
		goto out;
	}
	if (MlnImpl_81026D94(reply, 1) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, reply->items, &id);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (id == NULL) {
		err = 4;
	} else {
		err = MlnImpl_8102B718(id, &blob);
		if (err == 0) {
			*result = blob.buffer;
			blob.buffer = NULL;
		}
	}
	*out = NULL;
	*status = err;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	return ret;
}

/* the state in the reply's one argument, checked by the core */
int MlnImpl_8102D138(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnBuffer *state = NULL;
	MlnBuffer empty = { 0, NULL };
	int err, ret;

	if (MlnImpl_81026D94(reply, 1) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, reply->items, &state);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	err = MlnImpl_8102A7F0(state != NULL ? state : &empty);
	*out = NULL;
	*status = err;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, state);
	return ret;
}

/* the core's state, handed to the caller */
int MlnImpl_8102D1D4(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnBlob blob = { NULL, 0 };
	int err;

	if (result == NULL)
		return 4;
	if (MlnImpl_81026D94(reply, 0) != 0)
		return 0x200;
	err = MlnImpl_8102A69C(&blob);
	if (err == 0)
		*result = blob.buffer;
	*out = NULL;
	*status = err;
	return 0;
}

static inline void MlnServiceListFree(MlnServiceList *list)
{
	MlnServiceListEntry *entry;
	unsigned int i;

	if (list == NULL)
		return;
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

/* a reply of a handle the core opens for the kind and the named values in the reply */
int MlnImpl_8102D240(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	MlnServiceList *list = NULL;
	int *value = NULL;
	int kind = 0;
	int handle;
	int err, ret;

	if (MlnImpl_81026D94(reply, 2) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_81026FAC(MLN_HEAP_GENERAL, &reply->items[0], &kind);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_810271B8(MLN_HEAP_GENERAL, &reply->items[1], (void **)&list);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	err = MlnImpl_8104D5E0(kind, list, &handle);
	if (err == 0) {
		value = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*value));
		if (value == NULL) {
			ret = 2;
			goto fail;
		}
		*value = handle;
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto fail;
		}
		args->items[0].type = MLN_SERVICE_ARG_HANDLE;
		args->items[0].value = value;
		value = NULL;
	}
	*out = args;
	args = NULL;
	*status = err;
	goto out;
fail:
	if (ret != 0)
		MlnImpl_8104DAA4(handle);
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnFree(MLN_HEAP_GENERAL, value);
	MlnServiceListFree(list);
	return ret;
}

/* the handle in the reply's one argument, closed */
int MlnImpl_8102D454(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	int handle;
	int ret;

	if (MlnImpl_81026D94(reply, 1) != 0)
		return 0x200;
	ret = MlnImpl_8102701C(MLN_HEAP_GENERAL, reply->items, &handle);
	if (ret != 0)
		return MlnImpl_8102C64C(ret);
	if (handle < 0)
		return 3;
	*status = MlnImpl_8104DAA4(handle);
	*out = NULL;
	return 0;
}

/* a reply of the buffer the core makes from a handle, a buffer and a name */
int MlnImpl_8102D4D4(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *data = NULL;
	MlnBuffer *nameBuffer = NULL;
	MlnBuffer *result = NULL;
	char *name = NULL;
	MlnBlob blob = { NULL, 0 };
	int handle;
	int err, ret;

	if (MlnImpl_81026D94(reply, 3) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_8102701C(MLN_HEAP_GENERAL, &reply->items[0], &handle);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[1], &data);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[2], &nameBuffer);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	ret = MlnBufferToString(MLN_HEAP_GENERAL, nameBuffer, &name);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	err = MlnImpl_8104DC4C(handle, data != NULL ? data->size : 0,
			       data != NULL ? data->data : NULL, name, &blob);
	if (err == 0) {
		result = blob.buffer;
		blob.buffer = NULL;
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		args->items[0].type = MLN_SERVICE_ARG_BUFFER;
		args->items[0].value = result;
	}
	*out = args;
	args = NULL;
	result = NULL;
	*status = err;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnBufferDestroy(MLN_HEAP_GENERAL, nameBuffer);
	MlnFree(MLN_HEAP_GENERAL, name);
	MlnBufferDestroy(MLN_HEAP_GENERAL, result);
	return ret;
}

/* a reply of the first of two buffers the core makes from a handle and a buffer; the
 * second goes to the caller */
int MlnImpl_8102D6F8(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *data = NULL;
	MlnBuffer *object = NULL;
	MlnBlob first = { NULL, 0 };
	MlnBlob second = { NULL, 0 };
	int handle;
	int err, ret;

	if (result == NULL) {
		ret = 4;
		goto out;
	}
	if (MlnImpl_81026D94(reply, 2) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_8102701C(MLN_HEAP_GENERAL, &reply->items[0], &handle);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[1], &data);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	err = MlnImpl_8104DE10(handle, data, &first, &second);
	if (err == 0) {
		*result = second.buffer;
		second.buffer = NULL;
		object = first.buffer;
		first.buffer = NULL;
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		args->items[0].type = MLN_SERVICE_ARG_BUFFER;
		args->items[0].value = object;
	}
	*out = args;
	args = NULL;
	*status = err;
	object = NULL;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	return ret;
}

/* a reply of the first of two buffers the core makes from a handle, a buffer, a name and
 * two more buffers; the second goes to the caller */
int MlnImpl_8102D8B8(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *data = NULL;
	MlnBuffer *nameBuffer = NULL;
	MlnBuffer *third = NULL;
	MlnBuffer *fourth = NULL;
	MlnBuffer *object = NULL;
	char *name = NULL;
	MlnBlob first = { NULL, 0 };
	MlnBlob second = { NULL, 0 };
	int handle;
	int err, ret;

	if (MlnImpl_81026D94(reply, 5) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_8102701C(MLN_HEAP_GENERAL, &reply->items[0], &handle);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[1], &data);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[2], &nameBuffer);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[3], &third);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[4], &fourth);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	ret = MlnBufferToString(MLN_HEAP_GENERAL, nameBuffer, &name);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	err = MlnImpl_8104E18C(handle, data != NULL ? data->size : 0,
			       data != NULL ? data->data : NULL, name, third, fourth, &first,
			       &second);
	*result = second.buffer;
	second.buffer = NULL;
	if (err == 0) {
		object = first.buffer;
		first.buffer = NULL;
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		args->items[0].type = MLN_SERVICE_ARG_BUFFER;
		args->items[0].value = object;
	}
	*out = args;
	args = NULL;
	*status = err;
	object = NULL;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnBufferDestroy(MLN_HEAP_GENERAL, nameBuffer);
	MlnFree(MLN_HEAP_GENERAL, name);
	MlnBufferDestroy(MLN_HEAP_GENERAL, third);
	MlnBufferDestroy(MLN_HEAP_GENERAL, fourth);
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	return ret;
}

/* a reply of the first two of three buffers the core makes from a handle and a buffer;
 * the third goes to the caller */
int MlnImpl_8102DB4C(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *data = NULL;
	MlnBuffer *firstObject = NULL;
	MlnBuffer *secondObject = NULL;
	MlnBlob first = { NULL, 0 };
	MlnBlob second = { NULL, 0 };
	MlnBlob third = { NULL, 0 };
	int handle;
	int err, ret;

	if (result == NULL) {
		ret = 4;
		goto out;
	}
	if (MlnImpl_81026D94(reply, 2) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_8102701C(MLN_HEAP_GENERAL, &reply->items[0], &handle);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[1], &data);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	err = MlnImpl_8104E45C(handle, data, &first, &second, &third);
	if (err == 0) {
		firstObject = first.buffer;
		secondObject = second.buffer;
		*result = third.buffer;
		first.buffer = NULL;
		second.buffer = NULL;
		third.buffer = NULL;
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 2, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		args->items[0].type = MLN_SERVICE_ARG_BUFFER;
		args->items[0].value = firstObject;
		args->items[1].type = MLN_SERVICE_ARG_BUFFER;
		args->items[1].value = secondObject;
	}
	*out = args;
	args = NULL;
	*status = err;
	firstObject = NULL;
	secondObject = NULL;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnBufferDestroy(MLN_HEAP_GENERAL, firstObject);
	MlnBufferDestroy(MLN_HEAP_GENERAL, secondObject);
	return ret;
}

/* a reply of the first of two buffers the core makes from a handle, a buffer, a name and
 * one more buffer; the second goes to the caller */
int MlnImpl_8102DD64(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *data = NULL;
	MlnBuffer *nameBuffer = NULL;
	MlnBuffer *third = NULL;
	MlnBuffer *object = NULL;
	char *name = NULL;
	MlnBlob first = { NULL, 0 };
	MlnBlob second = { NULL, 0 };
	int handle;
	int err, ret;

	if (MlnImpl_81026D94(reply, 4) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_8102701C(MLN_HEAP_GENERAL, &reply->items[0], &handle);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[1], &data);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[2], &nameBuffer);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[3], &third);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	ret = MlnBufferToString(MLN_HEAP_GENERAL, nameBuffer, &name);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	err = MlnImpl_8104E9B8(handle, data != NULL ? data->size : 0,
			       data != NULL ? data->data : NULL, name, third, &first, &second);
	*result = second.buffer;
	second.buffer = NULL;
	if (err == 0) {
		object = first.buffer;
		first.buffer = NULL;
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		args->items[0].type = MLN_SERVICE_ARG_BUFFER;
		args->items[0].value = object;
	}
	*out = args;
	args = NULL;
	*status = err;
	object = NULL;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnBufferDestroy(MLN_HEAP_GENERAL, nameBuffer);
	MlnFree(MLN_HEAP_GENERAL, name);
	MlnBufferDestroy(MLN_HEAP_GENERAL, third);
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	return ret;
}
/* as 0x8102d6f8, with the core's other call */
int MlnImpl_8102DFC8(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *data = NULL;
	MlnBuffer *object = NULL;
	MlnBlob first = { NULL, 0 };
	MlnBlob second = { NULL, 0 };
	int handle;
	int err, ret;

	if (result == NULL) {
		ret = 4;
		goto out;
	}
	if (MlnImpl_81026D94(reply, 2) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_8102701C(MLN_HEAP_GENERAL, &reply->items[0], &handle);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[1], &data);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	err = MlnImpl_8104EBD8(handle, data, &first, &second);
	if (err == 0) {
		*result = second.buffer;
		second.buffer = NULL;
		object = first.buffer;
		first.buffer = NULL;
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		args->items[0].type = MLN_SERVICE_ARG_BUFFER;
		args->items[0].value = object;
	}
	*out = args;
	args = NULL;
	*status = err;
	object = NULL;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	return ret;
}
/* a reply of the first of two buffers the core makes from a handle, a buffer, a name and
 * three more buffers; the second goes to the caller */
int MlnImpl_8102E188(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *data = NULL;
	MlnBuffer *nameBuffer = NULL;
	MlnBuffer *third = NULL;
	MlnBuffer *fourth = NULL;
	MlnBuffer *fifth = NULL;
	MlnBuffer *object = NULL;
	char *name = NULL;
	MlnBlob first = { NULL, 0 };
	MlnBlob second = { NULL, 0 };
	int handle;
	int err, ret;

	if (MlnImpl_81026D94(reply, 6) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_8102701C(MLN_HEAP_GENERAL, &reply->items[0], &handle);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[1], &data);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[2], &nameBuffer);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[3], &third);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[4], &fourth);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[5], &fifth);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	ret = MlnBufferToString(MLN_HEAP_GENERAL, nameBuffer, &name);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	err = MlnImpl_8104F044(handle, data != NULL ? data->size : 0,
			       data != NULL ? data->data : NULL, name, third, fourth, fifth,
			       &first, &second);
	*result = second.buffer;
	second.buffer = NULL;
	if (err == 0) {
		object = first.buffer;
		first.buffer = NULL;
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		args->items[0].type = MLN_SERVICE_ARG_BUFFER;
		args->items[0].value = object;
	}
	*out = args;
	args = NULL;
	*status = err;
	object = NULL;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnBufferDestroy(MLN_HEAP_GENERAL, nameBuffer);
	MlnFree(MLN_HEAP_GENERAL, name);
	MlnBufferDestroy(MLN_HEAP_GENERAL, third);
	MlnBufferDestroy(MLN_HEAP_GENERAL, fourth);
	MlnBufferDestroy(MLN_HEAP_GENERAL, fifth);
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	return ret;
}
/* as 0x8102d6f8, with a third of the core's calls */
int MlnImpl_8102E468(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *data = NULL;
	MlnBuffer *object = NULL;
	MlnBlob first = { NULL, 0 };
	MlnBlob second = { NULL, 0 };
	int handle;
	int err, ret;

	if (result == NULL) {
		ret = 4;
		goto out;
	}
	if (MlnImpl_81026D94(reply, 2) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_8102701C(MLN_HEAP_GENERAL, &reply->items[0], &handle);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[1], &data);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	err = MlnImpl_8104F2F4(handle, data, &first, &second);
	if (err == 0) {
		*result = second.buffer;
		second.buffer = NULL;
		object = first.buffer;
		first.buffer = NULL;
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		args->items[0].type = MLN_SERVICE_ARG_BUFFER;
		args->items[0].value = object;
	}
	*out = args;
	args = NULL;
	*status = err;
	object = NULL;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	return ret;
}

/* three buffers in the reply given to the core with a handle */
int MlnImpl_8102E628(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnBuffer *first = NULL;
	MlnBuffer *second = NULL;
	MlnBuffer *third = NULL;
	int handle;
	int err, ret;

	if (MlnImpl_81026D94(reply, 4) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_8102701C(MLN_HEAP_GENERAL, &reply->items[0], &handle);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[1], &first);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[2], &second);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[3], &third);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	if (first != NULL && second != NULL && third != NULL)
		err = MlnImpl_8104FC70(handle, first->size, first->data, second->size, second->data,
				       third->size, third->data);
	else
		err = 4;
	*out = NULL;
	*status = err;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, first);
	MlnBufferDestroy(MLN_HEAP_GENERAL, second);
	MlnBufferDestroy(MLN_HEAP_GENERAL, third);
	return ret;
}

/* two buffers in the reply given to the core with a handle */
int MlnImpl_8102E7A8(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnBuffer *first = NULL;
	MlnBuffer *second = NULL;
	int handle;
	int err, ret;

	if (MlnImpl_81026D94(reply, 3) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_8102701C(MLN_HEAP_GENERAL, &reply->items[0], &handle);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[1], &first);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[2], &second);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	err = MlnImpl_81050110(handle, first, second);
	*out = NULL;
	*status = err;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, first);
	MlnBufferDestroy(MLN_HEAP_GENERAL, second);
	return ret;
}
