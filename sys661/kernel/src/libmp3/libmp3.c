/*
 * sceMp3_Library - libmp3.prx
 * MP3 stream and low-level decoding library using sceAudiocodec.
 */

#include <common_imp.h>

SCE_MODULE_INFO("sceMp3_Library", SCE_MODULE_USER | SCE_MODULE_ATTR_CANT_STOP | SCE_MODULE_ATTR_EXCLUSIVE_LOAD, 1, 6);
SCE_SDK_VERSION(0x06060110);

u32 module_start_thread_parameter[4] = { 3, 0x20, 0x400, 0 };
u32 module_stop_thread_parameter[4] = { 3, 0x20, 0x400, 0 };

/* Imports from Kernel_Library */
extern int sceKernelCpuSuspendIntr(void);
extern void sceKernelCpuResumeIntr(int intr);
extern void *sceKernelMemset(void *dst, int c, u32 size);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 size);

/* Imports from ThreadManForUser */
extern SceUID sceKernelCreateFpl(const char *name, SceUID part, int attr, u32 size, u32 blocks, void *opt);
extern int sceKernelAllocateFpl(SceUID uid, void **data, u32 *timeout);
extern int sceKernelFreeFpl(SceUID uid, void *data);
extern int sceKernelDeleteFpl(SceUID uid);

/* Imports from sceAudiocodec */
extern int sceAudiocodecCheckNeedMem(u32 *work, int type);
extern int sceAudiocodecGetEDRAM(u32 *work, int type);
extern int sceAudiocodecReleaseEDRAM(u32 *work);
extern int sceAudiocodecInit(u32 *work, int type);
extern int sceAudiocodecGetInfo(u32 *work, int type);
extern int sceAudiocodecDecode(u32 *work, int type);
extern int sceAudiocodecAlcExtendParameter(u32 *work, int type, u32 *sizeOut);

/* Imports from SysMemUserForUser */
extern int sceKernelGetCompiledSdkVersion(void);

typedef struct SceMp3InitArg {
    u32 mp3StreamStart;
    u32 unk1;
    u32 mp3StreamEnd;
    u32 unk2;
    void *mp3Buf;
    s32 mp3BufSize;
    void *pcmBuf;
    s32 pcmBufSize;
} SceMp3InitArg;

typedef struct Mp3Info {
    s32 streamStart;    /* 0x00 */
    s32 streamEnd;      /* 0x04 */
    s32 streamPos;      /* 0x08 */
    s32 dataAvailable;  /* 0x0C */
    u32 headerMask;     /* 0x10 */
    s32 mpegVersion;    /* 0x14 */
    s32 samplingRate;   /* 0x18 */
    s32 bitRate;        /* 0x1C */
    s32 maxSample;      /* 0x20 */
    s32 frameNum;       /* 0x24 */
    s32 channelNum;     /* 0x28 */
    u32 vbrFlags;       /* 0x2C */
    u8 xingId[4];       /* 0x30 */
    u32 xingFlags;      /* 0x34 */
    u32 xingFrames;     /* 0x38 */
    u32 xingBytes;      /* 0x3C */
    u8 xingToc[100];    /* 0x40 */
    u32 xingScale;      /* 0xA4 */
    u8 vbriId[4];       /* 0xA8 */
    u16 vbriVersion;    /* 0xAC */
    u16 vbriDelay;      /* 0xAE */
    u16 vbriQuality;    /* 0xB0 */
    u16 padB2;          /* 0xB2 */
    u32 vbriBytes;      /* 0xB4 */
    u32 vbriFrames;     /* 0xB8 */
    u16 vbriTocEntries; /* 0xBC */
    u16 vbriTocScale;   /* 0xBE */
    u16 vbriEntryBytes; /* 0xC0 */
    u16 vbriEntryFrames;/* 0xC2 */
    u32 *vbriToc;       /* 0xC4 */
    SceUID fplId;       /* 0xC8 */
    s32 padCC[13];      /* 0xCC..0xFF */
} Mp3Info;

typedef struct Mp3State {
    s32 loopNum;        /* 0x00 */
    s32 endReached;     /* 0x04 */
    u8 *mp3Buf;         /* 0x08 */
    s32 mp3HalfSize;    /* 0x0C */
    s32 writeHalfIdx;   /* 0x10 */
    u8 *writePtr;       /* 0x14 */
    s32 bufferedBytes;  /* 0x18 */
    u8 *pcmBuf;         /* 0x1C */
    s32 pcmHalfSize;    /* 0x20 */
    s32 pcmHalfIdx;     /* 0x24 */
    u32 sumDecodedBytes;/* 0x28 */
    s32 streamStatus;   /* 0x2C (0x1AC) */
    s32 lowLevelStatus; /* 0x30 (0x1B0) */
    s32 pad34[3];       /* 0x34..0x3F */
} Mp3State;

typedef struct Mp3Context {
    u32 codecWork[32];  /* 0x00..0x7F */
    Mp3Info info;       /* 0x80..0x17F */
    Mp3State state;     /* 0x180..0x1BF */
} Mp3Context;

static s32 g_initParam = 1;       /* 0x2E40 */
static u32 g_edramAddr = 0;       /* 0x2E50 */
static u32 g_edramHalfSize = 0;   /* 0x2E54 */
static Mp3Context g_mp3Ctx[2];    /* 0x2E80..0x31FF */

