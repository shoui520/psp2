#include <stdarg.h>

#include <common_imp.h>
#include <sysmem_sysclib.h>

#define CTYPE_DOWNCASE_LETTER 0x01
#define CTYPE_UPCASE_LETTER   0x02
#define CTYPE_CIPHER          0x04
#define CTYPE_TRANSPARENT     0x08
#define CTYPE_PUNCTUATION     0x10
#define CTYPE_CTRL            0x20
#define CTYPE_HEX_CIPHER      0x40

#define CTYPE_LETTER (CTYPE_DOWNCASE_LETTER | CTYPE_UPCASE_LETTER)

// 135F0
s8 _ctype_[] =
{
    0,
    CTYPE_CTRL, // 0
    CTYPE_CTRL,
    CTYPE_CTRL,
    CTYPE_CTRL,
    CTYPE_CTRL, // 4
    CTYPE_CTRL,
    CTYPE_CTRL,
    CTYPE_CTRL,
    CTYPE_CTRL, // 8
    CTYPE_TRANSPARENT,
    CTYPE_TRANSPARENT,
    CTYPE_TRANSPARENT,
    CTYPE_TRANSPARENT, // 12
    CTYPE_TRANSPARENT,
    CTYPE_CTRL,
    CTYPE_CTRL,
    CTYPE_CTRL, // 16
    CTYPE_CTRL,
    CTYPE_CTRL,
    CTYPE_CTRL,
    CTYPE_CTRL, // 20
    CTYPE_CTRL,
    CTYPE_CTRL,
    CTYPE_CTRL,
    CTYPE_CTRL, // 24
    CTYPE_CTRL,
    CTYPE_CTRL,
    CTYPE_CTRL,
    CTYPE_CTRL, // 28
    CTYPE_CTRL,
    CTYPE_CTRL,
    CTYPE_CTRL,
    CTYPE_TRANSPARENT | CTYPE_PUNCTUATION, // 32
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION, // 36
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION, // 40
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION, // 44
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION,
    CTYPE_CIPHER, // 48
    CTYPE_CIPHER,
    CTYPE_CIPHER,
    CTYPE_CIPHER,
    CTYPE_CIPHER, // 52
    CTYPE_CIPHER,
    CTYPE_CIPHER,
    CTYPE_CIPHER,
    CTYPE_CIPHER, // 56
    CTYPE_CIPHER,
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION, // 60
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION, // 64
    CTYPE_DOWNCASE_LETTER | CTYPE_HEX_CIPHER,
    CTYPE_DOWNCASE_LETTER | CTYPE_HEX_CIPHER,
    CTYPE_DOWNCASE_LETTER | CTYPE_HEX_CIPHER,
    CTYPE_DOWNCASE_LETTER | CTYPE_HEX_CIPHER, // 68
    CTYPE_DOWNCASE_LETTER | CTYPE_HEX_CIPHER,
    CTYPE_DOWNCASE_LETTER | CTYPE_HEX_CIPHER,
    CTYPE_DOWNCASE_LETTER,
    CTYPE_DOWNCASE_LETTER, // 72
    CTYPE_DOWNCASE_LETTER,
    CTYPE_DOWNCASE_LETTER,
    CTYPE_DOWNCASE_LETTER,
    CTYPE_DOWNCASE_LETTER, // 76
    CTYPE_DOWNCASE_LETTER,
    CTYPE_DOWNCASE_LETTER,
    CTYPE_DOWNCASE_LETTER,
    CTYPE_DOWNCASE_LETTER, // 80
    CTYPE_DOWNCASE_LETTER,
    CTYPE_DOWNCASE_LETTER,
    CTYPE_DOWNCASE_LETTER,
    CTYPE_DOWNCASE_LETTER, // 84
    CTYPE_DOWNCASE_LETTER,
    CTYPE_DOWNCASE_LETTER,
    CTYPE_DOWNCASE_LETTER,
    CTYPE_DOWNCASE_LETTER, // 88
    CTYPE_DOWNCASE_LETTER,
    CTYPE_DOWNCASE_LETTER,
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION, // 92
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION, // 96
    CTYPE_UPCASE_LETTER | CTYPE_HEX_CIPHER,
    CTYPE_UPCASE_LETTER | CTYPE_HEX_CIPHER,
    CTYPE_UPCASE_LETTER | CTYPE_HEX_CIPHER,
    CTYPE_UPCASE_LETTER | CTYPE_HEX_CIPHER, // 100
    CTYPE_UPCASE_LETTER | CTYPE_HEX_CIPHER,
    CTYPE_UPCASE_LETTER | CTYPE_HEX_CIPHER,
    CTYPE_UPCASE_LETTER,
    CTYPE_UPCASE_LETTER, // 104
    CTYPE_UPCASE_LETTER,
    CTYPE_UPCASE_LETTER,
    CTYPE_UPCASE_LETTER,
    CTYPE_UPCASE_LETTER, // 108
    CTYPE_UPCASE_LETTER,
    CTYPE_UPCASE_LETTER,
    CTYPE_UPCASE_LETTER,
    CTYPE_UPCASE_LETTER, // 112
    CTYPE_UPCASE_LETTER,
    CTYPE_UPCASE_LETTER,
    CTYPE_UPCASE_LETTER,
    CTYPE_UPCASE_LETTER, // 116
    CTYPE_UPCASE_LETTER,
    CTYPE_UPCASE_LETTER,
    CTYPE_UPCASE_LETTER,
    CTYPE_UPCASE_LETTER, // 120
    CTYPE_UPCASE_LETTER,
    CTYPE_UPCASE_LETTER,
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION, // 124
    CTYPE_PUNCTUATION,
    CTYPE_PUNCTUATION,
    CTYPE_CTRL
};

