#include <common_imp.h>

static inline __attribute__((always_inline)) int get_k1(void)
{
    int k1;
    __asm__ volatile("move %0, $k1" : "=r"(k1));
    return k1;
}

static inline __attribute__((always_inline)) void set_k1(int k1)
{
    __asm__ volatile("move $k1, %0" : : "r"(k1));
}

extern int Kprintf(const char *fmt, ...);
extern int sceKernelGetCompiledSdkVersion(void);
extern int sceKernelSetEventFlag(int evid, u32 bits);
extern int sceKernelDeleteSema(int semaid);
extern int sceKernelPollEventFlag(int evid, u32 bits, int wait, u32 *outBits);
extern int sceKernelSignalSema(int semaid, int signal);
extern int sceKernelWaitSema(int semaid, int signal, u32 *timeout);
extern int sceKernelCreateEventFlag(const char *name, int attr, int bits, void *opt);
extern int sceKernelUnlockMutex(int mutexid, int count);
extern int sceKernelLockMutex(int mutexid, int count, u32 *timeout);
extern int sceKernelCreateMutex(const char *name, int attr, int initCount, void *opt);
extern int sceKernelDelayThread(u32 delay);
extern int sceKernelCreateSema(const char *name, int attr, int initVal, int maxVal, void *opt);
extern int sceKernelDeleteEventFlag(int evid);
extern int sceKernelDeleteMutex(int mutexid);
extern int sceKernelDcacheWritebackInvalidateRange(const void *p, unsigned int size);
extern int sceKernelDcacheWritebackRange(const void *p, unsigned int size);
extern int sceKernelDcacheInvalidateRangeForUser(const void *p, unsigned int size);

extern int sceDmacplus_driver_282CA0D7(void *lli);
extern int sceDmacplus_driver_47D400CB(int flags, int timeout);
extern int sceDmacplus_driver_5FCF43BD(int flags, int timeout);
extern int sceDmacplus_driver_6945F1D3(void *lli);
extern int sceDmacplus_driver_9C492B9B(void *rgbEven, void *rgbOdd, void *yuvDesc);
extern int sceDmacplus_driver_B269EAC9(int mode, int width, const void *coeffs);
extern int sceDmacplus_driver_BF0DB45E(int flags, int timeout);
extern int sceDmacplus_driver_D126494B(void *rgbEven, void *rgbOdd, void *yuvDesc);
extern int sceDmacplus_driver_F652F09D(u32 startY, u32 startX, u32 height, u32 width);

extern int sceMeAudio_driver_6AD33F60(int codecType, void *info);
extern int sceMeAudio_driver_81956A0B(int codecType, void *workArea);
extern int sceMeAudio_driver_9A9E21EE(int codecType, void *workArea);
extern int sceMeAudio_driver_B57F033A(int codecType, void *buf);
extern int sceMeAudio_driver_C300D466(int codecType, int size, void *info);

extern int sceMeCore_driver_635397BB(int cmd, ...);
extern int sceMeCore_driver_905A7500(void);
extern int sceMeCore_driver_9EB2943B(int cmd);
extern int sceMeCore_driver_FA398D71(int cmd, ...);

extern void sceMeFree(void *ptr);
extern void *sceMeMalloc(u32 size);
extern void *sceMeCalloc(u32 num, u32 size);

extern int sceMePower_driver_984E2608(int arg);
extern int sceMePowerControlAvcPower(int arg);

extern int sceMeVideo_driver_21521BE5(void *workArea);
extern int sceMeVideo_driver_4D78330C(int type, void *arg);
extern int sceMeVideo_driver_6D68B223(int type, int cmd, void *arg);
extern int sceMeVideo_driver_8768915D(int type, void *buf);
extern int sceMeVideo_driver_8DD56014(int type, void *arg);
extern int sceMeVideo_driver_C441994C(int type, void *buf);
extern int sceMeVideo_driver_E8CD3C75(int type, void *arg);

extern u32 sceMeCore_driver_var_1B960AB4;
extern u32 sceMeCore_driver_var_61FF4837;
extern u32 sceMeCore_driver_var_AB354CA1;

/* .rodata tables */
static const int g_audioRetValues[5] = {
    0x807F00FC,
    0x807F00FD,
    0x80000002,
    0x807F00FF,
    0x807F0001
};

static const int g_videoRetValues[5] = {
    0x80628002,
    0x80628001,
    0x80620002,
    0x806201FE,
    0x00000000
};

static const u8 g_mp3Bitrates[2][15] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 18, 20 },
    { 0, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40 }
};

static const u16 g_mp3Freqs[3][4] = {
    { 22050, 24000, 16000, 0 },
    { 44100, 48000, 32000, 0 },
    { 11025, 12000, 8000,  0 }
};

static const u8 g_cscCoeffsJfif[24] = {
    0x80, 0x00, 0x00, 0x00, 0xB3, 0x00, 0x80, 0x00,
    0xD4, 0x03, 0xA5, 0x03, 0x80, 0x00, 0xE3, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00
};

static const u8 g_cscCoeffsBt601[24] = {
    0x95, 0x00, 0x00, 0x00, 0xCC, 0x00, 0x95, 0x00,
    0xCE, 0x03, 0x98, 0x03, 0x95, 0x00, 0x02, 0x01,
    0x00, 0x00, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00
};

typedef struct {
    int sema;
    int width;
    int height;
    int state;
} SceJpegCtx;

static SceJpegCtx g_jpegCtx;
static u32 g_jpegMeBuf[28];

typedef struct {
    u32 src;
    u32 dst;
    u32 next;
    u32 ctrl;
} SceMpegLli;

typedef struct {
    SceMpegLli lli[100];      /* 0x000..0x640 */
    u32 pad[4];               /* 0x640..0x650 */
    u8 coeffs[22];            /* 0x650..0x666 */
    s16 cscMode;              /* 0x666 */
    u32 cscWidth;             /* 0x668 */
    int cscMutex;             /* 0x66C */
    int lliSema;              /* 0x670 */
    int cscEvf;               /* 0x674 */
} SceMpegBaseCtx;

static SceMpegBaseCtx g_mpegCtx;

/* 0x0000 */
static int mpegBaseInitSync(void)
{
    g_mpegCtx.lliSema = sceKernelCreateSema("SceMpegBaseLLI", 1, 1, 1, NULL);
    g_mpegCtx.cscMutex = sceKernelCreateMutex("SceMpegBaseCSC", 0x201, 0, NULL);
    g_mpegCtx.cscWidth = 0;
    g_mpegCtx.cscMode = 0;
    g_mpegCtx.cscEvf = sceKernelCreateEventFlag("SceMpegBaseCSC", 0x201, 0, NULL);
    return 0;
}

/* 0x0094 */
static int mpegBaseTermSync(void)
{
    sceKernelDeleteEventFlag(g_mpegCtx.cscEvf);
    sceKernelDeleteMutex(g_mpegCtx.cscMutex);
    sceKernelDeleteSema(g_mpegCtx.lliSema);
    return 0;
}

/* 0x00D8 */
int sceMpegbase_119D5DDA(int cmd)
{
    return sceMeCore_driver_9EB2943B(cmd);
}

/* 0x00F4 */
int sceMpegbase_31007FF8(void)
{
    return sceMeCore_driver_var_AB354CA1;
}

/* === sceAudiocodec === */
static int audiocodecGetNeedMem(u32 *workArea, int codecType);
static int audiocodecGetWorkSize(u32 *workArea, int codecType);
static int mp3CalcFrameSize(u32 ver, u32 layer, int bitrateIdx, int sfIdx, u32 *outSize);

/* 0x0100 */
int sceAudiocodecCheckNeedMem(u32 *workArea, int codecType)
{
    if (((0x220202 >> ((u32)workArea >> 27)) & 1) == 0)
        return 0x807F0002;
    if ((u32)(codecType - 0x1000) >= 6)
        return 0x80000004;

    int k1 = get_k1();
    set_k1(k1 << 11);
    int res;
    if ((int)(((u32)workArea | ((u32)workArea + 0x68)) & (k1 << 11)) < 0) {
        res = 0x80000023;
    } else {
        workArea[0] = 0x05100601;
        int ret = sceMeAudio_driver_81956A0B(codecType, workArea);
        if (ret == 0) {
            res = 0;
        } else {
            if ((u32)ret < (u32)-4)
                ret = 0;
            res = g_audioRetValues[ret + 4];
        }
    }
    set_k1(k1);
    return res;
}

/* 0x01CC */
int sceAudiocodecInit(u32 *workArea, int codecType)
{
    if (((0x220202 >> ((u32)workArea >> 27)) & 1) == 0)
        return 0x807F0002;
    if ((u32)(codecType - 0x1000) >= 6)
        return 0x80000004;

    int k1 = get_k1();
    set_k1(k1 << 11);
    if ((int)(((u32)workArea | ((u32)workArea + 0x68)) & (k1 << 11)) < 0) {
        set_k1(k1);
        return 0x80000023;
    }
    u32 edram = workArea[3];
    u32 needed = workArea[4];
    if ((int)((edram | (edram + needed) | needed) & (k1 << 11)) < 0) {
        set_k1(k1);
        return 0x80000023;
    }
    if (codecType == 0x1002)
        workArea[14] = 0x270F;
    if ((0x220202 >> (edram >> 27)) & 1)
        sceKernelDcacheWritebackInvalidateRange((void *)edram, needed);
    workArea[0] = 0x05100601;
    int ret = sceMeAudio_driver_6AD33F60(codecType, workArea);
    int res = 0;
    if (ret != 0) {
        if ((u32)ret < (u32)-4)
            ret = 0;
        res = g_audioRetValues[ret + 4];
    }
    set_k1(k1);
    return res;
}

