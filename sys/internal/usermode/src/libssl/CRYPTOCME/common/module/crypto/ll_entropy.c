/* Filename and descriptive names are ours: CRYPTOCME's entropy sources: a
 * whitening source over a chain of sources, each a descriptor and a state.
 */
#include "ll_rand.h"
#include "../../../../memory.h"

static const SslEntropyDescriptor sslWhitenEntropyDescriptor = {
	sslInitWhitenEntropy,
	sslFreeWhitenEntropy,
	sslCollectWhitenEntropy,
	0,
	sslGetWhitenEntropyInfo,
	sslSetWhitenEntropyInfo,
	sizeof(SslWhitenState)
};

static const SslEntropyDescriptor sslCycleCounterEntropyDescriptor = {
	sslInitCycleCounterEntropy,
	sslFreeCycleCounterEntropy,
	sslCollectCycleCounterEntropy,
	0,
	sslGetCycleCounterEntropyInfo,
	sslSetCycleCounterEntropyInfo,
	0
};

static const SslEntropyDescriptor sslUrandomEntropyDescriptor = {
	sslInitUrandomEntropy,
	sslFreeUrandomEntropy,
	sslCollectUrandomEntropy,
	0,
	sslGetUrandomEntropyInfo,
	sslSetUrandomEntropyInfo,
	0
};

static const SslEntropyDescriptor sslTimeEntropyDescriptor = {
	sslInitTimeEntropy,
	sslFreeTimeEntropy,
	sslCollectTimeEntropy,
	0,
	sslGetTimeEntropyInfo,
	sslSetTimeEntropyInfo,
	sizeof(SceUInt32)
};

static const SslEntropyDescriptor sslFileEntropyDescriptor = {
	NULL,
	sslFreeFileEntropy,
	sslCollectFileEntropy,
	0,
	sslGetFileEntropyInfo,
	sslSetFileEntropyInfo,
	sizeof(SslFileEntropyState)
};

static const SslEntropyDescriptor sslRegistryEntropyDescriptor = {
	sslInitRegistryEntropy,
	sslFreeRegistryEntropy,
	sslCollectRegistryEntropy,
	0,
	sslGetRegistryEntropyInfo,
	sslSetRegistryEntropyInfo,
	0
};

static const SslEntropyDescriptor sslProcessTimesEntropyDescriptor = {
	sslInitProcessTimesEntropy,
	sslFreeProcessTimesEntropy,
	sslCollectProcessTimesEntropy,
	0,
	sslGetProcessTimesEntropyInfo,
	sslSetProcessTimesEntropyInfo,
	0
};

static const SslEntropyDescriptor sslMemoryStatisticsEntropyDescriptor = {
	sslInitMemoryStatisticsEntropy,
	sslFreeMemoryStatisticsEntropy,
	sslCollectMemoryStatisticsEntropy,
	0,
	sslGetMemoryStatisticsEntropyInfo,
	sslSetMemoryStatisticsEntropyInfo,
	0
};

static const SslEntropyDescriptor sslUserInterfaceEntropyDescriptor = {
	sslInitUserInterfaceEntropy,
	sslFreeUserInterfaceEntropy,
	sslCollectUserInterfaceEntropy,
	0,
	sslGetUserInterfaceEntropyInfo,
	sslSetUserInterfaceEntropyInfo,
	0
};

static const SslEntropyDescriptor sslCallbackEntropyDescriptor = {
	sslInitCallbackEntropy,
	sslFreeCallbackEntropy,
	sslCollectCallbackEntropy,
	0,
	sslGetCallbackEntropyInfo,
	sslSetCallbackEntropyInfo,
	16
};

int sslCollectLowLevelEntropy(SslEntropySource *source, SceUInt32 bits,
			      unsigned char *output, SceUInt32 max, int *length,
			      SceUInt32 *collected)
{
	return sslCollectLowLevelEntropyInline(source, bits, output, max,
					       length, collected);
}

const SslEntropyDescriptor *sslGetDefaultEntropyDescriptor(void)
{
	return &sslWhitenEntropyDescriptor;
}