int bcmp(void *lhs, void *rhs, int n)
{
    return memcmp(lhs, rhs, n);
}

void bcopy(void *src, void *dst, int n)
{
    memmove(dst, src, n);
}

void bzero(void *s, int n)
{
    memset(s, 0, n);
}

int toupper(int c)
{
    if ((_ctype_[c + 1] & CTYPE_UPCASE_LETTER) == 0)
        return c;
    return (u8)c - 32;
}

int tolower(int c)
{
    if ((_ctype_[c + 1] & CTYPE_DOWNCASE_LETTER) == 0)
        return c;
    return (u8)c + 32;
}

int look_ctype_table(int c)
{
    return _ctype_[c + 1];
}

char *index(char *s, int c)
{
    char match = (char)c;
    if (s == NULL)
        return NULL;
    char cur;
    do
    {
        cur = *s;
        if (cur == match)
            return s;
        s++;
    } while (cur != '\0');
    return NULL;
}

u64 __udivmoddi4(u64 dividend, u64 divisor, u64 *remOut)
{
    u64 combined = dividend | divisor;
    if (divisor == 0)
        return 0;
    int shift = 0;
    while ((combined & 1) == 0) {
        combined >>= 1;
        shift++;
    }
    int shiftCount = 0;
    if ((combined & 0xFFFFFFFF00000000ULL) != 0)
    {
        for (;;)
        {
            if (divisor >= dividend || (s64)divisor < 0)
                break;
            divisor <<= 1;
            shiftCount++;
        }
        u64 bit = (u64)1 << shiftCount;
        u64 quotient = 0;
        while (bit != 0)
        {
            if (dividend >= divisor)
            {
                dividend -= divisor;
                quotient |= bit;
            }
            bit >>= 1;
            divisor >>= 1;
        }
        *remOut = dividend;
        return quotient;
    }
    u32 divLo = (u32)(divisor >> shift);
    u32 numLo = (u32)(dividend >> shift);
    u64 rem = numLo % divLo;
    *remOut = rem << shift;
    return numLo / divLo;
}

u64 __udivdi3(u64 dividend, u64 divisor)
{
    int shift;
    u64 num, result;
    if (divisor == 0)
        return 0;
    for (shift = 0; (divisor & 1) == 0; shift++)
        divisor >>= 1;
    num = dividend >> shift;
    dividend = num;
    if (((num | divisor) >> 32) == 0) {
        return (u32)dividend / (u32)divisor;
    }
    num = dividend;
    if ((s64)dividend < 0) {
        num = (u64)-1;
    }
    for (shift = 0; divisor < num; shift++)
        divisor <<= 1;
    num = (u64)1 << shift;
    result = 0;
    while (num != 0)
    {
        if (dividend >= divisor)
        {
            dividend -= divisor;
            result |= num;
        }
        num >>= 1;
        divisor >>= 1;
    }
    return result;
}

