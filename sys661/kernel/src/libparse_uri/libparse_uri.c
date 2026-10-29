/*
 * SceParseURI_Library - libparse_uri.prx
 * URI parsing, building, resolving, escaping, and unescaping library.
 */

#include <common_imp.h>
#include <stdarg.h>

SCE_MODULE_INFO("SceParseURI_Library", SCE_MODULE_USER | SCE_MODULE_ATTR_CANT_STOP | SCE_MODULE_ATTR_EXCLUSIVE_LOAD, 1, 1);
SCE_SDK_VERSION(0x06060110);

typedef struct SceParsedUri {
    s32 noSlash;            /* 0x00 */
    char *scheme;           /* 0x04 */
    char *userInfoUserName; /* 0x08 */
    char *userInfoPassword; /* 0x0C */
    char *host;             /* 0x10 */
    char *path;             /* 0x14 */
    char *query;            /* 0x18 */
    char *fragment;         /* 0x1C */
    u16 port;               /* 0x20 */
    u16 reserved[5];        /* 0x22..0x2B */
} SceParsedUri;

/* Imports from Kernel_Library */
extern void *sceKernelMemset(void *dst, int c, u32 size);
extern int _sceKernelCheckThreadStack(void);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 size);

/* Character classification table at 0x2A80 (g_ctype_table[c + 1]) */
static const u8 g_ctype_table[129] = {
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
    0x02, 0x02, 0x02, 0x10, 0x10, 0x10, 0x10, 0x20
};

/* URI escape table at 0x2778: 1 = unescaped, 3 = must escape */
static const s8 g_uri_escape_table[256] = {
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 3, 3, 1, 1, 3,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 3,
    3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1,
    3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
};

static const char g_hex_upper[] = "0123456789ABCDEF";
static const char g_hex_lower[] = "0123456789abcdef";

/* Forward declarations of internal helpers */
static int is_scheme_char(u8 c);
static int get_unescaped_len(const char *str);
static int parse_userinfo(const char *str, char *workArea, u32 workAreaSize, u32 *userSizeOut, u32 *passSizeOut, int copyFlag);
static int parse_scheme(char *workArea, const char *uri, u32 *sizeOut, u32 workAreaSize, int copyFlag);
static int parse_host(const char *str, char *workArea, u32 *sizeOut, u32 workAreaSize, int copyFlag);
static void get_default_port(const char *scheme, u16 *portOut);
static int parse_path(const char *str, char *workArea, u32 *sizeOut, u32 workAreaSize, int copyFlag);
static int parse_fragment(const char *str, char *workArea, u32 *sizeOut, u32 workAreaSize, int copyFlag);
static int parse_port(const char *str, u16 *portOut);
static int parse_query(const char *str, char *workArea, u32 *sizeOut, u32 workAreaSize, int copyFlag);
static int append_rel_path(char *basePath, const char *relPath);
static int normalize_path(char *dst, const char *src, u32 size);
static int hex_char_to_val(u8 c);
static char uri_tolower(char c);
static s8 get_ctype(int c);
static char *uri_strcat(char *dst, const char *src);
static int uri_strcasecmp(const char *s1, const char *s2);
static u32 uri_strlen(const char *s);
static int uri_strncmp(const char *s1, const char *s2, int n);
static char *uri_strchr(const char *s, char c);
static char *uri_strrchr(const char *s, char c);
static char *uri_strcpy(char *dst, const char *src);
static int uri_sprintf(char *buf, const char *fmt, ...);
static int uri_vformat(void (*put_fn)(void *, int), void *ctx, const char *fmt, va_list ap);
static void sprintf_put_cb(void *ctx, int c);
static const char *uri_memchr(const char *s, int c, int n);

/* 0x0000: sub_0000 */
static int is_scheme_char(u8 c)
{
    u8 flags = g_ctype_table[c + 1];
    if ((flags & 7) != 0 || c == '+' || c == '-' || c == '.') {
        return 1;
    }
    return 0;
}

/* 0x0058: sceParseUri_49E950EC (sceUriEscape) */
int sceParseUri_49E950EC(char *escaped, u32 *escapedLenOut, u32 escapedBufSize, const char *src)
{
    if (_sceKernelCheckThreadStack() < 0x3E0) {
        return 0x80410005;
    }

    int err = 0;
    u32 totalLen = 0;
    int doCopy = 1;

    if (escaped == NULL) {
        if (escapedLenOut == NULL) {
            return 0x804331FE;
        }
        doCopy = 0;
    }

    u8 c = (u8)*src;
    if (c != 0) {
        while (c != 0) {
            s8 action = g_uri_escape_table[c];
            if (action == 3) {
                totalLen += 3;
                if (doCopy == 1 && escapedBufSize < totalLen) {
                    err = 0x80433022;
                    goto done;
                }
                if (doCopy == 1) {
                    u8 b = (u8)*src;
                    escaped[0] = '%';
                    escaped[1] = g_hex_upper[b >> 4];
                    escaped[2] = g_hex_upper[b & 0x0F];
                    escaped += 3;
                }
            } else {
                totalLen += 1;
                if (doCopy == 1 && escapedBufSize < totalLen) {
                    err = 0x80433022;
                    goto done;
                }
                if (doCopy == 1) {
                    *escaped++ = (char)c;
                }
            }
            src++;
            c = (u8)*src;
        }
    }

    totalLen += 1;
    if (doCopy == 1 && escapedBufSize < totalLen) {
        return 0x80433022;
    }
    if (doCopy == 1) {
        *escaped = '\0';
    }
    if (escapedLenOut != NULL) {
        *escapedLenOut = totalLen;
    }

done:
    return err;
}

