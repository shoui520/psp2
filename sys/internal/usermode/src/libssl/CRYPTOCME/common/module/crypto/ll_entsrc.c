/* Filename and descriptive names are ours: CRYPTOCME's Hash DF and entropy
 * sources. On this platform only the whitening source, the time and the
 * user callback produce data; the others are placeholders of the library
 * that collect nothing.
 */
#include "ll_rand.h"
#include "../../../../memory.h"
#include "../../../../format.h"

SceInt64 sceKernelGetSystemTimeWide(void);

/* The block counter and the output length in bits that precede the input */
#define SSL_HASH_DF_HEADER_SIZE 5

int sslFreeHashDfState(SslHashDf *df)
{
	SslHashDfState *state = df->state;

	if (state->context != NULL)
		sslDestroyDigestBackendContext(state->context);
	return 0;
}

int sslInitHashDfState(SslHashDf *df, int arg)
{
	SslHashDfState *state = df->state;
	SceUInt32 length;
	int status;

	status = sslCreateDigestBackendContext(&state->context, state->digest,
					       df->dmem);
	if (status == 0) {
		status = sslGetDigestBackendContextInfo(
			state->context, SSL_LL_DIGEST_INFO_DIGEST_LENGTH,
			&length, NULL);
		state->digestLength = length;
	}
	return status;
}

/* Hash_df: digest(counter || bits || input) for each block of the output */
int sslGenerateHashDfState(SslHashDf *df, SslHashDfOutput *outputs,
			   int count)
{
	SslHashDfState *state = df->state;
	SslDigestContext *context = state->context;
	int digestLength = state->digestLength;
	unsigned char header[SSL_HASH_DF_HEADER_SIZE];
	unsigned char *output;
	unsigned char *p;
	SceUInt32 bits;
	int extra;
	int left;
	int n;
	unsigned char previous, current;
	int status = 0;
	int i;

	for (i = 0; i < count; i++) {
		bits = outputs[i].bits;
		output = outputs[i].data;
		header[4] = bits;
		extra = bits & 7;
		header[0] = 1;
		header[3] = bits >> 8;
		header[2] = bits >> 16;
		left = (bits + 7) >> 3;
		header[1] = bits >> 24;
		while (left > 0) {
			status = sslInitDigestBackendContext(context);
			if (status != 0)
				return status;
			sslUpdateDigestBackendContext(context, header,
						      SSL_HASH_DF_HEADER_SIZE);
			status = sslUpdateDigestBackendContext(
				context, df->input[1].data,
				df->input[1].length);
			if (status != 0)
				return status;
			if (digestLength > left) {
				sslFinalizeDigestBackendContext(context,
								state->buffer);
				sslCopyMemory(output, state->buffer, left);
			} else {
				sslFinalizeDigestBackendContext(context,
								output);
			}
			header[0]++;
			left -= digestLength;
			output += digestLength;
		}
		/* Keep the leftmost bits, right-aligned */
		if (extra != 0) {
			p = outputs[i].data;
			previous = p[0];
			n = (outputs[i].bits + 7) >> 3;
			p[0] = previous >> (8 - extra);
			for (n--, p++; n != 0; n--, p++) {
				current = *p;
				*p = (previous << extra) | (current >> (8 - extra));
				previous = current;
			}
		}
	}
	return status;
}

int sslSetHashDfStateInfo(SslHashDf *df, int id, int arg, const void *value)
{
	SslHashDfState *state = df->state;

	if (id == SSL_HASH_DF_INFO_DIGEST)
		state->digest = value;
	return 0;
}

int sslInitWhitenEntropy(SslEntropySource *source)
{
	return 0;
}

int sslFreeWhitenEntropy(SslEntropySource *source)
{
	SslWhitenState *state = source->state;

	if (state->hashDf != NULL)
		sslFreeHashDf(state->hashDf);
	return 0;
}

/* Collect from the chain, around it again while it gives anything, until
 * it gives the bits asked for
 */