/* 0x02FC */
int sceAudiocodec_3DD7EE1A(u32 *workArea, int codecType)
{
    if (((0x220202 >> ((u32)workArea >> 27)) & 1) == 0)
        return 0x807F0002;
    if ((u32)(codecType - 0x1000) >= 6)
        return 0x80000004;

    int k1 = get_k1();
    set_k1(k1 << 11);
    if ((int)(((u32)workArea | ((u32)workArea + 0x68)) & (k1 << 11)) < 0) {
        set_k1(k1);
        return 0x80000023;
    }
    u32 edram = workArea[3];
    u32 needed = workArea[4];
    if ((int)((edram | (edram + needed) | needed) & (k1 << 11)) < 0) {
        set_k1(k1);
        return 0x80000023;
    }
    if (codecType == 0x1002)
        workArea[14] = 0x270F;
    if ((0x220202 >> (edram >> 27)) & 1)
        sceKernelDcacheWritebackInvalidateRange((void *)edram, needed);
    workArea[0] = 0x05100601;
    int ret = sceMeAudio_driver_B57F033A(codecType, workArea);
    int res = 0;
    if (ret != 0) {
        if ((u32)ret < (u32)-4)
            ret = 0;
        res = g_audioRetValues[ret + 4];
    }
    set_k1(k1);
    return res;
}

/* 0x042C */
int sceAudiocodecDecode(u32 *workArea, int codecType)
{
    if (((0x220202 >> ((u32)workArea >> 27)) & 1) == 0)
        return 0x807F0002;
    if ((u32)(codecType - 0x1000) >= 6)
        return 0x80000004;

    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = 0x80000023;
    if ((int)(((u32)workArea | ((u32)workArea + 0x68)) & (k1 << 11)) >= 0) {
        u32 edram = workArea[3];
        u32 needed = workArea[4];
        if ((int)((edram | (edram + needed) | needed) & (k1 << 11)) >= 0) {
            u32 inBuf = workArea[6];
            if ((int)((inBuf | (inBuf + 0x10000)) & (k1 << 11)) >= 0) {
                u32 outBuf = workArea[8];
                if ((int)((outBuf | (outBuf + 0x10000)) & (k1 << 11)) >= 0) {
                    int inSize;
                    if (codecType == 0x1002 || codecType == 0x1004)
                        inSize = (int)workArea[10];
                    else
                        inSize = audiocodecGetNeedMem(workArea, codecType);

                    if (inSize < 0) {
                        res = inSize;
                    } else {
                        sceKernelDcacheWritebackRange((void *)workArea[6], inSize);
                        int outSize = audiocodecGetWorkSize(workArea, codecType);
                        if (outSize < 0) {
                            res = outSize;
                        } else {
                            sceKernelDcacheWritebackInvalidateRange((void *)workArea[8], outSize);
                            int ret = sceMeAudio_driver_9A9E21EE(codecType, workArea);
                            if (ret == 0) {
                                res = 0;
                            } else {
                                if ((u32)ret < (u32)-4)
                                    ret = 0;
                                res = g_audioRetValues[ret + 4];
                            }
                        }
                    }
                }
            }
        }
    }
    if (sceMeCore_driver_var_1B960AB4 != 0)
        sceKernelDelayThread(sceMeCore_driver_var_1B960AB4);
    set_k1(k1);
    return res;
}

/* 0x05D4 */
int sceAudiocodecGetInfo(u32 *workArea, int codecType)
{
    if (((0x220202 >> ((u32)workArea >> 27)) & 1) == 0)
        return 0x807F0002;
    if ((u32)(codecType - 0x1000) >= 6)
        return 0x80000004;

    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = 0x80000023;
    if ((int)(((u32)workArea | ((u32)workArea + 0x68)) & (k1 << 11)) >= 0) {
        u32 edram = workArea[3];
        u32 needed = workArea[4];
        if ((int)((edram | (edram + needed) | needed) & (k1 << 11)) >= 0) {
            if (codecType == 0x1002 || codecType == 0x1004) {
                u32 inBuf = workArea[6];
                u32 inSize = workArea[10];
                if ((int)((inBuf | (inBuf + inSize) | inSize) & (k1 << 11)) < 0) {
                    set_k1(k1);
                    return 0x80000023;
                }
            }
            int sz = 0;
            if (codecType == 0x1002 || codecType == 0x1004) {
                sz = 2;
            } else if (codecType == 0x1000) {
                sz = 4;
            } else {
                set_k1(k1);
                return 0;
            }
            workArea[0] = 0x05100601;
            int ret = sceMeAudio_driver_C300D466(codecType, sz, workArea);
            if (ret == 0) {
                res = 0;
            } else {
                if ((u32)ret < (u32)-4)
                    ret = 0;
                res = g_audioRetValues[ret + 4];
            }
        }
    }
    set_k1(k1);
    return res;
}

/* 0x073C */
int sceAudiocodecAlcExtendParameter(u32 *workArea, int codecType, int *sizeAddr)
{
    if (((0x220202 >> ((u32)workArea >> 27)) & 1) == 0)
        return 0x807F0002;
    if ((u32)(codecType - 0x1000) >= 6)
        return 0x80000004;

    int k1 = get_k1();
    set_k1(k1 << 11);
    if ((int)((((u32)workArea | ((u32)workArea + 0x68)) | ((u32)sizeAddr | ((u32)sizeAddr + 4))) & (k1 << 11)) < 0) {
        set_k1(k1);
        return 0x80000023;
    }
    int res = audiocodecGetWorkSize(workArea, codecType);
    if (res >= 0) {
        *sizeAddr = res;
        res = 0;
    }
    set_k1(k1);
    return res;
}

/* 0x07F8 */
int sceAudiocodecGetEDRAM(u32 *workArea, int codecType)
{
    if (((0x220202 >> ((u32)workArea >> 27)) & 1) == 0)
        return 0x807F0002;
    if ((u32)(codecType - 0x1000) >= 6)
        return 0x80000004;

    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = 0;
    if ((int)(((u32)workArea | ((u32)workArea + 0x6C)) & (k1 << 11)) < 0) {
        res = 0x80000023;
    } else {
        void *p = sceMeMalloc((workArea[4] + 0x3F) | 0x3F);
        workArea[26] = (u32)p;
        if (p != NULL && (((u32)p & 0x1FFFFFFF) <= 0x3FFFFF)) {
            workArea[3] = ((u32)p + 0x3F) & ~0x3F;
        } else {
            sceMeFree((void *)workArea[26]);
            workArea[26] = 0;
            workArea[3] = 0;
            res = 0x807F0003;
        }
    }
    set_k1(k1);
    return res;
}

/* 0x08F4 */
int sceAudiocodecReleaseEDRAM(u32 *workArea)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = 0x80000023;
    if ((int)(((u32)workArea | ((u32)workArea + 0x6C)) & (k1 << 11)) >= 0) {
        u32 edram = workArea[3];
        u32 needed = workArea[4];
        if ((int)((edram | (edram + needed) | needed) & (k1 << 11)) >= 0) {
            u32 raw = workArea[26];
            res = 0x807F0004;
            if (raw != 0 && ((raw & 0x1FFFFFFF) <= 0x3FFFFF)) {
                sceMeFree((void *)raw);
                workArea[26] = 0;
                workArea[3] = 0;
                res = 0;
            }
        }
    }
    set_k1(k1);
    return res;
}

/* 0x09AC */
static int audiocodecGetNeedMem(u32 *workArea, int codecType)
{
    switch (codecType - 0x1000) {
    case 0:
        if (workArea[12] != 0)
            return 0x100A;
        return (int)workArea[16] + 2;
    case 1:
        return ((int)workArea[12] << 1) + 1;
    case 2:
        if (workArea[14] == 0x270F) {
            int res = sceAudiocodecGetInfo(workArea, 0x1002);
            if (res < 0)
                return res;
        }
        return mp3CalcFrameSize(workArea[14], workArea[15], workArea[17], workArea[18], &workArea[10]);
    case 3:
        return (((u8 *)workArea)[0x2C] != 0) ? 0x609 : 0x600;
    case 4:
        return mp3CalcFrameSize(workArea[12], workArea[13], workArea[15], workArea[16], &workArea[10]);
    case 5:
        return 0x420D;
    default:
        return 0x80000004;
    }
}

/* 0x0A9C */
static int audiocodecGetWorkSize(u32 *workArea, int codecType)
{
    switch (codecType - 0x1000) {
    case 0: {
        u32 sampleRateFlag = workArea[14];
        u32 channels = workArea[18];
        if (sampleRateFlag == 1 && channels != 1)
            return 0x2000;
        return (int)(channels << 12);
    }
    case 1:
        return 0x1000;
    case 2: {
        u32 mpegVer = workArea[14];
        if (mpegVer == 0x270F) {
            int res = sceAudiocodecGetInfo(workArea, 0x1002);
            if (res < 0)
                return res;
            mpegVer = workArea[14];
        }
        return (mpegVer == 1) ? 0x1200 : 0x900;
    }
    case 3:
        return (((u8 *)workArea)[0x2D] == 0) ? 0x1000 : 0x2000;
    case 4:
        return 0x1200;
    case 5:
        return 0x2F00;
    default:
        return 0x1000;
    }
}

/* 0x0B74 */
static int mp3CalcFrameSize(u32 ver, u32 layer, int bitrateIdx, int sfIdx, u32 *outSize)
{
    u32 br = 0;
    if (ver < 3 && bitrateIdx >= 0 && bitrateIdx < 15)
        br = (u32)g_mp3Bitrates[ver & 1][bitrateIdx] << 3;

    u32 freq = 0;
    if (ver < 3 && sfIdx >= 0 && sfIdx < 4)
        freq = g_mp3Freqs[ver][sfIdx];

    u32 sz = 0x7C9;
    if (br > 0 && freq > 0 && (u32)(layer - 1) < 2) {
        sz = ((br * 9) << 4) / freq + 1;
        if ((int)sz >= 0x7CA)
            sz = 0x7C9;
    }
    if (ver != 1)
        sz = (int)sz >> 1;
    *outSize = sz;
    return (int)sz;
}

/* === sceVideocodec === */
static int videocodecCheckWorkArea(u32 *workArea, u32 type);
static int videocodecCheckWorkAreaBuffers(u32 *workArea, u32 type);
static int videocodecCheckDecodeBuffers(u32 *workArea, u32 type);
static void videocodecFlushDecodeCache(u32 *workArea, u32 type);
static void videocodecInvalidateOutputCache(u32 *workArea, u32 type);
static int videocodecCheckInitBuffers(u32 *workArea, u32 type);
static void videocodecFlushInitCache(u32 *workArea, u32 type);
static int videocodecCheckWorkAreaWithCtrl(u32 *workArea, u32 type);
static int videocodecCalcFrameBufSizes(u32 *workArea, u32 *sizes);

