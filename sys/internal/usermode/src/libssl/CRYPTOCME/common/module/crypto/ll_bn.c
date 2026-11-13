/* Filename and descriptive names are ours; BSAFE's bignum core. Each function
 * names the SSLeay 0.9.1b function it was forked from; the word routines are
 * SSLeay's own (bn_asm.c, guarded by bn_asm_sce.patch).
 */
#include "ll_bn.h"
#include "../../../../memory.h"

#define sslWExpandBigNum(bn, words, keep, ctx)				\
	((SceUInt32)(words) > (SceUInt32)(bn)->dmax &&			\
	 sslExpandBigNum((bn), (words), (keep), (ctx)) != 0)

/* bn_expand2 */
int sslExpandBigNum(SslBigNum *bn, SceUInt32 words, int keep,
		    SslBigNumContext *ctx)
{
	SslDirectMemoryContext *dmem;
	void *data;
	int status;

	status = ctx->error;
	if (status != 0)
		return status;
	dmem = bn->dmem;
	if (words <= (SceUInt32)bn->dmax)
		return 0;
	if (bn->flags & SSL_BIGNUM_FLAG_STATIC_DATA) {
		status = SSL_BIGNUM_ERROR_STATIC_DATA;
	} else if (dmem == NULL) {
		status = SSL_BIGNUM_ERROR_NO_MEMORY_CONTEXT;
	} else {
		data = bn->d;
		if (data != NULL && keep != 0) {
			status = sslReallocateDirectMemory(&data, words * 4 + 4,
							   dmem, bn->top * 4, 0);
			if (status == 0) {
				bn->d = data;
				bn->dmax = words;
				return 0;
			}
		} else {
			status = sslAllocateDirectMemory(&data, words * 4 + 4,
							 dmem, 0);
			if (status == 0) {
				sslFreeDirectMemory(bn->d, dmem);
				bn->d = data;
				bn->dmax = words;
				return 0;
			}
		}
	}
	ctx->error = status;
	return status;
}

/* BN_bin2bn */
int sslSetBigNumData(SslBigNum *bn, const unsigned char *data,
		     SceUInt32 length, SslBigNumContext *ctx)
{
	SslBigNumWord l;
	SceUInt32 words;
	int i, m, n;
	int status = 0;

	if (ctx->error != 0)
		return ctx->error;
	if (length == 0) {
		bn->top = 0;
		return 0;
	}
	n = (length - 1) >> 2;
	words = n + 1;
	l = 0;
	if (sslWExpandBigNum(bn, words, 0, ctx))
		return ctx->error;
	bn->top = words;
	m = length & 3;
	if (m != 0) {
		for (; m > 0; m--)
			l = (l << 8) | *data++;
		bn->d[n] = l;
		words = n;
	}
	for (i = words - 1; i >= 0; i--) {
		bn->d[i] = ((SslBigNumWord)data[0] << 24) |
			   ((SslBigNumWord)data[1] << 16) |
			   ((SslBigNumWord)data[2] << 8) | data[3];
		data += 4;
	}
	for (; n >= 0; n--)
		if (bn->d[n] != 0)
			break;
	bn->top = n + 1;
	return status;
}

static const char sslNibbleBitLengths[16] = {
	0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4
};

/* BN_num_bits_word */
int sslGetWordBitLength(SslBigNumWord word)
{
	int n = 0;

	if (word & 0xffff0000) {
		n = 16;
		word >>= 16;
	}
	if (word & 0xff00) {
		word >>= 8;
		n += 8;
	}
	if (word & 0xf0)
		return sslNibbleBitLengths[word >> 4] + n + 4;
	return sslNibbleBitLengths[word] + n;
}

/* BN_num_bits */
__attribute__((noinline))
int sslGetBigNumBitLength(const SslBigNum *bn)
{
	int i;

	if (bn == NULL)
		return 0;
	i = bn->top;
	if (i == 0)
		return 0;
	return sslGetWordBitLength(bn->d[i - 1]) + (i - 1) * 32;
}

/* BN_bn2bin */
int sslGetBigNumData(SceUInt32 *length, unsigned char *data, SceUInt32 max,
		     const SslBigNum *bn, SslBigNumContext *ctx)
{
	SceUInt32 n = 0;
	SceUInt32 i;
	int status = 0;

	if (ctx->error != 0) {
		status = ctx->error;
	} else {
		n = (sslGetBigNumBitLength(bn) + 7) / 8;
		if (data != NULL) {
			if (max < n) {
				status = SSL_BIGNUM_ERROR_BUFFER_TOO_SMALL;
				ctx->error = status;
			} else {
				i = n - 1;
				if (n != 0) {
					do {
						*data++ = bn->d[i / 4] >>
							  (8 * (i % 4));
					} while (i-- != 0);
				}
			}
		}
	}
	if (length != NULL)
		*length = n;
	return status;
}

#define SCE_BN_ADD_WORDS
#include "bn_asm_sce.inc"
#undef SCE_BN_ADD_WORDS

/* BN_uadd */
int sslAddBigNumUnsigned(SslBigNum *r, const SslBigNum *a, const SslBigNum *b,
			 SslBigNumContext *ctx)
{
	const SslBigNum *tmp;
	SslBigNumWord *ap, *rp, carry, t1;
	int i, max, min;

	if (ctx->error != 0)
		return ctx->error;
	if (a->top < b->top) {
		tmp = a;
		a = b;
		b = tmp;
	}
	max = a->top;
	min = b->top;
	if (sslWExpandBigNum(r, max + 1, 1, ctx))
		return ctx->error;
	r->top = max;
	ap = a->d;
	rp = r->d;
	carry = bn_add_words(rp, ap, b->d, min);
	rp += min;
	ap += min;
	i = min;
	if (carry) {
		while (i < max) {
			i++;
			t1 = *ap++;
			if ((*rp++ = t1 + 1) >= t1) {
				carry = 0;
				break;
			}
		}
		if (i >= max && carry) {
			*rp++ = 1;
			r->top++;
		}
	}
	if (rp != ap) {
		for (; i < max; i++)
			*rp++ = *ap++;
	}
	return 0;
}

#define SCE_BN_SUB_WORDS
#include "bn_asm_sce.inc"
#undef SCE_BN_SUB_WORDS

/* BN_usub */
int sslSubtractBigNumUnsigned(SslBigNum *r, const SslBigNum *a,
			      const SslBigNum *b, SslBigNumContext *ctx)
{
	SslBigNumWord *ap, *rp, t1, t2;
	int i, max, min;

	if (ctx->error != 0)
		return ctx->error;
	max = a->top;
	min = b->top;
	if (max < min) {
		ctx->error = SSL_BIGNUM_ERROR_INVALID_ARGUMENT;
		return SSL_BIGNUM_ERROR_INVALID_ARGUMENT;
	}
	if (sslWExpandBigNum(r, max, 1, ctx))
		return ctx->error;
	ap = a->d;
	rp = r->d;
	i = bn_sub_words(rp, ap, b->d, min);
	ap += min;
	rp += min;
	if (i != 0) {
		i = min;
		while (i < max) {
			i++;
			t1 = *ap++;
			t2 = t1 - 1;
			*rp++ = t2;
			if (t1 > t2)
				break;
		}
	} else {
		i = min;
	}
	if (rp != ap)
		sslCopyMemory(rp, ap, (max - i) * 4);
	for (i = max - 1; i >= 0; i--)
		if (r->d[i] != 0)
			break;
	r->top = i + 1;
	if (r->top == 0 || (r->top == 1 && r->d[0] == 0))
		r->neg = 0;
	return 0;
}

