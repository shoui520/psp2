/* Filename and descriptive names are ours; BSAFE's bignum arithmetic:
 * multiplication, squaring, Montgomery and reciprocal exponentiation, random
 * numbers, shifts and single-word operations. Each function names the SSLeay
 * 0.9.1b function it was forked from.
 */
#include "ll_bn.h"
#include "ll_rand.h"

/* The window table of sslModExpBigNumReciprocal, kept in a context number */
#define SSL_BIGNUM_RECP_TABLE 16

/* bn_mul_normal */
__attribute__((noinline))
void sslMultiplyBigNumWords(SslBigNumWord *r, const SslBigNumWord *a, int na,
			    const SslBigNumWord *b, int nb)
{
	SslBigNumWord *rr;

	rr = &r[na];
	rr[0] = bn_mul_words(r, (SslBigNumWord *)a, na, b[0]);
	for (nb--; nb > 0; nb--) {
		r++;
		b++;
		rr++;
		rr[0] = bn_mul_add_words(r, (SslBigNumWord *)a, na, b[0]);
	}
}

/* BN_set_word */
int sslSetBigNumWord(SslBigNum *bn, SslBigNumWord word, SslBigNumContext *ctx)
{
	if (ctx->error != 0)
		return ctx->error;
	bn->neg = 0;
	if (word != 0) {
		if (bn->dmax == 0 && sslExpandBigNum(bn, 1, 1, ctx) != 0)
			return ctx->error;
		bn->d[0] = word;
		bn->top = 1;
	} else {
		bn->top = 0;
	}
	return 0;
}

/* BN_mul */
int sslMultiplyBigNum(SslBigNum *r, const SslBigNum *a, const SslBigNum *b,
		      SslBigNumContext *ctx)
{
	SslBigNum *rr;
	int na, nb, top, neg, i;

	if (ctx->error != 0)
		return ctx->error;
	na = a->top;
	nb = b->top;
	if (na == 0 || nb == 0) {
		r->neg = 0;
		r->top = 0;
		return 0;
	}
	top = na + nb;
	neg = a->neg ^ b->neg;
	if (r == a || r == b)
		rr = &ctx->bn[ctx->tos + 1];
	else
		rr = r;
	if ((SceUInt32)top > rr->dmax &&
	    sslExpandBigNum(rr, top, 1, ctx) != 0)
		return ctx->error;
	rr->top = top;
	if ((SceUInt32)na < (SceUInt32)nb)
		sslMultiplyBigNumWords(rr->d, b->d, nb, a->d, na);
	else
		sslMultiplyBigNumWords(rr->d, a->d, na, b->d, nb);
	for (i = rr->top - 1; i >= 0; i--)
		if (rr->d[i] != 0)
			break;
	rr->top = i + 1;
	if (r != rr)
		sslCopyBigNum(r, rr, ctx);
	r->neg = neg;
	return ctx->error;
}

/* bn_sqr_normal */
__attribute__((noinline))
void sslSquareBigNumWords(SslBigNumWord *r, const SslBigNumWord *a, int n,
			  SslBigNumWord *tmp)
{
	SslBigNumWord *ap, *rp;
	int j, max;

	max = n * 2;
	r[max - 1] = 0;
	r[0] = 0;
	if (n <= 1) {
		bn_sqr_words(r, (SslBigNumWord *)a, n);
		return;
	}
	ap = (SslBigNumWord *)a + 1;
	rp = r + 1;
	j = n - 1;
	rp[j] = bn_mul_words(rp, ap, j, ap[-1]);
	for (j--; j > 0; j--) {
		ap++;
		rp += 2;
		rp[j] = bn_mul_add_words(rp, ap, j, ap[-1]);
	}
	bn_add_words(r, r, r, max);
	bn_sqr_words(tmp, (SslBigNumWord *)a, n);
	bn_add_words(r, r, tmp, max);
}