static const s32 g_xingOffsets[4] = { 9, 17, 17, 32 };

static const s32 g_sampleRateTable[9] = {
    22050, 24000, 16000,
    44100, 48000, 32000,
    11025, 12000, 8000
};

static const s32 g_bitrateTable[48] = {
    /* MPEG 2 / 2.5 (index 0, 2) */
    0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0,
    /* MPEG 1 (index 1) */
    0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0,
    /* MPEG 2.5 */
    0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0
};

/* Forward declarations */
int sceMp3TermResource(void);
int sceMp3ReleaseMp3Handle(int handle);
int sceMp3ResetPlayPositionByFrame(int handle, int frame);
int sceMp3ResetPlayPosition(int handle);
static int alloc_edram(void);
static void check_and_flip_stream_buf(int handle);
static void compact_stream_buf(int handle);
static int manage_resources(int count);
static int reserve_handle_internal(const SceMp3InitArg *arg);
static int check_stream_header(int handle);
static int resync_next_frame(int handle);
static void check_stream_loop_or_advance(int handle);
static int parse_stream_info(int handle);
static int free_vbri_fpl(int handle);
static int parse_xing_header(int handle);
static int parse_vbri_header(int handle);
static s64 calc_frame_offset(int handle, int frame);
static s64 calc_xing_frame_offset(int handle, int frame);
static s64 calc_vbri_frame_offset(int handle, int frame);
static s64 calc_cbr_frame_offset(int handle, int frame);
static int parse_lowlevel_info(int handle);
static int mp3_memcmp(const void *s1, const void *s2, int n);
static const u8 *read_stream_bytes(void *dst, const u8 *src, u32 size, int bigEndian);
static int extract_codec_info(int handle);

/* 0x05d4: sub_05d4 */
static int alloc_edram(void)
{
    u32 *work = g_mp3Ctx[0].codecWork;
    int ret = sceAudiocodecCheckNeedMem(work, 0x1002);
    if (ret < 0) {
        return ret;
    }
    u32 alignedSize = (work[4] + 0x3F) & ~0x3FU;
    g_edramHalfSize = alignedSize;
    work[4] = alignedSize << 1;
    ret = sceAudiocodecGetEDRAM(work, 0x1002);
    if (ret < 0) {
        return ret;
    }
    g_edramAddr = work[3];
    return 0;
}

/* 0x0650: sub_0650 */
static void check_and_flip_stream_buf(int handle)
{
    Mp3Context *ctx = &g_mp3Ctx[handle];
    Mp3State *st = &ctx->state;
    s32 halfSize = st->mp3HalfSize;
    u8 *baseBuf = st->mp3Buf;
    s32 writeIdx = st->writeHalfIdx;
    u8 *readPtr = (u8 *)ctx->codecWork[6];

    s32 tailDist = (s32)((baseBuf + (halfSize << 1)) - readPtr) + 0x5C0;
    u8 *nextWriteEnd = baseBuf + halfSize * (writeIdx + 1) + 0x5C0;
    if (tailDist < 0x5A1) {
        compact_stream_buf(handle);
        halfSize = st->mp3HalfSize;
    }
    if (st->bufferedBytes < halfSize) {
        if (st->writePtr == nextWriteEnd) {
            s32 newIdx = st->writeHalfIdx ^ 1;
            st->writeHalfIdx = newIdx;
            st->writePtr = st->mp3Buf + halfSize * newIdx + 0x5C0;
        }
    }
}

/* 0x0720: sub_0720 */
static void compact_stream_buf(int handle)
{
    Mp3Context *ctx = &g_mp3Ctx[handle];
    Mp3State *st = &ctx->state;
    s32 halfSize = st->mp3HalfSize;
    u8 *baseBuf = st->mp3Buf;
    u8 *readPtr = (u8 *)ctx->codecWork[6];
    u32 rem = (u32)((baseBuf + (halfSize << 1)) - readPtr);
    u8 *dst = baseBuf - rem;
    sceKernelMemcpy(dst, readPtr, rem + 0x5C0);
    ctx->codecWork[6] = (u32)dst;
}

/* 0x0790: module_start / sceMp3_8AB81558 */
int module_start(SceSize args, void *argp) __attribute__((alias("sceMp3_8AB81558")));
int sceMp3_8AB81558(SceSize args, void *argp)
{
    s32 param = 1;
    if (args == 4 && argp != NULL) {
        param = *(s32 *)argp;
    }
    g_initParam = param;
    for (int i = 0; i < 2; i++) {
        g_mp3Ctx[i].info.fplId = -1;
    }
    return 0;
}

/* 0x07e0: sceMp3TermResource */
int sceMp3TermResource(void)
{
    for (int i = 0; i < 2; i++) {
        int ret = sceMp3ReleaseMp3Handle(i);
        if (ret < 0) {
            return ret;
        }
    }
    int ret = manage_resources(0);
    return (ret < 0) ? ret : 0;
}

