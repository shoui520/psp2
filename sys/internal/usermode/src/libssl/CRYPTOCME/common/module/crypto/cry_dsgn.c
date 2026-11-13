/* Filename and private descriptive names are ours; CRYPTOCME digest-sign backend. */
#include "cry_api.h"
#include "../dmem/dmem.h"
#include "../../key/pkey_pk.h"
#include "../../../../memory.h"

int sslNewBigNumContext(void **ctx, SslDirectMemoryContext *dmem);
void sslFreeBigNumContext(void *ctx, int flag);
int sslNewBigNum(void **bn, SslDirectMemoryContext *dmem);
void sslFreeBigNum(void *bn, int flag);
int sslSetBigNumData(void *bn, const void *data, SceUInt32 length, void *ctx);
int sslGetBigNumBitLength(void *bn);
const void *sslGetRsaPkcs1Type1EncodeMethod(void);
const void *sslGetRsaBlindingMethod(void);
const void *sslGetRsaPrivateReciprocalMethod(void);
const void *sslGetRsaPkcs1Type1DecodeMethod(void);
const void *sslGetRsaPublicReciprocalMethod(void);

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

typedef struct SslDigestSignBackendState {
	SslCryptoAlgorithm *digestAlgorithm;
	SslCryptoAlgorithm *signAlgorithm;
	SceUInt32 flags;
	void *buffer;
	SceUInt32 bufferLength;
} SslDigestSignBackendState;

int sslDigestSignBackendInitialize(SslCryptoAlgorithm *algorithm);
int sslDigestSignBackendDestroy(SslCryptoAlgorithm *algorithm);
int sslDigestSignBackendGetInfo(SslCryptoAlgorithm *algorithm, int selector,
				void *result);
int sslDigestSignBackendSetInfo(SslCryptoAlgorithm *algorithm, int selector,
				SceUIntPtr value);
int sslDigestSignBackendInit(SslCryptoAlgorithm *algorithm, void *key,
			     int signMode);
int sslDigestSignBackendSign(SslCryptoAlgorithm *algorithm, const void *data,
			     SceUInt32 length, void *signature,
			     SceUInt32 *signatureLength);
int sslDigestSignBackendUpdate(SslCryptoAlgorithm *algorithm, const void *data,
			       SceUInt32 length);
int sslDigestSignBackendSignFinal(SslCryptoAlgorithm *algorithm,
				  void *signature, SceUInt32 *signatureLength);
int sslDigestSignBackendVerify(SslCryptoAlgorithm *algorithm, const void *data,
			       SceUInt32 length, const void *signature,
			       SceUInt32 signatureLength, int *result);
int sslDigestSignBackendVerifyFinal(SslCryptoAlgorithm *algorithm,
				    const void *signature,
				    SceUInt32 signatureLength, int *result);

static const SslCryptoImplementationMethods sslDigestSignBackendMethods = {
	sslDigestSignBackendInitialize,
	sslDigestSignBackendDestroy,
	NULL,
	sslDigestSignBackendGetInfo,
	sslDigestSignBackendSetInfo,
	NULL,
	(void *)sslDigestSignBackendInit,
	(void *)sslDigestSignBackendSign,
	(void *)sslDigestSignBackendUpdate,
	(void *)sslDigestSignBackendSignFinal,
	(void *)sslDigestSignBackendVerify,
	sslDigestSignBackendUpdate,
	sslDigestSignBackendVerifyFinal
};

