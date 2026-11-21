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

/* the core's group as the library's: its buffers move over, its entries' list is freed */
static inline int MlnCoreGroupConvert(MlnCoreGroup *core, MlnBufferGroup **out)
{
	MlnBufferGroup *group;
	MlnBuffer ***entries;
	MlnCoreGroupEntry *entry;
	unsigned int count, i;

	group = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*group));
	if (group == NULL)
		goto fail;
	MlnMemset(group, 0, sizeof(*group));
	group->tag = core->tag;
	group->first = core->first;
	core->first = NULL;
	group->second = core->second;
	core->second = NULL;
	if (core->thirdPresent) {
		group->third = core->third;
		core->third = NULL;
	}
	if (core->entryCount == 0) {
		count = 0;
		entries = NULL;
	} else {
		entries = MlnAllocate(MLN_HEAP_GENERAL, core->entryCount * sizeof(*entries));
		if (entries == NULL)
			goto fail;
		MlnMemset(entries, 0, core->entryCount * sizeof(*entries));
		count = core->entryCount;
		for (i = 0; i < count; i++) {
			entries[i] = MlnAllocate(MLN_HEAP_GENERAL, 3 * sizeof(**entries));
			MlnMemset(entries[i], 0, 3 * sizeof(**entries));
			entry = &core->entries[i];
			if (entry->firstPresent) {
				entries[i][0] = entry->first;
				entry->first = NULL;
			}
			if (entry->secondPresent) {
				entries[i][1] = entry->second;
				entry->second = NULL;
			}
			if (entry->thirdPresent) {
				entries[i][2] = entry->third;
				entry->third = NULL;
			}
		}
	}
	MlnImpl_8102AAD0(core->entries);
	core->entries = NULL;
	group->entries = entries;
	group->entryCount = count;
	*out = group;
	group = NULL;
	MlnImpl_81006D14(MLN_HEAP_GENERAL, group);
	return 0;
fail:
	MlnImpl_81006D14(MLN_HEAP_GENERAL, group);
	return 2;
}

