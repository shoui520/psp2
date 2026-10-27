/*
 * uart_debug.c: shared-memory debug log writer, formatted output (snprintf /
 * vsnprintf), Secure Kernel panic handler, hardware SoC version reader, and
 * keyring slot writer (0x801056..0x801b09).
 *
 * Function names (except standard snprintf / vsnprintf) are ours.
 */

#include "secure_kernel.h"

/* .sdata (0x807cb8, -32768($gp)) */
static uint32_t s_panicArmInterruptCode = 0x00020000u;

/* 0x801056 (name ours) */
void writeDebugLogString(const char *str)
{
	uint32_t dstAddr = getDebugLogBufferAddr();
	uint32_t maxSize = getDebugLogBufferSize();
	uint32_t rem;
	char chunkBuf[16];

	if (maxSize == 0u)
		return;

	rem = strnlenSimple(str, maxSize - 1u) + 1u;
	while (rem != 0u) {
		uint32_t chunk = (rem < 17u) ? rem : 16u;
		uint32_t i;

		for (i = 0; i < chunk; i++)
			chunkBuf[i] = str[i];
		if (rem <= 16u)
			chunkBuf[chunk - 1u] = '\0';

		byteMemcpy((void *)(uintptr_t)dstAddr, chunkBuf, chunk);
		str += 16;
		dstAddr += 16u;
		rem -= chunk;
	}
}

/* 0x8010f2 (name ours) */
int skDebugPrintf(const char *fmt, ...)
{
	va_list ap;
	char buf[128];
	uint32_t len = 0u;

	if (srvkCheckQaFlagByte13Bit0() != 0 &&
	    getDebugLogBufferSize() != 0xffffffffu) {
		va_start(ap, fmt);
		len = (uint32_t)vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);

		if (len > getDebugLogBufferSize())
			len = getDebugLogBufferSize() - 1u;

		writeDebugLogString(buf);

		REG32(0xe0000000u) = 0x00040000u;
		(void)REG32(0xe0000000u);
		__asm__ volatile ("nop\n\tsyncm" ::: "memory");

		while ((REG32(0xe0000000u) & 0xffff0000u) != 0u)
			;
		while ((REG32(0xe0000000u) & 0xffff0000u) != 0u)
			;
	}
	return (int)len;
}

/* 0x8011b2 (name ours) */
void setPanicArmInterruptCode(uint32_t code)
{
	s_panicArmInterruptCode = code;
}

/* 0x8011b8 (name ours) */
void skPanic(const char *fmt, ...)
{
	va_list ap;
	char buf[128];

	__asm__ volatile ("di");
	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	writeDebugLogString(buf);
	REG32(0xe0000000u) = s_panicArmInterruptCode;
	for (;;)
		__asm__ volatile ("sleep");
}

/* 0x801216 (name ours) */
uint32_t getExpectedSocVersion(void)
{
	return SK_FW_VERSION;
}

/* 0x80121c (name ours) */
uint32_t readHardwareSocVersion(void)
{
	uint32_t ver = REG32(0xe00621c0u);

	(void)REG32(0xe00621c4u);
	(void)REG32(0xe00621c8u);
	(void)REG32(0xe00621ccu);
	(void)REG32(0xe00621d0u);
	(void)REG32(0xe00621d4u);
	(void)REG32(0xe00621d8u);
	(void)REG32(0xe00621dcu);
	return ver;
}

/* 0x80126e (name ours) */
uint32_t *getHardwareSocVersionPair(uint32_t *outPair)
{
	uint32_t ver = readHardwareSocVersion();

	outPair[0] = ver << 16;
	outPair[1] = ver >> 16;
	return outPair;
}

/* 0x801290 (name ours) */
int bufPutChar(char **bufPtr, size_t *remPtr, size_t totalSize, int ch)
{
	(void)totalSize;
	if (*remPtr == 0u)
		return 1;

	if (ch < 256) {
		if (*remPtr == 1u)
			ch = 0;
		**bufPtr = (char)ch;
		(*bufPtr)++;
		(*remPtr)--;
		return 1;
	}

	**bufPtr = '\0';
	return 0;
}

