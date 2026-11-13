/* Filename and descriptive names are ours: CRYPTOCME's low-level random
 * number generation: the Hash DF derivation, entropy sources chained under
 * a whitening source, and the generators.
 */
#ifndef LIBSSL_CRYPTOCME_LL_RAND_H
#define LIBSSL_CRYPTOCME_LL_RAND_H

#include <scetypes.h>
#include "ll_obj.h"
#include "../../object.h"
#include "../dmem/dmem.h"

/* The objects below all set this flag when they allocated themselves */
#define SSL_RAND_OBJECT_ALLOCATED 0x1

/* A derivation function over an input (Hash DF) */
typedef struct SslHashDf SslHashDf;

/* One output of a derivation: its buffer and its length in bits */
typedef struct SslHashDfOutput {
	unsigned char *data;
	SceUInt32 bits;
} SslHashDfOutput;

typedef struct SslHashDfDescriptor {
	const char *name;
	SceUInt32 stateSize;
	int (*Init)(SslHashDf *df, int arg);
	int (*Generate)(SslHashDf *df, SslHashDfOutput *outputs, int count);
	int (*SetInfo)(SslHashDf *df, int id, int arg, const void *value);
	int (*Free)(SslHashDf *df);
	SceUInt32 reserved[2];
} SslHashDfDescriptor;

typedef struct SslHashDfInput {
	unsigned char *data;
	SceUInt32 length;
} SslHashDfInput;

struct SslHashDf {
	SslDirectMemoryContext *dmem;
	const SslHashDfDescriptor *descriptor;
	SceUInt32 size;
	void *state;
	SslHashDfInput input[2];
	SceUInt32 flags;
};

_Static_assert(sizeof(SslHashDf) == 36, "SslHashDf");

/* SslHashDfDescriptor.SetInfo: the digest it uses */
#define SSL_HASH_DF_INFO_DIGEST 1

/* The Hash DF's state */
typedef struct SslHashDfState {
	const SslDigestDescriptor *digest;
	SslDigestContext *context;
	int digestLength;
	unsigned char buffer[64];
} SslHashDfState;

_Static_assert(sizeof(SslHashDfState) == 76, "SslHashDfState");

const SslHashDfDescriptor *sslGetSystemEntropySource(void);
int sslLayOutHashDf(SslHashDf **result, const SslHashDfDescriptor *descriptor,
		    SslHashDf *memory, int *size, SslDirectMemoryContext *dmem);
int sslFreeHashDf(SslHashDf *df);
int sslCreateHashDf(SslHashDf **result, const SslHashDfDescriptor *descriptor,
		    SslDirectMemoryContext *dmem);
int sslNewHashDf(SslHashDf **result, const SslHashDfDescriptor *descriptor,
		 SslDirectMemoryContext *dmem);
int sslSetHashDfInputs(SslHashDf *df, const void *input, SceUInt32 length,
		       const void *extra, SceUInt32 extraLength, int arg);
int sslGenerateHashDf(SslHashDf *df, SslHashDfOutput *outputs, int count);
int sslSetHashDfInfo(SslHashDf *df, int id, int arg, const void *value);

