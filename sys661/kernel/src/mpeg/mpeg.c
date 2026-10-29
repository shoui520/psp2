/*
 * ePSP 6.61 sceMpeg_library (mpeg.prx)
 */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;
typedef long long s64;
typedef unsigned long long u64;
typedef int SceUID;
typedef unsigned int SceSize;

/* Imports */
extern void *sceKernelMemset(void *dst, int val, SceSize size);
extern int sceKernelLockLwMutex(void *work, int count, unsigned int *timeout);
extern int _sceKernelCheckThreadStack(void);
extern int sceKernelUnlockLwMutex(void *work, int count);
extern void *sceKernelMemcpy(void *dst, const void *src, SceSize size);
extern SceUID sceKernelCreateFpl(const char *name, SceUID mpid, u32 attr, SceSize blocksize, int numblocks, void *opt);
extern int sceKernelAllocateFpl(SceUID uid, void **data, unsigned int *timeout);
extern int sceKernelDeleteFpl(SceUID uid);
extern int sceKernelFreeFpl(SceUID uid, void *data);
extern int sceKernelDeleteLwMutex(void *work);
extern int sceKernelCreateLwMutex(void *work, const char *name, u32 attr, int count, void *opt);
extern int sceKernelDcacheWritebackInvalidateRange(const void *p, unsigned int size);
extern int sceKernelDcacheWritebackRange(const void *p, unsigned int size);
extern int sceAudiocodecReleaseEDRAM(unsigned long *work);
extern int sceAudiocodecGetEDRAM(unsigned long *work, int type);
extern int sceAudiocodecInit(unsigned long *work, int type);
extern int sceAudiocodecDecode(unsigned long *work, int type);
extern int sceAudiocodecCheckNeedMem(unsigned long *work, int type);
extern int sceGeListEnQueue(const void *list, void *stall, int cbid, void *arg);
extern int sceMpegbase_0530BE4E(void *p);
extern int sceMpegbase_119D5DDA(void *p);
extern int sceMpegBaseCscAvcRange(void *a0, u32 a1, void *a2, void *a3);
extern int sceMpegbase_31007FF8(u32 a0);
extern int sceMpegBaseCscInit(u32 a0);
extern int sceMpegBaseYCrCbCopy(void *dst, const void *src, u32 flags);
extern int sceMpegBaseCscAvc(void *a0, u32 a1, u32 a2, void *a3);
extern int sceMpegBasePESpacketCopy(void *p);
extern int sceVideocodecInit(unsigned long *work, int type);
extern int sceVideocodecGetFrameCrop(unsigned long *work, int type);
extern int sceVideocodecGetVersion(unsigned long *work, int type);
extern int sceVideocodecGetEDRAM(unsigned long *work, int type);
extern int sceVideocodecDelete(unsigned long *work, int type);
extern int sceVideocodecReleaseEDRAM(unsigned long *work);
extern int sceVideocodecGetSEI(unsigned long *work, int type);
extern int sceVideocodecSetMemory(unsigned long *work, int type);
extern int sceVideocodec_893B32B1(unsigned long *work, int type);
extern int sceVideocodecStop(unsigned long *work, int type);
extern int sceVideocodecOpen(unsigned long *work, int type);
extern int sceVideocodec_D95C24D5(unsigned long *work, int type);
extern int sceVideocodecDecode(unsigned long *work, int type);

#define NOINLINE __attribute__((noinline))

/* .rodata & .data */
const char g_ver_str[] = "LIBMPEG005.000";
const int g_es_sizes[2] = { 0x840, 0x336 };

struct StreamDesc {
    u32 prefix;
    u32 mask;
};

const struct StreamDesc g_stream_descs[19] = {
    { 0xE0000000, 0xFF000000 },
    { 0xC0000000, 0xFF000000 },
    { 0xBD000000, 0xFFFF0000 },
    { 0xBD100000, 0xFFFF0000 },
    { 0xBD200000, 0xFFFF0000 },
    { 0xBD300000, 0xFFFF0000 },
    { 0xBD400000, 0xFFFF0000 },
    { 0xBD800000, 0xFFFF0000 },
    { 0xBD880000, 0xFFFF0000 },
    { 0xBD900000, 0xFFFF0000 },
    { 0xBDA00000, 0xFFFF0000 },
    { 0xBF01E000, 0xBF01E000 },
    { 0xBD000000, 0xFFFFFF00 },
    { 0xFFFFFFFF, 0xFFFFFFFF },
    { 0xFFFFFFFF, 0xFFFFFFFF },
    { 0xFFFFFFFF, 0xFFFFFFFF },
    { 0xFFFFFFFF, 0xFFFFFFFF },
    { 0xFFFFFFFF, 0xFFFFFFFF },
    { 0xFFFFFFFF, 0xFFFFFFFF },
};

static u32 g_init_flags;
static void *g_video_work_ptr;
static void *g_video_sub_ptr;
static u32 g_avc_res[2];
static u32 g_video_work_buf[24];
static u32 g_video_sub_buf[16];
static void *g_audio_work_ptr;
static u32 g_audio_work_buf[32];
static u32 g_audio_mode_val;
static u32 g_video_mutex[8];
static u32 g_audio_mutex[8];
static u32 g_res_mutex[8];
static u32 g_mpeg_mutex[8];

/* Forward declarations */
static NOINLINE int sub_69d0(void *work, int count, const char *name);
static NOINLINE int sub_69f8(void *work);
static NOINLINE int sub_6a14(void *work);
static NOINLINE int sub_6a34(void *work);
static NOINLINE int sub_6a50(void);
static NOINLINE void *sub_0b40(void);
static NOINLINE u32 sub_0b4c(void);
static NOINLINE void sub_0b58(u32 v);
static NOINLINE int sub_0b64(void);
static NOINLINE void sub_0b70(int v);
static NOINLINE int sub_6cc4(void);
static NOINLINE int sub_6d6c(u32 *out);
static NOINLINE int sub_6db0(u32 *vwork, void *s1, void *s0, void *s2, u32 t0, u32 *t1, u32 t2, u32 t3, u32 sp0, u32 sp1, u32 sp2);
static NOINLINE int sub_6f28(void);
static NOINLINE u32 sub_6f4c(void);
static NOINLINE int sub_6f5c(void *vwork);
static NOINLINE int sub_6fb0(void *vwork);
static NOINLINE int sub_7004(void *vwork);
static NOINLINE int sub_7070(u32 *vwork, u32 val);
static NOINLINE int sub_70d4(int mode);
static NOINLINE int sub_7160(u32 *out, int mode, u32 *out_flag);
static NOINLINE int sub_721c(u32 *awork, u32 edram, u32 size, const void *p1, const void *p2);
static NOINLINE int sub_732c(void);
static NOINLINE int sub_7364(u32 *out, int mode);
static NOINLINE u32 sub_73c8(u32 *out);
static NOINLINE int sub_73e8(void *awork);
static NOINLINE int sub_7454(const u8 *s1, const u8 *s2, int n);
static NOINLINE int sub_749c(const char *s1, const char *s2);
static NOINLINE int sub_751c(int x);
static NOINLINE u32 sub_752c(int type, int ch);
static NOINLINE int sub_75c0(int *out_type, int *out_ch, u32 code);
static NOINLINE u32 sub_780c(int type);
static NOINLINE u32 sub_7838(const u8 *p);
static NOINLINE u32 sub_7850(const u8 *p);
static NOINLINE u32 sub_7870(const u8 *p);
static NOINLINE u32 sub_789c(const u8 *p, int len);

/* Implementations of low-level helpers */

static NOINLINE int sub_69d0(void *work, int count, const char *name)
{
    return sceKernelCreateLwMutex(work, name, 0x300, count, 0);
}