/* 0x082c: sceMp3ReserveMp3Handle */
int sceMp3ReserveMp3Handle(const SceMp3InitArg *arg)
{
    if (arg != NULL) {
        if (arg->mp3Buf == NULL || arg->pcmBuf == NULL) {
            return 0x80671002;
        }
        s32 start = (s32)arg->mp3StreamStart;
        if (start < 0) {
            return 0x80671003;
        }
        s32 end = (s32)arg->mp3StreamEnd;
        if (start > end) {
            return 0x80671003;
        }
        if (start == end && (u32)arg->mp3StreamStart >= (u32)arg->mp3StreamEnd) {
            /* Note: checks arg->unk1 vs arg->unk2 for 64-bit stream offset */
            if (arg->unk1 >= arg->unk2) {
                return 0x80671003;
            }
        }
        if (arg->mp3BufSize < 0x2000 || arg->pcmBufSize < 0x2400) {
            return 0x80671003;
        }
    }
    return reserve_handle_internal(arg);
}

/* 0x08d8: sceMp3ReleaseMp3Handle */
int sceMp3ReleaseMp3Handle(int handle)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    int ret = free_vbri_fpl(handle);
    if (ret < 0) {
        return ret;
    }
    int intr = sceKernelCpuSuspendIntr();
    Mp3Context *ctx = &g_mp3Ctx[handle];
    if (ctx->state.streamStatus > 0) {
        ctx->state.streamStatus = 0;
        ctx->state.lowLevelStatus = 0;
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x0960: sceMp3Init */
int sceMp3Init(int handle)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    if (ctx->state.streamStatus < 2) {
        return 0x80671102;
    }
    int ret = sceAudiocodecInit(ctx->codecWork, 0x1002);
    if (ret < 0) {
        return ret;
    }
    ret = sceAudiocodecGetInfo(ctx->codecWork, 0x1002);
    if (ret < 0) {
        return ret;
    }
    ret = check_stream_header(handle);
    if (ret < 0) {
        return ret;
    }
    ret = parse_stream_info(handle);
    if (ret < 0) {
        return ret;
    }
    ctx->state.streamStatus = 3;
    return 0;
}

/* 0x0a34: sceMp3Decode */
int sceMp3Decode(int handle, void **pcmOut)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    Mp3State *st = &ctx->state;
    Mp3Info *info = &ctx->info;
    if (st->streamStatus < 3) {
        return 0x80671103;
    }
    if (pcmOut != NULL) {
        *pcmOut = (void *)ctx->codecWork[8];
    }
    if (st->endReached != 0) {
        sceKernelMemset((void *)ctx->codecWork[8], 0, st->pcmHalfSize);
        s32 nextHalf = st->pcmHalfIdx ^ 1;
        st->pcmHalfIdx = nextHalf;
        ctx->codecWork[8] = (u32)(st->pcmBuf + st->pcmHalfSize * nextHalf);
        return 0;
    }

    if (info->streamPos == info->streamEnd) {
        if (info->dataAvailable <= 0) {
            goto zero_fill;
        }
    } else if (st->bufferedBytes < 0x5A1) {
    zero_fill:
        sceKernelMemset((void *)ctx->codecWork[8], 0, st->pcmHalfSize);
        s32 nextHalf = st->pcmHalfIdx ^ 1;
        st->pcmHalfIdx = nextHalf;
        ctx->codecWork[8] = (u32)(st->pcmBuf + st->pcmHalfSize * nextHalf);
        return st->pcmHalfSize;
    }

    int ret = sceAudiocodecDecode(ctx->codecWork, 0x1002);
    if (ret < 0) {
        if (resync_next_frame(handle) == 0) {
            return ret;
        }
        check_stream_loop_or_advance(handle);
        return 0x80671402;
    }

    u32 decodedBytes = ctx->codecWork[9];
    if (decodedBytes == 0) {
        return 0x80671401;
    }

    u32 consumed = ctx->codecWork[7];
    st->bufferedBytes -= consumed;
    ctx->codecWork[6] += consumed;
    info->dataAvailable -= ctx->codecWork[7];

    s32 nextHalf = st->pcmHalfIdx ^ 1;
    st->pcmHalfIdx = nextHalf;
    st->sumDecodedBytes += decodedBytes;
    ctx->codecWork[8] = (u32)(st->pcmBuf + st->pcmHalfSize * nextHalf);
    check_stream_loop_or_advance(handle);
    return (int)ctx->codecWork[9];
}

/* 0x0c34: sceMp3GetInfoToAddStreamData */
int sceMp3GetInfoToAddStreamData(int handle, u8 **dstOut, s32 *toWriteOut, s32 *srcPosOut)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    Mp3State *st = &ctx->state;
    Mp3Info *info = &ctx->info;
    if (st->streamStatus < 2) {
        return 0x80671102;
    }

    u8 *writeLimit = st->mp3Buf + st->mp3HalfSize * (st->writeHalfIdx + 1) + 0x5C0;
    if (info->streamPos == info->streamEnd || st->writePtr >= writeLimit) {
        if (srcPosOut != NULL) {
            *srcPosOut = 0;
        }
        if (dstOut != NULL) {
            *dstOut = NULL;
        }
        if (toWriteOut != NULL) {
            *toWriteOut = 0;
        }
        return 0;
    }

    if (srcPosOut != NULL) {
        *srcPosOut = info->streamPos;
    }
    if (dstOut != NULL) {
        *dstOut = st->writePtr;
    }
    if (toWriteOut != NULL) {
        *toWriteOut = (s32)(writeLimit - st->writePtr);
    }
    return 0;
}

