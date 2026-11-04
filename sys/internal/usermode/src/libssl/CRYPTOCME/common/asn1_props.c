/* Filename and private names are ours; generic CRYPTOCME property schemas. */
#include "asn1_props.h"
#include "../../asn1.h"

#define SSL_KEY_COMMAND(opcode, first, second, third) \
	((SceUInt32)(opcode) | ((SceUInt32)(first) << 8) | \
	 ((SceUInt32)(second) << 16) | ((SceUInt32)(third) << 24))

static const SceUInt32 sslSubjectPublicKeyInfoEncodingSchema[] = {
	/* SubjectPublicKeyInfo SEQUENCE and AlgorithmIdentifier. */
	SSL_KEY_COMMAND(0x32, 0x0, 0x10, 0x0),
	SSL_KEY_COMMAND(0x11, 0x0, 0x0, 0x0),
	SSL_KEY_COMMAND(0x32, 0x0, 0x10, 0x0),
	SSL_KEY_COMMAND(0x11, 0x0, 0x0, 0x0),
	SSL_KEY_COMMAND(0x30, 0x6, 0x30, 0x1),
	SSL_KEY_COMMAND(0x35, 0x30, 0x2, 0x0),
	SSL_KEY_COMMAND(0x19, 0x0, 0x1, 0x0),
	/* Public-key BIT STRING with a zero unused-bits prefix. */
	SSL_KEY_COMMAND(0x30, 0x3, 0x30, 0x3),
	SSL_KEY_COMMAND(0x39, 0x0, 0x0, 0x0),
	SSL_KEY_COMMAND(0x1, 0x0, 0x0, 0x0),
	0
};

static const SceUInt32 sslRsaPublicKeyEncodingSchema[] = {
	/* RSAPublicKey SEQUENCE: modulus and public exponent. */
	SSL_KEY_COMMAND(0x32, 0x0, 0x10, 0x0),
	SSL_KEY_COMMAND(0x11, 0x0, 0x0, 0x0),
	SSL_KEY_COMMAND(0x31, 0x10, 0x10, 0x0),
	SSL_KEY_COMMAND(0x31, 0x10, 0x11, 0x0),
	SSL_KEY_COMMAND(0x1, 0x0, 0x0, 0x0),
	0
};

#undef SSL_KEY_COMMAND

#define SSL_SIGNATURE_COMMAND(opcode, first, second, third) \
	((SceUInt32)(opcode) | ((SceUInt32)(first) << 8) | \
	 ((SceUInt32)(second) << 16) | ((SceUInt32)(third) << 24))

static const SceUInt32 sslSignatureDecodingSchema[] = {
	/* AlgorithmIdentifier: OID and optional parameters. */
	SSL_SIGNATURE_COMMAND(0x60, 0x10, 0x0, 0x0),
	SSL_SIGNATURE_COMMAND(0x18, 0x0, 0x1, 0x0),
	SSL_SIGNATURE_COMMAND(0x60, 0x6, 0x0, 0x0),
	SSL_SIGNATURE_COMMAND(0x40, 0x42, 0x13, 0x15),
	SSL_SIGNATURE_COMMAND(0x1a, 0x80, 0x1, 0x0),
	SSL_SIGNATURE_COMMAND(0x70, 0x80, 0x1, 0x0),
	SSL_SIGNATURE_COMMAND(0x40, 0x42, 0x13, 0x18),
	SSL_SIGNATURE_COMMAND(0x20, 0x0, 0x1, 0x0),
	SSL_SIGNATURE_COMMAND(0x19, 0x0, 0x1, 0x0),
	SSL_SIGNATURE_COMMAND(0x1a, 0x0, 0x1, 0x0),
	/* Decode the following signature BIT STRING. */
	SSL_SIGNATURE_COMMAND(0x60, 0x3, 0x0, 0x0),
	SSL_SIGNATURE_COMMAND(0x41, 0x42, 0x13, 0x10),
	SSL_SIGNATURE_COMMAND(0x1, 0x0, 0x0, 0x0),
	0
};

#undef SSL_SIGNATURE_COMMAND

int sslEncodeRsaPublicKeyProperties(SslPropertyList *properties, SceUInt8 *data,
				    SceUInt32 *length, SceUInt32 capacity)
{
	return sslAsn1EncodeProperties(sslRsaPublicKeyEncodingSchema, properties, data,
				       length, capacity);
}

int sslDecodeSignatureProperties(SslPropertyList *properties, const SceUInt8 *data,
				 SceUInt32 length, SceUInt32 *consumed)
{
	return sslAsn1DecodeProperties(sslSignatureDecodingSchema, properties, data,
				       length, consumed);
}

int sslEncodeSubjectPublicKeyInfoProperties(SslPropertyList *properties, SceUInt8 *data,
					   SceUInt32 *length, SceUInt32 capacity)
{
	return sslAsn1EncodeProperties(sslSubjectPublicKeyInfoEncodingSchema, properties,
				       data, length, capacity);
}