static int sslGatherEntropy(SslEntropySource *whiten, SceUInt32 bits,
			    unsigned char *output, SceUInt32 *collected,
			    SceUInt32 *length)
{
	SslEntropySource *source = whiten->next;
	SceUInt32 total = 0;
	SceUInt32 previous = 0;
	SceUInt32 written = 0;
	int space = SSL_ENTROPY_POOL_SIZE;
	int used;
	SceUInt32 got;
	int status;

	do {
		status = sslCollectLowLevelEntropyInline(source, bits - total,
							 output, space, &used,
							 &got);
		source = source->next;
		if (status == 0) {
			output += used;
			written += used;
			space -= used;
			total += got;
		}
		if (source == NULL) {
			if (total == previous)
				return SSL_CRYPTO_ERROR_BAD_COMMAND;
			source = whiten->next;
			previous = total;
		}
	} while (bits > total && space > 0);
	if (bits <= total) {
		*collected = total;
		*length = written;
	} else {
		status = SSL_CRYPTO_ERROR_NOT_INITIALIZED;
	}
	return status;
}

int sslCollectWhitenEntropy(SslEntropySource *source, SceUInt32 bits,
			    unsigned char *output, SceUInt32 max, int *length,
			    SceUInt32 *collected)
{
	SslWhitenState *state = source->state;
	SslHashDfOutput out;
	SceUInt32 poolLength;
	SceUInt32 got;
	int status;

	if (source->next == NULL)
		return SSL_CRYPTO_ERROR_NO_DIGEST;
	poolLength = 0;
	got = 0;
	if (state->previousLength == 0) {
		status = sslGatherEntropy(source, bits, state->previous, &got,
					  &state->previousLength);
		if (status != 0)
			return status;
	}
	status = sslGatherEntropy(source, bits, state->pool, &got,
				  &poolLength);
	if (status != 0)
		return status;
	/* The sources must have moved on since the last time */
	if (state->previousLength == poolLength &&
	    sslCompareMemory(state->previous, state->pool, poolLength) == 0)
		return SSL_CRYPTO_ERROR_FAILED;
	state->previousLength = poolLength;
	sslCopyMemory(state->previous, state->pool, poolLength);
	status = sslSetHashDfInputs(state->hashDf, state->pool, poolLength,
				    NULL, 0, 0);
	if (status != 0)
		return status;
	out.data = output;
	out.bits = 8 * max;
	status = sslGenerateHashDfInline(state->hashDf, &out, 1);
	if (status != 0)
		return status;
	*length = max;
	*collected = got;
	return 0;
}

int sslGetWhitenEntropyInfo(SslEntropySource *source, int id, int arg,
			    void *value)
{
	switch (id) {
	case SSL_ENTROPY_INFO_NAME:
		*(const char **)value = "Whiten";
		return 0;
	case SSL_ENTROPY_INFO_WHITEN:
		return SSL_CRYPTO_ERROR_UNHANDLED;
	}
	if (source->next != NULL)
		return source->next->descriptor->GetInfo(source->next, id, arg,
							 value);
	return 0;
}

int sslSetWhitenEntropyInfo(SslEntropySource *source, int id, int arg,
			    const void *value)
{
	SslWhitenState *state = source->state;

	if (id == SSL_ENTROPY_INFO_WHITEN) {
		switch (arg) {
		case SSL_ENTROPY_WHITEN_HASH_DF:
			return sslCreateHashDf(&state->hashDf, value,
					       source->dmem);
		case SSL_ENTROPY_WHITEN_DIGEST:
			return sslSetHashDfInfoInline(state->hashDf,
						      SSL_HASH_DF_INFO_DIGEST,
						      0, value);
		}
		return 0;
	}
	if (source->next != NULL)
		return source->next->descriptor->SetInfo(source->next, id, arg,
							 value);
	return 0;
}

int sslInitCallbackEntropy(SslEntropySource *source)
{
	return 0;
}

int sslFreeCallbackEntropy(SslEntropySource *source)
{
	return 0;
}

int sslCollectCallbackEntropy(SslEntropySource *source, SceUInt32 bits,
			      unsigned char *output, SceUInt32 max,
			      int *length, SceUInt32 *collected)
{
	SslEntropyCallback *callback = source->state;
	SslEntropyRequest request;
	int status;

	request.output = output;
	request.length = max;
	request.bits = bits;
	status = callback->Read(callback, 0, 0, &request);
	if (status == 0) {
		*length = request.length;
		*collected = request.bits;
	}
	return status;
}

