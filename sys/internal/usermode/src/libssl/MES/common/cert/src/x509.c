/* Filename and private names are ours; component follows MES/common/cert/src. */
#include "x509.h"
#include "../../../../asn1.h"

#define SSL_X509_COMMAND(opcode, first, second, third) \
	((SceUInt32)(opcode) | ((SceUInt32)(first) << 8) | \
	 ((SceUInt32)(second) << 16) | ((SceUInt32)(third) << 24))

static const SceUInt32 sslX509CertificateEncodingSchema[] = {
	/* The outer SEQUENCE contains the encoded body and signature properties. */
	SSL_X509_COMMAND(0x32, 0x0, 0x10, 0x0),
	SSL_X509_COMMAND(0x11, 0x0, 0x0, 0x0),
	SSL_X509_COMMAND(0x35, 0x60, 0x40, 0x0),
	SSL_X509_COMMAND(0x35, 0x60, 0x41, 0x0),
	SSL_X509_COMMAND(0x1, 0x0, 0x0, 0x0),
	0
};

static const SceUInt32 sslX509BodyEncodingSchema[] = {
	/* Body SEQUENCE; include the explicit version only when present. */
	SSL_X509_COMMAND(0x32, 0x0, 0x10, 0x0),
	SSL_X509_COMMAND(0x11, 0x0, 0x0, 0x0),
	SSL_X509_COMMAND(0x40, 0x40, 0x60, 0x1),
	SSL_X509_COMMAND(0x70, 0x80, 0x1, 0x0),
	SSL_X509_COMMAND(0x36, 0x0, 0x20, 0x0),
	SSL_X509_COMMAND(0x11, 0x0, 0x0, 0x0),
	SSL_X509_COMMAND(0x40, 0x44, 0x60, 0x1),
	SSL_X509_COMMAND(0x33, 0x80, 0x0, 0x0),
	SSL_X509_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x20, 0x0, 0x1, 0x0),
	/* Serial, signature algorithm, issuer, validity, subject and public key. */
	SSL_X509_COMMAND(0x37, 0x60, 0x2, 0x0),
	SSL_X509_COMMAND(0x35, 0x60, 0x3, 0x0),
	SSL_X509_COMMAND(0x35, 0x60, 0x4, 0x0),
	SSL_X509_COMMAND(0x32, 0x0, 0x10, 0x0),
	SSL_X509_COMMAND(0x11, 0x0, 0x0, 0x0),
	SSL_X509_COMMAND(0x37, 0x60, 0x5, 0x0),
	SSL_X509_COMMAND(0x37, 0x60, 0x6, 0x0),
	SSL_X509_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x35, 0x60, 0x7, 0x0),
	SSL_X509_COMMAND(0x35, 0x60, 0x9, 0x0),
	/* Optional issuer/subject identifiers and extensions. */
	SSL_X509_COMMAND(0x40, 0x40, 0x60, 0xa),
	SSL_X509_COMMAND(0x70, 0x80, 0x3, 0x0),
	SSL_X509_COMMAND(0x36, 0x1, 0x20, 0x0),
	SSL_X509_COMMAND(0x11, 0x0, 0x0, 0x0),
	SSL_X509_COMMAND(0x35, 0x60, 0xa, 0x0),
	SSL_X509_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x20, 0x0, 0x3, 0x0),
	SSL_X509_COMMAND(0x40, 0x40, 0x60, 0xb),
	SSL_X509_COMMAND(0x70, 0x80, 0x4, 0x0),
	SSL_X509_COMMAND(0x36, 0x2, 0x20, 0x0),
	SSL_X509_COMMAND(0x11, 0x0, 0x0, 0x0),
	SSL_X509_COMMAND(0x35, 0x60, 0xb, 0x0),
	SSL_X509_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x20, 0x0, 0x4, 0x0),
	SSL_X509_COMMAND(0x40, 0x40, 0x60, 0xc),
	SSL_X509_COMMAND(0x70, 0x80, 0x5, 0x0),
	SSL_X509_COMMAND(0x36, 0x3, 0x20, 0x0),
	SSL_X509_COMMAND(0x11, 0x0, 0x0, 0x0),
	SSL_X509_COMMAND(0x35, 0x60, 0xc, 0x0),
	SSL_X509_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x20, 0x0, 0x5, 0x0),
	SSL_X509_COMMAND(0x20, 0x0, 0x2, 0x0),
	SSL_X509_COMMAND(0x1, 0x0, 0x0, 0x0),
	0
};

