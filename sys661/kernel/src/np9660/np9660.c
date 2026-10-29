#include <stdint.h>
#include <stddef.h>

typedef int32_t s32;
typedef uint32_t u32;
typedef int16_t s16;
typedef uint16_t u16;
typedef int8_t s8;
typedef uint8_t u8;
typedef int64_t s64;
typedef uint64_t u64;
typedef int SceUID;
typedef unsigned int SceSize;
typedef int64_t SceOff;

#define NOINLINE __attribute__((noinline))

/* Imports */
extern int sceIoOpen(const char *file, int flags, int mode);
extern SceOff sceIoLseek(int fd, SceOff offset, int whence);
extern int sceIoRead(int fd, void *data, SceSize size);
extern int sceIoClose(int fd);
extern int sceIoDevctl(const char *dev, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen);
extern int sceIoAssign(const char *dev1, const char *dev2, const char *dev3, int mode, void *unk1, long unk2);
extern int sceIoUnassign(const char *dev);
extern int sceIoAddDrv(const void *drv);
extern int sceIoDelDrv(const char *drv_name);

extern void *sceKernelMemcpy(void *dst, const void *src, u32 size);
extern void *sceKernelMemset(void *dst, int ch, u32 size);
extern void *sceKernelMemmove(void *dst, const void *src, u32 size);
extern int sceKernelGetCompiledSdkVersion(void);
extern int sceKernelQueryMemoryPartitionInfo(int pid, void *info);
extern int sceKernelGetUIDcontrolBlock(SceUID uid, void **cb);
extern void *sceKernelGetGameInfo(void);
extern int SysMemForKernel_F3BDB718(const void *id);
extern int sceKernelSetQTGP3(const void *qtgp3);

extern void *memset(void *s, int c, u32 n);
extern u16 *look_ctype_table(void);
extern u32 strlen(const char *s);
extern int strncmp(const char *s1, const char *s2, u32 n);
extern int memcmp(const void *s1, const void *s2, u32 n);
extern char *strncpy(char *dst, const char *src, u32 n);
extern int strcmp(const char *s1, const char *s2);
extern int snprintf(char *str, u32 size, const char *fmt, ...);

extern int sceKernelCpuSuspendIntr(void);
extern void sceKernelCpuResumeIntr(int intr);
extern void *sceKernelInitDiscImage(void);
extern int sceKernelSearchModuleByName(const char *name);

extern int sceKernelReceiveMbx(SceUID mbxid, void **pmp, u32 *timeout);
extern int sceKernelSetEventFlag(SceUID evid, u32 bits);
extern int sceKernelWaitThreadEnd(SceUID thid, u32 *timeout);
extern int sceKernelWaitEventFlagCB(SceUID evid, u32 bits, int wait, u32 *outBits, u32 *timeout);
extern u32 sceKernelGetSystemTimeLow(void);
extern int sceKernelWaitEventFlag(SceUID evid, u32 bits, int wait, u32 *outBits, u32 *timeout);
extern SceUID sceKernelCreateThread(const char *name, int (*entry)(SceSize, void *), int initPriority, int stackSize, u32 attr, void *opt);
extern SceUID sceKernelCreateEventFlag(const char *name, int attr, int bits, void *opt);
extern int sceKernelGetThreadmanIdType(SceUID uid);
extern int sceKernelTryAllocateFpl(SceUID uid, void **data);
extern int sceKernelUnlockMutex(SceUID mutexid, int unlockCount);
extern int sceKernelChangeThreadPriority(SceUID thid, int priority);
extern int sceKernelClearEventFlag(SceUID evid, u32 bits);
extern SceUID sceKernelCreateMbx(const char *name, u32 attr, void *opt);
extern s64 sceKernelGetSystemTimeWide(void);
extern int sceKernelIsUserModeThread(void);
extern int sceKernelDeleteMbx(SceUID mbxid);
extern int sceKernelCancelMutex(SceUID mutexid, int newcount, int *numWaitThreads);
extern int sceKernelGetThreadCurrentPriority(void);
extern int sceKernelDeleteThread(SceUID thid);
extern int sceKernelExitThread(int status);
extern int sceKernelLockMutex(SceUID mutexid, int lockCount, u32 *timeout);
extern SceUID sceKernelCreateMutex(const char *name, u32 attr, int initCount, void *opt);
extern int sceKernelExtendKernelStack(u32 size, int (*cb)(void *), void *arg);
extern SceUID sceKernelCreateFpl(const char *name, int part, int attr, u32 size, u32 blocks, void *opt);
extern int sceKernelNotifyCallback(SceUID cb, int arg2);
extern int sceKernelCancelEventFlag(SceUID evid, u32 setpattern, int *numWaitThreads);
extern int sceKernelDelayThread(u32 delay);
extern SceUID sceKernelCreateCallback(const char *name, int (*func)(int, int, void *), void *arg);
extern int sceKernelSendMbx(SceUID mbxid, void *msg);
extern int sceKernelDeleteFpl(SceUID uid);
extern int sceKernelDeleteCallback(SceUID cb);
extern int sceKernelDeleteEventFlag(SceUID evid);
extern int sceKernelStartThread(SceUID thid, SceSize arglen, void *argp);
extern int sceKernelDeleteMutex(SceUID mutexid);

extern void sceKernelDcacheWritebackRange(const void *p, unsigned int size);

extern int sceDrmBBCipherUpdate(void *ckey, void *buf, int size);
extern int sceDrmBBCipherInit(void *ckey, int type, int mode, void *header_key, void *version_key, u32 seed);
extern int sceDrmBBMacInit(void *mkey, int type);
extern int sceDrmBBMacUpdate(void *mkey, const void *buf, int size);
extern int sceAmctrl_driver_9227EA79(void *mkey, const void *buf, int size);
extern int sceDrmBBCipherFinal(void *ckey);
extern int sceAmctrl_driver_E04ADD4C(void *ckey, void *buf, int size);
extern int sceDrmBBMacFinal2(void *mkey, const void *hash, const void *vkey);

extern int sceDdrdbHash(const void *src, int size, void *digest);
extern int sceDdrdbSigvry(const void *pub, const void *digest, const void *sig);

extern int sceNpDrmGetFixedKey(void *key, const char *content_id, int type);
extern int sceNpDrmGetIDps(void *idps);
extern int sceNpDrmGetVersionKey(void *key, const void *act, const void *rif, int type);

extern int sceKernelRegisterSysEventHandler(void *handler);
extern int sceKernelUnregisterSysEventHandler(void *handler);
extern int sceWlanDrv_driver_7DD3787C(void);

/* Forward declarations */
static NOINLINE int sub_00c8(int fd, int mode);
static NOINLINE int sub_0a34(void *buf, int size, void *key, int seed, void *mac, int flags);
static NOINLINE int sub_0ad0(void *a0, int a1, int a2);
static NOINLINE void sub_0c38(void);
static NOINLINE int sub_0c4c(void *a0, int a1);
static NOINLINE int sub_0dc0(void *buf, int size);
static NOINLINE int sub_0df0(void);
static NOINLINE int sub_0df8(void *vkey, const char *cid, int flags);
static NOINLINE int sub_10d8(void *buf, int size, void *hkey, void *vkey, int mode, int seed, int flag);
static NOINLINE int sub_1188(const void *buf, int size, const void *vkey, const void *hash, int type, int flag);
static NOINLINE int sub_1228(void *dst, u32 dst_len, const void *src, u32 *next_in);
static NOINLINE s64 sub_1b38(s64 a, s64 b);

static NOINLINE int sub_2408(int mode);
static NOINLINE int sub_2640(void);
static NOINLINE int sub_26a4(void);
static NOINLINE void sub_27e0(int stat);
static NOINLINE int sub_2b28(int mode);
static NOINLINE int sub_2bf0(void);
static NOINLINE int sub_2d28(void);
static NOINLINE int sub_2de4(int mode);
static NOINLINE int sub_2eec(int mode);
static NOINLINE int sub_303c(void);
static NOINLINE int sub_3048(int state, int force);
static NOINLINE int sub_322c(int ev_id, char *ev_name, void *param, int *result);
static NOINLINE int sub_3300(SceSize args, void *argp);
static NOINLINE int sub_3634(void);
static NOINLINE int sub_3640(void);
static NOINLINE int sub_368c(void);
static NOINLINE void sub_36f8(int val);
static NOINLINE int sub_3704(int cbid, int count, void *arg);
static NOINLINE int sub_38a8(int state);
static NOINLINE int sub_3a24(int state);
static NOINLINE int sub_3b28(void);
static NOINLINE int sub_3c14(void);
static NOINLINE void sub_3cd0(int val);
static NOINLINE int sub_3d04(void);
static NOINLINE int sub_3d10(void);
static NOINLINE int sub_3d18(void);
static NOINLINE int sub_3d38(int val);
static NOINLINE int sub_3d70(void);

static NOINLINE int sub_41b0(void *arg, SceOff ofs, int whence);
static NOINLINE int sub_43ec(void *arg);
static NOINLINE int sub_4428(int (*fn)(void *), void *arg);
static NOINLINE int sub_4500(void *arg);
static NOINLINE void sub_4ccc(int val);
static NOINLINE int sub_4cd8(void);
static NOINLINE int sub_4ef4(int unit, int *out);
static NOINLINE int sub_4fa4(int unit, void *out);
static NOINLINE int sub_5054(void);
static NOINLINE int sub_50c4(void);
static NOINLINE int sub_50fc(void *buf, int size, u32 pos_lo, u32 pos_hi);
static NOINLINE int sub_51a4(int flag);
static NOINLINE int sub_5218(void);
static NOINLINE int sub_5268(void);
static NOINLINE void sub_52a0(void);
static NOINLINE int sub_52ac(int *type_out);
static NOINLINE int sub_52cc(void);
static NOINLINE void sub_52d8(void *buf);
static NOINLINE void sub_5308(void);
static NOINLINE void sub_531c(int sectors);
static NOINLINE int sub_5330(int lba, int count);
static NOINLINE int sub_535c(void *buf, int size, int lba);
static NOINLINE int sub_538c(int lba);
static NOINLINE int sub_53a8(void *buf, int size, int lba);
static NOINLINE int sub_58c4(const char *name);
static NOINLINE int sub_5968(int unit, u32 cmd, void *indata, int inlen, void *outdata, int outlen);
static NOINLINE int sub_5af8(void);
static NOINLINE int sub_5b04(u32 *req);
static NOINLINE int sub_5d18(void *info);
static NOINLINE int sub_5fac(int idx);
static NOINLINE int sub_5ffc(void);
static NOINLINE int sub_6028(void);
static NOINLINE void sub_6098(void);
static NOINLINE void sub_60a4(void);
static NOINLINE void sub_60b0(void);
static NOINLINE const char *sub_60bc(void);
static NOINLINE int sub_60cc(int diff, int flag);
static NOINLINE int sub_63b4(int diff, int flag);
static NOINLINE int sub_66f8(int lba);
static NOINLINE int sub_678c(void);
static NOINLINE int sub_68c4(void);
static NOINLINE int sub_6988(int prio);
static NOINLINE int sub_69dc(void *dst, u32 dst_len, const void *src);
static NOINLINE int sub_6a98(SceSize args, void *argp);
static NOINLINE void sub_6b80(void);
static NOINLINE int sub_6bd4(int lba, int count, int flag, int *a3, int *t0);
static NOINLINE int sub_6c9c(int lba, int count, int *a2, int *a3);
static NOINLINE void sub_6d10(int lba);
static NOINLINE int sub_6d24(int a0, int a1, int a2, int *a3, int *t0);
static NOINLINE int sub_6e4c(int a0, int a1, int a2, int *a3);
static NOINLINE int sub_7004(int lba);
static NOINLINE int sub_703c(int lba, int count, int idx);
static NOINLINE int sub_7104(void *req);
static NOINLINE void sub_73b4(int lba);
static NOINLINE int sub_73c0(int a0, int a1, int a2, int a3, int t0);
static NOINLINE int sub_78d0(int a0, int a1, int a2, int a3, int t0);
static NOINLINE int sub_7ca4(int lba, int count);

