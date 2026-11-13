/* Filename is ours: BSAFE's "word BrN" arithmetic methods. They split an
 * operand of N words in halves, level times (Karatsuba), down to one of the
 * fixed-size comba routines.
 */
#include "ll_bn.h"
#include "../../../../memory.h"

/* How deep to split, and the routines at the bottom */
typedef struct SslBigNumKaratsuba {
	SceUInt32 level;
	int words;
	void (*Multiply)(SslBigNumWord *r, const SslBigNumWord *a,
			 const SslBigNumWord *b, int words);
	void (*Square)(SslBigNumWord *r, const SslBigNumWord *a, int words);
} SslBigNumKaratsuba;

typedef void (*SslBigNumKaratsubaMultiply)(SslBigNumWord *r,
					   const SslBigNumWord *a,
					   const SslBigNumWord *b, int words);
typedef void (*SslBigNumKaratsubaSquare)(SslBigNumWord *r,
					 const SslBigNumWord *a, int words);

/* r = -a mod 2^(32 * n) */
void sslNegateBigNumWords(SslBigNumWord *r, const SslBigNumWord *a, int n)
{
	SslBigNumWord x;
	int i;

	for (i = 0; i < n; i++) {
		x = ~a[i] + 1;
		r[i] = x;
		if (x != 0)
			break;
	}
	for (i += 4; i < n; i += 4) {
		r[i - 3] = ~a[i - 3];
		r[i - 2] = ~a[i - 2];
		r[i - 1] = ~a[i - 1];
		r[i] = ~a[i];
	}
	for (i -= 3; i < n; i++)
		r[i] = ~a[i];
}

/* r[0..2n) = a[0..n) * b[0..n), t holding 2n words per level */
void sslMultiplyBigNumKaratsuba(SslBigNumWord *r, const SslBigNumWord *a,
				const SslBigNumWord *b, SslBigNumWord *t,
				SslBigNumKaratsuba *k)
{
	int n = k->words;
	int h = n >> 1;
	SslBigNumWord *rh = &r[n];
	const SslBigNumWord *bh = b + h;
	SslBigNumWord *db = &t[n + h];
	SslBigNumWord *da = &t[n];
	const SslBigNumWord *ah = a + h;
	SslBigNumKaratsubaMultiply multiply;
	SslBigNumWord *tt, c, *p, x;
	int neg, sign;

	if (k->level == 0) {
		multiply = k->Multiply;
		multiply(r, a, b, h);
		multiply(rh, ah, bh, h);
		neg = 0;
		if (bn_sub_words(da, (SslBigNumWord *)a, (SslBigNumWord *)ah,
				 h) != 0) {
			neg = 1;
			sslNegateBigNumWords(da, da, h);
		}
		sign = neg;
		if (bn_sub_words(db, (SslBigNumWord *)bh, (SslBigNumWord *)b,
				 h) != 0) {
			sign = neg ^ 1;
			sslNegateBigNumWords(db, db, h);
		}
		multiply(t, da, db, h);
	} else {
		k->level--;
		k->words = h;
		tt = &t[2 * n];
		sslMultiplyBigNumKaratsuba(r, a, b, tt, k);
		sslMultiplyBigNumKaratsuba(rh, ah, bh, tt, k);
		neg = 0;
		if (bn_sub_words(da, (SslBigNumWord *)a, (SslBigNumWord *)ah,
				 h) != 0) {
			neg = 1;
			sslNegateBigNumWords(da, da, h);
		}
		sign = neg;
		if (bn_sub_words(db, (SslBigNumWord *)bh, (SslBigNumWord *)b,
				 h) != 0) {
			sign = neg ^ 1;
			sslNegateBigNumWords(db, db, h);
		}
		sslMultiplyBigNumKaratsuba(t, da, db, tt, k);
		k->level++;
		k->words = n;
	}
	c = bn_add_words(da, r, rh, n);
	if (sign != 0)
		c -= bn_sub_words(t, da, t, n);
	else
		c += bn_add_words(t, da, t, n);
	t[n] = c;
	if (bn_add_words(&r[h], &r[h], t, n + 1) != 0) {
		p = &r[n + h + 1];
		do {
			x = *p + 1;
			*p++ = x;
		} while (x == 0);
	}
}

