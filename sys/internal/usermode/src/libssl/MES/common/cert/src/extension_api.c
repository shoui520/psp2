/* Filename and descriptive private names are ours; component path follows MES. */
#include <libssl/object.h>
#include "extension.h"
#include "../../../../memory.h"
#include "../../../../asn1.h"

/* OCSP basic response, 1.3.6.1.5.5.7.48.1.1. */
static const struct {
	SceUInt32 length;
	const unsigned char *oid;
	SceUInt32 mask;
} ocspResponseFormats[] = {
	{ 9, (const unsigned char *)"\x2b\x06\x01\x05\x05\x07\x30\x01\x01", 1 }
};

int SceSslInternal_F030A0DE(void *object)
{
	SslCertificateExtension *extension = object;
	SceUInt32 flags;

	if (extension == NULL)
		return 0;
	flags = extension->flags;
	if (flags & SSL_CERT_EXTENSION_OWNS_VALUE) {
		if (extension->value.data != NULL) {
			sslFree(extension->value.data);
			flags = extension->flags;
			extension->value.data = NULL;
		}
		flags &= ~SSL_CERT_EXTENSION_OWNS_VALUE;
		extension->flags = flags;
	}
	if (flags & SSL_CERT_EXTENSION_OWNS_OID) {
		if (extension->oid.data != NULL) {
			sslFree(extension->oid.data);
			flags = extension->flags;
			/* The original clears the value pointer after freeing the OID. */
			extension->value.data = NULL;
		}
		flags &= ~SSL_CERT_EXTENSION_OWNS_OID;
		extension->flags = flags;
	}
	if (extension->properties != NULL) {
		sslFreePropertyList(extension->properties);
		flags = extension->flags;
		extension->properties = NULL;
	}
	if (flags & 1)
		sslFree(extension);
	return 0;
}

int sslSetOcspAcceptableResponses(SslCertificateExtension *extension, int formats)
{
	SceUInt32 length = 2, offset = 2;
	unsigned int index;
	unsigned char *data;

	extension->type = 0x4018;
	for (index = 0;
	     index < sizeof(ocspResponseFormats) / sizeof(ocspResponseFormats[0]); index++)
		if (ocspResponseFormats[index].mask & formats)
			length += ocspResponseFormats[index].length + 2;
	if ((extension->flags & SSL_CERT_EXTENSION_OWNS_VALUE) && extension->value.data != NULL)
		sslFree(extension->value.data);
	data = sslAllocate(length);
	extension->value.data = data;
	if (data == NULL)
		return 10005;
	extension->value.length = length;
	extension->flags |= SSL_CERT_EXTENSION_OWNS_VALUE;
	data[0] = 0x30;
	data[1] = length - 2;
	for (index = 0;
	     index < sizeof(ocspResponseFormats) / sizeof(ocspResponseFormats[0]); index++) {
		if (!(ocspResponseFormats[index].mask & formats))
			continue;
		data = extension->value.data;
		data[offset] = 6;
		data[offset + 1] = ocspResponseFormats[index].length;
		sslCopyMemory((unsigned char *)extension->value.data + offset + 2,
			      ocspResponseFormats[index].oid, ocspResponseFormats[index].length);
		offset += ocspResponseFormats[index].length + 2;
	}
	return 0;
}