/* 0x01dc: sceParseUri_062BB07E (sceUriUnescape) */
int sceParseUri_062BB07E(char *unescaped, u32 *unescapedLenOut, u32 unescapedBufSize, const char *src)
{
    if (_sceKernelCheckThreadStack() < 0x3C0) {
        return 0x80410005;
    }
    if (src == NULL) {
        return 0x804331FE;
    }

    u32 totalLen = 0;
    int doCopy = 1;

    if (unescaped == NULL) {
        if (unescapedLenOut == NULL) {
            return 0x804331FE;
        }
        doCopy = 0;
    }

    int check = get_unescaped_len(src);
    if (check < 0) {
        return 0x80433060;
    }

    u8 c = (u8)*src;
    while (c != 0) {
        totalLen += 1;
        if (doCopy == 1 && unescapedBufSize < totalLen) {
            return 0x80433022;
        }
        if ((s8)c == '%') {
            if (doCopy == 1) {
                int hi = hex_char_to_val((u8)src[1]);
                int lo = hex_char_to_val((u8)src[2]);
                *unescaped++ = (char)((hi << 4) + lo);
            }
            src += 3;
        } else {
            if (doCopy == 1) {
                *unescaped++ = (char)c;
            }
            src += 1;
        }
        c = (u8)*src;
    }

    totalLen += 1;
    if (doCopy == 1 && unescapedBufSize < totalLen) {
        return 0x80433022;
    }
    if (doCopy == 1) {
        *unescaped = '\0';
    }
    if (unescapedLenOut != NULL) {
        *unescapedLenOut = totalLen;
    }
    return 0;
}

/* 0x0374: sceParseUri_568518C9 (sceUriParse) */
int sceParseUri_568518C9(SceParsedUri *parsedArea, const char *url, void *workArea, u32 *workAreaSizeOut, u32 workAreaSize)
{
    u16 portVal = 0;
    u32 elemSize = 0;
    u32 userSize = 0;
    u32 passSize = 0;
    int zeroPort = 0;

    if (_sceKernelCheckThreadStack() < 0x3B0) {
        return 0x80410005;
    }
    if (url == NULL) {
        return 0x80433060;
    }

    int doCopy = 1;
    if (workArea == NULL || parsedArea == NULL) {
        doCopy = 0;
        if (workAreaSizeOut == NULL) {
            return 0x804331FE;
        }
    }

    if (doCopy == 1) {
        sceKernelMemset(parsedArea, 0, sizeof(SceParsedUri));
    }

    int len = parse_scheme((char *)workArea, url, &elemSize, workAreaSize, doCopy);
    if (len < 0) {
        return len;
    }

    int hasScheme = 0;
    if (doCopy == 1) {
        parsedArea->scheme = (char *)workArea;
    }
    if (len != 0) {
        hasScheme = 1;
    }

    const char *p = url + len;
    u32 curOffset = len;
    u32 curWorkOffset = elemSize;

    int slashCount = 0;
    if (p != NULL && *p == '/') {
        slashCount = 1;
        while (p[slashCount] == '/') {
            slashCount++;
        }
    }

    int hasPort = 0;
    if (hasScheme == 1 && slashCount >= 2) {
        hasScheme = 1;
        curOffset += 2;
        p = url + curOffset;
    } else {
        if (doCopy == 1) {
            parsedArea->noSlash = 1;
        }
    }

    char *workPtr = (char *)workArea + curWorkOffset;
    len = parse_userinfo(p, workPtr, workAreaSize - curWorkOffset, &userSize, &passSize, doCopy);
    if (len < 0) {
        return len;
    }
    if (doCopy == 1) {
        parsedArea->userInfoUserName = workPtr;
        parsedArea->userInfoPassword = (char *)workArea + curWorkOffset + userSize;
    }

    curOffset += len;
    p = url + curOffset;
    curWorkOffset += userSize + passSize;
    workPtr = (char *)workArea + curWorkOffset;

    len = parse_host(p, workPtr, &elemSize, workAreaSize - curWorkOffset, doCopy);
    if (len < 0) {
        return len;
    }
    if (doCopy == 1) {
        parsedArea->host = workPtr;
    }

    curOffset += len;
    p = url + curOffset;
    curWorkOffset += elemSize;

    if (*p == ':') {
        curOffset += 1;
        hasPort = 1;
        p = url + curOffset;
    }

    if (hasPort == 1) {
        len = parse_port(p, &portVal);
        if (len < 0) {
            return len;
        }
        if (len == 0) {
            zeroPort = 1;
        } else {
            if (doCopy == 1) {
                parsedArea->port = portVal;
            }
            curOffset += len;
            p = url + curOffset;
        }
    }

    if (hasPort == 0 || zeroPort != 0) {
        if (doCopy == 1) {
            get_default_port(parsedArea->scheme, &portVal);
            parsedArea->port = portVal;
        }
    }

    workPtr = (char *)workArea + curWorkOffset;
    len = parse_path(p, workPtr, &elemSize, workAreaSize - curWorkOffset, doCopy);
    if (len < 0) {
        return len;
    }
    if (doCopy == 1) {
        parsedArea->path = workPtr;
    }

    curOffset += len;
    p = url + curOffset;
    curWorkOffset += elemSize;
    workPtr = (char *)workArea + curWorkOffset;

    len = parse_query(p, workPtr, &elemSize, workAreaSize - curWorkOffset, doCopy);
    if (len < 0) {
        return len;
    }
    if (doCopy == 1) {
        parsedArea->query = workPtr;
    }

    curOffset += len;
    p = url + curOffset;
    curWorkOffset += elemSize;
    workPtr = (char *)workArea + curWorkOffset;

    len = parse_fragment(p, workPtr, &elemSize, workAreaSize - curWorkOffset, doCopy);
    if (len < 0) {
        return len;
    }
    if (doCopy == 1) {
        parsedArea->fragment = workPtr;
    }

    curWorkOffset += elemSize;
    if (workAreaSizeOut != NULL) {
        *workAreaSizeOut = curWorkOffset;
    }

    return 0;
}

