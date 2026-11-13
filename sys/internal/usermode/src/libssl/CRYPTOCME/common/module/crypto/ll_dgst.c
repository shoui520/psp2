/* Filename and descriptive names are ours: CRYPTOCME's low-level digest
 * context: its layout, creation and copies, and the state saved as the
 * hash's chaining values followed by the implementation's own state; the
 * buffering of the Merkle-Damgard hashes of 64-byte blocks (MD5, SHA-1);
 * and HMAC as a digest over another one.
 */
#include "ll_obj.h"
#include "../../object.h"
#include "../dmem/dmem.h"
#include "../../../../memory.h"

int sslSaveDigestBackendContext(SslDigestContext *context,
				unsigned char *buffer, SceUInt32 *length,
				SceUInt32 max)
{
	const SslDigestDescriptor *descriptor = context->descriptor;
	const SslDigestCore *core = descriptor->innerImplementation;
	const SslDigestImplementation *implementation =
		descriptor->implementation;
	SceUInt32 (*transferValues)(SslDigestContext *context,
				    unsigned char *buffer, int save);
	SceUInt32 valuesLength;
	SceUInt32 value;
	int used;
	int status;

	if ((core == NULL || implementation == NULL) &&
	    descriptor->GetInfo != NULL) {
		value = max;
		status = descriptor->GetInfo(descriptor, context,
					     SSL_LL_DIGEST_INFO_SAVE_STATE,
					     &value, (const void **)buffer);
		if (status == 0 && length != NULL)
			*length = value;
		return status;
	}
	if (core == NULL || implementation == NULL ||
	    !(context->flags & SSL_DIGEST_CONTEXT_ALLOCATED))
		return SSL_CRYPTO_ERROR_UNHANDLED;
	transferValues = core->TransferValues;
	if (transferValues == NULL)
		transferValues = sslTransferDigestValues;
	valuesLength = core->valueCount * core->valueSize;
	if (buffer != NULL) {
		if (max < valuesLength)
			return SSL_CRYPTO_ERROR_NO_OPERATION;
		max -= transferValues(context, buffer, 1);
		buffer += valuesLength;
	}
	used = implementation->Transfer(context, buffer, max, 1);
	if (used < 0)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	if (length != NULL)
		*length = valuesLength + used;
	return 0;
}