/* DER object identifiers, in the middleware's extension-type order. */
const char *const sslCertificateExtensionOids[SSL_CERT_EXTENSION_COUNT] = {
	"\x55\x1d\x0e", /* 2.5.29.14 */
	"\x55\x1d\x0f", /* 2.5.29.15 */
	"\x55\x1d\x10", /* 2.5.29.16 */
	"\x55\x1d\x11", /* 2.5.29.17 */
	"\x55\x1d\x12", /* 2.5.29.18 */
	"\x55\x1d\x13", /* 2.5.29.19 */
	"\x55\x1d\x14", /* 2.5.29.20 */
	"\x55\x1d\x15", /* 2.5.29.21 */
	"\x55\x1d\x17", /* 2.5.29.23 */
	"\x55\x1d\x18", /* 2.5.29.24 */
	"\x55\x1d\x1b", /* 2.5.29.27 */
	"\x55\x1d\x1c", /* 2.5.29.28 */
	"\x55\x1d\x1d", /* 2.5.29.29 */
	"\x55\x1d\x1e", /* 2.5.29.30 */
	"\x55\x1d\x1f", /* 2.5.29.31 */
	"\x55\x1d\x20", /* 2.5.29.32 */
	"\x55\x1d\x23", /* 2.5.29.35 */
	"\x55\x1d\x25", /* 2.5.29.37 */
	"\x2b\x06\x01\x05\x05\x07\x30\x01\x02", /* 1.3.6.1.5.5.7.48.1.2 */
	"\x2b\x06\x01\x05\x05\x07\x30\x01\x03", /* 1.3.6.1.5.5.7.48.1.3 */
	"\x2b\x06\x01\x05\x05\x07\x30\x01\x05", /* 1.3.6.1.5.5.7.48.1.5 */
	"\x2b\x06\x01\x05\x05\x07\x01\x01", /* 1.3.6.1.5.5.7.1.1 */
	"\x60\x86\x48\x01\x86\xf8\x42\x01\x0d", /* 2.16.840.1.113730.1.13 */
	"\x60\x86\x48\x01\x86\xf8\x42\x01\x01", /* 2.16.840.1.113730.1.1 */
	"\x2b\x06\x01\x05\x05\x07\x30\x01\x04" /* 1.3.6.1.5.5.7.48.1.4 */
};

int sslFindCertificateExtensionType(const void *oid, SceUInt32 length, int *result)
{
	int index;

	for (index = 0; index < SSL_CERT_EXTENSION_COUNT; index++) {
		if (length == sslStringLength(sslCertificateExtensionOids[index]) &&
		    sslCompareMemory(oid, sslCertificateExtensionOids[index], length) == 0) {
			*result = index + SSL_CERT_EXTENSION_FIRST;
			return 0;
		}
	}
	return 10008;
}

