/*
 * paf::common::String keeps a string in UTF-8 or UTF-16, whichever it was given,
 * and converts on request. string_util splits strings into tokens. The free
 * functions format and parse numbers and cut paths and strings apart.
 */
#ifndef PAF_COMMON_STRING_UTIL_H
#define PAF_COMMON_STRING_UTIL_H

#include <std/string>
#include <std/vector>
#include <std/list>
#include <std/map>
#include <common/char_encoding.h>

namespace paf {
namespace common {

class String {
public:
	/*
	 * The exported members are defined in string_util.cpp, which inlines
	 * them; every other unit calls them.
	 */
#ifdef PAF_COMMON_STRING_UTIL_CPP
	__declspec(dllexport) String() : m_string(""), m_wstring(L"")
	{
	}

	__declspec(dllexport) String(const char *str) : m_string(str), m_wstring(L"")
	{
	}

	__declspec(dllexport) String(const wchar_t *str) : m_string(""), m_wstring(str)
	{
	}

	__declspec(dllexport) String(const string &str) : m_string(str), m_wstring(L"")
	{
	}

	__declspec(dllexport) String(const wstring &str) : m_string(""), m_wstring(str)
	{
	}

	__declspec(dllexport) ~String()
	{
	}

	__declspec(dllexport) string GetString() const
	{
		string result(m_string);

		if (result.length() == 0 && m_wstring.length() != 0)
			Utf16ToUtf8(m_wstring, &result);
		return result;
	}

	__declspec(dllexport) wstring GetWString() const
	{
		wstring result(m_wstring);

		if (result.length() == 0 && m_string.length() != 0)
			Utf8ToUtf16(m_string, &result, 0);
		return result;
	}
#else
	String();
	String(const char *str);
	String(const wchar_t *str);
	String(const string &str);
	String(const wstring &str);
	~String();
	string GetString() const;
	wstring GetWString() const;
#endif

	String(const String &src) : m_string(src.m_string), m_wstring(src.m_wstring)
	{
	}

	String &operator=(const String &src)
	{
		m_string = src.m_string;
		m_wstring = src.m_wstring;
		return *this;
	}

	String operator+(const String &str);
	bool operator==(const String &str);
	bool operator!=(const String &str);
	String &operator+=(const String &str);

	void SetFormattedString(const char *format, ...);
	void SetFormattedString(const wchar_t *format, ...);

	vector<String> Split(char separator);
	vector<String> Split(const string &separator);

	string m_string;
	wstring m_wstring;
};

string FormatString(const char *format, ...);
string FormatBytesize(size_t size, int unit);
string ToString(float value, bool suffix, bool trim, bool special);
string StripFilename(const string &path, const string &parts);
bool Rmatch(const string &str, const string &suffix);
string Subst(const string &str, const string &from, const string &to, size_t pos);

template<class _Ty>
_Ty FromString(const char *str, size_t len);

template<>
int FromString<int>(const char *str, size_t len);

template<>
float FromString<float>(const char *str, size_t len);

namespace string_util {

void tokenize(list<string> &tokens, const string &str, size_t pos, char delimiter);
void mapping(map<string, string> &pairs, const string &str, size_t pos, char delimiter,
	     char separator);
void dissolve(list<string> &tokens, const string &str, size_t pos, char open, char close);
string ToString(const void *data, size_t size);

}

}
}

#endif
