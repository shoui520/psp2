#include <common_imp.h>

extern int sceKernelBootFrom(void);
extern int sceKernelGetChunk(int chunk_id);
extern int sceKernelApplicationType(void);
extern int sceKernelCpuSuspendIntr(void);
extern void sceKernelCpuResumeIntrWithSync(int intr);
extern void sceKernelCpuResumeIntr(int intr);
extern int sceIoOpen(const char *file, int flags, int mode);
extern s64 sceIoLseek(int fileDesc, s64 offset, int whence);
extern int sceIoIoctl(int fileDesc, u32 cmd, void *indata, int inlen, void *outdata, int outlen);
extern int sceIoRead(int fileDesc, void *data, u32 size);
extern int sceIoUnassign(const char *dev);
extern int sceIoClose(int fileDesc);
extern int sceIoGetstat(const char *file, void *stat);
extern int sceIoAssign(const char *devAlias, const char *blockDev, const char *fsDev, int mode, void *fsArg, long fsArgLen);
extern int sceKernelExitVSHKernel(void *param);
extern int sceKernelStartModule(int modid, u32 argsize, void *argp, int *status, void *option);
extern int sceKernelLoadModuleForKernel(const char *path, int flags, void *option);
extern int sceKernelRegisterGetIdFunc(void *func);
extern void *sceKernelMemset(void *dst, int c, u32 size);
extern int sceKernelGetCompiledSdkVersion(void);
extern int sceKernelGetUIDcontrolBlock(int uid, void **block);
extern void *sceKernelGetBlockHeadAddr(int blockid);
extern int strncmp(const char *lhs, const char *rhs, u32 n);
extern u32 strnlen(const char *s, u32 maxlen);
extern void *memcpy(void *dst, const void *src, u32 n);
extern char *strncpy(char *dst, const char *src, u32 n);
extern int snprintf(char *str, u32 size, const char *format, ...);
extern int sceKernelCreateThread(const char *name, void *entry, int initPriority, int stackSize, u32 attr, void *option);
extern int sceKernelUnlockMutex(int mutexid, int unlockCount);
extern int sceKernelExitDeleteThread(int status);
extern int sceKernelLockMutex(int mutexid, int lockCount, u32 *timeout);
extern int sceKernelCreateMutex(const char *name, u32 attr, int initCount, void *option);
extern int sceKernelExtendKernelStack(int size, int (*cb)(void *), void *arg);
extern int sceKernelCreateFpl(const char *name, int part, int attr, u32 size, u32 blocks, void *opt);
extern int sceKernelDelayThread(u32 delay);
extern int sceKernelAllocateFpl(int uid, void **data, u32 *timeout);
extern int sceKernelDeleteFpl(int uid);
extern int sceKernelStartThread(int thid, u32 arglen, void *argp);
extern int sceKernelDcacheInvalidateRangeForUser(const void *p, u32 size);
extern int sceDrmBBCipherUpdate(void *ckey, void *data, int size);
extern int sceDrmBBCipherInit(void *ckey, int type, int mode, void *header_key, void *version_key, u32 seed);
extern int sceDrmBBMacInit(void *mkey, int type);
extern int sceDrmBBMacUpdate(void *mkey, void *buf, int size);
extern int sceDrmBBCipherFinal(void *ckey);
extern int sceDrmBBMacFinal(void *mkey, void *buf, void *vkey);
extern int sceDrmBBMacFinal2(void *mkey, void *out, void *vkey);
extern int sceCtrl_driver_6C86AF22(int mode);
extern int sceDisplaySetResumeMode(int mode);
extern int sceDisplayWaitVblankStartCB(void);
extern int sceGeListSync(int qid, int syncType);
extern int sceGeListEnQueue(const void *list, void *stall, int cbid, void *arg);
extern int sceImpose_driver_DC3BECFF(int item);
extern int sceKermitPeripheral_driver_8C7903E7(int key, int offset, int buf, int len);
extern int sceKermit_driver_36666181(void *cmd, int bus, int cmd_id, int argc, int allow_callback, u64 *resp);
extern int sceNpDrmGetVersionKey(void *vkey, void *act, void *rif, u32 flags);
extern int sceNpDrmCheckRifTimeLimit(void *rif);
extern int sceKernelPowerTick(int type);
extern int sceKernelPowerUnlock(int lockType);
extern int sceKernelPowerLock(int lockType);
extern int sceKernelRegisterSysEventHandler(void *handler);
extern int sceKernelUnregisterSysEventHandler(void *handler);

int sceMeAudio_A651EA7B(int fileDesc, void *buf, u32 offset, u32 size);
int popsCheckAppAndRegisterIdFunc(void);
static int popsRecheckLicense(void);
static int popsReadDiscTable(u32 idx, u32 offset, void *buf, u32 size);
static int popsLoadLicenseKey(const char *path, void *vkey, void *content_id);
static int popsVerifyRifCallback(void *arg);
static void popsInvokeNp9660Callback(int res, u32 flags, u32 t_lo, u32 t_hi, u32 e_lo, u32 e_hi);
static int popsGetIdCallback(const char *path, void *vkey);
static int popsSysEventHandler(int ev_id, char *ev_name, void *param, int *result);
static int popsInitStub(void);
static int popsTermStub(void);
static int popsInitKermitPocs(void);
static int popsKermitSendTwoArgs(int cmd_id, int firstArg, int secondArg);
static int popsKermitSendOneArg(int cmd_id, int firstArg);
static int popsKermitSendNoArg(int cmd_id);

typedef struct {
    u8 headerBytes[0x16];
    u16 attr;
} ScePopsUidCb;

typedef struct {
    u32 status;
    u32 size;
    u16 nextBlock;
} ScePopsMcDirEntry;

typedef struct {
    u32 version;
    u32 flags;
    u8 accountId[8];
    char contentId[0x30];
    u8 keyBlock[0x28];
    u32 startTimeHi;
    u32 startTimeLo;
    u8 signatureBlock[0x28];
    u32 expireHi;
    u32 expireLo;
} ScePopsRif;