/* 0x0d14: sceMp3NotifyAddStreamData */
int sceMp3NotifyAddStreamData(int handle, int size)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    Mp3State *st = &ctx->state;
    Mp3Info *info = &ctx->info;
    if (st->streamStatus < 2) {
        return 0x80671102;
    }

    u8 *writeLimit = st->mp3Buf + st->mp3HalfSize * (st->writeHalfIdx + 1) + 0x5C0;
    s32 maxWrite = (s32)(writeLimit - st->writePtr);
    s32 remStream = info->streamEnd - info->streamPos;
    if (size < 0 || size > maxWrite) {
        return 0x80671003;
    }

    if (remStream < size) {
        info->streamPos = info->streamEnd;
        st->writePtr += remStream;
        st->bufferedBytes += remStream;
    } else {
        info->streamPos += size;
        st->writePtr += size;
        st->bufferedBytes += size;
    }
    check_and_flip_stream_buf(handle);
    return 0;
}

/* 0x0e0c: sceMp3ResetPlayPositionByFrame */
int sceMp3ResetPlayPositionByFrame(int handle, int frame)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    Mp3State *st = &ctx->state;
    Mp3Info *info = &ctx->info;
    if (st->streamStatus < 3) {
        return 0x80671103;
    }

    s64 pos = calc_frame_offset(handle, frame);
    if (pos < 0) {
        return (int)pos;
    }
    if (pos >= (s64)info->streamEnd) {
        return 0x80671501;
    }

    s32 pos32 = (s32)pos;
    info->streamPos = pos32;
    st->writeHalfIdx = (g_initParam != 0) ? 1 : 0;
    st->bufferedBytes = 0;
    ctx->codecWork[6] = (u32)(st->mp3Buf + 0x5C0);
    info->dataAvailable = info->streamEnd - pos32;
    st->writePtr = st->mp3Buf + 0x5C0;
    st->sumDecodedBytes = (u32)(frame * info->maxSample) << 2;
    st->endReached = 0;
    return 0;
}

/* 0x0f2c: sceMp3ResetPlayPosition */
int sceMp3ResetPlayPosition(int handle)
{
    return sceMp3ResetPlayPositionByFrame(handle, 0);
}

/* 0x0f48: sceMp3SetLoopNum */
int sceMp3SetLoopNum(int handle, int loop)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    if (ctx->state.streamStatus < 2) {
        return 0x80671102;
    }
    if (loop < 0) {
        loop = -1;
    }
    ctx->state.loopNum = loop;
    return 0;
}

/* 0x0fa0: sceMp3LowLevelInit */
int sceMp3LowLevelInit(int handle, void * headerData)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    if (ctx->state.lowLevelStatus < 2) {
        return 0x80671102;
    }
    if (sceAudiocodecInit(ctx->codecWork, 0x1002) != 0) {
        return 0x80672001;
    }
    if (headerData != NULL) {
        ctx->codecWork[6] = (u32)headerData;
        if (sceAudiocodecGetInfo(ctx->codecWork, 0x1002) != 0) {
            return 0x80672001;
        }
        int ret = parse_lowlevel_info(handle);
        if (ret < 0) {
            return ret;
        }
    }
    ctx->state.lowLevelStatus = 3;
    return 0;
}

/* 0x1090: sceMp3LowLevelDecode */
int sceMp3LowLevelDecode(int handle, const void *src, s32 *srcConsumed, void *dst, s32 *sampleBytesWritten)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    if (ctx->state.lowLevelStatus < 3) {
        return 0x80671103;
    }
    if (src == NULL || dst == NULL) {
        return 0x80671002;
    }
    if (((u32)dst & 1) != 0) {
        return 0x80671004;
    }
    ctx->codecWork[8] = (u32)dst;
    ctx->codecWork[6] = (u32)src;
    if (sceAudiocodecDecode(ctx->codecWork, 0x1002) != 0) {
        return 0x80672001;
    }
    if (srcConsumed != NULL) {
        *srcConsumed = (s32)ctx->codecWork[7];
    }
    if (sampleBytesWritten != NULL) {
        *sampleBytesWritten = (s32)ctx->codecWork[9];
    }
    return 0;
}

/* 0x1170: module_stop / sceMp3_732B042A */
int module_stop(SceSize args, void *argp) __attribute__((alias("sceMp3_732B042A")));
int sceMp3_732B042A(SceSize args, void *argp)
{
    int ret = sceMp3TermResource();
    return (ret < 0) ? ret : 0;
}

/* 0x1194: sceMp3InitResource */
int sceMp3InitResource(void)
{
    int ret = manage_resources(2);
    return (ret < 0) ? ret : 0;
}

/* 0x11b8: sceMp3CheckStreamDataNeeded */
int sceMp3CheckStreamDataNeeded(int handle)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    Mp3State *st = &ctx->state;
    Mp3Info *info = &ctx->info;
    if (st->streamStatus < 2) {
        return 0x80671102;
    }
    if (info->streamPos == info->streamEnd) {
        return 0;
    }
    u8 *writeLimit = st->mp3Buf + st->mp3HalfSize * (st->writeHalfIdx + 1) + 0x5C0;
    return (st->writePtr < writeLimit) ? 1 : 0;
}

/* 0x1248: sceMp3GetFrameNum */
int sceMp3GetFrameNum(int handle)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    if (ctx->state.streamStatus < 3) {
        return 0x80671103;
    }
    return ctx->info.frameNum;
}

