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
