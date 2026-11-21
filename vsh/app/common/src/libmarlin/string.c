/*
 * The library's byte order and string helpers. The file's name is ours.
 */
#include "mln_internal.h"

unsigned short MlnHtons(unsigned int value)
{
	return (unsigned short)(value << 8 | value >> 8);
}

unsigned int MlnNtohl(unsigned int value)
{
	return (value << 24) + (value >> 24) + ((value & 0xff00) << 8) + ((value & 0xff0000) >> 8);
}

unsigned int MlnHtonl(unsigned int value)
{
	return (value << 24) + (value >> 24) + ((value & 0xff00) << 8) + ((value & 0xff0000) >> 8);
}

static inline __attribute__((always_inline)) unsigned int MlnStrlenInline(const char *str)
{
	const signed char *s = (const signed char *)str;
	unsigned int length = 0;

	while (s[length] != '\0')
		length++;
	return length;
}

__attribute__((noinline)) unsigned int MlnStrlen(const char *str)
{
	return MlnStrlenInline(str);
}

char *MlnStrncpy(char *dst, const char *src, unsigned int n)
{
	char *d = dst;
	char *end = dst + n;
	char c;

	do {
		if (d == end)
			break;
		c = *src++;
		*d++ = c;
	} while (c != '\0');
	return dst;
}

__attribute__((noinline)) int MlnStrcmp(const char *a, const char *b)
{
	const signed char *s1 = (const signed char *)a;
	const signed char *s2 = (const signed char *)b;
	unsigned int i;

	for (i = 0; s1[i] != '\0'; i++) {
		if (s1[i] != s2[i])
			return s1[i] > s2[i] ? 1 : -1;
	}
	return s2[i] != '\0' ? -1 : 0;
}

int MlnStrncmp(const char *a, const char *b, unsigned int n)
{
	const signed char *s1 = (const signed char *)a;
	const signed char *s2 = (const signed char *)b;
	unsigned int i;

	if (n == 0)
		return 0;
	for (i = 0; s1[i] != '\0'; i++) {
		if (s1[i] != s2[i])
			return s1[i] > s2[i] ? 1 : -1;
		if (i + 1 == n)
			return 0;
	}
	return s2[i] != '\0' ? -1 : 0;
}

char *MlnStrncat(char *dst, const char *src, unsigned int n)
{
	unsigned int length = MlnStrlenInline(dst);
	unsigned int i;
	char c;

	if (n != 0) {
		c = *src;
		if (c != '\0') {
			i = 0;
			do {
				dst[length + i] = c;
				i++;
				if (i == n)
					break;
				c = *++src;
			} while (c != '\0');
			length += i;
		}
	}
	dst[length] = '\0';
	return dst;
}

char *MlnStrstr(const char *haystack, const char *needle)
{
	unsigned int length = 0;
	unsigned int i;

	while (needle[length] != '\0')
		length++;
	if (*(const signed char *)haystack == '\0')
		return NULL;
	if (length == 0)
		return (char *)haystack;
	for (;;) {
		for (i = 0; haystack[i] != '\0' &&
		     (signed char)haystack[i] == (signed char)needle[i]; ) {
			if (++i == length)
				return (char *)haystack;
		}
		if (((const signed char *)needle)[i] == '\0')
			return (char *)haystack;
		if (*(const signed char *)++haystack == '\0')
			return NULL;
	}
}

char *MlnStrchr(const char *str, int c)
{
	const signed char *s = (const signed char *)str;

	while (*s != '\0' && *s != (signed char)c)
		s++;
	if (*s != c)
		return NULL;
	return (char *)s;
}

char *MlnStrrchr(const char *s, int c)
{
	const char *last;
	char ch = *s;

	if (ch == '\0')
		return NULL;
	last = NULL;
	do {
		if ((signed char)ch == (signed char)c)
			last = s;
		ch = *++s;
	} while (ch != '\0');
	return (char *)last;
}

unsigned int MlnStrspn(const char *s, const char *accept)
{
	const char *a;
	unsigned int count;
	char c = *s;
	char first;
	char ch;

	if (c == '\0')
		return 0;
	first = *accept;
	if (first == '\0')
		return 0;
	count = 0;
	for (;;) {
		if ((signed char)first != (signed char)c) {
			a = accept;
			do {
				ch = *++a;
				if (ch == '\0')
					return count;
			} while ((signed char)ch != (signed char)c);
		}
		c = *++s;
		count++;
		if (c == '\0')
			return count;
	}
}