u64 __umoddi3(u64 dividend, u64 divisor)
{
    u64 mod;
    __udivmoddi4(dividend, divisor, &mod);
    return mod;
}

void *memchr(const void *s, int c, int n)
{
    if (s != NULL)
    {
        while ((n--) > 0)
        {
            if (*(const u8 *)s == (u8)(c & 0xFF))
                return (void *)s;
            s = (const u8 *)s + 1;
        }
    }
    return NULL;
}

int memcmp(const void *lhs, const void *rhs, int n)
{
    const u8 *left = lhs;
    const u8 *right = rhs;
    while ((n--) > 0)
    {
        u8 a = *left++;
        u8 b = *right++;
        if (a != b)
        {
            if (a >= b)
                return 1;
            return -1;
        }
    }
    return 0;
}

typedef struct __attribute__((packed)) {
    u32 val;
} UnalignedU32;

void *sceKernelMemcpy(void *dst, const void *src, u32 n)
{
    const u8 *curSrc = (const u8 *)src;
    u8 *curDst = (u8 *)dst;
    u8 *end = (u8 *)dst + n;

    if (dst == src)
        return dst;
    if (n >= 8)
    {
        if (((u32)dst & 3) != 0)
        {
            u32 align = 4 - ((u32)dst & 3);
            u32 word = ((const UnalignedU32 *)src)->val;
            asm("swr %0, 0(%1)" : : "r"(word), "r"(dst) : "memory");
            curSrc += align;
            curDst += align;
        }
        if ((((u32)curDst >> 29) & 3) == 0 && n >= 128)
        {
            u8 *alignedHead = (u8 *)UPALIGN64((u32)curDst);
            u8 *alignedEnd = (u8 *)((u32)end & 0xFFFFFFC0);
            if (((u32)curSrc & 3) != 0)
            {
                while (curDst != alignedHead)
                {
                    *(u32 *)curDst = ((const UnalignedU32 *)curSrc)->val;
                    curSrc += 4;
                    curDst += 4;
                }
                while (curDst != alignedEnd)
                {
                    pspCache(0x18, curDst);
                    int i;
                    for (i = 0; i < 4; i++)
                    {
                        const UnalignedU32 *srcWords = (const UnalignedU32 *)curSrc;
                        u32 *dstWords = (u32 *)curDst;
                        u32 wordA = srcWords[0].val;
                        u32 wordB = srcWords[1].val;
                        u32 wordC = srcWords[2].val;
                        u32 wordD = srcWords[3].val;
                        dstWords[0] = wordA;
                        dstWords[1] = wordB;
                        dstWords[2] = wordC;
                        dstWords[3] = wordD;
                        curSrc += 16;
                        curDst += 16;
                    }
                }
            }
            else
            {
                while (curDst != alignedHead)
                {
                    *(u32 *)curDst = *(const u32 *)curSrc;
                    curSrc += 4;
                    curDst += 4;
                }
                while (curDst != alignedEnd)
                {
                    pspCache(0x18, curDst);
                    int i;
                    for (i = 0; i < 4; i++)
                    {
                        const u32 *srcWords = (const u32 *)curSrc;
                        u32 *dstWords = (u32 *)curDst;
                        u32 wordA = srcWords[0];
                        u32 wordB = srcWords[1];
                        u32 wordC = srcWords[2];
                        u32 wordD = srcWords[3];
                        dstWords[0] = wordA;
                        dstWords[1] = wordB;
                        dstWords[2] = wordC;
                        dstWords[3] = wordD;
                        curSrc += 16;
                        curDst += 16;
                    }
                }
            }
            if (alignedEnd == end)
                return dst;
        }
        u8 *wordEnd = (u8 *)((u32)end & 0xFFFFFFFC);
        while (curDst != wordEnd)
        {
            *(u32 *)curDst = ((const UnalignedU32 *)curSrc)->val;
            curDst += 4;
            curSrc += 4;
        }
    }
    while (curDst != end)
        *curDst++ = *curSrc++;
    return dst;
}

