// xstring.cpp: _String_base, the base the library's older basic_string threw its length and
// position errors through. The SDK's headers have basic_string throw them itself; the class
// stays for the programs built against the older headers.
/*
 * Sony's build inlined less into the exception constructors <stdexcept> defines: its
 * length_error and out_of_range wrappers call logic_error(const string&) (exported), whose
 * own copy has the string copy inlined. SNC's explicit-inline budget for that header's
 * definitions reproduces it.
 */
#pragma control %push inlinesize=160
#include <stdexcept>
#pragma control %pop inlinesize
#include <string>

_STD_BEGIN
class _String_base {
public:
	_NO_RETURN_MEMBER(_Xlen()) const;
	_NO_RETURN_MEMBER(_Xran()) const;
};

void _String_base::_Xlen() const
{	// report a length_error
	throw length_error(string("string too long"));
}

void _String_base::_Xran() const
{	// report an out_of_range error
	throw out_of_range(string("invalid string position"));
}
_STD_END
