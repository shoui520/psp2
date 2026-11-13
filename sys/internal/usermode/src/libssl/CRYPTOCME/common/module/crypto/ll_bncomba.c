/* Filename is ours: BSAFE's fixed-size multiply and square word routines
 * ("word c4" ... "word c16"). They work in 16-bit digits, one column of the
 * product at a time, written out like SSLeay's comba routines.
 */
#include "ll_bn.h"

/* A 16-bit digit of a word */
typedef unsigned short SslBigNumDigit;

#define DIGIT_MUL(x, y) ((SceUInt32)(x) * (SceUInt32)(y))
#define LO(p) ((p) & 0xffff)
#define HI(p) ((p) >> 16)

/* r[0..7] = a[0..3] * b[0..3] */
void sslMultiplyBigNumWordsC4(SslBigNumWord *r, const SslBigNumWord *a,
				const SslBigNumWord *b)
{
	SslBigNumDigit *rd = (SslBigNumDigit *)r;
	const SslBigNumDigit *ad = (const SslBigNumDigit *)a;
	const SslBigNumDigit *bd = (const SslBigNumDigit *)b;
	SceUInt32 p[8];
	SceUInt32 c, t;

	p[0] = DIGIT_MUL(ad[0], bd[0]);
	t = LO(p[0]);
	rd[0] = (SslBigNumDigit)t;
	c = HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[1]);
	p[1] = DIGIT_MUL(ad[1], bd[0]);
	t = c + LO(p[0]) + LO(p[1]);
	rd[1] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[2]);
	p[1] = DIGIT_MUL(ad[2], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[1]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]);
	rd[2] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[3]);
	p[1] = DIGIT_MUL(ad[3], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[2]);
	p[3] = DIGIT_MUL(ad[2], bd[1]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]);
	rd[3] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[4]);
	p[1] = DIGIT_MUL(ad[4], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[3]);
	p[3] = DIGIT_MUL(ad[3], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[2]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]);
	rd[4] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[5]);
	p[1] = DIGIT_MUL(ad[5], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[4]);
	p[3] = DIGIT_MUL(ad[4], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[3]);
	p[5] = DIGIT_MUL(ad[3], bd[2]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]);
	rd[5] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[6]);
	p[1] = DIGIT_MUL(ad[6], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[5]);
	p[3] = DIGIT_MUL(ad[5], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[4]);
	p[5] = DIGIT_MUL(ad[4], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[3]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]);
	rd[6] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[7]);
	p[1] = DIGIT_MUL(ad[7], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[6]);
	p[3] = DIGIT_MUL(ad[6], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[5]);
	p[5] = DIGIT_MUL(ad[5], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[4]);
	p[7] = DIGIT_MUL(ad[4], bd[3]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]);
	rd[7] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[1], bd[7]);
	p[1] = DIGIT_MUL(ad[7], bd[1]);
	p[2] = DIGIT_MUL(ad[2], bd[6]);
	p[3] = DIGIT_MUL(ad[6], bd[2]);
	p[4] = DIGIT_MUL(ad[3], bd[5]);
	p[5] = DIGIT_MUL(ad[5], bd[3]);
	p[6] = DIGIT_MUL(ad[4], bd[4]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]);
	rd[8] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[2], bd[7]);
	p[1] = DIGIT_MUL(ad[7], bd[2]);
	p[2] = DIGIT_MUL(ad[3], bd[6]);
	p[3] = DIGIT_MUL(ad[6], bd[3]);
	p[4] = DIGIT_MUL(ad[4], bd[5]);
	p[5] = DIGIT_MUL(ad[5], bd[4]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]);
	rd[9] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[3], bd[7]);
	p[1] = DIGIT_MUL(ad[7], bd[3]);
	p[2] = DIGIT_MUL(ad[4], bd[6]);
	p[3] = DIGIT_MUL(ad[6], bd[4]);
	p[4] = DIGIT_MUL(ad[5], bd[5]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]);
	rd[10] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[4], bd[7]);
	p[1] = DIGIT_MUL(ad[7], bd[4]);
	p[2] = DIGIT_MUL(ad[5], bd[6]);
	p[3] = DIGIT_MUL(ad[6], bd[5]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]);
	rd[11] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[5], bd[7]);
	p[1] = DIGIT_MUL(ad[7], bd[5]);
	p[2] = DIGIT_MUL(ad[6], bd[6]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]);
	rd[12] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[6], bd[7]);
	p[1] = DIGIT_MUL(ad[7], bd[6]);
	t = c + LO(p[0]) + LO(p[1]);
	rd[13] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[7], bd[7]);
	t = c + LO(p[0]);
	rd[14] = (SslBigNumDigit)t;
	rd[15] = (SslBigNumDigit)(HI(p[0]) + (t >> 16));
}

/* r[0..7] = a[0..3]^2 */
void sslSquareBigNumWordsC4(SslBigNumWord *r, const SslBigNumWord *a)
{
	SslBigNumDigit *rd = (SslBigNumDigit *)r;
	const SslBigNumDigit *ad = (const SslBigNumDigit *)a;
	SceUInt32 p[5];
	SceUInt32 c, t;

	p[0] = DIGIT_MUL(ad[0], ad[0]);
	t = LO(p[0]);
	rd[0] = (SslBigNumDigit)t;
	c = HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], ad[1]);
	t = c + 2 * LO(p[0]);
	rd[1] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[1], ad[1]);
	p[1] = DIGIT_MUL(ad[0], ad[2]);
	t = c + LO(p[0]) + 2 * LO(p[1]);
	rd[2] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[1], ad[2]);
	p[1] = DIGIT_MUL(ad[0], ad[3]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]);
	rd[3] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[2], ad[2]);
	p[1] = DIGIT_MUL(ad[1], ad[3]);
	p[2] = DIGIT_MUL(ad[0], ad[4]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[4] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[2], ad[3]);
	p[1] = DIGIT_MUL(ad[1], ad[4]);
	p[2] = DIGIT_MUL(ad[0], ad[5]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[5] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[3], ad[3]);
	p[1] = DIGIT_MUL(ad[2], ad[4]);
	p[2] = DIGIT_MUL(ad[1], ad[5]);
	p[3] = DIGIT_MUL(ad[0], ad[6]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[6] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[3], ad[4]);
	p[1] = DIGIT_MUL(ad[2], ad[5]);
	p[2] = DIGIT_MUL(ad[1], ad[6]);
	p[3] = DIGIT_MUL(ad[0], ad[7]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[7] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[4], ad[4]);
	p[1] = DIGIT_MUL(ad[3], ad[5]);
	p[2] = DIGIT_MUL(ad[2], ad[6]);
	p[3] = DIGIT_MUL(ad[1], ad[7]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[8] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[4], ad[5]);
	p[1] = DIGIT_MUL(ad[3], ad[6]);
	p[2] = DIGIT_MUL(ad[2], ad[7]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[9] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[5], ad[5]);
	p[1] = DIGIT_MUL(ad[4], ad[6]);
	p[2] = DIGIT_MUL(ad[3], ad[7]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[10] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[5], ad[6]);
	p[1] = DIGIT_MUL(ad[4], ad[7]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]);
	rd[11] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[6], ad[6]);
	p[1] = DIGIT_MUL(ad[5], ad[7]);
	t = c + LO(p[0]) + 2 * LO(p[1]);
	rd[12] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[6], ad[7]);
	t = c + 2 * LO(p[0]);
	rd[13] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[7], ad[7]);
	t = c + LO(p[0]);
	rd[14] = (SslBigNumDigit)t;
	rd[15] = (SslBigNumDigit)(HI(p[0]) + (t >> 16));
}

/* r[0..11] = a[0..5] * b[0..5] */
void sslMultiplyBigNumWordsC6(SslBigNumWord *r, const SslBigNumWord *a,
				const SslBigNumWord *b)
{
	SslBigNumDigit *rd = (SslBigNumDigit *)r;
	const SslBigNumDigit *ad = (const SslBigNumDigit *)a;
	const SslBigNumDigit *bd = (const SslBigNumDigit *)b;
	SceUInt32 p[12];
	SceUInt32 c, t;

	p[0] = DIGIT_MUL(ad[0], bd[0]);
	t = LO(p[0]);
	rd[0] = (SslBigNumDigit)t;
	c = HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[1]);
	p[1] = DIGIT_MUL(ad[1], bd[0]);
	t = c + LO(p[0]) + LO(p[1]);
	rd[1] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[2]);
	p[1] = DIGIT_MUL(ad[2], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[1]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]);
	rd[2] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[3]);
	p[1] = DIGIT_MUL(ad[3], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[2]);
	p[3] = DIGIT_MUL(ad[2], bd[1]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]);
	rd[3] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[4]);
	p[1] = DIGIT_MUL(ad[4], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[3]);
	p[3] = DIGIT_MUL(ad[3], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[2]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]);
	rd[4] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[5]);
	p[1] = DIGIT_MUL(ad[5], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[4]);
	p[3] = DIGIT_MUL(ad[4], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[3]);
	p[5] = DIGIT_MUL(ad[3], bd[2]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]);
	rd[5] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[6]);
	p[1] = DIGIT_MUL(ad[6], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[5]);
	p[3] = DIGIT_MUL(ad[5], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[4]);
	p[5] = DIGIT_MUL(ad[4], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[3]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]);
	rd[6] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[7]);
	p[1] = DIGIT_MUL(ad[7], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[6]);
	p[3] = DIGIT_MUL(ad[6], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[5]);
	p[5] = DIGIT_MUL(ad[5], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[4]);
	p[7] = DIGIT_MUL(ad[4], bd[3]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]);
	rd[7] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[8]);
	p[1] = DIGIT_MUL(ad[8], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[7]);
	p[3] = DIGIT_MUL(ad[7], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[6]);
	p[5] = DIGIT_MUL(ad[6], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[5]);
	p[7] = DIGIT_MUL(ad[5], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[4]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]);
	rd[8] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[9]);
	p[1] = DIGIT_MUL(ad[9], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[8]);
	p[3] = DIGIT_MUL(ad[8], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[7]);
	p[5] = DIGIT_MUL(ad[7], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[6]);
	p[7] = DIGIT_MUL(ad[6], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[5]);
	p[9] = DIGIT_MUL(ad[5], bd[4]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]);
	rd[9] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[10]);
	p[1] = DIGIT_MUL(ad[10], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[9]);
	p[3] = DIGIT_MUL(ad[9], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[8]);
	p[5] = DIGIT_MUL(ad[8], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[7]);
	p[7] = DIGIT_MUL(ad[7], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[6]);
	p[9] = DIGIT_MUL(ad[6], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[5]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]);
	rd[10] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[11]);
	p[1] = DIGIT_MUL(ad[11], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[10]);
	p[3] = DIGIT_MUL(ad[10], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[9]);
	p[5] = DIGIT_MUL(ad[9], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[8]);
	p[7] = DIGIT_MUL(ad[8], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[7]);
	p[9] = DIGIT_MUL(ad[7], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[6]);
	p[11] = DIGIT_MUL(ad[6], bd[5]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]);
	rd[11] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[1], bd[11]);
	p[1] = DIGIT_MUL(ad[11], bd[1]);
	p[2] = DIGIT_MUL(ad[2], bd[10]);
	p[3] = DIGIT_MUL(ad[10], bd[2]);
	p[4] = DIGIT_MUL(ad[3], bd[9]);
	p[5] = DIGIT_MUL(ad[9], bd[3]);
	p[6] = DIGIT_MUL(ad[4], bd[8]);
	p[7] = DIGIT_MUL(ad[8], bd[4]);
	p[8] = DIGIT_MUL(ad[5], bd[7]);
	p[9] = DIGIT_MUL(ad[7], bd[5]);
	p[10] = DIGIT_MUL(ad[6], bd[6]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]);
	rd[12] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[2], bd[11]);
	p[1] = DIGIT_MUL(ad[11], bd[2]);
	p[2] = DIGIT_MUL(ad[3], bd[10]);
	p[3] = DIGIT_MUL(ad[10], bd[3]);
	p[4] = DIGIT_MUL(ad[4], bd[9]);
	p[5] = DIGIT_MUL(ad[9], bd[4]);
	p[6] = DIGIT_MUL(ad[5], bd[8]);
	p[7] = DIGIT_MUL(ad[8], bd[5]);
	p[8] = DIGIT_MUL(ad[6], bd[7]);
	p[9] = DIGIT_MUL(ad[7], bd[6]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]);
	rd[13] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[3], bd[11]);
	p[1] = DIGIT_MUL(ad[11], bd[3]);
	p[2] = DIGIT_MUL(ad[4], bd[10]);
	p[3] = DIGIT_MUL(ad[10], bd[4]);
	p[4] = DIGIT_MUL(ad[5], bd[9]);
	p[5] = DIGIT_MUL(ad[9], bd[5]);
	p[6] = DIGIT_MUL(ad[6], bd[8]);
	p[7] = DIGIT_MUL(ad[8], bd[6]);
	p[8] = DIGIT_MUL(ad[7], bd[7]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]);
	rd[14] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[4], bd[11]);
	p[1] = DIGIT_MUL(ad[11], bd[4]);
	p[2] = DIGIT_MUL(ad[5], bd[10]);
	p[3] = DIGIT_MUL(ad[10], bd[5]);
	p[4] = DIGIT_MUL(ad[6], bd[9]);
	p[5] = DIGIT_MUL(ad[9], bd[6]);
	p[6] = DIGIT_MUL(ad[7], bd[8]);
	p[7] = DIGIT_MUL(ad[8], bd[7]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]);
	rd[15] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[5], bd[11]);
	p[1] = DIGIT_MUL(ad[11], bd[5]);
	p[2] = DIGIT_MUL(ad[6], bd[10]);
	p[3] = DIGIT_MUL(ad[10], bd[6]);
	p[4] = DIGIT_MUL(ad[7], bd[9]);
	p[5] = DIGIT_MUL(ad[9], bd[7]);
	p[6] = DIGIT_MUL(ad[8], bd[8]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]);
	rd[16] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[6], bd[11]);
	p[1] = DIGIT_MUL(ad[11], bd[6]);
	p[2] = DIGIT_MUL(ad[7], bd[10]);
	p[3] = DIGIT_MUL(ad[10], bd[7]);
	p[4] = DIGIT_MUL(ad[8], bd[9]);
	p[5] = DIGIT_MUL(ad[9], bd[8]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]);
	rd[17] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[7], bd[11]);
	p[1] = DIGIT_MUL(ad[11], bd[7]);
	p[2] = DIGIT_MUL(ad[8], bd[10]);
	p[3] = DIGIT_MUL(ad[10], bd[8]);
	p[4] = DIGIT_MUL(ad[9], bd[9]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]);
	rd[18] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[8], bd[11]);
	p[1] = DIGIT_MUL(ad[11], bd[8]);
	p[2] = DIGIT_MUL(ad[9], bd[10]);
	p[3] = DIGIT_MUL(ad[10], bd[9]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]);
	rd[19] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[9], bd[11]);
	p[1] = DIGIT_MUL(ad[11], bd[9]);
	p[2] = DIGIT_MUL(ad[10], bd[10]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]);
	rd[20] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[10], bd[11]);
	p[1] = DIGIT_MUL(ad[11], bd[10]);
	t = c + LO(p[0]) + LO(p[1]);
	rd[21] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[11], bd[11]);
	t = c + LO(p[0]);
	rd[22] = (SslBigNumDigit)t;
	rd[23] = (SslBigNumDigit)(HI(p[0]) + (t >> 16));
}

/* r[0..11] = a[0..5]^2 */
void sslSquareBigNumWordsC6(SslBigNumWord *r, const SslBigNumWord *a)
{
	SslBigNumDigit *rd = (SslBigNumDigit *)r;
	const SslBigNumDigit *ad = (const SslBigNumDigit *)a;
	SceUInt32 p[7];
	SceUInt32 c, t;

	p[0] = DIGIT_MUL(ad[0], ad[0]);
	t = LO(p[0]);
	rd[0] = (SslBigNumDigit)t;
	c = HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], ad[1]);
	t = c + 2 * LO(p[0]);
	rd[1] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[1], ad[1]);
	p[1] = DIGIT_MUL(ad[0], ad[2]);
	t = c + LO(p[0]) + 2 * LO(p[1]);
	rd[2] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[1], ad[2]);
	p[1] = DIGIT_MUL(ad[0], ad[3]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]);
	rd[3] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[2], ad[2]);
	p[1] = DIGIT_MUL(ad[1], ad[3]);
	p[2] = DIGIT_MUL(ad[0], ad[4]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[4] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[2], ad[3]);
	p[1] = DIGIT_MUL(ad[1], ad[4]);
	p[2] = DIGIT_MUL(ad[0], ad[5]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[5] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[3], ad[3]);
	p[1] = DIGIT_MUL(ad[2], ad[4]);
	p[2] = DIGIT_MUL(ad[1], ad[5]);
	p[3] = DIGIT_MUL(ad[0], ad[6]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[6] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[3], ad[4]);
	p[1] = DIGIT_MUL(ad[2], ad[5]);
	p[2] = DIGIT_MUL(ad[1], ad[6]);
	p[3] = DIGIT_MUL(ad[0], ad[7]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[7] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[4], ad[4]);
	p[1] = DIGIT_MUL(ad[3], ad[5]);
	p[2] = DIGIT_MUL(ad[2], ad[6]);
	p[3] = DIGIT_MUL(ad[1], ad[7]);
	p[4] = DIGIT_MUL(ad[0], ad[8]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]);
	rd[8] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[4], ad[5]);
	p[1] = DIGIT_MUL(ad[3], ad[6]);
	p[2] = DIGIT_MUL(ad[2], ad[7]);
	p[3] = DIGIT_MUL(ad[1], ad[8]);
	p[4] = DIGIT_MUL(ad[0], ad[9]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]);
	rd[9] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[5], ad[5]);
	p[1] = DIGIT_MUL(ad[4], ad[6]);
	p[2] = DIGIT_MUL(ad[3], ad[7]);
	p[3] = DIGIT_MUL(ad[2], ad[8]);
	p[4] = DIGIT_MUL(ad[1], ad[9]);
	p[5] = DIGIT_MUL(ad[0], ad[10]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]);
	rd[10] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[5], ad[6]);
	p[1] = DIGIT_MUL(ad[4], ad[7]);
	p[2] = DIGIT_MUL(ad[3], ad[8]);
	p[3] = DIGIT_MUL(ad[2], ad[9]);
	p[4] = DIGIT_MUL(ad[1], ad[10]);
	p[5] = DIGIT_MUL(ad[0], ad[11]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]);
	rd[11] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[6], ad[6]);
	p[1] = DIGIT_MUL(ad[5], ad[7]);
	p[2] = DIGIT_MUL(ad[4], ad[8]);
	p[3] = DIGIT_MUL(ad[3], ad[9]);
	p[4] = DIGIT_MUL(ad[2], ad[10]);
	p[5] = DIGIT_MUL(ad[1], ad[11]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]);
	rd[12] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[6], ad[7]);
	p[1] = DIGIT_MUL(ad[5], ad[8]);
	p[2] = DIGIT_MUL(ad[4], ad[9]);
	p[3] = DIGIT_MUL(ad[3], ad[10]);
	p[4] = DIGIT_MUL(ad[2], ad[11]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]);
	rd[13] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[7], ad[7]);
	p[1] = DIGIT_MUL(ad[6], ad[8]);
	p[2] = DIGIT_MUL(ad[5], ad[9]);
	p[3] = DIGIT_MUL(ad[4], ad[10]);
	p[4] = DIGIT_MUL(ad[3], ad[11]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]);
	rd[14] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[7], ad[8]);
	p[1] = DIGIT_MUL(ad[6], ad[9]);
	p[2] = DIGIT_MUL(ad[5], ad[10]);
	p[3] = DIGIT_MUL(ad[4], ad[11]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[15] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[8], ad[8]);
	p[1] = DIGIT_MUL(ad[7], ad[9]);
	p[2] = DIGIT_MUL(ad[6], ad[10]);
	p[3] = DIGIT_MUL(ad[5], ad[11]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[16] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[8], ad[9]);
	p[1] = DIGIT_MUL(ad[7], ad[10]);
	p[2] = DIGIT_MUL(ad[6], ad[11]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[17] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[9], ad[9]);
	p[1] = DIGIT_MUL(ad[8], ad[10]);
	p[2] = DIGIT_MUL(ad[7], ad[11]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[18] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[9], ad[10]);
	p[1] = DIGIT_MUL(ad[8], ad[11]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]);
	rd[19] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[10], ad[10]);
	p[1] = DIGIT_MUL(ad[9], ad[11]);
	t = c + LO(p[0]) + 2 * LO(p[1]);
	rd[20] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[10], ad[11]);
	t = c + 2 * LO(p[0]);
	rd[21] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[11], ad[11]);
	t = c + LO(p[0]);
	rd[22] = (SslBigNumDigit)t;
	rd[23] = (SslBigNumDigit)(HI(p[0]) + (t >> 16));
}