/* r[0..2n) = a[0..n)^2, t holding 2n words per level */
void sslSquareBigNumKaratsuba(SslBigNumWord *r, const SslBigNumWord *a,
			      SslBigNumWord *t, SslBigNumKaratsuba *k)
{
	int n = k->words;
	SceUInt32 level = k->level;
	int h = n / 2;
	SslBigNumWord *rh = r + n;
	const SslBigNumWord *ah = a + h;
	SslBigNumWord *d = t + n;
	SslBigNumKaratsubaSquare square;
	SslBigNumWord c, *p, x;

	if (level != 0) {
		k->words = h;
		k->level = level - 1;
		sslSquareBigNumKaratsuba(r, a, t + 2 * n, k);
		sslSquareBigNumKaratsuba(rh, ah, t + 2 * n, k);
		if (bn_sub_words(d, (SslBigNumWord *)a, (SslBigNumWord *)ah,
				 h) != 0)
			sslNegateBigNumWords(d, d, h);
		sslSquareBigNumKaratsuba(t, d, t + 2 * n, k);
		k->words = n;
		k->level++;
	} else {
		square = k->Square;
		square(r, a, h);
		square(rh, ah, h);
		if (bn_sub_words(d, (SslBigNumWord *)a, (SslBigNumWord *)ah,
				 h) != 0)
			sslNegateBigNumWords(d, d, h);
		square(t, d, h);
	}
	c = bn_add_words(d, r, rh, n);
	t[n] = c - bn_sub_words(t, d, t, n);
	if (bn_add_words(r + h, r + h, t, n + 1) != 0) {
		p = r + n + h + 1;
		do {
			x = *p + 1;
			*p++ = x;
		} while (x == 0);
	}
}

int sslKaratsubaBigNumMethodModMultiply(SslBigNumModulus *m,
					SslBigNumWord *r,
					const SslBigNumWord *a,
					const SslBigNumWord *b,
					SslBigNumWord *t)
{
	const SslBigNumMethod *method = m->method;
	SslBigNumMontgomery *mont = m->state;
	const SslBigNumWordMethods *wm = method->wordMethods;
	SslBigNumKaratsuba k;
	SslBigNumWord *product;
	SceUInt32 *pad;
	int i;

	k.level = method->level;
	k.words = mont->words;
	k.Multiply = (SslBigNumKaratsubaMultiply)wm->Multiply;
	k.Square = (SslBigNumKaratsubaSquare)wm->Square;
	if (a != NULL) {
		if (b == NULL)
			b = mont->RR.d;
		if (a == b)
			sslSquareBigNumKaratsuba(t, a, r, &k);
		else
			sslMultiplyBigNumKaratsuba(t, a, b, r, &k);
		product = t;
	} else {
		i = mont->words;
		if (i > 0) {
			pad = (SceUInt32 *)b + i;
			do
				*pad++ = 0;
			while (--i != 0);
		}
		product = (SslBigNumWord *)b;
	}
	wm->Reduce(r, product, mont->N.d, mont->words, mont->n0);
	return 0;
}

#define SSL_BIGNUM_PAD(w, s)						\
	do {								\
		SceUInt32 i_;						\
		for (i_ = (s)->words; i_ < (s)->padWords; i_++)		\
			(w)[i_] = 0;					\
	} while (0)