static NOINLINE int sub_69f8(void *work)
{
    return sceKernelDeleteLwMutex(work);
}

static NOINLINE int sub_6a14(void *work)
{
    return sceKernelLockLwMutex(work, 1, 0);
}

static NOINLINE int sub_6a34(void *work)
{
    return sceKernelUnlockLwMutex(work, 1);
}

static NOINLINE int sub_6a50(void)
{
    return _sceKernelCheckThreadStack();
}

static NOINLINE void *sub_0b40(void)
{
    return g_avc_res;
}

static NOINLINE u32 sub_0b4c(void)
{
    return g_init_flags;
}

static NOINLINE void sub_0b58(u32 v)
{
    g_init_flags = v;
}

static NOINLINE int sub_6a6c(void)
{
    u32 *res = (u32 *)sub_0b40();
    u32 opt[2];
    void *block = 0;
    int ret = 0x80618005;
    opt[0] = 8;
    opt[1] = 0x20000;
    sub_6a14(g_res_mutex);
    if (!(sub_0b4c() & 4)) {
        SceUID fpl = sceKernelCreateFpl("SceMpegAvcResource", 2, 0, 0x20000, 1, opt);
        ret = fpl;
        if (fpl >= 0) {
            ret = sceKernelAllocateFpl(fpl, &block, 0);
            if (ret < 0) {
                sceKernelDeleteFpl(fpl);
            } else {
                res[0] = fpl;
                res[1] = (u32)block;
                sub_0b58(sub_0b4c() | 4);
            }
        }
    }
    sub_6a34(g_res_mutex);
    return ret;
}

static NOINLINE int sub_6b64(void)
{
    u32 *res = (u32 *)sub_0b40();
    int val = 0;
    sub_6a14(g_res_mutex);
    if (sub_0b4c() & 4) {
        val = res[1];
    }
    sub_6a34(g_res_mutex);
    return val;
}

static NOINLINE int sub_6bc8(void)
{
    u32 *res = (u32 *)sub_0b40();
    u32 val = 0;
    sub_6a14(g_res_mutex);
    if (sub_0b4c() & 4) {
        val = res[1] & 0xFFC00000;
    }
    sub_6a34(g_res_mutex);
    return val;
}

static NOINLINE int sub_6c30(void)
{
    u32 *res = (u32 *)sub_0b40();
    int ret = 0x80618009;
    sub_6a14(g_res_mutex);
    if (sub_0b4c() & 4) {
        sceKernelFreeFpl(res[0], (void *)res[1]);
        ret = sceKernelDeleteFpl(res[0]);
        res[1] = 0;
        res[0] = 0;
        sub_0b58(sub_0b4c() & ~4u);
    }
    sub_6a14(g_res_mutex);
    return ret;
}

static NOINLINE int sub_6cc4(void)
{
    u32 dummy;
    g_video_work_ptr = g_video_work_buf;
    sceKernelMemset(g_video_work_buf, 0, 0x60);
    g_video_sub_ptr = g_video_sub_buf;
    sceKernelMemset(g_video_sub_buf, 0, 0x28);
    ((u32 *)g_video_work_ptr)[4] = (u32)g_video_sub_ptr;
    int ret = sub_6d6c(&dummy);
    if (ret == 0) {
        ret = sceVideocodecGetEDRAM((unsigned long *)g_video_work_ptr, 0);
        if (ret != 0) {
            ((u32 *)g_video_work_ptr)[5] = 0;
        }
    }
    return ret;
}

static NOINLINE int sub_6d6c(u32 *out)
{
    int ret = sceVideocodecOpen((unsigned long *)g_video_work_ptr, 0);
    *out = ((u32 *)g_video_work_ptr)[6];
    return ret;
}

static NOINLINE int sub_6db0(u32 *vwork, void *s1, void *s0, void *s2, u32 t0, u32 *t1, u32 t2, u32 t3, u32 sp0, u32 sp1, u32 sp2)
{
    sub_6a14(g_video_mutex);
    sceKernelMemset(s1, 0, 0x28);
    sceKernelMemset(s0, 0, 0xb0);
    sceKernelMemset(s2, 0, 0x64);
    vwork[11] = (u32)s0;
    vwork[4] = (u32)s1;
    vwork[12] = (u32)s2;
    vwork[15] = 4;
    vwork[14] = t0;
    int ret = sceVideocodecOpen((unsigned long *)vwork, 0);
    if (ret == 0) {
        vwork[5] = *t1;
        vwork[7] = t2;
        ret = sceVideocodecInit((unsigned long *)vwork, 0);
        if (ret == 0) {
            ret = sceVideocodecGetVersion((unsigned long *)vwork, 0);
            if (ret == 0) {
                vwork[16] = t3;
                vwork[17] = sp0;
                vwork[18] = sp1;
                vwork[19] = sp2;
                ret = sceVideocodecSetMemory((unsigned long *)vwork, 0);
                if (ret == 0) {
                    sub_6a34(g_video_mutex);
                    return 0;
                }
            }
        }
    }
    sub_6a34(g_video_mutex);
    return ret;
}

static NOINLINE int sub_6f28(void)
{
    return sceVideocodecReleaseEDRAM((unsigned long *)g_video_work_ptr);
}

static NOINLINE u32 sub_6f4c(void)
{
    return ((u32 *)g_video_work_ptr)[5];
}

static NOINLINE int sub_6f5c(void *vwork)
{
    sub_6a14(g_video_mutex);
    int ret = sceVideocodecDecode((unsigned long *)vwork, 0);
    sub_6a34(g_video_mutex);
    return ret;
}

static NOINLINE int sub_6fb0(void *vwork)
{
    sub_6a14(g_video_mutex);
    int ret = sceVideocodecStop((unsigned long *)vwork, 0);
    sub_6a34(g_video_mutex);
    return ret;
}

static NOINLINE int sub_7004(void *vwork)
{
    sub_6a14(g_video_mutex);
    int ret = sceVideocodecDelete((unsigned long *)vwork, 0);
    sub_6a34(g_video_mutex);
    return ret;
}

static NOINLINE int sub_7070(u32 *vwork, u32 val)
{
    sub_6a14(g_video_mutex);
    vwork[22] = val;
    int ret = sceVideocodec_893B32B1((unsigned long *)vwork, 0);
    sub_6a34(g_video_mutex);
    return ret;
}

static NOINLINE int sub_70d4(int mode)
{
    u32 dummy;
    g_audio_work_ptr = g_audio_work_buf;
    sceKernelMemset(g_audio_work_buf, 0, 0x6c);
    int ret = sub_7160(&dummy, mode, &g_audio_mode_val);
    if (ret == 0) {
        ret = sceAudiocodecGetEDRAM((unsigned long *)g_audio_work_ptr, 0x1000);
        if (ret != 0) {
            ((u32 *)g_audio_work_ptr)[3] = 0;
        }
    }
    return ret;
}

static const u8 g_audio_cfg1[4] = { 0x28, 0x5c, 0x5c, 0x5c };
static const u32 g_audio_cfg2[2] = { 0x06, 0x04 };

static NOINLINE int sub_7160(u32 *out, int mode, u32 *out_flag)
{
    int flag = (mode == 3) ? 1 : 0;
    if (out_flag) {
        *out_flag = flag;
    }
    u32 *awork = (u32 *)g_audio_work_ptr;
    awork[12] = 1;
    sceKernelMemcpy(&awork[10], &g_audio_cfg1[flag << 1], 2);
    sceKernelMemcpy(&awork[11], &g_audio_cfg2[flag], 4);
    awork[5] = 1;
    int ret = sceAudiocodecCheckNeedMem((unsigned long *)awork, 0x1000);
    *out = awork[4];
    return ret;
}

