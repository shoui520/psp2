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