/* Exports forward declarations */
int sceUmdMan_driver_65E2B3E0(void);
int sceUmd9660_driver_7EB57F56(void);
int sceUmd9660_driver_C0933C16(void);
int sceUmd9660_driver_887C3193(void);
void sceUmd9660_driver_3CC9CE54(void);
void sceUmd9660_driver_FE3A8B67(void);
int sceUmd_040A7090(int err);
int sceUmdActivate(int mode, const char *aliasname);
int sceUmdRegisterUMDCallBack(SceUID cbid);
int sceUmdUnRegisterUMDCallBack(SceUID cbid);
int sceUmdGetDriveStatus(void);
int sceUmdSetDriveStatus(int stat);
int sceUmdClearDriveStatus(int stat);
int sceUmdGetErrorStatus(void);
void sceUmdSetErrorStatus(int err);
int sceUmdCheckMedium(void);
int sceUmdDeactivate(int mode, const char *aliasname);
int sceUmdGetDiscInfo(u32 *info);
int sceUmdGetDriveStat(void);
int sceUmdGetErrorStat(void);
int sceUmdReplaceProhibit(void);
int sceUmdReplacePermit(void);
int sceUmdWaitDriveStat(int stat);
int sceUmdWaitDriveStatWithTimer(int stat, u32 timeout);
int sceUmdWaitDriveStatCB(int stat, u32 timeout);
int sceUmdCancelWaitDriveStat(void);
int sceUmdManRegisterInsertEjectUMDCallBack(int id, void *cb, void *arg);
int sceUmdManUnRegisterInsertEjectUMDCallBack(int id);
int sceUmdMan_driver_80D31D5D(void *a0);
int sceUmdMan_driver_34375DB0(int a0);
int sceNp9660_driver_8EF69DC6(const char *path, int a1);
int sceNp9660_driver_B925CA6C(void);
int sceNp9660_driver_7A05EB3C(void);

/* .rodata tables */
static const u8 g_ro_8690[12] = {
    0x10, 0x00, 0x00, 0x00, 0x11, 0x10, 0x7c, 0x11, 0x11, 0x7d, 0x00, 0x00
};

static const u8 g_npumd_pub_8784[40] = {
    0x01, 0x21, 0xea, 0x6e, 0xcd, 0xb2, 0x3a, 0x3e,
    0x23, 0x75, 0x67, 0x1c, 0x53, 0x62, 0xe8, 0xe2,
    0x8b, 0x1e, 0x78, 0x3b, 0x1a, 0x27, 0x32, 0x15,
    0x8b, 0x8c, 0xed, 0x98, 0x46, 0x6c, 0x18, 0xa3,
    0xac, 0x3b, 0x11, 0x06, 0xaf, 0xb4, 0xec, 0x3b,
};

static const u8 g_clz_tab_87d4[256] = {
    0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
};

static const char g_dummy_disc_id_8940[36] = "ULJS-00001|CCCD9317C815B9F5|0001";

