#ifndef PSPNET_ADHOCCTL_INTERNAL_H
#define PSPNET_ADHOCCTL_INTERNAL_H

#include <psptypes.h>

extern void *sceKernelMemset(void *dst, int val, u32 len);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 len);
extern int sceKernelLockLwMutex(void *workarea, int count, u32 *timeout);
extern int sceKernelUnlockLwMutex(void *workarea, int count);
extern int sceKernelCheckThreadStack(void);
extern int sceKernelGetModuleGPByAddress(void *addr, void *gp_out);
extern void KprintfForUser(const char *fmt, ...);
extern int sceKernelGetCompiledSdkVersion(void);

extern int sceKernelDelayThread(u32 delay);
extern int sceKernelSetEventFlag(int evid, u32 bits);
extern int sceKernelDeleteEventFlag(int evid);
extern int sceKernelStartThread(int thid, u32 arglen, void *argp);
extern int sceKernelWaitThreadEnd(int thid, u32 *timeout);
extern int sceKernelPollEventFlag(int evid, u32 bits, int wait, u32 *outBits);
extern int sceKernelWaitEventFlag(int evid, u32 bits, int wait, u32 *outBits, u32 *timeout);
extern int sceKernelCreateThread(const char *name, void *entry, int initPriority, int stackSize, u32 attr, void *option);
extern int sceKernelCreateEventFlag(const char *name, u32 attr, u32 bits, void *opt);
extern int sceKernelDeleteLwMutex(void *workarea);
extern int sceKernelTerminateThread(int thid);
extern int sceKernelClearEventFlag(int evid, u32 bits);
extern s64 sceKernelGetSystemTimeWide(void);
extern int sceKernelDeleteThread(int thid);
extern int sceKernelCreateLwMutex(void *workarea, const char *name, u32 attr, int initCount, void *opt);

extern int sceNetGetLocalEtherAddr(u8 *addr);
extern int sceNetFreeThreadinfo(int thid);

extern int sceNetAdhocPtpClose(int id, int flag);
extern int sceNetAdhocPtpSend(int id, const void *data, int *len, u32 timeout, int flag);
extern int sceNetAdhocPtpOpen(const u8 *srcmac, u16 srcport, const u8 *dstmac, u16 dstport, u32 bufsize, u32 delay, int count, int flag);
extern int sceNetAdhocPtpRecv(int id, void *data, int *len, u32 timeout, int flag);
extern int sceNetAdhocPtpFlush(int id, u32 timeout, int flag);
extern int sceNetAdhocPtpAccept(int id, u8 *mac, u16 *port, u32 timeout, int flag);
extern int sceNetAdhocPtpListen(const u8 *srcmac, u16 srcport, u32 bufsize, u32 delay, int count, int queue, int flag);
extern int sceNetAdhocPtpConnect(int id, u32 timeout, int flag);

extern int sceNetAdhocAuth_lib_1F9A90B8(const char *name, int *buflen, void *buf);
extern int sceNetAdhocAuth_lib_2E6AA271(void);
extern int sceNetAdhocAuth_lib_312BD812(void *buf, int len);
extern int sceNetAdhocAuth_lib_6074D8F1(void);
extern int sceNetAdhocAuth_lib_6CE209A3(void);
extern int sceNetAdhocAuth_lib_72AAC6D3(int len, const void *key);
extern int sceNetAdhocAuth_lib_76F26AB0(const u8 *mac, char *name);
extern int sceNetAdhocAuth_lib_86004235(void);
extern int sceNetAdhocAuth_lib_89F2A732(const char *ifname, int prio, int stack, void *opt, int max_peers, const char *name);
extern int sceNetAdhocAuth_lib_BD144DA6(int *buflen, void *buf);
extern int sceNetAdhocAuth_lib_CF4D9BED(const u8 *mac, int size, void *peer_info);

