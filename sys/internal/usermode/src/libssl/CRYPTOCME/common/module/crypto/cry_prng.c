/* Filename and private descriptive names are ours; CRYPTOCME PRNG backends. */
#include "ctx.h"
#include "../dmem/dmem.h"
#include "../../../../memory.h"
#include "../../../../runtime.h"

typedef struct SslPrngImplementationMethods {
	int (*Initialize)(SslCryptoAlgorithm *algorithm);
	int (*Destroy)(SslCryptoAlgorithm *algorithm);
	int (*Copy)(SslCryptoAlgorithm *source, SslCryptoAlgorithm *destination);
	int (*GetInfo)(SslCryptoAlgorithm *algorithm, int selector, void *result);
	int (*SetInfo)(SslCryptoAlgorithm *algorithm, int selector,
		       const int *value);
	void *reserved20;
	int (*Seed)(SslCryptoAlgorithm *algorithm, const void *buffer,
		    SceUInt32 length);
	int (*Generate)(SslCryptoAlgorithm *algorithm, SceUInt32 length,
			void *buffer, SceUInt32 *generatedLength);
} SslPrngImplementationMethods;

typedef struct SslPrngUserCallback {
	int (*read)(void *userCallback, void *buf, SceUInt32 maxLen);
	int (*init)(void);
} SslPrngUserCallback;

typedef struct SslPrngBackendState SslPrngBackendState;
struct SslPrngBackendState {
	void *lowLevelPrng;
	void *entropySource;
	SceUInt32 flags;
	int seedThreshold;
	int entropyStrength;
	SslPrngUserCallback *userCallback;
	int callbackArg0;
	int (*callbackFunc)(SslCryptoAlgorithm *algorithm, int arg1, int arg2,
			    SceUInt32 *ioParams);
	SslPrngBackendState *callbackSelf;
	int reserved24;
	unsigned char seedBuffer[64];
	int seedBuffered;
	int param58;
	int param57;
	void *lowLevelDigest;
};

extern int sslCreateLowLevelPrng(void **prng, const void *descriptor,
				 SslDirectMemoryContext *dmem);
extern int sslChainLowLevelPrng(void *prng, void *nextPrng);
extern int sslSetLowLevelPrngInfo(void *prng, int selector, int arg,
				  const void *value);
extern void sslFreeLowLevelPrng(void *prng);
extern void sslFreeLowLevelPrngChain(void *prng);
extern int sslControlLowLevelPrng(void *prng, int selector, int arg,
				  const void *value);
extern int sslSeedLowLevelPrng(void *prng, const void *buffer, SceUInt32 length);
extern int sslGenerateLowLevelPrng(void *prng, void *buffer,
				   SceUInt32 *generatedLength, SceUInt32 length);
extern void sslFreeLowLevelEntropySource(void *source);
extern const void *sslGetCallbackEntropyDescriptor(void);
extern const void *sslGetDefaultEntropyDescriptor(void);
extern void *sslGetSystemEntropySource(void);
extern int sslCreateLowLevelEntropySource(void **source, const void *descriptor,
					  SslDirectMemoryContext *dmem);
extern int sslSetLowLevelEntropySourceInfo(void *source, int selector, int arg,
					   const void *value);
extern int sslInitLowLevelEntropySourceMode1(void *source);
extern int sslInitLowLevelEntropySourceMode2(void *source, void *digestHandle);
extern int sslCollectLowLevelEntropy(void *source, int strength, void *buffer,
				     SceUInt32 length, int *outVal0,
				     int *outVal1);
extern const void *sslGetEntropyPrngDescriptor(void);
extern const void *sslGetSha1PrngDescriptor(void);
extern const void *sslGetFips186PrngDescriptor(void);

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

int sslDelegatingPrngBackendInitialize(SslCryptoAlgorithm *algorithm);
int sslDelegatingPrngBackendDestroy(SslCryptoAlgorithm *algorithm);
int sslDelegatingPrngBackendGetInfo(SslCryptoAlgorithm *algorithm, int selector,
				    void *result);
int sslDelegatingPrngBackendSetInfo(SslCryptoAlgorithm *algorithm, int selector,
				    const int *value);
int sslDelegatingPrngBackendSeed(SslCryptoAlgorithm *algorithm,
				 const void *buffer, SceUInt32 length);
