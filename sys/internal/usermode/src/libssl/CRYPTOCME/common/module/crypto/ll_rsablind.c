/* Filename and descriptive names are ours: CRYPTOCME's RSA method chains and
 * the blinding layer, which hides the private key operation's input from
 * timing (Kocher): x * A before the exponentiation, y * Ai after it.
 */
#include "cry_api.h"
#include "ll_rsa.h"
#include "../dmem/dmem.h"

static SslLowLevelMethodGetter sslRsaPkcs1SignChain[] = {
	sslGetPkcs1SignMethod,
	sslGetRsaPkcs1Type1EncodeMethod,
	sslGetRsaBlindingMethod,
	sslGetRsaPrivateMontgomeryMethod,
	NULL
};

static SslLowLevelMethodGetter sslRsaPkcs1VerifyChain[] = {
	sslGetPkcs1VerifyMethod,
	sslGetRsaPkcs1Type1DecodeMethod,
	sslGetRsaPublicMontgomeryMethod,
	NULL
};

static SslLowLevelMethodGetter sslRsaPkcs1EncryptChain[] = {
	sslGetRsaPkcs1Type2EncodeMethod,
	sslGetRsaPublicReciprocalMethod,
	NULL
};

static SslLowLevelMethodGetter sslRsaPkcs1DecryptChain[] = {
	sslGetRsaPkcs1Type2DecodeMethod,
	sslGetRsaBlindingMethod,
	sslGetRsaPrivateReciprocalMethod,
	NULL
};

static SslLowLevelMethod sslRsaBlindingMethod = {
	sslRsaBlindingOperate,
	sslRsaBlindingControl,
	sslSetRsaBlindingInfo,
	sslGetRsaBlindingInfo,
	{ NULL }
};

const SslLowLevelMethodGetter *sslGetRsaPkcs1SignChain(void)
{
	return sslRsaPkcs1SignChain;
}

const SslLowLevelMethodGetter *sslGetRsaPkcs1VerifyChain(void)
{
	return sslRsaPkcs1VerifyChain;
}

const SslLowLevelMethodGetter *sslGetRsaPkcs1EncryptChain(void)
{
	return sslRsaPkcs1EncryptChain;
}

const SslLowLevelMethodGetter *sslGetRsaPkcs1DecryptChain(void)
{
	return sslRsaPkcs1DecryptChain;
}

const SslLowLevelMethod *sslGetRsaBlindingMethod(void)
{
	return &sslRsaBlindingMethod;
}

int sslRsaBlindingControl(SslLowLevelObject *object, int command, int arg,
			  void *value)
{
	SslRsaBlindingState *state;
	int status = 0;

	switch (command) {
	case SSL_LL_CONTROL_INIT:
		status = sslAllocateDirectMemory((void **)&state, sizeof(*state),
						 object->dmem,
						 SSL_DMEM_ALLOCATE_ZERO);
		if (status == 0) {
			object->state = state;
			sslInitBigNumContext(&state->ctx, object->dmem);
			sslInitBigNum(&state->blind, object->dmem);
			sslInitBigNum(&state->unblind, object->dmem);
			sslInitBigNum(&state->modulus, object->dmem);
			sslInitBigNum(&state->exponent, object->dmem);
			state->flags = SSL_RSA_BLINDING_ENABLED;
		}
		break;
	case SSL_LL_CONTROL_FREE:
		state = object->state;
		if (state != NULL) {
			sslFreeBigNumContext(&state->ctx, 0);
			sslFreeBigNum(&state->blind, 0);
			sslFreeBigNum(&state->unblind, 0);
			sslFreeBigNum(&state->modulus, 0);
			sslFreeBigNum(&state->exponent, 0);
			sslFreeDirectMemory(state, object->dmem);
		}
		object->state = NULL;
		break;
	}
	return status;
}

/* Takes the flags, a blinding value, n, e and the PRNG, and passes
 * everything on down the chain
 */