/* bn_cmp_words */
int sslCompareBigNumWords(const SslBigNumWord *a, const SslBigNumWord *b,
			  int n)
{
	int i;

	for (i = n - 1; i >= 0; i--) {
		if (a[i] != b[i])
			return a[i] > b[i] ? 1 : -1;
	}
	return 0;
}

/* BN_ucmp */
int sslCompareBigNumUnsigned(const SslBigNum *a, const SslBigNum *b,
			     SslBigNumContext *ctx)
{
	int i;

	if (ctx->error != 0)
		return 0;
	i = a->top - b->top;
	if (i != 0)
		return i;
	return sslCompareBigNumWords(a->d, b->d, a->top);
}

/* BN_add */
int sslAddBigNum(SslBigNum *r, const SslBigNum *a, const SslBigNum *b,
		 SslBigNumContext *ctx)
{
	const SslBigNum *tmp;

	if (ctx->error != 0)
		return ctx->error;
	if (a->neg ^ b->neg) {
		if (a->neg) {
			tmp = a;
			a = b;
			b = tmp;
		}
		if (sslCompareBigNumUnsigned(a, b, ctx) < 0) {
			tmp = a;
			a = b;
			b = tmp;
			r->neg = 1;
		} else {
			r->neg = 0;
		}
		return sslSubtractBigNumUnsigned(r, a, b, ctx);
	}
	r->neg = a->neg != 0;
	return sslAddBigNumUnsigned(r, a, b, ctx);
}

/* BN_sub */
int sslSubtractBigNum(SslBigNum *r, const SslBigNum *a, const SslBigNum *b,
		      SslBigNumContext *ctx)
{
	const SslBigNum *tmp;
	int max, neg;

	if (ctx->error != 0)
		return ctx->error;
	if (a->neg) {
		if (b->neg) {
			tmp = a;
			a = b;
			b = tmp;
		} else {
			neg = 1;
			goto add;
		}
	} else if (b->neg) {
		neg = 0;
		goto add;
	}
	max = b->top;
	if (a->top > max)
		max = a->top;
	if ((SceUInt32)max > (SceUInt32)r->dmax)
		sslExpandBigNum(r, max, 1, ctx);
	if (sslCompareBigNumUnsigned(a, b, ctx) < 0) {
		sslSubtractBigNumUnsigned(r, b, a, ctx);
		neg = 1;
	} else {
		sslSubtractBigNumUnsigned(r, a, b, ctx);
		neg = 0;
	}
	goto done;
add:
	sslAddBigNumUnsigned(r, a, b, ctx);
done:
	if (ctx->error != 0)
		return ctx->error;
	r->neg = neg;
	return 0;
}

#define SCE_BN_MUL_WORDS
#include "bn_asm_sce.inc"
#undef SCE_BN_MUL_WORDS

/* BN_set_bit */
int sslSetBigNumBit(SslBigNum *bn, SceUInt32 bit, SslBigNumContext *ctx)
{
	SceUInt32 i, j, k;

	if (ctx->error != 0)
		return ctx->error;
	i = bit >> 5;
	j = bit & 31;
	if ((SceUInt32)bn->top <= i) {
		if (sslWExpandBigNum(bn, i + 1, 1, ctx))
			return ctx->error;
		for (k = bn->top; k < i + 1; k++)
			bn->d[k] = 0;
		bn->top = i + 1;
	}
	bn->d[i] |= 1 << j;
	return 0;
}

/* BN_is_bit_set */
int sslIsBigNumBitSet(const SslBigNum *bn, SceUInt32 bit)
{
	SceUInt32 i = bit >> 5;

	return (SceUInt32)bn->top > i &&
	       (bn->d[i] & (1 << (bit & 31))) != 0;
}

/* BN_cmp */
int sslCompareBigNum(const SslBigNum *a, const SslBigNum *b,
		     SslBigNumContext *ctx)
{
	const SslBigNum *tmp;

	if (ctx->error != 0)
		return 0;
	if (a->neg != b->neg)
		return a->neg ? -1 : 1;
	if (a->neg == 1) {
		tmp = a;
		a = b;
		b = tmp;
	}
	return sslCompareBigNumUnsigned(a, b, ctx);
}

/* BN_clear_free */
void sslFreeBigNum(SslBigNum *bn, SceUInt32 flags)
{
	SslDirectMemoryContext *dmem;
	void *self = NULL;
	void *data = NULL;
	SceUInt32 selfSize = 0;
	SceUInt32 dataSize = 0;

	if (bn == NULL)
		return;
	dmem = bn->dmem;
	if (bn->d != NULL && !(bn->flags & SSL_BIGNUM_FLAG_STATIC_DATA)) {
		data = bn->d;
		bn->d = NULL;
		dataSize = bn->dmax * 4;
	}
	if (bn->flags & SSL_BIGNUM_FLAG_ALLOCATED) {
		bn->flags &= ~SSL_BIGNUM_FLAG_ALLOCATED;
		selfSize = sizeof(*bn);
		self = bn;
	}
	if (flags & SSL_BIGNUM_FREE_WIPE) {
		if (self != NULL)
			sslFillMemory(self, 0, selfSize);
		if (data != NULL)
			sslFillMemory(data, 0, dataSize);
	}
	sslFreeDirectMemory(self, dmem);
	sslFreeDirectMemory(data, dmem);
}

/* BN_init */
void sslInitBigNum(SslBigNum *bn, SslDirectMemoryContext *dmem)
{
	bn->dmem = dmem;
	bn->top = 0;
	bn->dmax = 0;
	bn->neg = 0;
	bn->flags = 0;
	bn->d = NULL;
}

/* BN_new */
int sslNewBigNum(SslBigNum **result, SslDirectMemoryContext *dmem)
{
	SslBigNum *bn;
	int status;

	*result = NULL;
	if (dmem == NULL)
		return SSL_BIGNUM_ERROR_NO_MEMORY_CONTEXT;
	status = sslAllocateDirectMemory((void **)&bn, sizeof(*bn), dmem,
					 SSL_DMEM_ALLOCATE_ZERO);
	if (status == 0) {
		bn->dmem = dmem;
		bn->flags = SSL_BIGNUM_FLAG_ALLOCATED;
		*result = bn;
	}
	return status;
}

/* BN_CTX_init */
void sslInitBigNumContext(SslBigNumContext *ctx, SslDirectMemoryContext *dmem)
{
	int i;

	sslFillMemory(ctx, 0, sizeof(*ctx));
	ctx->dmem = dmem;
	for (i = 0; i < SSL_BIGNUM_CONTEXT_NUMBERS; i++)
		ctx->bn[i].dmem = dmem;
}

/* BN_CTX_new */
int sslNewBigNumContext(SslBigNumContext **result,
			SslDirectMemoryContext *dmem)
{
	SslBigNumContext *ctx;
	int status;

	*result = NULL;
	if (dmem == NULL)
		return SSL_BIGNUM_ERROR_NO_MEMORY_CONTEXT;
	status = sslAllocateDirectMemory((void **)&ctx, sizeof(*ctx), dmem, 0);
	if (status == 0) {
		sslInitBigNumContext(ctx, dmem);
		*result = ctx;
		ctx->flags = SSL_BIGNUM_CONTEXT_ALLOCATED;
	}
	return status;
}

/* BN_CTX_free */
void sslFreeBigNumContext(SslBigNumContext *ctx, SceUInt32 flags)
{
	int i;

	if (ctx == NULL)
		return;
	for (i = 0; i < SSL_BIGNUM_CONTEXT_NUMBERS; i++)
		sslFreeBigNum(&ctx->bn[i], flags);
	sslFillMemory(ctx->bn, 0, sizeof(ctx->bn));
	if (ctx->flags & SSL_BIGNUM_CONTEXT_ALLOCATED)
		sslFreeDirectMemory(ctx, ctx->dmem);
}

