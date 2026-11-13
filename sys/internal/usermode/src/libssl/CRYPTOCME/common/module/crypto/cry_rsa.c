/* Filename and private descriptive names are ours; CRYPTOCME RSA signature and
 * RSA cipher backends over the low-level object layer.
 */
#include "cry_api.h"
#include "ctx.h"
#include "ll_obj.h"
#include "ll_rsa.h"
#include "../dmem/dmem.h"
#include "../../key/pkey_ctx.h"
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


typedef const SslLowLevelMethodGetter *(*SslRsaMethodList)(void);

typedef struct SslRsaSignBackendData {
	SslRsaMethodList GetPrivateKeyMethods;
	SslRsaMethodList GetPublicKeyMethods;
	SslBigNumMethodSelector selectBigNumMethod;
	int (*RegisterHandlers)(SslCryptoPropertyNode **head, int type);
	int (*PostInit)(SslCryptoAlgorithm *algorithm, void *key);
	SceUInt32 reserved20;
	SceUInt32 flags;
} SslRsaSignBackendData;

typedef struct SslRsaSignBackendState {
	SslCryptoAlgorithm *random;
	SslLowLevelObject *object;
	SceUInt32 flags;
	int loaded;
	SslCryptoPropertyNode *infoHandlers;
	SslCryptoPropertyNode *importHandlers;
	SslCryptoPropertyNode *exportHandlers;
	SceUInt32 reserved28;
	SceUInt32 reserved32;
	SslBigNumMethodContext *bigNumMethods;
} SslRsaSignBackendState;

typedef struct SslRsaCipherBackendData {
	SslRsaMethodList GetPublicEncryptMethods;
	SslRsaMethodList GetPublicDecryptMethods;
	SslRsaMethodList GetPrivateEncryptMethods;
	SslRsaMethodList GetPrivateDecryptMethods;
	SslBigNumMethodSelector selectBigNumMethod;
	SceUInt32 flags;
	int (*RegisterHandlers)(SslCryptoPropertyNode **head, int type);
	int (*PostInit)(SslCryptoAlgorithm *algorithm, void *key);
	SceUInt32 reserved[2];
} SslRsaCipherBackendData;

typedef struct SslRsaCipherBackendState {
	SslCryptoAlgorithm *random;
	SslLowLevelObject *object;
	SceUInt32 flags;
	SslCryptoPropertyNode *infoHandlers;
	SslCryptoPropertyNode *exportHandlers;
	SceUInt32 modes;
	SslBigNumMethodContext *bigNumMethods;
} SslRsaCipherBackendState;

_Static_assert(sizeof(SslRsaSignBackendData) == 28, "SslRsaSignBackendData");
_Static_assert(sizeof(SslRsaSignBackendState) == 40, "SslRsaSignBackendState");
_Static_assert(sizeof(SslRsaCipherBackendData) == 40, "SslRsaCipherBackendData");
_Static_assert(sizeof(SslRsaCipherBackendState) == 28, "SslRsaCipherBackendState");

/* Backend state flags */
#define SSL_RSA_BLINDING 0x1
#define SSL_RSA_PRIVATE_OPERATION 0x4
#define SSL_RSA_PRIVATE_KEY 0x20000
#define SSL_RSA_PUBLIC_KEY 0x40000
#define SSL_RSA_KEY_MASK 0x70000

/* Cipher modes */
#define SSL_RSA_PUBLIC_ENCRYPT 0x1
#define SSL_RSA_PUBLIC_DECRYPT 0x2
#define SSL_RSA_PRIVATE_ENCRYPT 0x4
#define SSL_RSA_PRIVATE_DECRYPT 0x8

/* Handler flags */
#define SSL_RSA_HANDLER_CACHED 0x1000
#define SSL_RSA_HANDLER_OPTIONAL 0x2000

#define SSL_RSA_INFO_BLINDING 102

int sslRsaSignBackendInitialize(SslCryptoAlgorithm *algorithm);
int sslRsaSignBackendDestroy(SslCryptoAlgorithm *algorithm);
int sslRsaSignBackendGetInfo(SslCryptoAlgorithm *algorithm, int selector,
			     void *result);
int sslRsaSignBackendSetInfo(SslCryptoAlgorithm *algorithm, int selector,
			     const int *value);
int sslRsaSignBackendInit(SslCryptoAlgorithm *algorithm, void *key,
			  int privateKey);
int sslRsaSignBackendSign(SslCryptoAlgorithm *algorithm, const void *digest,
			  SceUInt32 digestLength, void *signature,
			  SceUInt32 *signatureLength);
int sslRsaSignBackendVerify(SslCryptoAlgorithm *algorithm, const void *digest,
			    SceUInt32 digestLength, const void *signature,
			    SceUInt32 signatureLength, int *result);

static SslCryptoPropertyHandler sslRsaSignPropertyHandlers[4] = {
	{ 53, NULL, sslCreateAndStoreRandomAlgorithmProperty, 0, 2, 1 },
	{ 73, NULL, sslStoreAlgorithmObjectProperty, 0, 2, 1 },
	{ 1, sslGetLowLevelOrCachedIntProperty, NULL, 0, 7, 1 },
	{ 111, NULL, sslStoreDigestDescriptorProperty, 0, 1, 3 }
};

