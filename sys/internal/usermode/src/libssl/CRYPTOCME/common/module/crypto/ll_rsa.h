/* Filename and descriptive names are ours: CRYPTOCME's low-level RSA layers,
 * chained below the signature and cipher backends: PKCS #1 v1.5 padding,
 * blinding, and the exponentiation with the key.
 */
#ifndef LIBSSL_CRYPTOCME_LL_RSA_H
#define LIBSSL_CRYPTOCME_LL_RSA_H

#include <scetypes.h>
#include "ll_obj.h"
#include "ll_bn.h"

/* The key numbers, in SetInfo order: n, e, d, then for each prime p, its
 * exponent and its CRT coefficient
 */
#define SSL_RSA_NUMBER_MODULUS 0
#define SSL_RSA_NUMBER_PUBLIC_EXPONENT 1
#define SSL_RSA_NUMBER_PRIVATE_EXPONENT 2
#define SSL_RSA_NUMBER_PRIMES 3
#define SSL_RSA_NUMBERS_PER_PRIME 3

/* SslRsaKeyState.modes: the operations the key supports */
#define SSL_RSA_MODE_PUBLIC 0x1
#define SSL_RSA_MODE_PRIVATE 0x2
#define SSL_RSA_MODE_CRT 0x4

/* The blinding exponent is missing */
#define SSL_RSA_ERROR_NO_BLINDING_EXPONENT 10042

/* The state of an exponentiation method */
typedef struct SslRsaKeyState {
	SslDirectMemoryContext *dmem;
	SceUInt32 unused;
	int ready;
	SceUInt32 modes;
	int primes;
	int numbersAllocated;
	int numbers;
	SslBigNum *number;
	int moduli;
	SslBigNumModulus **modulus;
	SslBigNumContext ctx;
	SslBigNumMethodContext *methods;
	const void *reserved;
	int (*Complete)(struct SslRsaKeyState *state);
} SslRsaKeyState;

_Static_assert(sizeof(SslRsaKeyState) == 392, "SslRsaKeyState");

/* SslRsaBlindingState.flags */
#define SSL_RSA_BLINDING_ENABLED 0x1
/* sslGetRsaBlindingInfo: blinding was applied to the last operation */
#define SSL_RSA_BLINDING_APPLIED 0x10

/* The state of the blinding layer: x * A before the exponentiation and
 * y * Ai after it, with A = r^e and Ai = r^-1 mod n
 */
typedef struct SslRsaBlindingState {
	SceUInt32 unused;
	void *random;
	int ready;
	SceUInt32 flags;
	SslBigNumContext ctx;
	SslBigNum blind;
	SslBigNum unblind;
	SslBigNum modulus;
	SslBigNum exponent;
	int active;
} SslRsaBlindingState;

_Static_assert(sizeof(SslRsaBlindingState) == 456, "SslRsaBlindingState");

/* A PKCS #1 v1.5 block type: its type byte and the mask every padding byte
 * matches
 */
typedef struct SslRsaPadding {
	unsigned char blockType;
	unsigned char mask;
} SslRsaPadding;

/* Options of a padded cipher operation: the SSLv2 rollback marker, eight
 * 0x03 padding bytes (RFC 2246 7.4.7.1)
 */
#define SSL_RSA_PADDING_ROLLBACK 0x1

/* SslRsaPaddingState.flags: the rollback marker was found */
#define SSL_RSA_PADDING_ROLLBACK_FOUND 0x1

/* The state of the padding layer */
typedef struct SslRsaPaddingState {
	void *random;
	void *scratch;
	SceUInt32 modulusLength;
	SceUInt32 flags;
} SslRsaPaddingState;

/* GetInfo SSL_LL_INFO_METHOD_NAME: the name of the arithmetic of one
 * modulus
 */
typedef struct SslRsaMethodName {
	const char *name;
	int index;
} SslRsaMethodName;

