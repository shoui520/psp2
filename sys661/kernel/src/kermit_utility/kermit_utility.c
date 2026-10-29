#include <kermit.h>

/* Imported functions declared in scetables.S */
extern int sceKernelBootFrom(void);
extern int sceKernelApplicationType(void);
extern int sceKernelInitDiscImage(void);
extern int sceIoDevctl(const char *dev, u32 cmd, void *indata, int inlen, void *outdata, int outlen);
extern int sceKernelIsToolMode(void);
extern int sceKernelDipsw(int bit);
extern int sceKernelIsDevelopmentToolMode(void);
extern int sceKernelUnloadModule(SceUID modid);
extern int sceKernelStartModule(SceUID modid, SceSize argsize, void *argp, int *status, void *opt);
extern SceUID sceKernelLoadModuleForKernel(const char *path, int flags, void *opt);
extern SceUID sceKernelLoadModuleToBlock(const char *path, SceUID block_id, SceUID *rem_block, int flags, void *opt);
extern SceUID sceKernelSearchModuleByName(const char *name);
extern int sceKernelStopModule(SceUID modid, SceSize argsize, void *argp, int *status, void *opt);
extern int sceKernelGetModel(void);
extern int sceKernelJointMemoryBlock(SceUID id1, SceUID id2);
extern int sceKernelMemset32(void *dst, int val, SceSize size);
extern SceUID sceKernelAllocPartitionMemory(int mpid, const char *name, int type, SceSize size, void *addr);
extern int sceKernelGetCompiledSdkVersion(void);
extern int sceKernelFreePartitionMemory(SceUID block_id);
extern int sceKernelQueryMemoryBlockInfo(SceUID block_id, void *info);
extern void *sceKernelGetGameInfo(void);
extern void *memset(void *dst, int val, SceSize n);
extern char *strncpy(char *dst, const char *src, SceSize n);
extern int strcmp(const char *s1, const char *s2);
extern int snprintf(char *buf, SceSize size, const char *fmt, ...);
extern int sceKernelWaitThreadEnd(SceUID thid, SceUInt *timeout);
extern SceUID sceKernelGetThreadId(void);
extern u32 sceKernelGetSystemTimeLow(void);
extern SceUID sceKernelCreateThread(const char *name, int (*entry)(SceSize, void *), int prio, int stackSize, SceUInt attr, void *opt);
extern int sceKernelUnlockMutex(SceUID mutexid, int count);
extern int sceKernelExitDeleteThread(int status);
extern int sceKernelDeleteThread(SceUID thid);
extern int sceKernelLockMutex(SceUID mutexid, int count, SceUInt *timeout);
extern SceUID sceKernelCreateMutex(const char *name, SceUInt attr, int init_count, void *opt);
extern int sceKernelExtendKernelStack(int type, void (*cb)(void *), void *arg);
extern int sceKernelStartThread(SceUID thid, SceSize arglen, void *argp);
extern int sceCtrlPeekBufferPositive(void *pad_data, int count);
extern int sceCtrlSetSamplingMode(int mode);
extern int sceCtrlGetSamplingMode(int *pmode);
extern int sceMeBootStart(int mebooterType);
extern int scePower_driver_315B8CB6(int a0);
extern int scePower_driver_766CD857(int a0);
extern int sceNpDrmSetDebugMode(int mode);
extern int sceRegGetKeyValueByName(u32 cat, const char *name, void *buf, SceSize size);
extern int sceRegCreateKey(u32 cat, const char *name, int type, SceSize size);
extern int sceRegSetKeyValue(u32 cat, const char *name, const void *buf, SceSize size);
extern int sceRegCloseRegistry(u32 reg);
extern int sceRegOpenCategory(u32 reg, const char *dir, int mode, u32 *cat);
extern int sceRegFlushRegistry(u32 reg);
extern int sceRegRemoveCategory(u32 reg, const char *dir);
extern int sceRegGetKeyInfo(u32 cat, const char *name, u32 *kh, u32 *type, SceSize *size);
extern int sceRegFlushCategory(u32 cat);
extern int sceRegOpenRegistry(void *param, int mode, u32 *reg);
extern int sceRegGetKeyInfoByName(u32 cat, const char *name, u32 *type, SceSize *size);
extern int sceRegGetKeyValue(u32 cat, u32 kh, void *buf, SceSize size);
extern int sceRegCloseCategory(u32 cat);
extern int sceKernelPowerUnlockForUser(int type);
extern int sceKernelVolatileMemLock(int type, void **paddr, int *psize);
extern int sceKernelVolatileMemUnlock(int type);
extern int sceKernelPowerLockForUser(int type);

/* Public / Exported prototypes */
int sceUtility_driver_F062AEA6(void);
int sceUtility_driver_20C68C34(void);
int sceUtilityLoadModule(u32 id);
int sceUtilityUnloadModule(u32 id);

/* Forward declarations */
static int sub_011c(u32 type, void *params);
static int sub_088c(u32 type);
static int sub_0984(u32 type, void *params);
static int sub_1124(int ev_id, char *ev_name, void *param, int *result);
static int sub_1224(u32 type, int speed);
static int sub_12ac(u32 type);
static int sub_12f4(u32 type);
static int sub_1694(SceSize args, void *argp);
static int sub_1c04(SceSize args, void *argp);
static int sub_1c74(SceSize args, void *argp);
static int sub_1edc(SceSize args, void *argp);
static int sub_1f84(SceUID modid, int reason);
static int sub_1ffc(SceUID modid, int reason);
static int sub_20ec(u32 type, void *params);
static int sub_2350(u32 type);
static int sub_25ec(void);
static int sub_2650(void);
static int sub_2680(u32 type, int speed);
static int sub_26f8(u32 type);
static int sub_296c(u32 type, u32 size, void *params);
static SceUID sub_29a4(const char *path, SceUID *block_p, int flags, void *opt);
static int sub_2a50(SceUID modid);
static SceUID sub_2ad4(const char *path, int mpid, SceSize argsize, void *argp, SceUID *block_p);
static int sub_3208(void);
static void sub_3264(SceUID block_id);
static int sub_3458(void);
static int sub_3588(void);
static int sub_3614(const char *path, int arg1);
static int sub_3654(SceUID modid);
static int sub_37d0(void);
static int sub_3e50(void);
static int sub_3f2c(void);
static int sub_3f78(u32 id, int a1);
static int sub_3fe8(u32 id, int a1);
static int sub_42f8(void);
static int sub_4344(u32 id);
static int sub_43f8(void);
static int sub_4444(u32 id, int a1);
static int sub_44b4(void);
static int sub_4540(u32 id);
static int sub_4720(u32 id);
static int sub_4aa8(int count, const u32 *ids);
static int sub_4b8c(int count, const u32 *ids);
static int sub_559c(void);
static int sub_621c(u32 id, u32 val);
static int sub_651c(u32 id, u32 *val);
static int sub_68f8(const char *dir, const char *name, u32 val);
static int sub_6a88(const char *dir, const char *name, u32 *val, u32 defval);
static int sub_6bec(const char *dir, const char *name, const char *str, int size);
static int sub_6ce8(const char *dir, const char *name, char *buf, int size, const char *defstr);
static int sub_6e68(u32 id, const char *str);
static int sub_7034(u32 id, char *str, int len);
static int sub_7a7c(int idx);
static int sub_7ae4(int idx);
static int sub_7b3c(int idx);
static int sub_7b94(void *buf);
static int sub_7cd0(int idx);
static int sub_7d50(int idx, int param, const void *data);
static int sub_7e60(int idx, int param, void *data);
static int sub_7f8c(const char *dir, int idx);
static int sub_8048(int dst_idx, int src_idx);
static int sub_80ec(int a0, int a1);
static int sub_817c(int idx);
static int sub_81d0(void);
static int sub_8200(int idx);
static int sub_825c(int idx);
static int sub_830c(int idx, int param, void *data, int set_mode);
static int sub_8c20(void);
static int sub_8d2c(int *idx_p);
static int sub_8df0(char *out_dir, int idx);
static int sub_8e60(const char *dir, int a1);
static int sub_8ee8(int idx);
static int sub_8f3c(u32 *reg_p);
static int sub_8f6c(u32 reg);
static int sub_90e4(int mode);
static int sub_91b0(void);
static void sub_9204(void);
static int sub_922c(int a0, int a1, void *a2);
static int sub_9270(int a0, int a1, void *a2);

/* Data structures */
typedef struct {
    const char *path;
    int mpid;
    int group;
    u32 dep_mask;
    int state_and_modid;
    int loaded_by_type;
} SceUtilityModEntry;

typedef struct {
    SceUID thid;              /* 0x00 (0xc400) */
    SceUID suspend_evf;       /* 0x04 (0xc404) */
    SceUID worktime_evf;      /* 0x08 (0xc408) */
    u32 active_type;          /* 0x0c (0xc40c) */
    u32 status;               /* 0x10 (0xc410) */
    u32 abort_flag;           /* 0x14 (0xc414) */
    u32 emulated_ms;          /* 0x18 (0xc418) */
    u32 speed;                /* 0x1c (0xc41c) */
    void *params;             /* 0x20 (0xc420) */
    u32 params_size;          /* 0x24 (0xc424) */
    const char *mod_path;     /* 0x28 (0xc428) */
    SceUID chnnlsv_modid;     /* 0x2c (0xc42c) */
    SceUID psheet_modid;      /* 0x30 (0xc430) */
    SceUID common_modid;      /* 0x34 (0xc434) */
    SceUID net_modid;         /* 0x38 (0xc438) */
    SceUID paf_modid;         /* 0x3c (0xc43c) */
    SceUID main_modid;        /* 0x40 (0xc440) */
    int res_44;               /* 0x44 (0xc444) */
    void *ext_data;           /* 0x48 (0xc448) */
    u32 ext_size;             /* 0x4c (0xc44c) */
    u32 saved_unk0c;          /* 0x50 (0xc450) */
    u32 saved_unk28;          /* 0x54 (0xc454) */
    u32 saved_unk2c;          /* 0x58 (0xc458) */
    int sdk_ver_override;     /* 0x5c (0xc45c) */
    void *vmem_addr;          /* 0x60 (0xc460) */
    u32 vmem_size;            /* 0x64 (0xc464) */
} SceUtilityDialogState;

