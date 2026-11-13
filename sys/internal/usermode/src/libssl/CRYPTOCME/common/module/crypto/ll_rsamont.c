/* Filename and descriptive names are ours: CRYPTOCME's RSA exponentiation
 * with the key, on the big-number moduli (Montgomery arithmetic), the
 * private key by the Chinese remainder theorem over any number of primes.
 */
#include "cry_api.h"
#include "ll_rsa.h"
#include "../dmem/dmem.h"

static SslLowLevelMethod sslRsaPublicMontgomeryMethod = {
	sslRsaKeyOperate,
	sslRsaKeyControl,
	sslSetRsaKeyInfo,
	sslGetRsaKeyInfo,
	{ sslRsaPublicMontgomeryModExp }
};

static SslLowLevelMethod sslRsaPrivateMontgomeryMethod = {
	sslRsaKeyOperate,
	sslRsaKeyControl,
	sslSetRsaKeyInfo,
	sslGetRsaKeyInfo,
	{ sslRsaPrivateMontgomeryModExp }
};

const SslLowLevelMethod *sslGetRsaPublicMontgomeryMethod(void)
{
	return &sslRsaPublicMontgomeryMethod;
}

const SslLowLevelMethod *sslGetRsaPrivateMontgomeryMethod(void)
{
	return &sslRsaPrivateMontgomeryMethod;
}

/* Drops the moduli, which the next operation prepares again */
int sslClearRsaKeyState(SslRsaKeyState *state)
{
	int i;

	state->ready = 0;
	for (i = 0; i < state->moduli; i++)
		sslFreeBigNumModulus(state->modulus[i]);
	sslFreeDirectMemory(state->modulus, state->dmem);
	sslFreeBigNumContext(&state->ctx, 0);
	sslInitBigNumContext(&state->ctx, state->dmem);
	state->moduli = 0;
	state->modulus = NULL;
	return 0;
}

int sslFreeRsaKeyState(SslRsaKeyState *state)
{
	int i;

	sslClearRsaKeyState(state);
	for (i = 0; i < state->numbersAllocated; i++)
		sslFreeBigNum(&state->number[i], 0);
	sslFreeDirectMemory(state->number, state->dmem);
	sslFreeDirectMemory(state, state->dmem);
	return 0;
}

int sslRsaKeyControl(SslLowLevelObject *object, int command, int arg,
		     void *value)
{
	SslRsaKeyState *state;
	int status = 0;

	switch (command) {
	case SSL_LL_CONTROL_INIT:
		status = sslAllocateDirectMemory((void **)&state, sizeof(*state),
						 object->dmem,
						 SSL_DMEM_ALLOCATE_ZERO);
		if (status == 0) {
			sslInitBigNumContext(&state->ctx, object->dmem);
			state->dmem = object->dmem;
			object->state = state;
		}
		break;
	case SSL_LL_CONTROL_FREE:
		state = object->state;
		if (state != NULL) {
			sslFreeRsaKeyState(state);
			object->state = NULL;
		}
		break;
	}
	return status;
}

int sslSetRsaKeyInfo(SslLowLevelObject *object, int type, int id,
		     const void *value)
{
	SslRsaKeyState *state = object->state;
	const SslLowLevelBuffer *buffer;
	SslBigNum *bn;
	void *memory;
	int index, count, i;
	int status = 0;

	switch (type) {
	case SSL_LL_INFO_GENERAL:
		switch (id) {
		case SSL_LL_INFO_RESERVED:
			state->reserved = value;
			break;
		case SSL_LL_INFO_METHODS:
			state->ready = 0;
			state->methods = (SslBigNumMethodContext *)value;
			break;
		case SSL_LL_INFO_CLEAR_STATE:
			state->Complete = sslClearRsaKeyState;
			break;
		case SSL_LL_INFO_KEEP_STATE:
			state->Complete = NULL;
			break;
		}
		break;
	case SSL_LL_INFO_NUMBER:
		if (id < SSL_LL_INFO_NUMBER_FIRST)
			break;
		state->ready = 0;
		index = id - SSL_LL_INFO_NUMBER_FIRST;
		if (index >= state->numbersAllocated) {
			count = index + SSL_RSA_NUMBERS_PER_PRIME;
			if (state->numbersAllocated == 0) {
				status = sslAllocateDirectMemory(
					&memory, count * sizeof(SslBigNum),
					object->dmem, SSL_DMEM_ALLOCATE_ZERO);
				if (status != 0)
					break;
			} else {
				memory = state->number;
				status = sslReallocateDirectMemory(
					&memory, count * sizeof(SslBigNum),
					object->dmem,
					state->numbersAllocated *
						sizeof(SslBigNum),
					SSL_DMEM_ALLOCATE_ZERO);
				if (status != 0)
					break;
			}
			for (i = state->numbersAllocated; i < count; i++)
				sslInitBigNum(&((SslBigNum *)memory)[i],
					      object->dmem);
			state->numbersAllocated = count;
			bn = &((SslBigNum *)memory)[index];
			state->numbers = index + 1;
			state->number = memory;
		} else {
			bn = &state->number[index];
			if (state->numbers <= index)
				state->numbers = index + 1;
		}
		buffer = value;
		status = sslSetBigNumData(bn, buffer->data, buffer->length,
					  &state->ctx);
		break;
	}
	return status;
}

