/* Filename and private names are ours; component follows MES/common/cert/src. */
#include "x509.h"
#include "../../../../asn1.h"
#include "../../../../memory.h"
#include "../../../../date.h"
#include "../../../../CRYPTOCME/common/asn1_props.h"

#define SSL_X509_COMMAND(opcode, first, second, third) \
	((SceUInt32)(opcode) | ((SceUInt32)(first) << 8) | \
	 ((SceUInt32)(second) << 16) | ((SceUInt32)(third) << 24))

static const SceUInt32 sslX509CertificateDecodingSchema[] = {
	/* Outer certificate and certificate-body SEQUENCEs. */
	SSL_X509_COMMAND(0x60, 0x10, 0x0, 0x0),
	SSL_X509_COMMAND(0x18, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x60, 0x10, 0x0, 0x0),
	SSL_X509_COMMAND(0x40, 0x46, 0x60, 0x40),
	SSL_X509_COMMAND(0x18, 0x0, 0x1, 0x0),
	/* Optional explicit version. */
	SSL_X509_COMMAND(0x6d, 0x0, 0x20, 0x20),
	SSL_X509_COMMAND(0x71, 0x80, 0x1, 0x0),
	SSL_X509_COMMAND(0x18, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x60, 0x2, 0x0, 0x0),
	SSL_X509_COMMAND(0x51, 0x80, 0x0, 0x0),
	SSL_X509_COMMAND(0x40, 0x43, 0x60, 0x1),
	SSL_X509_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x1a, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x20, 0x0, 0x1, 0x0),
	/* Serial number, signature algorithm and issuer. */
	SSL_X509_COMMAND(0x60, 0x2, 0x0, 0x0),
	SSL_X509_COMMAND(0x40, 0x42, 0x60, 0x2),
	SSL_X509_COMMAND(0x1a, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x40, 0x46, 0x60, 0x3),
	SSL_X509_COMMAND(0x1a, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x40, 0x46, 0x60, 0x4),
	SSL_X509_COMMAND(0x1a, 0x0, 0x1, 0x0),
	/* Validity accepts UTCTime or GeneralizedTime. */
	SSL_X509_COMMAND(0x60, 0x10, 0x0, 0x0),
	SSL_X509_COMMAND(0x18, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x68, 0x17, 0x0, 0x0),
	SSL_X509_COMMAND(0x70, 0x80, 0x2, 0x0),
	SSL_X509_COMMAND(0x68, 0x18, 0x0, 0x0),
	SSL_X509_COMMAND(0x70, 0x80, 0x2, 0x0),
	SSL_X509_COMMAND(0x1, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x20, 0x0, 0x2, 0x0),
	SSL_X509_COMMAND(0x40, 0x42, 0x60, 0x5),
	SSL_X509_COMMAND(0x1a, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x68, 0x17, 0x0, 0x0),
	SSL_X509_COMMAND(0x70, 0x80, 0x3, 0x0),
	SSL_X509_COMMAND(0x68, 0x18, 0x0, 0x0),
	SSL_X509_COMMAND(0x70, 0x80, 0x3, 0x0),
	SSL_X509_COMMAND(0x1, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x20, 0x0, 0x3, 0x0),
	/* Subject and SubjectPublicKeyInfo. */
	SSL_X509_COMMAND(0x40, 0x42, 0x60, 0x6),
	SSL_X509_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x1a, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x40, 0x46, 0x60, 0x7),
	SSL_X509_COMMAND(0x1a, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x60, 0x10, 0x0, 0x0),
	SSL_X509_COMMAND(0x40, 0x46, 0x60, 0x9),
	SSL_X509_COMMAND(0x18, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x60, 0x10, 0x0, 0x0),
	SSL_X509_COMMAND(0x18, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x60, 0x6, 0x0, 0x0),
	SSL_X509_COMMAND(0x40, 0x42, 0x30, 0x1),
	SSL_X509_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x19, 0x0, 0x1, 0x0),
	/* Optional issuer and subject identifiers, then extensions. */
	SSL_X509_COMMAND(0x1a, 0x84, 0x1, 0x0),
	SSL_X509_COMMAND(0x70, 0x84, 0x4, 0x0),
	SSL_X509_COMMAND(0x6d, 0x1, 0x0, 0x0),
	SSL_X509_COMMAND(0x71, 0x80, 0x5, 0x0),
	SSL_X509_COMMAND(0x40, 0x42, 0x60, 0xa),
	SSL_X509_COMMAND(0x1a, 0x84, 0x1, 0x0),
	SSL_X509_COMMAND(0x70, 0x84, 0x4, 0x0),
	SSL_X509_COMMAND(0x20, 0x0, 0x5, 0x0),
	SSL_X509_COMMAND(0x6d, 0x2, 0x0, 0x0),
	SSL_X509_COMMAND(0x71, 0x80, 0x6, 0x0),
	SSL_X509_COMMAND(0x40, 0x42, 0x60, 0xb),
	SSL_X509_COMMAND(0x1a, 0x84, 0x1, 0x0),
	SSL_X509_COMMAND(0x70, 0x84, 0x4, 0x0),
	SSL_X509_COMMAND(0x20, 0x0, 0x6, 0x0),
	SSL_X509_COMMAND(0x6d, 0x3, 0x20, 0x20),
	SSL_X509_COMMAND(0x71, 0x80, 0x7, 0x0),
	SSL_X509_COMMAND(0x40, 0x42, 0x60, 0xc),
	SSL_X509_COMMAND(0x20, 0x0, 0x7, 0x0),
	/* Finish the body and retain the signature algorithm encoding. */
	SSL_X509_COMMAND(0x70, 0x80, 0x4, 0x0),
	SSL_X509_COMMAND(0x1, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x20, 0x0, 0x4, 0x0),
	SSL_X509_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x1a, 0x0, 0x1, 0x0),
	/* Signature algorithm OID and optional NULL parameters. */
	SSL_X509_COMMAND(0x40, 0x46, 0x60, 0xd),
	SSL_X509_COMMAND(0x60, 0x10, 0x0, 0x0),
	SSL_X509_COMMAND(0x18, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x60, 0x6, 0x0, 0x0),
	SSL_X509_COMMAND(0x40, 0x42, 0x13, 0x15),
	SSL_X509_COMMAND(0x1a, 0x84, 0x1, 0x0),
	SSL_X509_COMMAND(0x70, 0x84, 0x8, 0x0),
	SSL_X509_COMMAND(0x60, 0x5, 0x0, 0x0),
	SSL_X509_COMMAND(0x20, 0x0, 0x8, 0x0),
	SSL_X509_COMMAND(0x19, 0x0, 0x1, 0x0),
	/* Signature BIT STRING, retained both as encoded and decoded properties. */
	SSL_X509_COMMAND(0x1a, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x60, 0x3, 0x0, 0x0),
	SSL_X509_COMMAND(0x41, 0x42, 0x60, 0xe),
	SSL_X509_COMMAND(0x41, 0x42, 0x13, 0x10),
	SSL_X509_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_X509_COMMAND(0x1, 0x0, 0x0, 0x0),
	0
};

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