extern int sceNet_lib_03164B12(const char *ifname, void *param, void *err);
extern int sceNet_lib_0D633F53(const char *ifname, void *param, const u8 *bssid, void *err);
extern int sceNet_lib_389728AB(const char *ifname, void *param, void *err);
extern int sceNet_lib_5216CBF5(const char *ifname);
extern int sceNet_lib_750F705D(int ch);
extern int sceNet_lib_7BA3ED91(const char *ifname, void *scan_req, int *buflen, void *buf, void *err);
extern int sceNet_lib_A55C914F(const char *ifname, void *buf);
extern int sceNet_lib_AB7DD9A5(const char *ifname, int evfid, int bit);
extern int sceNet_lib_AFA11338(const char *ifname, void *param);
extern int sceNet_lib_B20F84F8(const char *ifname, void *param);
extern int sceNet_lib_B5CE388A(char *dst, const char *src, u32 len);
extern int sceNet_lib_D1BE2CE9(const char *ifname, int *ev, int *reason);
extern int sceNet_lib_D2422E4D(const char *ifname);
extern int sceNet_lib_D5B64E37(const char *ifname, const char *ssid, int ssid_len);
extern int sceNet_lib_D8722983(const char *str);
extern int sceNet_lib_DA02F383(const char *ifname, void *err);
extern int sceNet_lib_E0A81C7C(const void *a, const void *b, u32 len);

extern int scePowerRequestSuspend(void);

extern int sceRegCloseCategory(int hd);
extern int sceRegOpenCategory(int hd, const char *name, int mode, int *cat_hd);
extern int sceRegGetKeyValue(int hd, int key_hd, void *buf, u32 size);
extern int sceRegOpenRegistry(void *reg, int mode, int *hd);
extern int sceRegGetKeyInfo(int hd, const char *name, int *key_hd, int *type, u32 *size);
extern int sceRegCloseRegistry(int hd);

extern int sceUtilityGetSystemParamString(int id, char *str, int len);
extern int sceUtilityGetSystemParamInt(int id, int *val);

extern int sceWlanGetSwitchState(void);

extern int sceWlanDrv_lib_2D0FAE4E(void *param);
extern int sceWlanDevAttach(void);
extern int sceWlanDrv_lib_56F467CA(void *param);
extern int sceWlanDrv_lib_5BAA1FE5(int flag);
extern int sceWlanSetHostDiscover(int flag, void *param);
extern int sceWlanDrv_lib_768E275B(void);
extern int sceWlanDrv_lib_B7865DB5(void);
extern int sceWlanDevDetach(void);
extern int sceWlanDrv_lib_E093A922(void);
extern int sceWlanDrv_lib_E47015D7(void);
extern int sceWlanSetWakeUp(int flag, void *param);


struct SceNetAdhocctlHandlerEntry {
    int id;
    void (*func)(int event, int error, void *arg);
    void *arg;
    u32 gp;
};

struct SceNetAdhocctlScanNode {
    struct SceNetAdhocctlScanNode *next; /* 0x00 */
    u8 bssid[6];                         /* 0x04..0x09 */
    u8 channel;                          /* 0x0a */
    u8 ssid_len;                         /* 0x0b */
    char ssid[32];                       /* 0x0c..0x2b */
    u32 mode;                            /* 0x2c */
    u16 beacon_period;                   /* 0x30 */
    u8 ie_prefix[0x1c];                  /* 0x32..0x4d */
    u8 gm_ie_type;                       /* 0x4e */
    u8 gm_ie_pad;                        /* 0x4f */
    u8 gm_peer_count;                    /* 0x50 */
    u8 gm_slot_start;                    /* 0x51 */
    u8 gm_slot_step;                     /* 0x52 */
    u8 tail_pad[0x0d];                   /* 0x53..0x5f */
};

typedef struct SceNetAdhocctlProductCode {
    u32 type;
    char product[12];
} SceNetAdhocctlProductCode;

typedef struct SceNetAdhocctlScanInfo {
    struct SceNetAdhocctlScanInfo *next; /* 0x00 */
    int channel;                         /* 0x04 */
    char name[8];                        /* 0x08 */
    u8 bssid[6];                         /* 0x10 */
    u8 pad[2];                           /* 0x16 */
    int mode;                            /* 0x18 */
} SceNetAdhocctlScanInfo;

typedef struct SceNetAdhocctlParameter {
    int channel;                         /* 0x00 */
    char group_name[8];                  /* 0x04 */
    char nickname[0x80];                 /* 0x0c */
    u8 bssid[6];                         /* 0x8c */
} SceNetAdhocctlParameter;

typedef struct SceNetAdhocctlWakeParam {
    u32 flags;
    u8 enable;
    u8 channel;
    char ssid[34];
} SceNetAdhocctlWakeParam;

