/* Filename and descriptive names are ours: CRYPTOCME's PKCS #1 v1.5 block
 * encoding (RFC 2313 8.1, 9.4): 00 || BT || PS || 00 || D, block type 1
 * for signatures (PS of 0xFF) and 2 for encryption (PS random, nonzero).
 */
#include "cry_api.h"
#include "ll_rsa.h"
#include "ll_rand.h"
#include "../dmem/dmem.h"
#include "../../../../memory.h"

/* The shortest padding string */
#define SSL_RSA_PKCS1_MIN_PADDING 8
/* 00 || BT || PS || 00 */
#define SSL_RSA_PKCS1_OVERHEAD (SSL_RSA_PKCS1_MIN_PADDING + 3)

static const SslRsaPadding sslRsaPkcs1Type1 = { 1, 0xff };
static const SslRsaPadding sslRsaPkcs1Type2 = { 2, 0 };

static SslLowLevelMethod sslRsaPkcs1Type1DecodeMethod = {
	sslRsaPkcs1DecodeOperate,
	sslRsaPaddingControl,
	sslSetRsaPaddingInfo,
	sslForwardLowLevelGetInfo,
	{ .parameters = &sslRsaPkcs1Type1 }
};

static SslLowLevelMethod sslRsaPkcs1Type1EncodeMethod = {
	sslRsaPkcs1EncodeOperate,
	sslRsaPaddingControl,
	sslSetRsaPaddingInfo,
	sslForwardLowLevelGetInfo,
	{ .parameters = &sslRsaPkcs1Type1 }
};

static SslLowLevelMethod sslRsaPkcs1Type2DecodeMethod = {
	sslRsaPkcs1DecodeOperate,
	sslRsaPaddingControl,
	sslSetRsaPaddingInfo,
	sslForwardLowLevelGetInfo,
	{ .parameters = &sslRsaPkcs1Type2 }
};

static SslLowLevelMethod sslRsaPkcs1Type2EncodeMethod = {
	sslRsaPkcs1EncodeOperate,
	sslRsaPaddingControl,
	sslSetRsaPaddingInfo,
	sslForwardLowLevelGetInfo,
	{ .parameters = &sslRsaPkcs1Type2 }
};

const SslLowLevelMethod *sslGetRsaPkcs1Type1DecodeMethod(void)
{
	return &sslRsaPkcs1Type1DecodeMethod;
}

const SslLowLevelMethod *sslGetRsaPkcs1Type1EncodeMethod(void)
{
	return &sslRsaPkcs1Type1EncodeMethod;
}

const SslLowLevelMethod *sslGetRsaPkcs1Type2DecodeMethod(void)
{
	return &sslRsaPkcs1Type2DecodeMethod;
}

const SslLowLevelMethod *sslGetRsaPkcs1Type2EncodeMethod(void)
{
	return &sslRsaPkcs1Type2EncodeMethod;
}

/* The object keeps its state pointer after FREE */
int sslRsaPaddingControl(SslLowLevelObject *object, int command, int arg,
			 void *value)
{
	SslRsaPaddingState *state;
	int status = 0;

	switch (command) {
	case SSL_LL_CONTROL_INIT:
		status = sslAllocateDirectMemory((void **)&state, sizeof(*state),
						 object->dmem,
						 SSL_DMEM_ALLOCATE_ZERO);
		if (status == 0)
			object->state = state;
		break;
	case SSL_LL_CONTROL_FREE:
		state = object->state;
		if (state != NULL) {
			if (state->scratch != NULL)
				sslFreeDirectMemory(state->scratch,
						    object->dmem);
			sslFreeDirectMemory(state, object->dmem);
		}
		break;
	}
	return status;
}

