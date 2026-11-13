/* Filename and descriptive names are ours; BSAFE's bignum, a fork of SSLeay
 * 0.9.1b's BN with a memory context in every number and an error kept in the
 * context.
 */
#ifndef SSL_LL_BN_H
#define SSL_LL_BN_H

#include <scetypes.h>
#include "../dmem/dmem.h"

/* Errors kept in SslBigNumContext.error */
#define SSL_BIGNUM_ERROR_NO_MEMORY_CONTEXT 10014
#define SSL_BIGNUM_ERROR_STATIC_DATA 10020
#define SSL_BIGNUM_ERROR_INVALID_ARGUMENT 10017
#define SSL_BIGNUM_ERROR_BUFFER_TOO_SMALL 10011
#define SSL_BIGNUM_ERROR_DIVIDE_BY_ZERO 10007
#define SSL_BIGNUM_ERROR_CANCELLED 10004
#define SSL_BIGNUM_ERROR_NO_INVERSE 10008
#define SSL_BIGNUM_ERROR_NO_METHOD 10015
#define SSL_BIGNUM_ERROR_NOT_FOUND 10006
#define SSL_BIGNUM_ERROR_NO_EXPONENT 10010
#define SSL_BIGNUM_ERROR_BAD_INFO 10018
#define SSL_BIGNUM_ERROR_NOT_REDUCED 10021
#define SSL_BIGNUM_ERROR_NO_PRNG 10016

/* SslBigNum.flags */
#define SSL_BIGNUM_FLAG_ALLOCATED 0x1
#define SSL_BIGNUM_FLAG_STATIC_DATA 0x2

/* Free flags: wipe the words and the number before freeing them */
#define SSL_BIGNUM_FREE_WIPE 0x100

#define SSL_BIGNUM_CONTEXT_NUMBERS 12

/* SslBigNumContext.flags */
#define SSL_BIGNUM_CONTEXT_ALLOCATED 0x1

typedef unsigned long SslBigNumWord;

typedef struct SslBigNum {
	SslDirectMemoryContext *dmem;
	SslBigNumWord *d;
	SceUInt32 top;
	SceUInt32 dmax;
	int neg;
	SceUInt32 flags;
} SslBigNum;

_Static_assert(sizeof(SslBigNum) == 24, "SslBigNum");

typedef struct SslBigNumContext SslBigNumContext;
typedef struct SslBigNumMethodContext SslBigNumMethodContext;
typedef struct SslBigNumModulus SslBigNumModulus;

/* A method selector picks the arithmetic implementation for an operand size
 * (in bits when flags has SSL_BIGNUM_SIZE_IN_BITS, else in words).
 */
#define SSL_BIGNUM_SIZE_IN_BITS 0x2

typedef int (*SslBigNumMethodSelector)(SslBigNumMethodContext *context,
				       const void **method, int size,
				       int flags, SslBigNumContext *ctx);

/* A progress callback a long exponentiation reports to; a nonzero return
 * stops it
 */
typedef struct SslBigNumSurrender {
	void *reserved0;
	int (*Report)(struct SslBigNumSurrender *surrender, int type,
		      int progress, int reserved);
} SslBigNumSurrender;

/* The big number method context: chooses the arithmetic implementation for an
 * operand size; every context bound to it inherits its surrender callback
 * and its random number generator.
 */
struct SslBigNumMethodContext {
	SslDirectMemoryContext *dmem;
	SslBigNumMethodSelector selector;
	SslBigNumSurrender *surrender;
	void *prng;
	SceUInt32 flags;
};

_Static_assert(sizeof(SslBigNumMethodContext) == 20, "SslBigNumMethodContext");

/* SslBigNumMethodContext.flags, SslBigNumModulus.flags */
#define SSL_BIGNUM_METHOD_CONTEXT_ALLOCATED 0x1

struct SslBigNumContext {
	SslDirectMemoryContext *dmem;
	SslBigNumMethodContext *methods;
	int tos;
	SslBigNum bn[SSL_BIGNUM_CONTEXT_NUMBERS];
	int reserved300[6];
	SceUInt32 flags;
	int error;
	SslBigNumSurrender *surrender;
	void *prng;
};

/* SslBigNumContext.flags: stop a long exponentiation */
#define SSL_BIGNUM_CONTEXT_CANCEL 0x4000

