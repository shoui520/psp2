/* Filename and private descriptive names are ours; CRYPTOCME cipher backend. */
#include "ctx.h"
#include "../dmem/dmem.h"
#include "../../key/key_api.h"
#include "../../../../memory.h"

typedef struct SslCipherBackendData {
	int fixedKeySize;
	const void *(*GetDescriptor)(int selector);
	const void *(**(*GetDescriptorList)(int selector))(void);
} SslCipherBackendData;

typedef struct SslPrngBackendStatePrefix {
	void *lowLevelPrng;
	void *entropySource;
	SceUInt32 flags;
	int seedThreshold;
	int seedCount;
	void *userCallback;
} SslPrngBackendStatePrefix;

extern void sslFreeLowLevelCipher(void *cipher);
extern int sslCreateLowLevelCipher(void **cipher, const void *descriptor,
				   SslDirectMemoryContext *dmem);
extern int sslControlLowLevelCipher(void *cipher, int op, SceUInt32 arg1,
				    int arg2, int arg3, int arg4);
extern int sslInitLowLevelCipher(void *cipher, const void *key,
				 SceUInt32 keyLen, const void *iv, int encrypt);
extern int sslProcessLowLevelCipher(void *cipher, void *out, SceUInt32 *outLen,
				    SceUInt32 maxOutLen, const void *in,
				    SceUInt32 inLen, int mode);
extern int sslSetLowLevelCipherInfo(void *cipher, int selector,
				    SceUInt32 intVal, const void *ptrVal);
extern int sslGetLowLevelCipherInfo(void *cipher, int selector, void *intOut,
				    void *ptrOut);
extern int sslSaveLowLevelCipherState(void *cipher, void *buf, SceUInt32 *len,
				      SceUInt32 maxLen);
extern int sslRestoreLowLevelCipherState(void *cipher, const void *buf,
					 SceUInt32 len);
extern int sslGetLowLevelPrngInfo(void *prng, int selector, int arg,
				  void *result);
extern int sslPkcs5Pad(void);
extern int sslIso10126Pad(void);

extern const void *sslGetDesEde3CbcCipherDescriptor(void);
extern const void *sslGetRc4CipherDescriptor(void);
extern const void *sslGetAesCbcCipherDescriptor(void);
extern const void *sslGetCamelliaCbcCipherDescriptor(void);

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

int sslCipherBackendInitialize(SslCryptoAlgorithm *algorithm);
int sslCipherBackendDestroy(SslCryptoAlgorithm *algorithm);
int sslCipherBackendCopy(SslCryptoAlgorithm *source,
			 SslCryptoAlgorithm *destination);
int sslCipherBackendGetInfo(SslCryptoAlgorithm *algorithm, int selector,
			    void *result);
int sslCipherBackendSetInfo(SslCryptoAlgorithm *algorithm, int selector,
			    SceUIntPtr value);
int sslCipherBackendInit(SslCryptoAlgorithm *algorithm, void *key,
			 const SslDesKeyBuffer *iv, int encrypt);
int sslCipherBackendOperate(SslCryptoAlgorithm *algorithm, const void *input,
			    SceUInt32 inputLength, void *output,
			    SceUInt32 *outputLength);
int sslCipherBackendUpdate(SslCryptoAlgorithm *algorithm, const void *input,
			   SceUInt32 inputLength, void *output,
			   SceUInt32 *outputLength);
int sslCipherBackendFinal(SslCryptoAlgorithm *algorithm, void *output,
			  SceUInt32 *outputLength);

static const SslCryptoImplementationMethods sslCipherBackendMethods = {
	sslCipherBackendInitialize,
	sslCipherBackendDestroy,
	sslCipherBackendCopy,
	sslCipherBackendGetInfo,
	sslCipherBackendSetInfo,
	NULL,
	(void *)sslCipherBackendInit,
	(void *)sslCipherBackendOperate,
	(void *)sslCipherBackendUpdate,
	(void *)sslCipherBackendFinal,
	NULL,
	NULL,
	NULL
};

SslCipherBackendData sslDesEde3CbcBackendData = {
	24,
	(const void *(*)(int))sslGetDesEde3CbcCipherDescriptor,
	NULL
};

