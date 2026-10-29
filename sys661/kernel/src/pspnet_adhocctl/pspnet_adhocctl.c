#include <psptypes.h>

extern void *sceKernelMemset(void *dst, int val, u32 len);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 len);
extern int sceKernelLockLwMutex(void *workarea, int count, u32 *timeout);
extern int sceKernelUnlockLwMutex(void *workarea, int count);
extern int _sceKernelCheckThreadStack(void);
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

static const u8 g_channels[4] = { 1, 6, 11, 0 };

static const u32 g_gm_templates[4][20] = {
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0x00010000, 0
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0x00000100, 0x080f0004, 0x00010000, 0x000c0000
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0x00000040, 0x080f0010, 0x00010000, 0x000c0000
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0x00000100, 0x080f0010, 0x00010000, 0x001c0000
    }
};

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
    u8 pad[0x2e];                        /* 0x32..0x5f */
};

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

static int g_initialized;                              /* 0x6730 */
static char g_ssid_prefix[4];                          /* 0x6734 */
static struct SceNetAdhocctlCtx g_ctx;                 /* 0x6738..0x6f0f */
static struct SceNetAdhocctlGameModeCtx g_gm_ctx;      /* 0x6f10..0x6fcf */
static struct SceNetAdhocctlHandlerEntry g_handlers[4];     /* 0x6fd0..0x700f */
static struct SceNetAdhocctlHandlerEntry g_lib_handlers[4]; /* 0x7010..0x704f */
static struct SceNetAdhocctlChannelLoad g_ch_load[3];  /* 0x7050..0x7073 */
static int g_saved_power_save;                         /* 0x7074 */
static u8 g_ptp_buf[0x400];                            /* 0x7078..0x7477 */
static u8 g_gm_scan_buf[0xc00];                        /* 0x7478..0x8077 */
static int g_mutex_created;                            /* 0x8078 */
static u8 g_mutex_work[0x20];                          /* 0x807c..0x809b */

static void sub_0490(char *dst);
static int sub_0668(const char *name, int game_type, int num_peers, const u8 *macs, u32 timeout, int flag, int min_peers);
static void sub_11ac(char *prefix);
static int sub_12c0(struct SceNetAdhocctlCtx *ctx);
static int sub_1534(struct SceNetAdhocctlCtx *ctx);
static int sub_17b4(struct SceNetAdhocctlCtx *ctx);
static int sub_18a8(struct SceNetAdhocctlCtx *ctx);
static int sub_1b9c(struct SceNetAdhocctlCtx *ctx);
static int sub_1ec8(struct SceNetAdhocctlCtx *ctx);
static int sub_20ac(struct SceNetAdhocctlCtx *ctx);
static void sub_22b8(struct SceNetAdhocctlCtx *ctx);
static void sub_233c(struct SceNetAdhocctlCtx *ctx);
static int sub_2390(struct SceNetAdhocctlCtx *ctx, char *ssid, int mode_char, const char *group_name);
static int sub_24ec(struct SceNetAdhocctlCtx *ctx, char *ssid, int *ch_out);
static int sub_25b0(struct SceNetAdhocctlCtx *ctx, char *ssid);
static int sub_2600(int event, int error);
static int sub_277c(int stackSize, int priority, const void *adhoc_id);
static int sub_2914(void (*func)(int, int, void *), void *arg, int *id_out);
static int sub_2a04(int *buflen, void *buf);
static int sub_2c4c(void (*func)(int, int, void *), void *arg, int *id_out);
static int sub_2d54(int event, int error);
static int sub_2e04(void);
static int sub_2ed4(const char *name);
static int sub_2f80(const char *name);
static int sub_3014(const void *scan_info);
static int sub_30cc(const char *name, int num_peers, const u8 *macs, const void *param, u32 timeout, int flag, int min_peers);
static int sub_31f0(const char *name, const u8 *gc_mac, const void *param, u32 timeout, int flag);
static int sub_32e0(void);
static int sub_3344(void);
static int sub_33a4(void);
static int sub_3404(int id);
static int sub_3470(int *state);
static int sub_3484(void *adhoc_id);
static int sub_34b0(void *param);
static int sub_3544(void *info);
static int sub_35b8(int id);
static int sub_3624(void *param);
static int sub_3680(const char *name);
static int sub_3714(void);
static int sub_3730(const char *name);
static int sub_37c4(u32 arglen, void *argp);
static int sub_3bf4(struct SceNetAdhocctlCtx *ctx);
static int sub_3d0c(struct SceNetAdhocctlCtx *ctx);
static int sub_3d2c(struct SceNetAdhocctlCtx *ctx);
static int sub_3db4(struct SceNetAdhocctlCtx *ctx, int *ev, int *reason);
static int sub_3f34(struct SceNetAdhocctlCtx *ctx, struct SceNetAdhocctlGameModeCtx *gm);
static int sub_46e0(struct SceNetAdhocctlCtx *ctx, struct SceNetAdhocctlGameModeCtx *gm);
static int sub_50fc(struct SceNetAdhocctlGameModeCtx *gm, u8 *ch_out, u8 *offset_out);
static int sub_550c(int power_save);
static void sub_5580(struct SceNetAdhocctlCtx *ctx, struct SceNetAdhocctlGameModeCtx *gm);
static int sub_5658(struct SceNetAdhocctlGameModeCtx *gm, int unused, u32 start_lo, u32 start_hi, int listen_id, const void *ie_buf, int ie_len);
static int sub_5a18(struct SceNetAdhocctlGameModeCtx *gm, int unused, u32 start_lo, u32 start_hi, int ptp_id, void *buf, int len);
static void *sub_5b28(int tag, void *buf, u32 len, int *tag_len_out);
static int sub_5bf4(void);
static void sub_5c44(void);
static int sub_5c88(void);
static void sub_5cb0(int lock_res);

/* 0x0000 */
int sceNetAdhocctlInit(int stackSize, int priority, const void *adhoc_id)
{
    if (_sceKernelCheckThreadStack() < 0xbe0)
        return 0x80410005;
    if (g_initialized > 0)
        return 0x80410b07;
    if ((u32)stackSize < 0xc00) {
        g_initialized = 0;
        return 0x80410b13;
    }
    if ((u32)stackSize == 0 || priority < 0 || adhoc_id == NULL) {
        g_initialized = 0;
        return 0x80410b04;
    }

    u32 type = *(const u32 *)adhoc_id;
    if (type >= 3) {
        g_initialized = 0;
        return 0x80410b04;
    }

    const char *id_str = (const char *)adhoc_id + 4;
    int i;
    for (i = 0; i < 9; i++) {
        if (id_str[i] == 0) {
            g_initialized = 0;
            return 0x80410b04;
        }
    }

    if (type == 1) {
        KprintfForUser("***********************************************\n");
        KprintfForUser("******** WARNING from pspnet_adhocctl  ********\n");
        KprintfForUser("***********************************************\n");
        KprintfForUser("* SCE_NET_ADHOCCTL_ADHOCTYPE_RESERVED is specified.\n");
        KprintfForUser("* This adhoc_id is for debug only.\n");
        KprintfForUser("* Don't use this adhoc_id for release program.\n");
        KprintfForUser("***********************************************\n");
    }

    sub_11ac(g_ssid_prefix);
    int res = sub_277c(stackSize, priority, adhoc_id);
    if (res < 0) {
        g_initialized = 0;
        return res;
    }
    g_initialized = 1;
    return res;
}

/* 0x0174 */
int sceNetAdhocctlJoin(const void *scan_info)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (scan_info == NULL)
        return 0x80410b04;

    const char *group_name = (const char *)scan_info + 8;
    if (group_name != NULL) {
        int i = 0;
        while (i < 8 && group_name[i] != 0) {
            int f = sceNet_lib_750F705D(group_name[i]);
            i++;
            if ((f & 7) == 0)
                return 0x80410b04;
        }
        if (i < 0)
            return i;
    }
    return sub_3014(scan_info);
}

/* 0x0238 */
int sceNetAdhocctlGetPeerList(int *buflen, void *buf)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if ((u32)buf & 3)
        return 0x80410b14;
    if (buflen == NULL)
        return 0x80410b04;

    if (sceKernelGetCompiledSdkVersion() > 0x03010110) {
        if (buf == NULL) {
            if (*buflen >= 0) {
                *buflen = 0;
                return sceNetAdhocAuth_lib_BD144DA6(buflen, buf);
            }
            return sceNetAdhocAuth_lib_BD144DA6(buflen, buf);
        }
    }
    if (*buflen < 0)
        return 0x80410b04;
    return sceNetAdhocAuth_lib_BD144DA6(buflen, buf);
}

/* 0x0304 */
int sceNetAdhocctlGetPeerInfo(const u8 *mac, int size, void *peer_info)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (mac == NULL || peer_info == NULL || size < 0)
        return 0x80410b04;
    return sceNetAdhocAuth_lib_CF4D9BED(mac, size, peer_info);
}

/* 0x03a8 */
int sceNetAdhocctlGetAddrByName(const char *name, int *buflen, void *buf)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if ((u32)buf & 3)
        return 0x80410b14;
    if (name == NULL || buflen == NULL)
        return 0x80410b04;

    if (sceKernelGetCompiledSdkVersion() > 0x03010110) {
        if (buf == NULL) {
            if (*buflen >= 0) {
                *buflen = 0;
                return sceNetAdhocAuth_lib_1F9A90B8(name, buflen, buf);
            }
            return sceNetAdhocAuth_lib_1F9A90B8(name, buflen, buf);
        }
    }
    if (*buflen < 0)
        return 0x80410b04;
    return sceNetAdhocAuth_lib_1F9A90B8(name, buflen, buf);
}

/* 0x0490 */
static void sub_0490(char *dst)
{
    sceKernelMemcpy(dst, g_ssid_prefix, 3);
}

/* 0x04b4 */
int sceNetAdhocctlTerm(void)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0;

    int res = sub_2e04();
    if (res >= 0)
        g_initialized = 0;
    return res;
}

/* 0x0518 */
int sceNetAdhocctlConnect(const char *name)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (name != NULL) {
        int i = 0;
        while (i < 8 && name[i] != 0) {
            int f = sceNet_lib_750F705D(name[i]);
            i++;
            if ((f & 7) == 0)
                return 0x80410b04;
        }
        if (i < 0)
            return i;
    }
    return sub_2ed4(name);
}

/* 0x05c0 */
int sceNetAdhocctlCreate(const char *name)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (name != NULL) {
        int i = 0;
        while (i < 8 && name[i] != 0) {
            int f = sceNet_lib_750F705D(name[i]);
            i++;
            if ((f & 7) == 0)
                return 0x80410b04;
        }
        if (i < 0)
            return i;
    }
    return sub_2f80(name);
}

/* 0x0668 */
static int sub_0668(const char *name, int game_type, int num_peers, const u8 *macs, u32 timeout, int flag, int min_peers)
{
    if (g_initialized == 0)
        return 0x80410b08;
    if (name == NULL || (u32)(game_type - 1) >= 3 || min_peers < 2 || num_peers < min_peers || num_peers < 2 || num_peers >= 17 || macs == NULL)
        return 0x80410b04;

    int i = 0;
    while (i < 8 && name[i] != 0) {
        int f = sceNet_lib_750F705D(name[i]);
        i++;
        if ((f & 7) == 0)
            return 0x80410b04;
    }
    if (i < 0)
        return i;

    const u8 *tmpl = (const u8 *)g_gm_templates[game_type];
    if (tmpl[0x44] < num_peers)
        return 0x80410b04;

    return sub_30cc(name, num_peers, macs, tmpl, timeout, flag, min_peers);
}