int sslDelegatingPrngBackendGenerate(SslCryptoAlgorithm *algorithm,
				     SceUInt32 length, void *buffer,
				     SceUInt32 *generatedLength);

int sslBasePrngBackendInitialize(SslCryptoAlgorithm *algorithm);
int sslBasePrngBackendGetInfo(SslCryptoAlgorithm *algorithm, int selector,
			      void *result);
int sslBasePrngBackendSetInfo(SslCryptoAlgorithm *algorithm, int selector,
			      const int *value);

int sslFips186PrngBackendInitialize(SslCryptoAlgorithm *algorithm);
int sslFips186PrngBackendGetInfo(SslCryptoAlgorithm *algorithm, int selector,
				 void *result);
int sslFips186PrngBackendSetInfo(SslCryptoAlgorithm *algorithm, int selector,
				 const int *value);
int sslFips186PrngBackendSeed(SslCryptoAlgorithm *algorithm, const void *buffer,
			      SceUInt32 length);
int sslFips186PrngBackendGenerate(SslCryptoAlgorithm *algorithm,
				  SceUInt32 length, void *buffer,
				  SceUInt32 *generatedLength);

int sslPrngBackendDestroy(SslCryptoAlgorithm *algorithm);
int sslPrngBackendSeedLowLevel(SslCryptoAlgorithm *algorithm,
			       const void *buffer, SceUInt32 length);
int sslPrngBackendGenerateLowLevel(SslCryptoAlgorithm *algorithm,
				   SceUInt32 length, void *buffer,
				   SceUInt32 *generatedLength);
int sslPrngEntropyAdapterCallback(SslCryptoAlgorithm *algorithm, int arg1,
				  int arg2, SceUInt32 *ioParams);
int sslPrngLockCallback(void *ctx, int lockFlag);

static const SslPrngImplementationMethods sslDelegatingPrngBackendMethods = {
	sslDelegatingPrngBackendInitialize,
	sslDelegatingPrngBackendDestroy,
	NULL,
	sslDelegatingPrngBackendGetInfo,
	sslDelegatingPrngBackendSetInfo,
	NULL,
	sslDelegatingPrngBackendSeed,
	sslDelegatingPrngBackendGenerate
};

static const SslPrngImplementationMethods sslBasePrngBackendMethods = {
	sslBasePrngBackendInitialize,
	sslPrngBackendDestroy,
	NULL,
	sslBasePrngBackendGetInfo,
	sslBasePrngBackendSetInfo,
	NULL,
	sslPrngBackendSeedLowLevel,
	sslPrngBackendGenerateLowLevel
};

static const char fips186RandSource[] =
	"S:\\work\\CRYPTOCME\\common\\module/crypto/nat_meth/ck_rand_fips186.c";

static const SslPrngImplementationMethods sslFips186PrngBackendMethods = {
	sslFips186PrngBackendInitialize,
	sslPrngBackendDestroy,
	NULL,
	sslFips186PrngBackendGetInfo,
	sslFips186PrngBackendSetInfo,
	NULL,
	sslFips186PrngBackendSeed,
	sslFips186PrngBackendGenerate
};

static const char ckRandSource[] =
	"S:\\work\\CRYPTOCME\\common\\module/crypto/nat_meth/ck_rand.c";

const void *(*sslBasePrngDescriptorList[3])(void) = {
	sslGetEntropyPrngDescriptor,
	sslGetSha1PrngDescriptor,
	NULL
};

const void *(*sslFips186PrngDescriptorList[3])(void) = {
	sslGetFips186PrngDescriptor,
	sslGetSha1PrngDescriptor,
	NULL
};

static int sslPrngLock(int mode, int index, const char *file, int line)
{
	if (sslLockCallback != NULL)
		return sslLockCallback(mode, index, file, line);
	return 1;
}

static SslDirectMemoryContext *sslGetPrngDirectMemoryContext(void)
{
	if (sslDefaultDirectMemoryContext == NULL) {
		sslNewDirectMemoryContext(&sslDefaultDirectMemoryContext,
					  sslGetDefaultDirectMemoryMethod(),
					  NULL, NULL);
	}
	return sslDefaultDirectMemoryContext;
}

int sslDelegatingPrngBackendInitialize(SslCryptoAlgorithm *algorithm)
{
	SslCryptoAlgorithm *subAlg = NULL;
	int status;

	status = sslCreateAlgorithm(algorithm->context, 4, 4134, 0, &subAlg);
	if (status == 0)
		algorithm->backendState = subAlg;
	return status;
}

