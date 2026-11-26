#ifndef APCTL_INTERNAL_H
#define APCTL_INTERNAL_H

#include <stdint.h>
#include <stddef.h>

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

static inline u16 bswap16(u16 x) { return (u16)__builtin_allegrex_wsbh(x); }
static inline u32 bswap32(u32 x) { return (u32)__builtin_allegrex_wsbw(x); }

/* Kernel_Library */
extern void *sceKernelMemset(void *dst, int c, SceSize len);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize len);
extern int sceKernelLockLwMutex(void *workarea, int count, u32 *timeout);
extern int sceKernelUnlockLwMutex(void *workarea, int count);
extern int _sceKernelCheckThreadStack(void);

/* ModuleMgrForUser */
extern int sceKernelGetModuleGPByAddress(void *addr, u32 *gp);

/* SysMemUserForUser */
extern int sceKernelQueryMemoryInfo(void *addr, SceUID *partId, int *attr);

/* ThreadManForUser */
extern int sceKernelDelayThread(u32 usec);
extern SceUID sceKernelCreateSema(const char *name, u32 attr, int initVal, int maxVal, void *opt);
extern int sceKernelDeleteSema(SceUID semaid);
extern int sceKernelSignalSema(SceUID semaid, int count);
extern int sceKernelWaitSema(SceUID semaid, int count, u32 *timeout);
extern int sceKernelPollSema(SceUID semaid, int count);
extern SceUID sceKernelCreateEventFlag(const char *name, u32 attr, u32 bits, void *opt);
extern int sceKernelDeleteEventFlag(SceUID evid);
extern int sceKernelSetEventFlag(SceUID evid, u32 bits);
extern int sceKernelClearEventFlag(SceUID evid, u32 bits);
extern int sceKernelWaitEventFlag(SceUID evid, u32 bits, u32 wait, u32 *outBits, u32 *timeout);
extern int sceKernelPollEventFlag(SceUID evid, u32 bits, u32 wait, u32 *outBits);
extern SceUID sceKernelCreateThread(const char *name, void *entry, int prio, int stackSize, u32 attr, void *opt);
extern int sceKernelStartThread(SceUID thid, SceSize arglen, void *argp);
extern int sceKernelWaitThreadEnd(SceUID thid, u32 *timeout);
extern int sceKernelTerminateThread(SceUID thid);
extern int sceKernelDeleteThread(SceUID thid);
extern int sceKernelExitThread(int status);
extern SceUID sceKernelSetAlarm(u32 clock, void *handler, void *common);
extern int sceKernelCancelAlarm(SceUID alarmid);
extern int sceKernelCreateLwMutex(void *workarea, const char *name, u32 attr, int initCount, void *opt);
extern int sceKernelDeleteLwMutex(void *workarea);
extern int sceKernelGetSystemTime(void *time);

/* UtilsForUser */
extern int sceKernelUtilsSha1BlockInit(void *ctx);
extern int sceKernelUtilsSha1BlockUpdate(void *ctx, const void *data, u32 len);
extern int sceKernelUtilsSha1BlockResult(void *ctx, u8 *digest);
extern int sceKernelUtilsMd5BlockInit(void *ctx);
extern int sceKernelUtilsMd5BlockUpdate(void *ctx, const void *data, u32 len);
extern int sceKernelUtilsMd5BlockResult(void *ctx, u8 *digest);

/* sceWlanDrv / sceWlanDrv_lib */
extern int sceWlanGetSwitchState(void);
extern int sceWlanDevAttach(void);
extern int sceWlanDevDetach(void);
extern int sceWlanDrv_lib_80B7162E(void);
extern int sceWlanDrv_lib_CAC1FB0D(void *info);

/* sceUtility_netparam_internal */
extern int sceUtility_netparam_internal_239F260D(int param, void *val);
extern int sceUtility_netparam_internal_4CB183A4(int from, int to);
extern int sceUtility_netparam_internal_61D0686E(int id);
extern int sceUtility_netparam_internal_6B1BC62C(int id);

/* sceNet / sceNet_lib */
extern int sceNetGetLocalEtherAddr(void *mac);
extern int sceNetFreeThreadinfo(SceUID thid);
extern int sceNet_lib_1858883D(void);
extern int sceNet_lib_3B617AA0(void);
extern void *sceNet_lib_4753D878(void *dst, const void *src, int n);
extern int sceNet_lib_5216CBF5(const char *ifname);
extern u32 sceNet_lib_75D9985C(void);
extern int sceNet_lib_7BA3ED91(const char *ifname, void *req, u32 *len, void *buf, u32 *status);
extern char *sceNet_lib_80C9F02A(char *dst, const char *src);
extern int sceNet_lib_8D33C11D(const char *ifname, void *mac);
extern int sceNet_lib_8DA79EB0(const char *ifname, const void *key);
extern int sceNet_lib_AB375964(const char *ifname, const void *key);
extern int sceNet_lib_AB7DD9A5(const char *ifname, SceUID evf, u32 bits);
extern char *sceNet_lib_B5CE388A(char *dst, const char *src, int n);
extern int sceNet_lib_B6FC0A5B(int bpf_fd, void *buf, u32 len);
extern int sceNet_lib_B9085A96(const char *lhs, const char *rhs, int n);
extern int sceNet_lib_C431A214(int bpf_fd, const void *buf, u32 len);
extern int sceNet_lib_D1BE2CE9(const char *ifname, u32 *ev, u32 *arg);
extern int sceNet_lib_D2422E4D(const char *ifname);
extern int sceNet_lib_D8722983(const char *s);
extern int sceNet_lib_DB88F458(int bpf_fd);
extern int sceNet_lib_E0A81C7C(const void *lhs, const void *rhs, int n);
extern int sceNet_lib_E1F4696F(int bpf_fd, u32 cmd, void *arg);
extern int sceNet_lib_F355C73B(const char *ifname, int type, void *out);

