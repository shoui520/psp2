/*
 * Path normalization: separators are collapsed to one, "." components are
 * dropped and ".." removes the component before it. A leading drive
 * ("app0:", "host0:") is kept; behind "host0:" a Windows path (a drive
 * letter or a "\\" share) may follow.
 */

#include <psp2kern/types.h>
#include "fios2_internal.h"

#define IS_SEP(c)	((c) == '/' || (c) == '\\')
#define IS_ALPHA(c)	((unsigned int)(((c) | 0x20) - 'a') < 26)
#define IS_DIGIT(c)	((unsigned int)((c) - '0') < 10)

/*
 * style: 0 or 2 is a FIOS path ('/', a drive of up to 16 letters and
 * digits), 1 a Windows path ('\', a drive letter), anything else a path
 * without a drive. Returns the length of dst, or dstSize + 1 (dst empty)
 * when it does not fit.
 */
size_t sceFiosKernelPathNormalize(char *dst, const char *src, size_t dstSize, int style)
{
	char *end = dst + dstSize;
	char *d, *root, *base;
	const char *s;
	unsigned char mask;
	signed char sep, c, n;
	int isWin, maxDrive, i;
	int hasRoot, absolute, keepRoot, relative, isHost;

	if (style == 0)
		style = 2;
	if (style == 2) {
		isWin = 0;
		mask = 0xff;
		sep = '/';
		maxDrive = 16;
	} else if (style == 1) {
		isWin = 1;
		mask = 0xdf;
		sep = '\\';
		maxDrive = 1;
	} else {
		isWin = 0;
		mask = 0xff;
		sep = '/';
		maxDrive = 0;
	}

	if (src == NULL || src[0] == '\0')
		goto overflow;

	s = src;
	d = dst;
	hasRoot = 0;
	relative = 0;
	keepRoot = 1;
	isHost = 0;

	/* the drive: letters (and digits) up to a ':' */
	if (maxDrive != 0) {
		if (style == 2) {
			for (i = 0; i <= maxDrive; i++) {
				c = src[i];
				if ((c == '/') | (c == '\0') | (c == '\\'))
					break;
				if ((i > 0) & (c == ':')) {
					if (i == 5 && src[0] == 'h' && src[1] == 'o' && src[2] == 's' &&
					    src[3] == 't') {
						isWin = 1;
						isHost = 1;
					}
					goto drive;
				}
				if (!IS_DIGIT(c) && !IS_ALPHA(c))
					break;
			}
		} else if (style == 1) {
			for (i = 0; i <= maxDrive; i++) {
				c = src[i];
				if ((c == '/') | (c == '\\') | (c == '\0'))
					break;
				if ((i > 0) & (c == ':'))
					goto drive;
				if (!IS_ALPHA(c))
					break;
			}
		} else {
			for (i = 0; i <= maxDrive; i++) {
				c = src[i];
				if ((c == '/') | (c == '\0') | (c == '\\'))
					break;
				if ((i > 0) & (c == ':'))
					goto drive;
			}
		}
	}
	goto path;

drive:
	hasRoot = 1;
	for (i++; i > 0; i--) {
		c = *s++;
		if (c != ':')
			c &= mask;
		*d++ = c;
		if (d >= end)
			goto overflow;
	}
path:
	c = *s;
	if (isHost) {
		/* host0: is followed by a Windows path */
		if (c == '\\' && s[1] == '\\') {
			*d++ = '\\';
			if (d >= end)
				goto overflow;
			*d++ = '\\';
			if (d >= end)
				goto overflow;
			s += 2;
			c = *s;
		}
		while (IS_SEP(c))
			c = *++s;
		relative = 1;
		keepRoot = 0;
		if (IS_ALPHA(c) && s[1] == ':') {
			*d++ = c & ~0x20;
			if (d >= end)
				goto overflow;
			*d++ = ':';
			if (d >= end)
				goto overflow;
			s += 2;
			c = *s;
			relative = 0;
			keepRoot = 1;
		}
	}
	root = d;
	base = d;

	/* leading "./" */
	absolute = 1;
	while (c == '.') {
		if (!IS_SEP(s[1]))
			break;
		if (isWin || !hasRoot)
			absolute = 0;
		s += 2;
		c = *s;
		while (IS_SEP(c))
			c = *++s;
	}
	if (!IS_SEP(c) && (isWin || !hasRoot))
		absolute = 0;

	if (style == 1 && s == src && src[0] == '\\' && s[1] == '\\') {
		/* a "\\server" share */
		*d++ = c;
		s++;
		root++;
		base++;
		if (d >= end)
			goto overflow;
		absolute = 1;
	} else if (!absolute) {
		keepRoot = 0;
		relative = 1;
	}

	/* the components */
	c = sep;
	do {
		if (IS_SEP(c) || c == sep) {
			n = *s;
			if (n == '\0' || IS_SEP(n))
				goto next;
			if (n == '.') {
				n = s[1];
				if (n == '\0' || IS_SEP(n)) {
					s++;
					goto next;
				}
				if (n == '.' && (s[2] == '\0' || IS_SEP(s[2]))) {
					c = s[2];
					s += 2;
					if (d != base) {
						do
							d--;
						while (d > base && *d != sep);
						goto next;
					}
					if (absolute) {
						base = d;
						goto next;
					}
					/* a relative path keeps the ".." it cannot remove */
					if (d != root) {
						*d++ = sep;
						if (d >= end)
							goto overflow;
					}
					*d++ = '.';
					if (d >= end)
						goto overflow;
					*d++ = '.';
					if (d >= end)
						goto overflow;
					base = d;
					s++;
					continue;
				}
			}
			if (!(relative && d == root))
				*d++ = sep;
		} else {
			*d++ = c;
		}
next:
		if (d >= end)
			goto overflow;
		c = *s++;
	} while (c != '\0');

	if (d == root) {
		*d++ = (absolute && keepRoot) ? sep : '.';
		if (d >= end)
			goto overflow;
	}
	*d = '\0';
	return d - dst;

overflow:
	*dst = '\0';
	return dstSize + 1;
}

size_t sceFiosKernelPathNormalizeDefault(char *dst, const char *src, size_t dstSize)
{
	return sceFiosKernelPathNormalize(dst, src, dstSize, 2);
}