/* 0x06f0: sceParseUri_7EE318AF (sceUriBuild) */
int sceParseUri_7EE318AF(char *url, u32 *urlLenOut, u32 urlBufSize, const SceParsedUri *parsedArea, u32 flags)
{
    u16 defaultPort = 0;
    char portBuf[16];
    u32 schemeLen = 0;
    u32 hostLen = 0;
    u32 portLen = 0;
    u32 pathLen = 0;
    u32 queryLen = 0;
    u32 userLen = 0;
    u32 passLen = 0;
    u32 fragLen = 0;
    int doCopy = 1;
    int hasUserOrPass = 0;

    if (_sceKernelCheckThreadStack() < 0x370) {
        return 0x80410005;
    }
    if (parsedArea == NULL) {
        return 0x80433060;
    }
    if (url == NULL) {
        if (urlLenOut == NULL) {
            return 0x804331FE;
        }
        doCopy = 0;
    }

    u32 totalLen = 0;
    int isDefaultPort = 0;

    if ((flags & 0x01) != 0) {
        if (parsedArea->scheme != NULL) {
            schemeLen = uri_strlen(parsedArea->scheme);
            totalLen = schemeLen + 1;
        }
    }
    if (parsedArea->noSlash != 1) {
        totalLen += 2;
    }
    if ((flags & 0x10) != 0) {
        if (parsedArea->userInfoUserName != NULL && parsedArea->userInfoUserName[0] != '\0') {
            userLen = uri_strlen(parsedArea->userInfoUserName);
            totalLen += userLen + 1;
            hasUserOrPass = 1;
        }
    }
    if ((flags & 0x20) != 0) {
        if (parsedArea->userInfoPassword != NULL && parsedArea->userInfoPassword[0] != '\0') {
            passLen = uri_strlen(parsedArea->userInfoPassword);
            if (hasUserOrPass == 1) {
                totalLen += passLen + 1;
            } else {
                hasUserOrPass = 1;
                totalLen += passLen + 1 + userLen + 1;
            }
        }
    }
    if ((flags & 0x02) != 0) {
        if (parsedArea->host != NULL) {
            hostLen = uri_strlen(parsedArea->host);
            totalLen += hostLen;
        }
    }
    if ((flags & 0x04) != 0) {
        if (parsedArea->scheme != NULL) {
            get_default_port(parsedArea->scheme, &defaultPort);
            if (defaultPort == 0 && uri_strcasecmp(parsedArea->scheme, "mailto") != 0) {
                if (parsedArea->port == defaultPort) {
                    isDefaultPort = 1;
                }
            } else if (defaultPort != 0) {
                if (parsedArea->port == defaultPort) {
                    isDefaultPort = 1;
                }
            }
        }
        if (isDefaultPort != 1 && parsedArea->port != 0) {
            uri_sprintf(portBuf, ":%d", parsedArea->port);
            portLen = uri_strlen(portBuf);
            totalLen += portLen;
        }
    }
    if ((flags & 0x08) != 0) {
        if (parsedArea->path != NULL) {
            pathLen = uri_strlen(parsedArea->path);
            totalLen += pathLen;
        }
    }
    if ((flags & 0x40) != 0) {
        if (parsedArea->query != NULL) {
            queryLen = uri_strlen(parsedArea->query);
            totalLen += queryLen;
        }
    }
    if ((flags & 0x80) != 0) {
        if (parsedArea->fragment != NULL) {
            fragLen = uri_strlen(parsedArea->fragment);
            totalLen += fragLen;
        }
    }

    totalLen += 1;
    if (urlLenOut != NULL) {
        *urlLenOut = totalLen;
    }
    if (doCopy == 0) {
        return 0;
    }
    if (urlBufSize < totalLen) {
        return 0x80433022;
    }

    u32 offset = 0;
    if ((flags & 0x01) != 0 && parsedArea->scheme != NULL && parsedArea->scheme[0] != '\0') {
        sceKernelMemcpy(url, parsedArea->scheme, schemeLen);
        sceKernelMemcpy(url + schemeLen, ":", 1);
        offset = schemeLen + 1;
    }
    if (parsedArea->noSlash != 1) {
        sceKernelMemcpy(url + offset, "//", 2);
        offset += 2;
    }
    if ((flags & 0x10) != 0 && parsedArea->userInfoUserName != NULL && parsedArea->userInfoUserName[0] != '\0') {
        sceKernelMemcpy(url + offset, parsedArea->userInfoUserName, userLen);
        offset += userLen;
    }
    if ((flags & 0x20) != 0 && parsedArea->userInfoPassword != NULL && parsedArea->userInfoPassword[0] != '\0') {
        sceKernelMemcpy(url + offset, ":", 1);
        sceKernelMemcpy(url + offset + 1, parsedArea->userInfoPassword, passLen);
        offset += passLen + 1;
    }
    if (hasUserOrPass == 1) {
        sceKernelMemcpy(url + offset, "@", 1);
        offset += 1;
    }
    if ((flags & 0x02) != 0 && parsedArea->host != NULL && parsedArea->host[0] != '\0') {
        sceKernelMemcpy(url + offset, parsedArea->host, hostLen);
        offset += hostLen;
    }
    if ((flags & 0x04) != 0 && isDefaultPort != 1) {
        sceKernelMemcpy(url + offset, portBuf, portLen);
        offset += portLen;
    }
    if ((flags & 0x08) != 0 && parsedArea->path != NULL) {
        sceKernelMemcpy(url + offset, parsedArea->path, pathLen);
        offset += pathLen;
    }
    if ((flags & 0x40) != 0 && parsedArea->query != NULL) {
        sceKernelMemcpy(url + offset, parsedArea->query, queryLen);
        offset += queryLen;
    }
    if ((flags & 0x80) != 0 && parsedArea->fragment != NULL) {
        sceKernelMemcpy(url + offset, parsedArea->fragment, fragLen);
        offset += fragLen;
    }
    url[offset] = '\0';
    return 0;
}

