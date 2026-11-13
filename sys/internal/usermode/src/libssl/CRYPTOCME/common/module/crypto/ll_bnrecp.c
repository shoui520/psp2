/* BSAFE big number reciprocal division (SSLeay bn_recp.c) */

#include "ll_bn.h"

/* SslBigNumReciprocal.flags */
#define SSL_BIGNUM_RECIPROCAL_ALLOCATED 0x1

/* BN_RECP_CTX_init */
void sslInitBigNumReciprocal(SslBigNumReciprocal *recp,
			     SslDirectMemoryContext *dmem)
{
	recp->dmem = dmem;
	sslInitBigNum(&recp->N, dmem);
	sslInitBigNum(&recp->Nr, dmem);
	recp->numBits = 0;
	recp->shift = 0;
	recp->flags = 0;
}

/* BN_RECP_CTX_free */
void sslFreeBigNumReciprocal(SslBigNumReciprocal *recp, SceUInt32 flags)
{
	if (recp == NULL)
		return;
	sslFreeBigNum(&recp->N, flags);
	sslFreeBigNum(&recp->Nr, flags);
	if (recp->flags & SSL_BIGNUM_RECIPROCAL_ALLOCATED)
		sslFreeDirectMemory(recp, recp->dmem);
}

/* BN_RECP_CTX_set */
int sslSetBigNumReciprocal(SslBigNumReciprocal *recp, const SslBigNum *d,
			   SslBigNumContext *ctx)
{
	if (ctx->error != 0)
		return ctx->error;
	sslCopyBigNum(&recp->N, d, ctx);
	sslSetBigNumWord(&recp->Nr, 0, ctx);
	recp->numBits = sslGetBigNumBitLength(d);
	recp->shift = 0;
	return ctx->error;
}

/* BN_reciprocal: r = 2^len / m */
int sslGetBigNumReciprocal(SslBigNum *r, const SslBigNum *m, int len,
			   SslBigNumContext *ctx)
{
	SslBigNum *t;

	if (ctx->error != 0)
		return ctx->error;
	t = &ctx->bn[ctx->tos++];
	sslSetBigNumWord(t, 0, ctx);
	sslSetBigNumBit(t, len, ctx);
	sslDivideBigNum(r, NULL, t, m, ctx);
	ctx->tos--;
	return ctx->error;
}

/* BN_div_recp */
int sslDivideBigNumReciprocal(SslBigNum *dv, SslBigNum *rem,
			      const SslBigNum *m, SslBigNumReciprocal *recp,
			      SslBigNumContext *ctx)
{
	SslBigNum *a, *b, *d, *r, *t;
	int tos, i, j;

	if (ctx->error != 0)
		return ctx->error;
	tos = ctx->tos;
	a = &ctx->bn[tos];
	b = &ctx->bn[tos + 1];
	ctx->tos = tos + 2;
	d = dv;
	if (d == NULL)
		d = &ctx->bn[ctx->tos++];
	r = rem;
	if (r == NULL)
		r = &ctx->bn[ctx->tos++];
	if (sslCompareBigNumUnsigned(m, &recp->N, ctx) < 0) {
		sslSetBigNumWord(d, 0, ctx);
		sslCopyBigNum(r, m, ctx);
		ctx->tos = tos;
		return ctx->error;
	}
	i = sslGetBigNumBitLength(m);
	if (i != recp->shift) {
		if (ctx->error == 0) {
			t = &ctx->bn[ctx->tos++];
			sslSetBigNumWord(t, 0, ctx);
			sslSetBigNumBit(t, i, ctx);
			sslDivideBigNum(&recp->Nr, NULL, t, &recp->N, ctx);
			ctx->tos--;
		}
		recp->shift = i;
	}
	sslRightShiftBigNum(a, m, recp->numBits, ctx);
	sslMultiplyBigNum(b, a, &recp->Nr, ctx);
	sslRightShiftBigNum(d, b, i - recp->numBits, ctx);
	d->neg = 0;
	sslMultiplyBigNum(b, &recp->N, d, ctx);
	sslSubtractBigNumUnsigned(r, m, b, ctx);
	r->neg = 0;
	j = 0;
	while (sslCompareBigNumUnsigned(r, &recp->N, ctx) >= 0 &&
	       ctx->error == 0) {
		if (j++ > 2) {
			ctx->error = SSL_BIGNUM_ERROR_NOT_REDUCED;
			goto out;
		}
		sslSubtractBigNumUnsigned(r, r, &recp->N, ctx);
		sslAddBigNumWord(d, 0, 1, ctx);
	}
	r->neg = sslIsBigNumZero(r) ? 0 : m->neg;
	d->neg = m->neg ^ recp->N.neg;
out:
	ctx->tos = tos;
	return ctx->error;
}

/* BN_mod_mul_reciprocal */
int sslModMultiplyBigNumReciprocal(SslBigNum *r, const SslBigNum *x,
				   const SslBigNum *y,
				   SslBigNumReciprocal *recp,
				   SslBigNumContext *ctx)
{
	SslBigNum *a;

	if (ctx->error != 0)
		return ctx->error;
	a = &ctx->bn[ctx->tos++];
	if (y != NULL) {
		if (x == y)
			sslSquareBigNum(a, x, ctx);
		else
			sslMultiplyBigNum(a, x, y, ctx);
	} else {
		a = (SslBigNum *)x;
	}
	sslDivideBigNumReciprocal(NULL, r, a, recp, ctx);
	ctx->tos--;
	return ctx->error;
}