static SslCryptoKeyPropertyHandler sslRsaSignExportHandlers[1] = {
	{ 1, 3, 0, sslExportDigestDescriptorToLowLevel, NULL }
};

static SslCryptoPropertyHandler sslRsaCipherPropertyHandlers[2] = {
	{ 53, NULL, sslCreateAndStoreRandomAlgorithmProperty, 0, 2, 1 },
	{ 73, NULL, sslStoreAlgorithmObjectProperty, 0, 2, 1 }
};

int sslExportRsaCipherBlindingRandom(SslCryptoAlgorithm *algorithm,
				     void *backend,
				     const SslCryptoKeyPropertyHandler *handler);

static SslCryptoKeyPropertyHandler sslRsaCipherExportHandlers[1] = {
	{ 2, 1, 0, sslExportAlgorithmBackendToLowLevel,
	  sslExportRsaCipherBlindingRandom }
};

static __inline int sslPrependHandlerTableInline(SslCryptoPropertyNode **head,
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

static __inline int sslPrependKeyHandlerTableInline(
	SslCryptoPropertyNode **head, const SslCryptoKeyPropertyHandler *table,
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

int sslRegisterRsaSignBasePropertyHandlers(SslCryptoPropertyNode **head,
					   int type)
{
	int status = 0;

	if (type == 0)
		status = sslPrependHandlerTableInline(
			head, sslBasePropertyHandlers, 3);
	if (status == 0 && type == 0)
		status = sslPrependHandlerTableInline(
			head, sslRsaSignPropertyHandlers, 4);
	return status;
}

int sslRegisterRsaSignPropertyHandlers(SslCryptoPropertyNode **head, int type)
{
	int status = sslRegisterRsaSignBasePropertyHandlers(head, type);

	if (status == 0 && type == 2)
		status = sslPrependKeyHandlerTableInline(
			head, sslRsaSignExportHandlers, 1);
	return status;
}

static const void *sslRsaSignBackendMethods[13];

const SslCryptoImplementationMethods *sslGetRsaSignBackendMethods(void)
{
	return (const SslCryptoImplementationMethods *)sslRsaSignBackendMethods;
}

void sslDestroyRsaRandomAlgorithm(SslCryptoAlgorithm **random)
{
	if (random != NULL)
		sslDestroyAlgorithm(*random);
}

void sslFreePropertyHandlerNodes(SslCryptoPropertyNode **head)
{
	SslCryptoPropertyNode *node;

	if (head != NULL) {
		node = *head;
		while (node != NULL) {
			SslCryptoPropertyNode *next = node->next;

			sslFree(node);
			*head = next;
			node = next;
		}
	}
}

static __inline void sslFreeHandlerListInline(SslCryptoPropertyNode **head)
{
	if (head != NULL) {
		SslCryptoPropertyNode *node = *head;

		while (node != NULL) {
			SslCryptoPropertyNode *next = node->next;

			sslFree(node);
			*head = next;
			node = next;
		}
	}
}

__attribute__((noinline)) int sslRsaSignBackendDestroy(SslCryptoAlgorithm *algorithm)
{
	SslRsaSignBackendState *state = algorithm->backendState;

	if (state != NULL) {
		sslFreeLowLevelObject(state->object);
		sslFreeBigNumMethodContext(state->bigNumMethods);
		sslFreeHandlerListInline(&state->infoHandlers);
		sslFreeHandlerListInline(&state->importHandlers);
		sslFreeHandlerListInline(&state->exportHandlers);
		sslDestroyRsaRandomAlgorithm(&state->random);
		sslFree(state);
		algorithm->backendState = NULL;
	}
	return 0;
}

int sslRsaSignBackendInitialize(SslCryptoAlgorithm *algorithm)
{
	const SslRsaSignBackendData *data = algorithm->implementationData;
	SslRsaSignBackendState *state;
	int status;

	if (data == NULL) {
		status = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
		goto error;
	}
	if (algorithm->family != 6) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		goto error;
	}
	state = sslAllocate(sizeof(*state));
	if (state == NULL) {
		status = SSL_LIBRARY_ERROR_ALLOCATION;
		goto error;
	}
	sslFillMemory(state, 0, sizeof(*state));
	algorithm->backendState = state;
	status = data->RegisterHandlers(&state->infoHandlers, 0);
	if (status != 0)
		goto error;
	if (algorithm->properties == NULL) {
		algorithm->properties = sslNewPropertyList();
		if (algorithm->properties == NULL) {
			status = SSL_LIBRARY_ERROR_ALLOCATION;
			goto error;
		}
		algorithm->flags |= SSL_CRYPTO_ALGORITHM_OWNS_PROPERTIES;
	}
	if (data->selectBigNumMethod != NULL) {
		SslDirectMemoryContext *dmem =
			sslGetDefaultDirectMemoryContextInline();

		status = sslNewBigNumMethodContext(&state->bigNumMethods, dmem);
		if (status != 0)
			status = sslMapCryptoBackendError(status);
		else
			sslSetBigNumMethodSelector(state->bigNumMethods,
						   data->selectBigNumMethod);
	}
	if (status != 0)
		goto error;
	return status;

error:
	sslRsaSignBackendDestroy(algorithm);
	algorithm->backendState = NULL;
	return status;
}

int sslImportRsaKeyProperties(SslCryptoAlgorithm *algorithm, void *key,
			      SslCryptoPropertyNode *node, SceUInt32 mask,
			      SceUInt32 optional)
{
	int status;

	while (node != NULL) {
		const SslCryptoPropertyHandler *handler = node->handler;

		if ((handler->flags & mask) != 0 && handler->set != NULL) {
			sslDeleteProperty(algorithm->properties, handler->type,
					  handler->id);
			status = handler->set(algorithm, handler, key);
			if (status != 0 && (handler->flags & optional) == 0)
				return status;
		}
		node = node->next;
	}
	return 0;
}

int sslRsaSignBackendInit(SslCryptoAlgorithm *algorithm, void *key,
			  int privateKey)
{
	const SslRsaSignBackendData *data = algorithm->implementationData;
	SslRsaSignBackendState *state = algorithm->backendState;
	SslDirectMemoryContext *dmem = sslGetDefaultDirectMemoryContextInline();
	const SslLowLevelMethodGetter *methods;
	SslCryptoPropertyNode *node;
	int algorithmId;
	int status;

	status = sslGetPublicKeyInfo(key, 2006, &algorithmId);
	if (status != 0)
		return status;
	if (algorithmId != 6)
		return SSL_CRYPTO_ERROR_FAILED;
	sslFreeLowLevelObject(state->object);
	state->flags &= ~SSL_RSA_KEY_MASK;
	state->object = NULL;
	state->loaded = 0;
	sslFreeHandlerListInline(&state->exportHandlers);
	if (privateKey != 0) {
		state->flags |= SSL_RSA_PRIVATE_KEY;
		methods = data->GetPrivateKeyMethods();
		status = sslRegisterRsaPrivateKeyPropertyHandlers(
			&state->exportHandlers, 2);
		if (status != 0)
			return status;
		status = sslRegisterRsaPrivateKeyPropertyHandlers(
			&state->importHandlers, 1);
		if (status != 0)
			return status;
	} else {
		state->flags |= SSL_RSA_PUBLIC_KEY;
		methods = data->GetPublicKeyMethods();
		status = sslRegisterRsaPublicKeyPropertyHandlers(
			&state->exportHandlers, 2);
		if (status != 0)
			return status;
		status = sslRegisterRsaPublicKeyPropertyHandlers(
			&state->importHandlers, 1);
		if (status != 0)
			return status;
	}
	status = data->RegisterHandlers(&state->exportHandlers, 2);
	if (status != 0)
		return status;
	status = data->RegisterHandlers(&state->importHandlers, 1);
	if (status != 0)
		return status;
	status = sslCreateLowLevelObject(&state->object, methods, dmem);
	if (status != 0)
		return sslMapCryptoBackendError(status);
	if (state->bigNumMethods != NULL) {
		status = sslSetLowLevelObjectInfo(state->object, 1, 4,
						  state->bigNumMethods);
		if (status != 0)
			return sslMapCryptoBackendError(status);
	}
	status = sslImportRsaKeyProperties(algorithm, key,
					   state->importHandlers, state->flags,
					   SSL_RSA_HANDLER_OPTIONAL);
	if (status != 0)
		return status;
	status = sslCopyRsaMultiPrimeComponentsToAlgorithmProperties(algorithm,
								     key);
	if (status != 0)
		return status;
	if (data->PostInit != NULL)
		status = data->PostInit(algorithm, key);
	return status;
}

int sslRsaGetHandlerInfo(SslCryptoAlgorithm *algorithm, void *backend,
			 SslCryptoPropertyNode *node, int selector, void *result)
{
	int status = SSL_CRYPTO_ERROR_NO_OPERATION;

	for (; node != NULL; node = node->next) {
		const SslCryptoPropertyHandler *handler;

		if (((const SslCryptoPropertyHandler *)node->handler)->selector !=
		    selector)
			continue;
		handler = node->handler;
		if (handler->get != NULL)
			return handler->get(algorithm, backend, handler->type,
					    handler->id,
					    (handler->flags & SSL_RSA_HANDLER_CACHED) != 0,
					    result);
	}
	return status;
}

int sslRsaGetInfo(SslCryptoAlgorithm *algorithm, int selector, void *result)
{
	const SslRsaSignBackendData *data = algorithm->implementationData;
	SslRsaSignBackendState *state = algorithm->backendState;
	int status = 0;

	if (selector == SSL_RSA_INFO_BLINDING) {
		if ((data->flags & 1) == 0)
			return SSL_CRYPTO_ERROR_NO_OPERATION;
		if ((state->flags & SSL_RSA_BLINDING) != 0)
			*(int *)result = 1;
		else
			*(int *)result = 0;
		return status;
	}
	return sslRsaGetHandlerInfo(algorithm, state->object,
				    state->infoHandlers, selector, result);
}

int sslRsaSetHandlerInfo(SslCryptoAlgorithm *algorithm,
			 SslCryptoPropertyNode *node, int selector,
			 const void *value)
{
	int status = SSL_CRYPTO_ERROR_NO_OPERATION;

	for (; node != NULL; node = node->next) {
		const SslCryptoPropertyHandler *handler = node->handler;

		if (handler->selector == selector && handler->set != NULL)
			return handler->set(algorithm, handler->type,
					    handler->id, value);
	}
	return status;
}

int sslRsaSignBackendSetInfo(SslCryptoAlgorithm *algorithm, int selector,
			     const int *value)
{
	const SslRsaSignBackendData *data = algorithm->implementationData;
	SslRsaSignBackendState *state = algorithm->backendState;
	int status = 0;

	if (selector == SSL_RSA_INFO_BLINDING) {
		if ((data->flags & 1) == 0)
			return SSL_CRYPTO_ERROR_NO_OPERATION;
		if (*value == 1)
			state->flags |= SSL_RSA_BLINDING;
		else
			state->flags &= ~SSL_RSA_BLINDING;
	} else {
		status = sslRsaSetHandlerInfo(algorithm, state->infoHandlers,
					      selector, value);
		if (status != 0)
			return status;
	}
	state->loaded = 0;
	return status;
}

__attribute__((noinline)) int sslExportRsaKeyProperties(SslCryptoAlgorithm *algorithm, void *backend,
			      SslCryptoPropertyNode *node)
{
	SslProperty *property = NULL;
	int status;

	for (; node != NULL; node = node->next) {
		const SslCryptoKeyPropertyHandler *handler = node->handler;

		if (sslFindProperty(algorithm->properties, handler->type,
				    handler->id, NULL, &property, 0) == 0 &&
		    handler->exportKey != NULL) {
			status = handler->exportKey(algorithm, backend,
						    property);
			if (status != 0)
				return status;
		} else if (handler->postExport != NULL) {
			status = handler->postExport(algorithm, backend,
						     handler);
			if (status != 0 &&
			    (handler->flags & SSL_RSA_HANDLER_OPTIONAL) == 0)
				return status;
		} else if ((handler->flags & SSL_RSA_HANDLER_OPTIONAL) == 0) {
			return SSL_CRYPTO_ERROR_UNSUPPORTED;
		}
	}
	return 0;
}

int sslRsaSignBackendSign(SslCryptoAlgorithm *algorithm, const void *digest,
			  SceUInt32 digestLength, void *signature,
			  SceUInt32 *signatureLength)
{
	SslRsaSignBackendState *state = algorithm->backendState;
	int status;

	if ((state->flags & SSL_RSA_PRIVATE_KEY) == 0)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	if (state->loaded == 0) {
		status = sslExportRsaKeyProperties(algorithm, state->object,
						   state->exportHandlers);
		if (status != 0)
			return status;
		status = sslExportRsaMultiPrimePropertiesToBackend(
			algorithm, state->object);
		if (status != 0)
			return status;
		state->loaded = 1;
	}
	if (signature == NULL || *signatureLength == 0) {
		SceUInt32 length = 0;

		status = sslGetLowLevelObjectInfo(state->object, 1, 7, &length);
		if (status != 0)
			return sslMapCryptoBackendError(status);
		*signatureLength = length;
		return status;
	}
	return sslMapCryptoBackendError(sslLowLevelSign(
		state->object, signature, signatureLength, *signatureLength,
		digest, digestLength, 0));
}

int sslRsaSignBackendVerify(SslCryptoAlgorithm *algorithm, const void *digest,
			    SceUInt32 digestLength, const void *signature,
			    SceUInt32 signatureLength, int *result)
{
	SslRsaSignBackendState *state = algorithm->backendState;
	int verified = 0;
	int status;

	*result = SSL_CRYPTO_ERROR_FAILED;
	if ((state->flags & SSL_RSA_PUBLIC_KEY) == 0)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	if (state->loaded == 0) {
		status = sslExportRsaKeyProperties(algorithm, state->object,
						   state->exportHandlers);
		if (status != 0)
			return status;
		status = sslExportRsaMultiPrimePropertiesToBackend(
			algorithm, state->object);
		if (status != 0)
			return status;
		state->loaded = 1;
	}
	status = sslLowLevelVerify(state->object, &verified, signature,
				   signatureLength, digest, digestLength,
				   (state->flags & SSL_RSA_BLINDING) != 0);
	if (status != 0)
		return sslMapCryptoBackendError(status);
	if (verified != 0)
		*result = 0;
	return 0;
}

int sslRegisterRsaCipherPropertyHandlers(SslCryptoPropertyNode **head,
					 int type)
{
	int status = 0;

	if (type == 0)
		status = sslPrependHandlerTableInline(
			head, sslBasePropertyHandlers, 3);
	if (status == 0) {
		if (type == 0)
			status = sslPrependHandlerTableInline(
				head, sslRsaCipherPropertyHandlers, 2);
		else if (type == 2)
			status = sslPrependKeyHandlerTableInline(
				head, sslRsaCipherExportHandlers, 1);
	}
	return status;
}

static const void *sslRsaCipherBackendMethods[11];

const SslCryptoImplementationMethods *sslGetRsaCipherBackendMethods(void)
{
	return (const SslCryptoImplementationMethods *)sslRsaCipherBackendMethods;
}

__attribute__((noinline)) int sslRsaCipherBackendDestroy(SslCryptoAlgorithm *algorithm)
{
	SslRsaCipherBackendState *state = algorithm->backendState;

	if (state != NULL) {
		sslFreeLowLevelObject(state->object);
		sslFreeBigNumMethodContext(state->bigNumMethods);
		sslFreeHandlerListInline(&state->infoHandlers);
		sslFreeHandlerListInline(&state->exportHandlers);
		sslDestroyRsaRandomAlgorithm(&state->random);
		sslFree(state);
		algorithm->backendState = NULL;
	}
	return 0;
}

int sslRsaCipherBackendInitialize(SslCryptoAlgorithm *algorithm)
{
	const SslRsaCipherBackendData *data = algorithm->implementationData;
	SslDirectMemoryContext *dmem = sslGetDefaultDirectMemoryContextInline();
	SslRsaCipherBackendState *state;
	SceUInt32 modes;
	int status;

	if (data == NULL) {
		status = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
		goto error;
	}
	state = sslAllocate(sizeof(*state));
	if (state == NULL) {
		status = SSL_LIBRARY_ERROR_ALLOCATION;
		goto error;
	}
	sslFillMemory(state, 0, sizeof(*state));
	algorithm->backendState = state;
	state->flags = data->flags;
	if (algorithm->family != 1) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		goto error;
	}
	modes = state->modes;
	if ((algorithm->mode & 0x50) == 0x50 &&
	    data->GetPrivateDecryptMethods != NULL)
		state->modes = modes |= SSL_RSA_PRIVATE_DECRYPT;
	if ((algorithm->mode & 0x48) == 0x48 &&
	    data->GetPrivateEncryptMethods != NULL)
		state->modes = modes |= SSL_RSA_PRIVATE_ENCRYPT;
	if ((algorithm->mode & 0x30) == 0x30 &&
	    data->GetPublicDecryptMethods != NULL)
		state->modes = modes |= SSL_RSA_PUBLIC_DECRYPT;
	if ((algorithm->mode & 0x28) == 0x28 &&
	    data->GetPublicEncryptMethods != NULL)
		state->modes = modes |= SSL_RSA_PUBLIC_ENCRYPT;
	if (modes == 0) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		goto error;
	}
	status = data->RegisterHandlers(&state->infoHandlers, 0);
	if (status != 0)
		goto error;
	if (algorithm->properties == NULL) {
		algorithm->properties = sslNewPropertyList();
		if (algorithm->properties == NULL) {
			status = SSL_LIBRARY_ERROR_ALLOCATION;
			goto error;
		}
		algorithm->flags |= SSL_CRYPTO_ALGORITHM_OWNS_PROPERTIES;
	}
	if (data->selectBigNumMethod != NULL) {
		status = sslNewBigNumMethodContext(&state->bigNumMethods, dmem);
		if (status != 0)
			status = sslMapCryptoBackendError(status);
		else
			sslSetBigNumMethodSelector(state->bigNumMethods,
						   data->selectBigNumMethod);
	}
	if (status != 0)
		goto error;
	return status;