SslCipherBackendData sslRc4BackendData = {
	-1,
	(const void *(*)(int))sslGetRc4CipherDescriptor,
	NULL
};

SslCipherBackendData sslAesCbcBackendData = {
	16,
	(const void *(*)(int))sslGetAesCbcCipherDescriptor,
	NULL
};

SslCipherBackendData sslCamelliaCbcBackendData = {
	32,
	(const void *(*)(int))sslGetCamelliaCbcCipherDescriptor,
	NULL
};

SslDirectMemoryContext *sslGetOrCreateDefaultDirectMemoryContext(void)
{
	return sslGetDefaultDirectMemoryContextInline();
}

int sslCipherBackendDestroy(SslCryptoAlgorithm *algorithm)
{
	if (algorithm->backendState != NULL) {
		sslFreeLowLevelCipher(algorithm->backendState);
		algorithm->backendState = NULL;
	}
	return 0;
}

int sslCipherBackendUpdate(SslCryptoAlgorithm *algorithm, const void *input,
			   SceUInt32 inputLength, void *output,
			   SceUInt32 *outputLength)
{
	return sslMapCryptoBackendError(sslProcessLowLevelCipher(
		algorithm->backendState, output, outputLength, *outputLength,
		input, inputLength, 0));
}

int sslSetCipherPaddingMode(SslCryptoAlgorithm *algorithm, int padFlag,
			    SceUInt32 mode)
{
	void *state = algorithm->backendState;
	int (*padFunc)(void);
	int status;

	status = sslMapCryptoBackendError(
		sslSetLowLevelCipherInfo(state, 34, (SceUInt32)padFlag, NULL));
	if (status == 0) {
		if (mode == 0)
			padFunc = sslPkcs5Pad;
		else if (mode == 1)
			padFunc = sslIso10126Pad;
		else
			return SSL_CRYPTO_ERROR_NO_OPERATION;
		return sslMapCryptoBackendError(
			sslSetLowLevelCipherInfo(state, 33, 0, padFunc));
	}
	return status;
}

int sslGetPrngBackendLowLevelInfo(SslCryptoAlgorithm *algorithm, int selector,
				  void *result)
{
	SslPrngBackendStatePrefix *state = algorithm->backendState;
	int status = 0;

	switch (selector) {
	case 53:
		*(SslCryptoAlgorithm **)result = algorithm;
		break;
	case 54:
		*(int *)result = state->seedThreshold;
		break;
	case 56:
		*(SceUInt32 *)result = state->flags & 1;
		break;
	case 91:
		return sslMapCryptoBackendError(sslGetLowLevelPrngInfo(
			state->lowLevelPrng, 6, 0, result));
	case 100:
		*(void **)result = state->userCallback;
		break;
	case 11003:
		*(void **)result = state->lowLevelPrng;
		break;
	default:
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	}
	return status;
}

