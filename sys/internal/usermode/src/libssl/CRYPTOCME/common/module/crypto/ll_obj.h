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
	/* The exponentiation of an RSA method, or a layer's parameters */
	union {
		int (*ModExp)(SslBigNum *r, SslBigNum *a, void *state);
		const void *parameters;
	} extension;
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

/* SetInfo and GetInfo types and ids */
#define SSL_LL_INFO_GENERAL 1
#define SSL_LL_INFO_RANDOM 2
#define SSL_LL_INFO_RESERVED 3
#define SSL_LL_INFO_METHODS 4
#define SSL_LL_INFO_KEEP_STATE 5
#define SSL_LL_INFO_CLEAR_STATE 6
#define SSL_LL_INFO_MODULUS_LENGTH 7
#define SSL_LL_INFO_KEY 2
#define SSL_LL_INFO_LENGTH 3
#define SSL_LL_INFO_METHOD_NAME 4
#define SSL_LL_INFO_NUMBER 33
#define SSL_LL_INFO_NUMBER_FIRST 16
#define SSL_LL_INFO_BLINDING 36
#define SSL_LL_INFO_BLINDING_VALUE 4098
#define SSL_LL_INFO_BLINDING_FLAGS 4099

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
	SceUInt32 options;
	SceUInt32 reserved24;
} SslLowLevelCipherParams;

/* Parameters of a signature verification */
typedef struct SslLowLevelVerifyParams {
	const void *signature;
	SceUInt32 signatureLength;
	const void *digest;
	SceUInt32 digestLength;
	int *result;
	SceUInt32 options;
} SslLowLevelVerifyParams;

/* Parameters of an operation on big numbers further down the chain */
typedef struct SslLowLevelBigNumParams {
	SslBigNumContext *ctx;
	SslBigNum *input;
	SslBigNum *output;
	SceUInt32 options;
} SslLowLevelBigNumParams;

/* Operate flag: the parameters are SslLowLevelBigNumParams */
#define SSL_LL_OPERATE_BIG_NUMBERS 0x10000

typedef struct SslDerBuffer {
	SceUInt32 length;
	const unsigned char *data;
} SslDerBuffer;

/* A low-level digest algorithm */
typedef struct SslDigestDescriptor SslDigestDescriptor;
struct SslDigestContext;

/* The functions a digest context runs; Transfer saves the state (save != 0)
 * or restores it and returns the bytes used, or -1
 */
typedef struct SslDigestImplementation {
	int (*Init)(struct SslDigestContext *context);
	int (*Update)(struct SslDigestContext *context, const void *data,
		      int length);
	int (*Final)(struct SslDigestContext *context, unsigned char *digest);
	int (*Free)(struct SslDigestContext *context);
	int (*Transfer)(struct SslDigestContext *context, unsigned char *buffer,
			SceUInt32 length, int save);
	SceUInt32 stateSize;
	SceUInt32 reserved;
} SslDigestImplementation;

_Static_assert(sizeof(SslDigestImplementation) == 28,
	       "SslDigestImplementation");

/* A hash's compression function on its chaining values: init, compress
 * whole blocks, output. The chaining values are valueCount words of
 * valueSize bytes at valueOffset in its state, saved big-endian unless
 * TransferValues says otherwise.
 */
typedef struct SslDigestCore {
	int (*Init)(void *state);
	int (*Compress)(void *state, const void *data, SceUInt32 length);
	int (*Output)(void *state, unsigned char *digest);
	int (*Free)(struct SslDigestContext *context);
	SceUInt32 reserved;
	SceUInt32 stateSize;
	unsigned char valueOffset;
	unsigned char valueCount;
	unsigned char valueSize;
	unsigned char reserved2;
	/* Unprototyped: the callers pass (context, buffer, save), the SHA-2
	 * cores' functions take (context, buffer, length, save) */
	SceUInt32 (*TransferValues)();
	SceUInt32 reserved3;
} SslDigestCore;

