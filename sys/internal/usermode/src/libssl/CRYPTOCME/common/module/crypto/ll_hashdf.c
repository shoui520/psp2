/* Filename and descriptive names are ours: CRYPTOCME's derivation function
 * object, the Hash DF of the whitening entropy source.
 */
#include "ll_rand.h"
#include "../../../../memory.h"

/* The object, then its state 8-byte aligned */
#define SSL_HASH_DF_HEADER_WORDS 10

static const SslHashDfDescriptor sslHashDfDescriptor = {
	"Hash DF",
	sizeof(SslHashDfState),
	sslInitHashDfState,
	sslGenerateHashDfState,
	sslSetHashDfStateInfo,
	sslFreeHashDfState,
	{ 0, 0 }
};

const SslHashDfDescriptor *sslGetSystemEntropySource(void)
{
	return &sslHashDfDescriptor;
}

int sslLayOutHashDf(SslHashDf **result, const SslHashDfDescriptor *descriptor,
		    SslHashDf *memory, int *size, SslDirectMemoryContext *dmem)
{
	int offset = 0;
	int status = 0;
	int needed = sizeof(SslHashDf);

	if (descriptor != NULL) {
		needed = ((descriptor->stateSize + 7) & ~7) +
			 SSL_HASH_DF_HEADER_WORDS * 4;
		offset = SSL_HASH_DF_HEADER_WORDS * 4;
	}
	if (memory != NULL) {
		if (*size < needed) {
			status = SSL_CRYPTO_ERROR_NOT_INITIALIZED;
		} else {
			if (dmem != NULL)
				memory->dmem = dmem;
			if (descriptor != NULL) {
				memory->descriptor = descriptor;
				memory->size = needed;
				memory->state = (unsigned char *)memory + offset;
			}
			*result = memory;
		}
	}
	*size = needed;
	return status;
}

int sslFreeHashDf(SslHashDf *df)
{
	const SslHashDfDescriptor *descriptor;
	SslDirectMemoryContext *dmem;

	if (df != NULL) {
		descriptor = df->descriptor;
		dmem = df->dmem;
		if (descriptor != NULL && descriptor->Free != NULL)
			descriptor->Free(df);
		if (df->input[0].data != NULL)
			dmem->methods->Free(df->input[0].data, dmem);
		if (df->input[1].data != NULL)
			dmem->methods->Free(df->input[1].data, dmem);
		if (df->flags & SSL_RAND_OBJECT_ALLOCATED)
			dmem->methods->Free(df, dmem);
	}
	return 0;
}

int sslCreateHashDf(SslHashDf **result, const SslHashDfDescriptor *descriptor,
		    SslDirectMemoryContext *dmem)
{
	SslHashDf *df = *result;
	SslHashDf *allocated;
	SceUInt32 flags;
	int size = 0;
	int status;

	if (df != NULL) {
		size = df->size;
		if (dmem == NULL && (dmem = df->dmem) == NULL)
			return SSL_CRYPTO_ERROR_NO_MEMORY;
	}
	status = sslLayOutHashDf(result, descriptor, df, &size, dmem);
	if (df == NULL || status == SSL_CRYPTO_ERROR_NOT_INITIALIZED) {
		status = dmem->methods->Allocate((void **)&allocated, size,
						 dmem, SSL_DMEM_ALLOCATE_ZERO);
		if (status == 0) {
			sslFreeHashDf(*result);
			flags = allocated->flags | SSL_RAND_OBJECT_ALLOCATED;
			*result = allocated;
			allocated->flags = flags;
			status = sslLayOutHashDf(result, descriptor, allocated,
						 &size, dmem);
		}
	}
	return status;
}

int sslNewHashDf(SslHashDf **result, SslDirectMemoryContext *dmem)
{
	*result = NULL;
	return sslCreateHashDf(result, NULL, dmem);
}

/* The input, and an extra input: each kept in a buffer of the object */
int sslSetHashDfInputs(SslHashDf *df, const void *input, SceUInt32 length,
		       const void *extra, SceUInt32 extraLength, int arg)
{
	const SslHashDfDescriptor *descriptor;
	SslHashDfInput *slot;
	SslDirectMemoryContext *dmem;
	int status;
	SceUInt32 i;

	if (df == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	descriptor = df->descriptor;
	if (descriptor == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	slot = &df->input[0];
	for (i = 0; i < 2; i++) {
		if (extra != NULL) {
			if (slot->length < extraLength) {
				dmem = df->dmem;
				if (slot->data != NULL) {
					dmem->methods->Free(slot->data, df->dmem);
					slot->data = NULL;
					dmem = df->dmem;
				}
				status = dmem->methods->Allocate(
					(void **)&slot->data, extraLength,
					dmem, 0);
				if (status != 0)
					return status;
			}
			sslCopyMemory(slot->data, extra, extraLength);
			slot->length = extraLength;
		}
		extraLength = length;
		extra = input;
		slot = &df->input[1];
	}
	return descriptor->Init(df, arg);
}

int sslGenerateHashDf(SslHashDf *df, SslHashDfOutput *outputs, int count)
{
	return sslGenerateHashDfInline(df, outputs, count);
}

int sslSetHashDfInfo(SslHashDf *df, int id, int arg, const void *value)
{
	return sslSetHashDfInfoInline(df, id, arg, value);
}