/* 0x0C54 */
int sceVideocodecGetEDRAM(u32 *workArea, int type)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = 0x80000023;
    if ((int)(((u32)workArea | ((u32)workArea + 0x60)) & (k1 << 11)) >= 0) {
        res = 0x806201FE;
        if (workArea[23] == 0) {
            void *p = sceMeCalloc((workArea[6] + 0x3F) | 0x3F, 1);
            workArea[23] = (u32)p;
            if (p != NULL) {
                workArea[5] = ((u32)p + 0x3F) & ~0x3F;
                res = 0;
            }
        }
    }
    set_k1(k1);
    return res;
}

/* 0x0CF4 */
int sceVideocodecReleaseEDRAM(u32 *workArea)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = 0x80000023;
    if ((int)(((u32)workArea | ((u32)workArea + 0x60)) & (k1 << 11)) >= 0) {
        res = 0x806201FE;
        if (workArea[23] != 0) {
            sceMeFree((void *)workArea[23]);
            workArea[5] = 0;
            workArea[23] = 0;
            res = 0;
        }
    }
    set_k1(k1);
    return res;
}

/* 0x0D74 */
int sceVideocodecOpen(u32 *workArea, int type)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = videocodecCheckWorkArea(workArea, type);
    if (res == 0) {
        workArea[0] = 0x05100601;
        sceKernelDcacheWritebackInvalidateRange((void *)workArea[4], (type == 0) ? 0x28 : 0x100);
        res = sceMeVideo_driver_C441994C(type, workArea);
        if (res != 0) {
            sceMePower_driver_984E2608(0);
            sceMePowerControlAvcPower(0);
            int idx = ((u32)(res - 1) < (u32)-5) ? -4 : res;
            res = g_videoRetValues[idx + 4];
        }
    }
    set_k1(k1);
    return res;
}

/* 0x0E3C */
int sceVideocodecInit(u32 *workArea, int type)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = videocodecCheckWorkAreaBuffers(workArea, type);
    if (res == 0) {
        sceKernelDcacheWritebackInvalidateRange((void *)workArea[4], (type == 0) ? 0x28 : 0x100);
        if ((workArea[5] & 0x1FFFFFFF) > 0x3FFFFF)
            sceKernelDcacheWritebackInvalidateRange((void *)workArea[5], workArea[6]);
        sceKernelDcacheWritebackInvalidateRange((void *)workArea[7], workArea[8]);
        int ret = sceMeVideo_driver_E8CD3C75(type, workArea);
        if ((u32)(ret - 1) < (u32)-5)
            ret = -4;
        res = g_videoRetValues[ret + 4];
        if (res >= 0)
            sceKernelSetEventFlag(g_mpegCtx.cscEvf, 1);
    }
    set_k1(k1);
    return res;
}

/* 0x0F28 */
int sceVideocodecSetMemory(u32 *workArea, int type)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = videocodecCheckWorkArea(workArea, type);
    if (res >= 0 && (int)(workArea[3] & (k1 << 11)) < 0) {
        set_k1(k1);
        return 0x80000023;
    }
    if (res == 0 && type == 0) {
        sceKernelDcacheWritebackInvalidateRange((void *)workArea[4], 0x28);
        int ret = sceMeVideo_driver_6D68B223(0, 1, workArea);
        if ((u32)(ret - 1) < (u32)-5)
            ret = -4;
        res = g_videoRetValues[ret + 4];
    }
    set_k1(k1);
    return res;
}

/* 0x0FDC */
int sceVideocodecScanHeader(u32 *workArea, int type)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = 0x80000023;
    if ((int)(((u32)workArea | ((u32)workArea + 0x60)) & (k1 << 11)) >= 0) {
        u32 infoBuf = workArea[4];
        if ((int)((infoBuf | (infoBuf + 0x100)) & (k1 << 11)) >= 0) {
            u32 streamBuf = workArea[9];
            u32 streamSize = workArea[10];
            if ((int)((streamBuf | (streamBuf + streamSize) | streamSize) & (k1 << 11)) >= 0 &&
                (int)(workArea[3] & (k1 << 11)) >= 0) {
                res = 0x806201FE;
                if ((u32)(type - 1) < 2) {
                    sceKernelDcacheWritebackInvalidateRange((void *)infoBuf, 0x100);
                    if ((workArea[9] & 0x1FFFFFFF) > 0x3FFFFF)
                        sceKernelDcacheWritebackInvalidateRange((void *)workArea[9], workArea[10]);
                    int ret = sceMeVideo_driver_6D68B223(type, 2, workArea);
                    if ((u32)(ret - 1) < (u32)-5)
                        ret = -4;
                    res = g_videoRetValues[ret + 4];
                }
            }
        }
    }
    set_k1(k1);
    return res;
}

/* 0x110C */
int sceVideocodecGetVersion(u32 *workArea, int type)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = 0x80000023;
    if ((int)(((u32)workArea | ((u32)workArea + 0x60)) & (k1 << 11)) >= 0) {
        res = 0x806201FE;
        if ((u32)type < 3) {
            int ret = sceMeVideo_driver_6D68B223(type, 3, workArea);
            if ((u32)(ret - 1) < (u32)-5)
                ret = -4;
            res = g_videoRetValues[ret + 4];
        }
    }
    set_k1(k1);
    return res;
}

/* 0x119C */
int sceVideocodecGetSEI(u32 *workArea, int type)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = 0x806201FE;
    if (type == 0) {
        res = videocodecCheckWorkArea(workArea, type);
        if (res == 0) {
            u32 seiBuf = workArea[20];
            if ((int)((seiBuf | (seiBuf + 0xA4)) & (k1 << 11)) < 0 ||
                (int)(workArea[3] & (k1 << 11)) < 0)
                res = 0x80000023;
        }
        if (res == 0) {
            sceKernelDcacheWritebackInvalidateRange((void *)workArea[4], (type == 0) ? 0x28 : 0x100);
            sceKernelDcacheInvalidateRangeForUser((void *)workArea[20], 0xC0);
            int ret = sceMeVideo_driver_6D68B223(type, 5, workArea);
            if ((u32)(ret - 1) < (u32)-5)
                ret = -4;
            res = g_videoRetValues[ret + 4];
        }
    }
    set_k1(k1);
    return res;
}

/* 0x1280 */
int sceVideocodecGetFrameCrop(u32 *workArea, int type)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = 0x806201FE;
    if (type == 0) {
        res = videocodecCheckWorkArea(workArea, type);
        if (res == 0) {
            u32 cropBuf = workArea[21];
            if ((int)((cropBuf | (cropBuf + 0x10)) & (k1 << 11)) < 0 ||
                (int)(workArea[3] & (k1 << 11)) < 0)
                res = 0x80000023;
        }
        if (res == 0) {
            sceKernelDcacheWritebackInvalidateRange((void *)workArea[4], (type == 0) ? 0x28 : 0x100);
            sceKernelDcacheInvalidateRangeForUser((void *)workArea[21], 0x40);
            int ret = sceMeVideo_driver_6D68B223(type, 6, workArea);
            if ((u32)(ret - 1) < (u32)-5)
                ret = -4;
            res = g_videoRetValues[ret + 4];
        }
    }
    set_k1(k1);
    return res;
}

/* 0x1364 */
int sceVideocodec_893B32B1(u32 *workArea, int type)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = 0x806201FE;
    if (type == 0) {
        res = videocodecCheckWorkArea(workArea, type);
        if (res >= 0 && (int)(workArea[3] & (k1 << 11)) < 0) {
            set_k1(k1);
            return 0x80000023;
        }
        if (res == 0) {
            sceKernelDcacheWritebackInvalidateRange((void *)workArea[4], (type == 0) ? 0x28 : 0x100);
            int ret = sceMeVideo_driver_6D68B223(type, 7, workArea);
            if ((u32)(ret - 1) < (u32)-5)
                ret = -4;
            res = g_videoRetValues[ret + 4];
        }
    }
    set_k1(k1);
    return res;
}

/* 0x1428 */
static int videocodecPowerOff(void)
{
    sceMePower_driver_984E2608(0);
    sceMePowerControlAvcPower(0);
    return 0;
}

/* 0x1450 */
int sceVideocodecDecode(u32 *workArea, int type)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = videocodecCheckDecodeBuffers(workArea, type);
    if (res == 0) {
        videocodecFlushDecodeCache(workArea, type);
        videocodecInvalidateOutputCache(workArea, type);
        int ret = sceMeVideo_driver_8768915D(type, workArea);
        if ((u32)(ret - 1) < (u32)-5)
            ret = -4;
        res = g_videoRetValues[ret + 4];
    }
    set_k1(k1);
    return res;
}

/* 0x14E4 */
int sceVideocodecStop(u32 *workArea, int type)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = videocodecCheckInitBuffers(workArea, type);
    if (res == 0) {
        videocodecFlushInitCache(workArea, type);
        videocodecInvalidateOutputCache(workArea, type);
        int ret = sceMeVideo_driver_4D78330C(type, workArea);
        if ((u32)(ret - 1) < (u32)-5)
            ret = -4;
        res = g_videoRetValues[ret + 4];
    }
    set_k1(k1);
    return res;
}

/* 0x1578 */
int sceVideocodecDelete(u32 *workArea, int type)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = (videocodecCheckWorkAreaWithCtrl(workArea, type) != 0);
    if (res == 0) {
        sceKernelDcacheWritebackInvalidateRange((void *)workArea[4], (type == 0) ? 0x28 : 0x100);
        sceKernelPollEventFlag(g_mpegCtx.cscEvf, 1, 0x21, NULL);
        int ret = sceMeVideo_driver_8DD56014(type, workArea);
        if ((u32)(ret - 1) < (u32)-5)
            ret = -4;
        res = g_videoRetValues[ret + 4];
    }
    set_k1(k1);
    return res;
}

