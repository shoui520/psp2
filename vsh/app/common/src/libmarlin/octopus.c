/*
 * The library's calls into the DRM core (Octopus, NEMO): its start and stop, counted in
 * the shared state, and the core's errors as the library's. The file's name is ours.
 */
#include "mln_internal.h"

/* a core error as the library's */
int MlnImpl_8102A228(unsigned int error)
{
	switch (error) {
	case 0:
	case 2:
		return error;
	case 6:
		return 68;
	case 0x230A0002:
		return 16;
	case 0x230A0003:
		return 18;
	case 0x230A0004:
		return 9;
	case 0x230A0005:
		return 10;
	case 0x230A0006:
		return 19;
	case 0x230A0007:
		return 12;
	case 0x230A000F:
		return 19;
	case 0x230A0011:
		return 17;
	case 0x230A0012:
		return 22;
	case 0x230A0013:
		return 67;
	case 0x23110005:
		return 16;
	default:
		return 7;
	}
}

/* a box as bytes: its size, big-endian, its type and its payload */
int MlnImpl_8102A33C(const MlnBox *box, unsigned char *out)
{
	unsigned int size = box->size;

	out[3] = size;
	out[1] = size >> 16;
	out[2] = size >> 8;
	out[0] = size >> 24;
	MlnMemcpy(out + 4, box->type, sizeof(box->type));
	MlnMemcpy(out + 8, box->data, box->size - 8);
	return 0;
}

/* the core's first start sets up the general heap */
int MlnImpl_8102A394(void *memory, unsigned int size)
{
	MlnShared *shared = MlnImpl_81006EAC();
	unsigned int ret = 0;

	if (shared != NULL && ++shared->coreRefCount == 1) {
		ret = MlnMemoryInit(MLN_HEAP_GENERAL, memory, size);
		if (ret == 0) {
			MlnImpl_81080BF4();
		} else {
			shared = MlnImpl_81006EAC();
			if (shared != NULL)
				shared->coreRefCount--;
		}
	}
	/* the memory layer's errors as the library's */
	switch (ret) {
	case 0:
		return 0;
	case 1:
		return 7;
	case 2:
		return 2;
	default:
		return 7;
	}
}

/* the core's last stop takes down the core and the general heap */
int MlnImpl_8102A418(void)
{
	MlnShared *shared = MlnImpl_81006EAC();

	if (shared == NULL || shared->coreRefCount == 0)
		return 8;
	shared = MlnImpl_81006EAC();
	if (shared != NULL && --shared->coreRefCount > 0)
		return 0;
	MlnImpl_8103B29C(NULL);
	MlnImpl_81043A78();
	MlnImpl_81080BF8();
	MlnMemoryTerm(MLN_HEAP_GENERAL);
	return 0;
}

/* sets the core's state up: 1 the first step, 2 the second, 0 all of them */
int MlnImpl_8102A484(unsigned int mode)
{
	void *handle = NULL;
	unsigned int ret;

	if (mode > 2) {
		ret = 4;
	} else if (MlnImpl_8104A448() != 0) {
		ret = 7;
	} else {
		if (mode == 1) {
			ret = MlnImpl_81043C70(mode);
		} else if (mode == 2) {
			ret = MlnImpl_81080BFC(&handle);
			if (ret == 0)
				ret = MlnImpl_81081374(0);
		} else {
			ret = MlnImpl_81043C70(2);
			if (ret == 0) {
				ret = MlnImpl_81080BFC(&handle);
				if (ret == 0) {
					ret = MlnImpl_81081374(1);
					if (ret == 0)
						ret = MlnImpl_81043C70(3);
				}
			}
		}
		MlnImpl_8104A558();
	}
	if (mode != 1)
		MlnImpl_81080C1C(handle);
	switch (ret) {
	case 0:
	case 4:
		return ret;
	case 6:
		MlnImpl_810485B4();
		return 68;
	case 0x230A0002:
		return 16;
	case 0x23110005:
		MlnImpl_810485B4();
		return 16;
	default:
		return 7;
	}
}

/* a copy of the core object's bytes */
int MlnImpl_8102A5B0(void *id, MlnBlob *out)
{
	MlnBuffer **object = NULL;
	MlnBuffer *buffer;
	int locked = 0;
	int ret;

	if (id == NULL || out == NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, NULL);
		return 4;
	}
	if (MlnImpl_8104A448() != 0) {
		ret = 7;
	} else {
		locked = 1;
		ret = MlnImpl_81042A60(id, &object, 0);
		if (ret != 0) {
			if (ret != 2)
				ret = 11;
		} else {
			buffer = MlnBufferCreate(MLN_HEAP_GENERAL, (*object)->size, (*object)->data);
			if (buffer != NULL) {
				out->buffer = buffer;
				out->size = buffer->size;
			} else {
				ret = 2;
			}
		}
	}
	if (object != NULL)
		MlnImpl_8103B8F8(object);
	MlnBufferDestroy(MLN_HEAP_GENERAL, NULL);
	if (locked == 1)
		MlnImpl_8104A558();
	return ret;
}

