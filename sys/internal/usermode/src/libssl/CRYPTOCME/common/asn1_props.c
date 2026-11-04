/* Filename and private names are ours; generic CRYPTOCME property schemas. */
#include "asn1_props.h"
#include "../../asn1.h"

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

int sslDecodeSignatureProperties(SslPropertyList *properties, const SceUInt8 *data,
				 SceUInt32 length, SceUInt32 *consumed)
{
	return sslAsn1DecodeProperties(sslSignatureDecodingSchema, properties, data,
				       length, consumed);
}