/* .data */
static int g_data_89c0 = 2;
static int g_data_89c4 = 1;
static struct {
    int size;
    const char *name;
    int type_mask;
    int (*handler)(int, char *, void *, int *);
    int r28;
    int busy;
    void *next;
    int reserved[9];
} g_sysevent_89c8 = {
    0x40,
    "SceUmdMedia",
    0x00ffff00,
    sub_322c,
    0,
    0,
    NULL,
    { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};
static int g_data_8a08 = 0x0006c0bf;
static int g_data_8a0c = 0x0006c0bf;
static int g_data_8a10 = 0x70;
static int g_data_8a14 = 0x6ed;
static int g_data_8a18 = 0x23d;

/* .bss */
static int g_bss_8a20;
static int g_bss_8a24;
static void *g_bss_8a28;
static u8 g_bss_8a2c[64];
static u8 g_bss_8a6c[16];
static u8 g_bss_8a7c[16];
static char g_dmu_str_8a8c[8];
static SceUID g_mediaman_evf_8a94;
static int g_drive_status_8a98;
static int g_error_status_8a9c;
static int g_active_mode_8aa0;
static int g_activate_res_8aa4;
static SceUID g_umd_cbid_8aa8;
static char g_assign_dev_8aac[32];
static u32 g_umdman_8acc;
static SceUID g_mount_thid_8ad0;
static SceUID g_fatms_cbid_8ad4;
static int g_fatms_cb_reg_8ad8;
static SceUID g_umdman_evf_8adc;
static int g_umdman_stop_8ae0;
static int g_umdman_busy_8ae4;
static int g_umdman_state_8ae8;
static int g_ins_ej_id_8aec;
static int (*g_ins_ej_cb_8af0)(int, void *, int);
static void *g_ins_ej_arg_8af4;
static void (*g_umdman_cb_8af8)(int, void *);
static void *g_umdman_cb_arg_8afc;
static char g_iso_path_8b00[64];
static void *g_disc_img_8b40;
static int g_replace_prohibit_8b44;
static struct {
    int fd;              /* 0x00 */
    int open_count;      /* 0x04 */
    SceUID mutex;        /* 0x08 */
    SceUID fpl;          /* 0x0c */
    SceUID nc_fpl;       /* 0x10 */
    void *sector_buf;    /* 0x14 */
    void *nc_buf;        /* 0x18 */
    int total_sectors;   /* 0x1c */
    int disc_type;       /* 0x20 */
    int unk_24;          /* 0x24 */
    int block_basis;     /* 0x28 */
    int num_blocks;      /* 0x2c */
    int unk_30;          /* 0x30 */
    int mutex_owner_pri; /* 0x34 */
    u32 img_offset_lo;   /* 0x38 */
    u32 img_offset_hi;   /* 0x3c */
    u32 tbl_offset_lo;   /* 0x40 */
    u32 tbl_offset_hi;   /* 0x44 */
    int cached_lba;      /* 0x48 */
    int cached_blk;      /* 0x4c */
    int flags;           /* 0x50 */
    int header_flags;    /* 0x54 */
    int lba_param;       /* 0x58 */
    char disc_id[36];    /* 0x5c */
    int cur_lba;         /* 0x80 */
    int unk_84[4];       /* 0x84..0x90 */
    struct {
        int in_use;
        int lba;
    } handles[4];        /* 0x94..0xb0 */
} g_umd_ctx_8b48;

static int g_emu_8c00;
static int g_emu_8c04;
static s64 g_emu_time_8c08;
static int g_emu_lba_8c10;
static int g_emu_8c14;
static int g_emu_8c18;
static int g_emu_8c1c;
static int g_emu_8c20;
static u16 g_emu_8c24;
static u16 g_emu_8c26;
static u8 g_emu_8c28;
static u8 g_emu_8c29;
static int g_emu_8c2c;
static SceUID g_dec_thid_8c30;
static SceUID g_dec_mbx_8c34;
static SceUID g_dec_evf_8c38;
static u32 g_dec_msg_8c3c[8];
static int g_cache_8c5c[516];

/* 0x0000 */
int NOINLINE sub_0000(void)
{
    int mode = 0;
    int fd = sub_3b28();
    if (fd < 0) {
        return fd;
    }
    sceKernelMemset(g_bss_8a2c, 0, 64);
    int res = sub_00c8(fd, mode);
    if (res < 0) {
        return res;
    }
    return 0;
}

/* 0x00c8 */
static NOINLINE int sub_00c8(int fd, int mode)
{
    u8 hdr[256];
    u8 mkey[32];
    u8 ckey[32];
    u8 idps[16];
    u8 digest[20];

    sub_0c38();
    if (fd < 0) {
        return -1;
    }
    g_umd_ctx_8b48.fd = fd;
    SceOff pos = sceIoLseek(fd, 0, 0);
    if (pos < 0) {
        sceIoClose(fd);
        return (int)pos;
    }
    int rd = sceIoRead(fd, hdr, 0x100);
    if (rd < 0x28) {
        sceIoClose(fd);
        return -1;
    }
    u32 magic = *(u32 *)hdr;
    u32 pbp_off = 0;
    if (magic == 0x50425000U) {
        pbp_off = *(u32 *)(hdr + 0x24);
        if (sceIoLseek(fd, (SceOff)pbp_off, 0) < 0) {
            sceIoClose(fd);
            return -1;
        }
        if (sceIoRead(fd, hdr, 0x100) != 0x100) {
            sceIoClose(fd);
            return -1;
        }
    }
    if (memcmp(hdr, "NPUMDIMG", 8) != 0) {
        sceIoClose(fd);
        return (int)0x80550902;
    }
    if (sceDdrdbHash(hdr, 0xd8, digest) != 0) {
        sceIoClose(fd);
        return -1;
    }
    if (sceDdrdbSigvry(g_npumd_pub_8784, digest, hdr + 0xd8) != 0) {
        sceIoClose(fd);
        return (int)0x80550904;
    }
    int r = sub_0df8(g_bss_8a7c, (const char *)(hdr + 0x10), *(int *)(hdr + 0x08));
    if (r < 0) {
        sceIoClose(fd);
        return r;
    }
    sceDrmBBMacInit(mkey, 3);
    sceDrmBBMacUpdate(mkey, hdr, 0xc0);
    sceAmctrl_driver_9227EA79(mkey, hdr, 0);
    if (sceDrmBBMacFinal2(mkey, hdr + 0xc0, g_bss_8a7c) != 0) {
        sceIoClose(fd);
        return (int)0x80550903;
    }
    sceKernelMemcpy(g_bss_8a6c, hdr + 0xa0, 16);
    sceDrmBBCipherInit(ckey, 1, 2, g_bss_8a6c, g_bss_8a7c, 0);
    sceDrmBBCipherUpdate(ckey, hdr + 0x40, 0x60);
    sceDrmBBCipherFinal(ckey);

    if (strcmp((const char *)(hdr + 0x10), (const char *)(hdr + 0x40)) != 0) {
        sceIoClose(fd);
        return -1;
    }
    if (sceNpDrmGetIDps(idps) < 0) {
        sceIoClose(fd);
        return -1;
    }
    if (mode == 0) {
        SysMemForKernel_F3BDB718(hdr + 0x10);
        sceKernelSetQTGP3(hdr + 0x80);
    }
    sceKernelDelayThread(100);
    int med = sub_368c();
    sub_36f8(med);
    sub_4ccc(sub_3d70());
    sub_531c(*(int *)(hdr + 0x64) - *(int *)(hdr + 0x60) + 1);
    g_umd_ctx_8b48.block_basis = *(int *)(hdr + 0x0c);
    g_umd_ctx_8b48.header_flags = *(int *)(hdr + 0x08);
    g_umd_ctx_8b48.img_offset_lo = pbp_off;
    g_umd_ctx_8b48.img_offset_hi = 0;
    g_umd_ctx_8b48.tbl_offset_lo = pbp_off + *(u32 *)(hdr + 0x6c);
    g_umd_ctx_8b48.tbl_offset_hi = 0;
    int total_sec = g_umd_ctx_8b48.total_sectors;
    int basis = g_umd_ctx_8b48.block_basis;
    g_umd_ctx_8b48.num_blocks = (total_sec + basis - 1) / basis;
    sceKernelMemcpy(g_umd_ctx_8b48.disc_id, hdr + 0x40, 32);
    sub_4cd8();
    return 0;
}

/* 0x0a34 */
static NOINLINE int sub_0a34(void *buf, int size, void *key, int seed, void *mac, int flags)
{
    int r;
    if (!(flags & 2)) {
        r = sub_1188(buf, size, g_bss_8a7c, mac, 3, flags & 1);
        if (r < 0) {
            return r;
        }
    }
    if (!(flags & 4)) {
        r = sub_10d8(buf, size, key, g_bss_8a7c, 2, seed, flags & 1);
        if (r < 0) {
            return r;
        }
    }
    return 0;
}

/* 0x0ad0 */
static NOINLINE int sub_0ad0(void *a0, int a1, int a2)
{
    u8 entry[40];
    u8 *p = (u8 *)a0;
    for (int i = 0; i < a2; i++) {
        int r = sub_0c4c(entry, a1 + i);
        if (r < 0) {
            return r;
        }
        r = sub_1188(p, *(int *)(entry + 0x18), g_bss_8a7c, entry + 8, 3, 0);
        if (r < 0) {
            return r;
        }
        r = sub_10d8(p, *(int *)(entry + 0x18), g_bss_8a6c, g_bss_8a7c, 2, *(int *)(entry + 0x1c), 0);
        if (r < 0) {
            return r;
        }
        p += *(int *)(entry + 0x18);
    }
    return 0;
}

/* 0x0c38 */
static NOINLINE void sub_0c38(void)
{
    g_bss_8a24 = -1;
}

/* 0x0c4c */
static NOINLINE int sub_0c4c(void *a0, int a1)
{
    if (a1 < 0 || a1 >= g_umd_ctx_8b48.num_blocks || g_bss_8a28 == NULL) {
        return (int)0x80550901;
    }
    int grp = a1 >> 4;
    if (g_bss_8a24 != grp) {
        u64 off = ((u64)g_umd_ctx_8b48.tbl_offset_hi << 32 | g_umd_ctx_8b48.tbl_offset_lo) + ((u64)grp << 9);
        int rem = g_umd_ctx_8b48.num_blocks - (grp << 4);
        int sz = (rem < 16 ? rem : 16) << 5;
        int rd = sub_50fc(g_bss_8a28, sz, (u32)off, (u32)(off >> 32));
        if (rd != sz) {
            return rd < 0 ? rd : (int)0x80550901;
        }
        u32 *words = (u32 *)g_bss_8a28;
        int cnt = sz >> 5;
        for (int i = 0; i < cnt; i++, words += 8) {
            u32 w4 = words[4] ^ words[0] ^ words[1];
            u32 w5 = words[5] ^ words[1] ^ words[2];
            u32 w6 = words[6] ^ words[2] ^ words[3];
            u32 w7 = words[7] ^ words[3] ^ words[0];
            words[4] = w4;
            words[5] = w5;
            words[6] = w6;
            words[7] = w7;
        }
        g_bss_8a24 = grp;
    }
    u8 *src = (u8 *)g_bss_8a28 + ((a1 & 15) << 5);
    sceKernelMemcpy((u8 *)a0 + 8, src, 32);
    return 0;
}

/* 0x0dc0 */
static NOINLINE int sub_0dc0(void *buf, int size)
{
    if (size != 0x200) {
        return (int)0x80550901;
    }
    g_bss_8a28 = buf;
    g_bss_8a24 = -1;
    return 0;
}

/* 0x0df0 */
static NOINLINE int sub_0df0(void)
{
    return 0;
}

/* 0x0df8 */
static NOINLINE int sub_0df8(void *vkey, const char *cid, int flags)
{
    char path[64];
    u8 rif[160];
    u8 act[4160];

    if (strlen(cid) >= 0x25) {
        return (int)0x80550912;
    }
    sceKernelMemset(vkey, 0, 16);
    if (flags & 0x01000000) {
        int r = sceNpDrmGetFixedKey(vkey, cid, 0x01000000);
        if (r < 0) {
            return r;
        }
        if ((flags & 0x01000003) == 0x01000003) {
            return 0;
        }
    }
    snprintf(path, 0x3b, "ms0:/PSP/LICENSE/%s.rif", cid);
    int fd = sceIoOpen(path, 1, 0);
    if (fd < 0) {
        return (int)0x80550910;
    }
    int rd = sceIoRead(fd, rif, 0x98);
    sceIoClose(fd);
    if (rd < 0x98) {
        return (int)0x80550910;
    }
    if (strcmp((const char *)(rif + 0x10), cid) != 0) {
        return (int)0x80550912;
    }
    sceKernelMemset(act, 0, 0x1038);
    u32 acc_hi = *(u32 *)(rif + 0x0c);
    u32 acc_lo = *(u32 *)(rif + 0x08);
    if ((acc_hi | acc_lo) != 0) {
        int afd = sceIoOpen("flash2:/act.dat", 1, 0);
        if (afd < 0) {
            return (int)0x80550911;
        }
        int ard = sceIoRead(afd, act, 0x1038);
        sceIoClose(afd);
        if (ard != 0x1038) {
            return (int)0x80550911;
        }
    }
    return sceNpDrmGetVersionKey(vkey, act, rif, flags);
}

/* 0x10d8 */
static NOINLINE int sub_10d8(void *buf, int size, void *hkey, void *vkey, int mode, int seed, int flag)
{
    u8 ckey[32];
    int r = sceDrmBBCipherInit(ckey, 1, mode, hkey, vkey, (u32)seed);
    if (r != 0) {
        return r;
    }
    if (flag == 0) {
        r = sceDrmBBCipherUpdate(ckey, buf, size);
    } else {
        r = sceAmctrl_driver_E04ADD4C(ckey, buf, size);
    }
    if (r != 0) {
        return r;
    }
    return sceDrmBBCipherFinal(ckey);
}

/* 0x1188 */
static NOINLINE int sub_1188(const void *buf, int size, const void *vkey, const void *hash, int type, int flag)
{
    u8 mkey[32];
    int r = sceDrmBBMacInit(mkey, type);
    if (r != 0) {
        return r;
    }
    if (flag == 0) {
        r = sceDrmBBMacUpdate(mkey, buf, size);
    } else {
        r = sceAmctrl_driver_9227EA79(mkey, buf, size);
    }
    if (r != 0) {
        return r;
    }
    return sceDrmBBMacFinal2(mkey, hash, vkey);
}

/* 0x1228 */
static NOINLINE int sub_1228(void *dst, u32 dst_len, const void *src, u32 *next_in)
{
    u8 probs[2048];
    const u8 *in = (const u8 *)src;
    u8 *out = (u8 *)dst;
    u8 *out_end = out + dst_len;

    u8 bm = *in++;
    if (bm & 0x80) {
        return (int)0x80000108;
    }
    for (int i = 0; i < 2048; i += 64) {
        __asm__ volatile("cache 0x18, 0(%0)" :: "r"(probs + i) : "memory");
        for (int j = 0; j < 64; j++) {
            probs[i + j] = 0x80;
        }
    }
    u32 code = ((u32)in[0] << 24) | ((u32)in[1] << 16) | ((u32)in[2] << 8) | (u32)in[3];
    in += 4;
    u32 range = 0xffffffffU;
    while (out < out_end && range != 0) {
        if (range < 0x01000000U) {
            range <<= 8;
            code = (code << 8) | *in++;
        }
        u32 bound = (range >> 8) * probs[bm & 0x7f];
        if (code < bound) {
            range = bound;
            probs[bm & 0x7f] += (0x100 - probs[bm & 0x7f]) >> 3;
            *out++ = *in++;
        } else {
            code -= bound;
            range -= bound;
            probs[bm & 0x7f] -= probs[bm & 0x7f] >> 3;
            break;
        }
    }
    if (next_in != NULL) {
        *next_in = (u32)in;
    }
    return (int)(out - (u8 *)dst);
}

/* 0x1b38 */
static NOINLINE s64 sub_1b38(s64 a, s64 b)
{
    int neg = 0;
    u64 ua = (u64)a;
    u64 ub = (u64)b;
    if (a < 0) {
        ua = -ua;
        neg = !neg;
    }
    if (b < 0) {
        ub = -ub;
        neg = !neg;
    }
    u32 hi_b = (u32)(ub >> 32);
    u32 lo_b = (u32)ub;
    u64 q = 0;
    if (hi_b == 0) {
        u32 hi_a = (u32)(ua >> 32);
        u32 lo_a = (u32)ua;
        u32 b_top = lo_b >> 24 ? g_clz_tab_87d4[lo_b >> 24] + 24 :
                    (lo_b >> 16 ? g_clz_tab_87d4[lo_b >> 16] + 16 :
                    (lo_b >> 8 ? g_clz_tab_87d4[lo_b >> 8] + 8 : g_clz_tab_87d4[lo_b]));
        (void)b_top;
        u32 q1 = hi_a / lo_b;
        u32 r1 = hi_a % lo_b;
        u64 rem = ((u64)r1 << 32) | lo_a;
        u32 q0 = 0;
        for (int i = 31; i >= 0; i--) {
            if ((rem >> i) >= lo_b) {
                rem -= (u64)lo_b << i;
                q0 |= 1U << i;
            }
        }
        q = ((u64)q1 << 32) | q0;
    } else {
        for (int i = 31; i >= 0; i--) {
            if ((ua >> i) >= ub) {
                ua -= ub << i;
                q |= 1ULL << i;
            }
        }
    }
    return neg ? -(s64)q : (s64)q;
}

/* IoDrv table forward declarations */
static int umd_io_init(void *arg);
static int umd_io_exit(void *arg);
static int umd_io_open(void *iot, char *file, int flags, int mode);
static int umd_io_close(void *iot);
static int umd_io_read(void *iot, char *data, int len);
static int umd_io_ioctl(void *iot, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen);
static int umd_io_devctl(void *iot, const char *dev, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen);

static const void * const g_umd_drv_funcs_88d4[22] = {
    umd_io_init,
    umd_io_exit,
    umd_io_open,
    umd_io_close,
    umd_io_read,
    NULL,
    sub_41b0,
    umd_io_ioctl,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    umd_io_devctl,
    NULL,
};

static const struct {
    const char *name;
    u32 dev_type;
    u32 unk2;
    const char *desc;
    const void *funcs;
} g_umd_drv_892c = {
    "umd",
    0x4,
    0x800,
    "NP9660",
    g_umd_drv_funcs_88d4,
};

/* 0x210c */
int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    int r = sub_2640();
    if (r >= 0) {
        r = sub_3d10();
        if (r >= 0) {
            r = sceIoAddDrv(&g_umd_drv_892c);
            if (r >= 0) {
                r = sub_2bf0();
                if (r >= 0) {
                    sub_6b80();
                    return 0;
                }
            }
        }
    }
    extern int module_stop(SceSize, void *);
    module_stop(0, NULL);
    return 1;
}