const unsigned char sslDesWeakKeys[64][8] = {
	{ 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 },
	{ 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe },
	{ 0x1f, 0x1f, 0x1f, 0x1f, 0x0e, 0x0e, 0x0e, 0x0e },
	{ 0xe0, 0xe0, 0xe0, 0xe0, 0xf1, 0xf1, 0xf1, 0xf1 },
	{ 0x01, 0xfe, 0x01, 0xfe, 0x01, 0xfe, 0x01, 0xfe },
	{ 0xfe, 0x01, 0xfe, 0x01, 0xfe, 0x01, 0xfe, 0x01 },
	{ 0x1f, 0xe0, 0x1f, 0xe0, 0x0e, 0xf1, 0x0e, 0xf1 },
	{ 0xe0, 0x1f, 0xe0, 0x1f, 0xf1, 0x0e, 0xf1, 0x0e },
	{ 0x01, 0xe0, 0x01, 0xe0, 0x01, 0xf1, 0x01, 0xf1 },
	{ 0xe0, 0x01, 0xe0, 0x01, 0xf1, 0x01, 0xf1, 0x01 },
	{ 0x1f, 0xfe, 0x1f, 0xfe, 0x0e, 0xfe, 0x0e, 0xfe },
	{ 0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 0x0e, 0xfe, 0x0e },
	{ 0x01, 0x1f, 0x01, 0x1f, 0x01, 0x0e, 0x01, 0x0e },
	{ 0x1f, 0x01, 0x1f, 0x01, 0x0e, 0x01, 0x0e, 0x01 },
	{ 0xe0, 0xfe, 0xe0, 0xfe, 0xf1, 0xfe, 0xf1, 0xfe },
	{ 0xfe, 0xe0, 0xfe, 0xe0, 0xfe, 0xf1, 0xfe, 0xf1 },
	{ 0x1f, 0x1f, 0x01, 0x01, 0x0e, 0x0e, 0x01, 0x01 },
	{ 0x01, 0x1f, 0x1f, 0x01, 0x01, 0x0e, 0x0e, 0x01 },
	{ 0x1f, 0x01, 0x01, 0x1f, 0x0e, 0x01, 0x01, 0x0e },
	{ 0x01, 0x01, 0x1f, 0x1f, 0x01, 0x01, 0x0e, 0x0e },
	{ 0xe0, 0xe0, 0x01, 0x01, 0xf1, 0xf1, 0x01, 0x01 },
	{ 0xfe, 0xfe, 0x01, 0x01, 0xfe, 0xfe, 0x01, 0x01 },
	{ 0xfe, 0xe0, 0x1f, 0x01, 0xfe, 0xf1, 0x0e, 0x01 },
	{ 0xe0, 0xfe, 0x1f, 0x01, 0xf1, 0xfe, 0x0e, 0x01 },
	{ 0xfe, 0xe0, 0x01, 0x1f, 0xfe, 0xf1, 0x01, 0x0e },
	{ 0xe0, 0xfe, 0x01, 0x1f, 0xf1, 0xfe, 0x01, 0x0e },
	{ 0xe0, 0xe0, 0x1f, 0x1f, 0xf1, 0xf1, 0x0e, 0x0e },
	{ 0xfe, 0xfe, 0x1f, 0x1f, 0xfe, 0xfe, 0x0e, 0x0e },
	{ 0xfe, 0x1f, 0xe0, 0x01, 0xfe, 0x0e, 0xf1, 0x01 },
	{ 0xe0, 0x1f, 0xfe, 0x01, 0xf1, 0x0e, 0xfe, 0x01 },
	{ 0xfe, 0x01, 0xe0, 0x1f, 0xfe, 0x01, 0xf1, 0x0e },
	{ 0xe0, 0x01, 0xfe, 0x1f, 0xf1, 0x01, 0xfe, 0x0e },
	{ 0x01, 0xe0, 0xe0, 0x01, 0x01, 0xf1, 0xf1, 0x01 },
	{ 0x1f, 0xfe, 0xe0, 0x01, 0x0e, 0xfe, 0xf0, 0x01 },
	{ 0x1f, 0xe0, 0xfe, 0x01, 0x0e, 0xf1, 0xfe, 0x01 },
	{ 0x01, 0xfe, 0xfe, 0x01, 0x01, 0xfe, 0xfe, 0x01 },
	{ 0x1f, 0xe0, 0xe0, 0x1f, 0x0e, 0xf1, 0xf1, 0x0e },
	{ 0x01, 0xfe, 0xe0, 0x1f, 0x01, 0xfe, 0xf1, 0x0e },
	{ 0x01, 0xe0, 0xfe, 0x1f, 0x01, 0xf1, 0xfe, 0x0e },
	{ 0x1f, 0xfe, 0xfe, 0x1f, 0x0e, 0xfe, 0xfe, 0x0e },
	{ 0xe0, 0x01, 0x01, 0xe0, 0xf1, 0x01, 0x01, 0xf1 },
	{ 0xfe, 0x1f, 0x01, 0xe0, 0xfe, 0x0e, 0x01, 0xf1 },
	{ 0xfe, 0x01, 0x1f, 0xe0, 0xfe, 0x01, 0x0e, 0xf1 },
	{ 0xe0, 0x1f, 0x1f, 0xe0, 0xf1, 0x0e, 0x0e, 0xf1 },
	{ 0xfe, 0x01, 0x01, 0xfe, 0xfe, 0x01, 0x01, 0xfe },
	{ 0xe0, 0x1f, 0x01, 0xfe, 0xf1, 0x0e, 0x01, 0xfe },
	{ 0xe0, 0x01, 0x1f, 0xfe, 0xf1, 0x01, 0x0e, 0xfe },
	{ 0xfe, 0x1f, 0x1f, 0xfe, 0xfe, 0x0e, 0x0e, 0xfe },
	{ 0x1f, 0xfe, 0x01, 0xe0, 0x0e, 0xfe, 0x01, 0xf1 },
	{ 0x01, 0xfe, 0x1f, 0xe0, 0x01, 0xfe, 0x0e, 0xf1 },
	{ 0x1f, 0xe0, 0x01, 0xfe, 0x0e, 0xf1, 0x01, 0xfe },
	{ 0x01, 0xe0, 0x1f, 0xfe, 0x01, 0xf1, 0x0e, 0xfe },
	{ 0x01, 0x01, 0xe0, 0xe0, 0x01, 0x01, 0xf1, 0xf1 },
	{ 0x1f, 0x1f, 0xe0, 0xe0, 0x0e, 0x0e, 0xf1, 0xf1 },
	{ 0x1f, 0x01, 0xfe, 0xe0, 0x0e, 0x01, 0xfe, 0xf1 },
	{ 0x01, 0x1f, 0xfe, 0xe0, 0x01, 0x0e, 0xfe, 0xf1 },
	{ 0x1f, 0x01, 0xe0, 0xfe, 0x0e, 0x01, 0xf1, 0xfe },
	{ 0x01, 0x1f, 0xe0, 0xfe, 0x01, 0x0e, 0xf1, 0xfe },
	{ 0x01, 0x01, 0xfe, 0xfe, 0x01, 0x01, 0xfe, 0xfe },
	{ 0x1f, 0x1f, 0xfe, 0xfe, 0x0e, 0x0e, 0xfe, 0xfe },
	{ 0xfe, 0xfe, 0xe0, 0xe0, 0xfe, 0xfe, 0xf1, 0xf1 },
	{ 0xe0, 0xfe, 0xfe, 0xe0, 0xf1, 0xfe, 0xfe, 0xf1 },
	{ 0xfe, 0xe0, 0xe0, 0xfe, 0xfe, 0xf1, 0xf1, 0xfe },
	{ 0xe0, 0xe0, 0xfe, 0xfe, 0xf1, 0xf1, 0xfe, 0xfe }
};