/* Fixed-size word routines of an arithmetic method */
typedef struct SslBigNumWordMethods {
	void (*Multiply)(SslBigNumWord *r, const SslBigNumWord *a,
			 const SslBigNumWord *b);
	void (*Square)(SslBigNumWord *r, const SslBigNumWord *a);
	void (*Reduce)(SslBigNumWord *r, SslBigNumWord *t,
		       const SslBigNumWord *n, int words, SslBigNumWord n0);
} SslBigNumWordMethods;

/* An arithmetic implementation for one operand size */
typedef struct SslBigNumMethod {
	SceUInt32 words;
	const char *name;
	int (*IsAvailable)(const struct SslBigNumMethod *method);
	int (*ModExp)(SslBigNumModulus *m, SslBigNum *r, const SslBigNum *a,
		      const SslBigNum *p, int flags, SslBigNumContext *ctx);
	int (*ModMultiply)(SslBigNumModulus *m, SslBigNumWord *r,
			   const SslBigNumWord *a, const SslBigNumWord *b,
			   SslBigNumWord *t);
	int (*Init)(SslBigNumModulus *m);
	int (*Cleanup)(SslBigNumModulus *m);
	int (*SetInfo)(SslBigNumModulus *m, int type, const SslBigNum *bn,
		       int flags, SslBigNumContext *ctx);
	SceUInt32 level;
	const SslBigNumWordMethods *wordMethods;
} SslBigNumMethod;

/* SslBigNumMethod.SetInfo types */
#define SSL_BIGNUM_MODULUS_INFO_MODULUS 1
#define SSL_BIGNUM_MODULUS_INFO_EXPONENT 3

/* A modulus bound to the arithmetic method chosen for its size */
struct SslBigNumModulus {
	SslDirectMemoryContext *dmem;
	const SslBigNumMethod *method;
	void *state;
	SceUInt32 reserved12[4];
	SceUInt32 words;
	SceUInt32 flags;
};

_Static_assert(sizeof(SslBigNumModulus) == 36, "SslBigNumModulus");

/* SslBigNumModulus.SetInfo flags: keep the current method */
#define SSL_BIGNUM_MODULUS_KEEP_METHOD 0x1

/* BN_MONT_CTX */
typedef struct SslBigNumMontgomery {
	SslDirectMemoryContext *dmem;
	int useWord;
	int reserved8;
	int words;
	SslBigNum RR;
	SslBigNum N;
	SslBigNum Ni;
	SslBigNumWord n0;
	SceUInt32 flags;
	unsigned char *recoding;
} SslBigNumMontgomery;

_Static_assert(sizeof(SslBigNumMontgomery) == 100, "SslBigNumMontgomery");

/* A recoded exponent: a header, then (power, squarings) byte pairs ending
 * with a zero pair
 */
#define SSL_BIGNUM_RECODING_HEADER 6

/* BN_RECP_CTX */
typedef struct SslBigNumReciprocal {
	SslDirectMemoryContext *dmem;
	SslBigNum N;
	SslBigNum Nr;
	int numBits;
	int shift;
	SceUInt32 flags;
} SslBigNumReciprocal;

_Static_assert(sizeof(SslBigNumReciprocal) == 64, "SslBigNumReciprocal");

_Static_assert(sizeof(struct SslBigNumContext) == 340, "SslBigNumContext");

#define sslIsBigNumZero(bn)						\
	((bn)->top == 0 || ((bn)->top == 1 && (bn)->d[0] == 0))
#define sslIsBigNumOne(bn) ((bn)->top == 1 && (bn)->d[0] == 1)

/* The working state of an exponentiation, kept in the words of a context
 * number
 */
typedef struct SslBigNumModExpState {
	SceUInt32 words;
	SceUInt32 padWords;
	SceUInt32 tableSize;
	SceUInt32 index;
	SceUInt32 reserved16;
	SceUInt32 squarings;
	SslBigNumMontgomery *mont;
	const unsigned char *recoding;
	SslBigNum *base;
	SslBigNum *table;
	SslBigNumWord *product;
	SslBigNumWord *temp;
	SslBigNumWord *square;
	SslBigNumWord *accumulator;
	SslBigNumSurrender *surrender;
	int progress;
	SslBigNumWord *powers[16];
} SslBigNumModExpState;

