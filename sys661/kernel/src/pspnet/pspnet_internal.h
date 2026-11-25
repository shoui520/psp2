#ifndef PSPNET_INTERNAL_H
#define PSPNET_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

typedef int SceUID;
typedef unsigned int SceSize;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

/* Error codes */
#define SCE_NET_ERROR_ENOMEM        ((int)0x80410001)
#define SCE_NET_ERROR_ENOENT        ((int)0x80410104)
#define SCE_NET_ERROR_EBUSY         ((int)0x80410105)
#define SCE_NET_ERROR_EINVAL        ((int)0x80410103)
#define SCE_NET_ERROR_SCAN_NONE     ((int)0x80410106)
#define SCE_NET_ERROR_JOIN_FAIL     ((int)0x80410107)
#define SCE_NET_ERROR_NO_IF         ((int)0x80410180)
#define SCE_NET_ERROR_IF_BUSY       ((int)0x80410181)
#define SCE_NET_ERROR_IOCTL_ERR     ((int)0x80410182)
#define SCE_NET_ERROR_IOCTL_CANCELED ((int)0x80410183)
#define SCE_NET_ERROR_IOCTL_ENOENT  ((int)0x80410184)
#define SCE_NET_ERROR_NO_BSS        ((int)0x80410185)
#define SCE_NET_ERROR_STACK_SMALL   ((int)0x80410005)
#define SCE_NET_ERROR_ADHOC_CANCEL  ((int)0x80410D11)

/* Kernel_Library imports */
extern int sceKernelCpuSuspendIntr(void);
extern void sceKernelCpuResumeIntr(int intr);
extern void *sceKernelMemset(void *dst, int c, SceSize len);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize len);
extern int sceKernelLockLwMutex(void *workarea, int count, u32 *timeout);
extern int sceKernelUnlockLwMutex(void *workarea, int count);
extern int _sceKernelCheckThreadStack(void);
extern SceUID sceKernelGetThreadId(void);

/* SysMemUserForUser imports */
extern int sceKernelQueryMemoryInfo(void *addr, SceUID *partitionId, int *attr);

/* ThreadManForUser imports */
extern int sceKernelDelayThread(u32 usec);
extern SceUID sceKernelCreateSema(const char *name, u32 attr, int initVal, int maxVal, void *opt);
extern int sceKernelDeleteSema(SceUID semaid);
extern int sceKernelSignalSema(SceUID semaid, int count);
extern int sceKernelWaitSema(SceUID semaid, int count, u32 *timeout);
extern int sceKernelPollSema(SceUID semaid, int count);
extern SceUID sceKernelCreateEventFlag(const char *name, u32 attr, u32 bits, void *opt);
extern int sceKernelDeleteEventFlag(SceUID evid);
extern int sceKernelSetEventFlag(SceUID evid, u32 bits);
extern int sceKernelWaitEventFlag(SceUID evid, u32 bits, u32 wait, u32 *outBits, u32 *timeout);
extern SceUID sceKernelCreateThread(const char *name, void *entry, int initPriority, int stackSize, u32 attr, void *opt);
extern int sceKernelStartThread(SceUID thid, SceSize arglen, void *argp);
extern int sceKernelTerminateThread(SceUID thid);
extern int sceKernelDeleteThread(SceUID thid);
extern SceUID sceKernelSetAlarm(u32 clock, void *handler, void *common);
extern int sceKernelCancelAlarm(SceUID alarmid);
extern int sceKernelCreateLwMutex(void *workarea, const char *name, u32 attr, int initCount, void *opt);
extern int sceKernelDeleteLwMutex(void *workarea);
extern int sceKernelGetSystemTime(void *time);
extern u32 sceKernelGetSystemTimeLow(void);
extern s64 sceKernelGetSystemTimeWide(void);
extern int sceKernelSysClock2USec(void *clock, u32 *low, u32 *high);

/* UtilsForUser imports */
extern int sceKernelUtilsSha1BlockInit(void *ctx);
extern int sceKernelUtilsSha1BlockUpdate(void *ctx, const void *data, u32 len);
extern int sceKernelUtilsSha1BlockResult(void *ctx, u8 *digest);