/* r[0..15] = a[0..7] * b[0..7] */
void sslMultiplyBigNumWordsC8(SslBigNumWord *r, const SslBigNumWord *a,
				const SslBigNumWord *b)
{
	SslBigNumDigit *rd = (SslBigNumDigit *)r;
	const SslBigNumDigit *ad = (const SslBigNumDigit *)a;
	const SslBigNumDigit *bd = (const SslBigNumDigit *)b;
	SceUInt32 p[16];
	SceUInt32 c, t;

	p[0] = DIGIT_MUL(ad[0], bd[0]);
	t = LO(p[0]);
	rd[0] = (SslBigNumDigit)t;
	c = HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[1]);
	p[1] = DIGIT_MUL(ad[1], bd[0]);
	t = c + LO(p[0]) + LO(p[1]);
	rd[1] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[2]);
	p[1] = DIGIT_MUL(ad[2], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[1]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]);
	rd[2] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[3]);
	p[1] = DIGIT_MUL(ad[3], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[2]);
	p[3] = DIGIT_MUL(ad[2], bd[1]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]);
	rd[3] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[4]);
	p[1] = DIGIT_MUL(ad[4], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[3]);
	p[3] = DIGIT_MUL(ad[3], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[2]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]);
	rd[4] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[5]);
	p[1] = DIGIT_MUL(ad[5], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[4]);
	p[3] = DIGIT_MUL(ad[4], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[3]);
	p[5] = DIGIT_MUL(ad[3], bd[2]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]);
	rd[5] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[6]);
	p[1] = DIGIT_MUL(ad[6], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[5]);
	p[3] = DIGIT_MUL(ad[5], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[4]);
	p[5] = DIGIT_MUL(ad[4], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[3]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]);
	rd[6] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[7]);
	p[1] = DIGIT_MUL(ad[7], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[6]);
	p[3] = DIGIT_MUL(ad[6], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[5]);
	p[5] = DIGIT_MUL(ad[5], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[4]);
	p[7] = DIGIT_MUL(ad[4], bd[3]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]);
	rd[7] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[8]);
	p[1] = DIGIT_MUL(ad[8], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[7]);
	p[3] = DIGIT_MUL(ad[7], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[6]);
	p[5] = DIGIT_MUL(ad[6], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[5]);
	p[7] = DIGIT_MUL(ad[5], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[4]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]);
	rd[8] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[9]);
	p[1] = DIGIT_MUL(ad[9], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[8]);
	p[3] = DIGIT_MUL(ad[8], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[7]);
	p[5] = DIGIT_MUL(ad[7], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[6]);
	p[7] = DIGIT_MUL(ad[6], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[5]);
	p[9] = DIGIT_MUL(ad[5], bd[4]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]);
	rd[9] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[10]);
	p[1] = DIGIT_MUL(ad[10], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[9]);
	p[3] = DIGIT_MUL(ad[9], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[8]);
	p[5] = DIGIT_MUL(ad[8], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[7]);
	p[7] = DIGIT_MUL(ad[7], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[6]);
	p[9] = DIGIT_MUL(ad[6], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[5]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]);
	rd[10] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[11]);
	p[1] = DIGIT_MUL(ad[11], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[10]);
	p[3] = DIGIT_MUL(ad[10], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[9]);
	p[5] = DIGIT_MUL(ad[9], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[8]);
	p[7] = DIGIT_MUL(ad[8], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[7]);
	p[9] = DIGIT_MUL(ad[7], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[6]);
	p[11] = DIGIT_MUL(ad[6], bd[5]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]);
	rd[11] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[12]);
	p[1] = DIGIT_MUL(ad[12], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[11]);
	p[3] = DIGIT_MUL(ad[11], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[10]);
	p[5] = DIGIT_MUL(ad[10], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[9]);
	p[7] = DIGIT_MUL(ad[9], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[8]);
	p[9] = DIGIT_MUL(ad[8], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[7]);
	p[11] = DIGIT_MUL(ad[7], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[6]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]);
	rd[12] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[13]);
	p[1] = DIGIT_MUL(ad[13], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[12]);
	p[3] = DIGIT_MUL(ad[12], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[11]);
	p[5] = DIGIT_MUL(ad[11], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[10]);
	p[7] = DIGIT_MUL(ad[10], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[9]);
	p[9] = DIGIT_MUL(ad[9], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[8]);
	p[11] = DIGIT_MUL(ad[8], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[7]);
	p[13] = DIGIT_MUL(ad[7], bd[6]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]);
	rd[13] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[14]);
	p[1] = DIGIT_MUL(ad[14], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[13]);
	p[3] = DIGIT_MUL(ad[13], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[12]);
	p[5] = DIGIT_MUL(ad[12], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[11]);
	p[7] = DIGIT_MUL(ad[11], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[10]);
	p[9] = DIGIT_MUL(ad[10], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[9]);
	p[11] = DIGIT_MUL(ad[9], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[8]);
	p[13] = DIGIT_MUL(ad[8], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[7]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]);
	rd[14] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[14]);
	p[3] = DIGIT_MUL(ad[14], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[13]);
	p[5] = DIGIT_MUL(ad[13], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[12]);
	p[7] = DIGIT_MUL(ad[12], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[11]);
	p[9] = DIGIT_MUL(ad[11], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[10]);
	p[11] = DIGIT_MUL(ad[10], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[9]);
	p[13] = DIGIT_MUL(ad[9], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[8]);
	p[15] = DIGIT_MUL(ad[8], bd[7]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]);
	rd[15] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[1], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[1]);
	p[2] = DIGIT_MUL(ad[2], bd[14]);
	p[3] = DIGIT_MUL(ad[14], bd[2]);
	p[4] = DIGIT_MUL(ad[3], bd[13]);
	p[5] = DIGIT_MUL(ad[13], bd[3]);
	p[6] = DIGIT_MUL(ad[4], bd[12]);
	p[7] = DIGIT_MUL(ad[12], bd[4]);
	p[8] = DIGIT_MUL(ad[5], bd[11]);
	p[9] = DIGIT_MUL(ad[11], bd[5]);
	p[10] = DIGIT_MUL(ad[6], bd[10]);
	p[11] = DIGIT_MUL(ad[10], bd[6]);
	p[12] = DIGIT_MUL(ad[7], bd[9]);
	p[13] = DIGIT_MUL(ad[9], bd[7]);
	p[14] = DIGIT_MUL(ad[8], bd[8]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]);
	rd[16] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[2], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[2]);
	p[2] = DIGIT_MUL(ad[3], bd[14]);
	p[3] = DIGIT_MUL(ad[14], bd[3]);
	p[4] = DIGIT_MUL(ad[4], bd[13]);
	p[5] = DIGIT_MUL(ad[13], bd[4]);
	p[6] = DIGIT_MUL(ad[5], bd[12]);
	p[7] = DIGIT_MUL(ad[12], bd[5]);
	p[8] = DIGIT_MUL(ad[6], bd[11]);
	p[9] = DIGIT_MUL(ad[11], bd[6]);
	p[10] = DIGIT_MUL(ad[7], bd[10]);
	p[11] = DIGIT_MUL(ad[10], bd[7]);
	p[12] = DIGIT_MUL(ad[8], bd[9]);
	p[13] = DIGIT_MUL(ad[9], bd[8]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]);
	rd[17] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[3], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[3]);
	p[2] = DIGIT_MUL(ad[4], bd[14]);
	p[3] = DIGIT_MUL(ad[14], bd[4]);
	p[4] = DIGIT_MUL(ad[5], bd[13]);
	p[5] = DIGIT_MUL(ad[13], bd[5]);
	p[6] = DIGIT_MUL(ad[6], bd[12]);
	p[7] = DIGIT_MUL(ad[12], bd[6]);
	p[8] = DIGIT_MUL(ad[7], bd[11]);
	p[9] = DIGIT_MUL(ad[11], bd[7]);
	p[10] = DIGIT_MUL(ad[8], bd[10]);
	p[11] = DIGIT_MUL(ad[10], bd[8]);
	p[12] = DIGIT_MUL(ad[9], bd[9]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]);
	rd[18] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[4], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[4]);
	p[2] = DIGIT_MUL(ad[5], bd[14]);
	p[3] = DIGIT_MUL(ad[14], bd[5]);
	p[4] = DIGIT_MUL(ad[6], bd[13]);
	p[5] = DIGIT_MUL(ad[13], bd[6]);
	p[6] = DIGIT_MUL(ad[7], bd[12]);
	p[7] = DIGIT_MUL(ad[12], bd[7]);
	p[8] = DIGIT_MUL(ad[8], bd[11]);
	p[9] = DIGIT_MUL(ad[11], bd[8]);
	p[10] = DIGIT_MUL(ad[9], bd[10]);
	p[11] = DIGIT_MUL(ad[10], bd[9]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]);
	rd[19] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[5], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[5]);
	p[2] = DIGIT_MUL(ad[6], bd[14]);
	p[3] = DIGIT_MUL(ad[14], bd[6]);
	p[4] = DIGIT_MUL(ad[7], bd[13]);
	p[5] = DIGIT_MUL(ad[13], bd[7]);
	p[6] = DIGIT_MUL(ad[8], bd[12]);
	p[7] = DIGIT_MUL(ad[12], bd[8]);
	p[8] = DIGIT_MUL(ad[9], bd[11]);
	p[9] = DIGIT_MUL(ad[11], bd[9]);
	p[10] = DIGIT_MUL(ad[10], bd[10]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]);
	rd[20] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[6], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[6]);
	p[2] = DIGIT_MUL(ad[7], bd[14]);
	p[3] = DIGIT_MUL(ad[14], bd[7]);
	p[4] = DIGIT_MUL(ad[8], bd[13]);
	p[5] = DIGIT_MUL(ad[13], bd[8]);
	p[6] = DIGIT_MUL(ad[9], bd[12]);
	p[7] = DIGIT_MUL(ad[12], bd[9]);
	p[8] = DIGIT_MUL(ad[10], bd[11]);
	p[9] = DIGIT_MUL(ad[11], bd[10]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]);
	rd[21] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[7], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[7]);
	p[2] = DIGIT_MUL(ad[8], bd[14]);
	p[3] = DIGIT_MUL(ad[14], bd[8]);
	p[4] = DIGIT_MUL(ad[9], bd[13]);
	p[5] = DIGIT_MUL(ad[13], bd[9]);
	p[6] = DIGIT_MUL(ad[10], bd[12]);
	p[7] = DIGIT_MUL(ad[12], bd[10]);
	p[8] = DIGIT_MUL(ad[11], bd[11]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]);
	rd[22] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[8], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[8]);
	p[2] = DIGIT_MUL(ad[9], bd[14]);
	p[3] = DIGIT_MUL(ad[14], bd[9]);
	p[4] = DIGIT_MUL(ad[10], bd[13]);
	p[5] = DIGIT_MUL(ad[13], bd[10]);
	p[6] = DIGIT_MUL(ad[11], bd[12]);
	p[7] = DIGIT_MUL(ad[12], bd[11]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]);
	rd[23] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[9], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[9]);
	p[2] = DIGIT_MUL(ad[10], bd[14]);
	p[3] = DIGIT_MUL(ad[14], bd[10]);
	p[4] = DIGIT_MUL(ad[11], bd[13]);
	p[5] = DIGIT_MUL(ad[13], bd[11]);
	p[6] = DIGIT_MUL(ad[12], bd[12]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]);
	rd[24] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[10], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[10]);
	p[2] = DIGIT_MUL(ad[11], bd[14]);
	p[3] = DIGIT_MUL(ad[14], bd[11]);
	p[4] = DIGIT_MUL(ad[12], bd[13]);
	p[5] = DIGIT_MUL(ad[13], bd[12]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]);
	rd[25] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[11], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[11]);
	p[2] = DIGIT_MUL(ad[12], bd[14]);
	p[3] = DIGIT_MUL(ad[14], bd[12]);
	p[4] = DIGIT_MUL(ad[13], bd[13]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]);
	rd[26] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[12], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[12]);
	p[2] = DIGIT_MUL(ad[13], bd[14]);
	p[3] = DIGIT_MUL(ad[14], bd[13]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]);
	rd[27] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[13], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[13]);
	p[2] = DIGIT_MUL(ad[14], bd[14]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]);
	rd[28] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[14], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[14]);
	t = c + LO(p[0]) + LO(p[1]);
	rd[29] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[15], bd[15]);
	t = c + LO(p[0]);
	rd[30] = (SslBigNumDigit)t;
	rd[31] = (SslBigNumDigit)(HI(p[0]) + (t >> 16));
}

/* r[0..15] = a[0..7]^2 */
void sslSquareBigNumWordsC8(SslBigNumWord *r, const SslBigNumWord *a)
{
	SslBigNumDigit *rd = (SslBigNumDigit *)r;
	const SslBigNumDigit *ad = (const SslBigNumDigit *)a;
	SceUInt32 p[9];
	SceUInt32 c, t;

	p[0] = DIGIT_MUL(ad[0], ad[0]);
	t = LO(p[0]);
	rd[0] = (SslBigNumDigit)t;
	c = HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], ad[1]);
	t = c + 2 * LO(p[0]);
	rd[1] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[1], ad[1]);
	p[1] = DIGIT_MUL(ad[0], ad[2]);
	t = c + LO(p[0]) + 2 * LO(p[1]);
	rd[2] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[1], ad[2]);
	p[1] = DIGIT_MUL(ad[0], ad[3]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]);
	rd[3] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[2], ad[2]);
	p[1] = DIGIT_MUL(ad[1], ad[3]);
	p[2] = DIGIT_MUL(ad[0], ad[4]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[4] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[2], ad[3]);
	p[1] = DIGIT_MUL(ad[1], ad[4]);
	p[2] = DIGIT_MUL(ad[0], ad[5]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[5] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[3], ad[3]);
	p[1] = DIGIT_MUL(ad[2], ad[4]);
	p[2] = DIGIT_MUL(ad[1], ad[5]);
	p[3] = DIGIT_MUL(ad[0], ad[6]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[6] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[3], ad[4]);
	p[1] = DIGIT_MUL(ad[2], ad[5]);
	p[2] = DIGIT_MUL(ad[1], ad[6]);
	p[3] = DIGIT_MUL(ad[0], ad[7]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[7] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[4], ad[4]);
	p[1] = DIGIT_MUL(ad[3], ad[5]);
	p[2] = DIGIT_MUL(ad[2], ad[6]);
	p[3] = DIGIT_MUL(ad[1], ad[7]);
	p[4] = DIGIT_MUL(ad[0], ad[8]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]);
	rd[8] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[4], ad[5]);
	p[1] = DIGIT_MUL(ad[3], ad[6]);
	p[2] = DIGIT_MUL(ad[2], ad[7]);
	p[3] = DIGIT_MUL(ad[1], ad[8]);
	p[4] = DIGIT_MUL(ad[0], ad[9]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]);
	rd[9] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[5], ad[5]);
	p[1] = DIGIT_MUL(ad[4], ad[6]);
	p[2] = DIGIT_MUL(ad[3], ad[7]);
	p[3] = DIGIT_MUL(ad[2], ad[8]);
	p[4] = DIGIT_MUL(ad[1], ad[9]);
	p[5] = DIGIT_MUL(ad[0], ad[10]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]);
	rd[10] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[5], ad[6]);
	p[1] = DIGIT_MUL(ad[4], ad[7]);
	p[2] = DIGIT_MUL(ad[3], ad[8]);
	p[3] = DIGIT_MUL(ad[2], ad[9]);
	p[4] = DIGIT_MUL(ad[1], ad[10]);
	p[5] = DIGIT_MUL(ad[0], ad[11]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]);
	rd[11] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[6], ad[6]);
	p[1] = DIGIT_MUL(ad[5], ad[7]);
	p[2] = DIGIT_MUL(ad[4], ad[8]);
	p[3] = DIGIT_MUL(ad[3], ad[9]);
	p[4] = DIGIT_MUL(ad[2], ad[10]);
	p[5] = DIGIT_MUL(ad[1], ad[11]);
	p[6] = DIGIT_MUL(ad[0], ad[12]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]);
	rd[12] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[6], ad[7]);
	p[1] = DIGIT_MUL(ad[5], ad[8]);
	p[2] = DIGIT_MUL(ad[4], ad[9]);
	p[3] = DIGIT_MUL(ad[3], ad[10]);
	p[4] = DIGIT_MUL(ad[2], ad[11]);
	p[5] = DIGIT_MUL(ad[1], ad[12]);
	p[6] = DIGIT_MUL(ad[0], ad[13]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]);
	rd[13] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[7], ad[7]);
	p[1] = DIGIT_MUL(ad[6], ad[8]);
	p[2] = DIGIT_MUL(ad[5], ad[9]);
	p[3] = DIGIT_MUL(ad[4], ad[10]);
	p[4] = DIGIT_MUL(ad[3], ad[11]);
	p[5] = DIGIT_MUL(ad[2], ad[12]);
	p[6] = DIGIT_MUL(ad[1], ad[13]);
	p[7] = DIGIT_MUL(ad[0], ad[14]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]);
	rd[14] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[7], ad[8]);
	p[1] = DIGIT_MUL(ad[6], ad[9]);
	p[2] = DIGIT_MUL(ad[5], ad[10]);
	p[3] = DIGIT_MUL(ad[4], ad[11]);
	p[4] = DIGIT_MUL(ad[3], ad[12]);
	p[5] = DIGIT_MUL(ad[2], ad[13]);
	p[6] = DIGIT_MUL(ad[1], ad[14]);
	p[7] = DIGIT_MUL(ad[0], ad[15]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]);
	rd[15] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[8], ad[8]);
	p[1] = DIGIT_MUL(ad[7], ad[9]);
	p[2] = DIGIT_MUL(ad[6], ad[10]);
	p[3] = DIGIT_MUL(ad[5], ad[11]);
	p[4] = DIGIT_MUL(ad[4], ad[12]);
	p[5] = DIGIT_MUL(ad[3], ad[13]);
	p[6] = DIGIT_MUL(ad[2], ad[14]);
	p[7] = DIGIT_MUL(ad[1], ad[15]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]);
	rd[16] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[8], ad[9]);
	p[1] = DIGIT_MUL(ad[7], ad[10]);
	p[2] = DIGIT_MUL(ad[6], ad[11]);
	p[3] = DIGIT_MUL(ad[5], ad[12]);
	p[4] = DIGIT_MUL(ad[4], ad[13]);
	p[5] = DIGIT_MUL(ad[3], ad[14]);
	p[6] = DIGIT_MUL(ad[2], ad[15]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]);
	rd[17] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[9], ad[9]);
	p[1] = DIGIT_MUL(ad[8], ad[10]);
	p[2] = DIGIT_MUL(ad[7], ad[11]);
	p[3] = DIGIT_MUL(ad[6], ad[12]);
	p[4] = DIGIT_MUL(ad[5], ad[13]);
	p[5] = DIGIT_MUL(ad[4], ad[14]);
	p[6] = DIGIT_MUL(ad[3], ad[15]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]);
	rd[18] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[9], ad[10]);
	p[1] = DIGIT_MUL(ad[8], ad[11]);
	p[2] = DIGIT_MUL(ad[7], ad[12]);
	p[3] = DIGIT_MUL(ad[6], ad[13]);
	p[4] = DIGIT_MUL(ad[5], ad[14]);
	p[5] = DIGIT_MUL(ad[4], ad[15]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]);
	rd[19] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[10], ad[10]);
	p[1] = DIGIT_MUL(ad[9], ad[11]);
	p[2] = DIGIT_MUL(ad[8], ad[12]);
	p[3] = DIGIT_MUL(ad[7], ad[13]);
	p[4] = DIGIT_MUL(ad[6], ad[14]);
	p[5] = DIGIT_MUL(ad[5], ad[15]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]);
	rd[20] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[10], ad[11]);
	p[1] = DIGIT_MUL(ad[9], ad[12]);
	p[2] = DIGIT_MUL(ad[8], ad[13]);
	p[3] = DIGIT_MUL(ad[7], ad[14]);
	p[4] = DIGIT_MUL(ad[6], ad[15]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]);
	rd[21] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[11], ad[11]);
	p[1] = DIGIT_MUL(ad[10], ad[12]);
	p[2] = DIGIT_MUL(ad[9], ad[13]);
	p[3] = DIGIT_MUL(ad[8], ad[14]);
	p[4] = DIGIT_MUL(ad[7], ad[15]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]);
	rd[22] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[11], ad[12]);
	p[1] = DIGIT_MUL(ad[10], ad[13]);
	p[2] = DIGIT_MUL(ad[9], ad[14]);
	p[3] = DIGIT_MUL(ad[8], ad[15]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[23] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[12], ad[12]);
	p[1] = DIGIT_MUL(ad[11], ad[13]);
	p[2] = DIGIT_MUL(ad[10], ad[14]);
	p[3] = DIGIT_MUL(ad[9], ad[15]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[24] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[12], ad[13]);
	p[1] = DIGIT_MUL(ad[11], ad[14]);
	p[2] = DIGIT_MUL(ad[10], ad[15]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[25] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[13], ad[13]);
	p[1] = DIGIT_MUL(ad[12], ad[14]);
	p[2] = DIGIT_MUL(ad[11], ad[15]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[26] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[13], ad[14]);
	p[1] = DIGIT_MUL(ad[12], ad[15]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]);
	rd[27] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[14], ad[14]);
	p[1] = DIGIT_MUL(ad[13], ad[15]);
	t = c + LO(p[0]) + 2 * LO(p[1]);
	rd[28] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[14], ad[15]);
	t = c + 2 * LO(p[0]);
	rd[29] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[15], ad[15]);
	t = c + LO(p[0]);
	rd[30] = (SslBigNumDigit)t;
	rd[31] = (SslBigNumDigit)(HI(p[0]) + (t >> 16));
}