_Static_assert(sizeof(SslDigestCore) == 36, "SslDigestCore");

struct SslDigestDescriptor {
	const char *name;
	SslDerBuffer oid;
	unsigned char digestLength;
	unsigned char flags;
	unsigned short blockLength;
	const SslDigestImplementation *implementation;
	const SslDigestCore *innerImplementation;
	int (*GetInfo)(const SslDigestDescriptor *descriptor,
		       struct SslDigestContext *context, int selector,
		       SceUInt32 *value, const void **pointer);
	SceUInt32 type;
	SceUInt32 attributes;
	SceUInt32 reserved;
};

_Static_assert(sizeof(SslDigestDescriptor) == 40, "SslDigestDescriptor");

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
			     SceUInt32 options, int encrypt);
int sslLowLevelSign(SslLowLevelObject *object, void *signature,
		    SceUInt32 *signatureLength, SceUInt32 signatureMax,
		    const void *digest, SceUInt32 digestLength,
		    SceUInt32 options);
int sslLowLevelVerify(SslLowLevelObject *object, int *result,
		      const void *signature, SceUInt32 signatureLength,
		      const void *digest, SceUInt32 digestLength,
		      SceUInt32 options);
const SslLowLevelMethod *sslGetPkcs1SignMethod(void);
const SslLowLevelMethod *sslGetPkcs1VerifyMethod(void);
int sslLowLevelNullControl(SslLowLevelObject *object, int command, int arg,
			   void *value);
int sslForwardLowLevelSetInfo(SslLowLevelObject *object, int type, int id,
			      const void *value);
int sslForwardLowLevelGetInfo(SslLowLevelObject *object, int type, int id,
			      void *result);
int sslSetLowLevelObjectMethod(SslLowLevelObject *object,
			       const SslLowLevelMethod *method);
int sslSetLowLevelObjectNext(SslLowLevelObject *object,
			     SslLowLevelObject *next);
int sslInitLowLevelBigNumParams(SslLowLevelBigNumParams *params,
				const unsigned char *data, SceUInt32 length,
				SslBigNumContext *ctx, SceUInt32 options);

/* The saved state of an object, described by a schema: one word per
 * member, ended by 0. Bits 0-2 the kind (SSL_STATE_*), 3-5 the size of
 * one value in the saved data less one, 6-8 its size in the object less
 * one, 9 the offset is from the base an SSL_STATE_BASE member set (else
 * from the object), 10-21 the offset, 22-31 the count (0: the value of
 * the integer before it). Each member is saved as a 16-bit big-endian
 * length and the values, integers big-endian.
 */
#define SSL_STATE_END 0
#define SSL_STATE_BYTES 1
#define SSL_STATE_INTEGERS 2
#define SSL_STATE_POINTER 3
#define SSL_STATE_ALLOCATED 4
#define SSL_STATE_BASE 5
#define SSL_STATE_INTEGERS_SHIFT2 6
#define SSL_STATE_INTEGERS_SHIFT3 7

#define SSL_STATE_KIND(entry) ((entry) & 7)
#define SSL_STATE_WIDTH(entry) (((entry) >> 3) & 7)
#define SSL_STATE_SIZE(entry) (((entry) >> 6) & 7)
#define SSL_STATE_FROM_BASE 0x200
#define SSL_STATE_OFFSET(entry) (((entry) >> 10) & 0xfff)
#define SSL_STATE_COUNT_MASK 0xffc00000
#define SSL_STATE_COUNT(entry) ((entry) >> 22)

int sslEncodeObjectState(const void *object, const SceUInt32 *schema,
			 unsigned char *buffer, SceUInt32 *length,
			 SceUInt32 max);
int sslDecodeObjectState(void *object, const SceUInt32 *schema,
			 const unsigned char *buffer, SceUInt32 length,
			 SceUInt32 *used, SslDirectMemoryContext *dmem);

