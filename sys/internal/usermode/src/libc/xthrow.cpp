// xthrow.cpp: the library's out-of-line throws, which keep the exception code out of the
// templates that report errors.
#include <new>
#include <stdexcept>
#include <functional>
#include <regex>
_STD_BEGIN

void _Xlength_error(const char *_Message)
	{	// report a length_error
	_THROW_N(length_error, _Message);
	}

void _Xout_of_range(const char *_Message)
	{	// report an out_of_range error
	_THROW_N(out_of_range, _Message);
	}

void _Xbad_alloc()
	{	// report a bad_alloc (constructed from a null message)
	_THROW_N(bad_alloc, (const char *)0);
	}

void _Xinvalid_argument(const char *_Message)
	{	// report an invalid_argument error
	_THROW_N(invalid_argument, _Message);
	}

void _Xoverflow_error(const char *_Message)
	{	// report an overflow error
	_THROW_N(overflow_error, _Message);
	}

void _Xruntime_error(const char *_Message)
	{	// report a runtime_error
	_THROW_N(runtime_error, _Message);
	}

void _Xbad_function_call()
	{	// report a bad_function_call
	_THROW_N(bad_function_call, _EMPTY_ARGUMENT);
	}

void _Xregex_error(regex_constants::error_type _Code)
	{	// report a regex_error
	_THROW_N(regex_error, _Code);
	}

// the string assignment the exception constructors call, out of line (exported)
template basic_string<char, char_traits<char>, allocator<char> >&
	basic_string<char, char_traits<char>, allocator<char> >::assign(
		const basic_string<char, char_traits<char>, allocator<char> >&, size_type, size_type);
_STD_END