/* sslGetBigNumMemoryContext */
SslDirectMemoryContext *sslGetBigNumMemoryContext(const SslBigNum *bn)
{
	return bn->dmem;
}

/* BN_copy */
int sslCopyBigNum(SslBigNum *a, const SslBigNum *b, SslBigNumContext *ctx)
{
	SslBigNumWord *A;
	const SslBigNumWord *B;
	int i, top;

	if (a == b)
		return 0;
	if (ctx->error != 0)
		return ctx->error;
	if (sslWExpandBigNum(a, b->top, 0, ctx))
		return ctx->error;
	top = b->top;
	A = a->d;
	B = b->d;
	for (i = 0; i < top - 3; i += 4) {
		A[i] = B[i];
		A[i + 1] = B[i + 1];
		A[i + 2] = B[i + 2];
		A[i + 3] = B[i + 3];
	}
	for (; i < top; i++)
		A[i] = B[i];
	a->top = top;
	if (top == 0 && A != NULL)
		A[0] = 0;
	a->neg = b->neg;
	return 0;
}

/* BN_lshift */
int sslLeftShiftBigNum(SslBigNum *r, const SslBigNum *a, SceUInt32 n,
		       SslBigNumContext *ctx)
{
	SslBigNumWord *t, l;
	const SslBigNumWord *f;
	int i, nw, lb, rb, top;

	if (ctx->error != 0)
		return ctx->error;
	nw = n >> 5;
	lb = n & 31;
	if (sslWExpandBigNum(r, a->top + nw + 1, 1, ctx))
		return ctx->error;
	r->neg = a->neg;
	top = a->top;
	f = a->d;
	t = r->d;
	t[top + nw] = 0;
	if (lb == 0) {
		for (i = top - 1; i >= 0; i--)
			t[nw + i] = f[i];
	} else {
		rb = 32 - lb;
		for (i = top - 1; i >= 0; i--) {
			l = f[i];
			t[nw + i + 1] |= l >> rb;
			t[nw + i] = l << lb;
		}
	}
	sslFillMemory(t, 0, nw * 4);
	for (i = nw + a->top; i >= 0; i--)
		if (r->d[i] != 0)
			break;
	r->top = i + 1;
	return 0;
}

/* BN_div's locals, which BSAFE keeps in the words of a context number */
typedef struct SslBigNumDivState {
	SslBigNumWord reserved0;
	SslBigNum wnum;
	SslBigNum *res;
	SslBigNumWord *resp;
	SslBigNumWord *wnump;
	SslBigNumWord d0;
	SslBigNumWord d1;
	SslBigNumWord q;
	SslBigNumWord n0;
	SslBigNumWord n1;
	SslBigNumWord l0;
	int normShift;
	int i;
	int j;
	int loop;
	int numN;
	int divN;
	SslBigNumWord t1l;
	SslBigNumWord t1h;
	SslBigNumWord t2l;
	SslBigNumWord t2h;
	SslBigNumWord t3l;
	SslBigNumWord t3h;
	SslBigNumWord t3t;
	SslBigNumWord reserved116[2];
} SslBigNumDivState;

_Static_assert(sizeof(SslBigNumDivState) == 124, "SslBigNumDivState");

/* BN_div */
int sslDivideBigNum(SslBigNum *dv, SslBigNum *rm, const SslBigNum *num,
		    const SslBigNum *divisor, SslBigNumContext *ctx)
{
	SslBigNumDivState *s;
	SslBigNum *bns, *state, *tmp, *snum, *sdiv, *res;
	SslBigNumWord ql, qh, d1;
	int i, numN, divN;

	if (ctx->error != 0)
		return ctx->error;
	if (divisor->top == 0 || (divisor->top == 1 && divisor->d[0] == 0)) {
		ctx->error = SSL_BIGNUM_ERROR_DIVIDE_BY_ZERO;
		return SSL_BIGNUM_ERROR_DIVIDE_BY_ZERO;
	}
	if (sslCompareBigNumUnsigned(num, divisor, ctx) < 0) {
		if (rm != NULL)
			sslCopyBigNum(rm, num, ctx);
		if (dv != NULL)
			sslSetBigNumWord(dv, 0, ctx);
		return ctx->error;
	}
	state = &ctx->bn[ctx->tos + 3];
	state->top = 0;
	state->neg = 0;
	if (sslWExpandBigNum(state, sizeof(*s), 0, ctx))
		return ctx->error;
	s = (SslBigNumDivState *)state->d;
	bns = &ctx->bn[ctx->tos];
	tmp = &bns[0];
	snum = &bns[1];
	sdiv = &bns[2];
	s->res = dv != NULL ? dv : &bns[4];
	tmp->neg = 0;

	/* First we normalise the numbers */
	s->normShift = 32 - sslGetBigNumBitLength(divisor) % 32;
	sslLeftShiftBigNum(sdiv, divisor, s->normShift, ctx);
	sdiv->neg = 0;
	s->normShift += 32;
	sslLeftShiftBigNum(snum, num, s->normShift, ctx);
	if (ctx->error != 0)
		return ctx->error;
	snum->neg = 0;
	numN = snum->top;
	divN = sdiv->top;
	s->numN = numN;
	s->divN = divN;
	s->loop = numN - divN;

	/* A window into snum: the part being divided */
	sslInitBigNum(&s->wnum, NULL);
	s->wnum.d = &snum->d[numN - divN];
	s->wnum.top = divN;
	s->wnum.dmax = snum->dmax + 1;

	/* The top two words of sdiv */
	s->d0 = sdiv->d[divN - 1];
	s->d1 = divN == 1 ? 0 : sdiv->d[divN - 2];
	s->wnump = &snum->d[numN - 1];

	tmp->top = 0;
	tmp->neg = 0;
	if (sslWExpandBigNum(tmp, divN + 1, 0, ctx))
		return ctx->error;
	if (sslWExpandBigNum(s->res, s->loop + 1, 0, ctx))
		return ctx->error;
	res = s->res;
	res->neg = num->neg ^ divisor->neg;
	res->top = s->loop;
	s->resp = &res->d[s->loop - 1];
	if (sslCompareBigNumUnsigned(&s->wnum, sdiv, ctx) >= 0) {
		sslSubtractBigNumUnsigned(&s->wnum, &s->wnum, sdiv, ctx);
		*s->resp = 1;
		s->res->d[s->res->top - 1] = 1;
	} else {
		res->top--;
	}
	s->resp--;

	for (s->i = 0; s->i < s->loop - 1; s->i++) {
		s->wnum.d--;
		s->wnum.top++;
		s->n0 = s->wnump[0];
		s->n1 = s->wnump[-1];
		if (s->n0 == s->d0)
			s->q = BN_MASK2;
		else
			s->q = bn_div_words(s->n0, s->n1, s->d0);
		s->t1h = s->n0;
		s->t1l = s->n1;
		ql = LBITS(s->q);
		qh = HBITS(s->q);
		s->t2l = LBITS(s->d1);
		s->t2h = HBITS(s->d1);
		mul64(s->t2l, s->t2h, ql, qh);
		s->t3t = LBITS(s->d0);
		s->t3h = HBITS(s->d0);
		mul64(s->t3t, s->t3h, ql, qh);
		for (;;) {
			s->t3l = s->t1l - s->t3t;
			if (s->t3l > s->t1l)
				s->t3h++;
			s->t3h = s->t1h - s->t3h;
			if (s->t3h != 0)
				break;
			if (s->t2h < s->t3l)
				break;
			if (s->t2h == s->t3l && s->t2l <= s->wnump[-2])
				break;
			s->q--;
			d1 = s->d1;
			if (s->t2l < d1)
				s->t2h--;
			s->t2l -= d1;
			if (s->t3t < s->d0)
				s->t3h--;
			s->t3t -= d1;
		}
		s->l0 = bn_mul_words(tmp->d, sdiv->d, s->divN, s->q);
		tmp->d[s->divN] = s->l0;
		for (s->j = s->divN + 1; s->j > 0; s->j--)
			if (tmp->d[s->j - 1] != 0)
				break;
		tmp->top = s->j;

		s->j = s->wnum.top;
		sslSubtractBigNum(&s->wnum, &s->wnum, tmp, ctx);
		snum->top = snum->top + s->wnum.top - s->j;
		if (s->wnum.neg) {
			s->q--;
			s->j = s->wnum.top;
			sslAddBigNum(&s->wnum, &s->wnum, sdiv, ctx);
			snum->top += s->wnum.top - s->j;
		}
		*(s->resp--) = s->q;
		s->wnump--;
	}
	if (ctx->error == 0) {
		for (i = snum->top - 1; i >= 0; i--)
			if (snum->d[i] != 0)
				break;
		snum->top = i + 1;
		if (rm != NULL) {
			s->i = num->neg;
			sslRightShiftBigNum(rm, snum, s->normShift, ctx);
			rm->neg = rm->top == 0 || (rm->top == 1 && rm->d[0] == 0) ?
				  0 : s->i;
		}
	}
	return ctx->error;
}

