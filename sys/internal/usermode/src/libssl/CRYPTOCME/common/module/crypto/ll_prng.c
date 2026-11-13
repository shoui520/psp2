/* Filename and descriptive names are ours: CRYPTOCME's random number
 * generators, each a descriptor and a state, chained.
 */
#include "ll_rand.h"
#include "../../../../memory.h"

/* The state of each generator */
#define SSL_ENTROPY_PRNG_STATE_SIZE sizeof(SslEntropyPrngState)
#define SSL_FIPS186_PRNG_STATE_SIZE sizeof(SslFips186State)
#define SSL_LOCKED_PRNG_STATE_SIZE sizeof(SslPrngLock)

static const SslPrngDescriptor sslEntropyPrngDescriptor = {
	NULL,
	NULL,
	sslGenerateEntropyPrng,
	sslSeedEntropyPrng,
	sslSetEntropyPrngInfo,
	sslGetEntropyPrngInfo,
	sslControlEntropyPrng,
	0,
	SSL_ENTROPY_PRNG_STATE_SIZE
};

static const SslPrngDescriptor sslFips186PrngDescriptor = {
	sslInitFips186Prng,
	sslFreeFips186Prng,
	sslGenerateFips186Prng,
	sslSeedFips186Prng,
	NULL,
	sslGetFips186PrngInfo,
	sslControlFips186Prng,
	0,
	SSL_FIPS186_PRNG_STATE_SIZE
};

static const SslPrngDescriptor sslLockedPrngDescriptor = {
	sslInitLockedPrng,
	NULL,
	sslGenerateLockedPrng,
	sslSeedLockedPrng,
	sslSetLockedPrngInfo,
	sslGetLockedPrngInfo,
	sslControlLockedPrng,
	0,
	SSL_LOCKED_PRNG_STATE_SIZE
};

const SslPrngDescriptor *sslGetEntropyPrngDescriptor(void)
{
	return &sslEntropyPrngDescriptor;
}

int sslGenerateLowLevelPrng(SslPrng *prng, unsigned char *output,
			    SceUInt32 *length, SceUInt32 max)
{
	if (prng == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (prng->descriptor != NULL)
		return prng->descriptor->Generate(prng, output, length, max);
	return SSL_CRYPTO_ERROR_NO_METHODS;
}

const SslPrngDescriptor *sslGetFips186PrngDescriptor(void)
{
	return &sslFips186PrngDescriptor;
}

int sslSeedLowLevelPrng(SslPrng *prng, const unsigned char *seed,
			SceUInt32 length)
{
	if (prng == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (prng->descriptor != NULL)
		return prng->descriptor->Seed(prng, seed, length);
	return SSL_CRYPTO_ERROR_NO_METHODS;
}

int sslSetLowLevelPrngInfo(SslPrng *prng, int id, int arg, const void *value)
{
	if (prng == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (prng->descriptor == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (prng->descriptor->SetInfo != NULL)
		return prng->descriptor->SetInfo(prng, id, arg, value);
	return 0;
}

int sslGetLowLevelPrngInfo(SslPrng *prng, int id, int arg, void *value)
{
	if (prng == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (prng->descriptor == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (prng->descriptor->GetInfo != NULL)
		return prng->descriptor->GetInfo(prng, id, arg, value);
	return 0;
}

int sslControlLowLevelPrng(SslPrng *prng, int id, int arg, const void *value)
{
	if (prng == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (prng->descriptor == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (prng->descriptor->Control != NULL)
		return prng->descriptor->Control(prng, id, arg, value);
	return 0;
}

const SslPrngDescriptor *sslGetSha1PrngDescriptor(void)
{
	return &sslLockedPrngDescriptor;
}

__attribute__((noinline))
int sslFreeLowLevelPrng(SslPrng *prng)
{
	SslDirectMemoryContext *dmem;

	if (prng != NULL) {
		dmem = prng->dmem;
		if (prng->descriptor != NULL && prng->state != NULL) {
			if (prng->descriptor->Free != NULL)
				prng->descriptor->Free(prng);
			if (prng->state != NULL)
				dmem->methods->Free(prng->state, dmem);
		}
		if (prng->flags & SSL_RAND_OBJECT_ALLOCATED)
			dmem->methods->Free(prng, dmem);
	}
	return 0;
}

void sslFreeLowLevelPrngChain(SslPrng *prng)
{
	SslPrng *next;

	while (prng != NULL) {
		next = prng->next;
		prng->next = NULL;
		sslFreeLowLevelPrng(prng);
		prng = next;
	}
}

int sslLayOutLowLevelPrng(SslPrng **result,
			  const SslPrngDescriptor *descriptor,
			  SslPrng *memory, int *size,
			  SslDirectMemoryContext *dmem)
{
	const SslPrngDescriptor *used;
	SceUInt32 stateSize;
	int status = 0;

	if (memory != NULL) {
		if (*size < (int)sizeof(SslPrng)) {
			status = SSL_CRYPTO_ERROR_NOT_INITIALIZED;
		} else {
			sslFillMemory(memory, 0, sizeof(*memory));
			used = memory->descriptor;
			if (dmem != NULL)
				memory->dmem = dmem;
			if (descriptor != NULL) {
				memory->descriptor = descriptor;
				used = descriptor;
			}
			stateSize = used->stateSize;
			if (stateSize != 0) {
				status = dmem->methods->Allocate(
					&memory->state, stateSize, dmem,
					SSL_DMEM_ALLOCATE_ZERO);
				used = memory->descriptor;
			}
			if (status == 0 && used->Init != NULL)
				status = used->Init(memory);
			if (status != 0)
				sslFreeLowLevelPrng(memory);
			else
				*result = memory;
		}
	}
	*size = sizeof(SslPrng);
	return status;
}

int sslCreateLowLevelPrng(SslPrng **result,
			  const SslPrngDescriptor *descriptor,
			  SslDirectMemoryContext *dmem)
{
	int size;
	SslPrng *memory;
	int status;

	if (dmem == NULL) {
		if (*result != NULL)
			dmem = (*result)->dmem;
		if (dmem == NULL)
			return SSL_CRYPTO_ERROR_NO_MEMORY;
	}
	sslLayOutLowLevelPrng(result, descriptor, NULL, &size, dmem);
	status = dmem->methods->Allocate((void **)&memory, size, dmem,
					 SSL_DMEM_ALLOCATE_ZERO);
	if (status == 0) {
		if (*result != NULL)
			sslFreeLowLevelPrng(*result);
		*result = NULL;
		status = sslLayOutLowLevelPrng(result, descriptor, memory,
					       &size, dmem);
		if (status != 0 && memory != NULL)
			dmem->methods->Free(memory, dmem);
		if (status == 0 && *result != NULL)
			(*result)->flags |= SSL_RAND_OBJECT_ALLOCATED;
	}
	return status;
}

int sslChainLowLevelPrng(SslPrng *prng, SslPrng *next)
{
	prng->next = next;
	return 0;
}
