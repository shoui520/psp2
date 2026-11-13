/* Filename and descriptive names are ours: CRYPTOCME's RSA exponentiation
 * with the key, by Barrett reduction (the reciprocal of the modulus).
 */
#include "cry_api.h"
#include "ll_rsa.h"
#include "../dmem/dmem.h"

static SslLowLevelMethod sslRsaPublicReciprocalMethod = {
	sslRsaKeyOperate,
	sslRsaReciprocalKeyControl,
	sslSetRsaReciprocalKeyInfo,
	sslGetRsaReciprocalKeyInfo,
	{ sslRsaPublicReciprocalModExp }
};

static SslLowLevelMethod sslRsaPrivateReciprocalMethod = {
	sslRsaKeyOperate,
	sslRsaReciprocalKeyControl,
	sslSetRsaReciprocalKeyInfo,
	sslGetRsaReciprocalKeyInfo,
	{ sslRsaPrivateReciprocalModExp }
};

const SslLowLevelMethod *sslGetRsaPublicReciprocalMethod(void)
{
	return &sslRsaPublicReciprocalMethod;
}

const SslLowLevelMethod *sslGetRsaPrivateReciprocalMethod(void)
{
	return &sslRsaPrivateReciprocalMethod;
}

int sslFreeRsaReciprocalKeyState(SslRsaKeyState *state)
{
	int i;

	for (i = 0; i < state->numbersAllocated; i++)
		sslFreeBigNum(&state->number[i], 0);
	sslFreeDirectMemory(state->number, state->dmem);
	sslFreeBigNumContext(&state->ctx, SSL_BIGNUM_FREE_WIPE);
	sslFreeDirectMemory(state, state->dmem);
	return 0;
}

int sslRsaReciprocalKeyControl(SslLowLevelObject *object, int command,
			       int arg, void *value)
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
			sslFreeRsaReciprocalKeyState(state);
			object->state = NULL;
		}
		break;
	}
	return status;
}

/* Takes n, e and d */
int sslSetRsaReciprocalKeyInfo(SslLowLevelObject *object, int type, int id,
			       const void *value)
{
	SslRsaKeyState *state = object->state;
	const SslLowLevelBuffer *buffer;
	SslBigNumContext *ctx;
	SslBigNum *bn;
	void *memory;
	int index, count, i;
	int status = 0;

	switch (type) {
	case SSL_LL_INFO_GENERAL:
		if (id == SSL_LL_INFO_METHODS) {
			state->ready = 0;
			state->methods = (SslBigNumMethodContext *)value;
		}
		break;
	case SSL_LL_INFO_NUMBER:
		if (id != SSL_LL_INFO_NUMBER_FIRST &&
		    id != SSL_LL_INFO_NUMBER_FIRST + 1 &&
		    id != SSL_LL_INFO_NUMBER_FIRST + 2)
			break;
		state->ready = 0;
		index = id - SSL_LL_INFO_NUMBER_FIRST;
		ctx = &state->ctx;
		if (index >= state->numbersAllocated) {
			count = index + SSL_RSA_NUMBERS_PER_PRIME;
			/* The old size is the count of numbers set */
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
					state->numbers * sizeof(SslBigNum),
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
					  ctx);
		break;
	}
	return status;
}

int sslPrepareRsaReciprocalPublicKey(SslRsaKeyState *state, SceUInt32 modes)
{
	SslBigNum *number = state->number;
	int status = 0;

	if (state->numbers <= 0 ||
	    sslIsBigNumZero(&number[SSL_RSA_NUMBER_MODULUS]) ||
	    sslIsBigNumZero(&number[SSL_RSA_NUMBER_PUBLIC_EXPONENT]))
		return SSL_BIGNUM_ERROR_NOT_FOUND;
	if (sslIsBigNumOne(&number[SSL_RSA_NUMBER_PUBLIC_EXPONENT]))
		return SSL_BIGNUM_ERROR_INVALID_ARGUMENT;
	state->ready = 1;
	state->modes = SSL_RSA_MODE_PUBLIC;
	if ((modes & SSL_RSA_MODE_PUBLIC) == 0)
		return SSL_BIGNUM_ERROR_NOT_FOUND;
	return status;
}

