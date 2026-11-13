/* Filename and descriptive names are ours: CRYPTOCME's generator over an
 * application's functions, and the FIPS 186-2 generator with SHA-1.
 */
#include "ll_rand.h"
#include <string.h>
#include "../../../../memory.h"

int sslSeedEntropyPrng(SslPrng *prng, const unsigned char *seed,
		       SceUInt32 length)
{
	SslEntropyPrngState *state = prng->state;

	if (state->Seed != NULL)
		return state->Seed(state->context, seed, length);
	return 0;
}

int sslGenerateEntropyPrng(SslPrng *prng, unsigned char *output,
			   SceUInt32 *length, SceUInt32 max)
{
	SslEntropyPrngState *state = prng->state;

	if (state->Generate != NULL)
		return state->Generate(state->context, output, length, max);
	return 0;
}

int sslSetEntropyPrngInfo(SslPrng *prng, int id, int arg, const void *value)
{
	return 0;
}

int sslGetEntropyPrngInfo(SslPrng *prng, int id, int arg, void *value)
{
	return 0;
}

int sslControlEntropyPrng(SslPrng *prng, int id, int arg, const void *value)
{
	SslEntropyPrngState *state = prng->state;

	if (id == SSL_PRNG_CONTROL_ENTROPY && arg == 1)
		sslCopyMemory(state, value, sizeof(*state));
	return 0;
}

int sslInitFips186Prng(SslPrng *prng)
{
	SslFips186State *state = prng->state;

	state->outputSize = sizeof(state->output);
	return 0;
}

int sslFreeFips186Prng(SslPrng *prng)
{
	SslFips186State *state = prng->state;

	if (state->context != NULL) {
		sslDestroyDigestBackendContext(state->context);
		((SslFips186State *)prng->state)->context = NULL;
	}
	return 0;
}

/* Gather the seed into the buffer; a full buffer is used up by a round
 * of output first
 */
int sslSeedFips186Prng(SslPrng *prng, const unsigned char *seed,
		       SceUInt32 length)
{
	SslFips186State *state = prng->state;
	SceUInt32 got;
	unsigned char block[SSL_FIPS186_BLOCK_SIZE];
	int total = state->count + (int)length;
	int n;
	int status;

	if (total <= state->keyLength) {
		n = length;
	} else {
		n = state->keyLength - state->count;
		if (n == 0) {
			total -= state->count;
			status = sslGenerateLowLevelPrngInline(prng,
							       state->buffer,
							       &got,
							       state->count);
			if (status != 0)
				return status;
		}
	}
	if (n > 0) {
		sslCopyMemory(state->buffer + state->count, seed, n);
		state->count += n;
		seed += n;
		total -= state->count;
	}
	while (total > state->keyLength) {
		status = sslGenerateLowLevelPrngInline(prng, block, &got,
						       sizeof(block));
		if (status != 0)
			return status;
		n = total;
		if (n > state->keyLength)
			n = state->keyLength;
		sslCopyMemory(state->buffer, seed, n);
		state->count = n;
		total -= n;
		seed += n;
	}
	return 0;
}

/* a += b, both big-endian, b right-aligned under a; the carry out of a is
 * dropped
 */
static __inline void sslAddFips186Bytes(unsigned char *a, int aLength,
					const unsigned char *b, int bLength)
{
	const unsigned char *in = b + bLength - 1;
	unsigned char *out = a + aLength - 1;
	unsigned int carry = 0;
	unsigned int sum;
	int i = bLength - 1;
	int j = aLength - 1;

	while (i >= 0) {
		i--;
		sum = carry + *out + *in--;
		*out-- = sum;
		carry = sum >> 8;
		j--;
	}
	if (carry != 0) {
		while (j >= 0) {
			if (++*out != 0)
				break;
			j--;
			out--;
		}
	}
}

/* x = G(t, XKEY + XSEED mod 2^b), then XKEY = 1 + XKEY + x mod 2^b */
static int sslComputeFips186Output(SslFips186State *state,
				   unsigned char *output)
{
	unsigned char block[SSL_FIPS186_BLOCK_SIZE];
	SslDigestContext *context;
	unsigned char *p;
	int n;
	int status;

	memset(block, 0, sizeof(block));
	sslCopyMemory(block, state->key, state->keyLength);
	if (state->count != 0)
		sslAddFips186Bytes(block, state->keyLength, state->buffer,
				   state->count);
	context = state->context;
	status = sslInitDigestBackendContext(context);
	if (status == 0) {
		sslUpdateDigestBackendContext(context, block, sizeof(block));
		status = sslGetSha1DigestValues(context, output);
	}
	if (status == 0) {
		n = state->keyLength;
		sslAddFips186Bytes(state->key, n, output,
				   SSL_FIPS186_OUTPUT_SIZE);
		for (p = state->key + n; n != 0; n--, p--)
			if (++p[-1] != 0)
				break;
	}
	return status;
}

