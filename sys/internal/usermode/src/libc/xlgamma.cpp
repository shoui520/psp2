// xlgamma.cpp: a moderately accurate log gamma for the distributions of <random> (Lanczos's
// approximation, as Numerical Recipes gives it), for float, double and long double.
#include <cmath>

_STD_BEGIN
template<class _Ty> inline
	_Ty _Lgamma(_Ty _Xx)
	{	// compute log gamma
	static const _Ty _Coeffs[] =
		{	// the series coefficients
		(_Ty)76.18009172947146,
		(_Ty)-86.50532032941678,
		(_Ty)24.01409824083091,
		(_Ty)-1.23173972450155,
		(_Ty)0.1208650973866179e-2,
		(_Ty)-0.5395239384953e-5
		};
	_Ty _Tmp = _Xx + (_Ty)5.5;
	_Ty _Ser = (_Ty)1.000000000190015;
	_Ty _Yx = _Xx + (_Ty)1;

	_Tmp -= (_Xx + (_Ty)0.5) * _CSTD log(_Tmp);
	for (int _Idx = 0; _Idx < (int)(sizeof (_Coeffs) / sizeof (_Coeffs[0])); ++_Idx)
		{	// add a term of the series
		_Ser += _Coeffs[_Idx] / _Yx;
		_Yx += 1.0;
		}
	return (_CSTD log((_Ty)2.5066282746310005 * _Ser / _Xx) - _Tmp);
	}

float _XLgamma(float _Xx)
	{	// compute log gamma for float
	return (_Lgamma(_Xx));
	}

double _XLgamma(double _Xx)
	{	// compute log gamma for double
	return (_Lgamma(_Xx));
	}

long double _XLgamma(long double _Xx)
	{	// compute log gamma for long double
	return (_Lgamma(_Xx));
	}
_STD_END