/* BN_mod */
int sslModBigNum(SslBigNum *rem, const SslBigNum *m, const SslBigNum *d,
		 SslBigNumContext *ctx)
{
	return sslDivideBigNum(NULL, rem, m, d, ctx);
}

/* Recodings of the common public exponents 3, 17 and 65537 */
static const unsigned char sslExponentRecoding3[12] = {
	0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00
};
static const unsigned char sslExponentRecoding17[12] = {
	0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x01, 0x04, 0x01, 0x00, 0x00, 0x00
};
static const unsigned char sslExponentRecoding65537[12] = {
	0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00
};

/* Trailing zero bits of a 6-bit window (6 for zero); a w-bit window reads
 * the last 2^w entries, from sslWindowZeroOffsets[w]
 */
static const unsigned char sslWindowTrailingZeros[64] = {
	6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};
static const unsigned char sslWindowZeroOffsets[8] = {
	0x3f, 0x3e, 0x3c, 0x38, 0x30, 0x20, 0x00, 0x00
};

/* Recode an exponent for a sliding-window exponentiation: from the top,
 * pairs of (odd window value, squarings before it)
 */
int sslRecodeBigNumExponent(const SslBigNum *e, unsigned char **result,
			    SslDirectMemoryContext *dmem, int unused,
			    SslBigNumContext *ctx)
{
	const unsigned char *preset = NULL;
	const unsigned char *zeros;
	const SslBigNumWord *p;
	unsigned char *buf, *header, *out, *pos, *entry;
	SslBigNumWord lo, hi, x, win, mask;
	SceUInt32 squarings, z, shift;
	int status = 0;
	int zero = 0;
	int top, bits, window, tableSize, perWord, size, w, n, count;
	int length;

	if (ctx->error != 0)
		return ctx->error;
	top = e->top;
	bits = top * 32;
	if (top == 0)
		return 0;
	if (top == 1) {
		if (top == 1 && e->d[0] == 65537) {
			preset = sslExponentRecoding65537;
		} else if (top == 1) {
			x = e->d[0];
			if (x == 17)
				preset = sslExponentRecoding17;
			if (x == 3)
				preset = sslExponentRecoding3;
		}
		window = 1;
		tableSize = 1;
		perWord = 32;
	} else if (bits >= 256) {
		window = 5;
		tableSize = 16;
		perWord = 7;
	} else if (bits >= 128) {
		window = 4;
		tableSize = 8;
		perWord = 8;
	} else {
		window = 3;
		tableSize = 4;
		perWord = 11;
	}
	size = 12;
	if (preset == NULL)
		size = top * perWord * 2 + 7;
	status = sslAllocateDirectMemory((void **)&buf, size + 6, dmem, 0);
	if (status != 0) {
		ctx->error = status;
		return status;
	}
	if (preset != NULL) {
		sslCopyMemory(buf, preset, sizeof(sslExponentRecoding3));
		goto done;
	}
	header = buf;
	out = buf + SSL_BIGNUM_RECODING_HEADER;
	w = window;
	if (w > 6)
		w = 6;
	zeros = sslWindowTrailingZeros + sslWindowZeroOffsets[w];
	mask = (1 << w) - 1;
	pos = out + 2 * ((e->top * 32 + w - 1) / w);
	pos[2] = zero;
	pos[1] = zero;
	hi = zero;
	squarings = zero;
	p = e->d;
	n = e->top;
	lo = *p++;
	win = lo & mask;
	if (n > 1)
		hi = *p++;
	count = 1;
	shift = zero;
	for (;;) {
		while ((z = zeros[win]) != 0) {
			squarings += z;
			if (squarings > 255) {
				entry = pos;
				squarings -= 255;
				*pos = 255;
				pos -= 2;
				count++;
				entry[-1] = zero;
			}
			shift += z;
			if (shift >= 32) {
				if (n <= 1)
					break;
				n--;
				lo = hi;
				hi = zero;
				if (n > 1)
					hi = *p++;
				shift -= 32;
			}
			x = lo;
			if (shift != 0)
				x = (hi << (32 - shift)) | (lo >> shift);
			win = mask & x;
		}
		if (win == 0)
			break;
		entry = pos;
		*pos = squarings;
		pos -= 2;
		count++;
		entry[-1] = win;
		squarings = w;
		shift += w;
		if (shift >= 32) {
			if (n <= 1)
				break;
			n--;
			lo = hi;
			hi = zero;
			if (n > 1)
				hi = *p++;
			shift -= 32;
		}
		x = lo;
		if (shift != 0)
			x = (hi << (32 - shift)) | (lo >> shift);
		win = mask & x;
	}
	count--;
	length = 2;
	pos++;
	while (count != 0) {
		count--;
		out[0] = pos[0];
		out[1] = pos[1];
		out += 2;
		pos += 2;
		length += 2;
	}
	length += SSL_BIGNUM_RECODING_HEADER;
	out[0] = zero;
	out[1] = zero;
	header[1] = length;
	header[0] = length >> 8;
	header[2] = window;
	header[3] = tableSize;
	header[4] = zero;
	header[5] = zero;
done:
	*result = buf;
	return status;
}

/* Montgomery reduction of the 2n words at t into the n words at r; the
 * result is selected without a branch on the final subtraction
 */
void sslMontgomeryReduceWords(SslBigNumWord *r, SslBigNumWord *t,
			      const SslBigNumWord *n, int words,
			      SslBigNumWord n0)
{
	SslBigNumWord *select[4];
	SslBigNumWord *tp, *src, c, v, old, carry;
	SslBigNumWord zero = 0;
	int i, k;

	carry = zero;
	tp = t + words;
	for (i = 0; i < words; i++) {
		old = carry;
		c = bn_mul_add_words(t, (SslBigNumWord *)n, words, t[0] * n0);
		c += old;
		carry = zero;
		if (c < old)
			carry = 1;
		v = *tp + c;
		*tp = v;
		if (v < c)
			carry++;
		t++;
		tp++;
	}
	k = bn_sub_words(r, t, (SslBigNumWord *)n, words) + carry * 2;
	select[0] = r;
	select[1] = t;
	select[2] = r;
	select[3] = r;
	src = select[k];
	for (i = words; i > 0; i--)
		*r++ = *src++;
}

