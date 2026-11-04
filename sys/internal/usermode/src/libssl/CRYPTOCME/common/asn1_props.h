/* Filename and descriptive private names are ours. */
#ifndef LIBSSL_CRYPTOCME_ASN1_PROPS_H
#define LIBSSL_CRYPTOCME_ASN1_PROPS_H

#include "property.h"

int sslEncodeRsaPublicKeyProperties(SslPropertyList *properties, SceUInt8 *data,
				    SceUInt32 *length, SceUInt32 capacity);
int sslEncodeSubjectPublicKeyInfoProperties(SslPropertyList *properties, SceUInt8 *data,
					   SceUInt32 *length, SceUInt32 capacity);

int sslDecodeSignatureProperties(SslPropertyList *properties, const SceUInt8 *data,
				 SceUInt32 length, SceUInt32 *consumed);

#endif