/* Sets up the moduli for the operations in modes: the public key in
 * modulus 0, then the private key in modulus 1, or each prime in its own
 * modulus when the key has its primes.
 */
int sslPrepareRsaKeyState(SslRsaKeyState *state, SceUInt32 modes)
{
	SslBigNumModulus **modulus;
	SslBigNum *number;
	SceUInt32 supported;
	int numbers, count, i;
	int status;

	sslClearRsaKeyState(state);
	status = sslSetBigNumContextMethods(&state->ctx, state->methods);
	if (status != 0)
		return status;
	numbers = state->numbers;
	number = state->number;
	if (numbers <= 0 ||
	    sslIsBigNumZero(&number[SSL_RSA_NUMBER_MODULUS]))
		return SSL_BIGNUM_ERROR_NOT_FOUND;
	count = numbers / SSL_RSA_NUMBERS_PER_PRIME;
	if (count == 0)
		count = 1;
	supported = 0;
	if (modes & SSL_RSA_MODE_PRIVATE) {
		if (numbers / SSL_RSA_NUMBERS_PER_PRIME <= 0)
			return SSL_BIGNUM_ERROR_NOT_FOUND;
		if (numbers >= 3 &&
		    !sslIsBigNumZero(&number[SSL_RSA_NUMBER_PRIVATE_EXPONENT])) {
			supported = SSL_RSA_MODE_PRIVATE;
			if (count < 2)
				count = 2;
		}
		if (numbers > SSL_RSA_NUMBER_PRIMES) {
			/* The first prime's coefficient is not used */
			supported |= SSL_RSA_MODE_CRT;
			for (i = SSL_RSA_NUMBER_PRIMES; i < numbers; i++) {
				if (sslIsBigNumZero(&number[i]) && i != 5) {
					supported &= ~SSL_RSA_MODE_CRT;
					break;
				}
			}
		}
	}
	if (modes & SSL_RSA_MODE_PUBLIC) {
		if (sslIsBigNumZero(&number[SSL_RSA_NUMBER_PUBLIC_EXPONENT]))
			return SSL_BIGNUM_ERROR_NOT_FOUND;
		if (sslIsBigNumOne(&number[SSL_RSA_NUMBER_PUBLIC_EXPONENT]))
			return SSL_BIGNUM_ERROR_INVALID_ARGUMENT;
		supported |= SSL_RSA_MODE_PUBLIC;
	}
	status = sslAllocateDirectMemory((void **)&modulus,
					 count * sizeof(*modulus),
					 state->dmem, SSL_DMEM_ALLOCATE_ZERO);
	if (status != 0)
		return status;
	state->modulus = modulus;
	for (i = state->moduli; i < count; i++) {
		status = sslNewBigNumModulus(&modulus[i], state->dmem);
		if (status != 0)
			return status;
		state->moduli++;
	}
	if (supported & SSL_RSA_MODE_PUBLIC) {
		sslSetBigNumModulusInfo(
			modulus[0], SSL_BIGNUM_MODULUS_INFO_MODULUS,
			&number[SSL_RSA_NUMBER_MODULUS], NULL, 0, &state->ctx);
		sslSetBigNumModulusInfo(
			modulus[0], SSL_BIGNUM_MODULUS_INFO_EXPONENT,
			&number[SSL_RSA_NUMBER_PUBLIC_EXPONENT], NULL, 0,
			&state->ctx);
	}
	if (supported & SSL_RSA_MODE_CRT) {
		for (i = 1; i < state->moduli; i++) {
			sslSetBigNumModulusInfo(
				modulus[i], SSL_BIGNUM_MODULUS_INFO_MODULUS,
				&number[i * SSL_RSA_NUMBERS_PER_PRIME], NULL, 0,
				&state->ctx);
			sslSetBigNumModulusInfo(
				modulus[i], SSL_BIGNUM_MODULUS_INFO_EXPONENT,
				&number[i * SSL_RSA_NUMBERS_PER_PRIME + 1],
				NULL, 0, &state->ctx);
		}
	} else if (supported & SSL_RSA_MODE_PRIVATE) {
		sslSetBigNumModulusInfo(
			modulus[1], SSL_BIGNUM_MODULUS_INFO_MODULUS,
			&number[SSL_RSA_NUMBER_MODULUS], NULL, 0, &state->ctx);
		sslSetBigNumModulusInfo(
			modulus[1], SSL_BIGNUM_MODULUS_INFO_EXPONENT,
			&number[SSL_RSA_NUMBER_PRIVATE_EXPONENT], NULL, 0,
			&state->ctx);
	}
	status = state->ctx.error;
	state->primes = count - 1;
	if (status != 0)
		supported = 0;
	state->ready = 1;
	state->modes = supported;
	if ((supported & modes) == 0 && status == 0)
		status = SSL_BIGNUM_ERROR_NOT_FOUND;
	return status;
}

