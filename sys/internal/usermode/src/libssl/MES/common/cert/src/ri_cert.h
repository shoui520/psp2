/* Private descriptive names and this header filename are ours. */
#ifndef LIBSSL_RI_CERT_H
#define LIBSSL_RI_CERT_H

#include "../../../../CRYPTOCME/common/object.h"
#include "../../../../CRYPTOCME/common/property.h"
#include "../../../../CRYPTOCME/common/library/lib_ctx.h"

/* Shared method prefix; implementations extend this interface. */
typedef struct SslCertificateMethods {
	int (*CreateContext)(void *library, int flags, void **result);
	int (*DestroyContext)(void *context);
	int (*GetContextInfo)(void *context, int selector, void *result);
	int (*SetContextInfo)(void *context, int selector, void *value);
	int (*CreateCertificate)(void *context, int flags, void **result);
	int (*DestroyCertificate)(void *certificate);
} SslCertificateMethods;

typedef struct SslCertificateHeader {
	const SslCertificateMethods *methods;
} SslCertificateHeader;

typedef struct SslCertificateContext {
	const SslCertificateMethods *methods;
	SslLibrary *library;
	void *cryptoContext;
	int cryptoOwnership;
	void *keyContext;
	SceUInt32 flags;
	SceUInt32 supportedTypes;
	SceUInt32 opaqueConfiguration;
	const void *backendMethods;
	int references;
} SslCertificateContext;

typedef struct SslCertificateName {
	const SslObjectMethods *methods;
	SslCertificateContext *context;
	SslPropertyList *properties;
	SceUInt32 flags;
	const void *backendMethods;
} SslCertificateName;

typedef struct SslCertificateKeyCache {
	SceUInt32 flags;
	int algorithm;
	void *key;
} SslCertificateKeyCache;

/* Only the property-list prefix is needed by cache destruction. */
typedef struct SslCertificatePropertyCache {
	SslPropertyList *properties;
} SslCertificatePropertyCache;

typedef struct SslCertificate {
	const SslCertificateMethods *methods;
	SslCertificateContext *context;
	const void *backendMethods;
	SceUInt32 flags;
	int references;
	SslPropertyList *properties;
	SceUInt32 encodedSize;
	SslCertificateName *issuer;
	SslCertificateName *subject;
	SslCertificateKeyCache *publicKey;
	SslCertificatePropertyCache *propertyCaches[2];
	SceUInt32 type;
	SceUInt32 opaqueConfiguration;
	SslPropertyList attributes;
	SceUInt32 opaqueTail;
} SslCertificate;

_Static_assert(offsetof(SslCertificateMethods, DestroyCertificate) == 20,
	       "DestroyCertificate");
_Static_assert(sizeof(SslCertificateContext) == 40, "SslCertificateContext");
_Static_assert(offsetof(SslCertificateContext, references) == 36, "references");
_Static_assert(sizeof(SslCertificateName) == 20, "SslCertificateName");
_Static_assert(sizeof(SslCertificateKeyCache) == 12, "SslCertificateKeyCache");
_Static_assert(sizeof(SslCertificate) == 84, "SslCertificate");
_Static_assert(offsetof(SslCertificate, publicKey) == 36, "publicKey");
_Static_assert(offsetof(SslCertificate, attributes) == 56, "attributes");

int sslFreeCertificateContext(void *object);
int sslFreeCertificateName(void *object);
int sslCacheCertificatePublicKey(SslCertificate *certificate, int mode, void *source);
int sslClearCertificateCache(SslCertificate *certificate, int selector);
int sslInvalidateCertificateProperties(SslCertificate *certificate);
int sslSetCertificateProperty(SslCertificate *certificate, int id,
			      SceUInt32 value, const void *data, SceUInt32 length);
int sslFreeCertificate(void *object);
int sslRetainCertificateContext(void *object);
int sslNewCertificate(void *object, int type, void **result);
int sslCopyCertificateName(SslCertificateName *source, int mode,
			   SslCertificateName **result);
int sslSetCertificateSubject(SslCertificate *certificate, int mode,
			     SslCertificateName *source);
int sslSetCertificateIssuer(SslCertificate *certificate, int mode,
			    SslCertificateName *source);
int sslSetCertificatePublicKey(SslCertificate *certificate, int mode, void *source);
int sslNewCertificateContext(SslLibrary *library, int unused, SceUInt32 types,
			     void **result);
int sslGetCertificateContextInfo(void *object, int selector, void *result);
int sslSetCertificateContextInfo(void *object, int selector, void *value);

#endif
