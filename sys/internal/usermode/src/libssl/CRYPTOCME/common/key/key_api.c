/* Native key-object dispatch; filename and helper name are ours. */
#include <stddef.h>
#include "../object.h"
#include "key_api.h"

int sslDestroyKey(void *key)
{
	SslObjectHeader *object = key;

	if (object == NULL)
		return 0;
	if (object->methods->Destroy == NULL)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	return object->methods->Destroy(key);
}

int sslGetKeyInfo(void *key, int selector, void *result)
{
	SslObjectHeader *object = key;

	if (object == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->methods->GetInfo == NULL)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	return object->methods->GetInfo(key, selector, result);
}

int sslSetKeyInfo(void *key, int selector, void *value)
{
	SslObjectHeader *object = key;

	if (object == NULL || value == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->methods->SetInfo == NULL)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	return object->methods->SetInfo(key, selector, value);
}
