/* Filename and private descriptive names are ours; CRYPTOCME digest backend. */
#include "cry_api.h"
#include "../dmem/dmem.h"
#include "../../../../memory.h"

typedef struct SslDigestBackendData {
	const void *(*GetDescriptor)(void);
	const void *(**(*GetDescriptorList)(int selector))(void);
} SslDigestBackendData;

typedef struct SslDigestBackendState {
	void *context;
	int initialized;
} SslDigestBackendState;

typedef struct SslDigestImplementationMethods {
	int (*Initialize)(SslCryptoAlgorithm *algorithm);
	int (*Destroy)(SslCryptoAlgorithm *algorithm);
	int (*Copy)(SslCryptoAlgorithm *source, SslCryptoAlgorithm *destination);
	int (*GetInfo)(SslCryptoAlgorithm *algorithm, int selector, void *result);
	int (*SetInfo)(SslCryptoAlgorithm *algorithm, int selector,
		       SceUIntPtr value);
	void *reserved20;
	int (*Init)(SslCryptoAlgorithm *algorithm);
	int (*Operate)(SslCryptoAlgorithm *algorithm, const void *data,
		       SceUInt32 length, void *digest, SceUInt32 *digestLength);
	int (*Update)(SslCryptoAlgorithm *algorithm, const void *data,
		      SceUInt32 length);
	int (*Final)(SslCryptoAlgorithm *algorithm, void *digest,
		     SceUInt32 *length);
} SslDigestImplementationMethods;

extern int sslCreateDigestBackendContext(void **context, const void *descriptor,
					 void *directMemory);
extern int sslDestroyDigestBackendContext(void *context);
extern int sslInitDigestBackendContext(void *context);
extern int sslUpdateDigestBackendContext(void *context, const void *data,
					 SceUInt32 length);
extern int sslFinalizeDigestBackendContext(void *context, void *digest);
extern int sslGetDigestBackendContextInfo(void *context, int selector,
					  void *result, SceUInt32 length);
extern int sslSaveDigestBackendContext(void *context, void *buffer,
				       SceUInt32 *length, SceUInt32 maxLength);
extern int sslRestoreDigestBackendContext(void *context, const void *buffer,
					  SceUInt32 *length,
					  SceUInt32 inputLength);

extern const void *sslGetMd5DigestDescriptor(void);
extern const void *sslGetSha1DigestDescriptor(void);
extern const void *sslGetSha224DigestDescriptor(void);
extern const void *sslGetSha256DigestDescriptor(void);
extern const void *sslGetSha384DigestDescriptor(void);
extern const void *sslGetSha512DigestDescriptor(void);

int sslDigestBackendInitialize(SslCryptoAlgorithm *algorithm);
int sslDigestBackendDestroy(SslCryptoAlgorithm *algorithm);
int sslDigestBackendCopy(SslCryptoAlgorithm *source,
			 SslCryptoAlgorithm *destination);
int sslDigestBackendGetInfo(SslCryptoAlgorithm *algorithm, int selector,
			    void *result);
int sslDigestBackendSetInfo(SslCryptoAlgorithm *algorithm, int selector,
			    SceUIntPtr value);
int sslDigestBackendInit(SslCryptoAlgorithm *algorithm);
int sslDigestBackendOperate(SslCryptoAlgorithm *algorithm, const void *data,
			    SceUInt32 length, void *digest,
			    SceUInt32 *digestLength);
int sslDigestBackendUpdate(SslCryptoAlgorithm *algorithm, const void *data,
			   SceUInt32 length);
int sslDigestBackendFinal(SslCryptoAlgorithm *algorithm, void *digest,
			  SceUInt32 *length);

static const SslDigestImplementationMethods sslDigestBackendMethods = {
	sslDigestBackendInitialize,
	sslDigestBackendDestroy,
	sslDigestBackendCopy,
	sslDigestBackendGetInfo,
	sslDigestBackendSetInfo,
	NULL,
	sslDigestBackendInit,
	sslDigestBackendOperate,
	sslDigestBackendUpdate,
	sslDigestBackendFinal
};

SslDigestBackendData sslMd5BackendData = {
	sslGetMd5DigestDescriptor,
	NULL
};

SslDigestBackendData sslSha1BackendData = {
	sslGetSha1DigestDescriptor,
	NULL
};

SslDigestBackendData sslSha224BackendData = {
	sslGetSha224DigestDescriptor,
	NULL
};