error:
	sslRsaCipherBackendDestroy(algorithm);
	return status;
}

int sslRsaCipherBackendInit(SslCryptoAlgorithm *algorithm, void *key)
{
	SslRsaCipherBackendState *state = algorithm->backendState;
	const SslRsaCipherBackendData *data = algorithm->implementationData;
	SslCryptoPropertyNode *publicHandlers = NULL;
	SslCryptoPropertyNode *privateHandlers = NULL;
	int value[2] = { 0, 0 };
	SslCryptoPropertyNode *node;
	int status;

	state->flags &= ~SSL_RSA_KEY_MASK;
	sslFreeHandlerListInline(&state->exportHandlers);
	if ((state->modes & (SSL_RSA_PRIVATE_ENCRYPT |
			     SSL_RSA_PRIVATE_DECRYPT)) != 0 &&
	    sslGetPublicKeyInfo(key, 18, value) == 0) {
		status = sslRegisterRsaPrivateKeyPropertyHandlers(
			&privateHandlers, 1);
		if (status != 0)
			goto out;
	}
	if ((state->modes & (SSL_RSA_PUBLIC_ENCRYPT |
			     SSL_RSA_PUBLIC_DECRYPT)) != 0 &&
	    sslGetPublicKeyInfo(key, 17, value) == 0) {
		status = sslRegisterRsaPublicKeyPropertyHandlers(
			&publicHandlers, 1);
		if (status != 0)
			goto out;
	}
	if (publicHandlers != NULL) {
		status = sslImportRsaKeyProperties(algorithm, key, publicHandlers,
						   SSL_RSA_PUBLIC_KEY,
						   SSL_RSA_HANDLER_OPTIONAL);
		if (status != SSL_CRYPTO_ERROR_UNSUPPORTED && status != 0)
			goto out;
		if (status == 0)
			state->flags |= SSL_RSA_PUBLIC_KEY;
	}
	if (privateHandlers != NULL) {
		status = sslImportRsaKeyProperties(algorithm, key, privateHandlers,
						   SSL_RSA_PRIVATE_KEY,
						   SSL_RSA_HANDLER_OPTIONAL);
		if (status != SSL_CRYPTO_ERROR_UNSUPPORTED && status != 0)
			goto out;
		if (status == 0)
			state->flags |= SSL_RSA_PRIVATE_KEY;
	}
	if ((state->flags & SSL_RSA_KEY_MASK) == 0) {
		status = 10022;
		goto out;
	}
	status = sslCopyRsaMultiPrimeComponentsToAlgorithmProperties(algorithm,
								     key);
	if (status == 0 && data->PostInit != NULL)
		status = data->PostInit(algorithm, key);
out:
	while (privateHandlers != NULL) {
		node = privateHandlers->next;
		sslFree(privateHandlers);
		privateHandlers = node;
	}
	while (publicHandlers != NULL) {
		node = publicHandlers->next;
		sslFree(publicHandlers);
		publicHandlers = node;
	}
	return status;
}