/* a reply of the buffer group the core keeps for the handle in the reply's one argument */
int MlnImpl_8102E8A0(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	MlnBufferGroup *group = NULL;
	MlnCoreGroup core;
	int handle;
	int err, ret;

	core.first = NULL;
	core.firstPresent = 0;
	core.second = NULL;
	core.secondPresent = 0;
	core.third = NULL;
	core.thirdPresent = 0;
	core.entries = NULL;
	core.entryCount = 0;
	if (MlnImpl_81026D94(reply, 1) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_8102701C(MLN_HEAP_GENERAL, reply->items, &handle);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	err = MlnImpl_8104F620(handle, &core);
	if (err == 0) {
		ret = MlnCoreGroupConvert(&core, &group);
		if (ret != 0)
			goto out;
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		args->items[0].type = MLN_SERVICE_ARG_GROUP;
		args->items[0].value = group;
	}
	*out = args;
	args = NULL;
	group = NULL;
	*status = err;
out:
	MlnImpl_81006D14(MLN_HEAP_GENERAL, group);
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	return ret;
}

/* the buffer the core makes from a handle and the reply's buffer, handed to the caller;
 * the core may give it with error 10 too */
int MlnImpl_8102EBD0(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnBuffer *data = NULL;
	MlnBlob blob = { NULL, 0 };
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
	if (data == NULL) {
		err = 4;
	} else {
		err = MlnImpl_8104AB18(handle, data->size, data->data, &blob);
		if (err == 0 || err == 10) {
			*result = blob.buffer;
			blob.buffer = NULL;
		}
	}
	*out = NULL;
	*status = err;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	return ret;
}

static inline void MlnServiceRecordValueFree(MlnServiceRecordValue *record)
{
	if (record == NULL)
		return;
	MlnBufferDestroy(MLN_HEAP_GENERAL, record->buffer);
	MlnFree(MLN_HEAP_GENERAL, record);
}

/* a reply of the handle the core opens for a handle and the reply's record */
int MlnImpl_8102ED08(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	MlnServiceRecordValue *record = NULL;
	int *value = NULL;
	int handle;
	int opened = -1;
	int err, ret;

	if (MlnImpl_81026D94(reply, 2) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_8102701C(MLN_HEAP_GENERAL, &reply->items[0], &handle);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_810271F4(MLN_HEAP_GENERAL, &reply->items[1], (void **)&record);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	err = MlnImpl_8104AF48(handle, record, &opened);
	if (err == 0) {
		value = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*value));
		if (value == NULL) {
			ret = 2;
			goto fail;
		}
		*value = opened;
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
	if (ret != 0 && opened != -1)
		MlnImpl_8104B1C8(opened);
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnFree(MLN_HEAP_GENERAL, value);
	MlnServiceRecordValueFree(record);
	return ret;
}

/* the handle in the reply's one argument, closed */
int MlnImpl_8102EED4(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	int handle;
	int ret;

	if (MlnImpl_81026D94(reply, 1) != 0)
		return 0x200;
	ret = MlnImpl_8102701C(MLN_HEAP_GENERAL, reply->items, &handle);
	if (ret != 0)
		return MlnImpl_8102C64C(ret);
	*status = MlnImpl_8104B1C8(handle);
	*out = NULL;
	return 0;
}

/* a reply of the buffer the core makes for a handle and a kind, by the command: 1068,
 * 1069 (a second buffer goes to the caller) or 1070; the core may give it with error 29 */
int MlnImpl_8102EF48(int command, const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *object = NULL;
	MlnBlob blob = { NULL, 0 };
	MlnBlob second;
	int handle, kind;
	int made, err, ret;

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
	ret = MlnImpl_81026FAC(MLN_HEAP_GENERAL, &reply->items[1], &kind);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	switch (command) {
	case 1069:
		second.buffer = NULL;
		second.size = 0;
		err = MlnImpl_8104B4C4(handle, kind, 0, &blob, &second);
		made = err == 0 || err == 29;
		if (made)
			*result = second.buffer;
		break;
	case 1070:
		err = MlnImpl_8104BFB0(handle, kind, 0, &blob);
		made = err == 0 || err == 29;
		break;
	case 1068:
		err = MlnImpl_8104B2AC(handle, kind, 0, &blob);
		made = err == 0 || err == 29;
		break;
	default:
		ret = 4;
		goto out;
	}
	if (made) {
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
	*status = err;
	object = NULL;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	return ret;
}

static inline void MlnServiceObjectFree(MlnServiceObject *object)
{
	if (object == NULL)
		return;
	MlnBufferDestroy(MLN_HEAP_GENERAL, object->buffer);
	MlnFree(MLN_HEAP_GENERAL, object);
}

/* a reply of the first of two buffers the core makes from a handle, an object and a
 * buffer; the second goes to the caller, and the core may give them with error 29 */
int MlnImpl_8102F1B0(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnServiceArgs *args = NULL;
	MlnServiceObject *object = NULL;
	MlnBuffer *data = NULL;
	MlnBuffer *made = NULL;
	MlnBlob first = { NULL, 0 };
	MlnBlob second = { NULL, 0 };
	int handle;
	int err, ret;

	if (result == NULL) {
		ret = 4;
		goto out;
	}
	if (MlnImpl_81026D94(reply, 3) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_8102701C(MLN_HEAP_GENERAL, &reply->items[0], &handle);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81027140(MLN_HEAP_GENERAL, &reply->items[1], (void **)&object);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[2], &data);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	err = MlnImpl_8104C32C(handle, object, data != NULL ? data->size : 0,
			       data != NULL ? data->data : NULL, &first, &second);
	if (err == 0 || err == 29) {
		made = first.buffer;
		*result = second.buffer;
		first.buffer = NULL;
		second.buffer = NULL;
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		args->items[0].type = MLN_SERVICE_ARG_BUFFER;
		args->items[0].value = made;
	}
	*out = args;
	args = NULL;
	*status = err;
	made = NULL;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnServiceObjectFree(object);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnBufferDestroy(MLN_HEAP_GENERAL, made);
	return ret;
}

/* a reply of the first of two buffers the core makes from a handle and a buffer; the
 * second goes to the caller also with error 10, the first only without one */
int MlnImpl_8102F408(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
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
	err = MlnImpl_8104C7B4(handle, data, &first, &second);
	if (err == 0 || err == 10) {
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

/* a reply of the position the core moves a handle to, from an offset and two sizes; the
 * position asked for is kept within the offset */
int MlnImpl_8102F5E4(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	unsigned int *position = NULL;
	unsigned int offset;
	unsigned int size = 0;
	unsigned int limit = 0;
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
	ret = MlnImpl_810270BC(MLN_HEAP_GENERAL, &reply->items[1], &offset);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_8102704C(MLN_HEAP_GENERAL, &reply->items[2], &size);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	position = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*position));
	ret = MlnImpl_810270BC(MLN_HEAP_GENERAL, &reply->items[3], position);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_8102704C(MLN_HEAP_GENERAL, &reply->items[4], &limit);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	if (size == 0 || limit == 0) {
		err = 4;
	} else {
		if (*position > offset)
			*position = offset;
		err = MlnImpl_8104CDA0(handle, offset, size, position, limit);
		if (err == 0) {
			ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
			if (ret != 0) {
				ret = MlnImpl_8102C64C(ret);
				goto out;
			}
			args->items[0].type = MLN_SERVICE_ARG_OFFSET;
			args->items[0].value = position;
			position = NULL;
		}
	}
	*out = args;
	args = NULL;
	*status = err;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnFree(MLN_HEAP_GENERAL, position);
	return ret;
}

/* two buffers in the reply given to the core with a handle and a kind */
int MlnImpl_8102F83C(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnBuffer *first = NULL;
	MlnBuffer *second = NULL;
	int handle = -1;
	int kind;
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
	ret = MlnImpl_81026FAC(MLN_HEAP_GENERAL, &reply->items[1], &kind);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[2], &first);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[3], &second);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	if (first != NULL && second != NULL)
		err = MlnImpl_8104CF00(handle, kind, first->size, first->data, second->size,
				       second->data);
	else
		err = 4;
	*out = NULL;
	*status = err;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, first);
	MlnBufferDestroy(MLN_HEAP_GENERAL, second);
	return ret;
}

