// multprec.cpp: multiprecision arithmetic for the random number engines of <random>, on
// arrays of _MP_len words of 32 bits each held in 64-bit elements.
#include <functional>	/* its placeholders: Sony's object has their empty initializer */
#include <limits>

_STD_BEGIN
typedef unsigned long long _Max_type;	/* as <random> declares them */
static const int _MP_len = 5;
typedef _Max_type _MP_arr[_MP_len];

static const int shift = numeric_limits<_Max_type>::digits / 2;	/* bits in a word */
static const _Max_type mask = ~(~_Max_type(0) << shift);
static const _Max_type maxval = mask + 1;	/* 2^shift */

_Max_type _MP_Get(_MP_arr u)
	{	// convert multi-word value to scalar value
	return ((u[1] << shift) + u[0]);
	}

void _MP_Addarr(_MP_arr u, int ulen, _MP_arr v, int vlen)
	{	// add multi-word value to multi-word value
	int i;
	_Max_type k = 0;

	for (i = 0; i < vlen; ++i)
		{	// add multi-word values
		u[i] += v[i] + k;
		k = u[i] >> shift;
		u[i] &= mask;
		}
	for (; k != 0 && i < ulen; ++i)
		{	// propagate carry
		u[i] += k;
		k = u[i] >> shift;
		u[i] &= mask;
		}
	}

void _MP_Add(_MP_arr u, _Max_type v0)
	{	// add scalar value to multi-word value
	_Max_type v[2];

	v[0] = v0 & mask;
	v[1] = v0 >> shift;
	_MP_Addarr(u, _MP_len, v, 2);
	}

static void mul(_MP_arr u, int ulen, _Max_type v0)
	{	// multiply multi-word value by single-word value
	_Max_type k = 0;

	for (int i = 0; i < ulen; ++i)
		{	// multiply and propagate carry
		u[i] = u[i] * v0 + k;
		k = u[i] >> shift;
		u[i] &= mask;
		}
	}

void _MP_Mul(_MP_arr w, _Max_type u0, _Max_type v0)
	{	// multiply multi-word value by multi-word value
	const int m = 2, n = 2;
	_Max_type u[2], v[2];

	u[0] = u0 & mask;
	u[1] = u0 >> shift;
	v[0] = v0 & mask;
	v[1] = v0 >> shift;

	// Knuth, vol. 2, p. 268, Algorithm M
	// M1: [Initialize.]
	for (int i = 0; i < _MP_len; ++i)
		w[i] = 0;
	for (int j = 0; j < n; ++j)
		{	// M2: [Zero multiplier?]
		if (v[j] == 0)
			w[j + m] = 0;
		else
			{	// multiply by non-zero multiplier
			// M3: [Initialize i.]
			_Max_type k = 0;

			for (int i = 0; i < m; ++i)
				{	// M4: [Multiply and add.]
				_Max_type t = u[i] * v[j] + w[i + j] + k;

				w[i + j] = t & mask;
				k = t >> shift;
				// M5: [Loop on i.]
				}
			w[j + m] = k;
			}
		// M6: [Loop on j.]
		}
	}

static void div(_MP_arr u, _Max_type v0)
	{	// divide multi-word value by scalar value
	_Max_type k = 0;
	int ulen = _MP_len;

	while (0 <= --ulen)
		{	// propagate remainder and divide
		_Max_type t = (k << shift) + u[ulen];

		u[ulen] = t / v0;
		k = t % v0;
		}
	}

static int limit(const _Max_type *u, int n)
	{	// get number of non-zero elements
	int res = n;

	while (0 < res && u[res - 1] == 0)
		--res;
	return (res);
	}

void _MP_Rem(_MP_arr u, _Max_type v0)
	{	// divide multi-word value by value, leaving remainder in u
	_Max_type v[2];

	v[0] = v0 & mask;
	v[1] = v0 >> shift;
	const int n = limit(v, 2);
	const int m = limit(u, _MP_len) - n;

	// Knuth, vol. 2, p. 272, Algorithm D
	// D1: [Normalize.]
	_Max_type d = maxval / (v[n - 1] + 1);

	if (d != 1)
		{	// scale numerator and divisor
		mul(u, _MP_len, d);
		mul(v, n, d);
		}
	// D2: [Initialize j.]
	for (int j = m; 0 <= j; --j)
		{	// D3: [Calculate qh.]
		_Max_type qh = ((u[j + n] << shift) + u[j + n - 1]) / v[n - 1];

		if (qh == 0)
			continue;
		_Max_type rh = ((u[j + n] << shift) + u[j + n - 1]) % v[n - 1];

		for (;;)
			if (qh < maxval && (n < 2
				|| qh * v[n - 2] <= (rh << shift) + u[j + n - 2]))
				break;
			else
				{	// reduce tentative value and retry
				--qh;
				rh += v[n - 1];
				if (maxval <= rh)
					break;
				}

		// D4: [Multiply and subtract.]
		_Max_type k = 0;
		int i;

		for (i = 0; i < n; ++i)
			{	// multiply and subtract
			u[j + i] -= qh * v[i] + k;
			k = u[j + i] >> shift;
			if (k)
				k = maxval - k;
			u[j + i] &= mask;
			}
		for (; k != 0 && j + i < _MP_len; ++i)
			{	// propagate borrow
			u[j + i] -= k;
			k = u[j + i] >> shift;
			if (k)
				k = maxval - k;
			u[j + i] &= mask;
			}

		// D5: [Test remainder.]
		// D6: [Add back.]
		if (k != 0)
			{	// add back
			--qh;
			_MP_Addarr(u + j, n + 1, v, n);
			}
		// D7: [Loop on j.]
		}

	// D8: [Unnormalize.]
	if (d != 1)
		div(u, d);
	}
_STD_END
