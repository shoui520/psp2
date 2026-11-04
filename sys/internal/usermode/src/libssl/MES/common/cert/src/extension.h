/* Filename and descriptive private names are ours. */
#ifndef LIBSSL_CERT_EXTENSION_H
#define LIBSSL_CERT_EXTENSION_H

#include "../../../../CRYPTOCME/common/object.h"
#include "../../../../CRYPTOCME/common/property.h"

#define SSL_CERT_EXTENSION_COUNT 25
#define SSL_CERT_EXTENSION_FIRST 0x4000
#define SSL_CERT_EXTENSION_CUSTOM (-1)
#define SSL_CERT_EXTENSION_VALUE 1
#define SSL_CERT_EXTENSION_OID 2
#define SSL_CERT_EXTENSION_OWNS_VALUE 2
#define SSL_CERT_EXTENSION_OWNS_OID 4

typedef struct SslCertificateExtensionData {
	SceUInt32 length;
	void *data;
} SslCertificateExtensionData;

typedef struct SslCertificateExtension {
	const SslObjectMethods *methods;
	SceUInt32 flags;
	int index;
	int critical;
	int type;
	SslCertificateExtensionData value;
	SslCertificateExtensionData oid;
	SslPropertyList *properties;
} SslCertificateExtension;

_Static_assert(sizeof(SslCertificateExtensionData) == 8,
	       "SslCertificateExtensionData");
_Static_assert(sizeof(SslCertificateExtension) == 40, "SslCertificateExtension");
_Static_assert(offsetof(SslCertificateExtension, value) == 20, "value");
_Static_assert(offsetof(SslCertificateExtension, oid) == 28, "oid");
_Static_assert(offsetof(SslCertificateExtension, properties) == 36, "properties");

extern const char *const sslCertificateExtensionOids[SSL_CERT_EXTENSION_COUNT];

int sslSetOcspAcceptableResponses(SslCertificateExtension *extension, int formats);
int sslGetOcspAcceptableResponses(SslCertificateExtension *extension, SceUInt32 *result);
int sslSetCertificateKeyUsage(SslCertificateExtension *extension, int usage);
int sslGetCertificateKeyUsage(SslCertificateExtension *extension, int *result);
int sslSetCertificateExtensionInfo(SslCertificateExtension *extension,
				   int selector, const void *value);
int sslGetCertificateExtensionReason(SslCertificateExtension *extension, int *result);
int sslGetCertificateAuthorityKeyIdentifier(SslCertificateExtension *extension,
					  int selector,
					  SslCertificateExtensionData *result);
int sslDecodeAuthorityKeyIdentifier(SslPropertyList *list, const unsigned char *data,
				    SceUInt32 size, SceUInt32 *consumed);
int SceSslInternal_93EAE08A(SslCertificateExtension *extension, int selector, void *result);
int sslGetCertificateGeneralName(SslCertificateExtension *extension,
				 int selector, int base,
				 SslCertificateExtensionData *result);
int sslGetCertificateBasicConstraint(SslCertificateExtension *extension,
				     int selector, int *result);
int SceSslInternal_BF99D664(const SslObjectMethods *methods, int unused,
			  SslCertificateExtension **result);

int sslFindCertificateExtensionType(const void *oid, SceUInt32 length, int *result);
int sslFindCertificateExtensionPropertyType(const void *oid, SceUInt32 length,
					  int *result);
int sslGetCertificateExtensionOid(int type, const char **oid, SceUInt32 *length);
int sslSetCertificateExtensionData(SslCertificateExtension *extension,
				   int selector, int mode,
				   const SslCertificateExtensionData *value);
int sslGetCertificateExtension(SslPropertyList *list, int mode,
			       SslCertificateExtension *extension);
int sslGetCertificateExtensionCount(SslPropertyList *list, SceUInt32 *result);
int sslFindCertificateExtension(SslPropertyList *list, int mode,
				SslCertificateExtension *extension);
int sslFindCertificateExtensionByType(SslPropertyList *list, int type, int mode,
				      SslCertificateExtension *extension);
int sslStoreCertificateExtension(SslPropertyList *list, int mode,
				 SslCertificateExtension *extension);

#endif
