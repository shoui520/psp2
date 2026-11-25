#include "pspnet_internal.h"

/* Standard ASCII ctype table (index 0 = EOF, 1..128 = 0x00..0x7F) */
static const unsigned char g_ctype_table[129] = {
    0x00,
    /* 0x00..0x0F */ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x08, 0x08, 0x08, 0x08, 0x08, 0x20, 0x20,
    /* 0x10..0x1F */ 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    /* 0x20..0x2F */ 0x18, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    /* 0x30..0x3F */ 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    /* 0x40..0x4F */ 0x10, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    /* 0x50..0x5F */ 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x10, 0x10, 0x10, 0x10, 0x10,
    /* 0x60..0x6F */ 0x10, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    /* 0x70..0x7F */ 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x10, 0x10, 0x10, 0x10, 0x20
};

int sceNet_lib_C13C9307(int c)
{
    signed char ch = (signed char)c;
    if ((g_ctype_table[ch + 1] & 2) != 0)
        return (signed char)(ch - 32);
    return ch;
}

int sceNet_lib_96EF9DA1(int c)
{
    signed char ch = (signed char)c;
    if ((g_ctype_table[ch + 1] & 1) != 0)
        return (signed char)(ch + 32);
    return ch;
}

int sceNet_lib_750F705D(int c)
{
    return (signed char)g_ctype_table[c + 1];
}

int sceNet_lib_E0A81C7C(const void *s1, const void *s2, int n)
{
    const u8 *p1 = (const u8 *)s1;
    const u8 *p2 = (const u8 *)s2;
    if (n <= 0)
        return 0;
    while (n > 0) {
        u32 a = *p1++;
        u32 b = *p2++;
        n--;
        if (a != b)
            return (a < b) ? -1 : 1;
    }
    return 0;
}

void *sceNet_lib_4753D878(void *dst, const void *src, int n)
{
    char *d = (char *)dst;
    const char *s = (const char *)src;
    int diff;

    if (d == NULL)
        return NULL;
    if (d == s)
        return d;
    diff = (d > s) ? (int)(d - s) : (int)(s - d);
    if (diff >= n)
        return sceKernelMemcpy(d, s, n);
    if (d < s) {
        int i = n;
        char *dp = d;
        const char *sp = s;
        while (i > 0) {
            *dp++ = *sp++;
            i--;
        }
    } else {
        int i = n - 1;
        while (i >= 0) {
            d[i] = s[i];
            i--;
        }
    }
    return d;
}

void *sceNet_lib_8E798A2F(const void *s, int c, int n)
{
    const u8 *p = (const u8 *)s;
    u8 target = (u8)c;
    if (p == NULL || n <= 0)
        return NULL;
    while (n > 0) {
        if (*p == target)
            return (void *)p;
        p++;
        n--;
    }
    return NULL;
}

int sceNet_lib_4BF83CF4(const void *s1, const void *s2, int n)
{
    return sceNet_lib_E0A81C7C(s1, s2, n);
}

void *sceNet_lib_2F305274(const void *src, void *dst, int n)
{
    return sceNet_lib_4753D878(dst, src, n);
}

void *sceNet_lib_8B615F52(void *dst, int n)
{
    return sceKernelMemset(dst, 0, n);
}

static void sprintf_putc(void *ctx, int ch)
{
    char **pp = (char **)ctx;
    if (ch < 256) {
        *(*pp)++ = (char)ch;
    } else {
        **pp = '\0';
    }
}