int sslRsaCipherCreateObject(SslRsaCipherBackendState *state,
			     const SslLowLevelMethodGetter *methods)
{
	SslDirectMemoryContext *dmem = sslGetDefaultDirectMemoryContextInline();
	int status;

	sslFreeLowLevelObject(state->object);
	state->object = NULL;
	status = sslCreateLowLevelObject(&state->object, methods, dmem);
	if (status != 0)
		return sslMapCryptoBackendError(status);
	if (state->bigNumMethods != NULL) {
		status = sslSetLowLevelObjectInfo(state->object, 1, 4,
						  state->bigNumMethods);
		if (status != 0)
			return sslMapCryptoBackendError(status);
	}
	return status;
}

int sslRsaPadToModulusLength(SslCryptoAlgorithm *algorithm,
			     unsigned char *output, SceUInt32 outputMax,
			     SceUInt32 *outputLength)
{
	SslRsaCipherBackendState *state = algorithm->backendState;
	SceUInt32 modulusLength = 0;
	SceUInt32 length = *outputLength;
	int status;

	status = sslGetRsaKeyModulusByteLength(algorithm, state->object, 0, 0,
					       0, &modulusLength);
	if (status == 0) {
		if ((int)modulusLength > (int)outputMax) {
			status = 10016;
		} else {
			int shift = (int)(modulusLength - length);

			if (shift > 0) {
				int i;

				for (i = (int)length - 1; i >= 0; i--)
					output[i + shift] = output[i];
				sslFillMemory(output, 0, shift);
				*outputLength = modulusLength;
			}
		}
	}
	return status;
}