const SslLowLevelMethod *sslGetRsaPublicMontgomeryMethod(void);
const SslLowLevelMethod *sslGetRsaPrivateMontgomeryMethod(void);
int sslClearRsaKeyState(SslRsaKeyState *state);
int sslFreeRsaKeyState(SslRsaKeyState *state);
int sslRsaKeyControl(SslLowLevelObject *object, int command, int arg,
		     void *value);
int sslSetRsaKeyInfo(SslLowLevelObject *object, int type, int id,
		     const void *value);
int sslPrepareRsaKeyState(SslRsaKeyState *state, SceUInt32 modes);
int sslPrepareRsaPublicKeyState(SslRsaKeyState *state, SceUInt32 modes);
int sslRsaPrivateMontgomeryModExp(SslBigNum *r, SslBigNum *a,
				  void *state);
int sslRsaPublicMontgomeryModExp(SslBigNum *r, SslBigNum *a,
				 void *state);
int sslRsaKeyOperate(SslLowLevelObject *object, void *params, int command);
int sslGetRsaKeyInfo(SslLowLevelObject *object, int type, int id,
		     void *result);

const SslLowLevelMethod *sslGetRsaPublicReciprocalMethod(void);
const SslLowLevelMethod *sslGetRsaPrivateReciprocalMethod(void);
int sslFreeRsaReciprocalKeyState(SslRsaKeyState *state);
int sslRsaReciprocalKeyControl(SslLowLevelObject *object, int command,
			       int arg, void *value);
int sslSetRsaReciprocalKeyInfo(SslLowLevelObject *object, int type, int id,
			       const void *value);
int sslPrepareRsaReciprocalPublicKey(SslRsaKeyState *state, SceUInt32 modes);
int sslRsaPublicReciprocalModExp(SslBigNum *r, SslBigNum *a,
				 void *state);
int sslGetRsaReciprocalKeyInfo(SslLowLevelObject *object, int type, int id,
			       void *result);
int sslPrepareRsaReciprocalPrivateKey(SslRsaKeyState *state,
				      SceUInt32 modes);
int sslRsaPrivateReciprocalModExp(SslBigNum *r, SslBigNum *a,
				  void *state);

const SslLowLevelMethodGetter *sslGetRsaPkcs1SignChain(void);
const SslLowLevelMethodGetter *sslGetRsaPkcs1VerifyChain(void);
const SslLowLevelMethodGetter *sslGetRsaPkcs1EncryptChain(void);
const SslLowLevelMethodGetter *sslGetRsaPkcs1DecryptChain(void);
const SslLowLevelMethod *sslGetRsaBlindingMethod(void);
int sslRsaBlindingControl(SslLowLevelObject *object, int command, int arg,
			  void *value);
int sslSetRsaBlindingInfo(SslLowLevelObject *object, int type, int id,
			  const void *value);
int sslGetRsaBlindingInfo(SslLowLevelObject *object, int type, int id,
			  void *result);
int sslPrepareRsaBlinding(SslLowLevelObject *object);
int sslRsaBlindingOperate(SslLowLevelObject *object, void *params,
			  int command);

const SslLowLevelMethod *sslGetRsaPkcs1Type1DecodeMethod(void);
const SslLowLevelMethod *sslGetRsaPkcs1Type1EncodeMethod(void);
const SslLowLevelMethod *sslGetRsaPkcs1Type2DecodeMethod(void);
const SslLowLevelMethod *sslGetRsaPkcs1Type2EncodeMethod(void);
int sslRsaPaddingControl(SslLowLevelObject *object, int command, int arg,
			 void *value);
int sslSetRsaPaddingInfo(SslLowLevelObject *object, int type, int id,
			 const void *value);
int sslRsaPkcs1DecodeOperate(SslLowLevelObject *object, void *params,
			     int command);
int sslRsaPkcs1EncodeOperate(SslLowLevelObject *object, void *params,
			     int command);

#endif