int sslPrepareRsaPublicKeyState(SslRsaKeyState *state, SceUInt32 modes)
{
	SslBigNumModulus **modulus;
	SslBigNum *number;
	SceUInt32 supported;
	int status;

	sslClearRsaKeyState(state);
	status = sslSetBigNumContextMethods(&state->ctx, state->methods);
	if (status != 0)
		return status;
	number = state->number;
	if (state->numbers <= 0 ||
	    sslIsBigNumZero(&number[SSL_RSA_NUMBER_MODULUS]) ||
	    sslIsBigNumZero(&number[SSL_RSA_NUMBER_PUBLIC_EXPONENT]))
		return SSL_BIGNUM_ERROR_NOT_FOUND;
	if (sslIsBigNumOne(&number[SSL_RSA_NUMBER_PUBLIC_EXPONENT]))
		return SSL_BIGNUM_ERROR_INVALID_ARGUMENT;
	status = sslAllocateDirectMemory((void **)&modulus, sizeof(*modulus),
					 state->dmem, SSL_DMEM_ALLOCATE_ZERO);
	if (status != 0)
		return status;
	state->modulus = modulus;
	status = sslNewBigNumModulus(&modulus[0], state->dmem);
	if (status != 0)
		return status;
	state->moduli = 1;
	sslSetBigNumModulusInfo(modulus[0], SSL_BIGNUM_MODULUS_INFO_MODULUS,
				&number[SSL_RSA_NUMBER_MODULUS], NULL, 0,
				&state->ctx);
	sslSetBigNumModulusInfo(modulus[0], SSL_BIGNUM_MODULUS_INFO_EXPONENT,
				&number[SSL_RSA_NUMBER_PUBLIC_EXPONENT], NULL, 0,
				&state->ctx);
	status = state->ctx.error;
	state->primes = 0;
	supported = status == 0;
	state->ready = 1;
	state->modes = supported;
	if ((supported & modes) == 0 && status == 0)
		status = SSL_BIGNUM_ERROR_NOT_FOUND;
	return status;
}

/* r = a^d mod n; with the primes, Garner's recombination of a^d_i mod p_i:
 * r += ((a^d_i mod p_i - r) * c_i mod p_i) * p_1 ... p_(i-1)
 */
