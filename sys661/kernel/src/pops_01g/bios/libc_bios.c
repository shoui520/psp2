/*
 * PlayStation 1 BIOS C runtime (A0/B0 libc, heap allocator, printf/sprintf,
 * Shift-JIS ROM font lookup Krom2RawAdd, and system metadata queries).
 */

#include "bios_internal.h"
#include <stdarg.h>

void *memcpy(void *dst, const void *src, size_t n)
{
    return biosMemcpy(dst, src, n);
}

void *memset(void *dst, s32 c, size_t n)
{
    return biosMemset(dst, c, n);
}

void *biosMemcpy(void *dst, const void *src, size_t n)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    size_t i;

    if (dst == NULL || src == NULL)
        return dst;
    for (i = 0; i < n; i++)
        d[i] = s[i];
    return dst;
}

void *biosMemmove(void *dst, const void *src, size_t n)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    size_t i;

    if (dst == NULL || src == NULL || d == s)
        return dst;
    if (d < s) {
        for (i = 0; i < n; i++)
            d[i] = s[i];
    } else {
        for (i = n; i > 0U; i--)
            d[i - 1U] = s[i - 1U];
    }
    return dst;
}

void *biosMemset(void *dst, s32 c, size_t n)
{
    u8 *d = (u8 *)dst;
    size_t i;

    if (dst == NULL)
        return NULL;
    for (i = 0; i < n; i++)
        d[i] = (u8)c;
    return dst;
}

s32 biosMemcmp(const void *lhs, const void *rhs, size_t n)
{
    const u8 *a = (const u8 *)lhs;
    const u8 *b = (const u8 *)rhs;
    size_t i;

    if (lhs == NULL || rhs == NULL)
        return 0;
    for (i = 0; i < n; i++) {
        if (a[i] != b[i])
            return (s32)a[i] - (s32)b[i];
    }
    return 0;
}

void *biosMemchr(const void *buf, s32 c, size_t n)
{
    const u8 *p = (const u8 *)buf;
    size_t i;

    if (buf == NULL)
        return NULL;
    for (i = 0; i < n; i++) {
        if (p[i] == (u8)c)
            return (void *)&p[i];
    }
    return NULL;
}

void biosBcopy(const void *src, void *dst, size_t n)
{
    biosMemmove(dst, src, n);
}

void biosBzero(void *dst, size_t n)
{
    biosMemset(dst, 0, n);
}

s32 biosBcmp(const void *lhs, const void *rhs, size_t n)
{
    return biosMemcmp(lhs, rhs, n);
}

char *biosStrcpy(char *dst, const char *src)
{
    char *out = dst;

    if (dst == NULL || src == NULL)
        return dst;
    while ((*dst++ = *src++) != '\0')
        ;
    return out;
}

char *biosStrncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;

    if (dst == NULL || src == NULL)
        return dst;
    while (i < n && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    while (i < n)
        dst[i++] = '\0';
    return dst;
}

char *biosStrcat(char *dst, const char *src)
{
    char *end = dst;

    if (dst == NULL || src == NULL)
        return dst;
    while (*end != '\0')
        end++;
    while ((*end++ = *src++) != '\0')
        ;
    return dst;
}

char *biosStrncat(char *dst, const char *src, size_t n)
{
    char *end = dst;
    size_t i = 0;

    if (dst == NULL || src == NULL)
        return dst;
    while (*end != '\0')
        end++;
    while (i < n && src[i] != '\0') {
        *end++ = src[i];
        i++;
    }
    *end = '\0';
    return dst;
}

s32 biosStrcmp(const char *lhs, const char *rhs)
{
    if (lhs == NULL || rhs == NULL)
        return (lhs == rhs) ? 0 : (lhs == NULL ? -1 : 1);
    while (*lhs != '\0' && *lhs == *rhs) {
        lhs++;
        rhs++;
    }
    return (s32)(u8)*lhs - (s32)(u8)*rhs;
}

s32 biosStrncmp(const char *lhs, const char *rhs, size_t n)
{
    size_t i;

    if (lhs == NULL || rhs == NULL)
        return (lhs == rhs) ? 0 : (lhs == NULL ? -1 : 1);
    for (i = 0; i < n; i++) {
        if (lhs[i] != rhs[i])
            return (s32)(u8)lhs[i] - (s32)(u8)rhs[i];
        if (lhs[i] == '\0')
            return 0;
    }
    return 0;
}

size_t biosStrlen(const char *str)
{
    size_t len = 0;

    if (str == NULL)
        return 0;
    while (str[len] != '\0')
        len++;
    return len;
}

char *biosStrchr(const char *str, s32 ch)
{
    if (str == NULL)
        return NULL;
    for (;;) {
        if (*str == (char)ch)
            return (char *)str;
        if (*str == '\0')
            return NULL;
        str++;
    }
}