/* BN_mod_exp_mont, over a recoded exponent */
int sslModExpBigNumMontgomery(SslBigNum *r, const SslBigNum *a,
			      const SslBigNum *p, SslBigNumMontgomery *mont,
			      SslBigNumContext *ctx)
{
	SslBigNumWord *powers[16];
	unsigned char *recoding = NULL;
	const unsigned char *rec;
	SslBigNumWord *square, *prod, *acc, *nd, n0;
	const SslBigNum *aa;
	SslBigNum *bns, *t;
	SceUInt32 window, tableSize, windows, words, bytes, powerWords, wide;
	SceUInt32 tableWords;
	int tos, i, k;
	SceUInt32 index, squarings;
	int status;

	status = ctx->error;
	if (status != 0)
		goto out;
	if (!(mont->N.d[0] & 1)) {
		status = SSL_BIGNUM_ERROR_INVALID_ARGUMENT;
		ctx->error = status;
		goto out;
	}
	tos = ctx->tos;
	if (sslIsBigNumZero(a)) {
		sslSetBigNumWord(r, 0, ctx);
		status = ctx->error;
		goto out;
	}
	if (sslIsBigNumZero(p)) {
		sslSetBigNumWord(r, 1, ctx);
		status = ctx->error;
		goto out;
	}
	if (sslIsBigNumOne(p)) {
		sslCopyBigNum(r, a, ctx);
		status = ctx->error;
		goto out;
	}
	if (sslRecodeBigNumExponent(p, &recoding, ctx->dmem, 0, ctx) != 0) {
		status = ctx->error;
		goto out;
	}
	ctx->tos++;
	window = recoding[2];
	tableSize = recoding[3];
	windows = (window + 63) / window;
	words = mont->N.top;
	rec = recoding + SSL_BIGNUM_RECODING_HEADER;
	if (a->top == words && a->d[words - 1] < mont->N.d[words - 1]) {
		aa = a;
		bytes = words * 4;
		powerWords = tableSize * words;
		wide = words * 2;
	} else if (a->top >= words) {
		t = &ctx->bn[ctx->tos++];
		sslDivideBigNum(NULL, t, a, &mont->N, ctx);
		bytes = words * 4;
		powerWords = tableSize * words;
		wide = words * 2;
		aa = t;
		if (!sslWExpandBigNum(t, words, 1, ctx)) {
			for (i = t->top; i < words; i++)
				t->d[i] = 0;
		}
	} else {
		t = &ctx->bn[ctx->tos++];
		t->top = a->top;
		aa = t;
		if (sslWExpandBigNum(t, words, 1, ctx))
			goto free;
		bytes = words * 4;
		powerWords = tableSize * words;
		wide = words * 2;
		for (i = t->top; i < words; i++)
			t->d[i] = 0;
		for (i = 0; i < (int)a->top; i++)
			t->d[i] = a->d[i];
	}

	bns = &ctx->bn[ctx->tos];
	tableWords = powerWords + ((2 * p->top * windows + 7) >> 2);
	ctx->tos += 4;
	if ((SceUInt32)words > r->dmax)
		sslExpandBigNum(r, words, 1, ctx);
	bns[1].top = 0;
	bns[1].neg = 0;
	if ((SceUInt32)bytes > bns[1].dmax)
		sslExpandBigNum(&bns[1], bytes, 0, ctx);
	bns[2].top = 0;
	bns[2].neg = 0;
	if ((SceUInt32)wide > bns[2].dmax)
		sslExpandBigNum(&bns[2], wide, 0, ctx);
	bns[0].top = 0;
	bns[0].neg = 0;
	if ((SceUInt32)tableWords > bns[0].dmax)
		sslExpandBigNum(&bns[0], tableWords, 0, ctx);
	bns[3].top = 0;
	bns[3].neg = 0;
	if ((SceUInt32)wide > bns[3].dmax)
		sslExpandBigNum(&bns[3], wide, 0, ctx);
	if (ctx->error != 0)
		goto free;

	square = bns[1].d;
	prod = bns[3].d;
	acc = bns[2].d;
	nd = mont->N.d;
	n0 = mont->n0;
	powers[0] = bns[0].d;

	/* The odd powers a, a^3, a^5, ... in Montgomery form */
	sslMultiplyBigNumWords(prod, aa->d, words, mont->RR.d, words);
	sslMontgomeryReduceWords(powers[0], prod, nd, words, n0);
	if (tableSize > 1) {
		sslSquareBigNumWords(prod, powers[0], words, square);
		sslMontgomeryReduceWords(square, prod, nd, words, n0);
		for (k = 1; k < tableSize; k++) {
			powers[k] = powers[k - 1] + words;
			sslMultiplyBigNumWords(prod, powers[k - 1], words,
					       square, words);
			sslMontgomeryReduceWords(powers[k], prod, nd, words,
						 n0);
		}
	}

	index = *rec++;
	squarings = *rec++;
	sslCopyMemory(acc, powers[index >> 1], bytes);
	while (squarings != 0) {
		do {
			sslSquareBigNumWords(prod, acc, words, square);
			sslMontgomeryReduceWords(acc, prod, nd, words, n0);
		} while (--squarings != 0);
		index = *rec++;
		squarings = *rec++;
		if (index + squarings == 0)
			break;
		if (index != 0) {
			if (squarings == 0 && index == 1) {
				sslMultiplyBigNumWords(prod, acc, words, aa->d,
						       words);
				sslMontgomeryReduceWords(r->d, prod, nd, words,
							 n0);
				goto fix;
			}
			sslMultiplyBigNumWords(prod, acc, words,
					       powers[index >> 1], words);
			sslMontgomeryReduceWords(acc, prod, nd, words, n0);
			if (ctx->flags & SSL_BIGNUM_CONTEXT_CANCEL) {
				ctx->error = SSL_BIGNUM_ERROR_CANCELLED;
				goto free;
			}
		}
	}
	for (i = words; i < 2 * words; i++)
		acc[i] = 0;
	sslMontgomeryReduceWords(r->d, acc, nd, words, n0);
fix:
	for (i = words - 1; i >= 0; i--)
		if (r->d[i] != 0)
			break;
	r->top = i + 1;
free:
	sslFreeDirectMemory(recoding, ctx->dmem);
	status = ctx->error;
	ctx->tos = tos;
out:
	return status;
}

/* BN_mod_exp */
int sslModExpBigNum(SslBigNum *r, const SslBigNum *a, const SslBigNum *p,
		    const SslBigNum *m, SslBigNumContext *ctx)
{
	SslBigNumMontgomery mont;
	SslBigNumReciprocal recp;
	SslDirectMemoryContext *dmem = ctx->dmem;

	if (m->top != 0 && (m->d[0] & 1)) {
		sslInitBigNumMontgomery(&mont, dmem);
		sslSetBigNumMontgomery(&mont, m, ctx);
		sslModExpBigNumMontgomery(r, a, p, &mont, ctx);
		sslFreeBigNumMontgomery(&mont);
	} else {
		sslInitBigNumReciprocal(&recp, dmem);
		sslSetBigNumReciprocal(&recp, m, ctx);
		sslModExpBigNumReciprocal(r, a, p, &recp, ctx);
		sslFreeBigNumReciprocal(&recp, 0);
	}
	return ctx->error;
}