static __inline int sslRsaCipherCreateObjectInline(
	SslRsaCipherBackendState *state, const SslLowLevelMethodGetter *methods)
{
	SslDirectMemoryContext *dmem = sslGetDefaultDirectMemoryContextInline();
	int status;

	sslFreeLowLevelObject(state->object);
	state->object = NULL;
	status = sslCreateLowLevelObject(&state->object, methods, dmem);
	if (status != 0)
		return sslMapCryptoBackendError(status);
	if (state->bigNumMethods != NULL) {
		status = sslSetLowLevelObjectInfo(state->object, 1, 4,
						  state->bigNumMethods);
		if (status != 0)
			return sslMapCryptoBackendError(status);
	}
	return status;
}

static __inline int sslRsaPadToModulusLengthInline(
	SslCryptoAlgorithm *algorithm, unsigned char *output,
	SceUInt32 outputMax, SceUInt32 *outputLength)
{
	SslRsaCipherBackendState *state = algorithm->backendState;
	SceUInt32 modulusLength = 0;
	SceUInt32 length = *outputLength;
	int status;

	status = sslGetRsaKeyModulusByteLength(algorithm, state->object, 0, 0,
					       0, &modulusLength);
	if (status == 0) {
		if ((int)modulusLength > (int)outputMax) {
			status = 10016;
		} else {
			int shift = (int)(modulusLength - length);

			if (shift > 0) {
				int i;

				for (i = (int)length - 1; i >= 0; i--)
					output[i + shift] = output[i];
				sslFillMemory(output, 0, shift);
				*outputLength = modulusLength;
			}
		}
	}
	return status;
}