void sslFreeLowLevelEntropySource(SslEntropySource *source)
{
	const SslEntropyDescriptor *descriptor;
	SslDirectMemoryContext *dmem;
	SslEntropySource *next;

	if (source == NULL)
		return;
	descriptor = source->descriptor;
	dmem = source->dmem;
	if (source->next != NULL) {
		next = source->next;
		source->next = NULL;
		sslFreeLowLevelEntropySource(next);
	}
	if (descriptor != NULL && source->state != NULL) {
		if (descriptor->Free != NULL)
			descriptor->Free(source);
		if (source->state != NULL)
			dmem->methods->Free(source->state, dmem);
	}
	if (source->flags & SSL_RAND_OBJECT_ALLOCATED)
		dmem->methods->Free(source, dmem);
}

int sslLayOutLowLevelEntropySource(SslEntropySource **result,
				   const SslEntropyDescriptor *descriptor,
				   SslEntropySource *memory, int *size,
				   SslDirectMemoryContext *dmem)
{
	const SslEntropyDescriptor *used;
	SceUInt32 stateSize;
	int status = 0;

	if (memory != NULL) {
		if (*size >= (int)sizeof(SslEntropySource)) {
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
			if (status == 0 && descriptor->Init != NULL)
				status = used->Init(memory);
			if (status != 0)
				sslFreeLowLevelEntropySource(memory);
			else
				*result = memory;
		} else {
			status = SSL_CRYPTO_ERROR_NOT_INITIALIZED;
		}
	}
	*size = sizeof(SslEntropySource);
	return status;
}

int sslCreateLowLevelEntropySource(SslEntropySource **result,
				   const SslEntropyDescriptor *descriptor,
				   SslDirectMemoryContext *dmem)
{
	SslEntropySource *memory;
	int size;
	int status;

	if (dmem == NULL) {
		if (*result != NULL)
			dmem = (*result)->dmem;
		if (dmem == NULL)
			return SSL_CRYPTO_ERROR_NO_MEMORY;
	}
	sslLayOutLowLevelEntropySource(result, descriptor, NULL, &size, dmem);
	status = dmem->methods->Allocate((void **)&memory, size, dmem,
					 SSL_DMEM_ALLOCATE_ZERO);
	if (status == 0) {
		if (*result != NULL)
			sslFreeLowLevelEntropySource(*result);
		*result = NULL;
		status = sslLayOutLowLevelEntropySource(result, descriptor,
							memory, &size, dmem);
		if (status != 0 && memory != NULL)
			dmem->methods->Free(memory, dmem);
		if (status == 0 && *result != NULL)
			(*result)->flags |= SSL_RAND_OBJECT_ALLOCATED;
	}
	return status;
}

int sslAddLowLevelEntropySource(SslEntropySource *source,
				SslEntropySource *added)
{
	added->next = source->next;
	source->next = added;
	return 0;
}

