/* Filename and descriptive helper name are ours. The component path follows
 * the recovered CRYPTOCME/common/key source paths.
 */
#ifndef LIBSSL_CRYPTOCME_KEY_API_H
#define LIBSSL_CRYPTOCME_KEY_API_H

#include "../library/lib_ctx.h"

typedef struct SslKeyMaterial {
	SceUInt32 length;
	const void *data;
} SslKeyMaterial;

typedef struct SslKeyGeneratorModule {
	int (*CreateKey)(SslLibrary *library, int type, const void *info,
			 void **result);
	int (*Destroy)(void *key);
	int (*GetInfo)(void *key, int selector, void *result);
	int (*SetInfo)(void *key, int selector, void *value);
} SslKeyGeneratorModule;

_Static_assert(sizeof(SslKeyGeneratorModule) == 16, "SslKeyGeneratorModule");

typedef const SslKeyGeneratorModule *(*SslKeyGeneratorFactory)(void *data);

extern const SslKeyGeneratorModule sslDefaultKeyGeneratorModule;

int sslCreateKey(SslLibrary *library, int type, const void *info, void **result);
int sslDestroyKey(void *key);
int sslGetKeyInfo(void *key, int selector, void *result);
int sslSetKeyInfo(void *key, int selector, void *value);

#endif