/* sceWlanDrv_lib imports */
extern int sceWlanDrv_lib_8D5F551B(int flags);

/* sceNetIfhandle_lib imports */
extern int sceNetIfhandle_lib_0296C7D6(void *ifp, int cmd, void *arg);
extern int sceNetIfhandle_lib_0C391E9F(SceUID partId, int poolSize);
extern void sceNetIfhandle_lib_0FB8AE0D(void);
extern void *sceNetIfhandle_lib_15CFE3C0(u32 size);
extern void sceNetIfhandle_lib_29ED84C5(void);
extern void sceNetIfhandle_lib_30F69334(SceUID evf);
extern int sceNetIfhandle_lib_4C2886CB(void *stat);
extern int sceNetIfhandle_lib_4FB43BCE(void **ifp, SceUID *sema);
extern void sceNetIfhandle_lib_76BAD213(void *ptr);
extern int sceNetIfhandle_lib_8FCB05A1(void *ifp);
extern int sceNetIfhandle_lib_9173FD47(void **ifp, SceUID *sema);
extern int sceNetIfhandle_lib_B1F5BB87(void *ifp, void *m);
extern int sceNetIfhandle_lib_B40A882F(void **ifp, SceUID *sema);
extern int sceNetIfhandle_lib_B8188F96(void **ifp, SceUID *sema);
extern int sceNetIfhandle_lib_B9096E48(void);
extern void sceNetIfhandle_lib_BFF3CEA5(void *m, int len);
extern void *sceNetIfhandle_lib_E440A7D8(void);
extern int sceNetIfhandle_lib_EAD3A759(void *ifp);
extern void sceNetIfhandle_lib_F56FAC82(void *m);

/* LwMutex workarea (32 bytes on PSP) */
typedef struct SceLwMutexWorkarea {
    u32 data[8];
} SceLwMutexWorkarea;

/* Thread opt param */
typedef struct SceKernelThreadOptParam {
    SceSize size;
    SceUID stackMpid;
} SceKernelThreadOptParam;

/* Callout structure (36 bytes) */
typedef struct NetCallout {
    struct NetCallout *ready_next;
    struct NetCallout **ready_prev;
    struct NetCallout *pend_next;
    struct NetCallout **pend_prev;
    void (*func)(void *);
    void *arg;
    u32 timeout_us;
    u32 flags;
    SceUID alarm_id;
} NetCallout;

/* Thread info structure (92 bytes) */
typedef struct NetThreadInfo {
    struct NetThreadInfo *list_next;   /* 0x00 */
    struct NetThreadInfo **list_prev;  /* 0x04 */
    SceUID thid;                       /* 0x08 */
    s16 refcount;                      /* 0x0C */
    u16 pad0e;                         /* 0x0E */
    int err;                           /* 0x10 */
    u32 reserved14;                    /* 0x14 */
    struct NetThreadInfo *slp_next;    /* 0x18 */
    struct NetThreadInfo **slp_prev;   /* 0x1C */
    u32 wchan;                         /* 0x20 */
    NetCallout callout;                /* 0x24..0x47 */
    u32 slp_flags;                     /* 0x48 */
    SceUID sema_id;                    /* 0x4C */
    u32 reserved50[3];                 /* 0x50..0x5B */
} NetThreadInfo;

/* uio / iovec structures for BPF */
typedef struct NetIovec {
    void *iov_base;
    u32 iov_len;
} NetIovec;

typedef struct NetUio {
    NetIovec *uio_iov;
    int uio_iovcnt;
    u32 uio_offset;
    u32 uio_resid;
    int uio_rw;
} NetUio;

