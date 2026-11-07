/* SSLeay object names and layout; private lookup names, struct and filename are ours. */
#ifndef LIBSSL_OBJECTS_H
#define LIBSSL_OBJECTS_H

#include <stddef.h>
#include <scetypes.h>
#include <libssl/object.h>
#include "asn1_object_sce.h"
#include "objects_sce.h"

_Static_assert(sizeof(ASN1_OBJECT) == 24, "ASN1_OBJECT");
_Static_assert(offsetof(ASN1_OBJECT, length) == 12, "length");
_Static_assert(offsetof(ASN1_OBJECT, data) == 16, "data");

struct SslObjectIdentifier {
	SceUInt32 reserved;
	void *data;
	SceUInt32 length;
	SceUInt32 flags;
};

typedef struct SslObjectIdentifier SslObjectIdentifier;

int sslGetObjectShortNameByNid(int nid, const char **result);
int sslGetObjectLongNameByNid(int nid, const char **result);
int sslGetObjectNid(const void *oid, SceUInt32 length, int *result);
int sslGetObjectShortName(const void *oid, SceUInt32 length, const char **result);
int sslGetObjectLongName(const void *oid, SceUInt32 length, const char **result);
int sslFindObjectByShortName(const char *name, SceUInt32 length,
			     const unsigned char **oid, SceUInt32 *oidLength);
int sslFindObjectByLongName(const char *name, SceUInt32 length,
			    const unsigned char **oid, SceUInt32 *oidLength);
int sslGetObjectEncodingByNid(int nid, const unsigned char **oid,
			      SceUInt32 *oidLength);
int sslNewObjectIdentifier(void *unused, SslObjectIdentifier **result);
int sslFreeObjectIdentifier(SslObjectIdentifier *object);
int sslAppendObjectArc(SceUInt32 value, SceUInt32 capacity,
		       SceUInt32 *position, char *data);
int sslFormatObjectIdentifier(const void *oid, SceUInt32 length,
			      SceUInt32 capacity, char *data);

#endif