/* the commands without arguments: 1033, 1039 and 1089 */
int MlnImpl_8102F9A0(int command, const MlnServiceReply *reply, MlnServiceArgs **out,
		     int *status)
{
	int err;

	if (MlnImpl_81026D94(reply, 0) != 0)
		return 0x200;
	switch (command) {
	case 1039:
		err = MlnImpl_8102B2E0();
		break;
	case 1089:
		err = MlnImpl_8102A914();
		break;
	case 1033:
		MlnImpl_8102B054();
		err = 0;
		break;
	default:
		return 7;
	}
	*out = NULL;
	*status = err;
	return 0;
}

int MlnImpl_8102FA24(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	int ret;

	if (MlnImpl_81026D94(reply, 0) != 0)
		return 0x200;
	ret = MlnImpl_81026D94(reply, 0);
	if (ret != 0)
		return 0x200;
	*status = MlnImpl_81052600();
	*out = NULL;
	return ret;
}

int MlnImpl_8102FA70(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	if (MlnImpl_81026D94(reply, 0) != 0)
		return 0x200;
	*status = MlnImpl_81052608();
	*out = NULL;
	return 0;
}

/* a reply of the buffer the core makes from the reply's two values */
int MlnImpl_8102FAAC(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *object = NULL;
	MlnBlob blob = { NULL, 0 };
	unsigned int first, second;
	int err, ret;

	if (MlnImpl_81026D94(reply, 2) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_81026FE4(MLN_HEAP_GENERAL, &reply->items[0], &first);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026FE4(MLN_HEAP_GENERAL, &reply->items[1], &second);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	err = MlnImpl_81052610(second, first, &blob);
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
	*status = err;
	object = NULL;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	return ret;
}

/* the buffer the core makes from the reply's buffer, handed to the caller */
int MlnImpl_8102FC18(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnBuffer *data = NULL;
	MlnBlob blob = { NULL, 0 };
	int err, ret;

	if (MlnImpl_81026D94(reply, 1) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, reply->items, &data);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	err = MlnImpl_8105272C(data->size, data->data, 1, &blob);
	if (err == 0) {
		*result = blob.buffer;
		blob.buffer = NULL;
	}
	*out = NULL;
	*status = err;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	return ret;
}

/* a reply of the buffer the core makes with no values */
int MlnImpl_8102FCCC(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *object = NULL;
	MlnBlob blob = { NULL, 0 };
	int err, ret;

	ret = MlnImpl_81026D94(reply, 0);
	if (ret != 0) {
		ret = 0x200;
		goto out;
	}
	err = MlnImpl_81052610(0, 0xffffffff, &blob);
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
	*status = err;
	object = NULL;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	return ret;
}

/* the same as 0x8102fc18, for another command */
int MlnImpl_8102FDB4(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnBuffer *data = NULL;
	MlnBlob blob = { NULL, 0 };
	int err, ret;

	if (MlnImpl_81026D94(reply, 1) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, reply->items, &data);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	err = MlnImpl_8105272C(data->size, data->data, 1, &blob);
	if (err == 0) {
		*result = blob.buffer;
		blob.buffer = NULL;
	}
	*out = NULL;
	*status = err;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	return ret;
}

/*
 * A reply of the handle the core opens for the reply's named values. With them alone, the
 * core fills two lists of buffers and two more buffers, and the reply carries them all;
 * otherwise the reply gives them: a count and its buffers, a count and its buffers, then
 * the two buffers.
 */
int MlnImpl_8102FE68(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	MlnServiceList *list = NULL;
	MlnBuffer **firsts = NULL;
	MlnBuffer **seconds = NULL;
	MlnBuffer *third = NULL;
	MlnBuffer *fourth = NULL;
	MlnBuffer *buffer = NULL;
	unsigned int *firstCountValue = NULL;
	unsigned int *secondCountValue = NULL;
	int *value = NULL;
	unsigned int firstCount, secondCount;
	unsigned int i, j;
	int handle;
	int err, ret;

	ret = MlnImpl_810271B8(MLN_HEAP_GENERAL, &reply->items[0], (void **)&list);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (reply->items != NULL && reply->count > 1) {
		if (reply->count <= 4) {
			ret = 0x200;
			goto out;
		}
		ret = MlnImpl_81026FE4(MLN_HEAP_GENERAL, &reply->items[1], &firstCount);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		if ((reply->items != NULL ? reply->count : 0) < firstCount + 2) {
			ret = 0x200;
			goto out;
		}
		i = 2;
		if (firstCount != 0) {
			firsts = MlnAllocate(MLN_HEAP_GENERAL, firstCount * sizeof(*firsts));
			if (firsts == NULL) {
				ret = 2;
				goto out;
			}
			MlnMemset(firsts, 0, firstCount * sizeof(*firsts));
			for (j = 0; i < firstCount + 2; i++, j++) {
				ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[i], &buffer);
				if (ret != 0) {
					ret = MlnImpl_8102C64C(ret);
					goto out;
				}
				firsts[j] = buffer;
			}
		}
		ret = MlnImpl_81026FE4(MLN_HEAP_GENERAL, &reply->items[i], &secondCount);
		i++;
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		if ((reply->items != NULL ? reply->count : 0) < secondCount + firstCount + 3) {
			ret = 0x200;
			goto out;
		}
		if (secondCount != 0) {
			seconds = MlnAllocate(MLN_HEAP_GENERAL, secondCount * sizeof(*seconds));
			if (seconds == NULL) {
				ret = 2;
				goto out;
			}
			MlnMemset(seconds, 0, secondCount * sizeof(*seconds));
			for (j = 0; i < firstCount + secondCount + 3; i++, j++) {
				ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[i], &buffer);
				if (ret != 0) {
					ret = MlnImpl_8102C64C(ret);
					goto out;
				}
				seconds[j] = buffer;
			}
		}
		ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[i], &buffer);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		third = buffer;
		ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[i + 1], &buffer);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		fourth = buffer;
		if (MlnImpl_81026D94(reply, i + 2) != 0 || firsts == NULL || seconds == NULL ||
		    third == NULL || fourth == NULL) {
			ret = 0x200;
			goto out;
		}
	}
	err = MlnImpl_81050778(&handle, list, &firstCount, &firsts, &secondCount, &seconds,
			       &third, &fourth);
	if (err != 0) {
		*out = args;
		args = NULL;
		*status = err;
		goto out;
	}
	value = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*value));
	if (value == NULL) {
		ret = 2;
		goto fail;
	}
	*value = handle;
	if (reply->items != NULL && reply->count == 1) {
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, firstCount + secondCount + 5, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto fail;
		}
		firstCountValue = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*firstCountValue));
		if (firstCountValue == NULL) {
			ret = 2;
			goto fail;
		}
		*firstCountValue = firstCount;
		secondCountValue = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*secondCountValue));
		if (secondCountValue == NULL) {
			ret = 2;
			goto fail;
		}
		*secondCountValue = secondCount;
		args->items[1].type = MLN_SERVICE_ARG_ID;
		args->items[1].value = firstCountValue;
		for (j = 0; j < firstCount; j++) {
			args->items[2 + j].type = MLN_SERVICE_ARG_BUFFER;
			args->items[2 + j].value = firsts[j];
			firsts[j] = NULL;
		}
		args->items[j + 2].type = MLN_SERVICE_ARG_ID;
		args->items[j + 2].value = secondCountValue;
		for (j = 0; j < secondCount; j++) {
			args->items[firstCount + 3 + j].type = MLN_SERVICE_ARG_BUFFER;
			args->items[firstCount + 3 + j].value = seconds[j];
			seconds[j] = NULL;
		}
		args->items[firstCount + secondCount + 3].type = MLN_SERVICE_ARG_BUFFER;
		args->items[firstCount + secondCount + 3].value = third;
		args->items[firstCount + secondCount + 4].type = MLN_SERVICE_ARG_BUFFER;
		args->items[firstCount + secondCount + 4].value = fourth;
		third = NULL;
		fourth = NULL;
	} else {
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto fail;
		}
	}
	args->items[0].type = MLN_SERVICE_ARG_HANDLE;
	args->items[0].value = value;
	*out = args;
	args = NULL;
	*status = err;
	value = NULL;
	firstCountValue = NULL;
	secondCountValue = NULL;
	goto out;