const SslCryptoImplementationMethods *sslGetDigestSignBackendMethods(void)
{
	return &sslDigestSignBackendMethods;
}

int sslDigestSignBackendDestroy(SslCryptoAlgorithm *algorithm)
{
	SslDigestSignBackendState *state = algorithm->backendState;

	if (state != NULL) {
		algorithm->backendState = NULL;
		if (state->digestAlgorithm != NULL)
			sslDestroyAlgorithm(state->digestAlgorithm);
		if (state->signAlgorithm != NULL)
			sslDestroyAlgorithm(state->signAlgorithm);
		if (state->buffer != NULL)
			sslFree(state->buffer);
		sslFree(state);
	}
	return 0;
}

int sslDigestSignBackendInitialize(SslCryptoAlgorithm *algorithm)
{
	int digestAlgorithm = 0;
	int macAlgorithm = 0;
	SslDigestSignBackendState *state;
	int status;

	status = sslGetMacAlgorithms(algorithm->context, algorithm->algorithm,
				     &digestAlgorithm, &macAlgorithm);
	if (status == 0) {
		state = sslAllocate(sizeof(*state));
		if (state != NULL) {
			sslFillMemory(state, 0, sizeof(*state));
			algorithm->backendState = state;
			status = sslCreateAlgorithm(algorithm->context, 3,
						    digestAlgorithm, 0,
						    &state->digestAlgorithm);
			if (status == 0) {
				status = sslCreateAlgorithm(
					algorithm->context, 6, macAlgorithm,
					algorithm->mode, &state->signAlgorithm);
				if (status == 0)
					return status;
			}
		} else {
			status = SSL_LIBRARY_ERROR_ALLOCATION;
		}
	}
	sslDigestSignBackendDestroy(algorithm);
	return status;
}