/* 0x8012c0 (name ours) */
int vsnprintfCore(char **bufPtr, size_t size, const char *fmt, va_list ap)
{
	size_t rem = size;
	int written = 0;
	char prefixBuf[2];
	char convBuf[64];

	while (*fmt != '\0') {
		uint32_t flags = 0u;
		int width = 0;
		int prec = 0;
		const char *strPtr;
		int strLen;
		int prefixLen;
		int zeroPad;
		char padCh;
		int i;

		if (*fmt != '%') {
			written += bufPutChar(bufPtr, &rem, size, (uint8_t)*fmt);
			if (*fmt != '\0')
				fmt++;
			continue;
		}

		for (;;) {
			char c;

			fmt++;
			c = *fmt;
			if (c == '-') {
				flags |= 0x01u;
			} else if (c == '+') {
				flags |= 0x02u;
			} else if (c == '0') {
				flags |= 0x04u;
			} else if (c == ' ') {
				flags |= 0x08u;
			} else if (c == '#') {
				flags |= 0x10u;
			} else if (c >= '1' && c <= '9') {
				int w = 0;

				while (*fmt >= '0' && *fmt <= '9') {
					w = w * 10 + (*fmt - '0');
					fmt++;
				}
				width = w;
				if (*fmt == '\0')
					goto fmt_done;
				fmt--;
			} else if (c == '*') {
				width = va_arg(ap, int);
				if (width < 0)
					width = -1;
			} else if (c == '.') {
				flags |= 0x20u;
				fmt++;
				if (*fmt == '*') {
					prec = va_arg(ap, int);
					if (prec < 0)
						prec = -1;
				} else {
					int p = 0;

					while (*fmt >= '0' && *fmt <= '9') {
						p = p * 10 + (*fmt - '0');
						fmt++;
					}
					if (*fmt == '\0')
						goto fmt_done;
					if (p < 0)
						p = -1;
					prec = p;
					fmt--;
				}
			} else if (c == 'l') {
				flags |= 0x40u;
			} else if (c == 'h') {
				flags |= 0x80u;
			} else {
				break;
			}
		}

		if (*fmt == 'o') {
			uint64_t val;
			char *p;

			if ((flags & 0x40u) != 0u)
				val = va_arg(ap, uint64_t);
			else if ((flags & 0x80u) != 0u)
				val = (uint16_t)va_arg(ap, uint32_t);
			else
				val = va_arg(ap, uint32_t);

			p = &convBuf[63];
			if (val == 0ULL) {
				if ((flags & 0x20u) == 0u || prec != 0)
					*p-- = '0';
			} else {
				do {
					*p-- = (char)('0' + ((uint32_t)val & 7u));
					val >>= 3;
				} while (val != 0ULL);
				if ((flags & 0x10u) != 0u)
					*p-- = '0';
			}
			strPtr = p + 1;
			strLen = (int)(&convBuf[63] - p);
			prefixLen = 0;
		} else if (*fmt == 'x' || *fmt == 'X') {
			const char *digits;
			uint64_t val;
			char *p;

			if (*fmt == 'X') {
				if ((flags & 0x10u) != 0u) {
					prefixBuf[0] = '0';
					prefixBuf[1] = 'X';
					prefixLen = 2;
				} else {
					prefixLen = 0;
				}
				digits = "0123456789ABCDEF";
			} else {
				if ((flags & 0x10u) != 0u) {
					prefixBuf[0] = '0';
					prefixBuf[1] = *fmt;
					prefixLen = 2;
				} else {
					prefixLen = 0;
				}
				digits = "0123456789abcdef";
			}

			if ((flags & 0x40u) != 0u)
				val = va_arg(ap, uint64_t);
			else if ((flags & 0x80u) != 0u)
				val = (uint16_t)va_arg(ap, uint32_t);
			else
				val = va_arg(ap, uint32_t);

			p = &convBuf[63];
			if (val == 0ULL) {
				if ((flags & 0x20u) == 0u || prec != 0)
					*p-- = '0';
				prefixLen = 0;
			} else {
				do {
					*p-- = digits[(uint32_t)val & 0xfu];
					val >>= 4;
				} while (val != 0ULL);
			}
			strPtr = p + 1;
			strLen = (int)(&convBuf[63] - p);
		} else if (*fmt == 's') {
			strPtr = va_arg(ap, const char *);
			if (strPtr == NULL) {
				(void)bufPutChar(bufPtr, &rem, size, '(');
				(void)bufPutChar(bufPtr, &rem, size, 'n');
				(void)bufPutChar(bufPtr, &rem, size, 'u');
				(void)bufPutChar(bufPtr, &rem, size, 'l');
				(void)bufPutChar(bufPtr, &rem, size, 'l');
				(void)bufPutChar(bufPtr, &rem, size, ')');
				if (*fmt != '\0')
					fmt++;
				continue;
			}
			strLen = 0;
			if ((flags & 0x20u) != 0u) {
				int maxChars = (prec >= 0) ? prec : 0;

				while (maxChars > 0 && strPtr[strLen] != '\0') {
					strLen++;
					maxChars--;
				}
			} else {
				while (strPtr[strLen] != '\0')
					strLen++;
			}
			prec = 0;
			prefixLen = 0;
		} else if (*fmt == 'c') {
			int ch = va_arg(ap, int);

			flags |= 0x100u;
			convBuf[63] = (char)ch;
			strPtr = &convBuf[63];
			if ((flags & 0x04u) != 0u)
				flags = (flags & ~0x04u) | 0x08u;
			prefixLen = 0;
			prec = 1;
			strLen = 1;
		} else if (*fmt == 'p') {
			const char *digits = "0123456789abcdef";
			uint64_t val;
			char *p;

			if ((flags & 0x40u) != 0u)
				val = va_arg(ap, uint64_t);
			else if ((flags & 0x80u) != 0u)
				val = (uint16_t)va_arg(ap, uint32_t);
			else
				val = va_arg(ap, uint32_t);

			p = &convBuf[63];
			if (val == 0ULL) {
				if ((flags & 0x20u) == 0u || prec != 0)
					*p-- = '0';
			} else {
				do {
					*p-- = digits[(uint32_t)val & 0xfu];
					val >>= 4;
				} while (val != 0ULL);
			}
			prefixBuf[0] = '0';
			prefixBuf[1] = 'x';
			prefixLen = 2;
			strPtr = p + 1;
			strLen = (int)(&convBuf[63] - p);
		} else {
			if (*fmt == '\0')
				break;
			convBuf[63] = *fmt;
			flags |= 0x100u;
			prefixLen = 0;
			prec = 1;
			strPtr = &convBuf[63];
			strLen = 1;
		}

		zeroPad = (strLen < prec) ? (prec - strLen) : 0;
		width -= strLen + prefixLen + zeroPad;

		if ((flags & 0x100u) != 0u) {
			padCh = ((flags & 0x04u) != 0u && width >= 1) ? '0' : ' ';
		} else {
			padCh = ((flags & 0x24u) == 0x04u && width >= 1) ? '0' : ' ';
		}

		if ((flags & 0x01u) != 0u) {
			for (i = 0; i < prefixLen; i++)
				written += bufPutChar(bufPtr, &rem, size, (uint8_t)prefixBuf[i]);
			for (i = 0; i < zeroPad; i++)
				written += bufPutChar(bufPtr, &rem, size, '0');
			for (i = 0; i < strLen; i++)
				written += bufPutChar(bufPtr, &rem, size, (uint8_t)strPtr[i]);
			for (i = 0; i < width; i++)
				written += bufPutChar(bufPtr, &rem, size, ' ');
		} else if ((flags & 0x04u) != 0u && padCh == '0') {
			for (i = 0; i < prefixLen; i++)
				written += bufPutChar(bufPtr, &rem, size, (uint8_t)prefixBuf[i]);
			for (i = 0; i < width; i++)
				written += bufPutChar(bufPtr, &rem, size, '0');
			for (i = 0; i < zeroPad; i++)
				written += bufPutChar(bufPtr, &rem, size, '0');
			for (i = 0; i < strLen; i++)
				written += bufPutChar(bufPtr, &rem, size, (uint8_t)strPtr[i]);
		} else {
			for (i = 0; i < width; i++)
				written += bufPutChar(bufPtr, &rem, size, (uint8_t)padCh);
			for (i = 0; i < prefixLen; i++)
				written += bufPutChar(bufPtr, &rem, size, (uint8_t)prefixBuf[i]);
			for (i = 0; i < zeroPad; i++)
				written += bufPutChar(bufPtr, &rem, size, '0');
			for (i = 0; i < strLen; i++)
				written += bufPutChar(bufPtr, &rem, size, (uint8_t)strPtr[i]);
		}

fmt_done:
		if (*fmt != '\0')
			fmt++;
	}

	(void)bufPutChar(bufPtr, &rem, size, 256);
	return written;
}

/* 0x801a1e */
int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;
	char *outPtr = buf;
	int ret;

	va_start(ap, fmt);
	ret = vsnprintfCore(&outPtr, size, fmt, ap);
	va_end(ap);
	return ret;
}

/* 0x801a62 */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	char *outPtr = buf;

	return vsnprintfCore(&outPtr, size, fmt, ap);
}

/* 0x801a96 (name ours) */
void writeKeyringSlot32(uint16_t slotIndex, const uint32_t *words8)
{
	REG32(0xe0030000u) = words8[0];
	REG32(0xe0030004u) = words8[1];
	REG32(0xe0030008u) = words8[2];
	REG32(0xe003000cu) = words8[3];
	REG32(0xe0030010u) = words8[4];
	REG32(0xe0030014u) = words8[5];
	REG32(0xe0030018u) = words8[6];
	REG32(0xe003001cu) = words8[7];
	REG32(0xe0030020u) = (uint32_t)slotIndex;
}