int sslRsaCipherPublicOperation(SslCryptoAlgorithm *algorithm,
				unsigned char *output, SceUInt32 *outputLength,
				SceUInt32 outputMax, const void *input,
				SceUInt32 inputLength, void *random,
				int encrypt)
{
	const SslRsaCipherBackendData *data = algorithm->implementationData;
	SslRsaCipherBackendState *state = algorithm->backendState;
	int status;

	if (output == NULL)
		return sslRsaGetHandlerInfo(algorithm, state->object,
					    state->infoHandlers, 1,
					    outputLength);
	if ((state->flags & SSL_RSA_PUBLIC_KEY) == 0 ||
	    ((encrypt & 1) != 0 && (state->modes & SSL_RSA_PUBLIC_ENCRYPT) == 0) ||
	    (encrypt | (state->modes & SSL_RSA_PUBLIC_DECRYPT)) == 0)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (encrypt == 0)
		state->flags &= ~SSL_RSA_PRIVATE_OPERATION;
	else
		state->flags |= SSL_RSA_PRIVATE_OPERATION;
	if (state->exportHandlers == NULL) {
		status = sslRegisterRsaPublicKeyPropertyHandlers(
			&state->exportHandlers, 2);
		if (status != 0)
			return status;
		status = data->RegisterHandlers(&state->exportHandlers, 2);
		if (status != 0)
			return status;
	}
	if (encrypt != 0)
		status = sslRsaCipherCreateObjectInline(
			state, data->GetPublicEncryptMethods());
	else
		status = sslRsaCipherCreateObjectInline(
			state, data->GetPublicDecryptMethods());
	if (status != 0)
		return status;
	status = sslExportRsaKeyProperties(algorithm, state->object,
					   state->exportHandlers);
	if (status != 0)
		return status;
	status = sslExportRsaMultiPrimePropertiesToBackend(algorithm,
							   state->object);
	if (status != 0)
		return status;
	status = sslLowLevelCipherOperate(state->object, output, outputLength,
					  outputMax, input, inputLength, 0,
					  encrypt);
	if (status != 0)
		return sslMapCryptoBackendError(status);
	if (encrypt == 0 && (data->flags & 0x10) != 0)
		status = sslRsaPadToModulusLengthInline(algorithm, output,
							outputMax,
							outputLength);
	return status;
}