static NOINLINE int sub_721c(u32 *awork, u32 edram, u32 size, const void *p1, const void *p2)
{
    sub_6a14(g_audio_mutex);
    awork[3] = edram;
    awork[12] = 1;
    sceKernelMemcpy(&awork[10], p1, 2);
    sceKernelMemcpy(&awork[11], p2, 4);
    awork[5] = 1;
    int ret = sceAudiocodecCheckNeedMem((unsigned long *)awork, 0x1000);
    if (ret == 0) {
        if (size < awork[4]) {
            sub_6a34(g_audio_mutex);
            return 0x807f00ff;
        }
        ret = sceAudiocodecInit((unsigned long *)awork, 0x1000);
    }
    sub_6a34(g_audio_mutex);
    return ret;
}

static NOINLINE int sub_732c(void)
{
    u32 *awork = (u32 *)g_audio_work_ptr;
    if (awork[3] != 0) {
        return sceAudiocodecReleaseEDRAM((unsigned long *)awork);
    }
    return 0;
}

static NOINLINE int sub_7364(u32 *out, int mode)
{
    sub_6a14(g_audio_mutex);
    int ret = sub_7160(out, mode, 0);
    sub_6a34(g_audio_mutex);
    return ret;
}

static NOINLINE u32 sub_73c8(u32 *out)
{
    u32 *awork = (u32 *)g_audio_work_ptr;
    if (out) {
        *out = g_audio_mode_val;
    }
    return awork[3];
}

static NOINLINE int sub_73e8(void *awork)
{
    sub_6a14(g_audio_mutex);
    int ret = sceAudiocodecDecode((unsigned long *)awork, 0x1000);
    sub_6a34(g_audio_mutex);
    return ret;
}

static NOINLINE int sub_7454(const u8 *s1, const u8 *s2, int n)
{
    if (n <= 0)
        return 0;
    while (n > 0) {
        u8 c1 = *s1++;
        u8 c2 = *s2++;
        n--;
        if (c1 != c2)
            return (c1 < c2) ? -1 : 1;
    }
    return 0;
}

static NOINLINE int sub_749c(const char *s1, const char *s2)
{
    if (!s1 || !s2) {
        if (s1 == s2)
            return 0;
        return s1 ? 1 : -1;
    }
    while (*s1 == *s2) {
        if (*s1 == 0)
            return 0;
        s1++;
        s2++;
    }
    return (int)(signed char)*s1 - (int)(signed char)*s2;
}

static NOINLINE int sub_751c(int x)
{
    return (x < 0) ? -x : x;
}

static NOINLINE u32 sub_752c(int type, int ch)
{
    if (type >= 19)
        return 0x806101fe;
    u32 mask = g_stream_descs[type].mask;
    u32 prefix = g_stream_descs[type].prefix;
    int shift = 0;
    if (mask == 0xFFFF0000) {
        shift = 16;
    } else if (mask > 0xFFFF0000u) {
        if (mask == 0xFFFFFF00u)
            shift = 8;
        else if (mask != 0xFFFFFFFFu)
            return 0;
    } else if (mask == 0xFF000000u) {
        shift = 24;
    } else {
        return 0;
    }
    return prefix | ((u32)ch << shift);
}

static NOINLINE int sub_75c0(int *out_type, int *out_ch, u32 code)
{
    int found = 0;
    for (int i = 0; i < 19 && !found; i++) {
        u32 mask = g_stream_descs[i].mask;
        u32 prefix = g_stream_descs[i].prefix;
        if (mask == 0xFFFF0000u) {
            u32 c = code;
            u32 cmask = 0;
            if (prefix == 0xBD400000u || prefix == 0xBD100000u || prefix == 0xBD000000u ||
                prefix == 0xBD200000u || prefix == 0xBD300000u) {
                c &= 0xFFF00000u;
                cmask = 0xF;
            } else if (prefix == 0xBD880000u || prefix == 0xBD900000u || prefix == 0xBDA00000u) {
                c &= 0xFFF80000u;
                cmask = 0x7;
            } else if (prefix == 0xBD800000u) {
                c &= 0xFFE00000u;
                cmask = 0x1F;
            } else {
                c &= 0xFFFF0000u;
                cmask = 0;
            }
            if (c == prefix) {
                *out_type = i;
                *out_ch = (code >> 16) & cmask;
                found = 1;
            }
        } else if (mask > 0xFFFF0000u) {
            if (mask == 0xFFFFFF00u) {
                u32 c = code;
                u32 cmask = 0;
                if (prefix == 0xBF01E000u) {
                    c &= 0xFFFFE000u;
                    cmask = 0x1F;
                }
                if (c == prefix) {
                    *out_type = i;
                    *out_ch = (code >> 8) & cmask;
                    found = 1;
                }
            } else if (mask == 0xFFFFFFFFu && prefix == 0xFFFFFFFFu) {
                if ((code & 0xFFFFFF00u) == prefix) {
                    *out_type = i;
                    *out_ch = code & 0xFFFFu;
                    found = 1;
                }
            }
        } else if (mask == 0xFF000000u) {
            u32 c = code;
            u32 cmask = 0;
            if (prefix == 0xE0000000u) {
                c &= 0xF0000000u;
                cmask = 0xF;
            } else if (prefix == 0xC0000000u) {
                c &= 0xE0000000u;
                cmask = 0x1F;
            } else {
                c &= 0xFF000000u;
            }
            if (c == prefix) {
                *out_type = i;
                *out_ch = (code >> 24) & cmask;
                found = 1;
            }
        }
    }
    return found;
}

static NOINLINE u32 sub_780c(int type)
{
    if (type >= 19)
        return 0x806101fe;
    return g_stream_descs[type].mask;
}

static NOINLINE u32 sub_7838(const u8 *p)
{
    return ((u32)p[0] << 8) | p[1];
}

static NOINLINE u32 sub_7850(const u8 *p)
{
    return ((u32)p[0] << 16) | ((u32)p[1] << 8) | p[2];
}

static NOINLINE u32 sub_7870(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static NOINLINE u32 sub_789c(const u8 *p, int len)
{
    if (len == 1)
        return p[0];
    if (len == 2)
        return ((u32)p[0] << 8) | p[1];
    if (len == 3)
        return ((u32)p[0] << 16) | ((u32)p[1] << 8) | p[2];
    if (len == 4)
        return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
    return 0;
}

/* Stream & Ringbuffer & AVC/ATRAC internal functions */

static NOINLINE int sub_5164(u32 *mp_ptr, void *stream);
static NOINLINE int sub_518c(u32 *s);
static NOINLINE int sub_5198(u32 *s, int mode);
static NOINLINE int sub_51b8(u32 *s, int mode);
static NOINLINE int sub_51d8(u32 *s);
static NOINLINE int sub_51e0(u32 *s, int v);
static NOINLINE int sub_5204(u32 *s);
static NOINLINE int sub_520c(u32 *s, int v);
static NOINLINE int sub_522c(u32 *s);
static NOINLINE int sub_5234(u32 *mp_ptr, u32 *stream);
static NOINLINE int sub_53c4(u32 *mp_ptr, u32 *stream, int mode);
static NOINLINE int sub_54a4(u32 *mp_ptr, u32 *stream, int mode);
static NOINLINE int sub_3460(u32 *rb, int idx);
static NOINLINE int sub_34d0(u32 *rb);
static NOINLINE int sub_3594(u32 *rb, int count);
static NOINLINE int sub_3644(u32 *mp_ptr, u32 *au, u32 *attr);
static NOINLINE int sub_4b84(u32 *mp, u32 *stream);
static NOINLINE void sub_4bb8(u32 *s, u32 v);
static NOINLINE int sub_4be8(u32 *mp, u32 *s);
static NOINLINE int sub_4c40(u32 *mp);
static NOINLINE u32 *sub_4cf4(u32 *mp, int type, int ch, u32 code);
static NOINLINE int sub_4e4c(u32 *mp, u32 *s);
static NOINLINE void *sub_4f18(u32 *mp_ptr);
static NOINLINE int sub_4fb4(u32 *mp_ptr, void *buf);
static NOINLINE int sub_5040(u32 *mp, void *buf);
static NOINLINE int sub_507c(u32 *mp_ptr, int *es_size, int *out_size);
static NOINLINE int sub_50cc(u32 *mp_ptr, int *es_size, int *out_size);
static NOINLINE int sub_5118(u32 *mp_ptr, int *a1, int *a2);

static NOINLINE int sub_0000(u32 *mp_ptr, u32 *stream)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (sub_4b84(mp, stream) == 0)
        return 0x806101fe;
    u32 *rb = (u32 *)mp[291];
    sub_6a14(&rb[6]);
    int idx = stream[2];
    if (idx != -1) {
        while (idx != -1) {
            int next = sub_3460(rb, idx);
            idx = next;
        }
        stream[2] = -1;
        stream[3] = -1;
        stream[4] = 0;
    }
    sub_520c(stream, 0);
    sub_4bb8(stream, 0);
    int ret = sub_34d0(rb);
    if (ret < 0)
        return ret;
    sub_6a34(&rb[6]);
    return 0;
}

