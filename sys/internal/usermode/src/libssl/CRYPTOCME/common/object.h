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
	int (*GetInfo)(void *object, int selector, void *result);
	int (*SetInfo)(void *object, int selector, void *value);
} SslObjectMethods;

typedef struct SslObjectHeader {
	const SslObjectMethods *methods;
} SslObjectHeader;

/* Descriptive names for the native CRYPTOCME status values are ours. */
#define SSL_CRYPTO_ERROR_NULL_ARGUMENT 10017
#define SSL_CRYPTO_ERROR_NULL_OUTPUT 10018
#define SSL_CRYPTO_ERROR_NO_METHODS 10015
#define SSL_CRYPTO_ERROR_NOT_INITIALIZED 10013
#define SSL_CRYPTO_ERROR_INVALID_STATE 10012
#define SSL_CRYPTO_ERROR_NO_OPERATION 10011
#define SSL_CRYPTO_ERROR_NO_IMPLEMENTATION 10010
#define SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD 10009
#define SSL_CRYPTO_ERROR_UNSUPPORTED 10008
#define SSL_CRYPTO_ERROR_NOT_FOUND 10009
#define SSL_CRYPTO_ERROR_INVALID_ARGUMENT 10018
#define SSL_CRYPTO_ERROR_FAILED 10001
#define SSL_CRYPTO_ERROR_NO_DIGEST 10006
#define SSL_CRYPTO_ERROR_UNHANDLED 10019
#define SSL_CRYPTO_ERROR_BAD_COMMAND 10021

#endif
