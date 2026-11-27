#ifndef URI_IN_H
#define URI_IN_H

#include <common_imp.h>
#include <stdarg.h>

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
extern int sceKernelCheckThreadStack(void);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 size);


#endif /* URI_IN_H */