static const char sslX509RsaEncryptionOid[] =
	"\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01";

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

int sslParseX509Certificate(SslCertificate *certificate, SceUInt32 flags,
			    SceUInt32 length, const void *data, SceUInt32 *consumed)
{
	const void *encoding = data;
	SslProperty *original, *signature, *algorithm;
	SslAsn1Element element = {0};
	SceUInt32 decoded = 0, total;
	SceUInt8 *buffer, *tail;
	int header, status;

	if (sslAddPropertyValue(certificate->properties, 96, 127, 0, data, length,
				flags) != 0)
		return 10005;
	if (flags & SSL_PROPERTY_COPY_DATA) {
		if (sslFindProperty(certificate->properties, 96, 127, NULL, &original, 0) != 0)
			return 10008;
		encoding = original->data;
	}
	status = sslDecodeX509CertificateProperties(certificate->properties, encoding,
						   length, &decoded);
	if (status != 0)
		return status;
	if (consumed != NULL)
		*consumed = decoded;
	certificate->encodedSize = decoded;
	if (sslFindProperty(certificate->properties, 19, 16, NULL, &signature, 0) != 0)
		return 10001;
	if (sslFindProperty(certificate->properties, 96, 13, NULL, &algorithm, 0) != 0)
		return 10001;
	element.data = signature->data;
	element.tag = 3;
	element.length = signature->length + 1u;
	element.flags = 8;
	header = sslAsn1HeaderSize(&element);
	total = algorithm->length + (SceUInt32)header + signature->length + 1u;
	buffer = sslAllocate(total);
	if (buffer == NULL)
		return 10005;
	sslCopyMemory(buffer, algorithm->data, algorithm->length);
	tail = buffer + algorithm->length;
	header = sslAsn1EncodeHeader(&element, tail);
	tail[header] = 0;
	sslCopyMemory(tail + header + 1, signature->data, signature->length);
	status = sslAddPropertyValue(certificate->properties, 96, 65, 0, buffer, total,
				     SSL_PROPERTY_COPY_DATA) == 0 ? 0 : 10005;
	sslFree(buffer);
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

int sslGetX509SignatureEncoding(SslCertificate *certificate, void *result)
{
	SslX509SignatureEncoding *encoding = result;
	SslProperty *property;
	SslPropertyList temporary;
	SceUInt32 consumed = 0;
	const void *data;
	int status;

	if (sslFindProperty(certificate->properties, 19, 21, NULL, &property, 0) != 0) {
		if (sslFindProperty(certificate->properties, 96, 65, NULL, &property, 0) != 0)
			return 10008;
		sslInitPropertyList(&temporary);
		status = sslDecodeSignatureProperties(&temporary, property->data,
						      property->length, &consumed);
		if (status == 0 && sslFindProperty(&temporary, 19, 21, NULL, &property, 0) != 0)
			status = 10008;
		sslFreePropertyList(&temporary);
		if (status != 0)
			return status;
	}
	/* The native fallback inspects the selected property after list cleanup. */
	data = property->data;
	encoding->length = property->length;
	encoding->data = data;
	return 0;
}

#define SSL_MATCH_SIGNATURE_OID(encoded, size, algorithm) \
	do { \
		if (length == (size) && sslCompareMemory(oid, (encoded), (size)) == 0) { \
			*result = (algorithm); \
			return 0; \
		} \
	} while (0)

int sslMapX509SignatureAlgorithm(const void *oid, SceUInt32 length, int *result)
{
	SSL_MATCH_SIGNATURE_OID("\x2a\x86\x48\x86\xf7\x0d\x01\x01\x02", 9, 7);
	SSL_MATCH_SIGNATURE_OID("\x2a\x86\x48\x86\xf7\x0d\x01\x01\x04", 9, 8);
	SSL_MATCH_SIGNATURE_OID("\x2b\x0e\x03\x02\x03", 5, 8);
	SSL_MATCH_SIGNATURE_OID("\x2b\x0e\x03\x02\x0f", 5, 65);
	SSL_MATCH_SIGNATURE_OID("\x2a\x86\x48\x86\xf7\x0d\x01\x01\x05", 9, 65);
	SSL_MATCH_SIGNATURE_OID("\x2b\x0e\x03\x02\x1d", 5, 65);
	SSL_MATCH_SIGNATURE_OID("\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0e", 9, 133);
	SSL_MATCH_SIGNATURE_OID("\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b", 9, 130);
	SSL_MATCH_SIGNATURE_OID("\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0c", 9, 131);
	SSL_MATCH_SIGNATURE_OID("\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0d", 9, 132);
	*result = -1;
	return 0;
}

#undef SSL_MATCH_SIGNATURE_OID

int sslGetX509SignatureAlgorithm(SslCertificate *certificate)
{
	SslProperty *property;
	SslPropertyList temporary;
	SceUInt32 consumed = 0;
	int algorithm = -1, status;

	if (sslFindProperty(certificate->properties, 19, 21, NULL, &property, 0) != 0) {
		if (sslFindProperty(certificate->properties, 96, 65, NULL, &property, 0) != 0)
			return algorithm;
		sslInitPropertyList(&temporary);
		status = sslDecodeSignatureProperties(&temporary, property->data,
						      property->length, &consumed);
		if (status == 0 && sslFindProperty(&temporary, 19, 21, NULL, &property, 0) != 0)
			status = 10008;
		sslFreePropertyList(&temporary);
		if (status != 0)
			return algorithm;
	}
	sslMapX509SignatureAlgorithm(property->data, property->length, &algorithm);
	return algorithm;
}

int sslEncodeX509PublicKey(void *unused, void *key, void **data,
			  SceUInt32 *length, SceUInt32 capacity)
{
	SslX509KeyExport export;
	SslPropertyList *properties;
	SceUInt8 *parameters = NULL, *encoded = NULL;
	SceUInt32 encodedLength, oidLength;
	int algorithm, status;

	(void)unused;
	if (key == NULL || length == NULL)
		return 10017;
	properties = sslNewPropertyList();
	if (properties == NULL)
		return 10005;
	export.properties = properties;
	algorithm = sslGetPublicKeyAlgorithm(key);
	export.algorithm = algorithm;
	status = sslEnumerateKeyComponents(key, sslCollectX509KeyComponent, &export);
	if (status != 0)
		goto out;
	if (algorithm != 6) {
		status = 10011;
		goto out;
	}
	parameters = sslAllocate(2);
	if (parameters == NULL) {
		status = 10005;
		goto out;
	}
	parameters[1] = 0;
	parameters[0] = 5;
	status = sslEncodeRsaPublicKeyProperties(properties, NULL, &encodedLength, 0);
	if (status != 0)
		goto out;
	encoded = sslAllocate(encodedLength);
	if (encoded == NULL) {
		status = 10005;
		goto out;
	}
	status = sslEncodeRsaPublicKeyProperties(properties, encoded, &encodedLength,
						encodedLength);
	if (status != 0)
		goto out;
	if (sslAddPropertyValue(properties, 48, 2, 16, parameters, 2,
				SSL_PROPERTY_COPY_DATA) != 0 ||
	    sslAddPropertyValue(properties, 48, 3, 3, encoded, encodedLength,
				SSL_PROPERTY_COPY_DATA) != 0) {
		status = 10001;
		goto out;
	}
	oidLength = sslStringLength(sslX509RsaEncryptionOid);
	if (sslAddPropertyValue(properties, 48, 1, 6, sslX509RsaEncryptionOid, oidLength,
				SSL_PROPERTY_COPY_DATA) != 0) {
		status = 10001;
		goto out;
	}
	status = sslEncodeSubjectPublicKeyInfoProperties(properties, data != NULL ? *data : NULL,
							length, capacity);
out:
	if (encoded != NULL)
		sslFree(encoded);
	if (parameters != NULL)
		sslFree(parameters);
	sslFreePropertyList(properties);
	return status;
}

int sslUpdateX509Properties(SslCertificate *certificate, SslPropertyList *properties)
{
	SslCertificateName *name;
	SslCertificateKeyCache *cache;
	void *key, *buffer;
	SceUInt32 length;
	int index, status;

	for (index = 0; index < 2; index++) {
		name = index == 0 ? certificate->issuer : certificate->subject;
		if (name == NULL || !(name->flags & 1))
			continue;
		status = sslEncodeCertificateNameProperties(name->properties, NULL, &length, 0);
		if (status != 0)
			return status;
		buffer = sslAllocate(length);
		if (buffer == NULL)
			return 10005;
		status = sslEncodeCertificateNameProperties(name->properties, buffer, &length, length);
		if (status == 0 && sslAddPropertyValue(properties, 96, index == 0 ? 4 : 7, 0,
						      buffer, length,
						      SSL_PROPERTY_COPY_DATA | SSL_PROPERTY_REPLACE) != 0)
			status = 10001;
		sslFree(buffer);
		if (status != 0)
			return status;
	}
	cache = certificate->publicKey;
	if (cache == NULL || !(cache->flags & 1))
		return 0;
	key = cache->key;
	status = sslEncodeX509PublicKey(NULL, key, NULL, &length, 0);
	if (status != 0)
		return status;
	buffer = sslAllocate(length);
	if (buffer == NULL)
		return 10005;
	status = sslEncodeX509PublicKey(NULL, key, &buffer, &length, length);
	if (status == 0 && sslAddPropertyValue(properties, 96, 9, 0, buffer, length,
					      SSL_PROPERTY_COPY_DATA | SSL_PROPERTY_REPLACE) != 0)
		status = 10001;
	sslFree(buffer);
	return status;
}

int sslParseX509PublicKey(void *unused, void *context, void **result,
			  const void *data, SceUInt32 length, int algorithm, int mode)
{
	SslPropertyList *properties = NULL;
	SslPropertyList *components = NULL;
	SslProperty *keyBits, *oid;
	SslKeyComponent component;
	SceUInt32 consumed = 0;
	void *key = *result;
	int status, i;

	if (key == NULL) {
		status = sslCreatePublicKey(context, algorithm, &key);
		if (status != 0)
			goto failed;
	}
	properties = sslNewPropertyList();
	if (properties == NULL) {
		status = 10005;
		goto failed;
	}
	status = sslDecodeSubjectPublicKeyInfoProperties(properties, data, length, &consumed);
	if (status != 0)
		goto failed;
	if (mode == 0 && sslPackProperties(properties, 0) != 0) {
		status = 10005;
		goto failed;
	}
	if (sslFindProperty(properties, 48, 3, NULL, &keyBits, 0) != 0 ||
	    sslFindProperty(properties, 48, 1, NULL, &oid, 0) != 0) {
		status = 10008;
		goto failed;
	}
	components = sslNewPropertyList();
	if (components == NULL) {
		status = 10005;
		goto failed;
	}
	if (oid->length != 9 || sslCompareMemory(oid->data, sslX509RsaEncryptionOid, 9) != 0) {
		status = SSL_CRYPTO_ERROR_NO_OPERATION;
		goto failed;
	}
	status = sslDecodeRsaPublicKeyProperties(components, (SceUInt8 *)keyBits->data + 1,
					       keyBits->length - 1, &consumed);
	if (status != 0)
		goto failed;
	for (i = 0; i < components->count; i++) {
		keyBits = components->entries[i];
		component.length = keyBits->length;
		component.data = keyBits->data;
		status = sslSetPublicKeyInfo(key, keyBits->id, &component);
		if (status != 0)
			goto failed;
	}
	if (*result == NULL)
		*result = key;
	goto out;

failed:
	if (key != NULL) {
		SceSslInternal_46358909(key);
		*result = NULL;
	}
out:
	if (properties != NULL)
		sslFreePropertyList(properties);
	if (components != NULL)
		sslFreePropertyList(components);
	return status;
}

int sslDecodeX509PublicKey(SslCertificate *certificate, SslProperty *property,
			   int mode, void **result)
{
	SslPropertyList properties;
	SslProperty *oid;
	void *context = NULL;
	int status;

	sslInitPropertyList(&properties);
	status = sslDecodeSubjectPublicKeyInfoProperties(&properties, property->data,
							property->length, NULL);
	if (status == 0) {
		if (sslFindProperty(&properties, 48, 1, NULL, &oid, 0) != 0 ||
		    oid->length != 9 ||
		    sslCompareMemory(oid->data, sslX509RsaEncryptionOid, 9) != 0) {
			status = 10001;
		} else {
			context = certificate->context->keyContext;
			sslRetainKeyContext(context);
			status = sslParseX509PublicKey(certificate, context, result,
						      property->data, property->length, 6, mode);
		}
	}
	sslDestroyKeyContext(context);
	sslFreePropertyList(&properties);
	return status;
}

static const SslCertificateBackendMethods sslX509BackendMethods = {
	.ExportProperties = sslExportX509Properties,
	.ImportProperties = sslImportX509Properties,
	.DecodePublicKey = sslDecodeX509PublicKey,
	.GetSignatureAlgorithm = sslGetX509SignatureAlgorithm,
	.GetSignatureAlgorithmEncoding = sslGetX509SignatureEncoding,
	.DecodeVersion = sslDecodeX509Version,
	.EncodeVersion = sslEncodeX509Version,
	.Parse = sslParseX509Certificate,
	.Serialize = sslSerializeX509Certificate,
	.Finalize = sslFinalizeX509Certificate,
	.GetNameComponentCount = sslGetX509NameComponentCount,
	.GetNameComponent = sslGetX509NameComponent
};

const SslCertificateBackendMethods *sslGetX509BackendMethods(void)
{
	return &sslX509BackendMethods;
}

int sslNewX509CertificateContext(SslLibrary *library, int unused, void **result)
{
	int status;

	status = sslNewCertificateContext(library, unused, 1, result);
	if (status == 0)
		((SslCertificateContext *)*result)->backendMethods = &sslX509BackendMethods;
	return status;
}

int sslExportX509Properties(SslCertificate *certificate, SslPropertyList *properties)
{
	void *buffer;
	SceUInt32 length;
	int status;

	status = sslUpdateX509Properties(certificate, properties);
	if (status != 0)
		return status;
	status = sslEncodeX509BodyProperties(properties, NULL, &length, 0);
	if (status != 0)
		return status;
	buffer = sslAllocate(length);
	if (buffer == NULL)
		return 10005;
	status = sslEncodeX509BodyProperties(properties, buffer, &length, length);
	if (status == 0 && sslAddPropertyValue(properties, 96, 64, 0, buffer, length,
					      SSL_PROPERTY_COPY_DATA | SSL_PROPERTY_REPLACE) != 0)
		status = 10001;
	sslFree(buffer);
	return status;
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

int sslDecodeX509Date(void *unused, const SslCertificatePropertyValue *value, void *date)
{
	int format;

	switch (value->value) {
	case 2:
		format = SSL_TIME_FORMAT_SECONDS;
		break;
	case 23:
		format = 1;
		break;
	case 24:
		format = 2;
		break;
	default:
		return 10022;
	}
	return SceSslInternal_88391E03(date, format, value->data, value->length);
}

int sslGetX509NameComponentCount(SslCertificateName *name, SceUInt32 *result)
{
	SslProperty *property;
	SceUInt32 count = 0;

	if (sslFindProperty(name->properties, 104, 15, NULL, &property, 0) == 0)
		count = property->length;
	*result = count;
	return 0;
}

int sslGetX509NameComponent(SslCertificateName *name, SceUInt32 index,
			   SslCertificateNameComponent **result)
{
	SslCertificateNameComponent *component;
	SslProperty *property;
	SceUInt32 offset = index * 2;
	int status;

	if (sslFindProperty(name->properties, 104, (int)(offset + 16), NULL,
			    &property, 0) != 0)
		return 10008;
	status = sslNewCertificateNameComponent(name, &component);
	if (status != 0)
		return status;
	if (sslCloneProperty(component->oid, property, SSL_PROPERTY_COPY_DATA) == NULL) {
		status = 10001;
		goto out;
	}
	if (sslFindProperty(name->properties, 104, (int)(offset + 17), NULL,
			    &property, 0) != 0) {
		status = 10001;
		goto out;
	}
	if (sslCloneProperty(component->value, property, SSL_PROPERTY_COPY_DATA) == NULL) {
		status = 10001;
		goto out;
	}
	*result = component;
	return status;
out:
	if (component != NULL)
		sslFreeCertificateNameComponent(component);
	return status;
}

int sslCollectX509KeyComponent(void *arg, int id, const SslKeyComponent *component)
{
	SslX509KeyExport *export = arg;

	if (export->algorithm != 6)
		return 10021;
	if (sslAddPropertyValue(export->properties, 16, id, 0, component->data,
				component->length, SSL_PROPERTY_COPY_DATA) != 0)
		return 10001;
	return 0;
}

int sslDecodeX509CertificateProperties(SslPropertyList *properties, const SceUInt8 *data,
				       SceUInt32 length, SceUInt32 *consumed)
{
	return sslAsn1DecodeProperties(sslX509CertificateDecodingSchema, properties,
				       data, length, consumed);
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