int sslKaratsubaBigNumMethodModExp(SslBigNumModulus *m, SslBigNum *r,
				   const SslBigNum *a, const SslBigNum *p,
				   int flags, SslBigNumContext *ctx)
{
	SslBigNumModExpState *s = NULL;
	const SslBigNumMethod *method;
	const SslBigNumWordMethods *wm;
	SslBigNumKaratsuba k;
	void (*reduce)(SslBigNumWord *r, SslBigNumWord *t,
		       const SslBigNumWord *n, int words, SslBigNumWord n0);
	SceUInt32 i, j;
	int status, top, tos;

	if (ctx->error != 0)
		return ctx->error;
	tos = ctx->tos;
	if (sslNewBigNumModExpState(&s, m, r, a, p, 9, ctx) != 0)
		goto out;
	s->accumulator = s->table->d + s->padWords * 3;
	s->temp = s->table->d + s->padWords * 5;
	method = m->method;
	k.level = method->level;
	k.words = s->padWords;
	wm = method->wordMethods;
	k.Multiply = (SslBigNumKaratsubaMultiply)wm->Multiply;
	k.Square = (SslBigNumKaratsubaSquare)wm->Square;
	reduce = wm->Reduce;
	sslMultiplyBigNumKaratsuba(s->product, s->base->d, s->mont->RR.d,
				   s->temp, &k);
	reduce(s->powers[0], s->product, s->mont->N.d, s->words,
	       s->mont->n0);
	SSL_BIGNUM_PAD(s->powers[0], s);
	if (s->tableSize > 1) {
		sslSquareBigNumKaratsuba(s->product, s->powers[0], s->temp,
					 &k);
		reduce(s->square, s->product, s->mont->N.d, s->words,
		       s->mont->n0);
		SSL_BIGNUM_PAD(s->square, s);
		for (j = 1; j < s->tableSize; j++) {
			sslMultiplyBigNumKaratsuba(s->product, s->powers[j - 1],
						   s->square, s->temp, &k);
			reduce(s->powers[j], s->product, s->mont->N.d,
			       s->words, s->mont->n0);
			SSL_BIGNUM_PAD(s->powers[j], s);
		}
	}
	sslCopyMemory(s->accumulator, s->powers[s->index >> 1],
		      s->padWords * 4);
	while (s->squarings != 0) {
		for (i = 0; i < s->squarings; i++) {
			sslSquareBigNumKaratsuba(s->product, s->accumulator,
						 s->temp, &k);
			reduce(s->accumulator, s->product, s->mont->N.d,
			       s->words, s->mont->n0);
		}
		s->index = *s->recoding++;
		s->squarings = *s->recoding++;
		if (s->index + s->squarings == 0)
			break;
		if (s->index == 0)
			continue;
		if (s->squarings == 0 && s->index == 1) {
			sslMultiplyBigNumKaratsuba(s->product, s->accumulator,
						   s->base->d, s->temp, &k);
			reduce(r->d, s->product, s->mont->N.d, s->words,
			       s->mont->n0);
			top = s->words;
			goto fix;
		}
		sslMultiplyBigNumKaratsuba(s->product, s->accumulator,
					   s->powers[s->index >> 1], s->temp,
					   &k);
		reduce(s->accumulator, s->product, s->mont->N.d, s->words,
		       s->mont->n0);
	}
	for (i = s->words; i < s->words * 2; i++)
		s->accumulator[i] = 0;
	reduce(r->d, s->accumulator, s->mont->N.d, s->words, s->mont->n0);
	top = s->words;
fix:
	for (top--; top >= 0; top--)
		if (r->d[top] != 0)
			break;
	r->top = top + 1;
out:
	status = ctx->error;
	ctx->tos = tos;
	return status;
}

/* The comba routines at the bottom of the splits; writable like BSAFE's */
static SslBigNumWordMethods sslWordR11BigNumWordMethods = {
	sslMultiplyBigNumWordsC11,
	sslSquareBigNumWordsC11,
	sslMontgomeryReduceWords
};

static SslBigNumWordMethods sslWordR16BigNumWordMethods = {
	sslMultiplyBigNumWordsC16,
	sslSquareBigNumWordsC16,
	sslMontgomeryReduceWords
};

static SslBigNumWordMethods sslWordR6BigNumWordMethods = {
	sslMultiplyBigNumWordsC6,
	sslSquareBigNumWordsC6,
	sslMontgomeryReduceWords
};

#define SSL_BIGNUM_KARATSUBA_METHOD(base, n, level)			\
static const SslBigNumMethod sslWordR##base##n##BigNumMethod = {	\
	n,								\
	"word " #base "r" #n,						\
	sslWordBigNumMethodIsAvailable,					\
	sslKaratsubaBigNumMethodModExp,					\
	sslKaratsubaBigNumMethodModMultiply,				\
	sslWordBigNumMethodInit,					\
	sslWordBigNumMethodCleanup,					\
	sslWordBigNumMethodSetInfo,					\
	level,								\
	&sslWordR##base##BigNumWordMethods				\
};									\
									\
const SslBigNumMethod *sslGetWordR##base##n##BigNumMethod(void)	\
{									\
	return &sslWordR##base##n##BigNumMethod;			\
}

SSL_BIGNUM_KARATSUBA_METHOD(11, 22, 0)
SSL_BIGNUM_KARATSUBA_METHOD(11, 44, 1)
SSL_BIGNUM_KARATSUBA_METHOD(11, 88, 2)
SSL_BIGNUM_KARATSUBA_METHOD(16, 32, 0)
SSL_BIGNUM_KARATSUBA_METHOD(16, 64, 1)
SSL_BIGNUM_KARATSUBA_METHOD(16, 128, 2)
SSL_BIGNUM_KARATSUBA_METHOD(16, 256, 3)
SSL_BIGNUM_KARATSUBA_METHOD(6, 12, 0)
SSL_BIGNUM_KARATSUBA_METHOD(6, 24, 1)
SSL_BIGNUM_KARATSUBA_METHOD(6, 48, 2)
SSL_BIGNUM_KARATSUBA_METHOD(6, 96, 3)

