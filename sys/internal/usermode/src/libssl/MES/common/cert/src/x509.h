/* Filename and descriptive private names are ours. */
#ifndef LIBSSL_X509_H
#define LIBSSL_X509_H

#include "ri_cert.h"

int sslSerializeX509Certificate(SslCertificate *certificate, SceUInt32 capacity,
			       void *data, SceUInt32 *length);
int sslDecodeX509Extensions(SslPropertyList **properties, int mode, SceUInt32 length,
			    const void *data, SceUInt32 *consumed);
int sslFinalizeX509Certificate(SslCertificate *certificate);
int sslDecodeX509Version(SslCertificate *certificate, int present,
			 SceUInt32 encoded, int *version);
int sslEncodeX509Version(SslCertificate *certificate, int version,
			 SceUInt32 *encoded, SceUInt32 *tag);
int sslImportX509Properties(SslCertificate *certificate, SslPropertyList *properties);
int sslEncodeX509CertificateProperties(SslPropertyList *properties, SceUInt8 *data,
				       SceUInt32 *length, SceUInt32 capacity);
int sslEncodeX509BodyProperties(SslPropertyList *properties, SceUInt8 *data,
				SceUInt32 *length, SceUInt32 capacity);
int sslDecodeX509ExtensionProperties(SslPropertyList *properties, const SceUInt8 *data,
				     SceUInt32 length, SceUInt32 *consumed);

#endif
