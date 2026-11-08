/* Filename and helper name are ours; recovered CRYPTOCME dispatch behavior. */
#include "cry_api.h"

int sslCreateCryptoContext(SslLibrary *library, int mode, void **result)
{
	SceUIntPtr factory = 0;
	void *data;
	const SslCryptoLifecycleMethods *methods;
	const SslCryptoLifecycleMethods *(*getMethods)(void *data);
	int status;

	if (library == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	status = sslGetLibraryMethod(library, SSL_CRYPTO_MODULE_TYPE, 0, 0,
				     mode, SSL_LIBRARY_GET_METHOD_FACTORY, &factory);
	if (status != 0)
		return status;
	if (factory == 0)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	getMethods = (const SslCryptoLifecycleMethods *(*)(void *))factory;
	status = sslGetLibraryData(library, SSL_CRYPTO_MODULE_TYPE, &data);
	if (status != 0)
		return status;
	methods = getMethods(data);
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	return methods->CreateContext(library, mode, result);
}

int sslDestroyCryptoContext(void *context)
{
	SslObjectHeader *object = context;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (object->methods->Destroy == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->methods->Destroy(context);
}

int sslCreateAlgorithm(SslCryptoContextHeader *context, int family, int algorithm,
		       SceUInt32 mode, SslCryptoAlgorithm **result)
{
	const SslCryptoLifecycleMethods *methods;

	if (context == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = context->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->CreateAlgorithm == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->CreateAlgorithm(context, family, algorithm, mode, result);
}

int sslCopyAlgorithm(SslCryptoAlgorithm *algorithm, SslCryptoAlgorithm **result)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->CopyAlgorithm == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->CopyAlgorithm(algorithm, result);
}

int sslDestroyAlgorithm(SslCryptoAlgorithm *algorithm)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->DestroyAlgorithm == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->DestroyAlgorithm(algorithm);
}

int sslGetAlgorithmInfo(SslCryptoAlgorithm *algorithm, int selector, void *result)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->GetAlgorithmInfo == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;

	switch (selector) {
	case SSL_CRYPTO_INFO_APPLICATION_DATA:
		*(void **)result = algorithm->applicationData;
		break;
	case SSL_CRYPTO_INFO_FAMILY:
		*(int *)result = algorithm->family;
		break;
	case SSL_CRYPTO_INFO_ALGORITHM:
		*(int *)result = algorithm->algorithm;
		break;
	case SSL_CRYPTO_INFO_MODE:
		*(SceUInt32 *)result = algorithm->mode;
		break;
	case SSL_CRYPTO_INFO_FLAGS:
		*(SceUInt32 *)result = algorithm->flags;
		break;
	default:
		return methods->GetAlgorithmInfo(algorithm, selector, result);
	}
	return 0;
}

int sslSetAlgorithmInfo(SslCryptoAlgorithm *algorithm, int selector,
			SceUIntPtr value)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->SetAlgorithmInfo == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	if (selector == SSL_CRYPTO_INFO_APPLICATION_DATA) {
		algorithm->applicationData = (void *)value;
		return 0;
	}
	if (selector == SSL_CRYPTO_INFO_FLAGS) {
		algorithm->flags = value;
		return 0;
	}
	return methods->SetAlgorithmInfo(algorithm, selector, value);
}

int sslEncryptInit(SslCryptoAlgorithm *algorithm, void *key, const void *iv)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->EncryptInit == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->EncryptInit(algorithm, key, iv);
}

int sslEncryptUpdate(SslCryptoAlgorithm *algorithm, const void *input,
		     SceUInt32 inputLength, void *output, SceUInt32 *outputLength)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->EncryptUpdate == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->EncryptUpdate(algorithm, input, inputLength, output,
				      outputLength);
}

int sslDecryptInit(SslCryptoAlgorithm *algorithm, void *key, const void *iv)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->DecryptInit == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->DecryptInit(algorithm, key, iv);
}

int sslDecryptUpdate(SslCryptoAlgorithm *algorithm, const void *input,
		     SceUInt32 inputLength, void *output, SceUInt32 *outputLength)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->DecryptUpdate == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->DecryptUpdate(algorithm, input, inputLength, output,
				      outputLength);
}

