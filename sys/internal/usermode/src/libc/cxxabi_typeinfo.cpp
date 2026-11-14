// cxxabi_typeinfo.cpp: the destructors of the C++ ABI's run-time type information classes,
// which give each class its virtual table and type_info
#include <typeinfo>
#include "cxxabi_internal.h"

namespace __cxxabiv1 {
__fundamental_type_info::~__fundamental_type_info() _THROW0()
	{	// destroy the object
	}

__array_type_info::~__array_type_info() _THROW0()
	{	// destroy the object
	}

__function_type_info::~__function_type_info() _THROW0()
	{	// destroy the object
	}

__enum_type_info::~__enum_type_info() _THROW0()
	{	// destroy the object
	}

__class_type_info::~__class_type_info() _THROW0()
	{	// destroy the object
	}

__si_class_type_info::~__si_class_type_info() _THROW0()
	{	// destroy the object
	}

__vmi_class_type_info::~__vmi_class_type_info() _THROW0()
	{	// destroy the object
	}

__pbase_type_info::~__pbase_type_info() _THROW0()
	{	// destroy the object
	}

__pointer_type_info::~__pointer_type_info() _THROW0()
	{	// destroy the object
	}

__pointer_to_member_type_info::~__pointer_to_member_type_info() _THROW0()
	{	// destroy the object
	}
}	// namespace __cxxabiv1