/* The ModMultiply of the fixed-size methods, through their word routines */
int sslFixedBigNumMethodModMultiply(SslBigNumModulus *m, SslBigNumWord *r,
				    const SslBigNumWord *a,
				    const SslBigNumWord *b, SslBigNumWord *t)
{
	const SslBigNumWordMethods *wm = m->method->wordMethods;
	SslBigNumMontgomery *mont = m->state;
	SslBigNumWord *product;
	SceUInt32 *pad;
	int i;

	if (a != NULL) {
		if (b == NULL)
			b = mont->RR.d;
		if (a == b)
			wm->Square(t, a);
		else
			wm->Multiply(t, a, b);
		product = t;
	} else {
		i = mont->words;
		if (i > 0) {
			pad = (SceUInt32 *)b + i;
			do
				*pad++ = 0;
			while (--i != 0);
		}
		product = (SslBigNumWord *)b;
	}
	wm->Reduce(r, product, mont->N.d, mont->words, mont->n0);
	return 0;
}

/* The ModExp of the fixed-size methods, through their word routines */
int sslFixedBigNumMethodModExp(SslBigNumModulus *m, SslBigNum *r,
			       const SslBigNum *a, const SslBigNum *p,
			       int flags, SslBigNumContext *ctx)
{
	SslBigNumModExpState *s = NULL;
	const SslBigNumWordMethods *wm;
	void (*multiply)(SslBigNumWord *r, const SslBigNumWord *a,
			 const SslBigNumWord *b);
	void (*square)(SslBigNumWord *r, const SslBigNumWord *a);
	void (*reduce)(SslBigNumWord *r, SslBigNumWord *t,
		       const SslBigNumWord *n, int words, SslBigNumWord n0);
	SceUInt32 i, k;
	int status, top, tos;

	if (ctx->error != 0)
		return ctx->error;
	tos = ctx->tos;
	if (sslNewBigNumModExpState(&s, m, r, a, p, 8, ctx) != 0)
		goto out;
	wm = m->method->wordMethods;
	multiply = wm->Multiply;
	square = wm->Square;
	reduce = wm->Reduce;
	s->accumulator = s->table->d + s->padWords * 4;
	multiply(s->product, s->base->d, s->mont->RR.d);
	reduce(s->powers[0], s->product, s->mont->N.d, s->words, s->mont->n0);
	SSL_BIGNUM_PAD(s->powers[0], s);
	if (s->tableSize > 1) {
		square(s->product, s->powers[0]);
		reduce(s->square, s->product, s->mont->N.d, s->words,
		       s->mont->n0);
		SSL_BIGNUM_PAD(s->square, s);
		for (k = 1; k < s->tableSize; k++) {
			multiply(s->product, s->powers[k - 1], s->square);
			reduce(s->powers[k], s->product, s->mont->N.d,
			       s->words, s->mont->n0);
			SSL_BIGNUM_PAD(s->powers[k], s);
		}
	}
	sslCopyMemory(s->accumulator, s->powers[s->index >> 1],
		      s->padWords * 4);
	while (s->squarings != 0) {
		for (i = 0; i < s->squarings; i++) {
			square(s->product, s->accumulator);
			reduce(s->accumulator, s->product, s->mont->N.d,
			       s->words, s->mont->n0);
		}
		s->index = *s->recoding++;
		s->squarings = *s->recoding++;
		if (s->index + s->squarings == 0)
			break;
		if (s->index == 0)
			continue;
		if (s->squarings == 0 && s->index == 1) {
			multiply(s->product, s->accumulator, s->base->d);
			reduce(r->d, s->product, s->mont->N.d, s->words,
			       s->mont->n0);
			top = s->words;
			goto fix;
		}
		multiply(s->product, s->accumulator, s->powers[s->index >> 1]);
		reduce(s->accumulator, s->product, s->mont->N.d, s->words,
		       s->mont->n0);
	}
	sslFillMemory(s->accumulator + s->words, 0, s->words * 4);
	reduce(r->d, s->accumulator, s->mont->N.d, s->words, s->mont->n0);
	top = s->words;
fix:
	for (top--; top >= 0; top--)
		if (r->d[top] != 0)
			break;
	r->top = top + 1;
out:
	status = ctx->error;
	ctx->tos = tos;
	return status;
}