typedef struct {
    SceUID shutdown_thid;     /* 0x00 (0xc468) */
    SceUID worktime_evf;      /* 0x04 (0xc46c) */
    u32 active_type;          /* 0x08 (0xc470) */
    u32 status;               /* 0x0c (0xc474) */
    void *params;             /* 0x10 (0xc478) */
    u32 params_size;          /* 0x14 (0xc47c) */
    u32 speed;                /* 0x18 (0xc480) */
    SceUID modid;             /* 0x1c (0xc484) */
    SceUID block_id;          /* 0x20 (0xc488) */
    const char *mod_path;     /* 0x24 (0xc48c) */
    SceUID init_thid;         /* 0x28 (0xc490) */
} SceUtilityAppletState;

typedef struct {
    SceUID block_id;
    SceUID modid;
} SceUtilityAppletBlockMod;

typedef struct SceUtilityDialogMemNode {
    struct SceUtilityDialogMemNode *next;
    struct SceUtilityDialogMemNode *prev;
    SceUID block_id;
    SceUID modid;
} SceUtilityDialogMemNode;

/* .data globals */
static SceSysEventHandler g_sys_ev_handler = {
    0x40,
    "SceUtility",
    0x00FFFF00,
    sub_1124,
    0, 0, NULL,
    { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static SceUtilityModEntry g_mods_01[25] = {
    { "flash0:/kd/pspnet.prx",                2, 1, 0, 0, 0 },
    { "flash0:/kd/pspnet_adhoc.prx",          2, 1, 1, 0, 0 },
    { "flash0:/kd/pspnet_adhocctl.prx",       2, 1, 3, 0, 0 },
    { "flash0:/kd/pspnet_adhoc_matching.prx", 2, 1, 3, 0, 0 },
    { "flash0:/kd/pspnet_adhoc_download.prx", 2, 1, 5, 0, 0 },
    { "flash0:/kd/pspnet_inet.prx",           2, 1, 1, 0, 0 },
    { "flash0:/kd/pspnet_apctl.prx",          2, 1, 0x21, 0, 0 },
    { "flash0:/kd/parseuri.prx",              2, 1, 0x20, 0, 0 },
    { "flash0:/kd/parsehttp.prx",             2, 1, 0x80, 0, 0 },
    { "flash0:/kd/libhttp.prx",               2, 1, 0x1e0, 0, 0 },
    { "flash0:/kd/libssl.prx",                2, 1, 0x20, 0, 0 },
    { "flash0:/kd/np_core.prx",               1, 2, 0, 0, 0 },
    { "flash0:/kd/np.prx",                    2, 2, 0xe60, 0, 0 },
    { "flash0:/kd/np_service.prx",            2, 2, 0x1e60, 0, 0 },
    { "flash0:/kd/np_matching2.prx",          2, 2, 0x1e60, 0, 0 },
    { "flash0:/kd/np_auth.prx",               2, 2, 0x1000, 0, 0 },
    { "flash0:/kd/libupnp.prx",               2, 1, 0x2e0, 0, 0 },
    { "flash0:/kd/pspnet_upnp.prx",           2, 1, 0x102e0, 0, 0 },
    { "flash0:/kd/rhttp.prx",                 2, 1, 0x6e0, 0, 0 },
    { "flash0:/kd/np_commerce2.prx",          2, 2, 0x1600, 0, 0 },
    { "flash0:/kd/np_inst.prx",               1, 2, 0, 0, 0 },
    { "flash0:/kd/np_campaign.prx",           2, 2, 0x16e0, 0, 0 },
    { "flash0:/kd/libdnssock.prx",            2, 1, 0x20, 0, 0 },
    { "flash0:/kd/pspnet_adhoc_discover.prx", 2, 1, 4, 0, 0 },
    { "flash0:/kd/pspnet_adhoc_auth.prx",     1, 1, 3, 0, 0 },
};

static SceUtilityModEntry g_mods_02[5] = {
    { "flash0:/kd/usbpspcm.prx", 1, 0, 0, 0, 0 },
    { "flash0:/kd/usbstor.prx",  1, 0, 0, 0, 0 },
    { "flash0:/kd/usbmic.prx",   1, 0, 0, 0, 0 },
    { "flash0:/kd/usbcam.prx",   1, 0, 0, 0, 0 },
    { "flash0:/kd/usbgps.prx",   1, 0, 0, 0, 0 },
};

static SceUtilityModEntry g_mods_03[11] = {
    { "flash0:/kd/avcodec.prx",        1, 1, 0, 0, 0 },
    { "flash0:/kd/vaudio.prx",         1, 1, 0, 0, 0 },
    { "flash0:/kd/vshbridge_msapp.prx", 1, 1, 1, 0, 0 },
    { "flash0:/kd/mpeg_vsh.prx",       2, 1, 1, 0, 0 },
    { "flash0:/kd/mp4msv.prx",         2, 2, 0, 0, 0 },
    { "flash0:/kd/libatrac3plus.prx",  2, 1, 1, 0, 0 },
    { "flash0:/kd/mediasync.prx",      1, 1, 0, 0, 0 },
    { "flash0:/kd/libaac.prx",         2, 1, 1, 0, 0 },
    { "flash0:/kd/libmp3.prx",         2, 1, 1, 0, 0 },
    { "flash0:/kd/sc_sascore.prx",     1, 1, 0, 0, 0 },
    { "flash0:/kd/g729.prx",           1, 1, 0, 0, 0 },
};

static SceUtilityModEntry g_mods_04[2] = {
    { "flash0:/kd/http_storage.prx", 1, 0, 0, 0, 0 },
    { "flash0:/vsh/module/updown.prx", 2, 0, 1, 0, 0 },
};

static SceUtilityModEntry g_mods_05[3] = {
    { "flash0:/kd/amctrl.prx",  1, 0, 0, 0, 0 },
    { "flash0:/kd/npdrm.prx",   1, 0, 1, 0, 0 },
    { "flash0:/kd/iofilemgr_dnas.prx", 1, 0, 3, 0, 0 },
};

static SceUtilityModEntry g_mods_06[2] = {
    { "flash0:/kd/irda.prx", 1, 0, 0, 0, 0 },
    { "flash0:/kd/sircs.prx", 1, 0, 0, 0, 0 },
};

/* .bss globals */
static SceUtilityDialogState g_dialog;
static SceUtilityAppletState g_applet;
static SceUtilityAppletBlockMod g_applet_mods[8];
static int g_dialog_mem_inited;
static u32 g_net_mod_flags;
static SceUID g_net_block_id;
static SceUID g_usb_mutex;
static SceUID g_av_mutex;
static SceUID g_updown_mutex;
static SceUID g_np_mutex;
static SceUID g_mod_mutex;
static int g_netparam_locked;
static u8 g_kermit_buf[64] __attribute__((aligned(64)));
static SceUtilityDialogMemNode g_dialog_mem_nodes[3];
static SceUtilityDialogMemNode g_dialog_mem_head;
static u32 g_dialog_mem_time;
static u32 g_status_flags;

/* Module entry points */
int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    return sceUtility_driver_F062AEA6();
}

int sceUtility_driver_F062AEA6(void)
{
    int ret;

    sub_559c();
    sub_91b0();

    g_dialog.worktime_evf = sceKernelCreateEventFlag("SceUtilityWorktime", 0x200, 0, NULL);
    if (g_dialog.worktime_evf < 0)
        return 0x80000042;

    g_dialog.suspend_evf = sceKernelCreateEventFlag("SceUtilitySuspend", 0x200, 3, NULL);
    if (g_dialog.suspend_evf < 0)
        return 0x80000042;

    g_dialog.emulated_ms = -1;
    sceKernelRegisterSysEventHandler(&g_sys_ev_handler);

    ret = sub_25ec();
    if (ret < 0) return ret;
    ret = sub_3458();
    if (ret < 0) return ret;
    ret = sub_37d0();
    if (ret < 0) return ret;
    ret = sub_3e50();
    if (ret < 0) return ret;
    ret = sub_3f2c();
    if (ret < 0) return ret;
    ret = sub_42f8();
    if (ret < 0) return ret;
    ret = sub_43f8();
    if (ret < 0) return ret;
    ret = sub_3588();
    if (ret < 0) return ret;
    ret = sub_44b4();
    if (ret < 0) return ret;

    return 0;
}

int sceUtility_driver_20C68C34(void)
{
    sub_2650();
    sceKernelUnregisterSysEventHandler(&g_sys_ev_handler);
    return 0;
}

static int sub_1124(int ev_id, char *ev_name, void *param, int *result)
{
    int status;
    (void)ev_name;
    (void)param;
    (void)result;

    if (ev_id == 0x400) {
        status = g_dialog.status;
        if (status != 0 && (u32)(status - 4) >= 2) {
            if (sceKernelWaitEventFlag(g_dialog.suspend_evf, 1, 0, NULL, NULL) == 0)
                return 0;
            return 0x80110006;
        }
    } else if (ev_id == 0x4000) {
        sceKernelClearEventFlag(g_dialog.suspend_evf, ~2);
    } else if (ev_id == 0x10000) {
        sceKernelSetEventFlag(g_dialog.suspend_evf, 2);
    }
    return 0;
}

/* Dialog core */
static int sub_011c(u32 type, void *params)
{
    int intr, ret;
    u32 size, ver;
    u32 *p = (u32 *)params;

    if (g_dialog.status != 0) {
        if (g_dialog.active_type != type)
            return 0x80110005;
        return 0x80110001;
    }

    if (p == NULL)
        return 0x80110002;

    intr = sceKernelCpuSuspendIntr();
    g_dialog.status = 1;
    g_dialog.active_type = type;
    g_dialog.params = params;
    g_dialog.abort_flag = 0;
    g_dialog.sdk_ver_override = 0;
    sceKernelCpuResumeIntr(intr);

    size = p[0];
    g_dialog.params_size = size;
    g_dialog.mod_path = NULL;
    g_dialog.ext_data = NULL;
    g_dialog.ext_size = 0;

    switch (type) {
    case 1: /* Savedata */
        if (size == 0x5c8) {
            ver = 1;
        } else if (size == 0x5dc) {
            ver = 2;
        } else if (size == 0x600) {
            ver = 3;
        } else {
            ret = 0x80110004;
            goto err;
        }
        g_dialog.saved_unk0c = p[3];
        if (ver >= 2) {
            g_dialog.saved_unk28 = p[10];
            g_dialog.saved_unk2c = p[11];
        }
        p[3] = ver;
        g_dialog.mod_path = "flash0:/vsh/module/savedata_utility.prx";
        break;
    case 3: /* GameSharing */
        if (size != 0x94) { ret = 0x80110004; goto err; }
        g_dialog.mod_path = "flash0:/vsh/module/gamesharing_utility.prx";
        break;
    case 4: /* Netconf */
        if (size == 0x3c) {
            g_dialog.saved_unk0c = p[3];
            p[3] = 1;
        } else if (size == 0x44) {
            g_dialog.saved_unk0c = p[3];
            p[3] = 2;
        } else if (size == 0x4c) {
            g_dialog.saved_unk0c = p[3];
            p[3] = 3;
        } else {
            ret = 0x80110004;
            goto err;
        }
        g_dialog.mod_path = "flash0:/vsh/module/netconf_utility.prx";
        break;
    case 5: /* MsgDialog */
        if (size == 0x240) {
            g_dialog.saved_unk0c = p[3];
            p[3] = 1;
        } else if (size == 0x24c) {
            g_dialog.saved_unk0c = p[3];
            p[3] = 2;
        } else if (size == 0x2cc) {
            g_dialog.saved_unk0c = p[3];
            p[3] = 3;
        } else {
            ret = 0x80110004;
            goto err;
        }
        g_dialog.mod_path = "flash0:/vsh/module/msgdialog_utility.prx";
        break;
    case 6: /* Osk */
        if (size != 0x3c) { ret = 0x80110004; goto err; }
        g_dialog.mod_path = "flash0:/vsh/module/osk_utility.prx";
        break;
    case 7: /* Netplay */
        if (size != 0x5c) { ret = 0x80110004; goto err; }
        g_dialog.mod_path = "flash0:/vsh/module/netplay_Dialog.prx";
        break;
    case 8: /* GamedataInstall */
        if (size != 0x688) { ret = 0x80110004; goto err; }
        g_dialog.mod_path = "flash0:/vsh/module/gamedata_utility.prx";
        break;
    case 9: /* RssSubscriber */
        if (size != 0x38) { ret = 0x80110004; goto err; }
        g_dialog.mod_path = "flash0:/vsh/module/rss_subscriber.prx";
        break;
    case 11: /* StoreCheckout */
        if (size != 0x40) { ret = 0x80110004; goto err; }
        g_dialog.mod_path = "flash0:/vsh/module/store_checkout_utility.prx";
        break;
    case 12: /* RssReader */
        if (size != 0x50) { ret = 0x80110004; goto err; }
        g_dialog.mod_path = "flash0:/vsh/module/rss_reader.prx";
        break;
    case 13: /* Eula */
        if (size != 0x34) { ret = 0x80110004; goto err; }
        g_dialog.mod_path = "flash0:/vsh/module/eula_netconf_utility.prx";
        break;
    case 14: /* Screenshot */
        if (size != 0x3a4) { ret = 0x80110004; goto err; }
        g_dialog.mod_path = "flash0:/vsh/module/screenshot_utility.prx";
        break;
    case 15: /* NpSignin */
        if (size != 0x3c) { ret = 0x80110004; goto err; }
        g_dialog.mod_path = "flash0:/vsh/module/np_signin_utility.prx";
        break;
    case 16: /* Hick */
        if (size != 0x84) { ret = 0x80110004; goto err; }
        g_dialog.mod_path = "flash0:/vsh/module/hick_utility.prx";
        break;
    case 17:
        if (size != 0x109c) { ret = 0x80110004; goto err; }
        g_dialog.mod_path = "flash0:/vsh/module/savedata_utility.prx";
        break;
    case 18:
        if (size != 0x3c) { ret = 0x80110004; goto err; }
        g_dialog.mod_path = "flash0:/vsh/module/np_friend_utility.prx";
        break;
    case 19:
        if (size != 0x10d8) { ret = 0x80110004; goto err; }
        g_dialog.mod_path = "flash0:/vsh/module/psn_utility.prx";
        break;
    default:
        ret = 0x80110003;
        goto err;
    }

    ret = sceKernelVolatileMemLock(0, &g_dialog.vmem_addr, (int *)&g_dialog.vmem_size);
    if (ret < 0)
        goto err;

    sub_90e4(1);
    sceKernelClearEventFlag(g_dialog.worktime_evf, 0);
    g_dialog.thid = sceKernelCreateThread("SceUtilityInit", sub_1694, p[4], 0x1000, 0x00100000, NULL);
    if (g_dialog.thid <= 0) {
        ret = g_dialog.thid;
        sceKernelVolatileMemUnlock(0);
        sub_90e4(0);
        goto err;
    }

    ret = sceKernelStartThread(g_dialog.thid, 0, NULL);
    if (ret != 0) {
        sceKernelDeleteThread(g_dialog.thid);
        sceKernelVolatileMemUnlock(0);
        sub_90e4(0);
        goto err;
    }

    return 0;

err:
    intr = sceKernelCpuSuspendIntr();
    g_dialog.status = 0;
    g_dialog.active_type = 0;
    g_dialog.params = NULL;
    g_dialog.params_size = 0;
    sceKernelCpuResumeIntr(intr);
    return ret;
}

static int sub_088c(u32 type)
{
    int ret;
    u32 *p;

    if (g_dialog.active_type != type)
        return 0x80110005;
    if (g_dialog.status != 3)
        return 0x80110001;

    g_dialog.status = 4;
    p = (u32 *)g_dialog.params;
    g_dialog.thid = sceKernelCreateThread("SceUtilityShutdown", sub_1c74, p[4], 0x1000, 0x00100000, NULL);
    if (g_dialog.thid <= 0) {
        g_dialog.thid = 0;
        return g_dialog.thid;
    }

    ret = sceKernelStartThread(g_dialog.thid, 0, NULL);
    if (ret != 0) {
        sceKernelDeleteThread(g_dialog.thid);
        g_dialog.thid = 0;
        return ret;
    }

    return 0;
}

static int sub_0984(u32 type, void *params)
{
    int ret;

    ret = sub_011c(type, params);
    if (ret == 0)
        g_dialog.sdk_ver_override = 0x06060110;
    return ret;
}

static int sub_1224(u32 type, int speed)
{
    if (g_dialog.active_type != type)
        return 0x80110005;
    if (g_dialog.status != 2)
        return 0x80110001;

    g_dialog.speed = speed;
    sceKernelSetEventFlag(g_dialog.worktime_evf, 0x10);
    sceKernelWaitEventFlag(g_dialog.worktime_evf, 0x20, 0x20, NULL, NULL);
    return 0;
}

static int sub_12ac(u32 type)
{
    if (g_dialog.active_type != type) {
        if (g_dialog.status == 0)
            return 0;
        return 0x80110005;
    }
    return g_dialog.status;
}

static int sub_12f4(u32 type)
{
    if (g_dialog.active_type != type)
        return 0x80110005;
    if ((u32)(g_dialog.status - 1) >= 2)
        return 0x80110001;
    g_dialog.abort_flag = 1;
    return 0;
}

/* sceUtility_private dialog helpers */
int sceUtility_private_363A12E2(int type, void *ext_data, u32 ext_size, void *params)
{
    int ret = sub_011c(type, params);
    if (ret == 0) {
        g_dialog.ext_data = ext_data;
        g_dialog.ext_size = ext_size;
    }
    return ret;
}

int sceUtility_private_59EB3A17(int type)
{
    return sub_088c(type);
}

int sceUtility_private_78EC982D(int type, int speed)
{
    return sub_1224(type, speed);
}

int sceUtility_private_5ED16EBA(int type)
{
    return sub_12ac(type);
}

int sceUtility_private_E8EE4E87(int type)
{
    return sub_12f4(type);
}

int sceUtility_private_14E94D25(void)
{
    sceKernelWaitEventFlag(g_dialog.worktime_evf, 1, 0x20, NULL, NULL);
    return 0;
}

int sceUtility_private_0D6B23A4(void)
{
    sceKernelSetEventFlag(g_dialog.worktime_evf, 2);
    return 0;
}

int sceUtility_private_0027D953(void)
{
    sceKernelWaitEventFlag(g_dialog.worktime_evf, 4, 0x20, NULL, NULL);
    return 0;
}

int sceUtility_private_C1916D25(void)
{
    sceKernelSetEventFlag(g_dialog.worktime_evf, 8);
    return 0;
}

int sceUtility_private_AC7FD75C(void)
{
    sceKernelWaitEventFlag(g_dialog.worktime_evf, 0x10, 0x20, NULL, NULL);
    return 0;
}

int sceUtility_private_8D681EE0(void)
{
    sceKernelSetEventFlag(g_dialog.worktime_evf, 0x20);
    return 0;
}

int sceUtility_private_39E7692B(int status)
{
    g_dialog.status = status;
    return 0;
}

int sceUtility_private_EA446CD5(void)
{
    return g_dialog.active_type;
}

void *sceUtility_private_A25E0194(void)
{
    return g_dialog.params;
}

u32 sceUtility_private_DF025E50(void)
{
    return g_dialog.params_size;
}

u32 sceUtility_private_5809B44A(void)
{
    return g_dialog.speed;
}

u32 sceUtility_private_1E82FB49(void)
{
    return g_dialog.abort_flag;
}

void *sceUtility_private_5DCBD3C0(void)
{
    return g_dialog.ext_data;
}

u32 sceUtility_private_10D1B3FF(void)
{
    return g_dialog.ext_size;
}

int sceUtility_private_A1348D82(SceUID thid)
{
    g_dialog.thid = thid;
    return 0;
}

int sceUtility_private_421E2726(int ms)
{
    int prev = g_dialog.emulated_ms;
    g_dialog.emulated_ms = ms;
    return prev;
}

int sceUtility_private_A6B323E1(void)
{
    if (g_dialog.sdk_ver_override != 0)
        return g_dialog.sdk_ver_override;
    return sceKernelGetCompiledSdkVersion();
}

int sceUtility_private_031D0944(void)
{
    return sceKernelSetEventFlag(g_dialog.suspend_evf, 1);
}

int sceUtility_private_B9D9C78F(void)
{
    return sceKernelClearEventFlag(g_dialog.suspend_evf, ~1);
}

int sceUtility_private_31516141(void)
{
    return sceKernelWaitEventFlag(g_dialog.suspend_evf, 2, 0, NULL, NULL);
}

int sceUtility_private_966912D2(u32 *out_bits)
{
    SceUInt timeout = 0;
    return sceKernelWaitEventFlag(g_dialog.suspend_evf, 2, 0, out_bits, &timeout);
}

static int sub_1694(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    sceKernelSetEventFlag(g_dialog.worktime_evf, 1);
    sceKernelWaitEventFlag(g_dialog.worktime_evf, 2, 0x20, NULL, NULL);
    sceKernelExitDeleteThread(0);
    return 0;
}

static int sub_1c04(SceSize args, void *argp)
{
    g_applet.modid = sub_2ad4(g_applet.mod_path, 2, args, argp, &g_applet.block_id);
    sceKernelSetEventFlag(g_applet.worktime_evf, 1);
    sceKernelWaitEventFlag(g_applet.worktime_evf, 2, 0x20, NULL, NULL);
    sceKernelExitDeleteThread(0);
    return 0;
}

static int sub_1c74(SceSize args, void *argp)
{
    int intr;
    (void)args;
    (void)argp;

    sceKernelSetEventFlag(g_dialog.worktime_evf, 4);
    sceKernelWaitEventFlag(g_dialog.worktime_evf, 8, 0x20, NULL, NULL);
    sceKernelWaitThreadEnd(g_dialog.thid, NULL);

    sub_1f84(g_dialog.main_modid, 5);
    g_dialog.main_modid = 0;
    sub_1f84(g_dialog.net_modid, 5);
    g_dialog.net_modid = 0;
    sub_1f84(g_dialog.common_modid, 5);
    g_dialog.common_modid = 0;
    if (g_dialog.paf_modid != 0) {
        sub_1f84(g_dialog.paf_modid, 5);
        g_dialog.paf_modid = 0;
    }

    sub_9204();
    intr = sceKernelCpuSuspendIntr();
    g_dialog.status = 0;
    g_dialog.params = NULL;
    g_dialog.params_size = 0;
    g_dialog.abort_flag = 0;
    g_dialog.emulated_ms = 0;
    g_dialog.speed = 0;
    sceKernelCpuResumeIntr(intr);

    sub_90e4(0);
    sceKernelVolatileMemUnlock(0);
    sceKernelSetEventFlag(g_dialog.suspend_evf, 2);
    sceKernelExitDeleteThread(0);
    return 0;
}

static int sub_1edc(SceSize args, void *argp)
{
    int intr;
    (void)args;
    (void)argp;

    sceKernelSetEventFlag(g_applet.worktime_evf, 4);
    sceKernelWaitEventFlag(g_applet.worktime_evf, 8, 0x20, NULL, NULL);
    sceKernelWaitThreadEnd(g_applet.shutdown_thid, NULL);
    sub_1ffc(g_applet.modid, 0);
    if (g_applet.block_id > 0) {
        sceKernelFreePartitionMemory(g_applet.block_id);
        g_applet.block_id = 0;
    }
    intr = sceKernelCpuSuspendIntr();
    g_applet.status = 0;
    g_applet.params = NULL;
    g_applet.params_size = 0;
    sceKernelCpuResumeIntr(intr);
    sceKernelExitDeleteThread(0);
    return 0;
}

static int sub_1f84(SceUID modid, int reason)
{
    int status, ret;
    u32 opt[5];

    opt[0] = 0x14;
    opt[1] = reason;
    opt[2] = 0x1000;
    opt[3] = ((u32 *)g_dialog.params)[4];
    opt[4] = 0;

    ret = sceKernelStopModule(modid, 0, NULL, &status, opt);
    if (ret >= 0) {
        ret = sceKernelUnloadModule(modid);
        if (ret > 0)
            ret = 0;
    }
    return ret;
}

static int sub_1ffc(SceUID modid, int reason)
{
    int status, ret;
    u32 opt[5];

    opt[0] = 0x14;
    opt[1] = reason;
    opt[2] = 0x1000;
    opt[3] = ((u32 *)g_applet.params)[4];
    opt[4] = 0;

    ret = sceKernelStopModule(modid, 0, NULL, &status, opt);
    if (ret >= 0) {
        ret = sub_2a50(modid);
        if (ret > 0)
            ret = 0;
    }
    return ret;
}

/* Applet core (HtmlViewer) */
static int sub_20ec(u32 type, void *params)
{
    u32 *p = (u32 *)params;
    u32 size, ver, part_size;
    SceUID block;
    int ret;
    u32 arg[10];
    void *game_info;

    if (g_applet.status != 0)
        return 0x80110001;
    if (type != 1)
        return 0x80110003;

    size = p[0];
    g_applet.mod_path = "flash0:/vsh/module/htmlviewer_utility.prx";
    if (size == 0x70) ver = 0xc8;
    else if (size == 0x78) ver = 0xfa;
    else if (size == 0x80) ver = 0x104;
    else if (size == 0x98) ver = 0x10e;
    else if (size == 0xa4) ver = 0x118;
    else if (size == 0xa8) ver = 0x12c;
    else return 0x80110004;

    part_size = sub_296c(type, ver, params);
    block = sceKernelAllocPartitionMemory(2, "SceUtilityAppletPartition", 0, part_size, NULL);
    if (block <= 0)
        return block;

    g_applet.status = 1;
    g_applet.active_type = type;
    g_applet.params_size = ver;
    g_applet.block_id = block;
    g_applet.params = params;

    arg[0] = 0x28;
    arg[1] = sceKernelGetModel();
    arg[9] = sceKernelBootFrom();
    arg[2] = type;
    arg[4] = ver;
    arg[3] = (u32)params;
    game_info = sceKernelGetGameInfo();
    strncpy((char *)&arg[5], (const char *)game_info + 0x44, 13);
    ((char *)arg)[0x21] = '\0';

    g_applet.init_thid = sceKernelCreateThread("SceUtilityAppletInit", sub_1c04, p[4], 0x800, 0x00100000, NULL);
    if (g_applet.init_thid <= 0) {
        ret = g_applet.init_thid;
        g_applet.init_thid = 0;
        return ret;
    }

    ret = sceKernelStartThread(g_applet.init_thid, 0x28, arg);
    if (ret != 0) {
        sceKernelDeleteThread(g_applet.init_thid);
        g_applet.init_thid = 0;
        return ret;
    }

    return 0;
}

static int sub_2350(u32 type)
{
    int ret;
    u32 *p;

    if (g_applet.active_type != type)
        return 0x80110005;
    if (g_applet.status != 3)
        return 0x80110001;

    g_applet.status = 4;
    p = (u32 *)g_applet.params;
    g_applet.init_thid = sceKernelCreateThread("SceUtilityAppletShutdown", sub_1edc, p[4], 0x800, 0x00100000, NULL);
    if (g_applet.init_thid <= 0) {
        ret = g_applet.init_thid;
        g_applet.init_thid = 0;
        return ret;
    }

    ret = sceKernelStartThread(g_applet.init_thid, 0, NULL);
    if (ret != 0) {
        sceKernelDeleteThread(g_applet.init_thid);
        g_applet.init_thid = 0;
        return ret;
    }
    return 0;
}

int sceUtility_private_048BFC46(const char *name, int flags, void *opt)
{
    const char *path = NULL;

    if (strcmp(name, "htmlviewer_ui") == 0)
        path = "flash0:/vsh/module/htmlviewer_ui.prx";
    else if (strcmp(name, "hvauth_r") == 0)
        path = "flash0:/vsh/module/hvauth_r.prx";
    else if (strcmp(name, "hvauth_t") == 0)
        path = "flash0:/vsh/module/hvauth_t.prx";
    else if (strcmp(name, "netfront") == 0)
        path = "flash0:/vsh/module/netfront.prx";
    else if (strcmp(name, "mgvideo") == 0)
        path = "flash0:/kd/mgvideo.prx";
    else if (strcmp(name, "mm_flash") == 0)
        path = "flash0:/vsh/module/mm_flash.prx";
    else if (strcmp(name, "libslim") == 0)
        path = "flash0:/vsh/module/libslim.prx";
    else if (strcmp(name, "libwww") == 0)
        path = "flash0:/vsh/module/libwww.prx";
    else if (strcmp(name, "libfont_hv") == 0)
        path = "flash0:/vsh/module/libfont_hv.prx";
    else
        return 0x80010002;

    return sub_29a4(path, &g_applet.block_id, flags, opt);
}

static int sub_25ec(void)
{
    g_applet.worktime_evf = sceKernelCreateEventFlag("SceUtilityAppletWorktime", 0x200, 0, NULL);
    if (g_applet.worktime_evf < 0)
        return 0x80000042;
    g_applet.params_size = 0;
    g_applet.active_type = 0;
    g_applet.status = 0;
    g_applet.params = NULL;
    return 0;
}

static int sub_2650(void)
{
    if (g_applet.worktime_evf > 0)
        sceKernelDeleteEventFlag(g_applet.worktime_evf);
    return 0;
}

static int sub_2680(u32 type, int speed)
{
    if (g_applet.active_type != type)
        return 0x80110005;
    g_applet.speed = speed;
    if (g_applet.status != 2)
        return 0x80110001;
    sceKernelSetEventFlag(g_applet.worktime_evf, 0x10);
    sceKernelWaitEventFlag(g_applet.worktime_evf, 0x20, 0x20, NULL, NULL);
    return 0;
}

static int sub_26f8(u32 type)
{
    if (g_applet.active_type != type)
        return 0x80110005;
    return g_applet.status;
}

int sceUtility_private_186A2C84(void)
{
    sceKernelWaitEventFlag(g_applet.worktime_evf, 1, 0x20, NULL, NULL);
    return 0;
}

int sceUtility_private_29AFE096(void)
{
    sceKernelSetEventFlag(g_applet.worktime_evf, 2);
    return 0;
}

int sceUtility_private_E0F5DEC1(void)
{
    sceKernelWaitEventFlag(g_applet.worktime_evf, 4, 0x20, NULL, NULL);
    return 0;
}

int sceUtility_private_8024FCA7(void)
{
    sceKernelSetEventFlag(g_applet.worktime_evf, 8);
    return 0;
}

int sceUtility_private_7E0D4BCD(void)
{
    sceKernelWaitEventFlag(g_applet.worktime_evf, 0x10, 0x20, NULL, NULL);
    return 0;
}

int sceUtility_private_CDEEBEE2(void)
{
    sceKernelSetEventFlag(g_applet.worktime_evf, 0x20);
    return 0;
}

int sceUtility_private_86AA70BC(u32 status)
{
    g_applet.status = status;
    return 0;
}

u32 sceUtility_private_FC841AC3(void)
{
    return g_applet.active_type;
}

void *sceUtility_private_369D25E4(void)
{
    return g_applet.params;
}

u32 sceUtility_private_ED4E37CA(void)
{
    return g_applet.params_size;
}

u32 sceUtility_private_BAFFB9D5(void)
{
    return g_applet.speed;
}

int sceUtility_private_69AB28E8(SceUID thid)
{
    g_applet.shutdown_thid = thid;
    return 0;
}

int sceUtility_private_78A2FE0C(SceUID modid)
{
    return sub_2a50(modid);
}

int sceUtility_private_4F6FFC7C(int a0)
{
    return scePower_driver_766CD857(a0);
}

int sceUtility_private_34340686(int a0)
{
    return scePower_driver_315B8CB6(a0);
}

static int sub_296c(u32 type, u32 size, void *params)
{
    if (type != 1)
        return 0;
    if (size < 0x10e)
        return 0x380000;
    if (((u32 *)params)[19] & 0x400)
        return 0x480000;
    return 0x380000;
}

static SceUID sub_29a4(const char *path, SceUID *block_p, int flags, void *opt)
{
    SceUID modid, new_block = 0;
    int i;

    if (*block_p != 0)
        modid = sceKernelLoadModuleToBlock(path, *block_p, &new_block, flags, opt);
    else
        modid = sceKernelLoadModuleForKernel(path, flags, opt);

    if (modid > 0) {
        for (i = 0; i < 8; i++) {
            if (g_applet_mods[i].modid == 0) {
                g_applet_mods[i].block_id = *block_p;
                g_applet_mods[i].modid = modid;
                break;
            }
        }
        if (*block_p != 0)
            *block_p = new_block;
    }
    return modid;
}

static int sub_2a50(SceUID modid)
{
    int ret, i;

    ret = sceKernelUnloadModule(modid);
    for (i = 0; i < 8; i++) {
        if (g_applet_mods[i].modid == modid) {
            if (g_applet_mods[i].block_id != 0)
                sceKernelFreePartitionMemory(g_applet_mods[i].block_id);
            g_applet_mods[i].modid = 0;
            break;
        }
    }
    return ret;
}

static SceUID sub_2ad4(const char *path, int mpid, SceSize argsize, void *argp, SceUID *block_p)
{
    SceUID modid;
    int status, ret;
    u32 load_opt[5];
    u32 start_opt[5];

    load_opt[0] = 0x14;
    load_opt[1] = mpid;
    load_opt[2] = mpid;
    load_opt[3] = 0;
    ((u8 *)load_opt)[16] = 0;
    ((u8 *)load_opt)[17] = 1;

    modid = sub_29a4(path, block_p, 0, load_opt);
    if (modid < 0)
        return modid;

    start_opt[0] = 0x14;
    start_opt[1] = mpid;
    start_opt[2] = 0x1000;
    start_opt[3] = ((u32 *)g_applet.params)[4];
    start_opt[4] = 0;

    ret = sceKernelStartModule(modid, argsize, argp, &status, start_opt);
    if (ret >= 0)
        return modid;
    return ret;
}

/* Dialog module memory management */
int sceUtility_private_0C0AE566(SceSize size)
{
    int i;
    SceUID block;

    if (g_dialog_mem_inited != 0)
        return 0x80110001;

    for (i = 0; i < 3; i++) {
        g_dialog_mem_nodes[i].next = NULL;
        g_dialog_mem_nodes[i].prev = NULL;
        g_dialog_mem_nodes[i].block_id = -1;
        g_dialog_mem_nodes[i].modid = -1;
    }

    g_dialog_mem_head.next = &g_dialog_mem_head;
    g_dialog_mem_head.prev = &g_dialog_mem_head;

    block = sceKernelAllocPartitionMemory(2, "SceUtilityDialogModule", 0, size, NULL);
    if (block < 0)
        return block;

    g_dialog_mem_nodes[0].block_id = block;
    g_dialog_mem_nodes[0].next = g_dialog_mem_head.next;
    g_dialog_mem_nodes[0].prev = &g_dialog_mem_head;
    g_dialog_mem_head.next->prev = &g_dialog_mem_nodes[0];
    g_dialog_mem_head.next = &g_dialog_mem_nodes[0];

    if (sub_3208() != 0) {
        g_dialog_mem_time = sceKernelGetSystemTimeLow();
        sub_3264(g_dialog_mem_nodes[0].block_id);
    }

    g_dialog_mem_inited = 1;
    return 0;
}

int sceUtility_private_B34EC4AD(void)
{
    SceUtilityDialogMemNode *node;

    if (g_dialog_mem_inited == 0)
        return 0x80110001;

    for (node = g_dialog_mem_head.next; node != &g_dialog_mem_head; node = node->next) {
        if (node->modid >= 0)
            return 0x80110001;
    }

    if (g_dialog_mem_nodes[0].block_id >= 0)
        sceKernelFreePartitionMemory(g_dialog_mem_nodes[0].block_id);

    g_dialog_mem_inited = 0;
    return 0;
}

SceUID sceUtility_private_ACB67A01(const char *path, int flags, void *opt)
{
    SceUtilityDialogMemNode *node, *free_node = NULL;
    SceUID modid, rem_block = 0;
    int i;

    if (g_dialog_mem_inited == 0)
        return 0x80110001;

    for (i = 0; i < 3; i++) {
        if (g_dialog_mem_nodes[i].next == NULL && g_dialog_mem_nodes[i].prev == NULL) {
            free_node = &g_dialog_mem_nodes[i];
            break;
        }
    }

    for (node = g_dialog_mem_head.next; node != &g_dialog_mem_head; node = node->next) {
        if (node->modid < 0 && node->block_id >= 0) {
            modid = sceKernelLoadModuleToBlock(path, node->block_id, &rem_block, flags, opt);
            if (modid >= 0) {
                node->modid = modid;
                if (rem_block > 0 && free_node != NULL) {
                    free_node->block_id = rem_block;
                    free_node->modid = -1;
                    free_node->next = node->next;
                    free_node->prev = node;
                    node->next->prev = free_node;
                    node->next = free_node;
                    if (sub_3208() != 0)
                        sub_3264(rem_block);
                }
                return modid;
            }
        }
    }
    return 0x800200d9;
}

int sceUtility_private_BBCD1885(SceUID modid)
{
    SceUtilityDialogMemNode *node;
    int ret;

    if (g_dialog_mem_inited == 0)
        return 0x80110001;

    for (node = g_dialog_mem_head.next; node != &g_dialog_mem_head; node = node->next) {
        if (node->modid == modid) {
            ret = sceKernelUnloadModule(modid);
            if (ret < 0)
                return ret;
            node->modid = -1;
            if (sub_3208() != 0)
                sub_3264(node->block_id);
            return ret;
        }
    }
    return 0x8002013e;
}

static int sub_3208(void)
{
    if (!sceKernelIsToolMode())
        return 0;
    if (sceKernelIsDevelopmentToolMode())
        return (sceKernelDipsw(0x19) == 0) ? 1 : 0;
    return (sceKernelDipsw(0x18) != 0) ? 1 : 0;
}

static void sub_3264(SceUID block_id)
{
    u32 info[14];
    info[0] = 0x38;
    if (sceKernelQueryMemoryBlockInfo(block_id, info) >= 0)
        sceKernelMemset32((void *)info[10], g_dialog_mem_time, info[11]);
}

/* NetModule / UsbModule / AvModule / LoadModule */
static int sub_3458(void)
{
    g_net_mod_flags = 0;
    g_net_block_id = -1;
    return 0;
}

int sceUtilityLoadNetModule(u32 id)
{
    int ret = sceUtilityLoadModule(id - 1 + 0x100);
    if (ret < 0 && ret != (int)0x80111102 && ret != (int)0x80111103)
        return ret;
    return 0;
}

int sceUtilityUnloadNetModule(u32 id)
{
    int ret = sceUtilityUnloadModule(id - 1 + 0x100);
    if (ret < 0 && ret != (int)0x80111102 && ret != (int)0x80111103)
        return ret;
    return 0;
}

static int sub_3588(void)
{
    g_np_mutex = sceKernelCreateMutex("SceUtilityNpModule", 0, 0, NULL);
    return (g_np_mutex < 0) ? g_np_mutex : 0;
}

int sceUtility_private_C7D5983F(u32 id)
{
    return sub_4344(id);
}

int sceUtility_private_5C494AC6(u32 id)
{
    return sub_4444(id, 0);
}

static int sub_3614(const char *path, int mpid)
{
    u32 opt[5];
    opt[0] = 0x14;
    opt[1] = mpid;
    opt[2] = mpid;
    opt[3] = 0;
    ((u8 *)opt)[16] = 0;
    ((u8 *)opt)[17] = 1;
    return sceKernelLoadModuleForKernel(path, 0, opt);
}

static int sub_3654(SceUID modid)
{
    int status, ret;
    ret = sceKernelStopModule(modid, 0, NULL, &status, NULL);
    if (ret >= 0)
        ret = sceKernelUnloadModule(modid);
    return ret;
}

static int sub_37d0(void)
{
    g_usb_mutex = sceKernelCreateMutex("SceUtilityUsbModule", 0, 0, NULL);
    return (g_usb_mutex < 0) ? g_usb_mutex : 0;
}

int sceUtilityLoadUsbModule(u32 id)
{
    if (id < 1 || id > 5)
        return 0x80110201;
    return sceUtilityLoadModule(id - 1 + 0x200);
}

int sceUtilityUnloadUsbModule(u32 id)
{
    if (id < 1 || id > 5)
        return 0x80110201;
    return sceUtilityUnloadModule(id - 1 + 0x200);
}

static int sub_3e50(void)
{
    g_av_mutex = sceKernelCreateMutex("SceUtilityAvModule", 0, 0, NULL);
    return (g_av_mutex < 0) ? g_av_mutex : 0;
}

int sceUtilityLoadAvModule(u32 id)
{
    if (id > 7)
        return 0x80110301;
    return sceUtilityLoadModule(id + 0x300);
}

int sceUtilityUnloadAvModule(u32 id)
{
    if (id > 7)
        return 0x80110301;
    return sceUtilityUnloadModule(id + 0x300);
}

static int sub_3f2c(void)
{
    g_updown_mutex = sceKernelCreateMutex("SceUtilityUpdownModule", 0, 0, NULL);
    return (g_updown_mutex < 0) ? g_updown_mutex : 0;
}

static int sub_3f78(u32 id, int a1)
{
    (void)id;
    (void)a1;
    return 0;
}

static int sub_3fe8(u32 id, int a1)
{
    (void)id;
    (void)a1;
    return 0;
}

static int sub_42f8(void)
{
    return 0;
}

static int sub_4344(u32 id)
{
    (void)id;
    return 0;
}

static int sub_43f8(void)
{
    return 0;
}

static int sub_4444(u32 id, int a1)
{
    (void)id;
    (void)a1;
    return 0;
}

static int sub_44b4(void)
{
    g_mod_mutex = sceKernelCreateMutex("SceUtilityModule", 0, 0, NULL);
    return (g_mod_mutex < 0) ? g_mod_mutex : 0;
}

int sceUtility_private_7EBD6208(u32 id)
{
    return sub_3f78(id, 0);
}

int sceUtility_private_40FBA532(u32 id)
{
    return sub_3fe8(id, 0);
}

static SceUtilityModEntry *get_mod_table(u32 id, u32 *out_idx, u32 *out_count)
{
    u32 cat = id >> 8;
    u32 idx = id & 0xFF;

    switch (cat) {
    case 1:
        if (idx == 0 || idx > 25) return NULL;
        *out_idx = idx - 1;
        *out_count = 25;
        return g_mods_01;
    case 2:
        if (idx == 0 || idx > 5) return NULL;
        *out_idx = idx - 1;
        *out_count = 5;
        return g_mods_02;
    case 3:
        if (idx == 0 || idx > 11) return NULL;
        *out_idx = idx - 1;
        *out_count = 11;
        return g_mods_03;
    case 4:
        if (idx == 0 || idx > 2) return NULL;
        *out_idx = idx - 1;
        *out_count = 2;
        return g_mods_04;
    case 5:
        if (idx == 0 || idx > 3) return NULL;
        *out_idx = idx - 1;
        *out_count = 3;
        return g_mods_05;
    case 6:
        if (idx == 0 || idx > 2) return NULL;
        *out_idx = idx - 1;
        *out_count = 2;
        return g_mods_06;
    default:
        return NULL;
    }
}

static int sub_4540(u32 id)
{
    u32 idx, count;
    SceUtilityModEntry *tab = get_mod_table(id, &idx, &count);
    SceUID modid;

    if (tab == NULL)
        return 0x80111101;
    if (tab[idx].state_and_modid > 0)
        return 0x80111102;

    modid = sub_3614(tab[idx].path, tab[idx].mpid);
    if (modid < 0)
        return modid;
    tab[idx].state_and_modid = modid;
    return 0;
}

static int sub_4720(u32 id)
{
    u32 idx, count;
    SceUtilityModEntry *tab = get_mod_table(id, &idx, &count);
    int ret;

    if (tab == NULL)
        return 0x80111101;
    if (tab[idx].state_and_modid <= 0)
        return 0x80111103;

    ret = sub_3654(tab[idx].state_and_modid);
    if (ret >= 0)
        tab[idx].state_and_modid = 0;
    return ret;
}

int sceUtilityLoadModule(u32 id)
{
    int ret;
    sceKernelLockMutex(g_mod_mutex, 1, NULL);
    ret = sub_4540(id);
    sceKernelUnlockMutex(g_mod_mutex, 1);
    return ret;
}

int sceUtilityUnloadModule(u32 id)
{
    int ret;
    sceKernelLockMutex(g_mod_mutex, 1, NULL);
    ret = sub_4720(id);
    sceKernelUnlockMutex(g_mod_mutex, 1);
    return ret;
}

static int sub_4aa8(int count, const u32 *ids)
{
    int i, ret = 0;
    for (i = 0; i < count; i++) {
        ret = sub_4540(ids[i]);
        if (ret < 0 && ret != (int)0x80111102)
            break;
        ret = 0;
    }
    return ret;
}

static int sub_4b8c(int count, const u32 *ids)
{
    int i, ret = 0;
    for (i = count - 1; i >= 0; i--) {
        ret = sub_4720(ids[i]);
        if (ret < 0 && ret != (int)0x80111103)
            break;
        ret = 0;
    }
    return ret;
}

int sceUtility_private_3B6D7CED(int count, const u32 *ids)
{
    int ret;
    sceKernelLockMutex(g_mod_mutex, 1, NULL);
    ret = sub_4aa8(count, ids);
    sceKernelUnlockMutex(g_mod_mutex, 1);
    return ret;
}

int sceUtility_private_CF6799A3(int count, const u32 *ids)
{
    int ret;
    sceKernelLockMutex(g_mod_mutex, 1, NULL);
    ret = sub_4b8c(count, ids);
    sceKernelUnlockMutex(g_mod_mutex, 1);
    return ret;
}

int sceUtility_private_390A738F(u32 id)
{
    return sceUtilityLoadModule(id);
}

int sceUtility_private_48B0FBCD(u32 id)
{
    return sceUtilityUnloadModule(id);
}

/* Public Dialog Wrappers */
int sceUtilityGameSharingInitStart(void *params) { return sub_011c(3, params); }
int sceUtilityGameSharingShutdownStart(void) { return sub_088c(3); }
int sceUtilityGameSharingUpdate(int speed) { return sub_1224(3, speed); }
int sceUtilityGameSharingGetStatus(void) { return sub_12ac(3); }

int sceUtilityNetconfInitStart(void *params) { return sub_011c(4, params); }
int sceUtilityNetconfShutdownStart(void) { return sub_088c(4); }
int sceUtilityNetconfUpdate(int speed) { return sub_1224(4, speed); }
int sceUtilityNetconfGetStatus(void) { return sub_12ac(4); }

int sceUtilitySavedataInitStart(void *params) { return sub_011c(1, params); }
int sceUtilitySavedataShutdownStart(void) { return sub_088c(1); }
int sceUtilitySavedataUpdate(int speed) { return sub_1224(1, speed); }
int sceUtilitySavedataGetStatus(void) { return sub_12ac(1); }

int sceUtilityMsgDialogInitStart(void *params) { return sub_011c(5, params); }
int sceUtilityMsgDialogShutdownStart(void) { return sub_088c(5); }
int sceUtilityMsgDialogUpdate(int speed) { return sub_1224(5, speed); }
int sceUtilityMsgDialogGetStatus(void) { return sub_12ac(5); }
int sceUtilityMsgDialogAbort(void) { return sub_12f4(5); }

int sceUtilityOskInitStart(void *params) { return sub_011c(6, params); }
int sceUtilityOskShutdownStart(void) { return sub_088c(6); }
int sceUtilityOskUpdate(int speed) { return sub_1224(6, speed); }
int sceUtilityOskGetStatus(void) { return sub_12ac(6); }

static int sub_559c(void)
{
    return 0;
}

int sceUtility_private_B08B2B48(void)
{
    return 0;
}

int sceUtility_DDE5389D(void *params) { return sub_011c(7, params); }
int sceUtility_149A7895(void) { return sub_088c(7); }
int sceUtility_4A833BA4(int speed) { return sub_1224(7, speed); }
int sceUtility_A50E5B30(void) { return sub_12ac(7); }

int sceUtility_943CBA46(void *params) { return sub_011c(8, params); }
int sceUtility_0F3EEAAC(void) { return sub_088c(8); }
int sceUtility_147F7C85(int speed) { return sub_1224(8, speed); }
int sceUtility_16A1A8D8(void) { return sub_12ac(8); }

int sceUtility_1281DA8E(void *params) { return sub_011c(9, params); }
int sceUtility_5EF1C24A(void) { return sub_088c(9); }
int sceUtility_A03D29BA(int speed) { return sub_1224(9, speed); }
int sceUtility_C4700FA3(void) { return sub_12ac(9); }

int sceUtility_24AC31EB(void *params) { return sub_011c(17, params); }
int sceUtility_32E32DCB(void) { return sub_088c(17); }
int sceUtility_4AECD179(int speed) { return sub_1224(17, speed); }
int sceUtility_B57E95D9(void) { return sub_12ac(17); }
int sceUtility_180F7B62(void) { return sub_12f4(17); }

int sceUtility_private_756A93B3(void *params) { return sub_011c(10, params); }
int sceUtility_private_26D0B065(void) { return sub_088c(10); }
int sceUtility_private_CE484145(int speed) { return sub_1224(10, speed); }
int sceUtility_private_981FFD72(void) { return sub_12ac(10); }

int sceUtility_0251B134(void *params) { return sub_011c(11, params); }
int sceUtility_F9E0008C(void) { return sub_088c(11); }
int sceUtility_AB083EA9(int speed) { return sub_1224(11, speed); }
int sceUtility_D81957B7(void) { return sub_12ac(11); }

int sceUtility_16D02AF0(void *params) { return sub_011c(12, params); }
int sceUtility_E19C97D6(void) { return sub_088c(12); }
int sceUtility_F3FBC572(int speed) { return sub_1224(12, speed); }
int sceUtility_86ABDB1B(void) { return sub_12ac(12); }

int sceUtility_42071A83(void *params) { return sub_011c(13, params); }
int sceUtility_D17A0573(void) { return sub_088c(13); }
int sceUtility_D852CDCE(int speed) { return sub_1224(13, speed); }
int sceUtility_89317C8F(void) { return sub_12ac(13); }

int sceUtility_4B0A8FE5(void *params) { return sub_011c(14, params); }
int sceUtility_06A48659(void) { return sub_088c(14); }
int sceUtility_A084E056(int speed) { return sub_1224(14, speed); }
int sceUtility_2B96173B(void) { return sub_12ac(14); }

int sceUtility_81C44706(void *params) { return sub_011c(15, params); }
int sceUtility_E7B778D8(void) { return sub_088c(15); }
int sceUtility_6F56F9CF(int speed) { return sub_1224(15, speed); }
int sceUtility_8326AB05(void) { return sub_12ac(15); }

int sceUtility_DA97F1AA(void *params) { return sub_011c(16, params); }
int sceUtility_54A5C62F(void) { return sub_088c(16); }
int sceUtility_B8592D5F(int speed) { return sub_1224(16, speed); }
int sceUtility_3AAD51DC(void) { return sub_12ac(16); }

int sceUtility_A7BB7C67(void *params) { return sub_011c(18, params); }
int sceUtility_C130D441(void) { return sub_088c(18); }
int sceUtility_0940A1B9(int speed) { return sub_1224(18, speed); }
int sceUtility_094198B8(void) { return sub_12ac(18); }

int sceUtility_3A15CD0A(void *params) { return sub_011c(19, params); }
int sceUtility_9F313D14(void) { return sub_088c(19); }
int sceUtility_D23665F4(int speed) { return sub_1224(19, speed); }
int sceUtility_D4C2BD73(void) { return sub_12ac(19); }
int sceUtility_0E0C27AF(void) { return sub_12f4(19); }

int sceUtility_B0FB7FF5(void *params) { return sub_0984(15, params); }
int sceUtility_86A03A27(void *params) { return sub_0984(11, params); }

int sceUtilityHtmlViewerInitStart(void *params) { return sub_20ec(1, params); }
int sceUtilityHtmlViewerShutdownStart(void) { return sub_2350(1); }
int sceUtilityHtmlViewerUpdate(int speed) { return sub_2680(1, speed); }
int sceUtilityHtmlViewerGetStatus(void) { return sub_26f8(1); }

void sceUtility_3AD50AE7(void) {}
void sceUtility_BC6B6296(void) {}
void sceUtility_417BED54(void) {}
void sceUtility_B6CEE597(void) {}
void sceUtility_2995D020(void) {}
void sceUtility_B62A4061(void) {}
void sceUtility_ED0FAD38(void) {}
void sceUtility_88BC7406(void) {}

/* SystemParam & Registry */
static int sub_68f8(const char *dir, const char *name, u32 val)
{
    u32 reg = 0, cat = 0;
    int ret;

    ret = sub_8f3c(&reg);
    if (ret == 0) {
        ret = sceRegOpenCategory(reg, dir, 2, &cat);
        if (ret == 0) {
            ret = sceRegSetKeyValue(cat, name, &val, 4);
            if (ret == 0)
                ret = sceRegFlushCategory(cat);
            sceRegCloseCategory(cat);
        }
        sub_8f6c(reg);
    }
    return ret;
}

static int sub_6a88(const char *dir, const char *name, u32 *val, u32 defval)
{
    u32 reg = 0, cat = 0, kh = 0, type = 0;
    SceSize size = 0;
    int ret;

    ret = sub_8f3c(&reg);
    if (ret == 0) {
        ret = sceRegOpenCategory(reg, dir, 1, &cat);
        if (ret == 0) {
            ret = sceRegGetKeyInfo(cat, name, &kh, &type, &size);
            if (ret == 0)
                ret = sceRegGetKeyValue(cat, kh, val, 4);
            sceRegCloseCategory(cat);
        }
        sub_8f6c(reg);
    }
    if (ret < 0)
        *val = defval;
    return ret;
}

static int sub_6bec(const char *dir, const char *name, const char *str, int size)
{
    u32 reg = 0, cat = 0;
    int ret;
    char buf[128];

    memset(buf, 0, sizeof(buf));
    strncpy(buf, str, size);
    ret = sub_8f3c(&reg);
    if (ret == 0) {
        ret = sceRegOpenCategory(reg, dir, 2, &cat);
        if (ret == 0) {
            ret = sceRegSetKeyValue(cat, name, buf, size);
            if (ret == 0)
                ret = sceRegFlushCategory(cat);
            sceRegCloseCategory(cat);
        }
        sub_8f6c(reg);
    }
    return ret;
}

static int sub_6ce8(const char *dir, const char *name, char *buf, int size, const char *defstr)
{
    u32 reg = 0, cat = 0, kh = 0, type = 0;
    SceSize ksize = 0;
    int ret;

    ret = sub_8f3c(&reg);
    if (ret == 0) {
        ret = sceRegOpenCategory(reg, dir, 1, &cat);
        if (ret == 0) {
            ret = sceRegGetKeyInfo(cat, name, &kh, &type, &ksize);
            if (ret == 0)
                ret = sceRegGetKeyValue(cat, kh, buf, size);
            sceRegCloseCategory(cat);
        }
        sub_8f6c(reg);
    }
    if (ret < 0 && defstr != NULL) {
        strncpy(buf, defstr, size);
        buf[size - 1] = '\0';
    }
    return ret;
}

static int sub_621c(u32 id, u32 val)
{
    switch (id) {
    case 2:
        if (val > 1) return 0x80110102;
        return sub_68f8("/CONFIG/SYSTEM/NET", "adhoc_channel", val);
    case 3:
        if (val > 11) return 0x80110102;
        return sub_68f8("/CONFIG/DATE", "date_format", val);
    case 4:
        if (val > 2) return 0x80110102;
        return sub_68f8("/CONFIG/DATE", "time_format", val);
    case 5:
        return sub_68f8("/CONFIG/DATE", "time_zone", val);
    case 6:
        if (val > 1) return 0x80110102;
        return sub_68f8("/CONFIG/DATE", "summer_time", val);
    case 7:
        if (val > 11) return 0x80110102;
        return sub_68f8("/CONFIG/SYSTEM/XMB", "language", val);
    case 8:
        if (val > 1) return 0x80110102;
        return sub_68f8("/CONFIG/SYSTEM/XMB", "button_assign", val);
    case 9:
        if (val > 1) return 0x80110102;
        return sub_68f8("/CONFIG/MUSIC", "wma_play", val);
    case 10:
        if (val > 1) return 0x80110102;
        return sub_68f8("/CONFIG/BROWSER", "flash_activated", val);
    default:
        return 0x80110101;
    }
}

static int sub_651c(u32 id, u32 *val)
{
    switch (id) {
    case 2:
        return sub_6a88("/CONFIG/SYSTEM/NET", "adhoc_channel", val, 0);
    case 3:
        return sub_6a88("/CONFIG/DATE", "date_format", val, 0);
    case 4:
        return sub_6a88("/CONFIG/DATE", "time_format", val, 0);
    case 5:
        return sub_6a88("/CONFIG/DATE", "time_zone", val, 0);
    case 6:
        return sub_6a88("/CONFIG/DATE", "summer_time", val, 0);
    case 7:
        return sub_6a88("/CONFIG/SYSTEM/XMB", "language", val, 1);
    case 8:
        return sub_6a88("/CONFIG/SYSTEM/XMB", "button_assign", val, 0);
    case 9:
        return sub_6a88("/CONFIG/MUSIC", "wma_play", val, 0);
    case 10:
        return sub_6a88("/CONFIG/BROWSER", "flash_activated", val, 0);
    default:
        return 0x80110101;
    }
}

int sceUtilitySetSystemParamInt(u32 id, u32 val)
{
    return sub_621c(id, val);
}

int sceUtilityGetSystemParamInt(u32 id, u32 *val)
{
    return sub_651c(id, val);
}

static int sub_6e68(u32 id, const char *str)
{
    if (id != 1)
        return 0x80110101;
    if (str == NULL)
        return 0x80110104;
    return sub_6bec("/CONFIG/SYSTEM", "owner_name", str, 0x80);
}

static int sub_7034(u32 id, char *str, int len)
{
    if (id != 1)
        return 0x80110101;
    if (str == NULL)
        return 0x80110104;
    if (len < 0x80)
        return 0x80110103;
    return sub_6ce8("/CONFIG/SYSTEM", "owner_name", str, 0x80, "");
}

int sceUtilitySetSystemParamString(u32 id, const char *str)
{
    return sub_6e68(id, str);
}

int sceUtilityGetSystemParamString(u32 id, char *str, int len)
{
    return sub_7034(id, str, len);
}

/* NetParam & Kermit WLAN sync */
int sceUtilityCheckNetParam(int id)
{
    return sub_7a7c(id);
}

int sceUtility_netparam_internal_6B1BC62C(void *buf)
{
    return sub_7b94(buf);
}

int sceUtility_netparam_internal_61D0686E(int id)
{
    return sub_7cd0(id);
}

int sceUtility_netparam_internal_4CB183A4(int dst, int src)
{
    return sub_8048(dst, src);
}

int sceUtilityGetNetParam(int id, int param, void *data)
{
    return sub_7e60(id, param, data);
}

int sceUtility_netparam_internal_67C2105B(int id, int param, void *data)
{
    return sub_7e60(id, param, data);
}

int sceUtility_netparam_internal_5D63AA06(int id, int param, const void *data)
{
    return sub_7d50(id, param, data);
}

int sceUtility_netparam_internal_239F260D(int id, int param, const void *data)
{
    return sub_7d50(id, param, data);
}

int sceUtility_ECE1D3E5(int id)
{
    return sub_7ae4(id);
}

int sceUtility_28D35634(int id, int param, const void *data)
{
    return sub_7d50(id, param, data);
}

int sceUtility_70267ADF(int id)
{
    return sub_7b3c(id);
}

int sceUtility_EF3582B2(int dst, int src)
{
    return sub_8048(dst, src);
}

int sceUtility_netparam_internal_02598DB8(int a0, int a1)
{
    return sub_80ec(a0, a1);
}

int sceUtility_netparam_internal_09905636(int id)
{
    return sub_817c(id);
}

int sceUtility_netparam_internal_6D77B975(void)
{
    return sub_81d0();
}

int sceUtility_4FED24D8(int id)
{
    return sub_8200(id);
}

static int sub_7a7c(int idx)
{
    char dir[48];
    int ret = sub_8df0(dir, idx);
    if (ret < 0)
        return ret;
    return sub_8e60(dir, 1);
}

static int sub_7ae4(int idx)
{
    if (g_netparam_locked != 0)
        return 0x80110602;
    g_netparam_locked = idx;
    return 0;
}

static int sub_7b3c(int idx)
{
    if (g_netparam_locked != idx)
        return 0x80110602;
    g_netparam_locked = 0;
    return 0;
}

static int sub_7b94(void *buf)
{
    int idx = 0;
    int ret = sub_8d2c(&idx);
    if (ret < 0)
        return ret;
    if (buf != NULL)
        *(int *)buf = idx;
    return 0;
}

static int sub_7cd0(int idx)
{
    char dir[48];
    int ret = sub_8df0(dir, idx);
    if (ret < 0)
        return ret;
    return sub_7f8c(dir, idx);
}

static int sub_7d50(int idx, int param, const void *data)
{
    return sub_830c(idx, param, (void *)data, 1);
}

static int sub_7e60(int idx, int param, void *data)
{
    return sub_830c(idx, param, data, 0);
}

static int sub_7f8c(const char *dir, int idx)
{
    (void)dir;
    (void)idx;
    return 0;
}

static int sub_8048(int dst_idx, int src_idx)
{
    (void)dst_idx;
    (void)src_idx;
    return 0;
}

static int sub_80ec(int a0, int a1)
{
    (void)a0;
    (void)a1;
    return 0;
}

static int sub_817c(int idx)
{
    return sub_8ee8(idx);
}

static int sub_81d0(void)
{
    return sub_8c20();
}

static int sub_8200(int idx)
{
    return sub_825c(idx);
}

static int sub_825c(int idx)
{
    (void)idx;
    return 0;
}

static int sub_830c(int idx, int param, void *data, int set_mode)
{
    char dir[48];
    int ret;
    (void)param;

    if (data == NULL)
        return 0x80110604;
    ret = sub_8df0(dir, idx);
    if (ret < 0)
        return ret;
    if (set_mode)
        return sub_68f8(dir, "conf_serial_no", *(u32 *)data);
    return sub_6a88(dir, "conf_serial_no", (u32 *)data, 0);
}

static int sub_8c20(void)
{
    return 0;
}

static int sub_8d2c(int *idx_p)
{
    if (idx_p != NULL)
        *idx_p = 1;
    return 0;
}

static int sub_8df0(char *out_dir, int idx)
{
    if (idx < 0 || idx > 24)
        return 0x80110601;
    snprintf(out_dir, 48, "/CONFIG/NETWORK/INFRASTRUCTURE/%d", idx);
    return 0;
}

static int sub_8e60(const char *dir, int a1)
{
    u32 reg = 0, cat = 0;
    int ret;
    (void)a1;

    ret = sub_8f3c(&reg);
    if (ret == 0) {
        ret = sceRegOpenCategory(reg, dir, 1, &cat);
        if (ret == 0)
            sceRegCloseCategory(cat);
        sub_8f6c(reg);
    }
    return (ret < 0) ? 0x80110601 : 0;
}

static int sub_8ee8(int idx)
{
    (void)idx;
    return 0;
}

static int sub_8f3c(u32 *reg_p)
{
    u32 param[8];
    memset(param, 0, sizeof(param));
    param[0] = 1;
    param[1] = 7;
    strncpy((char *)&param[2], "/system", 8);
    return sceRegOpenRegistry(param, 2, reg_p);
}

static int sub_8f6c(u32 reg)
{
    int ret = sceRegFlushRegistry(reg);
    if (ret == 0)
        ret = sceRegCloseRegistry(reg);
    return ret;
}

static int sub_90e4(int mode)
{
    return sub_922c(9, (mode != 0) ? 3 : 4, NULL);
}

int sceUtility_private_74E69D03(void *buf)
{
    return sub_922c(9, 1, buf);
}

static int sub_91b0(void)
{
    sub_90e4(0);
    sub_9270(3, 0, NULL);
    return 0;
}

static void sub_9204(void)
{
    g_status_flags = 0;
}

static int sub_922c(int fac, int cmd, void *arg)
{
    SceKermitRequest *req;
    u64 resp = 0;
    int ret;

    req = (SceKermitRequest *)KERMIT_UNCACHED(&g_kermit_buf[0]);
    req->cmd = cmd;
    req->args[0] = (u32)arg;
    ret = sceKermit_driver_36666181(req, fac, cmd, 1, 0, &resp);
    if (ret < 0)
        return ret;
    return (int)resp;
}

static int sub_9270(int fac, int cmd, void *arg)
{
    return sub_922c(fac, cmd, arg);
}