int sslRsaPrivateMontgomeryModExp(SslBigNum *r, SslBigNum *a, void *context)
{
	SslRsaKeyState *state = context;
	SslBigNumContext *ctx;
	SslBigNum u;
	SslBigNum product;
	SslBigNum t;
	SslBigNumModulus **modulus;
	SslBigNum *number;
	const SslBigNum *prime;
	const SslBigNum *coefficient;
	const SslBigNum *m;
	int i, status;

	if (!state->ready) {
		status = sslPrepareRsaKeyState(state, SSL_RSA_MODE_PRIVATE |
							      SSL_RSA_MODE_CRT);
		if (status != 0)
			return status;
	}
	ctx = &state->ctx;
	if (!(state->modes & SSL_RSA_MODE_CRT))
		return sslModExpBigNumModulus(state->modulus[1], r, a, NULL, 0,
					      ctx);
	sslInitBigNum(&t, state->ctx.dmem);
	sslInitBigNum(&u, state->ctx.dmem);
	sslInitBigNum(&product, state->ctx.dmem);
	number = state->number;
	modulus = state->modulus;
	m = &number[SSL_RSA_NUMBER_PRIMES];
	sslModExpBigNumModulus(modulus[1], r, a, NULL, 0, ctx);
	prime = &number[2 * SSL_RSA_NUMBERS_PER_PRIME];
	coefficient = &number[2 * SSL_RSA_NUMBERS_PER_PRIME + 2];
	for (i = 1;;) {
		sslModExpBigNumModulus(modulus[i + 1], &t, a, NULL, 0, ctx);
		sslSubtractBigNum(&t, &t, r, ctx);
		if (t.neg) {
			sslAddBigNum(&t, &t, prime, ctx);
			if (t.neg) {
				sslModBigNum(&t, &t, prime, ctx);
				sslAddBigNum(&t, &t, prime, ctx);
			}
		}
		sslMultiplyBigNum(&u, &t, coefficient, ctx);
		sslModBigNum(&t, &u, prime, ctx);
		sslMultiplyBigNum(&u, &t, m, ctx);
		sslAddBigNum(r, r, &u, ctx);
		if (state->ctx.error != 0)
			break;
		if (++i >= state->primes)
			break;
		sslMultiplyBigNum(&product, m, prime, ctx);
		m = &product;
		prime += SSL_RSA_NUMBERS_PER_PRIME;
		coefficient += SSL_RSA_NUMBERS_PER_PRIME;
	}
	sslFreeBigNum(&t, 0);
	sslFreeBigNum(&u, 0);
	sslFreeBigNum(&product, 0);
	return state->ctx.error;
}

int sslRsaPublicMontgomeryModExp(SslBigNum *r, SslBigNum *a, void *context)
{
	SslRsaKeyState *state = context;
	int status;

	if (!state->ready) {
		status = sslPrepareRsaPublicKeyState(state,
						     SSL_RSA_MODE_PUBLIC);
		if (status != 0)
			return status;
	}
	return sslModExpBigNumModulus(state->modulus[0], r, a, NULL, 0,
				      &state->ctx);
}

/* The exponentiation of the method on a number below the modulus */
int sslRsaKeyOperate(SslLowLevelObject *object, void *params, int command)
{
	SslLowLevelBigNumParams *numbers = params;
	SslRsaKeyState *state;
	int status;

	if ((command & (SSL_LL_OPERATE_ENCRYPT | SSL_LL_OPERATE_DECRYPT)) == 0 ||
	    (command & SSL_LL_OPERATE_CLASS_MASK) != SSL_LL_OPERATE_BIG_NUMBERS)
		return SSL_CRYPTO_ERROR_BAD_COMMAND;
	state = object->state;
	if (sslCompareBigNum(numbers->input, &state->number[0],
			     numbers->ctx) >= 0)
		return SSL_CRYPTO_ERROR_NOT_INITIALIZED;
	status = object->method->extension.ModExp(numbers->output,
						  numbers->input, state);
	if (status == 0 && state->Complete != NULL)
		state->Complete(state);
	return status;
}

static SceUInt32 sslGetRsaKeyLength(const SslRsaKeyState *state)
{
	SceUInt32 length = 0;

	if (state->numbers > 0)
		length = (sslGetBigNumBitLength(&state->number[0]) + 7) / 8;
	return length;
}

int sslGetRsaKeyInfo(SslLowLevelObject *object, int type, int id,
		     void *result)
{
	SslRsaKeyState *state = object->state;
	SslRsaMethodName *method;
	const SslBigNumMethod *arithmetic;

	switch (type) {
	case SSL_LL_INFO_GENERAL:
		if (id == SSL_LL_INFO_MODULUS_LENGTH) {
			*(SceUInt32 *)result = sslGetRsaKeyLength(state);
			return 0;
		}
		break;
	case 2:
		/* Asks for the same ids, but the modulus length */
		break;
	default:
		return SSL_CRYPTO_ERROR_UNHANDLED;
	}
	switch (id) {
	case SSL_LL_INFO_LENGTH:
		*(SceUInt32 *)result = sslGetRsaKeyLength(state);
		return 0;
	case SSL_LL_INFO_METHOD_NAME:
		method = result;
		if (method->index < state->moduli) {
			arithmetic = state->modulus[method->index]->method;
			if (arithmetic != NULL)
				method->name = arithmetic->name;
			else
				method->name = "undef";
		} else {
			method->name = "NA";
		}
		return 0;
	}
	return SSL_CRYPTO_ERROR_UNHANDLED;
}