typedef struct {
    char contentId[0x30];
    u32 drmFlags;
} ScePopsCidBlock;

typedef struct {
    u32 magic;
    u32 version;
    u32 paramSfoOffset;
    u32 iconPngOffset;
    u32 iconPmfOffset;
    u32 picZeroOffset;
    u32 picOneOffset;
    u32 sndAtOffset;
    u32 dataPspOffset;
    u32 dataPsarOffset;
} ScePopsPbpHeader;

typedef struct {
    u8 leadingData[0x3D0];
    u32 docOffset;
    u32 docSize;
    u8 middleData[0x1220 - 0x3D8];
    u32 discBaseLo;
    u32 discBaseHi;
    u8 discMetaGap[0x12B4 - 0x1228];
    u32 altDocOffset;
    u32 altDocSize;
    u8 tableMetaGap[0x12D4 - 0x12BC];
    u32 indexTableOffset;
    u32 indexTableCount;
} ScePopsPsarHeader;

typedef struct SceSysEventHandler {
    int size;
    char *name;
    int type_mask;
    int (*handler)(int ev_id, char *ev_name, void *param, int *result);
    int gpVal;
    int busy;
    struct SceSysEventHandler *next;
    int reserved[9];
} SceSysEventHandler;

static SceSysEventHandler g_sys_event = {
    0x40,
    "ScePopsMan",
    0x00FFFF00,
    popsSysEventHandler,
    0, 0, NULL,
    { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static u8 g_version_key[16];      /* 0x2cd0 */
static u32 g_psar_offset;         /* 0x2ce0 */
static u32 g_disc_base_lo;        /* 0x2ce8 */
static u32 g_disc_base_hi;        /* 0x2cec */
static int g_need_recheck;        /* 0x2cf0 */
static int g_rif_status;          /* 0x2cf4 */
static u32 g_hw_rev;              /* 0x2cf8 */
static u32 g_doc_offset;          /* 0x2cfc */
static u32 g_doc_size;            /* 0x2d00 */
static int g_resume_state;        /* 0x2d04 */
static int g_key_inited;          /* 0x2d08 */
static char g_content_id[48];     /* 0x2d0c */
static u32 g_magic_val;           /* 0x2d3c */
static u32 g_disc_table[28];      /* 0x2d40..0x2db0 */
static int g_sdk_ver;             /* 0x2db0 */
static u32 g_vsh_param[12];       /* 0x2db4 */
static u32 g_chunk_param[8];      /* 0x2de4 */
static u32 g_act_path[4];         /* 0x2e04 */
static u32 g_fl2_dev[2];          /* 0x2e14 */
static u32 g_lfl_dev[3];          /* 0x2e1c */
static u32 g_flf_dev[3];          /* 0x2e28 */
static u32 g_np9660_gp;           /* 0x2e34 */
static void (*g_np9660_cb)(int, u32, u32, u32, u32, u32); /* 0x2e38 */
static int g_resume_flag;         /* 0x2e40 */
static int g_resume_thid;         /* 0x2e44 */
static int g_pocs_mutex;          /* 0x2e48 */
static u32 g_pocs_base1;          /* 0x2e4c */
static u32 g_pocs_base2;          /* 0x2e50 */
static u32 g_pocs_size;           /* 0x2e54 */
static char g_pbp_path[256];      /* 0x2e58 */

u32 sceMeCore_driver_var_385A4CC6; /* 0x2f58 */
u32 sceMeCore_driver_var_61FF4837; /* 0x2f5c */
u32 sceMeCore_driver_var_AB354CA1; /* 0x2f60 */
u32 sceUsb_driver_var_3EA085BF;    /* 0x2f64 */
u32 sceUsb_driver_var_A942D291;    /* 0x2f68 */

/* 0x0000 */
static int popsInitPbpPath(const char *path)
{
    u32 hdr[10];
    int fileDesc;
    int res;

    if (g_pbp_path[0] != 0) {
        return (strncmp(path, g_pbp_path, 0x100) == 0) ? 0 : -1;
    }
    sceKernelMemset(g_pbp_path, 0, 0x100);
    if (strnlen(path, 0x100) >= 0x100) {
        return -1;
    }
    strncpy(g_pbp_path, path, 0xFF);
    fileDesc = sceIoOpen(path, 1, 0);
    if (fileDesc <= 0) {
        return -1;
    }
    if (sceIoRead(fileDesc, hdr, 0x28) != 0x28) {
        sceIoClose(fileDesc);
        return -1;
    }
    res = sceMeAudio_A651EA7B(fileDesc, &g_magic_val, hdr[8], 4);
    sceIoClose(fileDesc);
    if (res != 4 || hdr[0] != 0x50425000) {
        return -1;
    }
    g_psar_offset = hdr[9];
    return 0;
}

/* 0x0124 */
static int popsSetVersionKey(const char *path, const u8 *vkey, const char *cid)
{
    int i;
    (void)path;
    if (!g_key_inited) {
        g_key_inited = 1;
        for (i = 0; i < 16; i++) {
            g_version_key[i] = vkey[i];
        }
        for (i = 0; i < 48; i++) {
            g_content_id[i] = cid[i];
        }
    }
    return 0;
}

/* 0x0190 */
static int popsBuildRifPath(const char *cid, char *out_path)
{
    char driveChar;

    if (cid == NULL || out_path == NULL) {
        return 0x80010016;
    }
    driveChar = g_pbp_path[0];
    if (driveChar != 'e' && driveChar != 'm') {
        return 0x80010016;
    }
    if (strnlen(cid, 0x25) != 0x24) {
        return 0x80010016;
    }
    out_path[0] = g_pbp_path[0];
    out_path[1] = g_pbp_path[1];
    memcpy(out_path + 2, "0:/PSP/LICENSE/", 0x0F);
    memcpy(out_path + 0x11, cid, 0x24);
    memcpy(out_path + 0x35, ".rif", 4);
    out_path[0x39] = 0;
    return 0;
}

/* 0x0280 */
int sceMeAudio_A8176E49(u32 *out_offset)
{
    u32 k1;
    int fileDesc;
    void *cb;

    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");
    if (((k1 << 11) & (u32)out_offset) & 0x80000000) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0x80000023;
    }
    if (g_rif_status < 0) {
        if (g_resume_state == 0 || g_rif_status != (int)0x80550914) {
            __asm__ volatile ("move $k1, %0" : : "r"(k1));
            return -1;
        }
    }
    fileDesc = sceIoOpen(g_pbp_path, 1, 0);
    if (fileDesc > 0) {
        if (sceKernelGetUIDcontrolBlock(fileDesc, &cb) == 0) {
            ((ScePopsUidCb *)cb)->attr |= 0x15;
        }
        *out_offset = g_psar_offset;
    }
    __asm__ volatile ("move $k1, %0" : : "r"(k1));
    return fileDesc;
}

/* 0x0358 */
int sceMeAudio_34F2548F(u8 *buf)
{
    u32 k1;
    u32 off;
    int fileDesc, res;

    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");
    if (((k1 << 11) & (u32)buf) & 0x80000000) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0x80000023;
    }
    off = g_psar_offset + 0x200;
    fileDesc = sceIoOpen(g_pbp_path, 0x40000001, 0);
    if (fileDesc >= 0) {
        res = sceIoIoctl(fileDesc, 0x04100002, &off, 4, NULL, 0);
        if (res < 0 || (res = sceIoIoctl(fileDesc, 0x04100001, g_version_key, 16, NULL, 0)) < 0) {
            sceIoClose(fileDesc);
            fileDesc = res;
        }
    }
    if (fileDesc < 0) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return fileDesc;
    }
    res = sceIoRead(fileDesc, buf + 0x200, 0x200);
    sceIoClose(fileDesc);
    __asm__ volatile ("move $k1, %0" : : "r"(k1));
    if (res != 0x200) {
        return -1;
    }
    const ScePopsPsarHeader *psarHdr = (const ScePopsPsarHeader *)buf;
    memcpy(g_disc_table, buf + 0x200, 0x64);
    g_doc_offset = g_psar_offset + psarHdr->docOffset;
    g_doc_size = psarHdr->docSize;
    return g_magic_val ^ 0x4A08B53F;
}

