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

/* restores the core's state */
int MlnImpl_8102A7F0(const MlnBuffer *state)
{
	int ret = MlnImpl_81043A6C(state);

	if (ret != 0)
		ret = MlnImpl_8102A228(ret);
	return ret;
}

/* clears the core's state */
int MlnImpl_8102A914(void)
{
	int ret = MlnImpl_81043A78();

	if (ret != 0)
		ret = MlnImpl_8102A228(ret);
	return ret;
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
	int ret;
	int err;

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
	int ret;

	if (state == NULL)
		return 4;
	copy = *state;
	ret = MlnImpl_8103B29C(&copy);
	if (ret == 0)
		ret = MlnImpl_81043D7C(&copy);
	if (ret != 0)
		ret = MlnImpl_8102A228(ret);
	return ret;
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

int MlnImpl_8102B064(const MlnBuffer *id, MlnBlob *out)
{
	MlnBuffer *buffer = NULL;
	int ret;

	(void)id;
	if (out == NULL)
		return 4;
	ret = MlnImpl_810393D0(0, &buffer);
	if (ret == 0) {
		out->buffer = buffer;
		out->size = buffer->size;
	} else {
		ret = MlnImpl_8102A228(ret);
	}
	return ret;
}

/* a 20-byte identifier for the core */
int MlnImpl_8102B198(const MlnBuffer *id)
{
	int ret;

	if (id == NULL || id->size != 20 || id->data == NULL)
		return 4;
	ret = MlnImpl_81047DC4(id);
	if (ret != 0)
		ret = MlnImpl_8102A228(ret);
	return ret;
}

int MlnImpl_8102B2E0(void)
{
	int ret = MlnImpl_81047E00();

	if (ret != 0)
		ret = MlnImpl_8102A228(ret);
	return ret;
}

/* the flags of the core and of an identifier: a flag in the top nibble is an error */
int MlnImpl_8102B404(void *id, unsigned int *flags)
{
	unsigned int coreFlags = 0;
	unsigned int idFlags = 0;
	unsigned int value = 0;
	int ret;

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
	int ret;

	if (out == NULL) {
		ret = 4;
	} else if (id != NULL && (id->size != 20 || id->data == NULL)) {
		ret = 4;
	} else {
		ret = MlnImpl_810486D4(id);
		if (ret == 0) {
			ret = MlnImpl_81043A60(1, &buffer);
			if (ret == 0) {
				out->buffer = buffer;
				out->size = buffer->size;
				buffer = NULL;
			}
		}
		if (ret != 0)
			ret = MlnImpl_8102A228(ret);
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/* the core's state after an identifier is removed */
int MlnImpl_8102B718(const MlnBuffer *id, MlnBlob *out)
{
	MlnBuffer *buffer = NULL;
	int ret;

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
	int ret, err;
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

/* adds an entry; an existing entry for the identifier is moved first */
int MlnImpl_8102BDBC(const MlnBuffer *first, const MlnBuffer *second, const MlnBuffer *id,
		     void *a3, void *a4)
{
	MlnBuffer *entry = NULL;
	MlnCoreRecord *record = NULL;
	MlnBuffer **base = NULL;
	MlnBuffer *key;
	int ret, err;

	if (first == NULL || first->size == 0 || first->data == NULL || second == NULL ||
	    second->size == 0 || second->data == NULL || a3 == NULL || a4 == NULL) {
		ret = 4;
		goto out;
	}
	if (id != NULL && id->size != 0 && id->data != NULL) {
		ret = MlnImpl_8104436C(id, &entry);
		if (ret != 0x230A0003) {
			if (ret != 0) {
				ret = MlnImpl_8102A228(ret);
				if (ret != 0)
					goto out;
			} else {
				err = MlnImpl_810422E8(entry, &record);
				if (err != 0) {
					if (err == 2) {
						ret = err;
						goto out;
					}
					ret = 10;
					MlnImpl_81043F1C(entry);
					goto out;
				}
				err = MlnImpl_81043B94(&base);
				if (err == 0) {
					key = NULL;
					if (MlnBufferCompare(MLN_HEAP_GENERAL, *base, record->key) != 0)
						key = record->key;
					err = MlnImpl_81044984(key, record->value, first, second);
				}
				if (err != 0) {
					ret = MlnImpl_8102A228(err);
					goto out;
				}
			}
		}
	}
	ret = MlnImpl_810444DC(first, second, a3, a4);
	if (ret == 0x230A0003)
		ret = 0;
	else if (ret != 0)
		ret = MlnImpl_8102A228(ret);
out:
	MlnImpl_8103B8F8(base);
	MlnImpl_8103BBA0(record);
	MlnBufferDestroy(MLN_HEAP_GENERAL, entry);
	return ret;
}

int MlnImpl_8102C08C(void *context, const MlnBuffer *data, MlnBlob *out)
{
	MlnBuffer *buffer = NULL;
	int ret;

	if (out == NULL) {
		ret = 4;
		goto out;
	}
	ret = MlnImpl_810458B0(context, data);
	if (ret == 0) {
		ret = MlnImpl_81043A60(1, &buffer);
		if (ret == 0) {
			out->buffer = buffer;
			out->size = buffer->size;
			buffer = NULL;
			goto out;
		}
	}
	ret = MlnImpl_8102A228(ret);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/* the client's name boxes: a box of the string's bytes */
static inline int MlnNameBoxInit(MlnBox *box, const char *type, const char *name)
{
	unsigned int len = MlnStrlen(name);

	if (len == 0)
		return 4;
	box->size = len + 8;
	box->type[0] = type[0];
	box->type[1] = type[1];
	box->type[2] = type[2];
	box->type[3] = type[3];
	box->data = MlnAllocate(MLN_HEAP_GENERAL, len);
	if (box->data == NULL)
		return 2;
	MlnMemcpy(box->data, name, len);
	return 0;
}

static inline int MlnAttributeBoxWrite(const MlnAttributeBox *box, unsigned char *out)
{
	unsigned int size;

	if (out == NULL)
		return 4;
	size = box->size;
	out[0] = size >> 24;
	out[1] = size >> 16;
	out[3] = size;
	out[2] = size >> 8;
	MlnMemcpy(out + 4, box->type, sizeof(box->type));
	out[8] = box->flags[0];
	out[9] = box->flags[1];
	out[11] = box->count;
	out[10] = box->count >> 8;
	out[13] = box->encoding;
	out[12] = box->encoding >> 8;
	out[15] = box->bits;
	out[14] = box->bits >> 8;
	MlnMemcpy(out + 16, box->data, box->bits >> 3);
	return 0;
}

/* what SceDrmBridge keeps between the identity messages it signs */
static unsigned char s_authState[64];

/* the client's identity message: its maker, model and version, and the device ID */
int MlnImpl_8102C1E8(unsigned char kind, MlnBlob *out)
{
	MlnBox mnfc, mdel, mver;
	MlnAttributeBox duid;
	unsigned long long tick;
	unsigned char input[32];
	unsigned char output[128];
	MlnBuffer *device;
	MlnBuffer *message = NULL;
	MlnBuffer *result = NULL;
	unsigned char *p;
	int ret;

	mnfc.data = NULL;
	mdel.data = NULL;
	mver.data = NULL;
	duid.data = NULL;
	ret = MlnNameBoxInit(&mnfc, "MNFC", "Sony");
	if (ret != 0)
		goto out;
	ret = MlnNameBoxInit(&mdel, "MDEL", "Nautilus Client");
	if (ret != 0)
		goto out;
	ret = MlnNameBoxInit(&mver, "MVER", "1.2.1");
	if (ret != 0)
		goto out;

	/* the secure tick in milliseconds since the Unix epoch */
	if (sceDrmBridgeGetCurrentSecureTick(&tick) == 0)
		tick = (tick - 62135596800000000ULL) / 1000;
	else
		MlnMemset(&tick, 0, sizeof(tick));
	MlnMemset(input, 0, sizeof(input));
	MlnMemset(output, 0, sizeof(output));
	input[0] = kind;
	sceDrmBridgeMlnpsnlAuth1(input, &tick, output, s_authState);

	device = MlnBufferCreate(MLN_HEAP_GENERAL, sizeof(output), output);
	if (device == NULL || device->size == 0)
		goto no_device;
	duid.size = device->size + 16;
	duid.type[0] = 'D';
	duid.type[1] = 'U';
	duid.type[2] = 'I';
	duid.type[3] = 'D';
	duid.flags[0] = 0;
	duid.flags[1] = 0;
	duid.encoding = 5;
	duid.count = 1;
	duid.bits = device->size * 8;
	duid.data = MlnAllocate(MLN_HEAP_GENERAL, device->size);
	if (duid.data == NULL)
		goto no_device;
	MlnMemcpy(duid.data, device->data, device->size);
	MlnBufferDestroy(MLN_HEAP_GENERAL, device);

	message = MlnBufferCreate(MLN_HEAP_GENERAL,
				  mnfc.size + mdel.size + 4 + mver.size + duid.size, NULL);
	if (message == NULL) {
		ret = 2;
		goto out;
	}
	p = message->data;
	p[0] = 0;
	p[1] = 0x10;
	p[2] = 0;
	p[3] = 0;
	p += 4;
	ret = MlnImpl_8102A33C(&mnfc, p);
	if (ret != 0)
		goto out;
	p += mnfc.size;
	ret = MlnImpl_8102A33C(&mdel, p);
	if (ret != 0)
		goto out;
	p += mdel.size;
	ret = MlnImpl_8102A33C(&mver, p);
	if (ret != 0)
		goto out;
	p += mver.size;
	ret = MlnAttributeBoxWrite(&duid, p);
	if (ret == 0) {
		result = message;
		message = NULL;
	}
	goto out;
no_device:
	ret = 2;
	MlnBufferDestroy(MLN_HEAP_GENERAL, device);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, message);
	MlnFree(MLN_HEAP_GENERAL, mnfc.data);
	mnfc.data = NULL;
	MlnFree(MLN_HEAP_GENERAL, mdel.data);
	mdel.data = NULL;
	MlnFree(MLN_HEAP_GENERAL, mver.data);
	mver.data = NULL;
	MlnFree(MLN_HEAP_GENERAL, duid.data);
	if (ret == 0) {
		out->buffer = result;
		out->size = result->size;
		result = NULL;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, result);
	return ret;
}

