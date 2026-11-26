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

int sceNet_lib_C13C9307(int ch)
{
    signed char sc = (signed char)ch;
    if ((g_ctype_table[sc + 1] & 2) != 0)
        return (signed char)(sc - 32);
    return sc;
}

int sceNet_lib_96EF9DA1(int ch)
{
    signed char sc = (signed char)ch;
    if ((g_ctype_table[sc + 1] & 1) != 0)
        return (signed char)(sc + 32);
    return sc;
}

int sceNet_lib_750F705D(int ch)
{
    return (signed char)g_ctype_table[ch + 1];
}

int sceNet_lib_E0A81C7C(const void *lhs, const void *rhs, int len)
{
    const u8 *lp = (const u8 *)lhs;
    const u8 *rp = (const u8 *)rhs;
    if (len <= 0)
        return 0;
    while (len > 0) {
        u32 lb = *lp++;
        u32 rb = *rp++;
        len--;
        if (lb != rb)
            return (lb < rb) ? -1 : 1;
    }
    return 0;
}

void *sceNet_lib_4753D878(void *dst, const void *src, int len)
{
    char *dp = (char *)dst;
    const char *sp = (const char *)src;
    int diff;

    if (dp == NULL)
        return NULL;
    if (dp == sp)
        return dp;
    diff = (dp > sp) ? (int)(dp - sp) : (int)(sp - dp);
    if (diff >= len)
        return sceKernelMemcpy(dp, sp, len);
    if (dp < sp) {
        int rem = len;
        char *out = dp;
        const char *in = sp;
        while (rem > 0) {
            *out++ = *in++;
            rem--;
        }
    } else {
        int idx = len - 1;
        while (idx >= 0) {
            dp[idx] = sp[idx];
            idx--;
        }
    }
    return dp;
}

void *sceNet_lib_8E798A2F(const void *buf, int ch, int len)
{
    const u8 *cursor = (const u8 *)buf;
    u8 target = (u8)ch;
    if (cursor == NULL || len <= 0)
        return NULL;
    while (len > 0) {
        if (*cursor == target)
            return (void *)cursor;
        cursor++;
        len--;
    }
    return NULL;
}

int sceNet_lib_4BF83CF4(const void *lhs, const void *rhs, int len)
{
    return sceNet_lib_E0A81C7C(lhs, rhs, len);
}

void *sceNet_lib_2F305274(const void *src, void *dst, int len)
{
    return sceNet_lib_4753D878(dst, src, len);
}