static int net_vformat(void (*put)(void *, int), void *ctx, const char *fmt, va_list ap)
{
    static const char hex_lower[] = "0123456789abcdef";
    static const char hex_upper[] = "0123456789ABCDEF";
    const char *digits = hex_lower;
    int total = 0;
    const char *p = fmt;
    char numbuf[21];

    if (fmt == NULL)
        return 0;

    put(ctx, 512);

    while (*p != '\0') {
        if (*p != '%') {
            put(ctx, *p);
            total++;
            p++;
            continue;
        }

        u32 flags = 0;
        int prec = -1;
        int dprec = 0;
        int width = 0;
        int sign = 0;
        int size = 0;
        const char *cp = NULL;

        p++;
        for (;;) {
            char ch = *p;
            if (ch == '\0')
                goto done;

            switch (ch) {
            case ' ':
                if (!sign)
                    sign = ' ';
                p++;
                continue;
            case '#':
                flags |= 0x08;
                p++;
                continue;
            case '*':
                width = va_arg(ap, int);
                if (width < 0) {
                    width = -width;
                    flags |= 0x10;
                }
                p++;
                continue;
            case '+':
                sign = '+';
                p++;
                continue;
            case '-':
                flags |= 0x10;
                p++;
                continue;
            case '.': {
                int v = 0;
                p++;
                if (*p == '*') {
                    v = va_arg(ap, int);
                } else {
                    while (*p >= 0 && (sceNet_lib_750F705D(*p) & 4) != 0) {
                        v = v * 10 + (*p - '0');
                        p++;
                    }
                    p--;
                }
                prec = (v < 0) ? -1 : v;
                p++;
                continue;
            }
            case '0':
                flags |= 0x20;
                p++;
                continue;
            case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9': {
                int v = 0;
                for (;;) {
                    v = v * 10 + (*p - '0');
                    p++;
                    if (*p < 0 || (sceNet_lib_750F705D(*p) & 4) == 0) {
                        p--;
                        break;
                    }
                }
                width = v;
                p++;
                continue;
            }
            case 'h':
                flags |= 0x04;
                p++;
                continue;
            case 'l':
                flags |= 0x01;
                p++;
                continue;
            case 'L':
                flags |= 0x02;
                p++;
                continue;
            case 'c':
                numbuf[0] = (char)va_arg(ap, int);
                cp = numbuf;
                size = 1;
                sign = 0;
                goto output;
            case 'D':
                flags |= 0x01;
                /* fallthrough */
            case 'd':
            case 'i': {
                int val = va_arg(ap, int);
                u32 uval;
                if (val < 0) {
                    sign = '-';
                    uval = (u32)(-val);
                } else {
                    uval = (u32)val;
                }
                dprec = prec;
                if (prec >= 0)
                    flags &= ~0x20u;
                char *ep = numbuf + sizeof(numbuf);
                if (uval != 0 || prec != 0) {
                    do {
                        *--ep = digits[uval % 10];
                        uval /= 10;
                    } while (uval != 0);
                    digits = hex_lower;
                }
                cp = ep;
                size = (int)((numbuf + sizeof(numbuf)) - ep);
                goto output;
            }
            case 'n':
                if (flags & 0x01) {
                    *va_arg(ap, int *) = total;
                } else if (flags & 0x04) {
                    *va_arg(ap, short *) = (short)total;
                } else {
                    *va_arg(ap, int *) = total;
                }
                goto next_char;
            case 'O':
                flags |= 0x01;
                /* fallthrough */
            case 'o': {
                u32 uval = va_arg(ap, u32);
                sign = 0;
                dprec = prec;
                if (prec >= 0)
                    flags &= ~0x20u;
                char *ep = numbuf + sizeof(numbuf);
                if (uval != 0 || prec != 0) {
                    do {
                        *--ep = digits[uval % 8];
                        uval /= 8;
                    } while (uval != 0);
                    digits = hex_lower;
                    if ((flags & 0x08) && *ep != '0')
                        *--ep = '0';
                }
                cp = ep;
                size = (int)((numbuf + sizeof(numbuf)) - ep);
                goto output;
            }
            case 'p': {
                u32 uval = va_arg(ap, u32);
                sign = 0;
                dprec = prec;
                if (prec >= 0)
                    flags &= ~0x20u;
                char *ep = numbuf + sizeof(numbuf);
                if (uval != 0 || prec != 0) {
                    do {
                        *--ep = digits[uval % 16];
                        uval /= 16;
                    } while (uval != 0);
                    digits = hex_lower;
                }
                cp = ep;
                size = (int)((numbuf + sizeof(numbuf)) - ep);
                goto output;
            }
            case 's': {
                const char *sp = va_arg(ap, const char *);
                if (sp == NULL)
                    sp = "(null)";
                cp = sp;
                if (prec < 0) {
                    size = sceNet_lib_D8722983(cp);
                } else {
                    const char *nul = (const char *)sceNet_lib_8E798A2F(cp, 0, prec);
                    if (nul != NULL) {
                        int len = (int)(nul - cp);
                        size = (len < prec) ? len : prec;
                    } else {
                        size = prec;
                    }
                }
                sign = 0;
                goto output;
            }
            case 'U':
                flags |= 0x01;
                /* fallthrough */
            case 'u': {
                u32 uval = va_arg(ap, u32);
                sign = 0;
                dprec = prec;
                if (prec >= 0)
                    flags &= ~0x20u;
                char *ep = numbuf + sizeof(numbuf);
                if (uval != 0 || prec != 0) {
                    do {
                        *--ep = digits[uval % 10];
                        uval /= 10;
                    } while (uval != 0);
                    digits = hex_lower;
                }
                cp = ep;
                size = (int)((numbuf + sizeof(numbuf)) - ep);
                goto output;
            }
            case 'X':
                digits = hex_upper;
                /* fallthrough */
            case 'x': {
                u32 uval = va_arg(ap, u32);
                if ((flags & 0x08) && uval != 0)
                    flags |= 0x40;
                sign = 0;
                dprec = prec;
                if (prec >= 0)
                    flags &= ~0x20u;
                char *ep = numbuf + sizeof(numbuf);
                if (uval != 0 || prec != 0) {
                    do {
                        *--ep = digits[uval % 16];
                        uval /= 16;
                    } while (uval != 0);
                    digits = hex_lower;
                }
                cp = ep;
                size = (int)((numbuf + sizeof(numbuf)) - ep);
                goto output;
            }
            default:
                put(ctx, ch);
                total++;
                goto next_char;
            }

        output: {
            int realsz = size;
            if (sign)
                realsz++;
            if (flags & 0x40)
                realsz += 2;
            int fld = (realsz > dprec) ? realsz : dprec;

            if ((flags & 0x30) == 0 && width != 0 && fld < width) {
                int pad = width - fld;
                while (pad-- > 0)
                    put(ctx, ' ');
            }
            if (sign)
                put(ctx, sign);
            if (flags & 0x40) {
                put(ctx, '0');
                put(ctx, *p);
            }
            if ((flags & 0x30) == 0x20 && fld < width) {
                int pad = width - fld;
                while (pad-- > 0)
                    put(ctx, '0');
            }
            if (realsz < dprec) {
                int pad = dprec - realsz;
                while (pad-- > 0)
                    put(ctx, '0');
            }
            {
                int i;
                for (i = 0; i < size; i++)
                    put(ctx, cp[i]);
            }
            if ((flags & 0x10) && fld < width) {
                int pad = width - fld;
                while (pad-- > 0)
                    put(ctx, ' ');
            }
            total += (fld > width) ? fld : width;
            goto next_char;
        }
        }
    next_char:
        p++;
    }