typedef struct SceAdhocctlRegParam {
    u32 regtype;
    char name[256];
    u32 namelen;
    u32 version;
    u32 reg_flags;
} SceAdhocctlRegParam;

typedef struct SceAdhocctlScanReq {
    u32 bss_type;
    u8 bssid[6];
    u8 channels[14];
    u8 ssid_len;
    u8 ssid_pad[3];
    char ssid[32];
    u32 scan_type;
    u32 min_dwell;
    u32 max_dwell;
    u32 probe_delay;
} SceAdhocctlScanReq;

typedef struct SceAdhocctlBssParam {
    u8 bssid[6];
    u8 channel;
    u8 ssid_len;
    char ssid[32];
    u32 bss_type;
    u32 beacon_period;
    u8 dtim_pad[0x0e];
    u16 capability;
} SceAdhocctlBssParam;

typedef struct SceAdhocctlGameModeParam {
    u8 bss_header[0x2e];
    u16 key_len;
    u8 key[0x10];
    u16 port_be;
    u8 gm_meta[0x08];
    u16 active_flag;
    u16 active_pad;
    u16 beacon_period;
} SceAdhocctlGameModeParam;

typedef struct SceAdhocctlJoinParam {
    u8 bss_desc[0x6c];
    u32 auth_flag;
} SceAdhocctlJoinParam;

struct SceNetAdhocctlCtx {
    int evfid;                           /* 0x000 */
    int thid;                            /* 0x004 */
    int state;                           /* 0x008 */
    int channel;                         /* 0x00c */
    int scan_buf_len;                    /* 0x010 */
    int attach_retry;                    /* 0x014 */
    int pending_error;                   /* 0x018 */
    int min_stack;                       /* 0x01c */
    u32 flags;                           /* 0x020 */
    char group_name[8];                  /* 0x024..0x02b */
    u8 scan_buf[0x780];                  /* 0x02c..0x7ab */
    u32 join_next;                       /* 0x7ac */
    int join_channel;                    /* 0x7b0 */
    char join_group[8];                  /* 0x7b4..0x7bb */
    u8 join_bssid[6];                    /* 0x7bc..0x7c1 */
    u8 join_pad[2];                      /* 0x7c2..0x7c3 */
    int join_mode;                       /* 0x7c4 */
    int adhoc_type;                      /* 0x7c8 */
    char adhoc_id[12];                   /* 0x7cc..0x7d7 */
};

struct SceNetAdhocctlGameModeCtx {
    int num_peers;       /* 0x00 */
    u8 peers[16][6];     /* 0x04..0x63 */
    u8 gm_param[0x50];   /* 0x64..0xb3 */
    u32 timeout;         /* 0xb4 */
    u32 flag;            /* 0xb8 */
    int min_peers;       /* 0xbc */
};

struct SceNetAdhocctlChannelLoad {
    int skip;
    int count;
    u32 bitmap;
};


extern const u8 g_channels[4];
extern const u32 g_gm_templates[4][20];
extern int g_initialized;
extern char g_ssid_prefix[4];
extern struct SceNetAdhocctlCtx g_ctx;
extern struct SceNetAdhocctlGameModeCtx g_gm_ctx;
extern struct SceNetAdhocctlHandlerEntry g_handlers[4];
extern struct SceNetAdhocctlHandlerEntry g_lib_handlers[4];
extern struct SceNetAdhocctlChannelLoad g_ch_load[3];
extern int g_saved_power_save;
extern u8 g_ptp_buf[0x400];
extern u8 g_gm_scan_buf[0xc00];
extern int g_mutex_created;
extern u8 g_mutex_work[0x20];