int sslSignInit(SslCryptoAlgorithm *algorithm, void *key)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->SignInit == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->SignInit(algorithm, key);
}

int sslSign(SslCryptoAlgorithm *algorithm, const void *digest,
	    SceUInt32 digestLength, void *signature, SceUInt32 *signatureLength)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->Sign == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->Sign(algorithm, digest, digestLength, signature,
			     signatureLength);
}

int sslVerifyInit(SslCryptoAlgorithm *algorithm, void *key)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->VerifyInit == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->VerifyInit(algorithm, key);
}

int sslVerifyUpdate(SslCryptoAlgorithm *algorithm, const void *data, SceUInt32 length)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->VerifyUpdate == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->VerifyUpdate(algorithm, data, length);
}

int sslVerifyFinal(SslCryptoAlgorithm *algorithm, const void *signature,
		   SceUInt32 length, int *result)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->VerifyFinal == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->VerifyFinal(algorithm, signature, length, result);
}

int sslVerify(SslCryptoAlgorithm *algorithm, const void *digest,
	      SceUInt32 digestLength, const void *signature,
	      SceUInt32 signatureLength, int *result)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->Verify == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->Verify(algorithm, digest, digestLength, signature,
			       signatureLength, result);
}

int sslAsymEncryptInit(SslCryptoAlgorithm *algorithm, void *key)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->AsymEncryptInit == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->AsymEncryptInit(algorithm, key);
}

int sslAsymEncrypt(SslCryptoAlgorithm *algorithm, const void *input,
		   SceUInt32 inputLength, void *output, SceUInt32 *outputLength)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->AsymEncrypt == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->AsymEncrypt(algorithm, input, inputLength, output,
				    outputLength);
}

int sslAsymDecryptInit(SslCryptoAlgorithm *algorithm, void *key)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->AsymDecryptInit == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->AsymDecryptInit(algorithm, key);
}

int sslAsymDecrypt(SslCryptoAlgorithm *algorithm, const void *input,
		   SceUInt32 inputLength, void *output, SceUInt32 *outputLength)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->AsymDecrypt == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->AsymDecrypt(algorithm, input, inputLength, output,
				    outputLength);
}

int sslDigestInit(SslCryptoAlgorithm *algorithm)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->DigestInit == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->DigestInit(algorithm);
}

int sslDigestUpdate(SslCryptoAlgorithm *algorithm, const void *data, SceUInt32 length)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->DigestUpdate == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->DigestUpdate(algorithm, data, length);
}

int sslDigestFinal(SslCryptoAlgorithm *algorithm, void *data, SceUInt32 *length)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->DigestFinal == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->DigestFinal(algorithm, data, length);
}

int sslDigest(SslCryptoAlgorithm *algorithm, const void *data, SceUInt32 length,
	      void *digest, SceUInt32 *digestLength)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->Digest == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->Digest(algorithm, data, length, digest, digestLength);
}

int sslMacInit(SslCryptoAlgorithm *algorithm, void *key)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->MacInit == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->MacInit(algorithm, key);
}

int sslMacUpdate(SslCryptoAlgorithm *algorithm, const void *data, SceUInt32 length)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->MacUpdate == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->MacUpdate(algorithm, data, length);
}

int sslMacFinal(SslCryptoAlgorithm *algorithm, void *mac, SceUInt32 *length)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->MacFinal == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->MacFinal(algorithm, mac, length);
}

int sslKeyGenInit(SslCryptoAlgorithm *algorithm)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->KeyGenInit == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->KeyGenInit(algorithm);
}

int sslGenerateKey(SslCryptoAlgorithm *algorithm, void **key)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->GenerateKey == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->GenerateKey(algorithm, key);
}

int sslSeedRandom(SslCryptoAlgorithm *algorithm, const void *buffer,
		  SceUInt32 length)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->SeedRandom == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->SeedRandom(algorithm, buffer, length);
}

int sslGenerateRandom(SslCryptoAlgorithm *algorithm, SceUInt32 length,
		      void *buffer, SceUInt32 *generatedLength)
{
	const SslCryptoLifecycleMethods *methods;

	if (algorithm == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->GenerateRandom == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return methods->GenerateRandom(algorithm, length, buffer, generatedLength);
}

int sslUnavailableCryptoOperation(void)
{
	return 10010;
}