SslDigestBackendData sslSha256BackendData = {
	sslGetSha256DigestDescriptor,
	NULL
};

SslDigestBackendData sslSha384BackendData = {
	sslGetSha384DigestDescriptor,
	NULL
};

SslDigestBackendData sslSha512BackendData = {
	sslGetSha512DigestDescriptor,
	NULL
};

int sslDigestBackendInitialize(SslCryptoAlgorithm *algorithm)
{
	void *context = NULL;
	int status = SSL_CRYPTO_ERROR_NO_IMPLEMENTATION;
	void *directMemory = sslGetDefaultDirectMemoryContext();
	const SslDigestBackendData *data = algorithm->implementationData;
	const void *(*getDescriptor)(void) = data->GetDescriptor;
	SslDigestBackendState *state;

	if (getDescriptor != NULL) {
		state = sslAllocate(sizeof(*state));
		if (state != NULL) {
			status = sslMapCryptoBackendError(
				sslCreateDigestBackendContext(&context,
							      getDescriptor(),
							      directMemory));
			if (status == 0) {
				state->context = context;
				state->initialized = 0;
				algorithm->backendState = state;
			} else {
				sslFree(state);
			}
		}
	}
	return status;
}

int sslDigestBackendDestroy(SslCryptoAlgorithm *algorithm)
{
	SslDigestBackendState *state = algorithm->backendState;
	int status = 0;

	if (state != NULL) {
		status = sslMapCryptoBackendError(
			sslDestroyDigestBackendContext(state->context));
		sslFree(state);
		algorithm->backendState = NULL;
	}
	return status;
}

int sslDigestBackendCopy(SslCryptoAlgorithm *source,
			 SslCryptoAlgorithm *destination)
{
	SslDigestBackendState *dstState = destination->backendState;
	SslDigestBackendState *srcState;
	void *buffer;
	SceUInt32 stateSize;
	int status;

	if (dstState == NULL || (srcState = source->backendState) == NULL)
		return 10005;
	stateSize = 0;
	status = sslMapCryptoBackendError(
		sslInitDigestBackendContext(dstState->context));
	if (status == 0) {
		dstState->initialized = 1;
		status = sslMapCryptoBackendError(
			sslSaveDigestBackendContext(srcState->context, NULL,
						    &stateSize, 0));
		if (status == 0) {
			buffer = sslAllocate(stateSize);
			if (buffer != NULL) {
				status = sslMapCryptoBackendError(
					sslSaveDigestBackendContext(
						srcState->context, buffer,
						&stateSize, stateSize));
				if (status == 0) {
					status = sslMapCryptoBackendError(
						sslRestoreDigestBackendContext(
							dstState->context,
							buffer, NULL,
							stateSize));
				}
			} else {
				status = 10005;
			}
			if (buffer != NULL)
				sslFree(buffer);
		}
	}
	return status;
}

int sslDigestBackendGetInfo(SslCryptoAlgorithm *algorithm, int selector,
			    void *result)
{
	SslDigestBackendState *state = algorithm->backendState;
	void *context = state->context;
	int status = 0;
	const SslDigestBackendData *data = algorithm->implementationData;
	SceUInt32 value = 0;
	const void *(*getDescriptor)(void) = data->GetDescriptor;

	switch (selector) {
	case 11:
		status = sslMapCryptoBackendError(
			sslGetDigestBackendContextInfo(context, 5, &value, 0));
		*(SceUInt32 *)result = value;
		break;
	case 13:
		status = sslMapCryptoBackendError(
			sslGetDigestBackendContextInfo(context, 4, &value, 0));
		*(SceUInt32 *)result = value;
		break;
	case 11001:
		*(void **)result = context;
		break;
	case 11002:
		*(const void **)result = getDescriptor();
		break;
	default:
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		break;
	}
	return status;
}