void *sceNet_lib_8B615F52(void *dst, int len)
{
    return sceKernelMemset(dst, 0, len);
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

static int net_vformat(void (*put)(void *, int), void *ctx, const char *fmt, void *ap_raw)
{
    static const char hex_lower[] = "0123456789abcdef";
    static const char hex_upper[] = "0123456789ABCDEF";
    const char *digits = hex_lower;
    int total = 0;
    const char *cursor = fmt;
    char numbuf[21];
    va_list ap_copy;
    va_list *app = &ap_copy;

    if (fmt == NULL)
        return 0;

    ap_copy = *(va_list *)ap_raw;
    put(ctx, 512);

    while (*cursor != '\0') {
        if (*cursor != '%') {
            put(ctx, *cursor);
            total++;
            cursor++;
            continue;
        }

        u32 flags = 0;
        int prec = -1;
        int dprec = 0;
        int width = 0;
        int sign = 0;
        int size = 0;
        const char *out_str = NULL;
        int emit_field = 0;
        int eof_hit = 0;

        cursor++;
        while (!emit_field && !eof_hit) {
            char ch = *cursor;
            if (ch == '\0') {
                eof_hit = 1;
                break;
            }

            switch (ch) {
            case ' ':
                if (!sign)
                    sign = ' ';
                cursor++;
                break;
            case '#':
                flags |= 0x08;
                cursor++;
                break;
            case '*':
                width = va_arg(*app, int);
                if (width < 0) {
                    width = -width;
                    flags |= 0x10;
                }
                cursor++;
                break;
            case '+':
                sign = '+';
                cursor++;
                break;
            case '-':
                flags |= 0x10;
                cursor++;
                break;
            case '.': {
                int val = 0;
                cursor++;
                if (*cursor == '*') {
                    val = va_arg(*app, int);
                } else {
                    while (*cursor >= 0 && (sceNet_lib_750F705D(*cursor) & 4) != 0) {
                        val = val * 10 + (*cursor - '0');
                        cursor++;
                    }
                    cursor--;
                }
                prec = (val < 0) ? -1 : val;
                cursor++;
                break;
            }
            case '0':
                flags |= 0x20;
                cursor++;
                break;
            case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9': {
                int val = 0;
                for (;;) {
                    val = val * 10 + (*cursor - '0');
                    cursor++;
                    if (*cursor < 0 || (sceNet_lib_750F705D(*cursor) & 4) == 0) {
                        cursor--;
                        break;
                    }
                }
                width = val;
                cursor++;
                break;
            }
            case 'h':
                flags |= 0x04;
                cursor++;
                break;
            case 'l':
                flags |= 0x01;
                cursor++;
                break;
            case 'L':
                flags |= 0x02;
                cursor++;
                break;
            case 'c':
                numbuf[0] = (char)va_arg(*app, int);
                out_str = numbuf;
                size = 1;
                sign = 0;
                emit_field = 1;
                break;
            case 'D':
                flags |= 0x01;
                /* fallthrough */
            case 'd':
            case 'i': {
                int val = va_arg(*app, int);
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
                out_str = ep;
                size = (int)((numbuf + sizeof(numbuf)) - ep);
                emit_field = 1;
                break;
            }
            case 'n':
                if (flags & 0x01) {
                    *va_arg(*app, int *) = total;
                } else if (flags & 0x04) {
                    *va_arg(*app, short *) = (short)total;
                } else {
                    *va_arg(*app, int *) = total;
                }
                emit_field = -1;
                break;
            case 'O':
                flags |= 0x01;
                /* fallthrough */
            case 'o': {
                u32 uval = va_arg(*app, u32);
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
                out_str = ep;
                size = (int)((numbuf + sizeof(numbuf)) - ep);
                emit_field = 1;
                break;
            }
            case 'p': {
                u32 uval = va_arg(*app, u32);
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
                out_str = ep;
                size = (int)((numbuf + sizeof(numbuf)) - ep);
                emit_field = 1;
                break;
            }
            case 's': {
                const char *sp = va_arg(*app, const char *);
                if (sp == NULL)
                    sp = "(null)";
                out_str = sp;
                if (prec < 0) {
                    size = sceNet_lib_D8722983(out_str);
                } else {
                    const char *nul = (const char *)sceNet_lib_8E798A2F(out_str, 0, prec);
                    if (nul != NULL) {
                        int len = (int)(nul - out_str);
                        size = (len < prec) ? len : prec;
                    } else {
                        size = prec;
                    }
                }
                sign = 0;
                emit_field = 1;
                break;
            }
            case 'U':
                flags |= 0x01;
                /* fallthrough */
            case 'u': {
                u32 uval = va_arg(*app, u32);
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
                out_str = ep;
                size = (int)((numbuf + sizeof(numbuf)) - ep);
                emit_field = 1;
                break;
            }
            case 'X':
                digits = hex_upper;
                /* fallthrough */
            case 'x': {
                u32 uval = va_arg(*app, u32);
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
                out_str = ep;
                size = (int)((numbuf + sizeof(numbuf)) - ep);
                emit_field = 1;
                break;
            }
            default:
                put(ctx, ch);
                total++;
                emit_field = -1;
                break;
            }
        }

        if (eof_hit)
            break;

        if (emit_field > 0) {
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
                put(ctx, *cursor);
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
                int idx;
                for (idx = 0; idx < size; idx++)
                    put(ctx, out_str[idx]);
            }
            if ((flags & 0x10) && fld < width) {
                int pad = width - fld;
                while (pad-- > 0)
                    put(ctx, ' ');
            }
            total += (fld > width) ? fld : width;
        }
        cursor++;
    }

    put(ctx, 513);
    return total;
}

