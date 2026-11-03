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