/* 0x04e4 */
int sceMeAudio_F1502A62(u8 *buf, u32 idx)
{
    u8 mkey[48];
    int res;
    u32 k1;
    const ScePopsPsarHeader *psarHdr = (const ScePopsPsarHeader *)buf;

    res = popsReadDiscTable(idx, 0x400, buf + 0x400, 0xB3880);
    if (res < 0) {
        return res;
    }
    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");
    if (g_disc_table[0] < g_disc_table[1]) {
        if (sceDrmBBMacInit(mkey, 3) < 0 ||
            sceDrmBBMacUpdate(mkey, buf, 0xB3C80) < 0 ||
            sceDrmBBMacFinal2(mkey, &g_disc_table[5 + (idx << 2)], g_version_key) < 0) {
            __asm__ volatile ("move $k1, %0" : : "r"(k1));
            return -1;
        }
        if (g_sdk_ver <= 0x0304FFFF) {
            g_disc_table[1] = g_sdk_ver;
        }
    } else {
        g_doc_offset = g_psar_offset + psarHdr->altDocOffset;
        g_doc_size = psarHdr->altDocSize;
    }
    {
        u64 base = (u64)g_psar_offset + ((u64)psarHdr->discBaseHi << 32) + psarHdr->discBaseLo;
        g_disc_base_lo = (u32)base;
        g_disc_base_hi = (u32)(base >> 32);
    }
    g_hw_rev = (*(volatile u32 *)0xBC100040) & 3;
    __asm__ volatile ("move $k1, %0" : : "r"(k1));
    return g_magic_val ^ 0x4A08B53F;
}

/* 0x067c */
int popsReadDocData(void *buf)
{
    u32 k1, size, off;
    int fileDesc, res;

    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");
    size = g_doc_size;
    if ((((k1 << 11) & ((u32)buf | ((u32)buf + size) | size)) & 0x80000000) || size >= 0x4001) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0x80000023;
    }
    off = g_doc_offset;
    fileDesc = sceIoOpen(g_pbp_path, 0x40000001, 0);
    if (fileDesc >= 0) {
        res = sceIoIoctl(fileDesc, 0x04100002, &off, 4, NULL, 0);
        if (res < 0 || (res = sceIoIoctl(fileDesc, 0x04100001, g_version_key, 16, NULL, 0)) < 0) {
            sceIoClose(fileDesc);
            fileDesc = res;
        }
    }
    if (g_sdk_ver <= 0x0601FFFF) {
        popsTermStub();
    }
    if (fileDesc < 0) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return fileDesc;
    }
    res = sceIoRead(fileDesc, buf, g_doc_size);
    sceIoClose(fileDesc);
    __asm__ volatile ("move $k1, %0" : : "r"(k1));
    return ((u32)res == g_doc_size) ? res : -1;
}

/* 0x0e84 */
static int popsMacMemcardChain(u8 *base, u8 *entry, u8 *block, u32 skip, int mode)
{
    u8 mkey[48];
    u8 *cur_entry = entry;

    if (sceDrmBBMacInit(mkey, 1) < 0) {
        return -1;
    }
    do {
        if (sceDrmBBMacUpdate(mkey, block + skip, 0x2000 - skip) < 0) {
            return -1;
        }
        skip = 0;
        u16 next = ((const ScePopsMcDirEntry *)cur_entry)->nextBlock;
        if (next == 0xFFFF) {
            cur_entry = NULL;
        } else {
            u32 idx = (u32)next + 1;
            if (idx >= 16) {
                return -1;
            }
            block = base + (idx << 13);
            cur_entry = base + (idx << 7);
        }
    } while (cur_entry != NULL);

    if (mode == 2) {
        return (sceDrmBBMacFinal2(mkey, entry + 0x30, g_version_key) < 0) ? -1 : 0;
    }
    return (sceDrmBBMacFinal(mkey, entry + 0x30, g_version_key) < 0) ? -1 : 0;
}