int sceNet_lib_CF705E46(char *buf, const char *fmt, ...)
{
    va_list ap;
    char *out = buf;
    va_start(ap, fmt);
    int ret = net_vformat(sprintf_putc, &out, fmt, &ap);
    va_end(ap);
    return ret;
}

int sceNet_lib_8687B5AB(char *buf, const char *fmt, void *ap)
{
    char *out = buf;
    return net_vformat(sprintf_putc, &out, fmt, ap);
}

u64 sceNet_lib_303A2F80(const char *str, char **endptr, int base)
{
    u64 val = 0;
    const char *cursor = str;
    int radix = base;

    if (cursor == NULL)
        return 0;

    while ((sceNet_lib_750F705D(*cursor) & 8) != 0)
        cursor++;

    if ((u32)(radix - 2) >= 35)
        radix = 0;

    if (*cursor == '0') {
        cursor++;
        if ((*cursor == 'X' || *cursor == 'x') && (radix == 0 || radix == 16)) {
            radix = 16;
            cursor++;
        } else if ((*cursor == 'B' || *cursor == 'b') && (radix == 0 || radix == 2)) {
            radix = 2;
            cursor++;
        } else if (radix == 0 || radix == 8) {
            radix = 8;
        } else if (radix == 0) {
            radix = 10;
        }
    } else if (sceNet_lib_C13C9307(*cursor) == 'O') {
        radix = 8;
        cursor++;
    } else if (radix == 0) {
        radix = 10;
    }

    for (;;) {
        int ch = *cursor++;
        u32 digit;
        if ((sceNet_lib_750F705D(ch) & 4) != 0) {
            digit = (u32)(ch - '0');
        } else if ((sceNet_lib_750F705D(ch) & 3) != 0) {
            digit = (u32)(sceNet_lib_96EF9DA1(ch) - 'a' + 10);
        } else {
            digit = 9999999;
        }
        if (digit >= (u32)radix)
            break;
        val = val * (u32)radix + digit;
    }

    if (endptr != NULL)
        *endptr = (char *)(cursor - 1);
    return val;
}

