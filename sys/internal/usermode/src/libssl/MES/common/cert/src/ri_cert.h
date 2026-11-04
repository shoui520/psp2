/* Private descriptive names and this header filename are ours. */
#ifndef LIBSSL_RI_CERT_H
#define LIBSSL_RI_CERT_H

#include "../../../../CRYPTOCME/common/object.h"
#include "../../../../CRYPTOCME/common/property.h"
#include "../../../../CRYPTOCME/common/library/lib_ctx.h"

typedef struct SslCertificate SslCertificate;

/* Backend interface prefix. Uninterpreted extension words are retained without
 * assigning them operations; this declaration does not describe the whole table.
 */
typedef struct SslCertificateBackendMethods {
	SceUIntPtr opaqueHeader[4];
	int (*ExportProperties)(SslCertificate *certificate, SslPropertyList *properties);
	int (*ImportProperties)(SslCertificate *certificate, SslPropertyList *properties);
	int (*DecodePublicKey)(SslCertificate *certificate, SslProperty *property,
			       int mode, void **result);
	int (*GetSignatureAlgorithm)(SslCertificate *certificate);
	int (*GetSignatureAlgorithmEncoding)(SslCertificate *certificate, void *result);
	int (*QuerySignedValue)(SslCertificate *certificate);
	int (*DecodeVersion)(SslCertificate *certificate, int present,
			     SceUInt32 encodedVersion, int *result);
	int (*EncodeVersion)(SslCertificate *certificate, int version,
			     SceUInt32 *encodedVersion, SceUInt32 *tag);
	int (*QueryNonzeroValue)(SslCertificate *certificate);
	SceUIntPtr opaqueQueries[2];
	int (*Parse)(SslCertificate *certificate, SceUInt32 flags, SceUInt32 length,
		     const void *data, SceUInt32 *consumed);
	int (*Serialize)(SslCertificate *certificate, SceUInt32 capacity,
			 void *data, SceUInt32 *length);
	int (*Finalize)(SslCertificate *certificate);
	int (*Prepare)(SslCertificate *certificate);
} SslCertificateBackendMethods;

typedef struct SslCertificateNameMethods {
	int (*Create)();
	int (*Destroy)(void *name);
	SceUIntPtr opaqueQueries[4];
	int (*Decode)(void *context, int mode, SceUInt32 length, const void *data,
		      SceUInt32 *consumed, void **result);
} SslCertificateNameMethods;

/* Shared method prefix; implementations extend this interface. */
typedef struct SslCertificateMethods {
	int (*CreateContext)(void *library, int flags, void **result);
	int (*DestroyContext)(void *context);
	int (*GetContextInfo)(void *context, int selector, void *result);
	int (*SetContextInfo)(void *context, int selector, void *value);
	int (*CreateCertificate)(void *context, int flags, void **result);
	int (*DestroyCertificate)(void *certificate);
	int (*SerializeCertificate)(SslCertificate *certificate, SceUInt32 capacity,
				    void *data, SceUInt32 *length);
	int (*NewCertificateFromData)(void *context, int mode, int type,
				      SceUInt32 length, const void *data,
				      SceUInt32 *consumed, void **result);
	int (*RetainCertificate)(SslCertificate *certificate, int mode);
	int (*GetCertificateInfo)(SslCertificate *certificate, int selector, void *result);
	int (*SetCertificateInfo)(SslCertificate *certificate, int selector, void *value);
	SceUIntPtr opaqueInterface[9];
	const SslCertificateNameMethods *nameMethods;
} SslCertificateMethods;

typedef struct SslCertificatePropertyValue {
	SceUInt32 value;
	SceUInt32 length;
	void *data;
} SslCertificatePropertyValue;

typedef struct SslCertificateIndexedData {
	SceUInt32 index;
	void *data;
	SceUInt32 length;
} SslCertificateIndexedData;

typedef struct SslCertificateInfoRequest {
	int selector;
	void *result;
} SslCertificateInfoRequest;

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
	const SslCertificateBackendMethods *backendMethods;
	int references;
} SslCertificateContext;

typedef struct SslCertificateName {
	const SslObjectMethods *methods;
	SslCertificateContext *context;
	SslPropertyList *properties;
	SceUInt32 flags;
	const SslCertificateBackendMethods *backendMethods;
} SslCertificateName;

typedef struct SslCertificateKeyCache {
	SceUInt32 flags;
	int algorithm;
	void *key;
} SslCertificateKeyCache;

/* Shared cache prefix used by cleanup and dirty-state tracking. */
typedef struct SslCertificatePropertyCache {
	SslPropertyList *properties;
	SceUInt32 flags;
} SslCertificatePropertyCache;

struct SslCertificate {
	const SslCertificateMethods *methods;
	SslCertificateContext *context;
	const SslCertificateBackendMethods *backendMethods;
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
};

_Static_assert(offsetof(SslCertificateBackendMethods, ExportProperties) == 16,
	       "ExportProperties");
