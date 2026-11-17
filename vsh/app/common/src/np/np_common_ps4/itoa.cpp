/*
 * SceNpCommonPs4: number to string conversions (a signed and an unsigned one of each width, in any
 * base up to 36; flags: 1 upper case digits, 2 base prefix, 4 explicit plus sign).
 */

#include "np_common_ps4_internal.h"

extern "C" {

int SceNpCommonPs4_9CAF0FE7(char *buf, SceSize size, SceInt32 value, int base, char flags, SceSize *pLength)
{
	char *prefix;
	char *p;
	SceUInt32 v;
	SceSize extra;
	SceSize count;
	SceSize last;
	SceUInt32 digit;
	char ch;

	extra = 0;
	if (base <= 0 || base > 36)
		return NP_COMMON_PS4_ERROR_80558009;
	if ((flags & 4) && value > 0) {
		if (buf != NULL) {
			if (size < 2)
				return NP_COMMON_PS4_ERROR_BUFFER_TOO_SMALL;
			*buf++ = '+';
			size--;
		}
		extra = 1;
	}
	if (value < 0) {
		if (buf != NULL) {
			if (size < 2)
				return NP_COMMON_PS4_ERROR_BUFFER_TOO_SMALL;
			*buf++ = '-';
			size--;
		}
		extra++;
	}
	if (flags & 2) {
		if (base == 8) {
			if (buf != NULL) {
				if (size < 2)
					return NP_COMMON_PS4_ERROR_BUFFER_TOO_SMALL;
				*buf++ = '0';
				size--;
			}
			extra++;
		}
		prefix = buf;
		if (base == 16) {
			if (buf != NULL) {
				buf[0] = '0';
				ch = 'x';
				if (flags & 1)
					ch = 'X';
				buf += 2;
				prefix[1] = ch;
				size -= 2;
			}
			extra += 2;
		}
	}
	count = 0;
	v = (value < 0 ? -value : value);
	do {
		last = count++;
		v /= (SceUInt32)(SceInt32)base;
	} while (v != 0);
	if (buf != NULL) {
		if (size < last + 2)
			return NP_COMMON_PS4_ERROR_BUFFER_TOO_SMALL;
		p = &buf[count - 1];
		v = (value < 0 ? -value : value);
		do {
			digit = v % (SceUInt32)(SceInt32)base;
			if (digit < 10) {
				ch = digit + 48;
			} else {
				ch = digit + 87;
				if (flags & 1)
					ch = digit + 55;
			}
			*p-- = ch;
			v /= (SceUInt32)(SceInt32)base;
		} while (v != 0);
		buf[last + 1] = '\0';
	}
	if (pLength != NULL)
		*pLength = count + extra;
	return 0;
}

int NpCommonPs4UIntToString(char *buf, SceSize size, SceUInt32 value, int base, char flags, SceSize *pLength)
{
	char *prefix;
	char *p;
	SceUInt32 v;
	SceSize extra;
	SceSize count;
	SceSize last;
	SceUInt32 digit;
	char ch;

	extra = 0;
	if (base <= 0 || base > 36)
		return NP_COMMON_PS4_ERROR_80558009;
	if ((flags & 4) && value != 0) {
		if (buf != NULL) {
			if (size < 2)
				return NP_COMMON_PS4_ERROR_BUFFER_TOO_SMALL;
			*buf++ = '+';
			size--;
		}
		extra = 1;
	}
	if (flags & 2) {
		if (base == 8) {
			if (buf != NULL) {
				if (size < 2)
					return NP_COMMON_PS4_ERROR_BUFFER_TOO_SMALL;
				*buf++ = '0';
				size--;
			}
			extra++;
		}
		prefix = buf;
		if (base == 16) {
			if (buf != NULL) {
				buf[0] = '0';
				ch = 'x';
				if (flags & 1)
					ch = 'X';
				buf += 2;
				prefix[1] = ch;
				size -= 2;
			}
			extra += 2;
		}
	}
	count = 0;
	v = value;
	do {
		last = count++;
		v /= (SceUInt32)(SceInt32)base;
	} while (v != 0);
	if (buf != NULL) {
		if (size < last + 2)
			return NP_COMMON_PS4_ERROR_BUFFER_TOO_SMALL;
		p = &buf[count - 1];
		v = value;
		do {
			digit = v % (SceUInt32)(SceInt32)base;
			if (digit < 10) {
				ch = digit + 48;
			} else {
				ch = digit + 87;
				if (flags & 1)
					ch = digit + 55;
			}
			*p-- = ch;
			v /= (SceUInt32)(SceInt32)base;
		} while (v != 0);
		buf[last + 1] = '\0';
	}
	if (pLength != NULL)
		*pLength = count + extra;
	return 0;
}

int NpCommonPs4Int64ToString(char *buf, SceSize size, SceInt64 value, int base, char flags, SceSize *pLength)
{
	char *prefix;
	char *p;
	SceUInt64 v;
	SceSize extra;
	SceSize count;
	SceSize last;
	SceUInt64 digit;
	char ch;

	extra = 0;
	if (base <= 0 || base > 36)
		return NP_COMMON_PS4_ERROR_80558009;
	if (flags & 4) {
		if (value > 0) {
			if (buf != NULL) {
				if (size < 2)
					return NP_COMMON_PS4_ERROR_BUFFER_TOO_SMALL;
				*buf++ = '+';
				size--;
			}
			extra = 1;
		}
		if (value < 0) {
			if (buf != NULL) {
				if (size < 2)
					return NP_COMMON_PS4_ERROR_BUFFER_TOO_SMALL;
				*buf++ = '-';
				size--;
			}
			extra++;
		}
	}
	if (flags & 2) {
		if (base == 8) {
			if (buf != NULL) {
				if (size < 2)
					return NP_COMMON_PS4_ERROR_BUFFER_TOO_SMALL;
				*buf++ = '0';
				size--;
			}
			extra++;
		}
		prefix = buf;
		if (base == 16) {
			if (buf != NULL) {
				buf[0] = '0';
				ch = 'x';
				if (flags & 1)
					ch = 'X';
				buf += 2;
				prefix[1] = ch;
				size -= 2;
			}
			extra += 2;
		}
	}
	count = 0;
	v = (value < 0 ? -value : value);
	do {
		last = count++;
		v /= (SceUInt64)(SceInt64)base;
	} while (v != 0);
	if (buf != NULL) {
		if (size < last + 2)
			return NP_COMMON_PS4_ERROR_BUFFER_TOO_SMALL;
		p = &buf[count - 1];
		v = (value < 0 ? -value : value);
		do {
			digit = v % (SceUInt64)(SceInt64)base;
			if (digit < 10) {
				ch = digit + 48;
			} else {
				ch = digit + 87;
				if (flags & 1)
					ch = digit + 55;
			}
			*p-- = ch;
			v /= (SceUInt64)(SceInt64)base;
		} while (v != 0);
		buf[last + 1] = '\0';
	}
	if (pLength != NULL)
		*pLength = count + extra;
	return 0;
}

int NpCommonPs4UInt64ToString(char *buf, SceSize size, SceUInt64 value, int base, char flags, SceSize *pLength)
{
	char *prefix;
	char *p;
	SceUInt64 v;
	SceSize extra;
	SceSize count;
	SceSize last;
	SceUInt64 digit;
	char ch;

	extra = 0;
	if (base <= 0 || base > 36)
		return NP_COMMON_PS4_ERROR_80558009;
	if ((flags & 4) && value != 0) {
		if (buf != NULL) {
			if (size < 2)
				return NP_COMMON_PS4_ERROR_BUFFER_TOO_SMALL;
			*buf++ = '+';
			size--;
		}
		extra = 1;
	}
	if (flags & 2) {
		if (base == 8) {
			if (buf != NULL) {
				if (size < 2)
					return NP_COMMON_PS4_ERROR_BUFFER_TOO_SMALL;
				*buf++ = '0';
				size--;
			}
			extra++;
		}
		prefix = buf;
		if (base == 16) {
			if (buf != NULL) {
				buf[0] = '0';
				ch = 'x';
				if (flags & 1)
					ch = 'X';
				buf += 2;
				prefix[1] = ch;
				size -= 2;
			}
			extra += 2;
		}
	}
	count = 0;
	v = value;
	do {
		last = count++;
		v /= (SceUInt64)(SceInt64)base;
	} while (v != 0);
	if (buf != NULL) {
		if (size < last + 2)
			return NP_COMMON_PS4_ERROR_BUFFER_TOO_SMALL;
		p = &buf[count - 1];
		v = value;
		do {
			digit = v % (SceUInt64)(SceInt64)base;
			if (digit < 10) {
				ch = digit + 48;
			} else {
				ch = digit + 87;
				if (flags & 1)
					ch = digit + 55;
			}
			*p-- = ch;
			v /= (SceUInt64)(SceInt64)base;
		} while (v != 0);
		buf[last + 1] = '\0';
	}
	if (pLength != NULL)
		*pLength = count + extra;
	return 0;
}

int NpCommonPs4StringCopy(char *dst, SceSize size, const char *src)
{
	if (size < sceClibStrnlen(src, 0xFFFFFFFF) + 1)
		return NP_COMMON_PS4_ERROR_BUFFER_TOO_SMALL;
	sceClibStrncpy(dst, src, size - 1);
	dst[size - 1] = '\0';
	return 0;
}

}
