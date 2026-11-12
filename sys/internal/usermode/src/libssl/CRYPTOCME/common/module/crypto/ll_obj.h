/* Filename and descriptive names are ours: CRYPTOCME's low-level object chain,
 * the native layer below the crypto backends.
 */
#ifndef LIBSSL_CRYPTOCME_LL_OBJ_H
#define LIBSSL_CRYPTOCME_LL_OBJ_H

#include <scetypes.h>
#include "../dmem/dmem.h"

typedef struct SslLowLevelObject SslLowLevelObject;

typedef struct SslLowLevelMethod {
	int (*Operate)(SslLowLevelObject *object, void *params, int command);
	int (*Control)(SslLowLevelObject *object, int command, int arg,
		       void *value);
	int (*SetInfo)(SslLowLevelObject *object, int type, int id,
		       const void *value);
	int (*GetInfo)(SslLowLevelObject *object, int type, int id,
		       void *result);
} SslLowLevelMethod;

typedef const SslLowLevelMethod *(*SslLowLevelMethodGetter)(void);

struct SslLowLevelObject {
	const SslLowLevelMethod *method;
	SslLowLevelObject *next;
	SslDirectMemoryContext *dmem;
	void *state;
	void *reserved16;
	SceUInt32 flags;
};

_Static_assert(sizeof(SslLowLevelObject) == 24, "SslLowLevelObject");

/* Control commands */
#define SSL_LL_CONTROL_INIT 1
#define SSL_LL_CONTROL_FREE 2

/* Object flags */
#define SSL_LL_OBJECT_OWNED 1

/* Operate commands */
#define SSL_LL_OPERATE_SIGN 0x4011
#define SSL_LL_OPERATE_VERIFY 0x6020

void sslFreeLowLevelObject(SslLowLevelObject *object);
int sslSetLowLevelObjectInfo(void *object, int type, int id, const void *value);
int sslGetLowLevelObjectInfo(void *object, int type, int id, void *result);
int sslCreateLowLevelObject(SslLowLevelObject **result,
			    const SslLowLevelMethodGetter *methods,
			    SslDirectMemoryContext *dmem);
int sslLowLevelCipherOperate(SslLowLevelObject *object, void *output,
			     SceUInt32 *outputLength, SceUInt32 outputMax,
			     const void *input, SceUInt32 inputLength,
			     void *random, int encrypt);
int sslLowLevelSign(SslLowLevelObject *object, void *signature,
		    SceUInt32 *signatureLength, SceUInt32 signatureMax,
		    const void *digest, SceUInt32 digestLength, int flags);
int sslLowLevelVerify(SslLowLevelObject *object, int *result,
		      const void *signature, SceUInt32 signatureLength,
		      const void *digest, SceUInt32 digestLength, int flags);

/* The big number method context: chooses the arithmetic implementation for an
 * operand size; names ours.
 */
typedef struct SslBigNumMethodContext SslBigNumMethodContext;
typedef int (*SslBigNumMethodSelector)(SslBigNumMethodContext *context,
				       const void **method, int size,
				       int flags);

struct SslBigNumMethodContext {
	SslDirectMemoryContext *dmem;
	SslBigNumMethodSelector selector;
	SceUInt32 reserved8;
	SceUInt32 reserved12;
	SceUInt32 flags;
};

_Static_assert(sizeof(SslBigNumMethodContext) == 20, "SslBigNumMethodContext");

int sslSetBigNumMethodSelector(SslBigNumMethodContext *context,
			       SslBigNumMethodSelector selector);
int sslNewBigNumMethodContext(SslBigNumMethodContext **result,
			      SslDirectMemoryContext *dmem);
int sslFreeBigNumMethodContext(SslBigNumMethodContext *context);
int sslSelectBigNumMethod(SslBigNumMethodContext *context, const void **method,
			  int size, int flags);
int sslSelectDefaultBigNumMethod(SslBigNumMethodContext *context,
				 const void **method, int size, int flags);

/* The low-level digest layer below the digest and HMAC backends */
int sslCreateDigestBackendContext(void **context, const void *descriptor,
				  void *directMemory);
int sslDestroyDigestBackendContext(void *context);
int sslInitDigestBackendContext(void *context);
int sslUpdateDigestBackendContext(void *context, const void *data,
				  SceUInt32 length);
int sslFinalizeDigestBackendContext(void *context, void *digest);
int sslSetDigestBackendContextKey(void *context, const void *key,
				  SceUInt32 length);
int sslDuplicateDigestBackendContext(void **result, void *context);
int sslGetDigestBackendContextInfo(void *context, int selector, void *result,
				   SceUInt32 length);
int sslGetLowLevelDigestInfo(const void *descriptor, void *context,
			     int selector, void *result, SceUInt32 length);
const void *sslGetHmacDigestDescriptor(void);

#endif
