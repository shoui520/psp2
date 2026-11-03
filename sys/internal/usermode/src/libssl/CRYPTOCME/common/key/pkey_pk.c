#include "pkey_pk.h"
#include "../../../memory.h"
#include "../../../runtime.h"

static const char publicKeySource[] =
	"S:\\work\\CRYPTOCME\\common\\key/pkey_pk.c";

int sslFreePublicKey(void *key)
{
	SslPublicKey *object = key;

	if (sslAddReference(&object->references, -1, SSL_KEY_CONTEXT_LOCK,
			    publicKeySource, 910) == 0) {
		SslKeyContext *context = object->context;

		if (context != NULL && context->module != NULL &&
		    context->module->DestroyContext != NULL)
			context->module->DestroyContext(context);
		if (object != NULL) {
			if (object->properties != NULL) {
				sslFreePropertyList(object->properties);
				object->properties = NULL;
			}
			if (!(object->flags & SSL_PUBLIC_KEY_BORROWED_DATA) &&
			    object->externalData != NULL)
				sslFree(object->externalData);
			object->externalData = NULL;
			object->context = NULL;
			sslFree(object);
		}
	}
	return 0;
}