/* 0x07d0 */
int sceNetAdhocctlCreateEnterGameMode(const char *name, int game_type, int num_peers, const u8 *macs, u32 timeout, int flag)
{
    if (_sceKernelCheckThreadStack() < 0xbe0)
        return 0x80410005;
    return sub_0668(name, game_type, num_peers, macs, timeout, flag, num_peers);
}

/* 0x086c */
int sceNetAdhocctl_B0B80E80(const char *name, int game_type, int min_peers, int num_peers, const u8 *macs, u32 timeout, int flag)
{
    if (_sceKernelCheckThreadStack() < 0xbe0)
        return 0x80410005;
    return sub_0668(name, game_type, num_peers, macs, timeout, flag, min_peers);
}

/* 0x0914 */
int sceNetAdhocctlJoinEnterGameMode(const char *name, const u8 *gc_mac, u32 timeout, int flag)
{
    if (_sceKernelCheckThreadStack() < 0xbe0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (name == NULL || gc_mac == NULL)
        return 0x80410b04;

    int i = 0;
    while (i < 8 && name[i] != 0) {
        int f = sceNet_lib_750F705D(name[i]);
        i++;
        if ((f & 7) == 0)
            return 0x80410b04;
    }
    if (i < 0)
        return i;

    return sub_31f0(name, gc_mac, g_gm_templates[0], timeout, flag);
}

/* 0x0a08 */
int sceNetAdhocctlScan(void)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    return sub_32e0();
}

/* 0x0a5c */
int sceNetAdhocctlDisconnect(void)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    return sub_3344();
}

/* 0x0ab0 */
int sceNetAdhocctlExitGameMode(void)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    return sub_33a4();
}

/* 0x0b04 */
int sceNetAdhocctlAddHandler(void (*func)(int, int, void *), void *arg)
{
    int id;
    if (_sceKernelCheckThreadStack() < 0xbe0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (func == NULL)
        return 0x80410b04;
    int res = sub_2914(func, arg, &id);
    return (res < 0) ? res : id;
}

/* 0x0b90 */
int sceNetAdhocctlDelHandler(int id)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    return sub_3404(id);
}

/* 0x0bf0 */
int sceNetAdhocctlGetState(int *state)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (state == NULL)
        return 0x80410b04;
    return sub_3470(state);
}

/* 0x0c5c */
int sceNetAdhocctlGetAdhocId(void *adhoc_id)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (adhoc_id == NULL)
        return 0x80410b04;
    return sub_3484(adhoc_id);
}

/* 0x0cc8 */
int sceNetAdhocctlGetNameByAddr(const u8 *mac, char *name)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (mac == NULL || name == NULL)
        return 0x80410b04;
    return sceNetAdhocAuth_lib_76F26AB0(mac, name);
}

/* 0x0d50 */
int sceNetAdhocctlGetParameter(void *param)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (param == NULL)
        return 0x80410b04;
    return sub_34b0(param);
}

/* 0x0dbc */
int sceNetAdhocctlGetScanInfo(int *buflen, void *buf)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if ((u32)buf & 3)
        return 0x80410b14;
    return sub_2a04(buflen, buf);
}

/* 0x0e3c */
int sceNetAdhocctlGetGameModeInfo(void *info)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (info == NULL)
        return 0x80410b04;
    return sub_3544(info);
}

/* 0x0ea8 */
int sceNetAdhocctl_lib_EFA6AEAF(const void *param, int num_peers, const u8 *macs, u32 timeout, int flag)
{
    if (g_initialized == 0)
        return 0x80410b08;
    if (param == NULL)
        return 0x80410b04;
    sub_30cc(NULL, num_peers, macs, param, timeout, flag | 1, num_peers);
    return 0;
}

/* 0x0f18 */
int sceNetAdhocctl_lib_1593C05D(const void *param, const u8 *gc_mac, u32 timeout, int flag)
{
    if (g_initialized == 0)
        return 0x80410b08;
    if (param == NULL)
        return 0x80410b04;
    sub_31f0(NULL, gc_mac, param, timeout, flag | 1);
    return 0;
}

/* 0x0f84 */
int sceNetAdhocctl_lib_F8BABD85(void (*func)(int, int, void *), void *arg)
{
    int id;
    if (g_initialized == 0)
        return 0x80410b08;
    if (func == NULL)
        return 0x80410b04;
    int res = sub_2c4c(func, arg, &id);
    return (res < 0) ? res : id;
}

/* 0x0fd8 */
int sceNetAdhocctl_lib_1C679240(int id)
{
    if (g_initialized == 0)
        return 0x80410b08;
    return sub_35b8(id);
}

/* 0x1008 */
int sceNetAdhocctl_lib_CDEA7ACB(void *param)
{
    if (g_initialized == 0)
        return 0x80410b08;
    if (param == NULL)
        return 0x80410b04;
    return sub_3624(param);
}

/* 0x1044 */
int sceNetAdhocctl_lib_1A2B6D03(const char *name)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (name != NULL) {
        int i = 0;
        while (i < 8 && name[i] != 0) {
            int f = sceNet_lib_750F705D(name[i]);
            i++;
            if ((f & 7) == 0)
                return 0x80410b04;
        }
        if (i < 0)
            return i;
    }
    return sub_3680(name);
}

/* 0x10e8 */
int sceNetAdhocctl_lib_1572422C(void)
{
    return sub_3714();
}