int sslRsaCipherPublicEncrypt(SslCryptoAlgorithm *algorithm,
			      unsigned char *output, SceUInt32 *outputLength,
			      SceUInt32 outputMax, const void *input,
			      SceUInt32 inputLength, void *random)
{
	return sslRsaCipherPublicOperation(algorithm, output, outputLength,
					   outputMax, input, inputLength,
					   random, 1);
}

int sslRsaCipherPublicDecrypt(SslCryptoAlgorithm *algorithm,
			      unsigned char *output, SceUInt32 *outputLength,
			      SceUInt32 outputMax, const void *input,
			      SceUInt32 inputLength, void *random)
{
	return sslRsaCipherPublicOperation(algorithm, output, outputLength,
					   outputMax, input, inputLength,
					   random, 0);
}

int sslRsaCipherPrivateOperation(SslCryptoAlgorithm *algorithm,
				 unsigned char *output,
				 SceUInt32 *outputLength, SceUInt32 outputMax,
				 const void *input, SceUInt32 inputLength,
				 void *random, int encrypt)
{
	const SslRsaCipherBackendData *data = algorithm->implementationData;
	SslRsaCipherBackendState *state = algorithm->backendState;
	int status;

	if (output == NULL)
		return sslRsaGetHandlerInfo(algorithm, state->object,
					    state->infoHandlers, 1,
					    outputLength);
	if ((state->flags & SSL_RSA_PRIVATE_KEY) == 0 ||
	    ((encrypt & 1) != 0 &&
	     (state->modes & SSL_RSA_PRIVATE_ENCRYPT) == 0) ||
	    (encrypt | (state->modes & SSL_RSA_PRIVATE_DECRYPT)) == 0)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	state->flags |= SSL_RSA_PRIVATE_OPERATION;
	if (state->exportHandlers == NULL) {
		status = sslRegisterRsaPrivateKeyPropertyHandlers(
			&state->exportHandlers, 2);
		if (status != 0)
			return status;
		status = data->RegisterHandlers(&state->exportHandlers, 2);
		if (status != 0)
			return status;
	}
	if (encrypt != 0)
		status = sslRsaCipherCreateObjectInline(
			state, data->GetPrivateEncryptMethods());
	else
		status = sslRsaCipherCreateObjectInline(
			state, data->GetPrivateDecryptMethods());
	if (status != 0)
		return status;
	status = sslExportRsaKeyProperties(algorithm, state->object,
					   state->exportHandlers);
	if (status != 0)
		return status;
	status = sslExportRsaMultiPrimePropertiesToBackend(algorithm,
							   state->object);
	if (status != 0)
		return status;
	status = sslLowLevelCipherOperate(state->object, output, outputLength,
					  outputMax, input, inputLength, 0,
					  encrypt);
	if (status != 0)
		return sslMapCryptoBackendError(status);
	if (encrypt == 0 && (data->flags & 0x10) != 0)
		status = sslRsaPadToModulusLengthInline(algorithm, output,
							outputMax,
							outputLength);
	return status;
}

int sslRsaCipherPrivateEncrypt(SslCryptoAlgorithm *algorithm,
			       unsigned char *output, SceUInt32 *outputLength,
			       SceUInt32 outputMax, const void *input,
			       SceUInt32 inputLength, void *random)
{
	return sslRsaCipherPrivateOperation(algorithm, output, outputLength,
					    outputMax, input, inputLength,
					    random, 1);
}

