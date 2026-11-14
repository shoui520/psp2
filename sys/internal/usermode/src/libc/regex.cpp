// regex.cpp: regex_error's destructor, which the psp2 headers declare out of line, and the
// character class names regex_traits looks up
#define _REGEX_BUILD	1	/* the class name tables are defined here */
#include <regex>

_STD_BEGIN
extern _Cl_names<char> _Cl_charnames[];
extern _Cl_names<wchar_t> _Cl_wcharnames[];

template<>
	_Cl_names<char> *_Regex_traits<char>::_Names = _Cl_charnames;
template<>
	_Cl_names<wchar_t> *_Regex_traits<wchar_t>::_Names = _Cl_wcharnames;

_Cl_names<char> _Cl_charnames[] =
	{	// the names [:name:] and the escapes \d, \s and \w look up, with their masks
	{"alnum", 5, _ctype_base::alnum},
	{"alpha", 5, _ctype_base::alpha},
	{"blank", 5, _ctype_base::blank},
	{"cntrl", 5, _ctype_base::cntrl},
	{"d", 1, _ctype_base::digit},
	{"digit", 5, _ctype_base::digit},
	{"graph", 5, _ctype_base::graph},
	{"lower", 5, _ctype_base::lower},
	{"print", 5, _ctype_base::print},
	{"punct", 5, _ctype_base::punct},
	{"space", 5, _ctype_base::space},
	{"s", 1, _ctype_base::space},
	{"upper", 5, _ctype_base::upper},
	{"w", 1, (_ctype_base::mask)-1},
	{"xdigit", 6, _ctype_base::xdigit},
	{0, 0, 0}};

_Cl_names<wchar_t> _Cl_wcharnames[] =
	{	// the names [:name:] and the escapes \d, \s and \w look up, with their masks
	{L"alnum", 5, _ctype_base::alnum},
	{L"alpha", 5, _ctype_base::alpha},
	{L"blank", 5, _ctype_base::blank},
	{L"cntrl", 5, _ctype_base::cntrl},
	{L"d", 1, _ctype_base::digit},
	{L"digit", 5, _ctype_base::digit},
	{L"graph", 5, _ctype_base::graph},
	{L"lower", 5, _ctype_base::lower},
	{L"print", 5, _ctype_base::print},
	{L"punct", 5, _ctype_base::punct},
	{L"space", 5, _ctype_base::space},
	{L"s", 1, _ctype_base::space},
	{L"upper", 5, _ctype_base::upper},
	{L"w", 1, (_ctype_base::mask)-1},
	{L"xdigit", 6, _ctype_base::xdigit},
	{0, 0, 0}};

regex_error::~regex_error() _NOEXCEPT
	{	// destroy the object
	}
_STD_END
