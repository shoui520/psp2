/* Filename and descriptive names are ours: CRYPTOCME's low-level object chain,
 * the native layer below the crypto backends.
 */
#ifndef LIBSSL_CRYPTOCME_LL_OBJ_H
#define LIBSSL_CRYPTOCME_LL_OBJ_H

#include <scetypes.h>
#include "../dmem/dmem.h"
#include "ll_bn.h"

typedef struct SslLowLevelObject SslLowLevelObject;

typedef struct SslLowLevelMethod {
	int (*Operate)(SslLowLevelObject *object, void *params, int command);
	int (*Control)(SslLowLevelObject *object, int command, int arg,
		       void *value);
	int (*SetInfo)(SslLowLevelObject *object, int type, int id,
		       const void *value);
	int (*GetInfo)(SslLowLevelObject *object, int type, int id,
		       void *result);
	void *reserved16;
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

/* Operate commands: an operation class in bits 12-19 and its kind below */
#define SSL_LL_OPERATE_CLASS_MASK 0xff000
#define SSL_LL_OPERATE_CIPHER 0x4000
#define SSL_LL_OPERATE_CHECK 0x6000
#define SSL_LL_OPERATE_ENCRYPT 0x1
#define SSL_LL_OPERATE_DECRYPT 0x2
#define SSL_LL_OPERATE_SIGNATURE 0x10
#define SSL_LL_OPERATE_VERIFICATION 0x20
#define SSL_LL_OPERATE_SIGN 0x4011
#define SSL_LL_OPERATE_VERIFY 0x6020

/* Parameters of a cipher or signature operation */
typedef struct SslLowLevelCipherParams {
	void *output;
	SceUInt32 outputMax;
	SceUInt32 *outputLength;
	const void *input;
	SceUInt32 inputLength;
	void *random;
	SceUInt32 reserved24;
} SslLowLevelCipherParams;

/* Parameters of a signature verification */
typedef struct SslLowLevelVerifyParams {
	const void *signature;
	SceUInt32 signatureLength;
	const void *digest;
	SceUInt32 digestLength;
	int *result;
	void *random;
} SslLowLevelVerifyParams;

typedef struct SslDerBuffer {
	SceUInt32 length;
	const unsigned char *data;
} SslDerBuffer;

/* A low-level digest algorithm */
typedef struct SslDigestDescriptor SslDigestDescriptor;
typedef struct SslDigestImplementation {
	SceUInt32 reserved[5];
	SceUInt32 stateSize;
} SslDigestImplementation;

struct SslDigestDescriptor {
	const char *name;
	SslDerBuffer oid;
	unsigned char digestLength;
	unsigned char flags;
	unsigned short blockLength;
	const SslDigestImplementation *implementation;
	const SslDigestImplementation *innerImplementation;
	int (*GetInfo)(const SslDigestDescriptor *descriptor,
		       struct SslDigestContext *context, int selector,
		       SceUInt32 *value, const void **pointer);
	SceUInt32 type;
};

/* A low-level digest context: this header, then the digest's state and the
 * inner digest's state, each 8-byte aligned.
 */
typedef struct SslDigestContext {
	SslDirectMemoryContext *dmem;
	const SslDigestDescriptor *descriptor;
	void *state;
	void *innerState;
	SceUInt32 size;
	SceUInt32 flags;
} SslDigestContext;

_Static_assert(sizeof(SslDigestContext) == 24, "SslDigestContext");

/* sslGetLowLevelDigestInfo selectors */
#define SSL_LL_DIGEST_INFO_CONTEXT_SIZE 1
#define SSL_LL_DIGEST_INFO_TYPE 2
#define SSL_LL_DIGEST_INFO_OID 3
#define SSL_LL_DIGEST_INFO_BLOCK_LENGTH 4
#define SSL_LL_DIGEST_INFO_DIGEST_LENGTH 5
#define SSL_LL_DIGEST_INFO_NAME 9

/* The DigestInfo encodings: with and without the NULL parameters */
#define SSL_DIGEST_INFO_NULL_PARAMETERS 1
#define SSL_DIGEST_INFO_NO_PARAMETERS 2

int sslEncodeDigestInfo(SceUInt32 *outputLength, unsigned char *output,
			SceUInt32 outputMax, const SslDerBuffer *oid,
			const unsigned char *digest, int digestLength,
			int form);
int sslAllocateLowLevelObject(SslLowLevelObject **result,
			      SslDirectMemoryContext *dmem);
void sslDestroyLowLevelObject(SslLowLevelObject *object);
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
int sslGetLowLevelDigestInfo(const SslDigestDescriptor *descriptor,
			     struct SslDigestContext *context, int selector,
			     SceUInt32 *value, const void **pointer);
const void *sslGetHmacDigestDescriptor(void);

#endif