/* 0x1620 */
int sceVideocodec_D95C24D5(u32 *workArea)
{
    u32 sizes[6];
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = videocodecCalcFrameBufSizes(workArea, sizes);
    if (res == 0) {
        sceKernelDcacheWritebackInvalidateRange((void *)workArea[3], sizes[0]);
        sceKernelDcacheWritebackInvalidateRange((void *)workArea[4], sizes[0]);
        sceKernelDcacheWritebackInvalidateRange((void *)workArea[7], sizes[1]);
        sceKernelDcacheWritebackInvalidateRange((void *)workArea[8], sizes[1]);
        sceKernelDcacheWritebackInvalidateRange((void *)workArea[5], sizes[2]);
        sceKernelDcacheWritebackInvalidateRange((void *)workArea[6], sizes[2]);
        sceKernelDcacheWritebackInvalidateRange((void *)workArea[9], sizes[3]);
        sceKernelDcacheWritebackInvalidateRange((void *)workArea[10], sizes[3]);
        sceKernelDcacheInvalidateRangeForUser((void *)workArea[11], (sizes[4] + 0x3F) & ~0x3F);
        sceKernelDcacheInvalidateRangeForUser((void *)workArea[12], (sizes[5] + 0x3F) & ~0x3F);
        sceKernelDcacheInvalidateRangeForUser((void *)workArea[13], (sizes[5] + 0x3F) & ~0x3F);
        sceKernelDcacheWritebackInvalidateRange(workArea, 0x38);
        int ret = sceMeVideo_driver_21521BE5(workArea);
        if ((u32)(ret - 1) < (u32)-5)
            ret = -4;
        res = g_videoRetValues[ret + 4];
    }
    set_k1(k1);
    return res;
}

/* 0x1738 */
static int videocodecCheckWorkArea(u32 *workArea, u32 type)
{
    int k1 = get_k1();
    if ((int)(((u32)workArea | ((u32)workArea + 0x60)) & k1) < 0)
        return 0x80000023;
    u32 sz = 0x28;
    if (type != 0) {
        if (type >= 3)
            return 0x806201FE;
        sz = 0x100;
    }
    u32 infoBuf = workArea[4];
    if ((int)((infoBuf | (infoBuf + sz) | sz) & k1) < 0)
        return 0x80000023;
    return 0;
}

/* 0x179C */
static int videocodecCheckWorkAreaBuffers(u32 *workArea, u32 type)
{
    int k1 = get_k1();
    if ((int)(((u32)workArea | ((u32)workArea + 0x60)) & k1) < 0)
        return 0x80000023;
    u32 edramBuf = workArea[5], edramSize = workArea[6];
    if ((int)((edramBuf | (edramBuf + edramSize) | edramSize) & k1) < 0)
        return 0x80000023;
    u32 workBuf = workArea[7], workSize = workArea[8];
    if ((int)((workBuf | (workBuf + workSize) | workSize) & k1) < 0)
        return 0x80000023;
    u32 sz = 0x28;
    if (type != 0) {
        if (type >= 3)
            return 0x806201FE;
        sz = 0x100;
    }
    u32 infoBuf = workArea[4];
    if ((int)((infoBuf | (infoBuf + sz) | sz) & k1) < 0)
        return 0x80000023;
    return 0;
}

/* 0x1840 */
static int videocodecCheckDecodeBuffers(u32 *workArea, u32 type)
{
    int k1 = get_k1();
    if ((int)(((u32)workArea | ((u32)workArea + 0x60)) & k1) < 0)
        return 0x80000023;
    if ((int)(workArea[3] & k1) < 0)
        return 0x80000023;
    u32 edramBuf = workArea[5], edramSize = workArea[6];
    if ((int)((edramBuf | (edramBuf + edramSize) | edramSize) & k1) < 0)
        return 0x80000023;
    u32 workBuf = workArea[7], workSize = workArea[8];
    if ((int)((workBuf | (workBuf + workSize) | workSize) & k1) < 0)
        return 0x80000023;
    u32 streamBuf = workArea[9], streamSize = workArea[10];
    if ((int)((streamBuf | (streamBuf + streamSize) | streamSize) & k1) < 0)
        return 0x80000023;

    u32 sz = 0x28;
    if (type != 0) {
        if (type >= 3)
            return 0x806201FE;
        sz = 0x100;
    }
    u32 infoBuf = workArea[4];
    if ((int)((infoBuf | (infoBuf + sz)) & k1) < 0)
        return 0x80000023;
    if (type != 0)
        return 0;

    u32 count = workArea[15];
    u32 frameInfoBuf = workArea[11];
    u32 frameInfoSize = count * 0x2C;
    if ((int)((frameInfoBuf | (frameInfoBuf + frameInfoSize) | frameInfoSize) & k1) < 0)
        return 0x80000023;
    u32 picInfoBuf = workArea[12];
    if ((int)((picInfoBuf | (picInfoBuf + 0x64)) & k1) < 0)
        return 0x80000023;

    u32 extBuf = workArea[14];
    if (extBuf != 0) {
        u32 extBufSize = count * 0x148;
        if ((int)((extBuf | (extBuf + extBufSize) | extBufSize) & k1) < 0)
            return 0x80000023;
    }
    int flags = (int)workArea[13];
    if (flags < 0) {
        u32 seiBuf = workArea[20];
        if (seiBuf != 0 && (int)((seiBuf | (seiBuf + 0xA4)) & k1) < 0)
            return 0x80000023;
    }
    if (flags & 0x40000000) {
        u32 cropBuf = workArea[21];
        if (cropBuf != 0 && (int)((cropBuf | (cropBuf + 0x10)) & k1) < 0)
            return 0x80000023;
    }
    return 0;
}

/* 0x19FC */
static void videocodecFlushDecodeCache(u32 *workArea, u32 type)
{
    if ((workArea[9] & 0x1FFFFFFF) > 0x3FFFFF)
        sceKernelDcacheWritebackInvalidateRange((void *)workArea[9], workArea[10]);
    u32 sz = 0x28;
    if (type != 0) {
        if ((workArea[5] & 0x1FFFFFFF) > 0x3FFFFF)
            sceKernelDcacheWritebackInvalidateRange((void *)workArea[5], workArea[6]);
        sceKernelDcacheWritebackInvalidateRange((void *)workArea[7], workArea[8]);
        sz = 0x100;
    }
    sceKernelDcacheWritebackInvalidateRange((void *)workArea[4], sz);
}

/* 0x1AA8 */
static void videocodecInvalidateOutputCache(u32 *workArea, u32 type)
{
    if (type != 0)
        return;
    sceKernelDcacheInvalidateRangeForUser((void *)workArea[11], (workArea[15] * 0x2C + 0x3F) & ~0x3F);
    sceKernelDcacheInvalidateRangeForUser((void *)workArea[12], 0x40);
    if (workArea[14] != 0)
        sceKernelDcacheInvalidateRangeForUser((void *)workArea[14], (workArea[15] * 0x148 + 0x3F) & ~0x3F);
    int flags = (int)workArea[13];
    if (flags < 0 && workArea[20] != 0)
        sceKernelDcacheInvalidateRangeForUser((void *)workArea[20], 0xC0);
    if ((flags & 0x40000000) && workArea[21] != 0)
        sceKernelDcacheInvalidateRangeForUser((void *)workArea[21], 0x40);
}

/* 0x1B80 */
static int videocodecCheckInitBuffers(u32 *workArea, u32 type)
{
    int k1 = get_k1();
    if ((int)(((u32)workArea | ((u32)workArea + 0x60)) & k1) < 0)
        return 0x80000023;
    if ((int)(workArea[3] & k1) < 0)
        return 0x80000023;
    u32 sz = 0x28;
    if (type != 0) {
        if (type >= 3)
            return 0x806201FE;
        sz = 0x100;
    }
    u32 infoBuf = workArea[4];
    if ((int)((infoBuf | (infoBuf + sz)) & k1) < 0)
        return 0x80000023;
    if (type != 0)
        return 0;

    u32 count = workArea[15];
    u32 frameInfoBuf = workArea[11];
    u32 frameInfoSize = count * 0x2C;
    if ((int)((frameInfoBuf | (frameInfoBuf + frameInfoSize) | frameInfoSize) & k1) < 0)
        return 0x80000023;
    u32 picInfoBuf = workArea[12];
    if ((int)((picInfoBuf | (picInfoBuf + 0x64)) & k1) < 0)
        return 0x80000023;
    u32 extBuf = workArea[14];
    if (extBuf != 0) {
        u32 extBufSize = count * 0x148;
        if ((int)((extBuf | (extBuf + extBufSize) | extBufSize) & k1) < 0)
            return 0x80000023;
    }
    return 0;
}

/* 0x1C74 */
static void videocodecFlushInitCache(u32 *workArea, u32 type)
{
    u32 sz = 0x28;
    if (type != 0) {
        if ((workArea[5] & 0x1FFFFFFF) > 0x3FFFFF)
            sceKernelDcacheWritebackInvalidateRange((void *)workArea[5], workArea[6]);
        sz = 0x100;
    }
    sceKernelDcacheWritebackInvalidateRange((void *)workArea[4], sz);
}

/* 0x1CDC */
static int videocodecCheckWorkAreaWithCtrl(u32 *workArea, u32 type)
{
    int res = videocodecCheckWorkArea(workArea, type);
    if (res >= 0 && (int)(workArea[3] & get_k1()) < 0)
        res = 0x80000023;
    return res;
}

