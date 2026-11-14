// typeinfo.cpp: bad_cast, bad_typeid and type_info, whose members the psp2 headers declare
// out of line
#include <typeinfo>

_STD_BEGIN
bad_cast::bad_cast() _THROW0()
	{	// construct with no message string
	}

bad_cast::bad_cast(const bad_cast& _Right) _THROW0()
	: exception(_Right)
	{	// copy construct
	}

bad_cast& bad_cast::operator=(const bad_cast& _Right) _THROW0()
	{	// assign
	exception::operator=(_Right);
	return (*this);
	}

bad_cast::~bad_cast() _THROW0()
	{	// destroy the object
	}

const char *bad_cast::what() const _THROW0()
	{	// report a bad cast
	return ("bad cast");
	}

bad_typeid::bad_typeid() _THROW0()
	{	// construct with no message string
	}

bad_typeid::bad_typeid(const bad_typeid& _Right) _THROW0()
	: exception(_Right)
	{	// copy construct
	}

bad_typeid& bad_typeid::operator=(const bad_typeid& _Right) _THROW0()
	{	// assign
	exception::operator=(_Right);
	return (*this);
	}

bad_typeid::~bad_typeid() _THROW0()
	{	// destroy the object
	}

const char *bad_typeid::what() const _THROW0()
	{	// report a bad typeid
	return ("bad typeid");
	}

type_info::~type_info() _NOEXCEPT
	{	// destroy the object
	}

bool type_info::before(const type_info& _Right) const _NOEXCEPT
	{	// test if this precedes arg
	return (_Myname < _Right._Myname);
	}

bool type_info::operator==(const type_info& _Right) const _NOEXCEPT
	{	// test for equality
	return (_Myname == _Right._Myname);
	}

bool type_info::operator!=(const type_info& _Right) const _NOEXCEPT
	{	// test for inequality
	return (_Myname != _Right._Myname);
	}

const char *type_info::name() const _NOEXCEPT
	{	// return name of type
	return (_Myname);
	}
_STD_END

	// THE TYPE_INFO OF THE FUNDAMENTAL TYPES
/*
 * The type_info objects of the fundamental types, of pointers to them and of pointers to
 * const, each followed by its name: Sony's object of type_info's members holds them, ahead of
 * type_info's own. Their classes are polymorphic, so the objects are written as data with the
 * classes' layout: the virtual table pointer, the name, and for a pointer its qualifier flags
 * (__pbase_type_info::__const_mask) and the pointee.
 */
struct _Fundamental_info
	{	// __cxxabiv1::__fundamental_type_info
	const void *_Vptr;
	const char *_Name;
	};

struct _Pointer_info
	{	// __cxxabiv1::__pointer_type_info
	const void *_Vptr;
	const char *_Name;
	unsigned int _Flags;
	const _Fundamental_info *_Pointee;
	};

