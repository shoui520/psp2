/* Source filename is recovered; descriptive private names are ours. */
#include "ctx.h"
#include "../../../../memory.h"
#include "../../../../runtime.h"

static const char contextSource[] =
	"S:\\work\\CRYPTOCME\\common\\module/crypto/ctx_init.c";

int sslFreeCryptoContext(void *object)
{
	SslCryptoContext *context = object;
	int references;

	if (context == NULL)
		return 0;
	references = sslAddReference(&context->references, -1, SSL_CRYPTO_LOCK,
				     contextSource, 186);
	if (references > 0) {
		if (context->randomAlgorithm != NULL)
			sslDestroyAlgorithm(context->randomAlgorithm);
	} else if (references == 0) {
		if (context->ownedStorage != NULL) {
			sslFree(context->ownedStorage);
			context->ownedStorage = NULL;
		}
		SceSslInternal_4725F9E7(context->interface.library);
		sslFree(context);
	}
	return 0;
}

int sslNewCryptoContext(void *libraryObject, int mode, void **result)
{
	SslLibrary *library = libraryObject;
	SslCryptoContext *context;
	const SslCryptoLifecycleMethods *(*getMethods)(void *data);
	const SslCryptoAlgorithmDescriptor *(*getAlgorithms)(void *data);
	void *data = NULL;
	SceUIntPtr factory;
	int status;

	if (library == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	context = sslAllocate(sizeof(*context));
	if (context == NULL)
		return SSL_LIBRARY_ERROR_ALLOCATION;
	sslFillMemory(context, 0, sizeof(*context));
	context->references = 1;
	status = sslGetLibraryMethod(library, SSL_CRYPTO_MODULE_TYPE, 0, 0,
				     mode, SSL_LIBRARY_GET_METHOD_FACTORY, &factory);
	if (status != 0)
		goto error;
	getMethods = (const SslCryptoLifecycleMethods *(*)(void *))factory;
	status = sslGetLibraryData(library, SSL_CRYPTO_MODULE_TYPE, &data);
	if (status != 0)
		goto error;
	context->interface.methods = getMethods(data);
	status = sslGetLibraryMethod(library, SSL_CRYPTO_MODULE_TYPE, 0, 0,
				     mode, SSL_LIBRARY_GET_IMPLEMENTATION_FACTORY,
				     &factory);
	if (status != 0)
		goto error;
	if (factory == 0) {
		status = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
		goto error;
	}
	getAlgorithms = (const SslCryptoAlgorithmDescriptor *(*)(void *))factory;
	context->interface.algorithms = getAlgorithms(data);
	status = sslLibraryAddReference(library);
	if (status != 0)
		goto error;
	context->interface.library = library;
	context->randomAlgorithm = NULL;
	context->entropySource = NULL;
	if (context->interface.methods->CreateAlgorithm(context,
			SSL_CRYPTO_FAMILY_RANDOM, SSL_CRYPTO_RANDOM_DEFAULT, 0,
			&context->randomAlgorithm) != 0)
		context->randomAlgorithm = NULL;
	*result = context;
	return status;
error:
	sslFreeCryptoContext(context);
	return status;
}

int sslRandomMethodSeed(SslCryptoAlgorithm *algorithm, const void *buffer,
			SceUInt32 length)
{
	int (*seed)(SslCryptoAlgorithm *, const void *, SceUInt32);
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (buffer != NULL) {
		status = SSL_LIBRARY_ERROR_SELECTOR;
		if (algorithm->family == 4) {
			status = SSL_CRYPTO_ERROR_NO_METHODS;
			if (algorithm->implementationMethods != NULL) {
				int callStatus =
					SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

				seed = algorithm->implementationMethods->Init;
				if (seed != NULL)
					callStatus = seed(algorithm, buffer,
							  length);
				status = callStatus;
			}
		}
	}
	return status;
}

int sslRandomMethodGenerate(SslCryptoAlgorithm *algorithm, SceUInt32 length,
			    void *buffer, SceUInt32 *generatedLength)
{
	int (*generate)(SslCryptoAlgorithm *, SceUInt32, void *, SceUInt32 *);
	int status;

	if (buffer == NULL || generatedLength == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	status = 10022;
	if (length != 0) {
		status = SSL_LIBRARY_ERROR_SELECTOR;
		if (algorithm->family == 4) {
			status = SSL_CRYPTO_ERROR_NO_METHODS;
			if (algorithm->implementationMethods != NULL) {
				int callStatus =
					SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

				generate = algorithm->implementationMethods->Operate;
				if (generate != NULL)
					callStatus = generate(algorithm, length,
							      buffer,
							      generatedLength);
				status = callStatus;
			}
		}
	}
	return status;
}

int sslMacMethodInit(SslCryptoAlgorithm *algorithm, void *key)
{
	int (*init)(SslCryptoAlgorithm *, void *);
	int status = SSL_LIBRARY_ERROR_SELECTOR;

	if (algorithm->family == 7) {
		status = SSL_CRYPTO_ERROR_NO_METHODS;
		if (algorithm->implementationMethods != NULL) {
			int callStatus = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

			init = algorithm->implementationMethods->Init;
			if (init != NULL)
				callStatus = init(algorithm, key);
			status = callStatus;
		}
	}
	return status;
}

int sslMacMethodUpdate(SslCryptoAlgorithm *algorithm, const void *data,
		       SceUInt32 length)
{
	int (*update)(SslCryptoAlgorithm *, const void *, SceUInt32);
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (data != NULL) {
		status = 10022;
		if (length != 0) {
			status = SSL_LIBRARY_ERROR_SELECTOR;
			if (algorithm->mode == 0x80) {
				status = SSL_CRYPTO_ERROR_NO_METHODS;
				if (algorithm->implementationMethods != NULL) {
					int callStatus =
						SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

					update = algorithm->implementationMethods->Update;
					if (update != NULL)
						callStatus = update(algorithm,
								    data,
								    length);
					status = callStatus;
				}
			}
		}
	}
	return status;
}

int sslMacMethodFinal(SslCryptoAlgorithm *algorithm, void *mac,
		      SceUInt32 *length)
{
	int (*finalOp)(SslCryptoAlgorithm *, void *, SceUInt32 *);
	int status;

	if (mac == NULL || length == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	status = SSL_LIBRARY_ERROR_SELECTOR;
	if (algorithm->mode == 0x80) {
		status = SSL_CRYPTO_ERROR_NO_METHODS;
		if (algorithm->implementationMethods != NULL) {
			int callStatus = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

			finalOp = algorithm->implementationMethods->Final;
			if (finalOp != NULL)
				callStatus = finalOp(algorithm, mac, length);
			status = callStatus;
		}
	}
	return status;
}

int sslMacMethodOperate(SslCryptoAlgorithm *algorithm, const void *data,
			SceUInt32 length, void *mac, SceUInt32 *macLength)
{
	int (*operate)(SslCryptoAlgorithm *, const void *, SceUInt32, void *,
		       SceUInt32 *);
	int status;

	if (data == NULL || mac == NULL || macLength == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	status = 10022;
	if (length != 0) {
		status = SSL_LIBRARY_ERROR_SELECTOR;
		if (algorithm->mode == 0x80) {
			status = SSL_CRYPTO_ERROR_NO_METHODS;
			if (algorithm->implementationMethods != NULL) {
				int callStatus =
					SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

				operate = algorithm->implementationMethods->Operate;
				if (operate != NULL)
					callStatus = operate(algorithm, data,
							     length, mac,
							     macLength);
				status = callStatus;
			}
		}
	}
	return status;
}

int sslDigestMethodInit(SslCryptoAlgorithm *algorithm)
{
	int (*init)(SslCryptoAlgorithm *);
	int status = SSL_LIBRARY_ERROR_SELECTOR;

	if (algorithm->family == 3) {
		status = SSL_CRYPTO_ERROR_NO_METHODS;
		if (algorithm->implementationMethods != NULL) {
			int callStatus = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

			init = algorithm->implementationMethods->Init;
			if (init != NULL)
				callStatus = init(algorithm);
			status = callStatus;
		}
	}
	return status;
}

int sslDigestMethodOperate(SslCryptoAlgorithm *algorithm, const void *data,
			   SceUInt32 length, void *digest,
			   SceUInt32 *digestLength)
{
	int (*operate)(SslCryptoAlgorithm *, const void *, SceUInt32, void *,
		       SceUInt32 *);
	int status;

	if (data == NULL || digest == NULL || digestLength == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	status = SSL_LIBRARY_ERROR_SELECTOR;
	if (algorithm->family == 3) {
		status = SSL_CRYPTO_ERROR_NO_METHODS;
		if (algorithm->implementationMethods != NULL) {
			int callStatus = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

			operate = algorithm->implementationMethods->Operate;
			if (operate != NULL)
				callStatus = operate(algorithm, data, length,
						     digest, digestLength);
			status = callStatus;
		}
	}
	return status;
}

int sslDigestMethodUpdate(SslCryptoAlgorithm *algorithm, const void *data,
			  SceUInt32 length)
{
	int (*update)(SslCryptoAlgorithm *, const void *, SceUInt32);
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (data != NULL) {
		status = SSL_LIBRARY_ERROR_SELECTOR;
		if (algorithm->family == 3) {
			status = SSL_CRYPTO_ERROR_NO_METHODS;
			if (algorithm->implementationMethods != NULL) {
				int callStatus =
					SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

				update = algorithm->implementationMethods->Update;
				if (update != NULL)
					callStatus = update(algorithm, data,
							    length);
				status = callStatus;
			}
		}
	}
	return status;
}

int sslDigestMethodFinal(SslCryptoAlgorithm *algorithm, void *digest,
			 SceUInt32 *length)
{
	int (*finalOp)(SslCryptoAlgorithm *, void *, SceUInt32 *);
	int status;

	if (digest == NULL || length == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	status = SSL_LIBRARY_ERROR_SELECTOR;
	if (algorithm->family == 3) {
		status = SSL_CRYPTO_ERROR_NO_METHODS;
		if (algorithm->implementationMethods != NULL) {
			int callStatus = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

			finalOp = algorithm->implementationMethods->Final;
			if (finalOp != NULL)
				callStatus = finalOp(algorithm, digest, length);
			status = callStatus;
		}
	}
	return status;
}
