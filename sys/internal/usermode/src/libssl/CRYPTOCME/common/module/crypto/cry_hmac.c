/* Filename and private descriptive names are ours; CRYPTOCME HMAC backend over
 * the low-level digest layer.
 */
#include "cry_api.h"
#include "ll_obj.h"
#include "../dmem/dmem.h"
#include "../../key/key_api.h"
#include "../../../../memory.h"

static void sslFreeInline(void *ptr)
{
	sslFreeCallback(ptr);
}

static void *sslAllocateInline(SceUInt32 size)
{
	return sslAllocCallback(size);
}

#define sslFree sslFreeInline
#define sslAllocate sslAllocateInline

/* Key information selector of sslGetKeyInfo: the raw key bytes */
#define SSL_KEY_INFO_DATA 14

/* GetInfo selectors */
#define SSL_HMAC_INFO_MAC_LENGTH 11
#define SSL_HMAC_INFO_BLOCK_LENGTH 13

#define SSL_HMAC_MAX_MAC_LENGTH 64

typedef struct SslHmacBackendData {
	const void *(*GetDigestDescriptor)(void);
} SslHmacBackendData;

typedef struct SslHmacBackendState {
	void *context;
	void *key;
} SslHmacBackendState;

_Static_assert(sizeof(SslHmacBackendState) == 8, "SslHmacBackendState");

int sslHmacBackendInitialize(SslCryptoAlgorithm *algorithm)
{
	const SslHmacBackendData *data = algorithm->implementationData;
	void *context = NULL;
	SslHmacBackendState *state = NULL;
	const void *hmac = sslGetHmacDigestDescriptor();
	SslDirectMemoryContext *dmem = sslGetDefaultDirectMemoryContextInline();
	int status;

	status = sslCreateDigestBackendContext(&context, hmac, dmem);
	if (status == 0) {
		status = sslGetDigestBackendContextInfo(
			context, 16, NULL,
			(SceUInt32)data->GetDigestDescriptor());
		if (status == 0) {
			state = sslAllocate(sizeof(*state));
			if (state == NULL) {
				status = SSL_LIBRARY_ERROR_ALLOCATION;
			} else {
				int i;

				for (i = 0; i < (int)sizeof(*state); i++)
					((unsigned char *)state)[i] = 0;
				state->context = context;
				algorithm->backendState = state;
				return status;
			}
		}
	}
	if (context != NULL)
		sslDestroyDigestBackendContext(context);
	if (state != NULL)
		sslFree(state);
	return status;
}