int sslDelegatingPrngBackendDestroy(SslCryptoAlgorithm *algorithm)
{
	return sslDestroyAlgorithm(algorithm->backendState);
}

int sslDelegatingPrngBackendGetInfo(SslCryptoAlgorithm *algorithm, int selector,
				    void *result)
{
	return sslGetAlgorithmInfo(algorithm->backendState, selector, result);
}

int sslDelegatingPrngBackendSetInfo(SslCryptoAlgorithm *algorithm, int selector,
				    const int *value)
{
	return sslSetAlgorithmInfo(algorithm->backendState, selector,
				   (SceUIntPtr)value);
}

int sslDelegatingPrngBackendSeed(SslCryptoAlgorithm *algorithm,
				 const void *buffer, SceUInt32 length)
{
	return sslSeedRandom(algorithm->backendState, buffer, length);
}

int sslDelegatingPrngBackendGenerate(SslCryptoAlgorithm *algorithm,
				     SceUInt32 length, void *buffer,
				     SceUInt32 *generatedLength)
{
	return sslGenerateRandom(algorithm->backendState, length, buffer,
				 generatedLength);
}

const SslCryptoImplementationMethods *sslGetPrngBackendMethods(void)
{
	return (const SslCryptoImplementationMethods *)
		&sslDelegatingPrngBackendMethods;
}

const void *(**sslGetBasePrngDescriptorList(void))(void)
{
	return sslBasePrngDescriptorList;
}

int sslCreateLowLevelPrngChain(SslPrngBackendState *state,
			       const void *(**descriptors)(void))
{
	void *prng = NULL;
	SslDirectMemoryContext *dmem = sslGetPrngDirectMemoryContext();
	const void *(*getDesc)(void);
	int status = 0;

	for (getDesc = *descriptors; getDesc != NULL;
	     descriptors++, getDesc = *descriptors) {
		status = sslMapCryptoBackendError(sslCreateLowLevelPrng(
			&prng, getDesc(), dmem));
		if (status != 0)
			goto cleanup;
		if (state->lowLevelPrng != NULL) {
			status = sslMapCryptoBackendError(
				sslChainLowLevelPrng(
					prng, state->lowLevelPrng));
			if (status != 0)
				goto cleanup;
		}
		state->lowLevelPrng = prng;
		prng = NULL;
	}
	if (state->entropySource != NULL) {
		sslSetLowLevelPrngInfo(state->lowLevelPrng, 7, 0,
				       state->entropySource);
	}
cleanup:
	if (prng != NULL)
		sslFreeLowLevelPrng(prng);
	return status;
}

int sslInitBasePrngBackendState(SslCryptoAlgorithm *algorithm,
				const void *(**descriptors)(void))
{
	SslPrngBackendState *state = algorithm->backendState;
	SslDirectMemoryContext *dmem = sslGetPrngDirectMemoryContext();
	void *lockParams[4];
	int status;

	sslFillMemory(state, 0, 0x28u);
	status = sslCreateLowLevelPrngChain(state, descriptors);
	if (status == 0) {
		lockParams[0] = dmem;
		lockParams[1] = (void *)sslPrngLockCallback;
		lockParams[2] = NULL;
		lockParams[3] = NULL;
		status = sslMapCryptoBackendError(sslControlLowLevelPrng(
			state->lowLevelPrng, 4240, 1, lockParams));
	}
	return status;
}

int sslBasePrngBackendInitialize(SslCryptoAlgorithm *algorithm)
{
	SslPrngBackendState *state = sslAllocate(0x28u);

	if (state != NULL) {
		sslFillMemory(state, 0, 0x28u);
		algorithm->backendState = state;
		return sslInitBasePrngBackendState(algorithm,
						   sslBasePrngDescriptorList);
	}
	return SSL_LIBRARY_ERROR_ALLOCATION;
}

int sslBasePrngBackendGetInfo(SslCryptoAlgorithm *algorithm, int selector,
			      void *result)
{
	if (selector == 100 || selector == 56 || selector == 54)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return sslGetPrngBackendLowLevelInfo(algorithm, selector, result);
}

