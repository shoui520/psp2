/* Descriptive interface names are ours, not recovered Sony spellings. */
#ifndef LIBSSL_CRYPTOCME_OBJECT_H
#define LIBSSL_CRYPTOCME_OBJECT_H

#include <scetypes.h>

/* Common polymorphic header. Concrete object storage follows this header;
 * methods can extend the create/destroy interface with object-specific calls.
 */
typedef struct SslObjectMethods {
	int (*Create)();
	int (*Destroy)(void *object);
} SslObjectMethods;

typedef struct SslObjectHeader {
	const SslObjectMethods *methods;
} SslObjectHeader;

/* Descriptive names for the native CRYPTOCME status values are ours. */
#define SSL_CRYPTO_ERROR_NULL_ARGUMENT 10017
#define SSL_CRYPTO_ERROR_NO_METHODS 10015
#define SSL_CRYPTO_ERROR_NO_OPERATION 10011
#define SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD 10009

#endif