/* BN_sqr */
int sslSquareBigNum(SslBigNum *r, const SslBigNum *a, SslBigNumContext *ctx)
{
	SslBigNum *rr, *tmp;
	int al, max;

	if (ctx->error != 0)
		return ctx->error;
	tmp = &ctx->bn[ctx->tos];
	rr = (a != r) ? r : &ctx->bn[ctx->tos + 1];
	al = a->top;
	if (al == 0) {
		r->top = 0;
		return 0;
	}
	max = al * 2;
	if ((SceUInt32)max > rr->dmax &&
	    sslExpandBigNum(rr, max, 1, ctx) != 0)
		return ctx->error;
	rr->top = max;
	rr->neg = 0;
	tmp->top = 0;
	tmp->neg = 0;
	if ((SceUInt32)max > tmp->dmax &&
	    sslExpandBigNum(tmp, max, 0, ctx) != 0)
		return ctx->error;
	sslSquareBigNumWords(rr->d, a->d, al, tmp->d);
	if (max != 0 && rr->d[max - 1] == 0)
		rr->top--;
	if (rr != r)
		sslCopyBigNum(r, rr, ctx);
	return ctx->error;
}

/* BN_mod_mul */
int sslModMultiplyBigNum(SslBigNum *r, const SslBigNum *a, const SslBigNum *b,
			 const SslBigNum *m, SslBigNumContext *ctx)
{
	SslBigNum *t;
	int status;

	if (ctx->error != 0)
		return ctx->error;
	t = &ctx->bn[ctx->tos++];
	if (a == b)
		sslSquareBigNum(t, a, ctx);
	else
		sslMultiplyBigNum(t, a, b, ctx);
	sslModBigNum(r, t, m, ctx);
	status = ctx->error;
	ctx->tos--;
	return status;
}

/* BN_MONT_CTX_init */
void sslInitBigNumMontgomery(SslBigNumMontgomery *mont,
			     SslDirectMemoryContext *dmem)
{
	mont->useWord = 0;
	mont->dmem = dmem;
	mont->words = 0;
	sslInitBigNum(&mont->RR, dmem);
	sslInitBigNum(&mont->N, dmem);
	sslInitBigNum(&mont->Ni, dmem);
	mont->flags = 0;
}

/* BN_MONT_CTX_free */
void sslFreeBigNumMontgomery(SslBigNumMontgomery *mont)
{
	if (mont == NULL)
		return;
	sslFreeBigNum(&mont->RR, 0);
	sslFreeBigNum(&mont->N, 0);
	sslFreeBigNum(&mont->Ni, 0);
	if (mont->flags & SSL_BIGNUM_FLAG_ALLOCATED)
		sslFreeDirectMemory(mont, mont->dmem);
}

/* BN_MONT_CTX_set */
int sslSetBigNumMontgomery(SslBigNumMontgomery *mont, const SslBigNum *mod,
			   SslBigNumContext *ctx)
{
	SceUInt32 i;

	if (ctx->error != 0)
		return ctx->error;
	if (mod->top == 0) {
		ctx->error = SSL_BIGNUM_ERROR_DIVIDE_BY_ZERO;
		return SSL_BIGNUM_ERROR_DIVIDE_BY_ZERO;
	}
	sslCopyBigNum(&mont->N, mod, ctx);
	mont->useWord = 1;
	mont->words = (sslGetBigNumBitLength(mod) + 31) / 32;
	mont->n0 = sslGetMontgomeryN0(mod->d[0]);
	sslSetBigNumWord(&mont->RR, 0, ctx);
	sslSetBigNumBit(&mont->RR, mont->words * 64, ctx);
	sslModBigNum(&mont->RR, &mont->RR, &mont->N, ctx);
	if ((SceUInt32)mont->words > mont->RR.dmax &&
	    sslExpandBigNum(&mont->RR, mont->words, 1, ctx) != 0)
		return ctx->error;
	for (i = mont->RR.top; i < (SceUInt32)mont->words; i++)
		mont->RR.d[i] = 0;
	return ctx->error;
}