/* The info functions of a source: its name, its own id, else the next */
#define SSL_ENTROPY_INFO_FUNCTIONS(Kind, NAME, ID)                          \
	int sslGet##Kind##EntropyInfo(SslEntropySource *source, int id,     \
				      int arg, void *value)                 \
	{                                                                   \
		switch (id) {                                               \
		case SSL_ENTROPY_INFO_NAME:                                 \
			*(const char **)value = NAME;                       \
			return 0;                                           \
		case ID:                                                    \
			return SSL_CRYPTO_ERROR_UNHANDLED;                  \
		}                                                           \
		if (source->next != NULL)                                   \
			return source->next->descriptor->GetInfo(           \
				source->next, id, arg, value);              \
		return 0;                                                   \
	}

#define SSL_ENTROPY_SET_INFO_FUNCTION(Kind, ID)                             \
	int sslSet##Kind##EntropyInfo(SslEntropySource *source, int id,     \
				      int arg, const void *value)           \
	{                                                                   \
		if (id == ID)                                               \
			return 0;                                           \
		if (source->next != NULL)                                   \
			return source->next->descriptor->SetInfo(           \
				source->next, id, arg, value);              \
		return 0;                                                   \
	}

/* A source with nothing to collect */
#define SSL_ENTROPY_EMPTY_COLLECT(Kind)                                     \
	int sslCollect##Kind##Entropy(SslEntropySource *source,             \
				      SceUInt32 bits, unsigned char *output, \
				      SceUInt32 max, int *length,           \
				      SceUInt32 *collected)                 \
	{                                                                   \
		*length = 0;                                                \
		*collected = 0;                                             \
		return 0;                                                   \
	}

SSL_ENTROPY_INFO_FUNCTIONS(Callback, "User Callback",
			   SSL_ENTROPY_INFO_USER_CALLBACK)

int sslSetCallbackEntropyInfo(SslEntropySource *source, int id, int arg,
			      const void *value)
{
	if (id == SSL_ENTROPY_INFO_USER_CALLBACK) {
		if (arg == 1)
			sslCopyMemory(source->state, value,
				      sizeof(SslEntropyCallback));
		return 0;
	}
	if (source->next != NULL)
		return source->next->descriptor->SetInfo(source->next, id, arg,
							 value);
	return 0;
}

int sslInitCycleCounterEntropy(SslEntropySource *source)
{
	return 0;
}

int sslFreeCycleCounterEntropy(SslEntropySource *source)
{
	unsigned char *state = source->state;

	state[0] = 0;
	state[1] = 0;
	state[2] = 0;
	state[3] = 0;
	state[4] = 0;
	state[5] = 0;
	state[6] = 0;
	state[7] = 0;
	return 0;
}

SSL_ENTROPY_EMPTY_COLLECT(CycleCounter)
SSL_ENTROPY_SET_INFO_FUNCTION(CycleCounter, SSL_ENTROPY_INFO_CYCLE_COUNTER)
SSL_ENTROPY_INFO_FUNCTIONS(CycleCounter, "Cycle Counter",
			   SSL_ENTROPY_INFO_CYCLE_COUNTER)

int sslFreeFileEntropy(SslEntropySource *source)
{
	SslFileEntropyState *state = source->state;

	if (state->path != NULL)
		source->dmem->methods->Free(state->path, source->dmem);
	sslFillMemory(state, 0, sizeof(*state));
	return 0;
}

int sslCollectFileEntropy(SslEntropySource *source, SceUInt32 bits,
			  unsigned char *output, SceUInt32 max, int *length,
			  SceUInt32 *collected)
{
	return 0;
}

SSL_ENTROPY_INFO_FUNCTIONS(File, "File", SSL_ENTROPY_INFO_FILE)

int sslSetFileEntropyInfo(SslEntropySource *source, int id, int arg,
			  const void *value)
{
	SslFileEntropyState *state = source->state;
	int status;

	if (id == SSL_ENTROPY_INFO_FILE) {
		if (arg == SSL_ENTROPY_FILE_PATH) {
			status = source->dmem->methods->Allocate(
				(void **)&state->path,
				sslStringLength(value) + 1, source->dmem,
				SSL_DMEM_ALLOCATE_ZERO);
			if (status != 0)
				return status;
			sslCopyString(state->path, value,
				      sslStringLength(value) + 1);
		}
		return 0;
	}
	if (source->next != NULL)
		return source->next->descriptor->SetInfo(source->next, id, arg,
							 value);
	return 0;
}

int sslInitMemoryStatisticsEntropy(SslEntropySource *source)
{
	return 0;
}

int sslFreeMemoryStatisticsEntropy(SslEntropySource *source)
{
	return 0;
}