int sslInvokePrngUserInitCallback(const SslPrngUserCallback *userCallback)
{
	int status = 0;

	if (userCallback == NULL || userCallback->init == NULL)
		status = 10021;
	else if (userCallback->init() == -1)
		status = 10001;
	return status;
}

int sslInvokePrngUserReadCallback(SslPrngUserCallback *userCallback, void *buf,
				  SceUInt32 *outLen, SceUInt32 maxLen)
{
	int status = 0;
	int ret;

	if (userCallback == NULL || userCallback->read == NULL) {
		status = 10021;
	} else {
		ret = userCallback->read(userCallback, buf, maxLen);
		*outLen = (SceUInt32)ret;
		if (ret == -1) {
			*outLen = 0;
			status = 10001;
		}
	}
	return status;
}

int sslPrngReturnOne(void)
{
	return 1;
}

int sslAttachPrngUserEntropyCallback(SslPrngBackendState *state,
				     SslPrngUserCallback *userCallback)
{
	SslDirectMemoryContext *dmem = sslDefaultDirectMemoryContext;
	int status;

	if (dmem == NULL) {
		sslNewDirectMemoryContext(&sslDefaultDirectMemoryContext,
					  sslGetDefaultDirectMemoryMethod(),
					  NULL, NULL);
		dmem = sslDefaultDirectMemoryContext;
	}
	if (userCallback == NULL)
		return SSL_CRYPTO_ERROR_INVALID_ARGUMENT;
	if (state->entropySource != NULL) {
		sslFreeLowLevelEntropySource(state->entropySource);
		state->entropySource = NULL;
	}
	status = sslMapCryptoBackendError(sslCreateLowLevelEntropySource(
		&state->entropySource, sslGetCallbackEntropyDescriptor(),
		dmem));
	if (status == 0) {
		status = sslMapCryptoBackendError(sslSetLowLevelPrngInfo(
			state->lowLevelPrng, 7, 0, state->entropySource));
		if (status == 0) {
			state->userCallback = userCallback;
			state->callbackFunc = sslPrngEntropyAdapterCallback;
			state->callbackSelf = state;
			return sslMapCryptoBackendError(
				sslSetLowLevelEntropySourceInfo(
					state->entropySource, 4097, 1,
					&state->callbackArg0));
		}
	}
	return status;
}

int sslSetCommonPrngBackendInfo(SslCryptoAlgorithm *algorithm, int selector,
				const int *value)
{
	SslPrngBackendState *state = algorithm->backendState;
	int status = 0;

	switch (selector) {
	case 54:
		state->seedThreshold = *value;
		break;
	case 56:
		if (*value == 0)
			state->flags &= ~1u;
		else
			state->flags |= 1u;
		break;
	case 100:
		status = sslAttachPrngUserEntropyCallback(
			state, (SslPrngUserCallback *)value);
		break;
	case 51:
		status = 1;
		break;
	default:
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		break;
	}
	return status;
}

int sslBasePrngBackendSetInfo(SslCryptoAlgorithm *algorithm, int selector,
			      const int *value)
{
	SslPrngBackendState *state = algorithm->backendState;
	void *cbParams[3];

	if (selector == 100 || selector == 56 || selector == 54)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	if (selector == 53) {
		cbParams[2] = (void *)sslInvokePrngUserInitCallback;
		cbParams[0] = (void *)value;
		cbParams[1] = (void *)sslInvokePrngUserReadCallback;
		return sslMapCryptoBackendError(sslControlLowLevelPrng(
			state->lowLevelPrng, 4136, 1, cbParams));
	}
	return sslSetCommonPrngBackendInfo(algorithm, selector, value);
}

const SslCryptoImplementationMethods *sslGetBasePrngBackendMethods(void)
{
	return (const SslCryptoImplementationMethods *)
		&sslBasePrngBackendMethods;
}

const void *(**sslGetFips186PrngDescriptorList(void))(void)
{
	return sslFips186PrngDescriptorList;
}

int sslGetDigestBackendLowLevelHandle(SslCryptoAlgorithm *algorithm,
				      void *result)
{
	return sslGetAlgorithmInfo(algorithm, 11002, result);
}

