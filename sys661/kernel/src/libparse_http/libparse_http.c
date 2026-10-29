#include <common_imp.h>
#include <sysmem_sysclib.h>
#include <threadman_kernel.h>

SCE_MODULE_INFO("SceParseHTTPheader_Library", SCE_MODULE_KIRK_MEMLMD_LIB | SCE_MODULE_ATTR_CANT_STOP, 1, 1);
SCE_SDK_VERSION(0x06060110);

static u8 g_ctype[129] = {
    0x00,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x08, 0x08, 0x08, 0x08, 0x08, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x18, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x10, 0x10, 0x10, 0x10, 0x20,
};

#define CTYPE_TABLE (&g_ctype[1])

static s32 __attribute__((noipa)) parse_version(const char *buf, u32 len, s32 *ver_major, s32 *ver_minor);
static s32 __attribute__((noipa)) parse_status_code(const char *buf, u32 len, s32 *status_code);
static s32 __attribute__((noipa)) parse_strncasecmp(const char *s1, const char *s2, u32 n);
static s32 __attribute__((noipa)) parse_reason_phrase(const char *buf, u32 len, const char **phrase_out, u32 *phrase_len_out);
static u32 __attribute__((noipa)) parse_strlen(const char *s);
static s32 __attribute__((noipa)) parse_strncmp(const char *s1, const char *s2, s32 n);

s32 sceParseHttp_8077A433(const char *buf, u32 len, s32 *ver_major, s32 *ver_minor,
                          s32 *status_code, const char **reason_phrase, u32 *reason_len)
{
    if (sceKernelCheckThreadStack() < 0x3e0)
        return 0x80410005;
    if (buf == NULL)
        return 0x80432060;
    if (ver_major == NULL || ver_minor == NULL || status_code == NULL ||
        reason_phrase == NULL || reason_len == NULL)
        return 0x804321fe;

    s32 res = parse_version(buf, len, ver_major, ver_minor);
    if (res < 0)
        return res;
    len -= res;
    const char *p = buf + res;

    res = parse_status_code(p, len, status_code);
    if (res < 0)
        return res;
    u32 rem = len - res;
    p += res;

    res = parse_reason_phrase(p, rem, reason_phrase, reason_len);
    if (res < 0)
        return res;
    return (p + res) - buf;
}

s32 sceParseHttp_AD7BFDEF(const char *buf, u32 len, const char *field_name,
                          const char **value_out, u32 *value_len_out)
{
    volatile s32 has_cr = 0;

    if (sceKernelCheckThreadStack() < 0x3c0)
        return 0x80410005;
    if (buf == NULL)
        return 0x80432060;
    if (field_name == NULL || value_out == NULL || value_len_out == NULL)
        return 0x804321fe;
    if (len == 0)
        return 0x80432025;

    s32 at_line_start = 1;
    u32 i = 0;
    while (i < len) {
        const char *cur = buf + i;
        if (at_line_start != 0) {
            at_line_start = 0;
            if ((CTYPE_TABLE[(s8)*cur] & 0x08) == 0) {
                u32 flen = parse_strlen(field_name);
                if (flen < len - i) {
                    u32 flen2 = parse_strlen(field_name);
                    if (parse_strncasecmp(field_name, cur, flen2) == 0) {
                        u32 flen3 = parse_strlen(field_name);
                        if (buf[i + flen3] == ':') {
                            i += parse_strlen(field_name);
                            goto found_field;
                        }
                    }
                }
            }
        }
        s8 ch = *cur;
        i++;
        if (ch == '\n')
            at_line_start = 1;
    }
    return 0x80432025;

found_field:
    while (i <= len) {
        if (buf[i] == ':') {
            i++;
            while (i < len && (CTYPE_TABLE[(s8)buf[i]] & 0x08) != 0)
                i++;
            u32 val_start = i;
            while (i < len) {
                const char *p = buf + i;
                u32 next_i = i + 1;
                if (*p == '\n') {
                    if (i != 1 && p[-1] == '\r')
                        has_cr = 1;
                    if (next_i < len && (p[1] == ' ' || p[1] == '\t')) {
                        i = next_i;
                        continue;
                    }
                    u32 end_pos = i - ((has_cr == 1) ? 1 : 0);
                    *value_out = buf + val_start;
                    *value_len_out = end_pos - val_start;
                    return end_pos;
                }
                i = next_i;
            }
            *value_out = buf + val_start;
            *value_len_out = len - val_start;
            return i;
        }
        i++;
    }
    return 0x80432025;
}

