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

static inline int pspSdkSetK1(int k1)
{
    int old_k1;
    __asm__ volatile("move %0, $k1\n\tmove $k1, %1" : "=&r"(old_k1) : "r"(k1));
    return old_k1;
}

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
extern int look_ctype_table(int c);
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
int sub_0000(const char *path, int flag, int *out_err);
static NOINLINE int sub_00c8(const char *path, int flag);
static NOINLINE int sub_0a34(void *buf, void *entry);
static NOINLINE int sub_0ad0(void *buf, u32 base_file_off, u32 total_len, int first_blk, int last_blk, void *entry);
static NOINLINE void sub_0c38(void);
static NOINLINE int sub_0c4c(void *a0, int a1);
static NOINLINE int sub_0dc0(void *buf, int size);
static NOINLINE int sub_0df0(void);
static NOINLINE int sub_0df8(void *vkey, const char *cid, int flags, void *act_buf);
static NOINLINE int sub_10d8(void *buf, int size, u32 seed);
static NOINLINE int sub_1188(const void *buf, int size, const void *hash);
static NOINLINE int sub_1228(void *dst, u32 dst_len, const void *src, u32 *next_in);
static NOINLINE s64 sub_1b38(s64 a, s64 b);

static NOINLINE int sub_2408(int mode, const char *aliasname);
static NOINLINE int sub_2640(void);
static NOINLINE int sub_26a4(void);
static NOINLINE void sub_27e0(int stat);
static NOINLINE int sub_2b28(int mode, const char *aliasname);
static NOINLINE int sub_2bf0(void);
static NOINLINE int sub_2d28(void);
static NOINLINE int sub_2de4(int mode);
static NOINLINE int sub_2eec(int mode);
static NOINLINE int sub_303c(void);
static NOINLINE void sub_3048(int state);
static NOINLINE int sub_322c(int ev_id, char *ev_name, void *param, int *result);
static NOINLINE int sub_3300(SceSize args, void *argp);
static NOINLINE int sub_3634(void);
static NOINLINE int sub_3640(void);
static NOINLINE int sub_368c(void);
static NOINLINE void sub_36f8(int val);
static NOINLINE int sub_3704(int cbid, int count, void *arg);
static NOINLINE int sub_38a8(int mask, int state);
static NOINLINE void sub_3a24(void);
static NOINLINE void sub_3b28(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5);
static NOINLINE int sub_3c14(int flag);
static NOINLINE void sub_3cd0(int val);
static NOINLINE int sub_3d04(void);
static NOINLINE int sub_3d10(void);
static NOINLINE int sub_3d18(void);
static NOINLINE int sub_3d38(int val);
static NOINLINE int sub_3d70(void);
static NOINLINE int sub_3d94(void *arg);
static NOINLINE int sub_3f74(void *arg);

static NOINLINE SceOff sub_41b0(void *iot, SceOff ofs, int whence);
static NOINLINE int sub_43ec(int lba, void *buf, int nsectors);
static NOINLINE int sub_4428(u32 byte_pos, void *buf, u32 byte_len);
static NOINLINE int sub_4500(void *arg);
static NOINLINE void *sub_4ccc(void);
static NOINLINE int sub_4cd8(const u32 *ctx_in);
static NOINLINE int sub_4ef4(void *out);
static NOINLINE int sub_4fa4(int *out0, int *out1, int *out2);
static NOINLINE int sub_5054(void);
static NOINLINE int sub_50c4(void);
static NOINLINE int sub_50fc(void *buf, u32 pos_lo, u32 pos_hi, int size);
static NOINLINE int sub_51a4(int addr);
static NOINLINE int sub_5218(void);
static NOINLINE int sub_5268(void);
static NOINLINE int sub_52a0(void);
static NOINLINE int sub_52ac(int *type_out);
static NOINLINE int sub_52cc(void);
static NOINLINE int sub_52d8(void *buf, int size);
static NOINLINE int sub_5308(void);
static NOINLINE int sub_531c(void);
static NOINLINE int sub_5330(void **out, int blk);
static NOINLINE int sub_535c(void **out, int blk);
static NOINLINE int sub_538c(int blk);
static NOINLINE int sub_53a8(int blk);
static NOINLINE int sub_58c4(const char *name, int *unit_out);
static NOINLINE int sub_5968(void *outdata, u32 outlen, const u32 *in);
static NOINLINE void sub_5af8(u32 lba);
static NOINLINE int sub_5b04(u32 lba, u32 cnt, int seek_only, int table_sel);
static NOINLINE int sub_5d18(const u32 *in, int *step_out);
static NOINLINE int sub_5fac(int idx);
static NOINLINE void sub_5ffc(void);
static NOINLINE int sub_6028(void);
static NOINLINE void sub_6098(u16 val);
static NOINLINE void sub_60a4(u16 val);
static NOINLINE void sub_60b0(u8 val);
static NOINLINE void sub_60bc(void **out);
static NOINLINE void sub_60cc(u32 lba, u32 cnt, u32 seek_dist, int mode, int cross_layer);
static NOINLINE void sub_63b4(u32 lba, u32 cnt, u32 seek_dist, int mode, int cross_layer);
static NOINLINE u32 sub_66f8(u32 lba, u32 prev_lba, int *cross_layer);
static NOINLINE int sub_678c(void);
static NOINLINE int sub_68c4(void);
static NOINLINE int sub_6988(int prio);
static NOINLINE int sub_69dc(void *dst, u32 dst_len, const void *src);
static NOINLINE int sub_6a98(SceSize args, void *argp);
static NOINLINE void sub_6b80(void);
static NOINLINE int sub_6bd4(u32 lba, u32 count, int skip, int table_sel, u32 *rem_lba, u32 *rem_blks);
static NOINLINE int sub_6c9c(u32 lba, u32 count, int seek_only, int table_sel);
static NOINLINE void sub_6d10(u32 lo, u32 hi);
static NOINLINE int sub_6d24(u32 start_lba, u32 blks, int table_sel, u32 *rem_lba, u32 *rem_blks);
static NOINLINE int sub_6e4c(u32 start_lba, int blks, int table_sel);
static NOINLINE u32 sub_7004(u32 lba, u32 count, u32 *aligned_start, u32 *aligned_end);
static NOINLINE int sub_703c(u32 end_lba, int table_sel);
static NOINLINE int sub_7104(u32 lba, u32 cnt, int seek_only, int mode, u32 *rem_lba, u32 *rem_blks);
static NOINLINE void sub_73b4(u32 lba);
static NOINLINE void sub_73c0(u32 lba, u32 cnt, u32 seek_dist, int mode, int cross_flags, int req_mode);
static NOINLINE void sub_78d0(u32 lba, u32 cnt, u32 seek_dist, int mode, int cross_flags);
static NOINLINE u32 sub_7ca4(u32 lba, u32 prev_lba, int *cross_flags);

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
void sceUmdSetDriveStatus(int stat);
void sceUmdClearDriveStatus(int stat);
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
int sceUmdMan_driver_80D31D5D(void *cb, void *arg);
int sceUmdMan_driver_34375DB0(void);
int sceNp9660_driver_8EF69DC6(void (*cb)(u32, u32, u32, u32, u32, u32));
int sceNp9660_driver_B925CA6C(void);
int sceNp9660_driver_7A05EB3C(int *out);

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
static u32 *g_bss_8a20;
static int g_bss_8a24;
static void *g_bss_8a28;
static u8 g_bss_8a2c[64];
static u8 g_bss_8a6c[16];
static u8 g_bss_8a7c[16];
static int g_bss_8a8c;
static int g_bss_8a90;
static int g_bss_8a94;
static int g_bss_8a98;
static int g_active_mode_8a9c;
static int g_drive_status_8aa0;
static int g_error_status_8aa4;
static SceUID g_umd_cbid_8aa8;
static SceUID g_mediaman_evf_8aac;
static char g_assign_dev_8ab0[32];
static int g_umdman_suspend_err_8ad0;
static int g_umdman_init_res_8ad4;
static SceUID g_mount_thid_8ad8;
static SceUID g_umdman_evf_8adc;
static int g_sysevent_reg_8ae0;
static int g_umdman_resume_flag_8ae4;
static int g_umdman_state_8ae8;
static int g_ins_ej_id_8aec;
static int g_ins_ej_gp_8af0;
static void *g_ins_ej_arg_8af4;
static int (*g_ins_ej_cb_8af8)(int, void *, int);
static int g_umdman_cb_gp_8afc;
static void *g_umdman_cb_arg_8b00;
static void (*g_umdman_cb_8b04)(int);
static int g_np9660_cb_gp_8b08;
static void (*g_np9660_cb_8b0c)(u32, u32, u32, u32, u32, u32);
static u32 g_np9660_err_8b10[6];
static int g_mount_status_8b28;
static int g_replace_permit_8b2c;
static int g_last_mount_err_8b30;
static SceUID g_cur_mutex_8b34;
static SceUID g_nc_mutex_8b38;
static SceUID g_main_mutex_8b3c;
static SceUID g_fpl_8b40;
static void *g_fpl_block_8b44;
static struct {
    int fd;              /* 0x00 (0x8b48) */
    int total_sectors;   /* 0x04 (0x8b4c) */
    int num_blocks;      /* 0x08 (0x8b50) */
    int block_basis;     /* 0x0c (0x8b54) */
    char disc_id[20];    /* 0x10 (0x8b58..0x8b6b) */
    int lba_param;       /* 0x24 (0x8b6c) */
    int unk_28;          /* 0x28 (0x8b70) */
    int unk_2c;          /* 0x2c (0x8b74) */
    int unk_30;          /* 0x30 (0x8b78) */
    int last_lba;        /* 0x34 (0x8b7c) */
    int thread_prio;     /* 0x38 (0x8b80) */
    u16 emu_rate2;       /* 0x3c (0x8b84) */
    u16 emu_rate1;       /* 0x3e (0x8b86) */
    u8 emu_flags;        /* 0x40 (0x8b88) */
    u8 pad_41;           /* 0x41 (0x8b89) */
    u8 flags_42;         /* 0x42 (0x8b8a) */
    u8 pad_43;           /* 0x43 (0x8b8b) */
    u32 unk_44[5];       /* 0x44..0x57 (0x8b8c..0x8b9f) */
} g_umd_ctx_8b48;
static struct {
    int in_use;
    int flags;
} g_handles_8ba0[8];
static int g_mutex_owner_pri_8be0;
static u32 g_part_info_8be4[4];
static void *g_cache_buf_8bf4;
static int g_cached_blk_8bf8;

static u32 g_emu_8bfc;
static u32 g_emu_8c00;
static u32 g_emu_8c04;
static s64 g_emu_time_8c08;
static s64 g_emu_time_8c10;
static s64 g_emu_time_8c18;
static u32 g_emu_8c20;
static int g_emu_8c24;
static u32 g_emu_8c28;
static int g_emu_8c2c;
static u32 g_emu_8c30;
static u8 g_emu_flags_8c34;
static u8 g_pvd_buf_8c38[0x800];
static SceUID g_dec_thid_9438;
static SceUID g_dec_mbx_943c;
static SceUID g_dec_evf_9440;
static int g_ring0_idx_9444;
static int g_ring1_idx_9448;
static u32 g_ring_elapsed_lo_9450;
static int g_ring_elapsed_hi_9454;
static struct {
    u8 valid;
    u8 pad[3];
    u32 lba;
} g_ring0_9480[0xe8];
static struct {
    u8 valid;
    u8 pad[3];
    u32 lba;
} g_ring1_9bc0[8];
static u32 g_emu_9c00;
static int g_emu_9c04;
static u32 g_emu_9c08;
static u32 g_emu_9c0c;
static s64 g_emu_time_9c10;
static s64 g_emu_time_9c18;
static u16 g_emu_rate1_9c20;
static u16 g_emu_rate2_9c22;
static int g_emu_9c24;

/* 0x0000 */
int NOINLINE sub_0000(const char *path, int flag, int *out_err)
{
    u32 info[6];
    sceKernelMemset(info, 0, sizeof(info));
    g_bss_8a20 = info;
    int r = sub_00c8(path, flag);
    if (r != 0) {
        if (info[0] == 0 || (info[0] == 0x80550914U && flag != 0)) {
            info[0] = (u32)r;
        }
    }
    g_bss_8a20 = NULL;
    if (out_err != NULL) {
        *out_err = (int)info[1];
    }
    sub_3b28(info[0], info[1], info[2], info[3], info[4], info[5]);
    return r;
}

/* 0x00c8 */
static NOINLINE int sub_00c8(const char *path, int flag)
{
    u8 mkey[0x30];
    u8 ckey[0x20];
    u8 idps[0x10];
    u8 vkey[0x10];
    u8 pbp_hdr[0x30];
    u8 hdr[0x100];
    u8 ctx_info[0x60];
    u8 digest[0x20];
    u8 pub[0x28];
    void *work_buf;

    for (int i = 0; i < 0x28; i++) {
        pub[i] = g_npumd_pub_8784[i];
    }
    work_buf = sub_4ccc();
    int s4 = -1;
    int s0 = sceNpDrmGetIDps(idps);
    if (s0 < 0) {
        return s0;
    }
    if (sub_368c() == 0) {
        return (int)0x80210003;
    }
    int retry = 0;
    for (;;) {
        s4 = sceIoOpen(path, 1, 0);
        retry++;
        if (s4 != (int)0x802e1012 || retry >= 30) {
            break;
        }
        sceKernelDelayThread(100000);
    }
    if (s4 < 0) {
        s0 = s4;
        goto fail;
    }
    s0 = sceIoRead(s4, pbp_hdr, 0x28);
    if (s0 < 0x28) {
        goto fail;
    }
    if (*(u32 *)(pbp_hdr + 0x00) != 0x50425000U) {
        goto fail;
    }
    u32 psar_off = *(u32 *)(pbp_hdr + 0x24);
    SceOff pos = sceIoLseek(s4, (SceOff)psar_off, 0);
    s0 = (int)pos;
    if (pos != (SceOff)psar_off) {
        goto fail;
    }
    s0 = sceIoRead(s4, hdr, 0x100);
    if (s0 < 0x100) {
        goto fail;
    }
    if (*(u32 *)(hdr + 0x00) != 0x4d55504eU || *(u32 *)(hdr + 0x04) != 0x474d4944U) {
        goto fail;
    }
    s0 = sceDdrdbHash(hdr, 0xd8, digest);
    if (s0 < 0) {
        goto fail;
    }
    s0 = sceDdrdbSigvry(pub, digest, hdr + 0xd8);
    if (s0 < 0) {
        goto fail;
    }
    int cached_match = 0;
    if (memcmp(g_bss_8a2c, hdr + 0x10, 0x30) == 0 && memcmp(g_bss_8a7c, hdr + 0xa0, 0x10) == 0) {
        cached_match = 1;
    } else {
        if (g_bss_8a2c[0] != 0) {
            if (sub_3d70() != 1) {
                s0 = (int)0x80210007;
                goto fail;
            }
        }
        sub_36f8(1);
        sub_0c38();
        sub_531c();
    }
    s0 = sub_0df8(vkey, (const char *)(hdr + 0x10), *(int *)(hdr + 0x08), work_buf);
    if (s0 < 0) {
        *g_bss_8a20 = (u32)s0;
        if (cached_match != 0 && s0 == (int)0x80550914 && flag != 0) {
            sceKernelMemcpy(vkey, g_bss_8a6c, 0x10);
        } else {
            goto fail;
        }
    }
    if (cached_match != 0) {
        if (memcmp(vkey, g_bss_8a6c, 0x10) != 0) {
            s0 = (int)0x80210007;
            goto fail;
        }
    }
    s0 = sceDrmBBMacInit(mkey, 3);
    if (s0 < 0) {
        goto fail;
    }
    s0 = sceDrmBBMacUpdate(mkey, hdr, 0xc0);
    if (s0 < 0) {
        goto fail;
    }
    s0 = sceDrmBBMacFinal2(mkey, hdr + 0xc0, vkey);
    if (s0 < 0) {
        goto fail;
    }
    s0 = sceDrmBBCipherInit(ckey, 1, 2, hdr + 0xa0, vkey, 0);
    if (s0 < 0) {
        goto fail;
    }
    s0 = sceDrmBBCipherUpdate(ckey, hdr + 0x40, 0x60);
    if (s0 < 0) {
        goto fail;
    }
    s0 = sceDrmBBCipherFinal(ckey);
    if (s0 < 0) {
        goto fail;
    }
    u32 total_sec = *(u32 *)(hdr + 0x64) - *(u32 *)(hdr + 0x54) + 1U;
    u32 basis = *(u32 *)(hdr + 0x0c);
    u32 num_blks = (total_sec / basis) + ((total_sec % basis) != 0 ? 1U : 0U);
    u32 tbl_bytes = num_blks << 5;
    if ((*(u32 *)(hdr + 0x68) & (1U << idps[5])) == 0) {
        goto fail;
    }
    u32 off_08 = *(u32 *)(pbp_hdr + 0x08);
    u32 off_0c = *(u32 *)(pbp_hdr + 0x0c);
    u32 param_len = off_0c - off_08;
    if (param_len != 0) {
        u32 off_20 = *(u32 *)(pbp_hdr + 0x20);
        if (param_len + 0x30U >= 0x801U) {
            goto fail;
        }
        pos = sceIoLseek(s4, (SceOff)off_08, 0);
        s0 = (int)pos;
        if (pos != (SceOff)off_08) {
            goto fail;
        }
        s0 = sceIoRead(s4, work_buf, param_len);
        if ((u32)s0 < param_len) {
            goto fail;
        }
        sceKernelMemcpy((u8 *)work_buf + param_len, hdr + 0x10, 0x30);
        pos = sceIoLseek(s4, (SceOff)off_20, 0);
        s0 = (int)pos;
        if (pos != (SceOff)off_20) {
            goto fail;
        }
        s0 = sceIoRead(s4, (u8 *)work_buf + param_len + 0x30, 0x28);
        if (s0 < 0x28) {
            goto fail;
        }
        s0 = sceDdrdbHash(work_buf, (int)(param_len + 0x30U), digest);
        if (s0 < 0) {
            goto fail;
        }
        s0 = sceDdrdbSigvry(pub, digest, (u8 *)work_buf + param_len + 0x30);
        if (s0 < 0) {
            goto fail;
        }
    }
    u32 off_20 = *(u32 *)(pbp_hdr + 0x20);
    pos = sceIoLseek(s4, (SceOff)off_20 + 0x560, 0);
    s0 = (int)pos;
    if (pos != (SceOff)off_20 + 0x560) {
        goto fail;
    }
    s0 = sceIoRead(s4, work_buf, 0x34);
    if (s0 < 0x34) {
        goto fail;
    }
    if (strcmp((const char *)(hdr + 0x10), (const char *)work_buf) != 0) {
        goto fail;
    }
    for (int i = 0; i < 4; i++) {
        if (hdr[0x08 + i] != ((u8 *)work_buf)[0x33 - i]) {
            goto fail;
        }
    }
    s0 = sceDrmBBMacInit(mkey, 3);
    if (s0 < 0) {
        goto fail;
    }
    u32 tbl_rel = *(u32 *)(hdr + 0x6c);
    SceOff tbl_pos = (SceOff)psar_off + (SceOff)tbl_rel;
    pos = sceIoLseek(s4, tbl_pos, 0);
    s0 = (int)pos;
    if (pos != tbl_pos) {
        goto fail;
    }
    if ((int)tbl_bytes > 0) {
        for (int read_so_far = 0; read_so_far < (int)tbl_bytes; read_so_far += 0x8000) {
            if (sub_368c() == 0) {
                s0 = (int)0x80210003;
                goto fail;
            }
            int chunk = (int)tbl_bytes - read_so_far;
            if (chunk > 0x8000) {
                chunk = 0x8000;
            }
            s0 = sceIoRead(s4, work_buf, 0x8000);
            if (s0 < 0x8000) {
                goto fail;
            }
            s0 = sceAmctrl_driver_9227EA79(mkey, work_buf, chunk);
            if (s0 < 0) {
                goto fail;
            }
        }
    }
    s0 = sceDrmBBMacFinal2(mkey, hdr + 0xb0, vkey);
    if (s0 < 0) {
        goto fail;
    }
    sceKernelMemset(ctx_info, 0, 0x58);
    *(int *)(ctx_info + 0x00) = s4;
    *(u32 *)(ctx_info + 0x04) = total_sec;
    *(u32 *)(ctx_info + 0x08) = num_blks;
    *(u32 *)(ctx_info + 0x0c) = basis;
    sceKernelMemcpy(ctx_info + 0x10, hdr + 0x40, 0x28);
    *(u32 *)(ctx_info + 0x38) = *(u32 *)(hdr + 0x84);
    *(u16 *)(ctx_info + 0x3c) = *(u16 *)(hdr + 0x80);
    *(u16 *)(ctx_info + 0x3e) = *(u16 *)(hdr + 0x82);
    sceKernelMemcpy(ctx_info + 0x40, hdr + 0x88, 0x18);

    g_bss_8a8c = (int)psar_off;
    g_bss_8a90 = (int)tbl_rel;
    g_bss_8a94 = *(int *)(hdr + 0x68);
    g_bss_8a98 = (int)hdr[0x89];
    sceKernelMemcpy(g_bss_8a2c + 0x00, hdr + 0x10, 0x30);
    sceKernelMemcpy(g_bss_8a2c + 0x30, hdr + 0x70, 0x10);
    sceKernelMemcpy(g_bss_8a6c, vkey, 0x10);
    sceKernelMemcpy(g_bss_8a7c, hdr + 0xa0, 0x10);

    s0 = sub_4cd8(ctx_info);
    sceKernelMemset(ctx_info, 0, 0x58);
    if (s0 < 0) {
        goto fail;
    }
    SysMemForKernel_F3BDB718(g_bss_8a2c + 0x30);
    sceKernelSetQTGP3(g_bss_8a2c + 0x30);
    sceKernelMemset(idps, 0, 0x10);
    sceKernelMemset(vkey, 0, 0x10);
    sceKernelMemset(hdr, 0, 0x100);
    sceKernelMemset(&work_buf, 0, 4);
    return 0;

fail:
    sceKernelMemset(idps, 0, 0x10);
    sceKernelMemset(vkey, 0, 0x10);
    sceKernelMemset(hdr, 0, 0x100);
    sceKernelMemset(&work_buf, 0, 4);
    if (s0 >= 0) {
        s0 = (int)0x80210003;
    }
    if (s0 != (int)0x80210007 && s0 != (int)0x80210003) {
        s0 = (sub_368c() == 0) ? (int)0x80210003 : (int)0x80210001;
    }
    if (s4 >= 0) {
        sceIoClose(s4);
    }
    return s0;
}

