/* Private descriptive interface names are ours, not recovered Sony spellings. */
#ifndef LIBSSL_CRYPTOCME_PKEY_CTX_H
#define LIBSSL_CRYPTOCME_PKEY_CTX_H

#include <stddef.h>
#include "../library/lib_ctx.h"

typedef struct SslKeyComponent {
	SceUInt32 length;
	const void *data;
} SslKeyComponent;

typedef int (*SslKeyComponentCallback)(void *arg, int id,
				       const SslKeyComponent *component);

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
	int (*GetKeyInfo)(void *key, int selector, void *result);
	int (*SetKeyInfo)(void *key, int selector, void *value);
	int (*CopyKey)(void *source, void *destination, int mode);
	SceUIntPtr opaqueKeyOperations[4];
	int (*RetainKey)(void *key);
	int (*EnumerateComponents)(void *key, SslKeyComponentCallback callback, void *arg);
	SceUIntPtr opaqueComponentOperations[3];
	int (*CompareKeys)(void *left, void *right);
	int (* opaqueContextOperation)(void);
	int (*RetainContext)(void *context);
} SslKeyModule;

typedef const SslKeyModule *(*SslKeyModuleFactory)(void *data);

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
#define SSL_KEY_MODULE_TYPE 16
#define SSL_KEY_INFO_CONTEXT 2001
#define SSL_KEY_INFO_ALGORITHM 2006
#define SSL_KEY_INFO_BIT_LENGTH 2007

_Static_assert(sizeof(SslKeyContext) == 24, "SslKeyContext");
_Static_assert(offsetof(SslKeyContext, references) == 20, "references");
_Static_assert(offsetof(SslKeyModule, DestroyKey) == 32, "DestroyKey");
_Static_assert(offsetof(SslKeyModule, GetKeyInfo) == 36, "GetKeyInfo");
_Static_assert(offsetof(SslKeyModule, CopyKey) == 44, "CopyKey");
_Static_assert(offsetof(SslKeyModule, RetainKey) == 64, "RetainKey");
_Static_assert(offsetof(SslKeyModule, EnumerateComponents) == 68, "EnumerateComponents");
_Static_assert(offsetof(SslKeyModule, CompareKeys) == 84, "CompareKeys");
_Static_assert(offsetof(SslKeyModule, RetainContext) == 92, "RetainContext");
_Static_assert(sizeof(SslKeyComponent) == 8, "SslKeyComponent");

int sslCreateKeyContext(SslLibrary *library, int parameter, SceUInt32 flags,
			void **result);
int sslCreatePublicKey(void *context, int algorithm, void **result);
int sslCopyPublicKey(void *source, void *destination, int mode);
int sslClonePublicKey(void *source, int mode, void **result);
int sslRetainPublicKey(void *key);
int sslEnumerateKeyComponents(void *key, SslKeyComponentCallback callback, void *arg);
int sslGetPublicKeyBitLength(void *key);
int sslComparePublicKeys(void *left, void *right);
int sslGetPublicKeyInfo(void *key, int selector, void *result);
int sslSetPublicKeyInfo(void *key, int selector, void *value);
int sslRetainKeyContext(void *context);
int sslGetPublicKeyAlgorithm(void *key);
int sslDestroyKeyContext(void *context);
int sslFreeKeyContext(void *context);
int sslGetKeyContextInfo(void *context, int selector, void *result);
int sslSetKeyContextInfo(void *context, int selector, void *value);

#endif