/* BN_mod_exp_recp */
int sslModExpBigNumReciprocal(SslBigNum *r, const SslBigNum *a,
			      const SslBigNum *p, SslBigNumReciprocal *recp,
			      SslBigNumContext *ctx)
{
	SslBigNum *aa, *table, *val;
	int i, j, bits, wstart, wend, window, wvalue, start, ts = 0;

	if (ctx->error != 0)
		return ctx->error;
	bits = sslGetBigNumBitLength(p);
	aa = &ctx->bn[ctx->tos];
	table = &ctx->bn[ctx->tos + 1];
	ctx->tos += 2;
	if (sslIsBigNumZero(a)) {
		sslSetBigNumWord(r, 0, ctx);
		goto out;
	}
	if (sslIsBigNumZero(p)) {
		sslSetBigNumWord(r, 1, ctx);
		goto out;
	}
	if (sslIsBigNumOne(p)) {
		sslCopyBigNum(r, a, ctx);
		goto out;
	}
	table->top = 0;
	table->neg = 0;
	if (table->dmax <= SSL_BIGNUM_RECP_TABLE * sizeof(SslBigNum) /
			   sizeof(SslBigNumWord) &&
	    sslExpandBigNum(table, SSL_BIGNUM_RECP_TABLE * sizeof(SslBigNum) /
				   sizeof(SslBigNumWord) + 1, 0, ctx) != 0)
		goto out;
	val = (SslBigNum *)table->d;

	sslInitBigNum(&val[0], ctx->dmem);
	sslModBigNum(&val[0], a, &recp->N, ctx);
	sslModMultiplyBigNumReciprocal(aa, &val[0], &val[0], recp, ctx);

	/* bits <= 17: probably 3 or 0x10001, so just do singles */
	if (bits <= 17)
		window = 1;
	else if (bits >= 256)
		window = 5;
	else if (bits >= 128)
		window = 4;
	else
		window = 3;

	j = 1 << (window - 1);
	for (i = 1; i < j; i++) {
		sslInitBigNum(&val[i], ctx->dmem);
		sslModMultiplyBigNumReciprocal(&val[i], &val[i - 1], aa, recp,
					       ctx);
	}
	ts = i;

	sslSetBigNumWord(r, 1, ctx);
	start = 1;
	wstart = bits - 1;
	for (;;) {
		if (sslIsBigNumBitSet(p, wstart) == 0) {
			if (!start)
				sslModMultiplyBigNumReciprocal(r, r, r, recp,
							       ctx);
			if (wstart == 0)
				break;
			wstart--;
			continue;
		}
		/* The window ends at the last set bit within its size */
		wvalue = 1;
		wend = 0;
		for (i = 1; i < window; i++) {
			if (wstart - i < 0)
				break;
			if (sslIsBigNumBitSet(p, wstart - i) != 0) {
				wvalue <<= i - wend;
				wvalue |= 1;
				wend = i;
			}
		}
		j = wend + 1;
		if (!start)
			for (i = 0; i < j; i++)
				sslModMultiplyBigNumReciprocal(r, r, r, recp,
							       ctx);
		sslModMultiplyBigNumReciprocal(r, r, &val[wvalue >> 1], recp,
					       ctx);
		wstart -= wend + 1;
		start = 0;
		if (wstart < 0)
			break;
	}
out:
	ctx->tos -= 2;
	for (i = 0; i < ts; i++)
		sslFreeBigNum(&val[i], SSL_BIGNUM_FREE_WIPE);
	return ctx->error;
}