/* r[0..21] = a[0..10] * b[0..10] */
void sslMultiplyBigNumWordsC11(SslBigNumWord *r, const SslBigNumWord *a,
				const SslBigNumWord *b)
{
	SslBigNumDigit *rd = (SslBigNumDigit *)r;
	const SslBigNumDigit *ad = (const SslBigNumDigit *)a;
	const SslBigNumDigit *bd = (const SslBigNumDigit *)b;
	SceUInt32 p[22];
	SceUInt32 c, t;

	p[0] = DIGIT_MUL(ad[0], bd[0]);
	t = LO(p[0]);
	rd[0] = (SslBigNumDigit)t;
	c = HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[1]);
	p[1] = DIGIT_MUL(ad[1], bd[0]);
	t = c + LO(p[0]) + LO(p[1]);
	rd[1] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[2]);
	p[1] = DIGIT_MUL(ad[2], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[1]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]);
	rd[2] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[3]);
	p[1] = DIGIT_MUL(ad[3], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[2]);
	p[3] = DIGIT_MUL(ad[2], bd[1]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]);
	rd[3] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[4]);
	p[1] = DIGIT_MUL(ad[4], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[3]);
	p[3] = DIGIT_MUL(ad[3], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[2]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]);
	rd[4] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[5]);
	p[1] = DIGIT_MUL(ad[5], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[4]);
	p[3] = DIGIT_MUL(ad[4], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[3]);
	p[5] = DIGIT_MUL(ad[3], bd[2]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]);
	rd[5] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[6]);
	p[1] = DIGIT_MUL(ad[6], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[5]);
	p[3] = DIGIT_MUL(ad[5], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[4]);
	p[5] = DIGIT_MUL(ad[4], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[3]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]);
	rd[6] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[7]);
	p[1] = DIGIT_MUL(ad[7], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[6]);
	p[3] = DIGIT_MUL(ad[6], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[5]);
	p[5] = DIGIT_MUL(ad[5], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[4]);
	p[7] = DIGIT_MUL(ad[4], bd[3]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]);
	rd[7] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[8]);
	p[1] = DIGIT_MUL(ad[8], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[7]);
	p[3] = DIGIT_MUL(ad[7], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[6]);
	p[5] = DIGIT_MUL(ad[6], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[5]);
	p[7] = DIGIT_MUL(ad[5], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[4]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]);
	rd[8] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[9]);
	p[1] = DIGIT_MUL(ad[9], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[8]);
	p[3] = DIGIT_MUL(ad[8], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[7]);
	p[5] = DIGIT_MUL(ad[7], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[6]);
	p[7] = DIGIT_MUL(ad[6], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[5]);
	p[9] = DIGIT_MUL(ad[5], bd[4]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]);
	rd[9] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[10]);
	p[1] = DIGIT_MUL(ad[10], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[9]);
	p[3] = DIGIT_MUL(ad[9], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[8]);
	p[5] = DIGIT_MUL(ad[8], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[7]);
	p[7] = DIGIT_MUL(ad[7], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[6]);
	p[9] = DIGIT_MUL(ad[6], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[5]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]);
	rd[10] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[11]);
	p[1] = DIGIT_MUL(ad[11], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[10]);
	p[3] = DIGIT_MUL(ad[10], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[9]);
	p[5] = DIGIT_MUL(ad[9], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[8]);
	p[7] = DIGIT_MUL(ad[8], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[7]);
	p[9] = DIGIT_MUL(ad[7], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[6]);
	p[11] = DIGIT_MUL(ad[6], bd[5]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]);
	rd[11] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[12]);
	p[1] = DIGIT_MUL(ad[12], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[11]);
	p[3] = DIGIT_MUL(ad[11], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[10]);
	p[5] = DIGIT_MUL(ad[10], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[9]);
	p[7] = DIGIT_MUL(ad[9], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[8]);
	p[9] = DIGIT_MUL(ad[8], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[7]);
	p[11] = DIGIT_MUL(ad[7], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[6]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]);
	rd[12] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[13]);
	p[1] = DIGIT_MUL(ad[13], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[12]);
	p[3] = DIGIT_MUL(ad[12], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[11]);
	p[5] = DIGIT_MUL(ad[11], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[10]);
	p[7] = DIGIT_MUL(ad[10], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[9]);
	p[9] = DIGIT_MUL(ad[9], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[8]);
	p[11] = DIGIT_MUL(ad[8], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[7]);
	p[13] = DIGIT_MUL(ad[7], bd[6]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]);
	rd[13] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[14]);
	p[1] = DIGIT_MUL(ad[14], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[13]);
	p[3] = DIGIT_MUL(ad[13], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[12]);
	p[5] = DIGIT_MUL(ad[12], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[11]);
	p[7] = DIGIT_MUL(ad[11], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[10]);
	p[9] = DIGIT_MUL(ad[10], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[9]);
	p[11] = DIGIT_MUL(ad[9], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[8]);
	p[13] = DIGIT_MUL(ad[8], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[7]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]);
	rd[14] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[14]);
	p[3] = DIGIT_MUL(ad[14], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[13]);
	p[5] = DIGIT_MUL(ad[13], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[12]);
	p[7] = DIGIT_MUL(ad[12], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[11]);
	p[9] = DIGIT_MUL(ad[11], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[10]);
	p[11] = DIGIT_MUL(ad[10], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[9]);
	p[13] = DIGIT_MUL(ad[9], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[8]);
	p[15] = DIGIT_MUL(ad[8], bd[7]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]);
	rd[15] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[16]);
	p[1] = DIGIT_MUL(ad[16], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[15]);
	p[3] = DIGIT_MUL(ad[15], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[14]);
	p[5] = DIGIT_MUL(ad[14], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[13]);
	p[7] = DIGIT_MUL(ad[13], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[12]);
	p[9] = DIGIT_MUL(ad[12], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[11]);
	p[11] = DIGIT_MUL(ad[11], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[10]);
	p[13] = DIGIT_MUL(ad[10], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[9]);
	p[15] = DIGIT_MUL(ad[9], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[8]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]);
	rd[16] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[17]);
	p[1] = DIGIT_MUL(ad[17], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[16]);
	p[3] = DIGIT_MUL(ad[16], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[15]);
	p[5] = DIGIT_MUL(ad[15], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[14]);
	p[7] = DIGIT_MUL(ad[14], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[13]);
	p[9] = DIGIT_MUL(ad[13], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[12]);
	p[11] = DIGIT_MUL(ad[12], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[11]);
	p[13] = DIGIT_MUL(ad[11], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[10]);
	p[15] = DIGIT_MUL(ad[10], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[9]);
	p[17] = DIGIT_MUL(ad[9], bd[8]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]);
	rd[17] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[18]);
	p[1] = DIGIT_MUL(ad[18], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[17]);
	p[3] = DIGIT_MUL(ad[17], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[16]);
	p[5] = DIGIT_MUL(ad[16], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[15]);
	p[7] = DIGIT_MUL(ad[15], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[14]);
	p[9] = DIGIT_MUL(ad[14], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[13]);
	p[11] = DIGIT_MUL(ad[13], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[12]);
	p[13] = DIGIT_MUL(ad[12], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[11]);
	p[15] = DIGIT_MUL(ad[11], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[10]);
	p[17] = DIGIT_MUL(ad[10], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[9]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]);
	rd[18] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[19]);
	p[1] = DIGIT_MUL(ad[19], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[18]);
	p[3] = DIGIT_MUL(ad[18], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[17]);
	p[5] = DIGIT_MUL(ad[17], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[16]);
	p[7] = DIGIT_MUL(ad[16], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[15]);
	p[9] = DIGIT_MUL(ad[15], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[14]);
	p[11] = DIGIT_MUL(ad[14], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[13]);
	p[13] = DIGIT_MUL(ad[13], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[12]);
	p[15] = DIGIT_MUL(ad[12], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[11]);
	p[17] = DIGIT_MUL(ad[11], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[10]);
	p[19] = DIGIT_MUL(ad[10], bd[9]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]);
	rd[19] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[20]);
	p[1] = DIGIT_MUL(ad[20], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[19]);
	p[3] = DIGIT_MUL(ad[19], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[18]);
	p[5] = DIGIT_MUL(ad[18], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[17]);
	p[7] = DIGIT_MUL(ad[17], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[16]);
	p[9] = DIGIT_MUL(ad[16], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[15]);
	p[11] = DIGIT_MUL(ad[15], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[14]);
	p[13] = DIGIT_MUL(ad[14], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[13]);
	p[15] = DIGIT_MUL(ad[13], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[12]);
	p[17] = DIGIT_MUL(ad[12], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[11]);
	p[19] = DIGIT_MUL(ad[11], bd[9]);
	p[20] = DIGIT_MUL(ad[10], bd[10]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]);
	rd[20] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[18]);
	p[7] = DIGIT_MUL(ad[18], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[17]);
	p[9] = DIGIT_MUL(ad[17], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[16]);
	p[11] = DIGIT_MUL(ad[16], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[15]);
	p[13] = DIGIT_MUL(ad[15], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[14]);
	p[15] = DIGIT_MUL(ad[14], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[13]);
	p[17] = DIGIT_MUL(ad[13], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[12]);
	p[19] = DIGIT_MUL(ad[12], bd[9]);
	p[20] = DIGIT_MUL(ad[10], bd[11]);
	p[21] = DIGIT_MUL(ad[11], bd[10]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]);
	rd[21] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[1], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[1]);
	p[2] = DIGIT_MUL(ad[2], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[2]);
	p[4] = DIGIT_MUL(ad[3], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[3]);
	p[6] = DIGIT_MUL(ad[4], bd[18]);
	p[7] = DIGIT_MUL(ad[18], bd[4]);
	p[8] = DIGIT_MUL(ad[5], bd[17]);
	p[9] = DIGIT_MUL(ad[17], bd[5]);
	p[10] = DIGIT_MUL(ad[6], bd[16]);
	p[11] = DIGIT_MUL(ad[16], bd[6]);
	p[12] = DIGIT_MUL(ad[7], bd[15]);
	p[13] = DIGIT_MUL(ad[15], bd[7]);
	p[14] = DIGIT_MUL(ad[8], bd[14]);
	p[15] = DIGIT_MUL(ad[14], bd[8]);
	p[16] = DIGIT_MUL(ad[9], bd[13]);
	p[17] = DIGIT_MUL(ad[13], bd[9]);
	p[18] = DIGIT_MUL(ad[10], bd[12]);
	p[19] = DIGIT_MUL(ad[12], bd[10]);
	p[20] = DIGIT_MUL(ad[11], bd[11]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]);
	rd[22] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[2], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[2]);
	p[2] = DIGIT_MUL(ad[3], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[3]);
	p[4] = DIGIT_MUL(ad[4], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[4]);
	p[6] = DIGIT_MUL(ad[5], bd[18]);
	p[7] = DIGIT_MUL(ad[18], bd[5]);
	p[8] = DIGIT_MUL(ad[6], bd[17]);
	p[9] = DIGIT_MUL(ad[17], bd[6]);
	p[10] = DIGIT_MUL(ad[7], bd[16]);
	p[11] = DIGIT_MUL(ad[16], bd[7]);
	p[12] = DIGIT_MUL(ad[8], bd[15]);
	p[13] = DIGIT_MUL(ad[15], bd[8]);
	p[14] = DIGIT_MUL(ad[9], bd[14]);
	p[15] = DIGIT_MUL(ad[14], bd[9]);
	p[16] = DIGIT_MUL(ad[10], bd[13]);
	p[17] = DIGIT_MUL(ad[13], bd[10]);
	p[18] = DIGIT_MUL(ad[11], bd[12]);
	p[19] = DIGIT_MUL(ad[12], bd[11]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]);
	rd[23] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[3], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[3]);
	p[2] = DIGIT_MUL(ad[4], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[4]);
	p[4] = DIGIT_MUL(ad[5], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[5]);
	p[6] = DIGIT_MUL(ad[6], bd[18]);
	p[7] = DIGIT_MUL(ad[18], bd[6]);
	p[8] = DIGIT_MUL(ad[7], bd[17]);
	p[9] = DIGIT_MUL(ad[17], bd[7]);
	p[10] = DIGIT_MUL(ad[8], bd[16]);
	p[11] = DIGIT_MUL(ad[16], bd[8]);
	p[12] = DIGIT_MUL(ad[9], bd[15]);
	p[13] = DIGIT_MUL(ad[15], bd[9]);
	p[14] = DIGIT_MUL(ad[10], bd[14]);
	p[15] = DIGIT_MUL(ad[14], bd[10]);
	p[16] = DIGIT_MUL(ad[11], bd[13]);
	p[17] = DIGIT_MUL(ad[13], bd[11]);
	p[18] = DIGIT_MUL(ad[12], bd[12]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]);
	rd[24] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[4], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[4]);
	p[2] = DIGIT_MUL(ad[5], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[5]);
	p[4] = DIGIT_MUL(ad[6], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[6]);
	p[6] = DIGIT_MUL(ad[7], bd[18]);
	p[7] = DIGIT_MUL(ad[18], bd[7]);
	p[8] = DIGIT_MUL(ad[8], bd[17]);
	p[9] = DIGIT_MUL(ad[17], bd[8]);
	p[10] = DIGIT_MUL(ad[9], bd[16]);
	p[11] = DIGIT_MUL(ad[16], bd[9]);
	p[12] = DIGIT_MUL(ad[10], bd[15]);
	p[13] = DIGIT_MUL(ad[15], bd[10]);
	p[14] = DIGIT_MUL(ad[11], bd[14]);
	p[15] = DIGIT_MUL(ad[14], bd[11]);
	p[16] = DIGIT_MUL(ad[12], bd[13]);
	p[17] = DIGIT_MUL(ad[13], bd[12]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]);
	rd[25] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[5], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[5]);
	p[2] = DIGIT_MUL(ad[6], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[6]);
	p[4] = DIGIT_MUL(ad[7], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[7]);
	p[6] = DIGIT_MUL(ad[8], bd[18]);
	p[7] = DIGIT_MUL(ad[18], bd[8]);
	p[8] = DIGIT_MUL(ad[9], bd[17]);
	p[9] = DIGIT_MUL(ad[17], bd[9]);
	p[10] = DIGIT_MUL(ad[10], bd[16]);
	p[11] = DIGIT_MUL(ad[16], bd[10]);
	p[12] = DIGIT_MUL(ad[11], bd[15]);
	p[13] = DIGIT_MUL(ad[15], bd[11]);
	p[14] = DIGIT_MUL(ad[12], bd[14]);
	p[15] = DIGIT_MUL(ad[14], bd[12]);
	p[16] = DIGIT_MUL(ad[13], bd[13]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]);
	rd[26] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[6], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[6]);
	p[2] = DIGIT_MUL(ad[7], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[7]);
	p[4] = DIGIT_MUL(ad[8], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[8]);
	p[6] = DIGIT_MUL(ad[9], bd[18]);
	p[7] = DIGIT_MUL(ad[18], bd[9]);
	p[8] = DIGIT_MUL(ad[10], bd[17]);
	p[9] = DIGIT_MUL(ad[17], bd[10]);
	p[10] = DIGIT_MUL(ad[11], bd[16]);
	p[11] = DIGIT_MUL(ad[16], bd[11]);
	p[12] = DIGIT_MUL(ad[12], bd[15]);
	p[13] = DIGIT_MUL(ad[15], bd[12]);
	p[14] = DIGIT_MUL(ad[13], bd[14]);
	p[15] = DIGIT_MUL(ad[14], bd[13]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]);
	rd[27] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[7], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[7]);
	p[2] = DIGIT_MUL(ad[8], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[8]);
	p[4] = DIGIT_MUL(ad[9], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[9]);
	p[6] = DIGIT_MUL(ad[10], bd[18]);
	p[7] = DIGIT_MUL(ad[18], bd[10]);
	p[8] = DIGIT_MUL(ad[11], bd[17]);
	p[9] = DIGIT_MUL(ad[17], bd[11]);
	p[10] = DIGIT_MUL(ad[12], bd[16]);
	p[11] = DIGIT_MUL(ad[16], bd[12]);
	p[12] = DIGIT_MUL(ad[13], bd[15]);
	p[13] = DIGIT_MUL(ad[15], bd[13]);
	p[14] = DIGIT_MUL(ad[14], bd[14]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]);
	rd[28] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[8], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[8]);
	p[2] = DIGIT_MUL(ad[9], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[9]);
	p[4] = DIGIT_MUL(ad[10], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[10]);
	p[6] = DIGIT_MUL(ad[11], bd[18]);
	p[7] = DIGIT_MUL(ad[18], bd[11]);
	p[8] = DIGIT_MUL(ad[12], bd[17]);
	p[9] = DIGIT_MUL(ad[17], bd[12]);
	p[10] = DIGIT_MUL(ad[13], bd[16]);
	p[11] = DIGIT_MUL(ad[16], bd[13]);
	p[12] = DIGIT_MUL(ad[14], bd[15]);
	p[13] = DIGIT_MUL(ad[15], bd[14]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]);
	rd[29] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[9], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[9]);
	p[2] = DIGIT_MUL(ad[10], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[10]);
	p[4] = DIGIT_MUL(ad[11], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[11]);
	p[6] = DIGIT_MUL(ad[12], bd[18]);
	p[7] = DIGIT_MUL(ad[18], bd[12]);
	p[8] = DIGIT_MUL(ad[13], bd[17]);
	p[9] = DIGIT_MUL(ad[17], bd[13]);
	p[10] = DIGIT_MUL(ad[14], bd[16]);
	p[11] = DIGIT_MUL(ad[16], bd[14]);
	p[12] = DIGIT_MUL(ad[15], bd[15]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]);
	rd[30] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[10], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[10]);
	p[2] = DIGIT_MUL(ad[11], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[11]);
	p[4] = DIGIT_MUL(ad[12], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[12]);
	p[6] = DIGIT_MUL(ad[13], bd[18]);
	p[7] = DIGIT_MUL(ad[18], bd[13]);
	p[8] = DIGIT_MUL(ad[14], bd[17]);
	p[9] = DIGIT_MUL(ad[17], bd[14]);
	p[10] = DIGIT_MUL(ad[15], bd[16]);
	p[11] = DIGIT_MUL(ad[16], bd[15]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]);
	rd[31] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[11], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[11]);
	p[2] = DIGIT_MUL(ad[12], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[12]);
	p[4] = DIGIT_MUL(ad[13], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[13]);
	p[6] = DIGIT_MUL(ad[14], bd[18]);
	p[7] = DIGIT_MUL(ad[18], bd[14]);
	p[8] = DIGIT_MUL(ad[15], bd[17]);
	p[9] = DIGIT_MUL(ad[17], bd[15]);
	p[10] = DIGIT_MUL(ad[16], bd[16]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]);
	rd[32] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[12], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[12]);
	p[2] = DIGIT_MUL(ad[13], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[13]);
	p[4] = DIGIT_MUL(ad[14], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[14]);
	p[6] = DIGIT_MUL(ad[15], bd[18]);
	p[7] = DIGIT_MUL(ad[18], bd[15]);
	p[8] = DIGIT_MUL(ad[16], bd[17]);
	p[9] = DIGIT_MUL(ad[17], bd[16]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]);
	rd[33] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[13], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[13]);
	p[2] = DIGIT_MUL(ad[14], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[14]);
	p[4] = DIGIT_MUL(ad[15], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[15]);
	p[6] = DIGIT_MUL(ad[16], bd[18]);
	p[7] = DIGIT_MUL(ad[18], bd[16]);
	p[8] = DIGIT_MUL(ad[17], bd[17]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]);
	rd[34] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[14], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[14]);
	p[2] = DIGIT_MUL(ad[15], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[15]);
	p[4] = DIGIT_MUL(ad[16], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[16]);
	p[6] = DIGIT_MUL(ad[17], bd[18]);
	p[7] = DIGIT_MUL(ad[18], bd[17]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]);
	rd[35] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[15], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[15]);
	p[2] = DIGIT_MUL(ad[16], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[16]);
	p[4] = DIGIT_MUL(ad[17], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[17]);
	p[6] = DIGIT_MUL(ad[18], bd[18]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]);
	rd[36] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[16], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[16]);
	p[2] = DIGIT_MUL(ad[17], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[17]);
	p[4] = DIGIT_MUL(ad[18], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[18]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]);
	rd[37] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[17], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[17]);
	p[2] = DIGIT_MUL(ad[18], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[18]);
	p[4] = DIGIT_MUL(ad[19], bd[19]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]);
	rd[38] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[18], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[18]);
	p[2] = DIGIT_MUL(ad[19], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[19]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]);
	rd[39] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[19], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[19]);
	p[2] = DIGIT_MUL(ad[20], bd[20]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]);
	rd[40] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[20], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[20]);
	t = c + LO(p[0]) + LO(p[1]);
	rd[41] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[21], bd[21]);
	t = c + LO(p[0]);
	rd[42] = (SslBigNumDigit)t;
	rd[43] = (SslBigNumDigit)(HI(p[0]) + (t >> 16));
}