int sslRsaPublicReciprocalModExp(SslBigNum *r, SslBigNum *a, void *context)
{
	SslRsaKeyState *state = context;
	SslBigNumReciprocal recp;
	int status;

	if (!state->ready) {
		status = sslPrepareRsaReciprocalPublicKey(state,
							  SSL_RSA_MODE_PUBLIC);
		if (status != 0)
			return status;
	}
	sslInitBigNumReciprocal(&recp, state->dmem);
	sslSetBigNumReciprocal(&recp, &state->number[SSL_RSA_NUMBER_MODULUS],
			       &state->ctx);
	status = sslModExpBigNumReciprocalSimple(
		r, a, &state->number[SSL_RSA_NUMBER_PUBLIC_EXPONENT], &recp,
		&state->ctx);
	sslFreeBigNumReciprocal(&recp, 0);
	return status;
}

int sslGetRsaReciprocalKeyInfo(SslLowLevelObject *object, int type, int id,
			       void *result)
{
	SslRsaKeyState *state = object->state;
	SceUInt32 length;

	switch (type) {
	case SSL_LL_INFO_GENERAL:
		if (id == SSL_LL_INFO_MODULUS_LENGTH)
			goto modulusLength;
		break;
	case 2:
		/* Asks for the same ids, but the modulus length */
		break;
	default:
		return SSL_CRYPTO_ERROR_UNHANDLED;
	}
	switch (id) {
	case SSL_LL_INFO_LENGTH:
modulusLength:
		length = 0;
		if (state->numbers > 0)
			length = (sslGetBigNumBitLength(&state->number[0]) +
				  7) / 8;
		*(SceUInt32 *)result = length;
		return 0;
	case SSL_LL_INFO_METHOD_NAME:
		((SslRsaMethodName *)result)->name = "recp";
		return 0;
	}
	return SSL_CRYPTO_ERROR_UNHANDLED;
}

/* Needs n and d; checks for two numbers only */
int sslPrepareRsaReciprocalPrivateKey(SslRsaKeyState *state,
				      SceUInt32 modes)
{
	SslBigNum *number = state->number;
	int status = 0;

	if (state->numbers < 2 ||
	    sslIsBigNumZero(&number[SSL_RSA_NUMBER_MODULUS]) ||
	    sslIsBigNumZero(&number[SSL_RSA_NUMBER_PRIVATE_EXPONENT]))
		return SSL_BIGNUM_ERROR_NOT_FOUND;
	if (sslIsBigNumOne(&number[SSL_RSA_NUMBER_PRIVATE_EXPONENT]))
		return SSL_BIGNUM_ERROR_INVALID_ARGUMENT;
	state->ready = 1;
	state->modes = SSL_RSA_MODE_PRIVATE;
	if ((modes & SSL_RSA_MODE_PRIVATE) == 0)
		return SSL_BIGNUM_ERROR_NOT_FOUND;
	return status;
}

int sslRsaPrivateReciprocalModExp(SslBigNum *r, SslBigNum *a, void *context)
{
	SslRsaKeyState *state = context;
	SslBigNumReciprocal recp;
	int status;

	if (!state->ready) {
		status = sslPrepareRsaReciprocalPrivateKey(state,
							   SSL_RSA_MODE_PRIVATE);
		if (status != 0)
			return status;
	}
	sslInitBigNumReciprocal(&recp, state->dmem);
	sslSetBigNumReciprocal(&recp, &state->number[SSL_RSA_NUMBER_MODULUS],
			       &state->ctx);
	status = sslModExpBigNumReciprocalSimple(
		r, a, &state->number[SSL_RSA_NUMBER_PRIVATE_EXPONENT], &recp,
		&state->ctx);
	sslFreeBigNumReciprocal(&recp, 0);
	return status;
}