/* BN_mod_exp_simple, through the reciprocal */
int sslModExpBigNumReciprocalSimple(SslBigNum *r, const SslBigNum *a,
				    const SslBigNum *p,
				    SslBigNumReciprocal *recp,
				    SslBigNumContext *ctx)
{
	SslBigNum *t, *v;
	int i, bits, tos;

	if (ctx->error != 0)
		return ctx->error;
	tos = ctx->tos;
	t = &ctx->bn[ctx->tos++];
	if (r == a || r == p)
		v = &ctx->bn[ctx->tos++];
	else
		v = r;
	sslCopyBigNum(t, a, ctx);
	bits = sslGetBigNumBitLength(p);
	if (p->top != 0 && (p->d[0] & 1))
		sslCopyBigNum(v, a, ctx);
	else
		sslSetBigNumWord(v, 1, ctx);
	for (i = 1; i < bits; i++) {
		sslModMultiplyBigNumReciprocal(t, t, t, recp, ctx);
		if (sslIsBigNumBitSet(p, i) != 0)
			sslModMultiplyBigNumReciprocal(v, v, t, recp, ctx);
	}
	ctx->tos = tos;
	if (r != v)
		sslCopyBigNum(r, v, ctx);
	return ctx->error;
}

/* BN_rand: top sets the two top bits, bottom makes the number odd */
int sslRandomBigNum(SslBigNum *rnd, void *prng, SceUInt32 bits, int top,
		    int bottom, SslBigNumContext *ctx)
{
	SslBigNum *buffer;
	unsigned char *buf;
	SceUInt32 generated, bytes, bit, mask;
	int status;

	if (ctx->error != 0)
		return ctx->error;
	bit = (bits - 1) & 7;
	mask = 0xff << bit;
	bytes = (bits + 7) >> 3;
	buffer = &ctx->bn[ctx->tos];
	buffer->top = 0;
	buffer->neg = 0;
	if (bytes / 4 + 1 > buffer->dmax &&
	    sslExpandBigNum(buffer, bytes / 4 + 1, 0, ctx) != 0)
		return ctx->error;
	buf = (unsigned char *)buffer->d;
	if (prng == NULL) {
		prng = ctx->prng;
		if (prng == NULL) {
			ctx->error = SSL_BIGNUM_ERROR_NO_PRNG;
			return SSL_BIGNUM_ERROR_NO_PRNG;
		}
	}
	status = sslGenerateLowLevelPrng(prng, buf, &generated, bytes + 1);
	if (status != 0)
		return status;
	if (top) {
		if (bit == 0) {
			buf[0] = 1;
			if (bytes > 1)
				buf[1] |= 0x80;
		} else {
			buf[0] |= 3 << (bit - 1);
			buf[0] &= ~(mask << 1);
		}
	} else {
		buf[0] &= ~(mask << 1);
	}
	if (bottom)
		buf[bytes - 1] |= 1;
	return sslSetBigNumData(rnd, buf, bytes, ctx);
}

/* r = |a - b| over n words; returns whether b >= a */
int sslSubtractBigNumWordsAbsolute(SslBigNumWord *r, SslBigNumWord *a,
				   SslBigNumWord *b, int n)
{
	SslBigNumWord x, y;
	int i;

	i = n - 1;
	y = b[i];
	x = a[i];
	if (x == y) {
		i--;
		y = b[i];
		x = a[i];
		while (i > 0 && x == y) {
			i--;
			y = b[i];
			x = a[i];
		}
	}
	if (x > y) {
		bn_sub_words(r, a, b, n);
		return 0;
	}
	bn_sub_words(r, b, a, n);
	return 1;
}

/* BN_rshift */
int sslRightShiftBigNum(SslBigNum *r, const SslBigNum *a, SceUInt32 n,
			SslBigNumContext *ctx)
{
	SslBigNumWord *t, *f, l, tmp;
	SceUInt32 nw, rb, lb;
	int i, j;

	if (ctx->error != 0)
		return ctx->error;
	nw = n / 32;
	rb = n % 32;
	lb = 32 - rb;
	if (nw > a->top) {
		sslSetBigNumWord(r, 0, ctx);
		return 0;
	}
	j = a->top - nw;
	if (r != a) {
		if ((SceUInt32)(j + 2) > r->dmax &&
		    sslExpandBigNum(r, j + 2, 0, ctx) != 0)
			return ctx->error;
		j = a->top - nw;
		r->neg = a->neg;
	}
	f = &a->d[nw];
	t = r->d;
	if (rb == 0) {
		for (i = j + 1; i > 0; i--)
			*t++ = *f++;
	} else {
		l = *f++;
		for (i = 1; i < j; i++) {
			tmp = l >> rb;
			l = *f++;
			*t++ = tmp | (l << lb);
		}
		*t++ = l >> rb;
		*t = 0;
	}
	for (i = j - 1; i >= 0; i--)
		if (r->d[i] != 0)
			break;
	r->top = i + 1;
	return 0;
}

