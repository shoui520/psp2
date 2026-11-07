/* Filename and descriptive private names are ours. */
#ifndef LIBSSL_X509_H
#define LIBSSL_X509_H

#include "ri_cert.h"
#include "../../../../CRYPTOCME/common/key/pkey_ctx.h"

typedef struct SslX509SignatureEncoding {
	SceUInt32 length;
	const void *data;
} SslX509SignatureEncoding;

typedef struct SslX509KeyExport {
	int algorithm;
	SslPropertyList *properties;
} SslX509KeyExport;

_Static_assert(sizeof(SslX509SignatureEncoding) == 8, "SslX509SignatureEncoding");
_Static_assert(sizeof(SslX509KeyExport) == 8, "SslX509KeyExport");

int sslEncodeX509PublicKey(void *unused, void *key, void **data,
			  SceUInt32 *length, SceUInt32 capacity);
int sslUpdateX509Properties(SslCertificate *certificate, SslPropertyList *properties);
int sslExportX509Properties(SslCertificate *certificate, SslPropertyList *properties);
int sslCollectX509KeyComponent(void *arg, int id, const SslKeyComponent *component);
int sslParseX509PublicKey(void *unused, void *context, void **result,
			  const void *data, SceUInt32 length, int algorithm, int mode);
int sslDecodeX509PublicKey(SslCertificate *certificate, SslProperty *property,
			   int mode, void **result);
const SslCertificateBackendMethods *sslGetX509BackendMethods(void);
int sslNewX509CertificateContext(SslLibrary *library, int unused, void **result);
int sslDecodeX509Date(void *unused, const SslCertificatePropertyValue *value, void *date);
int sslGetX509NameComponentCount(SslCertificateName *name, SceUInt32 *result);
int sslGetX509NameComponent(SslCertificateName *name, SceUInt32 index,
			   SslCertificateNameComponent **result);

int sslSerializeX509Certificate(SslCertificate *certificate, SceUInt32 capacity,
			       void *data, SceUInt32 *length);
int sslParseX509Certificate(SslCertificate *certificate, SceUInt32 flags,
			    SceUInt32 length, const void *data, SceUInt32 *consumed);
int sslGetX509SignatureEncoding(SslCertificate *certificate, void *result);
int sslMapX509SignatureAlgorithm(const void *oid, SceUInt32 length, int *result);
int sslGetX509SignatureAlgorithm(SslCertificate *certificate);
int sslDecodeX509Extensions(SslPropertyList **properties, int mode, SceUInt32 length,
			    const void *data, SceUInt32 *consumed);
int sslFinalizeX509Certificate(SslCertificate *certificate);
int sslDecodeX509Version(SslCertificate *certificate, int present,
			 SceUInt32 encoded, int *version);
int sslEncodeX509Version(SslCertificate *certificate, int version,
			 SceUInt32 *encoded, SceUInt32 *tag);
int sslImportX509Properties(SslCertificate *certificate, SslPropertyList *properties);
int sslDecodeX509CertificateProperties(SslPropertyList *properties, const SceUInt8 *data,
				       SceUInt32 length, SceUInt32 *consumed);
int sslEncodeX509CertificateProperties(SslPropertyList *properties, SceUInt8 *data,
				       SceUInt32 *length, SceUInt32 capacity);
int sslEncodeX509BodyProperties(SslPropertyList *properties, SceUInt8 *data,
				SceUInt32 *length, SceUInt32 capacity);
int sslDecodeX509ExtensionProperties(SslPropertyList *properties, const SceUInt8 *data,
				     SceUInt32 length, SceUInt32 *consumed);

#endif
