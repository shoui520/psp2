/* Native implementation-info dispatch; filename and descriptive names are ours.
 * The source folder follows the recovered crypto/cry_init.c path.
 */
#include "cry_api.h"

int sslGetImplementationInfo(SslCryptoAlgorithm *algorithm, int selector,
			     void *result)
{
	const SslCryptoImplementationMethods *methods;

	if (algorithm == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->implementationMethods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->GetInfo == NULL)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	return methods->GetInfo(algorithm, selector, result);
}

int sslSetImplementationInfo(SslCryptoAlgorithm *algorithm, int selector,
			     SceUIntPtr value)
{
	const SslCryptoImplementationMethods *methods;

	if (algorithm == NULL || value == 0)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = algorithm->implementationMethods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (methods->SetInfo == NULL)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	return methods->SetInfo(algorithm, selector, value);
}