/* 0x1D2C */
static int videocodecCalcFrameBufSizes(u32 *workArea, u32 *sizes)
{
    int k1 = get_k1();
    if ((int)(((u32)workArea | ((u32)workArea + 0x38)) & k1) < 0)
        return 0x80000023;
    u32 w = workArea[0];
    if (w & 0xF)
        return 0x806201FE;
    u32 h = workArea[1];
    if ((h & 0xF) || w == 0 || w >= 0x2D1 || h == 0 || h >= 0x241)
        return 0x806201FE;

    u32 halfHeight = (h >> 4) << 3;
    u32 wAligned = w & ~0x1F;
    if (w & 0x10) {
        sizes[1] = (wAligned >> 1) * halfHeight;
        sizes[0] = ((wAligned + 0x20) >> 1) * halfHeight;
    } else {
        u32 planeSize = (wAligned >> 1) * halfHeight;
        sizes[1] = planeSize;
        sizes[0] = planeSize;
    }
    sizes[2] = sizes[0] >> 1;
    sizes[3] = sizes[1] >> 1;
    u32 area = workArea[0] * workArea[1];
    u32 chromaArea = area >> 2;
    sizes[4] = area;
    sizes[5] = chromaArea;
    if (((workArea[0] >> 4) & 7) == 7) {
        sizes[4] = area + (workArea[1] << 4);
        sizes[5] = chromaArea + ((workArea[1] << 4) >> 2);
    }

    u32 sizeYEven = sizes[0], sizeYOdd = sizes[1];
    u32 sizeCEven = sizes[2], sizeCOdd = sizes[3];
    u32 sizeOutY = sizes[4], sizeOutC = sizes[5];
    if ((int)((workArea[3] | (workArea[3] + sizeYEven) | sizeYEven) & k1) < 0 ||
        (int)((workArea[4] | (workArea[4] + sizeYEven) | sizeYEven) & k1) < 0 ||
        (int)((workArea[7] | (workArea[7] + sizeYOdd) | sizeYOdd) & k1) < 0 ||
        (int)((workArea[8] | (workArea[8] + sizeYOdd) | sizeYOdd) & k1) < 0 ||
        (int)((workArea[5] | (workArea[5] + sizeCEven) | sizeCEven) & k1) < 0 ||
        (int)((workArea[6] | (workArea[6] + sizeCEven) | sizeCEven) & k1) < 0 ||
        (int)((workArea[9] | (workArea[9] + sizeCOdd) | sizeCOdd) & k1) < 0 ||
        (int)((workArea[10] | (workArea[10] + sizeCOdd) | sizeCOdd) & k1) < 0 ||
        (int)((workArea[11] | (workArea[11] + sizeOutY) | sizeOutY) & k1) < 0 ||
        (int)((workArea[12] | (workArea[12] + sizeOutC) | sizeOutC) & k1) < 0 ||
        (int)((workArea[13] | (workArea[13] + sizeOutC) | sizeOutC) & k1) < 0)
        return 0x80000023;
    return 0;
}

/* === sceMpegbase (LLI / Copy) === */
static SceMpegLli *mpegBaseBuildLliChain(SceMpegLli *lli, u32 dst, u32 src, int blocks);
static int mpegBaseRunLliTransfer(SceMpegLli *endLli);

/* 0x1FB4 */
int sceMpegBaseYCrCbCopy(u32 *dst, u32 *src, int flags)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    if ((int)((((u32)src | ((u32)src + 0x30)) | ((u32)dst | ((u32)dst + 0x30))) & (k1 << 11)) < 0) {
        set_k1(k1);
        return 0x80000023;
    }

    u32 srcBase = src[4];
    if (flags & 1) {
        u32 dstBase = dst[4];
        dst[5] = dstBase + (src[5] - srcBase);
        dst[8] = dstBase + (src[8] - srcBase);
        dst[9] = dstBase + (src[9] - srcBase);
    }
    if (flags & 2) {
        u32 dstBase = dst[4];
        dst[6] = dstBase + (src[6] - srcBase);
        dst[7] = dstBase + (src[7] - srcBase);
        dst[10] = dstBase + (src[10] - srcBase);
        dst[11] = dstBase + (src[11] - srcBase);
    }

    int blocksY = ((int)(src[6] - srcBase)) >> 4;
    if ((u32)blocksY > 0xBFF4) {
        set_k1(k1);
        return 0x80000104;
    }
    int blocksC = (u32)blocksY >> 1;

    int res = sceKernelWaitSema(g_mpegCtx.lliSema, 1, NULL);
    if (res >= 0) {
        SceMpegLli *lli = g_mpegCtx.lli;
        __builtin_allegrex_cache(0x18, (unsigned int)lli);
        if (flags & 1) {
            lli = mpegBaseBuildLliChain(lli, dst[4], src[4], blocksY);
            lli = mpegBaseBuildLliChain(lli, dst[5], src[5], blocksY);
            lli = mpegBaseBuildLliChain(lli, dst[8], src[8], blocksC);
            lli = mpegBaseBuildLliChain(lli, dst[9], src[9], blocksC);
        }
        if (flags & 2) {
            lli = mpegBaseBuildLliChain(lli, dst[6], src[6], blocksY);
            lli = mpegBaseBuildLliChain(lli, dst[7], src[7], blocksY);
            lli = mpegBaseBuildLliChain(lli, dst[10], src[10], blocksC);
            lli = mpegBaseBuildLliChain(lli, dst[11], src[11], blocksC);
        }
        if (lli != g_mpegCtx.lli)
            res = mpegBaseRunLliTransfer(lli);
        sceKernelSignalSema(g_mpegCtx.lliSema, 1);
    }
    set_k1(k1);
    return res;
}

/* 0x2210 */
int sceMpegBaseYCrCbCopyVme(u32 *dst, u32 *src, int flags)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    if ((int)((((u32)src | ((u32)src + 0x28)) | ((u32)dst | ((u32)dst + 0x28))) & (k1 << 11)) < 0) {
        set_k1(k1);
        return 0x80000023;
    }

    u32 h = src[1];
    u32 w = src[0];
    u32 hPad = ((h & 0x7F) == 0x50) ? (h + 0x30) : (h + 0x20);
    u32 prod = hPad * w;
    u32 blocksY = prod >> 5;
    if (blocksY > 0xBFF4) {
        set_k1(k1);
        return 0x80000104;
    }
    u32 blocksC = prod >> 7;

    int res = sceKernelWaitSema(g_mpegCtx.lliSema, 1, NULL);
    if (res >= 0) {
        SceMpegLli *lli = g_mpegCtx.lli;
        __builtin_allegrex_cache(0x18, (unsigned int)lli);
        if (flags & 1) {
            lli = mpegBaseBuildLliChain(lli, dst[4], src[4], blocksY);
            lli = mpegBaseBuildLliChain(lli, dst[6], src[6], blocksC);
            lli = mpegBaseBuildLliChain(lli, dst[7], src[7], blocksC);
        }
        if (flags & 2) {
            lli = mpegBaseBuildLliChain(lli, dst[5], src[5], blocksY);
            lli = mpegBaseBuildLliChain(lli, dst[8], src[8], blocksC);
            lli = mpegBaseBuildLliChain(lli, dst[9], src[9], blocksC);
        }
        if (lli != g_mpegCtx.lli)
            res = mpegBaseRunLliTransfer(lli);
        sceKernelSignalSema(g_mpegCtx.lliSema, 1);
    }
    set_k1(k1);
    return res;
}

/* 0x23CC */
static SceMpegLli *mpegBaseBuildLliChain(SceMpegLli *lli, u32 dst, u32 src, int blocks)
{
    if (blocks == 0)
        return lli;
    u32 k1Top = ((u32)get_k1() >> 31) << 30;
    while (blocks != 0) {
        int cur = (blocks < 0xFFF) ? blocks : 0xFFF;
        SceMpegLli *next = lli + 1;
        lli->src = k1Top | (src & 0x1FFFFFFF);
        lli->dst = k1Top | (dst & 0x1FFFFFFF);
        lli->next = (u32)next & 0x1FFFFFFF;
        lli->ctrl = 0x01909000 | cur;
        u32 bytes = cur << 4;
        dst += bytes;
        src += bytes;
        blocks -= cur;
        lli = next;
    }
    return lli;
}

/* 0x2454 */
static int mpegBaseRunLliTransfer(SceMpegLli *endLli)
{
    endLli[-1].next = 0;
    endLli[-1].ctrl |= 0x80000000;
    sceKernelDcacheWritebackInvalidateRange(g_mpegCtx.lli, (u32)endLli - (u32)g_mpegCtx.lli);
    for (;;) {
        int res = sceDmacplus_driver_6945F1D3(g_mpegCtx.lli);
        if (res == 0)
            return sceDmacplus_driver_5FCF43BD(0, 0);
        if (res != (int)0x80000021)
            return res;
        sceDmacplus_driver_5FCF43BD(0, 0);
    }
}

/* 0x24E4 */
int sceMpegBasePESpacketCopy(u32 *pkt)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    if ((int)((u32)pkt & (k1 << 11)) < 0) {
        set_k1(k1);
        return 0x80000023;
    }
    int res = sceKernelWaitSema(g_mpegCtx.lliSema, 1, NULL);
    if (res < 0) {
        set_k1(k1);
        return res;
    }

    SceMpegLli *lli = g_mpegCtx.lli;
    __builtin_allegrex_cache(0x18, (unsigned int)lli);
    u32 k1Top = ((u32)(k1 << 11) >> 31) << 30;
    SceMpegLli *end = &g_mpegCtx.lli[100];

    res = 0x80000104;
    for (;;) {
        u32 srcAddr = pkt[0];
        u32 dstAddr = pkt[1];
        u32 len = pkt[3] & 0xFFF;
        SceMpegLli *nextLli = lli + 1;
        lli->src = k1Top | (srcAddr & 0x1FFFFFFF);
        lli->dst = k1Top | (dstAddr & 0x1FFFFFFF);
        if (len == 0)
            break;
        u32 hasNext = pkt[2];
        if (hasNext == 0) {
            lli->ctrl = 0x8A909000 | len;
            lli->next = 0;
            sceKernelDcacheWritebackRange(g_mpegCtx.lli, (u32)lli + 0x10 - (u32)g_mpegCtx.lli);
            for (;;) {
                res = sceDmacplus_driver_282CA0D7(g_mpegCtx.lli);
                if (res == 0) {
                    res = sceDmacplus_driver_47D400CB(0, 0);
                    break;
                }
                if (res != (int)0x80000021)
                    break;
                sceDmacplus_driver_47D400CB(0, 0);
            }
            break;
        }
        pkt += 4;
        lli->next = (u32)nextLli & 0x1FFFFFFF;
        lli->ctrl = 0x0A909000 | len;
        lli = nextLli;
        if (nextLli == end)
            break;
    }

    sceKernelSignalSema(g_mpegCtx.lliSema, 1);
    set_k1(k1);
    return res;
}

/* === sceJpeg === */
static int mpegBaseRunCsc(void *rgbEven, void *rgbOdd, void *yuvDesc);
static int mpegBaseConfigCsc(int mode, int width, const void *coeffs);
int sceJpegDecodeMJpegSuccessively(const u8 *jpegbuf, u32 size, void *rgba, int mode);
int sceJpegDecodeMJpegYCbCrSuccessively(const u8 *jpegbuf, u32 size, u8 *ycbcr, u32 ycbcrSize, int mode);