/* 0x1104 */
int sceNetAdhocctl_lib_F6057CC4(const char *name)
{
    if (_sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (name != NULL) {
        int i = 0;
        while (i < 8 && name[i] != 0) {
            int f = sceNet_lib_750F705D(name[i]);
            i++;
            if ((f & 7) == 0)
                return 0x80410b04;
        }
        if (i < 0)
            return i;
    }
    return sub_3730(name);
}

/* 0x11ac */
static void sub_11ac(char *prefix)
{
    u8 reg_param[0x110];
    int reg_hd, cat_hd, key_hd, type;
    u32 size;

    sceKernelMemset(reg_param, 0, 0x110);
    *(u32 *)(reg_param + 0x00) = 1;
    *(u32 *)(reg_param + 0x108) = 1;
    *(u32 *)(reg_param + 0x10c) = 1;

    if (sceRegOpenRegistry(reg_param, 2, &reg_hd) >= 0) {
        if (sceRegOpenCategory(reg_hd, "/CONFIG/NETWORK/ADHOC", 2, &cat_hd) >= 0) {
            if (sceRegGetKeyInfo(cat_hd, "ssid_prefix", &key_hd, &type, &size) >= 0 &&
                size == 4 && type == 3) {
                if (sceRegGetKeyValue(cat_hd, key_hd, prefix, 4) >= 0) {
                    sceRegCloseCategory(cat_hd);
                    sceRegCloseRegistry(reg_hd);
                    return;
                }
            }
            sceRegCloseCategory(cat_hd);
        }
        sceRegCloseRegistry(reg_hd);
    }

    int len = sceNet_lib_D8722983("PSP");
    sceKernelMemcpy(prefix, "PSP", len);
}

/* 0x12c0 */
static int sub_12c0(struct SceNetAdhocctlCtx *ctx)
{
    if (ctx->state != 0)
        return 0x80410b02;
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    int retry = 0;
    while (1) {
        int res = sceWlanDevAttach();
        retry++;
        if (res == 0 || res == (int)0x80410d0f)
            break;
        if (res < 0 && res != (int)0x80410d0e)
            return res;
        if (retry >= ctx->attach_retry)
            return 0x80410b05;
        sceKernelDelayThread(1000000);
    }

    ctx->flags &= ~2u;
    int res = sceNet_lib_5216CBF5("wlan");
    if (res < 0)
        goto fail;
    res = sceNet_lib_AB7DD9A5("wlan", ctx->evfid, 8);
    if (res < 0)
        goto fail;

    char ssid[0x24];
    int ch;
    sceKernelMemset(ssid, 0, 0x21);
    res = sub_24ec(ctx, ssid, &ch);
    if (res < 0)
        goto fail;
    if (sceWlanGetSwitchState() == 0) {
        res = 0x80410b03;
        goto fail;
    }

    sceNet_lib_D5B64E37("wlan", ssid, res);
    res = sub_3d2c(ctx);
    if ((((u32)res >> 16) & 0xfff) == 0x41) {
        if (res >= 0)
            return res;
        goto fail;
    }
    if (res < 0)
        goto fail;

    char nickname[0x80];
    res = sceUtilityGetSystemParamString(1, nickname, 0x80);
    if (res < 0)
        goto fail;
    res = sub_3d2c(ctx);
    if (res < 0)
        goto fail;

    u32 auth_opt[5];
    auth_opt[0] = 1000000;
    auth_opt[1] = 500000;
    auth_opt[2] = 5;
    auth_opt[3] = 30000000;
    auth_opt[4] = 300000000;
    res = sceNetAdhocAuth_lib_89F2A732("wlan", 0x30, 0x2000, auth_opt, 16, nickname);
    if (res < 0)
        goto fail;

    ctx->state = 1;
    ctx->channel = ch;
    if (res >= 0)
        return res;

fail:
    sceNetAdhocAuth_lib_2E6AA271();
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    int err = 0;
    sceNet_lib_DA02F383("wlan", &err);
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    return res;
}

/* 0x1534 */
static int sub_1534(struct SceNetAdhocctlCtx *ctx)
{
    int lock_res = 0;
    if (ctx->state != 0)
        return 0x80410b02;
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    ctx->state = 2;
    int retry = 0;
    int res;
    while (1) {
        res = sceWlanDevAttach();
        retry++;
        if (res == 0 || res == (int)0x80410d0f)
            break;
        if (res < 0 && res != (int)0x80410d0e) {
            ctx->state = 0;
            sub_5cb0(lock_res);
            ctx->pending_error = 0;
            return res;
        }
        if (retry >= ctx->attach_retry) {
            res = 0x80410b05;
            ctx->state = 0;
            sub_5cb0(lock_res);
            ctx->pending_error = 0;
            return res;
        }
        sceKernelDelayThread(1000000);
    }

    res = sceNet_lib_5216CBF5("wlan");
    if (res < 0) {
        sceWlanDevDetach();
        ctx->state = 0;
        sub_5cb0(lock_res);
        ctx->pending_error = 0;
        return res;
    }

    res = sceNet_lib_AB7DD9A5("wlan", ctx->evfid, 8);
    if (res < 0)
        goto detach;
    if (sceWlanGetSwitchState() == 0) {
        res = 0x80410b03;
        goto detach;
    }

    lock_res = sub_5c88();
    u8 scan_req[0x4c];
    sceKernelMemset(scan_req, 0, 0x4c);
    sceKernelMemset(ctx->scan_buf, 0, 0x780);
    *(u32 *)(scan_req + 0x00) = 2;

    int ch = 0;
    res = sceUtilityGetSystemParamInt(2, &ch);
    if (res < 0)
        goto detach;
    if (ch != 6 && ch != 11) {
        if (ch != 1)
            ch = 0;
    }

    int i;
    for (i = 0; i < 14; i++)
        scan_req[0x0a + i] = 0;

    if (ch == 0) {
        scan_req[0x0a] = 11;
        scan_req[0x0b] = 6;
        scan_req[0x0c] = 1;
    } else {
        scan_req[0x0a] = (u8)ch;
    }

    ctx->scan_buf_len = 0x780;
    *(u32 *)(scan_req + 0x3c) = 1;
    *(u32 *)(scan_req + 0x40) = 0;
    *(u32 *)(scan_req + 0x44) = 6;
    *(u32 *)(scan_req + 0x48) = 100;
    int err = 0;
    sceNet_lib_7BA3ED91("wlan", scan_req, &ctx->scan_buf_len, ctx->scan_buf, &err);
    res = sub_3d2c(ctx);
    if ((((u32)res >> 16) & 0xfff) != 0x41)
        res = 0;

detach:
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    ctx->state = 0;
    sub_5cb0(lock_res);
    ctx->pending_error = 0;
    return res;
}

/* 0x17b4 */
static int sub_17b4(struct SceNetAdhocctlCtx *ctx)
{
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    u8 scan_req[0x4c];
    u8 scan_buf[0x60];
    sceKernelMemset(scan_req, 0, 0x4c);
    sceKernelMemset(scan_buf, 0, 0x60);
    *(u32 *)(scan_req + 0x00) = 2;
    scan_req[0x0a] = 1;
    int i;
    for (i = 1; i < 14; i++)
        scan_req[0x0a + i] = 0;

    *(u32 *)(scan_req + 0x3c) = 1;
    *(u32 *)(scan_req + 0x40) = 0;
    *(u32 *)(scan_req + 0x44) = 6;
    *(u32 *)(scan_req + 0x48) = 100;
    int buflen = 0x60;
    int err = 0;
    sceNet_lib_7BA3ED91("wlan", scan_req, &buflen, scan_buf, &err);
    int res = sub_3d2c(ctx);
    if ((((u32)res >> 16) & 0xfff) == 0x41)
        return res;
    return (res < 0) ? res : 0;
}

/* 0x18a8 */
static int sub_18a8(struct SceNetAdhocctlCtx *ctx)
{
    if (ctx->state != 0)
        return 0x80410b02;
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    int retry = 0;
    while (1) {
        int res = sceWlanDevAttach();
        retry++;
        if (res == 0 || res == (int)0x80410d0f)
            break;
        if (res < 0 && res != (int)0x80410d0e)
            return res;
        if (retry >= ctx->attach_retry)
            return 0x80410b05;
        sceKernelDelayThread(1000000);
    }

    ctx->flags &= ~2u;
    int res = sceNet_lib_5216CBF5("wlan");
    if (res < 0)
        goto fail;
    res = sceNet_lib_AB7DD9A5("wlan", ctx->evfid, 8);
    if (res < 0)
        goto fail;
    res = sub_17b4(ctx);
    if (res < 0)
        goto fail;

    char ssid[0x24];
    int ch;
    sceKernelMemset(ssid, 0, 0x21);
    int ssid_len = sub_24ec(ctx, ssid, &ch);
    if (ssid_len < 0) {
        res = ssid_len;
        goto fail;
    }
    res = sub_3d2c(ctx);
    if (res < 0)
        goto fail;

    u8 param[0x70];
    sceKernelMemset(param, 0, 0x70);
    sceKernelMemcpy(param + 8, ssid, ssid_len);
    param[6] = (u8)ch;
    param[7] = (u8)ssid_len;
    *(u32 *)(param + 0x28) = 2;
    *(u32 *)(param + 0x2c) = 100;
    *(u16 *)(param + 0x3e) = 0x22;
    int err = 0;
    sceNet_lib_03164B12("wlan", param, &err);
    res = sub_3d2c(ctx);
    if ((((u32)res >> 16) & 0xfff) == 0x41) {
        if (res >= 0)
            return res;
        goto fail;
    }
    if (res < 0)
        goto fail;

    res = sub_17b4(ctx);
    if (res < 0)
        goto fail;
    res = sub_3d2c(ctx);
    if (res < 0)
        goto fail;

    char nickname[0x80];
    res = sceUtilityGetSystemParamString(1, nickname, 0x80);
    if (res < 0)
        goto fail;
    res = sub_3d2c(ctx);
    if (res < 0)
        goto fail;

    u32 auth_opt[5];
    auth_opt[0] = 1000000;
    auth_opt[1] = 500000;
    auth_opt[2] = 5;
    auth_opt[3] = 30000000;
    auth_opt[4] = 300000000;
    res = sceNetAdhocAuth_lib_89F2A732("wlan", 0x30, 0x2000, auth_opt, 16, nickname);
    if (res < 0)
        goto fail;

    ctx->state = 1;
    ctx->channel = ch;
    return res;

fail:
    sceNetAdhocAuth_lib_2E6AA271();
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    err = 0;
    sceNet_lib_DA02F383("wlan", &err);
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    return res;
}

/* 0x1b9c */
static int sub_1b9c(struct SceNetAdhocctlCtx *ctx)
{
    if (ctx->state != 0)
        return 0x80410b02;
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    int retry = 0;
    while (1) {
        int res = sceWlanDevAttach();
        retry++;
        if (res == 0 || res == (int)0x80410d0f)
            break;
        if (res < 0 && res != (int)0x80410d0e)
            return res;
        if (retry >= ctx->attach_retry)
            return 0x80410b05;
        sceKernelDelayThread(1000000);
    }

    ctx->flags &= ~2u;
    int res = sceNet_lib_5216CBF5("wlan");
    if (res < 0)
        goto fail;
    res = sceNet_lib_AB7DD9A5("wlan", ctx->evfid, 8);
    if (res < 0)
        goto fail;

    int idx = -1;
    int i;
    for (i = 0; i < 20; i++) {
        if (sceNet_lib_E0A81C7C(ctx->scan_buf + i * 0x60 + 4, ctx->join_bssid, 6) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        res = 0x80410b0a;
        goto fail;
    }

    int sys_ch = 0;
    res = sceUtilityGetSystemParamInt(2, &sys_ch);
    if (res < 0)
        goto fail;
    if (sys_ch != 0 && sys_ch != ctx->join_channel) {
        res = 0x80410b11;
        goto fail;
    }
    if (sceWlanGetSwitchState() == 0) {
        res = 0x80410b03;
        goto fail;
    }

    u8 param[0x70];
    sceKernelMemset(param, 0, 0x70);
    u8 *entry = ctx->scan_buf + idx * 0x60;
    sceKernelMemcpy(param, entry + 4, 0x5c);
    int err = 0;
    sceNet_lib_389728AB("wlan", param, &err);
    res = sub_3d2c(ctx);
    if ((((u32)res >> 16) & 0xfff) == 0x41) {
        if (res >= 0)
            return res;
        goto fail;
    }
    if (res < 0)
        goto fail;

    res = sub_3d2c(ctx);
    if (res < 0)
        goto fail;

    char nickname[0x80];
    res = sceUtilityGetSystemParamString(1, nickname, 0x80);
    if (res < 0)
        goto fail;
    res = sub_3d2c(ctx);
    if (res < 0)
        goto fail;

    u32 auth_opt[5];
    auth_opt[0] = 1000000;
    auth_opt[1] = 500000;
    auth_opt[2] = 5;
    auth_opt[3] = 30000000;
    auth_opt[4] = 300000000;
    res = sceNetAdhocAuth_lib_89F2A732("wlan", 0x30, 0x2000, auth_opt, 16, nickname);
    if (res < 0)
        goto fail;

    ctx->state = 1;
    ctx->channel = entry[0x0a];
    return res;

fail:
    sceNetAdhocAuth_lib_2E6AA271();
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    err = 0;
    sceNet_lib_DA02F383("wlan", &err);
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    return res;
}

/* 0x1ec8 */
static int sub_1ec8(struct SceNetAdhocctlCtx *ctx)
{
    if (ctx->state != 0)
        return 0x80410b02;
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    int retry = 0;
    while (1) {
        int res = sceWlanDevAttach();
        retry++;
        if (res == 0 || res == (int)0x80410d0f)
            break;
        if (res < 0 && res != (int)0x80410d0e)
            return res;
        if (retry >= ctx->attach_retry)
            return 0x80410b05;
        sceKernelDelayThread(1000000);
    }

    ctx->flags &= ~2u;
    int res = sceNet_lib_5216CBF5("wlan");
    if (res < 0)
        goto fail;
    res = sceNet_lib_AB7DD9A5("wlan", ctx->evfid, 8);
    if (res < 0)
        goto fail;

    char ssid[0x30];
    int ch;
    res = sub_24ec(ctx, ssid, &ch);
    if (res < 0)
        goto fail;

    u8 hd_param[0x28];
    sceKernelMemset(hd_param, 0, 0x28);
    *(u32 *)(hd_param + 0) = 0;
    hd_param[4] = 1;
    hd_param[5] = (u8)ch;
    sceKernelMemcpy(hd_param + 6, ssid, res);

    if (sceWlanGetSwitchState() == 0) {
        res = 0x80410b03;
        goto fail;
    }

    res = sub_3d2c(ctx);
    if (res < 0)
        goto fail;
    res = sceWlanSetHostDiscover(1, hd_param);
    if (res < 0)
        goto fail;

    ctx->state = 4;
    ctx->channel = ch;
    return res;

fail:
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    return res;
}

/* 0x20ac */
static int sub_20ac(struct SceNetAdhocctlCtx *ctx)
{
    if (ctx->state != 0)
        return 0x80410b02;
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    int retry = 0;
    while (1) {
        int res = sceWlanDevAttach();
        retry++;
        if (res == 0 || res == (int)0x80410d0f)
            break;
        if (res < 0 && res != (int)0x80410d0e)
            return res;
        if (retry >= ctx->attach_retry)
            return 0x80410b05;
        sceKernelDelayThread(1000000);
    }

    ctx->flags &= ~2u;
    int res = sceNet_lib_5216CBF5("wlan");
    if (res < 0)
        goto fail;
    res = sceNet_lib_AB7DD9A5("wlan", ctx->evfid, 8);
    if (res < 0)
        goto fail;

    char ssid[0x30];
    int ch;
    res = sub_24ec(ctx, ssid, &ch);
    if (res < 0)
        goto fail;

    u8 wu_param[0x28];
    sceKernelMemset(wu_param, 0, 0x28);
    *(u32 *)(wu_param + 0) = 0;
    wu_param[4] = 1;
    wu_param[5] = (u8)ch;
    sceKernelMemcpy(wu_param + 6, ssid, res);

    if (sceWlanGetSwitchState() == 0) {
        res = 0x80410b03;
        goto fail;
    }

    res = sub_3d2c(ctx);
    if (res < 0)
        goto fail;
    res = sceWlanSetWakeUp(1, wu_param);
    if (res < 0)
        goto fail;
    res = scePowerRequestSuspend();
    if (res < 0) {
        sceWlanSetWakeUp(0, wu_param);
        goto fail;
    }

    ctx->state = 5;
    ctx->channel = ch;
    return res;

fail:
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    return res;
}

/* 0x22b8 */
static void sub_22b8(struct SceNetAdhocctlCtx *ctx)
{
    int err = 0;
    sceNetAdhocAuth_lib_2E6AA271();
    sceWlanSetHostDiscover(0, NULL);
    sceWlanSetWakeUp(0, NULL);
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    sceNet_lib_DA02F383("wlan", &err);
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    ctx->state = 0;
    ctx->pending_error = 0;
}

/* 0x233c */
static void sub_233c(struct SceNetAdhocctlCtx *ctx)
{
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    ctx->state = 0;
}

/* 0x2390 */
static int sub_2390(struct SceNetAdhocctlCtx *ctx, char *ssid, int mode_char, const char *group_name)
{
    char mch = (char)mode_char;
    char tch;
    if (ctx->adhoc_type == 0)
        tch = 'A';
    else if (ctx->adhoc_type == 1)
        tch = 'X';
    else if (ctx->adhoc_type == 2)
        tch = 'S';
    else
        return 0x80410b04;

    char prefix[16];
    sub_0490(prefix);
    sceKernelMemset(ssid, 0, 0x21);
    sceKernelMemcpy(ssid, prefix, 3);
    sceKernelMemcpy(ssid + 3, "_", 1);
    sceKernelMemcpy(ssid + 4, &tch, 1);
    sceKernelMemcpy(ssid + 5, ctx->adhoc_id, 9);
    sceKernelMemcpy(ssid + 14, "_", 1);
    sceKernelMemcpy(ssid + 15, &mch, 1);
    sceKernelMemcpy(ssid + 16, "_", 1);

    int pos = 17;
    if (group_name != NULL && group_name[0] != 0) {
        int i = 0;
        while (i < 8 && group_name[i] != 0) {
            sceKernelMemcpy(ssid + pos, &group_name[i], 1);
            i++;
            pos++;
        }
    }
    return pos;
}

/* 0x24ec */
static int sub_24ec(struct SceNetAdhocctlCtx *ctx, char *ssid, int *ch_out)
{
    int ch;
    sceKernelMemset(ssid, 0, 0x21);
    int res = sceUtilityGetSystemParamInt(2, &ch);
    if (res < 0)
        return res;
    if (ch == 6 || ch == 11) {
        *ch_out = ch;
    } else {
        if (ch != 1)
            ch = 11;
        *ch_out = ch;
    }
    if ((u32)(*ch_out - 1) >= 14)
        return 0x80410b81;
    return sub_2390(ctx, ssid, 'L', ctx->group_name);
}

/* 0x25b0 */
static int sub_25b0(struct SceNetAdhocctlCtx *ctx, char *ssid)
{
    sceKernelMemset(ssid, 0, 0x21);
    return sub_2390(ctx, ssid, 'G', ctx->group_name);
}

/* 0x2600 */
static int sub_2600(int event, int error)
{
    int lock_res = sub_5c88();
    int i;
    for (i = 0; i < 4; i++) {
        if (g_handlers[i].func != NULL) {
            if (sceWlanDrv_lib_B7865DB5() == 0) {
                u32 saved_gp;
                __asm__ volatile ("move %0, $gp" : "=r"(saved_gp));
                __asm__ volatile ("move $gp, %0" : : "r"(g_handlers[i].gp));
                int rem = _sceKernelCheckThreadStack() - g_ctx.min_stack;
                if (rem > 0) {
                    u32 alloc = (rem + 15) & ~15;
                    void * volatile dummy = __builtin_alloca(alloc);
                    (void)dummy;
                    g_handlers[i].func(event, error, g_handlers[i].arg);
                } else {
                    g_handlers[i].func(event, error, g_handlers[i].arg);
                }
                __asm__ volatile ("move $gp, %0" : : "r"(saved_gp));
            }
        }
    }
    for (i = 0; i < 4; i++) {
        if (g_lib_handlers[i].func != NULL) {
            u32 saved_gp;
            __asm__ volatile ("move %0, $gp" : "=r"(saved_gp));
            __asm__ volatile ("move $gp, %0" : : "r"(g_lib_handlers[i].gp));
            g_lib_handlers[i].func(event, error, g_lib_handlers[i].arg);
            __asm__ volatile ("move $gp, %0" : : "r"(saved_gp));
        }
    }
    sub_5cb0(lock_res);
    return 0;
}

/* 0x277c */
static int sub_277c(int stackSize, int priority, const void *adhoc_id)
{
    struct SceNetAdhocctlCtx *ctx = &g_ctx;
    sceWlanDrv_lib_768E275B();
    sceKernelMemset(ctx, 0, sizeof(*ctx));
    sceKernelMemset(g_handlers, 0, sizeof(g_handlers));
    sceKernelMemset(g_lib_handlers, 0, sizeof(g_lib_handlers));
    ctx->state = 0;

    int res = sceNetAdhocAuth_lib_86004235();
    if (res < 0)
        return res;

    sceKernelMemcpy(&ctx->adhoc_type, adhoc_id, 0x10);
    res = sub_5bf4();
    if (res < 0) {
        sceNetAdhocAuth_lib_6074D8F1();
        return res;
    }

    ctx->evfid = sceKernelCreateEventFlag("SceNetAdhocctl", 0, 0, NULL);
    if (ctx->evfid < 0) {
        res = ctx->evfid;
        sub_5c44();
        sceNetAdhocAuth_lib_6074D8F1();
        return res;
    }

    ctx->thid = sceKernelCreateThread("SceNetAdhocctl", sub_37c4, priority, stackSize, 0, NULL);
    if (ctx->thid < 0) {
        res = ctx->thid;
        sceKernelDeleteEventFlag(ctx->evfid);
        sub_5c44();
        sceNetAdhocAuth_lib_6074D8F1();
        return res;
    }

    res = sceKernelStartThread(ctx->thid, 4, &ctx);
    if (res < 0) {
        sceKernelTerminateThread(ctx->thid);
        sceKernelDeleteThread(ctx->thid);
        sceKernelDeleteEventFlag(ctx->evfid);
        sub_5c44();
        sceNetAdhocAuth_lib_6074D8F1();
        return res;
    }

    ctx->min_stack = stackSize - 0xc00;
    ctx->attach_retry = 5;
    return res;
}

/* 0x2914 */
static int sub_2914(void (*func)(int, int, void *), void *arg, int *id_out)
{
    int lock_res = sub_5c88();
    if (lock_res < 0)
        return lock_res;

    int i;
    for (i = 0; i < 4; i++) {
        if (g_handlers[i].func == NULL)
            break;
    }
    if (i == 4) {
        sub_5cb0(lock_res);
        return 0x80410b12;
    }

    g_handlers[i].id = i;
    g_handlers[i].func = func;
    g_handlers[i].arg = arg;
    if (sceKernelGetModuleGPByAddress(func, &g_handlers[i].gp) < 0) {
        u32 cur_gp;
        __asm__ volatile ("move %0, $gp" : "=r"(cur_gp));
        g_handlers[i].gp = cur_gp;
    }
    *id_out = i;
    sub_5cb0(lock_res);
    return 0;
}

/* 0x2a04 */
static int sub_2a04(int *buflen, void *buf)
{
    int lock_res = sub_5c88();
    if (lock_res < 0)
        return lock_res;

    char id_match[16];
    sceKernelMemset(id_match, 0, 10);
    char tch;
    if (g_ctx.adhoc_type == 0)
        tch = 'A';
    else if (g_ctx.adhoc_type == 1)
        tch = 'X';
    else if (g_ctx.adhoc_type == 2)
        tch = 'S';
    else
        return 0x80410b81;

    sceKernelMemcpy(id_match, &tch, 1);
    sceKernelMemcpy(id_match + 1, g_ctx.adhoc_id, 9);

    char prefix[4];
    sceKernelMemset(prefix, 0, 4);
    sub_0490(prefix);

    struct SceNetAdhocctlScanNode *node = (struct SceNetAdhocctlScanNode *)g_ctx.scan_buf;
    u8 *prev = NULL;
    u8 *cur = (u8 *)buf;
    int count = 0;
    int req_bytes = 0x1c;

    while (node != NULL) {
        if (node->ssid_len >= 0x10) {
            int plen = sceNet_lib_D8722983("PSP");
            if (sceNet_lib_E0A81C7C(id_match, (const u8 *)node + plen + 0x0d, 10) == 0 &&
                sceNet_lib_E0A81C7C(prefix, node->ssid, 3) == 0) {
                if (buf != NULL) {
                    if (*buflen < req_bytes)
                        break;
                    if (prev != NULL)
                        *(u32 *)prev = (u32)cur;
                    *(u32 *)(cur + 4) = node->channel;
                    u8 m = (u8)node->ssid[15];
                    if (m == 'L')
                        *(u32 *)(cur + 0x18) = 0;
                    else
                        *(u32 *)(cur + 0x18) = (m == 'G') ? 1 : -1;
                    sceKernelMemcpy(cur + 8, node->ssid + 17, 8);
                    sceKernelMemcpy(cur + 0x10, node->bssid, 6);
                    *(u32 *)(cur + 0) = 0;
                    prev = cur;
                    cur += 0x1c;
                }
                count++;
                req_bytes += 0x1c;
            }
        }
        node = node->next;
    }

    *buflen = count * 0x1c;
    sub_5cb0(lock_res);
    return 0;
}

/* 0x2c4c */
static int sub_2c4c(void (*func)(int, int, void *), void *arg, int *id_out)
{
    int lock_res = sub_5c88();
    if (lock_res < 0)
        return lock_res;

    if (g_ctx.pending_error != 0) {
        int err = g_ctx.pending_error;
        sub_5cb0(lock_res);
        return err;
    }

    int i;
    for (i = 0; i < 4; i++) {
        if (g_lib_handlers[i].func == NULL)
            break;
    }
    if (i == 4) {
        sub_5cb0(lock_res);
        return 0x80410b12;
    }

    g_lib_handlers[i].id = i;
    g_lib_handlers[i].func = func;
    g_lib_handlers[i].arg = arg;
    if (sceKernelGetModuleGPByAddress(func, &g_lib_handlers[i].gp) < 0) {
        u32 cur_gp;
        __asm__ volatile ("move %0, $gp" : "=r"(cur_gp));
        g_lib_handlers[i].gp = cur_gp;
    }
    *id_out = i;
    sub_5cb0(lock_res);
    return 0;
}

/* 0x2d54 */
static int sub_2d54(int event, int error)
{
    int lock_res = sub_5c88();
    int i;
    for (i = 0; i < 4; i++) {
        if (g_lib_handlers[i].func != NULL) {
            u32 saved_gp;
            __asm__ volatile ("move %0, $gp" : "=r"(saved_gp));
            __asm__ volatile ("move $gp, %0" : : "r"(g_lib_handlers[i].gp));
            g_lib_handlers[i].func(event, error, g_lib_handlers[i].arg);
            __asm__ volatile ("move $gp, %0" : : "r"(saved_gp));
        }
    }
    sub_5cb0(lock_res);
    return 0;
}

/* 0x2e04 */
static int sub_2e04(void)
{
    if (g_ctx.flags & 8)
        return 0x80410b10;

    g_ctx.flags |= 8;
    sceKernelSetEventFlag(g_ctx.evfid, 0x200);
    int res = sceKernelWaitThreadEnd(g_ctx.thid, NULL);
    if (res >= 0) {
        sceNetFreeThreadinfo(g_ctx.thid);
        sceKernelTerminateThread(g_ctx.thid);
        sceKernelDeleteThread(g_ctx.thid);
        if (g_ctx.evfid >= 0) {
            sceKernelDeleteEventFlag(g_ctx.evfid);
            g_ctx.evfid = -1;
        }
        sub_5c44();
        sceNetAdhocAuth_lib_6074D8F1();
        sceWlanDrv_lib_E093A922();
    }
    g_ctx.flags &= ~8u;
    return res;
}

/* 0x2ed4 */
static int sub_2ed4(const char *name)
{
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    sceKernelMemset(g_ctx.group_name, 0, 8);
    if (name != NULL)
        sceNet_lib_B5CE388A(g_ctx.group_name, name, 8);
    sceKernelSetEventFlag(g_ctx.evfid, 1);
    int d = sceWlanDrv_lib_E47015D7();
    if (d > 0)
        sceKernelDelayThread(d);
    return 0;
}

/* 0x2f80 */
static int sub_2f80(const char *name)
{
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    sceKernelMemset(g_ctx.group_name, 0, 8);
    if (name != NULL)
        sceNet_lib_B5CE388A(g_ctx.group_name, name, 8);
    sceKernelSetEventFlag(g_ctx.evfid, 0x80);
    return 0;
}

/* 0x3014 */
static int sub_3014(const void *scan_info)
{
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    if (scan_info == NULL)
        return 0x80410b04;
    sceKernelMemcpy(&g_ctx.join_next, scan_info, 0x1c);
    sceKernelMemset(g_ctx.group_name, 0, 8);
    if ((const char *)scan_info + 8 != NULL)
        sceNet_lib_B5CE388A(g_ctx.group_name, (const char *)scan_info + 8, 8);
    sceKernelSetEventFlag(g_ctx.evfid, 0x100);
    return 0;
}

/* 0x30cc */
static int sub_30cc(const char *name, int num_peers, const u8 *macs, const void *param, u32 timeout, int flag, int min_peers)
{
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    if (name != NULL)
        sceKernelMemcpy(g_ctx.group_name, name, 8);
    else
        sceKernelMemset(g_ctx.group_name, 0, 8);
    g_gm_ctx.num_peers = num_peers;
    sceKernelMemcpy(g_gm_ctx.peers, macs, num_peers * 6);
    sceKernelMemcpy(g_gm_ctx.gm_param, param, 0x50);
    *(u16 *)(g_gm_ctx.gm_param + 0x2e) = 0x10;
    sceNetAdhocAuth_lib_312BD812(g_gm_ctx.gm_param + 0x30, 0x10);
    g_gm_ctx.min_peers = min_peers;
    g_gm_ctx.timeout = timeout;
    g_gm_ctx.flag = flag;
    sceKernelSetEventFlag(g_ctx.evfid, 0x10);
    return 0;
}

/* 0x31f0 */
static int sub_31f0(const char *name, const u8 *gc_mac, const void *param, u32 timeout, int flag)
{
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    if (name != NULL)
        sceKernelMemcpy(g_ctx.group_name, name, 8);
    else
        sceKernelMemset(g_ctx.group_name, 0, 8);
    g_gm_ctx.num_peers = 0;
    sceKernelMemcpy(g_gm_ctx.peers[0], gc_mac, 6);
    sceKernelMemcpy(g_gm_ctx.gm_param, param, 0x50);
    g_gm_ctx.flag = flag;
    g_gm_ctx.timeout = timeout;
    sceKernelSetEventFlag(g_ctx.evfid, 0x20);
    return 0;
}

/* 0x32e0 */
static int sub_32e0(void)
{
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    sceKernelMemset(g_ctx.scan_buf, 0, 0x780);
    sceKernelSetEventFlag(g_ctx.evfid, 4);
    return 0;
}

/* 0x3344 */
static int sub_3344(void)
{
    if (g_ctx.state == 3)
        return 0x80410b0b;
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    sceKernelSetEventFlag(g_ctx.evfid, 2);
    return 0;
}

/* 0x33a4 */
static int sub_33a4(void)
{
    if (g_ctx.state == 1)
        return 0x80410b0b;
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    sceKernelSetEventFlag(g_ctx.evfid, 0x40);
    return 0;
}

/* 0x3404 */
static int sub_3404(int id)
{
    int lock_res = sub_5c88();
    if (lock_res < 0)
        return lock_res;
    int res = 0;
    if (g_handlers[id].func == NULL)
        res = 0x80410b06;
    else
        g_handlers[id].func = NULL;
    sub_5cb0(lock_res);
    return res;
}

/* 0x3470 */
static int sub_3470(int *state)
{
    *state = g_ctx.state;
    return 0;
}

/* 0x3484 */
static int sub_3484(void *adhoc_id)
{
    sceKernelMemcpy(adhoc_id, &g_ctx.adhoc_type, 0x10);
    return 0;
}

/* 0x34b0 */
static int sub_34b0(void *param)
{
    u8 bssid[0x60];
    if (g_ctx.state == 0)
        return 0x80410b09;
    int res = sceNet_lib_A55C914F("wlan", bssid);
    if (res < 0)
        return res;
    sceKernelMemcpy((u8 *)param + 0x8c, bssid, 6);
    *(int *)param = g_ctx.channel;
    sceKernelMemcpy((u8 *)param + 4, g_ctx.group_name, 8);
    return sceUtilityGetSystemParamString(1, (char *)param + 0x0c, 0x80);
}

/* 0x3544 */
static int sub_3544(void *info)
{
    if (g_ctx.state != 3)
        return 0x80410b0c;
    *(int *)info = g_gm_ctx.num_peers;
    sceKernelMemcpy((u8 *)info + 4, g_gm_ctx.peers, g_gm_ctx.num_peers * 6);
    return 0;
}

/* 0x35b8 */
static int sub_35b8(int id)
{
    int lock_res = sub_5c88();
    if (lock_res < 0)
        return lock_res;
    int res = 0;
    if (g_lib_handlers[id].func == NULL)
        res = 0x80410b06;
    else
        g_lib_handlers[id].func = NULL;
    sub_5cb0(lock_res);
    return res;
}

/* 0x3624 */
static int sub_3624(void *param)
{
    if (g_ctx.state != 3)
        return 0x80410b0c;
    sceKernelMemcpy(param, g_gm_ctx.gm_param, 0x50);
    return 0;
}

/* 0x3680 */
static int sub_3680(const char *name)
{
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    sceKernelMemset(g_ctx.group_name, 0, 8);
    if (name != NULL)
        sceNet_lib_B5CE388A(g_ctx.group_name, name, 8);
    sceKernelSetEventFlag(g_ctx.evfid, 0x800);
    return 0;
}

/* 0x3714 */
static int sub_3714(void)
{
    return scePowerRequestSuspend();
}

/* 0x3730 */
static int sub_3730(const char *name)
{
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    sceKernelMemset(g_ctx.group_name, 0, 8);
    if (name != NULL)
        sceNet_lib_B5CE388A(g_ctx.group_name, name, 8);
    sceKernelSetEventFlag(g_ctx.evfid, 0x400);
    return 0;
}

/* 0x37c4 */
static int sub_37c4(u32 arglen, void *argp)
{
    (void)arglen;
    struct SceNetAdhocctlCtx *ctx = *(struct SceNetAdhocctlCtx **)argp;
    while (1) {
        u32 bits = 0;
        sceKernelWaitEventFlag(ctx->evfid, 0xfff, 1, &bits, NULL);
        if (bits & 0x200) {
            sceKernelClearEventFlag(ctx->evfid, ~0x200u);
            if (ctx->state == 1)
                sub_22b8(ctx);
            else if (ctx->state == 3)
                sub_5580(ctx, &g_gm_ctx);
            return 0;
        }
        if (bits & 8) {
            sceKernelClearEventFlag(ctx->evfid, ~8u);
            if (ctx->state == 1 || ctx->state == 4 || ctx->state == 5) {
                int res = sub_3d0c(ctx);
                if (res < 0) {
                    sub_22b8(ctx);
                    sub_2600(0, res);
                    ctx->flags &= ~1u;
                    continue;
                }
                if (ctx->state == 4) {
                    sub_233c(ctx);
                    sub_2600(6, 0);
                } else if (ctx->state == 5) {
                    sub_233c(ctx);
                    int ev = 7;
                    if (ctx->flags & 0x10) {
                        ctx->flags &= ~0x10u;
                        ev = 8;
                    }
                    sub_2600(ev, 0);
                }
                if (ctx->flags & 0x10)
                    ctx->flags &= ~0x10u;
            } else if (ctx->state == 3) {
                int res = sub_3d0c(ctx);
                if (res < 0) {
                    sub_5580(ctx, &g_gm_ctx);
                    sub_2600(0, res);
                    ctx->flags &= ~1u;
                    continue;
                }
            }
        }
        if (bits & 1) {
            sceKernelClearEventFlag(ctx->evfid, ~1u);
            int res = sub_12c0(ctx);
            if (res < 0)
                goto err_out;
            sub_2600(1, 0);
        }
        if (bits & 2) {
            sceKernelClearEventFlag(ctx->evfid, ~2u);
            sub_22b8(ctx);
            sub_2600(2, 0);
        }
        if (bits & 4) {
            sceKernelClearEventFlag(ctx->evfid, ~4u);
            int res = sub_1534(ctx);
            if (res < 0)
                goto err_out;
            sub_2600(3, 0);
        }
        if (bits & 0x10) {
            sceKernelClearEventFlag(ctx->evfid, ~0x10u);
            int res = sub_3f34(ctx, &g_gm_ctx);
            if (res < 0)
                goto err_out;
            sub_2600(4, 0);
        }
        if (bits & 0x20) {
            sceKernelClearEventFlag(ctx->evfid, ~0x20u);
            int res = sub_46e0(ctx, &g_gm_ctx);
            if (res < 0)
                goto err_out;
            sub_2600(4, 0);
        }
        if (bits & 0x40) {
            sceKernelClearEventFlag(ctx->evfid, ~0x40u);
            sub_5580(ctx, &g_gm_ctx);
            sub_2600(2, 0);
        }
        if (bits & 0x80) {
            sceKernelClearEventFlag(ctx->evfid, ~0x80u);
            int res = sub_18a8(ctx);
            if (res < 0)
                goto err_out;
            sub_2600(1, 0);
        }
        if (bits & 0x100) {
            sceKernelClearEventFlag(ctx->evfid, ~0x100u);
            int res = sub_1b9c(ctx);
            if (res < 0)
                goto err_out;
            sub_2600(1, 0);
        }
        if (bits & 0x400) {
            sceKernelClearEventFlag(ctx->evfid, ~0x400u);
            int res = sub_1ec8(ctx);
            if (res < 0)
                goto err_out;
            sub_2d54(5, 1);
        }
        if (bits & 0x800) {
            sceKernelClearEventFlag(ctx->evfid, ~0x800u);
            int res = sub_20ac(ctx);
            if (res < 0) {
            err_out:;
                int poll_res = sub_3d2c(ctx);
                if (poll_res < 0)
                    res = poll_res;
                sub_2600(0, res);
                ctx->flags &= ~1u;
                continue;
            }
            sub_2d54(5, 1);
        }
        ctx->flags &= ~1u;
    }
}

/* 0x3bf4 */
static int sub_3bf4(struct SceNetAdhocctlCtx *ctx)
{
    int ev, reason;
    while (1) {
        int res = sceNet_lib_D1BE2CE9("wlan", &ev, &reason);
        if (res == (int)0x80410184) {
            u32 out;
            res = sceKernelWaitEventFlag(ctx->evfid, 8, 1, &out, NULL);
            if (res < 0)
                return res;
            sceKernelClearEventFlag(ctx->evfid, ~8u);
            continue;
        }
        if (res < 0)
            return res;
        if (ev == 8) {
            if (ctx->state == 5)
                ctx->flags |= 0x10;
            return 0;
        }
        if (ev == 9)
            return 0;
    }
}

/* 0x3d0c */
static int sub_3d0c(struct SceNetAdhocctlCtx *ctx)
{
    int ev, reason;
    return sub_3db4(ctx, &ev, &reason);
}

/* 0x3d2c */
static int sub_3d2c(struct SceNetAdhocctlCtx *ctx)
{
    u32 out;
    int res = sceKernelPollEventFlag(ctx->evfid, 8, 1, &out);
    if (res == (int)0x800201af)
        return 0;
    if (res < 0)
        return res;
    if (out & 8) {
        sceKernelClearEventFlag(ctx->evfid, ~8u);
        int ev, reason;
        res = sub_3db4(ctx, &ev, &reason);
    }
    return res;
}

/* 0x3db4 */
static int sub_3db4(struct SceNetAdhocctlCtx *ctx, int *ev, int *reason)
{
    int ret = 0;
    while (1) {
        int res = sceNet_lib_D1BE2CE9("wlan", ev, reason);
        if (res == (int)0x80410184)
            break;
        if (res < 0) {
            ret = res;
            break;
        }
        if (*ev == 5) {
            if (!(ctx->flags & 2)) {
                ctx->flags |= 2;
                ret = 0x80410b0e;
            }
        } else if (*ev < 6) {
            if (*ev == 4) {
                if (ctx->state == 1)
                    ctx->pending_error = 0x80410b83;
                ret = 0x80410b03;
            }
        } else if (*ev == 7) {
            if (ctx->state == 1) {
                sceNetAdhocAuth_lib_2E6AA271();
                ctx->pending_error = 0x80410b84;
            }
            sub_2d54(5, 0);
            res = sub_3bf4(ctx);
            if (res < 0)
                ret = res;
            else if (ctx->state != 5)
                ret = 0x80410b0f;
        }
    }
    if (sceKernelPollEventFlag(ctx->evfid, 0x42, 1, NULL) == 0)
        ret = 0;
    return ret;
}

/* 0x3f34 */
static int sub_3f34(struct SceNetAdhocctlCtx *ctx, struct SceNetAdhocctlGameModeCtx *gm)
{
    if (ctx->state != 0)
        return 0x80410b02;
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    s64 start_time = sceKernelGetSystemTimeWide();
    g_saved_power_save = -1;
    u32 rem_timeout = 0;

    while (1) {
        if (gm->timeout != 0) {
            s64 elapsed = sceKernelGetSystemTimeWide() - start_time;
            if ((u64)elapsed >= (u64)gm->timeout) {
                int res = 0x80410b05;
                goto fail_all;
            }
            rem_timeout = gm->timeout - (u32)elapsed;
        } else {
            rem_timeout = 0;
        }
        int res = sceWlanDevAttach();
        if (res == 0 || res == (int)0x80410d0f)
            break;
        if (res < 0 && res != (int)0x80410d0e)
            return res;
        sceKernelDelayThread(1000000);
    }

    ctx->flags &= ~2u;
    int res = sceNet_lib_5216CBF5("wlan");
    if (res < 0)
        goto fail_all;
    res = sceNet_lib_AB7DD9A5("wlan", ctx->evfid, 8);
    if (res < 0)
        goto fail_all;
    if (sceWlanGetSwitchState() == 0) {
        res = 0x80410b03;
        goto fail_all;
    }

    if (!(gm->flag & 1)) {
        char ssid[0x30];
        sceKernelMemset(ssid, 0, 0x21);
        res = sub_25b0(ctx, ssid);
        if (res < 0)
            goto fail_all;
        gm->gm_param[0x0c] = (u8)res;
        sceKernelMemcpy(gm->gm_param + 0x0e, ssid, res & 0xff);
        u8 ch, offset;
        sub_50fc(gm, &ch, &offset);
        res = sub_3d2c(ctx);
        if ((((u32)res >> 16) & 0xfff) == 0x41)
            goto done_check;
        if (res < 0)
            goto fail_all;
        gm->gm_param[0x46] = offset;
        gm->gm_param[0x0d] = ch;
    }

    if (gm->flag & 2) {
        res = sub_550c(4);
        if (res < 0)
            goto fail_all;
        g_saved_power_save = res;
        res = sceWlanDrv_lib_5BAA1FE5(1);
        if (res < 0)
            goto fail_all;
    }

    u8 param[0x70];
    sceKernelMemset(param, 0, 0x70);
    u8 ssid_len = gm->gm_param[0x0c];
    param[7] = ssid_len;
    sceKernelMemcpy(param + 8, gm->gm_param + 0x0e, ssid_len);
    param[6] = gm->gm_param[0x0d];
    *(u32 *)(param + 0x28) = 2;
    *(u32 *)(param + 0x2c) = *(u16 *)(gm->gm_param + 0x4e);
    *(u16 *)(param + 0x3e) = 0x22;
    int err = 0;
    sceNet_lib_03164B12("wlan", param, &err);
    res = sub_3d2c(ctx);
    if ((((u32)res >> 16) & 0xfff) == 0x41)
        goto done_check;
    if (res < 0)
        goto fail_all;

    res = sub_17b4(ctx);
    if (res < 0)
        goto fail_all;
    res = sub_3d2c(ctx);
    if (res < 0)
        goto fail_all;

    char nickname[0x80];
    res = sceUtilityGetSystemParamString(1, nickname, 0x80);
    if (res < 0)
        goto fail_all;
    res = sub_3d2c(ctx);
    if (res < 0)
        goto fail_all;

    u32 auth_opt[5];
    auth_opt[0] = 1000000;
    auth_opt[1] = 500000;
    auth_opt[2] = 5;
    auth_opt[3] = 30000000;
    auth_opt[4] = 300000000;
    res = sceNetAdhocAuth_lib_89F2A732("wlan", 0x30, 0x2000, auth_opt, 16, nickname);
    if (res < 0)
        goto fail_all;

    u16 key_len = *(u16 *)(gm->gm_param + 0x2e);
    int payload_len = key_len + gm->num_peers * 6 + 19;
    int total_len = key_len + gm->num_peers * 6 + 25;
    g_ptp_buf[0] = 1;
    g_ptp_buf[1] = 1;
    u32 be_len = __builtin_bswap32(payload_len);
    sceKernelMemcpy(g_ptp_buf + 2, &be_len, 4);

    u8 *p = g_ptp_buf + 6;
    p[0] = 1;
    u16 be_klen = __builtin_bswap16(key_len);
    sceKernelMemcpy(p + 1, &be_klen, 2);
    if (key_len != 0)
        sceKernelMemcpy(p + 3, gm->gm_param + 0x30, key_len);
    p += 3 + key_len;

    p[0] = 2;
    u16 be_plen = __builtin_bswap16((u16)(gm->num_peers * 6));
    sceKernelMemcpy(p + 1, &be_plen, 2);
    int i;
    for (i = 0; i < gm->num_peers; i++)
        sceKernelMemcpy(p + 3 + i * 6, gm->peers[i], 6);
    p += 3 + gm->num_peers * 6;

    p[0] = 3;
    u16 be_two = __builtin_bswap16(2);
    sceKernelMemcpy(p + 1, &be_two, 2);
    u16 be_a4 = __builtin_bswap16(*(u16 *)(gm->gm_param + 0x40));
    sceKernelMemcpy(p + 3, &be_a4, 2);

    p[5] = 4;
    u16 be_one = __builtin_bswap16(1);
    sceKernelMemcpy(p + 6, &be_one, 2);
    p[8] = gm->gm_param[0x46];

    p[9] = 5;
    sceKernelMemcpy(p + 10, &be_one, 2);
    p[12] = gm->gm_param[0x47];

    int retry_cnt = (int)(gm->timeout / 200000) + 1;
    int listen_id = sceNetAdhocPtpListen(gm->peers[0], 0x8001, 0x2000, 200000, retry_cnt, gm->num_peers - 1, 0);
    if (listen_id < 0) {
        res = listen_id;
        goto fail_all;
    }

    res = sub_5658(gm, 0, (u32)start_time, (u32)(start_time >> 32), listen_id, g_ptp_buf, total_len);
    if (res < 0) {
        sceNetAdhocPtpClose(listen_id, 0);
        goto done_check;
    }
    res = sceNetAdhocPtpClose(listen_id, 0);
    if (res < 0)
        goto fail_all;

    sceNetAdhocAuth_lib_6CE209A3();
    u16 wparam[8];
    res = sceWlanDrv_lib_56F467CA(wparam);
    if (res < 0)
        goto fail_all;
    u16 bp = *(u16 *)(gm->gm_param + 0x4e);
    wparam[0] = bp - 2;
    wparam[1] = 0xff;
    wparam[2] = (1000 / (int)bp) + 1;
    res = sceWlanDrv_lib_2D0FAE4E(wparam);
    if (res < 0)
        goto fail_all;
    res = sub_3d2c(ctx);
    if (res < 0)
        goto fail_all;

    sceKernelMemcpy(gm->gm_param + 6, gm->peers[0], 6);
    gm->gm_param[6] |= 3;
    *(u16 *)(gm->gm_param + 0x4a) = 1;
    gm->gm_param[0x44] = (u8)gm->num_peers;
    gm->gm_param[0x43] = 0;
    sceNetAdhocAuth_lib_72AAC6D3(*(u16 *)(gm->gm_param + 0x2e), gm->gm_param + 0x30);
    u16 saved_klen = *(u16 *)(gm->gm_param + 0x2e);
    *(u16 *)(gm->gm_param + 0x2e) = 0;
    err = 0;
    sceNet_lib_0D633F53("wlan", gm->gm_param, gm->peers[0], &err);
    *(u16 *)(gm->gm_param + 0x2e) = saved_klen;
    res = sub_3d2c(ctx);
    if ((((u32)res >> 16) & 0xfff) == 0x41)
        goto done_check;
    if (res < 0)
        goto fail_all;

    ctx->state = 3;
    ctx->channel = gm->gm_param[0x0d];

done_check:
    if (res >= 0)
        return res;

fail_all:
    sceNetAdhocAuth_lib_72AAC6D3(0, NULL);
    sceNetAdhocAuth_lib_2E6AA271();
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    err = 0;
    sceNet_lib_DA02F383("wlan", &err);
    if (gm->flag & 2) {
        sceWlanDrv_lib_5BAA1FE5(0);
        if (g_saved_power_save >= 0)
            sub_550c(g_saved_power_save);
    }
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    return res;
}

/* 0x46e0 */
static int sub_46e0(struct SceNetAdhocctlCtx *ctx, struct SceNetAdhocctlGameModeCtx *gm)
{
    int ptp_id = -1;
    if (ctx->state != 0)
        return 0x80410b02;
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    s64 start_time = sceKernelGetSystemTimeWide();
    g_saved_power_save = -1;
    u32 rem_timeout = 0;

    while (1) {
        if (gm->timeout != 0) {
            s64 elapsed = sceKernelGetSystemTimeWide() - start_time;
            if ((u64)elapsed >= (u64)gm->timeout) {
                int res = 0x80410b05;
                goto fail_ptp;
            }
            rem_timeout = gm->timeout - (u32)elapsed;
        } else {
            rem_timeout = 0;
        }
        int res = sceWlanDevAttach();
        if (res == 0 || res == (int)0x80410d0f)
            break;
        if (res < 0 && res != (int)0x80410d0e)
            return res;
        sceKernelDelayThread(1000000);
    }

    ctx->flags &= ~2u;
    int res = sceNet_lib_5216CBF5("wlan");
    if (res < 0)
        goto fail_all;
    res = sceNet_lib_AB7DD9A5("wlan", ctx->evfid, 8);
    if (res < 0)
        goto fail_all;
    if (sceWlanGetSwitchState() == 0) {
        res = 0x80410b03;
        goto fail_ptp;
    }

    if (!(gm->flag & 1)) {
        char ssid[0x30];
        sceKernelMemset(ssid, 0, 0x21);
        res = sub_25b0(ctx, ssid);
        if (res < 0)
            goto fail_all;
        gm->gm_param[0x0c] = (u8)res;
        sceKernelMemcpy(gm->gm_param + 0x0e, ssid, res & 0xff);
    }

    if (gm->flag & 2) {
        res = sub_550c(4);
        if (res < 0)
            goto fail_all;
        g_saved_power_save = res;
        res = sceWlanDrv_lib_5BAA1FE5(1);
        if (res < 0)
            goto fail_all;
    }

    u8 scan_req[0x4c];
    sceKernelMemset(scan_req, 0, 0x4c);
    sceKernelMemcpy(scan_req + 0x1c, gm->gm_param + 0x0e, gm->gm_param[0x0c]);
    scan_req[0x18] = gm->gm_param[0x0c];
    *(u32 *)(scan_req + 0x00) = 2;
    sceKernelMemcpy(scan_req + 0x0a, g_channels, 3);
    *(u32 *)(scan_req + 0x3c) = 1;
    *(u32 *)(scan_req + 0x40) = 0;
    *(u32 *)(scan_req + 0x44) = 6;
    *(u32 *)(scan_req + 0x48) = 100;

    struct SceNetAdhocctlScanNode *found = NULL;
    while (1) {
        sceKernelMemset(g_gm_scan_buf, 0, 0xc00);
        int buflen = 0xc00;
        int err = 0;
        sceNet_lib_7BA3ED91("wlan", scan_req, &buflen, g_gm_scan_buf, &err);
        res = sub_3d2c(ctx);
        if ((((u32)res >> 16) & 0xfff) == 0x41)
            goto done_check;
        if (res < 0)
            goto fail_all;

        if (gm->timeout != 0) {
            s64 elapsed = sceKernelGetSystemTimeWide() - start_time;
            if ((u64)elapsed >= (u64)gm->timeout) {
                res = 0x80410b05;
                goto fail_all;
            }
            rem_timeout = gm->timeout - (u32)elapsed;
        } else {
            rem_timeout = 0;
        }

        if (buflen == 0)
            continue;

        struct SceNetAdhocctlScanNode *node = (struct SceNetAdhocctlScanNode *)g_gm_scan_buf;
        while (node != NULL) {
            if (sceNet_lib_E0A81C7C(node->ssid, gm->gm_param + 0x0e, 0x20) == 0 && node->mode == 2) {
                found = node;
                break;
            }
            node = node->next;
        }
        if (found != NULL)
            break;
    }

    u8 join_param[0x70];
    sceKernelMemset(join_param, 0, 0x70);
    sceKernelMemcpy(join_param, (u8 *)found + 4, 0x5c);
    *(u32 *)(join_param + 0x6c) = 1;
    int err = 0;
    sceNet_lib_389728AB("wlan", join_param, &err);
    res = sub_3d2c(ctx);
    if ((((u32)res >> 16) & 0xfff) == 0x41)
        goto done_check;
    if (res < 0)
        goto fail_all;

    char nickname[0x80];
    res = sceUtilityGetSystemParamString(1, nickname, 0x80);
    if (res < 0)
        goto fail_all;
    res = sub_3d2c(ctx);
    if (res < 0)
        goto fail_all;

    u32 auth_opt[5];
    auth_opt[0] = (u32)-1;
    auth_opt[1] = 500000;
    auth_opt[2] = 5;
    auth_opt[3] = 30000000;
    auth_opt[4] = 300000000;
    res = sceNetAdhocAuth_lib_89F2A732("wlan", 0x30, 0x2000, auth_opt, 16, nickname);
    if (res < 0)
        goto fail_all;

    u8 local_mac[16];
    res = sceNetGetLocalEtherAddr(local_mac);
    if (res < 0)
        goto fail_all;

    while (1) {
        int retry_cnt = (int)(gm->timeout / 200000) + 1;
        ptp_id = sceNetAdhocPtpOpen(local_mac, 0x8001, gm->peers[0], 0x8001, 0x2000, 200000, retry_cnt, 0);
        if (ptp_id < 0) {
            res = ptp_id;
            goto fail_all;
        }
        if (gm->timeout != 0) {
            s64 elapsed = sceKernelGetSystemTimeWide() - start_time;
            if ((u64)elapsed >= (u64)gm->timeout) {
                res = 0x80410b05;
                goto fail_ptp;
            }
            rem_timeout = gm->timeout - (u32)elapsed;
        } else {
            rem_timeout = 0;
        }
        res = sceNetAdhocPtpConnect(ptp_id, rem_timeout, 0);
        if (res >= 0)
            break;
        if (res != (int)0x80410718)
            goto done_check;
        sceNetAdhocPtpClose(ptp_id, 0);
        sceKernelDelayThread(1000000);
    }

    res = sub_5a18(gm, 0, (u32)start_time, (u32)(start_time >> 32), ptp_id, g_ptp_buf, 6);
    if (res < 0)
        goto fail_ptp;
    if ((s8)g_ptp_buf[0] != 1 || g_ptp_buf[1] != g_ptp_buf[0]) {
        res = 0x80410b82;
        goto fail_ptp;
    }

    u32 body_len;
    sceKernelMemcpy(&body_len, g_ptp_buf + 2, 4);
    body_len = __builtin_bswap32(body_len);
    if ((int)body_len >= 0x3fb) {
        res = 0x80410b83;
        goto fail_ptp;
    }

    res = sub_5a18(gm, 0, (u32)start_time, (u32)(start_time >> 32), ptp_id, g_ptp_buf + 6, body_len);
    if (res < 0)
        goto fail_ptp;

    res = sceNetAdhocPtpClose(ptp_id, 0);
    if (res < 0)
        goto fail_all;
    ptp_id = -1;

    int tag_len;
    void *val = sub_5b28(1, g_ptp_buf + 6, body_len, &tag_len);
    if (val == NULL || tag_len >= 17) {
        res = 0x80410b84;
        goto fail_ptp;
    }
    u16 klen = (u16)tag_len;
    *(u16 *)(gm->gm_param + 0x2e) = klen;
    if (klen != 0)
        sceKernelMemcpy(gm->gm_param + 0x30, val, klen);

    val = sub_5b28(2, g_ptp_buf + 6, body_len, &tag_len);
    if (val == NULL || tag_len < 12 || tag_len >= 97 || (tag_len % 6) != 0) {
        res = 0x80410b85;
        goto fail_ptp;
    }
    gm->num_peers = tag_len / 6;
    int i;
    for (i = 0; i < gm->num_peers; i++)
        sceKernelMemcpy(gm->peers[i], (u8 *)val + i * 6, 6);

    val = sub_5b28(3, g_ptp_buf + 6, body_len, &tag_len);
    if (val == NULL || tag_len != 2) {
        res = 0x80410b86;
        goto fail_ptp;
    }
    sceKernelMemcpy(gm->gm_param + 0x40, val, 2);
    *(u16 *)(gm->gm_param + 0x40) = __builtin_bswap16(*(u16 *)(gm->gm_param + 0x40));

    val = sub_5b28(4, g_ptp_buf + 6, body_len, &tag_len);
    if (val == NULL || tag_len != 1) {
        res = 0x80410b87;
        goto fail_ptp;
    }
    gm->gm_param[0x46] = *(u8 *)val;

    val = sub_5b28(5, g_ptp_buf + 6, body_len, &tag_len);
    if (val == NULL || tag_len != 1) {
        res = 0x80410b88;
        goto fail_ptp;
    }
    gm->gm_param[0x47] = *(u8 *)val;

    sceNetAdhocAuth_lib_6CE209A3();
    u16 wparam[8];
    res = sceWlanDrv_lib_56F467CA(wparam);
    if (res < 0)
        goto fail_all;
    u16 bp = found->beacon_period;
    wparam[0] = bp - 2;
    wparam[1] = 0xff;
    wparam[2] = (1000 / (int)*(u32 *)&found->beacon_period) + 1;
    res = sceWlanDrv_lib_2D0FAE4E(wparam);
    if (res < 0)
        goto fail_all;
    res = sub_3d2c(ctx);
    if (res < 0)
        goto fail_all;

    sceKernelMemcpy(gm->gm_param + 6, gm->peers[0], 6);
    gm->gm_param[6] |= 3;
    *(u16 *)(gm->gm_param + 0x4e) = found->beacon_period;
    int self_idx = 1;
    for (self_idx = 1; self_idx < gm->num_peers; self_idx++) {
        if (sceNet_lib_E0A81C7C(gm->peers[self_idx], local_mac, 6) == 0)
            break;
    }
    *(u16 *)(gm->gm_param + 0x4a) = 1;
    gm->gm_param[0x43] = (u8)self_idx;
    gm->gm_param[0x44] = (u8)gm->num_peers;
    sceNetAdhocAuth_lib_72AAC6D3(*(u16 *)(gm->gm_param + 0x2e), gm->gm_param + 0x30);
    u16 saved_klen = *(u16 *)(gm->gm_param + 0x2e);
    *(u16 *)(gm->gm_param + 0x2e) = 0;
    err = 0;
    sceNet_lib_0D633F53("wlan", gm->gm_param, gm->peers[0], &err);
    *(u16 *)(gm->gm_param + 0x2e) = saved_klen;
    res = sub_3d2c(ctx);
    if ((((u32)res >> 16) & 0xfff) == 0x41)
        goto done_check;
    if (res < 0)
        goto fail_all;

    ctx->state = 3;
    ctx->channel = found->channel;

done_check:
    if (res >= 0)
        return res;

fail_ptp:
    if (ptp_id >= 0)
        sceNetAdhocPtpClose(ptp_id, 0);

fail_all:
    sceNetAdhocAuth_lib_72AAC6D3(0, NULL);
    sceNetAdhocAuth_lib_2E6AA271();
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    err = 0;
    sceNet_lib_DA02F383("wlan", &err);
    if (gm->flag & 2) {
        sceWlanDrv_lib_5BAA1FE5(0);
        if (g_saved_power_save >= 0)
            sub_550c(g_saved_power_save);
    }
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    return res;
}

/* 0x50fc */
static int sub_50fc(struct SceNetAdhocctlGameModeCtx *gm, u8 *ch_out, u8 *offset_out)
{
    u8 scan_req[0x4c];
    u32 mask = 0;
    sceKernelMemset(scan_req, 0, 0x4c);
    *(u32 *)(scan_req + 0x00) = 2;
    sceKernelMemcpy(scan_req + 0x0a, g_channels, 3);
    *(u32 *)(scan_req + 0x3c) = 1;
    *(u32 *)(scan_req + 0x40) = 0;
    *(u32 *)(scan_req + 0x44) = 6;
    *(u32 *)(scan_req + 0x48) = 500;
    sceKernelMemset(g_gm_scan_buf, 0, 0xc00);
    int buflen = 0xc00;
    int err = 0;
    int res = sceNet_lib_7BA3ED91("wlan", scan_req, &buflen, g_gm_scan_buf, &err);
    if (res < 0)
        return res;

    sceKernelMemset(g_ch_load, 0, sizeof(g_ch_load));
    if (buflen != 0) {
        struct SceNetAdhocctlScanNode *node = (struct SceNetAdhocctlScanNode *)g_gm_scan_buf;
        while (node != NULL) {
            if (node->mode == 2 && ((u8 *)node)[0x4e] == 2) {
                int cidx;
                for (cidx = 0; cidx < 3; cidx++) {
                    if (node->channel == g_channels[cidx])
                        break;
                }
                if (cidx != 3) {
                    int start_off = ((u8 *)node)[0x51];
                    int n = ((u8 *)node)[0x50];
                    int step = ((u8 *)node)[0x52];
                    if (start_off >= 15 && start_off + step * (n - 1) < 256) {
                        int k;
                        for (k = 0; k < n; k++) {
                            int slot = (start_off + step * k - 15) / 8;
                            g_ch_load[cidx].bitmap |= (1u << slot);
                        }
                        g_ch_load[cidx].count += n;
                    }
                }
            }
            node = node->next;
        }
    }

    int sys_ch = 0;
    res = sceUtilityGetSystemParamInt(2, &sys_ch);
    if (res < 0)
        return res;
    if (sys_ch != 6 && sys_ch != 11) {
        if (sys_ch != 1)
            sys_ch = 0;
    }

    int i;
    for (i = 0; i < gm->num_peers; i++)
        mask |= (1u << i);

    while (1) {
        int best_idx = -1;
        int best_cnt = 0;
        for (i = 0; i < 3; i++) {
            if (g_ch_load[i].skip == 1)
                continue;
            if (sys_ch == 0 || g_channels[i] == sys_ch) {
                if (best_idx == -1 || g_ch_load[i].count < best_cnt) {
                    best_idx = i;
                    best_cnt = g_ch_load[i].count;
                }
            }
        }
        if (best_idx == -1)
            return 0x80410b0d;
        if (g_ch_load[best_idx].count >= 8 || g_ch_load[best_idx].count + gm->num_peers >= 17)
            return 0x80410b0d;

        int max_shift = 29 - gm->num_peers;
        int shift = 0;
        if (max_shift >= 0) {
            for (shift = 0; shift <= max_shift; shift++) {
                if ((g_ch_load[best_idx].bitmap & (mask << shift)) == 0)
                    break;
            }
        }
        if (shift > 29 - gm->num_peers) {
            g_ch_load[best_idx].skip = 1;
            continue;
        }
        *ch_out = g_channels[best_idx];
        *offset_out = (u8)(shift * 8 + 15);
        return res;
    }
}

/* 0x550c */
static int sub_550c(int power_save)
{
    u8 buf[0x18];
    sceKernelMemset(buf, 0, 0x18);
    int res = sceNet_lib_B20F84F8("wlan", buf);
    if (res < 0)
        return res;
    int old = buf[0x11];
    buf[0x11] = (u8)power_save;
    res = sceNet_lib_AFA11338("wlan", buf);
    return (res < 0) ? res : old;
}

/* 0x5580 */
static void sub_5580(struct SceNetAdhocctlCtx *ctx, struct SceNetAdhocctlGameModeCtx *gm)
{
    int err = 0;
    sceNet_lib_DA02F383("wlan", &err);
    sceNetAdhocAuth_lib_72AAC6D3(0, NULL);
    sceNetAdhocAuth_lib_2E6AA271();
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    err = 0;
    sceNet_lib_DA02F383("wlan", &err);
    if (gm->flag & 2) {
        sceWlanDrv_lib_5BAA1FE5(0);
        if (g_saved_power_save >= 0)
            sub_550c(g_saved_power_save);
    }
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    ctx->state = 0;
    ctx->pending_error = 0;
}

/* 0x5658 */
static int sub_5658(struct SceNetAdhocctlGameModeCtx *gm, int unused, u32 start_lo, u32 start_hi, int listen_id, const void *ie_buf, int ie_len)
{
    (void)unused;
    u8 accepted_macs[15][6];
    int accepted_cnt = 0;
    int res = 0;
    int client_id = -1;
    s64 start_time = ((s64)start_hi << 32) | start_lo;

    sceKernelMemset(accepted_macs, 0, 0x5a);
    if (gm->num_peers - 1 <= 0)
        goto done;

    u8 *next_mac_slot = (u8 *)accepted_macs;
    while (accepted_cnt < gm->num_peers - 1) {
        u32 rem = 0;
        if (gm->timeout != 0) {
            s64 elapsed = sceKernelGetSystemTimeWide() - start_time;
            if ((u64)elapsed >= (u64)gm->timeout) {
                res = 0x80410b05;
                goto done;
            }
            rem = gm->timeout - (u32)elapsed;
        }
        u8 peer_mac[6];
        u16 peer_port;
        res = sceNetAdhocPtpAccept(listen_id, peer_mac, &peer_port, rem, 0);
        if (res < 0)
            goto done;
        client_id = res;
        if (peer_port != 0x8001) {
            sceNetAdhocPtpClose(client_id, 0);
            client_id = -1;
            continue;
        }
        int idx;
        for (idx = 1; idx < gm->num_peers; idx++) {
            if (sceNet_lib_E0A81C7C(gm->peers[idx], peer_mac, 6) == 0)
                break;
        }
        if (idx == gm->num_peers) {
            sceNetAdhocPtpClose(client_id, 0);
            client_id = -1;
            continue;
        }
        int dup = 0;
        int j;
        for (j = 0; j < accepted_cnt; j++) {
            if (sceNet_lib_E0A81C7C(accepted_macs[j], peer_mac, 6) == 0) {
                sceNetAdhocPtpClose(client_id, 0);
                client_id = -1;
                dup = 1;
            }
        }
        if (dup)
            continue;

        rem = 0;
        if (gm->timeout != 0) {
            s64 elapsed = sceKernelGetSystemTimeWide() - start_time;
            if ((u64)elapsed >= (u64)gm->timeout) {
                res = 0x80410b05;
                goto close_client;
            }
            rem = gm->timeout - (u32)elapsed;
        }
        int slen = ie_len;
        res = sceNetAdhocPtpSend(client_id, ie_buf, &slen, rem, 0);
        if (res < 0)
            goto close_client;

        rem = 0;
        if (gm->timeout != 0) {
            s64 elapsed = sceKernelGetSystemTimeWide() - start_time;
            if ((u64)elapsed >= (u64)gm->timeout) {
                res = 0x80410b05;
                goto close_client;
            }
            rem = gm->timeout - (u32)elapsed;
        }
        if (rem > 3000000)
            rem = 3000000;
        res = sceNetAdhocPtpFlush(client_id, rem, 0);
        if (res < 0) {
            if (rem != 3000000 || (res != (int)0x80410715 && res != (int)0x8041070c))
                goto close_client;
        }
        res = sceNetAdhocPtpClose(client_id, 0);
        if (res < 0)
            goto done;
        client_id = -1;
        sceKernelMemcpy(next_mac_slot, peer_mac, 6);
        next_mac_slot += 6;
        accepted_cnt++;
    }

close_client:
    if (client_id >= 0)
        sceNetAdhocPtpClose(client_id, 0);

done:
    if (accepted_cnt + 1 >= gm->min_peers && res == (int)0x80410715)
        res = 0;
    return res;
}

/* 0x5a18 */
static int sub_5a18(struct SceNetAdhocctlGameModeCtx *gm, int unused, u32 start_lo, u32 start_hi, int ptp_id, void *buf, int len)
{
    (void)unused;
    int res = 0;
    u8 *p = (u8 *)buf;
    s64 start_time = ((s64)start_hi << 32) | start_lo;

    while (len > 0) {
        u32 rem = 0;
        if (gm->timeout != 0) {
            s64 elapsed = sceKernelGetSystemTimeWide() - start_time;
            if ((u64)elapsed >= (u64)gm->timeout)
                return 0x80410b05;
            rem = gm->timeout - (u32)elapsed;
        }
        int rlen = len;
        res = sceNetAdhocPtpRecv(ptp_id, p, &rlen, rem, 0);
        if (res < 0)
            return res;
        len -= rlen;
        p += rlen;
    }
    return res;
}

/* 0x5b28 */
static void *sub_5b28(int tag, void *buf, u32 len, int *tag_len_out)
{
    u8 *cur = (u8 *)buf;
    u8 *end = cur + len;
    u8 target = (u8)tag;

    while (cur < end) {
        u8 *lptr = cur + 1;
        if (end - lptr < 2)
            return NULL;
        u8 t = *cur;
        cur = lptr + 2;
        u16 tlen;
        sceKernelMemcpy(&tlen, lptr, 2);
        tlen = __builtin_bswap16(tlen);
        if (end - cur < (int)tlen)
            return NULL;
        if (t == target) {
            *tag_len_out = tlen;
            return cur;
        }
        cur += tlen;
    }
    return NULL;
}

/* 0x5bf4 */
static int sub_5bf4(void)
{
    int res = sceKernelCreateLwMutex(g_mutex_work, "SceNetAdhocctl", 0x200, 0, NULL);
    if (res < 0)
        return res;
    g_mutex_created = 1;
    return 0;
}

/* 0x5c44 */
static void sub_5c44(void)
{
    if (g_mutex_created != 0)
        sceKernelDeleteLwMutex(g_mutex_work);
    g_mutex_created = 0;
}

/* 0x5c88 */
static int sub_5c88(void)
{
    return sceKernelLockLwMutex(g_mutex_work, 1, NULL);
}

/* 0x5cb0 */
static void sub_5cb0(int lock_res)
{
    if (lock_res == 0)
        sceKernelUnlockLwMutex(g_mutex_work, 1);
}