static __inline int sslFillFips186Output(SslFips186State *state)
{
	int status;

	status = sslComputeFips186Output(state, state->output);
	if (status == 0) {
		status = sslComputeFips186Output(
			state, state->output + SSL_FIPS186_OUTPUT_SIZE);
		if (status == 0)
			state->available = sizeof(state->output);
		state->count = 0;
	}
	return status;
}

int sslGenerateFips186Prng(SslPrng *prng, unsigned char *output,
			   SceUInt32 *length, SceUInt32 max)
{
	SslFips186State *state = prng->state;
	unsigned char previous[sizeof(state->output)];
	int test = (prng->flags & SSL_FIPS186_NO_OUTPUT_TEST) == 0;
	SceUInt32 left = max;
	int n;
	int status;

	if (state->keyLength == 0)
		return SSL_CRYPTO_ERROR_BAD_COMMAND;
	/* The seed must not be the key */
	if (!(prng->flags & SSL_FIPS186_NO_SEED_TEST) &&
	    ((prng->flags & SSL_FIPS186_FAIL_SEED_TEST) ||
	     (state->keyLength == state->count &&
	      sslCompareMemory(state->initialKey, state->buffer,
			       state->keyLength) == 0)))
		return SSL_CRYPTO_ERROR_FAILED;
	/* The first output is only kept to compare the next with */
	if (test && !(prng->flags & SSL_FIPS186_OUTPUT_READY)) {
		status = sslFillFips186Output(state);
		if (status != 0)
			return status;
		prng->flags |= SSL_FIPS186_OUTPUT_READY;
	}
	n = state->available;
	if (n > 0) {
		int offset = sizeof(state->output) - n;

		if (n > left)
			n = left;
		sslCopyMemory(output, state->output + offset, n);
		state->available -= n;
		output += n;
		left -= n;
	}
	while (left != 0) {
		if (test)
			sslCopyMemory(previous, state->output,
				      sizeof(previous));
		status = sslFillFips186Output(state);
		if (status != 0)
			return status;
		/* Continuous test: no output twice in a row */
		if ((prng->flags & SSL_FIPS186_FAIL_OUTPUT_TEST) ||
		    (test && sslCompareMemory(previous, state->output,
					      sizeof(previous)) == 0))
			return SSL_CRYPTO_ERROR_FAILED;
		n = sizeof(state->output);
		if (left < sizeof(state->output))
			n = left;
		sslCopyMemory(output, state->output, n);
		state->available -= n;
		left -= n;
		output += n;
	}
	*length = max;
	return 0;
}

int sslGetFips186PrngInfo(SslPrng *prng, int id, int arg, void *value)
{
	SslFips186State *state = prng->state;

	switch (id) {
	case SSL_PRNG_INFO_OUTPUT_SIZE:
		*(int *)value = state->outputSize;
		break;
	case SSL_PRNG_INFO_NAME:
		*(const char **)value = "FIPS 186-2 SHA-1 Random";
		break;
	}
	return 0;
}

int sslControlFips186Prng(SslPrng *prng, int id, int arg, const void *value)
{
	SslFips186State *state = prng->state;
	const SslHashDfInput *key;

	switch (id) {
	case SSL_PRNG_CONTROL_KEY:
		switch (arg) {
		case SSL_PRNG_KEY_DIGEST:
			if (state->context != NULL) {
				sslDestroyDigestBackendContext(state->context);
				state->context = NULL;
			}
			return sslCreateDigestBackendContext(&state->context,
							     value, prng->dmem);
		case SSL_PRNG_KEY_DATA:
			key = value;
			sslCopyMemory(state->key, key->data, key->length);
			sslCopyMemory(state->initialKey, key->data,
				      key->length);
			state->keyLength = key->length;
			break;
		}
		return 0;
	case SSL_PRNG_CONTROL_OUTPUT_SIZE:
		state->available = 0;
		state->outputSize = arg;
		state->keyLength = 0;
		state->count = 0;
		break;
	case SSL_PRNG_CONTROL_SEED_TEST:
		prng->flags &= ~(SSL_FIPS186_NO_SEED_TEST |
				 SSL_FIPS186_FAIL_SEED_TEST);
		if (arg == SSL_PRNG_TEST_OFF)
			prng->flags |= SSL_FIPS186_NO_SEED_TEST;
		else if (arg == SSL_PRNG_TEST_FAIL)
			prng->flags |= SSL_FIPS186_FAIL_SEED_TEST;
		break;
	case SSL_PRNG_CONTROL_OUTPUT_TEST:
		prng->flags &= ~(SSL_FIPS186_NO_OUTPUT_TEST |
				 SSL_FIPS186_FAIL_OUTPUT_TEST);
		if (arg == SSL_PRNG_TEST_OFF)
			prng->flags |= SSL_FIPS186_NO_OUTPUT_TEST;
		else if (arg == SSL_PRNG_TEST_FAIL)
			prng->flags |= SSL_FIPS186_FAIL_OUTPUT_TEST;
		break;
	}
	return 0;
}
