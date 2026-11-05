/* Filename is ours; common/cert/src follows recovered MES source paths. */
#include <stddef.h>
#include "../../../../CRYPTOCME/common/object.h"
#include <libssl/name.h>
#include "ri_cert.h"
#include "../../../../asn1.h"

#define SSL_ASN1_NAME_COMMAND(opcode, first, second, third) \
	((SceUInt32)(opcode) | ((SceUInt32)(first) << 8) | \
	 ((SceUInt32)(second) << 16) | ((SceUInt32)(third) << 24))

static const SceUInt32 sslCertificateNameEncodingSchema[] = {
	/* Name SEQUENCE and the two property slots per attribute. */
	SSL_ASN1_NAME_COMMAND(0x32, 0x0, 0x10, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x85, 0x2, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x86, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x11, 0x0, 0x0, 0x0),
	/* Look for the next relative distinguished name. */
	SSL_ASN1_NAME_COMMAND(0x20, 0x0, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x88, 0x68, 0x0),
	SSL_ASN1_NAME_COMMAND(0x92, 0x89, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x10, 0x0, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x87, 0x86, 0x89),
	SSL_ASN1_NAME_COMMAND(0x40, 0x40, 0x88, 0x87),
	SSL_ASN1_NAME_COMMAND(0x70, 0x80, 0x2, 0x0),
	/* SET and AttributeTypeAndValue SEQUENCE. */
	SSL_ASN1_NAME_COMMAND(0x32, 0x0, 0x11, 0x0),
	SSL_ASN1_NAME_COMMAND(0x11, 0x0, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x32, 0x0, 0x10, 0x0),
	SSL_ASN1_NAME_COMMAND(0x11, 0x0, 0x0, 0x0),
	/* Attribute OID, then its encoded value. */
	SSL_ASN1_NAME_COMMAND(0x80, 0x88, 0x68, 0x0),
	SSL_ASN1_NAME_COMMAND(0x92, 0x89, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x10, 0x0, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x87, 0x86, 0x89),
	SSL_ASN1_NAME_COMMAND(0x30, 0x6, 0x68, 0x87),
	SSL_ASN1_NAME_COMMAND(0x80, 0x88, 0x68, 0x0),
	SSL_ASN1_NAME_COMMAND(0x92, 0x89, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x11, 0x0, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x87, 0x86, 0x89),
	SSL_ASN1_NAME_COMMAND(0x37, 0x68, 0x87, 0x0),
	/* Return to the next relative distinguished name. */
	SSL_ASN1_NAME_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x86, 0x86, 0x85),
	SSL_ASN1_NAME_COMMAND(0x70, 0x0, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x20, 0x0, 0x2, 0x0),
	SSL_ASN1_NAME_COMMAND(0x1, 0x0, 0x0, 0x0),
	0
};

static const SceUInt32 sslCertificateNameSchema[] = {
	/* Name SEQUENCE and the initial property indices. */
	SSL_ASN1_NAME_COMMAND(0x60, 0x10, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x85, 0x2, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x86, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x18, 0x80, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x70, 0x80, 0x2, 0x0),
	SSL_ASN1_NAME_COMMAND(0x20, 0x0, 0x1, 0x0),
	/* Each relative distinguished name is a SET of attribute SEQUENCEs. */
	SSL_ASN1_NAME_COMMAND(0x60, 0x11, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x18, 0x0, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x20, 0x0, 0x3, 0x0),
	SSL_ASN1_NAME_COMMAND(0x60, 0x10, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x18, 0x0, 0x1, 0x0),
	/* Attribute OID and value become consecutive properties. */
	SSL_ASN1_NAME_COMMAND(0x60, 0x6, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x92, 0x88, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x10, 0x0, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x87, 0x86, 0x88),
	SSL_ASN1_NAME_COMMAND(0x40, 0x42, 0x68, 0x87),
	SSL_ASN1_NAME_COMMAND(0x1a, 0x0, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x92, 0x88, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x11, 0x0, 0x0, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x87, 0x86, 0x88),
	SSL_ASN1_NAME_COMMAND(0x40, 0x42, 0x68, 0x87),
	SSL_ASN1_NAME_COMMAND(0x19, 0x0, 0x1, 0x0),
	/* Advance through attributes and relative distinguished names. */
	SSL_ASN1_NAME_COMMAND(0x1a, 0x84, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x71, 0x84, 0x3, 0x0),
	SSL_ASN1_NAME_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x80, 0x86, 0x86, 0x85),
	SSL_ASN1_NAME_COMMAND(0x1a, 0x84, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x71, 0x84, 0x1, 0x0),
	SSL_ASN1_NAME_COMMAND(0x20, 0x0, 0x2, 0x0),
	SSL_ASN1_NAME_COMMAND(0x83, 0x80, 0x86, 0x85),
	SSL_ASN1_NAME_COMMAND(0x40, 0x43, 0x68, 0xf),
	SSL_ASN1_NAME_COMMAND(0x1, 0x0, 0x0, 0x0),
	0
};

#undef SSL_ASN1_NAME_COMMAND

int SceSslInternal_CE37B69E(void *name)
{
	SslObjectHeader *object = name;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->methods->Destroy == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->methods->Destroy(name);
}

int sslGetCertificateName(SslCertificate *certificate, int selector, int mode,
			  void **result)
{
	SslCertificatePropertyValue encoded;
	SslCertificateContext *context;
	void *name = NULL;
	SceUInt32 consumed = 0;
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (certificate == NULL || result == NULL)
		return status;
	status = certificate->methods->GetCertificateInfo(certificate, selector, &encoded);
	if (status == 0)
		status = certificate->methods->GetCertificateInfo(certificate, 0x8005, &context);
	if (status == 0)
		status = context->methods->nameMethods->Decode(context, mode, encoded.length,
							     encoded.data, &consumed, &name);
	if (status == 0)
		*result = name;
	else if (name != NULL)
		((SslObjectHeader *)name)->methods->Destroy(name);
	return status;
}

int SceSslInternal_81FA5DBE(void *certificate, int mode, void **result)
{
	return sslGetCertificateName(certificate, 4, mode, result);
}

int SceSslInternal_08179EBD(void *certificate, int mode, void **result)
{
	return sslGetCertificateName(certificate, 7, mode, result);
}

int sslEncodeCertificateName(SslCertificateName *name, SceUInt32 capacity,
			     void *data, SceUInt32 *length)
{
	return sslEncodeCertificateNameProperties(name->properties, data, length, capacity);
}

int sslDecodeCertificateName(SslCertificateContext *context, int mode, SceUInt32 length,
			     const void *data, SceUInt32 *consumed, void **result)
{
	SslCertificateName *name;
	int status;

	status = sslNewCertificateName(context, &name);
	if (status != 0)
		return status;
	status = sslDecodeCertificateNameProperties(name->properties, data, length, consumed);
	if (status == 0 && mode == 0 && sslPackProperties(name->properties, 0) != 0)
		status = 10001;
	if (status == 0)
		*result = name;
	else if (name != NULL)
		sslFreeCertificateName(name);
	return status;
}

int sslDecodeCertificateNameProperties(SslPropertyList *properties,
				       const SceUInt8 *data, SceUInt32 length,
				       SceUInt32 *consumed)
{
	return sslAsn1DecodeProperties(sslCertificateNameSchema, properties, data, length,
				       consumed);
}

int sslEncodeCertificateNameProperties(SslPropertyList *properties, SceUInt8 *data,
				       SceUInt32 *length, SceUInt32 capacity)
{
	return sslAsn1EncodeProperties(sslCertificateNameEncodingSchema, properties, data,
				       length, capacity);
}