/* One 16-bit digit of (h:l) / d: divides h:(l >> 16) by d */
static void sslDivideBigNumWordStep(SslBigNumWord h, SslBigNumWord l,
				    SslBigNumWord d, SslBigNumWord *q,
				    SslBigNumWord *rem)
{
	SslBigNumWord nh, nl, qt, ph, pl, t1, t2;

	nh = h >> 16;
	nl = (h << 16) | (l >> 16);
	qt = h / (d >> 16);
	t1 = (qt >> 16) * (d & 0xffff);
	ph = (qt >> 16) * (d >> 16);
	t2 = (qt & 0xffff) * (d >> 16) + t1;
	if (t2 < t1)
		ph += 0x10000;
	pl = (qt & 0xffff) * (d & 0xffff) + (t2 << 16);
	ph += t2 >> 16;
	if (pl < (t2 << 16))
		ph++;
	while (ph > nh || (ph == nh && pl > nl)) {
		qt--;
		if (pl < d) {
			pl = -(d - pl);
			ph--;
		} else {
			pl -= d;
		}
	}
	*rem = (pl > nl) ? -(pl - nl) : nl - pl;
	*q = qt;
}

/* (h:l) / d, d normalized */
SslBigNumWord sslDivideBigNumWord(SslBigNumWord h, SslBigNumWord l,
				  SslBigNumWord d)
{
	SslBigNumWord q0, q1, r0, r1;

	if (d == h)
		return 0xffffffff;
	if (h == 0)
		return l / d;
	sslDivideBigNumWordStep(h, l, d, &q0, &r0);
	sslDivideBigNumWordStep(r0, l << 16, d, &q1, &r1);
	return (q0 << 16) + q1;
}

#define SCE_BN_DIV_WORDS
#include "bn_asm_sce.inc"
#undef SCE_BN_DIV_WORDS

/* BN_add_word and BN_sub_word: adds word when neg is the sign of bn */
int sslAddBigNumWord(SslBigNum *bn, int neg, SslBigNumWord word,
		     SslBigNumContext *ctx)
{
	SslBigNumWord *d, l;
	SceUInt32 top;
	int i;

	if (ctx->error != 0)
		return ctx->error;
	if (word == 0)
		return 0;
	if (bn->top + 1 > bn->dmax &&
	    sslExpandBigNum(bn, bn->top + 1, 1, ctx) != 0)
		return ctx->error;
	top = bn->top;
	d = bn->d;
	if ((bn->neg ^ neg) == 0) {
		d[top] = 0;
		for (i = 0;; i++) {
			l = d[i] + word;
			d[i] = l;
			if (l >= word)
				break;
			word = 1;
		}
		if ((SceUInt32)i >= top)
			bn->top = top + 1;
	} else if (top == 0) {
		bn->top = 1;
		d[0] = word;
		bn->neg = 1;
	} else if (top == 1) {
		if (d[0] > word) {
			d[0] -= word;
		} else if (d[0] == word) {
			bn->top = 0;
			bn->neg = 0;
		} else {
			d[0] = word - d[0];
			bn->neg = bn->neg == 0;
		}
	} else {
		for (i = 0;; i++) {
			if (d[i] >= word) {
				d[i] -= word;
				break;
			}
			d[i] -= word;
			word = 1;
		}
		if (d[i] == 0 && (SceUInt32)i == top - 1)
			bn->top = top - 1;
	}
	return 0;
}