/* 0x0a34 */
static NOINLINE int sub_0a34(void *buf, void *entry)
{
    u8 *s0 = (u8 *)entry + 8;
    u32 comp_size = *(u32 *)(s0 + 0x14);
    u32 rel_off = *(u32 *)(s0 + 0x10);
    int aligned_size = (int)((comp_size + 15U) & ~15U);
    u32 seed = rel_off >> 4;
    int r = sub_1188(buf, aligned_size, s0);
    if (r < 0) {
        return r;
    }
    if ((*(u32 *)(s0 + 0x18) & 4U) == 0) {
        r = sub_10d8(buf, aligned_size, seed);
        if (r < 0) {
            return r;
        }
    }
    return 0;
}

/* 0x0ad0 */
static NOINLINE int sub_0ad0(void *buf, u32 base_file_off, u32 total_len, int first_blk, int last_blk, void *entry)
{
    (void)total_len;
    u32 span_start = 0;
    u32 span_end = 0;
    if (last_blk < first_blk) {
        return 0;
    }
    for (int blk = last_blk; blk >= first_blk; blk--) {
        int r = sub_0c4c(entry, blk);
        if (r < 0) {
            return r;
        }
        u8 *s1 = (u8 *)entry + 8;
        u32 comp_size = *(u32 *)(s1 + 0x14);
        u32 blk_file_off = *(u32 *)entry;
        u32 aligned_size = (comp_size + 15U) & ~15U;
        r = sub_1188((u8 *)buf + blk_file_off - base_file_off, (int)aligned_size, s1);
        if (r < 0) {
            return r;
        }
        if (*(u32 *)(s1 + 0x18) & 4U) {
            if (span_start != 0) {
                r = sub_10d8((u8 *)buf + span_start - base_file_off, (int)(span_end - span_start), (span_start - (u32)g_bss_8a8c) >> 4);
                span_start = 0;
                span_end = 0;
                if (r < 0) {
                    return r;
                }
            }
        } else {
            if (span_end == 0) {
                span_start = blk_file_off;
                span_end = span_start + aligned_size;
            } else {
                span_start = blk_file_off;
            }
        }
    }
    if (span_start != 0) {
        int r = sub_10d8((u8 *)buf + span_start - base_file_off, (int)(span_end - span_start), (span_start - (u32)g_bss_8a8c) >> 4);
        if (r < 0) {
            return r;
        }
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
    int s1 = g_bss_8a24;
    if (s1 == -1 || (u32)a1 < (u32)s1 || (u32)a1 >= (u32)(s1 + 16)) {
        s1 = a1 & ~15;
        u32 off = (u32)g_bss_8a8c + (u32)g_bss_8a90 + ((u32)s1 << 5);
        int rd = sub_50fc(g_bss_8a28, off, 0, 0x200);
        if (rd < 0) {
            g_bss_8a24 = -1;
            return rd;
        }
        u32 *words = (u32 *)g_bss_8a28;
        for (int i = 15; i >= 0; i--, words += 8) {
            u32 w0 = words[0];
            u32 w1 = words[1];
            u32 w2 = words[2];
            u32 w3 = words[3];
            words[4] ^= w3 ^ w2;
            words[5] ^= w2 ^ w1;
            words[6] ^= w1 ^ w0;
            words[7] ^= w0 ^ w3;
        }
        g_bss_8a24 = s1;
    }
    u8 *src = (u8 *)g_bss_8a28 + (((u32)(a1 - s1)) << 5);
    sceKernelMemcpy((u8 *)a0 + 8, src, 0x20);
    *(u32 *)((u8 *)a0 + 0) = (u32)g_bss_8a8c + *(u32 *)(src + 0x10);
    *(u32 *)((u8 *)a0 + 4) = *(u32 *)(src + 0x14);
    return 0;
}

/* 0x0dc0 */
static NOINLINE int sub_0dc0(void *buf, int size)
{
    if (size != 0x200) {
        return (int)0x80010016;
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
static NOINLINE int sub_0df8(void *vkey, const char *cid, int flags, void *act_buf)
{
    char path[0x40];
    u8 rif[0xa0];
    u32 ext_words[2] = { 0, 0 };

    if (flags < 0) {
        return (int)0x80550912;
    }
    if ((flags & 0x01000000) != 0) {
        g_bss_8a20[1] = 0x01000003U;
        return sceNpDrmGetFixedKey(vkey, cid, 0x01000003);
    }
    if (strlen(cid) >= 0x2aU) {
        return -1;
    }
    snprintf(path, 0x40, "ms0:/PSP/LICENSE/%s.rif", cid);
    int fd = sceIoOpen(path, 1, 0);
    if (fd < 0) {
        return (int)0x80550910;
    }
    int rd = sceIoRead(fd, rif, 0x98);
    if (rd == 0x98) {
        if (sceIoRead(fd, ext_words, 8) != 8) {
            ext_words[0] = 0;
            ext_words[1] = 0;
        }
    }
    sceIoClose(fd);
    if (rd < 0x98) {
        return (int)0x80550910;
    }
    if (strcmp(cid, (const char *)(rif + 0x10)) != 0) {
        return (int)0x80550909;
    }
    u32 rtype = __builtin_bswap32(*(u32 *)(rif + 0x04));
    g_bss_8a20[1] = rtype;
    g_bss_8a20[4] = __builtin_bswap32(ext_words[1]);
    g_bss_8a20[5] = __builtin_bswap32(ext_words[0]);
    u32 t_lo = *(u32 *)(rif + 0x68);
    u32 t_hi = *(u32 *)(rif + 0x6c);
    if ((t_lo | t_hi) != 0) {
        u64 t = ((u64)__builtin_bswap32(t_lo) << 32) | (u64)__builtin_bswap32(t_hi);
        t = t * 1000ULL + 0x00dcbffeff2bc000ULL;
        g_bss_8a20[2] = (u32)t;
        g_bss_8a20[3] = (u32)(t >> 32);
    }
    if (rtype == 3U) {
        sceKernelMemset(act_buf, 0, 0x1038);
    } else {
        int afd = sceIoOpen("flash2:/act.dat", 0x04000001, 0);
        if (afd < 0) {
            return (int)0x80550911;
        }
        int ard = sceIoRead(afd, act_buf, 0x1038);
        sceIoClose(afd);
        if (ard != 0x1038) {
            return (int)0x80550911;
        }
    }
    int r = sceNpDrmGetVersionKey(vkey, act_buf, rif, flags);
    return (r < 0) ? r : 0;
}

/* 0x10d8 */
static NOINLINE int sub_10d8(void *buf, int size, u32 seed)
{
    u8 ckey[32];
    int r = sceDrmBBCipherInit(ckey, 1, 2, g_bss_8a7c, g_bss_8a6c, seed);
    if (r < 0) {
        return r;
    }
    if ((g_bss_8a98 != 0) && (((u32)buf & 3U) == 0)) {
        r = sceAmctrl_driver_E04ADD4C(ckey, buf, size);
    } else {
        r = sceDrmBBCipherUpdate(ckey, buf, size);
    }
    if (r < 0) {
        return r;
    }
    r = sceDrmBBCipherFinal(ckey);
    return r < 0 ? r : 0;
}

/* 0x1188 */
static NOINLINE int sub_1188(const void *buf, int size, const void *hash)
{
    u8 mkey[32];
    int r = sceDrmBBMacInit(mkey, 3);
    if (r < 0) {
        return r;
    }
    if ((g_bss_8a98 != 0) && (((u32)buf & 3U) == 0)) {
        r = sceAmctrl_driver_9227EA79(mkey, buf, size);
    } else {
        r = sceDrmBBMacUpdate(mkey, buf, size);
    }
    if (r < 0) {
        return r;
    }
    r = sceDrmBBMacFinal2(mkey, hash, g_bss_8a6c);
    return r < 0 ? r : 0;
}

/* 0x1228 */
static NOINLINE int sub_1228(void *dst, u32 dst_len, const void *src, u32 *next_in)
{
    u8 probs[0xaf8];
    const u8 *in = (const u8 *)src;
    u8 *out = (u8 *)dst;
    u8 *out_end = out + dst_len;

    s8 hdr = (s8)*in;
    u32 code = ((u32)in[1] << 24) | ((u32)in[2] << 16) | ((u32)in[3] << 8) | (u32)in[4];
    const u8 *in_ptr = in + 5;

    if (hdr < 0) {
        u8 *copy_end = out + code;
        if (copy_end >= out_end) {
            return (int)0x80000104;
        }
        if (code != 0xffffffffU) {
            do {
                *out++ = *in_ptr++;
            } while (out < copy_end + 1);
        }
        if (next_in != NULL) {
            *next_in = (u32)in_ptr;
        }
        return (int)code;
    }

    u8 *p_end = probs + 0xaf8;
    u32 align_end = ((u32)probs + 0xaf8) & ~0x3fU;
    u8 *p = probs;
    u32 init_prob = 0x80 - ((((u32)(u8)hdr >> 3) & 3) << 3);

    do {
        if (((u32)p & 0x3fU) == 0 && (u32)p < align_end) {
            __asm__ volatile("cache 0x18, 0(%0)" :: "r"(p) : "memory");
        }
        *p++ = (u8)init_prob;
    } while (p != p_end);

    u32 lc = (u32)(u8)hdr & 7U;
    u32 range = 0xffffffffU;
    u32 prev_byte = 0;
    u8 *state = probs;

    for (;;) {
        u8 *s0 = state;
        for (;;) {
            if (range < 0x01000000U) {
                range <<= 8;
                code = (code << 8) + *in_ptr++;
            }
            u8 prob = s0[0x9a0];
            u32 bound = (range >> 8) * prob;
            if (code >= bound) {
                range -= bound;
                code -= bound;
                s0[0x9a0] = (u8)(prob - (prob >> 3));
                break;
            }
            if (out >= out_end) {
                return (int)0x80000104;
            }
            u32 ctx = prev_byte >> lc;
            range = bound;
            s0[0x9a0] = (u8)(prob + (((0x100 - (int)prob) >> 3) & 0x1f));
            u8 *lit_probs = probs + ctx * 0xff;
            u32 sym = 1;
            do {
                if (range < 0x01000000U) {
                    range <<= 8;
                    code = (code << 8) + *in_ptr++;
                }
                u8 lprob = lit_probs[sym];
                u32 lbound = (range >> 8) * lprob;
                sym <<= 1;
                if (code < lbound) {
                    range = lbound;
                    lit_probs[sym >> 1] = (u8)(lprob + (((0x100 - (int)lprob) >> 3) & 0x1f));
                } else {
                    range -= lbound;
                    code -= lbound;
                    lit_probs[sym >> 1] = (u8)(lprob - (lprob >> 3));
                    sym |= 1;
                }
            } while ((sym >> 8) == 0);
            prev_byte = sym & 0xff;
            *out++ = (u8)prev_byte;
            s0 = probs;
        }

        u32 len_cat = 0xffffffffU;
        for (;;) {
            state += 4;
            if (range < 0x01000000U) {
                range <<= 8;
                code = (code << 8) + *in_ptr++;
            }
            len_cat++;
            u8 prob = state[0x9c0];
            u32 bound = (range >> 8) * prob;
            if (code < bound) {
                range = bound;
                state[0x9c0] = (u8)(prob + (((0x100 - (int)prob) >> 3) & 0x1f));
                break;
            }
            range -= bound;
            code -= bound;
            state[0x9c0] = (u8)(prob - (prob >> 3));
            if (len_cat >= 6) {
                len_cat++;
                break;
            }
        }

        u32 match_len;
        if ((int)len_cat <= 0) {
            match_len = len_cat;
        } else {
            u32 l_bits = len_cat - 1;
            u32 limit = 1U << l_bits;
            u8 *len_base;
            if (len_cat < 4) {
                len_base = probs + (len_cat << 5) + limit;
            } else {
                len_base = probs + (l_bits << 3);
            }
            u32 tree = 1;
            do {
                if (range < 0x01000000U) {
                    range <<= 8;
                    code = (code << 8) + *in_ptr++;
                }
                u8 *p_prob = len_base + tree;
                u8 prob;
                if (tree >= 0x10) {
                    if (tree >= 0x20) {
                        if (tree >= 0x40) {
                            prob = p_prob[0x9f8];
                            u32 bound = (range >> 8) * prob;
                            tree <<= 1;
                            if (code < bound) {
                                range = bound;
                                p_prob[0x9f8] = (u8)(prob + (((0x100 - (int)prob) >> 3) & 0x1f));
                            } else {
                                range -= bound;
                                code -= bound;
                                p_prob[0x9f8] = (u8)(prob - (prob >> 3));
                                tree |= 1;
                            }
                            continue;
                        }
                        prob = p_prob[0xa00];
                        u32 bound = (range >> 8) * prob;
                        tree <<= 1;
                        if (code < bound) {
                            range = bound;
                            p_prob[0xa00] = (u8)(prob + (((0x100 - (int)prob) >> 3) & 0x1f));
                        } else {
                            range -= bound;
                            code -= bound;
                            p_prob[0xa00] = (u8)(prob - (prob >> 3));
                            tree |= 1;
                        }
                        continue;
                    }
                    prob = p_prob[0xa08];
                    u32 bound = (range >> 8) * prob;
                    tree <<= 1;
                    if (code < bound) {
                        range = bound;
                        p_prob[0xa08] = (u8)(prob + (((0x100 - (int)prob) >> 3) & 0x1f));
                    } else {
                        range -= bound;
                        code -= bound;
                        p_prob[0xa08] = (u8)(prob - (prob >> 3));
                        tree |= 1;
                    }
                    continue;
                }
                prob = p_prob[0xa10];
                u32 bound = (range >> 8) * prob;
                tree <<= 1;
                if (code < bound) {
                    range = bound;
                    p_prob[0xa10] = (u8)(prob + (((0x100 - (int)prob) >> 3) & 0x1f));
                } else {
                    range -= bound;
                    code -= bound;
                    p_prob[0xa10] = (u8)(prob - (prob >> 3));
                    tree |= 1;
                }
            } while (tree < limit);

            match_len = tree + 1;
            if (match_len == 0x100) {
                if (next_in != NULL) {
                    *next_in = (u32)in_ptr;
                }
                return (int)(out - (u8 *)dst);
            }
        }

        u32 dist_ctx = len_cat < 4 ? len_cat : 3;
        u32 dist_limit = (match_len == 0) ? 0x40 : 0x160;
        u8 *dist_base = probs + (dist_ctx << 6) + dist_limit;
        u32 dist_tree = 1;
        do {
            if (range < 0x01000000U) {
                range <<= 8;
                code = (code << 8) + *in_ptr++;
            }
            u8 *p_prob = dist_base + dist_tree;
            u8 prob = p_prob[0x7f1];
            u32 bound = (range >> 8) * prob;
            dist_tree <<= 1;
            if (code < bound) {
                range = bound;
                p_prob[0x7f1] = (u8)(prob + (((0x100 - (int)prob) >> 3) & 0x1f));
            } else {
                range -= bound;
                code -= bound;
                p_prob[0x7f1] = (u8)(prob - (prob >> 3));
                dist_tree |= 1;
            }
        } while (dist_tree < 0x40);

        u32 dist_slot = dist_tree & 0x3f;
        u32 dist;
        if (dist_slot == 0) {
            dist = 0;
        } else {
            u32 d_bits = ((dist_slot + 6) >> 1) - 3;
            u32 d_base_idx = 0;
            u32 d_low = 2 + (dist_slot & 1);
            if (dist_slot < 8) {
                d_base_idx = (d_low << d_bits) - dist_slot;
            }
            u8 *d_probs = probs + d_base_idx + 0x928;
            u32 d_val = 1;
            if ((int)d_bits > 0) {
                u32 bit_cnt = 0;
                do {
                    if (range < 0x01000000U) {
                        range <<= 8;
                        code = (code << 8) + *in_ptr++;
                    }
                    bit_cnt++;
                    if (d_val < 0x10) {
                        u8 *p_prob = d_probs + d_val;
                        u8 prob = *p_prob;
                        u32 bound = (range >> 8) * prob;
                        d_val <<= 1;
                        if (code < bound) {
                            range = bound;
                            *p_prob = (u8)(prob + (((0x100 - (int)prob) >> 3) & 0x1f));
                        } else {
                            range -= bound;
                            code -= bound;
                            *p_prob = (u8)(prob - (prob >> 3));
                            d_val |= 1;
                        }
                    } else {
                        range >>= 1;
                        u32 diff = code - range;
                        d_val <<= 1;
                        if ((int)diff >= 0) {
                            code = diff;
                            d_val |= 1;
                        }
                    }
                } while (bit_cnt != d_bits);
            }
            dist = (d_val & ((1U << d_bits) - 1)) + (d_low << d_bits);
        }

        u8 *match_end = out + match_len + 2;
        if (match_end > out_end) {
            return (int)0x80000104;
        }
        u8 *match_src = out - dist - 1;
        if (match_src < (u8 *)dst) {
            return (int)0x80000108;
        }

        state = probs + (dist == 0 ? 4 : 8);
        if (dist >= 3 && (((u32)match_src ^ (u32)out) & 3) == 0) {
            while (((u32)out & 3) != 0) {
                *out++ = *match_src++;
                if (out == match_end) {
                    goto match_done;
                }
            }
            u8 *fast_end = match_end - 3;
            while (out < fast_end) {
                u32 w = *(u32 *)match_src;
                match_src += 4;
                *(u32 *)out = w;
                out += 4;
            }
            if (out >= match_end) {
                goto match_done;
            }
        }
        do {
            *out++ = *match_src++;
        } while (out < match_end);
match_done:
        prev_byte = match_src[-1];
    }
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
    if (err == 0) {
        return 0;
    }
    if (sceKernelGetCompiledSdkVersion() != 0) {
        return err;
    }
    if (err == (int)0x80010074) {
        return (int)0x8001006e;
    }
    if (err > (int)0x80010074) {
        if (err == (int)0x80010087) {
            return (int)0x8001007b;
        }
        if (err > (int)0x80010087) {
            if (err == (int)0x8001b006) {
                return (int)0x8001007c;
            }
            return err;
        }
        if (err == (int)0x80010086) {
            return (int)0x8001b000;
        }
        return err;
    }
    if (err == (int)0x80010070) {
        return (int)0x80010062;
    }
    if (err > (int)0x80010070) {
        if (err == (int)0x80010071) {
            return (int)0x80010067;
        }
        return err;
    }
    if (err == (int)0x8001005b) {
        return (int)0x80010024;
    }
    return err;
}

/* 0x2348 */
int sceUmdActivate(int mode, const char *aliasname)
{
    int k1 = pspSdkSetK1(0);
    int r = (int)0x80010016;
    if ((((u32)(mode - 1) >= 2U) || aliasname == NULL) == 0 &&
        ((k1 << 11) & (int)aliasname) >= 0 &&
        strcmp(aliasname, "disc0:") == 0) {
        int res = sub_2408(mode, aliasname);
        int intr = sceKernelCpuSuspendIntr();
        g_error_status_8aa4 = res;
        sceKernelCpuResumeIntr(intr);
        r = 0;
    }
    pspSdkSetK1(k1);
    return r;
}

/* 0x2408 */
static NOINLINE int sub_2408(int mode, const char *aliasname)
{
    if (strcmp(g_assign_dev_8ab0, aliasname) != 0) {
        int ronly = 1;
        if (g_active_mode_8a9c != 0) {
            return (int)0x80010010;
        }
        int r = sceIoAssign(aliasname, "umd0:", "isofs0:", 1, &ronly, 4);
        if (r != 0) {
            g_active_mode_8a9c = 0;
            return r;
        }
    }
    int r = sub_2de4(mode);
    if (r == 0) {
        strncpy(g_assign_dev_8ab0, aliasname, 0x20);
        g_active_mode_8a9c = 2;
        return 0;
    }
    g_active_mode_8a9c = 0;
    return r;
}

/* 0x24e0 */
int sceUmdRegisterUMDCallBack(SceUID cbid)
{
    int k1 = pspSdkSetK1(0);
    void *cb = NULL;
    int r = (int)0x80010016;
    if (sceKernelGetThreadmanIdType(cbid) == 8 &&
        sceKernelGetUIDcontrolBlock(cbid, &cb) == 0) {
        int ok = 2;
        if (((u32)(k1 << 11) >> 31) != 0) {
            ok = *(u16 *)((u8 *)cb + 0x16) & 2;
        }
        if (ok != 0) {
            int intr = sceKernelCpuSuspendIntr();
            g_umd_cbid_8aa8 = cbid;
            sceKernelCpuResumeIntr(intr);
            r = 0;
        }
    }
    pspSdkSetK1(k1);
    return r;
}

/* 0x258c */
int sceUmdUnRegisterUMDCallBack(SceUID cbid)
{
    int k1 = pspSdkSetK1(0);
    void *cb = NULL;
    int r = (int)0x80010016;
    if (sceKernelGetUIDcontrolBlock(cbid, &cb) == 0) {
        int ok = 2;
        if (((u32)(k1 << 11) >> 31) != 0) {
            ok = *(u16 *)((u8 *)cb + 0x16) & 2;
        }
        if (ok != 0 && g_umd_cbid_8aa8 == cbid) {
            int intr = sceKernelCpuSuspendIntr();
            g_umd_cbid_8aa8 = 0;
            sceKernelCpuResumeIntr(intr);
            int sdk = sceKernelGetCompiledSdkVersion();
            r = (sdk > 0x03000000) ? 0 : cbid;
        }
    }
    pspSdkSetK1(k1);
    return r;
}

/* 0x2640 */
static NOINLINE int sub_2640(void)
{
    strncpy(g_assign_dev_8ab0, ":DMU", 0x20);
    SceUID evf = sceKernelCreateEventFlag("SceMediaManUser", 0x201, 0, NULL);
    if (evf >= 0) {
        g_active_mode_8a9c = 0;
        g_mediaman_evf_8aac = evf;
        return 0;
    }
    return evf;
}

/* 0x26a4 */
static NOINLINE int sub_26a4(void)
{
    if (g_mediaman_evf_8aac > 0) {
        sceKernelDeleteEventFlag(g_mediaman_evf_8aac);
        g_mediaman_evf_8aac = 0;
    }
    return 0;
}

/* 0x26e0 */
int NOINLINE sceUmdGetDriveStatus(void)
{
    return g_drive_status_8aa0;
}

/* 0x26ec */
void NOINLINE sceUmdSetDriveStatus(int stat)
{
    int intr = sceKernelCpuSuspendIntr();
    g_drive_status_8aa0 |= stat;
    sceKernelSetEventFlag(g_mediaman_evf_8aac, (u32)stat);
    sceKernelCpuResumeIntr(intr);
}

/* 0x2744 */
void NOINLINE sceUmdClearDriveStatus(int stat)
{
    int intr = sceKernelCpuSuspendIntr();
    int mask = ~stat;
    sceKernelClearEventFlag(g_mediaman_evf_8aac, (u32)mask);
    g_drive_status_8aa0 &= mask;
    sceKernelCpuResumeIntr(intr);
}

/* 0x27a0 */
int sceUmdGetErrorStatus(void)
{
    return g_error_status_8aa4;
}

/* 0x27ac */
void NOINLINE sceUmdSetErrorStatus(int err)
{
    int intr = sceKernelCpuSuspendIntr();
    g_error_status_8aa4 = err;
    sceKernelCpuResumeIntr(intr);
}

/* 0x27e0 */
static NOINLINE void sub_27e0(int stat)
{
    SceUID cbid = g_umd_cbid_8aa8;
    if (cbid != 0) {
        sceKernelNotifyCallback(cbid, stat);
    }
}

/* 0x2818 */
int sceUmdCheckMedium(void)
{
    int k1 = pspSdkSetK1(0);
    int r = sub_368c();
    pspSdkSetK1(k1);
    return r;
}

/* 0x2844 */
int sceUmdDeactivate(int mode, const char *aliasname)
{
    int k1 = pspSdkSetK1(0);
    int r = (int)0x80010016;
    if (((k1 << 11) & (int)aliasname) >= 0) {
        r = sub_2b28(mode, aliasname);
        int intr = sceKernelCpuSuspendIntr();
        g_error_status_8aa4 = r;
        sceKernelCpuResumeIntr(intr);
    }
    pspSdkSetK1(k1);
    return r;
}

/* 0x28ac */
int sceUmdGetDiscInfo(u32 *info)
{
    int k1 = pspSdkSetK1(0);
    int r = (int)0x80010016;
    if (info != NULL && (((int)info | ((int)info + 8)) & (k1 << 11)) >= 0) {
        if (info[0] == 8) {
            r = sub_52ac((int *)info);
        }
    }
    pspSdkSetK1(k1);
    return r;
}

/* 0x291c */
int sceUmdGetDriveStat(void)
{
    int k1 = pspSdkSetK1(0);
    int r = g_drive_status_8aa0;
    pspSdkSetK1(k1);
    return r;
}

/* 0x2934 */
int sceUmdGetErrorStat(void)
{
    int k1 = pspSdkSetK1(0);
    int r = sceUmd_040A7090(g_error_status_8aa4);
    pspSdkSetK1(k1);
    return r;
}

/* 0x2960 */
int sceUmdReplaceProhibit(void)
{
    int k1 = pspSdkSetK1(0);
    int r = sub_3d38(0);
    pspSdkSetK1(k1);
    return r;
}

/* 0x2990 */
int sceUmdReplacePermit(void)
{
    int k1 = pspSdkSetK1(0);
    int r = sub_3d38(1);
    pspSdkSetK1(k1);
    return r;
}

/* 0x29c0 */
int sceUmdWaitDriveStat(int stat)
{
    int k1 = pspSdkSetK1(0);
    u32 out;
    int r = (int)0x80010016;
    if ((stat & 0x3b) != 0) {
        r = sceKernelWaitEventFlag(g_mediaman_evf_8aac, (u32)stat, 1, &out, NULL);
    }
    pspSdkSetK1(k1);
    return r;
}

/* 0x2a18 */
int sceUmdWaitDriveStatWithTimer(int stat, u32 timeout)
{
    int k1 = pspSdkSetK1(0);
    u32 to = timeout;
    u32 out;
    int r = (int)0x80010016;
    if ((stat & 0x3b) != 0) {
        r = sceKernelWaitEventFlag(g_mediaman_evf_8aac, (u32)stat, 1, &out, timeout ? &to : NULL);
    }
    pspSdkSetK1(k1);
    return r;
}

/* 0x2a80 */
int sceUmdWaitDriveStatCB(int stat, u32 timeout)
{
    int k1 = pspSdkSetK1(0);
    u32 to = timeout;
    u32 out;
    int r = (int)0x80010016;
    if ((stat & 0x3b) != 0) {
        r = sceKernelWaitEventFlagCB(g_mediaman_evf_8aac, (u32)stat, 1, &out, timeout ? &to : NULL);
    }
    pspSdkSetK1(k1);
    return r;
}

/* 0x2ae8 */
int sceUmdCancelWaitDriveStat(void)
{
    int k1 = pspSdkSetK1(0);
    int r = sceKernelCancelEventFlag(g_mediaman_evf_8aac, (u32)g_drive_status_8aa0, NULL);
    pspSdkSetK1(k1);
    return r;
}

/* 0x2b28 */
static NOINLINE int sub_2b28(int mode, const char *aliasname)
{
    if ((u32)mode >= 3U || (mode == 2 && aliasname == NULL)) {
        return (int)0x80010016;
    }
    if (g_active_mode_8a9c == 0) {
        return 0;
    }
    int r = sceIoUnassign(aliasname);
    if (r != 0) {
        return r;
    }
    sub_2eec(mode);
    g_active_mode_8a9c = 0;
    memset(g_assign_dev_8ab0, 0, 0x20);
    __builtin_memcpy(g_assign_dev_8ab0, "UMD:", 5);
    return 0;
}

/* 0x2bf0 */
static NOINLINE int sub_2bf0(void)
{
    g_umdman_state_8ae8 = 0x20;
    sceUmdClearDriveStatus(0x3e);
    sceUmdSetDriveStatus(1);
    int r = sceKernelRegisterSysEventHandler(&g_sysevent_89c8);
    if (r != 0) {
        sub_2d28();
        return r;
    }
    g_sysevent_reg_8ae0 = 1;
    SceUID evf = sceKernelCreateEventFlag("SceUmdManState", 0x201, 0, NULL);
    g_umdman_evf_8adc = evf;
    if (evf < 0) {
        sub_2d28();
        return evf;
    }
    SceUID th = sceKernelCreateThread("SceNpUmdMount", sub_3300, 0x0f, 0x1000, 0x00100001, NULL);
    g_mount_thid_8ad8 = th;
    if (th < 0) {
        sub_2d28();
        return th;
    }
    r = sceKernelStartThread(th, 0, NULL);
    if (r < 0) {
        sub_2d28();
        return r;
    }
    r = sceKernelWaitEventFlagCB(g_umdman_evf_8adc, 0x40, 0x21, NULL, NULL);
    if (r >= 0 && g_umdman_init_res_8ad4 >= 0) {
        return 0;
    }
    sub_2d28();
    return (r < 0) ? r : g_umdman_init_res_8ad4;
}

/* 0x2d28 */
static NOINLINE int sub_2d28(void)
{
    sub_3048(0x80);
    sceUmdSetErrorStatus((int)0x80210003);
    if (g_mount_thid_8ad8 != 0) {
        sceKernelSetEventFlag(g_umdman_evf_8adc, 0x80);
        sceKernelWaitThreadEnd(g_mount_thid_8ad8, NULL);
        sceKernelDeleteThread(g_mount_thid_8ad8);
        g_mount_thid_8ad8 = 0;
    }
    if (g_umdman_evf_8adc != 0) {
        sceKernelDeleteEventFlag(g_umdman_evf_8adc);
        g_umdman_evf_8adc = 0;
    }
    if (g_sysevent_reg_8ae0 != 0) {
        sceKernelUnregisterSysEventHandler(&g_sysevent_89c8);
        g_sysevent_reg_8ae0 = 0;
    }
    return 0;
}

/* 0x2de4 */
static NOINLINE int sub_2de4(int mode)
{
    (void)mode;
    int r = sub_5054();
    if (r < 0) {
        return r;
    }
    int intr = sceKernelCpuSuspendIntr();
    int prev = g_data_89c0;
    int st = sub_303c();
    g_data_89c0 = 2;
    if ((st & 0xc0) == 0) {
        if (st == 0x20) {
            sub_3048(0x20);
        } else if (prev == 0) {
            sub_3048(1);
            sceUmdSetErrorStatus(0);
            sceKernelSetEventFlag(g_umdman_evf_8adc, 1);
        } else if (prev == 1) {
            if (st == 1) {
                sub_3048(4);
                sceKernelSetEventFlag(g_umdman_evf_8adc, 1);
            } else if (st != prev) {
                sub_3048(0x10);
            }
        } else {
            if (st == 8 || (st & 5) == 0) {
                sub_3048((st == 8) ? 8 : 0x10);
            }
        }
    }
    sceKernelCpuResumeIntr(intr);
    return sub_50c4();
}

/* 0x2eec */
static NOINLINE int sub_2eec(int mode)
{
    int r = sub_5054();
    if (r < 0) {
        return r;
    }
    int intr = sceKernelCpuSuspendIntr();
    int prev = g_data_89c0;
    int st = sub_303c();
    int next_mode;
    if (mode == 1) {
        next_mode = 1;
    } else if (mode < 2) {
        next_mode = 0;
    } else if (mode == 2) {
        next_mode = (prev != 0) ? 1 : 0;
    } else {
        next_mode = 0;
    }
    g_data_89c0 = next_mode;
    g_data_89c4 = 1;
    if ((st & 0xc0) == 0) {
        if (st == 0x20) {
            sub_3048(0x20);
        } else if (next_mode == 1) {
            if (prev == 0) {
                sub_3048(1);
                sceUmdSetErrorStatus(0);
                sceKernelSetEventFlag(g_umdman_evf_8adc, 1);
            } else if ((st & 0x0e) != 0) {
                sub_3048(2);
            } else if (st != 1) {
                sub_3048(0x10);
            }
        } else {
            sub_3048(0x10);
            sceUmdSetErrorStatus((int)0xc0210006);
            if (sub_3d04() != (int)0x80210003) {
                sub_5218();
            }
        }
    }
    sceKernelCpuResumeIntr(intr);
    return sub_50c4();
}

/* 0x303c */
static NOINLINE int sub_303c(void)
{
    return g_umdman_state_8ae8;
}

/* 0x3048 */
static NOINLINE void sub_3048(int state)
{
    int intr = sceKernelCpuSuspendIntr();
    int prev = g_umdman_state_8ae8;
    g_umdman_state_8ae8 = state;
    if (state == 8) {
        sceUmdClearDriveStatus(9);
        sceUmdSetDriveStatus(0x32);
        if (sceKernelGetCompiledSdkVersion() != 0 || sub_5268() == 1) {
            sub_27e0(0x32);
        } else {
            sub_27e0(0x22);
        }
    } else if ((u32)state < 9U) {
        if (state == 2 || state == 4) {
            sceUmdClearDriveStatus(0x2d);
            sceUmdSetDriveStatus(0x12);
            if (sub_5268() != 1) {
                sub_27e0(0x12);
            }
        } else if (state == 1) {
            sceUmdClearDriveStatus(0x3d);
            sceUmdSetDriveStatus(2);
            sub_27e0(2);
        }
    } else if (state == 0x20) {
        if (prev != 0x40) {
            sceUmdClearDriveStatus(0x3e);
            sceUmdSetDriveStatus(1);
            sub_27e0(1);
            if (prev != state) {
                sub_3a24();
            }
        }
    } else if ((u32)state < 0x21U) {
        if (state == 0x10) {
            sceUmdClearDriveStatus(0x35);
            sceUmdSetDriveStatus(0x0a);
            sub_27e0(8);
        }
    } else if (state == 0x40 || state == 0x80) {
        if (state != 0x40 || (sub_52a0() & 8) == 0) {
            sceUmdClearDriveStatus(0x3e);
            sceUmdSetDriveStatus(1);
            if (state == 0x40 && prev != 0x20) {
                sub_27e0(1);
            }
        }
    }
    sceKernelCpuResumeIntr(intr);
}

/* 0x322c */
static int sub_322c(int ev_id, char *ev_name, void *param, int *result)
{
    (void)ev_name;
    (void)param;
    (void)result;
    if (ev_id == 0x402) {
        return g_umdman_suspend_err_8ad0;
    }
    if (ev_id < 0x403) {
        if (ev_id == 0x400) {
            g_umdman_suspend_err_8ad0 = (int)0x80000021;
            sub_3048(0x40);
            sceUmdSetErrorStatus((int)0x80210003);
            sceKernelSetEventFlag(g_umdman_evf_8adc, 0x10);
        }
        return 0;
    }
    if (ev_id == 0x10009) {
        g_umdman_resume_flag_8ae4 = 1;
        sub_3048(0x20);
        return 0;
    }
    if (ev_id == 0x400000) {
        sceKernelSetEventFlag(g_umdman_evf_8adc, 0x20);
    }
    return 0;
}

/* 0x3300 */
static int sub_3300(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    SceUID cbid = 0;
    int r = sceKernelCreateCallback("SceNp9660", sub_3704, NULL);
    if (r >= 0) {
        cbid = r;
        SceUID cb_arg = cbid;
        r = sceIoDevctl("fatms0:", 0x02415821, &cb_arg, 4, NULL, 0);
        if (r < 0) {
            sceKernelDeleteCallback(cbid);
        }
    }
    g_umdman_init_res_8ad4 = r;
    sceKernelSetEventFlag(g_umdman_evf_8adc, 0x40);
    if (r < 0) {
        return 0;
    }
    for (;;) {
        u32 out = 0;
        if (sceKernelWaitEventFlagCB(g_umdman_evf_8adc, 0xb3, 0x21, &out, NULL) < 0) {
            break;
        }
        if ((out & 0x80U) != 0) {
            break;
        }
        if ((out & 0x20U) == 0) {
            if (sub_5054() < 0) {
                break;
            }
        }
        if ((out & 1U) != 0) {
            int intr = sceKernelCpuSuspendIntr();
            int st = sub_303c();
            if (st == 1) {
                int r_flag = g_umdman_resume_flag_8ae4;
                g_umdman_resume_flag_8ae4 = 0;
                if (g_data_89c0 == 0) {
                    sub_3048(0x10);
                    sceUmdSetErrorStatus((int)0xc0210006);
                    if (sub_3d04() != (int)0x80210003) {
                        sub_5218();
                    }
                    sceKernelCpuResumeIntr(intr);
                    goto check_10;
                }
                sceKernelCpuResumeIntr(intr);
                int mr = sub_3c14(r_flag);
                intr = sceKernelCpuSuspendIntr();
                st = sub_303c();
                if (mr < 0) {
                    if (st == 1) {
                        sub_3048(0x10);
                        sceUmdSetErrorStatus(mr);
                    }
                    sceKernelCpuResumeIntr(intr);
                    goto check_10;
                }
                if (st != 1) {
                    sub_5218();
                    sceKernelCpuResumeIntr(intr);
                    goto check_10;
                }
                if (g_data_89c0 != 2) {
                    sub_3048(2);
                    sceKernelCpuResumeIntr(intr);
                    goto check_10;
                }
                sub_3048(4);
                st = 4;
            }
            if (st == 4) {
                int arg1 = g_data_89c4;
                sceKernelCpuResumeIntr(intr);
                int cb_res = sub_38a8(0x20, arg1);
                if (cb_res != (int)0x80010002) {
                    intr = sceKernelCpuSuspendIntr();
                    st = sub_303c();
                    if (cb_res < 0) {
                        if (st == 4) {
                            sub_3048(0x10);
                            sceUmdSetErrorStatus((int)0x80210001);
                        }
                    } else if (st == 4) {
                        g_data_89c4 = 0;
                        sub_3048(8);
                        sceUmdSetErrorStatus(0);
                    } else {
                        sceUmdClearDriveStatus(4);
                    }
                    sceKernelCpuResumeIntr(intr);
                }
            } else {
                sceKernelCpuResumeIntr(intr);
            }
        }
check_10:
        if ((out & 0x10U) != 0) {
            if (sub_3d04() != (int)0x80210003) {
                sub_5218();
            }
            g_umdman_suspend_err_8ad0 = 0;
        } else if (sub_50c4() < 0) {
            break;
        }
    }
    SceUID cb_arg = cbid;
    sceIoDevctl("fatms0:", 0x02415822, &cb_arg, 4, NULL, 0);
    sceKernelDeleteCallback(cbid);
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
    sub_3048(0x80);
    sceUmdSetErrorStatus((int)0x80210003);
    if (sub_3d04() != (int)0x80210003) {
        sub_5218();
    }
    return 0;
}

/* 0x368c */
static NOINLINE int sub_368c(void)
{
    u32 st = 0;
    if ((sub_303c() & 0xe0) != 0) {
        return 0;
    }
    int r = sceIoDevctl("fatms0:", 0x02425823, NULL, 0, &st, 4);
    if (r < 0) {
        return r;
    }
    return (st == 1U) ? 1 : 0;
}

/* 0x36f8 */
static NOINLINE void sub_36f8(int val)
{
    g_data_89c4 = val;
}

/* 0x3704 */
static int sub_3704(int cbid, int count, void *arg)
{
    (void)cbid;
    (void)arg;
    if ((sub_303c() & 0xc0) != 0) {
        return 0;
    }
    if (count == 1) {
        if (sub_303c() != 1) {
            sub_3048(1);
            sceUmdSetErrorStatus(0);
            sceKernelSetEventFlag(g_umdman_evf_8adc, 1);
        }
    } else if (count == 2) {
        sub_5218();
        if (sub_303c() != 0x20) {
            sub_3048(0x20);
            sceUmdSetErrorStatus((int)0x80210003);
        }
    } else {
        sub_3048(1);
        sub_3048(0x10);
        sceUmdSetErrorStatus((int)0x80210001);
    }
    return 0;
}

/* 0x37d4 */
int sceUmdManRegisterInsertEjectUMDCallBack(int id, void *cb, void *arg)
{
    int intr = sceKernelCpuSuspendIntr();
    int r = (int)0x8001000c;
    if (g_ins_ej_id_8aec == 0) {
        u32 gp_val;
        __asm__ volatile ("move %0, $gp" : "=r"(gp_val));
        g_ins_ej_id_8aec = id;
        g_ins_ej_gp_8af0 = gp_val;
        g_ins_ej_arg_8af4 = arg;
        g_ins_ej_cb_8af8 = (int (*)(int, void *, int))cb;
        r = 0;
    }
    sceKernelCpuResumeIntr(intr);
    if (r == 0) {
        if ((sceUmdGetDriveStatus() & 0x12) != 0 && sub_303c() == 4) {
            SceUID evf = sub_3634();
            if (evf != 0) {
                sceKernelSetEventFlag(evf, 1);
            }
        }
    }
    return r;
}

/* 0x38a8 */
static NOINLINE int sub_38a8(int mask, int arg2)
{
    int r = (int)0x80010002;
    if (g_ins_ej_cb_8af8 != NULL && (g_ins_ej_id_8aec & mask) != 0) {
        u32 saved_gp;
        __asm__ volatile ("move %0, $gp\n\tmove $gp, %1" : "=r"(saved_gp) : "r"(g_ins_ej_gp_8af0));
        r = g_ins_ej_cb_8af8(g_ins_ej_id_8aec, g_ins_ej_arg_8af4, arg2);
        __asm__ volatile ("move $gp, %0" : : "r"(saved_gp));
    }
    return r;
}

/* 0x3920 */
int sceUmdManUnRegisterInsertEjectUMDCallBack(int id)
{
    int intr = sceKernelCpuSuspendIntr();
    int r = (int)0x80010002;
    if (g_ins_ej_id_8aec == id) {
        g_ins_ej_id_8aec = 0;
        g_ins_ej_gp_8af0 = 0;
        g_ins_ej_arg_8af4 = NULL;
        g_ins_ej_cb_8af8 = NULL;
        r = 0;
    }
    sceKernelCpuResumeIntr(intr);
    return r;
}

/* 0x398c */
int sceUmdMan_driver_80D31D5D(void *cb, void *arg)
{
    int intr = sceKernelCpuSuspendIntr();
    u32 gp_val;
    __asm__ volatile ("move %0, $gp" : "=r"(gp_val));
    g_umdman_cb_gp_8afc = gp_val;
    g_umdman_cb_arg_8b00 = arg;
    g_umdman_cb_8b04 = (void (*)(int))cb;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x39e4 */
int sceUmdMan_driver_34375DB0(void)
{
    int intr = sceKernelCpuSuspendIntr();
    g_umdman_cb_gp_8afc = 0;
    g_umdman_cb_arg_8b00 = NULL;
    g_umdman_cb_8b04 = NULL;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x3a24 */
static NOINLINE void sub_3a24(void)
{
    if (g_umdman_cb_8b04 != NULL) {
        u32 saved_gp;
        __asm__ volatile ("move %0, $gp\n\tmove $gp, %1" : "=r"(saved_gp) : "r"(g_umdman_cb_gp_8afc));
        g_umdman_cb_8b04(1);
        __asm__ volatile ("move $gp, %0" : : "r"(saved_gp));
    }
}

/* 0x3a78 */
int sceNp9660_driver_8EF69DC6(void (*cb)(u32, u32, u32, u32, u32, u32))
{
    int intr = sceKernelCpuSuspendIntr();
    u32 gp_val;
    __asm__ volatile ("move %0, $gp" : "=r"(gp_val));
    g_np9660_cb_8b0c = cb;
    g_np9660_cb_gp_8b08 = gp_val;
    sceKernelCpuResumeIntr(intr);
    cb(g_np9660_err_8b10[0], g_np9660_err_8b10[1], g_np9660_err_8b10[2],
       g_np9660_err_8b10[3], g_np9660_err_8b10[4], g_np9660_err_8b10[5]);
    return 0;
}

/* 0x3af0 */
int sceNp9660_driver_B925CA6C(void)
{
    int intr = sceKernelCpuSuspendIntr();
    g_np9660_cb_gp_8b08 = 0;
    g_np9660_cb_8b0c = NULL;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x3b28 */
static NOINLINE void sub_3b28(u32 a0, u32 a1, u32 a2, u32 a3, u32 t0, u32 t1)
{
    int intr = sceKernelCpuSuspendIntr();
    g_np9660_err_8b10[0] = a0;
    g_np9660_err_8b10[1] = a1;
    g_np9660_err_8b10[2] = a2;
    g_np9660_err_8b10[3] = a3;
    g_np9660_err_8b10[4] = t0;
    g_np9660_err_8b10[5] = t1;
    sceKernelCpuResumeIntr(intr);
    if (g_np9660_cb_8b0c != NULL) {
        u32 saved_gp;
        __asm__ volatile ("move %0, $gp\n\tmove $gp, %1" : "=r"(saved_gp) : "r"(g_np9660_cb_gp_8b08));
        g_np9660_cb_8b0c(a0, a1, a2, a3, t0, t1);
        __asm__ volatile ("move $gp, %0" : : "r"(saved_gp));
    }
}

/* 0x3c14 */
static NOINLINE int sub_3c14(int resume_flag)
{
    int err_out = 0;
    if (sub_3d04() == 1) {
        return 0;
    }
    sub_3cd0(0);
    const char *path = (const char *)sceKernelInitDiscImage();
    if (path == NULL) {
        sub_3cd0((int)0x80210001);
        return (int)0x80210001;
    }
    int r = sub_0000(path, resume_flag, &err_out);
    if (r < 0) {
        sub_3cd0(r);
        return r;
    }
    int intr = sceKernelCpuSuspendIntr();
    g_last_mount_err_8b30 = err_out;
    sceKernelCpuResumeIntr(intr);
    sub_3cd0(1);
    return 0;
}

/* 0x3cd0 */
static NOINLINE void sub_3cd0(int val)
{
    int intr = sceKernelCpuSuspendIntr();
    g_mount_status_8b28 = val;
    sceKernelCpuResumeIntr(intr);
}

/* 0x3d04 */
static NOINLINE int sub_3d04(void)
{
    return g_mount_status_8b28;
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
    g_replace_permit_8b2c = val;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x3d70 */
static NOINLINE int sub_3d70(void)
{
    return g_replace_permit_8b2c;
}

/* 0x3d7c */
int NOINLINE sceNp9660_driver_7A05EB3C(int *out)
{
    if (out != NULL) {
        *out = g_last_mount_err_8b30;
    }
    return 0;
}

/* 0x3d94 (merged into 0x3d7c by epsp_prx.py because in .rodata) */
static int umd_io_init(void *arg)
{
    (void)arg;
    u32 opt[2];
    void *blk = NULL;
    memset(&g_umd_ctx_8b48, 0, 0x58);
    SceUID fpl = sceKernelCreateFpl("SceNp9660", 1, 0x100, 0x10200, 1, NULL);
    if (fpl >= 0) {
        g_fpl_8b40 = fpl;
        int r = sceKernelTryAllocateFpl(fpl, &blk);
        if (r >= 0) {
            g_fpl_block_8b44 = blk;
            u8 *p1 = (u8 *)blk + 0x8000;
            r = sub_52d8(p1, 0x8000);
            if (r >= 0) {
                u8 *p2 = p1 + 0x8000;
                r = sub_0dc0(p2, 0x200);
                if (r >= 0) {
                    SceUID mtx1 = sceKernelCreateMutex("SceNp9660NC", 0x201, 0, NULL);
                    if (mtx1 >= 0) {
                        g_nc_mutex_8b38 = mtx1;
                        opt[0] = 8;
                        opt[1] = 0x0f;
                        SceUID mtx2 = sceKernelCreateMutex("SceNp9660", 0x601, 0, opt);
                        if (mtx2 >= 0) {
                            g_main_mutex_8b3c = mtx2;
                            g_cur_mutex_8b34 = mtx2;
                            r = sub_678c();
                            if (r >= 0) {
                                g_part_info_8be4[0] = 0x10;
                                sceKernelQueryMemoryPartitionInfo(5, g_part_info_8be4);
                                g_part_info_8be4[1] &= 0x1fffffffU;
                                return 0;
                            }
                        } else {
                            r = mtx2;
                        }
                    } else {
                        r = mtx1;
                    }
                }
            }
        }
        sub_68c4();
        if (g_cur_mutex_8b34 > 0) {
            sceKernelDeleteMutex(g_cur_mutex_8b34);
            g_cur_mutex_8b34 = 0;
        }
        sub_0df0();
        sub_5308();
        if (g_fpl_8b40 > 0) {
            sceKernelDeleteFpl(g_fpl_8b40);
            g_fpl_8b40 = 0;
        }
        return r;
    }
    sub_68c4();
    if (g_cur_mutex_8b34 > 0) {
        sceKernelDeleteMutex(g_cur_mutex_8b34);
        g_cur_mutex_8b34 = 0;
    }
    sub_0df0();
    sub_5308();
    if (g_fpl_8b40 > 0) {
        sceKernelDeleteFpl(g_fpl_8b40);
        g_fpl_8b40 = 0;
    }
    return fpl;
}

/* 0x3f74 */
static int umd_io_exit(void *arg)
{
    (void)arg;
    sub_68c4();
    if (g_cur_mutex_8b34 > 0) {
        sceKernelDeleteMutex(g_cur_mutex_8b34);
        g_cur_mutex_8b34 = 0;
    }
    if (g_nc_mutex_8b38 > 0) {
        sceKernelDeleteMutex(g_nc_mutex_8b38);
        g_nc_mutex_8b38 = 0;
    }
    if (g_main_mutex_8b3c > 0) {
        sceKernelDeleteMutex(g_main_mutex_8b3c);
        g_main_mutex_8b3c = 0;
    }
    memset(&g_umd_ctx_8b48, 0, 0x58);
    sub_0df0();
    sub_5308();
    if (g_fpl_8b40 > 0) {
        sceKernelDeleteFpl(g_fpl_8b40);
        g_fpl_8b40 = 0;
    }
    return 0;
}

/* 0x4044 */
static int umd_io_open(void *iot, char *file, int flags, int mode)
{
    (void)mode;
    if (iot == NULL || file == NULL) {
        return (int)0x80010016;
    }
    if ((flags & 0x0f02) != 0 || (flags & 1) == 0) {
        return (int)0x8001b004;
    }
    if (*(int *)((u8 *)iot + 4) >= 2) {
        return (int)0x8001b002;
    }
    int intr = sceKernelCpuSuspendIntr();
    int r = (int)0x80010018;
    for (int i = 0; i < 8; i++) {
        if (g_handles_8ba0[i].in_use == 0) {
            g_handles_8ba0[i].in_use = 1;
            g_handles_8ba0[i].flags = (flags & 0x4000) ? 0xf0 : 0;
            *(void **)((u8 *)iot + 0x10) = &g_handles_8ba0[i];
            *(u32 *)((u8 *)iot + 0x18) = 0;
            *(u32 *)((u8 *)iot + 0x1c) = 0;
            r = 0;
            break;
        }
    }
    sceKernelCpuResumeIntr(intr);
    return r;
}

/* 0x4144 */
static int umd_io_close(void *iot)
{
    if (iot == NULL) {
        return (int)0x80010016;
    }
    int *h = *(int **)((u8 *)iot + 0x10);
    if (h == NULL) {
        return (int)0x80010009;
    }
    int r = (int)0x80010009;
    int intr = sceKernelCpuSuspendIntr();
    if (h[0] != 0) {
        h[0] = 0;
        r = 0;
    }
    sceKernelCpuResumeIntr(intr);
    return r;
}

/* 0x41b0 */
static NOINLINE SceOff sub_41b0(void *iot, SceOff ofs, int whence)
{
    if (iot == NULL) {
        return (SceOff)(u32)0x80010016U;
    }
    int *h = *(int **)((u8 *)iot + 0x10);
    if (h == NULL) {
        return (SceOff)(u32)0x80010016U;
    }
    if (h[0] == 0) {
        return (SceOff)(u32)0x80010009U;
    }
    SceOff target;
    if (whence == 1) {
        target = *(SceOff *)((u8 *)iot + 0x18) + ofs;
    } else if (whence < 2) {
        if (whence == 0) {
            target = ofs;
        } else {
            return (SceOff)(int)0x80010016;
        }
    } else if (whence == 2) {
        target = (SceOff)(u64)g_umd_ctx_8b48.last_lba + ofs;
    } else {
        return (SceOff)(int)0x80010016;
    }
    if (target < (SceOff)(u64)g_umd_ctx_8b48.lba_param ||
        target > (SceOff)(u64)g_umd_ctx_8b48.last_lba) {
        return (SceOff)(int)0x80210002;
    }
    *(SceOff *)((u8 *)iot + 0x18) = target;
    sub_73b4((u32)target);
    return target;
}

/* 0x4308 (merged into 0x41b0 by epsp_prx.py because in .rodata) */
static int umd_io_read(void *iot, char *data, int len)
{
    if (iot == NULL) {
        return (int)0x80010016;
    }
    int *h = *(int **)((u8 *)iot + 0x10);
    if (h == NULL) {
        return (int)0x80010009;
    }
    if (h[0] == 0) {
        return (int)0x80010009;
    }
    u32 lba = *(u32 *)((u8 *)iot + 0x18);
    sub_5ffc();
    int r = sub_43ec((int)lba, data, len);
    if (r > 0) {
        *(SceOff *)((u8 *)iot + 0x18) += (SceOff)r;
        if (sub_6028() != 0) {
            sub_5b04(lba, (u32)r, 0, h[1] & 0xf0);
        }
    }
    return r;
}

/* 0x43ec */
static NOINLINE int sub_43ec(int lba, void *buf, int nsectors)
{
    int r = sub_4428((u32)lba << 11, buf, (u32)nsectors << 11);
    return (r < 0) ? r : (r / 2048);
}

/* 0x4428 */
static NOINLINE int sub_4428(u32 byte_pos, void *buf, u32 byte_len)
{
    if (((0x00220202U >> (((u32)buf >> 27) & 0x1fU)) & 1U) == 0) {
        return (int)0x8001b001;
    }
    if (sub_51a4((int)buf) != 0) {
        sceKernelDelayThread(10000);
        return (int)0x80210001;
    }
    int r = sub_5054();
    if (r < 0) {
        return r;
    }
    u32 req[3];
    req[0] = byte_pos;
    req[1] = (u32)buf;
    req[2] = byte_len;
    int r1 = sceKernelExtendKernelStack(0x800, (void *)sub_4500, req);
    int r2 = sub_50c4();
    if (r1 >= 0 && r2 < 0) {
        return r2;
    }
    return r1;
}

/* 0x4500 */
static NOINLINE int sub_4500(void *arg)
{
    u8 entry[48];
    void *cache_ptr;
    u32 *req = (u32 *)arg;
    u32 byte_pos = req[0];
    u8 *cur_dst = (u8 *)req[1];
    u32 byte_len = req[2];
    u32 basis = g_umd_ctx_8b48.block_basis;
    u32 block_bytes = basis << 11;
    u32 in_blk_off = byte_pos % block_bytes;
    u32 start_sec = byte_pos >> 11;
    u32 lba_param = g_umd_ctx_8b48.lba_param;
    u32 last_lba = g_umd_ctx_8b48.last_lba;
    u32 done_bytes = 0;

    if (start_sec < lba_param || start_sec > last_lba) {
        return (int)0x80210002;
    }
    u32 max_bytes = ((last_lba << 11) - byte_pos) + 0x800U;
    if (max_bytes < byte_len) {
        byte_len = max_bytes;
    }
    u32 rel_end_byte = (byte_pos - (lba_param << 11)) + byte_len - 1U;
    u32 end_blk_excl = (rel_end_byte / block_bytes) + 1U;
    u32 blk = (start_sec - lba_param) / basis;
    u32 total_rel_secs = last_lba - lba_param;
    u32 last_disc_blk = total_rel_secs / basis;
    int r = 0;
    int fp_mode = -1;

    if (blk < end_blk_excl) {
        do {
            if (blk == last_disc_blk) {
                basis = (total_rel_secs % basis) + 1U;
            }
            if (in_blk_off == 0 && (((u32)cur_dst & 15U) == 0)) {
                u32 cur_blk_bytes = g_umd_ctx_8b48.block_basis << 11;
                u32 rem_bytes = byte_len - done_bytes;
                if (rem_bytes >= cur_blk_bytes) {
                    r = sub_535c(&cache_ptr, (int)blk);
                    if (r < 0) {
                        goto cleanup;
                    }
                    if (r == 1) {
                        fp_mode = 1;
                    } else {
                        r = sub_0c4c(entry, (int)blk);
                        if (r < 0) {
                            goto cleanup;
                        }
                        if ((*(u32 *)(entry + 0x20) & 8U) != 0) {
                            r = sub_5330(&cache_ptr, (int)blk);
                            if (r < 0) {
                                goto cleanup;
                            }
                            fp_mode = 0;
                        } else {
                            u32 two_blks = g_umd_ctx_8b48.block_basis << 12;
                            fp_mode = (rem_bytes < two_blks) ? 2 : 3;
                            cache_ptr = cur_dst;
                        }
                    }
                    goto process_blk;
                }
            }
            r = sub_5330(&cache_ptr, (int)blk);
            if (r < 0) {
                goto cleanup;
            }
            if (r == 1) {
                fp_mode = 1;
            } else {
                r = sub_0c4c(entry, (int)blk);
                if (r < 0) {
                    goto cleanup;
                }
                fp_mode = 0;
            }

process_blk:
            if (fp_mode == 3) {
                u32 b_bytes = g_umd_ctx_8b48.block_basis << 11;
                u32 span_blks = (byte_len - done_bytes) / b_bytes;
                u32 span_last = blk + span_blks - 1U;
                u32 first_off = *(u32 *)(entry + 0x00);
                r = sub_0c4c(entry, (int)span_last);
                if (r < 0) {
                    goto cleanup;
                }
                u32 raw_span = (*(u32 *)(entry + 0x00) + *(u32 *)(entry + 0x04)) - first_off;
                u32 aligned_span = (raw_span + 15U) & ~15U;
                u32 total_span_bytes = (span_blks * g_umd_ctx_8b48.block_basis) << 11;
                u8 *read_ptr = (u8 *)(((u32)((u8 *)cache_ptr + total_span_bytes - aligned_span)) & ~15U);
                r = sub_50fc(read_ptr, first_off, 0, (int)aligned_span);
                if (r < 0) {
                    goto cleanup;
                }
                r = sub_0ad0(read_ptr, first_off, aligned_span, (int)blk, (int)span_last, entry);
                if (r < 0) {
                    goto cleanup;
                }
                if (raw_span < ((span_blks * g_umd_ctx_8b48.block_basis) << 11)) {
                    u8 *out_p = (u8 *)cache_ptr;
                    for (u32 b = blk; b <= span_last; b++) {
                        r = sub_0c4c(entry, (int)b);
                        if (r < 0) {
                            goto cleanup;
                        }
                        u32 b_bytes_cur = g_umd_ctx_8b48.block_basis << 11;
                        u32 b_comp = *(u32 *)(entry + 0x04);
                        u8 *b_src = read_ptr + (*(u32 *)(entry + 0x00) - first_off);
                        if (b_comp < b_bytes_cur) {
                            void *tmp = sub_4ccc();
                            sceKernelMemcpy(tmp, b_src, b_comp);
                            r = sub_69dc(out_p, tmp, (int)(g_umd_ctx_8b48.block_basis << 11));
                            if (r < 0) {
                                goto cleanup;
                            }
                        } else if (out_p != b_src) {
                            sceKernelMemmove(out_p, b_src, b_bytes_cur);
                        }
                        out_p += (g_umd_ctx_8b48.block_basis << 11);
                    }
                }
                blk = span_last;
                u32 step_bytes = (span_blks * g_umd_ctx_8b48.block_basis) << 11;
                goto advance_blk;
            } else if (fp_mode == 1) {
                sceKernelDelayThread(100);
                block_bytes = basis << 11;
            } else {
                u32 comp_sz = *(u32 *)(entry + 0x04);
                block_bytes = basis << 11;
                u32 aligned_comp = (comp_sz + 15U) & ~15U;
                void *io_buf = (comp_sz < block_bytes) ? sub_4ccc() : cache_ptr;
                r = sub_50fc(io_buf, *(u32 *)(entry + 0x00), 0, (int)aligned_comp);
                if (r < 0) {
                    goto cleanup;
                }
                r = sub_0a34(io_buf, entry);
                if (r < 0) {
                    goto cleanup;
                }
                if (*(u32 *)(entry + 0x04) < block_bytes) {
                    r = sub_69dc(cache_ptr, io_buf, (int)block_bytes);
                    if (r < 0) {
                        goto cleanup;
                    }
                }
            }
            {
                u32 step_bytes = block_bytes - in_blk_off;
                u32 rem_req = byte_len - done_bytes;
                if (rem_req < step_bytes) {
                    step_bytes = rem_req;
                }
            advance_blk:
                if ((u32)(fp_mode - 2) >= 2U) {
                    sceKernelMemcpy(cur_dst, (u8 *)cache_ptr + in_blk_off, step_bytes);
                    r = sub_538c((int)blk);
                    if (r < 0) {
                        fp_mode = -1;
                        goto cleanup;
                    }
                }
                fp_mode = -1;
                cur_dst += step_bytes;
                done_bytes += step_bytes;
                blk++;
                in_blk_off = 0;
            }
        } while (blk < end_blk_excl);
    }

cleanup:
    if (fp_mode != -1 && fp_mode != 2 && fp_mode != 3) {
        sub_53a8((int)blk);
    }
    if (r < 0) {
        sceKernelDelayThread(10000);
        return (int)0x80210001;
    }
    return (int)done_bytes;
}

/* 0x4acc (merged into 0x4500 by epsp_prx.py because in .rodata) */
static int umd_io_ioctl(void *iot, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
    if (iot == NULL) {
        return (int)0x80010016;
    }
    int *h = *(int **)((u8 *)iot + 0x10);
    if (h == NULL) {
        return (int)0x80010016;
    }
    if (h[0] == 0) {
        return (int)0x80010009;
    }
    if ((int)cmd > (int)0x00208082U) {
        if (cmd == 0x01f010dbU) {
            return 0;
        }
        if ((int)cmd > (int)0x01f010dbU) {
            if (cmd == 0x01f100a6U) {
                if ((u32)inlen < 4U || indata == NULL) {
                    return (int)0x80010016;
                }
                return (int)sub_41b0(iot, *(SceOff *)indata, *((int *)indata + 3));
            }
            if (cmd != 0x01f30003U) {
                return (int)0x80010086;
            }
            if ((u32)inlen < 4U || indata == NULL) {
                return (int)0x80010016;
            }
            int nsec = *(int *)indata;
            if (outdata == NULL || outlen < nsec) {
                return (int)0x80010016;
            }
            u32 cur_lba = *(u32 *)((u8 *)iot + 0x18);
            u32 max_sec = (g_umd_ctx_8b48.last_lba - cur_lba) + 1U;
            if ((int)max_sec < nsec) {
                nsec = (int)max_sec;
            }
            int r = sub_43ec((int)cur_lba, outdata, nsec);
            if (r > 0) {
                *(SceOff *)((u8 *)iot + 0x18) += (SceOff)r;
            }
            return r;
        }
        if (cmd != 0x01d20001U) {
            return (int)0x80010086;
        }
        if ((u32)outlen < 4U || outdata == NULL) {
            return (int)0x80010016;
        }
        *(u32 *)outdata = *(u32 *)((u8 *)iot + 0x18);
        return 0;
    }
    if ((int)cmd >= (int)0x00208081U) {
        return 0;
    }
    if (cmd == 0x0020800cU) {
        return 0;
    }
    if ((int)cmd > (int)0x0020800cU) {
        if (cmd - 0x00208010U < 2U) {
            return 0;
        }
        return (int)0x80010086;
    }
    if (cmd == 0x00208001U || cmd == 0x00208006U) {
        return 0;
    }
    return (int)0x80010086;
}

/* 0x4ccc */
static NOINLINE void *sub_4ccc(void)
{
    return g_fpl_block_8b44;
}

/* 0x4cd8 */
static NOINLINE int sub_4cd8(const u32 *ctx_in)
{
    u32 count_out;
    int async_mode = 1;
    if (ctx_in == NULL || (int)ctx_in[0] <= 0) {
        return (int)0x80010016;
    }
    u8 flg = *((const u8 *)ctx_in + 0x42);
    if ((flg & 1U) != 0) {
        if (g_cur_mutex_8b34 == g_main_mutex_8b3c) {
            sceKernelLockMutex(g_nc_mutex_8b38, 1, NULL);
            int intr = sceKernelCpuSuspendIntr();
            sceKernelCancelMutex(g_cur_mutex_8b34, 1, (int *)&count_out);
            g_cur_mutex_8b34 = g_nc_mutex_8b38;
            sceKernelCpuResumeIntr(intr);
            sceKernelUnlockMutex(g_main_mutex_8b3c, 1);
            flg = *((const u8 *)ctx_in + 0x42);
        }
    }
    if ((flg & 2U) != 0) {
        int r = sceIoDevctl("ms0:", 0x02425858, &async_mode, 4, NULL, 0);
        if (r != 0) {
            return r;
        }
    }
    int intr = sceKernelCpuSuspendIntr();
    u32 *dst = (u32 *)&g_umd_ctx_8b48;
    const u32 *src = ctx_in;
    for (int i = 0; i < 20; i += 4) {
        dst[i + 0] = src[i + 0];
        dst[i + 1] = src[i + 1];
        dst[i + 2] = src[i + 2];
        dst[i + 3] = src[i + 3];
    }
    dst[20] = src[20];
    dst[21] = src[21];
    sub_6988((int)g_umd_ctx_8b48.thread_prio);
    void *gi = sceKernelGetGameInfo();
    if (gi != NULL) {
        const char *id = (const char *)gi + 0x44;
        if (strcmp(id, "ULUS10021") == 0 || strcmp(id, "ULES00108") == 0) {
            g_umd_ctx_8b48.emu_rate2 = 0x44c;
        }
        if (strcmp(id, "ULUS10059") == 0) {
            g_umd_ctx_8b48.emu_flags = 8;
            g_umd_ctx_8b48.emu_rate1 = 0x64;
            g_umd_ctx_8b48.emu_rate2 = 0x1f4;
        }
    }
    sub_6098(g_umd_ctx_8b48.emu_rate1);
    sub_60a4(g_umd_ctx_8b48.emu_rate2);
    sub_60b0(g_umd_ctx_8b48.emu_flags);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x4ef4 */
static NOINLINE int sub_4ef4(void *out)
{
    int intr = sceKernelCpuSuspendIntr();
    if (g_umd_ctx_8b48.fd > 0) {
        u32 *dst = (u32 *)out;
        const u32 *src = (const u32 *)g_umd_ctx_8b48.disc_id;
        for (int i = 0; i < 8; i += 4) {
            dst[i + 0] = src[i + 0];
            dst[i + 1] = src[i + 1];
            dst[i + 2] = src[i + 2];
            dst[i + 3] = src[i + 3];
        }
        dst[8] = g_umd_ctx_8b48.unk_30;
        dst[9] = g_umd_ctx_8b48.last_lba;
        sceKernelCpuResumeIntr(intr);
        return 0;
    }
    int r = sub_3d04();
    if (r >= 0) {
        r = (int)0x80210001;
    }
    sceKernelCpuResumeIntr(intr);
    return r;
}

/* 0x4fa4 */
static NOINLINE int sub_4fa4(int *out0, int *out1, int *out2)
{
    int intr = sceKernelCpuSuspendIntr();
    if (g_umd_ctx_8b48.fd > 0) {
        if (out0 != NULL) {
            *out0 = (int)g_umd_ctx_8b48.lba_param;
        }
        if (out1 != NULL) {
            *out1 = (int)g_umd_ctx_8b48.unk_2c;
        }
        if (out2 != NULL) {
            *out2 = (int)g_umd_ctx_8b48.last_lba;
        }
        sceKernelCpuResumeIntr(intr);
        return 0;
    }
    int r = sub_3d04();
    if (r >= 0) {
        r = (int)0x80210001;
    }
    sceKernelCpuResumeIntr(intr);
    return r;
}

/* 0x5054 */
static NOINLINE int sub_5054(void)
{
    if (g_cur_mutex_8b34 == 0) {
        return (int)0x80000041;
    }
    g_mutex_owner_pri_8be0 = sceKernelGetThreadCurrentPriority();
    int r;
    do {
        r = sceKernelLockMutex(g_cur_mutex_8b34, 1, NULL);
    } while (r == (int)0x800201a9);
    return r;
}

/* 0x50c4 */
static NOINLINE int sub_50c4(void)
{
    if (g_cur_mutex_8b34 == 0) {
        return (int)0x80000041;
    }
    return sceKernelUnlockMutex(g_cur_mutex_8b34, 1);
}

/* 0x50fc */
static NOINLINE int sub_50fc(void *buf, u32 pos_lo, u32 pos_hi, int size)
{
    SceOff off = ((SceOff)pos_hi << 32) | pos_lo;
    SceOff r = sceIoLseek(g_umd_ctx_8b48.fd, off, 0);
    if (r != off) {
        return (r < 0) ? (int)r : (int)0x80210001;
    }
    int rd = sceIoRead(g_umd_ctx_8b48.fd, buf, (SceSize)size);
    if (rd == size) {
        return 0;
    }
    return (rd < 0) ? rd : (int)0x80210001;
}

/* 0x51a4 */
static NOINLINE int sub_51a4(int addr)
{
    if (sub_303c() != 0x40) {
        return 0;
    }
    if (sceKernelIsUserModeThread() == 0) {
        return 0;
    }
    u32 uaddr = (u32)addr & 0x1fffffffU;
    if (sceKernelGetCompiledSdkVersion() <= 0x03000000) {
        return 0;
    }
    return ((uaddr - g_part_info_8be4[1]) < g_part_info_8be4[2]) ? 1 : 0;
}

/* 0x5218 */
static NOINLINE int sub_5218(void)
{
    sub_3cd0((int)0x80210003);
    int r = 0;
    if (g_umd_ctx_8b48.fd > 0) {
        r = sceIoClose(g_umd_ctx_8b48.fd);
    }
    g_umd_ctx_8b48.fd = 0;
    return r;
}

/* 0x5268 */
static NOINLINE int sub_5268(void)
{
    u8 *pvd = NULL;
    sub_60bc((void **)&pvd);
    return memcmp(pvd + 0x373, g_dummy_disc_id_8940, 0x20) == 0;
}

/* 0x52a0 */
static NOINLINE int sub_52a0(void)
{
    return g_umd_ctx_8b48.flags_42;
}

/* 0x52ac */
static NOINLINE int sub_52ac(int *info_out)
{
    if (info_out == NULL) {
        return (int)0x80010016;
    }
    info_out[1] = 0x10;
    return 0;
}

/* 0x52cc */
static NOINLINE int sub_52cc(void)
{
    return g_mutex_owner_pri_8be0;
}

/* 0x52d8 */
static NOINLINE int sub_52d8(void *buf, int size)
{
    if (size != 0x8000) {
        return (int)0x80010016;
    }
    g_cache_buf_8bf4 = buf;
    g_cached_blk_8bf8 = -1;
    return 0;
}

/* 0x5308 */
static NOINLINE int sub_5308(void)
{
    g_cached_blk_8bf8 = -1;
    return 0;
}

/* 0x531c */
static NOINLINE int sub_531c(void)
{
    g_cached_blk_8bf8 = -1;
    return 0;
}

/* 0x5330 */
static NOINLINE int sub_5330(void **out, int blk)
{
    *out = g_cache_buf_8bf4;
    if (g_cached_blk_8bf8 == blk) {
        return 1;
    }
    g_cached_blk_8bf8 = blk;
    return 0;
}

/* 0x535c */
static NOINLINE int sub_535c(void **out, int blk)
{
    if (g_cached_blk_8bf8 == blk) {
        *out = g_cache_buf_8bf4;
        return 1;
    }
    *out = NULL;
    return 0;
}

/* 0x538c */
static NOINLINE int sub_538c(int blk)
{
    return (g_cached_blk_8bf8 == blk) ? 0 : (int)0x80010006;
}

/* 0x53a8 */
static NOINLINE int sub_53a8(int blk)
{
    if (g_cached_blk_8bf8 == blk) {
        g_cached_blk_8bf8 = -1;
        return 0;
    }
    return (int)0x80010006;
}

/* 0x53d0 (merged into 0x53a8 by epsp_prx.py because in .rodata) */
static int umd_io_devctl(void *iot, const char *dev, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
    (void)iot;
    (void)dev;
    if (cmd == 0x01e38012U) {
        if (outdata == NULL || (u32)outlen < 0x28U) {
            return (int)0x80010016;
        }
        return sub_4ef4(outdata);
    }
    if ((int)cmd > (int)0x01e38012U) {
        if (cmd == 0x01f100a9U) {
            if (indata == NULL || (u32)inlen < 4U) {
                return (int)0x80010016;
            }
            return 0;
        }
        if ((int)cmd > (int)0x01f100a9U) {
            if ((int)cmd > (int)0x01f200a2U) {
                if (cmd == 0x01f21011U) {
                    return 0;
                }
                if ((int)cmd > (int)0x01f21011U) {
                    if (cmd == 0x01f300a5U) {
                        if (indata == NULL || (u32)inlen < 0x10U || outdata == NULL || (u32)outlen < 4U) {
                            return (int)0x80010016;
                        }
                        return 0;
                    }
                    if (cmd == 0x01f300a7U) {
                        if (indata == NULL || (u32)inlen < 4U) {
                            return (int)0x80010016;
                        }
                        return 0;
                    }
                    return (int)0x80010086;
                }
                if (cmd - 0x01f200a4U < 2U) {
                    return 0;
                }
                return (int)0x80010086;
            }
            if ((int)cmd >= (int)0x01f200a0U) {
                return 0;
            }
            if (cmd == 0x01f20002U) {
                if (outdata == NULL || (u32)outlen < 4U) {
                    return (int)0x80010016;
                }
                return sub_4fa4(NULL, NULL, (int *)outdata);
            }
            if ((int)cmd > (int)0x01f20002U) {
                if (cmd == 0x01f20003U) {
                    if (outdata == NULL || (u32)outlen < 4U) {
                        return (int)0x80010016;
                    }
                    return sub_4fa4(NULL, (int *)outdata, NULL);
                }
                if (cmd == 0x01f20014U) {
                    return 0;
                }
                return (int)0x80010086;
            }
            if (cmd != 0x01f20001U) {
                return (int)0x80010086;
            }
            if (outdata == NULL || outlen != 8) {
                return (int)0x80010016;
            }
            return sub_52ac((int *)outdata);
        }
        if (cmd == 0x01f00003U) {
            sub_5af8(*(int *)indata);
            return 0;
        }
        if ((int)cmd > (int)0x01f00003U) {
            if (cmd == 0x01f100a4U) {
                if (indata == NULL || (u32)inlen < 0x10U) {
                    return (int)0x80010016;
                }
                return 0;
            }
            if ((int)cmd > (int)0x01f100a4U) {
                if (cmd == 0x01f100a6U || cmd == 0x01f100a8U) {
                    if (indata == NULL || (u32)inlen < 4U) {
                        return (int)0x80010016;
                    }
                    return 0;
                }
                return (int)0x80010086;
            }
            if (cmd == 0x01f000a6U) {
                return 0;
            }
            if (cmd == 0x01f100a3U) {
                sub_5af8(*(int *)indata);
                return 0;
            }
            return (int)0x80010086;
        }
        if (cmd == 0x01e38034U) {
            if (indata == NULL || outdata == NULL || (u32)outlen < 4U) {
                return (int)0x80010016;
            }
            return sub_58c4((const char *)indata, (int *)outdata);
        }
        if ((int)cmd > (int)0x01e38034U) {
            if (cmd != 0x01e380c0U) {
                return (int)0x80010086;
            }
            if (indata == NULL || inlen != 0x24 || outdata == NULL) {
                return (int)0x80010016;
            }
            return sub_5968(outdata, (u32)outlen, (const u32 *)indata);
        }
        if (cmd - 0x01e38031U < 2U) {
            return 0;
        }
        return (int)0x80010086;
    }
    if (cmd == 0x01e18033U) {
        return 0;
    }
    if ((int)cmd > (int)0x01e18033U) {
        if (cmd == 0x01e28035U) {
            if (outdata == NULL || (u32)outlen < 4U) {
                return (int)0x80010016;
            }
            int r = sub_43ec(0x10, g_pvd_buf_8c38, 1);
            if (r == 1) {
                *(void **)outdata = g_pvd_buf_8c38;
                return 0;
            }
            if (r < 0) {
                return r;
            }
            return (int)0x80010005;
        }
        if ((int)cmd > (int)0x01e28035U) {
            if ((int)cmd > (int)0x01e280d2U) {
                if (cmd - 0x01e280d9U < 2U) {
                    return 0;
                }
                return (int)0x80010086;
            }
            if ((int)cmd >= (int)0x01e280d1U) {
                return 0;
            }
            if (cmd != 0x01e280a9U) {
                return (int)0x80010086;
            }
            if (outdata == NULL || (u32)outlen < 4U) {
                return (int)0x80010016;
            }
            *(int *)outdata = 0x800;
            return 0;
        }
        if (cmd == 0x01e180d3U || cmd == 0x01e180c1U || cmd == 0x01e180d0U || cmd == 0x01e180d6U || cmd == 0x01e28013U) {
            return 0;
        }
        return (int)0x80010086;
    }
    if (cmd == 0x01e080d4U || cmd == 0x01e000d5U || cmd == 0x01e080a8U || cmd == 0x01e18021U || cmd == 0x01e18024U || cmd == 0x01e18030U) {
        return 0;
    }
    if (cmd - 0x00208810U < 2U || cmd - 0x01e080d7U < 2U) {
        return 0;
    }
    return (int)0x80010086;
}

/* 0x58c4 */
static NOINLINE int sub_58c4(const char *name, int *unit_out)
{
    const char *p = name;
    while ((look_ctype_table((u8)*p) & 8) != 0) {
        p++;
    }
    if (strncmp(p, "umd", 3) != 0) {
        return (int)0x80010013;
    }
    int c3 = p[3] - '0';
    if (p[4] != ':' || c3 < 0 || c3 >= 10) {
        return (int)0x80010013;
    }
    *unit_out = c3;
    return 0;
}

/* 0x5968 */
static NOINLINE int sub_5968(void *outdata, u32 outlen, const u32 *in)
{
    if (outlen < in[4]) {
        return (int)0x80010069;
    }
    u32 byte_pos = in[2] << 11;
    u32 off_18 = in[6];
    if (off_18 != 0) {
        if (in[5] != 0 || in[7] != 0) {
            byte_pos = (byte_pos - off_18) + 0x800U;
        } else {
            byte_pos += off_18;
        }
    }
    sub_5ffc();
    int r = sub_4428(byte_pos, outdata, in[4]);
    if (r < 0) {
        return r;
    }
    if (sub_6028() == 0) {
        return (int)in[3];
    }
    u8 flags = g_emu_flags_8c34;
    if ((flags & 4U) != 0) {
        if (sceKernelSearchModuleByName("sceNetAdhoc_Library") < 0) {
            return (int)in[3];
        }
        flags = g_emu_flags_8c34;
    }
    int seek_only = (int)((in[0] << 3) & 8U);
    if ((flags & 2U) == 0) {
        sub_5b04(in[2], in[3], seek_only, (int)(in[1] & 1U));
        return (int)in[3];
    }
    int step = 0;
    int mode = (int)(in[1] & 3U);
    if (sub_5d18(in, &step) == 0) {
        u32 rem_lba = 0;
        u32 rem_blks = 0;
        if (sub_7104(in[2], in[3], seek_only, mode, &rem_lba, &rem_blks) == 0) {
            u32 lba = rem_lba;
            u32 cnt = rem_blks << 4;
            if (rem_lba == 0 || rem_blks == 0) {
                lba = in[2];
                cnt = in[3];
            }
            sub_6c9c(lba, cnt, seek_only, mode);
        }
        sub_5fac(step);
    }
    return (int)in[3];
}

/* 0x5af8 */
static NOINLINE void sub_5af8(u32 lba)
{
    g_emu_8c04 = lba;
}

/* 0x5b04 */
static NOINLINE int sub_5b04(u32 lba, u32 cnt, int seek_only, int table_sel)
{
    u32 rem_lba = 0;
    u32 rem_blks = 0;
    int cross_layer = 0;
    int hit = 0;

    if (((g_emu_flags_8c34 ^ 1U) == 0) && seek_only == 0) {
        hit = (sub_6bd4(lba, cnt, 0, table_sel, &rem_lba, &rem_blks) != 0);
    }
    u32 seek_dist = 0;
    if (seek_only != 0 || hit == 0) {
        seek_dist = sub_66f8(lba, g_emu_8c04, &cross_layer);
    }
    g_emu_time_8c08 = sceKernelGetSystemTimeWide();
    if (g_emu_time_8c10 == 0 || (u64)(g_emu_time_8c08 - g_emu_time_8c10) > 0x4c4b40ULL) {
        g_emu_8c20 = 0;
    }
    if (lba == g_emu_8c04) {
        if (hit != 0) {
            sub_63b4(lba, cnt, seek_dist, 1, cross_layer);
        } else {
            sub_60cc(lba, cnt, seek_dist, 1, cross_layer);
        }
    } else {
        if (hit != 0) {
            sub_63b4(lba, cnt, seek_dist, 1, cross_layer);
        } else {
            sub_60cc(lba, cnt, seek_dist, 2, cross_layer);
        }
    }
    g_emu_time_8c10 = sceKernelGetSystemTimeWide();
    g_emu_8c04 = lba + cnt;
    g_emu_8bfc = lba + cnt;
    return sub_6c9c(lba, cnt, seek_only, table_sel);
}

/* 0x5d18 */
static NOINLINE int sub_5d18(const u32 *in, int *step_out)
{
    if (in[6] != 0) {
        *step_out |= 1;
    }
    if (in[5] != 0) {
        *step_out |= 2;
    }
    if (in[7] != 0) {
        *step_out |= 4;
    }
    u32 lba = in[2];
    u16 cnt = (u16)in[3];
    int substep = 0;

    if (lba == g_emu_8c30 && (*step_out == 1 || *step_out == 4) && cnt == 1) {
        if (g_emu_8c24 == 2) {
            substep = 1;
            if (lba == g_emu_8c30 + 1U) {
                goto check_5ef4;
            }
            goto check_5dac;
        }
        g_emu_8c28 = lba;
        goto check_5e40;
    }
    if (lba == g_emu_8c30 + 1U) {
check_5ef4:
        if ((*step_out == 1 || *step_out == 4) && cnt == 1 && g_emu_8c24 == 5) {
            substep = 2;
            g_emu_8c28 = lba;
            goto check_5e40;
        }
    }
check_5dac:
    if (lba == g_emu_8c30 && *step_out == 5 && cnt == 2 && g_emu_8c24 == 2) {
        g_emu_8c28 = lba + 1U;
        substep = 3;
        goto check_5e40;
    }
    g_emu_8c28 = lba + cnt - 1U;
    g_emu_8c30 = lba;
    g_emu_8c2c = 0x20;
    return 0;

check_5e40:
    if (g_emu_8c2c != 0x10) {
        g_emu_8c2c = 0x20;
        return 0;
    }
    int mode = *step_out;
    if (mode == 1) {
        if (g_emu_8c24 == 1 || g_emu_8c24 == 4) {
            return 1;
        }
        if (g_emu_8c24 == 5 && (substep == 1 || substep == 2)) {
            return 1;
        }
    } else if (mode == 4) {
        if (g_emu_8c24 == 1 || g_emu_8c24 == 4) {
            return 1;
        }
        if (g_emu_8c24 == 5) {
            if (substep == 1) {
                return 1;
            }
            if (substep == 3) {
                return 1;
            }
        } else {
            return 0;
        }
    } else if (mode == 5) {
        if (substep == 3) {
            return 1;
        }
    }
    g_emu_8c2c = 0x20;
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
    g_emu_8c24 = idx;
    return g_emu_fn_tab_8960[idx]();
}

/* 0x5fd8 (in .rodata table 0x8960) */
static int sub_5fd8(void)
{
    g_emu_8c2c = 0x10;
    return 0;
}

/* 0x5fe8 (in .rodata table 0x8960) */
static int sub_5fe8(void)
{
    g_emu_8c2c = 0x10;
    return 0;
}

/* 0x5ff4 (in .rodata table 0x8960) */
static int sub_5ff4(void)
{
    return 0;
}

/* 0x5ffc */
static NOINLINE void sub_5ffc(void)
{
    g_emu_time_9c18 = sceKernelGetSystemTimeWide();
}

/* 0x6028 */
static NOINLINE int sub_6028(void)
{
    if (g_emu_rate1_9c20 != 0 || g_emu_rate2_9c22 != 0) {
        if (sceWlanDrv_driver_7DD3787C() != 1) {
            return 1;
        }
    }
    if ((g_emu_flags_8c34 & 0xcU) != 0) {
        return 1;
    }
    return 0;
}

/* 0x6098 */
static NOINLINE void sub_6098(u16 val)
{
    g_emu_rate1_9c20 = val;
}

/* 0x60a4 */
static NOINLINE void sub_60a4(u16 val)
{
    g_emu_rate2_9c22 = val;
}

/* 0x60b0 */
static NOINLINE void sub_60b0(u8 val)
{
    g_emu_flags_8c34 = val;
}

/* 0x60bc */
static NOINLINE void sub_60bc(void **out)
{
    *out = g_pvd_buf_8c38;
}

/* 0x60cc */
static NOINLINE void sub_60cc(u32 lba, u32 cnt, u32 seek_dist, int mode, int cross_layer)
{
    u64 elapsed;
    if ((u64)g_emu_time_8c08 > (u64)g_emu_time_9c18) {
        elapsed = (u64)g_emu_time_8c08 - (u64)g_emu_time_9c18;
    } else {
        elapsed = ((u64)0 - (u64)g_emu_time_8c08) | (~(u64)g_emu_time_9c18);
    }
    sceUmd9660_driver_887C3193();
    u32 low = sceKernelGetSystemTimeLow();
    u32 s0 = low + 0x23dU;
    u32 half = (u32)g_data_8a08;
    u32 mult;
    if (lba <= half) {
        if (lba < s0) {
            mult = 0x6edU;
        } else {
            u32 q = (u32)(((u64)lba * 0xe4bf37d9ULL) >> 41);
            mult = 0x6edU - q;
            if (mult < 0x3e8U) {
                mult = 0x3e8U;
            }
        }
    } else {
        u32 v1 = lba - half;
        if (v1 < s0) {
            mult = 0x3e8U;
        } else {
            u32 q = (u32)(((u64)v1 * 0xe4bf37d9ULL) >> 41);
            mult = q + 0x3e8U;
            if (mult >= 0x6eeU) {
                mult = 0x6edU;
            }
        }
    }

    u32 factor;
    if (cnt < 0x10U) {
        factor = (low & 0x40U) + 0x11bU;
    } else if (cnt < 0x200U) {
        factor = 0x236U;
    } else {
        factor = (low & 0x80U) + 0x351U;
    }

    u32 a1 = 0;
    if (factor != 0 || cnt != 0) {
        u32 prod = (factor * cnt + 0x35b60U) * mult;
        a1 = (u32)(((u64)prod * 0x10624dd3ULL) >> 38);
    }
    u32 xfer = (u32)(((u64)(a1 * (u32)g_emu_rate1_9c20) * 0x10624dd3ULL) >> 38);
    u32 total = xfer;
    if (mode != 1) {
        u32 s_us = seek_dist * 6U;
        if (s_us > 0x28847aU) {
            s_us = 0x28847aU;
        }
        if (cross_layer != 0) {
            s_us += 0xc350U;
        }
        u32 seek_scaled = (u32)(((u64)(s_us * (u32)g_emu_rate2_9c22) * 0x10624dd3ULL) >> 38);
        total = xfer + seek_scaled;
    }

    if (elapsed >= (u64)total) {
        g_emu_8c20 += (u32)elapsed - total;
    } else {
        u32 wait_us = total - (u32)elapsed;
        if (wait_us != 0) {
            sceKernelDelayThread(wait_us);
        }
    }
}

/* 0x63b4 */
static NOINLINE void sub_63b4(u32 lba, u32 cnt, u32 seek_dist, int mode, int cross_layer)
{
    u64 elapsed = (u64)g_emu_time_8c08 - (u64)g_emu_time_9c18;
    sceUmd9660_driver_887C3193();
    u32 low = sceKernelGetSystemTimeLow();
    u32 s0 = low + 0x23dU;
    u32 half = (u32)g_data_8a08;
    u32 mult;
    if (lba <= half) {
        if (lba < s0) {
            mult = 0x6edU;
        } else {
            u32 q = (u32)(((u64)lba * 0xe4bf37d9ULL) >> 41);
            mult = 0x6edU - q;
            if (mult < 0x3e8U) {
                mult = 0x3e8U;
            }
        }
    } else {
        u32 v1 = lba - half;
        if (v1 < s0) {
            mult = 0x3e8U;
        } else {
            u32 q = (u32)(((u64)v1 * 0xe4bf37d9ULL) >> 41);
            mult = q + 0x3e8U;
            if (mult >= 0x6eeU) {
                mult = 0x6edU;
            }
        }
    }

    u32 factor;
    if (cnt >= 0x800U) {
        u32 qtr = half >> 1;
        if (lba < qtr) {
            factor = 0x362U;
        } else if (lba <= qtr + half) {
            factor = 0x16eU;
        } else {
            factor = 0x362U;
        }
    } else if (cnt >= 0x400U) {
        u32 qtr = half >> 1;
        if (lba < qtr) {
            factor = (low & 0x40U) + 0x351U;
        } else if (lba <= qtr + half) {
            factor = (low & 0x40U) + 0x11bU;
        } else {
            factor = (low & 0x40U) + 0x351U;
        }
    } else if (cnt >= 0x200U) {
        factor = 0U;
    } else if (cnt >= 0x100U) {
        u32 qtr = half >> 1;
        if (lba < qtr) {
            factor = (low & 0x40U) + 0xfaU;
        } else if (lba <= qtr + half) {
            factor = 0x64U;
        } else {
            factor = (low & 0x40U) + 0xfaU;
        }
    } else if (cnt >= 0x80U) {
        u32 qtr = half >> 1;
        if (lba < qtr) {
            factor = (low & 0x40U) + 0x244U;
        } else if (lba <= qtr + half) {
            factor = 0x53U;
        } else {
            factor = (low & 0x40U) + 0x244U;
        }
    } else if (cnt >= 0x40U) {
        factor = 0x15cU;
    } else {
        factor = 0U;
        mult = 0x3e8U;
    }

    u32 t4 = (u32)(((u64)((factor * cnt + 0x157cU) * mult) * 0x10624dd3ULL) >> 38);
    u32 xfer = (u32)(((u64)(t4 * (u32)g_emu_rate1_9c20) * 0x10624dd3ULL) >> 38);
    u32 total = xfer;
    if (mode != 1) {
        u32 s_us = seek_dist * 6U;
        if (s_us > 0x28847aU) {
            s_us = 0x28847aU;
        }
        if (cross_layer != 0) {
            s_us += 0xc350U;
        }
        u32 xfer2 = (u32)(((u64)(xfer * mult) * 0x10624dd3ULL) >> 38);
        u32 seek_scaled = (u32)(((u64)(s_us * (u32)g_emu_rate2_9c22) * 0x10624dd3ULL) >> 38);
        total = xfer2 + seek_scaled;
    }

    if (elapsed >= (u64)total) {
        g_emu_8c20 += (u32)elapsed - total;
    } else {
        u32 wait_us = total - (u32)elapsed;
        if (wait_us != 0) {
            sceKernelDelayThread(wait_us);
        }
    }
}

/* 0x66f8 */
static NOINLINE u32 sub_66f8(u32 lba, u32 prev_lba, int *cross_layer)
{
    u32 cur = g_emu_8c04;
    u32 half = (u32)g_data_8a08;
    if (cur < lba) {
        u32 diff = lba - prev_lba;
        if (lba > half && prev_lba < half) {
            *cross_layer = 1;
            u32 d1 = lba - half;
            u32 d2 = half - g_emu_8c04;
            diff = (d2 < d1) ? (d1 - d2) : (d2 - d1);
        }
        return diff;
    } else {
        u32 diff = prev_lba - lba;
        if (prev_lba > half && lba < half) {
            *cross_layer = 1;
            u32 d1 = prev_lba - half;
            u32 d2 = half - lba;
            diff = (d2 < d1) ? (d1 - d2) : (d2 - d1);
        }
        return diff;
    }
}

/* 0x678c */
static NOINLINE int sub_678c(void)
{
    int r = sceKernelCreateMbx("SceNp9660Dec", 1, NULL);
    if (r >= 0) {
        g_dec_mbx_943c = r;
        r = sceKernelCreateEventFlag("SceNp9660Dec", 1, 0, NULL);
        if (r >= 0) {
            g_dec_evf_9440 = r;
            r = sceKernelCreateThread("SceNp9660Dec", sub_6a98, 0x70, 0x1000, 0x00100001, NULL);
            if (r >= 0) {
                g_dec_thid_9438 = r;
                r = sceKernelStartThread(r, 0, NULL);
                if (r >= 0) {
                    return 0;
                }
            }
        }
    }
    if (g_dec_thid_9438 > 0) {
        sceKernelDeleteThread(g_dec_thid_9438);
        g_dec_thid_9438 = 0;
    }
    if (g_dec_evf_9440 > 0) {
        sceKernelDeleteEventFlag(g_dec_evf_9440);
        g_dec_evf_9440 = 0;
    }
    if (g_dec_mbx_943c > 0) {
        sceKernelDeleteMbx(g_dec_mbx_943c);
        g_dec_mbx_943c = 0;
    }
    return r;
}

/* 0x68c4 */
static NOINLINE int sub_68c4(void)
{
    u32 msg[6];
    if (g_dec_thid_9438 > 0 && g_dec_mbx_943c > 0) {
        msg[0] = 0;
        msg[1] = 0;
        msg[2] = 0;
        msg[3] = 0;
        msg[4] = 0;
        msg[5] = 0;
        sceKernelSendMbx(g_dec_mbx_943c, msg);
        sceKernelWaitThreadEnd(g_dec_thid_9438, NULL);
    }
    if (g_dec_thid_9438 > 0) {
        sceKernelDeleteThread(g_dec_thid_9438);
        g_dec_thid_9438 = 0;
    }
    if (g_dec_evf_9440 > 0) {
        sceKernelDeleteEventFlag(g_dec_evf_9440);
        g_dec_evf_9440 = 0;
    }
    if (g_dec_mbx_943c > 0) {
        sceKernelDeleteMbx(g_dec_mbx_943c);
        g_dec_mbx_943c = 0;
    }
    return 0;
}

/* 0x6988 */
static NOINLINE int sub_6988(int prio)
{
    int r = 0;
    if (prio >= 0) {
        if (prio == 0) {
            prio = 0x70;
        }
        r = sceKernelChangeThreadPriority(g_dec_thid_9438, prio);
        if (r < 0) {
            return r;
        }
    }
    g_data_8a10 = prio;
    return r;
}

/* 0x69dc */
static NOINLINE int sub_69dc(void *dst, u32 dst_len, const void *src)
{
    u32 msg[6];
    if (g_data_8a10 < 0) {
        int r = sceKernelChangeThreadPriority(g_dec_thid_9438, sub_52cc());
        if (r < 0) {
            return r;
        }
    }
    msg[2] = (u32)dst;
    msg[3] = dst_len;
    msg[4] = (u32)src;
    int r = sceKernelSendMbx(g_dec_mbx_943c, msg);
    if (r < 0) {
        return r;
    }
    r = sceKernelWaitEventFlag(g_dec_evf_9440, 1, 0x21, NULL, NULL);
    return (r < 0) ? r : (int)msg[5];
}

/* 0x6a98 */
static NOINLINE int sub_6a98(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    u32 *msg = NULL;
    int r;
    while ((r = sceKernelReceiveMbx(g_dec_mbx_943c, (void **)&msg, NULL)) >= 0) {
        u32 dst = msg[2];
        if (dst == 0) {
            break;
        }
        u32 cached_addr = dst & ~(3U << 29);
        if (((0xd3 >> (dst >> 29)) & 1) != 0) {
            r = sub_1228((void *)dst, msg[4], (const void *)msg[3], NULL);
            msg[5] = (u32)r;
        } else {
            r = sub_1228((void *)cached_addr, msg[4], (const void *)msg[3], NULL);
            sceKernelDcacheWritebackRange((const void *)cached_addr, msg[4]);
            msg[5] = (u32)r;
        }
        sceKernelSetEventFlag(g_dec_evf_9440, 1);
    }
    return sceKernelExitThread(r);
}

/* 0x6b80 */
static NOINLINE void sub_6b80(void)
{
    for (int i = 0; i < 0xe8; i++) {
        g_ring0_9480[i].lba = 0;
        g_ring0_9480[i].valid = 0;
    }
    for (int i = 0; i < 8; i++) {
        g_ring1_9bc0[i].lba = 0;
        g_ring1_9bc0[i].valid = 0;
    }
    g_ring0_idx_9444 = 0;
    g_ring1_idx_9448 = 0;
}

/* 0x6bd4 */
static NOINLINE int sub_6bd4(u32 lba, u32 count, int skip, int table_sel, u32 *rem_lba, u32 *rem_blks)
{
    u32 aligned_start = 0;
    u32 aligned_end = 0;
    if (skip != 0) {
        return 0;
    }
    u32 blks = sub_7004(lba, count, &aligned_start, &aligned_end);
    int r = sub_6d24(aligned_start, blks, table_sel, rem_lba, rem_blks);
    if (r != 0) {
        if (table_sel == 0) {
            sub_703c(aligned_end, 0);
        }
        return 1;
    }
    return sub_6d24(aligned_start, blks, 0, rem_lba, rem_blks) != 0;
}

/* 0x6c9c */
static NOINLINE int sub_6c9c(u32 lba, u32 count, int seek_only, int table_sel)
{
    (void)seek_only;
    u32 aligned_start = 0;
    u32 aligned_end = 0;
    g_emu_9c24 = 0;
    u32 blks = sub_7004(lba, count, &aligned_start, &aligned_end);
    sub_6e4c(aligned_start, (int)blks, table_sel);
    if (table_sel != 0) {
        g_emu_9c24 = 0;
        return 0;
    }
    sub_703c(aligned_end, 0);
    return 0;
}

/* 0x6d10 */
static NOINLINE void sub_6d10(u32 lo, u32 hi)
{
    g_ring_elapsed_lo_9450 = lo;
    g_ring_elapsed_hi_9454 = (int)hi;
}

/* 0x6d24 */
static NOINLINE int sub_6d24(u32 start_lba, u32 blks, int table_sel, u32 *rem_lba, u32 *rem_blks)
{
    u32 off = 0;
    u32 matched = 0;
    if (table_sel != 0) {
        int idx = g_ring1_idx_9448;
        for (int step = 0; step < 9; step++, idx++) {
            if (idx >= 8) {
                idx = 0;
            }
            if (g_ring1_9bc0[idx].valid == 1) {
                u32 target = start_lba + off;
                if (g_ring1_9bc0[idx].lba == target) {
                    matched++;
                    off += 0x10U;
                    if (matched == blks) {
                        return 1;
                    }
                } else {
                    if (matched != 0) {
                        matched = 0;
                    }
                }
                *rem_lba = start_lba + (matched << 4);
                *rem_blks = blks - matched;
            }
        }
    } else {
        int idx = g_ring0_idx_9444;
        for (int step = 0; step < 0xe9; step++, idx++) {
            if (idx >= 0xe8) {
                idx = 0;
            }
            if (g_ring0_9480[idx].valid == 1) {
                u32 target = start_lba + off;
                if (g_ring0_9480[idx].lba == target) {
                    matched++;
                    off += 0x10U;
                    if (matched == blks) {
                        return 1;
                    }
                } else {
                    if (matched != 0) {
                        matched = 0;
                    }
                }
                *rem_lba = start_lba + (matched << 4);
                *rem_blks = blks - matched;
            }
        }
    }
    return (matched == blks) ? 1 : 0;
}

/* 0x6e4c */
static NOINLINE int sub_6e4c(u32 start_lba, int blks, int table_sel)
{
    int full = 0;
    int off = 0;
    if (table_sel != 0) {
        int idx = 0;
        for (; idx < 8; idx++) {
            full++;
            if (g_ring1_9bc0[idx].valid != 1) {
                full = 0;
                break;
            }
        }
        if (full != 0) {
            idx = g_ring1_idx_9448;
        }
        if (blks > 0) {
            for (int rem = blks; rem != 0; rem--, idx++) {
                if (idx >= 8) {
                    idx = 0;
                }
                g_ring1_idx_9448 = idx;
                if (full != 0) {
                    g_ring1_9bc0[idx].lba = start_lba + (u32)off;
                    g_ring1_9bc0[idx].valid = 1;
                    off += 0x10;
                } else if (g_ring1_9bc0[idx].valid != 1) {
                    g_ring1_9bc0[idx].valid = 1;
                    g_ring1_9bc0[idx].lba = start_lba + (u32)off;
                    off += 0x10;
                    full = 1;
                }
            }
        }
        g_ring1_idx_9448++;
        return 0;
    } else {
        int idx = 0;
        for (; idx < 0xe8; idx++) {
            full++;
            if (g_ring0_9480[idx].valid != 1) {
                full = 0;
                break;
            }
        }
        if (full != 0) {
            idx = g_ring0_idx_9444;
        }
        if (blks > 0) {
            for (int rem = blks; rem != 0; rem--, idx++) {
                if (idx >= 0xe8) {
                    idx = 0;
                }
                g_ring0_idx_9444 = idx;
                if (full != 0) {
                    g_ring0_9480[idx].lba = start_lba + (u32)off;
                    g_ring0_9480[idx].valid = 1;
                    off += 0x10;
                } else if (g_ring0_9480[idx].valid != 1) {
                    g_ring0_9480[idx].valid = 1;
                    g_ring0_9480[idx].lba = start_lba + (u32)off;
                    off += 0x10;
                    full = 1;
                }
            }
        }
        g_ring0_idx_9444++;
        return 0;
    }
}

/* 0x7004 */
static NOINLINE u32 sub_7004(u32 lba, u32 count, u32 *aligned_start, u32 *aligned_end)
{
    u32 end_blk = (lba + count - 1U) & ~15U;
    u32 start_blk = lba & ~15U;
    u32 blks = (start_blk < end_blk) ? (((end_blk - start_blk) >> 4) + 1U) : 1U;
    *aligned_start = start_blk;
    *aligned_end = end_blk + 0x10U;
    return blks;
}

/* 0x703c */
static NOINLINE int sub_703c(u32 end_lba, int table_sel)
{
    int span = 0x40;
    if (g_ring_elapsed_hi_9454 > 0 || (g_ring_elapsed_hi_9454 == 0 && g_ring_elapsed_lo_9450 > 0x9c3fU)) {
        u64 elapsed = ((u64)(u32)g_ring_elapsed_hi_9454 << 32) | (u64)g_ring_elapsed_lo_9450;
        s64 q = sub_1b38((s64)(elapsed - 40000ULL), 8000);
        int v = (int)q + 0x40;
        span = (v < 0x3f0) ? v : 0x3f0;
    }
    g_emu_9c24 = span;
    return sub_6e4c(end_lba, span / 16, table_sel);
}

/* 0x7104 */
static NOINLINE int sub_7104(u32 lba, u32 cnt, int seek_only, int mode, u32 *rem_lba, u32 *rem_blks)
{
    int cross_flags = 0;
    int hit = 0;
    if (seek_only == 0) {
        hit = (sub_6bd4(lba, cnt, 0, mode, rem_lba, rem_blks) != 0);
    }
    u32 prev_cur = g_emu_8c00;
    int small_adj = 0;
    if ((mode & 2) == 0 && hit == 0 && cnt < 0x41U) {
        u32 d = (lba < prev_cur) ? (prev_cur - lba) : (lba - prev_cur);
        if (cnt + d < 0x41U && prev_cur != 0) {
            small_adj = (lba >= prev_cur) ? 1 : 0;
        }
    }
    u32 seek_dist = 0;
    if (seek_only != 0 || hit == 0) {
        seek_dist = sub_7ca4(lba, g_emu_9c0c, &cross_flags);
    }
    s64 now = sceKernelGetSystemTimeWide();
    g_emu_time_9c10 = now;
    if (g_emu_time_8c18 != 0) {
        u64 diff = (u64)now - (u64)g_emu_time_8c18;
        sub_6d10((u32)diff, (u32)(diff >> 32));
    }
    u32 cur = g_emu_8c00;
    if (lba == cur) {
        if ((hit | small_adj) != 0) {
            sub_78d0(lba, cnt, seek_dist, 1, cross_flags);
            if (hit == 0 && small_adj != 0) {
                g_emu_9c0c = lba + cnt + 0x80U;
            }
        } else {
            sub_73c0(lba, cnt, seek_dist, 1, cross_flags, mode);
        }
    } else if (lba < cur && (lba >> 4) == (cur >> 4)) {
        if ((hit | small_adj) != 0) {
            sub_78d0(lba, cnt, seek_dist, 1, cross_flags);
            if (hit == 0 && small_adj != 0) {
                g_emu_9c0c = lba + cnt + 0x80U;
            }
        } else {
            sub_73c0(lba, cnt, seek_dist, 3, cross_flags, mode);
        }
    } else if (hit != 0) {
        sub_78d0(lba, cnt, seek_dist, 1, cross_flags);
    } else if (small_adj != 0 ||
               (lba < cur && (lba >> 4) == (cur >> 4)) ||
               (lba > cur && (lba & 0x0ffffff0U) == (cur >> 4))) {
        sub_78d0(lba, cnt, seek_dist, 2, cross_flags);
        if (hit == 0 && small_adj != 0) {
            g_emu_9c0c = lba + cnt + 0x80U;
        }
    } else {
        sub_73c0(lba, cnt, seek_dist, 3, cross_flags, mode);
    }

    u32 next_cur = lba + cnt;
    g_emu_time_8c18 = sceKernelGetSystemTimeWide();
    g_emu_9c00 = cnt;
    g_emu_8c00 = next_cur;
    if (hit == 0) {
        g_emu_9c08 = (u32)mode;
        g_emu_9c0c = next_cur + (u32)g_emu_9c24;
    } else {
        g_emu_9c08 = (u32)mode | 0xf0000000U;
    }
    return hit;
}

/* 0x73b4 */
static NOINLINE void sub_73b4(u32 lba)
{
    g_emu_9c0c = lba;
}

/* 0x73c0 */
static NOINLINE void sub_73c0(u32 lba, u32 cnt, u32 seek_dist, int mode, int cross_flags, int req_mode)
{
    u64 elapsed;
    if ((u64)g_emu_time_9c10 > (u64)g_emu_time_9c18) {
        elapsed = (u64)g_emu_time_9c10 - (u64)g_emu_time_9c18;
    } else {
        elapsed = ((u64)0 - (u64)g_emu_time_9c10) | (~(u64)g_emu_time_9c18);
    }
    if (cnt - 0x81U < 0x80U) {
        g_data_8a14 = 0x756;
        g_data_8a18 = 0x1f8;
    } else if (cnt - 0x41U < 0x40U) {
        g_data_8a14 = 0x8b8;
        g_data_8a18 = 0x167;
    } else {
        g_data_8a14 = 0x6ed;
        g_data_8a18 = 0x304;
    }
    sceUmd9660_driver_887C3193();
    u32 low = sceKernelGetSystemTimeLow();
    u32 s1 = (u32)g_data_8a18 + low;
    u32 half = (u32)g_data_8a0c;
    u32 mult;
    if (lba <= half) {
        if (lba < s1) {
            mult = (u32)g_data_8a14;
        } else {
            u32 q = lba / (u32)g_data_8a18;
            mult = (u32)g_data_8a14 - q;
            if (mult < 0x3e8U) {
                mult = 0x3e8U;
            }
        }
    } else {
        u32 v1 = lba - half;
        if (v1 < s1) {
            mult = 0x3e8U;
        } else {
            u32 q = v1 / (u32)g_data_8a18;
            mult = q + 0x3e8U;
            if (mult < 0x3e8U) {
                mult = 0x3e8U;
            }
            if (mult > (u32)g_data_8a14) {
                mult = (u32)g_data_8a14;
            }
        }
    }

    u32 factor;
    if (cnt >= 0x800U) {
        factor = ((cnt * 5U) >> 10) + 0x3e8U;
    } else if (cnt >= 0x400U) {
        factor = ((cnt * 5U) >> 9) + 0x3b6U;
    } else if (cnt >= 0x200U) {
        factor = ((cnt * 5U) >> 8) + 0x384U;
    } else {
        u32 r31 = low & 0x1fU;
        if (cnt >= 0x100U) {
            factor = r31 + 0x35cU;
        } else if (cnt >= 0x80U) {
            factor = r31 + 0x348U;
        } else if (cnt >= 0x40U) {
            factor = r31 + 0x334U;
        } else if (cnt >= 0x20U) {
            factor = r31 + 0x320U;
        } else if (cnt >= 8U) {
            factor = r31 + 0x30cU;
        } else {
            factor = r31 + 0x2eeU;
        }
    }

    u32 xfer = 0;
    if (factor != 0 || cnt != 0) {
        u32 v1 = (u32)(((u64)(factor * cnt) * 0x10624dd3ULL) >> 38);
        xfer = v1 * mult;
    }

    u32 total;
    int cf2 = cross_flags & 2;
    if (mode == 1) {
        total = (xfer * (u32)g_emu_rate1_9c20) >> 10;
    } else {
        xfer += (cnt < 0x80U) ? 0x3bd08U : 0x249f0U;
        u32 prev_mode = g_emu_9c08;
        if ((prev_mode & 1U) != 0 && (prev_mode & 0xf0000000U) != 0 && req_mode == 0) {
            xfer += 0x5000U;
        }
        u32 s_us = seek_dist * 5U;
        if ((cross_flags & 1) != 0) {
            s_us += 0xc350U;
        }
        if (g_emu_9c04 == 0 && cf2 != 0) {
            if (seek_dist - 0x2a01U <= 0xd5feU) {
                s_us += 0x60000U;
            } else if (seek_dist <= 0x10000U) {
                s_us += 0x28000U;
            } else {
                s_us += 0xa0000U;
            }
        } else {
            if ((g_emu_9c04 & 2) != 0 && cf2 == 0) {
                if (seek_dist < 0x3001U) {
                    s_us += 0x30000U;
                } else if (seek_dist <= 0xffffU) {
                    s_us += 0x80000U;
                } else if (seek_dist - 0x10001U <= 0xffffU) {
                    s_us += 0x50000U;
                } else if (seek_dist <= 0x2ffffU) {
                    s_us += 0xa0000U;
                }
            } else if (seek_dist >= 0x41U) {
                if (seek_dist - 0x201U < 0x1f0U) {
                    s_us += 0x15000U;
                } else if (seek_dist - 0x3f1U < 0xc10U) {
                    s_us += 0x5000U;
                } else if (seek_dist - 0x1001U <= 0xefffU) {
                    s_us += 0x90000U;
                }
            }
        }
        if (s_us > 0x28847aU) {
            s_us = 0x28847aU;
        }
        total = ((xfer * (u32)g_emu_rate1_9c20) >> 10) + ((s_us * (u32)g_emu_rate2_9c22) >> 10);
    }

    if (elapsed < (u64)total) {
        u32 wait_us = total - (u32)elapsed;
        if (wait_us != 0) {
            sceKernelDelayThread(wait_us);
        }
    }
    g_emu_9c04 = (cf2 == 0) ? 0 : 2;
}

/* 0x78d0 */
static NOINLINE void sub_78d0(u32 lba, u32 cnt, u32 seek_dist, int mode, int cross_flags)
{
    u64 elapsed = (u64)g_emu_time_9c10 - (u64)g_emu_time_9c18;
    sceUmd9660_driver_887C3193();
    u32 old_8a18 = (u32)g_data_8a18;
    u32 low = sceKernelGetSystemTimeLow();
    u32 s3 = old_8a18 + low;
    if (cnt - 0x81U < 0x80U) {
        g_data_8a14 = 0x728;
        g_data_8a18 = 0x213;
    } else if (cnt - 0x41U < 0x40U) {
        g_data_8a14 = 0x8d6;
        g_data_8a18 = 0x15e;
    } else if (cnt - 0x21U < 0x20U) {
        g_data_8a14 = 0x6b5;
        g_data_8a18 = 0x269;
    } else {
        g_data_8a18 = 0x23d;
    }

    u32 half = (u32)g_data_8a0c;
    u32 mult;
    if (lba <= half) {
        if (lba < s3) {
            mult = (u32)g_data_8a14;
        } else {
            u32 q = lba / (u32)g_data_8a18;
            if ((u32)g_data_8a14 < q) {
                mult = 0x3e8U;
            } else {
                mult = (u32)g_data_8a14 - q;
                if (mult < 0x3e8U) {
                    mult = 0x3e8U;
                }
            }
        }
    } else {
        u32 v1 = lba - half;
        if (v1 < s3) {
            mult = 0x3e8U;
        } else {
            u32 q = v1 / (u32)g_data_8a18;
            mult = q + 0x3e8U;
            if (mult < 0x3e8U) {
                mult = 0x3e8U;
            }
            if (mult > (u32)g_data_8a14) {
                mult = (u32)g_data_8a14;
            }
        }
    }

    u32 s6 = 0;
    u32 factor;
    if (cnt >= 0x800U) {
        s6 = (cnt * 5U) >> 10;
        factor = s6 + 0x3d4U;
    } else if (cnt >= 0x400U) {
        s6 = (cnt * 5U) >> 9;
        factor = s6 + 0x366U;
    } else if (cnt >= 0x100U) {
        if (mode == 2) {
            s6 = (u32)(((int)(cnt * 100U)) / 256) + 0x11170U;
        }
        factor = (low & 0x1fU) + 0x352U;
    } else if (cnt >= 0x80U) {
        if (mode == 2) {
            s6 = (u32)(((int)(cnt * 100U)) / 128) + 0x11170U;
        }
        factor = 0x320U;
    } else if (cnt >= 0x40U) {
        factor = 0x2eeU;
    } else if (cnt >= 0x20U) {
        factor = 0x2c6U;
    } else {
        factor = 0x280U;
    }

    u32 t6 = (u32)(((u64)(factor * cnt) * 0x10624dd3ULL) >> 38);
    u32 xfer = t6 * mult;
    if (mode == 2) {
        xfer = (cnt < 0x41U) ? (xfer << 3) : (xfer + s6);
    }
    if (g_emu_9c04 != cross_flags) {
        xfer += (low & 0x4fffU) + 0x1000U;
    }
    if (cnt < 0x11U) {
        xfer += 0x1388U;
    }

    u32 total;
    if (mode == 1) {
        total = (xfer * (u32)g_emu_rate1_9c20) >> 10;
    } else {
        u32 s_us = seek_dist * 5U;
        if (s_us > 0x28847aU) {
            s_us = 0x28847aU;
        }
        if ((cross_flags & 1) != 0) {
            s_us += 0xc350U;
        }
        total = ((xfer * (u32)g_emu_rate1_9c20) >> 10) + ((s_us * (u32)g_emu_rate2_9c22) >> 10);
    }

    if (elapsed < (u64)total) {
        u32 wait_us = total - (u32)elapsed;
        if (wait_us != 0) {
            sceKernelDelayThread(wait_us);
        }
    }
}

/* 0x7ca4 */
static NOINLINE u32 sub_7ca4(u32 lba, u32 prev_lba, int *cross_flags)
{
    *cross_flags = 0;
    u32 ref = g_emu_9c0c;
    u32 half = (u32)g_data_8a0c;
    if (ref < lba) {
        u32 diff = lba - prev_lba;
        if (lba > half) {
            if (prev_lba < half) {
                *cross_flags = (g_emu_8c00 > half) ? 2 : 3;
                u32 d1 = lba - half;
                u32 d2 = half - g_emu_9c0c;
                diff = (d2 < d1) ? (d1 - d2) : (d2 - d1);
            } else {
                *cross_flags = 2;
            }
        }
        return diff;
    } else {
        u32 diff = prev_lba - lba;
        if (prev_lba > half) {
            if (lba < half) {
                *cross_flags = (g_emu_8c00 > half) ? 1 : 0;
                u32 d1 = prev_lba - half;
                u32 d2 = half - lba;
                diff = (d1 < d2) ? (d2 - d1) : (d1 - d2);
            } else {
                *cross_flags = 2;
            }
        } else {
            *cross_flags = 2;
        }
        return diff;
    }
}
