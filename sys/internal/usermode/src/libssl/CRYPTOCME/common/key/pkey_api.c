/* Filename and descriptive interface names are ours, in the recovered key
 * component. These types describe shared interface prefixes only.
 */
#include <stddef.h>
#include <libssl/object.h>
#include "../object.h"
#include "pkey_ctx.h"

int sslGetPublicKeyInfo(void *key, int selector, void *result)
{
	SslPublicKeyHeader *object = key;

	if (result == NULL || object == NULL || object->context->module == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->context->module->GetKeyInfo == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->context->module->GetKeyInfo(key, selector, result);
}

int SceSslInternal_46358909(void *key)
{
	SslPublicKeyHeader *object = key;

	if (object == NULL)
		return 0;
	if (object->context->module == NULL)
		return 0;
	if (object->context->module->DestroyKey == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->context->module->DestroyKey(key);
}

int sslGetPublicKeyAlgorithm(void *key)
{
	int algorithm;

	if (sslGetPublicKeyInfo(key, SSL_KEY_INFO_ALGORITHM, &algorithm) != 0)
		return -1;
	return algorithm;
}

int sslSetPublicKeyInfo(void *key, int selector, void *value)
{
	SslPublicKeyHeader *object = key;

	if (object == NULL || object->context->module == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->context->module->SetKeyInfo == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->context->module->SetKeyInfo(key, selector, value);
}

int sslRetainKeyContext(void *context)
{
	SslKeyContext *keyContext = context;

	if (keyContext == NULL || keyContext->module == NULL)
		return 0;
	if (keyContext->module->RetainContext == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return keyContext->module->RetainContext(context);
}

int sslDestroyKeyContext(void *context)
{
	SslKeyContext *keyContext = context;

	if (keyContext == NULL || keyContext->module == NULL)
		return 0;
	if (keyContext->module->DestroyContext == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return keyContext->module->DestroyContext(context);
}

int sslCreatePublicKey(void *context, int algorithm, void **result)
{
	SslKeyContext *keyContext = context;

	if (result == NULL || keyContext == NULL || keyContext->module == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (keyContext->module->CreateKey == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return keyContext->module->CreateKey(context, algorithm, result);
}

int sslCopyPublicKey(void *source, void *destination, int mode)
{
	SslPublicKeyHeader *object = source;

	if (destination == NULL || object == NULL || object->context->module == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->context->module->CopyKey == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->context->module->CopyKey(source, destination, mode);
}

int sslEnumerateKeyComponents(void *key, SslKeyComponentCallback callback, void *arg)
{
	SslPublicKeyHeader *object = key;

	if (callback == NULL || object == NULL || object->context->module == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->context->module->EnumerateComponents == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->context->module->EnumerateComponents(key, callback, arg);
}

int sslCreateKeyContext(SslLibrary *library, int parameter, SceUInt32 flags,
			void **result)
{
	SslKeyModuleFactory factory;
	SslKeyModuleFactory getMethods;
	const SslKeyModule *module;
	void *data = NULL;
	int status;

	if (library == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	status = sslGetLibraryMethod(library, SSL_KEY_MODULE_TYPE, 0, flags,
				     parameter, SSL_LIBRARY_GET_METHOD_FACTORY, &factory);
	if (status != 0)
		return status;
	getMethods = factory;
	status = sslGetLibraryData(library, SSL_KEY_MODULE_TYPE, &data);
	if (status != 0)
		return status;
	module = getMethods(data);
	if (module == NULL)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	return module->CreateContext(library, parameter, flags, result);
}