_Static_assert(sizeof(SslBigNumModExpState) == 128, "SslBigNumModExpState");

int sslExpandBigNum(SslBigNum *bn, SceUInt32 words, int keep,
		    SslBigNumContext *ctx);
int sslSetBigNumData(SslBigNum *bn, const unsigned char *data,
		     SceUInt32 length, SslBigNumContext *ctx);
int sslGetWordBitLength(SslBigNumWord word);
int sslGetBigNumBitLength(const SslBigNum *bn);
int sslGetBigNumData(SceUInt32 *length, unsigned char *data, SceUInt32 max,
		     const SslBigNum *bn, SslBigNumContext *ctx);
int sslAddBigNumUnsigned(SslBigNum *r, const SslBigNum *a, const SslBigNum *b,
			 SslBigNumContext *ctx);
int sslSubtractBigNumUnsigned(SslBigNum *r, const SslBigNum *a,
			      const SslBigNum *b, SslBigNumContext *ctx);
int sslCompareBigNumWords(const SslBigNumWord *a, const SslBigNumWord *b,
			  int n);
int sslCompareBigNumUnsigned(const SslBigNum *a, const SslBigNum *b,
			     SslBigNumContext *ctx);
int sslAddBigNum(SslBigNum *r, const SslBigNum *a, const SslBigNum *b,
		 SslBigNumContext *ctx);
int sslSubtractBigNum(SslBigNum *r, const SslBigNum *a, const SslBigNum *b,
		      SslBigNumContext *ctx);
int sslSetBigNumBit(SslBigNum *bn, SceUInt32 bit, SslBigNumContext *ctx);
int sslIsBigNumBitSet(const SslBigNum *bn, SceUInt32 bit);
int sslCompareBigNum(const SslBigNum *a, const SslBigNum *b,
		     SslBigNumContext *ctx);
void sslFreeBigNum(SslBigNum *bn, SceUInt32 flags);
void sslInitBigNum(SslBigNum *bn, SslDirectMemoryContext *dmem);
int sslNewBigNum(SslBigNum **result, SslDirectMemoryContext *dmem);
void sslInitBigNumContext(SslBigNumContext *ctx, SslDirectMemoryContext *dmem);
int sslNewBigNumContext(SslBigNumContext **result,
			SslDirectMemoryContext *dmem);
void sslFreeBigNumContext(SslBigNumContext *ctx, SceUInt32 flags);
SslDirectMemoryContext *sslGetBigNumMemoryContext(const SslBigNum *bn);
int sslCopyBigNum(SslBigNum *a, const SslBigNum *b, SslBigNumContext *ctx);
int sslLeftShiftBigNum(SslBigNum *r, const SslBigNum *a, SceUInt32 n,
		       SslBigNumContext *ctx);
int sslDivideBigNum(SslBigNum *dv, SslBigNum *rm, const SslBigNum *num,
		    const SslBigNum *divisor, SslBigNumContext *ctx);
int sslModBigNum(SslBigNum *rem, const SslBigNum *m, const SslBigNum *d,
		 SslBigNumContext *ctx);
int sslSetBigNumWord(SslBigNum *bn, SslBigNumWord word, SslBigNumContext *ctx);
int sslMultiplyBigNum(SslBigNum *r, const SslBigNum *a, const SslBigNum *b,
		      SslBigNumContext *ctx);
int sslSquareBigNum(SslBigNum *r, const SslBigNum *a, SslBigNumContext *ctx);
int sslAddBigNumWord(SslBigNum *bn, int neg, SslBigNumWord word,
		     SslBigNumContext *ctx);
void sslMultiplyBigNumWords(SslBigNumWord *r, const SslBigNumWord *a, int na,
			    const SslBigNumWord *b, int nb);
void sslSquareBigNumWords(SslBigNumWord *r, const SslBigNumWord *a, int n,
			  SslBigNumWord *tmp);
void sslMontgomeryReduceWords(SslBigNumWord *r, SslBigNumWord *t,
			      const SslBigNumWord *n, int words,
			      SslBigNumWord n0);
int sslRecodeBigNumExponent(const SslBigNum *e, unsigned char **result,
			    SslDirectMemoryContext *dmem, int unused,
			    SslBigNumContext *ctx);
