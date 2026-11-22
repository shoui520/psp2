/*
 * SceLibMarlin's side of the Marlin HCI: the core's calls into SceMarlinHci's syscalls. A key is
 * the HCI's handle; what an operation makes (a signature, a decrypted text, an exported key)
 * the HCI keeps under a new handle, which is read out into a buffer of the handle's size.
 */
#include <marlin_hci/hci.h>
#include "mln_internal.h"

#define MLN_HCI_ERROR_MEMORY	2
#define MLN_HCI_ERROR_ARGUMENT	3
#define MLN_HCI_CHUNK_MAX	0x800	/* the most one decrypt call takes */
#define MLN_HCI_IV_SIZE		16	/* the block before each chunk (its IV) */

/* the core's first start: the HCI layer has nothing to set up */
int MlnImpl_81080BF4(void)
{
	return 0;
}

/* the core's last stop: nothing to take down */
void MlnImpl_81080BF8(void)
{
}

/* takes the HCI's lock; the token (4 bytes) goes back to the unlock */
unsigned int MlnImpl_81080BFC(void **handle)
{
	unsigned int ret = MLN_HCI_ERROR_ARGUMENT;

	if (handle == NULL)
		return ret;
	ret = SceMarlinHci_4406594F();
	if (ret != 0)
		return ret;
	*handle = MlnAllocate(MLN_HEAP_GENERAL, 4);
	return ret;
}

unsigned int MlnImpl_81080C1C(void *handle)
{
	unsigned int ret = MLN_HCI_ERROR_ARGUMENT;

	if (handle == NULL)
		return ret;
	ret = SceMarlinHci_457F6683();
	if (ret != 0)
		return ret;
	MlnFree(MLN_HEAP_GENERAL, handle);
	return ret;
}

/* the key the HCI stores of a kind */
unsigned int MlnImpl_81080C3C(int kind, void **key)
{
	unsigned int handle = 0;
	unsigned int ret;

	if (key == NULL)
		return MLN_HCI_ERROR_ARGUMENT;
	ret = SceMarlinHci_C3E201D5(kind, &handle);
	if (ret == 0)
		*key = (void *)handle;
	return ret;
}

int MlnImpl_81080C74(void *key)
{
	if (key == NULL)
		return 0;
	return SceMarlinHci_A626EBBC((unsigned int)key);
}

int MlnImpl_81080C80(void *key)
{
	if (key == NULL)
		return MLN_HCI_ERROR_ARGUMENT;
	return SceMarlinHci_9EA124CA((unsigned int)key);
}

/* reads what the HCI keeps under a handle into a new buffer (none when it is empty) */
static inline unsigned int MlnHciTakeBuffer(unsigned int handle, unsigned int *size,
					    MlnBuffer **out)
{
	unsigned int ret;
	MlnBuffer *buffer;

	ret = SceMarlinHci_527EB697(handle, size);
	if (ret != 0)
		return ret;
	buffer = NULL;
	if (*size != 0) {
		buffer = MlnBufferCreate(MLN_HEAP_GENERAL, *size, NULL);
		if (buffer == NULL)
			return MLN_HCI_ERROR_MEMORY;
		ret = SceMarlinHci_C889F85B(handle, buffer->size, buffer->data);
		if (ret != 0) {
			MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
			return ret;
		}
	}
	*out = buffer;
	return 0;
}

/* signs data with a key */
unsigned int MlnImpl_81080C90(void *key, int kind, const MlnBuffer *data, MlnBuffer **out)
{
	unsigned int handle = 0;
	unsigned int size = 0;
	unsigned int ret;

	if (key == NULL || out == NULL)
		return MLN_HCI_ERROR_ARGUMENT;
	ret = SceMarlinHci_B7FB96EC((unsigned int)key, kind, (const MarlinUserDataInput *)data,
				    &handle);
	if (ret != 0)
		return ret;
	return MlnHciTakeBuffer(handle, &size, out);
}

/*
 * decrypts data with a key; kind 2 (CBC) goes in chunks of at most 2 KiB, each given with the
 * 16 bytes before it as its IV, and only the last with the caller's mode
 */
