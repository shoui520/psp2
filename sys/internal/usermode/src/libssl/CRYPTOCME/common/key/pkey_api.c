/* Filename and descriptive interface names are ours, in the recovered key
 * component. These types describe shared interface prefixes only.
 */
#include <stddef.h>
#include <libssl/object.h>
#include "../object.h"

typedef struct SslKeyModule {
	const char *name;
	/* Native module identification words; their individual meanings are not
	 * recovered. These are metadata, not padding or omitted operations.
	 */
	SceUInt32 metadata[2];
	int (*CreateContext)(void *library, int flags, int mode, void **result);
	int (*DestroyContext)(void *context);
	int (*GetContextInfo)(void *context, int selector, void *result);
	int (*SetContextInfo)(void *context, int selector, void *value);
	int (*CreateKey)(void *context, int type, void **result);
	int (*DestroyKey)(void *key);
} SslKeyModule;

typedef struct SslKeyContextHeader {
	const SslKeyModule *module;
} SslKeyContextHeader;

typedef struct SslPublicKeyHeader {
	SslKeyContextHeader *context;
} SslPublicKeyHeader;

_Static_assert(offsetof(SslKeyModule, DestroyKey) == 32, "DestroyKey");

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