void *memcpy(void *dst, const void *src, u32 n)
{
    if (dst == NULL)
        return NULL;
    return sceKernelMemcpy(dst, src, n);
}

void *memmove(void *dst, const void *src, int n)
{
    char *dstCur = dst;
    const char *srcCur = src;
    if (dst == NULL)
        return NULL;
    if (dst < src)
    {
        while ((n--) > 0)
            *(dstCur++) = *(srcCur++);
    }
    else
    {
        while ((--n) >= 0)
            dstCur[n] = srcCur[n];
    }
    return dst;
}

int prnt(prnt_callback cb, void *ctx, const char *fmt, va_list args)
{
    if (fmt == NULL)
        return 0;
    cb(ctx, 512);
    char *ciphers = "0123456789abcdef";
    int count = 0;
    char string[21];

    for (;;)
    {
        if (*fmt == '\0')
        {
            cb(ctx, 513);
            return count;
        }
        if (*fmt == '%')
        {
            int flag = 0;
            int precision = -1;
            int usedPrecision = 0;
            int numAlign = 0;
            int sign = '\0';
            int base = 10;
            u32 number = 0;
            u64 longNum = 0;
            int stringLen = 0;
            char *s = NULL;
            int formatKind = 0; /* 0: done, 1: 32-bit int, 2: 64-bit int, 3: string */

            for (;;)
            {
                fmt++;
                switch (*fmt)
                {
                case ' ':
                    if (sign == '\0')
                        sign = ' ';
                    continue;

                case '\0':
                    cb(ctx, 513);
                    return count;

                case '#':
                    flag |= 8;
                    continue;

                case '*':
                    numAlign = va_arg(args, int);
                    if (numAlign >= 0)
                        continue;
                    numAlign = -numAlign;
                    /* FALLTHRU */

                case '-':
                    flag |= 0x10;
                    continue;

                case '+':
                    sign = '+';
                    continue;

                case '.':
                    precision = 0;
                    if (*(++fmt) == '*')
                    {
                        precision = va_arg(args, int);
                    }
                    else
                    {
                        for (;;)
                        {
                            if (*fmt < 0)
                                break;
                            if ((look_ctype_table(*fmt) & CTYPE_CIPHER) == 0)
                                break;
                            precision = precision * 10 + *(fmt++) - '0';
                        }
                        fmt--;
                    }
                    precision = pspMax(precision, -1);
                    continue;

                case '0':
                    flag |= 0x20;
                    continue;

                case '1': case '2': case '3':
                case '4': case '5': case '6':
                case '7': case '8': case '9':
                {
                    int cur = 0;
                    for (;;)
                    {
                        cur = cur * 10 + *fmt - '0';
                        if (*(++fmt) < 0)
                            break;
                        if ((look_ctype_table(*fmt) & CTYPE_CIPHER) == 0)
                            break;
                    }
                    fmt--;
                    numAlign = cur;
                    continue;
                }

                case 'D':
                    flag |= 1;
                    /* FALLTHRU */

                case 'd':
                case 'i':
                    base = 10;
                    if ((flag & 2) == 0)
                    {
                        int signedVal = va_arg(args, int);
                        if (signedVal < 0)
                        {
                            signedVal = -signedVal;
                            sign = '-';
                        }
                        number = (u32)signedVal;
                        formatKind = 1;
                    }
                    else
                    {
                        s64 signedLong = va_arg(args, s64);
                        if (signedLong < 0)
                        {
                            signedLong = -signedLong;
                            sign = '-';
                        }
                        longNum = (u64)signedLong;
                        formatKind = 2;
                    }
                    break;

                case 'L':
                    flag |= 2;
                    continue;

                case 'O':
                    flag |= 1;
                    /* FALLTHRU */

                case 'o':
                    base = 8;
                    sign = '\0';
                    if ((flag & 2) == 0)
                    {
                        number = va_arg(args, u32);
                        formatKind = 1;
                    }
                    else
                    {
                        longNum = va_arg(args, u64);
                        formatKind = 2;
                    }
                    break;

                case 'U':
                    flag |= 1;
                    /* FALLTHRU */

                case 'u':
                    base = 10;
                    sign = '\0';
                    if ((flag & 2) == 0)
                    {
                        number = va_arg(args, u32);
                        formatKind = 1;
                    }
                    else
                    {
                        longNum = va_arg(args, u64);
                        formatKind = 2;
                    }
                    break;

                case 'X':
                    ciphers = "0123456789ABCDEF";
                    /* FALLTHRU */

                case 'x':
                    base = 16;
                    sign = '\0';
                    if ((flag & 2) == 0)
                    {
                        number = va_arg(args, u32);
                        if (((flag >> 3) & 1) && number != 0)
                            flag |= 0x40;
                        formatKind = 1;
                    }
                    else
                    {
                        longNum = va_arg(args, u64);
                        if (longNum != 0 && ((flag >> 3) & 1) != 0)
                            flag |= 0x40;
                        formatKind = 2;
                    }
                    break;

                case 'c':
                    s = string;
                    stringLen = 1;
                    sign = '\0';
                    string[0] = (char)va_arg(args, int);
                    formatKind = 3;
                    break;

                case 'h':
                    flag |= 4;
                    continue;

                case 'l':
                    if ((flag & 1) == 0)
                        flag |= 1;
                    else
                        flag = (flag & 0xFFFFFFFE) | 2;
                    continue;

                case 'n':
                    if ((flag & 1) != 0 || (flag & 4) == 0)
                        *va_arg(args, int*) = count;
                    else
                        *va_arg(args, short*) = count;
                    break;

                case 'p':
                    base = 16;
                    number = va_arg(args, u32);
                    sign = '\0';
                    formatKind = 1;
                    break;

                case 's':
                    s = va_arg(args, char*);
                    if (s == NULL)
                        s = "(null)";
                    if (precision < 0)
                    {
                        stringLen = strlen(s);
                    }
                    else
                    {
                        char *cur = memchr(s, '\0', precision);
                        stringLen = precision;
                        if (cur != NULL)
                        {
                            stringLen = cur - s;
                            if (precision < cur - s)
                                stringLen = precision;
                        }
                    }
                    sign = '\0';
                    formatKind = 3;
                    break;

                default:
                    cb(ctx, *fmt);
                    count++;
                    break;
                }
                break;
            }

            if (formatKind == 2)
            {
                if (precision >= 0)
                    flag &= 0xFFFFFFDF;
                s = &string[21];
                usedPrecision = precision;
                if (longNum != 0 || precision != 0)
                {
                    char lastC;
                    do
                    {
                        lastC = ciphers[__umoddi3(longNum, base)];
                        *(--s) = lastC;
                        longNum = __udivdi3(longNum, base);
                    } while (longNum != 0);
                    ciphers = "0123456789abcdef";
                    if (((flag >> 3) & 1) && base == 8 && lastC != '0')
                        *(--s) = '0';
                }
                stringLen = &string[21] - s;
            }
            else if (formatKind == 1)
            {
                usedPrecision = precision;
                s = &string[21];
                if (precision >= 0)
                    flag &= 0xFFFFFFDF;
                if (number != 0 || precision != 0)
                {
                    char lastC;
                    do
                    {
                        lastC = ciphers[number % (u32)base];
                        number /= (u32)base;
                        *(--s) = lastC;
                    } while (number != 0);
                    ciphers = "0123456789abcdef";
                    if (((flag >> 3) & 1) && base == 8 && lastC != '0')
                        *(--s) = '0';
                }
                stringLen = &string[21] - s;
            }

            if (formatKind != 0)
            {
                int len = stringLen;
                if (sign != '\0')
                    len = stringLen + 1;
                if ((flag & 0x40) != 0)
                    len += 2;
                int maxSize = pspMax(len, usedPrecision);
                int i;
                if ((flag & 0x30) == 0 && numAlign != 0)
                {
                    for (i = 0; i < numAlign - maxSize; i++)
                        cb(ctx, ' ');
                }
                if (sign != '\0')
                    cb(ctx, sign);
                if ((flag & 0x40) != 0)
                {
                    cb(ctx, '0');
                    cb(ctx, *fmt);
                }
                if ((flag & 0x30) == 0x20)
                {
                    for (i = 0; i < numAlign - maxSize; i++)
                        cb(ctx, '0');
                }
                for (i = 0; i < usedPrecision - len; i++)
                    cb(ctx, '0');
                for (i = 0; i < stringLen; i++)
                    cb(ctx, *(s++));
                if ((flag & 0x10) != 0)
                {
                    for (i = 0; i < numAlign - maxSize; i++)
                        cb(ctx, ' ');
                }
                count += pspMax(maxSize, numAlign);
            }
        }
        else
        {
            cb(ctx, *fmt);
            count++;
        }
        fmt++;
    }
}