/* 0x267C */
int sceJpegInitMJpeg(void)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = sceKernelWaitSema(g_jpegCtx.sema, 1, NULL);
    if (res >= 0) {
        g_jpegMeBuf[0] = 0x05100601;
        res = sceMeVideo_driver_E8CD3C75(3, g_jpegMeBuf);
        if (res >= 0) {
            g_jpegCtx.state = 1;
            sceKernelSetEventFlag(g_mpegCtx.cscEvf, 2);
        } else if (res >= -9) {
            res = 0x80650042;
        }
        sceKernelSignalSema(g_jpegCtx.sema, 1);
    }
    set_k1(k1);
    return res;
}

/* 0x2748 */
int sceJpegCreateMJpeg(int maxWidth, int maxHeight)
{
    int k1 = get_k1();
    int res = 0x80650020;
    if (maxWidth <= 0x400) {
        set_k1(k1 << 11);
        res = sceKernelWaitSema(g_jpegCtx.sema, 1, NULL);
        if (res >= 0) {
            if (g_jpegCtx.state == 1) {
                g_jpegCtx.state = 2;
                g_jpegCtx.width = maxWidth;
                g_jpegCtx.height = maxHeight;
                sceKernelSignalSema(g_jpegCtx.sema, 1);
            } else {
                sceKernelSignalSema(g_jpegCtx.sema, 1);
                res = 0x80650039;
            }
        }
        set_k1(k1);
    }
    return res;
}

/* 0x2828 */
int sceJpegDeleteMJpeg(void)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = sceKernelWaitSema(g_jpegCtx.sema, 1, NULL);
    if (res >= 0) {
        if (g_jpegCtx.state == 2) {
            g_jpegCtx.state = 1;
            sceKernelSignalSema(g_jpegCtx.sema, 1);
            res = 0;
        } else {
            sceKernelSignalSema(g_jpegCtx.sema, 1);
            res = 0x80650039;
        }
    }
    set_k1(k1);
    return res;
}

/* 0x28C4 */
int sceJpegFinishMJpeg(void)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = sceKernelWaitSema(g_jpegCtx.sema, 1, NULL);
    if (res >= 0) {
        if (g_jpegCtx.state == 1) {
            sceKernelPollEventFlag(g_mpegCtx.cscEvf, 2, 0x21, NULL);
            g_jpegMeBuf[0] = 0x05100601;
            res = sceMeVideo_driver_8DD56014(3, g_jpegMeBuf);
            if (res >= 0) {
                g_jpegCtx.state = 0;
            } else if (res >= -9) {
                res = 0x80650043;
            }
            sceKernelSignalSema(g_jpegCtx.sema, 1);
        } else {
            sceKernelSignalSema(g_jpegCtx.sema, 1);
            res = 0x80650039;
        }
    }
    set_k1(k1);
    return res;
}

/* 0x29C0 */
int sceJpegMJpegCscWithColorOption(void *rgba, const void *ycbcr, int widthHeight, int lineWidth, int colorOption)
{
    int w = ((u32)widthHeight >> 16) & 0xFFF;
    int h = widthHeight & 0xFFF;
    int optHi = colorOption >> 4;
    int optLo = colorOption & 0xF;
    if (lineWidth > 0x400 || w > 0x2D0 || h > 0x1E0)
        return 0x80650020;

    int mode;
    if (optLo == 1) {
        mode = 2;
    } else if (optLo < 2) {
        if (optLo != 0)
            return 0x80650050;
        mode = 1;
    } else if (optLo == 2) {
        mode = 3;
    } else if (optLo == 3) {
        mode = 0;
    } else {
        return 0x80650050;
    }

    int sdkVer = sceKernelGetCompiledSdkVersion();
    if (sdkVer <= 0x06030010)
        mode <<= 1;
    if ((u32)optHi >= 2)
        return 0x80650050;

    int k1 = get_k1();
    set_k1(k1 << 11);
    if ((int)(((u32)ycbcr | (u32)rgba) & (k1 << 11)) < 0) {
        set_k1(k1);
        return 0x80000023;
    }

    int area = w * h;
    int halfArea = area / 2;
    int quarterArea = halfArea / 4;
    int effH = (mode == 0) ? (h << 1) : h;
    int strideBytes = lineWidth * effH;

    u32 desc[10];
    desc[0] = h >> 4;
    desc[1] = w >> 4;
    desc[2] = 0;
    desc[3] = 1;
    desc[4] = (u32)ycbcr;
    desc[5] = (u32)ycbcr + halfArea;
    desc[6] = (u32)ycbcr + halfArea * 2;
    desc[7] = (u32)ycbcr + halfArea * 2 + quarterArea * 2;
    desc[8] = (u32)ycbcr + halfArea * 2 + quarterArea;
    desc[9] = (u32)ycbcr + halfArea * 2 + quarterArea * 3;

    sceKernelDcacheInvalidateRangeForUser(rgba, strideBytes << 1);
    mpegBaseConfigCsc(mode, lineWidth, (optHi == 0) ? g_cscCoeffsJfif : g_cscCoeffsBt601);
    int res = mpegBaseRunCsc(rgba, (u8 *)rgba + strideBytes, desc);
    sceKernelUnlockMutex(g_mpegCtx.cscMutex, 1);
    set_k1(k1);
    return res;
}

/* 0x2BE4 */
int sceJpegDecodeMJpeg(const u8 *jpegbuf, u32 size, void *rgba, int mode)
{
    if (sceMeCore_driver_var_61FF4837 != 0)
        return sceJpegDecodeMJpegSuccessively(jpegbuf, size, rgba, mode);
    if ((sceMeCore_driver_905A7500() & ~8) != 0)
        return 0x80650052;

    int m = 1;
    if (mode != 0) {
        m = 2;
        if (mode == 2)
            m = 3;
        if (mode == 3)
            m = 0;
    }

    int k1 = get_k1();
    set_k1(k1 << 11);
    if ((int)(((u32)jpegbuf | ((u32)jpegbuf + size) | size) & (k1 << 11)) < 0) {
        set_k1(k1);
        return 0x80000023;
    }

    sceKernelDcacheWritebackRange(jpegbuf, size);
    if ((0x220202 >> ((u32)jpegbuf >> 27)) & 1)
        sceKernelDcacheInvalidateRangeForUser((void *)0x40, 0x100000);

    int dec = sceMeCore_driver_FA398D71(0x144, jpegbuf, size, 0x40, 0x100000, m);
    set_k1(k1);
    if (dec < 0)
        return dec;
    if (g_jpegCtx.height < (dec & 0xFFFF) || g_jpegCtx.width < (int)((u32)dec >> 16))
        return 0x80650020;

    int res = sceJpegMJpegCscWithColorOption(rgba, (const void *)0x40, dec, g_jpegCtx.width, 3);
    return (res < 0) ? res : dec;
}

/* 0x2D78 */
int sceJpegDecodeMJpegYCbCr(const u8 *jpegbuf, u32 size, u8 *ycbcr, u32 ycbcrSize, int mode)
{
    if (sceMeCore_driver_var_61FF4837 != 0)
        return sceJpegDecodeMJpegYCbCrSuccessively(jpegbuf, size, ycbcr, ycbcrSize, mode);

    int m = 1;
    if (mode != 0) {
        m = 2;
        if (mode == 2)
            m = 3;
        if (mode == 3)
            m = 0;
    }

    if (((0x220202 >> ((u32)jpegbuf >> 27)) & 1) == 0)
        return 0x80650051;

    int k1 = get_k1();
    set_k1(k1 << 11);
    if ((int)(((u32)jpegbuf | ((u32)jpegbuf + size) | size) & (k1 << 11)) < 0 ||
        (int)(((u32)ycbcr | ((u32)ycbcr + ycbcrSize) | ycbcrSize) & (k1 << 11)) < 0) {
        set_k1(k1);
        return 0x80000023;
    }

    sceKernelDcacheWritebackRange(jpegbuf, size);
    sceKernelDcacheInvalidateRangeForUser(ycbcr, ycbcrSize);
    int res = sceMeCore_driver_FA398D71(0x144, jpegbuf, size, ycbcr, ycbcrSize, m);
    set_k1(k1);
    return res;
}

/* 0x2EAC */
int sceJpegDecodeMJpegYCbCrSuccessively(const u8 *jpegbuf, u32 size, u8 *ycbcr, u32 ycbcrSize, int mode)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int sz = sceMeCore_driver_FA398D71(0x141);
    if (sz < 0) {
        set_k1(k1);
        return sz;
    }
    if ((int)(((u32)jpegbuf | ((u32)jpegbuf + size) | size) & (k1 << 11)) < 0 ||
        (int)(((u32)ycbcr | ((u32)ycbcr + ycbcrSize) | ycbcrSize) & (k1 << 11)) < 0) {
        set_k1(k1);
        return 0x80000023;
    }

    void *raw = sceMeMalloc(sz);
    if (raw == NULL) {
        set_k1(k1);
        return 0x80650041;
    }
    void *aligned = (void *)(((u32)raw + 0x7F) & ~0x7F);
    int m = 1;
    if (mode != 0) {
        m = 2;
        if (mode == 2)
            m = 3;
        if (mode == 3)
            m = 0;
    }

    int res = sceMeCore_driver_FA398D71(0x140, aligned, jpegbuf, size, 1, m);
    if (res >= 0) {
        if (ycbcrSize < (u32)res) {
            res = 0x80650041;
        } else {
            res = sceMeCore_driver_FA398D71(0x143, aligned, ycbcr, 0, 0, 0, 0);
            if (res >= 0) {
                do {
                    res = sceMeCore_driver_635397BB(0x142, aligned);
                } while (res > 0);
                if (res == 0)
                    res = sceMeCore_driver_FA398D71(0x147, aligned);
            }
        }
    }
    sceMeFree(raw);
    set_k1(k1);
    return res;
}