/* BN_mod_inverse */
int sslModInverseBigNum(SslBigNum *r, const SslBigNum *a, const SslBigNum *n,
			SslBigNumContext *ctx)
{
	SslBigNum *bns, *A, *B, *X, *Y, *M, *D, *T;
	int sign, status;

	status = ctx->error;
	if (status != 0)
		return status;
	bns = &ctx->bn[ctx->tos];
	ctx->tos += 6;
	A = &bns[0];
	B = &bns[1];
	X = &bns[2];
	Y = &bns[3];
	M = &bns[4];
	D = &bns[5];
	sslSetBigNumWord(X, 0, ctx);
	sslSetBigNumWord(Y, 1, ctx);
	sslCopyBigNum(A, a, ctx);
	sslCopyBigNum(B, n, ctx);
	sign = 1;
	while (!sslIsBigNumZero(B) && ctx->error == 0) {
		sslDivideBigNum(D, M, A, B, ctx);
		T = A;
		A = B;
		B = M;
		sslMultiplyBigNum(T, D, X, ctx);
		sslAddBigNum(T, T, Y, ctx);
		M = Y;
		Y = X;
		X = T;
		sign = -sign;
	}
	if (sign < 0)
		sslSubtractBigNum(Y, n, Y, ctx);
	if (sslIsBigNumOne(A)) {
		sslDivideBigNum(NULL, r, Y, n, ctx);
		status = ctx->error;
	} else {
		status = SSL_BIGNUM_ERROR_NO_INVERSE;
		ctx->error = status;
	}
	ctx->tos -= 6;
	return status;
}

/* -n^-1 mod 2^32, bit by bit */
SslBigNumWord sslGetMontgomeryN0(SslBigNumWord n)
{
	SslBigNumWord x = 1;
	SslBigNumWord mask = 3;
	SslBigNumWord bit = 2;
	SslBigNumWord prev;
	SceUInt32 i;

	if ((n & 3) > 2)
		x = 3;
	for (i = 3; i <= 32; i++) {
		prev = bit;
		mask |= bit << 1;
		bit <<= 1;
		if (((x * n) & mask) > bit)
			x += prev << 1;
	}
	return ~x + 1;
}

int sslSetBigNumContextMethods(SslBigNumContext *ctx,
			       SslBigNumMethodContext *methods)
{
	if (ctx->error != 0)
		return ctx->error;
	ctx->methods = methods;
	if (methods != NULL) {
		ctx->surrender = methods->surrender;
		ctx->prng = methods->prng;
	}
	return 0;
}

int sslSetBigNumMethodSelector(SslBigNumMethodContext *context,
			       SslBigNumMethodSelector selector)
{
	context->selector = selector;
	return 0;
}

int sslNewBigNumMethodContext(SslBigNumMethodContext **result,
			      SslDirectMemoryContext *dmem)
{
	SslBigNumMethodContext *context;
	int status;

	*result = NULL;
	if (dmem == NULL)
		return SSL_BIGNUM_ERROR_NO_MEMORY_CONTEXT;
	status = sslAllocateDirectMemory((void **)&context, sizeof(*context),
					 dmem, SSL_DMEM_ALLOCATE_ZERO);
	if (status == 0) {
		context->dmem = dmem;
		context->flags = SSL_BIGNUM_METHOD_CONTEXT_ALLOCATED;
		*result = context;
	}
	return status;
}

int sslFreeBigNumMethodContext(SslBigNumMethodContext *context)
{
	if (context != NULL &&
	    (context->flags & SSL_BIGNUM_METHOD_CONTEXT_ALLOCATED))
		sslFreeDirectMemory(context, context->dmem);
	return 0;
}

int sslWordBigNumMethodIsAvailable(const SslBigNumMethod *method)
{
	return 0;
}

void sslInitBigNumModulus(SslBigNumModulus *m, SslDirectMemoryContext *dmem)
{
	m->state = NULL;
	m->reserved12[0] = 0;
	m->reserved12[1] = 0;
	m->reserved12[2] = 0;
	m->reserved12[3] = 0;
	m->words = 0;
	m->flags = 0;
	m->dmem = dmem;
	m->method = NULL;
}

int sslNewBigNumModulus(SslBigNumModulus **result,
			SslDirectMemoryContext *dmem)
{
	SslBigNumModulus *m;
	int status;

	*result = NULL;
	if (dmem == NULL)
		return SSL_BIGNUM_ERROR_NO_MEMORY_CONTEXT;
	status = sslAllocateDirectMemory((void **)&m, sizeof(*m), dmem, 0);
	if (status == 0) {
		sslFillMemory(m, 0, sizeof(*m));
		m->dmem = dmem;
		*result = m;
		m->flags = SSL_BIGNUM_METHOD_CONTEXT_ALLOCATED;
	}
	return status;
}

void sslFreeBigNumModulus(SslBigNumModulus *m)
{
	if (m == NULL)
		return;
	if (m->method != NULL)
		m->method->Cleanup(m);
	if (m->flags & SSL_BIGNUM_METHOD_CONTEXT_ALLOCATED)
		sslFreeDirectMemory(m, m->dmem);
}

int sslSetBigNumModulusInfo(SslBigNumModulus *m, int type, const SslBigNum *bn,
			    const SslBigNumMethod *method, int flags,
			    SslBigNumContext *ctx)
{
	const SslBigNumMethod *current;
	SslBigNumMethodContext *methods;
	SslBigNumMethodSelector select;
	int status;

	status = ctx->error;
	if (status != 0)
		return status;
	current = m->method;
	if (type == SSL_BIGNUM_MODULUS_INFO_MODULUS) {
		methods = ctx->methods;
		if (methods == NULL || (select = methods->selector) == NULL)
			select = sslSelectDefaultBigNumMethod;
		if (method == NULL) {
			if (bn != NULL) {
				if (current == NULL ||
				    !(flags & SSL_BIGNUM_MODULUS_KEEP_METHOD)) {
					status = select(methods,
							(const void **)&method,
							sslGetBigNumBitLength(bn),
							SSL_BIGNUM_SIZE_IN_BITS,
							ctx);
					if (status != 0) {
						ctx->error = status;
						return status;
					}
				}
				if (method == NULL)
					method = current;
			}
			if (method == NULL) {
				ctx->error = SSL_BIGNUM_ERROR_NO_METHOD;
				return SSL_BIGNUM_ERROR_NO_METHOD;
			}
		}
		if (current != method) {
			if (current != NULL)
				current->Cleanup(m);
			m->method = method;
			status = method->Init(m);
			if (status != 0) {
				ctx->error = status;
				return status;
			}
		}
		m->words = bn->top;
		current = m->method;
	}
	if (current == NULL) {
		ctx->error = SSL_BIGNUM_ERROR_NO_METHOD;
		return SSL_BIGNUM_ERROR_NO_METHOD;
	}
	return current->SetInfo(m, type, bn, flags, ctx);
}

int sslModExpBigNumModulus(SslBigNumModulus *m, SslBigNum *r, SslBigNum *a,
			   const SslBigNum *p, int flags,
			   SslBigNumContext *ctx)
{
	const SslBigNumMethod *method;
	SceUInt32 top, words, i;

	if (ctx->error != 0)
		return ctx->error;
	top = a->top;
	if (top == 0 || (top == 1 && a->d[0] == 0)) {
		sslSetBigNumWord(r, 0, ctx);
		return ctx->error;
	}
	if (p != NULL) {
		if (sslIsBigNumZero(p)) {
			sslSetBigNumWord(r, 1, ctx);
			return ctx->error;
		}
		if (sslIsBigNumOne(p)) {
			sslCopyBigNum(r, a, ctx);
			return ctx->error;
		}
	}
	method = m->method;
	if (method == NULL) {
		ctx->error = SSL_BIGNUM_ERROR_NO_METHOD;
		return SSL_BIGNUM_ERROR_NO_METHOD;
	}
	words = method->words;
	if (words > a->dmax) {
		sslExpandBigNum(a, words, 1, ctx);
		top = a->top;
		method = m->method;
	}
	for (i = top; i < words; i++)
		a->d[i] = 0;
	return method->ModExp(m, r, a, p, flags, ctx);
}

