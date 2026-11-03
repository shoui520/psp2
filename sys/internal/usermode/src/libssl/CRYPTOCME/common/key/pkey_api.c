/* Filename and descriptive interface names are ours, in the recovered key
 * component. These types describe shared interface prefixes only.
 */
#include <stddef.h>
#include <libssl/object.h>
#include "../object.h"
#include "pkey_ctx.h"

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

int sslDestroyKeyContext(void *context)
{
	SslKeyContext *keyContext = context;

	if (keyContext == NULL || keyContext->module == NULL)
		return 0;
	if (keyContext->module->DestroyContext == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return keyContext->module->DestroyContext(context);
}