static s32 __attribute__((noipa)) parse_version(const char *buf, u32 len, s32 *ver_major, s32 *ver_minor)
{
    *ver_major = 0;
    *ver_minor = 0;
    if (len < 9)
        return 0x80432060;
    if (parse_strncmp(buf, "HTTP/", 5) != 0)
        return 0x80432060;
    if ((CTYPE_TABLE[(s8)buf[5]] & 0x04) == 0)
        return 0x80432060;

    u32 idx = 5;
    do {
        *ver_major = (*ver_major * 10) + buf[idx] - '0';
        idx++;
    } while ((CTYPE_TABLE[(s8)buf[idx]] & 0x04) != 0);

    if (buf[idx] != '.')
        return 0x80432060;
    idx++;
    if ((CTYPE_TABLE[(s8)buf[idx]] & 0x04) == 0)
        return 0x80432060;

    do {
        *ver_minor = (*ver_minor * 10) + buf[idx] - '0';
        idx++;
    } while ((CTYPE_TABLE[(s8)buf[idx]] & 0x04) != 0);

    return (buf[idx] == ' ') ? (s32)(idx + 1) : (s32)0x80432060;
}

static s32 __attribute__((noipa)) parse_status_code(const char *buf, u32 len, s32 *status_code)
{
    if (len < 4)
        return 0x80432060;
    s8 c0 = buf[0];
    if ((CTYPE_TABLE[c0] & 0x04) == 0)
        return 0x80432060;
    s8 c1 = buf[1];
    if ((CTYPE_TABLE[c1] & 0x04) == 0)
        return 0x80432060;
    s8 c2 = buf[2];
    if ((CTYPE_TABLE[c2] & 0x04) == 0)
        return 0x80432060;

    *status_code = (c0 * 100) + (c1 * 10) + c2 - 0x14d0;
    return 3;
}

static s32 __attribute__((noipa)) parse_strncasecmp(const char *s1, const char *s2, u32 n)
{
    if (n == 0)
        return 0;
    for (u32 i = 0; i < n; i++) {
        s8 c1 = s1[i];
        s8 c2 = s2[i];
        s32 u1 = c1 - (((CTYPE_TABLE[c1] >> 1) & 1) << 5);
        s32 u2 = c2 - (((CTYPE_TABLE[c2] >> 1) & 1) << 5);
        s32 diff = u1 - u2;
        if (diff != 0)
            return diff;
        if (c1 == 0)
            return 0;
    }
    return 0;
}

static s32 __attribute__((noipa)) parse_reason_phrase(const char *buf, u32 len,
                                                      const char **phrase_out, u32 *phrase_len_out)
{
    u32 i = 0;
    s32 has_cr = 0;
    while (i <= len) {
        if (buf[i] == '\n') {
            if (i != 0 && buf[i - 1] == '\r')
                has_cr = 1;
            *phrase_len_out = has_cr ? (i - 1) : i;
            *phrase_out = buf;
            return i + 1;
        }
        i++;
    }
    return 0x80432060;
}

static u32 __attribute__((noipa)) parse_strlen(const char *s)
{
    if (s == NULL)
        return 0;
    u32 len = 0;
    while (*s++ != '\0')
        len++;
    return len;
}

static s32 __attribute__((noipa)) parse_strncmp(const char *s1, const char *s2, s32 n)
{
    if (s1 == NULL || s2 == NULL) {
        if (s1 == s2)
            return 0;
        return (s1 != NULL) ? 1 : -1;
    }
    while (--n >= 0) {
        s8 c1 = *s1;
        s8 c2 = *s2++;
        if (c1 != c2)
            return c1 - c2;
        if (c1 == '\0')
            return 0;
        s1++;
    }
    return 0;
}