/* 0x2184 */
int NOINLINE module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    sub_2d28();
    sceIoDelDrv("umd");
    sub_3d18();
    sub_26a4();
    return 0;
}

/* 0x21c0 */
int module_reboot_phase(int phase, void *argp, u32 unk1, u32 unk2)
{
    (void)argp;
    (void)unk1;
    (void)unk2;
    int r = 0;
    if (phase == 1) {
        r = sub_50c4();
    } else if (phase == 2) {
        r = sub_5054();
    }
    return r < 0 ? r : 0;
}

/* 0x220c */
int module_reboot_before(void *argp, u32 unk1, u32 unk2, u32 unk3)
{
    (void)argp;
    (void)unk1;
    (void)unk2;
    (void)unk3;
    sub_3640();
    return 0;
}

/* 0x222c */
int sceUmdMan_driver_65E2B3E0(void)
{
    return 0;
}

/* 0x2234 */
int sceUmd9660_driver_7EB57F56(void)
{
    return 0;
}

/* 0x223c */
int sceUmd9660_driver_C0933C16(void)
{
    return 0;
}

/* 0x2244 */
int NOINLINE sceUmd9660_driver_887C3193(void)
{
    return 0;
}

/* 0x224c */
void sceUmd9660_driver_3CC9CE54(void)
{
}

/* 0x2254 */
void sceUmd9660_driver_FE3A8B67(void)
{
}

/* 0x225c */
int NOINLINE sceUmd_040A7090(int err)
{
    if (err >= 0) {
        return err;
    }
    if (sceKernelGetCompiledSdkVersion() != 0) {
        return err;
    }
    if (err == (int)0x80010013) {
        return (int)0x80010016;
    }
    if (err == (int)0x80010086) {
        return (int)0x8001005b;
    }
    if ((u32)(err - 0x8001006f) < 3U) {
        return (int)0x80010016;
    }
    if ((err & 0xffff0000) == 0x80210000 && (err & 0xff) >= 4) {
        return (int)0x80010005;
    }
    return err;
}

/* 0x2348 */
int sceUmdActivate(int mode, const char *aliasname)
{
    if (aliasname == NULL) {
        return (int)0x80010016;
    }
    int intr = sceKernelCpuSuspendIntr();
    if (((mode ^ 1) >= 2) || strcmp(aliasname, "disc0:") != 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)0x80010016;
    }
    if (g_active_mode_8aa0 == mode) {
        int r = 0;
        if (!(g_drive_status_8a98 & 0x20)) {
            r = g_activate_res_8aa4;
        }
        sceKernelCpuResumeIntr(intr);
        return r;
    }
    sceKernelCpuResumeIntr(intr);
    int r = sub_2408(mode);
    g_activate_res_8aa4 = r;
    return r;
}

/* 0x2408 */
static NOINLINE int sub_2408(int mode)
{
    int r;
    if (g_active_mode_8aa0 == 0) {
        r = sceIoAssign("disc0:", "umd0:", "isofs0:", 1, (void *)g_ro_8690, 4);
        if (r < 0) {
            if (r != (int)0x80020321) {
                return r;
            }
        } else {
            strncpy(g_assign_dev_8aac, "disc0:", 0x1f);
        }
    } else if (strcmp(g_assign_dev_8aac, "disc0:") != 0) {
        return (int)0x80010010;
    }
    g_active_mode_8aa0 = mode;
    return sub_2de4(mode);
}

/* 0x24e0 */
int sceUmdRegisterUMDCallBack(SceUID cbid)
{
    void *cb;
    int intr = sceKernelCpuSuspendIntr();
    if (sceKernelGetUIDcontrolBlock(cbid, &cb) != 0 ||
        sceKernelGetThreadmanIdType(cbid) != 8) {
        sceKernelCpuResumeIntr(intr);
        return (int)0x80010016;
    }
    g_umd_cbid_8aa8 = cbid;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x258c */
int sceUmdUnRegisterUMDCallBack(SceUID cbid)
{
    void *cb;
    int intr = sceKernelCpuSuspendIntr();
    if (sceKernelGetUIDcontrolBlock(cbid, &cb) == 0 &&
        g_umd_cbid_8aa8 == cbid) {
        g_umd_cbid_8aa8 = -1;
        sceKernelCpuResumeIntr(intr);
        return 0;
    }
    int err = (int)0x80010016;
    if (sceKernelGetCompiledSdkVersion() == 0) {
        err = (int)0x80020066;
    }
    sceKernelCpuResumeIntr(intr);
    return err;
}

/* 0x2640 */
static NOINLINE int sub_2640(void)
{
    strncpy(g_dmu_str_8a8c, ":DMU", 8);
    g_mediaman_evf_8a94 = sceKernelCreateEventFlag("SceMediaManUser", 0x201, 0, NULL);
    g_drive_status_8a98 = 1;
    g_error_status_8a9c = 0;
    g_active_mode_8aa0 = 0;
    g_umd_cbid_8aa8 = -1;
    return g_mediaman_evf_8a94 < 0 ? g_mediaman_evf_8a94 : 0;
}

/* 0x26a4 */
static NOINLINE int sub_26a4(void)
{
    int r = sceKernelDeleteEventFlag(g_mediaman_evf_8a94);
    return r < 0 ? r : 0;
}

/* 0x26e0 */
int NOINLINE sceUmdGetDriveStatus(void)
{
    return g_drive_status_8a98;
}

/* 0x26ec */
int NOINLINE sceUmdSetDriveStatus(int stat)
{
    int intr = sceKernelCpuSuspendIntr();
    g_drive_status_8a98 |= stat;
    sceKernelSetEventFlag(g_mediaman_evf_8a94, stat);
    sceKernelCpuResumeIntr(intr);
    return g_drive_status_8a98;
}

/* 0x2744 */
int NOINLINE sceUmdClearDriveStatus(int stat)
{
    int intr = sceKernelCpuSuspendIntr();
    g_drive_status_8a98 &= ~stat;
    sceKernelClearEventFlag(g_mediaman_evf_8a94, ~stat);
    sceKernelCpuResumeIntr(intr);
    return g_drive_status_8a98;
}

/* 0x27a0 */
int sceUmdGetErrorStatus(void)
{
    return g_error_status_8a9c;
}

/* 0x27ac */
void NOINLINE sceUmdSetErrorStatus(int err)
{
    int intr = sceKernelCpuSuspendIntr();
    g_error_status_8a9c = err;
    sceKernelCpuResumeIntr(intr);
}

/* 0x27e0 */
static NOINLINE void sub_27e0(int stat)
{
    if (g_umd_cbid_8aa8 >= 0) {
        sceKernelNotifyCallback(g_umd_cbid_8aa8, stat);
    }
}

/* 0x2818 */
int sceUmdCheckMedium(void)
{
    return sub_368c() > 0 ? 1 : 0;
}

/* 0x2844 */
int sceUmdDeactivate(int mode, const char *aliasname)
{
    if (aliasname == NULL) {
        return (int)0x80010016;
    }
    int intr = sceKernelCpuSuspendIntr();
    if ((mode != 1 && mode != 2 && mode != 18) || g_active_mode_8aa0 == 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)0x80010016;
    }
    sceKernelCpuResumeIntr(intr);
    return sub_2b28(mode);
}

/* 0x28ac */
int sceUmdGetDiscInfo(u32 *info)
{
    int type = 0;
    if (info == NULL || info[0] != 8) {
        return (int)0x80010016;
    }
    if (sub_52ac(&type) < 0 || type == 0) {
        return (int)0x80210001;
    }
    info[1] = (u32)type;
    return 0;
}

/* 0x291c */
int sceUmdGetDriveStat(void)
{
    return g_drive_status_8a98 & 0x3b;
}

/* 0x2934 */
int sceUmdGetErrorStat(void)
{
    return sceUmd_040A7090(g_error_status_8a9c);
}

/* 0x2960 */
int sceUmdReplaceProhibit(void)
{
    int r = sub_3d38(1);
    return r < 0 ? r : 0;
}

/* 0x2990 */
int sceUmdReplacePermit(void)
{
    int r = sub_3d38(0);
    return r < 0 ? r : 0;
}

/* 0x29c0 */
int sceUmdWaitDriveStat(int stat)
{
    if (!(stat & 0x3b)) {
        return (int)0x80010016;
    }
    return sceKernelWaitEventFlag(g_mediaman_evf_8a94, stat & 0x3b, 1, NULL, NULL);
}

/* 0x2a18 */
int sceUmdWaitDriveStatWithTimer(int stat, u32 timeout)
{
    u32 to = timeout;
    if (!(stat & 0x3b)) {
        return (int)0x80010016;
    }
    return sceKernelWaitEventFlag(g_mediaman_evf_8a94, stat & 0x3b, 1, NULL, timeout ? &to : NULL);
}

/* 0x2a80 */
int sceUmdWaitDriveStatCB(int stat, u32 timeout)
{
    u32 to = timeout;
    if (!(stat & 0x3b)) {
        return (int)0x80010016;
    }
    return sceKernelWaitEventFlagCB(g_mediaman_evf_8a94, stat & 0x3b, 1, NULL, timeout ? &to : NULL);
}

/* 0x2ae8 */
int sceUmdCancelWaitDriveStat(void)
{
    int r = sceKernelCancelEventFlag(g_mediaman_evf_8a94, (u32)g_drive_status_8a98, NULL);
    return r < 0 ? r : 0;
}

/* 0x2b28 */
static NOINLINE int sub_2b28(int mode)
{
    if ((mode ^ 2) == 0 || (mode & 0x10)) {
        if (g_assign_dev_8aac[0] != '\0') {
            int r = sceIoUnassign(g_assign_dev_8aac);
            if (r >= 0) {
                memset(g_assign_dev_8aac, 0, 32);
                strncpy(g_dmu_str_8a8c, ":DMU", 8);
            }
        }
        g_active_mode_8aa0 = 0;
    }
    return sub_2eec(mode);
}

/* 0x2bf0 */
static NOINLINE int sub_2bf0(void)
{
    g_umdman_8acc = 0;
    g_umdman_stop_8ae0 = 0;
    g_umdman_state_8ae8 = 1;
    g_ins_ej_cb_8af0 = NULL;
    g_ins_ej_arg_8af4 = NULL;
    g_ins_ej_id_8aec = 0;
    int r = sceKernelRegisterSysEventHandler(&g_sysevent_89c8);
    if (r >= 0) {
        g_umdman_evf_8adc = sceKernelCreateEventFlag("SceUmdManState", 0x201, 0, NULL);
        if (g_umdman_evf_8adc >= 0) {
            g_mount_thid_8ad0 = sceKernelCreateThread("SceNpUmdMount", sub_3300, 0x18, 0x1000, 0x00100001, NULL);
            if (g_mount_thid_8ad0 >= 0) {
                r = sceKernelStartThread(g_mount_thid_8ad0, 0, NULL);
                if (r >= 0) {
                    u32 to = 1000000;
                    sceKernelWaitEventFlagCB(g_umdman_evf_8adc, 0xff, 1, NULL, &to);
                    sceUmdClearDriveStatus(2);
                    sceUmdSetDriveStatus(1);
                    return 0;
                }
            }
        }
    }
    sub_2d28();
    return r < 0 ? r : -1;
}