/* Core exported functions used across files */
extern int sceNet_lib_94B44F26(void); /* splnet */
extern int sceNet_lib_515B2F33(int s); /* splx */
extern void sceNet_lib_7C3B86C5(int bit); /* schednetisr_nosig */
extern int sceNet_lib_05D525E4(int bit); /* schednetisr */
extern NetThreadInfo *sceNet_lib_CA3CF5EB(int *errp);
extern int sceNet_lib_757085B0(NetThreadInfo *ti, int err);
extern NetThreadInfo *sceNet_lib_BB2B3DDB(SceUID thid);
extern int sceNet_lib_A8B6205A(NetCallout *c, u32 usec, void (*fn)(void *), void *arg);
extern int sceNet_lib_A93A93E9(NetCallout *c);
extern void sceNet_lib_25CC373A(NetCallout *c);
extern void sceNet_lib_DCBC596E(NetCallout *c);
extern int sceNet_lib_7C86FBA4(NetCallout *c, u32 ticks, void (*fn)(void *), void *arg);
extern int sceNet_lib_F6DB0A0B(u32 wchan, u32 usec);
extern int sceNet_lib_205E8D17(u32 wchan, u32 ticks);
extern int sceNet_lib_7574FDA1(u32 wchan);
extern int sceNet_lib_B8C4A858(SceUID thid, u32 wchan);
extern int sceNet_lib_C9C97945(NetThreadInfo *ti);
extern u64 sceNet_lib_4B2B3416(u32 *tv);
extern int sceNet_lib_2B42872F(void *cp, int n, NetUio *uio);
extern void sceNet_lib_92633D8D(NetThreadInfo *ti, SceUID *sel_thid);
extern int sceNet_lib_B9C780C7(SceUID *sel_thid);
extern void *sceNet_lib_B68E1EEA(void);
extern int sceNet_lib_13A8B98A(void *so, u32 cmd, void *data, int wait);
extern void *sceNet_lib_1D10419C(const char *name);
extern int sceNet_lib_701DDDC3(void *ifp);
extern int sceNet_lib_D5A03BC0(void *ifp);
extern int sceNet_lib_EDB11CB4(void *ifp);
extern int sceNet_lib_8C55B410(void);
extern int sceNet_lib_EB6DE71A(int a1, int a2, int a3);
extern int sceNet_lib_1858883D(void);
extern int sceNet_lib_DA02F383(const char *ifname, u32 *out);
extern int sceNet_lib_83FE280A(const char *ifname, void *arg);
extern int sceNet_lib_891723D5(const char *ifname, void *arg);
extern int sceNet_lib_A55C914F(const char *ifname, void *arg);

/* Libc exports */
extern int sceNet_lib_C13C9307(int c); /* toupper */
extern int sceNet_lib_96EF9DA1(int c); /* tolower */
extern int sceNet_lib_750F705D(int c); /* ctype lookup */
extern int sceNet_lib_E0A81C7C(const void *s1, const void *s2, int n); /* memcmp */
extern void *sceNet_lib_4753D878(void *dst, const void *src, int n); /* memmove */
extern void *sceNet_lib_8E798A2F(const void *s, int c, int n); /* memchr */
extern int sceNet_lib_4BF83CF4(const void *s1, const void *s2, int n); /* bcmp */
extern void *sceNet_lib_2F305274(const void *src, void *dst, int n); /* bcopy */
extern void *sceNet_lib_8B615F52(void *dst, int n); /* bzero */
extern int sceNet_lib_CF705E46(char *buf, const char *fmt, ...); /* sprintf */
extern int sceNet_lib_8687B5AB(char *buf, const char *fmt, va_list ap); /* vsprintf */
extern int sceNet_lib_A0F16ABD(const char *s1, const char *s2); /* strcmp */
extern char *sceNet_lib_80C9F02A(char *dst, const char *src); /* strcpy */
extern int sceNet_lib_D8722983(const char *s); /* strlen */
extern int sceNet_lib_94DCA9F0(const char *s1, const char *s2, int n); /* strncmp */
extern char *sceNet_lib_B5CE388A(char *dst, const char *src, int n); /* strncpy */

/* BPF internal hooks called by pspnet.c */
extern void bpf_init(int n);
extern void bpf_attach(void **bp_ptr, void *ifp, u32 dlt, u32 hdrlen);
extern void bpf_detach(void *ifp);
extern void bpf_mtap(void *bp, void *m);

/* Select wait channel */
extern u32 g_selwait;

#endif /* PSPNET_INTERNAL_H */
