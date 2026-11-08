#include <stddef.h>
#include "pkey_ctx.h"
#include "pkey_pk.h"
#include "../../../memory.h"
#include "../../../runtime.h"

static const char keyContextSource[] =
	"S:\\work\\CRYPTOCME\\common\\key/pkey_ctx.c";

int sslInitKeyContext(void *libraryObject, int parameter, SceUInt32 flags,
		      void **result)
{
	SslLibrary *library = libraryObject;
	SslKeyContext *context;
	SslKeyModuleFactory factory;
	SslKeyModuleFactory getMethods;
	int (*(*getImplementation)(void *data))(void);
	void *data;
	int status;

	context = sslAllocate(sizeof(*context));
	if (context != NULL) {
		sslFillMemory(context, 0, sizeof(*context));
		context->references = 1;
		status = sslLibraryAddReference(library);
		if (status == 0) {
			context->library = library;
			status = sslGetLibraryMethod(
				library, SSL_KEY_MODULE_TYPE, 0, flags,
				parameter, SSL_LIBRARY_GET_METHOD_FACTORY,
				&factory);
			if (status == 0) {
				getMethods = factory;
				status = sslGetLibraryData(
					library, SSL_KEY_MODULE_TYPE, &data);
				if (status != 0)
					goto done;
				context->module = getMethods(data);
			}
			if (status == 0 || status == 10008 || status == 10009) {
				if (context->module != NULL) {
					status = sslCreateLibraryObject(
						library, 6, 0, 0, 0,
						&context->backend);
					if (status == 0 || status == 10008 ||
					    status == 10009) {
						if (context->module->opaqueContextOperation !=
						    NULL)
							goto publish;
						status = sslGetLibraryMethod(
							library,
							SSL_KEY_MODULE_TYPE, 6,
							0, 0,
							SSL_LIBRARY_GET_IMPLEMENTATION_FACTORY,
							&factory);
						if (status == 0) {
							getImplementation =
								(int (*(*)(void *))(void))factory;
							status = sslGetLibraryData(
								library,
								SSL_KEY_MODULE_TYPE,
								&data);
							if (status != 0)
								goto done;
							((SslKeyModule *)context->module)->opaqueContextOperation =
								getImplementation(
									data);
						}
						if (status == 0 ||
						    status == 10008) {
publish:
							status = 0;
							*result = context;
						}
					}
				} else {
					status = 10015;
				}
			}
		}
	} else {
		status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	}
done:
	if (context != NULL && status != 0) {
		sslFree(context);
		SceSslInternal_4725F9E7(library);
	}
	return status;
}

int sslAddKeyContextReference(void *context)
{
	SslKeyContext *keyContext = context;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (keyContext != NULL) {
		sslAddReference(&keyContext->references, 1,
				SSL_KEY_CONTEXT_LOCK, keyContextSource, 198);
		status = 0;
	}
	return status;
}

int sslFreeKeyContext(void *context)
{
	SslKeyContext *keyContext = context;

	if (sslAddReference(&keyContext->references, -1, SSL_KEY_CONTEXT_LOCK,
			    keyContextSource, 222) == 0 && keyContext != NULL) {
		if (keyContext->library != NULL)
			SceSslInternal_4725F9E7(keyContext->library);
		sslFree(keyContext);
	}
	return 0;
}

int sslGetKeyContextInfo(void *context, int selector, void *result)
{
	SslKeyContext *keyContext = context;
	SceUIntPtr *value = result;

	if (keyContext == NULL || value == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	switch (selector) {
	case SSL_KEY_CONTEXT_INFO_LIBRARY:
		*value = (SceUIntPtr)keyContext->library;
		break;
	case SSL_KEY_CONTEXT_INFO_MODULE:
		*value = (SceUIntPtr)keyContext->module;
		break;
	case SSL_KEY_CONTEXT_INFO_BACKEND:
		*value = (SceUIntPtr)keyContext->backend;
		break;
	case SSL_KEY_CONTEXT_INFO_FLAGS:
		*value = keyContext->flags;
		break;
	case SSL_KEY_CONTEXT_INFO_APPLICATION_DATA:
		*value = (SceUIntPtr)keyContext->applicationData;
		break;
	default:
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	}
	return 0;
}

int sslSetKeyContextInfo(void *context, int selector, void *value)
{
	SslKeyContext *keyContext = context;

	if (keyContext == NULL ||
	    (value == NULL && selector == SSL_KEY_CONTEXT_INFO_FLAGS))
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	switch (selector) {
	case SSL_KEY_CONTEXT_INFO_FLAGS:
		keyContext->flags |= *(SceUInt32 *)value;
		return 0;
	case SSL_KEY_CONTEXT_INFO_APPLICATION_DATA:
		if (keyContext->applicationData != NULL &&
		    !(keyContext->flags & SSL_KEY_CONTEXT_BORROWED_DATA))
			sslFree(keyContext->applicationData);
		keyContext->applicationData = value;
		keyContext->flags &= ~SSL_KEY_CONTEXT_BORROWED_DATA;
		return 0;
	default:
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	}
}

int sslAddPublicKeyReference(void *key)
{
	SslPublicKey *object = key;

	sslAddReference(&object->references, 1, SSL_KEY_CONTEXT_LOCK,
			keyContextSource, 396);
	return 0;
}
