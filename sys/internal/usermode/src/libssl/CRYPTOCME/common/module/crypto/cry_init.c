/* Descriptive helper name is ours; source filename is recovered. */
#include "ctx.h"
#include "../../property.h"
#include "../../../../memory.h"
#include "../../../../runtime.h"

static const char algorithmSource[] =
	"S:\\work\\CRYPTOCME\\common\\module/crypto/cry_init.c";

int sslRetainCryptoContext(void *object)
{
	SslCryptoContext *context = object;
	SslCryptoAlgorithm *random;

	if (context == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	sslAddReference(&context->references, 1, SSL_CRYPTO_LOCK,
			algorithmSource, 55);
	random = context->randomAlgorithm;
	if (random != NULL)
		sslAddReference(&random->references, 1, SSL_CRYPTO_LOCK,
				algorithmSource, 58);
	return 0;
}

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

int sslNewAlgorithm(void *object, int family, int identifier, SceUInt32 mode,
		     SslCryptoAlgorithm **result)
{
	SslCryptoContext *context = object;
	SslCryptoAlgorithm *algorithm = NULL;
	int status;

	if (context == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	mode &= ~SSL_CRYPTO_MODE_COPY;
	if (mode & SSL_CRYPTO_MODE_UNSUPPORTED)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	algorithm = sslAllocate(sizeof(*algorithm));
	if (algorithm == NULL)
		return SSL_LIBRARY_ERROR_ALLOCATION;
	sslFillMemory(algorithm, 0, sizeof(*algorithm));
	algorithm->references = 1;
	algorithm->state = NULL;
	algorithm->methods = context->interface.methods;
	algorithm->family = family;
	algorithm->algorithm = identifier;
	algorithm->mode = mode;
	algorithm->flags = 0;
	algorithm->applicationData = NULL;
	sslRetainCryptoContext(context);
	algorithm->context = context;
	algorithm->backendState = NULL;
	status = sslBindAlgorithm(&algorithm);
	if (status == 0)
		*result = algorithm;
	else if (algorithm != NULL)
		sslFreeAlgorithm(algorithm);
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

int sslVerifyMethodInit(SslCryptoAlgorithm *algorithm, void *key)
{
	int (*init)(SslCryptoAlgorithm *, void *, int);
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (key != NULL) {
		status = SSL_LIBRARY_ERROR_SELECTOR;
		if (algorithm->family == 6) {
			status = SSL_CRYPTO_ERROR_NO_METHODS;
			if (algorithm->implementationMethods != NULL) {
				int callStatus =
					SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

				init = algorithm->implementationMethods->Init;
				if (init != NULL)
					callStatus = init(algorithm, key, 0);
				status = callStatus;
			}
		}
	}
	return status;
}

int sslVerifyMethodVerify(SslCryptoAlgorithm *algorithm, const void *digest,
			  SceUInt32 digestLength, const void *signature,
			  SceUInt32 signatureLength, int *result)
{
	int (*verify)(SslCryptoAlgorithm *, const void *, SceUInt32,
		      const void *, SceUInt32, int *);
	int status;

	if (signature == NULL || digest == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (digestLength == 0 || signatureLength == 0) {
		status = 10022;
	} else if (algorithm->family == 6 && algorithm->mode == 4) {
		status = SSL_CRYPTO_ERROR_NO_METHODS;
		if (algorithm->implementationMethods != NULL) {
			int callStatus = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

			verify = algorithm->implementationMethods->Verify;
			if (verify != NULL)
				callStatus = verify(algorithm, digest,
						    digestLength, signature,
						    signatureLength, result);
			status = callStatus;
		}
	} else {
		status = SSL_LIBRARY_ERROR_SELECTOR;
	}
	return status;
}

int sslVerifyMethodUpdate(SslCryptoAlgorithm *algorithm, const void *data,
			  SceUInt32 length)
{
	int (*update)(SslCryptoAlgorithm *, const void *, SceUInt32);
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (data != NULL) {
		status = 10022;
		if (length != 0) {
			if (algorithm->family == 6 && algorithm->mode == 4) {
				int innerStatus = SSL_CRYPTO_ERROR_NO_METHODS;

				if (algorithm->implementationMethods != NULL) {
					int callStatus =
						SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

					update = algorithm->implementationMethods->VerifyUpdate;
					if (update != NULL)
						callStatus = update(algorithm,
								    data,
								    length);
					innerStatus = callStatus;
				}
				status = innerStatus;
			} else {
				status = SSL_LIBRARY_ERROR_SELECTOR;
			}
		}
	}
	return status;
}

int sslVerifyMethodFinal(SslCryptoAlgorithm *algorithm, const void *signature,
			 SceUInt32 length, int *result)
{
	int (*finalOp)(SslCryptoAlgorithm *, const void *, SceUInt32, int *);
	int status;

	if (signature == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	status = 10022;
	if (length != 0) {
		if (algorithm->family == 6 && algorithm->mode == 4) {
			int innerStatus = SSL_CRYPTO_ERROR_NO_METHODS;

			if (algorithm->implementationMethods != NULL) {
				int callStatus =
					SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

				finalOp = algorithm->implementationMethods->VerifyFinal;
				if (finalOp != NULL)
					callStatus = finalOp(algorithm,
							     signature, length,
							     result);
				innerStatus = callStatus;
			}
			status = innerStatus;
		} else {
			status = SSL_LIBRARY_ERROR_SELECTOR;
		}
	}
	return status;
}