static NOINLINE int sub_012c(u32 *mp_ptr)
{
    u32 *mp = (u32 *)(*mp_ptr);
    for (int i = 0; i < 16; i++) {
        u32 *s = &mp[20 + i * 11];
        if (s[0] != 0) {
            int ret = sub_0000(mp_ptr, s);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

static NOINLINE int sub_01bc(u32 *mp_ptr, u32 *stream)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (sub_4b84(mp, stream) == 0)
        return 0x806101fe;
    int ret = sub_0000(mp_ptr, stream);
    if (ret >= 0) {
        ret = sub_4e4c(mp, stream);
    }
    return ret;
}

static NOINLINE u32 *sub_0284(u32 *mp_ptr, int type, int ch)
{
    u32 *mp = (u32 *)(*mp_ptr);
    u32 code = sub_752c(type, ch);
    if (code == 0x806101fe)
        return 0;
    return sub_4cf4(mp, type, ch, code);
}

static NOINLINE int sub_037c(void)
{
    sub_69d0(g_mpeg_mutex, 0, "SceMpeg");
    sub_6a14(g_mpeg_mutex);
    if (sub_0b4c() & 1) {
        sub_6a34(g_mpeg_mutex);
        return 0x80618005;
    }
    sub_69d0(g_video_mutex, 0, "SceMpegVideo");
    sub_69d0(g_audio_mutex, 0, "SceMpegAudio");
    sub_69d0(g_res_mutex, 0, "SceMpegAvcResource");
    int ret = sub_6cc4();
    if (ret == 0) {
        ret = sub_70d4(2);
        if (ret == 0) {
            sub_0b58(sub_0b4c() | 1);
        } else {
            sub_6f28();
        }
    }
    sub_6a34(g_mpeg_mutex);
    return ret;
}

static NOINLINE int sub_0438(void)
{
    sub_6a14(g_mpeg_mutex);
    if (!(sub_0b4c() & 1)) {
        sub_6a34(g_mpeg_mutex);
        return 0x80618009;
    }
    sub_732c();
    sub_6f28();
    sub_69f8(g_video_mutex);
    sub_69f8(g_audio_mutex);
    sub_69f8(g_res_mutex);
    sub_0b58(sub_0b4c() & ~1u);
    sub_6a34(g_mpeg_mutex);
    sub_69f8(g_mpeg_mutex);
    return 0;
}

static NOINLINE int sub_04dc(int mode)
{
    if (mode < 0 || mode >= 4)
        return 0x806101fe;
    if (mode == 2 || mode == 3)
        return 0x2800;
    return 0x10000;
}

static NOINLINE int sub_0b68(u32 *mp, int frameWidth, int mode, u32 ddrtop)
{
    u32 edram = sub_6f4c();
    u32 sp[3];
    sp[0] = ddrtop;
    sp[1] = 0x20000;
    sp[2] = mode;
    return sub_6db0(&mp[296], &mp[320], &mp[330], &mp[374], frameWidth, &edram, mode, ddrtop, sp[0], sp[1], sp[2]);
}

static NOINLINE int sub_19b4(u32 *mp, int mode)
{
    u32 flag = 0;
    u32 edram = sub_73c8(&flag);
    u32 size = 0;
    int ret = sub_7364(&size, mode);
    if (ret == 0) {
        ret = sub_721c(&mp[400], edram, size, &g_audio_cfg1[flag << 1], &g_audio_cfg2[flag]);
    }
    return ret;
}

static NOINLINE int sub_0524(u32 *mp_ptr, void *data, int size, u32 *rb, int frameWidth, int mode, u32 ddrtop)
{
    if (!mp_ptr || !data)
        return 0x806101fe;
    int need = sub_04dc(mode);
    if (need < 0 || size < need)
        return 0x80618001;
    sceMpegbase_31007FF8(frameWidth);
    u32 *mp = (u32 *)(((u32)data + 63) & ~63u);
    sceKernelMemset(mp, 0, 0x700);
    sceKernelMemcpy(mp, g_ver_str, 15);
    mp[291] = (u32)rb;
    mp[292] = frameWidth;
    mp[427] = mode;
    sub_4c40(mp);
    sceMpegBaseCscInit(frameWidth);
    int ret = 0;
    if (mode != 2 && mode != 3) {
        ret = sub_0b68(mp, frameWidth, mode, ddrtop);
    }
    if (ret == 0 && mode != 1) {
        ret = sub_19b4(mp, mode);
    }
    if (ret == 0) {
        sceGeListEnQueue(mp, mp, 0, 0);
        *mp_ptr = (u32)mp;
    }
    return ret;
}

static NOINLINE int sub_0a28(u32 *mp_ptr)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp)
        return 0x806101fe;
    int mode = mp[427];
    if (mode != 2 && mode != 3) {
        sub_7004(&mp[296]);
    }
    if (mp[291] != 0) {
        u32 *rb = (u32 *)mp[291];
        rb[10] = 0;
    }
    *mp_ptr = 0;
    return 0;
}

static NOINLINE int sub_0af8(u32 *mp_ptr, void *buf, u32 *au)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!au)
        return 0x806101fe;
    au[0] = 0xFFFFFFFF;
    au[1] = 0xFFFFFFFF;
    au[2] = 0xFFFFFFFF;
    au[3] = 0xFFFFFFFF;
    au[4] = (u32)buf;
    au[5] = 0;
    return sub_5040(mp, buf);
}

static NOINLINE int sub_0cf4(u32 *mp_ptr, const int *mode)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !mode)
        return 0x806101fe;
    if (mp[427] == 2 || mp[427] == 3)
        return 0x806101fe;
    mp[293] = mode[0];
    mp[294] = mode[1];
    sceMpegbase_0530BE4E(mp);
    return 0;
}

static NOINLINE int sub_0da0(u32 *mp_ptr, int idx, u32 *detail)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !detail || idx < 0 || idx >= 4)
        return 0x806101fe;
    sceKernelMemcpy(detail, &mp[330 + idx * 11], 44);
    return 0;
}

static NOINLINE int sub_0eb0(u32 *mp, void *buf, int frameWidth)
{
    return sceMpegBaseCscAvc(buf, 0, frameWidth, mp);
}