/* 0x0f90 */
static int popsCipherMemcardChain(u8 *base, u8 *entry, u8 *block, u32 skip, int mode)
{
    u8 ckey[48];
    u8 *cur_entry = entry;

    if (sceDrmBBCipherInit(ckey, 1, mode, entry + 0x20, g_version_key, 0) < 0) {
        return -1;
    }
    do {
        if (sceDrmBBCipherUpdate(ckey, block + skip, 0x2000 - skip) < 0) {
            return -1;
        }
        skip = 0;
        u16 next = ((const ScePopsMcDirEntry *)cur_entry)->nextBlock;
        if (next == 0xFFFF) {
            cur_entry = NULL;
        } else {
            u32 idx = (u32)next + 1;
            if (idx >= 16) {
                return -1;
            }
            block = base + (idx << 13);
            cur_entry = base + (idx << 7);
        }
    } while (cur_entry != NULL);

    return (sceDrmBBCipherFinal(ckey) < 0) ? -1 : 0;
}

/* 0x0804 */
int popsCryptMemcardBlock(u8 *base, u32 idx, int flag)
{
    u32 k1;
    u8 *entry, *block;
    u32 skip;
    int i, res;

    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");
    entry = base + (idx << 7);
    block = base + (idx << 13);
    if ((((k1 << 11) & ((u32)base | ((u32)base + 0x20000))) & 0x80000000) || idx >= 16) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0x80000023;
    }
    if (*(u32 *)entry != 0x51 || block[0x52] != 0x4D) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0;
    }
    skip = block[2] & 3;
    if (skip == 0) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0;
    }
    skip = (skip << 7) + 0x80;
    if (flag != 0) {
        u32 *hdr_key = (u32 *)(entry + 0x20);
        for (i = 0; i < 8; i++) {
            if (hdr_key[i] != 0) {
                __asm__ volatile ("move $k1, %0" : : "r"(k1));
                return -1;
            }
        }
        if (popsCipherMemcardChain(base, entry, block, skip, 1) != 0) {
            __asm__ volatile ("move $k1, %0" : : "r"(k1));
            return -1;
        }
        res = popsMacMemcardChain(base, entry, block, skip, 1);
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return res;
    } else {
        u32 *hdr_key = (u32 *)(entry + 0x20);
        if (popsMacMemcardChain(base, entry, block, skip, 2) != 0 ||
            popsCipherMemcardChain(base, entry, block, skip, 2) != 0) {
            __asm__ volatile ("move $k1, %0" : : "r"(k1));
            return -1;
        }
        for (i = 0; i < 8; i++) {
            hdr_key[i] = 0;
        }
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0;
    }
}

/* 0x09b0 */
u64 popsGetDiscBaseOffset(void)
{
    if (g_disc_base_lo == g_psar_offset && g_disc_base_hi == 0) {
        return 0;
    }
    return ((u64)g_disc_base_hi << 32) | g_disc_base_lo;
}

/* 0x0a00 */
int popsVerifyMac(void *buf, void *mac, int size)
{
    u8 mkey[48];
    int res;

    if (sceDrmBBMacInit(mkey, 3) < 0) {
        return 0;
    }
    if (sceDrmBBMacUpdate(mkey, buf, size) < 0) {
        return 0;
    }
    res = sceDrmBBMacFinal2(mkey, mac, g_version_key);
    return (res >= 0) ? 1 : 0;
}

/* 0x0a90 */
int sceMeAudio_6FCDD82D(void *buf, void *mac, u32 size)
{
    u32 k1;
    u8 mkey[48];
    int res;

    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");
    if ((((k1 << 11) & ((u32)buf | ((u32)buf + size) | size)) & 0x80000000) ||
        (((k1 << 11) & (u32)mac) & 0x80000000)) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0x80000023;
    }
    if (sceDrmBBMacInit(mkey, 3) < 0 ||
        sceDrmBBMacUpdate(mkey, buf, size) < 0) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0;
    }
    res = sceDrmBBMacFinal2(mkey, mac, g_version_key);
    __asm__ volatile ("move $k1, %0" : : "r"(k1));
    return (res >= 0) ? 1 : 0;
}

/* 0x0b60 */
static int popsResumeThread(u32 args, void *argp)
{
    (void)args;
    (void)argp;
    while (popsRecheckLicense() != 0) {
        sceDisplayWaitVblankStartCB();
    }
    g_resume_thid = 0;
    sceKernelExitDeleteThread(0);
    return 0;
}

/* 0x0ba4 */
static void popsCheckRifLicense(void)
{
    u32 k1;
    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");
    g_rif_status = sceKernelExtendKernelStack(0x800, popsVerifyRifCallback, g_content_id);
    __asm__ volatile ("move $k1, %0" : : "r"(k1));
}

/* 0x0bec */
int sceMeAudio_83609AC9(u8 *hdr, u32 idx, void *buf)
{
    const ScePopsPsarHeader *psarHdr = (const ScePopsPsarHeader *)hdr;
    u32 count = psarHdr->indexTableCount;
    u32 offset = psarHdr->indexTableOffset;
    return popsReadDiscTable(idx, offset, buf, count * 12);
}