/* r[0..21] = a[0..10]^2 */
void sslSquareBigNumWordsC11(SslBigNumWord *r, const SslBigNumWord *a)
{
	SslBigNumDigit *rd = (SslBigNumDigit *)r;
	const SslBigNumDigit *ad = (const SslBigNumDigit *)a;
	SceUInt32 p[12];
	SceUInt32 c, t;

	p[0] = DIGIT_MUL(ad[0], ad[0]);
	t = LO(p[0]);
	rd[0] = (SslBigNumDigit)t;
	c = HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], ad[1]);
	t = c + 2 * LO(p[0]);
	rd[1] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[1], ad[1]);
	p[1] = DIGIT_MUL(ad[0], ad[2]);
	t = c + LO(p[0]) + 2 * LO(p[1]);
	rd[2] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[1], ad[2]);
	p[1] = DIGIT_MUL(ad[0], ad[3]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]);
	rd[3] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[2], ad[2]);
	p[1] = DIGIT_MUL(ad[1], ad[3]);
	p[2] = DIGIT_MUL(ad[0], ad[4]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[4] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[2], ad[3]);
	p[1] = DIGIT_MUL(ad[1], ad[4]);
	p[2] = DIGIT_MUL(ad[0], ad[5]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[5] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[3], ad[3]);
	p[1] = DIGIT_MUL(ad[2], ad[4]);
	p[2] = DIGIT_MUL(ad[1], ad[5]);
	p[3] = DIGIT_MUL(ad[0], ad[6]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[6] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[3], ad[4]);
	p[1] = DIGIT_MUL(ad[2], ad[5]);
	p[2] = DIGIT_MUL(ad[1], ad[6]);
	p[3] = DIGIT_MUL(ad[0], ad[7]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[7] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[4], ad[4]);
	p[1] = DIGIT_MUL(ad[3], ad[5]);
	p[2] = DIGIT_MUL(ad[2], ad[6]);
	p[3] = DIGIT_MUL(ad[1], ad[7]);
	p[4] = DIGIT_MUL(ad[0], ad[8]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]);
	rd[8] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[4], ad[5]);
	p[1] = DIGIT_MUL(ad[3], ad[6]);
	p[2] = DIGIT_MUL(ad[2], ad[7]);
	p[3] = DIGIT_MUL(ad[1], ad[8]);
	p[4] = DIGIT_MUL(ad[0], ad[9]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]);
	rd[9] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[5], ad[5]);
	p[1] = DIGIT_MUL(ad[4], ad[6]);
	p[2] = DIGIT_MUL(ad[3], ad[7]);
	p[3] = DIGIT_MUL(ad[2], ad[8]);
	p[4] = DIGIT_MUL(ad[1], ad[9]);
	p[5] = DIGIT_MUL(ad[0], ad[10]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]);
	rd[10] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[5], ad[6]);
	p[1] = DIGIT_MUL(ad[4], ad[7]);
	p[2] = DIGIT_MUL(ad[3], ad[8]);
	p[3] = DIGIT_MUL(ad[2], ad[9]);
	p[4] = DIGIT_MUL(ad[1], ad[10]);
	p[5] = DIGIT_MUL(ad[0], ad[11]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]);
	rd[11] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[6], ad[6]);
	p[1] = DIGIT_MUL(ad[5], ad[7]);
	p[2] = DIGIT_MUL(ad[4], ad[8]);
	p[3] = DIGIT_MUL(ad[3], ad[9]);
	p[4] = DIGIT_MUL(ad[2], ad[10]);
	p[5] = DIGIT_MUL(ad[1], ad[11]);
	p[6] = DIGIT_MUL(ad[0], ad[12]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]);
	rd[12] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[6], ad[7]);
	p[1] = DIGIT_MUL(ad[5], ad[8]);
	p[2] = DIGIT_MUL(ad[4], ad[9]);
	p[3] = DIGIT_MUL(ad[3], ad[10]);
	p[4] = DIGIT_MUL(ad[2], ad[11]);
	p[5] = DIGIT_MUL(ad[1], ad[12]);
	p[6] = DIGIT_MUL(ad[0], ad[13]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]);
	rd[13] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[7], ad[7]);
	p[1] = DIGIT_MUL(ad[6], ad[8]);
	p[2] = DIGIT_MUL(ad[5], ad[9]);
	p[3] = DIGIT_MUL(ad[4], ad[10]);
	p[4] = DIGIT_MUL(ad[3], ad[11]);
	p[5] = DIGIT_MUL(ad[2], ad[12]);
	p[6] = DIGIT_MUL(ad[1], ad[13]);
	p[7] = DIGIT_MUL(ad[0], ad[14]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]);
	rd[14] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[7], ad[8]);
	p[1] = DIGIT_MUL(ad[6], ad[9]);
	p[2] = DIGIT_MUL(ad[5], ad[10]);
	p[3] = DIGIT_MUL(ad[4], ad[11]);
	p[4] = DIGIT_MUL(ad[3], ad[12]);
	p[5] = DIGIT_MUL(ad[2], ad[13]);
	p[6] = DIGIT_MUL(ad[1], ad[14]);
	p[7] = DIGIT_MUL(ad[0], ad[15]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]);
	rd[15] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[8], ad[8]);
	p[1] = DIGIT_MUL(ad[7], ad[9]);
	p[2] = DIGIT_MUL(ad[6], ad[10]);
	p[3] = DIGIT_MUL(ad[5], ad[11]);
	p[4] = DIGIT_MUL(ad[4], ad[12]);
	p[5] = DIGIT_MUL(ad[3], ad[13]);
	p[6] = DIGIT_MUL(ad[2], ad[14]);
	p[7] = DIGIT_MUL(ad[1], ad[15]);
	p[8] = DIGIT_MUL(ad[0], ad[16]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]);
	rd[16] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[8], ad[9]);
	p[1] = DIGIT_MUL(ad[7], ad[10]);
	p[2] = DIGIT_MUL(ad[6], ad[11]);
	p[3] = DIGIT_MUL(ad[5], ad[12]);
	p[4] = DIGIT_MUL(ad[4], ad[13]);
	p[5] = DIGIT_MUL(ad[3], ad[14]);
	p[6] = DIGIT_MUL(ad[2], ad[15]);
	p[7] = DIGIT_MUL(ad[1], ad[16]);
	p[8] = DIGIT_MUL(ad[0], ad[17]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]);
	rd[17] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[9], ad[9]);
	p[1] = DIGIT_MUL(ad[8], ad[10]);
	p[2] = DIGIT_MUL(ad[7], ad[11]);
	p[3] = DIGIT_MUL(ad[6], ad[12]);
	p[4] = DIGIT_MUL(ad[5], ad[13]);
	p[5] = DIGIT_MUL(ad[4], ad[14]);
	p[6] = DIGIT_MUL(ad[3], ad[15]);
	p[7] = DIGIT_MUL(ad[2], ad[16]);
	p[8] = DIGIT_MUL(ad[1], ad[17]);
	p[9] = DIGIT_MUL(ad[0], ad[18]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]);
	rd[18] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[9], ad[10]);
	p[1] = DIGIT_MUL(ad[8], ad[11]);
	p[2] = DIGIT_MUL(ad[7], ad[12]);
	p[3] = DIGIT_MUL(ad[6], ad[13]);
	p[4] = DIGIT_MUL(ad[5], ad[14]);
	p[5] = DIGIT_MUL(ad[4], ad[15]);
	p[6] = DIGIT_MUL(ad[3], ad[16]);
	p[7] = DIGIT_MUL(ad[2], ad[17]);
	p[8] = DIGIT_MUL(ad[1], ad[18]);
	p[9] = DIGIT_MUL(ad[0], ad[19]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]);
	rd[19] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[10], ad[10]);
	p[1] = DIGIT_MUL(ad[9], ad[11]);
	p[2] = DIGIT_MUL(ad[8], ad[12]);
	p[3] = DIGIT_MUL(ad[7], ad[13]);
	p[4] = DIGIT_MUL(ad[6], ad[14]);
	p[5] = DIGIT_MUL(ad[5], ad[15]);
	p[6] = DIGIT_MUL(ad[4], ad[16]);
	p[7] = DIGIT_MUL(ad[3], ad[17]);
	p[8] = DIGIT_MUL(ad[2], ad[18]);
	p[9] = DIGIT_MUL(ad[1], ad[19]);
	p[10] = DIGIT_MUL(ad[0], ad[20]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]);
	rd[20] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[10], ad[11]);
	p[1] = DIGIT_MUL(ad[9], ad[12]);
	p[2] = DIGIT_MUL(ad[8], ad[13]);
	p[3] = DIGIT_MUL(ad[7], ad[14]);
	p[4] = DIGIT_MUL(ad[6], ad[15]);
	p[5] = DIGIT_MUL(ad[5], ad[16]);
	p[6] = DIGIT_MUL(ad[4], ad[17]);
	p[7] = DIGIT_MUL(ad[3], ad[18]);
	p[8] = DIGIT_MUL(ad[2], ad[19]);
	p[9] = DIGIT_MUL(ad[1], ad[20]);
	p[10] = DIGIT_MUL(ad[0], ad[21]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]);
	rd[21] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[11], ad[11]);
	p[1] = DIGIT_MUL(ad[10], ad[12]);
	p[2] = DIGIT_MUL(ad[9], ad[13]);
	p[3] = DIGIT_MUL(ad[8], ad[14]);
	p[4] = DIGIT_MUL(ad[7], ad[15]);
	p[5] = DIGIT_MUL(ad[6], ad[16]);
	p[6] = DIGIT_MUL(ad[5], ad[17]);
	p[7] = DIGIT_MUL(ad[4], ad[18]);
	p[8] = DIGIT_MUL(ad[3], ad[19]);
	p[9] = DIGIT_MUL(ad[2], ad[20]);
	p[10] = DIGIT_MUL(ad[1], ad[21]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]);
	rd[22] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[11], ad[12]);
	p[1] = DIGIT_MUL(ad[10], ad[13]);
	p[2] = DIGIT_MUL(ad[9], ad[14]);
	p[3] = DIGIT_MUL(ad[8], ad[15]);
	p[4] = DIGIT_MUL(ad[7], ad[16]);
	p[5] = DIGIT_MUL(ad[6], ad[17]);
	p[6] = DIGIT_MUL(ad[5], ad[18]);
	p[7] = DIGIT_MUL(ad[4], ad[19]);
	p[8] = DIGIT_MUL(ad[3], ad[20]);
	p[9] = DIGIT_MUL(ad[2], ad[21]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]);
	rd[23] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[12], ad[12]);
	p[1] = DIGIT_MUL(ad[11], ad[13]);
	p[2] = DIGIT_MUL(ad[10], ad[14]);
	p[3] = DIGIT_MUL(ad[9], ad[15]);
	p[4] = DIGIT_MUL(ad[8], ad[16]);
	p[5] = DIGIT_MUL(ad[7], ad[17]);
	p[6] = DIGIT_MUL(ad[6], ad[18]);
	p[7] = DIGIT_MUL(ad[5], ad[19]);
	p[8] = DIGIT_MUL(ad[4], ad[20]);
	p[9] = DIGIT_MUL(ad[3], ad[21]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]);
	rd[24] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[12], ad[13]);
	p[1] = DIGIT_MUL(ad[11], ad[14]);
	p[2] = DIGIT_MUL(ad[10], ad[15]);
	p[3] = DIGIT_MUL(ad[9], ad[16]);
	p[4] = DIGIT_MUL(ad[8], ad[17]);
	p[5] = DIGIT_MUL(ad[7], ad[18]);
	p[6] = DIGIT_MUL(ad[6], ad[19]);
	p[7] = DIGIT_MUL(ad[5], ad[20]);
	p[8] = DIGIT_MUL(ad[4], ad[21]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]);
	rd[25] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[13], ad[13]);
	p[1] = DIGIT_MUL(ad[12], ad[14]);
	p[2] = DIGIT_MUL(ad[11], ad[15]);
	p[3] = DIGIT_MUL(ad[10], ad[16]);
	p[4] = DIGIT_MUL(ad[9], ad[17]);
	p[5] = DIGIT_MUL(ad[8], ad[18]);
	p[6] = DIGIT_MUL(ad[7], ad[19]);
	p[7] = DIGIT_MUL(ad[6], ad[20]);
	p[8] = DIGIT_MUL(ad[5], ad[21]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]);
	rd[26] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[13], ad[14]);
	p[1] = DIGIT_MUL(ad[12], ad[15]);
	p[2] = DIGIT_MUL(ad[11], ad[16]);
	p[3] = DIGIT_MUL(ad[10], ad[17]);
	p[4] = DIGIT_MUL(ad[9], ad[18]);
	p[5] = DIGIT_MUL(ad[8], ad[19]);
	p[6] = DIGIT_MUL(ad[7], ad[20]);
	p[7] = DIGIT_MUL(ad[6], ad[21]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]);
	rd[27] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[14], ad[14]);
	p[1] = DIGIT_MUL(ad[13], ad[15]);
	p[2] = DIGIT_MUL(ad[12], ad[16]);
	p[3] = DIGIT_MUL(ad[11], ad[17]);
	p[4] = DIGIT_MUL(ad[10], ad[18]);
	p[5] = DIGIT_MUL(ad[9], ad[19]);
	p[6] = DIGIT_MUL(ad[8], ad[20]);
	p[7] = DIGIT_MUL(ad[7], ad[21]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]);
	rd[28] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[14], ad[15]);
	p[1] = DIGIT_MUL(ad[13], ad[16]);
	p[2] = DIGIT_MUL(ad[12], ad[17]);
	p[3] = DIGIT_MUL(ad[11], ad[18]);
	p[4] = DIGIT_MUL(ad[10], ad[19]);
	p[5] = DIGIT_MUL(ad[9], ad[20]);
	p[6] = DIGIT_MUL(ad[8], ad[21]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]);
	rd[29] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[15], ad[15]);
	p[1] = DIGIT_MUL(ad[14], ad[16]);
	p[2] = DIGIT_MUL(ad[13], ad[17]);
	p[3] = DIGIT_MUL(ad[12], ad[18]);
	p[4] = DIGIT_MUL(ad[11], ad[19]);
	p[5] = DIGIT_MUL(ad[10], ad[20]);
	p[6] = DIGIT_MUL(ad[9], ad[21]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]);
	rd[30] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[15], ad[16]);
	p[1] = DIGIT_MUL(ad[14], ad[17]);
	p[2] = DIGIT_MUL(ad[13], ad[18]);
	p[3] = DIGIT_MUL(ad[12], ad[19]);
	p[4] = DIGIT_MUL(ad[11], ad[20]);
	p[5] = DIGIT_MUL(ad[10], ad[21]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]);
	rd[31] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[16], ad[16]);
	p[1] = DIGIT_MUL(ad[15], ad[17]);
	p[2] = DIGIT_MUL(ad[14], ad[18]);
	p[3] = DIGIT_MUL(ad[13], ad[19]);
	p[4] = DIGIT_MUL(ad[12], ad[20]);
	p[5] = DIGIT_MUL(ad[11], ad[21]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]);
	rd[32] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[16], ad[17]);
	p[1] = DIGIT_MUL(ad[15], ad[18]);
	p[2] = DIGIT_MUL(ad[14], ad[19]);
	p[3] = DIGIT_MUL(ad[13], ad[20]);
	p[4] = DIGIT_MUL(ad[12], ad[21]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]);
	rd[33] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[17], ad[17]);
	p[1] = DIGIT_MUL(ad[16], ad[18]);
	p[2] = DIGIT_MUL(ad[15], ad[19]);
	p[3] = DIGIT_MUL(ad[14], ad[20]);
	p[4] = DIGIT_MUL(ad[13], ad[21]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]);
	rd[34] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[17], ad[18]);
	p[1] = DIGIT_MUL(ad[16], ad[19]);
	p[2] = DIGIT_MUL(ad[15], ad[20]);
	p[3] = DIGIT_MUL(ad[14], ad[21]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[35] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[18], ad[18]);
	p[1] = DIGIT_MUL(ad[17], ad[19]);
	p[2] = DIGIT_MUL(ad[16], ad[20]);
	p[3] = DIGIT_MUL(ad[15], ad[21]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[36] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[18], ad[19]);
	p[1] = DIGIT_MUL(ad[17], ad[20]);
	p[2] = DIGIT_MUL(ad[16], ad[21]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[37] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[19], ad[19]);
	p[1] = DIGIT_MUL(ad[18], ad[20]);
	p[2] = DIGIT_MUL(ad[17], ad[21]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[38] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[19], ad[20]);
	p[1] = DIGIT_MUL(ad[18], ad[21]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]);
	rd[39] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[20], ad[20]);
	p[1] = DIGIT_MUL(ad[19], ad[21]);
	t = c + LO(p[0]) + 2 * LO(p[1]);
	rd[40] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[20], ad[21]);
	t = c + 2 * LO(p[0]);
	rd[41] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[21], ad[21]);
	t = c + LO(p[0]);
	rd[42] = (SslBigNumDigit)t;
	rd[43] = (SslBigNumDigit)(HI(p[0]) + (t >> 16));
}

