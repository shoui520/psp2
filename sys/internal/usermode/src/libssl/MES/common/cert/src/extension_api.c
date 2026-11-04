/* Filename and descriptive private names are ours; component path follows MES. */
#include "extension.h"
#include "../../../../memory.h"

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