int sslHmacBackendInit(SslCryptoAlgorithm *algorithm, void *key)
{
	SslHmacBackendState *state = algorithm->backendState;
	SslCryptoContextHeader *context;
	SslDesKeyBuffer keyData;
	void *digest;
	int status;

	if (state == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	digest = state->context;
	if (digest == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (key == NULL) {
		if (state->key == NULL)
			return SSL_CRYPTO_ERROR_INVALID_STATE;
		status = sslGetKeyInfo(state->key, SSL_KEY_INFO_DATA, &keyData);
		if (status != 0)
			return status;
	} else {
		if (state->key != NULL) {
			sslDestroyKey(state->key);
			state->key = NULL;
		}
		status = sslGetKeyInfo(key, SSL_KEY_INFO_DATA, &keyData);
		if (status != 0)
			return status;
		context = algorithm->context;
		status = sslCreateKey(context->library, 1, &keyData,
				      &state->key);
		if (status != 0)
			return status;
	}
	status = sslSetDigestBackendContextKey(digest, keyData.data,
					       keyData.length);
	if (status == 0)
		status = sslInitDigestBackendContext(digest);
	return status;
}

int sslHmacBackendDestroy(SslCryptoAlgorithm *algorithm)
{
	SslHmacBackendState *state = algorithm->backendState;

	if (state != NULL) {
		if (state->context != NULL)
			sslDestroyDigestBackendContext(state->context);
		if (state->key != NULL)
			sslDestroyKey(state->key);
		sslFree(state);
		algorithm->backendState = NULL;
	}
	return 0;
}

int sslHmacBackendCopy(SslCryptoAlgorithm *source,
		       SslCryptoAlgorithm *destination)
{
	SslHmacBackendState *dst = destination->backendState;
	SslHmacBackendState *src;
	SslCryptoContextHeader *context;
	SslDesKeyBuffer keyData;
	void *digest;
	int status;

	if (dst == NULL || (src = source->backendState) == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	digest = NULL;
	if (src->context == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (dst->context != NULL) {
		sslDestroyDigestBackendContext(dst->context);
		dst->context = NULL;
	}
	if (dst->key != NULL) {
		sslDestroyKey(dst->key);
		dst->key = NULL;
	}
	status = sslGetKeyInfo(src->key, SSL_KEY_INFO_DATA, &keyData);
	if (status == 0) {
		context = source->context;
		status = sslCreateKey(context->library, 1, &keyData,
				      &dst->key);
		if (status == 0) {
			status = sslDuplicateDigestBackendContext(&digest,
								  src->context);
			if (status == 0) {
				dst->context = digest;
				destination->backendState = dst;
				return status;
			}
		}
	}
	if (dst != NULL && destination->backendState == NULL) {
		if (dst->context != NULL)
			sslDestroyDigestBackendContext(dst->context);
		if (dst->key != NULL)
			sslDestroyKey(dst->key);
		sslFree(dst);
	}
	return status;
}

int sslHmacBackendGetInfo(SslCryptoAlgorithm *algorithm, int selector,
			  SceUInt32 *result)
{
	const SslHmacBackendData *data = algorithm->implementationData;
	SslHmacBackendState *state;
	const void *descriptor;
	SceUInt32 length;
	int status;

	if (data == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	length = 0;
	descriptor = data->GetDigestDescriptor();
	if (descriptor == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	switch (selector) {
	case SSL_HMAC_INFO_MAC_LENGTH:
		status = sslGetLowLevelDigestInfo(descriptor, NULL, 5, &length,
						  0);
		*result = length;
		return status;
	case SSL_HMAC_INFO_BLOCK_LENGTH:
		state = algorithm->backendState;
		if (state == NULL)
			return SSL_CRYPTO_ERROR_INVALID_STATE;
		if (state->context == NULL)
			return SSL_CRYPTO_ERROR_INVALID_STATE;
		status = sslGetLowLevelDigestInfo(descriptor, state->context, 4,
						  &length, 0);
		*result = length;
		return status;
	}
	return SSL_CRYPTO_ERROR_NO_OPERATION;
}

int sslHmacBackendUpdate(SslCryptoAlgorithm *algorithm, const void *data,
			 SceUInt32 length)
{
	SslHmacBackendState *state = algorithm->backendState;

	if (state == NULL || state->context == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	return sslUpdateDigestBackendContext(state->context, data, length);
}

int sslHmacBackendFinal(SslCryptoAlgorithm *algorithm, void *mac,
			SceUInt32 *macLength)
{
	SslHmacBackendState *state;
	SceUInt32 length = 0;
	int status;

	status = sslHmacBackendGetInfo(algorithm, SSL_HMAC_INFO_MAC_LENGTH,
				       &length);
	if (status == 0) {
		state = algorithm->backendState;
		status = sslFinalizeDigestBackendContext(state->context, mac);
		if (status == 0)
			*macLength = length;
	}
	return status;
}

int sslHmacBackendOperate(SslCryptoAlgorithm *algorithm, const void *data,
			  SceUInt32 length, void *mac, SceUInt32 *macLength)
{
	int status = sslHmacBackendUpdate(algorithm, data, length);

	if (status == 0)
		status = sslHmacBackendFinal(algorithm, mac, macLength);
	return status;
}

int sslHmacBackendVerifyFinal(SslCryptoAlgorithm *algorithm,
			      const void *expected, SceUInt32 expectedLength,
			      int *result)
{
	SslHmacBackendState *state;
	unsigned char mac[SSL_HMAC_MAX_MAC_LENGTH];
	SceUInt32 length;
	int status;

	*result = SSL_CRYPTO_ERROR_FAILED;
	length = 0;
	status = sslHmacBackendGetInfo(algorithm, SSL_HMAC_INFO_MAC_LENGTH,
				       &length);
	if (status == 0) {
		state = algorithm->backendState;
		status = sslFinalizeDigestBackendContext(state->context, mac);
		if (status == 0 && length == expectedLength &&
		    sslCompareMemory(mac, expected, length) == 0)
			*result = 0;
	}
	return status;
}

int sslHmacBackendVerify(SslCryptoAlgorithm *algorithm, const void *data,
			 SceUInt32 length, const void *expected,
			 SceUInt32 expectedLength, int *result)
{
	int status = sslHmacBackendUpdate(algorithm, data, length);

	*result = SSL_CRYPTO_ERROR_FAILED;
	if (status == 0)
		status = sslHmacBackendVerifyFinal(algorithm, expected,
						   expectedLength, result);
	return status;
}

static const SslCryptoImplementationMethods sslHmacBackendMethods = {
	sslHmacBackendInitialize,
	sslHmacBackendDestroy,
	sslHmacBackendCopy,
	(int (*)(SslCryptoAlgorithm *, int, void *))sslHmacBackendGetInfo,
	NULL,
	NULL,
	sslHmacBackendInit,
	sslHmacBackendOperate,
	sslHmacBackendUpdate,
	sslHmacBackendFinal,
	sslHmacBackendVerify,
	sslHmacBackendUpdate,
	sslHmacBackendVerifyFinal
};

const SslCryptoImplementationMethods *sslGetHmacBackendMethods(void)
{
	return &sslHmacBackendMethods;
}
