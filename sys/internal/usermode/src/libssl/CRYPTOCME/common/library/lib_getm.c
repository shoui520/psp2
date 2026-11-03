/* lib_getm.c is a recovered component filename; placement of the shared
 * descriptor matcher here and the descriptive private names are ours.
 */
#include "lib_ctx.h"

int sslFindLibraryModule(SslLibraryModule *table, int type, int selector,
			SceUInt32 flags, int parameter, SslLibraryModule **result,
			SslLibraryModule **next)
{
	if (table == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	while (table->type != 0) {
		if ((type == SSL_LIBRARY_ANY_TYPE || table->type == type) &&
		    (selector == SSL_LIBRARY_ANY_SELECTOR ||
		     table->selector == selector) &&
		    (parameter == SSL_LIBRARY_ANY_PARAMETER ||
		     table->parameter == parameter) &&
		    (flags == SSL_LIBRARY_ANY_FLAGS ||
		     (table->flags & flags) == flags))
			break;
		table++;
	}
	if (table->type == 0 && type != 0)
		return SSL_LIBRARY_ERROR_NOT_FOUND;
	*result = table;
	if (next != NULL)
		*next = table->type != 0 ? table + 1 : NULL;
	return 0;
}

int sslConstructLibraryObject(SslLibrary *library, int type, int selector,
			      SceUInt32 flags, int parameter,
			      SslObjectHeader **result)
{
	SslObjectHeader *object = NULL;
	const SslObjectMethods *methods = NULL;
	int status;

	if (result == NULL || library == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	status = sslGetLibraryMethod(library, type, selector, flags, parameter,
				     SSL_LIBRARY_GET_METHODS, &methods);
	if (status != 0)
		return status;
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	status = methods->Create(library, parameter, &object);
	if (status != 0)
		return status;
	if (object->methods == NULL)
		object->methods = methods;
	*result = object;
	return 0;
}

int sslLookupLibraryMethod(SslLibrary *library, int type, int selector,
			   SceUInt32 flags, int parameter, int request,
			   SceUIntPtr *result)
{
	SslLibraryModule *module;
	SslLibraryModuleState *state = &library->modules[type];
	int status;

	status = sslFindLibraryModule(library->modules_table, type, selector,
				      flags, parameter, &module, NULL);
	if (status != 0)
		return status;
	switch (request) {
	case SSL_LIBRARY_GET_METHOD_FACTORY:
		*result = (SceUIntPtr)module->GetMethods;
		break;
	case SSL_LIBRARY_GET_IMPLEMENTATION_FACTORY:
		*result = (SceUIntPtr)module->GetImplementation;
		break;
	case SSL_LIBRARY_GET_IMPLEMENTATION:
		if (module->GetImplementation == NULL)
			return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
		*result = (SceUIntPtr)module->GetImplementation(state->data);
		break;
	case SSL_LIBRARY_GET_METHODS:
		if (module->GetMethods == NULL)
			return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
		*result = (SceUIntPtr)module->GetMethods(state->data);
		break;
	default:
		return SSL_LIBRARY_ERROR_SELECTOR;
	}
	return 0;
}