unsigned int MlnImpl_81080D18(void *key, int kind, int mode, const void *parameters,
			      const MlnBuffer *b, const MlnBuffer *data, MlnBuffer **out)
{
	MarlinKeyDecryptRequest request;
	unsigned int size = 0;
	unsigned int ret;
	MlnBuffer *buffer;
	unsigned int remaining, total, step;
	const unsigned char *from;
	unsigned char *to;

	if (key == NULL || out == NULL)
		return MLN_HCI_ERROR_ARGUMENT;
	MlnMemset(&request, 0, sizeof(request));
	request.keyHandle = (unsigned int)key;
	request.algorithm = kind;
	request.mode = mode;
	request.counter = parameters;
	if (b != NULL) {
		request.labelSize = b->size;
		request.labelData = b->data;
	}
	if (data != NULL) {
		request.encryptedSize = data->size;
		request.encryptedData = data->data;
	}
	total = 0;
	request.outputHandle = 0;
	if (kind != 2) {
		ret = SceMarlinHci_AE0FBC3F(&request);
		if (ret != 0)
			return ret;
		ret = SceMarlinHci_527EB697(request.outputHandle, &size);
		if (ret != 0)
			return ret;
		buffer = NULL;
		if (size != 0) {
			buffer = MlnBufferCreate(MLN_HEAP_GENERAL, size, NULL);
			if (buffer == NULL)
				return MLN_HCI_ERROR_MEMORY;
			ret = SceMarlinHci_C889F85B(request.outputHandle, buffer->size, buffer->data);
			if (ret != 0)
				goto fail;
		}
		*out = buffer;
		return 0;
	}
	buffer = MlnBufferCreate(MLN_HEAP_GENERAL, data->size, NULL);
	if (buffer == NULL)
		return MLN_HCI_ERROR_MEMORY;
	remaining = data->size - MLN_HCI_IV_SIZE;
	from = data->data + MLN_HCI_IV_SIZE;
	to = buffer->data;
	while (remaining != 0) {
		unsigned int chunk = remaining + MLN_HCI_IV_SIZE;

		if (chunk > MLN_HCI_CHUNK_MAX) {
			chunk = MLN_HCI_CHUNK_MAX;
			request.mode = 0;
			step = MLN_HCI_CHUNK_MAX - MLN_HCI_IV_SIZE;
		} else {
			request.mode = mode;
			step = remaining;
		}
		request.encryptedSize = chunk;
		request.encryptedData = from - MLN_HCI_IV_SIZE;
		ret = SceMarlinHci_AE0FBC3F(&request);
		if (ret != 0)
			goto fail;
		ret = SceMarlinHci_527EB697(request.outputHandle, &size);
		if (ret != 0)
			goto fail;
		ret = SceMarlinHci_C889F85B(request.outputHandle, size, to);
		if (ret != 0)
			goto fail;
		remaining -= step;
		from += step;
		total += size;
		to += size;
	}
	buffer->size = total;
	*out = buffer;
	return 0;
fail:
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/* a key unwrapped by another */
unsigned int MlnImpl_81080E80(void *wrapping, int kind, const MlnBuffer *param,
			      const MlnBuffer *data, int usage, void **key)
{
	MarlinWrappedKeyImport request;
	unsigned int ret;

	if (wrapping == NULL || key == NULL)
		return MLN_HCI_ERROR_ARGUMENT;
	MlnMemset(&request, 0, sizeof(request));
	request.wrappingKeyHandle = (unsigned int)wrapping;
	request.algorithm = kind;
	if (param != NULL) {
		request.labelSize = param->size;
		request.labelData = param->data;
	}
	if (data != NULL) {
		request.inputSize = data->size;
		request.inputData = data->data;
	}
	request.kind = usage;
	request.outputHandle = 0;
	ret = SceMarlinHci_24D94277(&request);
	if (ret == 0)
		*key = (void *)request.outputHandle;
	return ret;
}

/* a key derived from one of the HCI's seeds (a broadcast key block's node) */
unsigned int MlnImpl_81080EF8(int shift, int node, const MlnBuffer *key, const MlnBuffer *id,
			      const MlnBuffer *iv, const MlnBuffer *data, void **out)
{
	MarlinDerivedKeyImport request;
	unsigned int ret;

	if (out == NULL)
		return MLN_HCI_ERROR_ARGUMENT;
	MlnMemset(&request, 0, sizeof(request));
	request.iterations = shift;
	request.seedIndex = node;
	if (key != NULL) {
		request.wrappedSeedSize = key->size;
		request.wrappedSeedData = key->data;
	}
	if (id != NULL) {
		request.checkSize = id->size;
		request.checkData = id->data;
	}
	if (iv != NULL) {
		request.ivSize = iv->size;
		request.ivData = iv->data;
	}
	if (data != NULL) {
		request.encryptedSize = data->size;
		request.encryptedData = data->data;
	}
	request.outputHandle = 0;
	ret = SceMarlinHci_00F544DF(&request);
	if (ret == 0)
		*out = (void *)request.outputHandle;
	return ret;
}

/* checks a MAC over a message */
int MlnImpl_81080F80(void *key, int sha256, const MlnBuffer *message,
		     const MlnBuffer *signature, int *valid)
{
	MarlinKeyVerifyRequest request;
	int ret;

	if (key == NULL || valid == NULL)
		return MLN_HCI_ERROR_ARGUMENT;
	MlnMemset(&request, 0, sizeof(request));
	request.keyHandle = (unsigned int)key;
	request.algorithm = sha256;
	if (message != NULL) {
		request.dataSize = message->size;
		request.data = message->data;
	}
	if (signature != NULL) {
		request.expectedSize = signature->size;
		request.expectedData = signature->data;
	}
	request.verified = 0;
	ret = SceMarlinHci_CB99E399(&request);
	if (ret == 0)
		*valid = request.verified;
	return ret;
}

/* checks a signature over a digest */
int MlnImpl_81080FF4(void *key, int sha256, const MlnBuffer *message,
		     const MlnBuffer *digest, int *valid)
{
	MarlinKeyVerifyRequest request;
	int ret;

	MlnMemset(&request, 0, sizeof(request));
	if (key == NULL || valid == NULL)
		return MLN_HCI_ERROR_ARGUMENT;
	request.keyHandle = (unsigned int)key;
	request.algorithm = sha256;
	if (message != NULL) {
		request.dataSize = message->size;
		request.data = message->data;
	}
	if (digest != NULL) {
		request.expectedSize = digest->size;
		request.expectedData = digest->data;
	}
	request.verified = 0;
	ret = SceMarlinHci_44390C58(&request);
	if (ret == 0)
		*valid = request.verified;
	return ret;
}

/* disables a key, given its check data; what the HCI hands back */
unsigned int MlnImpl_8108106C(void *key, const MlnBuffer *data, MlnBuffer **out)
{
	unsigned int handle = 0;
	unsigned int size = 0;
	unsigned int ret;

	if (key == NULL || out == NULL)
		return MLN_HCI_ERROR_ARGUMENT;
	ret = SceMarlinHci_EAD075E3((unsigned int)key, (const MarlinUserDataInput *)data, &handle);
	if (ret != 0)
		return ret;
	return MlnHciTakeBuffer(handle, &size, out);
}

/* a key, encrypted for export */
unsigned int MlnImpl_810810F4(void *key, MlnBuffer **out)
{
	unsigned int handle = 0;
	unsigned int size = 0;
	unsigned int ret;

	if (key == NULL || out == NULL)
		return MLN_HCI_ERROR_ARGUMENT;
	ret = SceMarlinHci_68315912((unsigned int)key, &handle);
	if (ret != 0)
		return ret;
	return MlnHciTakeBuffer(handle, &size, out);
}

/*
 * a key wrapped by another for export: the key store passes its message's size and bytes, which
 * the HCI reads as the RSA modulus and exponent records
 */
unsigned int MlnImpl_8108117C(void *key, unsigned int messageSize, const void *message,
			      MlnBuffer **out)
{
	unsigned int handle = 0;
	unsigned int size = 0;
	unsigned int ret;

	if (key == NULL || out == NULL)
		return MLN_HCI_ERROR_ARGUMENT;
	ret = SceMarlinHci_F7303D28((unsigned int)key, (const MarlinUserDataInput *)messageSize,
				    (const MarlinUserDataInput *)message, &handle);
	if (ret != 0)
		return ret;
	return MlnHciTakeBuffer(handle, &size, out);
}

/* the names of the keys the HCI stores, one buffer each (none for an empty one) */
unsigned int MlnImpl_81081204(MlnBuffer ***names, unsigned int *count)
{
	unsigned int size = 0;
	unsigned int number = 0;
	unsigned int handles[16];
	MlnBuffer **list;
	unsigned int ret, i;

	if (names == NULL || count == NULL)
		return MLN_HCI_ERROR_ARGUMENT;
	ret = SceMarlinHci_CC2BA3B5(handles, &number);
	if (ret != 0)
		return ret;
	list = MlnAllocate(MLN_HEAP_GENERAL, number * sizeof(*list));
	if (list == NULL)
		return MLN_HCI_ERROR_MEMORY;
	MlnMemset(list, 0, number * sizeof(*list));
	for (i = 0; i < number; i++) {
		ret = SceMarlinHci_527EB697(handles[i], &size);
		if (ret != 0)
			goto fail;
		if (size == 0)
			continue;
		list[i] = MlnBufferCreate(MLN_HEAP_GENERAL, size, NULL);
		if (list[i] == NULL) {
			ret = MLN_HCI_ERROR_MEMORY;
			goto fail;
		}
		ret = SceMarlinHci_C889F85B(handles[i], list[i]->size, list[i]->data);
		if (ret != 0)
			goto fail;
	}
	*count = number;
	*names = list;
	return 0;
fail:
	for (i = 0; i < number; i++) {
		if (list[i] != NULL)
			MlnBufferDestroy(MLN_HEAP_GENERAL, list[i]);
	}
	MlnFree(MLN_HEAP_GENERAL, list);
	return ret;
}

/* the HCI's node id */
unsigned int MlnImpl_810812EC(MlnBuffer **nodeId)
{
	unsigned int handle = 0;
	unsigned int size = 0;
	unsigned int ret;

	if (nodeId == NULL)
		return MLN_HCI_ERROR_ARGUMENT;
	ret = SceMarlinHci_4DADE2FD(&handle);
	if (ret != 0)
		return ret;
	return MlnHciTakeBuffer(handle, &size, nodeId);
}

unsigned int MlnImpl_8108136C(int full, unsigned int size, const void *data)
{
	return SceMarlinHci_BFBA3A79(full, size, data);
}

unsigned int MlnImpl_81081374(int full)
{
	return SceMarlinHci_E8A352C2(full);
}