char *biosStrrchr(const char *str, s32 ch)
{
    const char *last = NULL;

    if (str == NULL)
        return NULL;
    for (;;) {
        if (*str == (char)ch)
            last = str;
        if (*str == '\0')
            break;
        str++;
    }
    return (char *)last;
}

char *biosStrpbrk(const char *str, const char *accept)
{
    if (str == NULL || accept == NULL)
        return NULL;
    while (*str != '\0') {
        if (biosStrchr(accept, *str) != NULL)
            return (char *)str;
        str++;
    }
    return NULL;
}

size_t biosStrspn(const char *str, const char *accept)
{
    size_t count = 0;

    if (str == NULL || accept == NULL)
        return 0;
    while (str[count] != '\0' && biosStrchr(accept, str[count]) != NULL)
        count++;
    return count;
}

size_t biosStrcspn(const char *str, const char *reject)
{
    size_t count = 0;

    if (str == NULL || reject == NULL)
        return 0;
    while (str[count] != '\0' && biosStrchr(reject, str[count]) == NULL)
        count++;
    return count;
}

static char *g_strtokCursor;

char *biosStrtok(char *str, const char *delim)
{
    char *start;
    char *end;

    if (str != NULL)
        g_strtokCursor = str;
    if (g_strtokCursor == NULL)
        return NULL;

    start = g_strtokCursor + biosStrspn(g_strtokCursor, delim);
    if (*start == '\0') {
        g_strtokCursor = NULL;
        return NULL;
    }
    end = start + biosStrcspn(start, delim);
    if (*end != '\0') {
        *end = '\0';
        g_strtokCursor = end + 1;
    } else {
        g_strtokCursor = NULL;
    }
    return start;
}

char *biosStrstr(const char *haystack, const char *needle)
{
    size_t needleLen;

    if (haystack == NULL || needle == NULL)
        return NULL;
    needleLen = biosStrlen(needle);
    if (needleLen == 0U)
        return (char *)haystack;
    while (*haystack != '\0') {
        if (biosStrncmp(haystack, needle, needleLen) == 0)
            return (char *)haystack;
        haystack++;
    }
    return NULL;
}

s32 biosToupper(s32 ch)
{
    if (ch >= 'a' && ch <= 'z')
        return ch - ('a' - 'A');
    return ch;
}

s32 biosTolower(s32 ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return ch + ('a' - 'A');
    return ch;
}

s32 biosStrtol(const char *str, char **endPtr, s32 base)
{
    const char *p = str;
    s32 neg = 0;
    u32 acc = 0;

    if (p == NULL)
        return 0;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    if (*p == '-') {
        neg = 1;
        p++;
    } else if (*p == '+') {
        p++;
    }
    if (base == 0) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            base = 16;
            p += 2;
        } else if (p[0] == '0') {
            base = 8;
        } else {
            base = 10;
        }
    } else if (base == 16 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }

    while (*p != '\0') {
        s32 d;
        if (*p >= '0' && *p <= '9')
            d = *p - '0';
        else if (*p >= 'a' && *p <= 'z')
            d = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'Z')
            d = *p - 'A' + 10;
        else
            break;
        if (d >= base)
            break;
        acc = acc * (u32)base + (u32)d;
        p++;
    }
    if (endPtr != NULL)
        *endPtr = (char *)p;
    return neg ? -(s32)acc : (s32)acc;
}

u32 biosStrtoul(const char *str, char **endPtr, s32 base)
{
    return (u32)biosStrtol(str, endPtr, base);
}

s32 biosAtoi(const char *str)
{
    return biosStrtol(str, NULL, 10);
}

s32 biosAbs(s32 val)
{
    return (val < 0) ? -val : val;
}

s32 biosRand(void)
{
    PSX_BIOS->randSeed = PSX_BIOS->randSeed * 1103515245U + 12345U;
    return (s32)((PSX_BIOS->randSeed >> 16) & 0x7FFFU);
}

void biosSrand(u32 seed)
{
    PSX_BIOS->randSeed = seed;
}

static void swapBytes(u8 *a, u8 *b, size_t size)
{
    size_t i;

    for (i = 0; i < size; i++) {
        u8 t = a[i];
        a[i] = b[i];
        b[i] = t;
    }
}

void biosQsort(void *base, size_t count, size_t elemSize, s32 (*cmp)(const void *, const void *))
{
    u8 *arr = (u8 *)base;
    size_t i;
    size_t j;

    if (arr == NULL || cmp == NULL || count < 2U || elemSize == 0U)
        return;

    /* Simple in-place shell sort (zero recursion stack risk in kernel RAM) */
    for (i = 1U; i < count; i++) {
        j = i;
        while (j > 0U && cmp(arr + (j - 1U) * elemSize, arr + j * elemSize) > 0) {
            swapBytes(arr + (j - 1U) * elemSize, arr + j * elemSize, elemSize);
            j--;
        }
    }
}