void adhocctlCopySsidPrefix(char *dst);
int adhocctlCreateGameModeInternal(const char *name, int game_type, int num_peers, const u8 *macs, u32 timeout, int flag, int min_peers);
void adhocctlLoadSsidPrefix(char *prefix);
int adhocctlDoConnect(struct SceNetAdhocctlCtx *ctx);
int adhocctlDoScan(struct SceNetAdhocctlCtx *ctx);
int adhocctlDoScanQuiet(struct SceNetAdhocctlCtx *ctx);
int adhocctlDoCreate(struct SceNetAdhocctlCtx *ctx);
int adhocctlDoJoin(struct SceNetAdhocctlCtx *ctx);
int adhocctlDoConnectRaw(struct SceNetAdhocctlCtx *ctx);
int adhocctlDoCreateRaw(struct SceNetAdhocctlCtx *ctx);
void adhocctlDisconnectWlan(struct SceNetAdhocctlCtx *ctx);
void adhocctlDetachWlan(struct SceNetAdhocctlCtx *ctx);
int adhocctlFormatSsid(struct SceNetAdhocctlCtx *ctx, char *ssid, int mode_char, const char *group_name);
int adhocctlBuildGroupSsidAndChannel(struct SceNetAdhocctlCtx *ctx, char *ssid, int *ch_out);
int adhocctlBuildGameModeSsid(struct SceNetAdhocctlCtx *ctx, char *ssid);
int adhocctlNotifyHandlers(int event, int error);
int adhocctlInitInternal(int stackSize, int priority, const void *adhoc_id);
int adhocctlAddHandlerInternal(void (*func)(int, int, void *), void *arg, int *id_out);
int adhocctlGetScanInfoInternal(int *buflen, void *buf);
int adhocctlAddGameModeHandlerInternal(void (*func)(int, int, void *), void *arg, int *id_out);
int adhocctlNotifyGameModeHandlers(int event, int error);
int adhocctlTermInternal(void);
int adhocctlRequestConnect(const char *name);
int adhocctlRequestCreate(const char *name);
int adhocctlRequestJoin(const void *scan_info);
int adhocctlRequestCreateGameMode(const char *name, int num_peers, const u8 *macs, const void *param, u32 timeout, int flag, int min_peers);
int adhocctlRequestJoinGameMode(const char *name, const u8 *gc_mac, const void *param, u32 timeout, int flag);
int adhocctlRequestScan(void);
int adhocctlRequestDisconnect(void);
int adhocctlRequestExitGameMode(void);
int adhocctlDelHandlerInternal(int id);
int adhocctlGetStateInternal(int *state);
int adhocctlGetAdhocIdInternal(void *adhoc_id);
int adhocctlGetParameterInternal(void *param);
int adhocctlGetGameModeInfoInternal(void *info);
int adhocctlDelGameModeHandlerInternal(int id);
int adhocctlGetGameModeParamInternal(void *param);
int adhocctlRequestConnectRaw(const char *name);
int adhocctlRequestSuspend(void);
int adhocctlRequestCreateRaw(const char *name);
int adhocctlWorkerThread(u32 arglen, void *argp);
int adhocctlDrainWlanEvents(struct SceNetAdhocctlCtx *ctx);
int adhocctlPollWlanEvent(struct SceNetAdhocctlCtx *ctx);
int adhocctlCheckCancelFlag(struct SceNetAdhocctlCtx *ctx);
int adhocctlWaitWlanEvent(struct SceNetAdhocctlCtx *ctx, int *ev, int *reason);
int adhocctlRunGameModeHost(struct SceNetAdhocctlCtx *ctx, struct SceNetAdhocctlGameModeCtx *gm);
int adhocctlRunGameModeClient(struct SceNetAdhocctlCtx *ctx, struct SceNetAdhocctlGameModeCtx *gm);
int adhocctlSelectGameModeChannel(struct SceNetAdhocctlGameModeCtx *gm, u8 *ch_out, u8 *offset_out);
int adhocctlSetWlanPowerSave(int power_save);
void adhocctlCleanupGameMode(struct SceNetAdhocctlCtx *ctx, struct SceNetAdhocctlGameModeCtx *gm);
int adhocctlAcceptGameModePeers(struct SceNetAdhocctlGameModeCtx *gm, s64 start_time, int listen_id, const void *ie_buf, int ie_len);
int adhocctlRecvGameModePacket(struct SceNetAdhocctlGameModeCtx *gm, s64 start_time, int ptp_id, void *buf, int len);
void *adhocctlFindIeTag(int tag, void *buf, u32 len, int *tag_len_out);
int adhocctlCreateLock(void);
void adhocctlDeleteLock(void);
int adhocctlLock(void);
void adhocctlUnlock(int lock_res);


#endif /* PSPNET_ADHOCCTL_INTERNAL_H */