/* 0x0c20 */
static int popsRecheckLicense(void)
{
    char cid[48];
    u8 vkey[16];
    int i;

    if (g_need_recheck == 0) {
        return 0;
    }
    if (popsLoadLicenseKey(g_pbp_path, vkey, cid) < 0) {
        return 1;
    }
    if (g_resume_state == 0 || g_rif_status != (int)0x80550914) {
        memcpy(g_version_key, vkey, 16);
    }
    for (i = 0; i < 48; i++) {
        if (cid[i] != g_content_id[i]) {
            g_need_recheck = 1;
            *(u32 *)&g_version_key[0] = 0;
            *(u32 *)&g_version_key[4] = 0;
            *(u32 *)&g_version_key[8] = 0;
            *(u32 *)&g_version_key[12] = 0;
            return 1;
        }
    }
    g_need_recheck = 0;
    return 0;
}

/* 0x0d14 */
static int popsReadDiscTable(u32 idx, u32 offset, void *buf, u32 size)
{
    u32 k1, file_off;
    int fileDesc, res;

    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");
    if (((k1 << 11) & ((u32)buf | ((u32)buf + size) | size)) & 0x80000000) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0x80000023;
    }
    if (idx >= 5) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return -1;
    }
    file_off = g_psar_offset + g_disc_table[idx] + offset;
    fileDesc = sceIoOpen(g_pbp_path, 0x40000001, 0);
    if (fileDesc >= 0) {
        res = sceIoIoctl(fileDesc, 0x04100002, &file_off, 4, NULL, 0);
        if (res < 0 || (res = sceIoIoctl(fileDesc, 0x04100001, g_version_key, 16, NULL, 0)) < 0) {
            sceIoClose(fileDesc);
            fileDesc = res;
        }
    }
    if (fileDesc < 0) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return fileDesc;
    }
    res = sceIoRead(fileDesc, buf, size);
    sceIoClose(fileDesc);
    __asm__ volatile ("move $k1, %0" : : "r"(k1));
    return ((u32)res == size) ? 0 : -1;
}

/* 0x1070 */
int scePopsMan_29B3FB24(const char *path, int mode)
{
    u32 k1;
    int modid = 0, res;

    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");
    if (((k1 << 11) & (u32)path) & 0x80000000) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0x80000023;
    }
    if (g_sdk_ver != 0) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return -1;
    }
    g_sdk_ver = sceKernelGetCompiledSdkVersion();
    if (g_sdk_ver <= 0x02FFFFFF) {
        __builtin_trap();
    }
    snprintf((char *)0x00013F80, 0x80, "branches/pops-for-vita-350/pops/popsman4vita(r%d)", 0x3661);
    if (popsInitPbpPath(path) < 0) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return -1;
    }
    if (mode != 2) {
        modid = sceKernelLoadModuleForKernel("flash0:/kd/pops_01g.prx", 0, NULL);
        if (modid >= 0) {
            res = sceKernelStartModule(modid, 0, NULL, NULL, NULL);
            if (res < 0) {
                modid = res;
            }
        }
    }
    sceCtrl_driver_6C86AF22(0);
    __asm__ volatile ("move $k1, %0" : : "r"(k1));
    return modid;
}

/* 0x118c */
int scePopsMan_0090B2C8(u32 exitCode)
{
    int i, chunk_id;
    u32 *chunk_ptr;

    __asm__ volatile ("sll $k1, $k1, 11");
    *(volatile u32 *)0x49FE01AC = exitCode;
    for (i = 0; i < 12; i++) {
        g_vsh_param[i] = 0;
    }
    for (i = 0; i < 8; i++) {
        g_chunk_param[i] = 0;
    }
    g_vsh_param[0] = 0x30;
    chunk_id = sceKernelGetChunk(0);
    if (chunk_id > 0) {
        chunk_ptr = (u32 *)sceKernelGetBlockHeadAddr(chunk_id);
    } else {
        chunk_ptr = g_chunk_param;
    }
    chunk_ptr[0] = 0x20;
    chunk_ptr[6] = exitCode;
    chunk_ptr[1] = 0x20;
    chunk_ptr[2] = 0;
    chunk_ptr[4] = 0;
    g_vsh_param[1] = 0x20;
    g_vsh_param[5] = (u32)chunk_ptr;
    g_vsh_param[4] = chunk_ptr[0];
    g_vsh_param[2] = (u32)chunk_ptr;
    g_vsh_param[3] = 0;
    g_vsh_param[6] = 0;
    g_vsh_param[7] = 0;
    sceKernelExitVSHKernel(g_vsh_param);
    for (;;) {
    }
}

/* 0x125c */
int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    g_pbp_path[0] = 0;
    if (popsCheckAppAndRegisterIdFunc() == 1) {
        return 1;
    }
    sceKernelRegisterSysEventHandler(&g_sys_event);
    popsInitKermitPocs();
    popsInitStub();
    return 0;
}

/* 0x12b4 */
int module_reboot_before(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    sceKernelUnregisterSysEventHandler(&g_sys_event);
    popsTermStub();
    return 0;
}

/* 0x12e0 */
void sceMeAudio_EB52DFE0(void)
{
    u32 k1;
    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");
    if (sceImpose_driver_DC3BECFF(2) != 4) {
        sceKernelPowerTick(6);
    }
    __asm__ volatile ("move $k1, %0" : : "r"(k1));
}

/* 0x1324 */
int sceMeAudio_A651EA7B(int fileDesc, void *buf, u32 offset, u32 size)
{
    if (sceIoLseek(fileDesc, offset, 0) != (s64)offset) {
        return -1;
    }
    return sceIoRead(fileDesc, buf, size);
}

/* 0x13ac */
int sceMeAudio_59759606(int fileDesc, void *buf, u32 offset)
{
    if (g_sdk_ver > 0x0302FFFF) {
        return 0;
    }
    if (sceIoLseek(fileDesc, offset, 0) != (s64)offset) {
        return -1;
    }
    return sceIoRead(fileDesc, buf, 0x9300);
}

/* 0x1444 */
int sceMeAudio_F7CD1362(int lockType)
{
    u32 k1;
    int res;
    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");
    res = sceKernelPowerUnlock(lockType);
    __asm__ volatile ("move $k1, %0" : : "r"(k1));
    return res;
}