/* the core's state as a buffer */
int MlnImpl_8102A69C(MlnBlob *out)
{
	MlnBuffer *buffer = NULL;
	int ret;

	if (out == NULL) {
		ret = 4;
	} else {
		ret = MlnImpl_8102A228(MlnImpl_81043A60(1, &buffer));
		if (ret == 0) {
			out->buffer = buffer;
			out->size = buffer->size;
			buffer = NULL;
		}
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/* restores the core's state */
int MlnImpl_8102A7F0(const MlnBuffer *state)
{
	return MlnImpl_8102A228(MlnImpl_81043A6C(state));
}

/* clears the core's state */
int MlnImpl_8102A914(void)
{
	return MlnImpl_8102A228(MlnImpl_81043A78());
}

/* a blob's bytes into the caller's buffer, and the blob freed */
int MlnImpl_8102AA38(MlnBlob *blob, MlnBuffer *out)
{
	MlnBuffer *buffer;

	if (blob == NULL)
		return 4;
	if (out != NULL && out->size != 0) {
		buffer = blob->buffer;
		if (buffer != NULL) {
			if (buffer->size != blob->size)
				return 7;
			if (out->size < buffer->size)
				return 33;
			MlnMemcpy(out->data, buffer->data, buffer->size);
			out->size = buffer->size;
		}
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, blob->buffer);
	blob->buffer = NULL;
	blob->size = 0;
	return 0;
}

int MlnImpl_8102AAD0(void *p)
{
	MlnFree(MLN_HEAP_GENERAL, p);
	return 0;
}

/* loads the core's saved state once: *loaded says whether there is one */
int MlnImpl_8102AAE8(int *loaded)
{
	MlnBuffer *state = NULL;
	int present = 0;
	unsigned int ret;
	unsigned int err;

	if (loaded == NULL) {
		ret = 4;
	} else if (MlnImpl_8104A448() != 0) {
		ret = 7;
	} else {
		ret = MlnImpl_8103B190(&present);
		if (ret != 0) {
			ret = MlnImpl_8102A228(ret);
		} else if (present) {
			*loaded = 1;
		} else {
			err = MlnImpl_81043CF0(&state);
			if (err != 0) {
				MlnImpl_8103B29C(NULL);
				if (err == 0x230A000E)
					*loaded = 0;
				else
					ret = MlnImpl_8102A228(err);
			} else {
				err = MlnImpl_8103B29C(state);
				if (err != 0) {
					if (err == 2 || err == 5)
						ret = MlnImpl_8102A228(err);
					else
						*loaded = 0;
				} else {
					ret = MlnImpl_8103B190(&present);
					if (ret != 0)
						ret = MlnImpl_8102A228(ret);
					else
						*loaded = present ? 1 : 0;
				}
			}
		}
		MlnImpl_8104A558();
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, state);
	return ret;
}

/* loads a state into the core and stores it */
int MlnImpl_8102AE84(const MlnBuffer *state)
{
	MlnBuffer copy;
	unsigned int ret;

	if (state == NULL)
		return 4;
	copy = *state;
	ret = MlnImpl_8103B29C(&copy);
	if (ret == 0)
		ret = MlnImpl_81043D7C(&copy);
	return MlnImpl_8102A228(ret);
}

MlnCoreInfo MlnImpl_8102AFBC(void)
{
	MlnCoreInfo info;
	MlnCoreInfo found;

	MlnMemset(&info, 0, sizeof(info));
	if (MlnImpl_8104A448() == 0) {
		if (MlnImpl_8103B3EC(&found) == 0)
			info = found;
		MlnImpl_8104A558();
	}
	return info;
}

unsigned int MlnImpl_8102B054(void)
{
	return MlnImpl_8103B29C(NULL);
}

int MlnImpl_8102B064(int unused, MlnBlob *out)
{
	MlnBuffer *buffer = NULL;
	unsigned int ret;

	(void)unused;
	if (out == NULL)
		return 4;
	ret = MlnImpl_810393D0(0, &buffer);
	if (ret == 0) {
		out->buffer = buffer;
		out->size = buffer->size;
		return 0;
	}
	return MlnImpl_8102A228(ret);
}

/* a 20-byte identifier for the core */
int MlnImpl_8102B198(const MlnBuffer *id)
{
	if (id == NULL || id->size != 20 || id->data == NULL)
		return 4;
	return MlnImpl_8102A228(MlnImpl_81047DC4(id));
}

int MlnImpl_8102B2E0(void)
{
	return MlnImpl_8102A228(MlnImpl_81047E00());
}

/* the flags of the core and of an identifier: a flag in the top nibble is an error */
int MlnImpl_8102B404(void *id, unsigned int *flags)
{
	unsigned int coreFlags = 0;
	unsigned int idFlags = 0;
	unsigned int value = 0;
	unsigned int ret;

	if (id == NULL || flags == NULL) {
		ret = 4;
	} else {
		*flags = 0;
		ret = MlnImpl_81048214(&coreFlags);
		if (ret == 0) {
			ret = MlnImpl_81047E48(id, &idFlags);
			if (ret == 0) {
				value = coreFlags | idFlags;
				if (value & 0xF0000000)
					ret = 68;
				goto out;
			}
			if (ret == 3) {
				ret = 4;
				goto out;
			}
		}
		ret = MlnImpl_8102A228(ret);
	}
out:
	if (flags != NULL)
		*flags = value;
	return ret;
}

/* the core's state for an identifier, or for all of them */
int MlnImpl_8102B590(const MlnBuffer *id, MlnBlob *out)
{
	MlnBuffer *buffer = NULL;
	unsigned int ret;

	if (out == NULL) {
		ret = 4;
	} else if (id != NULL && (id->size != 20 || id->data == NULL)) {
		ret = 4;
	} else {
		ret = MlnImpl_810486D4(id);
		if (ret == 0)
			ret = MlnImpl_81043A60(1, &buffer);
		if (ret == 0) {
			out->buffer = buffer;
			out->size = buffer->size;
			buffer = NULL;
		} else {
			ret = MlnImpl_8102A228(ret);
		}
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/* the core's state after an identifier is removed */
int MlnImpl_8102B718(const MlnBuffer *id, MlnBlob *out)
{
	MlnBuffer *buffer = NULL;
	unsigned int ret;

	if (id == NULL || out == NULL || id->size != 20 || id->data == NULL) {
		ret = 4;
	} else {
		ret = MlnImpl_810488A8(id);
		if (ret != 0) {
			ret = MlnImpl_8102A228(ret);
		} else {
			ret = MlnImpl_81043A60(1, &buffer);
			if (ret != 0) {
				ret = MlnImpl_8102A228(ret);
			} else {
				out->buffer = buffer;
				out->size = buffer->size;
				buffer = NULL;
			}
		}
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/*
 * The core's entries as key and value buffers, two per entry. An entry the core cannot
 * read is dropped from it; then the entries are not returned, the core's new state is,
 * and the result is 10.
 */
int MlnImpl_8102B950(unsigned int *count, MlnBuffer ***entries, MlnBlob *state)
{
	void *iterator = NULL;
	MlnBuffer **base = NULL;
	MlnBuffer *entry = NULL;
	MlnBuffer *newState = NULL;
	MlnCoreRecord *record = NULL;
	MlnBuffer **array;
	unsigned int ret, err;
	int n = 0;
	int all, i, j;

	if (count == NULL || entries == NULL) {
		ret = 4;
		goto out;
	}
	ret = MlnImpl_81045A84();
	if (ret == 0)
		ret = MlnImpl_8104411C(&n, &iterator);
	if (ret != 0) {
		ret = MlnImpl_8102A228(ret);
		goto out;
	}
	if (n <= 0) {
		array = NULL;
		goto done;
	}
	array = MlnAllocate(MLN_HEAP_GENERAL, n * 8);
	if (array == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(array, 0, n * 8);
	err = MlnImpl_81043B94(&base);
	if (err != 0) {
		ret = MlnImpl_8102A228(err);
		goto fail;
	}
	if (n <= 0)
		goto done;
	all = 1;
	for (i = 0; i < n; i++) {
		err = MlnImpl_810442D4(&iterator, &entry);
		if (err != 0) {
			ret = MlnImpl_8102A228(err);
			goto fail;
		}
		err = MlnImpl_810422E8(entry, &record);
		if (err != 0) {
			if (err == 2) {
				ret = err;
				goto fail;
			}
			MlnImpl_81043F1C(entry);
			MlnBufferDestroy(MLN_HEAP_GENERAL, entry);
			all = 0;
			entry = NULL;
		} else {
			if (MlnBufferCompare(MLN_HEAP_GENERAL, *base, record->key) != 0) {
				array[2 * i] = record->key;
				record->key = NULL;
			}
			array[2 * i + 1] = record->value;
			record->value = NULL;
			MlnBufferDestroy(MLN_HEAP_GENERAL, entry);
			MlnImpl_8103BBA0(record);
			entry = NULL;
			record = NULL;
		}
	}
	if (all)
		goto done;
	err = MlnImpl_81043A60(1, &newState);
	if (err != 0) {
		ret = MlnImpl_8102A228(err);
		goto fail;
	}
	state->buffer = newState;
	state->size = newState->size;
	newState = NULL;
	ret = 10;
fail:
	for (j = 0; j < 2 * n; j++)
		MlnBufferDestroy(MLN_HEAP_GENERAL, array[j]);
	MlnFree(MLN_HEAP_GENERAL, array);
	goto out;
done:
	*count = 2 * n;
	*entries = array;
out:
	MlnImpl_8104435C(iterator);
	MlnBufferDestroy(MLN_HEAP_GENERAL, entry);
	MlnBufferDestroy(MLN_HEAP_GENERAL, newState);
	MlnImpl_8103BBA0(record);
	MlnImpl_8103B8F8(base);
	return ret;
}