void sslInitBigNumMontgomery(SslBigNumMontgomery *mont,
			     SslDirectMemoryContext *dmem);
void sslFreeBigNumMontgomery(SslBigNumMontgomery *mont);
int sslSetBigNumMontgomery(SslBigNumMontgomery *mont, const SslBigNum *mod,
			   SslBigNumContext *ctx);
int sslModExpBigNumMontgomery(SslBigNum *r, const SslBigNum *a,
			      const SslBigNum *p, SslBigNumMontgomery *mont,
			      SslBigNumContext *ctx);
int sslModExpBigNum(SslBigNum *r, const SslBigNum *a, const SslBigNum *p,
		    const SslBigNum *m, SslBigNumContext *ctx);
int sslModInverseBigNum(SslBigNum *r, const SslBigNum *a, const SslBigNum *n,
			SslBigNumContext *ctx);
SslBigNumWord sslGetMontgomeryN0(SslBigNumWord n);
int sslSetBigNumContextMethods(SslBigNumContext *ctx,
			       SslBigNumMethodContext *methods);
int sslSetBigNumMethodSelector(SslBigNumMethodContext *context,
			       SslBigNumMethodSelector selector);
int sslNewBigNumMethodContext(SslBigNumMethodContext **result,
			      SslDirectMemoryContext *dmem);
int sslFreeBigNumMethodContext(SslBigNumMethodContext *context);
int sslWordBigNumMethodIsAvailable(const SslBigNumMethod *method);
void sslInitBigNumModulus(SslBigNumModulus *m, SslDirectMemoryContext *dmem);
int sslNewBigNumModulus(SslBigNumModulus **result,
			SslDirectMemoryContext *dmem);
void sslFreeBigNumModulus(SslBigNumModulus *m);
int sslSetBigNumModulusInfo(SslBigNumModulus *m, int type, const SslBigNum *bn,
			    const SslBigNumMethod *method, int flags,
			    SslBigNumContext *ctx);
int sslModExpBigNumModulus(SslBigNumModulus *m, SslBigNum *r, SslBigNum *a,
			   const SslBigNum *p, int flags,
			   SslBigNumContext *ctx);
int sslSelectBigNumMethod(SslBigNumMethodContext *context, const void **method,
			  int size, int flags, SslBigNumContext *ctx);
extern const SslBigNumMethod sslWordBigNumMethod;
const SslBigNumMethod *sslGetWordBigNumMethod(void);
int sslSelectDefaultBigNumMethod(SslBigNumMethodContext *context,
				 const void **method, int size, int flags,
				 SslBigNumContext *ctx);
void sslInitBigNumReciprocal(SslBigNumReciprocal *recp,
			     SslDirectMemoryContext *dmem);
void sslFreeBigNumReciprocal(SslBigNumReciprocal *recp, SceUInt32 flags);
int sslSetBigNumReciprocal(SslBigNumReciprocal *recp, const SslBigNum *d,
			   SslBigNumContext *ctx);
int sslGetBigNumReciprocal(SslBigNum *r, const SslBigNum *m, int len,
			   SslBigNumContext *ctx);
int sslDivideBigNumReciprocal(SslBigNum *dv, SslBigNum *rem,
			      const SslBigNum *m, SslBigNumReciprocal *recp,
			      SslBigNumContext *ctx);
int sslModMultiplyBigNumReciprocal(SslBigNum *r, const SslBigNum *x,
				   const SslBigNum *y,
				   SslBigNumReciprocal *recp,
				   SslBigNumContext *ctx);
int sslModExpBigNumReciprocal(SslBigNum *r, const SslBigNum *a,
			      const SslBigNum *p, SslBigNumReciprocal *recp,
			      SslBigNumContext *ctx);
int sslRightShiftBigNum(SslBigNum *r, const SslBigNum *a, SceUInt32 n,
			SslBigNumContext *ctx);
int sslModMultiplyBigNum(SslBigNum *r, const SslBigNum *a, const SslBigNum *b,
			 const SslBigNum *m, SslBigNumContext *ctx);
int sslModExpBigNumReciprocalSimple(SslBigNum *r, const SslBigNum *a,
				    const SslBigNum *p,
				    SslBigNumReciprocal *recp,
				    SslBigNumContext *ctx);