int sslSetCertificateExtensionData(SslCertificateExtension *extension,
				   int selector, int mode,
				   const SslCertificateExtensionData *value)
{
	SslCertificateExtensionData *buffer;
	SceUInt32 ownership;
	void *old;

	if (extension == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	switch (selector) {
	case SSL_CERT_EXTENSION_VALUE:
		ownership = SSL_CERT_EXTENSION_OWNS_VALUE;
		buffer = &extension->value;
		break;
	case SSL_CERT_EXTENSION_OID:
		ownership = SSL_CERT_EXTENSION_OWNS_OID;
		buffer = &extension->oid;
		break;
	default:
		return 10022;
	}
	if (selector == SSL_CERT_EXTENSION_OID) {
		if (sslFindCertificateExtensionType(value->data, value->length,
						    &extension->type) == 0)
			goto invalidate;
		extension->type = SSL_CERT_EXTENSION_CUSTOM;
	}
	if (mode != 0) {
		if (extension->flags & ownership) {
			if (buffer->data != NULL)
				sslFree(buffer->data);
			extension->flags &= ~ownership;
		}
		buffer->data = value->data;
		buffer->length = value->length;
	} else {
		old = buffer->data;
		if (old != NULL && (extension->flags & ownership)) {
			if (buffer->length < value->length) {
				buffer->data = sslReallocate(old, value->length,
							     buffer->length);
				if (buffer->data == NULL) {
					buffer->data = old;
					return 10005;
				}
			}
		} else {
			buffer->data = sslAllocate(value->length);
			if (buffer->data == NULL)
				return 10005;
		}
		extension->flags |= ownership;
		buffer->length = value->length;
		sslCopyMemory(buffer->data, value->data, value->length);
	}
invalidate:
	if (extension->properties != NULL) {
		sslFreePropertyList(extension->properties);
		extension->properties = NULL;
	}
	return 0;
}

int sslSetCertificateKeyUsage(SslCertificateExtension *extension, int usage)
{
	unsigned char encoded[5] = { 3, 2, 0, (unsigned char)usage, 0 };
	SslCertificateExtensionData value = { 4, encoded };
	int bit;

	extension->type = 0x4001;
	if (usage > 255) {
		encoded[1] = 3;
		encoded[4] = (SceUInt32)usage >> 8;
		for (bit = 0; bit <= 7; bit++)
			if (encoded[4] & (1u << bit))
				break;
		encoded[2] = bit;
		value.length = 5;
	}
	return sslSetCertificateExtensionData(extension, SSL_CERT_EXTENSION_VALUE, 0, &value);
}

int sslSetCertificateExtensionInfo(SslCertificateExtension *extension,
				   int selector, const void *value)
{
	if (extension == NULL || value == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	switch (selector) {
	case 0x4001:
		return sslSetCertificateKeyUsage(extension, *(const int *)value);
	case 0x8001:
		extension->critical = *(const int *)value;
		return 0;
	case 0x8002:
		return sslSetCertificateExtensionData(extension, SSL_CERT_EXTENSION_VALUE, 0, value);
	case 0x8003:
		return sslSetCertificateExtensionData(extension, SSL_CERT_EXTENSION_OID, 0, value);
	case 0x8004:
		extension->type = *(const int *)value;
		return 0;
	case 0x8006:
		extension->index = *(const int *)value;
		return 0;
	case 0x800a:
		return sslSetOcspAcceptableResponses(extension, *(const int *)value);
	default:
		return 10018;
	}
}

int sslGetCertificateExtensionReason(SslCertificateExtension *extension, int *result)
{
	SslAsn1Element element;

	if (sslAsn1DecodeHeader(&element, extension->value.data, extension->value.length) != 0 ||
	    element.tag != 10 || element.headerLength + element.length > extension->value.length)
		return 10001;
	*result = *element.data;
	return 0;
}

int sslGetOcspAcceptableResponses(SslCertificateExtension *extension, SceUInt32 *result)
{
	SslAsn1Element element = { 0 };
	SceUInt32 offset, length;
	unsigned int index;

	*result = 0;
	if (sslAsn1DecodeHeader(&element, extension->value.data, extension->value.length) != 0 ||
	    element.tag != 16)
		return 10022;
	offset = element.headerLength;
	element = (SslAsn1Element){ 0 };
	for (;;) {
		length = extension->value.length;
		if (offset >= length)
			break;
		if (sslAsn1DecodeHeader(&element, (unsigned char *)extension->value.data + offset,
					 length - offset) != 0)
			break;
		if (element.tag != 6 ||
		    element.headerLength + element.length > extension->value.length - offset)
			break;
		for (index = 0;
		     index < sizeof(ocspResponseFormats) / sizeof(ocspResponseFormats[0]); index++) {
			if (ocspResponseFormats[index].length == element.length &&
			    sslCompareMemory(ocspResponseFormats[index].oid, element.data, element.length) == 0) {
				*result |= ocspResponseFormats[index].mask;
				break;
			}
		}
		offset += element.headerLength + element.length;
		element = (SslAsn1Element){ 0 };
	}
	return offset == extension->value.length ? 0 : 10022;
}

int sslGetCertificateGeneralName(SslCertificateExtension *extension,
				 int selector, int base,
				 SslCertificateExtensionData *result)
{
	SslAsn1Element element;
	const unsigned char *data;
	int remaining, consumed;
	SceUInt32 tag = (SceUInt32)selector - (SceUInt32)base;

	if (extension == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (sslAsn1DecodeHeader(&element, extension->value.data, extension->value.length) != 0 ||
	    element.tag != 16 || element.headerLength + element.length != extension->value.length)
		return 10022;
	data = element.data;
	remaining = (int)element.length;
	while (remaining > 0) {
		if (sslAsn1DecodeHeader(&element, data, (SceUInt32)remaining) != 0 ||
		    (element.encoding & 0xc0) != 0x80)
			break;
		consumed = (int)(element.headerLength + element.length);
		if (consumed > remaining)
			break;
		if (element.tag == tag) {
			result->length = element.length;
			result->data = (void *)element.data;
			return 0;
		}
		remaining = (int)((SceUInt32)remaining - (SceUInt32)consumed);
		data = (const unsigned char *)((SceUInt32)data + (SceUInt32)consumed);
	}
	return 10022;
}

int sslGetCertificateBasicConstraint(SslCertificateExtension *extension,
				     int selector, int *result)
{
	SslAsn1Element element = { 0 };
	SceUInt32 offset;
	int value;

	if (sslAsn1DecodeHeader(&element, extension->value.data, extension->value.length) != 0 ||
	    element.tag != 16 || element.headerLength + element.length != extension->value.length)
		return 10022;
	if (selector == 0x8007) {
		if (element.length == 0) {
			*result = 0;
			return 0;
		}
		if (sslAsn1DecodeHeader(&element, element.data, element.length) != 0)
			return 10022;
		if (element.tag != 1) {
			*result = 0;
			return 0;
		}
		if (element.length != 1)
			return 10022;
		value = *element.data;
		if (value != 0 && value != 255)
			return 10022;
		*result = value;
		return 0;
	}
	if (selector != 0x8008)
		return 10021;
	if (element.length == 0)
		return 10009;
	if (sslAsn1DecodeHeader(&element, element.data, element.length) != 0)
		return 10022;
	if (element.tag == 1 && element.length == 1) {
		offset = (SceUInt32)element.data - (SceUInt32)extension->value.data + 1;
		if (offset == extension->value.length)
			return 10009;
		element.length = extension->value.length - offset;
		element.data++;
		if (sslAsn1DecodeHeader(&element, element.data, element.length) != 0)
			return 10022;
	}
	offset = (SceUInt32)element.data + element.length - (SceUInt32)extension->value.data;
	if (offset != extension->value.length || sslAsn1GetInteger(&element, result) != 0)
		return 10022;
	return 0;
}

int sslGetCertificateExtensionOid(int type, const char **oid, SceUInt32 *length)
{
	SceUInt32 index = (SceUInt32)type - SSL_CERT_EXTENSION_FIRST;

	if (index >= SSL_CERT_EXTENSION_COUNT)
		return 10008;
	*oid = sslCertificateExtensionOids[index];
	if (length != NULL)
		*length = sslStringLength(sslCertificateExtensionOids[index]);
	return 0;
}

int sslGetCertificateKeyUsage(SslCertificateExtension *extension, int *result)
{
	const unsigned char *data;
	SceUInt32 length;
	int usage;

	if (extension->type != 0x4001)
		return 10022;
	length = extension->value.length;
	if (length < 4 || length > 5)
		return 10023;
	data = extension->value.data;
	if (data[0] != 3 || data[1] != length - 2)
		return 10022;
	usage = data[3];
	if (length == 5)
		usage |= data[4] << 8;
	*result = usage;
	return 0;
}

int SceSslInternal_BF99D664(const SslObjectMethods *methods, int unused,
			  SslCertificateExtension **result)
{
	SslCertificateExtension *extension;

	if (result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	extension = sslAllocate(sizeof(*extension));
	if (extension == NULL)
		return 10005;
	sslFillMemory(extension, 0, sizeof(*extension));
	extension->methods = methods;
	*result = extension;
	extension->flags = 1;
	extension->type = SSL_CERT_EXTENSION_CUSTOM;
	return 0;
}