int sslRestoreDigestBackendContext(SslDigestContext *context,
				   const unsigned char *buffer,
				   SceUInt32 *length, SceUInt32 max)
{
	const SslDigestDescriptor *descriptor;
	const SslDigestImplementation *implementation;
	const SslDigestCore *core;
	SceUInt32 (*transferValues)(SslDigestContext *context,
				    unsigned char *buffer, int save);
	SceUInt32 valuesLength;
	SceUInt32 value;
	int used;
	int status;

	descriptor = context->descriptor;
	implementation = descriptor->implementation;
	core = descriptor->innerImplementation;
	if (buffer == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if ((core == NULL || implementation == NULL) &&
	    descriptor->GetInfo != NULL) {
		value = max;
		status = descriptor->GetInfo(descriptor, context,
					     SSL_LL_DIGEST_INFO_RESTORE_STATE,
					     &value, (const void **)buffer);
		if (status != 0 && length != NULL)
			*length = value;
		return status;
	}
	if (core == NULL || implementation == NULL ||
	    !(context->flags & SSL_DIGEST_CONTEXT_ALLOCATED))
		return SSL_CRYPTO_ERROR_UNHANDLED;
	transferValues = core->TransferValues;
	if (transferValues == NULL)
		transferValues = sslTransferDigestValues;
	valuesLength = core->valueCount * core->valueSize;
	if (max < valuesLength)
		return SSL_CRYPTO_ERROR_NOT_INITIALIZED;
	status = transferValues(context, (unsigned char *)buffer, 0);
	if (status == SSL_CRYPTO_ERROR_BAD_COMMAND)
		return status;
	used = implementation->Transfer(context,
					(unsigned char *)buffer + status,
					max - status, 0);
	if (used < 0)
		return SSL_CRYPTO_ERROR_NOT_INITIALIZED;
	if (length != NULL)
		*length = valuesLength + used;
	return 0;
}

/* The chaining values as big-endian words, the default */
SceUInt32 sslTransferDigestValues(SslDigestContext *context,
				  unsigned char *buffer, int save)
{
	const SslDigestCore *core = context->descriptor->innerImplementation;
	SceUInt32 size = core->valueSize;
	SceUInt32 length = size * core->valueCount;
	unsigned char *to;
	unsigned char *from;
	SceUInt32 i;

	if (buffer == NULL)
		return length;
	to = buffer;
	from = (unsigned char *)context->innerState + core->valueOffset;
	if (!save) {
		to = from;
		from = buffer;
	}
	switch (size) {
	case 1:
		sslCopyMemory(to, from, length);
		break;
	case 4:
		for (i = 0; i < length; i += 4) {
			to[0] = from[3];
			to[1] = from[2];
			to[2] = from[1];
			to[3] = from[0];
			from += 4;
			to += 4;
		}
		break;
	case 8:
		for (i = 0; i < length; i += 8) {
			to[0] = from[7];
			to[1] = from[6];
			to[2] = from[5];
			to[3] = from[4];
			to[4] = from[3];
			to[5] = from[2];
			to[6] = from[1];
			to[7] = from[0];
			from += 8;
			to += 8;
		}
		break;
	default:
		return 0;
	}
	return length;
}

/* The context in memory: this header, the state, then the inner state,
 * each 8-byte aligned. Without memory it only measures.
 */
int sslLayOutDigestContext(SslDigestContext **result,
			   const SslDigestDescriptor *descriptor,
			   void *memory, SceUInt32 *size,
			   SslDirectMemoryContext *dmem)
{
	SslDigestContext *context = memory;
	SceUInt32 headerWords = 0;
	SceUInt32 innerSize = 0;
	SceUInt32 stateSize = 0;
	int status = 0;
	SceUInt32 total = sizeof(SslDigestContext);
	SceUInt32 *state;

	if (descriptor != NULL) {
		headerWords = sizeof(SslDigestContext) / sizeof(SceUInt32);
		if (descriptor->innerImplementation != NULL)
			innerSize = (descriptor->innerImplementation->stateSize +
				     7) & ~7u;
		stateSize = (descriptor->implementation->stateSize + 7) & ~7u;
		total = stateSize + innerSize + sizeof(SslDigestContext) + 8;
	}
	if (context != NULL) {
		if ((int)*size < (int)total) {
			status = SSL_CRYPTO_ERROR_NOT_INITIALIZED;
		} else {
			sslFillMemory(context, 0, total);
			if (dmem != NULL)
				context->dmem = dmem;
			status = 0;
			if (descriptor != NULL) {
				state = (SceUInt32 *)context + headerWords;
				context->size = total;
				context->descriptor = descriptor;
				context->state = state;
				if (descriptor->innerImplementation != NULL)
					context->innerState =
						(unsigned char *)state +
						stateSize;
				else
					context->innerState = NULL;
			}
			*result = context;
		}
	}
	*size = total;
	return status;
}

int sslDestroyDigestBackendContext(SslDigestContext *context)
{
	const SslDigestDescriptor *descriptor;
	SslDirectMemoryContext *dmem;

	if (context != NULL) {
		descriptor = context->descriptor;
		dmem = context->dmem;
		if (descriptor != NULL) {
			if (descriptor->implementation->Free != NULL) {
				descriptor->implementation->Free(context);
			} else if (context->flags &
				   SSL_DIGEST_CONTEXT_STATE_ALLOCATED) {
				sslFreeDirectMemory(context->state,
						    context->dmem);
				context->state = NULL;
			}
			if (descriptor->innerImplementation != NULL &&
			    descriptor->innerImplementation->Free != NULL) {
				descriptor->innerImplementation->Free(context);
			} else if (context->flags &
				   SSL_DIGEST_CONTEXT_INNER_STATE_ALLOCATED) {
				sslFreeDirectMemory(context->innerState, dmem);
				context->innerState = NULL;
			}
		}
		if (context->flags & SSL_DIGEST_CONTEXT_ALLOCATED)
			sslFreeDirectMemory(context, dmem);
	}
	return 0;
}

int sslCreateDigestBackendContext(SslDigestContext **context,
				  const SslDigestDescriptor *descriptor,
				  SslDirectMemoryContext *dmem)
{
	SceUInt32 size = 0;
	void *memory;
	int status;

	if (dmem == NULL) {
		if (*context != NULL)
			dmem = (*context)->dmem;
		if (dmem == NULL)
			return SSL_BIGNUM_ERROR_NO_MEMORY_CONTEXT;
	}
	sslLayOutDigestContext(context, descriptor, NULL, &size, dmem);
	status = sslAllocateDirectMemory(&memory, size, dmem, 256);
	if (status == 0) {
		if (*context != NULL)
			sslDestroyDigestBackendContext(*context);
		*context = NULL;
		status = sslLayOutDigestContext(context, descriptor, memory,
						&size, dmem);
		if (status == 0 && *context != NULL)
			(*context)->flags |= SSL_DIGEST_CONTEXT_ALLOCATED;
	}
	return status;
}

int sslInitDigestBackendContext(SslDigestContext *context)
{
	if (context == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (context->descriptor == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	return context->descriptor->implementation->Init(context);
}

int sslUpdateDigestBackendContext(SslDigestContext *context,
				  const void *data, int length)
{
	if (context == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (context->descriptor == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	return context->descriptor->implementation->Update(context, data,
							   length);
}

int sslFinalizeDigestBackendContext(SslDigestContext *context,
				    unsigned char *digest)
{
	if (context == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (context->descriptor == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	return context->descriptor->implementation->Final(context, digest);
}

int sslSetDigestBackendContextKey(SslDigestContext *context,
				  const void *key, SceUInt32 length)
{
	SceUInt32 value = length;

	return sslGetLowLevelDigestInfo(context->descriptor, context,
					SSL_LL_DIGEST_INFO_SET_KEY, &value,
					(const void **)key);
}

int sslQueryDigestBackendContextInfo(SslDigestContext *context, int selector,
				     SceUInt32 *value, const void **pointer)
{
	return sslGetLowLevelDigestInfo(context->descriptor, context, selector,
					value, pointer);
}

/* The copy runs the descriptor's own copy, else saves the state into a
 * buffer and restores it into the copy
 */
int sslDuplicateDigestBackendContext(SslDigestContext **result,
				     SslDigestContext *context)
{
	int copied = 0;
	SslDigestContext *copy = NULL;
	unsigned char *buffer = NULL;
	SceUInt32 length;
	void *memory;
	int ret = 0;
	int status;
	int init;

	if (result == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (context == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (*result != NULL)
		copy = *result;
	status = sslCreateDigestBackendContext(&copy, context->descriptor,
					       context->dmem);
	if (status == 0) {
		status = sslGetLowLevelDigestInfo(context->descriptor, context,
						  SSL_LL_DIGEST_INFO_COPY, NULL,
						  (const void **)copy);
		if (status == 0 || (copied = status,
				    status == SSL_CRYPTO_ERROR_UNHANDLED)) {
			init = sslInitDigestBackendContext(copy);
			if (init == 0 ||
			    (copied == 0 &&
			     init == SSL_CRYPTO_ERROR_NO_METHODS)) {
				if (sslSaveDigestBackendContext(
					    context, NULL, &length, 0) == 0 &&
				    sslAllocateDirectMemory(&memory, length,
							    context->dmem,
							    256) == 0) {
					buffer = memory;
					if (sslSaveDigestBackendContext(
						    context, buffer, NULL,
						    length) == 0 &&
					    sslRestoreDigestBackendContext(
						    copy, buffer, NULL,
						    length) == 0)
						goto done;
				}
			}
		}
	}
	sslDestroyDigestBackendContext(copy);
	copy = NULL;
done:
	if (buffer != NULL)
		sslFreeDirectMemory(buffer, context->dmem);
	*result = copy;
	return ret;
}

int sslGetDigestBackendContextInfo(SslDigestContext *context, int selector,
				   SceUInt32 *value, const void **pointer)
{
	if (context == NULL || context->descriptor == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	return sslGetLowLevelDigestInfo(context->descriptor, context, selector,
					value, pointer);
}

/* The Merkle-Damgard buffering: whole blocks go to the hash's compression
 * function, the rest waits in a buffer, and the final block carries the
 * length in bits, little-endian for MD5 and big-endian for SHA-1.
 */
#define SSL_MD_BLOCK_SIZE 64
#define SSL_MD_LENGTH_OFFSET 56

/* The state: the waiting bytes and the length so far */
typedef struct SslMdBuffer {
	unsigned char data[SSL_MD_BLOCK_SIZE];
	SceUInt32 count;
	SceUInt32 lengthLow;
	SceUInt32 lengthHigh;
} SslMdBuffer;

_Static_assert(sizeof(SslMdBuffer) == 76, "SslMdBuffer");

int sslInitMdBuffer(SslDigestContext *context);
int sslFinalMdBufferBigEndian(SslDigestContext *context,
			      unsigned char *digest);
int sslFinalMdBufferLittleEndian(SslDigestContext *context,
				 unsigned char *digest);
int sslTransferMdBuffer(SslDigestContext *context, unsigned char *buffer,
			SceUInt32 length, int save);
int sslUpdateMdBuffer(SslDigestContext *context, const void *data,
		      int length);

const SslDigestImplementation sslMdBigEndianImplementation = {
	sslInitMdBuffer,
	sslUpdateMdBuffer,
	sslFinalMdBufferBigEndian,
	NULL,
	sslTransferMdBuffer,
	sizeof(SslMdBuffer),
	0
};

const SslDigestImplementation sslMdLittleEndianImplementation = {
	sslInitMdBuffer,
	sslUpdateMdBuffer,
	sslFinalMdBufferLittleEndian,
	NULL,
	sslTransferMdBuffer,
	sizeof(SslMdBuffer),
	0
};

int sslInitMdBuffer(SslDigestContext *context)
{
	SslMdBuffer *buffer = context->state;

	buffer->count = 0;
	buffer->lengthLow = 0;
	buffer->lengthHigh = 0;
	return context->descriptor->innerImplementation->Init(
		context->innerState);
}

int sslFinalMdBufferBigEndian(SslDigestContext *context,
			      unsigned char *digest)
{
	SslMdBuffer *buffer = context->state;
	int (*compress)(void *state, const void *data, SceUInt32 length) =
		context->descriptor->innerImplementation->Compress;
	SceUInt32 count = buffer->count;
	SceUInt32 next = count + 1;
	SceUInt32 low, high;
	SceUInt32 i;

	buffer->data[count] = 0x80;
	if (next > SSL_MD_LENGTH_OFFSET) {
		for (i = next; i < SSL_MD_BLOCK_SIZE; i++)
			buffer->data[i] = 0;
		compress(context->innerState, buffer->data,
			 SSL_MD_BLOCK_SIZE);
		sslFillMemory(buffer->data, 0, SSL_MD_LENGTH_OFFSET);
	} else {
		sslFillMemory(&buffer->data[next], 0,
			      SSL_MD_LENGTH_OFFSET - next);
	}
	low = buffer->lengthLow;
	buffer->data[63] = low << 3;
	buffer->data[62] = low >> 5;
	buffer->data[61] = low >> 13;
	buffer->data[60] = low >> 21;
	high = (low >> 29) | (buffer->lengthHigh << 3);
	buffer->data[59] = high;
	buffer->data[58] = high >> 8;
	buffer->data[57] = high >> 16;
	buffer->data[56] = high >> 24;
	compress(context->innerState, buffer->data, SSL_MD_BLOCK_SIZE);
	context->descriptor->innerImplementation->Output(context->innerState,
							 digest);
	buffer = context->state;
	buffer->count = 0;
	buffer->lengthLow = 0;
	buffer->lengthHigh = 0;
	context->descriptor->innerImplementation->Init(context->innerState);
	return 0;
}

int sslFinalMdBufferLittleEndian(SslDigestContext *context,
				 unsigned char *digest)
{
	SslMdBuffer *buffer = context->state;
	int (*compress)(void *state, const void *data, SceUInt32 length) =
		context->descriptor->innerImplementation->Compress;
	SceUInt32 count = buffer->count;
	SceUInt32 next = count + 1;
	SceUInt32 low, high;
	SceUInt32 i;

	buffer->data[count] = 0x80;
	if (next > SSL_MD_LENGTH_OFFSET) {
		for (i = next; i < SSL_MD_BLOCK_SIZE; i++)
			buffer->data[i] = 0;
		compress(context->innerState, buffer->data,
			 SSL_MD_BLOCK_SIZE);
		sslFillMemory(buffer->data, 0, SSL_MD_LENGTH_OFFSET);
	} else {
		sslFillMemory(&buffer->data[next], 0,
			      SSL_MD_LENGTH_OFFSET - next);
	}
	low = buffer->lengthLow;
	buffer->data[56] = low << 3;
	buffer->data[57] = low >> 5;
	buffer->data[58] = low >> 13;
	buffer->data[59] = low >> 21;
	high = (low >> 29) | (buffer->lengthHigh << 3);
	buffer->data[60] = high;
	buffer->data[61] = high >> 8;
	buffer->data[62] = high >> 16;
	buffer->data[63] = high >> 24;
	compress(context->innerState, buffer->data, SSL_MD_BLOCK_SIZE);
	context->descriptor->innerImplementation->Output(context->innerState,
							 digest);
	buffer = context->state;
	buffer->count = 0;
	buffer->lengthLow = 0;
	buffer->lengthHigh = 0;
	context->descriptor->innerImplementation->Init(context->innerState);
	return 0;
}

/* Saved as the length, big-endian high word first, then the waiting
 * bytes
 */
int sslTransferMdBuffer(SslDigestContext *context, unsigned char *out,
			SceUInt32 length, int save)
{
	SslMdBuffer *buffer = context->state;
	SceUInt32 low;

	if (save) {
		if (out == NULL)
			return buffer->count + 8;
		if (length < buffer->count + 8)
			return -1;
		out[3] = buffer->lengthHigh;
		out[2] = buffer->lengthHigh >> 8;
		out[1] = buffer->lengthHigh >> 16;
		out[0] = buffer->lengthHigh >> 24;
		out[7] = buffer->lengthLow;
		out[6] = buffer->lengthLow >> 8;
		out[5] = buffer->lengthLow >> 16;
		out[4] = buffer->lengthLow >> 24;
		sslCopyMemory(out + 8, buffer->data, buffer->count);
		return buffer->count + 8;
	}
	if (out == NULL)
		return SSL_MD_BLOCK_SIZE + 8;
	if (length < 8)
		return -1;
	buffer->lengthHigh = ((SceUInt32)out[0] << 24) |
			     ((SceUInt32)out[1] << 16) |
			     ((SceUInt32)out[2] << 8) | out[3];
	low = ((SceUInt32)out[4] << 24) | ((SceUInt32)out[5] << 16) |
	      ((SceUInt32)out[6] << 8) | out[7];
	buffer->lengthLow = low;
	buffer->count = low & (SSL_MD_BLOCK_SIZE - 1);
	if (length < (low & (SSL_MD_BLOCK_SIZE - 1)) + 8)
		return -1;
	sslCopyMemory(buffer->data, out + 8, low & (SSL_MD_BLOCK_SIZE - 1));
	return buffer->count + 8;
}

int sslUpdateMdBuffer(SslDigestContext *context, const void *data,
		      int length)
{
	const unsigned char *input = data;
	SslMdBuffer *buffer;
	int (*compress)(void *state, const void *data, SceUInt32 length);
	SceUInt32 low;
	SceUInt32 count;
	SceUInt32 rest;
	int whole;

	if (length > 0) {
		buffer = context->state;
		low = buffer->lengthLow;
		compress = context->descriptor->innerImplementation->Compress;
		if (low + length < low)
			buffer->lengthHigh++;
		buffer->lengthLow = low + length;
		count = buffer->count;
		if (count != 0) {
			if (count + length < SSL_MD_BLOCK_SIZE) {
				sslCopyMemory(&buffer->data[count], input,
					      length);
				buffer->count = count + length;
				return 0;
			}
			rest = SSL_MD_BLOCK_SIZE - count;
			sslCopyMemory(&buffer->data[count], input, rest);
			length -= rest;
			input += rest;
			compress(context->innerState, buffer->data,
				 SSL_MD_BLOCK_SIZE);
			buffer->count = 0;
		}
		if (length >= SSL_MD_BLOCK_SIZE) {
			whole = length & ~(SSL_MD_BLOCK_SIZE - 1);
			length -= whole;
			if ((SceUIntPtr)input & 3) {
				do {
					sslCopyMemory(buffer->data, input,
						      SSL_MD_BLOCK_SIZE);
					input += SSL_MD_BLOCK_SIZE;
					compress(context->innerState,
						 buffer->data,
						 SSL_MD_BLOCK_SIZE);
					whole -= SSL_MD_BLOCK_SIZE;
				} while (whole != 0);
			} else {
				compress(context->innerState, input, whole);
				input += whole;
			}
		}
		buffer->count = length;
		if (length > 0)
			sslCopyMemory(buffer->data, input, length);
	}
	return 0;
}

/* HMAC: the key, XORed with the pads, keeps the inner and outer digests'
 * states after their first block, so each message restarts from them.
 */
#define SSL_HMAC_INNER_PAD 0x36
#define SSL_HMAC_OUTER_PAD 0x5c

/* The state of an HMAC context */
typedef struct SslHmacState {
	SceUInt32 unused;
	unsigned char *key;
	SslDigestContext *digest;
	SceUInt32 keyLength;
	SceUInt32 digestLength;
	unsigned char *innerSaved;
	unsigned char *outerSaved;
	SceUInt32 savedLength;
	void *scratch;
} SslHmacState;

_Static_assert(sizeof(SslHmacState) == 36, "SslHmacState");

int sslInitHmacDigest(SslDigestContext *context);
int sslUpdateHmacDigest(SslDigestContext *context, const void *data,
			int length);
int sslFinalHmacDigest(SslDigestContext *context, unsigned char *digest);
int sslFreeHmacDigest(SslDigestContext *context);
int sslGetHmacDigestInfo(const SslDigestDescriptor *descriptor,
			 SslDigestContext *context, int selector,
			 SceUInt32 *value, const void **pointer);

static const SslDigestImplementation sslHmacImplementation = {
	sslInitHmacDigest,
	sslUpdateHmacDigest,
	sslFinalHmacDigest,
	sslFreeHmacDigest,
	NULL,
	sizeof(SslHmacState),
	0
};

static const unsigned char sslHmacOid[] = { 0x06, 0x03, 0x29, 0x01, 0x01 };

static const SslDigestDescriptor sslHmacDigestDescriptor = {
	"hmac",
	{ sizeof(sslHmacOid), sslHmacOid },
	0, 0x10, 0,
	&sslHmacImplementation,
	NULL,
	sslGetHmacDigestInfo,
	1,
	0x3000,
	0
};

const SslDigestDescriptor *sslGetHmacDigestDescriptor(void)
{
	return &sslHmacDigestDescriptor;
}

/* Puts the digest under the HMAC, its key buffer as long as its block */
int sslSetHmacDigest(SslHmacState *state,
		     const SslDigestDescriptor *descriptor,
		     SslDirectMemoryContext *dmem)
{
	SceUInt32 value;
	void *memory;
	int status;

	status = sslCreateDigestBackendContext(&state->digest, descriptor,
					       dmem);
	if (status == 0) {
		sslInitDigestBackendContext(state->digest);
		sslFreeDirectMemory(state->scratch, dmem);
		state->scratch = NULL;
		status = sslGetLowLevelDigestInfo(
			descriptor, state->digest,
			SSL_LL_DIGEST_INFO_BLOCK_LENGTH, &value, NULL);
		if (status == 0) {
			if (value > state->keyLength) {
				status = sslAllocateDirectMemory(&memory, value,
								 dmem, 256);
				if (status != 0)
					goto fail;
				sslCopyMemory(memory, state->key,
					      state->keyLength);
				sslFreeDirectMemory(state->key, dmem);
				state->key = memory;
			}
			state->keyLength = value;
			status = sslGetLowLevelDigestInfo(
				descriptor, state->digest,
				SSL_LL_DIGEST_INFO_DIGEST_LENGTH, &value, NULL);
			if (status == 0) {
				state->digestLength = value;
				sslFreeDirectMemory(state->innerSaved, dmem);
				state->innerSaved = NULL;
				return 0;
			}
		}
fail:
		sslDestroyDigestBackendContext(state->digest);
		state->digest = NULL;
	}
	return status;
}

int sslGetHmacDigestInfo(const SslDigestDescriptor *descriptor,
			 SslDigestContext *context, int selector,
			 SceUInt32 *value, const void **pointer)
{
	SslHmacState *state = context->state;
	unsigned char *out = (unsigned char *)pointer;
	const unsigned char *in;
	SceUInt32 max;
	SceUInt32 used;
	SceUInt32 rest;
	SceUInt32 saved;
	SceUInt32 size;
	SslDirectMemoryContext *dmem;
	const unsigned char *key;
	SceUInt32 length;
	SceUInt32 aligned;
	SceUInt32 i = 0;
	SceUInt32 j;
	int status = 0;

	switch (selector) {
	case SSL_LL_DIGEST_INFO_BLOCK_LENGTH:
	case SSL_LL_DIGEST_INFO_DIGEST_LENGTH:
		if (state == NULL)
			return SSL_CRYPTO_ERROR_NO_IMPLEMENTATION;
		status = sslGetLowLevelDigestInfo(state->digest->descriptor,
						  state->digest, selector,
						  value, pointer);
		break;
	case SSL_LL_DIGEST_INFO_CONTEXT_SIZE:
		size = ((descriptor->implementation->stateSize + 7) & ~7u) +
		       sizeof(SslDigestContext);
		if (value != NULL)
			*value = size;
		break;
	case SSL_LL_DIGEST_INFO_SET_DIGEST:
		status = sslSetHmacDigest(
			state, (const SslDigestDescriptor *)pointer,
			context->dmem);
		break;
	case SSL_LL_DIGEST_INFO_FLAGS:
		*value = descriptor->flags;
		break;
	case SSL_LL_DIGEST_INFO_SET_KEY:
		/* The key, hashed if longer than a block, padded and run
		 * through the digest's first block for the inner and the outer
		 * state
		 */
		if (state->digest == NULL)
			return SSL_CRYPTO_ERROR_NO_METHODS;
		key = (const unsigned char *)pointer;
		length = *value;
		if (key != NULL) {
			if (length <= state->keyLength) {
				for (i = 0; i < length; i++)
					state->key[i] =
						key[i] ^ SSL_HMAC_INNER_PAD;
			} else {
				sslInitDigestBackendContext(state->digest);
				sslUpdateDigestBackendContext(state->digest,
							      key, length);
				sslFinalizeDigestBackendContext(state->digest,
								state->key);
				for (i = 0; i < state->digestLength; i++)
					state->key[i] ^= SSL_HMAC_INNER_PAD;
			}
			for (; i < state->keyLength; i++)
				state->key[i] = SSL_HMAC_INNER_PAD;
		}
		sslInitDigestBackendContext(state->digest);
		sslUpdateDigestBackendContext(state->digest, state->key,
					      state->keyLength);
		if (key != NULL || state->innerSaved == NULL) {
			sslSaveDigestBackendContext(state->digest, NULL,
						    &length, 0);
			sslFreeDirectMemory(state->innerSaved, context->dmem);
			state->innerSaved = NULL;
			aligned = (length + 7) & ~7u;
			length = aligned;
			status = sslAllocateDirectMemory(
				(void **)&state->innerSaved, aligned * 2,
				context->dmem, 0);
			if (status != 0)
				break;
			state->outerSaved = state->innerSaved + length;
			state->savedLength = length;
			status = sslSaveDigestBackendContext(
				state->digest, state->innerSaved, NULL, length);
			if (status != 0)
				break;
		}
		for (j = 0; j < state->keyLength; j++)
			state->key[j] ^=
				SSL_HMAC_INNER_PAD ^ SSL_HMAC_OUTER_PAD;
		sslInitDigestBackendContext(state->digest);
		sslUpdateDigestBackendContext(state->digest, state->key,
					      state->keyLength);
		sslSaveDigestBackendContext(state->digest, state->outerSaved,
					    NULL, state->savedLength);
		for (j = 0; j < state->keyLength; j++)
			state->key[j] ^= SSL_HMAC_OUTER_PAD;
		break;
	case SSL_LL_DIGEST_INFO_SAVE_STATE:
		max = *value;
		status = sslSaveDigestBackendContext(state->digest, out, &used,
						     max);
		if (status != 0)
			break;
		if (out != NULL)
			out += used;
		if (max - used < state->savedLength * 2 + state->keyLength + 6)
			return SSL_CRYPTO_ERROR_NO_OPERATION;
		if (out != NULL) {
			out[0] = state->keyLength >> 8;
			out[1] = state->keyLength;
			out[2] = state->digestLength >> 8;
			out[3] = state->digestLength;
			out[4] = state->savedLength >> 8;
			out[5] = state->savedLength;
			sslCopyMemory(out + 6, state->key, state->keyLength);
			out += state->keyLength + 6;
			sslCopyMemory(out, state->innerSaved,
				      state->savedLength);
			sslCopyMemory(out + state->savedLength,
				      state->outerSaved, state->savedLength);
		}
		*value = state->keyLength + state->savedLength * 2 + used + 6;
		break;
	case SSL_LL_DIGEST_INFO_RESTORE_STATE:
		max = *value;
		status = sslRestoreDigestBackendContext(
			state->digest, (const unsigned char *)pointer, &used,
			max);
		if (status != 0)
			break;
		rest = max - used;
		in = (const unsigned char *)pointer + used;
		if (rest < 6)
			return SSL_CRYPTO_ERROR_NO_OPERATION;
		state->keyLength = in[1] | (in[0] << 8);
		state->digestLength = in[3] | (in[2] << 8);
		saved = state->savedLength;
		state->savedLength = in[5] | (in[4] << 8);
		if (rest - 6 < state->keyLength + state->savedLength * 2)
			return SSL_CRYPTO_ERROR_NO_OPERATION;
		if (saved < state->savedLength || state->innerSaved == NULL) {
			dmem = context->dmem;
			if (state->innerSaved != NULL) {
				sslFreeDirectMemory(state->innerSaved,
						    context->dmem);
				dmem = context->dmem;
			}
			status = sslAllocateDirectMemory(
				(void **)&state->innerSaved,
				state->savedLength * 2, dmem, 0);
			if (status != 0)
				break;
			state->outerSaved =
				state->innerSaved + state->savedLength;
		}
		in += 6;
		sslCopyMemory(state->key, in, state->keyLength);
		in += state->keyLength;
		sslCopyMemory(state->innerSaved, in, state->savedLength);
		sslCopyMemory(state->outerSaved, in + state->savedLength,
			      state->savedLength);
		break;
	case SSL_LL_DIGEST_INFO_COPY:
		status = sslSetHmacDigest(
			((SslDigestContext *)pointer)->state,
			state->digest->descriptor, context->dmem);
		break;
	default:
		status = SSL_CRYPTO_ERROR_UNHANDLED;
		break;
	}
	return status;
}

int sslInitHmacDigest(SslDigestContext *context)
{
	SslHmacState *state = context->state;

	if (state->innerSaved != NULL && state->digest != NULL)
		return sslRestoreDigestBackendContext(state->digest,
						      state->innerSaved, NULL,
						      state->savedLength);
	return SSL_CRYPTO_ERROR_NO_METHODS;
}

int sslUpdateHmacDigest(SslDigestContext *context, const void *data,
			int length)
{
	SslHmacState *state = context->state;

	return sslUpdateDigestBackendContext(state->digest, data, length);
}

int sslFinalHmacDigest(SslDigestContext *context, unsigned char *digest)
{
	SslHmacState *state = context->state;
	int status;

	status = sslFinalizeDigestBackendContext(state->digest, digest);
	if (status == 0) {
		status = sslRestoreDigestBackendContext(state->digest,
							state->outerSaved, NULL,
							state->savedLength);
		if (status == 0) {
			status = sslUpdateDigestBackendContext(
				state->digest, digest, state->digestLength);
			if (status == 0)
				return sslFinalizeDigestBackendContext(
					state->digest, digest);
		}
	}
	return status;
}

int sslFreeHmacDigest(SslDigestContext *context)
{
	SslHmacState *state = context->state;

	sslDestroyDigestBackendContext(state->digest);
	sslFreeDirectMemory(state->key, context->dmem);
	sslFreeDirectMemory(state->innerSaved, context->dmem);
	sslFreeDirectMemory(state->scratch, context->dmem);
	return 0;
}