/* 0x1294: sceMp3GetLoopNum */
int sceMp3GetLoopNum(int handle)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    if (ctx->state.streamStatus < 2) {
        return 0x80671102;
    }
    return ctx->state.loopNum;
}

/* 0x12e0: sceMp3GetMaxOutputSample */
int sceMp3GetMaxOutputSample(int handle)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    if (ctx->state.streamStatus < 3 && ctx->state.lowLevelStatus < 3) {
        return 0x80671103;
    }
    return ctx->info.maxSample;
}

/* 0x1340: sceMp3GetSumDecodedSample */
int sceMp3GetSumDecodedSample(int handle)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    if (ctx->state.streamStatus < 2) {
        return 0x80671102;
    }
    return (int)(ctx->state.sumDecodedBytes >> 2);
}

/* 0x1390: sceMp3GetMPEGVersion */
int sceMp3GetMPEGVersion(int handle)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    if (ctx->state.streamStatus < 3 && ctx->state.lowLevelStatus < 3) {
        return 0x80671102;
    }
    return ctx->info.mpegVersion;
}

/* 0x13f0: sceMp3GetSamplingRate */
int sceMp3GetSamplingRate(int handle)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    if (ctx->state.streamStatus < 3 && ctx->state.lowLevelStatus < 3) {
        return 0x80671103;
    }
    return ctx->info.samplingRate;
}

/* 0x1450: sceMp3GetBitRate */
int sceMp3GetBitRate(int handle)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    if (ctx->state.streamStatus < 3 && ctx->state.lowLevelStatus < 3) {
        return 0x80671103;
    }
    return ctx->info.bitRate;
}

/* 0x14b0: sceMp3GetMp3ChannelNum */
int sceMp3GetMp3ChannelNum(int handle)
{
    if ((u32)handle >= 2) {
        return 0x80671001;
    }
    Mp3Context *ctx = &g_mp3Ctx[handle];
    if (ctx->state.streamStatus < 3 && ctx->state.lowLevelStatus < 3) {
        return 0x80671103;
    }
    return ctx->info.channelNum;
}

/* 0x1510: sub_1510 */
static int manage_resources(int count)
{
    if (count >= 3) {
        return 0x80671001;
    }
    int intr = sceKernelCpuSuspendIntr();
    for (int i = 0; i < 2; i++) {
        if (g_mp3Ctx[i].state.streamStatus > 0) {
            sceKernelCpuResumeIntr(intr);
            return 0x80671101;
        }
    }
    for (int i = 0; i < 2; i++) {
        g_mp3Ctx[i].state.streamStatus = -1;
        g_mp3Ctx[i].state.lowLevelStatus = -1;
    }
    sceKernelCpuResumeIntr(intr);

    if (count == 0) {
        if (g_edramAddr != 0) {
            g_edramAddr = 0;
            g_edramHalfSize = 0;
            int ret = sceAudiocodecReleaseEDRAM(g_mp3Ctx[0].codecWork);
            if (ret < 0) {
                return ret;
            }
        }
        return 0;
    }

    if (g_edramAddr == 0) {
        int ret = alloc_edram();
        if (ret < 0) {
            return ret;
        }
    }

    for (int i = 0; i < count; i++) {
        Mp3Context *ctx = &g_mp3Ctx[i];
        ctx->codecWork[5] = 1;
        ctx->state.streamStatus = 0;
        ctx->codecWork[3] = g_edramAddr + g_edramHalfSize * i;
        ctx->state.lowLevelStatus = 0;
        sceKernelMemset(&ctx->info, 0, sizeof(Mp3Info));
        sceKernelMemset(&ctx->state, 0, 0x2C);
        ctx->info.fplId = -1;
    }
    return 0;
}

/* 0x16f0: sub_16f0 */
static int reserve_handle_internal(const SceMp3InitArg *arg)
{
    int handle = -1;
    int intr = sceKernelCpuSuspendIntr();
    for (int i = 0; i < 2; i++) {
        if (g_mp3Ctx[i].state.streamStatus == 0) {
            handle = i;
            g_mp3Ctx[i].state.lowLevelStatus = 1;
            g_mp3Ctx[i].state.streamStatus = 1;
            break;
        }
    }
    sceKernelCpuResumeIntr(intr);

    if (handle < 0) {
        return 0x80671201;
    }

    Mp3Context *ctx = &g_mp3Ctx[handle];
    Mp3State *st = &ctx->state;
    Mp3Info *info = &ctx->info;

    if (arg != NULL) {
        sceKernelMemset(info, 0, sizeof(Mp3Info));
        info->fplId = -1;
        info->dataAvailable = (s32)(arg->mp3StreamEnd - arg->mp3StreamStart);
        info->streamStart = (s32)arg->mp3StreamStart;
        info->streamPos = (s32)arg->mp3StreamStart;
        info->streamEnd = (s32)arg->mp3StreamEnd;
        info->vbrFlags = 0;

        st->loopNum = -1;
        st->endReached = 0;
        st->mp3Buf = (u8 *)arg->mp3Buf;
        u8 *writeStart = (u8 *)arg->mp3Buf + 0x5C0;
        st->writePtr = writeStart;
        st->mp3HalfSize = (arg->mp3BufSize - 0x5C0) / 2;
        st->writeHalfIdx = (g_initParam != 0) ? 1 : 0;
        st->bufferedBytes = 0;
        st->pcmBuf = (u8 *)arg->pcmBuf;
        ctx->codecWork[6] = (u32)writeStart;
        st->pcmHalfIdx = 0;
        st->pcmHalfSize = arg->pcmBufSize / 2;
        st->sumDecodedBytes = 0;
        ctx->codecWork[8] = (u32)st->pcmBuf;
        st->streamStatus = 2;
    } else {
        sceKernelMemset(info, 0, sizeof(Mp3Info));
        sceKernelMemset(st, 0, 0x2C);
        info->fplId = -1;
        st->lowLevelStatus = 2;
    }
    ctx->codecWork[10] = 0x5A1;
    return handle;
}