/* 0x304C */
int sceJpegDecodeMJpegSuccessively(const u8 *jpegbuf, u32 size, void *rgba, int mode)
{
    if ((sceMeCore_driver_905A7500() & ~8) != 0)
        return 0x80650052;
    int dec = sceJpegDecodeMJpegYCbCrSuccessively(jpegbuf, size, (u8 *)0x40, 0x100000, mode);
    if (dec < 0)
        return dec;
    if (g_jpegCtx.height < (dec & 0xFFFF) || g_jpegCtx.width < (int)((u32)dec >> 16))
        return 0x80650020;
    int res = sceJpegMJpegCscWithColorOption(rgba, (const void *)0x40, dec, g_jpegCtx.width, 3);
    return (res < 0) ? res : dec;
}

/* 0x3128 */
int sceJpegGetOutputInfo(const u8 *jpegbuf, u32 size, int *colourInfo, int mode)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    if ((int)(((u32)jpegbuf | ((u32)jpegbuf + size) | size) & (k1 << 11)) < 0 ||
        (colourInfo != NULL && (int)((u32)colourInfo & (k1 << 11)) < 0)) {
        set_k1(k1);
        return 0x80000023;
    }

    int sz = sceMeCore_driver_FA398D71(0x141);
    if (sz < 0) {
        set_k1(k1);
        return sz;
    }
    void *buf = sceMeMalloc(sz);
    if (buf == NULL) {
        set_k1(k1);
        return 0x80000022;
    }

    sceKernelDcacheWritebackRange(jpegbuf, size);
    int res = sceMeCore_driver_FA398D71(0x140, buf, jpegbuf, size, 1, (mode == 0) ? 0 : 2);
    if (res >= 0 && colourInfo != NULL) {
        int info = sceMeCore_driver_FA398D71(0x146, buf);
        int hSample = info & 3;
        int vSample = (info >> 2) & 3;
        int numComp = (info >> 4) & 0xF;
        *colourInfo = (numComp << 16) | ((1 << (vSample - 1)) << 8) | (1 << (hSample - 1));
        if (hSample == 0 || (info & 0xC) == 0 || (info & 0xF0) == 0)
            *colourInfo = 0;
    }
    sceMeFree(buf);
    set_k1(k1);
    return res;
}

/* 0x3298 */
int sceJpegMJpegCsc(void *rgba, const void *ycbcr, int widthHeight, int lineWidth)
{
    return sceJpegMJpegCscWithColorOption(rgba, ycbcr, widthHeight, lineWidth, 3);
}

/* 0x32B4 */
static int jpegInitSync(void)
{
    g_jpegCtx.state = 0;
    int sema = sceKernelCreateSema("SceJpeg", 1, 1, 1, NULL);
    g_jpegCtx.sema = sema;
    return (sema < 0) ? 1 : 0;
}

/* 0x3304 */
static int jpegTermSync(void)
{
    sceKernelDeleteSema(g_jpegCtx.sema);
    return 0;
}

/* === sceMpegbase (CSC) === */
static int mpegBaseFillYuvRowArgs(u32 *dst, const u32 *src, const u32 *offsets, int mode);
static int mpegBaseRunCscEx(void *rgbEven, void *rgbOdd, void *yuvDesc);
static int mpegBaseComputeRowOffsets(u32 *pRgbEven, u32 *pRgbOdd, const u32 *range, const u32 *src, u32 *offsets, u32 *outStep);
static int mpegBaseTransferYuvSlice(u32 rgbEven, u32 rgbOdd, const u32 *range, const u32 *src);

/* 0x332C */
int sceMpegBaseCscAvc(void *rgbEven, void *rgbOdd, int width, const u32 *src)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    if ((int)(((u32)src | ((u32)src + 0x30)) & (k1 << 11)) < 0) {
        set_k1(k1);
        return 0x80000023;
    }

    u32 w = src[0];
    u32 h = src[1];
    u32 macroblockBytes = (w * h) << 8;
    u32 szY = macroblockBytes >> 2;
    u32 szC = macroblockBytes >> 3;
    u32 stride = (width != 0) ? (u32)width : g_mpegCtx.cscWidth;
    u32 szRgb = (stride * h) << 2;

    if ((int)((src[4] | (src[4] + szY)) & (k1 << 11)) < 0 ||
        (int)((src[6] | (src[6] + szY)) & (k1 << 11)) < 0 ||
        (int)((src[8] | (src[8] + szC)) & (k1 << 11)) < 0 ||
        (int)((src[10] | (src[10] + szC)) & (k1 << 11)) < 0 ||
        (int)((src[5] | (src[5] + szY)) & (k1 << 11)) < 0 ||
        (int)((src[7] | (src[7] + szY)) & (k1 << 11)) < 0 ||
        (int)((src[9] | (src[9] + szC)) & (k1 << 11)) < 0 ||
        (int)((src[11] | (src[11] + szC)) & (k1 << 11)) < 0 ||
        (int)(((u32)rgbEven | ((u32)rgbEven + szRgb) | szRgb) & (k1 << 11)) < 0 ||
        (rgbOdd != NULL && (int)(((u32)rgbOdd | ((u32)rgbOdd + szRgb) | szRgb) & (k1 << 11)) < 0)) {
        set_k1(k1);
        return 0x80000023;
    }

    int res = sceDmacplus_driver_F652F09D(0, 0, h, w);
    if (res != 0) {
        Kprintf("Failed to set csc range : 0x%08x \n", res);
        set_k1(k1);
        return res;
    }

    res = mpegBaseConfigCsc(g_mpegCtx.cscMode, width, NULL);
    if (res == 0) {
        if (sceKernelPollEventFlag(g_mpegCtx.cscEvf, 3, 1, NULL) < 0) {
            res = 0x80000001;
        } else {
            for (;;) {
                res = sceDmacplus_driver_D126494B(rgbEven, rgbOdd, (void *)src);
                if (res == 0) {
                    res = sceDmacplus_driver_BF0DB45E(0, 0);
                    break;
                }
                if (res != (int)0x80000021)
                    break;
                sceDmacplus_driver_BF0DB45E(0, 0);
            }
        }
    }
    sceKernelUnlockMutex(g_mpegCtx.cscMutex, 1);
    set_k1(k1);
    return res;
}

/* 0x35BC */
int sceMpegBaseCscVme(void *rgbEven, void *rgbOdd, int width, const u32 *src)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    if ((int)(((u32)src | ((u32)src + 0x28)) & (k1 << 11)) < 0) {
        set_k1(k1);
        return 0x80000023;
    }

    u32 w = src[0];
    u32 h = src[1];
    u32 szY = (w * h) << 7;
    u32 szC = szY >> 2;
    u32 stride = (width != 0) ? (u32)width : g_mpegCtx.cscWidth;
    if (stride < (h << 4)) {
        set_k1(k1);
        return 0x800001FE;
    }
    u32 szRgb = (stride * h) << 2;
    if ((int)((src[4] | (src[4] + szY) | szY) & (k1 << 11)) < 0 ||
        (int)((src[5] | (src[5] + szY) | szY) & (k1 << 11)) < 0 ||
        (int)((src[6] | (src[6] + szC)) & (k1 << 11)) < 0 ||
        (int)((src[7] | (src[7] + szC)) & (k1 << 11)) < 0 ||
        (int)((src[8] | (src[8] + szC)) & (k1 << 11)) < 0 ||
        (int)((src[9] | (src[9] + szC)) & (k1 << 11)) < 0 ||
        (int)(((u32)rgbEven | ((u32)rgbEven + szRgb) | szRgb) & (k1 << 11)) < 0 ||
        (rgbOdd != NULL && (int)(((u32)rgbOdd | ((u32)rgbOdd + szRgb) | szRgb) & (k1 << 11)) < 0)) {
        set_k1(k1);
        return 0x80000023;
    }

    int res = mpegBaseConfigCsc(g_mpegCtx.cscMode, width, NULL);
    if (res == 0)
        res = mpegBaseRunCsc(rgbEven, rgbOdd, (void *)src);
    sceKernelUnlockMutex(g_mpegCtx.cscMutex, 1);
    set_k1(k1);
    return res;
}

/* 0x3790 */
static int mpegBaseRunCsc(void *rgbEven, void *rgbOdd, void *yuvDesc)
{
    if (sceKernelPollEventFlag(g_mpegCtx.cscEvf, 3, 1, NULL) < 0)
        return 0x80000001;
    for (;;) {
        int res = sceDmacplus_driver_9C492B9B(rgbEven, rgbOdd, yuvDesc);
        if (res == 0)
            return sceDmacplus_driver_BF0DB45E(0, 0);
        if (res != (int)0x80000021)
            return res;
        sceDmacplus_driver_BF0DB45E(0, 0);
    }
}