void *biosBsearch(const void *key, const void *base, size_t count, size_t elemSize,
                  s32 (*cmp)(const void *, const void *))
{
    const u8 *arr = (const u8 *)base;
    size_t lo = 0;
    size_t hi = count;

    if (key == NULL || arr == NULL || cmp == NULL || elemSize == 0U)
        return NULL;

    while (lo < hi) {
        size_t mid = lo + ((hi - lo) >> 1);
        const void *elem = arr + mid * elemSize;
        s32 c = cmp(key, elem);
        if (c == 0)
            return (void *)elem;
        if (c < 0)
            hi = mid;
        else
            lo = mid + 1U;
    }
    return NULL;
}

/* Simple linked-block heap allocator over PSX_BIOS->heapBase..heapSize */
typedef struct BiosHeapBlock {
    u32 size;
    u32 isFree;
    struct BiosHeapBlock *next;
} BiosHeapBlock;

static BiosHeapBlock *heapHead(void)
{
    BiosHeapBlock *head;

    if (PSX_BIOS->heapBase == 0U || PSX_BIOS->heapSize < sizeof(BiosHeapBlock) + 16U)
        return NULL;
    head = (BiosHeapBlock *)PSX_BIOS->heapBase;
    if (head->size == 0U) {
        head->size = PSX_BIOS->heapSize - sizeof(BiosHeapBlock);
        head->isFree = 1U;
        head->next = NULL;
    }
    return head;
}

void *biosMalloc(size_t size)
{
    BiosHeapBlock *cur = heapHead();
    size_t aligned = (size + 7U) & ~7U;

    if (aligned == 0U)
        aligned = 8U;

    while (cur != NULL) {
        if (cur->isFree && cur->size >= aligned) {
            if (cur->size >= aligned + sizeof(BiosHeapBlock) + 8U) {
                BiosHeapBlock *split = (BiosHeapBlock *)((u8 *)(cur + 1) + aligned);
                split->size = cur->size - aligned - sizeof(BiosHeapBlock);
                split->isFree = 1U;
                split->next = cur->next;
                cur->size = aligned;
                cur->next = split;
            }
            cur->isFree = 0U;
            return (void *)(cur + 1);
        }
        cur = cur->next;
    }
    return NULL;
}

