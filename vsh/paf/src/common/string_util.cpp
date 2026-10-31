/*
 * paf::common::String: an empty UTF-8 half means the string lives in the UTF-16
 * half, and the other is converted from it on request.
 */
#include <common/string_util.h>
#include <std/stdio.h>
#include <stdarg.h>

namespace paf {
namespace common {

String String::operator+(const String &str)
{
	String result(string(""));

	if (m_string.length() != 0) {
		string other = str.GetString();

		result = String(m_string + other);
	} else if (m_wstring.length() != 0) {
		wstring other = str.GetWString();

		result = String(m_wstring + other);
	}
	return result;
}

bool String::operator==(const String &str)
{
	bool result = false;

	if (m_string.length() != 0) {
		string other = str.GetString();

		result = m_string == other;
	} else if (m_wstring.length() != 0) {
		wstring other = str.GetWString();

		result = m_wstring == other;
	}
	return result;
}

bool String::operator!=(const String &str)
{
	bool result = false;

	if (m_string.length() != 0) {
		string other = str.GetString();

		result = m_string != other;
	} else if (m_wstring.length() != 0) {
		wstring other = str.GetWString();

		result = m_wstring != other;
	}
	return result;
}

String &String::operator+=(const String &str)
{
	if (m_string.length() != 0) {
		string other = str.GetString();

		m_string = m_string + other;
	} else if (m_wstring.length() != 0) {
		wstring other = str.GetWString();

		m_wstring = m_wstring + other;
	}
	return *this;
}

void String::SetFormattedString(const char *format, ...)
{
	va_list args;
	char dummy[4];

	va_start(args, format);

	vector<char> buffer(sce_paf_vsnprintf(dummy, 0, format, args) + 1);

	sce_paf_vsnprintf(buffer.ptr, buffer.count, format, args);
	m_string = string(buffer.ptr);
	va_end(args);
}

void String::SetFormattedString(const wchar_t *format, ...)
{
	va_list args;
	wchar_t dummy;

	va_start(args, format);

	vector<wchar_t> buffer(sce_paf_vswprintf(&dummy, 0, format, args) + 1);

	sce_paf_vswprintf(buffer.ptr, buffer.count, format, args);
	m_wstring = wstring(buffer.ptr);
	va_end(args);
}

void string_util::tokenize(list<string> &tokens, const string &str, size_t pos, char delimiter)
{
	size_t start = str.find_first_not_of(delimiter, pos);

	while (start != string::npos) {
		size_t end = str.find(delimiter, start);

		if (end == string::npos) {
			tokens.push_back(str.substr(start, string::npos));
			break;
		}
		tokens.push_back(str.substr(start, end - start));
		start = str.find_first_not_of(delimiter, end + 1);
	}
}

vector<String> String::Split(char separator)
{
	vector<String> result;
	list<string> tokens;

	string_util::tokenize(tokens, GetString(), 0, separator);
	for (list<string>::iterator it = tokens.begin(); it != tokens.end(); ++it)
		result.push_back(String(*it));
	return result;
}

vector<String> String::Split(const string &separator)
{
	vector<String> result;

	if (separator.length() != 0)
		result = Split(separator.c_str()[0]);
	return result;
}

void string_util::mapping(map<string, string> &pairs, const string &str, size_t pos,
			  char delimiter, char separator)
{
	list<string> tokens;

	tokenize(tokens, str, pos, delimiter);
	for (list<string>::iterator it = tokens.begin(); it != tokens.end(); ++it) {
		size_t at = it->find(separator);

		if (at != string::npos)
			pairs[it->substr(0, at)] = it->substr(at + 1, string::npos);
		else
			pairs[*it] = string();
	}
}

void string_util::dissolve(list<string> &tokens, const string &str, size_t pos, char open,
			   char close)
{
	size_t length = str.length();

	while (pos < length) {
		size_t start = str.find(open, pos);

		if (start == string::npos)
			break;

		size_t end = str.find(close, start);

		if (end == string::npos)
			break;

		size_t first = str.find_first_not_of(open, start);

		if (start + 1 != first)
			start = first - 1;
		if (start != pos)
			tokens.push_back(str.substr(pos, start - pos));
		tokens.push_back(str.substr(start, end - start + 1));
		pos = end + 1;
	}
	if (pos < length)
		tokens.push_back(str.substr(pos, string::npos));
}

static const char s_hex[] =
	"000102030405060708090A0B0C0D0E0F"
	"101112131415161718191A1B1C1D1E1F"
	"202122232425262728292A2B2C2D2E2F"
	"303132333435363738393A3B3C3D3E3F"
	"404142434445464748494A4B4C4D4E4F"
	"505152535455565758595A5B5C5D5E5F"
	"606162636465666768696A6B6C6D6E6F"
	"707172737475767778797A7B7C7D7E7F"
	"808182838485868788898A8B8C8D8E8F"
	"909192939495969798999A9B9C9D9E9F"
	"A0A1A2A3A4A5A6A7A8A9AAABACADAEAF"
	"B0B1B2B3B4B5B6B7B8B9BABBBCBDBEBF"
	"C0C1C2C3C4C5C6C7C8C9CACBCCCDCECF"
	"D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF"
	"E0E1E2E3E4E5E6E7E8E9EAEBECEDEEEF"
	"F0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF";

string string_util::ToString(const void *data, size_t size)
{
	string result;
	const unsigned char *at = (const unsigned char *)data;

	for (const unsigned char *end = at + size; at != end; ++at)
		result.append(&s_hex[*at * 2], 2);
	return result;
}

}
}
