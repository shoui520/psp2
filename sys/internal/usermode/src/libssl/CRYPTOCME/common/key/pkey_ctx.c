#include <stddef.h>
#include "pkey_ctx.h"
#include "../../../memory.h"
#include "../../../runtime.h"

static const char keyContextSource[] =
	"S:\\work\\CRYPTOCME\\common\\key/pkey_ctx.c";

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
