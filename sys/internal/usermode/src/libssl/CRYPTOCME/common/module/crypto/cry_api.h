/* Filename and descriptive private names are ours. The component path follows
 * CRYPTOCME's recovered common/module/crypto source paths.
 */
#ifndef LIBSSL_CRYPTOCME_CRY_API_H
#define LIBSSL_CRYPTOCME_CRY_API_H

#include <stddef.h>
#include "../../object.h"

typedef struct SslCryptoAlgorithm SslCryptoAlgorithm;

/* Shared interface at the start of the crypto module method table.
 * Cipher, digest and other operation interfaces follow this prefix; this is
 * not a declaration of the complete module method table.
 */
typedef struct SslCryptoLifecycleMethods {
	int (*CreateContext)(void *library, int mode, void **context);
	int (*DestroyContext)(void *context);
	int (*CreateAlgorithm)(void *context, int family, int algorithm,
			       SceUInt32 mode, SslCryptoAlgorithm **result);
	int (*CopyAlgorithm)(SslCryptoAlgorithm *algorithm,
			     SslCryptoAlgorithm **result);
	int (*DestroyAlgorithm)(SslCryptoAlgorithm *algorithm);
	/* The native implementation of this slot returns an unavailable status.
	 * Its wider interface has not been recovered; no caller here invokes it.
	 */
	int (*UnavailableOperation)();
	int (*GetAlgorithmInfo)(SslCryptoAlgorithm *algorithm, int selector,
				void *result);
	int (*SetAlgorithmInfo)(SslCryptoAlgorithm *algorithm, int selector,
				SceUIntPtr value);
} SslCryptoLifecycleMethods;

/* Shared implementation interface prefix; algorithm-specific operations follow.
 * These descriptive names are ours, derived from binding/copy/free callers.
 */
typedef struct SslCryptoImplementationMethods {
	int (*Initialize)(SslCryptoAlgorithm *algorithm);
	int (*Destroy)(SslCryptoAlgorithm *algorithm);
	int (*Copy)(SslCryptoAlgorithm *source, SslCryptoAlgorithm *destination);
	int (*GetInfo)(SslCryptoAlgorithm *algorithm, int selector, void *result);
	int (*SetInfo)(SslCryptoAlgorithm *algorithm, int selector,
		       SceUIntPtr value);
} SslCryptoImplementationMethods;

struct SslCryptoAlgorithm {
	void *state;
	void *context;
	int family;
	int algorithm;
	SceUInt32 mode;
	SceUInt32 reservedBeforeMethods;
	const SslCryptoLifecycleMethods *methods;
	void *applicationData;
	SceUInt32 flags;
	void *properties;
	const SslCryptoImplementationMethods *implementationMethods;
	void *implementationData;
	SceUInt32 reservedBeforeReferences;
	int references;
};

/* Reserved words have only initialization accesses in the recovered generic
 * constructor. They are not substitutes for live fields with unknown roles.
 */
_Static_assert(sizeof(SslCryptoAlgorithm) == 56, "SslCryptoAlgorithm");
_Static_assert(offsetof(SslCryptoAlgorithm, methods) == 24, "methods");
_Static_assert(offsetof(SslCryptoAlgorithm, references) == 52, "references");
_Static_assert(offsetof(SslCryptoLifecycleMethods, DestroyAlgorithm) == 16,
	       "DestroyAlgorithm");

int sslDestroyAlgorithm(SslCryptoAlgorithm *algorithm);
int sslCopyAlgorithm(SslCryptoAlgorithm *algorithm, SslCryptoAlgorithm **result);
int sslGetAlgorithmInfo(SslCryptoAlgorithm *algorithm, int selector, void *result);
int sslSetAlgorithmInfo(SslCryptoAlgorithm *algorithm, int selector,
			SceUIntPtr value);
int sslGetImplementationInfo(SslCryptoAlgorithm *algorithm, int selector,
			     void *result);
int sslSetImplementationInfo(SslCryptoAlgorithm *algorithm, int selector,
			     SceUIntPtr value);

/* Descriptive selector names are ours, established by the native field accesses. */
#define SSL_CRYPTO_INFO_APPLICATION_DATA 16
#define SSL_CRYPTO_INFO_FAMILY 17
#define SSL_CRYPTO_INFO_ALGORITHM 18
#define SSL_CRYPTO_INFO_MODE 19
#define SSL_CRYPTO_INFO_FLAGS 21

_Static_assert(offsetof(SslCryptoLifecycleMethods, GetAlgorithmInfo) == 24,
	       "GetAlgorithmInfo");
_Static_assert(offsetof(SslCryptoLifecycleMethods, SetAlgorithmInfo) == 28,
	       "SetAlgorithmInfo");
_Static_assert(offsetof(SslCryptoImplementationMethods, GetInfo) == 12,
	       "GetInfo");
_Static_assert(offsetof(SslCryptoImplementationMethods, SetInfo) == 16,
	       "SetInfo");

#endif