/* 0x0c48: sceParseUri_8AA55D17 */
int sceParseUri_8AA55D17(char *workArea, const char *baseUri, const char *relUri, u32 *workAreaSizeOut, u32 workAreaSize, u32 flags)
{
    SceParsedUri parsed;
    u32 baseWorkSize = 0;
    u32 relWorkSize = 0;

    if (_sceKernelCheckThreadStack() < 0x390) {
        return 0x80410005;
    }
    if (flags != 0) {
        return 0x804331FE;
    }

    int ret = sceParseUri_568518C9(NULL, baseUri, NULL, &baseWorkSize, 0);
    if (ret < 0) {
        return ret;
    }
    ret = sceParseUri_568518C9(NULL, relUri, NULL, &relWorkSize, 0);
    if (ret < 0) {
        return ret;
    }

    u32 baseLen = uri_strlen(baseUri);
    u32 relLen = uri_strlen(relUri);
    u32 reqSize = baseWorkSize + ((baseLen + relLen) << 1) + 2;
    if (workAreaSizeOut != NULL) {
        *workAreaSizeOut = reqSize;
    }
    if (workArea == NULL) {
        return 0;
    }
    if (workAreaSize < reqSize) {
        return 0x80433022;
    }

    baseLen = uri_strlen(baseUri);
    relLen = uri_strlen(relUri);
    u32 tempOffset = baseLen + relLen + 1;
    char *tempArea = workArea + tempOffset;
    u32 tempSize = workAreaSize - tempOffset;

    ret = sceParseUri_568518C9(&parsed, relUri, tempArea, &relWorkSize, tempSize);
    if (ret < 0) {
        return ret;
    }

    if (parsed.noSlash == 0) {
        uri_strcpy(workArea, relUri);
        if (workAreaSizeOut != NULL) {
            *workAreaSizeOut = uri_strlen(relUri) + 1;
        }
        return 0;
    }

    ret = sceParseUri_568518C9(&parsed, baseUri, tempArea, &baseWorkSize, tempSize);
    if (ret < 0) {
        return ret;
    }

    char *pathWork = workArea + tempOffset + baseWorkSize;
    uri_strcpy(pathWork, parsed.path);
    ret = append_rel_path(pathWork, relUri);
    if (ret < 0) {
        return ret;
    }

    u32 pathWorkLen = uri_strlen(pathWork);
    u32 bLen = uri_strlen(baseUri);
    parsed.path = pathWork;
    u32 rLen = uri_strlen(relUri);
    u32 buildSize = workAreaSize - pathWorkLen - bLen - rLen - 1;

    ret = sceParseUri_7EE318AF(workArea, NULL, buildSize, &parsed, 0x3F);
    if (ret < 0) {
        return ret;
    }
    return 0;
}

/* 0x0e60: sceParseUri_8885A782 */
int sceParseUri_8885A782(char *dst, const char *src, u32 size)
{
    if (_sceKernelCheckThreadStack() < 0x400) {
        return 0x80410005;
    }
    return normalize_path(dst, src, size);
}

/* 0x0ec0: sub_0ec0 */
static int get_unescaped_len(const char *str)
{
    int count = 0;
    s8 c = *str;
    while (c != 0) {
        if (c == '%') {
            u8 c1 = (u8)str[1];
            if (c1 == 0) {
                return -1;
            }
            if (!((u32)(c1 - '0') < 10 || (u32)(c1 - 'A') < 6 || (u32)(c1 - 'a') < 6)) {
                return -1;
            }
            u8 c2 = (u8)str[2];
            if (c2 == 0) {
                return -1;
            }
            if (!((u32)(c2 - '0') < 10 || (u32)(c2 - 'A') < 6 || (u32)(c2 - 'a') < 6)) {
                return -1;
            }
            count += 3;
            str += 1;
        } else if (c == '+') {
            count += 1;
            str += 1;
        } else {
            if (g_uri_escape_table[(u8)c] != 1) {
                return -1;
            }
            count += 1;
            str += 1;
        }
        c = *str;
    }
    return count;
}

