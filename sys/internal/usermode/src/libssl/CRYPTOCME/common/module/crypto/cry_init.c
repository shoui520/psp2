/* Descriptive helper name is ours; source filename is recovered. */
#include "cry_api.h"
#include "../../property.h"
#include "../../../../memory.h"
#include "../../../../runtime.h"

static const char algorithmSource[] =
	"S:\\work\\CRYPTOCME\\common\\module/crypto/cry_init.c";

int sslFreeAlgorithm(SslCryptoAlgorithm *algorithm)
{
	int status = 0;

	if (algorithm != NULL &&
	    !(algorithm->flags & SSL_CRYPTO_ALGORITHM_DESTROYING) &&
	    sslAddReference(&algorithm->references, -1, SSL_CRYPTO_LOCK,
			    algorithmSource, 282) == 0) {
		const SslCryptoImplementationMethods *methods =
			algorithm->implementationMethods;
		SslObjectHeader *context;

		if (methods == NULL) {
			status = SSL_CRYPTO_ERROR_NO_METHODS;
		} else if (methods->Destroy == NULL) {
			status = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
		} else {
			algorithm->flags |= SSL_CRYPTO_ALGORITHM_DESTROYING;
			status = methods->Destroy(algorithm);
		}
		if (algorithm->properties != NULL &&
		    (algorithm->flags & SSL_CRYPTO_ALGORITHM_OWNS_PROPERTIES)) {
			sslFreePropertyList(algorithm->properties);
			algorithm->properties = NULL;
		}
		context = algorithm->context;
		if (context != NULL && context->methods != NULL &&
		    context->methods->Destroy != NULL)
			context->methods->Destroy(context);
		sslFree(algorithm);
	}
	return status;
}

int sslBindAlgorithm(SslCryptoAlgorithm **result)
{
	SslCryptoAlgorithm *algorithm = *result;
	const SslCryptoAlgorithmDescriptor *descriptor = NULL;
	const SslCryptoImplementationMethods *methods;
	SslCryptoContextHeader *context;
	void *data;
	int status;

	if (algorithm->implementationMethods != NULL)
		return 0;
	context = algorithm->context;
	status = sslFindAlgorithmDescriptor(context->algorithms,
					    algorithm->family, algorithm->algorithm,
					    algorithm->mode, &descriptor);
	if (status != 0) {
		context = (*result)->context;
		if (context != NULL && context->methods != NULL &&
		    context->methods->DestroyContext != NULL)
			context->methods->DestroyContext(context);
		sslFree(*result);
		*result = NULL;
		return status;
	}
	if (descriptor->GetMethods == NULL)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	methods = descriptor->GetMethods();
	(*result)->implementationMethods = methods;
	data = descriptor->data;
	(*result)->implementationData = data;
	algorithm = *result;
	methods = algorithm->implementationMethods;
	if (methods->Initialize != NULL)
		status = methods->Initialize(algorithm);
	return status;
}

int sslCloneAlgorithm(SslCryptoAlgorithm *algorithm, SslCryptoAlgorithm **result)
{
	SslCryptoContextHeader *context;
	void *applicationData = NULL;
	int family;
	int identifier;
	SceUInt32 mode;
	SceUInt32 flags;
	int status;

	status = sslGetAlgorithmInfo(algorithm, SSL_CRYPTO_INFO_FAMILY, &family);
	if (status != 0)
		return status;
	status = sslGetAlgorithmInfo(algorithm, SSL_CRYPTO_INFO_ALGORITHM,
				     &identifier);
	if (status != 0)
		return status;
	status = sslGetAlgorithmInfo(algorithm, SSL_CRYPTO_INFO_MODE, &mode);
	if (status != 0)
		return status;
	status = sslGetAlgorithmInfo(algorithm, SSL_CRYPTO_INFO_FLAGS, &flags);
	if (status != 0)
		return status;
	mode |= SSL_CRYPTO_MODE_COPY;
	status = sslGetAlgorithmInfo(algorithm, SSL_CRYPTO_INFO_APPLICATION_DATA,
				     &applicationData);
	if (status != 0)
		return status;
	if (result == NULL)
		return SSL_CRYPTO_ERROR_NULL_OUTPUT;
	if (algorithm->implementationMethods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (algorithm->implementationMethods->Copy == NULL)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	context = algorithm->context;
	if (context == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (context->methods == NULL)
		return SSL_CRYPTO_ERROR_NO_METHODS;
	if (context->methods->CreateAlgorithm == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	status = context->methods->CreateAlgorithm(context, family, identifier,
						   mode, result);
	if (status != 0)
		return status;
	status = sslSetAlgorithmInfo(*result, SSL_CRYPTO_INFO_APPLICATION_DATA,
				     (SceUIntPtr)applicationData);
	if (status != 0)
		goto error;
	status = sslSetAlgorithmInfo(*result, SSL_CRYPTO_INFO_FLAGS, flags);
	if (status != 0)
		goto error;
	status = algorithm->implementationMethods->Copy(algorithm, *result);
	if (status == 0)
		return 0;
error:
	sslDestroyAlgorithm(*result);
	*result = NULL;
	return status;
}