char *rindex(const char *s, char c)
{
    const char *cur = s;
    if (s == NULL)
        return NULL;
    while (*(cur++) != '\0')
        ;
    // E22C
    do
        if (*(--cur) == c)
            return (char *)cur;
    while (s < cur);
    return 0;
}

int sprintf(char *str, const char *format, ...)
{
    va_list ap;
    char *ctx[1];
    va_start(ap, format);
    ctx[0] = str;
    int ret = prnt((prnt_callback)sprintf_char, (void*)ctx, format, ap);
    va_end(ap);
    return ret;
}

int snprintf(char *str, u32 size, const char *format, ...)
{
    va_list ap;
    int ctx[3];
    va_start(ap, format);
    ctx[0] = size - 1;
    ctx[1] = 0;
    ctx[2] = (int)str;
    int ret = prnt((prnt_callback)snprintf_char, ctx, format, ap);
    va_end(ap);
    return ret;
}

void sprintf_char(int *ctx, int c)
{
    if (c >= 256)
        *((char*)*ctx) = '\0';
    else {
        *((char*)*ctx) = c;
        ctx[0]++;
    }
}

void snprintf_char(int *ctx, int c)
{
    if (c >= 256 || ctx[1] >= ctx[0])
        *((char*)ctx[2]) = '\0';
    else
    {
        *((char*)ctx[2]) = c;
        ctx[2]++;
        ctx[1]++;
    }
}