/* 0x0f94: sub_0f94 */
static int parse_userinfo(const char *str, char *workArea, u32 workAreaSize, u32 *userSizeOut, u32 *passSizeOut, int copyFlag)
{
    int hasAt = 0;
    int hasColon = 0;
    u32 colonPos = 0;
    u32 atPos = 0;

    *userSizeOut = 0;
    *passSizeOut = 0;
    if (str == NULL) {
        return 0x80433060;
    }

    u32 idx = 0;
    while (str[idx] != '\0') {
        s8 ch = str[idx];
        if (ch == '@') {
            atPos = idx + 1;
            hasAt = 1;
            break;
        }
        if (!hasColon && ch == ':') {
            colonPos = idx;
            hasColon = 1;
            idx++;
            continue;
        }
        u8 uch = (u8)ch;
        int valid = 0;
        if ((g_ctype_table[uch + 1] & 7) != 0 || uch == '-' || uch == '_' || uch == '.' ||
            uch == '!' || uch == '~' || uch == '*' || uch == '\'' || uch == '(' || uch == ')') {
            valid = 1;
        }
        if (!valid) {
            if (uch == '%' || uch == ';' || uch == ':' || uch == '&' ||
                uch == '=' || uch == '+' || uch == ',' || uch == '$') {
                idx++;
                continue;
            }
            break;
        }
        idx++;
    }

    if (!hasAt) {
        *userSizeOut = 1;
        *passSizeOut = 1;
        u32 req = *userSizeOut + 1;
        if (copyFlag == 1) {
            if (workAreaSize < req) {
                return 0x80433022;
            }
            workArea[0] = '\0';
            workArea[1] = '\0';
        }
        return 0;
    }

    u32 passDiff = atPos - colonPos;
    u32 passCopyLen;
    u32 userCopyLen;
    if (hasColon) {
        *passSizeOut = passDiff - 1;
        passCopyLen = passDiff - 2;
        userCopyLen = colonPos;
    } else {
        *userSizeOut = atPos;
        *passSizeOut = 1;
        userCopyLen = atPos - 1;
        passCopyLen = 0;
    }

    u32 totalReq = *userSizeOut + *passSizeOut;
    if (copyFlag == 1) {
        if (workAreaSize < totalReq) {
            return 0x80433022;
        }
        sceKernelMemcpy(workArea, str, userCopyLen);
        workArea[userCopyLen] = '\0';
        u32 uSize = *userSizeOut;
        sceKernelMemcpy(workArea + uSize, str + uSize, passCopyLen);
        workArea[uSize + passCopyLen] = '\0';
    }
    return atPos;
}

/* 0x1248: sub_1248 */
static int parse_scheme(char *workArea, const char *uri, u32 *sizeOut, u32 workAreaSize, int copyFlag)
{
    if (uri == NULL) {
        return 0x80433060;
    }

    u32 len = uri_strlen(uri);
    u32 idx = 0;
    while (is_scheme_char((u8)uri[idx]) == 1 && idx != len) {
        idx++;
    }

    if ((g_ctype_table[(u8)uri[0] + 1] & 4) == 0 && uri[idx] == ':') {
        u32 req = idx + 1;
        *sizeOut = req;
        if (copyFlag == 1) {
            if (workAreaSize < req) {
                return 0x80433022;
            }
            sceKernelMemcpy(workArea, uri, idx);
            workArea[idx] = '\0';
        }
        return req;
    }

    if (sizeOut != NULL) {
        *sizeOut = 1;
    }
    if (copyFlag == 1) {
        if (workAreaSize < 2) {
            return 0x80433022;
        }
        workArea[0] = '\0';
    }
    return 0;
}

/* 0x1394: sub_1394 */
static int parse_host(const char *str, char *workArea, u32 *sizeOut, u32 workAreaSize, int copyFlag)
{
    if (str == NULL) {
        return 0x80433060;
    }

    u32 idx = 0;
    if (str[0] != '.') {
        while (1) {
            u8 c = (u8)str[idx];
            if ((g_ctype_table[c + 1] & 7) != 0 || c == '.' || c == '-') {
                idx++;
            } else {
                break;
            }
        }
    }

    u32 req = idx + 1;
    if (copyFlag == 1) {
        if (workAreaSize < req) {
            return 0x80433022;
        }
        sceKernelMemcpy(workArea, str, idx);
        workArea[idx] = '\0';
    }
    if (sizeOut != NULL) {
        *sizeOut = req;
    }
    return idx;
}

/* 0x1490: sub_1490 */
static void get_default_port(const char *scheme, u16 *portOut)
{
    char upperBuf[128];

    if (scheme == NULL) {
        *portOut = 0;
        return;
    }
    int len = uri_strlen(scheme);
    if (len >= 128) {
        *portOut = 0;
        return;
    }

    int i = 0;
    for (i = 0; i < len; i++) {
        s8 ch = scheme[i];
        u8 flags = g_ctype_table[(u8)ch + 1];
        if ((flags & 2) != 0) {
            ch -= 0x20;
        }
        upperBuf[i] = ch;
    }
    upperBuf[i] = '\0';

    if (uri_strncmp(upperBuf, "HTTPS", 5) == 0) {
        *portOut = 443;
    } else if (uri_strncmp(upperBuf, "HTTP", 4) == 0) {
        *portOut = 80;
    } else if (uri_strncmp(upperBuf, "FTP", 3) == 0) {
        *portOut = 21;
    } else if (uri_strncmp(upperBuf, "SVN+SSH", 7) == 0) {
        *portOut = 22;
    } else if (uri_strncmp(upperBuf, "IMAP", 4) == 0) {
        *portOut = 143;
    } else if (uri_strncmp(upperBuf, "GOPHER", 6) == 0) {
        *portOut = 70;
    } else if (uri_strncmp(upperBuf, "RTSP", 4) == 0) {
        *portOut = 554;
    } else if (uri_strncmp(upperBuf, "SSH", 3) == 0) {
        *portOut = 22;
    } else {
        *portOut = 0;
    }
}

