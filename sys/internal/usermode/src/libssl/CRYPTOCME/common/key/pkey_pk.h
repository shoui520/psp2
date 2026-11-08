/* Descriptive private type, field and helper names are ours. */
#ifndef LIBSSL_CRYPTOCME_PKEY_PK_H
#define LIBSSL_CRYPTOCME_PKEY_PK_H

#include "pkey_ctx.h"
#include "key_api.h"
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

int sslGetKeyUnsupportedError(void);
int sslNoopKeyMethod(void);
const SslKeyGeneratorModule *sslGetDefaultKeyGeneratorModule(void);
int sslSetPublicKeyProperty(void *key, int selector, const SceUInt32 *value);
int sslSetPublicKeyField(void *key, int selector, void *value);
int sslGetPublicKeyPropertySize(SslPublicKey *key, int selector);
int sslGetPublicKeyProperty(SslPublicKey *key, int selector, SceUInt32 *result);
int sslGetPublicKeyEncodingProperty(SslPublicKey *key, SceUInt32 *result);
int sslGetPublicKeyComponentBitLength(SslPublicKey *key, int type, int id,
				      int *result);
SceUInt32 sslGetRsaKeyParameterLength(SslPublicKey *key);
int sslGetRsaKeyModulusBitLength(SslPublicKey *key, int *result);
int sslGetPublicKeyType(SslPublicKey *key);
int sslGetPublicKeyField(void *key, int selector, void *result);
int sslEnumeratePublicKeyProperties(void *key, SslKeyComponentCallback callback,
				    void *arg);
int sslAllocatePublicKeyObject(SslPublicKey **result);
int sslCreatePublicKeyObject(void *context, int type, void **result);
int sslCleanupPublicKeyObject(SslPublicKey *key);
int sslFreePublicKey(void *key);
int sslCopyPublicKeyFields(void *source, void *destination, int mode);
int sslCopyPublicKeyObject(void *source, void *destination, int mode);
int sslDecodeRsaKeyFromPrivateProperties(SslPublicKey **result,
					 const SceUInt8 **data,
					 SceUInt32 length);
int sslDecodeRsaPublicKeyData(SslPublicKey **result, const SceUInt8 **data,
			      SceUInt32 length);
int sslDecodePublicKeyObject(void *context, int format, int algorithm,
			     SceUInt32 length, const void *data,
			     SceUInt32 *consumed, void **result);
int sslDecodeRsaPrivateKeyData(SslPublicKey **result, const SceUInt8 **data,
			       SceUInt32 length);
int sslDecodePrivateKeyObject(void *context, int format, int algorithm,
			      SceUInt32 length, const void *data,
			      SceUInt32 *consumed, void **result);
int sslComparePublicKeyPropertyLists(void *left, void *right);
int sslCompareKeyIntegerComponents(const SslKeyComponent *left,
				   const SslKeyComponent *right);
int sslComparePublicKeyObjects(void *left, void *right);

#endif
