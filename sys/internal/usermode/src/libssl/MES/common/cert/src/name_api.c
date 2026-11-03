/* Filename is ours; common/cert/src follows recovered MES source paths. */
#include <stddef.h>
#include "../../../../CRYPTOCME/common/object.h"
#include <libssl/name.h>

int SceSslInternal_CE37B69E(void *name)
{
	SslObjectHeader *object = name;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->methods->Destroy == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->methods->Destroy(name);
}