/* Takes the PRNG and the length of n, and passes everything on */
int sslSetRsaPaddingInfo(SslLowLevelObject *object, int type, int id,
			 const void *value)
{
	SslRsaPaddingState *state = object->state;
	const SslLowLevelBuffer *buffer;
	const unsigned char *data;
	SceUInt32 i;
	SslLowLevelObject *next;

	switch (type) {
	case SSL_LL_INFO_GENERAL:
		if (id == SSL_LL_INFO_RANDOM)
			state->random = (void *)value;
		break;
	case SSL_LL_INFO_NUMBER:
		if (id == SSL_LL_INFO_NUMBER_FIRST + SSL_RSA_NUMBER_MODULUS) {
			buffer = value;
			state->modulusLength = buffer->length;
			data = buffer->data;
			for (i = 0; i < buffer->length; i++, data++) {
				if (*data != 0)
					break;
			}
			state->modulusLength = buffer->length - i;
		}
		break;
	}
	next = object->next;
	if (next != NULL)
		return next->method->SetInfo(next, type, id, value);
	return 0;
}

/* Decrypts with the next layer and takes the data out of the block */
int sslRsaPkcs1DecodeOperate(SslLowLevelObject *object, void *params,
			     int command)
{
	const SslRsaPadding *padding = object->method->extension.parameters;
	SslRsaPaddingState *state = object->state;
	SslLowLevelCipherParams *cipher = params;
	SslLowLevelBigNumParams numbers;
	SslBigNumContext *ctx;
	unsigned char *allocated;
	unsigned char *block, *end, *start, *p, *out;
	SceUInt32 length;
	int status = 0;
	int i;

	if ((command & SSL_LL_OPERATE_DECRYPT) == 0 ||
	    (command & SSL_LL_OPERATE_CLASS_MASK) != SSL_LL_OPERATE_CIPHER)
		return SSL_CRYPTO_ERROR_BAD_COMMAND;
	allocated = NULL;
	ctx = NULL;
	status = sslNewBigNumContext(&ctx, object->dmem);
	if (status != 0)
		goto done;
	numbers.ctx = ctx;
	numbers.input = &ctx->bn[ctx->tos];
	numbers.output = &ctx->bn[ctx->tos + 1];
	ctx->tos += 2;
	numbers.options = cipher->options;
	status = sslSetBigNumData(numbers.input, cipher->input,
				  cipher->inputLength, ctx);
	if (status != 0)
		goto done;
	status = object->next->method->Operate(
		object->next, &numbers,
		(command & ~(SSL_LL_OPERATE_CIPHER | SSL_LL_OPERATE_BIG_NUMBERS)) |
			SSL_LL_OPERATE_BIG_NUMBERS);
	if (status != 0)
		goto done;
	length = (sslGetBigNumBitLength(numbers.output) + 7) / 8;
	if (length > cipher->outputMax) {
		status = sslAllocateDirectMemory((void **)&block, length,
						 object->dmem, 0);
		if (status != 0)
			goto done;
		allocated = block;
	} else {
		block = cipher->output;
	}
	status = sslGetBigNumData(&length, block, length, numbers.output, ctx);
	if (status != 0)
		goto done;
	/* The leading zero is gone with the conversion */
	if (length != state->modulusLength - 1 ||
	    length < SSL_RSA_PKCS1_OVERHEAD - 1)
		goto invalid;
	end = block + length;
	if (block[0] != padding->blockType)
		goto invalid;
	start = block + 1;
	for (p = start; p < end && *p != 0; p++) {
		if ((*p & padding->mask) != padding->mask)
			goto invalid;
	}
	if (p - start < SSL_RSA_PKCS1_MIN_PADDING || p == end)
		goto invalid;
	if (cipher->options & SSL_RSA_PADDING_ROLLBACK) {
		/* A client that can do better fell back to SSLv2 */
		state->flags &= ~SSL_RSA_PADDING_ROLLBACK_FOUND;
		for (i = -8; i < 0; i++) {
			if (p[i] != 3)
				break;
		}
		if (i == 0) {
			state->flags |= SSL_RSA_PADDING_ROLLBACK_FOUND;
			goto invalid;
		}
	}
	p++;
	length = end - p;
	if (length > cipher->outputMax) {
		status = SSL_BIGNUM_ERROR_BUFFER_TOO_SMALL;
		goto done;
	}
	*cipher->outputLength = length;
	out = cipher->output;
	while (p != end)
		*out++ = *p++;
	status = 0;
	goto done;
invalid:
	status = SSL_CRYPTO_ERROR_NOT_FOUND;
done:
	sslFreeDirectMemory(allocated, object->dmem);
	sslFreeBigNumContext(ctx, 0);
	return status;
}

