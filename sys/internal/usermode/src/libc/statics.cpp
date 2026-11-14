// statics.cpp: the library's copies of the templates' static data members it exports,
// _Iosb<int>'s flags and the strings' npos (the file name is ours)
#include <string>
_STD_BEGIN

template const _Iosb<int>::_Fmtflags _Iosb<int>::skipws;
template const _Iosb<int>::_Fmtflags _Iosb<int>::unitbuf;
template const _Iosb<int>::_Fmtflags _Iosb<int>::dec;
template const _Iosb<int>::_Fmtflags _Iosb<int>::oct;
template const _Iosb<int>::_Fmtflags _Iosb<int>::hex;
template const _Iosb<int>::_Fmtflags _Iosb<int>::basefield;
template const _Iosb<int>::_Iostate _Iosb<int>::failbit;
template const _Iosb<int>::_Iostate _Iosb<int>::badbit;
template const _Iosb<int>::_Openmode _Iosb<int>::in;
template const _Iosb<int>::_Openmode _Iosb<int>::out;
template const _Iosb<int>::_Openmode _Iosb<int>::ate;
template const _Iosb<int>::_Openmode _Iosb<int>::app;
template const _Iosb<int>::_Openmode _Iosb<int>::trunc;
template const _Iosb<int>::_Openmode _Iosb<int>::_Nocreate;
template const _Iosb<int>::_Openmode _Iosb<int>::_Noreplace;
template const _Iosb<int>::_Openmode _Iosb<int>::binary;
template const _Iosb<int>::_Seekdir _Iosb<int>::cur;
template const _Iosb<int>::_Seekdir _Iosb<int>::end;

template<class _Elem, class _Traits, class _Alloc>
	const typename basic_string<_Elem, _Traits, _Alloc>::size_type
		basic_string<_Elem, _Traits, _Alloc>::npos;

template const string::size_type string::npos;
template const wstring::size_type wstring::npos;
_STD_END