char *strcat(char *dst, const char *src)
{
    if (dst == NULL || src == NULL)
        return NULL;
    int dstLen = strlen(dst);
    int srcLen = strlen(src);
    if (dst + dstLen == src + srcLen)
        return NULL;
    memcpy(dst + dstLen, src, srcLen + 1);
    return dst;
}

char *strchr(const char *s, char c)
{
    if (s == NULL)
        return NULL;
    // E3F4
    do
    {
        if (*s == c)
            return (char *)s;
        s++;
    } while (*s != '\0');
    return NULL;
}

int strcmp(const char *lhs, const char *rhs)
{
    if (lhs != NULL && rhs != NULL)
    {
        while (*(lhs++) == *(rhs++))
            if (*(lhs - 1) == '\0')
                return 0;
        return *(lhs - 1) - *(rhs - 1);
    }
    if (lhs == rhs)
        return 0;
    if (lhs != NULL)
        return 1;
    return -1;
}

char *strcpy(char *dest, const char *src)
{
    char *curDest = dest;
    if (dest == NULL || src == NULL)
        return NULL;
    char c;
    do
    {
        c = *(src++);
        *(curDest++) = c;
    } while (c != '\0');
    return dest;
}

int strtol(const char *nptr, char **endptr, int base)
{
    int sign = +1;
    int num = 0;
    if (nptr == NULL)
        return 0;
    while ((look_ctype_table(*nptr) & CTYPE_TRANSPARENT) != 0)
        nptr++;
    while (*nptr == '-') {
        nptr++;
        sign = -sign;
    }
    if (base < 2 || base > 36)
        base = 0;
    if (*nptr == '0')
    {
        nptr++;
        if (*nptr == 'x' || *nptr == 'X')
        {
            if (base == 0 || base == 16) {
                nptr++;
                base = 16;
            }
        }
        else if (*nptr == 'b' || *nptr == 'B')
        {
            if (base == 2 || base == 0) {
                nptr++;
                base = 2;
            }
        }
        else if (base == 0 || base == 8)
            base = 8;
    }
    else if (toupper(*nptr) == 'O')
    {
        nptr++;
        base = 8;
    }
    if (base == 0)
        base = 10;
    for (;;)
    {
        char ch = *nptr++;
        int cipher = ch - '0';
        if ((look_ctype_table(ch) & CTYPE_CIPHER) == 0)
        {
            cipher = 9999999;
            if ((look_ctype_table(ch) & CTYPE_LETTER) != 0)
                cipher = tolower(ch) - 'a' + 10;
        }
        if (cipher >= base)
        {
            if (endptr != NULL)
                *endptr = (char*)nptr - 1;
            return num * sign;
        }
        num = num * base + cipher;
    }
}