int sslDigestBackendSetInfo(SslCryptoAlgorithm *algorithm, int selector,
			    SceUIntPtr value)
{
	int index;
	SceUInt32 stateSize;
	const SslDigestBackendData *data;
	void *directMemory;
	const void *(**(*getDescriptorList)(int))(void);
	const void *(**descriptors)(void);
	void *buffer;
	int status;

	if (selector != 87)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	index = *(int *)value;
	stateSize = 0;
	data = algorithm->implementationData;
	descriptors = NULL;
	directMemory = sslGetDefaultDirectMemoryContext();
	if (algorithm->backendState == NULL) {
		status = SSL_LIBRARY_ERROR_ALLOCATION;
	} else {
		getDescriptorList = data->GetDescriptorList;
		if (getDescriptorList != NULL)
			descriptors = getDescriptorList(0);
		for (; descriptors != NULL && *descriptors != NULL &&
		       index > 0;
		     descriptors++, index--)
			;
		if (descriptors == NULL || *descriptors == NULL) {
			status = SSL_CRYPTO_ERROR_NO_IMPLEMENTATION;
		} else {
			status = sslMapCryptoBackendError(
				sslSaveDigestBackendContext(
					((SslDigestBackendState *)
						 algorithm->backendState)
						->context,
					NULL, &stateSize, 0));
			if (status == 0) {
				buffer = sslAllocate(stateSize);
				if (buffer != NULL) {
					status = sslMapCryptoBackendError(
						sslSaveDigestBackendContext(
							((SslDigestBackendState *)
								 algorithm
									 ->backendState)
								->context,
							buffer, &stateSize,
							stateSize));
					if (status == 0) {
						status = sslMapCryptoBackendError(
							sslCreateDigestBackendContext(
								&((SslDigestBackendState *)
									  algorithm
										  ->backendState)
									 ->context,
								(const void *)*descriptors,
								directMemory));
						if (status == 0) {
							status = sslMapCryptoBackendError(
								sslRestoreDigestBackendContext(
									((SslDigestBackendState *)
										 algorithm
											 ->backendState)
										->context,
									buffer,
									NULL,
									stateSize));
						}
					}
				} else {
					status = SSL_LIBRARY_ERROR_ALLOCATION;
				}
				if (buffer != NULL)
					sslFree(buffer);
			}
		}
	}
	return status;
}

int sslDigestBackendInit(SslCryptoAlgorithm *algorithm)
{
	SslDigestBackendState *state = algorithm->backendState;
	int status;

	if (state == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	status = sslMapCryptoBackendError(
		sslInitDigestBackendContext(state->context));
	if (status == 0)
		state->initialized = 1;
	return status;
}

int sslDigestBackendOperate(SslCryptoAlgorithm *algorithm, const void *data,
			    SceUInt32 length, void *digest,
			    SceUInt32 *digestLength)
{
	SslDigestBackendState *state = algorithm->backendState;
	SceUInt32 size = 0;
	int status;

	if (state->initialized == 0)
		return SSL_CRYPTO_ERROR_NOT_INITIALIZED;
	status = sslMapCryptoBackendError(
		sslUpdateDigestBackendContext(state->context, data, length));
	if (status == 0) {
		status = sslMapCryptoBackendError(
			sslGetDigestBackendContextInfo(state->context, 5, &size,
						       0));
		if (status == 0) {
			status = sslMapCryptoBackendError(
				sslFinalizeDigestBackendContext(state->context,
								digest));
			if (status == 0) {
				status = sslMapCryptoBackendError(
					sslInitDigestBackendContext(
						state->context));
				if (status == 0 && digestLength != NULL)
					*digestLength = size;
			}
		}
	}
	return status;
}

int sslDigestBackendUpdate(SslCryptoAlgorithm *algorithm, const void *data,
			   SceUInt32 length)
{
	SslDigestBackendState *state = algorithm->backendState;

	if (state->initialized == 0)
		return SSL_CRYPTO_ERROR_NOT_INITIALIZED;
	return sslMapCryptoBackendError(
		sslUpdateDigestBackendContext(state->context, data, length));
}

int sslDigestBackendFinal(SslCryptoAlgorithm *algorithm, void *digest,
			  SceUInt32 *length)
{
	SslDigestBackendState *state = algorithm->backendState;
	void *context = state->context;
	SceUInt32 size = 0;
	int status;

	status = sslMapCryptoBackendError(
		sslGetDigestBackendContextInfo(context, 5, &size, 0));
	if (status == 0) {
		status = sslMapCryptoBackendError(
			sslFinalizeDigestBackendContext(context, digest));
		if (status == 0) {
			status = sslMapCryptoBackendError(
				sslInitDigestBackendContext(context));
			if (status == 0 && length != NULL)
				*length = size;
		}
	}
	return status;
}

const SslCryptoImplementationMethods *sslGetDigestBackendMethods(void)
{
	return (const SslCryptoImplementationMethods *)(SceUIntPtr)&sslDigestBackendMethods;
}