static NOINLINE int sub_0efc(u32 *mp_ptr, u32 *au, int frameWidth, void *buf, int *init)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !au || !buf || !init)
        return 0x806101fe;
    if (mp[427] == 2 || mp[427] == 3)
        return 0x806101fe;
    mp[302] = au[4];
    mp[303] = au[5];
    int ret = sub_6f5c(&mp[296]);
    if (ret == 0) {
        sceMpegbase_119D5DDA(mp);
        ret = sceMpegBaseCscAvc(buf, 0, frameWidth, mp);
        *init = mp[310];
    }
    return ret;
}

static NOINLINE int sub_10b0(u32 *mp_ptr, int frameWidth, void *buf, int *status)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !status)
        return 0x806101fe;
    int ret = sub_6fb0(&mp[296]);
    if (ret == 0 && buf) {
        ret = sceMpegBaseCscAvc(buf, 0, frameWidth, mp);
    }
    *status = 0;
    return ret;
}

static NOINLINE int sub_1200(u32 *mp_ptr)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp)
        return 0x806101fe;
    return sub_7070(&mp[296], 1);
}

static NOINLINE int sub_1244(u32 *mp_ptr, u32 *detail)
{
    return sub_0da0(mp_ptr, 0, detail);
}

static NOINLINE int sub_1264(u32 *mp_ptr, int mode, int w, int h, u8 *ycbcr)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !ycbcr || (mode != 1 && mode != 2))
        return 0x806101fe;
    if ((w & 15) || (h & 15) || w <= 0 || h <= 0)
        return 0x806101fe;
    u32 *hdr = (u32 *)ycbcr;
    sceKernelMemset(hdr, 0, 128);
    hdr[0] = w;
    hdr[1] = h;
    hdr[2] = mode;
    if (mode == 1) {
        sceKernelDcacheWritebackRange(ycbcr + 128, w * h);
    }
    return 0;
}

static NOINLINE int sub_1430(u32 *mp_ptr, int mode, int w, int h, int *res)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !res || (mode != 1 && mode != 2))
        return 0x806101fe;
    if ((w & 15) || (h & 15) || w <= 0 || h <= 0)
        return 0x806101fe;
    if (mode == 1) {
        *res = ((w * h * 3) >> 1) + 128;
    } else {
        *res = 128;
    }
    return 0;
}

static NOINLINE int sub_1540(u32 *mp_ptr, u32 *ycbcr, int *range, int frameWidth, void *buf)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !ycbcr || !range || !buf)
        return 0x806101fe;
    int x = range[0], y = range[1], w = range[2], h = range[3];
    if ((x & 15) || (y & 15) || (w & 15) || (h & 15))
        return 0x806101fe;
    return sceMpegBaseCscAvcRange(buf, frameWidth, range, ycbcr);
}

static NOINLINE int sub_1bb0(u32 *mp, u32 *au, void *buf, int init);

static NOINLINE int sub_175c(u32 *mp_ptr, void *src, void *dst)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !src || !dst)
        return 0x806101fe;
    return sub_1bb0(mp, (u32 *)src, dst, 0);
}

static NOINLINE int sub_17ec(u32 *mp_ptr, void *a1, void *a2, void *a3)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !a1 || !a2)
        return 0x806101fe;
    return sceVideocodec_D95C24D5((unsigned long *)&mp[296], 0);
}

static NOINLINE int sub_1880(u32 *mp_ptr, u32 *au, u32 *ycbcr, int *init)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !au || !ycbcr || !init)
        return 0x806101fe;
    mp[302] = au[4];
    mp[303] = au[5];
    int ret = sub_6f5c(&mp[296]);
    if (ret == 0) {
        ret = sceMpegbase_119D5DDA(ycbcr);
        *init = mp[310];
    }
    return ret;
}

static NOINLINE int sub_1950(u32 *mp_ptr, u32 *ycbcr, int *status)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !ycbcr || !status)
        return 0x806101fe;
    int ret = sub_6fb0(&mp[296]);
    *status = 0;
    return ret;
}

static NOINLINE int sub_1a58(u32 *mp, u32 *au, void *buf)
{
    mp[406] = au[4];
    mp[408] = (u32)buf;
    return sub_73e8(&mp[400]);
}

static NOINLINE int sub_1b08(u32 *mp, void *buf)
{
    int mode = mp[427];
    int bytes = (mode == 3) ? 0x1000 : 0x2000;
    sceKernelMemset(buf, 0, bytes);
    return 0;
}

static NOINLINE int sub_1bb0(u32 *mp, u32 *au, void *buf, int init)
{
    if (!mp || !au)
        return 0x806101fe;
    if (init == 1) {
        if (buf)
            sub_1b08(mp, buf);
        return 0;
    }
    if (!buf)
        return 0x806101fe;
    sceMpegBaseYCrCbCopy(buf, au, 1);
    sceVideocodec_D95C24D5((unsigned long *)&mp[296], 0);
    return sub_1a58(mp, au, buf);
}

static NOINLINE int sub_1d84(u32 *mp_ptr, u32 *au, void *buf, int init)
{
    u32 *mp = (u32 *)(*mp_ptr);
    return sub_1bb0(mp, au, buf, init);
}

static NOINLINE int sub_1e2c(u32 *mp_ptr, const u8 *buf, u32 *offset)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !buf || !offset)
        return 0x806101fe;
    if (sub_7454(buf, (const u8 *)"PSMF", 4) != 0)
        return 0x806101fe;
    if (sub_7454(buf + 4, (const u8 *)"0012", 4) != 0 &&
        sub_7454(buf + 4, (const u8 *)"0013", 4) != 0 &&
        sub_7454(buf + 4, (const u8 *)"0014", 4) != 0 &&
        sub_7454(buf + 4, (const u8 *)"0015", 4) != 0)
        return 0x806101fe;
    u32 off = sub_7870(buf + 8);
    if ((off & 0x7FF) != 0)
        return 0x806101fe;
    *offset = off;
    return 0;
}

static NOINLINE int sub_1f14(const u8 *buf, u32 *size)
{
    if (sub_7454(buf, (const u8 *)"PSMF", 4) != 0)
        return 0x806101fe;
    *size = sub_7870(buf + 12);
    return 0;
}

static NOINLINE int sub_1f70(const u8 *buf, u32 *size)
{
    if (!buf || !size)
        return 0x806101fe;
    return sub_1f14(buf, size);
}

/* Pack / PES parser functions with exact stream ID constants */

static NOINLINE int sub_1fb4(const u8 *p, u32 *scr_hi, u32 *scr_lo)
{
    u32 b0 = p[4];
    if ((b0 & 0xC4) != 0x44)
        return 0x806101fe;
    u32 hi = (b0 >> 5) & 1;
    u32 lo = ((b0 & 0x18) << 27) | ((b0 & 3) << 28) | ((u32)p[5] << 20) |
             (((u32)p[6] & 0xF8) << 12) | (((u32)p[6] & 3) << 13) |
             ((u32)p[7] << 5) | ((u32)p[8] >> 3);
    *scr_hi = hi;
    *scr_lo = lo;
    return 14 + (p[13] & 7);
}

static NOINLINE int sub_205c(const u8 *p, int max_len, int *out_hdr_len)
{
    u32 code = sub_7870(p);
    if (code != 0x000001BBu)
        return 0;
    int len = (int)sub_7838(p + 4) + 6;
    if (len > max_len)
        return 0x806101fe;
    const u8 *cur = p + 12;
    const u8 *end = p + len;
    while (cur < end && (*cur & 0x80)) {
        u32 sid = (u32)cur[0] << 24;
        if (sid == 0xBF01E000u || sid == 0xE0000000u || sid == 0xBD000000u ||
            sid == 0xBD800000u || sid == 0xBD100000u || sid == 0xBD200000u) {
            cur += 3;
        } else {
            cur += 3;
        }
    }
    *out_hdr_len = len;
    return len;
}

