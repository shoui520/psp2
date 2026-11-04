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

int sslFindCertificateExtensionType(const void *oid, SceUInt32 length, int *result);
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