/* 0x18b8: sub_18b8 */
static int check_stream_header(int handle)
{
    Mp3Context *ctx = &g_mp3Ctx[handle];
    u32 layer = ctx->codecWork[15];
    u32 sampleRateIdx = ctx->codecWork[18];
    u32 mpegVer = ctx->codecWork[14];
    if (layer != 3) {
        return 0x80671301;
    }
    int sdkVer = sceKernelGetCompiledSdkVersion();
    if (sdkVer > 0x05FFFFFF) {
        if (mpegVer >= 3) {
            return 0x80671301;
        }
    } else {
        if (mpegVer != 1) {
            return 0x80671301;
        }
    }
    sdkVer = sceKernelGetCompiledSdkVersion();
    if (sdkVer > 0x030904FF) {
        if (sampleRateIdx >= 3) {
            return 0x80671302;
        }
    } else {
        if (sampleRateIdx != 0) {
            return 0x80671302;
        }
    }
    return 0;
}

/* 0x1994: sub_1994 */
static int resync_next_frame(int handle)
{
    Mp3Context *ctx = &g_mp3Ctx[handle];
    u8 *readPtr = (u8 *)ctx->codecWork[6];
    u32 mask = ctx->info.headerMask;
    int found = 0;
    s32 offset = 1;
    while (offset < 0x5A2) {
        const u8 *p = readPtr + offset;
        u32 hdr = ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
        if ((hdr & 0xFFFE0C00U) == mask) {
            found = 1;
            break;
        }
        offset++;
    }
    ctx->codecWork[6] = (u32)(readPtr + offset);
    ctx->state.bufferedBytes -= offset;
    ctx->info.dataAvailable -= offset;
    return found;
}

/* 0x1a44: sub_1a44 */
static void check_stream_loop_or_advance(int handle)
{
    Mp3Context *ctx = &g_mp3Ctx[handle];
    Mp3Info *info = &ctx->info;
    Mp3State *st = &ctx->state;
    if (info->streamPos == info->streamEnd && info->dataAvailable <= 0) {
        s32 loop = st->loopNum;
        if (loop == 0) {
            st->endReached = 1;
            return;
        }
        if (loop > 0) {
            st->loopNum = loop - 1;
        }
        sceMp3ResetPlayPosition(handle);
        return;
    }
    check_and_flip_stream_buf(handle);
}

/* 0x1ac8: sub_1ac8 */
static int parse_stream_info(int handle)
{
    Mp3Context *ctx = &g_mp3Ctx[handle];
    Mp3Info *info = &ctx->info;
    int ret = extract_codec_info(handle);
    if (ret < 0) {
        return ret;
    }
    if (sceKernelGetCompiledSdkVersion() > 0x05FFFFFF) {
        ret = parse_xing_header(handle);
        if (ret < 0) {
            return ret;
        }
        ret = parse_vbri_header(handle);
        if (ret < 0) {
            return ret;
        }
    }
    if (info->vbrFlags == 0) {
        s64 denom = (s64)info->bitRate * (s64)info->maxSample * 1000LL;
        s64 num = ((s64)(info->streamEnd - info->streamStart) * (s64)info->samplingRate) << 3;
        info->frameNum = (s32)(num / denom);
    }
    return 0;
}

/* 0x1bd0: sub_1bd0 */
static int free_vbri_fpl(int handle)
{
    Mp3Info *info = &g_mp3Ctx[handle].info;
    int err = 0;
    if (info->vbriToc != NULL) {
        int r = sceKernelFreeFpl(info->fplId, info->vbriToc);
        info->vbriToc = NULL;
        if (r < 0) {
            err = r;
        }
    }
    if (info->fplId >= 0) {
        int r = sceKernelDeleteFpl(info->fplId);
        info->fplId = -1;
        if (r < 0) {
            err = r;
        }
    }
    return err;
}

