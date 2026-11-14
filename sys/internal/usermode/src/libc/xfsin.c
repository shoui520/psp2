/*
 * _FSinx function: the sine of x, or its cosine for an odd qoff, in float;
 * _FSin is it with no quadrants given. x is reduced to [-pi/4, pi/4] by
 * _FQuad and qoff adds the quadrants it was reduced by.
 */
#include "xmath.h"

static const float c[] = {	/* cos(x) - 1 is x^2 times this in x^2 */
	-0.0013602249F, 0.041656695F, -0.499999F,
};
static const float s[] = {	/* sin(x) - x is x^3 times this in x^2 */
	-0.0001950727F, 0.008332076F, -0.16666652F,
};

float _FSinx(float x, unsigned int qoff, int quads)
{
	switch (_FDtest(&x)) {
	case _NANCODE:
		return x;
	case 0:	/* sin(0) is 0, cos(0) 1 */
		if (qoff & 0x1)
			x = 1.0F;
		return (qoff & 0x2) ? -x : x;
	case _INFCODE:
		_Feraise(_FE_INVALID);
		return _FNan._Float;
	default:	/* finite */
		qoff += _FQuad(&x);
		if (-_FRteps._Float < x && x < _FRteps._Float) {	/* tiny: x or 1 */
			if (qoff & 0x1)
				x = 1.0F;
		} else {
			const float w = x * x;

			if (qoff & 0x1)
				x = 1.0F + w * ((c[0] * w + c[1]) * w + c[2]);
			else
				x += x * w * ((s[0] * w + s[1]) * w + s[2]);
		}
		return (qoff & 0x2) ? -x : x;
	}
}

float _FSin(float x, unsigned int qoff)
{
	return _FSinx(x, qoff, 0);
}
