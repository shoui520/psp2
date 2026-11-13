/* Filename and descriptive names are ours: CRYPTOCME's raw RSA, the key's
 * exponentiation on the octet string as it is, without padding.
 */
#include "cry_api.h"
#include "ll_rsa.h"

static int sslRsaRawOperate(SslLowLevelObject *object, void *params,
			    int command);

static SslLowLevelMethodGetter sslRsaRawPrivateDecryptMethods[] = {
	sslGetRsaRawMethod,
	sslGetRsaBlindingMethod,
	sslGetRsaPrivateReciprocalMethod,
	NULL
};

static SslLowLevelMethodGetter sslRsaRawPublicEncryptMethods[] = {
	sslGetRsaRawMethod,
	sslGetRsaPublicReciprocalMethod,
	NULL
};

static SslLowLevelMethod sslRsaRawMethod = {
	sslRsaRawOperate,
	sslLowLevelNullControl,
	sslForwardLowLevelSetInfo,
	sslForwardLowLevelGetInfo,
	{ NULL }
};

const SslLowLevelMethodGetter *sslGetRsaRawPrivateDecryptMethods(void)
{
	return sslRsaRawPrivateDecryptMethods;
}

const SslLowLevelMethodGetter *sslGetRsaRawPublicEncryptMethods(void)
{
	return sslRsaRawPublicEncryptMethods;
}

const SslLowLevelMethod *sslGetRsaRawMethod(void)
{
	return &sslRsaRawMethod;
}

static int sslRsaRawOperate(SslLowLevelObject *object, void *params,
			    int command)
{
	SslLowLevelCipherParams *cipher = params;
	SceUInt32 length;
	SslBigNumContext *ctx;
	SslLowLevelBigNumParams numbers;
	SslBigNum *output;
	int status;

	if ((command & (SSL_LL_OPERATE_ENCRYPT | SSL_LL_OPERATE_DECRYPT)) == 0 ||
	    (command & SSL_LL_OPERATE_CLASS_MASK) != SSL_LL_OPERATE_CIPHER)
		return SSL_CRYPTO_ERROR_BAD_COMMAND;
	ctx = NULL;
	status = sslNewBigNumContext(&ctx, object->dmem);
	if (status != 0)
		goto done;
	status = sslInitLowLevelBigNumParams(&numbers, cipher->input,
					     cipher->inputLength, ctx,
					     cipher->options);
	if (status != 0)
		goto done;
	status = object->next->method->Operate(
		object->next, &numbers,
		(command | SSL_LL_OPERATE_BIG_NUMBERS) & ~SSL_LL_OPERATE_CIPHER);
	if (status != 0)
		goto done;
	output = numbers.output;
	length = (sslGetBigNumBitLength(output) + 7) / 8;
	if (length > cipher->outputMax) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		goto done;
	}
	*cipher->outputLength = length;
	status = sslGetBigNumData(&length, cipher->output, length, output, ctx);
	if (status != 0)
		goto done;
	status = 0;
done:
	sslFreeBigNumContext(ctx, 0);
	return status;
}
