/* Filename and descriptive private names are ours. The component path follows
 * CRYPTOCME's recovered common/module/crypto source paths.
 */
#ifndef LIBSSL_CRYPTOCME_CRY_API_H
#define LIBSSL_CRYPTOCME_CRY_API_H

#include <stddef.h>
#include "../../object.h"
#include "../../property.h"
#include "../../library/lib_ctx.h"

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
	SceUIntPtr opaqueOperations[20];
	int (*DigestInit)(SslCryptoAlgorithm *algorithm);
	int (*Digest)(SslCryptoAlgorithm *algorithm, const void *data,
		      SceUInt32 length, void *digest, SceUInt32 *digestLength);
	int (*DigestUpdate)(SslCryptoAlgorithm *algorithm, const void *data,
			    SceUInt32 length);
	int (*DigestFinal)(SslCryptoAlgorithm *algorithm, void *data, SceUInt32 *length);
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

/* Complete algorithm descriptor; the names are ours. */
typedef struct SslCryptoAlgorithmDescriptor {
	int family;
	int algorithm;
	SceUInt32 mode;
	const SslCryptoImplementationMethods *(*GetMethods)(void);
	void *data;
} SslCryptoAlgorithmDescriptor;

/* Only the shared context interface is declared here. Concrete context storage
 * extends this prefix; no size for that allocation is implied by this type.
 */
typedef struct SslCryptoContextHeader {
	const SslCryptoLifecycleMethods *methods;
	SslLibrary *library;
	const SslCryptoAlgorithmDescriptor *algorithms;
} SslCryptoContextHeader;

/* The mapping's second and third values select families 3 and 6 respectively
 * in the native caller. These descriptive names are ours.
 */
typedef struct SslCryptoMacAlgorithmMapping {
	int algorithm;
	int digestAlgorithm;
	int macAlgorithm;
} SslCryptoMacAlgorithmMapping;

_Static_assert(sizeof(SslCryptoAlgorithmDescriptor) == 20,
	       "SslCryptoAlgorithmDescriptor");
_Static_assert(sizeof(SslCryptoMacAlgorithmMapping) == 12,
	       "SslCryptoMacAlgorithmMapping");
_Static_assert(offsetof(SslCryptoContextHeader, algorithms) == 8, "algorithms");

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
	SslPropertyList *properties;
	const SslCryptoImplementationMethods *implementationMethods;
	void *implementationData;
	void *backendState;
	int references;
};

/* Concrete implementations own backendState independently of the descriptor's
 * implementationData. The generic constructor initializes both to zero.
 */
_Static_assert(sizeof(SslCryptoAlgorithm) == 56, "SslCryptoAlgorithm");
_Static_assert(offsetof(SslCryptoAlgorithm, methods) == 24, "methods");
_Static_assert(offsetof(SslCryptoAlgorithm, references) == 52, "references");
_Static_assert(offsetof(SslCryptoLifecycleMethods, DestroyAlgorithm) == 16,
	       "DestroyAlgorithm");

int sslDestroyAlgorithm(SslCryptoAlgorithm *algorithm);
int sslCreateAlgorithm(SslCryptoContextHeader *context, int family, int algorithm,
		       SceUInt32 mode, SslCryptoAlgorithm **result);
int sslDigestInit(SslCryptoAlgorithm *algorithm);
int sslDigest(SslCryptoAlgorithm *algorithm, const void *data, SceUInt32 length,
	      void *digest, SceUInt32 *digestLength);
int sslDigestUpdate(SslCryptoAlgorithm *algorithm, const void *data, SceUInt32 length);
int sslDigestFinal(SslCryptoAlgorithm *algorithm, void *data, SceUInt32 *length);
int sslCreateCryptoContext(SslLibrary *library, int mode, void **result);
int sslDestroyCryptoContext(void *context);
int sslFreeAlgorithm(SslCryptoAlgorithm *algorithm);
int sslFindAlgorithmDescriptor(const SslCryptoAlgorithmDescriptor *table,
			       int family, int algorithm, SceUInt32 mode,
			       const SslCryptoAlgorithmDescriptor **result);
int sslGetAlgorithmDescriptor(SslCryptoAlgorithm *algorithm,
			      const SslCryptoAlgorithmDescriptor *table,
			      const SslCryptoAlgorithmDescriptor **result);
int sslBindAlgorithm(SslCryptoAlgorithm **result);
int sslCloneAlgorithm(SslCryptoAlgorithm *algorithm, SslCryptoAlgorithm **result);
int sslQueryAlgorithmSupport(SslCryptoContextHeader *context, int family,
			     int algorithm, SceUInt32 mode, int *result);
int sslMapMacAlgorithms(SslLibrary *library, int *algorithm,
			int *digestAlgorithm, int *macAlgorithm);
int sslGetMacAlgorithms(SslCryptoContextHeader *context, int algorithm,
			int *digestAlgorithm, int *macAlgorithm);
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

#define SSL_CRYPTO_LOCK 38
#define SSL_CRYPTO_MODULE_TYPE 6
#define SSL_CRYPTO_MAC_MAPPING_FLAGS 6
#define SSL_CRYPTO_ALGORITHM_SUPPORTED 2
#define SSL_CRYPTO_MODE_COPY 0x100
#define SSL_CRYPTO_ALGORITHM_DESTROYING 0x1000
#define SSL_CRYPTO_ALGORITHM_OWNS_PROPERTIES 8

_Static_assert(offsetof(SslCryptoLifecycleMethods, GetAlgorithmInfo) == 24,
	       "GetAlgorithmInfo");
_Static_assert(offsetof(SslCryptoLifecycleMethods, SetAlgorithmInfo) == 28,
	       "SetAlgorithmInfo");
_Static_assert(offsetof(SslCryptoLifecycleMethods, DigestInit) == 112, "DigestInit");
_Static_assert(offsetof(SslCryptoLifecycleMethods, DigestUpdate) == 120, "DigestUpdate");
_Static_assert(offsetof(SslCryptoLifecycleMethods, DigestFinal) == 124, "DigestFinal");
_Static_assert(offsetof(SslCryptoImplementationMethods, GetInfo) == 12,
	       "GetInfo");
_Static_assert(offsetof(SslCryptoImplementationMethods, SetInfo) == 16,
	       "SetInfo");

#endif