fail:
	if (ret != 0)
		MlnImpl_810505B8(handle);
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnServiceListFree(list);
	MlnFree(MLN_HEAP_GENERAL, value);
	MlnFree(MLN_HEAP_GENERAL, firstCountValue);
	MlnFree(MLN_HEAP_GENERAL, secondCountValue);
	if (firsts != NULL && firstCount != 0) {
		for (j = 0; j < firstCount; j++)
			MlnBufferDestroy(MLN_HEAP_GENERAL, firsts[j]);
	}
	if (seconds != NULL && secondCount != 0) {
		for (j = 0; j < secondCount; j++)
			MlnBufferDestroy(MLN_HEAP_GENERAL, seconds[j]);
	}
	MlnFree(MLN_HEAP_GENERAL, firsts);
	MlnFree(MLN_HEAP_GENERAL, seconds);
	MlnBufferDestroy(MLN_HEAP_GENERAL, third);
	MlnBufferDestroy(MLN_HEAP_GENERAL, fourth);
	return ret;
}

/* the handle in the reply's one argument, closed */
int MlnImpl_81030648(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
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
	*status = MlnImpl_810505B8(handle);
	*out = NULL;
	return 0;
}

/* a reply of the buffer the core makes from a handle and the reply's buffer */
int MlnImpl_810306C8(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *data = NULL;
	MlnBuffer *object = NULL;
	MlnBlob blob = { NULL, 0 };
	int handle;
	int err, ret;

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
	err = MlnImpl_81050E58(handle, data, &blob);
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
	*status = err;
	object = NULL;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	return ret;
}