_Static_assert(offsetof(SslCertificateBackendMethods, ImportProperties) == 20,
	       "ImportProperties");
_Static_assert(offsetof(SslCertificateBackendMethods, EncodeVersion) == 44,
	       "EncodeVersion");
_Static_assert(offsetof(SslCertificateBackendMethods, Parse) == 60, "Parse");
_Static_assert(offsetof(SslCertificateBackendMethods, Serialize) == 64, "Serialize");
_Static_assert(offsetof(SslCertificateBackendMethods, Prepare) == 72, "Prepare");
_Static_assert(offsetof(SslCertificatePropertyCache, flags) == 4, "flags");

_Static_assert(offsetof(SslCertificateMethods, DestroyCertificate) == 20,
	       "DestroyCertificate");
_Static_assert(offsetof(SslCertificateMethods, GetCertificateInfo) == 36,
	       "GetCertificateInfo");
_Static_assert(offsetof(SslCertificateMethods, nameMethods) == 80, "nameMethods");
_Static_assert(offsetof(SslCertificateNameMethods, Decode) == 24, "Decode");
_Static_assert(offsetof(SslCertificateBackendMethods, QuerySignedValue) == 36,
	       "QuerySignedValue");
_Static_assert(offsetof(SslCertificateBackendMethods, QueryNonzeroValue) == 48,
	       "QueryNonzeroValue");
_Static_assert(sizeof(SslCertificatePropertyValue) == 12, "SslCertificatePropertyValue");
_Static_assert(sizeof(SslCertificateIndexedData) == 12, "SslCertificateIndexedData");
_Static_assert(sizeof(SslCertificateInfoRequest) == 8, "SslCertificateInfoRequest");
_Static_assert(sizeof(SslCertificateContext) == 40, "SslCertificateContext");
_Static_assert(offsetof(SslCertificateContext, references) == 36, "references");
_Static_assert(sizeof(SslCertificateName) == 20, "SslCertificateName");
_Static_assert(sizeof(SslCertificateKeyCache) == 12, "SslCertificateKeyCache");
_Static_assert(sizeof(SslCertificate) == 84, "SslCertificate");
_Static_assert(offsetof(SslCertificate, publicKey) == 36, "publicKey");
_Static_assert(offsetof(SslCertificate, attributes) == 56, "attributes");

int sslFreeCertificateContext(void *object);
int sslApplyCertificateProperties(SslCertificate *certificate,
				 SslPropertyList *properties);
int sslCanonicalizeCertificate(SslCertificate *certificate);
int sslGetCertificateProperty(SslCertificate *certificate, SceUInt32 selector,
			       void *result);
int sslValidateCriticalCertificateExtensions(SslCertificate *certificate);
int sslGetCertificateMethods(SslCertificate *certificate, void *result);
int sslGetCertificateName(SslCertificate *certificate, int selector, int mode,
			  void **result);
int sslGetCertificatePublicKey(SslCertificate *certificate, int mode, void **result);
int sslGetCertificateInfo(SslCertificate *certificate, int selector, void *result);
int sslFreeCertificateName(void *object);
int sslCacheCertificatePublicKey(SslCertificate *certificate, int mode, void *source);
int sslClearCertificateCache(SslCertificate *certificate, int selector);
int sslInvalidateCertificateProperties(SslCertificate *certificate);
int sslSetCertificateProperty(SslCertificate *certificate, int id,
			      SceUInt32 value, const void *data, SceUInt32 length);
int sslFreeCertificate(void *object);
int sslRetainCertificateContext(void *object);
int sslNewCertificate(void *object, int type, void **result);
int sslNewCertificateFromData(void *context, int mode, int type, SceUInt32 length,
			      const void *data, SceUInt32 *consumed, void **result);
int sslResetCertificate(SslCertificate *certificate, int mode, const void *data,
			SceUInt32 length);
int sslSetCertificateEncodedProperty(SslCertificate *certificate, int id,
				     SceUInt32 value, const void *data,
				     SceUInt32 length);
int sslCopyCertificateName(SslCertificateName *source, int mode,
			   SslCertificateName **result);
int sslSetCertificateSubject(SslCertificate *certificate, int mode,
			     SslCertificateName *source);
int sslSetCertificateIssuer(SslCertificate *certificate, int mode,
			    SslCertificateName *source);
int sslSetCertificatePublicKey(SslCertificate *certificate, int mode, void *source);
int sslSetCertificateVersion(SslCertificate *certificate, int version);
int sslNewCertificateContext(SslLibrary *library, int unused, SceUInt32 types,
			     void **result);
int sslGetCertificateContextInfo(void *object, int selector, void *result);
int sslSetCertificateContextInfo(void *object, int selector, void *value);
int sslRetainCertificate(SslCertificate *certificate, int mode);
int sslSerializeCertificate(SslCertificate *certificate, SceUInt32 capacity,
			    void *data, SceUInt32 *length);

#endif