/* 0x1470 */
int sceMeAudio_C4169D0F(int key, int offset, int buf, int len)
{
    u32 k1;
    int res;
    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");
    res = sceKermitPeripheral_driver_8C7903E7(key, offset, buf, len);
    __asm__ volatile ("move $k1, %0" : : "r"(k1));
    return res;
}

/* 0x149c */
int popsCheckAppAndRegisterIdFunc(void)
{
    int boot_from = sceKernelBootFrom();
    int app_type = sceKernelApplicationType();
    if (app_type != 0x300 && (boot_from != 0x40 || app_type != 0x200)) {
        return 1;
    }
    return (sceKernelRegisterGetIdFunc(popsGetIdCallback) < 0) ? 1 : 0;
}

/* 0x14fc */
static int popsLoadLicenseKey(const char *path, void *vkey, void *content_id)
{
    u32 stat_buf[24];
    int fpl, fileDesc, res;
    u8 *work, *rif_buf, *cid_buf, *rif_path, *pbp_hdr, *tmp_vkey;

    if (path == NULL || vkey == NULL) {
        return 0x80010016;
    }
    fpl = sceKernelCreateFpl("ScePopsMan", 1, 0, 0x1420, 1, NULL);
    if (fpl < 0) {
        return fpl;
    }
    res = sceKernelAllocateFpl(fpl, (void **)&work, NULL);
    if (res < 0) {
        sceKernelDeleteFpl(fpl);
        return res;
    }
    rif_buf = work + 0x1038;
    cid_buf = work + 0x10D8;
    rif_path = work + 0x110C;
    pbp_hdr = work + 0x1210;
    tmp_vkey = work + 0x1410;

    do {
        fileDesc = sceIoOpen(path, 1, 0);
        if (fileDesc >= 0) {
            res = sceIoRead(fileDesc, pbp_hdr, 0x200);
            if (res >= 0) {
                if (pbp_hdr[0] == 0 && pbp_hdr[1] == 'P' && pbp_hdr[2] == 'B' && pbp_hdr[3] == 'P') {
                    u32 psar_off = ((const ScePopsPbpHeader *)pbp_hdr)->dataPspOffset + 0x560;
                    s64 seek_res = sceIoLseek(fileDesc, psar_off, 0);
                    res = (int)seek_res;
                    if (seek_res >= 0) {
                        res = sceIoRead(fileDesc, cid_buf, 0x34);
                        if (res >= 0) {
                            ((ScePopsCidBlock *)cid_buf)->drmFlags = __builtin_bswap32(((ScePopsCidBlock *)cid_buf)->drmFlags);
                            res = 0;
                        }
                    }
                } else {
                    res = 0x80020148;
                }
            }
            sceIoClose(fileDesc);
        } else {
            res = fileDesc;
        }
        if (res < 0) {
            break;
        }
        res = popsInitPbpPath(path);
        if (res < 0) {
            break;
        }
        res = popsBuildRifPath((const char *)cid_buf, (char *)rif_path);
        if (res < 0) {
            break;
        }
        res = sceIoGetstat((const char *)rif_path, stat_buf);
        if (res < 0) {
            break;
        }
        if ((s64)(((u64)stat_buf[3] << 32) | stat_buf[2]) < 0x98) {
            res = 0x80010016;
            break;
        }
        fileDesc = sceIoOpen((const char *)rif_path, 1, 0);
        if (fileDesc >= 0) {
            res = sceIoRead(fileDesc, rif_buf, 0xA0);
            if (res >= 0 && res < 0x98) {
                res = 0x80010016;
            }
            sceIoClose(fileDesc);
            if (res >= 0) {
                if (res < 0xA0) {
                    ((ScePopsRif *)rif_buf)->expireHi = 0;
                    ((ScePopsRif *)rif_buf)->expireLo = 0;
                }
                if (strncmp((const char *)(rif_buf + 0x10), (const char *)cid_buf, 0x30) != 0) {
                    res = 0x80010016;
                    break;
                }
                res = 0;
            }
        } else {
            res = fileDesc;
        }
        if (res < 0) {
            break;
        }
        void *act_buf = work;
        if (__builtin_bswap32(((ScePopsRif *)rif_buf)->flags) != 3) {
            int app_type;
            /* "flash2:/act.dat" */
            g_act_path[0] = 0x73616C66;
            g_act_path[1] = 0x2F3A3268;
            g_act_path[2] = 0x2E746361;
            g_act_path[3] = 0x00746164;
            app_type = sceKernelApplicationType();
            /* "lflash0:0,2", "flashfat2:", "flash2:" */
            g_lfl_dev[0] = 0x616C666C;
            g_lfl_dev[1] = 0x3A306873;
            g_lfl_dev[2] = 0x00322C30;
            g_flf_dev[0] = 0x73616C66;
            g_flf_dev[1] = 0x74616668;
            g_flf_dev[2] = 0x00003A32;
            g_fl2_dev[0] = 0x73616C66;
            g_fl2_dev[1] = 0x003A3268;
            if (app_type == 0x300) {
                if (sceIoAssign((const char *)g_fl2_dev, (const char *)g_lfl_dev, (const char *)g_flf_dev, 1, NULL, 0) < 0) {
                    res = 0x80020327;
                    break;
                }
            }
            res = sceIoGetstat((const char *)g_act_path, stat_buf);
            if (res >= 0) {
                if (stat_buf[2] == 0x1038 && stat_buf[3] == 0) {
                    fileDesc = sceIoOpen((const char *)g_act_path, 1, 0);
                    if (fileDesc >= 0) {
                        res = sceIoRead(fileDesc, work, 0x1038);
                        sceIoClose(fileDesc);
                        if (res < 0x1038) {
                            res = 0x80020327;
                        }
                    } else {
                        res = 0x80020327;
                    }
                } else {
                    res = 0x80010016;
                }
            } else if (res != (int)0x80010002) {
                res = 0x80020327;
            }
            if (app_type == 0x300) {
                int ures = sceIoUnassign((const char *)g_fl2_dev);
                if (ures < 0) {
                    res = 0x80020327;
                }
            }
            g_act_path[0] = 0; g_act_path[1] = 0; g_act_path[2] = 0; g_act_path[3] = 0;
            g_fl2_dev[0] = 0; g_fl2_dev[1] = 0;
            g_lfl_dev[0] = 0; g_lfl_dev[1] = 0; g_lfl_dev[2] = 0;
            g_flf_dev[0] = 0; g_flf_dev[1] = 0; g_flf_dev[2] = 0;
            if (res < 0) {
                break;
            }
        } else {
            act_buf = NULL;
        }
        res = sceNpDrmGetVersionKey(tmp_vkey, act_buf, rif_buf, ((ScePopsCidBlock *)cid_buf)->drmFlags);
        if (res < 0) {
            if (res != (int)0x80000001 && res != (int)0x8001003E) {
                res = 0x80020148;
            }
            break;
        }
        memcpy(vkey, tmp_vkey, 16);
        if (content_id != NULL) {
            memcpy(content_id, cid_buf, 0x30);
        }
        res = popsSetVersionKey(path, tmp_vkey, (const char *)cid_buf);
        if (res < 0) {
            sceKernelMemset(vkey, 0, 16);
            break;
        }
        res = 0;
    } while (0);

    sceKernelMemset(work, 0, 0x1420);
    sceKernelDeleteFpl(fpl);
    return res;
}