static NOINLINE int sub_2228(const u8 *p, u32 *ts_hi, u32 *ts_lo)
{
    u32 b0 = p[0];
    *ts_hi = (b0 >> 3) & 1;
    *ts_lo = (((b0 >> 1) & 3) << 30) | ((u32)p[1] << 22) |
             (((u32)p[2] >> 1) << 15) | ((u32)p[3] << 7) | ((u32)p[4] >> 1);
    return 0;
}

static NOINLINE int sub_22e8(const u8 *p, u32 *pts_hi, u32 *pts_lo, u32 *dts_hi, u32 *dts_lo)
{
    u8 flags = p[7];
    int hdr_len = p[8];
    if (flags & 0x80) {
        sub_2228(p + 9, pts_hi, pts_lo);
        if (flags & 0x40) {
            sub_2228(p + 14, dts_hi, dts_lo);
        }
    }
    return hdr_len + 9;
}

static NOINLINE int sub_2368(const u8 *p, u32 *info)
{
    u32 len = sub_7838(p + 4);
    int hdr = sub_22e8(p, &info[0], &info[1], &info[2], &info[3]);
    info[4] = len + 6 - hdr;
    return hdr;
}

static NOINLINE int sub_26a4(const u8 *p, u32 *info)
{
    int hdr = sub_2368(p, info);
    u32 sub_id = p[hdr];
    info[5] = sub_id;
    return hdr + 4;
}

static NOINLINE int sub_27b8(const u8 *p, u32 *info)
{
    u32 len = sub_7838(p + 4);
    info[4] = len;
    return 6;
}

static NOINLINE int sub_295c(u32 *mp, const u8 *pkt, u32 pkt_idx)
{
    u32 code = sub_7870(pkt);
    if (code != 0x000001BAu)
        return 0x806101fe;
    u32 scr_hi = 0, scr_lo = 0;
    int off = sub_1fb4(pkt, &scr_hi, &scr_lo);
    if (off < 0)
        return off;
    while (off < 2048) {
        u32 sc = sub_7870(pkt + off);
        if (sc == 0x000001B9u || sc == 0)
            break;
        int sys_len = 0;
        if (sc == 0x000001BBu) {
            int r = sub_205c(pkt + off, 2048 - off, &sys_len);
            if (r < 0)
                return r;
            off += sys_len;
            continue;
        }
        u32 sid = (sc & 0xFFu) << 24;
        u32 info[8];
        if (sid == 0xBC000000u || sid == 0xBE000000u || sid == 0xBF000000u) {
            int h = sub_27b8(pkt + off, info);
            off += h + info[4];
        } else if (sid == 0xBD000000u) {
            int h = sub_26a4(pkt + off, info);
            u32 full_code = sid | (info[5] << 16);
            int stype = 0, sch = 0;
            if (sub_75c0(&stype, &sch, full_code)) {
                sub_51d8(&mp[20]);
            }
            off += h + info[4];
        } else if (sc == 0xBF0142DFu || sc == 0xBF0142EFu || sc == 0xBF01427Fu || sc == 0xBF0142FFu) {
            int h = sub_27b8(pkt + off, info);
            off += h + info[4];
        } else {
            int h = sub_2368(pkt + off, info);
            int stype = 0, sch = 0;
            if (sub_75c0(&stype, &sch, sid)) {
                sub_51d8(&mp[20]);
            }
            off += h + info[4];
        }
    }
    (void)pkt_idx;
    return 0;
}

static NOINLINE int sub_3048(u32 *rb, int numPackets, int available)
{
    int (*cb)(void *data, int numPackets, void *param) = (void *)rb[7];
    if (!cb)
        return 0;
    u8 *base = (u8 *)rb[4];
    int write_idx = rb[2];
    int total = rb[0];
    int to_read = (numPackets < available) ? numPackets : available;
    if (write_idx + to_read > total) {
        to_read = total - write_idx;
    }
    void *dst = base + write_idx * 2048;
    int added = cb(dst, to_read, (void *)rb[8]);
    if (added > 0) {
        for (int i = 0; i < added; i++) {
            sub_295c((u32 *)rb[10], (const u8 *)dst + i * 2048, write_idx + i);
        }
        rb[2] = (write_idx + added) % total;
        rb[3] += added;
    }
    return added;
}

static NOINLINE int sub_2fdc(u32 *rb, int numPackets, int available)
{
    if (!rb || numPackets <= 0 || available <= 0)
        return 0;
    sub_6a14(&rb[6]);
    int ret = sub_3048(rb, numPackets, available);
    sub_6a34(&rb[6]);
    return ret;
}

static NOINLINE int sub_3200(u32 *rb)
{
    if (!rb)
        return 0x806101fe;
    sub_6a14(&rb[6]);
    int avail = rb[0] - rb[3];
    sub_6a34(&rb[6]);
    return avail;
}

static NOINLINE int sub_3348(int packets)
{
    if (packets <= 0)
        return 0x806101fe;
    return packets * (2048 + 104);
}

static NOINLINE int sub_3360(int packets)
{
    if (packets <= 0)
        return 0x806101fe;
    return packets * (2048 + 104) + 64;
}

static NOINLINE int sub_337c(u32 *rb, int packets, void *data, int size, void *callback, void *cb_param)
{
    if (!rb || !data || packets <= 0)
        return 0x806101fe;
    if (size < sub_3348(packets))
        return 0x80618001;
    int ret = sub_69d0(&rb[6], 0, "SceMpegRingbuffer");
    if (ret < 0)
        return ret;
    rb[0] = packets;
    rb[1] = 0;
    rb[2] = 0;
    rb[3] = 0;
    rb[4] = (u32)data;
    rb[5] = size;
    rb[7] = (u32)callback;
    rb[8] = (u32)cb_param;
    rb[9] = (u32)data + packets * 2048;
    rb[10] = 0;
    return 0;
}

static NOINLINE int sub_3458(u32 *rb)
{
    if (!rb)
        return 0x806101fe;
    return sub_69f8(&rb[6]);
}

static NOINLINE int sub_3460(u32 *rb, int idx)
{
    u32 *nodes = (u32 *)rb[9];
    u32 *node = &nodes[idx * 26];
    int next = (int)node[1];
    node[0] = 0;
    node[1] = 0xFFFFFFFF;
    return next;
}

static NOINLINE int sub_34d0(u32 *rb)
{
    u32 *nodes = (u32 *)rb[9];
    int read_idx = rb[1];
    int used = rb[3];
    while (used > 0) {
        u32 *node = &nodes[read_idx * 26];
        if (node[0] != 0)
            break;
        read_idx = (read_idx + 1) % (int)rb[0];
        used--;
    }
    rb[1] = read_idx;
    rb[3] = used;
    return 0;
}

static NOINLINE int sub_3594(u32 *rb, int count)
{
    return sub_3048(rb, count, rb[0] - rb[3]);
}

static NOINLINE int sub_3644(u32 *mp, u32 *stream, u32 *au)
{
    u32 *rb = (u32 *)mp[291];
    if (!rb || rb[3] == 0)
        return 0x80618001;
    int idx = stream[2];
    if (idx < 0) {
        sub_3594(rb, 1);
        return 0x80618001;
    }
    sceMpegBasePESpacketCopy(au);
    sceKernelDcacheWritebackInvalidateRange((void *)au[4], 64);
    stream[2] = sub_3460(rb, idx);
    sub_34d0(rb);
    au[5] = 0x100;
    return 0;
}