/* 0x160c: sub_160c */
static int parse_path(const char *str, char *workArea, u32 *sizeOut, u32 workAreaSize, int copyFlag)
{
    if (str == NULL) {
        return 0x80433060;
    }

    u32 idx = 0;
    while (str[idx] != '?' && str[idx] != '#' && str[idx] != '\0') {
        idx++;
    }

    u32 req = idx + 1;
    if (copyFlag == 1) {
        if (workAreaSize < req) {
            return 0x80433022;
        }
        sceKernelMemcpy(workArea, str, idx);
        workArea[idx] = '\0';
        normalize_path(workArea, str, req);
    }
    if (sizeOut != NULL) {
        *sizeOut = req;
    }
    return idx;
}

/* 0x16ec: sub_16ec */
static int parse_fragment(const char *str, char *workArea, u32 *sizeOut, u32 workAreaSize, int copyFlag)
{
    if (str == NULL) {
        return 0x80433060;
    }

    u32 idx = 0;
    while (str[idx] != '\0') {
        idx++;
    }

    u32 req = idx + 1;
    if (copyFlag == 1) {
        if (workAreaSize < req) {
            return 0x80433022;
        }
        sceKernelMemcpy(workArea, str, idx);
        workArea[idx] = '\0';
    }
    if (sizeOut != NULL) {
        *sizeOut = req;
    }
    return idx;
}

/* 0x1790: sub_1790 */
static int parse_port(const char *str, u16 *portOut)
{
    *portOut = 0;
    if (str == NULL) {
        return 0x80433060;
    }

    u32 val = 0;
    u32 idx = 0;
    while (idx < 6) {
        s8 ch = str[idx];
        if ((g_ctype_table[(u8)ch + 1] & 4) == 0) {
            if (val > 0xFFFF || (ch != '/' && ch != '\0')) {
                return 0x80433060;
            }
            *portOut = (u16)val;
            return idx;
        }
        val = val * 10 + (ch - '0');
        idx++;
    }
    return 0x80433060;
}

/* 0x1830: sub_1830 */
static int parse_query(const char *str, char *workArea, u32 *sizeOut, u32 workAreaSize, int copyFlag)
{
    if (str == NULL) {
        return 0x80433060;
    }

    u32 idx = 0;
    while (str[idx] != '#' && str[idx] != '\0') {
        idx++;
    }

    u32 req = idx + 1;
    if (copyFlag == 1) {
        if (workAreaSize < req) {
            return 0x80433022;
        }
        sceKernelMemcpy(workArea, str, idx);
        workArea[idx] = '\0';
    }
    if (sizeOut != NULL) {
        *sizeOut = req;
    }
    return idx;
}

/* 0x18e4: sub_18e4 */
static int append_rel_path(char *basePath, const char *relPath)
{
    char *slash = uri_strrchr(basePath, '/');
    if (slash != NULL) {
        slash[1] = '\0';
    } else {
        basePath[0] = '/';
        basePath[1] = '\0';
    }
    if (relPath[0] == '/') {
        basePath[0] = '\0';
    }
    uri_strcat(basePath, relPath);
    return 0;
}

/* 0x1968: sub_1968 */
static int normalize_path(char *dst, const char *src, u32 size)
{
    if (size == 0) {
        return 0;
    }
    if (src[0] != '/') {
        sceKernelMemcpy(dst, src, size - 1);
        dst[size - 1] = '\0';
        return 0;
    }

    u32 maxIdx = size - 1;
    u32 idx = 1;
    dst[0] = '/';
    if (idx >= maxIdx) {
        return 0;
    }
    dst[1] = '\0';

    char *outPtr = dst;
    while (idx < maxIdx) {
        const char *p = src + idx;
        if (p[0] == '.' && p[1] == '.' && p[2] == '/') {
            if (outPtr != dst) {
                *outPtr = '\0';
                char *prev = uri_strrchr(dst, '/');
                if (prev != NULL) {
                    outPtr = prev;
                    prev[1] = '\0';
                }
            }
            idx += 3;
            continue;
        }
        if (p[0] == '.' && p[1] == '/') {
            idx += 2;
            continue;
        }
        const char *nextSlash = uri_strchr(p, '/');
        u32 segLen = (size - idx) - 1;
        if (nextSlash != NULL) {
            u32 sLen = (u32)(nextSlash - p) + 1;
            if (sLen < segLen) {
                segLen = sLen;
            }
        }
        sceKernelMemcpy(outPtr + 1, p, segLen);
        outPtr += segLen;
        outPtr[1] = '\0';
        idx += segLen;
    }
    return 0;
}