int sslCipherBackendSetInfo(SslCryptoAlgorithm *algorithm, int selector,
			    SceUIntPtr value)
{
	const SslCipherBackendData *data = algorithm->implementationData;
	void *state = algorithm->backendState;
	int status = 0;
	SceUInt32 len;
	int padFlag = 0;
	void *prngObj = NULL;

	if (algorithm->family == 2) {
		switch (selector) {
		case 53:
		case 73:
			status = sslGetPrngBackendLowLevelInfo(
				(SslCryptoAlgorithm *)value, 11003, &prngObj);
			if (status == 0) {
				if (prngObj == NULL)
					status = SSL_CRYPTO_ERROR_NO_OPERATION;
				else
					status = sslMapCryptoBackendError(
						sslSetLowLevelCipherInfo(
							state, 36, 0, prngObj));
			}
			break;
		case 116: {
			SceUInt32 mode = *(SceUInt32 *)value;

			status = sslMapCryptoBackendError(
				sslGetLowLevelCipherInfo(state, 34, &padFlag,
							 NULL));
			if (status == 0)
				status = sslSetCipherPaddingMode(
					algorithm, padFlag, mode);
			break;
		}
		case 106: {
			const SslDesKeyBuffer *buf =
				(const SslDesKeyBuffer *)value;
			const void *bufData = buf->data;

			if (bufData == NULL) {
				status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
			} else {
				len = buf->length;
				status = sslMapCryptoBackendError(
					sslProcessLowLevelCipher(
						state, NULL, &len, len,
						bufData, len, 0));
			}
			break;
		}
		case 105:
			status = sslAddPropertyValue(
				algorithm->properties, 1, 105, 0,
				((const SslDesKeyBuffer *)value)->data,
				((const SslDesKeyBuffer *)value)->length, 2u);
			if (status != 0)
				status = 10001;
			break;
		case 20:
			status = sslMapCryptoBackendError(
				sslSetLowLevelCipherInfo(
					state, 35, *(SceUInt32 *)value, NULL));
			break;
		case 107:
			if (*(SceUInt32 *)value != 0)
				algorithm->flags |= 0x100u;
			else
				algorithm->flags &= ~0x100u;
			break;
		case 36:
			status = sslMapCryptoBackendError(
				sslSetLowLevelCipherInfo(
					state, 32, *(SceUInt32 *)value, NULL));
			break;
		case 8:
		case 14:
			status = sslMapCryptoBackendError(
				sslSetLowLevelCipherInfo(
					state, 9, *(SceUInt32 *)value, NULL));
			break;
		case 1: {
			SceUInt32 keyLen = *(SceUInt32 *)value;

			status = sslMapCryptoBackendError(
				sslControlLowLevelCipher(state, 1, keyLen, 0,
							 0, 2));
			if (status == 0 && data->fixedKeySize != -1 &&
			    keyLen != (SceUInt32)data->fixedKeySize)
				status = 10023;
			break;
		}
		case 87: {
			int index = *(int *)value;
			SceUInt32 stateLen = 0;
			SslDirectMemoryContext *dmem =
				sslGetOrCreateDefaultDirectMemoryContext();
			int switchStatus;

			if (algorithm->backendState == NULL) {
				switchStatus = SSL_LIBRARY_ERROR_ALLOCATION;
			} else {
				const void *(**list)(void) = NULL;

				if (data->GetDescriptorList != NULL) {
					list = data->GetDescriptorList(0);
					while (list != NULL) {
						if (*list == NULL)
							break;
						if (index <= 0)
							break;
						list++;
						index--;
					}
				}
				if (list == NULL || *list == NULL) {
					switchStatus =
						SSL_CRYPTO_ERROR_NO_IMPLEMENTATION;
				} else {
					switchStatus = sslMapCryptoBackendError(
						sslSaveLowLevelCipherState(
							algorithm->backendState,
							NULL, &stateLen, 0));
					if (switchStatus == 0) {
						void *buf =
							sslAllocate(stateLen);

						if (buf == NULL) {
							switchStatus =
								SSL_LIBRARY_ERROR_ALLOCATION;
						} else {
							switchStatus =
								sslMapCryptoBackendError(
									sslSaveLowLevelCipherState(
										algorithm->backendState,
										buf,
										&stateLen,
										stateLen));
							if (switchStatus == 0) {
								switchStatus =
									sslMapCryptoBackendError(
										sslCreateLowLevelCipher(
											&algorithm->backendState,
											(const void *)*list,
											dmem));
								if (switchStatus ==
								    0) {
									switchStatus =
										sslMapCryptoBackendError(
											sslRestoreLowLevelCipherState(
												algorithm->backendState,
												buf,
												stateLen));
								}
							}
						}
						if (buf != NULL)
							sslFree(buf);
					}
				}
			}
			status = switchStatus;
			break;
		}
		default:
			status = SSL_CRYPTO_ERROR_NO_OPERATION;
			break;
		}
	} else {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
	}
	return status;
}

int sslCipherBackendInitialize(SslCryptoAlgorithm *algorithm)
{
	void *cipher = NULL;
	int blockSize;
	SceUInt32 v13;
	SceUInt32 v14;
	SslDirectMemoryContext *dmem = sslGetDefaultDirectMemoryContextInline();
	int status = SSL_CRYPTO_ERROR_NO_IMPLEMENTATION;
	const SslCipherBackendData *data = algorithm->implementationData;

	v13 = 0;
	v14 = 0;
	if (data->GetDescriptor == NULL)
		goto error;
	status = sslMapCryptoBackendError(
		sslCreateLowLevelCipher(&cipher, data->GetDescriptor(0), dmem));
	if (status != 0)
		goto error;
	algorithm->backendState = cipher;
	status = sslMapCryptoBackendError(
		sslGetLowLevelCipherInfo(cipher, 1, &blockSize, NULL));
	if (status != 0)
		goto error;
	if (blockSize > 1) {
		v13 = 1;
		v14 = 1;
	}
	status = sslCipherBackendSetInfo(algorithm, 20, (SceUIntPtr)&v14);
	if (status != 0 ||
	    (status = sslCipherBackendSetInfo(algorithm, 36,
					      (SceUIntPtr)&v13)) != 0) {
error:
		sslCipherBackendDestroy(algorithm);
	}
	return status;
}