/* r[0..31] = a[0..15] * b[0..15] */
void sslMultiplyBigNumWordsC16(SslBigNumWord *r, const SslBigNumWord *a,
				const SslBigNumWord *b)
{
	SslBigNumDigit *rd = (SslBigNumDigit *)r;
	const SslBigNumDigit *ad = (const SslBigNumDigit *)a;
	const SslBigNumDigit *bd = (const SslBigNumDigit *)b;
	SceUInt32 p[32];
	SceUInt32 c, t;

	p[0] = DIGIT_MUL(ad[0], bd[0]);
	t = LO(p[0]);
	rd[0] = (SslBigNumDigit)t;
	c = HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[1]);
	p[1] = DIGIT_MUL(ad[1], bd[0]);
	t = c + LO(p[0]) + LO(p[1]);
	rd[1] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[2]);
	p[1] = DIGIT_MUL(ad[2], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[1]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]);
	rd[2] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[3]);
	p[1] = DIGIT_MUL(ad[3], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[2]);
	p[3] = DIGIT_MUL(ad[2], bd[1]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]);
	rd[3] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[4]);
	p[1] = DIGIT_MUL(ad[4], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[3]);
	p[3] = DIGIT_MUL(ad[3], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[2]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]);
	rd[4] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[5]);
	p[1] = DIGIT_MUL(ad[5], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[4]);
	p[3] = DIGIT_MUL(ad[4], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[3]);
	p[5] = DIGIT_MUL(ad[3], bd[2]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]);
	rd[5] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[6]);
	p[1] = DIGIT_MUL(ad[6], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[5]);
	p[3] = DIGIT_MUL(ad[5], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[4]);
	p[5] = DIGIT_MUL(ad[4], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[3]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]);
	rd[6] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[7]);
	p[1] = DIGIT_MUL(ad[7], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[6]);
	p[3] = DIGIT_MUL(ad[6], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[5]);
	p[5] = DIGIT_MUL(ad[5], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[4]);
	p[7] = DIGIT_MUL(ad[4], bd[3]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]);
	rd[7] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[8]);
	p[1] = DIGIT_MUL(ad[8], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[7]);
	p[3] = DIGIT_MUL(ad[7], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[6]);
	p[5] = DIGIT_MUL(ad[6], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[5]);
	p[7] = DIGIT_MUL(ad[5], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[4]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]);
	rd[8] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[9]);
	p[1] = DIGIT_MUL(ad[9], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[8]);
	p[3] = DIGIT_MUL(ad[8], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[7]);
	p[5] = DIGIT_MUL(ad[7], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[6]);
	p[7] = DIGIT_MUL(ad[6], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[5]);
	p[9] = DIGIT_MUL(ad[5], bd[4]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]);
	rd[9] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[10]);
	p[1] = DIGIT_MUL(ad[10], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[9]);
	p[3] = DIGIT_MUL(ad[9], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[8]);
	p[5] = DIGIT_MUL(ad[8], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[7]);
	p[7] = DIGIT_MUL(ad[7], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[6]);
	p[9] = DIGIT_MUL(ad[6], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[5]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]);
	rd[10] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[11]);
	p[1] = DIGIT_MUL(ad[11], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[10]);
	p[3] = DIGIT_MUL(ad[10], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[9]);
	p[5] = DIGIT_MUL(ad[9], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[8]);
	p[7] = DIGIT_MUL(ad[8], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[7]);
	p[9] = DIGIT_MUL(ad[7], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[6]);
	p[11] = DIGIT_MUL(ad[6], bd[5]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]);
	rd[11] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[12]);
	p[1] = DIGIT_MUL(ad[12], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[11]);
	p[3] = DIGIT_MUL(ad[11], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[10]);
	p[5] = DIGIT_MUL(ad[10], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[9]);
	p[7] = DIGIT_MUL(ad[9], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[8]);
	p[9] = DIGIT_MUL(ad[8], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[7]);
	p[11] = DIGIT_MUL(ad[7], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[6]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]);
	rd[12] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[13]);
	p[1] = DIGIT_MUL(ad[13], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[12]);
	p[3] = DIGIT_MUL(ad[12], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[11]);
	p[5] = DIGIT_MUL(ad[11], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[10]);
	p[7] = DIGIT_MUL(ad[10], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[9]);
	p[9] = DIGIT_MUL(ad[9], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[8]);
	p[11] = DIGIT_MUL(ad[8], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[7]);
	p[13] = DIGIT_MUL(ad[7], bd[6]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]);
	rd[13] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[14]);
	p[1] = DIGIT_MUL(ad[14], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[13]);
	p[3] = DIGIT_MUL(ad[13], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[12]);
	p[5] = DIGIT_MUL(ad[12], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[11]);
	p[7] = DIGIT_MUL(ad[11], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[10]);
	p[9] = DIGIT_MUL(ad[10], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[9]);
	p[11] = DIGIT_MUL(ad[9], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[8]);
	p[13] = DIGIT_MUL(ad[8], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[7]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]);
	rd[14] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[15]);
	p[1] = DIGIT_MUL(ad[15], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[14]);
	p[3] = DIGIT_MUL(ad[14], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[13]);
	p[5] = DIGIT_MUL(ad[13], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[12]);
	p[7] = DIGIT_MUL(ad[12], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[11]);
	p[9] = DIGIT_MUL(ad[11], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[10]);
	p[11] = DIGIT_MUL(ad[10], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[9]);
	p[13] = DIGIT_MUL(ad[9], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[8]);
	p[15] = DIGIT_MUL(ad[8], bd[7]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]);
	rd[15] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[16]);
	p[1] = DIGIT_MUL(ad[16], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[15]);
	p[3] = DIGIT_MUL(ad[15], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[14]);
	p[5] = DIGIT_MUL(ad[14], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[13]);
	p[7] = DIGIT_MUL(ad[13], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[12]);
	p[9] = DIGIT_MUL(ad[12], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[11]);
	p[11] = DIGIT_MUL(ad[11], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[10]);
	p[13] = DIGIT_MUL(ad[10], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[9]);
	p[15] = DIGIT_MUL(ad[9], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[8]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]);
	rd[16] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[17]);
	p[1] = DIGIT_MUL(ad[17], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[16]);
	p[3] = DIGIT_MUL(ad[16], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[15]);
	p[5] = DIGIT_MUL(ad[15], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[14]);
	p[7] = DIGIT_MUL(ad[14], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[13]);
	p[9] = DIGIT_MUL(ad[13], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[12]);
	p[11] = DIGIT_MUL(ad[12], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[11]);
	p[13] = DIGIT_MUL(ad[11], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[10]);
	p[15] = DIGIT_MUL(ad[10], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[9]);
	p[17] = DIGIT_MUL(ad[9], bd[8]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]);
	rd[17] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[18]);
	p[1] = DIGIT_MUL(ad[18], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[17]);
	p[3] = DIGIT_MUL(ad[17], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[16]);
	p[5] = DIGIT_MUL(ad[16], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[15]);
	p[7] = DIGIT_MUL(ad[15], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[14]);
	p[9] = DIGIT_MUL(ad[14], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[13]);
	p[11] = DIGIT_MUL(ad[13], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[12]);
	p[13] = DIGIT_MUL(ad[12], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[11]);
	p[15] = DIGIT_MUL(ad[11], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[10]);
	p[17] = DIGIT_MUL(ad[10], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[9]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]);
	rd[18] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[19]);
	p[1] = DIGIT_MUL(ad[19], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[18]);
	p[3] = DIGIT_MUL(ad[18], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[17]);
	p[5] = DIGIT_MUL(ad[17], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[16]);
	p[7] = DIGIT_MUL(ad[16], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[15]);
	p[9] = DIGIT_MUL(ad[15], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[14]);
	p[11] = DIGIT_MUL(ad[14], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[13]);
	p[13] = DIGIT_MUL(ad[13], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[12]);
	p[15] = DIGIT_MUL(ad[12], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[11]);
	p[17] = DIGIT_MUL(ad[11], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[10]);
	p[19] = DIGIT_MUL(ad[10], bd[9]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]);
	rd[19] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[20]);
	p[1] = DIGIT_MUL(ad[20], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[19]);
	p[3] = DIGIT_MUL(ad[19], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[18]);
	p[5] = DIGIT_MUL(ad[18], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[17]);
	p[7] = DIGIT_MUL(ad[17], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[16]);
	p[9] = DIGIT_MUL(ad[16], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[15]);
	p[11] = DIGIT_MUL(ad[15], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[14]);
	p[13] = DIGIT_MUL(ad[14], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[13]);
	p[15] = DIGIT_MUL(ad[13], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[12]);
	p[17] = DIGIT_MUL(ad[12], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[11]);
	p[19] = DIGIT_MUL(ad[11], bd[9]);
	p[20] = DIGIT_MUL(ad[10], bd[10]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]);
	rd[20] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[21]);
	p[1] = DIGIT_MUL(ad[21], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[20]);
	p[3] = DIGIT_MUL(ad[20], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[19]);
	p[5] = DIGIT_MUL(ad[19], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[18]);
	p[7] = DIGIT_MUL(ad[18], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[17]);
	p[9] = DIGIT_MUL(ad[17], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[16]);
	p[11] = DIGIT_MUL(ad[16], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[15]);
	p[13] = DIGIT_MUL(ad[15], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[14]);
	p[15] = DIGIT_MUL(ad[14], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[13]);
	p[17] = DIGIT_MUL(ad[13], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[12]);
	p[19] = DIGIT_MUL(ad[12], bd[9]);
	p[20] = DIGIT_MUL(ad[10], bd[11]);
	p[21] = DIGIT_MUL(ad[11], bd[10]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]);
	rd[21] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[22]);
	p[1] = DIGIT_MUL(ad[22], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[21]);
	p[3] = DIGIT_MUL(ad[21], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[20]);
	p[5] = DIGIT_MUL(ad[20], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[19]);
	p[7] = DIGIT_MUL(ad[19], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[18]);
	p[9] = DIGIT_MUL(ad[18], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[17]);
	p[11] = DIGIT_MUL(ad[17], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[16]);
	p[13] = DIGIT_MUL(ad[16], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[15]);
	p[15] = DIGIT_MUL(ad[15], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[14]);
	p[17] = DIGIT_MUL(ad[14], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[13]);
	p[19] = DIGIT_MUL(ad[13], bd[9]);
	p[20] = DIGIT_MUL(ad[10], bd[12]);
	p[21] = DIGIT_MUL(ad[12], bd[10]);
	p[22] = DIGIT_MUL(ad[11], bd[11]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]);
	rd[22] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[23]);
	p[1] = DIGIT_MUL(ad[23], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[22]);
	p[3] = DIGIT_MUL(ad[22], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[21]);
	p[5] = DIGIT_MUL(ad[21], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[20]);
	p[7] = DIGIT_MUL(ad[20], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[19]);
	p[9] = DIGIT_MUL(ad[19], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[18]);
	p[11] = DIGIT_MUL(ad[18], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[17]);
	p[13] = DIGIT_MUL(ad[17], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[16]);
	p[15] = DIGIT_MUL(ad[16], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[15]);
	p[17] = DIGIT_MUL(ad[15], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[14]);
	p[19] = DIGIT_MUL(ad[14], bd[9]);
	p[20] = DIGIT_MUL(ad[10], bd[13]);
	p[21] = DIGIT_MUL(ad[13], bd[10]);
	p[22] = DIGIT_MUL(ad[11], bd[12]);
	p[23] = DIGIT_MUL(ad[12], bd[11]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]);
	rd[23] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[24]);
	p[1] = DIGIT_MUL(ad[24], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[23]);
	p[3] = DIGIT_MUL(ad[23], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[22]);
	p[5] = DIGIT_MUL(ad[22], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[21]);
	p[7] = DIGIT_MUL(ad[21], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[20]);
	p[9] = DIGIT_MUL(ad[20], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[19]);
	p[11] = DIGIT_MUL(ad[19], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[18]);
	p[13] = DIGIT_MUL(ad[18], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[17]);
	p[15] = DIGIT_MUL(ad[17], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[16]);
	p[17] = DIGIT_MUL(ad[16], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[15]);
	p[19] = DIGIT_MUL(ad[15], bd[9]);
	p[20] = DIGIT_MUL(ad[10], bd[14]);
	p[21] = DIGIT_MUL(ad[14], bd[10]);
	p[22] = DIGIT_MUL(ad[11], bd[13]);
	p[23] = DIGIT_MUL(ad[13], bd[11]);
	p[24] = DIGIT_MUL(ad[12], bd[12]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]) + LO(p[24]);
	rd[24] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + HI(p[24]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[25]);
	p[1] = DIGIT_MUL(ad[25], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[24]);
	p[3] = DIGIT_MUL(ad[24], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[23]);
	p[5] = DIGIT_MUL(ad[23], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[22]);
	p[7] = DIGIT_MUL(ad[22], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[21]);
	p[9] = DIGIT_MUL(ad[21], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[20]);
	p[11] = DIGIT_MUL(ad[20], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[19]);
	p[13] = DIGIT_MUL(ad[19], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[18]);
	p[15] = DIGIT_MUL(ad[18], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[17]);
	p[17] = DIGIT_MUL(ad[17], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[16]);
	p[19] = DIGIT_MUL(ad[16], bd[9]);
	p[20] = DIGIT_MUL(ad[10], bd[15]);
	p[21] = DIGIT_MUL(ad[15], bd[10]);
	p[22] = DIGIT_MUL(ad[11], bd[14]);
	p[23] = DIGIT_MUL(ad[14], bd[11]);
	p[24] = DIGIT_MUL(ad[12], bd[13]);
	p[25] = DIGIT_MUL(ad[13], bd[12]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]) + LO(p[24]) + LO(p[25]);
	rd[25] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + HI(p[24]) + HI(p[25]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[26]);
	p[1] = DIGIT_MUL(ad[26], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[25]);
	p[3] = DIGIT_MUL(ad[25], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[24]);
	p[5] = DIGIT_MUL(ad[24], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[23]);
	p[7] = DIGIT_MUL(ad[23], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[22]);
	p[9] = DIGIT_MUL(ad[22], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[21]);
	p[11] = DIGIT_MUL(ad[21], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[20]);
	p[13] = DIGIT_MUL(ad[20], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[19]);
	p[15] = DIGIT_MUL(ad[19], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[18]);
	p[17] = DIGIT_MUL(ad[18], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[17]);
	p[19] = DIGIT_MUL(ad[17], bd[9]);
	p[20] = DIGIT_MUL(ad[10], bd[16]);
	p[21] = DIGIT_MUL(ad[16], bd[10]);
	p[22] = DIGIT_MUL(ad[11], bd[15]);
	p[23] = DIGIT_MUL(ad[15], bd[11]);
	p[24] = DIGIT_MUL(ad[12], bd[14]);
	p[25] = DIGIT_MUL(ad[14], bd[12]);
	p[26] = DIGIT_MUL(ad[13], bd[13]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]) + LO(p[24]) + LO(p[25]) +
	    LO(p[26]);
	rd[26] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + HI(p[24]) + HI(p[25]) + HI(p[26]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[27]);
	p[1] = DIGIT_MUL(ad[27], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[26]);
	p[3] = DIGIT_MUL(ad[26], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[25]);
	p[5] = DIGIT_MUL(ad[25], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[24]);
	p[7] = DIGIT_MUL(ad[24], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[23]);
	p[9] = DIGIT_MUL(ad[23], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[22]);
	p[11] = DIGIT_MUL(ad[22], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[21]);
	p[13] = DIGIT_MUL(ad[21], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[20]);
	p[15] = DIGIT_MUL(ad[20], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[19]);
	p[17] = DIGIT_MUL(ad[19], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[18]);
	p[19] = DIGIT_MUL(ad[18], bd[9]);
	p[20] = DIGIT_MUL(ad[10], bd[17]);
	p[21] = DIGIT_MUL(ad[17], bd[10]);
	p[22] = DIGIT_MUL(ad[11], bd[16]);
	p[23] = DIGIT_MUL(ad[16], bd[11]);
	p[24] = DIGIT_MUL(ad[12], bd[15]);
	p[25] = DIGIT_MUL(ad[15], bd[12]);
	p[26] = DIGIT_MUL(ad[13], bd[14]);
	p[27] = DIGIT_MUL(ad[14], bd[13]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]) + LO(p[24]) + LO(p[25]) +
	    LO(p[26]) + LO(p[27]);
	rd[27] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + HI(p[24]) + HI(p[25]) + HI(p[26]) +
	    HI(p[27]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[28]);
	p[1] = DIGIT_MUL(ad[28], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[27]);
	p[3] = DIGIT_MUL(ad[27], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[26]);
	p[5] = DIGIT_MUL(ad[26], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[25]);
	p[7] = DIGIT_MUL(ad[25], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[24]);
	p[9] = DIGIT_MUL(ad[24], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[23]);
	p[11] = DIGIT_MUL(ad[23], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[22]);
	p[13] = DIGIT_MUL(ad[22], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[21]);
	p[15] = DIGIT_MUL(ad[21], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[20]);
	p[17] = DIGIT_MUL(ad[20], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[19]);
	p[19] = DIGIT_MUL(ad[19], bd[9]);
	p[20] = DIGIT_MUL(ad[10], bd[18]);
	p[21] = DIGIT_MUL(ad[18], bd[10]);
	p[22] = DIGIT_MUL(ad[11], bd[17]);
	p[23] = DIGIT_MUL(ad[17], bd[11]);
	p[24] = DIGIT_MUL(ad[12], bd[16]);
	p[25] = DIGIT_MUL(ad[16], bd[12]);
	p[26] = DIGIT_MUL(ad[13], bd[15]);
	p[27] = DIGIT_MUL(ad[15], bd[13]);
	p[28] = DIGIT_MUL(ad[14], bd[14]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]) + LO(p[24]) + LO(p[25]) +
	    LO(p[26]) + LO(p[27]) + LO(p[28]);
	rd[28] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + HI(p[24]) + HI(p[25]) + HI(p[26]) +
	    HI(p[27]) + HI(p[28]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[29]);
	p[1] = DIGIT_MUL(ad[29], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[28]);
	p[3] = DIGIT_MUL(ad[28], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[27]);
	p[5] = DIGIT_MUL(ad[27], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[26]);
	p[7] = DIGIT_MUL(ad[26], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[25]);
	p[9] = DIGIT_MUL(ad[25], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[24]);
	p[11] = DIGIT_MUL(ad[24], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[23]);
	p[13] = DIGIT_MUL(ad[23], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[22]);
	p[15] = DIGIT_MUL(ad[22], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[21]);
	p[17] = DIGIT_MUL(ad[21], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[20]);
	p[19] = DIGIT_MUL(ad[20], bd[9]);
	p[20] = DIGIT_MUL(ad[10], bd[19]);
	p[21] = DIGIT_MUL(ad[19], bd[10]);
	p[22] = DIGIT_MUL(ad[11], bd[18]);
	p[23] = DIGIT_MUL(ad[18], bd[11]);
	p[24] = DIGIT_MUL(ad[12], bd[17]);
	p[25] = DIGIT_MUL(ad[17], bd[12]);
	p[26] = DIGIT_MUL(ad[13], bd[16]);
	p[27] = DIGIT_MUL(ad[16], bd[13]);
	p[28] = DIGIT_MUL(ad[14], bd[15]);
	p[29] = DIGIT_MUL(ad[15], bd[14]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]) + LO(p[24]) + LO(p[25]) +
	    LO(p[26]) + LO(p[27]) + LO(p[28]) + LO(p[29]);
	rd[29] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + HI(p[24]) + HI(p[25]) + HI(p[26]) +
	    HI(p[27]) + HI(p[28]) + HI(p[29]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[30]);
	p[1] = DIGIT_MUL(ad[30], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[29]);
	p[3] = DIGIT_MUL(ad[29], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[28]);
	p[5] = DIGIT_MUL(ad[28], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[27]);
	p[7] = DIGIT_MUL(ad[27], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[26]);
	p[9] = DIGIT_MUL(ad[26], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[25]);
	p[11] = DIGIT_MUL(ad[25], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[24]);
	p[13] = DIGIT_MUL(ad[24], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[23]);
	p[15] = DIGIT_MUL(ad[23], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[22]);
	p[17] = DIGIT_MUL(ad[22], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[21]);
	p[19] = DIGIT_MUL(ad[21], bd[9]);
	p[20] = DIGIT_MUL(ad[10], bd[20]);
	p[21] = DIGIT_MUL(ad[20], bd[10]);
	p[22] = DIGIT_MUL(ad[11], bd[19]);
	p[23] = DIGIT_MUL(ad[19], bd[11]);
	p[24] = DIGIT_MUL(ad[12], bd[18]);
	p[25] = DIGIT_MUL(ad[18], bd[12]);
	p[26] = DIGIT_MUL(ad[13], bd[17]);
	p[27] = DIGIT_MUL(ad[17], bd[13]);
	p[28] = DIGIT_MUL(ad[14], bd[16]);
	p[29] = DIGIT_MUL(ad[16], bd[14]);
	p[30] = DIGIT_MUL(ad[15], bd[15]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]) + LO(p[24]) + LO(p[25]) +
	    LO(p[26]) + LO(p[27]) + LO(p[28]) + LO(p[29]) + LO(p[30]);
	rd[30] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + HI(p[24]) + HI(p[25]) + HI(p[26]) +
	    HI(p[27]) + HI(p[28]) + HI(p[29]) + HI(p[30]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[0]);
	p[2] = DIGIT_MUL(ad[1], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[1]);
	p[4] = DIGIT_MUL(ad[2], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[2]);
	p[6] = DIGIT_MUL(ad[3], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[3]);
	p[8] = DIGIT_MUL(ad[4], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[4]);
	p[10] = DIGIT_MUL(ad[5], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[5]);
	p[12] = DIGIT_MUL(ad[6], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[6]);
	p[14] = DIGIT_MUL(ad[7], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[7]);
	p[16] = DIGIT_MUL(ad[8], bd[23]);
	p[17] = DIGIT_MUL(ad[23], bd[8]);
	p[18] = DIGIT_MUL(ad[9], bd[22]);
	p[19] = DIGIT_MUL(ad[22], bd[9]);
	p[20] = DIGIT_MUL(ad[10], bd[21]);
	p[21] = DIGIT_MUL(ad[21], bd[10]);
	p[22] = DIGIT_MUL(ad[11], bd[20]);
	p[23] = DIGIT_MUL(ad[20], bd[11]);
	p[24] = DIGIT_MUL(ad[12], bd[19]);
	p[25] = DIGIT_MUL(ad[19], bd[12]);
	p[26] = DIGIT_MUL(ad[13], bd[18]);
	p[27] = DIGIT_MUL(ad[18], bd[13]);
	p[28] = DIGIT_MUL(ad[14], bd[17]);
	p[29] = DIGIT_MUL(ad[17], bd[14]);
	p[30] = DIGIT_MUL(ad[15], bd[16]);
	p[31] = DIGIT_MUL(ad[16], bd[15]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]) + LO(p[24]) + LO(p[25]) +
	    LO(p[26]) + LO(p[27]) + LO(p[28]) + LO(p[29]) + LO(p[30]) +
	    LO(p[31]);
	rd[31] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + HI(p[24]) + HI(p[25]) + HI(p[26]) +
	    HI(p[27]) + HI(p[28]) + HI(p[29]) + HI(p[30]) + HI(p[31]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[1], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[1]);
	p[2] = DIGIT_MUL(ad[2], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[2]);
	p[4] = DIGIT_MUL(ad[3], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[3]);
	p[6] = DIGIT_MUL(ad[4], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[4]);
	p[8] = DIGIT_MUL(ad[5], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[5]);
	p[10] = DIGIT_MUL(ad[6], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[6]);
	p[12] = DIGIT_MUL(ad[7], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[7]);
	p[14] = DIGIT_MUL(ad[8], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[8]);
	p[16] = DIGIT_MUL(ad[9], bd[23]);
	p[17] = DIGIT_MUL(ad[23], bd[9]);
	p[18] = DIGIT_MUL(ad[10], bd[22]);
	p[19] = DIGIT_MUL(ad[22], bd[10]);
	p[20] = DIGIT_MUL(ad[11], bd[21]);
	p[21] = DIGIT_MUL(ad[21], bd[11]);
	p[22] = DIGIT_MUL(ad[12], bd[20]);
	p[23] = DIGIT_MUL(ad[20], bd[12]);
	p[24] = DIGIT_MUL(ad[13], bd[19]);
	p[25] = DIGIT_MUL(ad[19], bd[13]);
	p[26] = DIGIT_MUL(ad[14], bd[18]);
	p[27] = DIGIT_MUL(ad[18], bd[14]);
	p[28] = DIGIT_MUL(ad[15], bd[17]);
	p[29] = DIGIT_MUL(ad[17], bd[15]);
	p[30] = DIGIT_MUL(ad[16], bd[16]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]) + LO(p[24]) + LO(p[25]) +
	    LO(p[26]) + LO(p[27]) + LO(p[28]) + LO(p[29]) + LO(p[30]);
	rd[32] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + HI(p[24]) + HI(p[25]) + HI(p[26]) +
	    HI(p[27]) + HI(p[28]) + HI(p[29]) + HI(p[30]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[2], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[2]);
	p[2] = DIGIT_MUL(ad[3], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[3]);
	p[4] = DIGIT_MUL(ad[4], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[4]);
	p[6] = DIGIT_MUL(ad[5], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[5]);
	p[8] = DIGIT_MUL(ad[6], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[6]);
	p[10] = DIGIT_MUL(ad[7], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[7]);
	p[12] = DIGIT_MUL(ad[8], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[8]);
	p[14] = DIGIT_MUL(ad[9], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[9]);
	p[16] = DIGIT_MUL(ad[10], bd[23]);
	p[17] = DIGIT_MUL(ad[23], bd[10]);
	p[18] = DIGIT_MUL(ad[11], bd[22]);
	p[19] = DIGIT_MUL(ad[22], bd[11]);
	p[20] = DIGIT_MUL(ad[12], bd[21]);
	p[21] = DIGIT_MUL(ad[21], bd[12]);
	p[22] = DIGIT_MUL(ad[13], bd[20]);
	p[23] = DIGIT_MUL(ad[20], bd[13]);
	p[24] = DIGIT_MUL(ad[14], bd[19]);
	p[25] = DIGIT_MUL(ad[19], bd[14]);
	p[26] = DIGIT_MUL(ad[15], bd[18]);
	p[27] = DIGIT_MUL(ad[18], bd[15]);
	p[28] = DIGIT_MUL(ad[16], bd[17]);
	p[29] = DIGIT_MUL(ad[17], bd[16]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]) + LO(p[24]) + LO(p[25]) +
	    LO(p[26]) + LO(p[27]) + LO(p[28]) + LO(p[29]);
	rd[33] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + HI(p[24]) + HI(p[25]) + HI(p[26]) +
	    HI(p[27]) + HI(p[28]) + HI(p[29]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[3], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[3]);
	p[2] = DIGIT_MUL(ad[4], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[4]);
	p[4] = DIGIT_MUL(ad[5], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[5]);
	p[6] = DIGIT_MUL(ad[6], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[6]);
	p[8] = DIGIT_MUL(ad[7], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[7]);
	p[10] = DIGIT_MUL(ad[8], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[8]);
	p[12] = DIGIT_MUL(ad[9], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[9]);
	p[14] = DIGIT_MUL(ad[10], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[10]);
	p[16] = DIGIT_MUL(ad[11], bd[23]);
	p[17] = DIGIT_MUL(ad[23], bd[11]);
	p[18] = DIGIT_MUL(ad[12], bd[22]);
	p[19] = DIGIT_MUL(ad[22], bd[12]);
	p[20] = DIGIT_MUL(ad[13], bd[21]);
	p[21] = DIGIT_MUL(ad[21], bd[13]);
	p[22] = DIGIT_MUL(ad[14], bd[20]);
	p[23] = DIGIT_MUL(ad[20], bd[14]);
	p[24] = DIGIT_MUL(ad[15], bd[19]);
	p[25] = DIGIT_MUL(ad[19], bd[15]);
	p[26] = DIGIT_MUL(ad[16], bd[18]);
	p[27] = DIGIT_MUL(ad[18], bd[16]);
	p[28] = DIGIT_MUL(ad[17], bd[17]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]) + LO(p[24]) + LO(p[25]) +
	    LO(p[26]) + LO(p[27]) + LO(p[28]);
	rd[34] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + HI(p[24]) + HI(p[25]) + HI(p[26]) +
	    HI(p[27]) + HI(p[28]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[4], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[4]);
	p[2] = DIGIT_MUL(ad[5], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[5]);
	p[4] = DIGIT_MUL(ad[6], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[6]);
	p[6] = DIGIT_MUL(ad[7], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[7]);
	p[8] = DIGIT_MUL(ad[8], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[8]);
	p[10] = DIGIT_MUL(ad[9], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[9]);
	p[12] = DIGIT_MUL(ad[10], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[10]);
	p[14] = DIGIT_MUL(ad[11], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[11]);
	p[16] = DIGIT_MUL(ad[12], bd[23]);
	p[17] = DIGIT_MUL(ad[23], bd[12]);
	p[18] = DIGIT_MUL(ad[13], bd[22]);
	p[19] = DIGIT_MUL(ad[22], bd[13]);
	p[20] = DIGIT_MUL(ad[14], bd[21]);
	p[21] = DIGIT_MUL(ad[21], bd[14]);
	p[22] = DIGIT_MUL(ad[15], bd[20]);
	p[23] = DIGIT_MUL(ad[20], bd[15]);
	p[24] = DIGIT_MUL(ad[16], bd[19]);
	p[25] = DIGIT_MUL(ad[19], bd[16]);
	p[26] = DIGIT_MUL(ad[17], bd[18]);
	p[27] = DIGIT_MUL(ad[18], bd[17]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]) + LO(p[24]) + LO(p[25]) +
	    LO(p[26]) + LO(p[27]);
	rd[35] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + HI(p[24]) + HI(p[25]) + HI(p[26]) +
	    HI(p[27]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[5], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[5]);
	p[2] = DIGIT_MUL(ad[6], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[6]);
	p[4] = DIGIT_MUL(ad[7], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[7]);
	p[6] = DIGIT_MUL(ad[8], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[8]);
	p[8] = DIGIT_MUL(ad[9], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[9]);
	p[10] = DIGIT_MUL(ad[10], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[10]);
	p[12] = DIGIT_MUL(ad[11], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[11]);
	p[14] = DIGIT_MUL(ad[12], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[12]);
	p[16] = DIGIT_MUL(ad[13], bd[23]);
	p[17] = DIGIT_MUL(ad[23], bd[13]);
	p[18] = DIGIT_MUL(ad[14], bd[22]);
	p[19] = DIGIT_MUL(ad[22], bd[14]);
	p[20] = DIGIT_MUL(ad[15], bd[21]);
	p[21] = DIGIT_MUL(ad[21], bd[15]);
	p[22] = DIGIT_MUL(ad[16], bd[20]);
	p[23] = DIGIT_MUL(ad[20], bd[16]);
	p[24] = DIGIT_MUL(ad[17], bd[19]);
	p[25] = DIGIT_MUL(ad[19], bd[17]);
	p[26] = DIGIT_MUL(ad[18], bd[18]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]) + LO(p[24]) + LO(p[25]) +
	    LO(p[26]);
	rd[36] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + HI(p[24]) + HI(p[25]) + HI(p[26]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[6], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[6]);
	p[2] = DIGIT_MUL(ad[7], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[7]);
	p[4] = DIGIT_MUL(ad[8], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[8]);
	p[6] = DIGIT_MUL(ad[9], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[9]);
	p[8] = DIGIT_MUL(ad[10], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[10]);
	p[10] = DIGIT_MUL(ad[11], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[11]);
	p[12] = DIGIT_MUL(ad[12], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[12]);
	p[14] = DIGIT_MUL(ad[13], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[13]);
	p[16] = DIGIT_MUL(ad[14], bd[23]);
	p[17] = DIGIT_MUL(ad[23], bd[14]);
	p[18] = DIGIT_MUL(ad[15], bd[22]);
	p[19] = DIGIT_MUL(ad[22], bd[15]);
	p[20] = DIGIT_MUL(ad[16], bd[21]);
	p[21] = DIGIT_MUL(ad[21], bd[16]);
	p[22] = DIGIT_MUL(ad[17], bd[20]);
	p[23] = DIGIT_MUL(ad[20], bd[17]);
	p[24] = DIGIT_MUL(ad[18], bd[19]);
	p[25] = DIGIT_MUL(ad[19], bd[18]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]) + LO(p[24]) + LO(p[25]);
	rd[37] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + HI(p[24]) + HI(p[25]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[7], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[7]);
	p[2] = DIGIT_MUL(ad[8], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[8]);
	p[4] = DIGIT_MUL(ad[9], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[9]);
	p[6] = DIGIT_MUL(ad[10], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[10]);
	p[8] = DIGIT_MUL(ad[11], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[11]);
	p[10] = DIGIT_MUL(ad[12], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[12]);
	p[12] = DIGIT_MUL(ad[13], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[13]);
	p[14] = DIGIT_MUL(ad[14], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[14]);
	p[16] = DIGIT_MUL(ad[15], bd[23]);
	p[17] = DIGIT_MUL(ad[23], bd[15]);
	p[18] = DIGIT_MUL(ad[16], bd[22]);
	p[19] = DIGIT_MUL(ad[22], bd[16]);
	p[20] = DIGIT_MUL(ad[17], bd[21]);
	p[21] = DIGIT_MUL(ad[21], bd[17]);
	p[22] = DIGIT_MUL(ad[18], bd[20]);
	p[23] = DIGIT_MUL(ad[20], bd[18]);
	p[24] = DIGIT_MUL(ad[19], bd[19]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]) + LO(p[24]);
	rd[38] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + HI(p[24]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[8], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[8]);
	p[2] = DIGIT_MUL(ad[9], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[9]);
	p[4] = DIGIT_MUL(ad[10], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[10]);
	p[6] = DIGIT_MUL(ad[11], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[11]);
	p[8] = DIGIT_MUL(ad[12], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[12]);
	p[10] = DIGIT_MUL(ad[13], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[13]);
	p[12] = DIGIT_MUL(ad[14], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[14]);
	p[14] = DIGIT_MUL(ad[15], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[15]);
	p[16] = DIGIT_MUL(ad[16], bd[23]);
	p[17] = DIGIT_MUL(ad[23], bd[16]);
	p[18] = DIGIT_MUL(ad[17], bd[22]);
	p[19] = DIGIT_MUL(ad[22], bd[17]);
	p[20] = DIGIT_MUL(ad[18], bd[21]);
	p[21] = DIGIT_MUL(ad[21], bd[18]);
	p[22] = DIGIT_MUL(ad[19], bd[20]);
	p[23] = DIGIT_MUL(ad[20], bd[19]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]) + LO(p[23]);
	rd[39] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + HI(p[23]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[9], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[9]);
	p[2] = DIGIT_MUL(ad[10], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[10]);
	p[4] = DIGIT_MUL(ad[11], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[11]);
	p[6] = DIGIT_MUL(ad[12], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[12]);
	p[8] = DIGIT_MUL(ad[13], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[13]);
	p[10] = DIGIT_MUL(ad[14], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[14]);
	p[12] = DIGIT_MUL(ad[15], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[15]);
	p[14] = DIGIT_MUL(ad[16], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[16]);
	p[16] = DIGIT_MUL(ad[17], bd[23]);
	p[17] = DIGIT_MUL(ad[23], bd[17]);
	p[18] = DIGIT_MUL(ad[18], bd[22]);
	p[19] = DIGIT_MUL(ad[22], bd[18]);
	p[20] = DIGIT_MUL(ad[19], bd[21]);
	p[21] = DIGIT_MUL(ad[21], bd[19]);
	p[22] = DIGIT_MUL(ad[20], bd[20]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]) + LO(p[22]);
	rd[40] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    HI(p[22]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[10], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[10]);
	p[2] = DIGIT_MUL(ad[11], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[11]);
	p[4] = DIGIT_MUL(ad[12], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[12]);
	p[6] = DIGIT_MUL(ad[13], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[13]);
	p[8] = DIGIT_MUL(ad[14], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[14]);
	p[10] = DIGIT_MUL(ad[15], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[15]);
	p[12] = DIGIT_MUL(ad[16], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[16]);
	p[14] = DIGIT_MUL(ad[17], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[17]);
	p[16] = DIGIT_MUL(ad[18], bd[23]);
	p[17] = DIGIT_MUL(ad[23], bd[18]);
	p[18] = DIGIT_MUL(ad[19], bd[22]);
	p[19] = DIGIT_MUL(ad[22], bd[19]);
	p[20] = DIGIT_MUL(ad[20], bd[21]);
	p[21] = DIGIT_MUL(ad[21], bd[20]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]) +
	    LO(p[21]);
	rd[41] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + HI(p[21]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[11], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[11]);
	p[2] = DIGIT_MUL(ad[12], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[12]);
	p[4] = DIGIT_MUL(ad[13], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[13]);
	p[6] = DIGIT_MUL(ad[14], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[14]);
	p[8] = DIGIT_MUL(ad[15], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[15]);
	p[10] = DIGIT_MUL(ad[16], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[16]);
	p[12] = DIGIT_MUL(ad[17], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[17]);
	p[14] = DIGIT_MUL(ad[18], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[18]);
	p[16] = DIGIT_MUL(ad[19], bd[23]);
	p[17] = DIGIT_MUL(ad[23], bd[19]);
	p[18] = DIGIT_MUL(ad[20], bd[22]);
	p[19] = DIGIT_MUL(ad[22], bd[20]);
	p[20] = DIGIT_MUL(ad[21], bd[21]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]) + LO(p[20]);
	rd[42] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + HI(p[20]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[12], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[12]);
	p[2] = DIGIT_MUL(ad[13], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[13]);
	p[4] = DIGIT_MUL(ad[14], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[14]);
	p[6] = DIGIT_MUL(ad[15], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[15]);
	p[8] = DIGIT_MUL(ad[16], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[16]);
	p[10] = DIGIT_MUL(ad[17], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[17]);
	p[12] = DIGIT_MUL(ad[18], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[18]);
	p[14] = DIGIT_MUL(ad[19], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[19]);
	p[16] = DIGIT_MUL(ad[20], bd[23]);
	p[17] = DIGIT_MUL(ad[23], bd[20]);
	p[18] = DIGIT_MUL(ad[21], bd[22]);
	p[19] = DIGIT_MUL(ad[22], bd[21]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]) + LO(p[19]);
	rd[43] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + HI(p[19]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[13], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[13]);
	p[2] = DIGIT_MUL(ad[14], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[14]);
	p[4] = DIGIT_MUL(ad[15], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[15]);
	p[6] = DIGIT_MUL(ad[16], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[16]);
	p[8] = DIGIT_MUL(ad[17], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[17]);
	p[10] = DIGIT_MUL(ad[18], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[18]);
	p[12] = DIGIT_MUL(ad[19], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[19]);
	p[14] = DIGIT_MUL(ad[20], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[20]);
	p[16] = DIGIT_MUL(ad[21], bd[23]);
	p[17] = DIGIT_MUL(ad[23], bd[21]);
	p[18] = DIGIT_MUL(ad[22], bd[22]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]) + LO(p[18]);
	rd[44] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + HI(p[18]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[14], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[14]);
	p[2] = DIGIT_MUL(ad[15], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[15]);
	p[4] = DIGIT_MUL(ad[16], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[16]);
	p[6] = DIGIT_MUL(ad[17], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[17]);
	p[8] = DIGIT_MUL(ad[18], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[18]);
	p[10] = DIGIT_MUL(ad[19], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[19]);
	p[12] = DIGIT_MUL(ad[20], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[20]);
	p[14] = DIGIT_MUL(ad[21], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[21]);
	p[16] = DIGIT_MUL(ad[22], bd[23]);
	p[17] = DIGIT_MUL(ad[23], bd[22]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]) + LO(p[17]);
	rd[45] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    HI(p[17]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[15], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[15]);
	p[2] = DIGIT_MUL(ad[16], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[16]);
	p[4] = DIGIT_MUL(ad[17], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[17]);
	p[6] = DIGIT_MUL(ad[18], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[18]);
	p[8] = DIGIT_MUL(ad[19], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[19]);
	p[10] = DIGIT_MUL(ad[20], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[20]);
	p[12] = DIGIT_MUL(ad[21], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[21]);
	p[14] = DIGIT_MUL(ad[22], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[22]);
	p[16] = DIGIT_MUL(ad[23], bd[23]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]) +
	    LO(p[16]);
	rd[46] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + HI(p[16]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[16], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[16]);
	p[2] = DIGIT_MUL(ad[17], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[17]);
	p[4] = DIGIT_MUL(ad[18], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[18]);
	p[6] = DIGIT_MUL(ad[19], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[19]);
	p[8] = DIGIT_MUL(ad[20], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[20]);
	p[10] = DIGIT_MUL(ad[21], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[21]);
	p[12] = DIGIT_MUL(ad[22], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[22]);
	p[14] = DIGIT_MUL(ad[23], bd[24]);
	p[15] = DIGIT_MUL(ad[24], bd[23]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]) + LO(p[15]);
	rd[47] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + HI(p[15]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[17], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[17]);
	p[2] = DIGIT_MUL(ad[18], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[18]);
	p[4] = DIGIT_MUL(ad[19], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[19]);
	p[6] = DIGIT_MUL(ad[20], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[20]);
	p[8] = DIGIT_MUL(ad[21], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[21]);
	p[10] = DIGIT_MUL(ad[22], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[22]);
	p[12] = DIGIT_MUL(ad[23], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[23]);
	p[14] = DIGIT_MUL(ad[24], bd[24]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]) + LO(p[14]);
	rd[48] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + HI(p[14]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[18], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[18]);
	p[2] = DIGIT_MUL(ad[19], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[19]);
	p[4] = DIGIT_MUL(ad[20], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[20]);
	p[6] = DIGIT_MUL(ad[21], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[21]);
	p[8] = DIGIT_MUL(ad[22], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[22]);
	p[10] = DIGIT_MUL(ad[23], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[23]);
	p[12] = DIGIT_MUL(ad[24], bd[25]);
	p[13] = DIGIT_MUL(ad[25], bd[24]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]) + LO(p[13]);
	rd[49] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + HI(p[13]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[19], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[19]);
	p[2] = DIGIT_MUL(ad[20], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[20]);
	p[4] = DIGIT_MUL(ad[21], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[21]);
	p[6] = DIGIT_MUL(ad[22], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[22]);
	p[8] = DIGIT_MUL(ad[23], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[23]);
	p[10] = DIGIT_MUL(ad[24], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[24]);
	p[12] = DIGIT_MUL(ad[25], bd[25]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]) + LO(p[12]);
	rd[50] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    HI(p[12]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[20], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[20]);
	p[2] = DIGIT_MUL(ad[21], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[21]);
	p[4] = DIGIT_MUL(ad[22], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[22]);
	p[6] = DIGIT_MUL(ad[23], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[23]);
	p[8] = DIGIT_MUL(ad[24], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[24]);
	p[10] = DIGIT_MUL(ad[25], bd[26]);
	p[11] = DIGIT_MUL(ad[26], bd[25]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]) +
	    LO(p[11]);
	rd[51] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) + HI(p[11]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[21], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[21]);
	p[2] = DIGIT_MUL(ad[22], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[22]);
	p[4] = DIGIT_MUL(ad[23], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[23]);
	p[6] = DIGIT_MUL(ad[24], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[24]);
	p[8] = DIGIT_MUL(ad[25], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[25]);
	p[10] = DIGIT_MUL(ad[26], bd[26]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]) + LO(p[10]);
	rd[52] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + HI(p[10]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[22], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[22]);
	p[2] = DIGIT_MUL(ad[23], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[23]);
	p[4] = DIGIT_MUL(ad[24], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[24]);
	p[6] = DIGIT_MUL(ad[25], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[25]);
	p[8] = DIGIT_MUL(ad[26], bd[27]);
	p[9] = DIGIT_MUL(ad[27], bd[26]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]) + LO(p[9]);
	rd[53] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + HI(p[9]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[23], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[23]);
	p[2] = DIGIT_MUL(ad[24], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[24]);
	p[4] = DIGIT_MUL(ad[25], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[25]);
	p[6] = DIGIT_MUL(ad[26], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[26]);
	p[8] = DIGIT_MUL(ad[27], bd[27]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]) + LO(p[8]);
	rd[54] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + HI(p[8]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[24], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[24]);
	p[2] = DIGIT_MUL(ad[25], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[25]);
	p[4] = DIGIT_MUL(ad[26], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[26]);
	p[6] = DIGIT_MUL(ad[27], bd[28]);
	p[7] = DIGIT_MUL(ad[28], bd[27]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]) + LO(p[7]);
	rd[55] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + HI(p[7]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[25], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[25]);
	p[2] = DIGIT_MUL(ad[26], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[26]);
	p[4] = DIGIT_MUL(ad[27], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[27]);
	p[6] = DIGIT_MUL(ad[28], bd[28]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]) + LO(p[6]);
	rd[56] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[26], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[26]);
	p[2] = DIGIT_MUL(ad[27], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[27]);
	p[4] = DIGIT_MUL(ad[28], bd[29]);
	p[5] = DIGIT_MUL(ad[29], bd[28]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]) +
	    LO(p[5]);
	rd[57] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + HI(p[5]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[27], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[27]);
	p[2] = DIGIT_MUL(ad[28], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[28]);
	p[4] = DIGIT_MUL(ad[29], bd[29]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]) + LO(p[4]);
	rd[58] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[28], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[28]);
	p[2] = DIGIT_MUL(ad[29], bd[30]);
	p[3] = DIGIT_MUL(ad[30], bd[29]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]) + LO(p[3]);
	rd[59] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + HI(p[3]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[29], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[29]);
	p[2] = DIGIT_MUL(ad[30], bd[30]);
	t = c + LO(p[0]) + LO(p[1]) + LO(p[2]);
	rd[60] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[30], bd[31]);
	p[1] = DIGIT_MUL(ad[31], bd[30]);
	t = c + LO(p[0]) + LO(p[1]);
	rd[61] = (SslBigNumDigit)t;
	c = HI(p[0]) + HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[31], bd[31]);
	t = c + LO(p[0]);
	rd[62] = (SslBigNumDigit)t;
	rd[63] = (SslBigNumDigit)(HI(p[0]) + (t >> 16));
}

