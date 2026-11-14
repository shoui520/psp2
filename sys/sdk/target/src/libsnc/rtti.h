// rtti.h: the run-time type information classes of the C++ ABI (Itanium's, which ARM's
// EHABI adopts) that SNC's compiler emits for class, pointer and fundamental types, as the
// runtime (rtti.c) and the exception code (libc's cppsemantics.cpp) read them. The names
// are the C++ ABI's.
#ifndef _LIBSNC_RTTI_H
#define _LIBSNC_RTTI_H
#include <typeinfo>

namespace __cxxabiv1 {
class __fundamental_type_info
	: public _STD type_info
	{	// a fundamental type
public:
	virtual ~__fundamental_type_info() _THROW0();
	};

class __array_type_info
	: public _STD type_info
	{	// an array type
public:
	virtual ~__array_type_info() _THROW0();
	};

class __function_type_info
	: public _STD type_info
	{	// a function type
public:
	virtual ~__function_type_info() _THROW0();
	};

class __enum_type_info
	: public _STD type_info
	{	// an enumeration type
public:
	virtual ~__enum_type_info() _THROW0();
	};

class __class_type_info
	: public _STD type_info
	{	// a class with no base
public:
	virtual ~__class_type_info() _THROW0();
	};

class __si_class_type_info
	: public __class_type_info
	{	// a class with one public, non-virtual base at offset zero
public:
	virtual ~__si_class_type_info() _THROW0();

	const __class_type_info *__base_type;
	};

struct __base_class_type_info
	{	// a base of a class with several bases
	const __class_type_info *__base_type;
	long __offset_flags;

	enum __offset_flags_masks
		{	// __offset_flags: the base's offset, or its vtable slot when virtual
		__virtual_mask = 0x1,
		__public_mask = 0x2,
		__offset_shift = 8
		};
	};

class __vmi_class_type_info
	: public __class_type_info
	{	// a class with several, or virtual, or non-public bases
public:
	virtual ~__vmi_class_type_info() _THROW0();

	unsigned int __flags;
	unsigned int __base_count;
	__base_class_type_info __base_info[1];

	enum __flags_masks
		{	// __flags
		__non_diamond_repeat_mask = 0x1,	// a base class appears twice
		__diamond_shaped_mask = 0x2	// a virtual base appears twice
		};
	};

class __pbase_type_info
	: public _STD type_info
	{	// a pointer or a pointer to member
public:
	virtual ~__pbase_type_info() _THROW0();

	unsigned int __flags;
	const _STD type_info *__pointee;

	enum __masks
		{	// the qualifiers of the pointee
		__const_mask = 0x1,
		__volatile_mask = 0x2,
		__restrict_mask = 0x4,
		__incomplete_mask = 0x8,
		__incomplete_class_mask = 0x10
		};
	};

class __pointer_type_info
	: public __pbase_type_info
	{	// a pointer
public:
	virtual ~__pointer_type_info() _THROW0();
	};

class __pointer_to_member_type_info
	: public __pbase_type_info
	{	// a pointer to member
public:
	virtual ~__pointer_to_member_type_info() _THROW0();

	const __class_type_info *__context;
	};
}	// namespace __cxxabiv1

#endif /* _LIBSNC_RTTI_H */