int sslCipherBackendCopy(SslCryptoAlgorithm *source,
			 SslCryptoAlgorithm *destination)
{
	SceUInt32 stateLen;
	int status;
	void *buf;

	if (destination->backendState != NULL && source->backendState != NULL) {
		stateLen = 0;
		status = sslMapCryptoBackendError(sslSaveLowLevelCipherState(
			source->backendState, NULL, &stateLen, 0));
		if (status == 0) {
			buf = sslAllocate(stateLen);
			if (buf != NULL) {
				status = sslMapCryptoBackendError(
					sslSaveLowLevelCipherState(
						source->backendState, buf,
						&stateLen, stateLen));
				if (status == 0) {
					status = sslMapCryptoBackendError(
						sslRestoreLowLevelCipherState(
							destination->backendState,
							buf, stateLen));
				}
			} else {
				status = SSL_LIBRARY_ERROR_ALLOCATION;
			}
			if (buf != NULL)
				sslFree(buf);
		}
	} else {
		status = SSL_LIBRARY_ERROR_ALLOCATION;
	}
	return status;
}

int sslCipherBackendGetInfo(SslCryptoAlgorithm *algorithm, int selector,
			    void *result)
{
	const SslCipherBackendData *data = algorithm->implementationData;
	int status = 0;
	void *state = algorithm->backendState;
	int val = 0;

	if (algorithm->family == 2) {
		switch (selector) {
		case 105:
			status = sslMapCryptoBackendError(
				sslGetLowLevelCipherInfo(state, 19, &val,
							 ((void **)result)[1]));
			if (status != 0)
				status = SSL_LIBRARY_ERROR_ALLOCATION;
			*(int *)result = val;
			break;
		case 20:
			status = sslMapCryptoBackendError(
				sslGetLowLevelCipherInfo(state, 35, &val,
							 NULL));
			if (status == 0)
				*(int *)result = (val == 1);
			break;
		case 107:
			*(int *)result = (algorithm->flags & 0x100u) != 0;
			break;
		case 36:
			status = sslMapCryptoBackendError(
				sslGetLowLevelCipherInfo(state, 32, &val,
							 NULL));
			if (status == 0)
				*(int *)result = (val == 1);
			break;
		case 6:
			status = sslMapCryptoBackendError(
				sslGetLowLevelCipherInfo(state, 6, &val, NULL));
			*(int *)result = val;
			break;
		case 10:
			status = sslMapCryptoBackendError(
				sslGetLowLevelCipherInfo(state, 1, &val, NULL));
			*(int *)result = val;
			break;
		case 1:
		case 5:
			if (data->fixedKeySize != -1) {
				*(int *)result = data->fixedKeySize;
			} else {
				status = sslMapCryptoBackendError(
					sslGetLowLevelCipherInfo(state, 4, &val,
								 NULL));
				*(int *)result = val;
			}
			break;
		case 0:
		case 2:
		case 7:
		case 8:
		case 14:
			status = sslMapCryptoBackendError(
				sslGetLowLevelCipherInfo(state, 8, &val, NULL));
			*(int *)result = val;
			break;
		case 86:
			status = sslMapCryptoBackendError(
				sslGetLowLevelCipherInfo(state, 15, NULL,
							 result));
			break;
		case 11004:
			*(void **)result = state;
			break;
		default:
			status = SSL_CRYPTO_ERROR_NO_OPERATION;
			break;
		}
	} else {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
	}
	return status;
}

