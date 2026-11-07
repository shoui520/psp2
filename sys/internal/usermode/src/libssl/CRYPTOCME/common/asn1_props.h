/* Filename and descriptive private names are ours. */
#ifndef LIBSSL_CRYPTOCME_ASN1_PROPS_H
#define LIBSSL_CRYPTOCME_ASN1_PROPS_H

#include "property.h"

int sslDecodeRsaPublicKeyProperties(SslPropertyList *properties, const SceUInt8 *data,
				    SceUInt32 length, SceUInt32 *consumed);
int sslDecodeSubjectPublicKeyInfoProperties(SslPropertyList *properties,
					   const SceUInt8 *data, SceUInt32 length,
					   SceUInt32 *consumed);

int sslEncodeRsaPublicKeyProperties(SslPropertyList *properties, SceUInt8 *data,
				    SceUInt32 *length, SceUInt32 capacity);
int sslEncodeSubjectPublicKeyInfoProperties(SslPropertyList *properties, SceUInt8 *data,
					   SceUInt32 *length, SceUInt32 capacity);

int sslDecodeSignatureProperties(SslPropertyList *properties, const SceUInt8 *data,
				 SceUInt32 length, SceUInt32 *consumed);
int sslDecodeBase64Block(SceUInt32 inLength, const void *in, int *newlineFlags,
			 void *out, SceUInt32 *outLength);

#endif