/* 0x1aa0 */
static int popsVerifyRifCallback(void *arg)
{
    u8 rif_buf[0xA0];
    char rif_path[0x40];
    u32 stat_buf[24];
    int fileDesc, res;
    u32 flags = 0, t_lo = 0, t_hi = 0, e_lo = 0, e_hi = 0;

    res = popsBuildRifPath((const char *)arg, rif_path);
    if (res >= 0) {
        res = sceIoGetstat(rif_path, stat_buf);
        if (res >= 0) {
            if ((s64)(((u64)stat_buf[3] << 32) | stat_buf[2]) < 0x98) {
                res = 0x80010016;
            } else {
                fileDesc = sceIoOpen(rif_path, 1, 0);
                if (fileDesc >= 0) {
                    res = sceIoRead(fileDesc, rif_buf, 0xA0);
                    if (res >= 0 && res < 0x98) {
                        res = 0x80010016;
                    }
                    sceIoClose(fileDesc);
                    if (res >= 0) {
                        if (res < 0xA0) {
                            ((ScePopsRif *)rif_buf)->expireHi = 0;
                            ((ScePopsRif *)rif_buf)->expireLo = 0;
                        }
                        if (strncmp((const char *)(rif_buf + 0x10), (const char *)arg, 0x30) != 0) {
                            res = 0x80010016;
                        } else {
                            res = 0;
                        }
                    }
                } else {
                    res = fileDesc;
                }
            }
        }
    }
    if (res >= 0) {
        u32 startHiBe = ((ScePopsRif *)rif_buf)->startTimeHi;
        u32 startLoBe = ((ScePopsRif *)rif_buf)->startTimeLo;
        if (startHiBe | startLoBe) {
            u64 ts = (((u64)__builtin_bswap32(startHiBe) << 32) | __builtin_bswap32(startLoBe)) * 1000ULL + 0x00DCBFFEFF2BC000ULL;
            t_lo = (u32)ts;
            t_hi = (u32)(ts >> 32);
        }
        flags = __builtin_bswap32(((ScePopsRif *)rif_buf)->flags);
        e_lo = __builtin_bswap32(((ScePopsRif *)rif_buf)->expireLo);
        e_hi = __builtin_bswap32(((ScePopsRif *)rif_buf)->expireHi);
        res = sceNpDrmCheckRifTimeLimit(rif_buf);
    }
    popsInvokeNp9660Callback(res, flags, t_lo, t_hi, e_lo, e_hi);
    return res;
}

/* 0x1cb8 */
static void popsInvokeNp9660Callback(int res, u32 flags, u32 t_lo, u32 t_hi, u32 e_lo, u32 e_hi)
{
    if (g_np9660_cb != NULL) {
        u32 old_gp;
        __asm__ volatile ("move %0, $gp\n\tmove $gp, %1" : "=r"(old_gp) : "r"(g_np9660_gp));
        g_np9660_cb(res, flags, t_lo, t_hi, e_lo, e_hi);
        __asm__ volatile ("move $gp, %0" : : "r"(old_gp));
    }
}