int sslGetActiveRandomAlgorithmAndEnableFlag(SslCryptoAlgorithm *algorithm,
					     SslCryptoAlgorithm **result)
{
	SslCryptoContext *context = algorithm->context;
	int one = 1;
	int status = SSL_CRYPTO_ERROR_UNSUPPORTED;
	SslCryptoAlgorithm *random = context->randomAlgorithm;

	if (random != NULL) {
		*result = random;
		status = sslSetAlgorithmInfo(random, 56, (SceUIntPtr)&one);
	}
	return status;
}

int sslCipherBackendInitKeyAndIv(SslCryptoAlgorithm *algorithm, void *key,
				 const SslDesKeyBuffer *iv, int encrypt)
{
	void *state = algorithm->backendState;
	SceUInt32 ivLen = 0;
	const void *ivData;
	SceUInt32 reqIvLen;
	SslDesKeyBuffer keyBuf;
	int modeVal;
	const SslCipherBackendData *data;
	int status;

	if (state == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	ivData = NULL;
	reqIvLen = 0;
	status = sslMapCryptoBackendError(
		sslGetLowLevelCipherInfo(state, 6, &reqIvLen, NULL));
	if (status != 0 || iv != NULL) {
		if (reqIvLen != 0 && iv->data != NULL && iv->length != 0) {
			ivLen = iv->length;
			ivData = iv->data;
		}
	} else {
		if (reqIvLen != 0)
			return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	}
	status = sslGetKeyInfo(key, 14, &keyBuf);
	if (status != 0)
		return status;
	if (reqIvLen != 0 && reqIvLen > ivLen)
		return 10023;
	switch (algorithm->algorithm) {
	case 29:
	case 30:
	case 31:
	case 32:
	case 33:
	case 43:
	case 44:
	case 45:
	case 60:
	case 61:
	case 62:
	case 63:
		if ((algorithm->flags & 0x100u) != 0 &&
		    sslCheckDesWeakKeyInline(&keyBuf) != 0)
			return 10022;
		break;
	default:
		break;
	}
	sslGetLowLevelCipherInfo(state, 8, &modeVal, NULL);
	data = algorithm->implementationData;
	if (data->fixedKeySize != -1 &&
	    (SceUInt32)data->fixedKeySize != keyBuf.length)
		return 10023;
	status = sslMapCryptoBackendError(sslInitLowLevelCipher(
		state, keyBuf.data, keyBuf.length, ivData, encrypt));
	if (status == 0) {
		status = sslMapCryptoBackendError(sslSetLowLevelCipherInfo(
			state, 9, (SceUInt32)modeVal, NULL));
	}
	return status;
}

int sslCipherBackendInit(SslCryptoAlgorithm *algorithm, void *key,
			 const SslDesKeyBuffer *iv, int encrypt)
{
	void *oldState = algorithm->backendState;
	int status;
	void *state;
	SceUInt32 padFlag;

	status = sslCipherBackendInitKeyAndIv(algorithm, key, iv, encrypt);
	state = algorithm->backendState;
	if (status == 0) {
		padFlag = (encrypt == 0);
		algorithm->flags |= 2u;
		if ((algorithm->flags & 0x200u) == 0) {
			status = sslMapCryptoBackendError(
				sslSetLowLevelCipherInfo(state, 34, padFlag,
							 NULL));
			if (status == 0) {
				return sslMapCryptoBackendError(
					sslSetLowLevelCipherInfo(state, 33, 0,
								 sslPkcs5Pad));
			}
		} else {
			return sslMapCryptoBackendError(
				sslSetLowLevelCipherInfo(oldState, 34, padFlag,
							 NULL));
		}
	}
	return status;
}

int sslCipherBackendOperate(SslCryptoAlgorithm *algorithm, const void *input,
			    SceUInt32 inputLength, void *output,
			    SceUInt32 *outputLength)
{
	return sslMapCryptoBackendError(sslProcessLowLevelCipher(
		algorithm->backendState, output, outputLength, *outputLength,
		input, inputLength, 2));
}

int sslCipherBackendFinal(SslCryptoAlgorithm *algorithm, void *output,
			  SceUInt32 *outputLength)
{
	return sslMapCryptoBackendError(sslProcessLowLevelCipher(
		algorithm->backendState, output, outputLength, *outputLength,
		NULL, 0, 2));
}

const SslCryptoImplementationMethods *sslGetCipherBackendMethods(void)
{
	return &sslCipherBackendMethods;
}