/* r[0..31] = a[0..15]^2 */
void sslSquareBigNumWordsC16(SslBigNumWord *r, const SslBigNumWord *a)
{
	SslBigNumDigit *rd = (SslBigNumDigit *)r;
	const SslBigNumDigit *ad = (const SslBigNumDigit *)a;
	SceUInt32 p[17];
	SceUInt32 c, t;

	p[0] = DIGIT_MUL(ad[0], ad[0]);
	t = LO(p[0]);
	rd[0] = (SslBigNumDigit)t;
	c = HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[0], ad[1]);
	t = c + 2 * LO(p[0]);
	rd[1] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[1], ad[1]);
	p[1] = DIGIT_MUL(ad[0], ad[2]);
	t = c + LO(p[0]) + 2 * LO(p[1]);
	rd[2] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[1], ad[2]);
	p[1] = DIGIT_MUL(ad[0], ad[3]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]);
	rd[3] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[2], ad[2]);
	p[1] = DIGIT_MUL(ad[1], ad[3]);
	p[2] = DIGIT_MUL(ad[0], ad[4]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[4] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[2], ad[3]);
	p[1] = DIGIT_MUL(ad[1], ad[4]);
	p[2] = DIGIT_MUL(ad[0], ad[5]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[5] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[3], ad[3]);
	p[1] = DIGIT_MUL(ad[2], ad[4]);
	p[2] = DIGIT_MUL(ad[1], ad[5]);
	p[3] = DIGIT_MUL(ad[0], ad[6]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[6] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[3], ad[4]);
	p[1] = DIGIT_MUL(ad[2], ad[5]);
	p[2] = DIGIT_MUL(ad[1], ad[6]);
	p[3] = DIGIT_MUL(ad[0], ad[7]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[7] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[4], ad[4]);
	p[1] = DIGIT_MUL(ad[3], ad[5]);
	p[2] = DIGIT_MUL(ad[2], ad[6]);
	p[3] = DIGIT_MUL(ad[1], ad[7]);
	p[4] = DIGIT_MUL(ad[0], ad[8]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]);
	rd[8] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[4], ad[5]);
	p[1] = DIGIT_MUL(ad[3], ad[6]);
	p[2] = DIGIT_MUL(ad[2], ad[7]);
	p[3] = DIGIT_MUL(ad[1], ad[8]);
	p[4] = DIGIT_MUL(ad[0], ad[9]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]);
	rd[9] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[5], ad[5]);
	p[1] = DIGIT_MUL(ad[4], ad[6]);
	p[2] = DIGIT_MUL(ad[3], ad[7]);
	p[3] = DIGIT_MUL(ad[2], ad[8]);
	p[4] = DIGIT_MUL(ad[1], ad[9]);
	p[5] = DIGIT_MUL(ad[0], ad[10]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]);
	rd[10] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[5], ad[6]);
	p[1] = DIGIT_MUL(ad[4], ad[7]);
	p[2] = DIGIT_MUL(ad[3], ad[8]);
	p[3] = DIGIT_MUL(ad[2], ad[9]);
	p[4] = DIGIT_MUL(ad[1], ad[10]);
	p[5] = DIGIT_MUL(ad[0], ad[11]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]);
	rd[11] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[6], ad[6]);
	p[1] = DIGIT_MUL(ad[5], ad[7]);
	p[2] = DIGIT_MUL(ad[4], ad[8]);
	p[3] = DIGIT_MUL(ad[3], ad[9]);
	p[4] = DIGIT_MUL(ad[2], ad[10]);
	p[5] = DIGIT_MUL(ad[1], ad[11]);
	p[6] = DIGIT_MUL(ad[0], ad[12]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]);
	rd[12] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[6], ad[7]);
	p[1] = DIGIT_MUL(ad[5], ad[8]);
	p[2] = DIGIT_MUL(ad[4], ad[9]);
	p[3] = DIGIT_MUL(ad[3], ad[10]);
	p[4] = DIGIT_MUL(ad[2], ad[11]);
	p[5] = DIGIT_MUL(ad[1], ad[12]);
	p[6] = DIGIT_MUL(ad[0], ad[13]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]);
	rd[13] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[7], ad[7]);
	p[1] = DIGIT_MUL(ad[6], ad[8]);
	p[2] = DIGIT_MUL(ad[5], ad[9]);
	p[3] = DIGIT_MUL(ad[4], ad[10]);
	p[4] = DIGIT_MUL(ad[3], ad[11]);
	p[5] = DIGIT_MUL(ad[2], ad[12]);
	p[6] = DIGIT_MUL(ad[1], ad[13]);
	p[7] = DIGIT_MUL(ad[0], ad[14]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]);
	rd[14] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[7], ad[8]);
	p[1] = DIGIT_MUL(ad[6], ad[9]);
	p[2] = DIGIT_MUL(ad[5], ad[10]);
	p[3] = DIGIT_MUL(ad[4], ad[11]);
	p[4] = DIGIT_MUL(ad[3], ad[12]);
	p[5] = DIGIT_MUL(ad[2], ad[13]);
	p[6] = DIGIT_MUL(ad[1], ad[14]);
	p[7] = DIGIT_MUL(ad[0], ad[15]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]);
	rd[15] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[8], ad[8]);
	p[1] = DIGIT_MUL(ad[7], ad[9]);
	p[2] = DIGIT_MUL(ad[6], ad[10]);
	p[3] = DIGIT_MUL(ad[5], ad[11]);
	p[4] = DIGIT_MUL(ad[4], ad[12]);
	p[5] = DIGIT_MUL(ad[3], ad[13]);
	p[6] = DIGIT_MUL(ad[2], ad[14]);
	p[7] = DIGIT_MUL(ad[1], ad[15]);
	p[8] = DIGIT_MUL(ad[0], ad[16]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]);
	rd[16] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[8], ad[9]);
	p[1] = DIGIT_MUL(ad[7], ad[10]);
	p[2] = DIGIT_MUL(ad[6], ad[11]);
	p[3] = DIGIT_MUL(ad[5], ad[12]);
	p[4] = DIGIT_MUL(ad[4], ad[13]);
	p[5] = DIGIT_MUL(ad[3], ad[14]);
	p[6] = DIGIT_MUL(ad[2], ad[15]);
	p[7] = DIGIT_MUL(ad[1], ad[16]);
	p[8] = DIGIT_MUL(ad[0], ad[17]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]);
	rd[17] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[9], ad[9]);
	p[1] = DIGIT_MUL(ad[8], ad[10]);
	p[2] = DIGIT_MUL(ad[7], ad[11]);
	p[3] = DIGIT_MUL(ad[6], ad[12]);
	p[4] = DIGIT_MUL(ad[5], ad[13]);
	p[5] = DIGIT_MUL(ad[4], ad[14]);
	p[6] = DIGIT_MUL(ad[3], ad[15]);
	p[7] = DIGIT_MUL(ad[2], ad[16]);
	p[8] = DIGIT_MUL(ad[1], ad[17]);
	p[9] = DIGIT_MUL(ad[0], ad[18]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]);
	rd[18] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[9], ad[10]);
	p[1] = DIGIT_MUL(ad[8], ad[11]);
	p[2] = DIGIT_MUL(ad[7], ad[12]);
	p[3] = DIGIT_MUL(ad[6], ad[13]);
	p[4] = DIGIT_MUL(ad[5], ad[14]);
	p[5] = DIGIT_MUL(ad[4], ad[15]);
	p[6] = DIGIT_MUL(ad[3], ad[16]);
	p[7] = DIGIT_MUL(ad[2], ad[17]);
	p[8] = DIGIT_MUL(ad[1], ad[18]);
	p[9] = DIGIT_MUL(ad[0], ad[19]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]);
	rd[19] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[10], ad[10]);
	p[1] = DIGIT_MUL(ad[9], ad[11]);
	p[2] = DIGIT_MUL(ad[8], ad[12]);
	p[3] = DIGIT_MUL(ad[7], ad[13]);
	p[4] = DIGIT_MUL(ad[6], ad[14]);
	p[5] = DIGIT_MUL(ad[5], ad[15]);
	p[6] = DIGIT_MUL(ad[4], ad[16]);
	p[7] = DIGIT_MUL(ad[3], ad[17]);
	p[8] = DIGIT_MUL(ad[2], ad[18]);
	p[9] = DIGIT_MUL(ad[1], ad[19]);
	p[10] = DIGIT_MUL(ad[0], ad[20]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]);
	rd[20] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[10], ad[11]);
	p[1] = DIGIT_MUL(ad[9], ad[12]);
	p[2] = DIGIT_MUL(ad[8], ad[13]);
	p[3] = DIGIT_MUL(ad[7], ad[14]);
	p[4] = DIGIT_MUL(ad[6], ad[15]);
	p[5] = DIGIT_MUL(ad[5], ad[16]);
	p[6] = DIGIT_MUL(ad[4], ad[17]);
	p[7] = DIGIT_MUL(ad[3], ad[18]);
	p[8] = DIGIT_MUL(ad[2], ad[19]);
	p[9] = DIGIT_MUL(ad[1], ad[20]);
	p[10] = DIGIT_MUL(ad[0], ad[21]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]);
	rd[21] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[11], ad[11]);
	p[1] = DIGIT_MUL(ad[10], ad[12]);
	p[2] = DIGIT_MUL(ad[9], ad[13]);
	p[3] = DIGIT_MUL(ad[8], ad[14]);
	p[4] = DIGIT_MUL(ad[7], ad[15]);
	p[5] = DIGIT_MUL(ad[6], ad[16]);
	p[6] = DIGIT_MUL(ad[5], ad[17]);
	p[7] = DIGIT_MUL(ad[4], ad[18]);
	p[8] = DIGIT_MUL(ad[3], ad[19]);
	p[9] = DIGIT_MUL(ad[2], ad[20]);
	p[10] = DIGIT_MUL(ad[1], ad[21]);
	p[11] = DIGIT_MUL(ad[0], ad[22]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]);
	rd[22] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[11], ad[12]);
	p[1] = DIGIT_MUL(ad[10], ad[13]);
	p[2] = DIGIT_MUL(ad[9], ad[14]);
	p[3] = DIGIT_MUL(ad[8], ad[15]);
	p[4] = DIGIT_MUL(ad[7], ad[16]);
	p[5] = DIGIT_MUL(ad[6], ad[17]);
	p[6] = DIGIT_MUL(ad[5], ad[18]);
	p[7] = DIGIT_MUL(ad[4], ad[19]);
	p[8] = DIGIT_MUL(ad[3], ad[20]);
	p[9] = DIGIT_MUL(ad[2], ad[21]);
	p[10] = DIGIT_MUL(ad[1], ad[22]);
	p[11] = DIGIT_MUL(ad[0], ad[23]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]);
	rd[23] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[12], ad[12]);
	p[1] = DIGIT_MUL(ad[11], ad[13]);
	p[2] = DIGIT_MUL(ad[10], ad[14]);
	p[3] = DIGIT_MUL(ad[9], ad[15]);
	p[4] = DIGIT_MUL(ad[8], ad[16]);
	p[5] = DIGIT_MUL(ad[7], ad[17]);
	p[6] = DIGIT_MUL(ad[6], ad[18]);
	p[7] = DIGIT_MUL(ad[5], ad[19]);
	p[8] = DIGIT_MUL(ad[4], ad[20]);
	p[9] = DIGIT_MUL(ad[3], ad[21]);
	p[10] = DIGIT_MUL(ad[2], ad[22]);
	p[11] = DIGIT_MUL(ad[1], ad[23]);
	p[12] = DIGIT_MUL(ad[0], ad[24]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]) +
	    2 * LO(p[12]);
	rd[24] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    2 * HI(p[12]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[12], ad[13]);
	p[1] = DIGIT_MUL(ad[11], ad[14]);
	p[2] = DIGIT_MUL(ad[10], ad[15]);
	p[3] = DIGIT_MUL(ad[9], ad[16]);
	p[4] = DIGIT_MUL(ad[8], ad[17]);
	p[5] = DIGIT_MUL(ad[7], ad[18]);
	p[6] = DIGIT_MUL(ad[6], ad[19]);
	p[7] = DIGIT_MUL(ad[5], ad[20]);
	p[8] = DIGIT_MUL(ad[4], ad[21]);
	p[9] = DIGIT_MUL(ad[3], ad[22]);
	p[10] = DIGIT_MUL(ad[2], ad[23]);
	p[11] = DIGIT_MUL(ad[1], ad[24]);
	p[12] = DIGIT_MUL(ad[0], ad[25]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]) +
	    2 * LO(p[12]);
	rd[25] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    2 * HI(p[12]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[13], ad[13]);
	p[1] = DIGIT_MUL(ad[12], ad[14]);
	p[2] = DIGIT_MUL(ad[11], ad[15]);
	p[3] = DIGIT_MUL(ad[10], ad[16]);
	p[4] = DIGIT_MUL(ad[9], ad[17]);
	p[5] = DIGIT_MUL(ad[8], ad[18]);
	p[6] = DIGIT_MUL(ad[7], ad[19]);
	p[7] = DIGIT_MUL(ad[6], ad[20]);
	p[8] = DIGIT_MUL(ad[5], ad[21]);
	p[9] = DIGIT_MUL(ad[4], ad[22]);
	p[10] = DIGIT_MUL(ad[3], ad[23]);
	p[11] = DIGIT_MUL(ad[2], ad[24]);
	p[12] = DIGIT_MUL(ad[1], ad[25]);
	p[13] = DIGIT_MUL(ad[0], ad[26]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]) +
	    2 * LO(p[12]) + 2 * LO(p[13]);
	rd[26] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    2 * HI(p[12]) + 2 * HI(p[13]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[13], ad[14]);
	p[1] = DIGIT_MUL(ad[12], ad[15]);
	p[2] = DIGIT_MUL(ad[11], ad[16]);
	p[3] = DIGIT_MUL(ad[10], ad[17]);
	p[4] = DIGIT_MUL(ad[9], ad[18]);
	p[5] = DIGIT_MUL(ad[8], ad[19]);
	p[6] = DIGIT_MUL(ad[7], ad[20]);
	p[7] = DIGIT_MUL(ad[6], ad[21]);
	p[8] = DIGIT_MUL(ad[5], ad[22]);
	p[9] = DIGIT_MUL(ad[4], ad[23]);
	p[10] = DIGIT_MUL(ad[3], ad[24]);
	p[11] = DIGIT_MUL(ad[2], ad[25]);
	p[12] = DIGIT_MUL(ad[1], ad[26]);
	p[13] = DIGIT_MUL(ad[0], ad[27]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]) +
	    2 * LO(p[12]) + 2 * LO(p[13]);
	rd[27] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    2 * HI(p[12]) + 2 * HI(p[13]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[14], ad[14]);
	p[1] = DIGIT_MUL(ad[13], ad[15]);
	p[2] = DIGIT_MUL(ad[12], ad[16]);
	p[3] = DIGIT_MUL(ad[11], ad[17]);
	p[4] = DIGIT_MUL(ad[10], ad[18]);
	p[5] = DIGIT_MUL(ad[9], ad[19]);
	p[6] = DIGIT_MUL(ad[8], ad[20]);
	p[7] = DIGIT_MUL(ad[7], ad[21]);
	p[8] = DIGIT_MUL(ad[6], ad[22]);
	p[9] = DIGIT_MUL(ad[5], ad[23]);
	p[10] = DIGIT_MUL(ad[4], ad[24]);
	p[11] = DIGIT_MUL(ad[3], ad[25]);
	p[12] = DIGIT_MUL(ad[2], ad[26]);
	p[13] = DIGIT_MUL(ad[1], ad[27]);
	p[14] = DIGIT_MUL(ad[0], ad[28]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]) +
	    2 * LO(p[12]) + 2 * LO(p[13]) + 2 * LO(p[14]);
	rd[28] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    2 * HI(p[12]) + 2 * HI(p[13]) + 2 * HI(p[14]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[14], ad[15]);
	p[1] = DIGIT_MUL(ad[13], ad[16]);
	p[2] = DIGIT_MUL(ad[12], ad[17]);
	p[3] = DIGIT_MUL(ad[11], ad[18]);
	p[4] = DIGIT_MUL(ad[10], ad[19]);
	p[5] = DIGIT_MUL(ad[9], ad[20]);
	p[6] = DIGIT_MUL(ad[8], ad[21]);
	p[7] = DIGIT_MUL(ad[7], ad[22]);
	p[8] = DIGIT_MUL(ad[6], ad[23]);
	p[9] = DIGIT_MUL(ad[5], ad[24]);
	p[10] = DIGIT_MUL(ad[4], ad[25]);
	p[11] = DIGIT_MUL(ad[3], ad[26]);
	p[12] = DIGIT_MUL(ad[2], ad[27]);
	p[13] = DIGIT_MUL(ad[1], ad[28]);
	p[14] = DIGIT_MUL(ad[0], ad[29]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]) +
	    2 * LO(p[12]) + 2 * LO(p[13]) + 2 * LO(p[14]);
	rd[29] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    2 * HI(p[12]) + 2 * HI(p[13]) + 2 * HI(p[14]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[15], ad[15]);
	p[1] = DIGIT_MUL(ad[14], ad[16]);
	p[2] = DIGIT_MUL(ad[13], ad[17]);
	p[3] = DIGIT_MUL(ad[12], ad[18]);
	p[4] = DIGIT_MUL(ad[11], ad[19]);
	p[5] = DIGIT_MUL(ad[10], ad[20]);
	p[6] = DIGIT_MUL(ad[9], ad[21]);
	p[7] = DIGIT_MUL(ad[8], ad[22]);
	p[8] = DIGIT_MUL(ad[7], ad[23]);
	p[9] = DIGIT_MUL(ad[6], ad[24]);
	p[10] = DIGIT_MUL(ad[5], ad[25]);
	p[11] = DIGIT_MUL(ad[4], ad[26]);
	p[12] = DIGIT_MUL(ad[3], ad[27]);
	p[13] = DIGIT_MUL(ad[2], ad[28]);
	p[14] = DIGIT_MUL(ad[1], ad[29]);
	p[15] = DIGIT_MUL(ad[0], ad[30]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]) +
	    2 * LO(p[12]) + 2 * LO(p[13]) + 2 * LO(p[14]) + 2 * LO(p[15]);
	rd[30] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    2 * HI(p[12]) + 2 * HI(p[13]) + 2 * HI(p[14]) + 2 * HI(p[15]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[15], ad[16]);
	p[1] = DIGIT_MUL(ad[14], ad[17]);
	p[2] = DIGIT_MUL(ad[13], ad[18]);
	p[3] = DIGIT_MUL(ad[12], ad[19]);
	p[4] = DIGIT_MUL(ad[11], ad[20]);
	p[5] = DIGIT_MUL(ad[10], ad[21]);
	p[6] = DIGIT_MUL(ad[9], ad[22]);
	p[7] = DIGIT_MUL(ad[8], ad[23]);
	p[8] = DIGIT_MUL(ad[7], ad[24]);
	p[9] = DIGIT_MUL(ad[6], ad[25]);
	p[10] = DIGIT_MUL(ad[5], ad[26]);
	p[11] = DIGIT_MUL(ad[4], ad[27]);
	p[12] = DIGIT_MUL(ad[3], ad[28]);
	p[13] = DIGIT_MUL(ad[2], ad[29]);
	p[14] = DIGIT_MUL(ad[1], ad[30]);
	p[15] = DIGIT_MUL(ad[0], ad[31]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]) +
	    2 * LO(p[12]) + 2 * LO(p[13]) + 2 * LO(p[14]) + 2 * LO(p[15]);
	rd[31] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    2 * HI(p[12]) + 2 * HI(p[13]) + 2 * HI(p[14]) + 2 * HI(p[15]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[16], ad[16]);
	p[1] = DIGIT_MUL(ad[15], ad[17]);
	p[2] = DIGIT_MUL(ad[14], ad[18]);
	p[3] = DIGIT_MUL(ad[13], ad[19]);
	p[4] = DIGIT_MUL(ad[12], ad[20]);
	p[5] = DIGIT_MUL(ad[11], ad[21]);
	p[6] = DIGIT_MUL(ad[10], ad[22]);
	p[7] = DIGIT_MUL(ad[9], ad[23]);
	p[8] = DIGIT_MUL(ad[8], ad[24]);
	p[9] = DIGIT_MUL(ad[7], ad[25]);
	p[10] = DIGIT_MUL(ad[6], ad[26]);
	p[11] = DIGIT_MUL(ad[5], ad[27]);
	p[12] = DIGIT_MUL(ad[4], ad[28]);
	p[13] = DIGIT_MUL(ad[3], ad[29]);
	p[14] = DIGIT_MUL(ad[2], ad[30]);
	p[15] = DIGIT_MUL(ad[1], ad[31]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]) +
	    2 * LO(p[12]) + 2 * LO(p[13]) + 2 * LO(p[14]) + 2 * LO(p[15]);
	rd[32] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    2 * HI(p[12]) + 2 * HI(p[13]) + 2 * HI(p[14]) + 2 * HI(p[15]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[16], ad[17]);
	p[1] = DIGIT_MUL(ad[15], ad[18]);
	p[2] = DIGIT_MUL(ad[14], ad[19]);
	p[3] = DIGIT_MUL(ad[13], ad[20]);
	p[4] = DIGIT_MUL(ad[12], ad[21]);
	p[5] = DIGIT_MUL(ad[11], ad[22]);
	p[6] = DIGIT_MUL(ad[10], ad[23]);
	p[7] = DIGIT_MUL(ad[9], ad[24]);
	p[8] = DIGIT_MUL(ad[8], ad[25]);
	p[9] = DIGIT_MUL(ad[7], ad[26]);
	p[10] = DIGIT_MUL(ad[6], ad[27]);
	p[11] = DIGIT_MUL(ad[5], ad[28]);
	p[12] = DIGIT_MUL(ad[4], ad[29]);
	p[13] = DIGIT_MUL(ad[3], ad[30]);
	p[14] = DIGIT_MUL(ad[2], ad[31]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]) +
	    2 * LO(p[12]) + 2 * LO(p[13]) + 2 * LO(p[14]);
	rd[33] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    2 * HI(p[12]) + 2 * HI(p[13]) + 2 * HI(p[14]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[17], ad[17]);
	p[1] = DIGIT_MUL(ad[16], ad[18]);
	p[2] = DIGIT_MUL(ad[15], ad[19]);
	p[3] = DIGIT_MUL(ad[14], ad[20]);
	p[4] = DIGIT_MUL(ad[13], ad[21]);
	p[5] = DIGIT_MUL(ad[12], ad[22]);
	p[6] = DIGIT_MUL(ad[11], ad[23]);
	p[7] = DIGIT_MUL(ad[10], ad[24]);
	p[8] = DIGIT_MUL(ad[9], ad[25]);
	p[9] = DIGIT_MUL(ad[8], ad[26]);
	p[10] = DIGIT_MUL(ad[7], ad[27]);
	p[11] = DIGIT_MUL(ad[6], ad[28]);
	p[12] = DIGIT_MUL(ad[5], ad[29]);
	p[13] = DIGIT_MUL(ad[4], ad[30]);
	p[14] = DIGIT_MUL(ad[3], ad[31]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]) +
	    2 * LO(p[12]) + 2 * LO(p[13]) + 2 * LO(p[14]);
	rd[34] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    2 * HI(p[12]) + 2 * HI(p[13]) + 2 * HI(p[14]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[17], ad[18]);
	p[1] = DIGIT_MUL(ad[16], ad[19]);
	p[2] = DIGIT_MUL(ad[15], ad[20]);
	p[3] = DIGIT_MUL(ad[14], ad[21]);
	p[4] = DIGIT_MUL(ad[13], ad[22]);
	p[5] = DIGIT_MUL(ad[12], ad[23]);
	p[6] = DIGIT_MUL(ad[11], ad[24]);
	p[7] = DIGIT_MUL(ad[10], ad[25]);
	p[8] = DIGIT_MUL(ad[9], ad[26]);
	p[9] = DIGIT_MUL(ad[8], ad[27]);
	p[10] = DIGIT_MUL(ad[7], ad[28]);
	p[11] = DIGIT_MUL(ad[6], ad[29]);
	p[12] = DIGIT_MUL(ad[5], ad[30]);
	p[13] = DIGIT_MUL(ad[4], ad[31]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]) +
	    2 * LO(p[12]) + 2 * LO(p[13]);
	rd[35] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    2 * HI(p[12]) + 2 * HI(p[13]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[18], ad[18]);
	p[1] = DIGIT_MUL(ad[17], ad[19]);
	p[2] = DIGIT_MUL(ad[16], ad[20]);
	p[3] = DIGIT_MUL(ad[15], ad[21]);
	p[4] = DIGIT_MUL(ad[14], ad[22]);
	p[5] = DIGIT_MUL(ad[13], ad[23]);
	p[6] = DIGIT_MUL(ad[12], ad[24]);
	p[7] = DIGIT_MUL(ad[11], ad[25]);
	p[8] = DIGIT_MUL(ad[10], ad[26]);
	p[9] = DIGIT_MUL(ad[9], ad[27]);
	p[10] = DIGIT_MUL(ad[8], ad[28]);
	p[11] = DIGIT_MUL(ad[7], ad[29]);
	p[12] = DIGIT_MUL(ad[6], ad[30]);
	p[13] = DIGIT_MUL(ad[5], ad[31]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]) +
	    2 * LO(p[12]) + 2 * LO(p[13]);
	rd[36] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    2 * HI(p[12]) + 2 * HI(p[13]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[18], ad[19]);
	p[1] = DIGIT_MUL(ad[17], ad[20]);
	p[2] = DIGIT_MUL(ad[16], ad[21]);
	p[3] = DIGIT_MUL(ad[15], ad[22]);
	p[4] = DIGIT_MUL(ad[14], ad[23]);
	p[5] = DIGIT_MUL(ad[13], ad[24]);
	p[6] = DIGIT_MUL(ad[12], ad[25]);
	p[7] = DIGIT_MUL(ad[11], ad[26]);
	p[8] = DIGIT_MUL(ad[10], ad[27]);
	p[9] = DIGIT_MUL(ad[9], ad[28]);
	p[10] = DIGIT_MUL(ad[8], ad[29]);
	p[11] = DIGIT_MUL(ad[7], ad[30]);
	p[12] = DIGIT_MUL(ad[6], ad[31]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]) +
	    2 * LO(p[12]);
	rd[37] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    2 * HI(p[12]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[19], ad[19]);
	p[1] = DIGIT_MUL(ad[18], ad[20]);
	p[2] = DIGIT_MUL(ad[17], ad[21]);
	p[3] = DIGIT_MUL(ad[16], ad[22]);
	p[4] = DIGIT_MUL(ad[15], ad[23]);
	p[5] = DIGIT_MUL(ad[14], ad[24]);
	p[6] = DIGIT_MUL(ad[13], ad[25]);
	p[7] = DIGIT_MUL(ad[12], ad[26]);
	p[8] = DIGIT_MUL(ad[11], ad[27]);
	p[9] = DIGIT_MUL(ad[10], ad[28]);
	p[10] = DIGIT_MUL(ad[9], ad[29]);
	p[11] = DIGIT_MUL(ad[8], ad[30]);
	p[12] = DIGIT_MUL(ad[7], ad[31]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]) +
	    2 * LO(p[12]);
	rd[38] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    2 * HI(p[12]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[19], ad[20]);
	p[1] = DIGIT_MUL(ad[18], ad[21]);
	p[2] = DIGIT_MUL(ad[17], ad[22]);
	p[3] = DIGIT_MUL(ad[16], ad[23]);
	p[4] = DIGIT_MUL(ad[15], ad[24]);
	p[5] = DIGIT_MUL(ad[14], ad[25]);
	p[6] = DIGIT_MUL(ad[13], ad[26]);
	p[7] = DIGIT_MUL(ad[12], ad[27]);
	p[8] = DIGIT_MUL(ad[11], ad[28]);
	p[9] = DIGIT_MUL(ad[10], ad[29]);
	p[10] = DIGIT_MUL(ad[9], ad[30]);
	p[11] = DIGIT_MUL(ad[8], ad[31]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]);
	rd[39] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[20], ad[20]);
	p[1] = DIGIT_MUL(ad[19], ad[21]);
	p[2] = DIGIT_MUL(ad[18], ad[22]);
	p[3] = DIGIT_MUL(ad[17], ad[23]);
	p[4] = DIGIT_MUL(ad[16], ad[24]);
	p[5] = DIGIT_MUL(ad[15], ad[25]);
	p[6] = DIGIT_MUL(ad[14], ad[26]);
	p[7] = DIGIT_MUL(ad[13], ad[27]);
	p[8] = DIGIT_MUL(ad[12], ad[28]);
	p[9] = DIGIT_MUL(ad[11], ad[29]);
	p[10] = DIGIT_MUL(ad[10], ad[30]);
	p[11] = DIGIT_MUL(ad[9], ad[31]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]) + 2 * LO(p[11]);
	rd[40] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + 2 * HI(p[11]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[20], ad[21]);
	p[1] = DIGIT_MUL(ad[19], ad[22]);
	p[2] = DIGIT_MUL(ad[18], ad[23]);
	p[3] = DIGIT_MUL(ad[17], ad[24]);
	p[4] = DIGIT_MUL(ad[16], ad[25]);
	p[5] = DIGIT_MUL(ad[15], ad[26]);
	p[6] = DIGIT_MUL(ad[14], ad[27]);
	p[7] = DIGIT_MUL(ad[13], ad[28]);
	p[8] = DIGIT_MUL(ad[12], ad[29]);
	p[9] = DIGIT_MUL(ad[11], ad[30]);
	p[10] = DIGIT_MUL(ad[10], ad[31]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]);
	rd[41] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[21], ad[21]);
	p[1] = DIGIT_MUL(ad[20], ad[22]);
	p[2] = DIGIT_MUL(ad[19], ad[23]);
	p[3] = DIGIT_MUL(ad[18], ad[24]);
	p[4] = DIGIT_MUL(ad[17], ad[25]);
	p[5] = DIGIT_MUL(ad[16], ad[26]);
	p[6] = DIGIT_MUL(ad[15], ad[27]);
	p[7] = DIGIT_MUL(ad[14], ad[28]);
	p[8] = DIGIT_MUL(ad[13], ad[29]);
	p[9] = DIGIT_MUL(ad[12], ad[30]);
	p[10] = DIGIT_MUL(ad[11], ad[31]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]) + 2 * LO(p[10]);
	rd[42] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + 2 * HI(p[10]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[21], ad[22]);
	p[1] = DIGIT_MUL(ad[20], ad[23]);
	p[2] = DIGIT_MUL(ad[19], ad[24]);
	p[3] = DIGIT_MUL(ad[18], ad[25]);
	p[4] = DIGIT_MUL(ad[17], ad[26]);
	p[5] = DIGIT_MUL(ad[16], ad[27]);
	p[6] = DIGIT_MUL(ad[15], ad[28]);
	p[7] = DIGIT_MUL(ad[14], ad[29]);
	p[8] = DIGIT_MUL(ad[13], ad[30]);
	p[9] = DIGIT_MUL(ad[12], ad[31]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]);
	rd[43] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[22], ad[22]);
	p[1] = DIGIT_MUL(ad[21], ad[23]);
	p[2] = DIGIT_MUL(ad[20], ad[24]);
	p[3] = DIGIT_MUL(ad[19], ad[25]);
	p[4] = DIGIT_MUL(ad[18], ad[26]);
	p[5] = DIGIT_MUL(ad[17], ad[27]);
	p[6] = DIGIT_MUL(ad[16], ad[28]);
	p[7] = DIGIT_MUL(ad[15], ad[29]);
	p[8] = DIGIT_MUL(ad[14], ad[30]);
	p[9] = DIGIT_MUL(ad[13], ad[31]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]) + 2 * LO(p[9]);
	rd[44] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + 2 * HI(p[9]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[22], ad[23]);
	p[1] = DIGIT_MUL(ad[21], ad[24]);
	p[2] = DIGIT_MUL(ad[20], ad[25]);
	p[3] = DIGIT_MUL(ad[19], ad[26]);
	p[4] = DIGIT_MUL(ad[18], ad[27]);
	p[5] = DIGIT_MUL(ad[17], ad[28]);
	p[6] = DIGIT_MUL(ad[16], ad[29]);
	p[7] = DIGIT_MUL(ad[15], ad[30]);
	p[8] = DIGIT_MUL(ad[14], ad[31]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]);
	rd[45] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[23], ad[23]);
	p[1] = DIGIT_MUL(ad[22], ad[24]);
	p[2] = DIGIT_MUL(ad[21], ad[25]);
	p[3] = DIGIT_MUL(ad[20], ad[26]);
	p[4] = DIGIT_MUL(ad[19], ad[27]);
	p[5] = DIGIT_MUL(ad[18], ad[28]);
	p[6] = DIGIT_MUL(ad[17], ad[29]);
	p[7] = DIGIT_MUL(ad[16], ad[30]);
	p[8] = DIGIT_MUL(ad[15], ad[31]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]) +
	    2 * LO(p[8]);
	rd[46] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    2 * HI(p[8]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[23], ad[24]);
	p[1] = DIGIT_MUL(ad[22], ad[25]);
	p[2] = DIGIT_MUL(ad[21], ad[26]);
	p[3] = DIGIT_MUL(ad[20], ad[27]);
	p[4] = DIGIT_MUL(ad[19], ad[28]);
	p[5] = DIGIT_MUL(ad[18], ad[29]);
	p[6] = DIGIT_MUL(ad[17], ad[30]);
	p[7] = DIGIT_MUL(ad[16], ad[31]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]);
	rd[47] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[24], ad[24]);
	p[1] = DIGIT_MUL(ad[23], ad[25]);
	p[2] = DIGIT_MUL(ad[22], ad[26]);
	p[3] = DIGIT_MUL(ad[21], ad[27]);
	p[4] = DIGIT_MUL(ad[20], ad[28]);
	p[5] = DIGIT_MUL(ad[19], ad[29]);
	p[6] = DIGIT_MUL(ad[18], ad[30]);
	p[7] = DIGIT_MUL(ad[17], ad[31]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]) + 2 * LO(p[7]);
	rd[48] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + 2 * HI(p[7]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[24], ad[25]);
	p[1] = DIGIT_MUL(ad[23], ad[26]);
	p[2] = DIGIT_MUL(ad[22], ad[27]);
	p[3] = DIGIT_MUL(ad[21], ad[28]);
	p[4] = DIGIT_MUL(ad[20], ad[29]);
	p[5] = DIGIT_MUL(ad[19], ad[30]);
	p[6] = DIGIT_MUL(ad[18], ad[31]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]);
	rd[49] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[25], ad[25]);
	p[1] = DIGIT_MUL(ad[24], ad[26]);
	p[2] = DIGIT_MUL(ad[23], ad[27]);
	p[3] = DIGIT_MUL(ad[22], ad[28]);
	p[4] = DIGIT_MUL(ad[21], ad[29]);
	p[5] = DIGIT_MUL(ad[20], ad[30]);
	p[6] = DIGIT_MUL(ad[19], ad[31]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]) + 2 * LO(p[6]);
	rd[50] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + 2 * HI(p[6]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[25], ad[26]);
	p[1] = DIGIT_MUL(ad[24], ad[27]);
	p[2] = DIGIT_MUL(ad[23], ad[28]);
	p[3] = DIGIT_MUL(ad[22], ad[29]);
	p[4] = DIGIT_MUL(ad[21], ad[30]);
	p[5] = DIGIT_MUL(ad[20], ad[31]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]);
	rd[51] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[26], ad[26]);
	p[1] = DIGIT_MUL(ad[25], ad[27]);
	p[2] = DIGIT_MUL(ad[24], ad[28]);
	p[3] = DIGIT_MUL(ad[23], ad[29]);
	p[4] = DIGIT_MUL(ad[22], ad[30]);
	p[5] = DIGIT_MUL(ad[21], ad[31]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]) + 2 * LO(p[5]);
	rd[52] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + 2 * HI(p[5]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[26], ad[27]);
	p[1] = DIGIT_MUL(ad[25], ad[28]);
	p[2] = DIGIT_MUL(ad[24], ad[29]);
	p[3] = DIGIT_MUL(ad[23], ad[30]);
	p[4] = DIGIT_MUL(ad[22], ad[31]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]);
	rd[53] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[27], ad[27]);
	p[1] = DIGIT_MUL(ad[26], ad[28]);
	p[2] = DIGIT_MUL(ad[25], ad[29]);
	p[3] = DIGIT_MUL(ad[24], ad[30]);
	p[4] = DIGIT_MUL(ad[23], ad[31]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]) +
	    2 * LO(p[4]);
	rd[54] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    2 * HI(p[4]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[27], ad[28]);
	p[1] = DIGIT_MUL(ad[26], ad[29]);
	p[2] = DIGIT_MUL(ad[25], ad[30]);
	p[3] = DIGIT_MUL(ad[24], ad[31]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[55] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[28], ad[28]);
	p[1] = DIGIT_MUL(ad[27], ad[29]);
	p[2] = DIGIT_MUL(ad[26], ad[30]);
	p[3] = DIGIT_MUL(ad[25], ad[31]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]) + 2 * LO(p[3]);
	rd[56] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + 2 * HI(p[3]) +
	    (t >> 16);

	p[0] = DIGIT_MUL(ad[28], ad[29]);
	p[1] = DIGIT_MUL(ad[27], ad[30]);
	p[2] = DIGIT_MUL(ad[26], ad[31]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[57] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[29], ad[29]);
	p[1] = DIGIT_MUL(ad[28], ad[30]);
	p[2] = DIGIT_MUL(ad[27], ad[31]);
	t = c + LO(p[0]) + 2 * LO(p[1]) + 2 * LO(p[2]);
	rd[58] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + 2 * HI(p[2]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[29], ad[30]);
	p[1] = DIGIT_MUL(ad[28], ad[31]);
	t = c + 2 * LO(p[0]) + 2 * LO(p[1]);
	rd[59] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[30], ad[30]);
	p[1] = DIGIT_MUL(ad[29], ad[31]);
	t = c + LO(p[0]) + 2 * LO(p[1]);
	rd[60] = (SslBigNumDigit)t;
	c = HI(p[0]) + 2 * HI(p[1]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[30], ad[31]);
	t = c + 2 * LO(p[0]);
	rd[61] = (SslBigNumDigit)t;
	c = 2 * HI(p[0]) + (t >> 16);

	p[0] = DIGIT_MUL(ad[31], ad[31]);
	t = c + LO(p[0]);
	rd[62] = (SslBigNumDigit)t;
	rd[63] = (SslBigNumDigit)(HI(p[0]) + (t >> 16));
}