/* 0x1c60: sub_1c60 */
static int parse_xing_header(int handle)
{
    Mp3Context *ctx = &g_mp3Ctx[handle];
    Mp3Info *info = &ctx->info;
    Mp3State *st = &ctx->state;

    u32 idx = ((info->mpegVersion & 1) << 1) + info->channelNum - 1;
    if (idx >= 4) {
        return 0x800001FE;
    }
    s32 offset = g_xingOffsets[idx];
    if (st->bufferedBytes < offset + 0x7C) {
        return 0x800001FE;
    }

    const u8 *p = (const u8 *)ctx->codecWork[6] + offset + 4;
    p = read_stream_bytes(info->xingId, p, 4, 0);
    if (mp3_memcmp(info->xingId, "Xing", 4) != 0 && mp3_memcmp(info->xingId, "Info", 4) != 0) {
        return 0;
    }

    p = read_stream_bytes(&info->xingFlags, p, 4, 1);
    u32 flags = info->xingFlags;
    if ((flags & 1) != 0) {
        p = read_stream_bytes(&info->xingFrames, p, 4, 1);
    }
    if ((flags & 2) != 0) {
        p = read_stream_bytes(&info->xingBytes, p, 4, 1);
    }
    if ((flags & 4) != 0) {
        p = read_stream_bytes(info->xingToc, p, 100, 0);
    }
    if ((flags & 8) != 0) {
        p = read_stream_bytes(&info->xingScale, p, 4, 1);
    }

    if (mp3_memcmp(info->xingId, "Xing", 4) == 0) {
        info->frameNum = (s32)info->xingFrames;
        info->vbrFlags |= 1;
    }
    return 0;
}

/* 0x1e5c: sub_1e5c */
static int parse_vbri_header(int handle)
{
    Mp3Context *ctx = &g_mp3Ctx[handle];
    Mp3Info *info = &ctx->info;
    Mp3State *st = &ctx->state;

    u32 idx = ((info->mpegVersion & 1) << 1) + info->channelNum - 1;
    if (idx >= 4) {
        return 0x800001FE;
    }
    s32 xingOff = g_xingOffsets[idx];
    if (st->bufferedBytes < xingOff + 0x1E) {
        return 0x800001FE;
    }

    s32 vbriOff = xingOff + 4;
    const u8 *p = (const u8 *)ctx->codecWork[6] + vbriOff;
    p = read_stream_bytes(info->vbriId, p, 4, 0);
    if (mp3_memcmp(info->vbriId, "VBRI", 4) != 0) {
        return 0;
    }

    p = read_stream_bytes(&info->vbriVersion, p, 2, 1);
    p = read_stream_bytes(&info->vbriDelay, p, 2, 1);
    p = read_stream_bytes(&info->vbriQuality, p, 2, 0);
    p = read_stream_bytes(&info->vbriBytes, p, 4, 1);
    p = read_stream_bytes(&info->vbriFrames, p, 4, 1);
    p = read_stream_bytes(&info->vbriTocEntries, p, 2, 1);
    p = read_stream_bytes(&info->vbriTocScale, p, 2, 1);
    p = read_stream_bytes(&info->vbriEntryBytes, p, 2, 1);
    p = read_stream_bytes(&info->vbriEntryFrames, p, 2, 1);

    u16 entryBytes = info->vbriEntryBytes;
    if ((u16)(entryBytes - 1) < 4) {
        u32 tocDataBytes = (u32)(info->vbriTocEntries + 1) * entryBytes;
        if (st->bufferedBytes < vbriOff + (s32)tocDataBytes + 0x1A) {
            return 0x800001FE;
        }
        int ret = free_vbri_fpl(handle);
        if (ret < 0) {
            return ret;
        }
        u32 fplBlockSize = ((u32)info->vbriTocEntries << 2) + 4;
        SceUID fpl = sceKernelCreateFpl("SceLibmp3Fpl", 2, 0, fplBlockSize, 1, NULL);
        info->fplId = fpl;
        if (fpl < 0) {
            int r2 = free_vbri_fpl(handle);
            return (r2 < 0) ? r2 : fpl;
        }
        ret = sceKernelAllocateFpl(fpl, (void **)&info->vbriToc, NULL);
        if (ret < 0) {
            int r2 = free_vbri_fpl(handle);
            return (r2 < 0) ? r2 : ret;
        }
        sceKernelMemset(info->vbriToc, 0, fplBlockSize);
        u32 count = (u32)info->vbriTocEntries + 1;
        for (u32 i = 0; i < count; i++) {
            p = read_stream_bytes(&info->vbriToc[i], p, info->vbriEntryBytes, 1);
            info->vbriToc[i] *= info->vbriTocScale;
        }
    }

    info->frameNum = (s32)info->vbriFrames;
    info->vbrFlags |= 0x100;
    return 0;
}

/* 0x2168: sub_2168 */
static s64 calc_frame_offset(int handle, int frame)
{
    Mp3Info *info = &g_mp3Ctx[handle].info;
    if ((u32)frame >= (u32)info->frameNum) {
        return (s64)(s32)0x80671003;
    }
    s64 relOffset;
    if ((info->vbrFlags & 1) != 0) {
        relOffset = calc_xing_frame_offset(handle, frame);
    } else if ((info->vbrFlags & 0x100) != 0) {
        relOffset = calc_vbri_frame_offset(handle, frame);
    } else {
        relOffset = calc_cbr_frame_offset(handle, frame);
    }
    if (relOffset < 0) {
        return relOffset;
    }
    return (s64)info->streamStart + relOffset;
}

/* 0x2224: sub_2224 */
static s64 calc_xing_frame_offset(int handle, int frame)
{
    Mp3Info *info = &g_mp3Ctx[handle].info;
    u32 totalFrames = (u32)info->frameNum;
    if (totalFrames == 0) {
        return (s64)(s32)0x800001FE;
    }
    u32 scaled = (u32)(frame * 100);
    u32 tocIdx = scaled / totalFrames;
    u32 rem = scaled - tocIdx * totalFrames;
    s32 tocVal = info->xingToc[tocIdx];
    s32 diff = 0;
    if ((s32)tocIdx < 99) {
        diff = (s32)info->xingToc[tocIdx + 1] - tocVal;
    }
    s64 numer = (s64)(info->streamEnd - info->streamStart) * (s64)(tocVal * (s32)totalFrames + diff * (s32)rem);
    s64 denom = (s64)((u64)totalFrames * 256ULL);
    return numer / denom;
}