/* The low-level digest layer below the digest and HMAC backends */
/* SslDigestContext.flags */
#define SSL_DIGEST_CONTEXT_ALLOCATED 0x1
#define SSL_DIGEST_CONTEXT_INNER_STATE_ALLOCATED 0x10000
#define SSL_DIGEST_CONTEXT_STATE_ALLOCATED 0x20000

/* Digest GetInfo selectors of the state */
#define SSL_LL_DIGEST_INFO_SAVE_STATE 6
#define SSL_LL_DIGEST_INFO_RESTORE_STATE 7
#define SSL_LL_DIGEST_INFO_COPY 8
#define SSL_LL_DIGEST_INFO_SET_DIGEST 16
#define SSL_LL_DIGEST_INFO_SET_KEY 17
#define SSL_LL_DIGEST_INFO_FLAGS 19

int sslSaveDigestBackendContext(SslDigestContext *context,
				unsigned char *buffer, SceUInt32 *length,
				SceUInt32 max);
int sslRestoreDigestBackendContext(SslDigestContext *context,
				   const unsigned char *buffer,
				   SceUInt32 *length, SceUInt32 max);
SceUInt32 sslTransferDigestValues(SslDigestContext *context,
				  unsigned char *buffer, int save);
int sslLayOutDigestContext(SslDigestContext **result,
			   const SslDigestDescriptor *descriptor,
			   void *memory, SceUInt32 *size,
			   SslDirectMemoryContext *dmem);
int sslDestroyDigestBackendContext(SslDigestContext *context);
int sslCreateDigestBackendContext(SslDigestContext **context,
				  const SslDigestDescriptor *descriptor,
				  SslDirectMemoryContext *dmem);
int sslInitDigestBackendContext(SslDigestContext *context);
int sslUpdateDigestBackendContext(SslDigestContext *context,
				  const void *data, int length);
int sslFinalizeDigestBackendContext(SslDigestContext *context,
				    unsigned char *digest);
int sslSetDigestBackendContextKey(SslDigestContext *context,
				  const void *key, SceUInt32 length);
int sslQueryDigestBackendContextInfo(SslDigestContext *context, int selector,
				     SceUInt32 *value, const void **pointer);
int sslDuplicateDigestBackendContext(SslDigestContext **result,
				     SslDigestContext *context);
int sslGetDigestBackendContextInfo(SslDigestContext *context, int selector,
				   SceUInt32 *value, const void **pointer);
int sslGetLowLevelDigestInfo(const SslDigestDescriptor *descriptor,
			     struct SslDigestContext *context, int selector,
			     SceUInt32 *value, const void **pointer);
const SslDigestDescriptor *sslGetHmacDigestDescriptor(void);

/* The MD block buffering of the 64-byte block hashes */
extern const SslDigestImplementation sslMdBigEndianImplementation;
extern const SslDigestImplementation sslMdLittleEndianImplementation;

/* The hashes */
const SslDigestDescriptor *sslGetMd5SmallDigestDescriptor(void);
const SslDigestDescriptor *sslGetSha1SmallDigestDescriptor(void);
const SslDigestDescriptor *sslGetSha1Small16DigestDescriptor(void);
int sslGetSha1DigestValues(SslDigestContext *context, unsigned char *output);
const SslDigestDescriptor *sslGetSha256SdkDigestDescriptor(void);
const SslDigestDescriptor *sslGetSha224SdkDigestDescriptor(void);
const SslDigestDescriptor *sslGetSha512SdkDigestDescriptor(void);
const SslDigestDescriptor *sslGetSha384SdkDigestDescriptor(void);
SceUInt32 sslTransferSha256State(SslDigestContext *context,
				 unsigned char *buffer, SceUInt32 length,
				 int save);
SceUInt32 sslTransferSha512State(SslDigestContext *context,
				 unsigned char *buffer, SceUInt32 length,
				 int save);
extern const SslDigestImplementation sslSha256SdkImplementation;
extern const SslDigestImplementation sslSha512SdkImplementation;

#endif
