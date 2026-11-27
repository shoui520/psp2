#ifndef PSPNET_RESOLVER_INT_H
#define PSPNET_RESOLVER_INT_H

#include <common_imp.h>

typedef struct ResolverAsyncCtx {
    int type;           /* 0x00: 0 = NtoA, 1 = AtoN */
    u32 txid;           /* 0x04 */
    int retry_idx;      /* 0x08 */
    int dns_idx;        /* 0x0c */
    int state;          /* 0x10: 0=init, 1=sent, 2=waiting_timeout */
    u32 start_time;     /* 0x14 */
    int timeout;        /* 0x18 */
    int retry;          /* 0x1c */
    char *hostname;     /* 0x20 */
    u32 *addr_ptr;      /* 0x24 (or u8 addr[4] for AtoN) */
    char *out_name;     /* 0x28 */
    int out_len;        /* 0x2c */
} ResolverAsyncCtx;


typedef struct _sce_pspnet_resolver_ctx_list {
    int rid;            /* 0x00 */
    struct _sce_pspnet_resolver_ctx_list *next; /* 0x04 */
    u8 *buf;            /* 0x08 */
    u32 buflen;         /* 0x0c */
    int sock;           /* 0x10 */
    u32 flags;          /* 0x14 */
    ResolverAsyncCtx *async; /* 0x18 */
} _sce_pspnet_resolver_ctx_list;

extern int sceKernelCpuSuspendIntr(void);
extern int sceKernelCpuResumeIntr(int intr);
extern void *sceKernelMemset(void *dst, int c, SceSize n);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize n);
extern int sceKernelCheckThreadStack(void);
extern SceUID sceKernelGetThreadId(void);
extern int sceKernelGetSystemTime(void *time);
extern u32 sceKernelGetSystemTimeLow(void);

extern void *sceNetIfhandle_lib_15CFE3C0(SceSize size);
extern void sceNetIfhandle_lib_76BAD213(void *ptr);

extern int sceNetInetSocket(int domain, int type, int protocol);
extern int sceNetInetClose(int s);
extern int sceNetInetSetsockopt(int s, int level, int optname, const void *optval, int optlen);
extern int sceNetInetSendto(int s, const void *buf, int len, int flags, const void *to, int tolen);
extern int sceNetInetRecvfrom(int s, void *buf, int len, int flags, void *from, void *fromlen);
extern int sceNetInetGetErrno(void);
extern int sceNetInetSocketAbort(int s);
extern int sceNetInet_lib_AC9D90A5(u32 *dns1, u32 *dns2);

extern int sceNet_lib_750F705D(int c);
extern char *sceNet_lib_80C9F02A(char *dst, const char *src);
extern void *sceNet_lib_B5CE388A(void *dst, const void *src, SceSize n);
extern char *sceNet_lib_BCBE14CF(const char *s, int c);
extern SceSize sceNet_lib_D8722983(const char *s);


typedef struct _sce_pspnet_resolver_ctx_list ResolverNode;

extern int g_initialized;
extern int g_next_rid;
extern ResolverNode *g_resolver_list;

int _sce_pspnet_resolver_create_ctx(void *buf, SceSize buflen, int *rid);
int _sce_pspnet_resolver_delete_ctx(int rid);
int _sce_pspnet_resolver_init(void);
int _sce_pspnet_resolver_term(void);
ResolverNode *_sce_pspnet_resolver_get_resolver_ctx(int rid, ResolverNode **prev_out);
int _sce_pspnet_resolver_strncasecmp(const char *lhs, const char *rhs, SceSize n);

#endif /* PSPNET_RESOLVER_INT_H */