int sslSetRsaBlindingInfo(SslLowLevelObject *object, int type, int id,
			  const void *value)
{
	SslRsaBlindingState *state = object->state;
	const SslLowLevelBuffer *buffer = value;
	SslLowLevelObject *next;

	switch (type) {
	case SSL_LL_INFO_BLINDING:
		switch (id) {
		case SSL_LL_INFO_BLINDING_FLAGS:
			state->ready = 0;
			state->flags = *(const SceUInt32 *)value &
				       SSL_RSA_BLINDING_ENABLED;
			break;
		case SSL_LL_INFO_BLINDING_VALUE:
			sslSetBigNumData(&state->blind, buffer->data,
					 buffer->length, &state->ctx);
			state->ready = 0;
			break;
		}
		break;
	case SSL_LL_INFO_NUMBER:
		switch (id) {
		case SSL_LL_INFO_NUMBER_FIRST + SSL_RSA_NUMBER_PUBLIC_EXPONENT:
			sslSetBigNumData(&state->exponent, buffer->data,
					 buffer->length, &state->ctx);
			state->ready = 0;
			break;
		case SSL_LL_INFO_NUMBER_FIRST + SSL_RSA_NUMBER_MODULUS:
			sslSetBigNumData(&state->modulus, buffer->data,
					 buffer->length, &state->ctx);
			state->ready = 0;
			break;
		}
		break;
	case SSL_LL_INFO_GENERAL:
		if (id == SSL_LL_INFO_RANDOM)
			state->random = (void *)value;
		break;
	}
	next = object->next;
	if (next != NULL && next->method->SetInfo != NULL)
		return next->method->SetInfo(next, type, id, value);
	return 0;
}

int sslGetRsaBlindingInfo(SslLowLevelObject *object, int type, int id,
			  void *result)
{
	SslRsaBlindingState *state = object->state;
	SslLowLevelObject *next;

	if (type == SSL_LL_INFO_BLINDING) {
		if (id == SSL_LL_INFO_BLINDING_FLAGS) {
			if (result != NULL)
				*(SceUInt32 *)result =
					(state->active ?
						 SSL_RSA_BLINDING_APPLIED :
						 0) |
					state->flags;
			return 0;
		}
	} else {
		next = object->next;
		if (next != NULL && next->method->GetInfo != NULL)
			return next->method->GetInfo(next, type, id, result);
	}
	return SSL_CRYPTO_ERROR_UNHANDLED;
}

/* A = r^e mod n for a random r with an inverse Ai; without a PRNG, the
 * value given is stepped by 17 until it has one
 */
int sslPrepareRsaBlinding(SslLowLevelObject *object)
{
	SslRsaBlindingState *state = object->state;
	int status;

	if (sslIsBigNumZero(&state->exponent))
		return SSL_RSA_ERROR_NO_BLINDING_EXPONENT;
	do {
		if (sslIsBigNumZero(&state->blind)) {
			if (state->random == NULL)
				return SSL_BIGNUM_ERROR_NO_PRNG;
			sslRandomBigNum(&state->blind, state->random,
					sslGetBigNumBitLength(&state->modulus) - 1,
					1, 0, &state->ctx);
		}
		status = sslModInverseBigNum(&state->unblind, &state->blind,
					     &state->modulus, &state->ctx);
		if (status == 0)
			break;
		if (status == SSL_BIGNUM_ERROR_NO_INVERSE) {
			if (state->random != NULL) {
				status = sslRandomBigNum(
					&state->blind, state->random,
					sslGetBigNumBitLength(&state->modulus) -
						1,
					1, 0, &state->ctx);
			} else {
				state->ctx.error = 0;
				status = sslAddBigNumWord(&state->blind, 0, 17,
							  &state->ctx);
			}
		}
	} while (status != 0);
	return sslModExpBigNum(&state->blind, &state->blind, &state->exponent,
			       &state->modulus, &state->ctx);
}

/* Blinds the private key operations: decryption and signature */
int sslRsaBlindingOperate(SslLowLevelObject *object, void *params,
			  int command)
{
	SslRsaBlindingState *state = object->state;
	SslLowLevelBigNumParams *numbers = params;
	int blind = 0;
	int status;

	if ((command & SSL_LL_OPERATE_CLASS_MASK) != SSL_LL_OPERATE_BIG_NUMBERS)
		return SSL_CRYPTO_ERROR_BAD_COMMAND;
	if (state->flags & SSL_RSA_BLINDING_ENABLED) {
		if (command & SSL_LL_OPERATE_DECRYPT)
			blind = 1;
		else if (command & SSL_LL_OPERATE_SIGNATURE)
			blind = 1;
	}
	state->active = blind;
	if (blind) {
		if (!state->ready) {
			status = sslPrepareRsaBlinding(object);
			if (status != 0)
				return status;
		}
		status = sslModMultiplyBigNum(numbers->input, numbers->input,
					      &state->blind, &state->modulus,
					      &state->ctx);
		if (status != 0)
			return status;
	}
	status = object->next->method->Operate(object->next, params, command);
	if (status != 0)
		return status;
	if (!blind)
		return status;
	sslModMultiplyBigNum(numbers->output, numbers->output, &state->unblind,
			     &state->modulus, &state->ctx);
	sslModMultiplyBigNum(&state->blind, &state->blind, &state->blind,
			     &state->modulus, &state->ctx);
	sslModMultiplyBigNum(&state->unblind, &state->unblind,
			     &state->unblind, &state->modulus, &state->ctx);
	if (state->ctx.error != 0)
		return state->ctx.error;
	return status;
}