int sslRsaCipherPrivateDecrypt(SslCryptoAlgorithm *algorithm,
			       unsigned char *output, SceUInt32 *outputLength,
			       SceUInt32 outputMax, const void *input,
			       SceUInt32 inputLength, void *random)
{
	return sslRsaCipherPrivateOperation(algorithm, output, outputLength,
					    outputMax, input, inputLength,
					    random, 0);
}

int sslRsaCipherBackendGetInfo(SslCryptoAlgorithm *algorithm, int selector,
			       void *result)
{
	SslRsaCipherBackendState *state = algorithm->backendState;

	return sslRsaGetHandlerInfo(algorithm, state->object,
				    state->infoHandlers, selector, result);
}

int sslRsaCipherBackendSetInfo(SslCryptoAlgorithm *algorithm, int selector,
			       const void *value)
{
	SslRsaCipherBackendState *state = algorithm->backendState;

	return sslRsaSetHandlerInfo(algorithm, state->infoHandlers, selector,
				    value);
}

int sslGetRsaRandomAlgorithmInfo(SslCryptoAlgorithm *algorithm, void *result)
{
	return sslGetAlgorithmInfo(algorithm, 11003, result);
}

__attribute__((noinline)) int sslExportRsaBlindingRandomToLowLevel(
	SslCryptoAlgorithm *algorithm, void *backend,
	const SslCryptoKeyPropertyHandler *handler)
{
	SslCryptoContext *context = algorithm->context;
	SslRsaCipherBackendState *state = algorithm->backendState;
	int enable = 1;
	void *random = NULL;
	SslCryptoAlgorithm *randomAlgorithm = NULL;
	int status;

	if (context->randomAlgorithm != NULL) {
		randomAlgorithm = context->randomAlgorithm;
	} else if (state->random != NULL) {
		randomAlgorithm = state->random;
	} else {
		status = sslCreateAlgorithm(&context->interface,
					    SSL_CRYPTO_FAMILY_RANDOM,
					    SSL_CRYPTO_RANDOM_DEFAULT, 0,
					    &randomAlgorithm);
		if (status != 0)
			return status;
		state->random = randomAlgorithm;
	}
	status = sslSetAlgorithmInfo(randomAlgorithm, 56, (SceUIntPtr)&enable);
	if (status != 0)
		return status;
	if (sslGetAlgorithmInfo(randomAlgorithm, 11003, &random) != 0)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	status = sslSetLowLevelObjectInfo(backend, handler->type, handler->id,
					  random);
	if (status != 0)
		return sslMapCryptoBackendError(status);
	return 0;
}

int sslExportRsaCipherBlindingRandom(SslCryptoAlgorithm *algorithm,
				     void *backend,
				     const SslCryptoKeyPropertyHandler *handler)
{
	SslRsaCipherBackendState *state = algorithm->backendState;
	int status = 0;

	if ((state->flags & SSL_RSA_PRIVATE_OPERATION) != 0)
		status = sslExportRsaBlindingRandomToLowLevel(algorithm,
							      backend, handler);
	return status;
}

static const void *sslRsaSignBackendMethods[13] = {
	sslRsaSignBackendInitialize,
	sslRsaSignBackendDestroy,
	NULL,
	sslRsaGetInfo,
	sslRsaSignBackendSetInfo,
	NULL,
	sslRsaSignBackendInit,
	sslRsaSignBackendSign,
	NULL,
	NULL,
	sslRsaSignBackendVerify,
	NULL,
	NULL
};

static const void *sslRsaCipherBackendMethods[11] = {
	sslRsaCipherBackendInitialize,
	sslRsaCipherBackendDestroy,
	NULL,
	sslRsaCipherBackendGetInfo,
	sslRsaCipherBackendSetInfo,
	NULL,
	sslRsaCipherBackendInit,
	sslRsaCipherPublicEncrypt,
	sslRsaCipherPublicDecrypt,
	sslRsaCipherPrivateEncrypt,
	sslRsaCipherPrivateDecrypt
};

const SslRsaSignBackendData sslRsaSignBackendData = {
	sslGetRsaPkcs1SignChain,
	sslGetRsaPkcs1VerifyChain,
	sslSelectBigNumMethod,
	sslRegisterRsaSignPropertyHandlers,
	NULL,
	0xffffffffu,
	0
};

const SslRsaCipherBackendData sslRsaPkcs1BackendData = {
	sslGetRsaRawPublicEncryptMethods,
	sslGetRsaRawPublicEncryptMethods,
	sslGetRsaRawPrivateDecryptMethods,
	sslGetRsaRawPrivateDecryptMethods,
	sslSelectBigNumMethod,
	0x10,
	sslRegisterRsaCipherPropertyHandlers,
	NULL,
	{ 0, 0 }
};

const SslRsaCipherBackendData sslRsaRawBackendData = {
	sslGetRsaPkcs1EncryptChain,
	(SslRsaMethodList)sslGetRsaPublicKeyMethodList,
	(SslRsaMethodList)sslGetRsaPrivateKeyMethodList,
	sslGetRsaPkcs1DecryptChain,
	sslSelectDefaultBigNumMethod,
	0,
	sslRegisterRsaCipherPropertyHandlers,
	NULL,
	{ 0, 0 }
};
