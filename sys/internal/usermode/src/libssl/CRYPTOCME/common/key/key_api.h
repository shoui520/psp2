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

/* Shared module interface prefix; only the first slot is accessed here. */
typedef struct SslKeyGeneratorModule {
	int (*CreateKey)(SslLibrary *library, int type, const void *info,
			 void **result);
} SslKeyGeneratorModule;

typedef const SslKeyGeneratorModule *(*SslKeyGeneratorFactory)(void *data);

extern const SslKeyGeneratorModule sslDefaultKeyGeneratorModule;

int sslCreateKey(SslLibrary *library, int type, const void *info, void **result);
int sslDestroyKey(void *key);
int sslGetKeyInfo(void *key, int selector, void *result);
int sslSetKeyInfo(void *key, int selector, void *value);

#endif
