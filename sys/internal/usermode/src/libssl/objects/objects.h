/* SSLeay object names and layout; private lookup name and filename are ours. */
#ifndef LIBSSL_OBJECTS_H
#define LIBSSL_OBJECTS_H

#include <stddef.h>
#include <scetypes.h>
#include "asn1_object_sce.h"
#include "objects_sce.h"

_Static_assert(sizeof(ASN1_OBJECT) == 24, "ASN1_OBJECT");
_Static_assert(offsetof(ASN1_OBJECT, length) == 12, "length");
_Static_assert(offsetof(ASN1_OBJECT, data) == 16, "data");

int sslGetObjectShortName(const void *oid, SceUInt32 length, const char **result);
int sslAppendObjectArc(SceUInt32 value, SceUInt32 capacity,
		       SceUInt32 *position, char *data);
int sslFormatObjectIdentifier(const void *oid, SceUInt32 length,
			      SceUInt32 capacity, char *data);

#endif
