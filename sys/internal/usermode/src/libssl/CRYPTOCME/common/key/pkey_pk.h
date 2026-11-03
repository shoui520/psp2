/* Descriptive private type, field and helper names are ours. */
#ifndef LIBSSL_CRYPTOCME_PKEY_PK_H
#define LIBSSL_CRYPTOCME_PKEY_PK_H

#include "pkey_ctx.h"
#include "../property.h"

typedef struct SslPublicKey {
	SslKeyContext *context;
	SslPropertyList *properties;
	int references;
	int type;
	SceUInt32 flags;
	void *externalData;
} SslPublicKey;

#define SSL_PUBLIC_KEY_BORROWED_DATA 0x1000

_Static_assert(sizeof(SslPublicKey) == 24, "SslPublicKey");
_Static_assert(offsetof(SslPublicKey, references) == 8, "references");

int sslFreePublicKey(void *key);

#endif