/* The arithmetic implementations by operand size, smallest first */
typedef struct SslBigNumMethodEntry {
	const SslBigNumMethod *(*GetMethod)(void);
	int minimum;
	int maximum;
	SceUInt32 flags;
} SslBigNumMethodEntry;

/* SslBigNumMethodEntry.flags */
#define SSL_BIGNUM_METHOD_FALLBACK 0x1
#define SSL_BIGNUM_METHOD_IN_BITS 0x2
#define SSL_BIGNUM_METHOD_CONTINUE 0x4

static const SslBigNumMethodEntry sslBigNumMethodTable[17] = {
	{ sslGetWordBigNumMethod, 1, -1, SSL_BIGNUM_METHOD_FALLBACK },
	{ sslGetWordC4BigNumMethod, 4, 4, 0 },
	{ sslGetWordC6BigNumMethod, 6, 6, 0 },
	{ sslGetWordC8BigNumMethod, 8, 8, 0 },
	{ sslGetWordC11BigNumMethod, 11, 11, 0 },
	{ sslGetWordR612BigNumMethod, 12, 12, 0 },
	{ sslGetWordC16BigNumMethod, 16, 16, 0 },
	{ sslGetWordR1122BigNumMethod, 22, 22, 0 },
	{ sslGetWordR624BigNumMethod, 24, 24, 0 },
	{ sslGetWordR1632BigNumMethod, 32, 32, 0 },
	{ sslGetWordR1144BigNumMethod, 44, 44, 0 },
	{ sslGetWordR648BigNumMethod, 48, 48, 0 },
	{ sslGetWordR1664BigNumMethod, 64, 64, 0 },
	{ sslGetWordR1188BigNumMethod, 88, 88, 0 },
	{ sslGetWordR696BigNumMethod, 96, 96, 0 },
	{ sslGetWordR16128BigNumMethod, 128, 128, 0 },
	{ sslGetWordR16256BigNumMethod, 256, 256, 0 }
};

int sslSelectBigNumMethod(SslBigNumMethodContext *context, const void **method,
			  int size, int flags, SslBigNumContext *ctx)
{
	const SslBigNumMethodEntry *entry = sslBigNumMethodTable;
	const SslBigNumMethod *candidate;
	int fallback = -1;
	int minimum, maximum, i, bits;

	bits = size;
	if (!(flags & SSL_BIGNUM_SIZE_IN_BITS))
		bits = size * 32;
	for (i = 0; i < 17; i++, entry++) {
		minimum = entry->minimum;
		maximum = entry->maximum;
		if (!(entry->flags & SSL_BIGNUM_METHOD_IN_BITS)) {
			minimum = minimum * 32 - 31;
			maximum = maximum == -1 ? 0x7fffffff : maximum * 32;
		} else if (maximum == -1) {
			maximum = 0x7fffffff;
		}
		if (bits <= maximum && bits >= minimum) {
			candidate = entry->GetMethod();
			if (candidate->IsAvailable(candidate) == 0) {
				if (!(entry->flags & SSL_BIGNUM_METHOD_FALLBACK)) {
					*method = candidate;
					return 0;
				}
				fallback = i;
				continue;
			}
		}
		if (!(entry->flags & SSL_BIGNUM_METHOD_CONTINUE) &&
		    bits <= maximum)
			break;
	}
	if (fallback != -1) {
		*method = sslBigNumMethodTable[fallback].GetMethod();
		return 0;
	}
	return SSL_BIGNUM_ERROR_NOT_FOUND;
}

const SslBigNumMethod *sslGetWordBigNumMethod(void)
{
	return &sslWordBigNumMethod;
}

int sslSelectDefaultBigNumMethod(SslBigNumMethodContext *context,
				 const void **method, int size, int flags,
				 SslBigNumContext *ctx)
{
	*method = &sslWordBigNumMethod;
	return 0;
}

/* The word method keeps a Montgomery context as the modulus state */
int sslWordBigNumMethodInit(SslBigNumModulus *m)
{
	SslDirectMemoryContext *dmem = m->dmem;
	SslBigNumMontgomery *mont;
	int status;

	status = sslAllocateDirectMemory((void **)&mont, sizeof(*mont), dmem,
					 SSL_DMEM_ALLOCATE_ZERO);
	if (status == 0) {
		sslInitBigNumMontgomery(mont, dmem);
		m->state = mont;
		status = 0;
	}
	return status;
}

int sslWordBigNumMethodCleanup(SslBigNumModulus *m)
{
	SslBigNumMontgomery *mont = m->state;
	SslDirectMemoryContext *dmem;

	if (mont != NULL) {
		dmem = m->dmem;
		sslFreeBigNumMontgomery(mont);
		if (mont->recoding != NULL)
			sslFreeDirectMemory(mont->recoding, dmem);
		sslFreeDirectMemory(mont, dmem);
	}
	return 0;
}

int sslWordBigNumMethodSetInfo(SslBigNumModulus *m, int type,
			       const SslBigNum *bn, int flags,
			       SslBigNumContext *ctx)
{
	SslBigNumMontgomery *mont;
	SslDirectMemoryContext *dmem;
	unsigned char *recoding;
	SceUInt32 words, i;
	int status;

	status = ctx->error;
	if (status != 0)
		return status;
	mont = m->state;
	if (type == SSL_BIGNUM_MODULUS_INFO_MODULUS) {
		sslSetBigNumMontgomery(mont, bn, ctx);
		words = m->method->words;
		if (bn->top < words) {
			if (!sslWExpandBigNum(&mont->RR, words, 1, ctx)) {
				for (i = mont->RR.top; i < words; i++)
					mont->RR.d[i] = 0;
			}
			if (!sslWExpandBigNum(&mont->N, words, 1, ctx)) {
				for (i = mont->N.top; i < words; i++)
					mont->N.d[i] = 0;
			}
		}
		return ctx->error;
	}
	if (type == SSL_BIGNUM_MODULUS_INFO_EXPONENT) {
		dmem = m->dmem;
		status = sslRecodeBigNumExponent(bn, &recoding, dmem, flags,
						 ctx);
		if (status != 0)
			return status;
		if (mont->recoding != NULL)
			sslFreeDirectMemory(mont->recoding, dmem);
		mont->recoding = recoding;
		return 0;
	}
	return SSL_BIGNUM_ERROR_BAD_INFO;
}

