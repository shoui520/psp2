/* Filename and descriptive private helper names are ours. */
#include "lib_ctx.h"

int sslGetLibraryMethod(SslLibrary *library, int type, int selector,
			SceUInt32 flags, int parameter, int request, void *result)
{
	const SslLibraryMethods *methods;

	if (library == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = library->methods;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (methods->GetMethod == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	if (type < 0 || type >= SSL_LIBRARY_MODULE_COUNT)
		return SSL_LIBRARY_ERROR_MODULE_TYPE;
	return methods->GetMethod(library, type, selector, flags, parameter,
				  request, result);
}

int sslCreateLibraryObject(SslLibrary *library, int type, int selector,
			   SceUInt32 flags, int parameter, void *result)
{
	const SslLibraryMethods *methods;

	if (library == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = library->methods;
	if (methods->CreateObject == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	if (type < 0 || type >= SSL_LIBRARY_MODULE_COUNT)
		return SSL_LIBRARY_ERROR_MODULE_TYPE;
	return methods->CreateObject(library, type, selector, flags, parameter,
				     result);
}

int sslGetLibraryData(SslLibrary *library, int type, void **result)
{
	const SslLibraryMethods *methods;

	if (library == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	methods = library->methods;
	if (methods->GetModuleData == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	if (type < 0 || type >= SSL_LIBRARY_MODULE_COUNT)
		return SSL_LIBRARY_ERROR_MODULE_TYPE;
	return methods->GetModuleData(library, type, result);
}