/* sceNetInet / sceNetInet_lib */
extern int sceNetInetSocket(int domain, int type, int proto);
extern int sceNetInetBind(int s, const void *addr, int len);
extern int sceNetInetSetsockopt(int s, int level, int optname, const void *optval, int optlen);
extern int sceNetInetSendmsg(int s, const void *msg, int flags);
extern int sceNetInetClose(int s);
extern u32 sceNetInetInetAddr(const char *cp);
extern int sceNetInetInetAton(const char *cp, void *inp);
extern const char *sceNetInetInetNtop(int af, const void *src, char *dst, int size);
extern int sceNetInetInetPton(int af, const char *src, void *dst);
extern int sceNetInet_lib_0238B6DF(void *ctx, int bits1, int bits2, const void *key);
extern int sceNetInet_lib_4C13BE10(void);
extern int sceNetInet_lib_4F68DB0E(const char *ifname, void *ip, void *brd, void *mask);
extern int sceNetInet_lib_5155EC8A(const char *ifname, void *ip, void *brd, void *mask);
extern int sceNetInet_lib_59561561(const void *mac, const void *ip);
extern int sceNetInet_lib_6A046357(const char *ifname);
extern int sceNetInet_lib_7776A492(const void *gw);
extern int sceNetInet_lib_7CB1D9E3(const char *ifname);
extern int sceNetInet_lib_A94A75E7(const void *ip);
extern int sceNetInet_lib_AAF4895A(const void *dns1, const void *dns2);
extern int sceNetInet_lib_AEE60F84(int s, u32 cmd, void *arg);
extern int sceNetInet_lib_D609AD36(void *ctx, const void *in, void *out);
extern int sceNetInet_lib_DC38FEE9(const void *mac, const void *ip);
extern int sceNetInet_lib_E6F67D14(void);

/* sceBtdun_lib */
extern int sceBtdun_lib_412DA4A1(void);
extern int sceBtdun_lib_523FA5A5(void);
extern int sceBtdun_lib_5BED7B20(const void *buf, int len, u32 timeout);
extern int sceBtdun_lib_5C91E089(void);
extern int sceBtdun_lib_966A10F5(void *buf, int *len, u32 timeout);
extern int sceBtdun_lib_BC01BAC1(void);
extern int sceBtdun_lib_DDA6D0A5(int id);

/* BPF instruction & program */
typedef struct BpfInsn {
    u16 code;
    u8 jt;
    u8 jf;
    u32 k;
} BpfInsn;

typedef struct BpfProgram {
    u32 len;
    BpfInsn *insns;
} BpfProgram;

typedef struct BpfHdr {
    u32 tv_sec;
    u32 tv_usec;
    u32 caplen;
    u32 datalen;
    u16 hdrlen;
    u16 pad;
} BpfHdr;

/* Cross-file hooks between apctl_main and protocol clients */
extern int dhcp_start(const char *ifname, void (*cb)(int, int), int stack, int prio, int timeout, void *cfg);
extern int dhcp_stop(void);
extern int pppoe_start(const char *ifname, void (*cb)(int, int), int stack, int prio, int unused, void *cfg);
extern int pppoe_stop_signal(void);
extern int pppoe_stop_wait(void);
extern int eap8021x_start(const char *ifname, void (*cb)(int, int), int stack, int prio, int unused, void *cfg);
extern int eap8021x_stop(void);
extern int eap8021x_set_eap_flag(int val);
extern int wpa_set_config(void *cfg);
extern int wpa_start(const char *ifname, void (*cb)(void *, int), int stack, int prio, int timeout, void *bss);
extern int wpa_stop(void);
extern int wpa_notify_mic_err_unicast(void);
extern int wpa_notify_mic_err_multicast(void);
extern int btdun_start(void (*cb)(int, int), SceUID partId, int stack, int prio, void *cfg);
extern int btdun_stop(void);

/* Shared EAPOL BPF helpers (in apctl_8021x.c, used by apctl_wpa.c) */
extern int eapol_bpf_open(const char *ifname, BpfProgram *prog, const u8 *mac, u8 type_filter, int bufsize, u32 timeout_us);
extern int eapol_bpf_send(int bpf_fd, u8 *frame, const u8 *src_mac, const u8 *dst_mac, int write_hdr, u8 ver, u8 type, u16 len);
extern int eapol_bpf_parse(int bpf_fd, u8 *pkt, u32 pkt_len, const u8 *sta_mac, int check_ver, u8 *out_type, u8 **out_body, u8 **out_next, int *out_valid);
extern int eapol_bpf_close(int bpf_fd);
extern int eapol_bpf_read(int bpf_fd, void *buf, u32 len);
extern int eapol_bpf_recv_wpa(int bpf_fd, u8 *buf, u32 bufsize, const u8 *mac, u8 **out_pkt, int *out_len);
extern void apctl_hmac_sha1(const u8 *data, int data_len, u8 *key, u32 key_len, u8 *out);
extern void apctl_hmac_md5(const u8 *data, int data_len, u8 *key, u32 key_len, u8 *out);

/* RC4 (in apctl_wpa.c) */
typedef struct Rc4Key {
    u32 x;
    u32 y;
    u32 s[256];
} Rc4Key;

extern void rc4_set_key(Rc4Key *key, int len, const u8 *data);
extern void rc4_crypt(Rc4Key *key, u32 len, const u8 *in, u8 *out);
extern const char *rc4_options(void);

#endif /* APCTL_INTERNAL_H */
