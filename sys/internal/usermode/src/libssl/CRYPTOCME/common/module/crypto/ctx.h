/* Filename and descriptive private names are ours. */
#ifndef LIBSSL_CRYPTOCME_CTX_H
#define LIBSSL_CRYPTOCME_CTX_H

#include "cry_api.h"

/* Extension storage is zero-initialized and preserved by the context methods.
 * It is not interpreted as part of the shared interface.
 */
typedef struct SslCryptoContext {
	SslCryptoContextHeader interface;
	SceUInt32 opaqueConfiguration;
	SslCryptoAlgorithm *randomAlgorithm;
	void *entropySource;
	void *ownedStorage;
	SceUInt32 opaqueExtensions[5];
	int references;
	SceUInt32 opaqueTail;
} SslCryptoContext;

_Static_assert(sizeof(SslCryptoContext) == 56, "SslCryptoContext");
_Static_assert(offsetof(SslCryptoContext, randomAlgorithm) == 16,
	       "randomAlgorithm");
_Static_assert(offsetof(SslCryptoContext, entropySource) == 20, "entropySource");
_Static_assert(offsetof(SslCryptoContext, ownedStorage) == 24, "ownedStorage");
_Static_assert(offsetof(SslCryptoContext, references) == 48, "references");
_Static_assert(offsetof(SslCryptoAlgorithm, backendState) == 48, "backendState");

#define SSL_CRYPTO_CONTEXT_RANDOM_ALGORITHM 0
#define SSL_CRYPTO_CONTEXT_ENTROPY_SOURCE 1
#define SSL_CRYPTO_FAMILY_RANDOM 4
#define SSL_CRYPTO_RANDOM_DEFAULT 4096
#define SSL_CRYPTO_MODE_UNSUPPORTED 0x200

int sslRetainCryptoContext(void *object);
int sslFreeCryptoContext(void *object);
int sslNewCryptoContext(void *libraryObject, int mode, void **result);
int sslSetCryptoContextInfo(SslCryptoContext *context, int selector,
			    SceUIntPtr value);
int sslNewAlgorithm(void *object, int family, int identifier, SceUInt32 mode,
		     SslCryptoAlgorithm **result);

#endif