int sslBindPrngDigestAlgorithm(SslCryptoAlgorithm *algorithm, int digestAlg,
			       int selector, int param, void **outHandle)
{
	SslPrngBackendState *state = algorithm->backendState;
	SslCryptoContextHeader *context = algorithm->context;
	SslCryptoAlgorithm *digest = NULL;
	void *handle = NULL;
	void *prng = state->lowLevelPrng;
	int status;

	status = sslCreateAlgorithm(context, 3, digestAlg, 0, &digest);
	if (status == 0) {
		status = sslGetDigestBackendLowLevelHandle(digest, &handle);
		if (status == 0) {
			if (prng == NULL ||
			    (status = sslMapCryptoBackendError(
				     sslControlLowLevelPrng(prng, selector,
							    param, handle))) ==
				    0) {
				if (outHandle != NULL)
					*outHandle = handle;
			}
		}
	}
	if (digest != NULL)
		sslDestroyAlgorithm(digest);
	return status;
}

int sslInitFips186PrngBackendState(SslCryptoAlgorithm *algorithm,
				   const void *(**descriptors)(void))
{
	SslPrngBackendState *state = algorithm->backendState;
	int status;

	status = sslInitBasePrngBackendState(algorithm, descriptors);
	if (status == 0) {
		status = sslBindPrngDigestAlgorithm(algorithm, 64, 4096, 0,
						    &state->lowLevelDigest);
		if (status == 0) {
			state->seedThreshold = 40;
			state->flags |= 1u;
			state->param57 = 1;
			state->seedBuffered = 0;
			state->param58 = 1;
		}
	}
	return status;
}

int sslFips186PrngBackendInitialize(SslCryptoAlgorithm *algorithm)
{
	SslPrngBackendState *state = sslAllocate(0x78u);
	int status = SSL_LIBRARY_ERROR_ALLOCATION;

	if (state != NULL) {
		sslFillMemory(state, 0, 0x78u);
		algorithm->backendState = state;
		return sslInitFips186PrngBackendState(
			algorithm, sslFips186PrngDescriptorList);
	}
	return status;
}

int sslFips186PrngBackendGetInfo(SslCryptoAlgorithm *algorithm, int selector,
				 void *result)
{
	SslPrngBackendState *state = algorithm->backendState;

	switch (selector) {
	case 57:
		*(int *)result = state->param57;
		return 0;
	case 58:
		*(int *)result = state->param58;
		return 0;
	default:
		return sslGetPrngBackendLowLevelInfo(algorithm, selector,
						     result);
	}
}

int sslFeedFips186PrngEntropy(SslCryptoAlgorithm *algorithm, const void *buffer,
			      SceUInt32 length)
{
	SslPrngBackendState *state = algorithm->backendState;
	int status = 0;
	int buffered = state->seedBuffered;
	int threshold = state->seedThreshold;
	SceUInt32 copied = 0;
	void *prng = state->lowLevelPrng;
	SslLowLevelBuffer seedBuf;

	if (buffered < threshold) {
		copied = (SceUInt32)(threshold - buffered);
		if (length < copied)
			copied = length;
		sslCopyMemory(state->seedBuffer + buffered, buffer, copied);
		state->seedBuffered += (int)copied;
		seedBuf.data = state->seedBuffer;
		seedBuf.length = (SceUInt32)state->seedBuffered;
		status = sslMapCryptoBackendError(
			sslControlLowLevelPrng(prng, 4096, 1, &seedBuf));
		if (status != 0)
			return status;
	}
	if (length > copied) {
		status = sslMapCryptoBackendError(sslSeedLowLevelPrng(
			prng, (const unsigned char *)buffer + copied,
			length - copied));
	}
	return status;
}

int sslRecreatePrngEntropySource(SslPrngBackendState *state,
				 const void *descriptor)
{
	SslDirectMemoryContext *dmem = sslGetPrngDirectMemoryContext();

	if (state->entropySource != NULL) {
		sslFreeLowLevelEntropySource(state->entropySource);
		state->entropySource = NULL;
	}
	return sslMapCryptoBackendError(sslCreateLowLevelEntropySource(
		&state->entropySource, descriptor, dmem));
}

