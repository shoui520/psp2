/* Private descriptive names and this header filename are ours. */
#ifndef LIBSSL_RI_CERT_H
#define LIBSSL_RI_CERT_H

#include "../../../../CRYPTOCME/common/object.h"
#include "../../../../CRYPTOCME/common/property.h"
#include "../../../../CRYPTOCME/common/library/lib_ctx.h"
#include "../../../../objects/objects.h"

typedef struct SslCertificate SslCertificate;
typedef struct SslCertificateName SslCertificateName;
typedef struct SslCertificateNameComponent SslCertificateNameComponent;
typedef struct SslCertificatePropertyValue SslCertificatePropertyValue;

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
	int (*GetNameComponentCount)(SslCertificateName *name, SceUInt32 *result);
	int (*GetNameComponent)(SslCertificateName *name, SceUInt32 index,
				SslCertificateNameComponent **result);
} SslCertificateBackendMethods;

typedef struct SslCertificateKeyMethods {
	int (*VerifySignature)(SslCertificate *certificate, void *key,
			       void *cryptoContext);
	SceUIntPtr opaqueOperation;
	int (*GetPublicKey)(SslCertificate *certificate, int mode, void **result);
} SslCertificateKeyMethods;

typedef struct SslCertificateNameMethods {
	int (*Create)(void *context, void **result);
	int (*Destroy)(void *name);
	int (*Copy)(SslCertificateName *source, int mode, SslCertificateName **result);
	SceUIntPtr opaqueOperation;
	int (*Format)(SslCertificateName *name, int mode, SceUInt32 capacity, char *data);
	SceUIntPtr opaqueQuery;
	int (*Decode)(void *context, int mode, SceUInt32 length, const void *data,
		      SceUInt32 *consumed, void **result);
	int (*Encode)(SslCertificateName *name, SceUInt32 capacity, void *data,
		      SceUInt32 *length);
	int (*GetInfo)(SslCertificateName *name, int selector, void *result);
	SceUIntPtr opaqueSetter;
	int (*CreateComponent)(SslCertificateName *name,
			       SslCertificateNameComponent **result);
	int (*DestroyComponent)(SslCertificateNameComponent *component);
	int (*GetComponentInfo)(SslCertificateNameComponent *component, int selector,
				SslCertificatePropertyValue *result);
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
	int (*RetainCertificate)();
	int (*GetCertificateInfo)(SslCertificate *certificate, int selector, void *result);
	int (*SetCertificateInfo)(SslCertificate *certificate, int selector, void *value);
	SceUIntPtr opaqueOperation;
	int (*DecodeDate)(SslCertificate *certificate,
			  SslCertificatePropertyValue *encoded, void *result);
	SceUIntPtr opaqueInterface[6];
	const SslCertificateKeyMethods *keyMethods;
	const SslCertificateNameMethods *nameMethods;
} SslCertificateMethods;

struct SslCertificatePropertyValue {
	SceUInt32 value;
	SceUInt32 length;
	void *data;
};

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

struct SslCertificateName {
	const SslCertificateNameMethods *methods;
	SslCertificateContext *context;
	SslPropertyList *properties;
	SceUInt32 flags;
	const SslCertificateBackendMethods *backendMethods;
};

/* Component storage used by the name interface. Private type names are ours. */
struct SslCertificateNameComponent {
	const SslCertificateNameMethods *methods;
	SslCertificateContext *context;
	SslProperty *oid;
	SslProperty *value;
	SceUInt32 reserved;
};

typedef struct SslCertificateNameComponentRequest {
	SceInt32 index;
	SslCertificateNameComponent *component;
} SslCertificateNameComponentRequest;

typedef int (*SslCertificateNameLabelLookup)(const void *oid, SceUInt32 length,
					   const char **result);

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
_Static_assert(offsetof(SslCertificateBackendMethods, GetNameComponent) == 80,
	       "GetNameComponent");
_Static_assert(offsetof(SslCertificatePropertyCache, flags) == 4, "flags");

_Static_assert(offsetof(SslCertificateMethods, DestroyCertificate) == 20,
	       "DestroyCertificate");
_Static_assert(offsetof(SslCertificateMethods, GetCertificateInfo) == 36,
	       "GetCertificateInfo");
_Static_assert(offsetof(SslCertificateMethods, nameMethods) == 80, "nameMethods");
_Static_assert(offsetof(SslCertificateNameMethods, Decode) == 24, "Decode");
_Static_assert(offsetof(SslCertificateNameMethods, GetInfo) == 32, "GetInfo");
_Static_assert(offsetof(SslCertificateNameMethods, Encode) == 28, "Encode");
_Static_assert(offsetof(SslCertificateNameMethods, GetComponentInfo) == 48,
	       "GetComponentInfo");