void biosFree(void *ptr)
{
    BiosHeapBlock *blk;
    BiosHeapBlock *cur;

    if (ptr == NULL)
        return;
    blk = ((BiosHeapBlock *)ptr) - 1;
    blk->isFree = 1U;

    cur = heapHead();
    while (cur != NULL && cur->next != NULL) {
        if (cur->isFree && cur->next->isFree) {
            cur->size += sizeof(BiosHeapBlock) + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void *biosCalloc(size_t count, size_t size)
{
    size_t total = count * size;
    void *ptr = biosMalloc(total);

    if (ptr != NULL)
        biosMemset(ptr, 0, total);
    return ptr;
}

void *biosRealloc(void *ptr, size_t size)
{
    BiosHeapBlock *oldBlk;
    void *nextPtr;

    if (ptr == NULL)
        return biosMalloc(size);
    if (size == 0U) {
        biosFree(ptr);
        return NULL;
    }
    oldBlk = ((BiosHeapBlock *)ptr) - 1;
    if (oldBlk->size >= size)
        return ptr;
    nextPtr = biosMalloc(size);
    if (nextPtr != NULL) {
        biosMemcpy(nextPtr, ptr, oldBlk->size);
        biosFree(ptr);
    }
    return nextPtr;
}

/* Minimal printf / sprintf formatter */
static void emitChar(char **dst, char ch, s32 *count)
{
    if (dst != NULL && *dst != NULL) {
        **dst = ch;
        (*dst)++;
    } else {
        biosPutchar((u8)ch);
    }
    (*count)++;
}

static void emitUnsigned(char **dst, u32 val, u32 base, s32 upper, s32 width, char padCh, s32 *count)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char buf[16];
    s32 len = 0;

    if (val == 0U) {
        buf[len++] = '0';
    } else {
        while (val != 0U) {
            buf[len++] = digits[val % base];
            val /= base;
        }
    }
    while (len < width)
        buf[len++] = padCh;
    while (len > 0)
        emitChar(dst, buf[--len], count);
}

static s32 formatV(char *out, const char *fmt, va_list ap)
{
    char *cursor = out;
    s32 written = 0;

    if (fmt == NULL)
        return 0;

    while (*fmt != '\0') {
        if (*fmt != '%') {
            emitChar(out ? &cursor : NULL, *fmt++, &written);
            continue;
        }
        fmt++;
        char padCh = ' ';
        s32 width = 0;
        if (*fmt == '0') {
            padCh = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        if (*fmt == 'l')
            fmt++;
        switch (*fmt) {
        case 'd':
        case 'i': {
            s32 v = va_arg(ap, s32);
            if (v < 0) {
                emitChar(out ? &cursor : NULL, '-', &written);
                emitUnsigned(out ? &cursor : NULL, (u32)(-v), 10U, 0, width > 0 ? width - 1 : 0, padCh, &written);
            } else {
                emitUnsigned(out ? &cursor : NULL, (u32)v, 10U, 0, width, padCh, &written);
            }
            break;
        }
        case 'u':
            emitUnsigned(out ? &cursor : NULL, va_arg(ap, u32), 10U, 0, width, padCh, &written);
            break;
        case 'x':
            emitUnsigned(out ? &cursor : NULL, va_arg(ap, u32), 16U, 0, width, padCh, &written);
            break;
        case 'X':
        case 'p':
            emitUnsigned(out ? &cursor : NULL, va_arg(ap, u32), 16U, 1, width, padCh, &written);
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (s == NULL)
                s = "(null)";
            while (*s != '\0')
                emitChar(out ? &cursor : NULL, *s++, &written);
            break;
        }
        case 'c':
            emitChar(out ? &cursor : NULL, (char)va_arg(ap, s32), &written);
            break;
        case '%':
            emitChar(out ? &cursor : NULL, '%', &written);
            break;
        default:
            break;
        }
        if (*fmt != '\0')
            fmt++;
    }
    if (out != NULL)
        *cursor = '\0';
    return written;
}

s32 biosPrintf(const char *fmt, ...)
{
    va_list ap;
    s32 ret;

    va_start(ap, fmt);
    ret = formatV(NULL, fmt, ap);
    va_end(ap);
    return ret;
}

s32 biosSprintf(char *out, const char *fmt, ...)
{
    va_list ap;
    s32 ret;

    va_start(ap, fmt);
    ret = formatV(out, fmt, ap);
    va_end(ap);
    return ret;
}

void biosPutchar(s32 ch)
{
    u8 byteVal = (u8)ch;
    biosFileWrite(1, &byteVal, 1U);
}

void biosPuts(const char *str)
{
    if (str == NULL)
        return;
    while (*str != '\0')
        biosPutchar((u8)*str++);
    biosPutchar('\n');
}

s32 biosGetchar(void)
{
    return biosFileGetc(0);
}

char *biosGets(char *dst)
{
    char *cur = dst;
    s32 ch;

    if (dst == NULL)
        return NULL;
    while ((ch = biosGetchar()) >= 0 && ch != '\n')
        *cur++ = (char)ch;
    *cur = '\0';
    return dst;
}

/* Shift-JIS -> ROM 16x16 bitmap glyph offset (B0:5C / A0:7E Krom2RawAdd) */
s32 biosKrom2Offset(u32 sjis)
{
    u32 hi = (sjis >> 8) & 0xFFU;
    u32 lo = sjis & 0xFFU;
    u32 row;
    u32 col;

    if (hi < 0x81U || hi > 0x9FU || lo < 0x40U || lo > 0xFCU || lo == 0x7FU)
        return -1;
    row = (hi - 0x81U) * 2U;
    if (lo >= 0x9FU) {
        row++;
        col = lo - 0x9FU;
    } else {
        col = lo - 0x40U - ((lo > 0x7FU) ? 1U : 0U);
    }
    return (s32)((row * 94U + col) * 30U);
}

u32 biosKrom2RawAdd(u32 sjis)
{
    s32 off = biosKrom2Offset(sjis);

    if (off < 0)
        return 0xFFFFFFFFU;
    /* Wrap cleanly within our built-in ROM font table */
    return PSX_ROM_FONT_ADDR + ((u32)off % 0x10000U);
}

u32 biosGetSystemInfo(u32 index)
{
    switch (index) {
    case 0U:
        return 0x20000525U; /* BCD date: 2000-05-25 (System ROM 4.5) */
    case 1U:
        return 0x00000000U; /* Console type */
    case 2U:
        return PSX_ROM_VER_ADDR;
    default:
        return 0U;
    }
}

u32 *biosGetA0Table(void)
{
    return PSX_RAM_A0_TABLE;
}

u32 *biosGetB0Table(void)
{
    return PSX_RAM_B0_TABLE;
}

u32 *biosGetC0Table(void)
{
    return PSX_RAM_C0_TABLE;
}