static NOINLINE int sub_3a7c(u32 *mp_ptr, u32 *stream, u32 *au, u32 *attr)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !stream || !au)
        return 0x806101fe;
    if (sub_4b84(mp, stream) == 0)
        return 0x806101fe;
    if (attr)
        *attr = 0;
    return sub_3644(mp, stream, au);
}

static NOINLINE int sub_3ca8(u32 *mp, u32 *stream, u32 *au, u32 *attr)
{
    if (attr)
        *attr = 0;
    return sub_3644(mp, stream, au);
}

static NOINLINE int sub_40a0(u32 *mp_ptr, u32 *stream, u32 *au, u32 *attr)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !stream || !au)
        return 0x806101fe;
    if (sub_4b84(mp, stream) == 0)
        return 0x806101fe;
    return sub_3ca8(mp, stream, au, attr);
}

static NOINLINE int sub_439c(u32 *mp, u32 *stream, u32 *au, u32 *attr)
{
    if (attr)
        *attr = 0;
    return sub_3644(mp, stream, au);
}

static NOINLINE int sub_44a0(u32 *mp_ptr, u32 *stream, u32 *au, u32 *attr)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !stream || !au)
        return 0x806101fe;
    if (sub_4b84(mp, stream) == 0)
        return 0x806101fe;
    return sub_439c(mp, stream, au, attr);
}

static NOINLINE int sub_4808(u32 *mp_ptr, u32 *stream, u32 *au, u32 *attr)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !stream || !au)
        return 0x806101fe;
    if (sub_4b84(mp, stream) == 0)
        return 0x806101fe;
    return sub_439c(mp, stream, au, attr);
}

static NOINLINE int sub_48e8(u32 *mp_ptr, u32 *stream, u32 *au)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !stream || !au)
        return 0x806101fe;
    if (sub_4b84(mp, stream) == 0)
        return 0x806101fe;
    return sub_3644(mp, stream, au);
}

static NOINLINE int sub_4b84(u32 *mp, u32 *stream)
{
    if (!mp || !stream)
        return 0;
    for (int i = 0; i < 16; i++) {
        if (&mp[20 + i * 11] == stream && stream[0] != 0)
            return 1;
    }
    return 0;
}

static NOINLINE void sub_4bb8(u32 *s, u32 v)
{
    s[10] = v;
}

static NOINLINE int sub_4bc4(u32 *mp_ptr, u32 *stream, u32 *a2, u32 *a3)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !stream || !a2 || !a3)
        return 0x806101fe;
    *a2 = stream[8];
    *a3 = stream[9];
    return 0;
}

static NOINLINE int sub_4be8(u32 *mp, u32 *s)
{
    (void)mp;
    sceKernelMemset(s, 0, 44);
    s[2] = 0xFFFFFFFF;
    s[3] = 0xFFFFFFFF;
    return 0;
}

static NOINLINE int sub_4c40(u32 *mp)
{
    for (int i = 0; i < 16; i++) {
        sub_4be8(mp, &mp[20 + i * 11]);
    }
    return 0;
}

static NOINLINE u32 *sub_4cf4(u32 *mp, int type, int ch, u32 code)
{
    for (int i = 0; i < 16; i++) {
        u32 *s = &mp[20 + i * 11];
        if (s[0] == 0) {
            s[0] = 1;
            s[1] = code;
            s[5] = type;
            s[6] = ch;
            return s;
        }
    }
    return 0;
}

static NOINLINE int sub_4e4c(u32 *mp, u32 *s)
{
    return sub_4be8(mp, s);
}

static NOINLINE void *sub_4f18(u32 *mp_ptr)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp)
        return 0;
    for (int i = 0; i < 2; i++) {
        if (mp[4 + i] == 0) {
            mp[4 + i] = 1;
            return (void *)(i + 1);
        }
    }
    return 0;
}

static NOINLINE int sub_4fb4(u32 *mp_ptr, void *buf)
{
    u32 *mp = (u32 *)(*mp_ptr);
    u32 idx = (u32)buf - 1;
    if (!mp || idx >= 2 || mp[4 + idx] == 0)
        return 0x806101fe;
    mp[4 + idx] = 0;
    return 0;
}

static NOINLINE int sub_5040(u32 *mp, void *buf)
{
    if (!mp)
        return 0x806101fe;
    (void)buf;
    return 0;
}

static NOINLINE int sub_507c(u32 *mp_ptr, int *es_size, int *out_size)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !es_size || !out_size)
        return 0x806101fe;
    int mode = mp[427];
    *es_size = g_es_sizes[(mode == 3) ? 1 : 0];
    *out_size = (mode == 3) ? 0x1000 : 0x2000;
    return 0;
}

static NOINLINE int sub_50cc(u32 *mp_ptr, int *es_size, int *out_size)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !es_size || !out_size)
        return 0x806101fe;
    *es_size = 0x140;
    *out_size = 0x140;
    return 0;
}

static NOINLINE int sub_5118(u32 *mp_ptr, int *a1, int *a2)
{
    u32 *mp = (u32 *)(*mp_ptr);
    if (!mp || !a1 || !a2)
        return 0x806101fe;
    *a1 = 0x800;
    *a2 = 0x800;
    return 0;
}

static NOINLINE int sub_5164(u32 *mp_ptr, void *stream)
{
    u32 *mp = (u32 *)(*mp_ptr);
    return sub_4b84(mp, (u32 *)stream) ? 0 : 0x806101fe;
}

static NOINLINE int sub_518c(u32 *s)
{
    return s[7];
}

static NOINLINE int sub_5198(u32 *s, int mode)
{
    if (mode != 0 && mode != 1)
        return 0x806101fe;
    s[7] = mode;
    return 0;
}

static NOINLINE int sub_51b8(u32 *s, int mode)
{
    if (mode != 0 && mode != 1)
        return 0x806101fe;
    s[8] = mode;
    return 0;
}

static NOINLINE int sub_51d8(u32 *s)
{
    return s[8];
}

static NOINLINE int sub_51e0(u32 *s, int v)
{
    if (v != 0 && v != 1)
        return 0x806101fe;
    s[9] = v;
    return 0;
}

static NOINLINE int sub_5204(u32 *s)
{
    return s[9];
}

static NOINLINE int sub_520c(u32 *s, int v)
{
    if (v != 0 && v != 1)
        return 0x806101fe;
    s[10] = v;
    return 0;
}

static NOINLINE int sub_522c(u32 *s)
{
    return s[10];
}

static NOINLINE int sub_5234(u32 *mp_ptr, u32 *stream)
{
    int ret = sub_5164(mp_ptr, stream);
    if (ret < 0)
        return ret;
    sub_518c(stream);
    sub_5204(stream);
    sub_522c(stream);
    return sub_520c(stream, 1);
}

static NOINLINE int sub_53c4(u32 *mp_ptr, u32 *stream, int mode)
{
    int ret = sub_5164(mp_ptr, stream);
    if (ret < 0)
        return ret;
    return sub_5198(stream, mode);
}

static NOINLINE int sub_54a4(u32 *mp_ptr, u32 *stream, int mode)
{
    int ret = sub_5164(mp_ptr, stream);
    if (ret < 0)
        return ret;
    sub_51e0(stream, mode);
    return sub_51b8(stream, mode);
}

/* === 52 Exported Functions === */

int sceMpegQueryStreamOffset(u32 *mp, const u8 *buf, u32 *offset)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_1e2c(mp, buf, offset);
}

int sceMpegQueryStreamSize(const u8 *buf, u32 *size)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_1f70(buf, size);
}

int sceMpegInit(void)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_037c();
}

int sceMpegFinish(void)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_0438();
}

int sceMpegQueryMemSize(int mode)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_04dc(mode == 1 ? 1 : 0);
}

int sceMpeg_42C679F6(int mode)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_04dc(mode);
}

