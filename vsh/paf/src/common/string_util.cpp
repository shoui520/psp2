/*
 * paf::common::String: an empty UTF-8 half means the string lives in the UTF-16
 * half, and the other is converted from it on request.
 */
#define PAF_COMMON_STRING_UTIL_CPP
#include <common/string_util.h>
#include <std/stdio.h>
#include "../std/math.h"
#include <stdarg.h>

namespace paf {

/* Sony's Split calls push_back rather than inlining it. */
template<>
inline __attribute__((noinline)) int
vector<common::String>::push_back(const common::String &val)
{
	int result;

	if (count + 1 <= limit || (result = reserve(limit + 10)) != 0) {
		new (&ptr[count]) common::String(val);
		result = ++count;
	}
	return result;
}

namespace common {

/* 10 to the power -1 - i, and 10 to the power 1 + i */
static const float s_negative_power[10] = {
	1e-1f, 1e-2f, 1e-3f, 1e-4f, 1e-5f, 1e-6f, 1e-7f, 1e-8f, 1e-9f, 1e-10f
};

static const float s_positive_power[10] = {
	1e1f, 1e2f, 1e3f, 1e4f, 1e5f, 1e6f, 1e7f, 1e8f, 1e9f, 1e10f
};

string FormatString(const char *format, ...)
{
	va_list args;
	vector<char> buf(256);

	va_start(args, format);
	sce_paf_vsnprintf(buf.ptr, buf.count, format, args);
	return string(buf.ptr);
}

string FormatBytesize(size_t size, int unit)
{
	if ((size & 0xFFF00000) == size)
		return FormatString("%dM", size >> 20);
	if ((size & ~0x3FF) == size)
		return FormatString("%dK", size >> 10);
	if (size > 0x100000)
		return FormatString("%.2fM (%d bytes)", (double)((float)size / 1048576.0f), size);
	if (size > 0x400)
		return FormatString("%.2fK (%d bytes)", (double)((float)size / 1024.0f), size);
	return FormatString("%d bytes", size);
}

/* special spells zeros, infinities and NaN out itself, as the parser reads them */
string ToString(float value, bool suffix, bool trim, bool special)
{
	char buf[64];

	if (special) {
		union {
			float value;
			struct {
				uint32_t mantissa : 23;
				uint32_t exponent : 8;
				uint32_t sign : 1;
			} bits;
		} number;

		number.value = value;

		uint32_t exponent = number.bits.exponent;
		uint32_t mantissa = number.bits.mantissa;
		uint32_t sign = number.bits.sign;

		if (exponent == 0 && mantissa == 0 && sign == 0)
			return string(suffix ? "0.f" : "0");
		if (exponent == 0 && mantissa == 0 && sign == 1)
			return string(suffix ? "-0.f" : "-0");
		if (exponent == 255 && mantissa == 0 && sign == 0)
			return string("+INF");
		if (exponent == 255 && mantissa == 0 && sign == 1)
			return string("-INF");
		if (exponent == 255 && mantissa != 0)
			return string("NaN");
	}

	sce_paf_snprintf(buf, sizeof(buf), "%g", (double)value);

	/* drop the zeros after the point, and the point when nothing is left after it */
	if (trim) {
		int dot = -1;
		int exponent = -1;
		int len = 0;

		while (buf[len] != 0) {
			if (buf[len] == '.')
				dot = len;
			if (buf[len] == 'e')
				exponent = len;
			++len;
		}
		if (exponent == -1 && len > 1 && dot != -1) {
			int last = len - 1;

			while (dot < last && buf[last] == '0') {
				buf[last] = 0;
				--last;
			}
			if (dot == last && dot != 0)
				buf[last] = 0;
		}
	}

	/* a float literal: "1.5f", or "1.f" for a whole number */
	if (suffix) {
		int len;
		int decimals = 0;

		for (len = 0; len < 64; ++len) {
			if (buf[len] == 0)
				break;
			if (buf[len] == '.' || decimals != 0)
				++decimals;
		}
		if (decimals > 1 && len < 63) {
			buf[len] = 'f';
			buf[len + 1] = 0;
		}
		if (decimals == 0 && len < 62) {
			buf[len] = '.';
			buf[len + 1] = 'f';
			buf[len + 2] = 0;
		}
	}
	return string(buf);
}

/*
 * parts names what to keep of path: P the directory, F the file name without
 * its extension, E the extension, in any of the orders PFE, PF, FE, P, F, E.
 */
string StripFilename(const string &path, const string &parts)
{
	if (path == "" || parts == "")
		return path;

	size_t slash = path.rfind('/');
	size_t backslash = path.rfind('\\');
	size_t split;

	if (slash == string::npos && backslash == string::npos)
		split = string::npos;
	else if (slash != string::npos && backslash == string::npos)
		split = slash;
	else if (slash == string::npos && backslash != string::npos)
		split = backslash;
	else
		split = slash < backslash ? backslash : slash;

	string separator("");

	if (split != string::npos) {
		char ch[2];

		ch[0] = 0;
		ch[1] = 0;
		ch[0] = path.ptr[split];
		separator = ch;
	}

	string directory;

	if (split != string::npos)
		directory = string(path, 0, split);

	string filename(path);

	if (split != string::npos)
		filename = string(path, split + 1, path.count - split - 1);

	size_t dot = filename.rfind('.');
	string basename(filename);
	string extension;

	if (dot != string::npos) {
		basename = string(filename, 0, dot);
		extension = string(filename, dot + 1, filename.count - dot - 1);
	}

	if (parts == "PFE")
		return directory + separator + basename +
		       (extension.count != 0 ? "." + extension : string(""));
	if (parts == "PF")
		return directory + (basename.count != 0 ? separator + basename : string(""));
	if (parts == "FE")
		return basename + (extension.count != 0 ? "." + extension : string(""));
	if (parts == "P")
		return directory;
	if (parts == "F")
		return basename;
	if (parts == "E")
		return extension;
	return path;
}

bool Rmatch(const string &str, const string &suffix)
{
	int len = str.count;
	int matched = 0;

	if (len == 0)
		return false;

	int suffix_len = suffix.count;
	int at = len - suffix_len;

	if (suffix_len <= 0)
		return true;
	while (at >= 0 && at < len && str.ptr[at] == suffix.ptr[matched]) {
		++matched;
		++at;
		if (matched >= suffix_len)
			return true;
	}
	return false;
}

string Subst(const string &str, const string &from, const string &to, size_t pos)
{
	string result(str);

	for (;;) {
		size_t found = result.find(from.ptr, pos, from.count);

		if (found == string::npos)
			break;

		string head(result, 0, found);
		string tail(result, found + from.count, string::npos);

		pos = head.count + to.count;
		result = head + to + tail;
	}
	return result;
}

/* Sony's sign test reads the second character for the plus */
template<>
int FromString<int>(const char *str, size_t len)
{
	if (len == 0)
		return 0;

	int sign = 1;
	int scale = 1;

	if (str[0] == '-')
		sign = -1;

	int first = 0;

	if (str[0] == '-' || str[1] == '+')
		first = 1;

	int value = 0;

	for (int i = len - 1; i >= first; --i) {
		int digit = 0;

		if (str[i] >= '0' && str[i] <= '9')
			digit = str[i] - '0';
		value += scale * digit;
		scale *= 10;
	}
	return value * sign;
}

/* the exponent's power tables are one place off: e1 multiplies by 100 */
template<>
float FromString<float>(const char *str, size_t len)
{
	float fraction = 0.0f;

	if (len == 0 || str == NULL || *str == 0)
		return fraction;

	int state = 0;
	float value = fraction;
	int exponent = 0;
	bool negative = false;
	int decimals = 0;
	bool exponent_negative = false;

	for (size_t i = 0; i != len; ++i, ++str) {
		int ch = *str;
		int digit = ch - '0';
		bool is_digit = digit >= 0 && digit <= 9;

		switch (state) {
		case 0:
			if (ch == '-')
				negative = true;
			else if (ch == '+')
				negative = false;
			else if (ch == '.')
				state = 1;
			else if (ch == 'e' || ch == 'E')
				state = 2;
			else if (is_digit)
				value = value * 10.0f + digit;
			break;
		case 1:
			if (ch == 'e' || ch == 'E') {
				state = 2;
			} else if (is_digit) {
				float power;

				if (decimals < 10)
					power = s_negative_power[decimals];
				else
					power = sce_paf_powf(10.0f, -(float)(decimals + 1));
				++decimals;
				fraction += digit * power;
			}
			break;
		case 2:
			if (ch == '-')
				exponent_negative = true;
			else if (ch == '+')
				exponent_negative = false;
			else if (is_digit)
				exponent = exponent * 10 + digit;
			break;
		}
	}

	value += fraction;
	if (negative)
		value = -value;
	if (exponent != 0) {
		float power;

		if (exponent_negative) {
			if (exponent < 10)
				power = s_negative_power[exponent];
			else
				power = sce_paf_powf(10.0f, -(float)(exponent + 1));
		} else {
			if (exponent < 10)
				power = s_positive_power[exponent];
			else
				power = sce_paf_powf(10.0f, (float)(exponent + 1));
		}
		value *= power;
	}
	return value;
}

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