/* 0x1d10 */
int sceNp9660_driver_8EF69DC6(void (*cb)(int, u32, u32, u32, u32, u32))
{
    int intr = sceKernelCpuSuspendIntr();
    u32 gp_val;
    __asm__ volatile ("move %0, $gp" : "=r"(gp_val));
    g_np9660_gp = gp_val;
    g_np9660_cb = cb;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x1d54 */
int sceNp9660_driver_B925CA6C(void)
{
    int intr = sceKernelCpuSuspendIntr();
    g_np9660_gp = 0;
    g_np9660_cb = NULL;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x1d8c */
int sceNp9660_driver_7A05EB3C(u32 *out)
{
    if (out != NULL) {
        *out = 0;
    }
    return 0;
}

/* 0x1d9c */
static int popsGetIdCallback(const char *path, void *vkey)
{
    return popsLoadLicenseKey(path, vkey, NULL);
}

/* 0x1db8 */
static int popsSysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
    (void)ev_name;
    (void)param;
    (void)result;
    if (ev_id == 0x00100000) {
        sceDisplaySetResumeMode(0);
        sceKernelPowerLock(0);
    } else if (ev_id == 0x00400000) {
        g_resume_flag = 0;
        if (g_resume_state != 0) {
            g_resume_state = 2;
        }
        if (g_resume_thid == 0) {
            int thid = sceKernelCreateThread("resumeProc", popsResumeThread, 0x10, 0x800, 0x00200000, NULL);
            g_resume_thid = thid;
            sceKernelStartThread(thid, 0, NULL);
        }
    }
    return 0;
}

/* 0x1e78 */
static int popsInitStub(void)
{
    return 0;
}

/* 0x1e80 */
static int popsTermStub(void)
{
    return 0;
}

/* 0x1e88 */
static int popsInitKermitPocs(void)
{
    int mtx;
    sceKernelMemset((void *)0xABD40000, 0, 0x8000);
    g_pocs_base1 = 0xABD40000;
    g_pocs_base2 = 0xABD44000;
    g_pocs_size = 0x4000;
    mtx = sceKernelCreateMutex("SceKermitPocs", 0x100, 0, NULL);
    g_pocs_mutex = mtx;
    return (mtx > 0) ? 0 : 0x80020001;
}

/* 0x1f08 */
int sceMeAudio_9FBE4AD3(int firstArg, int secondArg)
{
    return popsKermitSendTwoArgs(0xF03, firstArg, secondArg & 0xFF);
}

/* 0x1f30 */
int sceMeAudio_10DABACD(int firstArg)
{
    return popsKermitSendOneArg(0xF05, firstArg);
}

/* 0x1f50 */
int sceMeAudio_7AA3E14B(void)
{
    return popsKermitSendNoArg(0xF04);
}

/* 0x1f6c */
int sceMeAudio_C47D3670(void)
{
    return popsKermitSendNoArg(0xF06);
}

/* 0x1f88 */
int sceMeAudio_13099620(void)
{
    return popsKermitSendNoArg(0xF02);
}

/* 0x1fa4 */
static int popsKermitSendTwoArgs(int cmd_id, int firstArg, int secondArg)
{
    u8 buf[0x80];
    u64 resp;
    u32 k1;
    u32 *cmd = (u32 *)(((u32)buf + 0x3F) & ~0x3F);

    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");
    sceKernelDcacheInvalidateRangeForUser(cmd, 0x40);
    cmd = (u32 *)((u32)cmd | ((((s32)cmd >> 31) + 2) << 29));
    sceKernelLockMutex(g_pocs_mutex, 1, NULL);
    cmd[4] = firstArg;
    cmd[6] = secondArg;
    sceKermit_driver_36666181(cmd, 8, cmd_id, 2, 0, &resp);
    sceKernelUnlockMutex(g_pocs_mutex, 1);
    __asm__ volatile ("move $k1, %0" : : "r"(k1));
    return (int)resp;
}

/* 0x2070 */
static int popsKermitSendOneArg(int cmd_id, int firstArg)
{
    u8 buf[0x80];
    u64 resp;
    u32 k1;
    u32 *cmd = (u32 *)(((u32)buf + 0x3F) & ~0x3F);

    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");
    sceKernelDcacheInvalidateRangeForUser(cmd, 0x40);
    cmd = (u32 *)((u32)cmd | ((((s32)cmd >> 31) + 2) << 29));
    sceKernelLockMutex(g_pocs_mutex, 1, NULL);
    cmd[4] = firstArg;
    sceKermit_driver_36666181(cmd, 8, cmd_id, 1, 0, &resp);
    sceKernelUnlockMutex(g_pocs_mutex, 1);
    __asm__ volatile ("move $k1, %0" : : "r"(k1));
    return (int)resp;
}

/* 0x212c */
static int popsKermitSendNoArg(int cmd_id)
{
    u8 buf[0x80];
    u64 resp;
    u32 k1;
    u32 *cmd = (u32 *)(((u32)buf + 0x3F) & ~0x3F);

    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");
    sceKernelDcacheInvalidateRangeForUser(cmd, 0x40);
    cmd = (u32 *)((u32)cmd | ((((s32)cmd >> 31) + 2) << 29));
    sceKernelLockMutex(g_pocs_mutex, 1, NULL);
    sceKermit_driver_36666181(cmd, 8, cmd_id, 0, 0, &resp);
    sceKernelUnlockMutex(g_pocs_mutex, 1);
    __asm__ volatile ("move $k1, %0" : : "r"(k1));
    return (int)resp;
}

/* 0x21d8 */
int sceAudioOutputBlocking(void)
{
    return sceKernelDelayThread(10000);
}

/* 0x21f4 */
void sceWlanDrv_driver_EDD207B1(void)
{
}

/* 0x21fc */
int sceMeAudio_D96DC042(int qid, u32 addr)
{
    int intr;
    u32 *uncached;
    u32 c0_val;
    int i, res;

    __asm__ volatile (
        ".word 0x70040024\n\t" /* mfic $4, $0 */
        ".word 0x70000026\n\t" /* mtic $0, $0 */
        "move %0, $4"
        : "=r"(intr) : : "$4"
    );
    addr &= 0x1FFFFFFF;
    uncached = (u32 *)(addr | 0x40000000);
    uncached[-1] = 0x0F000000;
    *(volatile u32 *)0xBD40010C = addr;
    __asm__ volatile ("mfc0 %0, $14" : "=r"(c0_val));
    for (i = 20; i > 0; i--) {
        __asm__ volatile ("cache 0x0a, 0x40(%0)" : : "r"(c0_val));
        if ((*(volatile u32 *)0xBD400304) & 4) {
            *(volatile u32 *)0xBD400310 = 4;
            sceKernelCpuResumeIntrWithSync(intr);
            return qid;
        }
        c0_val += 0x40;
    }
    uncached[0] = 0x0C000000;
    *(volatile u32 *)0xBD40010C = 0;
    sceKernelCpuResumeIntrWithSync(intr);
    res = sceGeListEnQueue(uncached, uncached, -1, NULL);
    sceGeListSync(qid, 0);
    return res;
}

/* 0x22c8 */
void sceMeAudio_899A3BC0(u32 addr)
{
    *(volatile u32 *)0xBD40010C = addr & 0x1FFFFFFF;
}