int sslSetLowLevelEntropySourceInfo(SslEntropySource *source, int id,
				    int arg, const void *value)
{
	if (source == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (source->descriptor == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (source->descriptor->SetInfo != NULL)
		return source->descriptor->SetInfo(source, id, arg, value);
	return SSL_CRYPTO_ERROR_NO_METHODS;
}

const SslEntropyDescriptor *sslGetCycleCounterEntropyDescriptor(void)
{
	return &sslCycleCounterEntropyDescriptor;
}

const SslEntropyDescriptor *sslGetUrandomEntropyDescriptor(void)
{
	return &sslUrandomEntropyDescriptor;
}

const SslEntropyDescriptor *sslGetTimeEntropyDescriptor(void)
{
	return &sslTimeEntropyDescriptor;
}

const SslEntropyDescriptor *sslGetFileEntropyDescriptor(void)
{
	return &sslFileEntropyDescriptor;
}

/* The seed file, the time, /dev/urandom and the cycle counter */
int sslInitLowLevelEntropySourceMode1(SslEntropySource *source)
{
	SslEntropySource *added = NULL;
	int status;

	status = sslCreateLowLevelEntropySource(
		&added, &sslFileEntropyDescriptor, source->dmem);
	if (status != 0)
		return status;
	status = sslSetLowLevelEntropySourceInfo(added, SSL_ENTROPY_INFO_FILE,
						 SSL_ENTROPY_FILE_PATH,
						 "entropy");
	if (status != 0)
		return status;
	sslAddLowLevelEntropySource(source, added);
	added = NULL;
	status = sslCreateLowLevelEntropySource(
		&added, &sslTimeEntropyDescriptor, source->dmem);
	if (status != 0)
		return status;
	sslAddLowLevelEntropySource(source, added);
	added = NULL;
	status = sslCreateLowLevelEntropySource(
		&added, &sslUrandomEntropyDescriptor, source->dmem);
	if (status != 0)
		return status;
	sslAddLowLevelEntropySource(source, added);
	added = NULL;
	status = sslCreateLowLevelEntropySource(
		&added, &sslCycleCounterEntropyDescriptor, source->dmem);
	if (status != 0)
		return status;
	sslAddLowLevelEntropySource(source, added);
	return 0;
}

const SslEntropyDescriptor *sslGetRegistryEntropyDescriptor(void)
{
	return &sslRegistryEntropyDescriptor;
}

const SslEntropyDescriptor *sslGetProcessTimesEntropyDescriptor(void)
{
	return &sslProcessTimesEntropyDescriptor;
}

const SslEntropyDescriptor *sslGetMemoryStatisticsEntropyDescriptor(void)
{
	return &sslMemoryStatisticsEntropyDescriptor;
}

const SslEntropyDescriptor *sslGetUserInterfaceEntropyDescriptor(void)
{
	return &sslUserInterfaceEntropyDescriptor;
}

/* Every source, then the digest of the whitening */
int sslInitLowLevelEntropySourceMode2(SslEntropySource *source,
				      const void *digest)
{
	SslEntropySource *added = NULL;
	int status;

	status = sslCreateLowLevelEntropySource(
		&added, &sslFileEntropyDescriptor, source->dmem);
	if (status != 0)
		return status;
	status = sslSetLowLevelEntropySourceInfo(added, SSL_ENTROPY_INFO_FILE,
						 SSL_ENTROPY_FILE_PATH,
						 "entropy");
	if (status != 0)
		return status;
	sslAddLowLevelEntropySource(source, added);
	added = NULL;
	status = sslCreateLowLevelEntropySource(
		&added, &sslUserInterfaceEntropyDescriptor, source->dmem);
	if (status != 0)
		return status;
	sslAddLowLevelEntropySource(source, added);
	added = NULL;
	status = sslCreateLowLevelEntropySource(
		&added, &sslMemoryStatisticsEntropyDescriptor, source->dmem);
	if (status != 0)
		return status;
	sslAddLowLevelEntropySource(source, added);
	added = NULL;
	status = sslCreateLowLevelEntropySource(
		&added, &sslProcessTimesEntropyDescriptor, source->dmem);
	if (status != 0)
		return status;
	sslAddLowLevelEntropySource(source, added);
	added = NULL;
	status = sslCreateLowLevelEntropySource(
		&added, &sslUrandomEntropyDescriptor, source->dmem);
	if (status != 0)
		return status;
	sslAddLowLevelEntropySource(source, added);
	added = NULL;
	status = sslCreateLowLevelEntropySource(
		&added, &sslRegistryEntropyDescriptor, source->dmem);
	if (status != 0)
		return status;
	sslAddLowLevelEntropySource(source, added);
	added = NULL;
	status = sslCreateLowLevelEntropySource(
		&added, &sslTimeEntropyDescriptor, source->dmem);
	if (status != 0)
		return status;
	sslAddLowLevelEntropySource(source, added);
	added = NULL;
	status = sslCreateLowLevelEntropySource(
		&added, &sslCycleCounterEntropyDescriptor, source->dmem);
	if (status != 0)
		return status;
	sslAddLowLevelEntropySource(source, added);
	added = NULL;
	return sslSetLowLevelEntropySourceInfo(source, 1, 1, digest);
}

const SslEntropyDescriptor *sslGetCallbackEntropyDescriptor(void)
{
	return &sslCallbackEntropyDescriptor;
}