/* a reply of the first of two buffers the core makes from a handle and a buffer; the
 * second goes to the caller, and the core may give them with error 29 */
int MlnImpl_81030858(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
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
	err = MlnImpl_81051084(handle, data, &first, &second);
	if (err == 0 || err == 29) {
		object = first.buffer;
		*result = second.buffer;
		second.buffer = NULL;
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
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	return ret;
}

/* a reply of the first of two buffers the core makes from a handle and a buffer; the
 * second goes to the caller */
int MlnImpl_81030A24(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *data = NULL;
	MlnBuffer *object = NULL;
	MlnBlob first = { NULL, 0 };
	MlnBlob second = { NULL, 0 };
	int handle;
	int err, ret;

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
	err = MlnImpl_8105152C(handle, data, &first, &second);
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
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	return ret;
}

/* a reply of the first of two buffers the core makes from a handle and a buffer; the
 * second goes to the caller */
int MlnImpl_81030BD0(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
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
	err = MlnImpl_8105183C(handle, data, &first, &second);
	if (err == 0) {
		object = first.buffer;
		*result = second.buffer;
		second.buffer = NULL;
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
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	return ret;
}

/* the same from a handle and two buffers */
int MlnImpl_81030D90(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *data = NULL;
	MlnBuffer *extra = NULL;
	MlnBuffer *object = NULL;
	MlnBlob first = { NULL, 0 };
	MlnBlob second = { NULL, 0 };
	int handle;
	int err, ret;

	if (result == NULL) {
		ret = 4;
		goto out;
	}
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
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[2], &extra);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	if (handle < 0) {
		ret = 3;
		goto out;
	}
	err = MlnImpl_81051F2C(handle, data, extra, &first, &second);
	if (err == 0) {
		object = first.buffer;
		*result = second.buffer;
		second.buffer = NULL;
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
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnBufferDestroy(MLN_HEAP_GENERAL, extra);
	return ret;
}

/* a reply of the buffer the core makes from a handle and the reply's buffer; the
 * handle is not checked here */
int MlnImpl_81030FA8(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *data = NULL;
	MlnBuffer *object = NULL;
	int handle;
	int err, ret;

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
	err = MlnImpl_8104D018(handle, data, &object);
	if (err == 0) {
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 1, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		args->items[0].type = MLN_SERVICE_ARG_BUFFER;
		args->items[0].value = object;
		object = NULL;
	}
	*out = args;
	args = NULL;
	*status = err;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data);
	MlnBufferDestroy(MLN_HEAP_GENERAL, object);
	return ret;
}

/* a reply of every entry 0x8102B950 lists; its state goes to the caller, also when
 * it fails with error 10 */
int MlnImpl_8103110C(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnServiceArgs *args = NULL;
	unsigned int count = 0;
	MlnBuffer **entries = NULL;
	MlnBlob state = { NULL, 0 };
	unsigned int i;
	int err, ret;

	ret = MlnImpl_81026D94(reply, 0);
	if (ret != 0) {
		ret = 0x200;
		goto out;
	}
	err = MlnImpl_8102B950(&count, &entries, &state);
	if (err != 0) {
		if (err != 10)
			goto reply;
	} else if (count != 0) {
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, count, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		for (i = 0; i < count; i++) {
			args->items[i].type = MLN_SERVICE_ARG_BUFFER;
			args->items[i].value = entries[i];
			entries[i] = NULL;
		}
	}
	*result = state.buffer;
	state.buffer = NULL;
reply:
	*out = args;
	args = NULL;
	*status = err;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	if (entries != NULL) {
		for (i = 0; i < count; i++)
			MlnBufferDestroy(MLN_HEAP_GENERAL, entries[i]);
		MlnFree(MLN_HEAP_GENERAL, entries);
	}
	return ret;
}

/* a reply of every entry 0x8102BDBC gives for the reply's three buffers */
int MlnImpl_81031288(const MlnServiceReply *reply, MlnServiceArgs **out, int *status)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *data[3] = { NULL, NULL, NULL };
	unsigned int count = 0;
	MlnBuffer **entries = NULL;
	unsigned int i;
	int err, ret;

	if (MlnImpl_81026D94(reply, 3) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[0], &data[0]);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[1], &data[1]);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[2], &data[2]);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	err = MlnImpl_8102BDBC(data[0], data[1], data[2], &count, &entries);
	if (err == 0 && count != 0) {
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, count, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		for (i = 0; i < count; i++) {
			args->items[i].type = MLN_SERVICE_ARG_BUFFER;
			args->items[i].value = entries[i];
			entries[i] = NULL;
		}
	}
	*out = args;
	args = NULL;
	*status = err;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	if (entries != NULL) {
		for (i = 0; i < count; i++)
			MlnBufferDestroy(MLN_HEAP_GENERAL, entries[i]);
		MlnFree(MLN_HEAP_GENERAL, entries);
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, data[0]);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data[1]);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data[2]);
	return ret;
}

