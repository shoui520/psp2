/*
 * inet.c: IPv4 addresses to text and back (sceNetInetNtop, sceNetInetPton). The parser is
 * BSD's inet_aton as Sony changed it: a leading 0 makes the number octal only before a digit
 * below 9, and STRICT accepts nothing but four decimal parts.
 */
#include <libkernel/libkernel.h>
#include <net/errno.h>
#include "libnet_internal.h"

#define LIBNET_ISDIGIT(c)	((unsigned int)(SceUChar8)(c) - '0' <= 9)
#define LIBNET_ISUPPER(c)	((unsigned int)(SceUChar8)(c) - 'A' <= 'Z' - 'A')
#define LIBNET_ISLOWER(c)	((unsigned int)(SceUChar8)(c) - 'a' <= 'z' - 'a')
#define LIBNET_ISSPACE(c)	((unsigned int)(SceUChar8)(c) - '\t' <= '\r' - '\t')

const char *libnet_inet_ntop(int af, const void *src, char *dst, unsigned int size)
{
	const SceUChar8 *address = src;
	char text[16];
	int length;

	if (af != SCE_NET_AF_INET) {
		*libnet_errno_loc() = SCE_NET_EAFNOSUPPORT;
		return NULL;
	}
	if (dst == NULL || src == NULL)
		goto nospace;
	length = sceClibSnprintf(text, sizeof(text), "%u.%u.%u.%u", address[0], address[1],
				 address[2], address[3]);
	if (size <= (unsigned int)length || length <= 0)
		goto nospace;
	sceClibMemcpy_safe(dst, text, libnet_strnlen(text, sizeof(text)) + 1);
	return dst;

nospace:
	*libnet_errno_loc() = SCE_NET_ENOSPC;
	return NULL;
}

int libnet_inet_pton(int af, const char *src, void *dst, int strict)
{
	SceUInt32 value;
	SceUInt32 parts[4], *part;
	unsigned int base, digit;
	int ret, error, n;
	signed char c;

	if (src == NULL) {
		ret = SCE_NET_ERROR_EINVAL;
		error = SCE_NET_EINVAL;
		goto fail;
	}
	if (af != SCE_NET_AF_INET) {
		ret = SCE_NET_ERROR_EAFNOSUPPORT;
		error = SCE_NET_EAFNOSUPPORT;
		goto fail;
	}
	c = *src;
	if (!LIBNET_ISDIGIT(c))
		return 0;
	strict = strict != 0;
	part = parts;
	for (;;) {
		/* a number: 0x hexadecimal, 0 octal, else decimal */
		base = 10;
		if (c == '0') {
			c = src[1];
			if ((c & ~0x20) == 'X') {
				c = src[2];
				base = 16;
				src += 2;
			} else {
				src++;
				if (LIBNET_ISDIGIT(c) && c != '9')
					base = 8;
			}
			if (base != 10 && strict)
				return 0;
		}
		value = 0;
		for (;;) {
			if (LIBNET_ISDIGIT(c)) {
				digit = c - '0';
				if (digit >= base)
					goto end;
				value = value * base + digit;
			} else if (base == 16 && (LIBNET_ISUPPER(c) || LIBNET_ISLOWER(c))) {
				digit = c + 10 - (LIBNET_ISUPPER(c) ? 'A' : 'a');
				if (digit > 15)
					break;
				value = value << 4 | digit;
			} else {
				break;
			}
			c = *++src;
		}
		if (c != '.')
			break;
		if (part == parts + 3)
			return 0;
		c = *++src;
		*part++ = value;
		if (!LIBNET_ISDIGIT(c))
			return 0;
	}
end:
	/* the address ends at the end of the text or at a space */
	if (c != '\0' && c != ' ' && !LIBNET_ISSPACE(c))
		return 0;
	n = part - parts + 1;
	if (n != 4 && strict)
		return 0;
	switch (n) {
	case 0:
		return 0;
	case 1:
		break;
	case 2:
		if (value > 0xffffff || parts[0] > 0xff)
			return 0;
		value |= parts[0] << 24;
		break;
	case 3:
		if (value > 0xffff || (parts[0] | parts[1]) > 0xff)
			return 0;
		value |= parts[0] << 24 | parts[1] << 16;
		break;
	case 4:
		if ((parts[0] | parts[1] | parts[2] | value) > 0xff)
			return 0;
		value |= parts[0] << 24 | parts[1] << 16 | parts[2] << 8;
		break;
	}
	if (dst != NULL) {
		value = sceNetHtonl(value);
		sceClibMemcpy_safe(dst, &value, sizeof(value));
	}
	return 1;

fail:
	*libnet_errno_loc() = error;
	return ret;
}