/* 0x1ae8: sub_1ae8 */
static int hex_char_to_val(u8 c)
{
    if ((u32)((c - '0') & 0xFF) < 10) {
        return c - '0';
    }
    if ((u32)(c - 'A') < 6) {
        return c - '7';
    }
    if ((u32)(c - 'a') < 6) {
        return c - 'W';
    }
    return -1;
}

/* 0x1b28: sub_1b28 */
static char uri_tolower(char c)
{
    if ((g_ctype_table[(s8)c + 1] & 1) != 0) {
        return (char)(c + 0x20);
    }
    return c;
}

/* 0x1b54: sub_1b54 */
static s8 get_ctype(int c)
{
    return (s8)g_ctype_table[c + 1];
}

/* 0x1b68: sub_1b68 */
static char *uri_strcat(char *dst, const char *src)
{
    if (dst == NULL || src == NULL) {
        return NULL;
    }
    u32 dLen = uri_strlen(dst);
    u32 sLen = uri_strlen(src);
    if (dst + dLen == src + sLen) {
        return NULL;
    }
    char *p = dst;
    while (*p != '\0') {
        p++;
    }
    while (1) {
        u8 c = (u8)*src++;
        *p++ = (char)c;
        if (c == 0) {
            break;
        }
    }
    return dst;
}

/* 0x1c08: sub_1c08 */
static int uri_strcasecmp(const char *s1, const char *s2)
{
    if (s1 == NULL || s2 == NULL) {
        if (s1 == s2) {
            return 0;
        }
        return (s1 != NULL) ? 1 : -1;
    }
    while (1) {
        char c1 = uri_tolower(*s1);
        char c2 = uri_tolower(*s2++);
        if (c1 != c2) {
            return uri_tolower(*s1) - uri_tolower(s2[-1]);
        }
        if (*s1 == '\0') {
            return 0;
        }
        s1++;
    }
}

/* 0x1cb4: sub_1cb4 */
static u32 uri_strlen(const char *s)
{
    if (s == NULL) {
        return 0;
    }
    u32 len = 0;
    while (*s++ != '\0') {
        len++;
    }
    return len;
}

/* 0x1ce8: sub_1ce8 */
static int uri_strncmp(const char *s1, const char *s2, int n)
{
    if (s1 == NULL || s2 == NULL) {
        if (s1 == s2) {
            return 0;
        }
        return (s1 != NULL) ? 1 : -1;
    }
    while (--n >= 0) {
        s8 c1 = *s1;
        s8 c2 = *s2++;
        if (c1 != c2) {
            return *s1 - s2[-1];
        }
        if (c1 == 0) {
            return 0;
        }
        s1++;
    }
    return 0;
}

/* 0x1d84: sub_1d84 */
static char *uri_strchr(const char *s, char c)
{
    if (s == NULL) {
        return NULL;
    }
    while (1) {
        s8 ch = *s;
        if (ch == c) {
            return (char *)s;
        }
        s++;
        if (ch == 0) {
            return NULL;
        }
    }
}

/* 0x1db4: sub_1db4 */
static char *uri_strrchr(const char *s, char c)
{
    if (s == NULL) {
        return NULL;
    }
    const char *p = s;
    while (*p != '\0') {
        p++;
    }
    while (1) {
        if (*p == c) {
            return (char *)p;
        }
        if (p <= s) {
            break;
        }
        p--;
    }
    return NULL;
}

/* 0x1dfc: sub_1dfc */
static char *uri_strcpy(char *dst, const char *src)
{
    if (dst == NULL || src == NULL) {
        return NULL;
    }
    char *p = dst;
    while (1) {
        u8 c = (u8)*src++;
        *p++ = (char)c;
        if (c == 0) {
            break;
        }
    }
    return dst;
}

/* 0x1e34: sub_1e34 */
static int uri_sprintf(char *buf, const char *fmt, ...)
{
    char *p = buf;
    va_list ap;
    va_start(ap, fmt);
    int ret = uri_vformat(sprintf_put_cb, &p, fmt, ap);
    va_end(ap);
    return ret;
}