static const SceUInt32 sslX509ExtensionDecodingSchema[] = {
	/* Extensions SEQUENCE, three property slots per extension. */
	SSL_X509_COMMAND(0x60, 0x10, 0x0, 0x0),
	SSL_X509_COMMAND(0x80, 0x85, 0x3, 0x0),
	SSL_X509_COMMAND(0x80, 0x86, 0x0, 0x0),
	SSL_X509_COMMAND(0x18, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x80, 0x88, 0x0, 0x0),
	SSL_X509_COMMAND(0x20, 0x0, 0x8, 0x0),
	/* Extension OID, optional critical BOOLEAN, and OCTET STRING value. */
	SSL_X509_COMMAND(0x60, 0x10, 0x0, 0x0),
	SSL_X509_COMMAND(0x18, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x60, 0x6, 0x0, 0x0),
	SSL_X509_COMMAND(0x80, 0x87, 0x86, 0x2),
	SSL_X509_COMMAND(0x40, 0x42, 0x6c, 0x87),
	SSL_X509_COMMAND(0x1a, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x68, 0x1, 0x0, 0x0),
	SSL_X509_COMMAND(0x71, 0x80, 0x9, 0x0),
	SSL_X509_COMMAND(0x80, 0x87, 0x86, 0x3),
	SSL_X509_COMMAND(0x40, 0x42, 0x6c, 0x87),
	SSL_X509_COMMAND(0x1a, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x20, 0x0, 0x9, 0x0),
	SSL_X509_COMMAND(0x60, 0x4, 0x0, 0x0),
	SSL_X509_COMMAND(0x80, 0x87, 0x86, 0x4),
	SSL_X509_COMMAND(0x40, 0x42, 0x6c, 0x87),
	SSL_X509_COMMAND(0x19, 0x0, 0x1, 0x0),
	/* Advance the slots and publish the extension count. */
	SSL_X509_COMMAND(0x80, 0x86, 0x86, 0x85),
	SSL_X509_COMMAND(0x80, 0x88, 0x88, 0x1),
	SSL_X509_COMMAND(0x1a, 0x84, 0x1, 0x0),
	SSL_X509_COMMAND(0x71, 0x84, 0x8, 0x0),
	SSL_X509_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x80, 0x80, 0x88, 0x0),
	SSL_X509_COMMAND(0x40, 0x43, 0x6c, 0x1),
	SSL_X509_COMMAND(0x1, 0x0, 0x0, 0x0),
	0
};

#undef SSL_X509_COMMAND

int sslSerializeX509Certificate(SslCertificate *certificate, SceUInt32 capacity,
			       void *data, SceUInt32 *length)
{
	SceUInt32 encoded = 0;
	int status;

	status = sslEncodeX509CertificateProperties(certificate->properties, data,
						   &encoded, capacity);
	if (status == 0)
		*length = encoded;
	return status;
}

int sslDecodeX509Extensions(SslPropertyList **properties, int mode, SceUInt32 length,
			    const void *data, SceUInt32 *consumed)
{
	SceUInt32 decoded;
	int created = 0, status;

	if (properties == NULL || data == NULL)
		return 10017;
	if (*properties == NULL) {
		*properties = sslNewPropertyList();
		if (*properties == NULL)
			return 10005;
		created = 1;
	}
	status = sslDecodeX509ExtensionProperties(*properties, data, length, &decoded);
	if (status == 0) {
		if (consumed != NULL)
			*consumed = decoded;
		if (created && mode == 0 && sslPackProperties(*properties, 0) != 0)
			status = 10001;
	}
	if (status != 0 && created && *properties != NULL)
		sslFreePropertyList(*properties);
	return status;
}

int sslFinalizeX509Certificate(SslCertificate *certificate)
{
	SslPropertyList *properties = &certificate->attributes;
	SslProperty *property;
	SceUInt32 consumed;

	sslFreePropertyList(properties);
	sslInitPropertyList(properties);
	if (sslFindProperty(certificate->properties, 96, 12, NULL, &property, 0) != 0)
		return 0;
	return sslDecodeX509Extensions(&properties, 1, property->length, property->data,
				       &consumed);
}

int sslDecodeX509Version(SslCertificate *certificate, int present,
			 SceUInt32 encoded, int *version)
{
	(void)certificate;
	if (!present) {
		*version = 1;
		return 0;
	}
	if (encoded > 2)
		return 10001;
	*version = encoded + 1u;
	return 0;
}

int sslEncodeX509Version(SslCertificate *certificate, int version,
			 SceUInt32 *encoded, SceUInt32 *tag)
{
	SceUInt32 value = (SceUInt32)version - 1u;

	(void)certificate;
	if (value > 2)
		return 10018;
	*encoded = value;
	*tag = 2;
	return 0;
}

int sslImportX509Properties(SslCertificate *certificate, SslPropertyList *properties)
{
	SslProperty *property;
	SceUInt32 length;
	int status;

	if (sslFindProperty(properties, 96, 65, NULL, &property, 0) != 0) {
		if (sslFindProperty(properties, 96, 64, NULL, &property, 0) != 0)
			return 10018;
		length = property->length;
	} else {
		status = sslEncodeX509CertificateProperties(properties, NULL, &length, 0);
		if (status != 0)
			return status;
	}
	sslFreePropertyList(certificate->properties);
	certificate->properties = properties;
	certificate->encodedSize = length;
	return sslFinalizeX509Certificate(certificate);
}

int sslEncodeX509CertificateProperties(SslPropertyList *properties, SceUInt8 *data,
				       SceUInt32 *length, SceUInt32 capacity)
{
	return sslAsn1EncodeProperties(sslX509CertificateEncodingSchema, properties,
				       data, length, capacity);
}

int sslEncodeX509BodyProperties(SslPropertyList *properties, SceUInt8 *data,
				SceUInt32 *length, SceUInt32 capacity)
{
	return sslAsn1EncodeProperties(sslX509BodyEncodingSchema, properties, data,
				       length, capacity);
}

int sslDecodeX509ExtensionProperties(SslPropertyList *properties, const SceUInt8 *data,
				     SceUInt32 length, SceUInt32 *consumed)
{
	return sslAsn1DecodeProperties(sslX509ExtensionDecodingSchema, properties,
				       data, length, consumed);
}