static s64 net_strtoll(const char *str, char **endptr, int base)
{
    s64 val = 0;
    int sign = 1;
    const char *cursor = str;
    int radix = base;

    if (cursor == NULL)
        return 0;

    while ((sceNet_lib_750F705D(*cursor) & 8) != 0)
        cursor++;

    while (*cursor == '-') {
        sign = -sign;
        cursor++;
    }

    if ((u32)(radix - 2) >= 35)
        radix = 0;
    if (radix == 0)
        radix = 10;

    if (*cursor == '0') {
        cursor++;
        if (*cursor == 'X' || *cursor == 'x') {
            radix = 16;
            cursor++;
        } else if (*cursor == 'B' || *cursor == 'b') {
            radix = 2;
            cursor++;
        }
    } else if (sceNet_lib_C13C9307(*cursor) == 'O') {
        radix = 8;
        cursor++;
    }

    for (;;) {
        int ch = *cursor++;
        u32 digit;
        if ((sceNet_lib_750F705D(ch) & 4) != 0) {
            digit = (u32)(ch - '0');
        } else if ((sceNet_lib_750F705D(ch) & 3) != 0) {
            digit = (u32)(sceNet_lib_96EF9DA1(ch) - 'a' + 10);
        } else {
            digit = 9999999;
        }
        if (digit >= (u32)radix)
            break;
        val = val * radix + digit;
    }

    if (endptr != NULL)
        *endptr = (char *)(cursor - 1);
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

int sceNet_lib_A0F16ABD(const char *lhs, const char *rhs)
{
    if (lhs == NULL || rhs == NULL) {
        if (lhs == rhs)
            return 0;
        return (lhs != NULL) ? 1 : -1;
    }
    while (*lhs == *rhs) {
        if (*lhs == '\0')
            return 0;
        lhs++;
        rhs++;
    }
    return *lhs - *rhs;
}

char *sceNet_lib_80C9F02A(char *dst, const char *src)
{
    char *out = dst;
    if (dst == NULL || src == NULL)
        return NULL;
    while ((*out++ = *src++) != '\0')
        ;
    return dst;
}

int sceNet_lib_D8722983(const char *str)
{
    int len = 0;
    if (str == NULL)
        return 0;
    while (*str++ != '\0')
        len++;
    return len;
}

int sceNet_lib_94DCA9F0(const char *lhs, const char *rhs, int len)
{
    if (lhs == NULL || rhs == NULL) {
        if (lhs == rhs)
            return 0;
        return (lhs != NULL) ? 1 : -1;
    }
    while (len-- > 0) {
        if (*lhs != *rhs)
            return *lhs - *rhs;
        if (*lhs == '\0')
            return 0;
        lhs++;
        rhs++;
    }
    return 0;
}

char *sceNet_lib_B5CE388A(char *dst, const char *src, int len)
{
    char *out = dst;
    int idx = 0;

    if (dst == NULL || src == NULL)
        return NULL;
    if (len <= 0)
        return dst;

    for (;;) {
        char ch = *src++;
        *out++ = ch;
        if (ch == '\0')
            break;
        if (++idx >= len)
            return dst;
    }
    for (idx = idx + 1; idx < len; idx++)
        *out++ = '\0';
    return dst;
}

char *sceNet_lib_BCBE14CF(const char *str, int ch)
{
    char target = (char)ch;
    if (str == NULL)
        return NULL;
    for (;;) {
        if (*str == target)
            return (char *)str;
        if (*str == '\0')
            return NULL;
        str++;
    }
}

int sceNet_lib_9CFBC7E3(const char *lhs, const char *rhs)
{
    if (lhs == NULL || rhs == NULL) {
        if (lhs == rhs)
            return 0;
        return (lhs != NULL) ? 1 : -1;
    }
    for (;;) {
        int lc = sceNet_lib_96EF9DA1(*lhs);
        int rc = sceNet_lib_96EF9DA1(*rhs);
        if (lc != rc)
            return lc - rc;
        if (*lhs == '\0')
            return 0;
        lhs++;
        rhs++;
    }
}

int sceNet_lib_B9085A96(const char *lhs, const char *rhs, int len)
{
    if (lhs == NULL || rhs == NULL) {
        if (lhs == rhs)
            return 0;
        return (lhs != NULL) ? 1 : -1;
    }
    while (len-- > 0) {
        int lc = sceNet_lib_96EF9DA1(*lhs);
        int rc = sceNet_lib_96EF9DA1(*rhs);
        if (lc != rc)
            return lc - rc;
        if (*lhs == '\0')
            return 0;
        lhs++;
        rhs++;
    }
    return 0;
}

char *sceNet_lib_5705F6F9(char *dst, const char *src)
{
    if (dst == NULL || src == NULL)
        return NULL;
    const char *dst_end = dst;
    while (*dst_end != '\0')
        dst_end++;
    const char *src_end = src;
    while (*src_end != '\0')
        src_end++;
    if (dst_end == src_end)
        return NULL;
    char *out = dst;
    while (*out != '\0')
        out++;
    while ((*out++ = *src++) != '\0')
        ;
    return dst;
}

char *sceNet_lib_F48963C6(const char *str, int ch)
{
    char target = (char)ch;
    const char *cursor;
    if (str == NULL)
        return NULL;
    cursor = str;
    while (*cursor != '\0')
        cursor++;
    for (;;) {
        if (*cursor == target)
            return (char *)cursor;
        if (cursor <= str)
            return NULL;
        cursor--;
    }
}

void *sceNet_lib_EF097716(void *dst, const void *src, int len)
{
    return sceKernelMemcpy(dst, src, len);
}

void *sceNet_lib_57CC57DA(void *dst, int ch, int len)
{
    return sceKernelMemset(dst, ch, len);
}
