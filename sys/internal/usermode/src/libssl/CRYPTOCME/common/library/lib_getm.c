/* lib_getm.c is a recovered component filename; placement of the shared
 * descriptor matcher here and the descriptive private names are ours.
 */
#include "lib_ctx.h"
#include "../../../runtime.h"

static const char getModuleSource[] =
	"S:\\work\\CRYPTOCME\\common\\library/lib_getm.c";

/* Descriptive names are ours; the originals keep two distinct writable lists. */
static int referenceModuleTypes[] = { 6, 9, 10, 16, 17, 0 };
static int initialReferenceModuleTypes[] = { 6, 9, 10, 16, 17, 0 };

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

int sslGetCachedLibraryObject(SslLibrary *library, int type, int selector,
			      SceUInt32 flags, int parameter,
			      SslObjectHeader **result)
{
	SslLibraryModuleState *state = &library->modules[type];
	SslObjectHeader *object = NULL;
	int existing;
	int referenced = 0;
	int discard = 0;
	int status = 0;
	int i;

	sslLock(SSL_LOCK_REFERENCE, SSL_LIBRARY_LOCK, getModuleSource, 44);
	existing = state->object != NULL;
	sslLock(SSL_UNLOCK_REFERENCE, SSL_LIBRARY_LOCK, getModuleSource, 49);
	if (existing) {
		*result = state->object;
		return 0;
	}
	for (i = 0; referenceModuleTypes[i] != 0; i++) {
		if (referenceModuleTypes[i] == type) {
			referenced = 1;
			break;
		}
	}
	status = sslConstructLibraryObject(library, type, selector, flags,
					   parameter, &object);
	sslLock(SSL_LOCK_REFERENCE, SSL_LIBRARY_LOCK, getModuleSource, 91);
	if (state->object != NULL) {
		discard = 1;
		status = 0;
	} else if (status == 0) {
		state->object = object;
		if (referenced)
			library->module_references =
				(int)((SceUInt32)library->module_references + 1);
	}
	sslLock(SSL_UNLOCK_REFERENCE, SSL_LIBRARY_LOCK, getModuleSource, 110);
	if (discard && object != NULL)
		object->methods->Destroy(object);
	if (status == 0)
		*result = state->object;
	return status;
}

int sslInitCachedLibraryObject(SslLibrary *library, int type, int selector,
			       SceUInt32 flags, int parameter,
			       SslObjectHeader **result)
{
	SslLibraryModuleState *state = &library->modules[type];
	int status;
	int i;

	if (state->object != NULL) {
		*result = state->object;
		return 0;
	}
	status = sslConstructLibraryObject(library, type, selector, flags,
					   parameter, &state->object);
	if (status != 0)
		return status;
	for (i = 0; initialReferenceModuleTypes[i] != 0; i++) {
		if (initialReferenceModuleTypes[i] == type) {
			library->module_references = 1;
			break;
		}
	}
	*result = state->object;
	return 0;
}