int sslRandomBigNum(SslBigNum *rnd, void *prng, SceUInt32 bits, int top,
		    int bottom, SslBigNumContext *ctx);
int sslSubtractBigNumWordsAbsolute(SslBigNumWord *r, SslBigNumWord *a,
				   SslBigNumWord *b, int n);
SslBigNumWord sslDivideBigNumWord(SslBigNumWord h, SslBigNumWord l,
				  SslBigNumWord d);

/* The low-level random number generator (cry_prng.c's backend) */
int sslGenerateLowLevelPrng(void *prng, void *buffer,
			    SceUInt32 *generatedLength, SceUInt32 length);

int sslNewBigNumModExpState(SslBigNumModExpState **result,
			    SslBigNumModulus *m, SslBigNum *r,
			    const SslBigNum *a, const SslBigNum *p,
			    SceUInt32 scratch, SslBigNumContext *ctx);
int sslWordBigNumMethodInit(SslBigNumModulus *m);
int sslWordBigNumMethodCleanup(SslBigNumModulus *m);
int sslWordBigNumMethodSetInfo(SslBigNumModulus *m, int type,
			       const SslBigNum *bn, int flags,
			       SslBigNumContext *ctx);
int sslFixedBigNumMethodModMultiply(SslBigNumModulus *m, SslBigNumWord *r,
				    const SslBigNumWord *a,
				    const SslBigNumWord *b, SslBigNumWord *t);
int sslFixedBigNumMethodModExp(SslBigNumModulus *m, SslBigNum *r,
			       const SslBigNum *a, const SslBigNum *p,
			       int flags, SslBigNumContext *ctx);

#define SSL_BIGNUM_FIXED_WORDS(n)					\
	void sslMultiplyBigNumWordsC##n(SslBigNumWord *r,		\
					const SslBigNumWord *a,		\
					const SslBigNumWord *b);	\
	void sslSquareBigNumWordsC##n(SslBigNumWord *r, const SslBigNumWord *a)
SSL_BIGNUM_FIXED_WORDS(4);
SSL_BIGNUM_FIXED_WORDS(6);
SSL_BIGNUM_FIXED_WORDS(8);
SSL_BIGNUM_FIXED_WORDS(11);
SSL_BIGNUM_FIXED_WORDS(16);

/* SSLeay's word routines (bn_asm.c) */
SslBigNumWord bn_mul_add_words(SslBigNumWord *rp, SslBigNumWord *ap, int num,
			       SslBigNumWord w);
SslBigNumWord bn_mul_words(SslBigNumWord *rp, SslBigNumWord *ap, int num,
			   SslBigNumWord w);
void bn_sqr_words(SslBigNumWord *r, SslBigNumWord *a, int n);
SslBigNumWord bn_div_words(SslBigNumWord h, SslBigNumWord l, SslBigNumWord d);
SslBigNumWord bn_add_words(SslBigNumWord *r, SslBigNumWord *a,
			   SslBigNumWord *b, int n);
SslBigNumWord bn_sub_words(SslBigNumWord *r, SslBigNumWord *a,
			   SslBigNumWord *b, int n);

const SslBigNumMethod *sslGetWordC4BigNumMethod(void);
const SslBigNumMethod *sslGetWordC6BigNumMethod(void);
const SslBigNumMethod *sslGetWordC8BigNumMethod(void);
const SslBigNumMethod *sslGetWordC11BigNumMethod(void);
const SslBigNumMethod *sslGetWordR612BigNumMethod(void);
const SslBigNumMethod *sslGetWordC16BigNumMethod(void);
const SslBigNumMethod *sslGetWordR1122BigNumMethod(void);
const SslBigNumMethod *sslGetWordR624BigNumMethod(void);
const SslBigNumMethod *sslGetWordR1632BigNumMethod(void);
const SslBigNumMethod *sslGetWordR1144BigNumMethod(void);
const SslBigNumMethod *sslGetWordR648BigNumMethod(void);
const SslBigNumMethod *sslGetWordR1664BigNumMethod(void);
const SslBigNumMethod *sslGetWordR1188BigNumMethod(void);
const SslBigNumMethod *sslGetWordR696BigNumMethod(void);
const SslBigNumMethod *sslGetWordR16128BigNumMethod(void);
const SslBigNumMethod *sslGetWordR16256BigNumMethod(void);

#endif