/* 0x2308: sub_2308 */
static s64 calc_vbri_frame_offset(int handle, int frame)
{
    Mp3Info *info = &g_mp3Ctx[handle].info;
    u32 entryFrames = info->vbriEntryFrames;
    if (entryFrames == 0) {
        return (s64)(s32)0x800001FE;
    }
    u32 *toc = info->vbriToc;
    if (toc == NULL) {
        return (s64)(s32)0x80000103;
    }
    u32 count = (u32)info->vbriTocEntries + 1;
    u32 curFrame = 0;
    u64 sumBytes = 0;
    u32 idx = 0;
    while (idx < count) {
        u32 entryBytes = toc[idx];
        curFrame += entryFrames;
        sumBytes += entryBytes;
        idx++;
        if (curFrame >= (u32)frame) {
            break;
        }
    }
    u32 lastBytes = toc[idx];
    u32 prevFrame = curFrame - entryFrames;
    u32 remFrames = (u32)frame - prevFrame;
    sumBytes -= lastBytes;
    u64 interp = ((u64)lastBytes * (u64)remFrames) / (u64)entryFrames;
    return (s64)(sumBytes + interp);
}

/* 0x2434: sub_2434 */
static s64 calc_cbr_frame_offset(int handle, int frame)
{
    Mp3Info *info = &g_mp3Ctx[handle].info;
    if (info->samplingRate == 0) {
        return (s64)(s32)0x800001FE;
    }
    s64 numer = (s64)frame * (s64)info->maxSample * (s64)info->bitRate * 1000LL;
    s64 denom = (s64)info->samplingRate * 8LL;
    s64 res = numer / denom;
    if (res > 0) {
        res -= 1;
    }
    return res;
}

/* 0x2540: sub_2540 */
static int parse_lowlevel_info(int handle)
{
    return extract_codec_info(handle);
}

/* 0x255c: sub_255c */
static int mp3_memcmp(const void *s1, const void *s2, int n)
{
    if ((u32)(n - 1) >= 0x80U) {
        return 0x80671003;
    }
    const s8 *p1 = (const s8 *)s1;
    const s8 *p2 = (const s8 *)s2;
    for (int i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return -1;
        }
    }
    return 0;
}

/* 0x25cc: sub_25cc */
static const u8 *read_stream_bytes(void *dst, const u8 *src, u32 size, int bigEndian)
{
    u8 *d = (u8 *)dst;
    if (size == 1) {
        d[0] = src[0];
    } else if (size == 2) {
        if (bigEndian == 1) {
            d[0] = src[1];
            d[1] = src[0];
        } else {
            d[0] = src[0];
            d[1] = src[1];
        }
    } else if (size == 3) {
        if (bigEndian == 1) {
            d[0] = src[3];
            d[1] = src[2];
            d[2] = src[1];
        } else {
            d[0] = src[0];
            d[1] = src[1];
            d[2] = src[2];
        }
    } else if (size == 4) {
        if (bigEndian == 1) {
            d[0] = src[3];
            d[1] = src[2];
            d[2] = src[1];
            d[3] = src[0];
        } else {
            d[0] = src[0];
            d[1] = src[1];
            d[2] = src[2];
            d[3] = src[3];
        }
    } else {
        if (bigEndian != 1) {
            sceKernelMemcpy(dst, src, size);
        }
    }
    return src + size;
}

/* 0x26fc: sub_26fc */
static int extract_codec_info(int handle)
{
    u32 maxOutBytes = 0;
    Mp3Context *ctx = &g_mp3Ctx[handle];
    Mp3Info *info = &ctx->info;

    info->headerMask = 0xFFE20000U;
    s32 rawVer = (s32)ctx->codecWork[14];
    s32 ver;
    if (rawVer == 1) {
        ver = 3;
    } else if (rawVer == 0) {
        ver = 2;
    } else if (rawVer == 2) {
        ver = 0;
    } else {
        ver = (s32)0x80671301;
    }
    info->mpegVersion = ver;
    info->headerMask |= ((u32)ver << 19);
    if (ver < 0) {
        return ver;
    }

    int ret = sceAudiocodecAlcExtendParameter(ctx->codecWork, 0x1002, &maxOutBytes);
    s32 maxSamples = (ret < 0) ? ret : (s32)(maxOutBytes >> 2);
    info->maxSample = maxSamples;
    if (maxSamples <= 0) {
        return maxSamples;
    }

    info->headerMask |= (ctx->codecWork[18] << 10);
    s32 sr = g_sampleRateTable[ctx->codecWork[14] * 3 + ctx->codecWork[18]];
    info->samplingRate = sr;
    if (sr <= 0) {
        return sr;
    }

    s32 br = g_bitrateTable[(ctx->codecWork[14] << 4) + ctx->codecWork[17]];
    info->bitRate = br;
    if (br <= 0) {
        return br;
    }

    info->channelNum = (ctx->codecWork[21] == 3) ? 1 : 2;
    return 0;
}