u32 strtoul(char *nptr, char **endptr, int base)
{
    u32 num = 0;
    if (nptr == NULL)
        return 0;
    while ((look_ctype_table(*nptr) & CTYPE_TRANSPARENT) != 0)
        nptr++;
    if (base < 2 || base > 36)
        base = 0;
    if (base == 0)
        base = 10;
    if (*nptr == '0')
    {
        nptr++;
        if (*nptr == 'x' || *nptr == 'X')
        {
            nptr++;
            base = 16;
        }
        else if (*nptr == 'b' || *nptr == 'B')
        {
            nptr++;
            base = 2;
        }
    }
    else if (toupper(*nptr) == 'O')
    {
        nptr++;
        base = 8;
    }
    for (;;)
    {
        char ch = *nptr++;
        int cipher = ch - '0';
        if ((look_ctype_table(ch) & CTYPE_CIPHER) == 0)
        {
            cipher = 9999999;
            if ((look_ctype_table(ch) & CTYPE_LETTER) != 0)
                cipher = tolower(ch) - 'a' + 10;
        }
        if (cipher >= base)
        {
            if (endptr != NULL)
                *endptr = nptr - 1;
            return num;
        }
        num = num * base + cipher;
    }
}

int strncmp(const char *lhs, const char *rhs, int n)
{
    if (lhs != NULL && rhs != NULL)
    {
        if ((--n) < 0)
            return 0;
        while (*(lhs++) == *(rhs++))
            if (*(lhs - 1) == '\0' || (--n) < 0)
                return 0;
        if (n < 0)
            return 0;
        return *(lhs - 1) - *(rhs - 1);
    }
    if (lhs == rhs)
        return 0;
    if (lhs != NULL)
        return 1;
    return -1;
}

char *strncpy(char *dest, const char *src, int n)
{
    char *curDst = dest;
    if (dest == NULL || src == NULL)
        return 0;
    int i;
    // E92C
    for (i = 0; i < n; i++)
    {
        char c = *(src++);
        *(curDst++) = c;
        if (c == '\0')
        {
            // E958, E968
            while ((++i) < n)
                *(curDst++) = '\0';
            // E97C
            break;
        }
    }
    // E94C
    return dest;
}

u32 strnlen(const char *s, u32 maxlen)
{
    u32 len = 0;
    if (s == NULL || maxlen == 0)
        return 0;

    // E9A4
    while (*(s++) != '\0')
    {
        len++;
        if (len == maxlen)
            break;
    }
    return len;
}

u32 strlen(const char *s)
{
    int len = 0;
    if (s == NULL)
        return 0;
    // E9E0
    while (*(s++) != '\0')
        len++;
    return len;
}

char *strrchr(char *s, int c)
{
    char *curS = s;
    if (s == NULL)
        return NULL;
    // EA0C
    while (*(curS++) != '\0')
        ;
    curS--;
    // EA20
    do
    {
        if (*curS == (char)c)
            return curS;
    } while (s < (curS--));
    return NULL;
}

char *strpbrk(char *s, const char *accept)
{
    // EA58
    while (*(s++) != '\0')
    {
        const char *cur = accept;
        // EA68
        while (*cur != '\0')
            if (*(cur++) == *s)
                return s;
        // EA80
    }
    return NULL;
}

char *strstr(char *haystack, const char *needle)
{
    if (*needle == 0)
        return haystack;
    if (*haystack == 0)
        return NULL;
    // EAB8
    do
    {
        char *curHaystack = haystack;
        const char *curNeedle = needle;
        char n, h;
        // EAC0
        do
        {
            n = *(curNeedle++);
            if (n == '\0')
                return haystack;
            h = *(curHaystack++);
            if (h == '\0')
                return NULL;
        } while (n == h);
    } while (*(haystack++) != '\0');
    return NULL;
}