int sceMpegCreate(u32 *mp, void *data, int size, u32 *rb, int frameWidth, int mode, u32 ddrtop)
{
    if (sub_6a50() < 0x3e0)
        return 0x80618008;
    return sub_0524(mp, data, size, rb, frameWidth, mode == 1 ? 1 : 0, ddrtop);
}

int sceMpeg_75E21135(u32 *mp, void *data, int size, u32 *rb, int frameWidth, int mode, u32 ddrtop)
{
    if (sub_6a50() < 0x3e0)
        return 0x80618008;
    return sub_0524(mp, data, size, rb, frameWidth, mode, ddrtop);
}

int sceMpegDelete(u32 *mp)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_0a28(mp);
}

u32 *sceMpegRegistStream(u32 *mp, int type, int ch)
{
    if (sub_6a50() < 0x3f0)
        return 0;
    return sub_0284(mp, type, ch);
}

int sceMpegUnRegistStream(u32 *mp, u32 *stream)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_01bc(mp, stream);
}

void *sceMpegMallocAvcEsBuf(u32 *mp)
{
    if (sub_6a50() < 0x3f0)
        return 0;
    return sub_4f18(mp);
}

int sceMpegFreeAvcEsBuf(u32 *mp, void *buf)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_4fb4(mp, buf);
}

int sceMpegQueryAtracEsSize(u32 *mp, int *es_size, int *out_size)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_507c(mp, es_size, out_size);
}

int sceMpeg_C02CF6B5(u32 *mp, int *es_size, int *out_size)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_50cc(mp, es_size, out_size);
}

int sceMpegInitAu(u32 *mp, void *buf, u32 *au)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_0af8(mp, buf, au);
}

int sceMpeg_234586AE(u32 *mp, u32 *stream, int mode)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_53c4(mp, stream, mode);
}

int sceMpeg_9DCFB7EA(u32 *mp, u32 *stream, int mode)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_54a4(mp, stream, mode);
}

int sceMpeg_3C37A7A6(u32 *mp, u32 *stream)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_5234(mp, stream);
}

int sceMpegGetAvcAu(u32 *mp, u32 *stream, u32 *au, u32 *attr)
{
    if (sub_6a50() < 0x3e0)
        return 0x80618008;
    return sub_44a0(mp, stream, au, attr);
}

int sceMpeg_8C1E027D(u32 *mp, u32 *stream, u32 *au, u32 *attr)
{
    if (sub_6a50() < 0x3e0)
        return 0x80618008;
    return sub_3a7c(mp, stream, au, attr);
}

int sceMpegGetAtracAu(u32 *mp, u32 *stream, u32 *au, u32 *attr)
{
    if (sub_6a50() < 0x3e0)
        return 0x80618008;
    return sub_40a0(mp, stream, au, attr);
}

int sceMpeg_500F0429(u32 *mp, u32 *stream)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_0000(mp, stream);
}

int sceMpegFlushAllStream(u32 *mp)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_012c(mp);
}

int sceMpegAvcDecode(u32 *mp, u32 *au, int frameWidth, void *buf, int *init)
{
    if (sub_6a50() < 0x3e0)
        return 0x80618008;
    return sub_0efc(mp, au, frameWidth, buf, init);
}

int sceMpeg_0F6C18D7(u32 *mp, u32 *detail)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_1244(mp, detail);
}

int sceMpeg_AB0E9556(u32 *mp, int idx, u32 *detail)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_0da0(mp, idx, detail);
}

int sceMpegAvcDecodeMode(u32 *mp, const int *mode)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_0cf4(mp, mode);
}

int sceMpegAvcDecodeStop(u32 *mp, int frameWidth, void *buf, int *status)
{
    if (sub_6a50() < 0x3e0)
        return 0x80618008;
    return sub_10b0(mp, frameWidth, buf, status);
}

int sceMpeg_4571CC64(u32 *mp)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_1200(mp);
}

int sceMpeg_211A057C(u32 *mp, int mode, int w, int h, int *res)
{
    if (sub_6a50() < 0x3e0)
        return 0x80618008;
    return sub_1430(mp, mode, w, h, res);
}

int sceMpeg_67179B1B(u32 *mp, int mode, int w, int h, u8 *ycbcr)
{
    if (sub_6a50() < 0x3e0)
        return 0x80618008;
    return sub_1264(mp, mode, w, h, ycbcr);
}

int sceMpeg_F0EB1125(u32 *mp, u32 *au, u32 *ycbcr, int *init)
{
    if (sub_6a50() < 0x3e0)
        return 0x80618008;
    return sub_1880(mp, au, ycbcr, init);
}

int sceMpeg_F2930C9C(u32 *mp, u32 *ycbcr, int *status)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_1950(mp, ycbcr, status);
}

int sceMpeg_31BD0272(u32 *mp, u32 *ycbcr, int *range, int frameWidth, void *buf)
{
    if (sub_6a50() < 0x3e0)
        return 0x80618008;
    return sub_1540(mp, ycbcr, range, frameWidth, buf);
}

int sceMpeg_0558B075(u32 *mp, void *src, void *dst)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_175c(mp, src, dst);
}

int sceMpeg_F5E7EA31(u32 *mp, void *a1, void *a2, void *a3)
{
    if (sub_6a50() < 0x3e0)
        return 0x80618008;
    return sub_17ec(mp, a1, a2, a3);
}

int sceMpegAtracDecode(u32 *mp, u32 *au, void *buf, int init)
{
    if (sub_6a50() < 0x3e0)
        return 0x80618008;
    u32 *m = (u32 *)(*mp);
    if (m[427] == 1)
        return 0x806101fe;
    return sub_1d84(mp, au, buf, init);
}

int sceMpeg_AF26BB01(void)
{
    if (sub_6a50() < 0x3f0)
        return 0;
    return sub_6b64();
}

int sceMpeg_63B9536A(void)
{
    if (sub_6a50() < 0x3f0)
        return 0;
    return sub_6bc8();
}

int sceMpeg_FCBDB5AD(int mode)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    if (mode != 1)
        return 0x806101fe;
    return sub_6a6c();
}

int sceMpeg_8160A2FE(void)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_6c30();
}

int sceMpeg_C45C99CC(u32 *mp, int *a1, int *a2)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_5118(mp, a1, a2);
}

int sceMpeg_01977054(u32 *mp, u32 *stream, u32 *a2, u32 *a3)
{
    if (sub_6a50() < 0x3e0)
        return 0x80618008;
    return sub_4bc4(mp, stream, a2, a3);
}

int sceMpeg_921FCCCF(u32 *mp, u32 *stream, u32 *au, u32 *attr)
{
    if (sub_6a50() < 0x3e0)
        return 0x80618008;
    return sub_4808(mp, stream, au, attr);
}

int sceMpeg_11F95CF1(u32 *mp, u32 *stream, u32 *au)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_48e8(mp, stream, au);
}

int sceMpegRingbufferQueryMemSize(int packets)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_3348(packets);
}

int sceMpeg_769BEBB6(int packets)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_3360(packets);
}

int sceMpegRingbufferConstruct(u32 *rb, int packets, void *data, int size, void *callback, void *cb_param)
{
    if (sub_6a50() < 0x3e0)
        return 0x80618008;
    return sub_337c(rb, packets, data, size, callback, cb_param);
}

int sceMpegRingbufferDestruct(u32 *rb)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_3458(rb);
}

int sceMpegRingbufferPut(u32 *rb, int numPackets, int available)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_2fdc(rb, numPackets, available);
}

int sceMpegRingbufferAvailableSize(u32 *rb)
{
    if (sub_6a50() < 0x3f0)
        return 0x80618008;
    return sub_3200(rb);
}