int sslInitFips186PrngEntropySource(SslPrngBackendState *state,
				    void *digestHandle, int mode)
{
	int status;

	if (mode != 1 && mode != 2)
		return SSL_CRYPTO_ERROR_INVALID_ARGUMENT;
	status = sslRecreatePrngEntropySource(
		state, sslGetDefaultEntropyDescriptor());
	if (status == 0) {
		status = sslMapCryptoBackendError(
			sslSetLowLevelEntropySourceInfo(
				state->entropySource, 4, 1,
				sslGetSystemEntropySource()));
		if (status == 0) {
			status = sslMapCryptoBackendError(
				sslSetLowLevelEntropySourceInfo(
					state->entropySource, 4, 2,
					digestHandle));
			if (status == 0) {
				if (mode == 1) {
					state->entropyStrength = 128;
					return sslMapCryptoBackendError(
						sslInitLowLevelEntropySourceMode1(
							state->entropySource));
				} else {
					state->entropyStrength = 256;
					return sslMapCryptoBackendError(
						sslInitLowLevelEntropySourceMode2(
							state->entropySource,
							digestHandle));
				}
			}
		}
	}
	return status;
}

int sslCollectFips186PrngEntropy(SslCryptoAlgorithm *algorithm, int mode,
				 SceUInt32 length)
{
	SslPrngBackendState *state = algorithm->backendState;
	int status = 0;
	int outVal0 = 0;
	int outVal1 = 0;
	unsigned char buf[64];

	sslPrngLock(9, 33, fips186RandSource, 461);
	if ((state->flags & 2u) == 0) {
		if (state->entropySource == NULL) {
			status = sslInitFips186PrngEntropySource(
				state, state->lowLevelDigest, mode);
			if (status != 0)
				goto done;
			if (state->lowLevelPrng != NULL) {
				sslSetLowLevelPrngInfo(state->lowLevelPrng, 7,
						       0,
						       state->entropySource);
			}
		}
		if (length <= 64u) {
			if (length == 0) {
				if (state->seedBuffered < state->seedThreshold)
					length = (SceUInt32)(
						state->seedThreshold -
						state->seedBuffered);
			}
			if (length != 0) {
				status = sslMapCryptoBackendError(
					sslCollectLowLevelEntropy(
						state->entropySource,
						state->entropyStrength, buf,
						length, &outVal0, &outVal1));
				if (status == 0) {
					status = sslFeedFips186PrngEntropy(
						algorithm, buf, length);
				}
			}
		} else {
			status = SSL_CRYPTO_ERROR_INVALID_ARGUMENT;
		}
	}
done:
	sslPrngLock(10, 33, fips186RandSource, 514);
	return status;
}

int sslSetFips186PrngSeedThreshold(SslCryptoAlgorithm *algorithm, int threshold)
{
	SslPrngBackendState *state = algorithm->backendState;
	int val;
	int status;

	if (threshold >= 20 && threshold <= 64) {
		val = threshold;
		status = sslMapCryptoBackendError(sslControlLowLevelPrng(
			state->lowLevelPrng, 5, threshold, NULL));
		if (status == 0) {
			state->seedBuffered = 0;
			status = sslSetCommonPrngBackendInfo(algorithm, 54,
							     &val);
		}
	} else {
		status = SSL_CRYPTO_ERROR_INVALID_ARGUMENT;
	}
	return status;
}

int sslFips186PrngBackendSetInfo(SslCryptoAlgorithm *algorithm, int selector,
				 const int *value)
{
	SslPrngBackendState *state = algorithm->backendState;
	void *prng = state->lowLevelPrng;
	int status = 0;

	switch (selector) {
	case 54:
		status = sslSetFips186PrngSeedThreshold(algorithm, *value);
		break;
	case 56:
		status = sslSetCommonPrngBackendInfo(algorithm, selector,
						     value);
		if (status == 0 && (state->flags & 1u) != 0)
			status = sslCollectFips186PrngEntropy(algorithm, 1, 0);
		break;
	case 57: {
		int v = *value;

		if (v == 0 || v == 1 || v == 2) {
			state->param57 = v;
			status = sslMapCryptoBackendError(
				sslControlLowLevelPrng(prng, 2, v, NULL));
		} else {
			status = SSL_CRYPTO_ERROR_INVALID_ARGUMENT;
		}
		break;
	}
	case 58: {
		int v = *value;

		if (v == 0 || v == 1 || v == 2) {
			state->param58 = v;
			status = sslMapCryptoBackendError(
				sslControlLowLevelPrng(prng, 1, v, NULL));
		} else {
			status = SSL_CRYPTO_ERROR_INVALID_ARGUMENT;
		}
		break;
	}
	case 59:
		state->seedBuffered = 0;
		break;
	case 50: {
		int mode = *value;

		if (mode == 1 || mode == 2)
			status = sslCollectFips186PrngEntropy(algorithm, mode,
							      40);
		else
			status = SSL_CRYPTO_ERROR_INVALID_ARGUMENT;
		break;
	}
	case 10002:
		if (*value == 0)
			state->flags &= ~2u;
		else
			state->flags |= 2u;
		break;
	default:
		status = sslSetCommonPrngBackendInfo(algorithm, selector,
						     value);
		break;
	}
	return status;
}