/* 0x2d28 */
static NOINLINE int sub_2d28(void)
{
    if (g_mount_thid_8ad0 > 0) {
        g_umdman_stop_8ae0 = 1;
        if (g_umdman_evf_8adc > 0) {
            sceKernelSetEventFlag(g_umdman_evf_8adc, 0x100);
        }
        u32 to = 100000;
        sceKernelWaitThreadEnd(g_mount_thid_8ad0, &to);
        sceKernelDeleteThread(g_mount_thid_8ad0);
        g_mount_thid_8ad0 = -1;
    }
    if (g_umdman_evf_8adc > 0) {
        sceKernelDeleteEventFlag(g_umdman_evf_8adc);
        g_umdman_evf_8adc = -1;
    }
    sceKernelUnregisterSysEventHandler(&g_sysevent_89c8);
    sub_3048(1, 0);
    sceUmdSetErrorStatus(0);
    return 0;
}

/* 0x2de4 */
static NOINLINE int sub_2de4(int mode)
{
    sub_5054();
    int intr = sceKernelCpuSuspendIntr();
    int prev = g_data_89c0;
    g_data_89c0 = mode;
    int st = sub_303c();
    if (st == 0x20) {
        sub_3048(0x20, 1);
        sceUmdSetErrorStatus(0);
    } else if (st == 0x10) {
        if (prev == 2) {
            sub_3048(0x20, 0);
            sceUmdSetErrorStatus(0);
        } else {
            sceKernelSetEventFlag(g_umdman_evf_8adc, 0x100);
        }
    } else if (st == 2 || st == 4) {
        sub_3048(4, 1);
    }
    sceKernelCpuResumeIntr(intr);
    sub_50c4();
    return 0;
}

/* 0x2eec */
static NOINLINE int sub_2eec(int mode)
{
    sub_5054();
    int intr = sceKernelCpuSuspendIntr();
    int prev = g_data_89c0;
    int st = sub_303c();
    if (mode & 0x40) {
        g_data_89c0 = 1;
        g_data_89c4 = 1;
    } else {
        g_data_89c0 = 2;
    }
    if (st == 0x20) {
        if (mode & 0xc0) {
            sub_5218();
            sub_3d04();
            sub_3048(1, 0);
        } else {
            sub_3048(0x10, 0);
        }
        sceUmdSetErrorStatus(0);
    } else if (st == 0x10) {
        if (prev == 1) {
            sceKernelSetEventFlag(g_umdman_evf_8adc, 0x100);
        } else {
            sub_3048(0x10, 1);
            sceUmdSetErrorStatus(0);
        }
    }
    sceKernelCpuResumeIntr(intr);
    sub_50c4();
    return 0;
}

/* 0x303c */
static NOINLINE int sub_303c(void)
{
    return g_umdman_state_8ae8;
}

/* 0x3048 */
static NOINLINE int sub_3048(int state, int force)
{
    int intr = sceKernelCpuSuspendIntr();
    int prev = g_umdman_state_8ae8;
    if (prev == state && !force) {
        sceKernelCpuResumeIntr(intr);
        return prev;
    }
    g_umdman_state_8ae8 = state;
    if (state == 1) {
        sceUmdClearDriveStatus(0x3e);
        sceUmdSetDriveStatus(1);
    } else if (state == 2 || state == 4 || state == 8) {
        sceUmdClearDriveStatus(0x31);
        sceUmdSetDriveStatus(state | 2);
    } else if (state == 0x10) {
        sceUmdClearDriveStatus(0x2d);
        sceUmdSetDriveStatus(0x12);
    } else if (state == 0x20) {
        if (sub_5268() != 0) {
            sub_52a0();
        }
        sceUmdClearDriveStatus(0x1d);
        sceUmdSetDriveStatus(0x32);
    }
    sub_3a24(state);
    sceKernelCpuResumeIntr(intr);
    if (prev != state || force) {
        int sdk = sceKernelGetCompiledSdkVersion();
        int notify_stat = g_drive_status_8a98;
        if (sdk != 0) {
            notify_stat &= 0x3b;
        }
        sub_27e0(notify_stat);
    }
    return prev;
}

/* 0x322c */
static NOINLINE int sub_322c(int ev_id, char *ev_name, void *param, int *result)
{
    (void)ev_name;
    (void)param;
    (void)result;
    if (ev_id == 0x402) {
        g_umdman_busy_8ae4 = 1;
    } else if (ev_id == 0x10000) {
        g_umdman_busy_8ae4 = 0;
        sub_3048(1, 0);
        sceUmdSetErrorStatus((int)0x80210003);
        if (g_umdman_evf_8adc > 0) {
            sceKernelSetEventFlag(g_umdman_evf_8adc, 0x100);
        }
    }
    return 0;
}

/* 0x3300 */
static NOINLINE int sub_3300(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    g_fatms_cbid_8ad4 = sceKernelCreateCallback("SceNp9660", sub_3704, NULL);
    if (g_fatms_cbid_8ad4 >= 0) {
        if (sceIoDevctl("fatms0:", 0x02415821, &g_fatms_cbid_8ad4, 4, NULL, 0) >= 0) {
            g_fatms_cb_reg_8ad8 = 1;
        }
    }
    while (!g_umdman_stop_8ae0) {
        sub_5054();
        int st = sub_303c();
        if (g_data_89c4 != 0 && st == 1) {
            g_data_89c4 = 0;
            int r = sub_3c14();
            if (r >= 0) {
                int target = (g_data_89c0 == 1) ? 0x20 : 0x10;
                sub_3048(target, 0);
                sceUmdSetErrorStatus(0);
                sub_38a8(1);
            } else {
                sub_5218();
                sub_3d04();
                sceUmdClearDriveStatus(0x3e);
                sub_3048(1, 0);
                sceUmdSetErrorStatus((int)0x80210001);
            }
        }
        int intr = sceKernelCpuSuspendIntr();
        sceKernelSetEventFlag(g_umdman_evf_8adc, sub_303c());
        sceKernelCpuResumeIntr(intr);
        sub_50c4();
        u32 out = 0;
        sceKernelWaitEventFlagCB(g_umdman_evf_8adc, 0x100, 0x21, &out, NULL);
    }
    if (g_fatms_cb_reg_8ad8) {
        sceIoDevctl("fatms0:", 0x02415822, &g_fatms_cbid_8ad4, 4, NULL, 0);
        g_fatms_cb_reg_8ad8 = 0;
    }
    if (g_fatms_cbid_8ad4 >= 0) {
        sceKernelDeleteCallback(g_fatms_cbid_8ad4);
        g_fatms_cbid_8ad4 = -1;
    }
    return 0;
}

/* 0x3634 */
static NOINLINE int sub_3634(void)
{
    return g_umdman_evf_8adc;
}

/* 0x3640 */
static NOINLINE int sub_3640(void)
{
    sub_5218();
    sub_3d04();
    sub_3048(1, 0);
    sceUmdSetErrorStatus(0);
    return 0;
}

/* 0x368c */
static NOINLINE int sub_368c(void)
{
    int st = 0;
    if (sub_303c() != 1) {
        return 1;
    }
    if (sceIoDevctl("fatms0:", 0x02425823, NULL, 0, &st, 4) < 0) {
        return 0;
    }
    return st == 1 ? 1 : 0;
}

/* 0x36f8 */
static NOINLINE void sub_36f8(int val)
{
    g_data_89c4 = val;
}

/* 0x3704 */
static NOINLINE int sub_3704(int cbid, int count, void *arg)
{
    (void)cbid;
    (void)arg;
    if (count == 2) {
        if (sub_303c() != 1) {
            sub_5218();
            sub_3048(1, 0);
            sceUmdSetErrorStatus((int)0x80210003);
        }
    } else if (count == 1) {
        g_data_89c4 = 1;
        if (g_umdman_evf_8adc > 0) {
            sceKernelSetEventFlag(g_umdman_evf_8adc, 0x100);
        }
    }
    return 0;
}

/* 0x37d4 */
int sceUmdManRegisterInsertEjectUMDCallBack(int id, void *cb, void *arg)
{
    int intr = sceKernelCpuSuspendIntr();
    if (g_ins_ej_id_8aec != 0 && g_ins_ej_id_8aec != id) {
        sceKernelCpuResumeIntr(intr);
        return (int)0x8001000c;
    }
    g_ins_ej_id_8aec = id;
    g_ins_ej_cb_8af0 = (int (*)(int, void *, int))cb;
    g_ins_ej_arg_8af4 = arg;
    int drv_st = sceUmdGetDriveStatus();
    sceKernelSetEventFlag(sub_3634(), sub_303c());
    sceKernelCpuResumeIntr(intr);
    if (cb != NULL) {
        int arg3 = (drv_st & 2) ? 1 : 2;
        ((int (*)(int, void *, int))cb)(id, arg, arg3);
    }
    return 0;
}

/* 0x38a8 */
static NOINLINE int sub_38a8(int state)
{
    if (g_ins_ej_cb_8af0 != NULL) {
        return g_ins_ej_cb_8af0(g_ins_ej_id_8aec, g_ins_ej_arg_8af4, state);
    }
    return 0;
}

