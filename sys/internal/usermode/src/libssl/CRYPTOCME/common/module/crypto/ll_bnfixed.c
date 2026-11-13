/* Filename is ours: BSAFE's fixed-size "word cN" arithmetic methods. Each
 * exponentiates with the comba routines for its size, zero-extending every
 * table entry to the method's size.
 */
#include "ll_bn.h"
#include "../../../../memory.h"

#define SSL_BIGNUM_PAD(w, s)						\
	do {								\
		SceUInt32 i_;						\
		for (i_ = (s)->words; i_ < (s)->padWords; i_++)		\
			(w)[i_] = 0;					\
	} while (0)

#define SSL_BIGNUM_FIXED_METHOD(n, name)				\
static int sslWordC##n##BigNumMethodModExp(SslBigNumModulus *m,	\
					   SslBigNum *r,		\
					   const SslBigNum *a,		\
					   const SslBigNum *p, int flags,	\
					   SslBigNumContext *ctx);		\
									\
static const SslBigNumWordMethods sslWordC##n##BigNumWordMethods = {	\
	sslMultiplyBigNumWordsC##n,					\
	sslSquareBigNumWordsC##n,					\
	sslMontgomeryReduceWords					\
};									\
									\
static const SslBigNumMethod sslWordC##n##BigNumMethod = {		\
	n,								\
	name,								\
	sslWordBigNumMethodIsAvailable,					\
	sslWordC##n##BigNumMethodModExp,				\
	sslFixedBigNumMethodModMultiply,				\
	sslWordBigNumMethodInit,					\
	sslWordBigNumMethodCleanup,					\
	sslWordBigNumMethodSetInfo,					\
	0,								\
	&sslWordC##n##BigNumWordMethods					\
};									\
									\
const SslBigNumMethod *sslGetWordC##n##BigNumMethod(void)		\
{									\
	return &sslWordC##n##BigNumMethod;				\
}									\
									\
static int sslWordC##n##BigNumMethodModExp(SslBigNumModulus *m,	\
					   SslBigNum *r,		\
					   const SslBigNum *a,		\
					   const SslBigNum *p, int flags,	\
					   SslBigNumContext *ctx)		\
{									\
	SslBigNumModExpState *s = NULL;					\
	SceUInt32 i, k;							\
	int status, top, tos;						\
									\
	if (ctx->error != 0)						\
		return ctx->error;					\
	tos = ctx->tos;							\
	if (sslNewBigNumModExpState(&s, m, r, a, p, 8, ctx) != 0)	\
		goto out;						\
	s->accumulator = s->table->d + s->padWords * 4;			\
	sslMultiplyBigNumWordsC##n(s->product, s->base->d,		\
				   s->mont->RR.d);			\
	sslMontgomeryReduceWords(s->powers[0], s->product,		\
				 s->mont->N.d, s->words, s->mont->n0);	\
	SSL_BIGNUM_PAD(s->powers[0], s);				\
	if (s->tableSize > 1) {						\
		sslSquareBigNumWordsC##n(s->product, s->powers[0]);	\
		sslMontgomeryReduceWords(s->square, s->product,		\
					 s->mont->N.d, s->words,	\
					 s->mont->n0);			\
		SSL_BIGNUM_PAD(s->square, s);				\
		for (k = 1; k < s->tableSize; k++) {			\
			sslMultiplyBigNumWordsC##n(s->product,		\
						   s->powers[k - 1],	\
						   s->square);		\
			sslMontgomeryReduceWords(s->powers[k],		\
						 s->product,		\
						 s->mont->N.d,		\
						 s->words,		\
						 s->mont->n0);		\
			SSL_BIGNUM_PAD(s->powers[k], s);		\
		}							\
	}								\
	sslCopyMemory(s->accumulator, s->powers[s->index >> 1],	\
		      s->padWords * 4);					\
	while (s->squarings != 0) {					\
		for (i = 0; i < s->squarings; i++) {			\
			sslSquareBigNumWordsC##n(s->product,		\
						 s->accumulator);	\
			sslMontgomeryReduceWords(s->accumulator,	\
						 s->product,		\
						 s->mont->N.d,		\
						 s->words,		\
						 s->mont->n0);		\
		}							\
		s->index = *s->recoding++;				\
		s->squarings = *s->recoding++;				\
		if (s->index + s->squarings == 0)			\
			break;						\
		if (s->index == 0)					\
			continue;					\
		if (s->squarings == 0 && s->index == 1) {		\
			sslMultiplyBigNumWordsC##n(s->product,		\
						   s->accumulator,	\
						   s->base->d);		\
			sslMontgomeryReduceWords(r->d, s->product,	\
						 s->mont->N.d,		\
						 s->words,		\
						 s->mont->n0);		\
			top = s->words;					\
			goto fix;					\
		}							\
		sslMultiplyBigNumWordsC##n(s->product, s->accumulator,	\
					   s->powers[s->index >> 1]);	\
		sslMontgomeryReduceWords(s->accumulator, s->product,	\
					 s->mont->N.d, s->words,	\
					 s->mont->n0);			\
	}								\
	sslFillMemory(s->accumulator + s->words, 0, s->words * 4);	\
	sslMontgomeryReduceWords(r->d, s->accumulator, s->mont->N.d,	\
				 s->words, s->mont->n0);		\
	top = s->words;							\
fix:									\
	for (top--; top >= 0; top--)					\
		if (r->d[top] != 0)					\
			break;						\
	r->top = top + 1;						\
out:									\
	status = ctx->error;						\
	ctx->tos = tos;							\
	return status;							\
}

SSL_BIGNUM_FIXED_METHOD(11, "word c11")
SSL_BIGNUM_FIXED_METHOD(16, "word c16")
SSL_BIGNUM_FIXED_METHOD(4, "word c4")
SSL_BIGNUM_FIXED_METHOD(6, "word c6")
SSL_BIGNUM_FIXED_METHOD(8, "word c8")