/* 0x1e80: sub_1e80 */
static int uri_vformat(void (*put_fn)(void *, int), void *ctx, const char *fmt, va_list ap)
{
    char numBuf[22];
    if (fmt == NULL) {
        return 0;
    }
    put_fn(ctx, 0x200);
    int count = 0;

    while (*fmt != '\0') {
        if (*fmt != '%') {
            put_fn(ctx, *fmt);
            count++;
            fmt++;
            continue;
        }

        u32 flags = 0;
        int width = 0;
        int prec = -1;
        int signChar = 0;
        const char *hexDigits = g_hex_lower;

    next_spec:
        fmt++;
        char c = *fmt;
        switch (c) {
        case '\0':
            goto done;
        case ' ':
            if (signChar == 0) {
                signChar = ' ';
            }
            goto next_spec;
        case '#':
            flags |= 0x08;
            goto next_spec;
        case '*':
            width = va_arg(ap, int);
            if (width < 0) {
                width = -width;
                flags |= 0x10;
            }
            goto next_spec;
        case '+':
            signChar = '+';
            goto next_spec;
        case '-':
            flags |= 0x10;
            goto next_spec;
        case '.':
            fmt++;
            if (*fmt == '*') {
                prec = va_arg(ap, int);
            } else {
                prec = 0;
                while (*fmt >= 0 && (get_ctype(*fmt) & 4) != 0) {
                    prec = prec * 10 + (*fmt - '0');
                    fmt++;
                }
                fmt--;
            }
            if (prec < -1) {
                prec = -1;
            }
            goto next_spec;
        case '0':
            flags |= 0x20;
            goto next_spec;
        case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            width = 0;
            do {
                width = width * 10 + (*fmt - '0');
                fmt++;
            } while (*fmt >= 0 && (get_ctype(*fmt) & 4) != 0);
            fmt--;
            goto next_spec;
        case 'h':
            flags |= 0x04;
            goto next_spec;
        case 'l':
            flags |= 0x01;
            goto next_spec;
        case 'L':
            flags |= 0x02;
            goto next_spec;
        case 'D':
        case 'O':
        case 'U':
            flags |= 0x01;
            /* fallthrough */
        case 'd':
        case 'i':
        case 'o':
        case 'u':
        case 'X':
        case 'x':
        case 'p': {
            u32 uval;
            u32 base = 10;
            if (c == 'd' || c == 'i' || c == 'D') {
                s32 sval = va_arg(ap, s32);
                if (sval < 0) {
                    signChar = '-';
                    uval = (u32)(-sval);
                } else {
                    uval = (u32)sval;
                }
            } else if (c == 'o' || c == 'O') {
                uval = va_arg(ap, u32);
                base = 8;
                signChar = 0;
            } else if (c == 'u' || c == 'U') {
                uval = va_arg(ap, u32);
                base = 10;
                signChar = 0;
            } else if (c == 'p') {
                uval = va_arg(ap, u32);
                base = 16;
                signChar = 0;
            } else {
                if (c == 'X') {
                    hexDigits = g_hex_upper;
                }
                uval = va_arg(ap, u32);
                base = 16;
                if ((flags & 0x08) != 0 && uval != 0) {
                    flags |= 0x40;
                }
                signChar = 0;
            }

            if (prec >= 0) {
                flags &= ~0x20;
            }

            char *pNum = &numBuf[21];
            if (uval != 0 || prec != 0) {
                do {
                    *--pNum = hexDigits[uval % base];
                    uval /= base;
                } while (uval != 0);
                if ((flags & 0x08) != 0 && base == 8 && *pNum != '0') {
                    *--pNum = '0';
                }
            }
            int sLen = (int)(&numBuf[21] - pNum);
            const char *sPtr = pNum;
            goto format_str;
        }
        case 'c': {
            numBuf[0] = (char)va_arg(ap, int);
            const char *sPtr = numBuf;
            int sLen = 1;
            signChar = 0;
            goto format_str;
        }
        case 's': {
            const char *sPtr = va_arg(ap, const char *);
            if (sPtr == NULL) {
                sPtr = "(null)";
            }
            int sLen;
            if (prec >= 0) {
                const char *end = uri_memchr(sPtr, 0, prec);
                if (end != NULL && (end - sPtr) < prec) {
                    sLen = (int)(end - sPtr);
                } else {
                    sLen = prec;
                }
            } else {
                sLen = (int)uri_strlen(sPtr);
            }
            signChar = 0;
        format_str:
            {
                int fullLen = sLen;
                if (signChar != 0) {
                    fullLen++;
                }
                if ((flags & 0x40) != 0) {
                    fullLen += 2;
                }
                int padLen = (fullLen > prec) ? fullLen : prec;
                if ((flags & 0x30) == 0 && padLen < width) {
                    int n = width - padLen;
                    while (n-- > 0) {
                        put_fn(ctx, ' ');
                    }
                }
                if (signChar != 0) {
                    put_fn(ctx, signChar);
                }
                if ((flags & 0x40) != 0) {
                    put_fn(ctx, '0');
                    put_fn(ctx, *fmt);
                }
                if ((flags & 0x30) == 0x20 && padLen < width) {
                    int n = width - padLen;
                    while (n-- > 0) {
                        put_fn(ctx, '0');
                    }
                }
                if (fullLen < prec) {
                    int n = prec - fullLen;
                    while (n-- > 0) {
                        put_fn(ctx, '0');
                    }
                }
                int i = sLen;
                while (--i >= 0) {
                    put_fn(ctx, *sPtr++);
                }
                if ((flags & 0x10) != 0 && padLen < width) {
                    int n = width - padLen;
                    while (n-- > 0) {
                        put_fn(ctx, ' ');
                    }
                }
                count += (padLen > width) ? padLen : width;
            }
            break;
        }
        case 'n':
            if ((flags & 0x01) != 0) {
                s32 *p = va_arg(ap, s32 *);
                *p = count;
            } else if ((flags & 0x04) != 0) {
                s16 *p = va_arg(ap, s16 *);
                *p = (s16)count;
            } else {
                s32 *p = va_arg(ap, s32 *);
                *p = count;
            }
            break;
        default:
            put_fn(ctx, c);
            count++;
            break;
        }
        fmt++;
    }

done:
    put_fn(ctx, 0x201);
    return count;
}

/* 0x2590: sub_2590 */
static void sprintf_put_cb(void *ctx, int c)
{
    char **p = (char **)ctx;
    if (c < 0x100) {
        *(*p)++ = (char)c;
    } else if (c == 0x201) {
        **p = '\0';
    }
}

/* 0x25b8: sub_25b8 */
static const char *uri_memchr(const char *s, int c, int n)
{
    if (s == NULL || n <= 0) {
        return NULL;
    }
    u8 target = (u8)c;
    while (n > 0) {
        if ((u8)*s == target) {
            return s;
        }
        s++;
        n--;
    }
    return NULL;
}