/* 0x3920 */
int sceUmdManUnRegisterInsertEjectUMDCallBack(int id)
{
    int intr = sceKernelCpuSuspendIntr();
    if (g_ins_ej_id_8aec != id) {
        sceKernelCpuResumeIntr(intr);
        return (int)0x80010002;
    }
    g_ins_ej_id_8aec = 0;
    g_ins_ej_cb_8af0 = NULL;
    g_ins_ej_arg_8af4 = NULL;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x398c */
int sceUmdMan_driver_80D31D5D(void *a0)
{
    int intr = sceKernelCpuSuspendIntr();
    u32 *p = (u32 *)a0;
    g_umdman_cb_8af8 = (void (*)(int, void *))p[2];
    g_umdman_cb_arg_8afc = (void *)p[3];
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x39e4 */
int sceUmdMan_driver_34375DB0(int a0)
{
    (void)a0;
    int intr = sceKernelCpuSuspendIntr();
    g_umdman_cb_8af8 = NULL;
    g_umdman_cb_arg_8afc = NULL;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x3a24 */
static NOINLINE int sub_3a24(int state)
{
    if (g_umdman_cb_8af8 != NULL) {
        g_umdman_cb_8af8(state, g_umdman_cb_arg_8afc);
    }
    return 0;
}

/* 0x3a78 */
int sceNp9660_driver_8EF69DC6(const char *path, int a1)
{
    (void)a1;
    if (path == NULL) {
        return (int)0x80010016;
    }
    int intr = sceKernelCpuSuspendIntr();
    for (int i = 0; i < 63; i++) {
        g_iso_path_8b00[i] = path[i];
        if (path[i] == '\0') {
            break;
        }
    }
    g_iso_path_8b00[63] = '\0';
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x3af0 */
int sceNp9660_driver_B925CA6C(void)
{
    int intr = sceKernelCpuSuspendIntr();
    g_iso_path_8b00[0] = '\0';
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x3b28 */
static NOINLINE int sub_3b28(void)
{
    int intr = sceKernelCpuSuspendIntr();
    const char *p = g_iso_path_8b00[0] ? g_iso_path_8b00 : (const char *)g_disc_img_8b40;
    sceKernelCpuResumeIntr(intr);
    if (p == NULL || p[0] == '\0') {
        return -1;
    }
    return sceIoOpen(p, 0x000f0001, 0);
}

/* 0x3c14 */
static NOINLINE int sub_3c14(void)
{
    int intr = sceKernelCpuSuspendIntr();
    if (g_disc_img_8b40 == NULL) {
        g_disc_img_8b40 = sceKernelInitDiscImage();
    }
    sceKernelCpuResumeIntr(intr);
    int r = sub_0000();
    if (r < 0) {
        sub_3d04();
        return r;
    }
    sub_3cd0(1);
    return 0;
}

/* 0x3cd0 */
static NOINLINE void sub_3cd0(int val)
{
    int intr = sceKernelCpuSuspendIntr();
    g_bss_8a20 = val;
    sceKernelCpuResumeIntr(intr);
}

/* 0x3d04 */
static NOINLINE int sub_3d04(void)
{
    return 0;
}

/* 0x3d10 */
static NOINLINE int sub_3d10(void)
{
    return 0;
}

/* 0x3d18 */
static NOINLINE int sub_3d18(void)
{
    sub_5218();
    return 0;
}

/* 0x3d38 */
static NOINLINE int sub_3d38(int val)
{
    int intr = sceKernelCpuSuspendIntr();
    g_replace_prohibit_8b44 = val;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x3d70 */
static NOINLINE int sub_3d70(void)
{
    return 0x10;
}

/* 0x3d7c */
int NOINLINE sceNp9660_driver_7A05EB3C(void)
{
    return g_umd_ctx_8b48.fd;
}

/* 0x3d94 (merged into 0x3d7c by epsp_prx.py because in .rodata) */
static int umd_io_init(void *arg)
{
    (void)arg;
    u32 part_info[6];
    memset(&g_umd_ctx_8b48, 0, sizeof(g_umd_ctx_8b48));
    g_umd_ctx_8b48.fd = -1;
    g_umd_ctx_8b48.cached_lba = -1;
    g_umd_ctx_8b48.cached_blk = -1;
    g_umd_ctx_8b48.mutex_owner_pri = -1;

    g_umd_ctx_8b48.mutex = sceKernelCreateMutex("SceNp9660", 0x101, 0, NULL);
    if (g_umd_ctx_8b48.mutex >= 0) {
        g_umd_ctx_8b48.fpl = sceKernelCreateFpl("SceNp9660", 1, 0, 0x8200, 1, NULL);
        if (g_umd_ctx_8b48.fpl >= 0) {
            if (sceKernelTryAllocateFpl(g_umd_ctx_8b48.fpl, &g_umd_ctx_8b48.sector_buf) >= 0) {
                int r = sub_0dc0((u8 *)g_umd_ctx_8b48.sector_buf + 0x8000, 0x200);
                if (r >= 0) {
                    part_info[0] = 0x14;
                    int pid = 1;
                    if (sceKernelQueryMemoryPartitionInfo(8, part_info) >= 0 && part_info[2] >= 0x8000U) {
                        pid = 8;
                    }
                    int fpl_opts[2] = { 8, 0x40 };
                    g_umd_ctx_8b48.nc_fpl = sceKernelCreateFpl("SceNp9660NC", pid, 0, 0x8000, 1, fpl_opts);
                    if (g_umd_ctx_8b48.nc_fpl >= 0) {
                        if (sceKernelTryAllocateFpl(g_umd_ctx_8b48.nc_fpl, &g_umd_ctx_8b48.nc_buf) >= 0) {
                            sub_52d8((void *)((u32)g_umd_ctx_8b48.nc_buf | 0x40000000U));
                            if (sub_678c() >= 0) {
                                return 0;
                            }
                        }
                    }
                }
            }
        }
    }
    sub_68c4();
    if (g_umd_ctx_8b48.mutex > 0) {
        sceKernelDeleteMutex(g_umd_ctx_8b48.mutex);
        g_umd_ctx_8b48.mutex = 0;
    }
    sub_0df0();
    sub_5308();
    if (g_umd_ctx_8b48.fpl > 0) {
        sceKernelDeleteFpl(g_umd_ctx_8b48.fpl);
        g_umd_ctx_8b48.fpl = 0;
    }
    return -1;
}

/* 0x3f74 */
static int umd_io_exit(void *arg)
{
    (void)arg;
    int intr = sceKernelCpuSuspendIntr();
    for (int i = 0; i < 4; i++) {
        g_umd_ctx_8b48.handles[i].in_use = 0;
    }
    sceKernelCpuResumeIntr(intr);
    sub_68c4();
    sub_5308();
    sub_0df0();
    if (g_umd_ctx_8b48.nc_fpl > 0) {
        sceKernelDeleteFpl(g_umd_ctx_8b48.nc_fpl);
        g_umd_ctx_8b48.nc_fpl = -1;
    }
    if (g_umd_ctx_8b48.fpl > 0) {
        sceKernelDeleteFpl(g_umd_ctx_8b48.fpl);
        g_umd_ctx_8b48.fpl = -1;
    }
    if (g_umd_ctx_8b48.mutex > 0) {
        sceKernelDeleteMutex(g_umd_ctx_8b48.mutex);
        g_umd_ctx_8b48.mutex = -1;
    }
    return 0;
}

/* 0x4044 */
static int umd_io_open(void *iot, char *file, int flags, int mode)
{
    (void)file;
    (void)flags;
    (void)mode;
    int intr = sceKernelCpuSuspendIntr();
    int slot = -1;
    for (int i = 0; i < 4; i++) {
        if (!g_umd_ctx_8b48.handles[i].in_use) {
            g_umd_ctx_8b48.handles[i].in_use = 1;
            g_umd_ctx_8b48.handles[i].lba = 0;
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)0x80010018;
    }
    g_umd_ctx_8b48.open_count++;
    *(void **)((u8 *)iot + 0x3c) = &g_umd_ctx_8b48.handles[slot];
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x4144 */
static int umd_io_close(void *iot)
{
    int intr = sceKernelCpuSuspendIntr();
    int *h = *(int **)((u8 *)iot + 0x3c);
    if (h == NULL || h[0] == 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)0x80010016;
    }
    h[0] = 0;
    g_umd_ctx_8b48.open_count--;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x41b0 */
static NOINLINE int sub_41b0(void *iot, SceOff ofs, int whence)
{
    int *h = *(int **)((u8 *)iot + 0x3c);
    int r = sub_43ec(iot);
    if (r < 0) {
        return r;
    }
    int base = 0;
    if (whence == 0) {
        base = 0;
    } else if (whence == 1) {
        base = h[1];
    } else if (whence == 2) {
        base = g_umd_ctx_8b48.total_sectors;
    } else {
        return (int)0x80010016;
    }
    int target = base + (int)ofs;
    if (target < 0 || target > g_umd_ctx_8b48.total_sectors) {
        return (int)0x80010016;
    }
    h[1] = target;
    if (sub_5ffc()) {
        u32 req[4] = { (u32)target, 0, 0, 0 };
        if (!sub_6028()) {
            sub_5b04(req);
        } else {
            sub_73b4(target);
        }
    }
    return target;
}

/* 0x4308 (merged into 0x41b0 by epsp_prx.py because in .rodata) */
static int umd_io_read(void *iot, char *data, int len)
{
    int *h = *(int **)((u8 *)iot + 0x3c);
    u32 req[4];
    req[0] = (u32)h[1];
    req[1] = (u32)len;
    req[2] = (u32)data;
    req[3] = 0;
    int r = sub_4428((int (*)(void *))sub_4500, req);
    if (r >= 0) {
        h[1] += r;
    }
    return r;
}

/* 0x43ec */
static NOINLINE int sub_43ec(void *arg)
{
    return sub_4428(NULL, arg);
}

/* 0x4428 */
static NOINLINE int sub_4428(int (*fn)(void *), void *arg)
{
    int r = sub_51a4(fn == NULL ? 1 : 0);
    if (r < 0) {
        sceKernelDelayThread(10000);
        return r;
    }
    sub_5054();
    if (fn != NULL) {
        r = sceKernelExtendKernelStack(0x2000, fn, arg);
    } else {
        r = 0;
    }
    sub_50c4();
    return r;
}

/* 0x4500 */
static NOINLINE int sub_4500(void *arg)
{
    u32 *req = (u32 *)arg;
    int lba = (int)req[0];
    int count = (int)req[1];
    u8 *dst = (u8 *)req[2];
    if (count == 0) {
        return 0;
    }
    if (dst == NULL || lba < 0 || count < 0 || lba + count > g_umd_ctx_8b48.total_sectors) {
        return (int)0x80010016;
    }
    int basis = g_umd_ctx_8b48.block_basis;
    int blk = lba / basis;
    u8 entry[40];
    int r = sub_0c4c(entry, blk);
    if (r < 0) {
        return r;
    }
    if (sub_5330(lba, count)) {
        sub_535c(dst, count << 11, lba);
        return count;
    }
    if (sub_538c(lba)) {
        sub_53a8(dst, count << 11, lba);
    }
    u64 off = ((u64)g_umd_ctx_8b48.img_offset_hi << 32 | g_umd_ctx_8b48.img_offset_lo) + *(u32 *)(entry + 0x14);
    int rd = sub_50fc(g_umd_ctx_8b48.sector_buf, *(int *)(entry + 0x18), (u32)off, (u32)(off >> 32));
    if (rd < 0) {
        sceKernelDelayThread(10000);
        return rd;
    }
    r = sub_0a34(g_umd_ctx_8b48.sector_buf, *(int *)(entry + 0x18), g_bss_8a6c, *(int *)(entry + 0x1c), entry + 8, *(int *)(entry + 0x10));
    if (r < 0) {
        sub_0ad0(g_umd_ctx_8b48.sector_buf, blk, 1);
    }
    if (*(int *)(entry + 0x18) < (basis << 11)) {
        sub_69dc(dst, count << 11, g_umd_ctx_8b48.sector_buf);
    } else {
        if (dst == g_umd_ctx_8b48.sector_buf) {
            sceKernelMemmove(dst, g_umd_ctx_8b48.sector_buf, count << 11);
        } else {
            sceKernelMemcpy(dst, g_umd_ctx_8b48.sector_buf, count << 11);
        }
    }
    g_umd_ctx_8b48.cur_lba = lba + count;
    if (req[3] != 0) {
        sub_41b0((void *)req[3], lba, 0);
        sub_43ec(NULL);
        sub_4ccc(0x10);
    }
    return count;
}

/* 0x4acc (merged into 0x4500 by epsp_prx.py because in .rodata) */
static int umd_io_ioctl(void *iot, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
    int *h = *(int **)((u8 *)iot + 0x3c);
    if (cmd == 0x01f20001) {
        int r = sub_43ec(iot);
        if (r < 0) {
            return r;
        }
        if (outdata == NULL || outlen < 4) {
            return (int)0x80010016;
        }
        *(int *)outdata = h[1];
        return 0;
    }
    if (cmd == 0x01f100a6) {
        if (indata == NULL || inlen < 0x10) {
            return (int)0x80010016;
        }
        s64 ofs = *(s64 *)indata;
        int whence = *(int *)((u8 *)indata + 0x0c);
        return sub_41b0(iot, ofs, whence);
    }
    if (cmd == 0x01f010db) {
        return sub_43ec(iot);
    }
    if (cmd == 0x01f30003) {
        if (indata == NULL || inlen < 4 || outdata == NULL) {
            return (int)0x80010016;
        }
        int cnt = *(int *)indata;
        if (outlen < cnt) {
            return (int)0x80010016;
        }
        return umd_io_read(iot, (char *)outdata, cnt);
    }
    return (int)0x80010086;
}

/* 0x4ccc */
static NOINLINE void sub_4ccc(int val)
{
    g_umd_ctx_8b48.disc_type = val;
}

/* 0x4cd8 */
static NOINLINE int sub_4cd8(void)
{
    int intr = sceKernelCpuSuspendIntr();
    g_umd_ctx_8b48.cached_lba = -1;
    g_umd_ctx_8b48.cached_blk = -1;
    sceKernelCpuResumeIntr(intr);

    if (g_umd_ctx_8b48.open_count > 0) {
        sceKernelCancelMutex(g_umd_ctx_8b48.mutex, 1, NULL);
        sceKernelUnlockMutex(g_umd_ctx_8b48.mutex, 1);
    }
    int async_mode = 0;
    if (!(g_umd_ctx_8b48.header_flags & 2)) {
        sceIoDevctl("ms0:", 0x02425858, &async_mode, 4, NULL, 0);
    }
    sub_6098();
    void *gi = sceKernelGetGameInfo();
    if (gi != NULL) {
        const char *id = (const char *)gi + 0x44;
        if (strcmp(id, "ULUS10021") == 0 || strcmp(id, "ULUS10059") == 0) {
            sub_60a4();
        } else if (strcmp(id, "ULES00108") == 0) {
            sub_60b0();
        }
    }
    if (g_umd_ctx_8b48.mutex_owner_pri >= 0) {
        u32 to = 10000;
        sceKernelLockMutex(g_umd_ctx_8b48.mutex, 1, &to);
    }
    sub_6988(0x18);
    return 0;
}

/* 0x4ef4 */
static NOINLINE int sub_4ef4(int unit, int *out)
{
    if (unit != 0 || out == NULL) {
        return (int)0x80010016;
    }
    int intr = sceKernelCpuSuspendIntr();
    if (g_umd_ctx_8b48.fd < 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)0x80210001;
    }
    *out = g_umd_ctx_8b48.lba_param;
    sub_3d04();
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x4fa4 */
static NOINLINE int sub_4fa4(int unit, void *out)
{
    if (unit != 0 || out == NULL) {
        return (int)0x80010016;
    }
    int intr = sceKernelCpuSuspendIntr();
    if (g_umd_ctx_8b48.fd < 0) {
        sceKernelCpuResumeIntr(intr);
        return (int)0x80210001;
    }
    int *p = (int *)out;
    p[1] = g_umd_ctx_8b48.total_sectors;
    p[2] = g_umd_ctx_8b48.total_sectors;
    sub_3d04();
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x5054 */
static NOINLINE int sub_5054(void)
{
    int pri = sceKernelGetThreadCurrentPriority();
    int r = sceKernelLockMutex(g_umd_ctx_8b48.mutex, 1, NULL);
    if (r >= 0) {
        g_umd_ctx_8b48.mutex_owner_pri = pri;
    }
    return r;
}

/* 0x50c4 */
static NOINLINE int sub_50c4(void)
{
    g_umd_ctx_8b48.mutex_owner_pri = -1;
    return sceKernelUnlockMutex(g_umd_ctx_8b48.mutex, 1);
}

/* 0x50fc */
static NOINLINE int sub_50fc(void *buf, int size, u32 pos_lo, u32 pos_hi)
{
    SceOff off = ((SceOff)pos_hi << 32) | pos_lo;
    SceOff r = sceIoLseek(g_umd_ctx_8b48.fd, off, 0);
    if (r < 0) {
        return (int)r;
    }
    return sceIoRead(g_umd_ctx_8b48.fd, buf, (SceSize)size);
}

/* 0x51a4 */
static NOINLINE int sub_51a4(int flag)
{
    if (g_umdman_busy_8ae4) {
        return (int)0x8001000c;
    }
    int st = sub_303c();
    if (st != 0x20) {
        if (st == 0x10 && flag && !sceKernelIsUserModeThread() && sceKernelGetCompiledSdkVersion() != 0) {
            return 0;
        }
        return (int)0x80210001;
    }
    return 0;
}

/* 0x5218 */
static NOINLINE int sub_5218(void)
{
    sub_3cd0(0);
    if (g_umd_ctx_8b48.fd >= 0) {
        sceIoClose(g_umd_ctx_8b48.fd);
        g_umd_ctx_8b48.fd = -1;
    }
    return 0;
}

/* 0x5268 */
static NOINLINE int sub_5268(void)
{
    return memcmp(g_umd_ctx_8b48.disc_id, sub_60bc(), 10) != 0;
}

/* 0x52a0 */
static NOINLINE void sub_52a0(void)
{
    g_umd_ctx_8b48.cur_lba = 0;
}

/* 0x52ac */
static NOINLINE int sub_52ac(int *type_out)
{
    if (type_out == NULL) {
        return (int)0x80010016;
    }
    *type_out = g_umd_ctx_8b48.disc_type;
    return 0;
}

/* 0x52cc */
static NOINLINE int sub_52cc(void)
{
    return g_umd_ctx_8b48.mutex_owner_pri;
}

/* 0x52d8 */
static NOINLINE void sub_52d8(void *buf)
{
    g_umd_ctx_8b48.nc_buf = buf;
    g_umd_ctx_8b48.cached_lba = -1;
    g_umd_ctx_8b48.cached_blk = -1;
}

/* 0x5308 */
static NOINLINE void sub_5308(void)
{
    g_umd_ctx_8b48.nc_buf = NULL;
    g_umd_ctx_8b48.cached_lba = -1;
}

/* 0x531c */
static NOINLINE void sub_531c(int sectors)
{
    g_umd_ctx_8b48.total_sectors = sectors;
    g_umd_ctx_8b48.lba_param = sectors - 1;
}

/* 0x5330 */
static NOINLINE int sub_5330(int lba, int count)
{
    if (g_umd_ctx_8b48.cached_lba < 0) {
        return 0;
    }
    return (lba >= g_umd_ctx_8b48.cached_lba &&
            lba + count <= g_umd_ctx_8b48.cached_lba + g_umd_ctx_8b48.block_basis);
}

/* 0x535c */
static NOINLINE int sub_535c(void *buf, int size, int lba)
{
    int off = (lba - g_umd_ctx_8b48.cached_lba) << 11;
    if (g_umd_ctx_8b48.nc_buf != NULL && size > 0) {
        u8 *s = (u8 *)g_umd_ctx_8b48.nc_buf + off;
        u8 *d = (u8 *)buf;
        for (int i = 0; i < size; i += 4) {
            *(u32 *)(d + i) = *(u32 *)(s + i);
        }
    }
    return size;
}

/* 0x538c */
static NOINLINE int sub_538c(int lba)
{
    return (g_umd_ctx_8b48.cached_lba == lba);
}

/* 0x53a8 */
static NOINLINE int sub_53a8(void *buf, int size, int lba)
{
    (void)buf;
    (void)size;
    g_umd_ctx_8b48.cached_lba = lba;
    return 0;
}

/* 0x53d0 (merged into 0x53a8 by epsp_prx.py because in .rodata) */
static int umd_io_devctl(void *iot, const char *dev, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
    int r = sub_58c4(dev);
    if (r < 0) {
        return r;
    }
    if (cmd == 0x01f20001) {
        if (outdata == NULL || outlen < 8) {
            return (int)0x80010016;
        }
        int type = 0;
        sub_52ac(&type);
        ((int *)outdata)[0] = -1;
        ((int *)outdata)[1] = type;
        return 0;
    }
    if (cmd == 0x01f20002) {
        int r2 = sub_43ec(iot);
        if (r2 < 0) {
            return r2;
        }
        return sub_4ef4(0, (int *)outdata);
    }
    if (cmd == 0x01f20003) {
        int r2 = sub_43ec(iot);
        if (r2 < 0) {
            return r2;
        }
        return sub_4fa4(0, outdata);
    }
    if (cmd == 0x01f100a9) {
        return sub_5af8();
    }
    return sub_5968(0, cmd, indata, inlen, outdata, outlen);
}

/* 0x58c4 */
static NOINLINE int sub_58c4(const char *name)
{
    if (name == NULL) {
        return (int)0x80010016;
    }
    if (strncmp(name, "umd", 3) != 0) {
        return (int)0x80010016;
    }
    u16 *ct = look_ctype_table();
    if (name[3] != ':' && !(ct[(u8)name[3] + 1] & 4)) {
        return (int)0x80010016;
    }
    return 0;
}

/* 0x5968 */
static NOINLINE int sub_5968(int unit, u32 cmd, void *indata, int inlen, void *outdata, int outlen)
{
    (void)unit;
    (void)inlen;
    (void)outlen;
    if (cmd == 0x01e380c0 || cmd == 0x01e38012) {
        u32 req[4];
        u32 *in = (u32 *)indata;
        req[0] = in ? in[2] : 0;
        req[1] = in ? in[1] : 0;
        req[2] = (u32)outdata;
        req[3] = 0;
        if (sub_5ffc()) {
            if (sceKernelSearchModuleByName("sceNetAdhoc_Library") >= 0 || sub_6028()) {
                int a2, a3;
                sub_6c9c((int)req[0], (int)req[1], &a2, &a3);
            } else {
                sub_7104(req);
            }
        }
        return sub_4428((int (*)(void *))sub_4500, req);
    }
    if (cmd == 0x01e18030) {
        return sub_5d18(indata);
    }
    if (cmd == 0x01e00001) {
        return sub_5fac(0);
    }
    if (cmd == 0x01e180d3) {
        return sub_5b04((u32 *)indata);
    }
    return 0;
}

/* 0x5af8 */
static NOINLINE int sub_5af8(void)
{
    return 0;
}

/* 0x5b04 */
static NOINLINE int sub_5b04(u32 *req)
{
    s64 now = sceKernelGetSystemTimeWide();
    int lba = req ? (int)req[0] : 0;
    int cnt = req ? (int)req[1] : 0;
    int a3 = 0, t0 = 0;
    if (cnt == 0) {
        int diff = sub_66f8(lba);
        if (diff > 0) {
            sub_60cc(diff, 0);
        } else {
            sub_63b4(-diff, 0);
        }
    } else {
        sub_6bd4(lba, cnt, 0, &a3, &t0);
        sub_6c9c(lba, cnt, &a3, &t0);
    }
    g_emu_time_8c08 = now;
    g_emu_lba_8c10 = lba + cnt;
    return 0;
}

/* 0x5d18 */
static NOINLINE int sub_5d18(void *info)
{
    if (info == NULL) {
        return (int)0x80010016;
    }
    u32 *p = (u32 *)info;
    g_emu_8c14 = (int)p[5];
    g_emu_8c18 = (int)p[7];
    g_emu_8c24 = *(u16 *)((u8 *)info + 0x0c);
    g_emu_8c2c = (int)p[0];
    return 0;
}

static int sub_5fd8(void);
static int sub_5fe8(void);
static int sub_5ff4(void);

static int (* const g_emu_fn_tab_8960[8])(void) = {
    sub_5ff4,
    sub_5fd8,
    sub_5ff4,
    sub_5ff4,
    sub_5fd8,
    sub_5fe8,
    sub_5ff4,
    sub_5ff4,
};

/* 0x5fac */
static NOINLINE int sub_5fac(int idx)
{
    if ((u32)idx >= 8U) {
        return 0;
    }
    return g_emu_fn_tab_8960[idx]();
}

/* 0x5fd8 (in .rodata table 0x8960) */
static int sub_5fd8(void)
{
    g_emu_8c00 = 1;
    return 0;
}

/* 0x5fe8 (in .rodata table 0x8960) */
static int sub_5fe8(void)
{
    g_emu_8c00 = 2;
    return 0;
}

/* 0x5ff4 (in .rodata table 0x8960) */
static int sub_5ff4(void)
{
    return 0;
}

/* 0x5ffc */
static NOINLINE int sub_5ffc(void)
{
    if (g_emu_8c00 == 0) {
        return 0;
    }
    g_emu_time_8c08 = sceKernelGetSystemTimeWide();
    return 1;
}

/* 0x6028 */
static NOINLINE int sub_6028(void)
{
    if (sceWlanDrv_driver_7DD3787C() == 1) {
        return 1;
    }
    return 0;
}

/* 0x6098 */
static NOINLINE void sub_6098(void)
{
    g_emu_8c04 = 0;
}

/* 0x60a4 */
static NOINLINE void sub_60a4(void)
{
    g_emu_8c04 = 1;
}

/* 0x60b0 */
static NOINLINE void sub_60b0(void)
{
    g_emu_8c04 = 2;
}

/* 0x60bc */
static NOINLINE const char *sub_60bc(void)
{
    return g_dummy_disc_id_8940;
}

/* 0x60cc */
static NOINLINE int sub_60cc(int diff, int flag)
{
    u32 t0 = sceKernelGetSystemTimeLow();
    int delay = (diff * 100) / (g_data_8a08 ? g_data_8a08 : 1);
    if (flag == 0 && delay > 200) {
        sceKernelDelayThread((u32)delay);
    }
    sceUmd9660_driver_887C3193();
    return (int)(sceKernelGetSystemTimeLow() - t0);
}

/* 0x63b4 */
static NOINLINE int sub_63b4(int diff, int flag)
{
    u32 t0 = sceKernelGetSystemTimeLow();
    int delay = (diff * 120) / (g_data_8a08 ? g_data_8a08 : 1);
    if (flag == 0 && delay > 200) {
        sceKernelDelayThread((u32)delay);
    }
    sceUmd9660_driver_887C3193();
    return (int)(sceKernelGetSystemTimeLow() - t0);
}

/* 0x66f8 */
static NOINLINE int sub_66f8(int lba)
{
    int d = lba - g_emu_lba_8c10;
    if (g_data_8a08 != 0) {
        return d;
    }
    return 0;
}

/* 0x678c */
static NOINLINE int sub_678c(void)
{
    g_dec_mbx_8c34 = sceKernelCreateMbx("SceNp9660Dec", 0, NULL);
    if (g_dec_mbx_8c34 >= 0) {
        g_dec_evf_8c38 = sceKernelCreateEventFlag("SceNp9660Dec", 0, 0, NULL);
        if (g_dec_evf_8c38 >= 0) {
            g_dec_thid_8c30 = sceKernelCreateThread("SceNp9660Dec", sub_6a98, 0x18, 0x1000, 0, NULL);
            if (g_dec_thid_8c30 >= 0) {
                if (sceKernelStartThread(g_dec_thid_8c30, 0, NULL) >= 0) {
                    return 0;
                }
                sceKernelDeleteThread(g_dec_thid_8c30);
            }
            sceKernelDeleteEventFlag(g_dec_evf_8c38);
        }
        sceKernelDeleteMbx(g_dec_mbx_8c34);
    }
    return -1;
}

/* 0x68c4 */
static NOINLINE int sub_68c4(void)
{
    if (g_dec_thid_8c30 > 0) {
        g_dec_msg_8c3c[2] = 0;
        sceKernelSendMbx(g_dec_mbx_8c34, g_dec_msg_8c3c);
        u32 to = 100000;
        sceKernelWaitThreadEnd(g_dec_thid_8c30, &to);
        sceKernelDeleteThread(g_dec_thid_8c30);
        g_dec_thid_8c30 = -1;
    }
    if (g_dec_evf_8c38 > 0) {
        sceKernelDeleteEventFlag(g_dec_evf_8c38);
        g_dec_evf_8c38 = -1;
    }
    if (g_dec_mbx_8c34 > 0) {
        sceKernelDeleteMbx(g_dec_mbx_8c34);
        g_dec_mbx_8c34 = -1;
    }
    return 0;
}

/* 0x6988 */
static NOINLINE int sub_6988(int prio)
{
    g_data_8a10 = prio;
    if (g_dec_thid_8c30 > 0) {
        return sceKernelChangeThreadPriority(g_dec_thid_8c30, prio);
    }
    return 0;
}

/* 0x69dc */
static NOINLINE int sub_69dc(void *dst, u32 dst_len, const void *src)
{
    int pri = sub_52cc();
    if (pri > 0 && pri < g_data_8a10) {
        sceKernelChangeThreadPriority(g_dec_thid_8c30, pri);
    }
    g_dec_msg_8c3c[2] = (u32)dst;
    g_dec_msg_8c3c[3] = dst_len;
    g_dec_msg_8c3c[4] = (u32)src;
    g_dec_msg_8c3c[5] = 0;
    sceKernelSendMbx(g_dec_mbx_8c34, g_dec_msg_8c3c);
    u32 out = 0;
    sceKernelWaitEventFlag(g_dec_evf_8c38, 1, 0x21, &out, NULL);
    if (pri > 0 && pri < g_data_8a10) {
        sceKernelChangeThreadPriority(g_dec_thid_8c30, g_data_8a10);
    }
    return (int)g_dec_msg_8c3c[5];
}

/* 0x6a98 */
static NOINLINE int sub_6a98(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    void *msg = NULL;
    while (sceKernelReceiveMbx(g_dec_mbx_8c34, &msg, NULL) >= 0) {
        u32 *m = (u32 *)msg;
        if (m[2] == 0) {
            break;
        }
        int r = sub_1228((void *)m[2], m[3], (const void *)m[4], NULL);
        m[5] = (u32)r;
        if (r > 0) {
            sceKernelDcacheWritebackRange((const void *)m[2], m[3]);
        }
        sceKernelSetEventFlag(g_dec_evf_8c38, 1);
    }
    return sceKernelExitThread(0);
}

/* 0x6b80 */
static NOINLINE void sub_6b80(void)
{
    for (int i = 0; i < 512; i++) {
        g_cache_8c5c[i] = -1;
    }
    g_cache_8c5c[512] = 0;
    g_cache_8c5c[513] = 0;
}

/* 0x6bd4 */
static NOINLINE int sub_6bd4(int lba, int count, int flag, int *a3, int *t0)
{
    int idx = sub_7004(lba);
    sub_703c(lba, count, idx);
    return sub_6d24(lba, count, flag, a3, t0);
}

/* 0x6c9c */
static NOINLINE int sub_6c9c(int lba, int count, int *a2, int *a3)
{
    int idx = sub_7004(lba);
    sub_703c(lba, count, idx);
    return sub_6e4c(lba, count, idx, a2);
}

/* 0x6d10 */
static NOINLINE void sub_6d10(int lba)
{
    g_cache_8c5c[514] = lba;
}

/* 0x6d24 */
static NOINLINE int sub_6d24(int a0, int a1, int a2, int *a3, int *t0)
{
    (void)a2;
    if (a3) {
        *a3 = a0;
    }
    if (t0) {
        *t0 = a1;
    }
    g_cache_8c5c[506] = a0 + a1;
    return 0;
}

/* 0x6e4c */
static NOINLINE int sub_6e4c(int a0, int a1, int a2, int *a3)
{
    if (a1 > 0) {
        g_cache_8c5c[507] = a0 + a1;
    } else {
        g_cache_8c5c[506] = a0;
    }
    if (a3) {
        *a3 = a2;
    }
    return 0;
}

/* 0x7004 */
static NOINLINE int sub_7004(int lba)
{
    return (lba >> 4) & 0x1ff;
}

/* 0x703c */
static NOINLINE int sub_703c(int lba, int count, int idx)
{
    s64 q = sub_1b38((s64)lba + count, 16);
    int out = (int)q;
    sub_6e4c(lba, count, idx, &out);
    return out;
}

/* 0x7104 */
static NOINLINE int sub_7104(void *req)
{
    u32 *r = (u32 *)req;
    int lba = (int)r[0];
    int cnt = (int)r[1];
    s64 now = sceKernelGetSystemTimeWide();
    int a3 = 0, t0 = 0;
    sub_6bd4(lba, cnt, 1, &a3, &t0);
    sub_6d10(lba);
    int d = sub_7ca4(lba, cnt);
    if (d >= 0) {
        sub_73c0(lba, cnt, d, a3, t0);
    } else {
        sub_78d0(lba, cnt, -d, a3, t0);
    }
    g_emu_time_8c08 = now;
    return 0;
}

/* 0x73b4 */
static NOINLINE void sub_73b4(int lba)
{
    g_emu_lba_8c10 = lba;
}

/* 0x73c0 */
static NOINLINE int sub_73c0(int a0, int a1, int a2, int a3, int t0)
{
    (void)a3;
    (void)t0;
    u32 start = sceKernelGetSystemTimeLow();
    g_data_8a14 = a0 + a1;
    g_data_8a18 = a2;
    int delay = (a2 * 100) / (g_data_8a0c ? g_data_8a0c : 1);
    if (delay > 200) {
        sceKernelDelayThread((u32)delay);
    }
    sceUmd9660_driver_887C3193();
    return (int)(sceKernelGetSystemTimeLow() - start);
}

/* 0x78d0 */
static NOINLINE int sub_78d0(int a0, int a1, int a2, int a3, int t0)
{
    (void)a3;
    (void)t0;
    u32 start = sceKernelGetSystemTimeLow();
    g_data_8a14 = a0 + a1;
    g_data_8a18 = a2;
    int delay = (a2 * 120) / (g_data_8a0c ? g_data_8a0c : 1);
    if (delay > 200) {
        sceKernelDelayThread((u32)delay);
    }
    sceUmd9660_driver_887C3193();
    return (int)(sceKernelGetSystemTimeLow() - start);
}

/* 0x7ca4 */
static NOINLINE int sub_7ca4(int lba, int count)
{
    int lim = g_data_8a0c;
    if (lim <= 0) {
        return 0;
    }
    return (lba + count) - g_emu_lba_8c10;
}
