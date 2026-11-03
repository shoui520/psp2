/* Private descriptive interface names are ours, not recovered Sony spellings. */
#ifndef LIBSSL_CRYPTOCME_PKEY_CTX_H
#define LIBSSL_CRYPTOCME_PKEY_CTX_H

#include <stddef.h>
#include "../library/lib_ctx.h"

/* Shared module interface prefix. Further key operations follow these slots. */
typedef struct SslKeyModule {
	const char *name;
	SceUInt32 metadata[2];
	int (*CreateContext)(void *library, int flags, int mode, void **result);
	int (*DestroyContext)(void *context);
	int (*GetContextInfo)(void *context, int selector, void *result);
	int (*SetContextInfo)(void *context, int selector, void *value);
	int (*CreateKey)(void *context, int type, void **result);
	int (*DestroyKey)(void *key);
} SslKeyModule;

typedef struct SslKeyContext {
	const SslKeyModule *module;
	SslLibrary *library;
	void *backend;
	SceUInt32 flags;
	void *applicationData;
	int references;
} SslKeyContext;

/* Only the public key's shared context-pointer prefix is described here. */
typedef struct SslPublicKeyHeader {
	SslKeyContext *context;
} SslPublicKeyHeader;

#define SSL_KEY_CONTEXT_BORROWED_DATA 1
#define SSL_KEY_CONTEXT_LOCK 10
#define SSL_KEY_CONTEXT_INFO_LIBRARY 1
#define SSL_KEY_CONTEXT_INFO_MODULE 2
#define SSL_KEY_CONTEXT_INFO_BACKEND 3
#define SSL_KEY_CONTEXT_INFO_FLAGS 4
#define SSL_KEY_CONTEXT_INFO_APPLICATION_DATA 5

_Static_assert(sizeof(SslKeyContext) == 24, "SslKeyContext");
_Static_assert(offsetof(SslKeyContext, references) == 20, "references");
_Static_assert(offsetof(SslKeyModule, DestroyKey) == 32, "DestroyKey");

int sslDestroyKeyContext(void *context);
int sslFreeKeyContext(void *context);
int sslGetKeyContextInfo(void *context, int selector, void *result);
int sslSetKeyContextInfo(void *context, int selector, void *value);

#endif