/* a reply of the two values the core gives for the reply's two buffers; its state goes
 * to the caller, also when it fails with error 10 */
int MlnImpl_8103149C(const MlnServiceReply *reply, MlnServiceArgs **out, int *status,
		     MlnBuffer **result)
{
	MlnServiceArgs *args = NULL;
	MlnBuffer *data[2] = { NULL, NULL };
	unsigned int *first = NULL;
	unsigned int *second = NULL;
	MlnBlob state = { NULL, 0 };
	int err, ret;

	if (MlnImpl_81026D94(reply, 2) != 0) {
		ret = 0x200;
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[0], &data[0]);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	ret = MlnImpl_81026F0C(MLN_HEAP_GENERAL, &reply->items[1], &data[1]);
	if (ret != 0) {
		ret = MlnImpl_8102C64C(ret);
		goto out;
	}
	first = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*first));
	if (first == NULL) {
		ret = 2;
		goto out;
	}
	second = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*second));
	if (second == NULL) {
		ret = 2;
		goto out;
	}
	err = MlnImpl_8104CA50(data[0], data[1], first, second, &state);
	if (err != 0) {
		if (err != 10)
			goto reply;
	} else {
		ret = MlnImpl_81026E20(MLN_HEAP_GENERAL, 2, &args);
		if (ret != 0) {
			ret = MlnImpl_8102C64C(ret);
			goto out;
		}
		args->items[0].type = MLN_SERVICE_ARG_ID;
		args->items[0].value = first;
		first = NULL;
		args->items[1].type = MLN_SERVICE_ARG_ID;
		args->items[1].value = second;
		second = NULL;
	}
	*result = state.buffer;
	state.buffer = NULL;
reply:
	*out = args;
	args = NULL;
	*status = err;
out:
	MlnImpl_81027390(MLN_HEAP_GENERAL, args);
	MlnFree(MLN_HEAP_GENERAL, first);
	MlnFree(MLN_HEAP_GENERAL, second);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data[0]);
	MlnBufferDestroy(MLN_HEAP_GENERAL, data[1]);
	return ret;
}