int sslDigestSignBackendInit(SslCryptoAlgorithm *algorithm, void *key,
			     int signMode)
{
	SslDigestSignBackendState *state = algorithm->backendState;
	int status;

	if (state == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if ((algorithm->flags & 0x400) == 0) {
		status = sslDigestInit(state->digestAlgorithm);
		if (status != 0)
			return status;
		algorithm->flags |= 0x400;
	}
	if (signMode != 0)
		status = sslSignInit(state->signAlgorithm, key);
	else
		status = sslVerifyInit(state->signAlgorithm, key);
	if (status == 0)
		status = sslSetAlgorithmInfo(
			state->signAlgorithm, 111,
			(SceUIntPtr)state->digestAlgorithm);
	return status;
}

int sslDigestSignBackendSignFinal(SslCryptoAlgorithm *algorithm,
				  void *signature, SceUInt32 *signatureLength)
{
	SslDigestSignBackendState *state = algorithm->backendState;
	SceUInt32 digestLength;
	unsigned char digestBuffer[64];
	unsigned char *digest;
	int status;

	if (state == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if ((state->flags & 1) != 0) {
		digest = state->buffer;
		if (digest == NULL)
			return SSL_CRYPTO_ERROR_INVALID_STATE;
		digestLength = state->bufferLength;
	} else {
		digestLength = sizeof(digestBuffer);
		digest = digestBuffer;
		status = sslDigestFinal(state->digestAlgorithm, digestBuffer,
					&digestLength);
		if (status != 0)
			return status;
	}
	return sslSign(state->signAlgorithm, digest, digestLength, signature,
		       signatureLength);
}

int sslDigestSignBackendUpdate(SslCryptoAlgorithm *algorithm, const void *data,
			       SceUInt32 length)
{
	SslDigestSignBackendState *state = algorithm->backendState;
	int status = 0;

	if (state == NULL) {
		status = SSL_CRYPTO_ERROR_INVALID_STATE;
	} else if ((state->flags & 1) != 0) {
		if (state->buffer != NULL && state->bufferLength < length) {
			sslFree(state->buffer);
			state->buffer = NULL;
			state->bufferLength = 0;
			state->buffer = sslAllocate(length);
			if (state->buffer == NULL)
				status = SSL_LIBRARY_ERROR_ALLOCATION;
		} else if (state->buffer == NULL) {
			state->buffer = sslAllocate(length);
			if (state->buffer == NULL)
				status = SSL_LIBRARY_ERROR_ALLOCATION;
		}
		if (status == 0) {
			sslCopyMemory(state->buffer, data, length);
			state->bufferLength = length;
		}
	} else {
		status = sslDigestUpdate(state->digestAlgorithm, data, length);
	}
	return status;
}

int sslDigestSignBackendSign(SslCryptoAlgorithm *algorithm, const void *data,
			     SceUInt32 length, void *signature,
			     SceUInt32 *signatureLength)
{
	SslDigestSignBackendState *state = algorithm->backendState;
	int status;

	if (state == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (signature == NULL)
		return sslSign(state->signAlgorithm, data, length, signature,
			       signatureLength);
	status = sslDigestSignBackendUpdate(algorithm, data, length);
	if (status == 0)
		status = sslDigestSignBackendSignFinal(algorithm, signature,
						       signatureLength);
	return status;
}

int sslDigestSignBackendVerifyFinal(SslCryptoAlgorithm *algorithm,
				    const void *signature,
				    SceUInt32 signatureLength, int *result)
{
	SslDigestSignBackendState *state = algorithm->backendState;
	SceUInt32 digestLength;
	unsigned char digestBuffer[64];
	unsigned char *digest;
	int status;

	*result = SSL_LIBRARY_ERROR_REFERENCE;
	if (state == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if ((state->flags & 1) != 0) {
		digest = state->buffer;
		if (digest == NULL)
			return SSL_CRYPTO_ERROR_INVALID_STATE;
		digestLength = state->bufferLength;
	} else {
		digestLength = sizeof(digestBuffer);
		digest = digestBuffer;
		status = sslDigestFinal(state->digestAlgorithm, digestBuffer,
					&digestLength);
		if (status != 0)
			return status;
	}
	return sslVerify(state->signAlgorithm, digest, digestLength, signature,
			 signatureLength, result);
}

int sslDigestSignBackendVerify(SslCryptoAlgorithm *algorithm, const void *data,
			       SceUInt32 length, const void *signature,
			       SceUInt32 signatureLength, int *result)
{
	int status;

	status = sslDigestSignBackendUpdate(algorithm, data, length);
	if (status == 0)
		status = sslDigestSignBackendVerifyFinal(
			algorithm, signature, signatureLength, result);
	return status;
}

int sslDigestSignBackendSetInfo(SslCryptoAlgorithm *algorithm, int selector,
				SceUIntPtr value)
{
	SslDigestSignBackendState *state = algorithm->backendState;
	int status = 0;

	if (state == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (selector == 101) {
		if (*(int *)value != 0)
			state->flags |= 1;
		else
			state->flags &= ~1u;
	} else {
		status = sslSetAlgorithmInfo(state->signAlgorithm, selector,
					     value);
		if (status == SSL_CRYPTO_ERROR_NO_OPERATION)
			status = sslSetAlgorithmInfo(state->digestAlgorithm,
						     selector, value);
	}
	return status;
}

int sslDigestSignBackendGetInfo(SslCryptoAlgorithm *algorithm, int selector,
				void *result)
{
	SslDigestSignBackendState *state = algorithm->backendState;
	int status;

	if (state == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	status = sslGetAlgorithmInfo(state->signAlgorithm, selector, result);
	if (status == SSL_LIBRARY_ERROR_SELECTOR ||
	    status == SSL_CRYPTO_ERROR_NO_OPERATION)
		status = sslGetAlgorithmInfo(state->digestAlgorithm, selector,
					     result);
	return status;
}

int sslCheckDesWeakKey(const SslDesKeyBuffer *key)
{
	int blockCount = (int)(key->length >> 3);
	int result = 0;
	int block = 0;
	int i;
	int j;

	if (blockCount != 0) {
		int offset = 0;
		const unsigned char *data = key->data;

		do {
			for (i = 0; i < 64; i++) {
				for (j = 0; j < 8; j++) {
					if ((sslDesWeakKeys[i][j] >> 1) !=
					    (data[offset + j] >> 1))
						break;
				}
				if (j == 8)
					return 1;
			}
			block++;
			offset += 8;
		} while (block < blockCount);
	}
	return result;
}

SslCryptoPropertyHandler sslBasePropertyHandlers[3] = {
	{ 86, sslGetLowLevelStringProperty, NULL, 0, 4, 2 },
	{ 1, sslGetRsaKeyModulusByteLength, NULL, 0, 0, 0 },
	{ 118, sslGetRsaPrivateKeyRepresentationType,
	  sslSetRsaPrivateKeyRepresentationType, 0, 0x1003, 36 }
};

static SslCryptoPropertyHandler sslRsaPrivateKeyPropertyHandlers[11] = {
	{ 18, NULL, sslCopyPublicKeyComponentToAlgorithmProperty, 0x20000, 18,
	  33 },
	{ 17, NULL, sslCopyPublicKeyComponentToAlgorithmProperty, 0x22000, 17,
	  33 },
	{ 22, NULL, sslCopyPublicKeyComponentToAlgorithmProperty, 0x22000, 22,
	  33 },
	{ 19, NULL, sslCopyPublicKeyComponentToAlgorithmProperty, 0x22000, 19,
	  33 },
	{ 24, NULL, sslCopyPublicKeyComponentToAlgorithmProperty, 0x22000, 24,
	  33 },
	{ 23, NULL, sslCopyPublicKeyComponentToAlgorithmProperty, 0x22000, 23,
	  33 },
	{ 20, NULL, sslCopyPublicKeyComponentToAlgorithmProperty, 0x22000, 20,
	  33 },
	{ 2008, NULL, sslStorePublicKeyPointerProperty, 0x20000, 1, 33 },
	{ 16, NULL, sslCopyPublicKeyComponentToAlgorithmProperty, 0x70000, 16,
	  33 },
	{ 2, NULL, sslCopyPublicKeyComponentToAlgorithmProperty, 0x22000,
	  0x1003, 36 },
	{ 4097, NULL, sslCopyPublicKeyComponentToAlgorithmProperty, 0x22000,
	  0x1002, 36 }
};

static SslCryptoKeyPropertyHandler sslRsaPrivateKeyExportHandlers[12] = {
	{ 17, 33, 0x2000, sslExportBufferPropertyToBackend, NULL },
	{ 18, 33, 0, sslExportBufferPropertyToBackend, NULL },
	{ 22, 33, 0x2000, sslExportBufferPropertyToBackend, NULL },
	{ 19, 33, 0x2000, sslExportBufferPropertyToBackend, NULL },
	{ 24, 33, 0x2000, sslExportBufferPropertyToBackend, NULL },
	{ 23, 33, 0x2000, sslExportBufferPropertyToBackend, NULL },
	{ 20, 33, 0x2000, sslExportBufferPropertyToBackend, NULL },
	{ 1, 33, 0, sslExportPointerPropertyToBackend, NULL },
	{ 16, 33, 0, sslExportBufferPropertyToBackend, NULL },
	{ 0x1002, 36, 0x2000, sslExportBufferPropertyToBackend, NULL },
	{ 0x1003, 36, 0, sslExportBooleanPropertyToBackend,
	  (int (*)(SslCryptoAlgorithm *, void *))sslClearBackendProperty },
	{ 2, 1, 0x2000, sslExportAlgorithmBackendToLowLevel,
	  sslExportRsaBlindingRandomToLowLevel }
};

static SslCryptoPropertyHandler sslRsaPublicKeyPropertyHandlers[2] = {
	{ 17, NULL, sslCopyPublicKeyComponentToAlgorithmProperty, 0x40000, 17,
	  33 },
	{ 16, NULL, sslCopyPublicKeyComponentToAlgorithmProperty, 0x70000, 16,
	  33 }
};

static SslCryptoKeyPropertyHandler sslRsaPublicKeyExportHandlers[2] = {
	{ 17, 33, 0, sslExportBufferPropertyToBackend, NULL },
	{ 16, 33, 0, sslExportBufferPropertyToBackend, NULL }
};

static const void *(*sslRsaPrivateKeyMethods[4])(void) = {
	sslGetRsaPkcs1Type1EncodeMethod,
	sslGetRsaBlindingMethod,
	sslGetRsaPrivateReciprocalMethod,
	NULL
};

static const void *(*sslRsaPublicKeyMethods[3])(void) = {
	sslGetRsaPkcs1Type1DecodeMethod,
	sslGetRsaPublicReciprocalMethod,
	NULL
};

int sslPrependPropertyHandlerNode(SslCryptoPropertyNode **head,
				  const void *handler)
{
	int status = 0;
	SslCryptoPropertyNode *node = sslAllocate(sizeof(*node));

	if (node == NULL)
		return SSL_LIBRARY_ERROR_ALLOCATION;
	node->handler = handler;
	node->next = *head;
	*head = node;
	return status;
}

int sslPrependBasePropertyHandlerTable(SslCryptoPropertyNode **head,
				       const SslCryptoPropertyHandler *table,
				       int count)
{
	int status = 0;
	int i;

	for (i = 0; i < count; i++, table++) {
		int itemStatus = 0;
		SslCryptoPropertyNode *node = sslAllocate(sizeof(*node));

		if (node == NULL) {
			itemStatus = SSL_LIBRARY_ERROR_ALLOCATION;
		} else {
			node->handler = table;
			node->next = *head;
			*head = node;
		}
		status = itemStatus;
		if (status != 0)
			break;
	}
	return status;
}

int sslRegisterBasePropertyHandlers(SslCryptoPropertyNode **head, int type)
{
	int status = 0;

	if (type == 0)
		status = sslPrependBasePropertyHandlerTable(
			head, sslBasePropertyHandlers, 3);
	return status;
}

int sslPrependPropertyHandlerTable(SslCryptoPropertyNode **head,
				   const SslCryptoPropertyHandler *table,
				   int count)
{
	int status = 0;
	int i;

	for (i = 0; i < count; i++, table++) {
		int itemStatus = 0;
		SslCryptoPropertyNode *node = sslAllocate(sizeof(*node));

		if (node == NULL) {
			itemStatus = SSL_LIBRARY_ERROR_ALLOCATION;
		} else {
			node->handler = table;
			node->next = *head;
			*head = node;
		}
		status = itemStatus;
		if (status != 0)
			break;
	}
	return status;
}

int sslPrependKeyPropertyHandlerTable(SslCryptoPropertyNode **head,
				      const SslCryptoKeyPropertyHandler *table,
				      int count)
{
	int status = 0;
	int i;

	for (i = 0; i < count; i++, table++) {
		int itemStatus = 0;
		SslCryptoPropertyNode *node = sslAllocate(sizeof(*node));

		if (node == NULL) {
			itemStatus = SSL_LIBRARY_ERROR_ALLOCATION;
		} else {
			node->handler = table;
			node->next = *head;
			*head = node;
		}
		status = itemStatus;
		if (status != 0)
			break;
	}
	return status;
}

int sslRegisterRsaPrivateKeyPropertyHandlers(SslCryptoPropertyNode **head,
					     int type)
{
	int status = 0;

	if (type == 2)
		status = sslPrependKeyPropertyHandlerTable(
			head, sslRsaPrivateKeyExportHandlers, 12);
	else if (type == 1)
		status = sslPrependPropertyHandlerTable(
			head, sslRsaPrivateKeyPropertyHandlers, 11);
	return status;
}

int sslRegisterRsaPublicKeyPropertyHandlers(SslCryptoPropertyNode **head,
					    int type)
{
	int status = 0;

	if (type == 2)
		status = sslPrependKeyPropertyHandlerTable(
			head, sslRsaPublicKeyExportHandlers, 2);
	else if (type == 1)
		status = sslPrependPropertyHandlerTable(
			head, sslRsaPublicKeyPropertyHandlers, 2);
	return status;
}

int sslGetRsaKeyModulusByteLength(SslCryptoAlgorithm *algorithm, void *backend,
				  int type, int id, int cached, void *result)
{
	SslDirectMemoryContext *dmem = sslGetDefaultDirectMemoryContextInline();
	void *bn = NULL;
	void *bnCtx = NULL;
	SslProperty *property = NULL;
	int status;
	int err;

	(void)backend;
	(void)type;
	(void)id;
	(void)cached;
	if (sslFindProperty(algorithm->properties, 33, 16, NULL, &property,
			    0) != 0) {
		status = SSL_CRYPTO_ERROR_NOT_INITIALIZED;
	} else {
		err = sslNewBigNumContext(&bnCtx, dmem);
		if (err != 0) {
			status = sslMapCryptoBackendError(err);
		} else {
			err = sslNewBigNum(&bn, dmem);
			if (err != 0 ||
			    (err = sslSetBigNumData(bn, property->data,
						    property->length,
						    bnCtx)) != 0) {
				status = sslMapCryptoBackendError(err);
			} else {
				*(int *)result =
					(sslGetBigNumBitLength(bn) + 7) / 8;
				status = 0;
			}
		}
	}
	if (bn != NULL)
		sslFreeBigNum(bn, 0);
	if (bnCtx != NULL)
		sslFreeBigNumContext(bnCtx, 0);
	return status;
}

int sslGetLowLevelOrCachedPointerProperty(SslCryptoAlgorithm *algorithm,
					  void *backend, int type, int id,
					  int cached, void *result)
{
	SslProperty *property;
	int err;

	if (cached == 0) {
		if (backend == NULL)
			return SSL_CRYPTO_ERROR_INVALID_STATE;
		err = sslGetLowLevelObjectInfo(backend, type, id, result);
		if (err != 0)
			return sslMapCryptoBackendError(err);
	} else {
		property = NULL;
		if (sslFindProperty(algorithm->properties, type, id, NULL,
				    &property, 0) != 0)
			return SSL_CRYPTO_ERROR_NOT_FOUND;
		if (property->value != 257)
			return SSL_LIBRARY_ERROR_SELECTOR;
		*(void **)result = property->data;
	}
	return 0;
}

int sslGetRsaPrivateKeyRepresentationType(SslCryptoAlgorithm *algorithm,
					  void *backend, int type, int id,
					  int cached, void *result)
{
	SceUInt32 *out = result;
	int flags;
	SslProperty *property;
	int status;
	int err;

	(void)cached;
	*out = 0;
	if (backend == NULL) {
		property = NULL;
		status = sslFindProperty(algorithm->properties, type, id, NULL,
					 &property, 0);
		if (status != 0)
			status = SSL_CRYPTO_ERROR_NOT_FOUND;
		else if (property->value == 256)
			*out = property->length;
		else
			status = SSL_LIBRARY_ERROR_SELECTOR;
	} else {
		err = sslGetLowLevelObjectInfo(backend, type, id, &flags);
		status = 0;
		if (err != 0)
			status = sslMapCryptoBackendError(err);
		if (status == 0 && flags != 0) {
			if ((flags & 0x10) != 0)
				*out = 2;
			else
				*out = 1;
		}
	}
	return status;
}

int sslSetRsaKeyBufferProperty(SslCryptoAlgorithm *algorithm, int type, int id,
			       const SslDesKeyBuffer *buffer)
{
	if (buffer == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (sslAddPropertyValue(algorithm->properties, type, id, 0x100u,
				buffer->data, buffer->length, 0x12u) != 0)
		return SSL_LIBRARY_ERROR_ALLOCATION;
	return 0;
}

int sslSetRsaPrivateKeyRepresentationType(SslCryptoAlgorithm *algorithm,
					  int type, int id,
					  const SceUInt32 *value)
{
	SceUInt32 rep = *value;
	SslDesKeyBuffer buffer;
	const SslDesKeyBuffer *bufferPtr;
	int status;

	if (rep != 0 && rep != 1) {
		status = SSL_CRYPTO_ERROR_INVALID_ARGUMENT;
	} else {
		buffer.length = rep;
		buffer.data = NULL;
		bufferPtr = &buffer;
		if (bufferPtr == NULL) {
			status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
		} else {
			status = 0;
			if (sslAddPropertyValue(algorithm->properties, type, id,
						0x100u, bufferPtr->data,
						bufferPtr->length, 0x12u) != 0)
				status = SSL_LIBRARY_ERROR_ALLOCATION;
		}
	}
	return status;
}

int sslCopyPublicKeyComponentToAlgorithmProperty(
	SslCryptoAlgorithm *algorithm,
	const SslCryptoPropertyHandler *handler, void *key)
{
	SslDesKeyBuffer buffer = { 0, NULL };
	int status;

	status = sslGetPublicKeyInfo(key, handler->selector, &buffer);
	if (status == 0)
		status = sslSetRsaKeyBufferProperty(algorithm, handler->type,
						    handler->id, &buffer);
	return status;
}

int sslCopyRsaMultiPrimeComponentsToAlgorithmProperties(
	SslCryptoAlgorithm *algorithm, void *key)
{
	int status = 0;
	int rId = 22;
	int dId = 23;
	int tId = 24;

	for (;;) {
		SslDesKeyBuffer tBuf;
		SslDesKeyBuffer dBuf;
		SslDesKeyBuffer rBuf = { 0, NULL };

		status = sslGetPublicKeyInfo(key, rId, &rBuf);
		if (status == 0)
			status = sslSetRsaKeyBufferProperty(algorithm, 33, rId,
							    &rBuf);
		if (status == SSL_CRYPTO_ERROR_UNSUPPORTED)
			break;
		if (status != 0)
			return status;
		dBuf.length = 0;
		dBuf.data = NULL;
		status = sslGetPublicKeyInfo(key, dId, &dBuf);
		if (status == 0)
			status = sslSetRsaKeyBufferProperty(algorithm, 33, dId,
							    &dBuf);
		if (status != 0)
			return status;
		tBuf.length = 0;
		tBuf.data = NULL;
		status = sslGetPublicKeyInfo(key, tId, &tBuf);
		if (status == 0)
			status = sslSetRsaKeyBufferProperty(algorithm, 33, tId,
							    &tBuf);
		if (status != 0)
			return status;
		rId += 3;
		dId += 3;
		tId += 3;
	}
	return 0;
}

int sslExportBooleanPropertyToBackend(SslCryptoAlgorithm *algorithm,
				      void *backend, SslProperty *property)
{
	int flag;
	int err;
	int status;

	(void)algorithm;
	if (property->value == 256) {
		flag = property->length != 0;
		err = sslSetLowLevelObjectInfo(backend, property->type,
					       property->id, &flag);
		if (err != 0)
			status = sslMapCryptoBackendError(err);
		else
			status = 0;
	} else {
		status = SSL_LIBRARY_ERROR_SELECTOR;
	}
	return status;
}

int sslClearBackendProperty(SslCryptoAlgorithm *algorithm, void *backend,
			    SslProperty *property)
{
	int zero = 0;
	int status;

	(void)algorithm;
	status = sslSetLowLevelObjectInfo(backend, property->id,
					  property->type, &zero);
	if (status != 0)
		status = sslMapCryptoBackendError(status);
	return status;
}

static __inline int sslExportBufferPropertyToBackendInline(
	SslCryptoAlgorithm *algorithm, void *backend, SslProperty *property)
{
	SslLowLevelBuffer buffer = { NULL, 0 };
	int err;
	int status;

	(void)algorithm;
	if (property->value == 256) {
		buffer.data = property->data;
		buffer.length = property->length;
		err = sslSetLowLevelObjectInfo(backend, property->type,
					       property->id, &buffer);
		status = 0;
		if (err != 0)
			status = sslMapCryptoBackendError(err);
	} else {
		status = SSL_LIBRARY_ERROR_SELECTOR;
	}
	return status;
}

int sslExportBufferPropertyToBackend(SslCryptoAlgorithm *algorithm,
				     void *backend, SslProperty *property)
{
	return sslExportBufferPropertyToBackendInline(algorithm, backend,
						      property);
}

int sslExportRsaMultiPrimePropertiesToBackend(SslCryptoAlgorithm *algorithm,
					      void *backend)
{
	SslProperty *property = NULL;
	int rId = 22;
	int dId = 23;
	int tId = 24;
	int status;

	while (sslFindProperty(algorithm->properties, 33, rId, NULL, &property,
			       0) == 0) {
		status = sslExportBufferPropertyToBackendInline(
			algorithm, backend, property);
		if (status != 0)
			return status;
		if (sslFindProperty(algorithm->properties, 33, dId, NULL,
				    &property, 0) == 0) {
			status = sslExportBufferPropertyToBackendInline(
				algorithm, backend, property);
			if (status != 0)
				return status;
		}
		if (sslFindProperty(algorithm->properties, 33, tId, NULL,
				    &property, 0) == 0) {
			status = sslExportBufferPropertyToBackendInline(
				algorithm, backend, property);
			if (status != 0)
				return status;
		}
		rId += 3;
		dId += 3;
		tId += 3;
	}
	return 0;
}

const void *sslGetRsaPrivateKeyMethodList(void)
{
	return sslRsaPrivateKeyMethods;
}

const void *sslGetRsaPublicKeyMethodList(void)
{
	return sslRsaPublicKeyMethods;
}

int sslGetLowLevelOrCachedIntProperty(SslCryptoAlgorithm *algorithm,
				      void *backend, int type, int id,
				      int cached, void *result)
{
	void *value;
	SslProperty *property;
	int err;

	if (cached == 0) {
		if (backend == NULL)
			return SSL_CRYPTO_ERROR_INVALID_STATE;
		err = sslGetLowLevelObjectInfo(backend, type, id, &value);
		if (err != 0)
			return sslMapCryptoBackendError(err);
		*(void **)result = value;
	} else {
		property = NULL;
		if (sslFindProperty(algorithm->properties, type, id, NULL,
				    &property, 0) != 0)
			return SSL_CRYPTO_ERROR_NOT_FOUND;
		if (property->value != 258)
			return SSL_LIBRARY_ERROR_SELECTOR;
		*(void **)result = property->data;
	}
	return 0;
}

int sslGetLowLevelStringProperty(SslCryptoAlgorithm *algorithm, void *backend,
				 int type, int id, int cached, void *result)
{
	char *info[2];
	SceUInt32 len;
	SceUInt32 allocLen;
	char *copy;
	int status;
	int err;

	(void)cached;
	if (backend == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	info[0] = NULL;
	info[1] = NULL;
	err = sslGetLowLevelObjectInfo(backend, type, id, info);
	if (err != 0)
		return sslMapCryptoBackendError(err);
	len = sslStringLength(info[0]);
	allocLen = len + 1;
	copy = sslAllocate(allocLen);
	if (copy != NULL) {
		sslCopyMemory(copy, info[0], allocLen);
		copy[len] = '\0';
		status = sslAddPropertyValue(algorithm->properties, type, id, 0,
					     copy, allocLen, 0x50u);
		if (status == 0) {
			*(char **)result = copy;
			return 0;
		}
	} else {
		status = SSL_LIBRARY_ERROR_ALLOCATION;
	}
	if (copy != NULL)
		sslFree(copy);
	return status;
}

int sslStorePublicKeyPointerProperty(SslCryptoAlgorithm *algorithm,
				     const SslCryptoPropertyHandler *handler,
				     void *key)
{
	void *data;
	int status;

	status = sslGetPublicKeyInfo(key, handler->selector, &data);
	if (status == 0) {
		if (sslAddPropertyValue(algorithm->properties, handler->type,
					handler->id, 0x101u, data, 0,
					0x10u) != 0)
			status = SSL_LIBRARY_ERROR_ALLOCATION;
		else
			status = 0;
	}
	return status;
}

int sslExportPointerPropertyToBackend(SslCryptoAlgorithm *algorithm,
				      void *backend, SslProperty *property)
{
	void *data;
	int err;
	int status;

	(void)algorithm;
	if (property->value == 257) {
		data = property->data;
		err = sslSetLowLevelObjectInfo(backend, property->type,
					       property->id, &data);
		status = 0;
		if (err != 0)
			status = sslMapCryptoBackendError(err);
	} else {
		status = SSL_LIBRARY_ERROR_SELECTOR;
	}
	return status;
}

int sslStoreAlgorithmObjectProperty(SslCryptoAlgorithm *algorithm, int type,
				    int id, const void *object)
{
	if (sslAddPropertyValue(algorithm->properties, type, id, 0x110u, object,
				0, 0x10u) != 0)
		return SSL_LIBRARY_ERROR_ALLOCATION;
	return 0;
}

int sslCreateAndStoreRandomAlgorithmProperty(SslCryptoAlgorithm *algorithm,
					     int type, int id,
					     SceUIntPtr value)
{
	SslCryptoAlgorithm **slot = algorithm->backendState;
	SslCryptoAlgorithm *randomAlgorithm = NULL;
	int status;

	status = sslCreateAlgorithm(algorithm->context, 4, 4181, 0,
				    &randomAlgorithm);
	if (status != 0 ||
	    (status = sslSetAlgorithmInfo(randomAlgorithm, 53, value)) != 0) {
		if (randomAlgorithm != NULL)
			sslDestroyAlgorithm(randomAlgorithm);
		return status;
	}
	if (sslAddPropertyValue(algorithm->properties, type, id, 0x110u,
				randomAlgorithm, 0, 0x10u) != 0) {
		status = SSL_LIBRARY_ERROR_ALLOCATION;
		if (randomAlgorithm != NULL)
			sslDestroyAlgorithm(randomAlgorithm);
		return status;
	}
	if (*slot != NULL)
		sslDestroyAlgorithm(*slot);
	*slot = randomAlgorithm;
	return status;
}

int sslStoreDigestDescriptorProperty(SslCryptoAlgorithm *algorithm, int type,
				     int id,
				     SslCryptoAlgorithm *digestAlgorithm)
{
	const void *descriptor = NULL;
	int status;

	if (sslGetAlgorithmInfo(digestAlgorithm, 11002, &descriptor) != 0) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
	} else {
		if (sslAddPropertyValue(algorithm->properties, type, id, 0x111u,
					descriptor, 0, 0x10u) == 0)
			status = 0;
		else
			status = SSL_LIBRARY_ERROR_ALLOCATION;
	}
	return status;
}

int sslExportAlgorithmBackendToLowLevel(SslCryptoAlgorithm *algorithm,
					void *backend, SslProperty *property)
{
	void *lowLevelBackend;
	int err;
	int status;

	(void)algorithm;
	if (property->value == 272) {
		lowLevelBackend = NULL;
		if (sslGetAlgorithmInfo(property->data, 11003,
					&lowLevelBackend) != 0) {
			status = SSL_CRYPTO_ERROR_NO_OPERATION;
		} else {
			err = sslSetLowLevelObjectInfo(backend, property->type,
						       property->id,
						       lowLevelBackend);
			if (err != 0)
				status = sslMapCryptoBackendError(err);
			else
				status = 0;
		}
	} else {
		status = SSL_LIBRARY_ERROR_SELECTOR;
	}
	return status;
}

int sslExportDigestDescriptorToLowLevel(SslCryptoAlgorithm *algorithm,
					void *backend, SslProperty *property)
{
	int err;

	(void)algorithm;
	if (property->value != 273)
		return SSL_LIBRARY_ERROR_SELECTOR;
	err = sslSetLowLevelObjectInfo(backend, property->type, property->id,
				       property->data);
	if (err != 0)
		return sslMapCryptoBackendError(err);
	return 0;
}