SSL_ENTROPY_EMPTY_COLLECT(MemoryStatistics)
SSL_ENTROPY_INFO_FUNCTIONS(MemoryStatistics, "Memory Statistics",
			   SSL_ENTROPY_INFO_MEMORY_STATISTICS)
SSL_ENTROPY_SET_INFO_FUNCTION(MemoryStatistics,
			      SSL_ENTROPY_INFO_MEMORY_STATISTICS)

int sslInitRegistryEntropy(SslEntropySource *source)
{
	return 0;
}

int sslFreeRegistryEntropy(SslEntropySource *source)
{
	sslFillMemory(source->state, 0, 0x50);
	return 0;
}

SSL_ENTROPY_EMPTY_COLLECT(Registry)
SSL_ENTROPY_INFO_FUNCTIONS(Registry, "Registry", SSL_ENTROPY_INFO_REGISTRY)
SSL_ENTROPY_SET_INFO_FUNCTION(Registry, SSL_ENTROPY_INFO_REGISTRY)

int sslInitTimeEntropy(SslEntropySource *source)
{
	return 0;
}

int sslFreeTimeEntropy(SslEntropySource *source)
{
	unsigned char *state = source->state;

	state[0] = 0;
	state[1] = 0;
	state[2] = 0;
	state[3] = 0;
	return 0;
}

/* The system time, not twice the same; the time is never kept, so only a
 * time of 0 is refused
 */
int sslCollectTimeEntropy(SslEntropySource *source, SceUInt32 bits,
			  unsigned char *output, SceUInt32 max, int *length,
			  SceUInt32 *collected)
{
	unsigned char data[8];
	SceInt64 now;
	SceUInt32 low, high;
	SceUInt32 n = sizeof(data);

	now = (long)sceKernelGetSystemTimeWide();
	if (now == *(SceUInt32 *)source->state)
		return SSL_CRYPTO_ERROR_BAD_COMMAND;
	low = (SceUInt32)now;
	high = (SceUInt32)((SceUInt64)now >> 32);
	data[0] = low;
	data[4] = high;
	data[1] = low >> 8;
	data[2] = low >> 16;
	data[3] = low >> 24;
	data[5] = high >> 8;
	data[6] = high >> 16;
	data[7] = high >> 24;
	if (max < sizeof(data))
		n = max;
	sslCopyMemory(output, data, n);
	*length = n;
	*collected = 1;
	return 0;
}

SSL_ENTROPY_INFO_FUNCTIONS(Time, "Time", SSL_ENTROPY_INFO_TIME)
SSL_ENTROPY_SET_INFO_FUNCTION(Time, SSL_ENTROPY_INFO_TIME)

int sslInitProcessTimesEntropy(SslEntropySource *source)
{
	return 0;
}

int sslFreeProcessTimesEntropy(SslEntropySource *source)
{
	return 0;
}

SSL_ENTROPY_EMPTY_COLLECT(ProcessTimes)
SSL_ENTROPY_INFO_FUNCTIONS(ProcessTimes, "Process Times",
			   SSL_ENTROPY_INFO_PROCESS_TIMES)
SSL_ENTROPY_SET_INFO_FUNCTION(ProcessTimes, SSL_ENTROPY_INFO_PROCESS_TIMES)

int sslInitUserInterfaceEntropy(SslEntropySource *source)
{
	return 0;
}

int sslFreeUserInterfaceEntropy(SslEntropySource *source)
{
	return 0;
}

SSL_ENTROPY_EMPTY_COLLECT(UserInterface)
SSL_ENTROPY_INFO_FUNCTIONS(UserInterface, "User Interface",
			   SSL_ENTROPY_INFO_USER_INTERFACE)
SSL_ENTROPY_SET_INFO_FUNCTION(UserInterface, SSL_ENTROPY_INFO_USER_INTERFACE)

int sslInitUrandomEntropy(SslEntropySource *source)
{
	return 0;
}

int sslFreeUrandomEntropy(SslEntropySource *source)
{
	return 0;
}

SSL_ENTROPY_EMPTY_COLLECT(Urandom)
SSL_ENTROPY_INFO_FUNCTIONS(Urandom, "/dev/urandom", SSL_ENTROPY_INFO_URANDOM)
SSL_ENTROPY_SET_INFO_FUNCTION(Urandom, SSL_ENTROPY_INFO_URANDOM)