int sslFips186PrngBackendSeed(SslCryptoAlgorithm *algorithm, const void *buffer,
			      SceUInt32 length)
{
	int status;

	sslPrngLock(9, 33, fips186RandSource, 309);
	status = sslFeedFips186PrngEntropy(algorithm, buffer, length);
	sslPrngLock(10, 33, fips186RandSource, 311);
	return status;
}

int sslFips186PrngBackendGenerate(SslCryptoAlgorithm *algorithm,
				  SceUInt32 length, void *buffer,
				  SceUInt32 *generatedLength)
{
	SslPrngBackendState *state = algorithm->backendState;
	void *prng = state->lowLevelPrng;
	int status;

	if ((state->flags & 1u) == 0 ||
	    (status = sslCollectFips186PrngEntropy(algorithm, 1, 0)) == 0) {
		if (state->seedBuffered >= state->seedThreshold) {
			return sslMapCryptoBackendError(sslGenerateLowLevelPrng(
				prng, buffer, generatedLength, length));
		}
		return 10001;
	}
	return status;
}

const SslCryptoImplementationMethods *sslGetFips186PrngBackendMethods(void)
{
	return (const SslCryptoImplementationMethods *)
		&sslFips186PrngBackendMethods;
}

int sslCleanupPrngBackendHandles(SslPrngBackendState *state)
{
	if (state != NULL) {
		if (state->lowLevelPrng != NULL) {
			sslFreeLowLevelPrngChain(state->lowLevelPrng);
			state->lowLevelPrng = NULL;
		}
		if (state->entropySource != NULL) {
			sslFreeLowLevelEntropySource(state->entropySource);
			state->entropySource = NULL;
		}
	}
	return 0;
}

int sslPrngBackendDestroy(SslCryptoAlgorithm *algorithm)
{
	SslPrngBackendState *state = algorithm->backendState;

	if (state != NULL) {
		sslCleanupPrngBackendHandles(state);
		sslFree(state);
		algorithm->backendState = NULL;
	}
	return 0;
}

int sslPrngBackendSeedLowLevel(SslCryptoAlgorithm *algorithm,
			       const void *buffer, SceUInt32 length)
{
	SslPrngBackendState *state = algorithm->backendState;

	return sslMapCryptoBackendError(
		sslSeedLowLevelPrng(state->lowLevelPrng, buffer, length));
}

int sslPrngBackendGenerateLowLevel(SslCryptoAlgorithm *algorithm,
				   SceUInt32 length, void *buffer,
				   SceUInt32 *generatedLength)
{
	SslPrngBackendState *state = algorithm->backendState;

	return sslMapCryptoBackendError(sslGenerateLowLevelPrng(
		state->lowLevelPrng, buffer, generatedLength, length));
}

int sslPrngEntropyAdapterCallback(SslCryptoAlgorithm *algorithm, int arg1,
				  int arg2, SceUInt32 *ioParams)
{
	int status = 0;
	SceUInt32 outVal0 = 0;
	SceUInt32 outVal1 = 0;
	void *entCtx = (void *)(SceUIntPtr)algorithm->family;
	int (**cbTable)(void *, SceUInt32, SceUInt32, SceUInt32, SceUInt32 *,
			SceUInt32 *) = *(void **)((char *)entCtx + 20);
	int ret;

	(void)arg1;
	(void)arg2;
	ret = (*cbTable)(((void **)cbTable)[1], ioParams[0], ioParams[1],
			 ioParams[1], &outVal0, &outVal1);
	if (ret != 0)
		status = 10001;
	ioParams[1] = outVal0;
	ioParams[2] = outVal1;
	return status;
}

int sslPrngLockCallback(void *ctx, int lockFlag)
{
	(void)ctx;
	if (lockFlag == 1)
		sslPrngLock(9, 16, ckRandSource, 908);
	else if (lockFlag == 0)
		sslPrngLock(10, 16, ckRandSource, 910);
	return 0;
}