static __inline int sslGenerateHashDfInline(SslHashDf *df,
					    SslHashDfOutput *outputs,
					    int count)
{
	if (df == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (df->descriptor != NULL)
		return df->descriptor->Generate(df, outputs, count);
	return SSL_CRYPTO_ERROR_NO_METHODS;
}

static __inline int sslSetHashDfInfoInline(SslHashDf *df, int id, int arg,
					   const void *value)
{
	if (df != NULL && df->descriptor != NULL)
		return df->descriptor->SetInfo(df, id, arg, value);
	return SSL_CRYPTO_ERROR_NO_METHODS;
}

/* An entropy source, in a chain under the whitening source */
typedef struct SslEntropySource SslEntropySource;

typedef struct SslEntropyDescriptor {
	int (*Init)(SslEntropySource *source);
	int (*Free)(SslEntropySource *source);
	int (*Collect)(SslEntropySource *source, SceUInt32 bits,
		       unsigned char *output, SceUInt32 max, int *length,
		       SceUInt32 *collected);
	SceUInt32 reserved;
	int (*GetInfo)(SslEntropySource *source, int id, int arg,
		       void *value);
	int (*SetInfo)(SslEntropySource *source, int id, int arg,
		       const void *value);
	SceUInt32 stateSize;
} SslEntropyDescriptor;

struct SslEntropySource {
	const SslEntropyDescriptor *descriptor;
	SslEntropySource *next;
	SslDirectMemoryContext *dmem;
	void *state;
	SceUInt32 flags;
};

_Static_assert(sizeof(SslEntropySource) == 20, "SslEntropySource");

/* GetInfo ids: the name of a source */
#define SSL_ENTROPY_INFO_NAME 2

/* The ids each source answers itself */
#define SSL_ENTROPY_INFO_WHITEN 4
#define SSL_ENTROPY_INFO_TIME 5
#define SSL_ENTROPY_INFO_CYCLE_COUNTER 6
#define SSL_ENTROPY_INFO_URANDOM 7
#define SSL_ENTROPY_INFO_MEMORY_STATISTICS 9
#define SSL_ENTROPY_INFO_PROCESS_TIMES 11
#define SSL_ENTROPY_INFO_USER_INTERFACE 13
#define SSL_ENTROPY_INFO_FILE 14
#define SSL_ENTROPY_INFO_REGISTRY 17
#define SSL_ENTROPY_INFO_USER_CALLBACK 4097

/* SSL_ENTROPY_INFO_WHITEN: create the Hash DF, set its digest */
#define SSL_ENTROPY_WHITEN_HASH_DF 1
#define SSL_ENTROPY_WHITEN_DIGEST 2
/* SSL_ENTROPY_INFO_FILE: the path of the seed file */
#define SSL_ENTROPY_FILE_PATH 2

/* The whitening source's buffers */
#define SSL_ENTROPY_POOL_SIZE 2048

typedef struct SslWhitenState {
	SslHashDf *hashDf;
	unsigned char pool[SSL_ENTROPY_POOL_SIZE];
	unsigned char previous[SSL_ENTROPY_POOL_SIZE];
	SceUInt32 previousLength;
} SslWhitenState;

_Static_assert(sizeof(SslWhitenState) == 0x1008, "SslWhitenState");

/* The user callback source: a function the application supplies, asked
 * for the data of a request
 */
typedef struct SslEntropyRequest {
	unsigned char *output;
	int length;
	SceUInt32 bits;
} SslEntropyRequest;

typedef struct SslEntropyCallback {
	void *context;
	int (*Read)(struct SslEntropyCallback *callback, int command, int arg,
		    SslEntropyRequest *request);
	SceUInt32 reserved[2];
} SslEntropyCallback;

/* The entropy file source's state */
typedef struct SslFileEntropyState {
	unsigned char reserved[68];
	char *path;
} SslFileEntropyState;

_Static_assert(sizeof(SslFileEntropyState) == 0x48, "SslFileEntropyState");

static __inline int sslCollectLowLevelEntropyInline(
	SslEntropySource *source, SceUInt32 bits, unsigned char *output,
	SceUInt32 max, int *length, SceUInt32 *collected)
{
	if (source == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (source->descriptor != NULL)
		return source->descriptor->Collect(source, bits, output, max,
						   length, collected);
	return SSL_CRYPTO_ERROR_NO_METHODS;
}

int sslCollectLowLevelEntropy(SslEntropySource *source, SceUInt32 bits,
			      unsigned char *output, SceUInt32 max, int *length,
			      SceUInt32 *collected);
const SslEntropyDescriptor *sslGetDefaultEntropyDescriptor(void);
void sslFreeLowLevelEntropySource(SslEntropySource *source);
int sslLayOutLowLevelEntropySource(SslEntropySource **result,
				   const SslEntropyDescriptor *descriptor,
				   SslEntropySource *memory, int *size,
				   SslDirectMemoryContext *dmem);
int sslCreateLowLevelEntropySource(SslEntropySource **result,
				   const SslEntropyDescriptor *descriptor,
				   SslDirectMemoryContext *dmem);
int sslAddLowLevelEntropySource(SslEntropySource *source,
				SslEntropySource *added);
int sslSetLowLevelEntropySourceInfo(SslEntropySource *source, int id,
				    int arg, const void *value);
const SslEntropyDescriptor *sslGetCycleCounterEntropyDescriptor(void);
const SslEntropyDescriptor *sslGetUrandomEntropyDescriptor(void);
const SslEntropyDescriptor *sslGetTimeEntropyDescriptor(void);
const SslEntropyDescriptor *sslGetFileEntropyDescriptor(void);
int sslInitLowLevelEntropySourceMode1(SslEntropySource *source);
const SslEntropyDescriptor *sslGetRegistryEntropyDescriptor(void);
const SslEntropyDescriptor *sslGetProcessTimesEntropyDescriptor(void);
const SslEntropyDescriptor *sslGetMemoryStatisticsEntropyDescriptor(void);
const SslEntropyDescriptor *sslGetUserInterfaceEntropyDescriptor(void);
int sslInitLowLevelEntropySourceMode2(SslEntropySource *source,
				      const void *digest);
const SslEntropyDescriptor *sslGetCallbackEntropyDescriptor(void);

/* The sources' functions */
int sslFreeHashDfState(SslHashDf *df);
int sslInitHashDfState(SslHashDf *df, int arg);
int sslGenerateHashDfState(SslHashDf *df, SslHashDfOutput *outputs,
			   int count);
int sslSetHashDfStateInfo(SslHashDf *df, int id, int arg, const void *value);
int sslInitWhitenEntropy(SslEntropySource *source);
int sslFreeWhitenEntropy(SslEntropySource *source);
int sslCollectWhitenEntropy(SslEntropySource *source, SceUInt32 bits,
			    unsigned char *output, SceUInt32 max, int *length,
			    SceUInt32 *collected);
int sslGetWhitenEntropyInfo(SslEntropySource *source, int id, int arg,
			    void *value);
int sslSetWhitenEntropyInfo(SslEntropySource *source, int id, int arg,
			    const void *value);
int sslInitCallbackEntropy(SslEntropySource *source);
int sslFreeCallbackEntropy(SslEntropySource *source);
int sslCollectCallbackEntropy(SslEntropySource *source, SceUInt32 bits,
			      unsigned char *output, SceUInt32 max, int *length,
			      SceUInt32 *collected);
int sslGetCallbackEntropyInfo(SslEntropySource *source, int id, int arg,
			      void *value);
int sslSetCallbackEntropyInfo(SslEntropySource *source, int id, int arg,
			      const void *value);
int sslInitCycleCounterEntropy(SslEntropySource *source);
int sslFreeCycleCounterEntropy(SslEntropySource *source);
int sslCollectCycleCounterEntropy(SslEntropySource *source, SceUInt32 bits,
				  unsigned char *output, SceUInt32 max, int *length,
				  SceUInt32 *collected);
int sslSetCycleCounterEntropyInfo(SslEntropySource *source, int id, int arg,
				  const void *value);
int sslGetCycleCounterEntropyInfo(SslEntropySource *source, int id, int arg,
				  void *value);
int sslFreeFileEntropy(SslEntropySource *source);
int sslCollectFileEntropy(SslEntropySource *source, SceUInt32 bits,
			  unsigned char *output, SceUInt32 max, int *length,
			  SceUInt32 *collected);
int sslGetFileEntropyInfo(SslEntropySource *source, int id, int arg,
			  void *value);
int sslSetFileEntropyInfo(SslEntropySource *source, int id, int arg,
			  const void *value);
int sslInitMemoryStatisticsEntropy(SslEntropySource *source);
int sslFreeMemoryStatisticsEntropy(SslEntropySource *source);
int sslCollectMemoryStatisticsEntropy(SslEntropySource *source,
				      SceUInt32 bits, unsigned char *output,
				      SceUInt32 max, int *length,
				      SceUInt32 *collected);
int sslGetMemoryStatisticsEntropyInfo(SslEntropySource *source, int id,
				      int arg, void *value);
int sslSetMemoryStatisticsEntropyInfo(SslEntropySource *source, int id,
				      int arg, const void *value);
int sslInitRegistryEntropy(SslEntropySource *source);
int sslFreeRegistryEntropy(SslEntropySource *source);
int sslCollectRegistryEntropy(SslEntropySource *source, SceUInt32 bits,
			      unsigned char *output, SceUInt32 max, int *length,
			      SceUInt32 *collected);
int sslGetRegistryEntropyInfo(SslEntropySource *source, int id, int arg,
			      void *value);
int sslSetRegistryEntropyInfo(SslEntropySource *source, int id, int arg,
			      const void *value);
int sslInitTimeEntropy(SslEntropySource *source);
int sslFreeTimeEntropy(SslEntropySource *source);
int sslCollectTimeEntropy(SslEntropySource *source, SceUInt32 bits,
			  unsigned char *output, SceUInt32 max, int *length,
			  SceUInt32 *collected);
int sslGetTimeEntropyInfo(SslEntropySource *source, int id, int arg,
			  void *value);
int sslSetTimeEntropyInfo(SslEntropySource *source, int id, int arg,
			  const void *value);
int sslInitProcessTimesEntropy(SslEntropySource *source);
int sslFreeProcessTimesEntropy(SslEntropySource *source);
int sslCollectProcessTimesEntropy(SslEntropySource *source, SceUInt32 bits,
				  unsigned char *output, SceUInt32 max, int *length,
				  SceUInt32 *collected);
int sslGetProcessTimesEntropyInfo(SslEntropySource *source, int id, int arg,
				  void *value);
int sslSetProcessTimesEntropyInfo(SslEntropySource *source, int id, int arg,
				  const void *value);
int sslInitUserInterfaceEntropy(SslEntropySource *source);
int sslFreeUserInterfaceEntropy(SslEntropySource *source);
int sslCollectUserInterfaceEntropy(SslEntropySource *source, SceUInt32 bits,
				   unsigned char *output, SceUInt32 max,
				   int *length, SceUInt32 *collected);
int sslGetUserInterfaceEntropyInfo(SslEntropySource *source, int id,
				   int arg, void *value);
int sslSetUserInterfaceEntropyInfo(SslEntropySource *source, int id,
				   int arg, const void *value);
int sslInitUrandomEntropy(SslEntropySource *source);
int sslFreeUrandomEntropy(SslEntropySource *source);
int sslCollectUrandomEntropy(SslEntropySource *source, SceUInt32 bits,
			     unsigned char *output, SceUInt32 max, int *length,
			     SceUInt32 *collected);
int sslGetUrandomEntropyInfo(SslEntropySource *source, int id, int arg,
			     void *value);
int sslSetUrandomEntropyInfo(SslEntropySource *source, int id, int arg,
			     const void *value);

/* A generator, in a chain: a wrapper over the next one */
typedef struct SslPrng SslPrng;

typedef struct SslPrngDescriptor {
	int (*Init)(SslPrng *prng);
	int (*Free)(SslPrng *prng);
	int (*Generate)(SslPrng *prng, unsigned char *output,
			SceUInt32 *length, SceUInt32 max);
	int (*Seed)(SslPrng *prng, const unsigned char *seed,
		    SceUInt32 length);
	int (*SetInfo)(SslPrng *prng, int id, int arg, const void *value);
	int (*GetInfo)(SslPrng *prng, int id, int arg, void *value);
	int (*Control)(SslPrng *prng, int id, int arg, const void *value);
	SceUInt32 reserved;
	SceUInt32 stateSize;
} SslPrngDescriptor;

struct SslPrng {
	const SslPrngDescriptor *descriptor;
	SslDirectMemoryContext *dmem;
	SslPrng *next;
	void *state;
	SceUInt32 flags;
};

_Static_assert(sizeof(SslPrng) == 20, "SslPrng");

static __inline int sslGenerateLowLevelPrngInline(SslPrng *prng,
						  unsigned char *output,
						  SceUInt32 *length,
						  SceUInt32 max)
{
	if (prng == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (prng->descriptor != NULL)
		return prng->descriptor->Generate(prng, output, length, max);
	return SSL_CRYPTO_ERROR_NO_METHODS;
}

static __inline int sslSeedLowLevelPrngInline(SslPrng *prng,
					      const unsigned char *seed,
					      SceUInt32 length)
{
	if (prng == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (prng->descriptor != NULL)
		return prng->descriptor->Seed(prng, seed, length);
	return SSL_CRYPTO_ERROR_NO_METHODS;
}

static __inline int sslSetLowLevelPrngInfoInline(SslPrng *prng, int id,
						 int arg, const void *value)
{
	if (prng == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (prng->descriptor == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (prng->descriptor->SetInfo != NULL)
		return prng->descriptor->SetInfo(prng, id, arg, value);
	return 0;
}

static __inline int sslGetLowLevelPrngInfoInline(SslPrng *prng, int id,
						 int arg, void *value)
{
	if (prng == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (prng->descriptor == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (prng->descriptor->GetInfo != NULL)
		return prng->descriptor->GetInfo(prng, id, arg, value);
	return 0;
}

static __inline int sslControlLowLevelPrngInline(SslPrng *prng, int id,
						 int arg, const void *value)
{
	if (prng == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (prng->descriptor == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (prng->descriptor->Control != NULL)
		return prng->descriptor->Control(prng, id, arg, value);
	return 0;
}

/* SslPrng.flags of the FIPS 186 generator */
#define SSL_FIPS186_OUTPUT_READY 0x4
#define SSL_FIPS186_NO_OUTPUT_TEST 0x20
#define SSL_FIPS186_FAIL_OUTPUT_TEST 0x40
#define SSL_FIPS186_NO_SEED_TEST 0x80
#define SSL_FIPS186_FAIL_SEED_TEST 0x100

/* Control ids: the generator's tests, the output size, its key or digest */
#define SSL_PRNG_CONTROL_OUTPUT_TEST 1
#define SSL_PRNG_CONTROL_SEED_TEST 2
#define SSL_PRNG_CONTROL_OUTPUT_SIZE 5
#define SSL_PRNG_CONTROL_KEY 4096
#define SSL_PRNG_CONTROL_ENTROPY 4136
#define SSL_PRNG_CONTROL_LOCK 4240

/* SSL_PRNG_CONTROL_OUTPUT_TEST and SEED_TEST: off, on, forced to fail */
#define SSL_PRNG_TEST_OFF 0
#define SSL_PRNG_TEST_FAIL 2

/* SSL_PRNG_CONTROL_KEY: the digest, or the key */
#define SSL_PRNG_KEY_DIGEST 0
#define SSL_PRNG_KEY_DATA 1

/* GetInfo ids */
#define SSL_PRNG_INFO_OUTPUT_SIZE 5
#define SSL_PRNG_INFO_NAME 6

/* The generator over an application's functions */
typedef struct SslEntropyPrngState {
	void *context;
	int (*Generate)(void *context, unsigned char *output,
			SceUInt32 *length, SceUInt32 max);
	int (*Seed)(void *context, const unsigned char *seed,
		    SceUInt32 length);
} SslEntropyPrngState;

/* FIPS 186-2 change notice 1, appendix 3.1, with SHA-1 as G: the key
 * XKEY as it was set and as it is now, the seed XSEED gathered in buffer,
 * two outputs x per round
 */
#define SSL_FIPS186_BLOCK_SIZE 64
#define SSL_FIPS186_OUTPUT_SIZE 20
#define SSL_FIPS186_OUTPUTS 2

typedef struct SslFips186State {
	SslDigestContext *context;
	unsigned char initialKey[SSL_FIPS186_BLOCK_SIZE];
	unsigned char key[SSL_FIPS186_BLOCK_SIZE];
	int keyLength;
	unsigned char buffer[SSL_FIPS186_BLOCK_SIZE];
	int count;
	unsigned char output[SSL_FIPS186_OUTPUTS * SSL_FIPS186_OUTPUT_SIZE];
	unsigned char reserved[24];
	int available;
	int outputSize;
} SslFips186State;

_Static_assert(sizeof(SslFips186State) == 0x114, "SslFips186State");

/* The lock around the next generator */
typedef struct SslPrngLock {
	void *context;
	int (*Lock)(struct SslPrngLock *lock, int acquire, int arg,
		    void *value);
	SceUInt32 reserved[2];
} SslPrngLock;

_Static_assert(sizeof(SslPrngLock) == 16, "SslPrngLock");

int sslSeedEntropyPrng(SslPrng *prng, const unsigned char *seed,
		       SceUInt32 length);
int sslGenerateEntropyPrng(SslPrng *prng, unsigned char *output,
			   SceUInt32 *length, SceUInt32 max);
int sslSetEntropyPrngInfo(SslPrng *prng, int id, int arg,
			  const void *value);
int sslGetEntropyPrngInfo(SslPrng *prng, int id, int arg, void *value);
int sslControlEntropyPrng(SslPrng *prng, int id, int arg,
			  const void *value);
int sslInitFips186Prng(SslPrng *prng);
int sslFreeFips186Prng(SslPrng *prng);
int sslSeedFips186Prng(SslPrng *prng, const unsigned char *seed,
		       SceUInt32 length);
int sslGenerateFips186Prng(SslPrng *prng, unsigned char *output,
			   SceUInt32 *length, SceUInt32 max);
int sslGetFips186PrngInfo(SslPrng *prng, int id, int arg, void *value);
int sslControlFips186Prng(SslPrng *prng, int id, int arg,
			  const void *value);
int sslInitLockedPrng(SslPrng *prng);
int sslSeedLockedPrng(SslPrng *prng, const unsigned char *seed,
		      SceUInt32 length);
int sslGenerateLockedPrng(SslPrng *prng, unsigned char *output,
			  SceUInt32 *length, SceUInt32 max);
int sslSetLockedPrngInfo(SslPrng *prng, int id, int arg, const void *value);
int sslGetLockedPrngInfo(SslPrng *prng, int id, int arg, void *value);
int sslControlLockedPrng(SslPrng *prng, int id, int arg, const void *value);

const SslPrngDescriptor *sslGetEntropyPrngDescriptor(void);
int sslGenerateLowLevelPrng(SslPrng *prng, unsigned char *output,
			    SceUInt32 *length, SceUInt32 max);
const SslPrngDescriptor *sslGetFips186PrngDescriptor(void);
int sslSeedLowLevelPrng(SslPrng *prng, const unsigned char *seed,
			SceUInt32 length);
int sslSetLowLevelPrngInfo(SslPrng *prng, int id, int arg,
			   const void *value);
int sslGetLowLevelPrngInfo(SslPrng *prng, int id, int arg, void *value);
int sslControlLowLevelPrng(SslPrng *prng, int id, int arg,
			   const void *value);
const SslPrngDescriptor *sslGetSha1PrngDescriptor(void);
int sslFreeLowLevelPrng(SslPrng *prng);
void sslFreeLowLevelPrngChain(SslPrng *prng);
int sslLayOutLowLevelPrng(SslPrng **result,
			  const SslPrngDescriptor *descriptor,
			  SslPrng *memory, int *size,
			  SslDirectMemoryContext *dmem);
int sslCreateLowLevelPrng(SslPrng **result,
			  const SslPrngDescriptor *descriptor,
			  SslDirectMemoryContext *dmem);
int sslChainLowLevelPrng(SslPrng *prng, SslPrng *next);

#endif
