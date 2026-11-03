/* Filename and descriptive private names are ours. The component path follows
 * CRYPTOCME's recovered common/module/crypto source paths.
 */
#ifndef LIBSSL_CRYPTOCME_CRY_API_H
#define LIBSSL_CRYPTOCME_CRY_API_H

#include <stddef.h>
#include "../../object.h"

typedef struct SslCryptoAlgorithm SslCryptoAlgorithm;

/* Shared lifecycle interface at the start of the crypto module method table.
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
} SslCryptoLifecycleMethods;

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
	void *implementationMethods;
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

#endif