done:
    put(ctx, 513);
    return total;
}

int sceNet_lib_CF705E46(char *buf, const char *fmt, ...)
{
    va_list ap;
    char *out = buf;
    va_start(ap, fmt);
    int ret = net_vformat(sprintf_putc, &out, fmt, ap);
    va_end(ap);
    return ret;
}

int sceNet_lib_8687B5AB(char *buf, const char *fmt, va_list ap)
{
    char *out = buf;
    return net_vformat(sprintf_putc, &out, fmt, ap);
}

u64 sceNet_lib_303A2F80(const char *str, char **endptr, int base)
{
    u64 val = 0;
    const char *p = str;
    int b = base;

    if (p == NULL)
        return 0;

    while ((sceNet_lib_750F705D(*p) & 8) != 0)
        p++;

    if ((u32)(b - 2) >= 35)
        b = 0;

    if (*p == '0') {
        p++;
        if ((*p == 'X' || *p == 'x') && (b == 0 || b == 16)) {
            b = 16;
            p++;
        } else if ((*p == 'B' || *p == 'b') && (b == 0 || b == 2)) {
            b = 2;
            p++;
        } else if (b == 0 || b == 8) {
            b = 8;
        } else if (b == 0) {
            b = 10;
        }
    } else if (sceNet_lib_C13C9307(*p) == 'O') {
        b = 8;
        p++;
    } else if (b == 0) {
        b = 10;
    }

    for (;;) {
        int ch = *p++;
        u32 digit;
        if ((sceNet_lib_750F705D(ch) & 4) != 0) {
            digit = (u32)(ch - '0');
        } else if ((sceNet_lib_750F705D(ch) & 3) != 0) {
            digit = (u32)(sceNet_lib_96EF9DA1(ch) - 'a' + 10);
        } else {
            digit = 9999999;
        }
        if (digit >= (u32)b)
            break;
        val = val * (u32)b + digit;
    }

    if (endptr != NULL)
        *endptr = (char *)(p - 1);
    return val;
}

static s64 net_strtoll(const char *str, char **endptr, int base)
{
    s64 val = 0;
    int sign = 1;
    const char *p = str;
    int b = base;

    if (p == NULL)
        return 0;

    while ((sceNet_lib_750F705D(*p) & 8) != 0)
        p++;

    while (*p == '-') {
        sign = -sign;
        p++;
    }

    if ((u32)(b - 2) >= 35)
        b = 0;
    if (b == 0)
        b = 10;

    if (*p == '0') {
        p++;
        if (*p == 'X' || *p == 'x') {
            b = 16;
            p++;
        } else if (*p == 'B' || *p == 'b') {
            b = 2;
            p++;
        }
    } else if (sceNet_lib_C13C9307(*p) == 'O') {
        b = 8;
        p++;
    }

    for (;;) {
        int ch = *p++;
        u32 digit;
        if ((sceNet_lib_750F705D(ch) & 4) != 0) {
            digit = (u32)(ch - '0');
        } else if ((sceNet_lib_750F705D(ch) & 3) != 0) {
            digit = (u32)(sceNet_lib_96EF9DA1(ch) - 'a' + 10);
        } else {
            digit = 9999999;
        }
        if (digit >= (u32)b)
            break;
        val = val * b + digit;
    }

    if (endptr != NULL)
        *endptr = (char *)(p - 1);
    return val * sign;
}