/* 0x384C */
int sceMpegBaseCscAvcRange(void *rgbEven, void *rgbOdd, const u32 *range, int width, const u32 *src)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    if ((int)((((u32)range | ((u32)range + 0x10)) | ((u32)src | ((u32)src + 0x30))) & (k1 << 11)) < 0) {
        set_k1(k1);
        return 0x80000023;
    }

    u32 w = src[0];
    u32 h = src[1];
    u32 szY = (w * h) << 6;
    u32 szC = szY >> 1;
    u32 stride = (width != 0) ? (u32)width : g_mpegCtx.cscWidth;
    if (stride < (range[2] << 4)) {
        set_k1(k1);
        return 0x800001FE;
    }
    u32 szRgb = (stride * h) << 2;
    if ((int)((src[4] | (src[4] + szY) | szY) & (k1 << 11)) < 0 ||
        (int)((src[6] | (src[6] + szY) | szY) & (k1 << 11)) < 0 ||
        (int)((src[8] | (src[8] + szC)) & (k1 << 11)) < 0 ||
        (int)((src[10] | (src[10] + szC)) & (k1 << 11)) < 0 ||
        (int)((src[5] | (src[5] + szY) | szY) & (k1 << 11)) < 0 ||
        (int)((src[7] | (src[7] + szY) | szY) & (k1 << 11)) < 0 ||
        (int)((src[9] | (src[9] + szC)) & (k1 << 11)) < 0 ||
        (int)((src[11] | (src[11] + szC)) & (k1 << 11)) < 0 ||
        (int)(((u32)rgbEven | ((u32)rgbEven + szRgb) | szRgb) & (k1 << 11)) < 0 ||
        (rgbOdd != NULL && (int)(((u32)rgbOdd | ((u32)rgbOdd + szRgb) | szRgb) & (k1 << 11)) < 0)) {
        set_k1(k1);
        return 0x80000023;
    }
    if (h < range[0] + range[2] || w < range[1] + range[3]) {
        set_k1(k1);
        return 0x800001FE;
    }

    int res = sceDmacplus_driver_F652F09D(range[0], range[1], range[2], range[3]);
    if (res != 0) {
        Kprintf("Failed to set csc range : 0x%08x \n", res);
        set_k1(k1);
        return res;
    }
    res = mpegBaseConfigCsc(g_mpegCtx.cscMode, width, NULL);
    if (res == 0) {
        if (w != range[3]) {
            res = mpegBaseTransferYuvSlice((u32)rgbEven, (u32)rgbOdd, range, src);
        } else if (h != range[2]) {
            u32 rowDesc[12];
            u32 offsets[4];
            u32 step = 0;
            u32 curRgbEven = (u32)rgbEven, curRgbOdd = (u32)rgbOdd;
            res = mpegBaseComputeRowOffsets(&curRgbEven, &curRgbOdd, range, src, offsets, &step);
            if (res == 0) {
                rowDesc[0] = range[3];
                rowDesc[1] = range[2] - step;
                rowDesc[2] = src[2];
                rowDesc[3] = src[3];
                mpegBaseFillYuvRowArgs(rowDesc, src, offsets, 0);
                res = mpegBaseRunCscEx((void *)curRgbEven, (void *)curRgbOdd, rowDesc);
            }
        } else {
            res = mpegBaseRunCscEx(rgbEven, rgbOdd, (void *)src);
        }
    }
    sceKernelUnlockMutex(g_mpegCtx.cscMutex, 1);
    set_k1(k1);
    return res;
}

/* 0x3B98 */
static int mpegBaseConfigCsc(int mode, int width, const void *coeffs)
{
    if (width == 0)
        width = g_mpegCtx.cscWidth;
    if (coeffs == NULL)
        coeffs = g_mpegCtx.coeffs;
    sceKernelLockMutex(g_mpegCtx.cscMutex, 1, NULL);
    for (;;) {
        int res = sceDmacplus_driver_B269EAC9(mode, width, coeffs);
        if (res != (int)0x80000021)
            return res;
        sceDmacplus_driver_BF0DB45E(0, 0);
    }
}

/* 0x3C40 */
int sceMpegbase_0530BE4E(int mode)
{
    g_mpegCtx.cscMode = (s16)mode;
    return 0;
}

/* 0x3C50 */
int sceMpegbase_AC9E717E(int width, const void *coeffs)
{
    int k1 = get_k1();
    if ((int)((k1 << 11) & ((u32)coeffs | ((u32)coeffs + 22))) < 0)
        return 0x80000023;
    g_mpegCtx.cscWidth = width;
    g_mpegCtx.cscMode = 0;
    __builtin_memcpy(g_mpegCtx.coeffs, coeffs, sizeof(g_mpegCtx.coeffs));
    set_k1(k1);
    return 0;
}

/* 0x3D58 */
int sceMpegBaseCscInit(int width)
{
    int k1 = get_k1();
    set_k1(k1 << 11);
    int res = sceMpegbase_AC9E717E(width, g_cscCoeffsBt601);
    set_k1(k1);
    return res;
}

/* 0x3D8C */
static int mpegBaseFillYuvRowArgs(u32 *dst, const u32 *src, const u32 *offsets, int mode)
{
    if (mode & 2) {
        dst[4] = src[5] + offsets[1];
        dst[6] = src[7] + offsets[1];
        dst[8] = src[9] + offsets[3];
        dst[10] = src[11] + offsets[3];
    } else {
        dst[4] = src[4] + offsets[0];
        dst[6] = src[6] + offsets[0];
        dst[8] = src[8] + offsets[2];
        dst[10] = src[10] + offsets[2];
    }
    if (mode == 0) {
        dst[5] = src[5] + offsets[1];
        dst[7] = src[7] + offsets[1];
        dst[9] = src[9] + offsets[3];
        dst[11] = src[11] + offsets[3];
    } else {
        dst[5] = dst[4];
        dst[7] = dst[6];
        dst[9] = dst[8];
        dst[11] = dst[10];
    }
    return 0;
}

/* 0x3E4C */
static int mpegBaseRunCscEx(void *rgbEven, void *rgbOdd, void *yuvDesc)
{
    if (sceKernelPollEventFlag(g_mpegCtx.cscEvf, 3, 1, NULL) < 0)
        return 0x80000001;
    for (;;) {
        int res = sceDmacplus_driver_D126494B(rgbEven, rgbOdd, yuvDesc);
        if (res == 0)
            return sceDmacplus_driver_BF0DB45E(0, 0);
        if (res != (int)0x80000021)
            return res;
        sceDmacplus_driver_BF0DB45E(0, 0);
    }
}

/* 0x3EFC */
static int mpegBaseComputeRowOffsets(u32 *pRgbEven, u32 *pRgbOdd, const u32 *range, const u32 *src, u32 *offsets, u32 *outStep)
{
    u32 rowDesc[12];
    rowDesc[1] = 0;
    u32 rx = range[0];
    u32 sw = src[0];
    u32 halfSw = (sw << 4) >> 1;
    if ((rx & 1) == 0) {
        u32 offY = (halfSw * (rx >> 1)) << 4;
        u32 offC = offY >> 1;
        offsets[0] = offY;
        offsets[1] = offY;
        offsets[2] = offC;
        offsets[3] = offC;
        *outStep = 0;
        return 0;
    }

    rowDesc[1] = 1;
    u32 offYEven = (halfSw * ((rx + 1) >> 1)) << 4;
    offsets[0] = offYEven;
    offsets[2] = offYEven >> 1;
    u32 offYOdd = (halfSw * (rx >> 1)) << 4;
    offsets[1] = offYOdd;
    offsets[3] = offYOdd >> 1;

    rowDesc[0] = range[3];
    rowDesc[2] = src[2];
    rowDesc[3] = src[3];
    mpegBaseFillYuvRowArgs(rowDesc, src, offsets, 2);
    int res = mpegBaseRunCscEx((void *)*pRgbEven, (void *)*pRgbOdd, rowDesc);
    if (res == 0) {
        *pRgbEven += 0x40;
        if (*pRgbOdd != 0)
            *pRgbOdd += 0x40;
        u32 stepY = halfSw << 4;
        offsets[1] += stepY;
        offsets[3] += stepY >> 1;
    }
    *outStep = rowDesc[1];
    return res;
}

/* 0x4074 */
static int mpegBaseTransferYuvSlice(u32 rgbEven, u32 rgbOdd, const u32 *range, const u32 *src)
{
    u32 rowDesc[12];
    u32 offsets[4];
    u32 step = 0;
    u32 curRgbEven = rgbEven, curRgbOdd = rgbOdd;
    int res = mpegBaseComputeRowOffsets(&curRgbEven, &curRgbOdd, range, src, offsets, &step);
    if (res != 0)
        return res;

    u32 rem = range[2] - step;
    u32 pairs = rem >> 1;
    rowDesc[0] = range[3];
    rowDesc[1] = 2;
    rowDesc[2] = src[2];
    rowDesc[3] = src[3];

    u32 i;
    u32 stepY = src[0] << 7;
    u32 stepC = stepY >> 1;
    for (i = 0; i < pairs; i++) {
        mpegBaseFillYuvRowArgs(rowDesc, src, offsets, 0);
        res = mpegBaseRunCscEx((void *)curRgbEven, (void *)curRgbOdd, rowDesc);
        if (res != 0)
            return res;
        curRgbEven += 0x80;
        if (curRgbOdd != 0)
            curRgbOdd += 0x80;
        offsets[0] += stepY;
        offsets[1] += stepY;
        offsets[2] += stepC;
        offsets[3] += stepC;
    }
    if (rem & 1) {
        rowDesc[1] = 1;
        mpegBaseFillYuvRowArgs(rowDesc, src, offsets, 0);
        res = mpegBaseRunCscEx((void *)curRgbEven, (void *)curRgbOdd, rowDesc);
    }
    return res;
}

/* 0x4204 */
int sceJpegCsc(void *rgba, const void *ycbcr, int widthHeight, int lineWidth, int colorOption)
{
    if (((0x220202 >> ((u32)ycbcr >> 27)) & 1) == 0)
        return 0x80650051;
    if (((0x220202 >> ((u32)rgba >> 27)) & 1) == 0)
        return 0x80650051;

    int fmt = colorOption & 0xFFFFF;
    if (fmt != 0x20101 && fmt != 0x20201 && fmt != 0x20202)
        return 0x80650013;

    int k1 = get_k1();
    if ((int)((k1 << 11) & ((u32)rgba | (u32)ycbcr)) < 0)
        return 0x80000023;
    set_k1(k1 << 11);

    int flags = (((colorOption >> 20) & 1) << 7) | (((colorOption & 0x300) >> 6) | (colorOption & 3));
    int res;
    if ((colorOption & 0x200000) == 0) {
        res = sceMeCore_driver_FA398D71(0x150, rgba, ycbcr, widthHeight, lineWidth, flags);
    } else {
        void *buf = sceMeMalloc(0x2C);
        if (buf == NULL) {
            res = 0x80650041;
        } else {
            res = sceMeCore_driver_635397BB(0x152, buf, rgba, ycbcr, (u32)widthHeight >> 16, widthHeight & 0xFFFF, flags, 0, lineWidth);
            if (res >= 0) {
                int step = (s8)res;
                int off = 0;
                do {
                    res = sceMeCore_driver_FA398D71(0x151, buf, off, step);
                    off += 16;
                } while (res > 0);
            }
            sceMeFree(buf);
        }
    }
    set_k1(k1);
    return res;
}

/* 0x43E4 */
int sceAvcodecStartEntry(SceSize args, void *argp)
{
    mpegBaseInitSync();
    videocodecPowerOff();
    jpegInitSync();
    return 0;
}

/* 0x4414 */
int sceAvcodecEndEntry(SceSize args, void *argp)
{
    mpegBaseTermSync();
    jpegTermSync();
    return 0;
}