_Static_assert(offsetof(SslCertificateNameComponent, value) == 12, "value");
_Static_assert(sizeof(SslCertificateNameComponent) == 20,
	       "SslCertificateNameComponent");
_Static_assert(sizeof(SslCertificateNameComponentRequest) == 8,
	       "SslCertificateNameComponentRequest");
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
int sslSetCertificateInfo(SslCertificate *certificate, int selector, void *value);
int sslNewCertificateName(SslCertificateContext *context, SslCertificateName **result);
int sslDecodeCertificateName(SslCertificateContext *context, int mode, SceUInt32 length,
			     const void *data, SceUInt32 *consumed, void **result);
int sslDecodeCertificateNameProperties(SslPropertyList *properties,
				       const SceUInt8 *data, SceUInt32 length,
				       SceUInt32 *consumed);
int sslEncodeCertificateNameProperties(SslPropertyList *properties, SceUInt8 *data,
				       SceUInt32 *length, SceUInt32 capacity);
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
int sslGetCertificateNameLabel(const void *oid, SceUInt32 length,
			       const char **result);
int sslFormatCertificateHex(const SslCertificatePropertyValue *value,
			    SceUInt32 capacity, int mode, char *data);
int sslFormatCertificateString(const SslCertificatePropertyValue *value,
			       SceUInt32 capacity, int mode, char *data);
int sslFormatCertificateAttribute(SslCertificateNameComponent *component, int mode,
				  SslCertificateNameLabelLookup lookup,
				  SceUInt32 capacity, char *data);
int sslFormatCertificateNameForward(SslCertificateName *name, int mode,
				    SceUInt32 capacity, char *data);
int sslFormatCertificateName(SslCertificateName *name, int mode,
			    SceUInt32 capacity, char *data);
int sslFreeCertificateNameComponent(SslCertificateNameComponent *component);
int sslNewCertificateNameComponent(SslCertificateName *name,
				   SslCertificateNameComponent **result);
int sslGetCertificateNameHash(SslCertificateName *name, int algorithm,
			       SceUInt32 *result);
int sslGetCertificateNameInfo(SslCertificateName *name, int selector, void *result);
int sslEncodeCertificateName(SslCertificateName *name, SceUInt32 capacity,
			     void *data, SceUInt32 *length);
int sslAllocateAndSerializeCertificate(SslCertificate *certificate, void **data,
				       SceUInt32 *length);
int sslCompareCertificates(SslCertificate *left, SslCertificate *right);
int sslAreCertificatesEqual(SslCertificate *left, SslCertificate *right);
int sslCompareCertificateNames(SslCertificateName *left,
			       SslCertificateName *right);
int sslCompareCertificateSubjects(SslCertificate *left, SslCertificate *right);
int sslAreCertificateSubjectsEqual(SslCertificate *left, SslCertificate *right);
int sslComputeCertificateDigest(SslCertificate *certificate, int selector,
				int algorithm, SceUInt32 capacity, void *digest,
				SceUInt32 *digestLength);
int sslGetCertificateDate(SslCertificate *certificate, int selector, void *result);
int sslSetObjectIdentifierByType(struct SslObjectIdentifier *object, int type);
int sslNewObjectIdentifierByType(void *unused, int type,
				 struct SslObjectIdentifier **result);
int sslValidateCertificateVersionFields(SslCertificate *certificate);
int sslCreateCertificateContext(SslLibrary *library, int parameter,
				SceUInt32 flags, SslObjectHeader **result);
int sslDestroyCertificateContext(void *context);
int sslSetCertificateContextOption(void *context, int selector, void *value);
int sslCreateCertificate(void *context, int flags, void **result);
int sslRetainCertificateObject(SslCertificate *certificate);
int sslDuplicateCertificateObject(SslCertificate *certificate);
int sslSetCertificateObjectInfo(SslCertificate *certificate, int selector,
				void *value);
int sslCreateCertificateNameObject(SslCertificateContext *context, void **result);
int sslCopyCertificateNameObject(SslCertificateName *source, int mode,
				 SslCertificateName **result);
int sslCreateCertificateNameComponentObject(SslCertificateName *name,
					    SslCertificateNameComponent **result);
int sslSerializeCertificateObject(SslCertificate *certificate, SceUInt32 capacity,
				  void *data, SceUInt32 *length);
int sslDecodeCertificateNameObject(SslCertificateContext *context, int mode,
				   SceUInt32 length, const void *data,
				   SceUInt32 *consumed, void **result);
int sslEncodeCertificateNameObject(SslCertificateName *name, SceUInt32 capacity,
				   void *data, SceUInt32 *length);

#endif