extern "C" {
extern const void *const _ZTVN10__cxxabiv123__fundamental_type_infoE[];
extern const void *const _ZTVN10__cxxabiv119__pointer_type_infoE[];

#define _FUNDAMENTAL_VPTR	(&_ZTVN10__cxxabiv123__fundamental_type_infoE[2])
#define _POINTER_VPTR		(&_ZTVN10__cxxabiv119__pointer_type_infoE[2])

#define _BUILTIN_TYPE_INFO(code) \
	extern const char _ZTS##code[], _ZTSP##code[], _ZTSPK##code[]; \
	extern const _Fundamental_info _ZTI##code = { _FUNDAMENTAL_VPTR, _ZTS##code }; \
	extern const char _ZTS##code[] = #code; \
	extern const _Pointer_info _ZTIP##code = { _POINTER_VPTR, _ZTSP##code, 0, &_ZTI##code }; \
	extern const char _ZTSP##code[] = "P" #code; \
	extern const _Pointer_info _ZTIPK##code = { _POINTER_VPTR, _ZTSPK##code, 1, &_ZTI##code }; \
	extern const char _ZTSPK##code[] = "PK" #code;

_BUILTIN_TYPE_INFO(v)	/* void */
_BUILTIN_TYPE_INFO(b)	/* bool */
_BUILTIN_TYPE_INFO(w)	/* wchar_t */
_BUILTIN_TYPE_INFO(c)	/* char */
_BUILTIN_TYPE_INFO(a)	/* signed char */
_BUILTIN_TYPE_INFO(h)	/* unsigned char */
_BUILTIN_TYPE_INFO(s)	/* short */
_BUILTIN_TYPE_INFO(t)	/* unsigned short */
_BUILTIN_TYPE_INFO(i)	/* int */
_BUILTIN_TYPE_INFO(j)	/* unsigned int */
_BUILTIN_TYPE_INFO(l)	/* long */
_BUILTIN_TYPE_INFO(m)	/* unsigned long */
_BUILTIN_TYPE_INFO(x)	/* long long */
_BUILTIN_TYPE_INFO(y)	/* unsigned long long */
_BUILTIN_TYPE_INFO(f)	/* float */
_BUILTIN_TYPE_INFO(d)	/* double */
_BUILTIN_TYPE_INFO(e)	/* long double */
_BUILTIN_TYPE_INFO(Dh)	/* __fp16 */
_BUILTIN_TYPE_INFO(15__simd64_int8_t)	/* NEON __simd64_int8_t */
_BUILTIN_TYPE_INFO(16__simd64_uint8_t)	/* NEON __simd64_uint8_t */
_BUILTIN_TYPE_INFO(16__simd64_int16_t)	/* NEON __simd64_int16_t */
_BUILTIN_TYPE_INFO(17__simd64_uint16_t)	/* NEON __simd64_uint16_t */
_BUILTIN_TYPE_INFO(16__simd64_int32_t)	/* NEON __simd64_int32_t */
_BUILTIN_TYPE_INFO(17__simd64_uint32_t)	/* NEON __simd64_uint32_t */
_BUILTIN_TYPE_INFO(16__simd64_int64_t)	/* NEON __simd64_int64_t */
_BUILTIN_TYPE_INFO(17__simd64_uint64_t)	/* NEON __simd64_uint64_t */
_BUILTIN_TYPE_INFO(18__simd64_float16_t)	/* NEON __simd64_float16_t */
_BUILTIN_TYPE_INFO(18__simd64_float32_t)	/* NEON __simd64_float32_t */
_BUILTIN_TYPE_INFO(16__simd64_poly8_t)	/* NEON __simd64_poly8_t */
_BUILTIN_TYPE_INFO(17__simd64_poly16_t)	/* NEON __simd64_poly16_t */
_BUILTIN_TYPE_INFO(16__simd128_int8_t)	/* NEON __simd128_int8_t */
_BUILTIN_TYPE_INFO(17__simd128_uint8_t)	/* NEON __simd128_uint8_t */
_BUILTIN_TYPE_INFO(17__simd128_int16_t)	/* NEON __simd128_int16_t */
_BUILTIN_TYPE_INFO(18__simd128_uint16_t)	/* NEON __simd128_uint16_t */
_BUILTIN_TYPE_INFO(17__simd128_int32_t)	/* NEON __simd128_int32_t */
_BUILTIN_TYPE_INFO(18__simd128_uint32_t)	/* NEON __simd128_uint32_t */
_BUILTIN_TYPE_INFO(17__simd128_int64_t)	/* NEON __simd128_int64_t */
_BUILTIN_TYPE_INFO(18__simd128_uint64_t)	/* NEON __simd128_uint64_t */
_BUILTIN_TYPE_INFO(19__simd128_float16_t)	/* NEON __simd128_float16_t */
_BUILTIN_TYPE_INFO(19__simd128_float32_t)	/* NEON __simd128_float32_t */
_BUILTIN_TYPE_INFO(17__simd128_poly8_t)	/* NEON __simd128_poly8_t */
_BUILTIN_TYPE_INFO(18__simd128_poly16_t)	/* NEON __simd128_poly16_t */
_BUILTIN_TYPE_INFO(Dn)	/* nullptr_t */
_BUILTIN_TYPE_INFO(Ds)	/* char16_t */
_BUILTIN_TYPE_INFO(Di)	/* char32_t */

/*
 * Sony's object also carries a function nothing calls, which keeps the address of the last
 * of them (const char32_t *) in a variable of its own (names ours).
 */
static const _Pointer_info *_Last_builtin_info;

void _Set_last_builtin_info(void)
	{	// store the address of the last builtin type_info
	_Last_builtin_info = &_ZTIPKDi;
	}
}
