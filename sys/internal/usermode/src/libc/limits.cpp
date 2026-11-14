// limits.cpp: the out-of-line definitions of numeric_limits' static constants, of the
// bases and of each specialization, for the members the specializations declare themselves
#include <limits>

_STD_BEGIN
const float_denorm_style _Num_base::has_denorm;
const bool _Num_base::has_denorm_loss;
const bool _Num_base::has_infinity;
const bool _Num_base::has_quiet_NaN;
const bool _Num_base::has_signaling_NaN;
const bool _Num_base::is_bounded;
const bool _Num_base::is_exact;
const bool _Num_base::is_iec559;
const bool _Num_base::is_integer;
const bool _Num_base::is_modulo;
const bool _Num_base::is_signed;
const bool _Num_base::is_specialized;
const bool _Num_base::tinyness_before;
const bool _Num_base::traps;
const float_round_style _Num_base::round_style;
const int _Num_base::digits;
const int _Num_base::digits10;
const int _Num_base::max_exponent;
const int _Num_base::max_exponent10;
const int _Num_base::min_exponent;
const int _Num_base::min_exponent10;
const int _Num_base::radix;

const bool _Num_int_base::is_bounded;
const bool _Num_int_base::is_exact;
const bool _Num_int_base::is_integer;
const bool _Num_int_base::is_modulo;
const bool _Num_int_base::is_specialized;
const int _Num_int_base::radix;

const float_denorm_style _Num_float_base::has_denorm;
const bool _Num_float_base::has_denorm_loss;
const bool _Num_float_base::has_infinity;
const bool _Num_float_base::has_quiet_NaN;
const bool _Num_float_base::has_signaling_NaN;
const bool _Num_float_base::is_bounded;
const bool _Num_float_base::is_exact;
const bool _Num_float_base::is_iec559;
const bool _Num_float_base::is_integer;
const bool _Num_float_base::is_modulo;
const bool _Num_float_base::is_signed;
const bool _Num_float_base::is_specialized;
const bool _Num_float_base::tinyness_before;
const bool _Num_float_base::traps;
const float_round_style _Num_float_base::round_style;
const int _Num_float_base::radix;

const bool numeric_limits<char>::is_signed;
const int numeric_limits<char>::digits;
const int numeric_limits<char>::digits10;

const bool numeric_limits<wchar_t>::is_signed;
const int numeric_limits<wchar_t>::digits;
const int numeric_limits<wchar_t>::digits10;

const bool numeric_limits<bool>::is_modulo;
const bool numeric_limits<bool>::is_signed;
const int numeric_limits<bool>::digits;
const int numeric_limits<bool>::digits10;

const bool numeric_limits<signed char>::is_signed;
const int numeric_limits<signed char>::digits;
const int numeric_limits<signed char>::digits10;

const bool numeric_limits<unsigned char>::is_signed;
const int numeric_limits<unsigned char>::digits;
const int numeric_limits<unsigned char>::digits10;

const bool numeric_limits<short>::is_signed;
const int numeric_limits<short>::digits;
const int numeric_limits<short>::digits10;

const bool numeric_limits<unsigned short>::is_signed;
const int numeric_limits<unsigned short>::digits;
const int numeric_limits<unsigned short>::digits10;

const bool numeric_limits<int>::is_signed;
const int numeric_limits<int>::digits;
const int numeric_limits<int>::digits10;

const bool numeric_limits<unsigned int>::is_signed;
const int numeric_limits<unsigned int>::digits;
const int numeric_limits<unsigned int>::digits10;

const bool numeric_limits<long>::is_signed;
const int numeric_limits<long>::digits;
const int numeric_limits<long>::digits10;

const bool numeric_limits<unsigned long>::is_signed;
const int numeric_limits<unsigned long>::digits;
const int numeric_limits<unsigned long>::digits10;

const bool numeric_limits<long long>::is_signed;
const int numeric_limits<long long>::digits;
const int numeric_limits<long long>::digits10;

const bool numeric_limits<unsigned long long>::is_signed;
const int numeric_limits<unsigned long long>::digits;
const int numeric_limits<unsigned long long>::digits10;

const int numeric_limits<float>::digits;
const int numeric_limits<float>::digits10;
const int numeric_limits<float>::max_exponent;
const int numeric_limits<float>::max_exponent10;
const int numeric_limits<float>::min_exponent;
const int numeric_limits<float>::min_exponent10;

const int numeric_limits<double>::digits;
const int numeric_limits<double>::digits10;
const int numeric_limits<double>::max_exponent;
const int numeric_limits<double>::max_exponent10;
const int numeric_limits<double>::min_exponent;
const int numeric_limits<double>::min_exponent10;

const int numeric_limits<long double>::digits;
const int numeric_limits<long double>::digits10;
const int numeric_limits<long double>::max_exponent;
const int numeric_limits<long double>::max_exponent10;
const int numeric_limits<long double>::min_exponent;
const int numeric_limits<long double>::min_exponent10;
_STD_END