u64 sceNet_lib_2A73ADDC(const char *str, char **endptr, int base)
{
    return sceNet_lib_303A2F80(str, endptr, base);
}

s64 sceNet_lib_1FB2FDDD(const char *str)
{
    return net_strtoll(str, NULL, 10);
}

int sceNet_lib_A0F16ABD(const char *s1, const char *s2)
{
    if (s1 == NULL || s2 == NULL) {
        if (s1 == s2)
            return 0;
        return (s1 != NULL) ? 1 : -1;
    }
    while (*s1 == *s2) {
        if (*s1 == '\0')
            return 0;
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

char *sceNet_lib_80C9F02A(char *dst, const char *src)
{
    char *d = dst;
    if (dst == NULL || src == NULL)
        return NULL;
    while ((*d++ = *src++) != '\0')
        ;
    return dst;
}

int sceNet_lib_D8722983(const char *s)
{
    int len = 0;
    if (s == NULL)
        return 0;
    while (*s++ != '\0')
        len++;
    return len;
}

int sceNet_lib_94DCA9F0(const char *s1, const char *s2, int n)
{
    if (s1 == NULL || s2 == NULL) {
        if (s1 == s2)
            return 0;
        return (s1 != NULL) ? 1 : -1;
    }
    while (n-- > 0) {
        if (*s1 != *s2)
            return *s1 - *s2;
        if (*s1 == '\0')
            return 0;
        s1++;
        s2++;
    }
    return 0;
}

char *sceNet_lib_B5CE388A(char *dst, const char *src, int n)
{
    char *d = dst;
    int i = 0;

    if (dst == NULL || src == NULL)
        return NULL;
    if (n <= 0)
        return dst;

    for (;;) {
        char c = *src++;
        *d++ = c;
        if (c == '\0')
            break;
        if (++i >= n)
            return dst;
    }
    for (i = i + 1; i < n; i++)
        *d++ = '\0';
    return dst;
}

char *sceNet_lib_BCBE14CF(const char *s, int c)
{
    char ch = (char)c;
    if (s == NULL)
        return NULL;
    for (;;) {
        if (*s == ch)
            return (char *)s;
        if (*s == '\0')
            return NULL;
        s++;
    }
}

int sceNet_lib_9CFBC7E3(const char *s1, const char *s2)
{
    if (s1 == NULL || s2 == NULL) {
        if (s1 == s2)
            return 0;
        return (s1 != NULL) ? 1 : -1;
    }
    for (;;) {
        int c1 = sceNet_lib_96EF9DA1(*s1);
        int c2 = sceNet_lib_96EF9DA1(*s2);
        if (c1 != c2)
            return c1 - c2;
        if (*s1 == '\0')
            return 0;
        s1++;
        s2++;
    }
}

int sceNet_lib_B9085A96(const char *s1, const char *s2, int n)
{
    if (s1 == NULL || s2 == NULL) {
        if (s1 == s2)
            return 0;
        return (s1 != NULL) ? 1 : -1;
    }
    while (n-- > 0) {
        int c1 = sceNet_lib_96EF9DA1(*s1);
        int c2 = sceNet_lib_96EF9DA1(*s2);
        if (c1 != c2)
            return c1 - c2;
        if (*s1 == '\0')
            return 0;
        s1++;
        s2++;
    }
    return 0;
}

char *sceNet_lib_5705F6F9(char *dst, const char *src)
{
    if (dst == NULL || src == NULL)
        return NULL;
    if (dst + sceNet_lib_D8722983(dst) == src + sceNet_lib_D8722983(src))
        return NULL;
    char *d = dst;
    while (*d != '\0')
        d++;
    while ((*d++ = *src++) != '\0')
        ;
    return dst;
}

char *sceNet_lib_F48963C6(const char *s, int c)
{
    char ch = (char)c;
    const char *p;
    if (s == NULL)
        return NULL;
    p = s;
    while (*p != '\0')
        p++;
    for (;;) {
        if (*p == ch)
            return (char *)p;
        if (p <= s)
            return NULL;
        p--;
    }
}

void *sceNet_lib_EF097716(void *dst, const void *src, int n)
{
    return sceKernelMemcpy(dst, src, n);
}

void *sceNet_lib_57CC57DA(void *dst, int c, int n)
{
    return sceKernelMemset(dst, c, n);
}