/* Builds the block in the output and encrypts it with the next layer */
int sslRsaPkcs1EncodeOperate(SslLowLevelObject *object, void *params,
			     int command)
{
	const SslRsaPadding *padding = object->method->extension.parameters;
	SslRsaPaddingState *state = object->state;
	SslLowLevelCipherParams *cipher = params;
	SslLowLevelBigNumParams numbers;
	SslBigNumContext *ctx;
	const unsigned char *input;
	unsigned char *block, *fill;
	SceUInt32 inputLength, generated, length;
	int count, i;
	int status;

	if ((command & SSL_LL_OPERATE_ENCRYPT) == 0 ||
	    (command & SSL_LL_OPERATE_CLASS_MASK) != SSL_LL_OPERATE_CIPHER)
		return SSL_CRYPTO_ERROR_BAD_COMMAND;
	ctx = NULL;
	inputLength = cipher->inputLength;
	if (inputLength + SSL_RSA_PKCS1_OVERHEAD > state->modulusLength)
		return SSL_CRYPTO_ERROR_NOT_INITIALIZED;
	if (cipher->outputMax < state->modulusLength)
		return SSL_BIGNUM_ERROR_BUFFER_TOO_SMALL;
	/* The data goes to the end of the block, last byte first: the input
	 * may be the output
	 */
	input = cipher->input;
	block = (unsigned char *)cipher->output + state->modulusLength -
		inputLength;
	for (i = inputLength - 1; i >= 0; i--)
		block[i] = input[i];
	block = cipher->output;
	block[0] = 0;
	block[1] = padding->blockType;
	fill = block + 2;
	count = state->modulusLength - inputLength - 3;
	if (padding->blockType == 1) {
		sslFillMemory(fill, 0xff, count);
	} else {
		if (state->random == NULL)
			return SSL_BIGNUM_ERROR_NO_PRNG;
		if (cipher->options & SSL_RSA_PADDING_ROLLBACK) {
			for (i = count - 8; i < count; i++)
				fill[i] = 3;
			count -= 8;
		}
		sslGenerateLowLevelPrng(state->random, fill, &generated,
					count);
		for (i = 0; i < count; i++) {
			if (fill[i] == 0)
				fill[i] = 0xff;
		}
		if (cipher->options & SSL_RSA_PADDING_ROLLBACK)
			count += 8;
	}
	block[count + 2] = 0;
	status = sslNewBigNumContext(&ctx, object->dmem);
	if (status == 0) {
		numbers.ctx = ctx;
		numbers.input = &ctx->bn[ctx->tos];
		numbers.output = &ctx->bn[ctx->tos + 1];
		ctx->tos += 2;
		numbers.options = cipher->options;
		status = sslSetBigNumData(numbers.input, cipher->output,
					  state->modulusLength, ctx);
		if (status == 0) {
			status = object->next->method->Operate(
				object->next, &numbers,
				(command & ~(SSL_LL_OPERATE_CIPHER |
					     SSL_LL_OPERATE_BIG_NUMBERS)) |
					SSL_LL_OPERATE_BIG_NUMBERS);
			if (status == 0) {
				status = sslGetBigNumData(
					&length, cipher->output,
					cipher->outputMax, numbers.output, ctx);
				if (status == 0)
					*cipher->outputLength = length;
			}
		}
	}
	sslFreeBigNumContext(ctx, 0);
	return status;
}