int sslNewBigNumModExpState(SslBigNumModExpState **result,
			    SslBigNumModulus *m, SslBigNum *r,
			    const SslBigNum *a, const SslBigNum *p,
			    SceUInt32 scratch, SslBigNumContext *ctx)
{
	SslBigNumModExpState *s;
	SslBigNumMontgomery *mont;
	SslBigNum *bn, *t, *table;
	unsigned char *recoding;
	SceUInt32 words, padWords, size, i, k;
	SslBigNumWord *power;
	int tos;

	if (ctx->error != 0)
		return ctx->error;
	bn = &ctx->bn[ctx->tos];
	bn->top = 0;
	bn->neg = 0;
	if (sslWExpandBigNum(bn, sizeof(*s) / 4, 0, ctx))
		return ctx->error;
	tos = ctx->tos;
	s = (SslBigNumModExpState *)bn->d;
	ctx->tos = tos + 1;
	s->surrender = ctx->surrender;
	s->progress = 0;
	mont = m->state;
	s->mont = mont;
	words = mont->N.top;
	s->words = words;
	padWords = words;
	if (m->method->words != 0)
		padWords = m->method->words;
	s->padWords = padWords;
	if (a->top == words && a->d[words - 1] < mont->N.d[words - 1]) {
		s->base = (SslBigNum *)a;
	} else {
		t = &ctx->bn[tos + 1];
		ctx->tos = tos + 2;
		s->base = t;
		if (a->top < words)
			sslCopyBigNum(t, a, ctx);
		else
			sslDivideBigNum(NULL, t, a, &mont->N, ctx);
		if (ctx->error != 0)
			return ctx->error;
	}
	if (s->padWords > s->base->dmax)
		sslExpandBigNum(s->base, s->padWords, 1, ctx);
	for (i = s->base->top; i < s->padWords; i++)
		s->base->d[i] = 0;
	if (p != NULL) {
		if (sslRecodeBigNumExponent(p, (unsigned char **)&s->recoding,
					    m->dmem, 0, ctx) != 0)
			return ctx->error;
		if (s->mont->recoding != NULL)
			sslFreeDirectMemory(s->mont->recoding, m->dmem);
		s->mont->recoding = (unsigned char *)s->recoding;
	} else {
		recoding = s->mont->recoding;
		if (recoding != NULL)
			s->recoding = recoding;
		else
			ctx->error = SSL_BIGNUM_ERROR_NO_EXPONENT;
	}
	s->tableSize = s->recoding[3];
	s->recoding += SSL_BIGNUM_RECODING_HEADER;
	size = (s->padWords + 1) * (scratch + s->tableSize);
	table = &ctx->bn[ctx->tos++];
	s->table = table;
	table->top = 0;
	table->neg = 0;
	if (sslWExpandBigNum(table, size, 0, ctx))
		return ctx->error;
	padWords = s->padWords;
	power = s->table->d;
	s->product = power;
	s->square = power + padWords * 2;
	power += padWords * scratch;
	s->powers[0] = power;
	power += padWords;
	for (k = 1; k < s->tableSize; k++) {
		s->powers[k] = power;
		padWords = s->padWords;
		power += padWords;
	}
	s->index = *s->recoding++;
	s->squarings = *s->recoding++;
	if (padWords > r->dmax)
		sslExpandBigNum(r, padWords, 1, ctx);
	*result = s;
	return ctx->error;
}

/* r = a * b / R mod N; b NULL multiplies by RR, a NULL reduces b (2 * words
 * long) out of Montgomery form; t is the 2 * words product
 */
static __inline int sslMontgomeryModMultiplyWords(SslBigNumModulus *m,
						SslBigNumWord *r,
						const SslBigNumWord *a,
						const SslBigNumWord *b,
						SslBigNumWord *t)
{
	SslBigNumMontgomery *mont = m->state;
	SslBigNumWord *product, *pad;
	int words = mont->words;
	int i;

	if (a != NULL) {
		if (b == NULL)
			b = mont->RR.d;
		if (a == b && r != a)
			sslSquareBigNumWords(t, a, words, r);
		else
			sslMultiplyBigNumWords(t, a, words, b, words);
		product = t;
	} else {
		pad = (SslBigNumWord *)b + words;
		for (i = words; i > 0; i--)
			*pad++ = 0;
		product = (SslBigNumWord *)b;
	}
	sslMontgomeryReduceWords(r, product, mont->N.d, mont->words, mont->n0);
	return 0;
}

int sslWordBigNumMethodModMultiply(SslBigNumModulus *m, SslBigNumWord *r,
				   const SslBigNumWord *a,
				   const SslBigNumWord *b, SslBigNumWord *t)
{
	return sslMontgomeryModMultiplyWords(m, r, a, b, t);
}

int sslWordBigNumMethodModExp(SslBigNumModulus *m, SslBigNum *r,
			      const SslBigNum *a, const SslBigNum *p,
			      int flags, SslBigNumContext *ctx)
{
	SslBigNumModExpState *s = NULL;
	SslBigNumSurrender *surrender;
	SceUInt32 i, k;
	int status, top, tos;

	if (ctx->error != 0)
		return ctx->error;
	tos = ctx->tos;
	if (sslNewBigNumModExpState(&s, m, r, a, p, 6, ctx) != 0)
		goto out;
	s->accumulator = s->table->d + s->padWords * 4;
	sslMultiplyBigNumWords(s->product, s->base->d, s->words,
			       s->mont->RR.d, s->words);
	sslMontgomeryReduceWords(s->powers[0], s->product, s->mont->N.d,
				 s->words, s->mont->n0);
	if (s->tableSize > 1) {
		sslMontgomeryModMultiplyWords(m, s->square, s->powers[0],
					      s->powers[0], s->product);
		for (k = 1; k < s->tableSize; k++) {
			sslMultiplyBigNumWords(s->product, s->powers[k - 1],
					       s->words, s->square, s->words);
			sslMontgomeryReduceWords(s->powers[k], s->product,
						 s->mont->N.d, s->words,
						 s->mont->n0);
		}
	}
	sslCopyMemory(s->accumulator, s->powers[s->index >> 1], s->words * 4);
	while (s->squarings != 0) {
		for (i = 0; i < s->squarings; i++) {
			sslSquareBigNumWords(s->product, s->accumulator,
					     s->words, s->square);
			sslMontgomeryReduceWords(s->accumulator, s->product,
						 s->mont->N.d, s->words,
						 s->mont->n0);
		}
		s->index = *s->recoding++;
		s->squarings = *s->recoding++;
		if (s->index + s->squarings == 0)
			break;
		if (s->index == 0)
			continue;
		if (s->squarings == 0 && s->index == 1) {
			sslMultiplyBigNumWords(s->product, s->accumulator,
					       s->words, s->base->d, s->words);
			sslMontgomeryReduceWords(r->d, s->product,
						 s->mont->N.d, s->words,
						 s->mont->n0);
			top = s->words;
			goto fix;
		}
		sslMultiplyBigNumWords(s->product, s->accumulator, s->words,
				       s->powers[s->index >> 1], s->words);
		sslMontgomeryReduceWords(s->accumulator, s->product,
					 s->mont->N.d, s->words, s->mont->n0);
		surrender = s->surrender;
		if (surrender != NULL) {
			status = surrender->Report(surrender, 255,
						   s->progress++, 0);
			if (status != 0) {
				ctx->error = status;
				goto out;
			}
		}
	}
	for (i = s->words; i < s->words * 2; i++)
		s->accumulator[i] = 0;
	sslMontgomeryReduceWords(r->d, s->accumulator, s->mont->N.d, s->words,
				 s->mont->n0);
	top = s->words;
fix:
	for (top--; top >= 0; top--)
		if (r->d[top] != 0)
			break;
	r->top = top + 1;
out:
	if (s != NULL && s->surrender != NULL)
		s->surrender->Report(s->surrender, 255, -1, 0);
	status = ctx->error;
	ctx->tos = tos;
	return status;
}

const SslBigNumMethod sslWordBigNumMethod = {
	0,
	"Word",
	sslWordBigNumMethodIsAvailable,
	sslWordBigNumMethodModExp,
	sslWordBigNumMethodModMultiply,
	sslWordBigNumMethodInit,
	sslWordBigNumMethodCleanup,
	sslWordBigNumMethodSetInfo,
	NULL,
	NULL
};
