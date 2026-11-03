/* Filename and helper name are ours; recovered CRYPTOCME dispatch behavior. */
#include "cry_api.h"

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
