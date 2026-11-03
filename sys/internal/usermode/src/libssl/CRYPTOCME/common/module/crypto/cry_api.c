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
