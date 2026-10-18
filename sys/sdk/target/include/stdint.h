/*
 * stdint.h: the SDK's integer types, with Sony's psp2 definitions (SDK Release 03.570.011,
 * target/include/stdint.h and yvals.h): the 32-bit types are int and unsigned int, pointers
 * fit int.
 */
#ifndef _STDINT
#define _STDINT

/* exact width */
#if !defined(__BIT_TYPES_DEFINED__) && !defined(_MACHTYPES_H_) && !defined(__int8_t_defined)
#define __int8_t_defined
typedef signed char		int8_t;
typedef short			int16_t;
typedef int			int32_t;
#endif
typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned int		uint32_t;
typedef long long		int64_t;
typedef unsigned long long	uint64_t;

/* least width */
typedef signed char		int_least8_t;
typedef short			int_least16_t;
typedef int			int_least32_t;
typedef long long		int_least64_t;
typedef unsigned char		uint_least8_t;
typedef unsigned short		uint_least16_t;
typedef unsigned int		uint_least32_t;
typedef unsigned long long	uint_least64_t;

/* fastest */
typedef signed char		int_fast8_t;
typedef short			int_fast16_t;
typedef int			int_fast32_t;
typedef long long		int_fast64_t;
typedef unsigned char		uint_fast8_t;
typedef unsigned short		uint_fast16_t;
typedef unsigned int		uint_fast32_t;
typedef unsigned long long	uint_fast64_t;

/* pointers */
#if !defined(_UINTPTR_T_DEFINED)
#define _UINTPTR_T_DEFINED
typedef unsigned int		uintptr_t;
#endif
#if !defined(__intptr_t_defined) && !defined(_INTPTR_T_DEFINED)
#define _INTPTR_T_DEFINED
typedef int			intptr_t;
#endif

/* greatest width */
#if !defined(_INTMAXT)
#define _INTMAXT
typedef long long		intmax_t;
typedef unsigned long long	uintmax_t;
#endif

#if !defined(__cplusplus) || defined(__STDC_LIMIT_MACROS)
#define INT8_MIN		(-0x7f - 1)
#define INT16_MIN		(-0x7fff - 1)
#define INT32_MIN		(-0x7fffffff - 1)
#define INT64_MIN		(-0x7fffffffffffffffLL - 1)
#define INT8_MAX		0x7f
#define INT16_MAX		0x7fff
#define INT32_MAX		0x7fffffff
#define INT64_MAX		0x7fffffffffffffffLL
#define UINT8_MAX		0xff
#define UINT16_MAX		0xffff
#define UINT32_MAX		0xffffffff
#define UINT64_MAX		0xffffffffffffffffULL

#define INT_LEAST8_MIN		(-0x7f - 1)
#define INT_LEAST16_MIN		(-0x7fff - 1)
#define INT_LEAST32_MIN		(-0x7fffffff - 1)
#define INT_LEAST64_MIN		(-0x7fffffffffffffffLL - 1)
#define INT_LEAST8_MAX		0x7f
#define INT_LEAST16_MAX		0x7fff
#define INT_LEAST32_MAX		0x7fffffff
#define INT_LEAST64_MAX		0x7fffffffffffffffLL
#define UINT_LEAST8_MAX		0xff
#define UINT_LEAST16_MAX	0xffff
#define UINT_LEAST32_MAX	0xffffffff
#define UINT_LEAST64_MAX	0xffffffffffffffffULL

#define INT_FAST8_MIN		(-0x7f - 1)
#define INT_FAST16_MIN		(-0x7fff - 1)
#define INT_FAST32_MIN		(-0x7fffffff - 1)
#define INT_FAST64_MIN		(-0x7fffffffffffffffLL - 1)
#define INT_FAST8_MAX		0x7f
#define INT_FAST16_MAX		0x7fff
#define INT_FAST32_MAX		0x7fffffff
#define INT_FAST64_MAX		0x7fffffffffffffffLL
#define UINT_FAST8_MAX		0xff
#define UINT_FAST16_MAX		0xffff
#define UINT_FAST32_MAX		0xffffffff
#define UINT_FAST64_MAX		0xffffffffffffffffULL

#define INTPTR_MIN		(-INTPTR_MAX - 1)
#define INTPTR_MAX		0x7fffffff
#define UINTPTR_MAX		0xffffffff

#define INTMAX_MIN		(-0x7fffffffffffffffLL - 1)
#define INTMAX_MAX		0x7fffffffffffffffLL
#define UINTMAX_MAX		0xffffffffffffffffULL

#define PTRDIFF_MIN		INT32_MIN
#define PTRDIFF_MAX		INT32_MAX
#define SIZE_MAX		UINT32_MAX

#define WCHAR_MIN		0x0
#define WCHAR_MAX		0xffff
#define WINT_MIN		0x0
#define WINT_MAX		0xffffffffU
#endif

#if !defined(__cplusplus) || defined(__STDC_CONSTANT_MACROS)
#define INT8_C(x)		x
#define INT16_C(x)		x
#define INT32_C(x)		x
#define INT64_C(x)		x ## LL
#define UINT8_C(x)		x ## U
#define UINT16_C(x)		x ## U
#define UINT32_C(x)		x ## U
#define UINT64_C(x)		x ## ULL
#define INTMAX_C(x)		x ## LL
#define UINTMAX_C(x)		x ## ULL
#endif

#endif /* _STDINT */
