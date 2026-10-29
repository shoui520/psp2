#include <common_imp.h>

SCE_MODULE_INFO("sceVaudio_driver", SCE_MODULE_KERNEL | SCE_MODULE_ATTR_EXCLUSIVE_LOAD | SCE_MODULE_ATTR_EXCLUSIVE_START, 1, 8);

extern int sceMeCore_driver_635397BB(int cmd, ...);
extern int sceMeCore_driver_FA398D71(int cmd, ...);
extern void sceKernelDcacheWritebackRange(const void *p, unsigned int size);
extern int sceKernelDcacheInvalidateRangeForUser(void *p, unsigned int size);
extern int sceKernelApplicationType(void);
extern void *sceKernelMemcpy(void *dst, const void *src, unsigned int size);
extern void *sceKernelMemset32(void *dst, int val, unsigned int size);
extern int sceKernelDeleteSema(SceUID semaid);
extern int sceKernelSignalSema(SceUID semaid, int signal);
extern int sceKernelWaitSema(SceUID semaid, int signal, SceUInt *timeout);
extern SceUID sceKernelCreateFpl(const char *name, int part, int attr, unsigned int size, unsigned int blocks, void *opt);
extern SceUID sceKernelCreateSema(const char *name, SceUInt attr, int initVal, int maxVal, void *option);
extern int sceKernelAllocateFpl(SceUID uid, void **data, unsigned int *timeout);
extern int sceKernelDeleteFpl(SceUID uid);
extern int sceAudioSetFrequency(int freq);
extern int sceAudioSRCChReserve(int samplecount, int freq, int channels);
extern int sceAudioOutput2GetRestSample(void);
extern int sceAudioSRCOutputBlocking(int vol, void *buf);
extern int sceAudioSRCChRelease(void);
extern int sceAudioSetVolumeOffset(int offset);

typedef struct {
    SceUID fpl_id;
    SceUID sema_id;
    u8 *work_buf;
    u16 sample_count;
    s8 effect_dirty;
    u8 ch_count;
    s8 effect_type;
    s8 buf_idx;
    u8 freq_idx;
    s8 alc_mode;
    s8 half_mode;
    s8 pad_15;
    s16 effect_vol;
} SceVaudioCtx;

static SceVaudioCtx g_vaudio;

static const u32 g_vaudio_effect_table[9][4] = {
    { 0x02343313, 0x04332234, 0x03224322, 0x01432331 },
    { 0x02343313, 0x04332234, 0x03224322, 0x01432331 },
    { 0x02343313, 0x04332234, 0x03224322, 0x01432331 },
    { 0x03433133, 0x03322343, 0x02243223, 0x04323313 },
    { 0x03433133, 0x03322343, 0x02243223, 0x04323313 },
    { 0x03433133, 0x03322343, 0x02243223, 0x04323313 },
    { 0x03433133, 0x03322343, 0x02243223, 0x04323313 },
    { 0x03433133, 0x03322343, 0x02243223, 0x04323313 },
    { 0x04331333, 0x03223433, 0x02432233, 0x03233133 },
};

static const u32 g_vaudio_err_table[8] = {
    0x80000104, 0x80000103, 0x80000001, 0x80000108,
    0x80000002, 0x800002FF, 0x800002FF, 0x00000000,
};

int sub_06e8(int vol, void *buf);
int sub_0ac8(int samplecount, int freq, int channels, int half_mode, int effect_vol);
int sub_0d84(int freq);
int sub_0e0c(u8 *work, int ch_bits, int out_bits, int freq, int samples);
int sub_0ef0(u8 *work, int samples, int a2, int a3, int a4, u32 effect_param);
int sub_0f70(u32 *work, void *in_buf, void *out_buf, int effect_type);

int sceVaudioOutputBlocking(int vol, void *buf)
{
    int shift = (g_vaudio.half_mode > 0) ? 1 : 0;
    u32 samples = g_vaudio.sample_count >> shift;
    u32 byte_len = (samples * g_vaudio.ch_count) << 1;

    int k1 = pspShiftK1();
    if (!pspK1DynBufOk(buf, byte_len)) {
        pspSetK1(k1);
        return 0x80000023;
    }

    if (g_vaudio.effect_vol < 0) {
        int ret = sub_06e8(vol, buf);
        pspSetK1(k1);
        return ret;
    }

    if (buf != NULL)
        sceKernelDcacheWritebackRange(buf, samples << 2);

    u32 half_bytes = samples << 2;
    int prev_state = 0;

    for (;;) {
        u8 *dst = g_vaudio.work_buf + ((int)g_vaudio.buf_idx << 13);
        if (g_vaudio.half_mode == 1)
            dst += half_bytes;

        int res = sceMeCore_driver_FA398D71(0x125, dst, buf, (int)g_vaudio.effect_vol);
        if (res < 0) {
            if (buf == NULL)
                res = sub_06e8(vol, NULL);
            else
                res = 0;
            pspSetK1(k1);
            return res;
        }

        if (res & 2) {
            sub_06e8(vol, dst);
        } else if (buf == NULL || prev_state > 0) {
            if (buf == NULL)
                res = sub_06e8(vol, NULL);
            else
                res = 0;
            pspSetK1(k1);
            return res;
        }

        if ((res & 1) == 0)
            continue;
        if (res == (res & 1)) {
            if (buf == NULL)
                res = sub_06e8(vol, NULL);
            else
                res = 0;
            pspSetK1(k1);
            return res;
        }
        prev_state = res;
    }
}

int sceVaudio_E8E78DC8(int samplecount, int freq, int channels)
{
    int k1 = pspShiftK1();
    int ret = sceKernelWaitSema(g_vaudio.sema_id, 1, NULL);
    if (ret < 0) {
        pspSetK1(k1);
        return ret;
    }

    ret = sceMeCore_driver_635397BB(0x124, freq, samplecount);
    sceKernelSignalSema(g_vaudio.sema_id, 1);
    pspSetK1(k1);

    if (ret < 0)
        return ret;

    return sub_0ac8(samplecount, freq, channels, 1, 0x800);
}

int sceVaudioChRelease(void)
{
    int k1 = pspShiftK1();
    int ret = sceKernelWaitSema(g_vaudio.sema_id, 1, NULL);
    if (ret < 0) {
        pspSetK1(k1);
        return ret;
    }

    sceAudioSRCOutputBlocking(0, NULL);
    ret = sceAudioSRCChRelease();
    if (ret < 0) {
        sceKernelSignalSema(g_vaudio.sema_id, 1);
        pspSetK1(k1);
        return ret;
    }

    if (g_vaudio.effect_vol > 0) {
        g_vaudio.effect_vol = -1;
        sceMeCore_driver_635397BB(0x126);
    }

    sceAudioSetFrequency(44100);
    if (g_vaudio.effect_dirty == 2) {
        g_vaudio.effect_dirty = 0;
        if (sceKernelApplicationType() == 0x100)
            sceAudioSetVolumeOffset(0);
    }
    g_vaudio.ch_count = 0;
    sceKernelSignalSema(g_vaudio.sema_id, 1);
    pspSetK1(k1);
    return ret;
}

int sceVaudio_driver_A3B71098(int effect_type, int vol)
{
    int k1 = pspShiftK1();
    if ((u32)effect_type >= 5) {
        pspSetK1(k1);
        return 0x80000102;
    }

    int ret = sceKernelWaitSema(g_vaudio.sema_id, 1, NULL);
    if (ret < 0) {
        pspSetK1(k1);
        return ret;
    }

    if (g_vaudio.effect_type != effect_type) {
        g_vaudio.effect_type = effect_type;
        g_vaudio.effect_dirty = 1;
        if (sceAudioOutput2GetRestSample() <= 0) {
            int active = (g_vaudio.effect_type > 0) ? 1 : 0;
            if (sceKernelApplicationType() == 0x100)
                sceAudioSetVolumeOffset(active);
        }
    }

    sceKernelSignalSema(g_vaudio.sema_id, 1);
    pspSetK1(k1);
    return vol;
}

int sceVaudioSetAlcMode(int mode)
{
    int k1 = pspShiftK1();
    if ((u32)mode >= 2) {
        pspSetK1(k1);
        return 0x80000102;
    }

    int ret = sceKernelWaitSema(g_vaudio.sema_id, 1, NULL);
    if (ret < 0) {
        pspSetK1(k1);
        return ret;
    }

    if (g_vaudio.alc_mode != mode) {
        g_vaudio.alc_mode = mode;
        g_vaudio.effect_dirty = 1;
    }

    sceKernelSignalSema(g_vaudio.sema_id, 1);
    pspSetK1(k1);
    return 0;
}

int sub_04d0(void)
{
    SceUID sema = sceKernelCreateSema("SceVaudio", 0x101, 1, 1, NULL);
    g_vaudio.sema_id = sema;
    if (sema < 0)
        return sema;

    SceUID fpl = sceKernelCreateFpl("SceVaudioWork", 2, 0x100, 0xA300, 1, NULL);
    g_vaudio.fpl_id = fpl;
    if (fpl < 0) {
        sceKernelDeleteSema(g_vaudio.sema_id);
        return g_vaudio.fpl_id;
    }

    void *block = NULL;
    int ret = sceKernelAllocateFpl(fpl, &block, NULL);
    if (ret < 0) {
        sceKernelDeleteSema(g_vaudio.sema_id);
        sceKernelDeleteFpl(g_vaudio.fpl_id);
        return ret;
    }

    u32 addr = (u32)block;
    u32 hi = (((s32)addr >> 31) + 2) << 29;
    g_vaudio.work_buf = (u8 *)(addr | hi);
    return 0;
}

int sceVaudioChReserve(int samplecount, int freq, int channels)
{
    return sub_0ac8(samplecount, freq, channels, 0, -1);
}

int sceVaudio_driver_27ACC20B(int samplecount, int freq, int channels)
{
    return sub_0ac8(samplecount, freq, channels, 1, -1);
}

int sceVaudio_504E4745(int effect_vol)
{
    if (g_vaudio.effect_vol < 0)
        return 0x80000001;
    if ((u32)(effect_vol + 0x400) > 0xC00)
        return 0x800001FE;
    g_vaudio.effect_vol = effect_vol;
    return 0;
}

int sceVaudioSetEffectType(int effect_type, int vol)
{
    int k1 = pspShiftK1();
    int ret = sceVaudio_driver_A3B71098(effect_type, 0);
    pspSetK1(k1);
    return ret;
}

int sceVaudio_driver_DCC18F25(void)
{
    return 5;
}

int module_start(SceSize args, void *argp)
{
    return (sub_04d0() < 0) ? 1 : 0;
}

int module_stop(SceSize args, void *argp)
{
    if (g_vaudio.ch_count != 0)
        sceVaudioChRelease();
    sceKernelDeleteSema(g_vaudio.sema_id);
    sceKernelDeleteFpl(g_vaudio.fpl_id);
    return 0;
}

int sub_06e8(int vol, void *buf)
{
    int ret = sceKernelWaitSema(g_vaudio.sema_id, 1, NULL);
    if (ret < 0)
        return ret;

    if (g_vaudio.ch_count == 0) {
        sceKernelSignalSema(g_vaudio.sema_id, 1);
        return 0x80000001;
    }

    if (buf == NULL) {
        u32 base = ((u32)g_vaudio.work_buf + 0x4050) & ~0x3FU;
        sceKernelMemset32((void *)(base + 0x100), 0, 0xB8);
        if (g_vaudio.half_mode == 1) {
            u16 sc = g_vaudio.sample_count;
            u8 *out_ptr = g_vaudio.work_buf + ((int)g_vaudio.buf_idx << 13);
            sceKernelMemset32(out_ptr + ((sc >> 1) << 2), 0, sc << 1);
            ret = sceAudioSRCOutputBlocking(vol, out_ptr);
        }
        if (ret >= 0)
            ret = sceAudioSRCOutputBlocking(0, NULL);

        int active = ((u8)g_vaudio.effect_type > 0) ? 1 : 0;
        if (sceKernelApplicationType() == 0x100)
            sceAudioSetVolumeOffset(active);
        g_vaudio.half_mode = ((u8)g_vaudio.half_mode > 0) ? 2 : 0;
        g_vaudio.buf_idx = 0;
        sceKernelSignalSema(g_vaudio.sema_id, 1);
        return ret;
    }

    u8 *cur_out = g_vaudio.work_buf + ((int)g_vaudio.buf_idx << 13);
    u8 *target_out = cur_out;
    int do_output = 1;
    if (g_vaudio.half_mode > 0) {
        do_output = g_vaudio.half_mode & 1;
        if (do_output != 0)
            target_out = cur_out + ((g_vaudio.sample_count >> 1) << 2);
        g_vaudio.half_mode = do_output + 1;
    }

    int use_me = 0;
    if ((g_vaudio.sample_count & 7) == 0)
        use_me = g_vaudio.alc_mode & 1;
    if (((u8)g_vaudio.effect_type > 0) | use_me) {
        ret = sub_0f70((u32 *)(g_vaudio.work_buf + 0x4000), buf, target_out, g_vaudio.effect_type);
    } else {
        u32 uncached_dst = (u32)target_out & 0x9FFFFFFFU;
        cur_out = (u8 *)((u32)cur_out & 0x9FFFFFFFU);
        if (g_vaudio.ch_count == 2) {
            if (g_vaudio.half_mode > 0) {
                u32 uncached_src = (u32)buf & 0x9FFFFFFFU;
                if (uncached_dst != uncached_src)
                    sceKernelMemcpy((void *)uncached_dst, buf, g_vaudio.sample_count << 1);
                ret = 0;
            } else {
                cur_out = (u8 *)buf;
                ret = 0;
            }
        } else {
            u32 count = g_vaudio.sample_count << 1;
            s16 *src16 = (s16 *)buf;
            s32 *dst32 = (s32 *)uncached_dst;
            for (u32 i = 0; i < count; i++)
                dst32[i] = src16[i];
            ret = 0;
        }
    }

    if (ret >= 0 && do_output) {
        int out_vol = vol;
        if (sceKernelApplicationType() != 0x100 && g_vaudio.effect_type != 0) {
            int doubled = vol << 1;
            out_vol = (doubled < 0xFFFFF) ? doubled : 0xFFFFF;
        }
        ret = sceAudioSRCOutputBlocking(out_vol, cur_out);
        g_vaudio.buf_idx ^= 1;
    }

    if (g_vaudio.effect_dirty == 1) {
        g_vaudio.effect_dirty = 2;
        if (sceKernelApplicationType() == 0x100)
            sceAudioSetVolumeOffset(-1);

        int shift = (g_vaudio.half_mode > 0) ? 1 : 0;
        int samples = g_vaudio.sample_count >> shift;
        int eff = g_vaudio.effect_type;
        int param_a2 = (eff != 0) ? 0x392CCF : 0x7FFFFF;
        u32 param_t1 = 0x03333333;
        if (eff != 0)
            param_t1 = g_vaudio_effect_table[g_vaudio.freq_idx][eff - 1];
        int me_res = sub_0ef0(g_vaudio.work_buf + 0x4000, samples, param_a2, 0x7FFFFF, 0, param_t1);
        if (me_res < 0)
            ret = me_res;
    } else if (g_vaudio.effect_dirty == 2) {
        int active = ((u8)g_vaudio.effect_type > 0) ? 1 : 0;
        g_vaudio.effect_dirty = 0;
        if (sceKernelApplicationType() == 0x100)
            sceAudioSetVolumeOffset(active);
    }

    sceKernelSignalSema(g_vaudio.sema_id, 1);
    return ret;
}

int sub_0ac8(int samplecount, int freq, int channels, int half_mode, int effect_vol)
{
    if ((u32)(samplecount - 0x100) > 0x700)
        return 0x80000104;
    if (channels != 2 && channels != 4)
        return 0x80000108;

    int k1 = pspShiftK1();
    int ret = sceKernelWaitSema(g_vaudio.sema_id, 1, NULL);
    if (ret < 0) {
        pspSetK1(k1);
        return ret;
    }

    if (g_vaudio.ch_count != 0) {
        sceKernelSignalSema(g_vaudio.sema_id, 1);
        pspSetK1(k1);
        return 0x80000021;
    }

    if (sceKernelApplicationType() != 0x100) {
        if (samplecount != 0x100 && samplecount != 0x240 &&
            samplecount != 0x400 && samplecount != 0x480 &&
            samplecount != 0x800) {
            pspSetK1(k1);
            return 0x80000104;
        }
        if (channels != 2) {
            pspSetK1(k1);
            return 0x80000108;
        }
    }

    int src_freq = freq;
    if (freq == 48000) {
        if (sceKernelApplicationType() == 0x100) {
            if (sceAudioSetFrequency(freq) >= 0)
                src_freq = 0;
            else
                sceAudioSetFrequency(44100);
        } else {
            sceAudioSetFrequency(44100);
        }
    } else if (freq == 44100) {
        if (sceAudioSetFrequency(freq) >= 0)
            src_freq = 0;
        else
            sceAudioSetFrequency(44100);
    } else {
        sceAudioSetFrequency(44100);
    }

    int actual_samples = samplecount;
    if (half_mode != 0 && (u32)samplecount <= 0x400) {
        g_vaudio.half_mode = 2;
        actual_samples = samplecount << 1;
    } else {
        g_vaudio.half_mode = 0;
    }

    g_vaudio.ch_count = channels;
    g_vaudio.sample_count = actual_samples;
    g_vaudio.freq_idx = sub_0d84(freq);
    g_vaudio.buf_idx = 0;

    sub_0e0c(g_vaudio.work_buf + 0x4000, channels << 3, 16, freq, samplecount);

    int eff = g_vaudio.effect_type;
    int param_a2 = (eff != 0) ? 0x392CCF : 0x7FFFFF;
    u32 param_t1 = 0x03333333;
    if (eff != 0)
        param_t1 = g_vaudio_effect_table[g_vaudio.freq_idx][eff - 1];

    ret = sub_0ef0(g_vaudio.work_buf + 0x4000, samplecount, param_a2, 0x7FFFFF, 0, param_t1);
    if (ret >= 0) {
        ret = sceAudioSRCChReserve(actual_samples, src_freq, 2);
        if (ret >= 0)
            g_vaudio.effect_vol = effect_vol;
    }

    sceKernelSignalSema(g_vaudio.sema_id, 1);
    pspSetK1(k1);
    return ret;
}

int sub_0d84(int freq)
{
    switch (freq) {
    case 44100: return 0;
    case 48000: return 1;
    case 32000: return 2;
    case 22050: return 3;
    case 24000: return 4;
    case 16000: return 5;
    case 11025: return 6;
    case 12000: return 7;
    case 8000:  return 8;
    default:    return 0;
    }
}

int sub_0e0c(u8 *work, int ch_bits, int out_bits, int freq, int samples)
{
    int res = sceMeCore_driver_635397BB(0x120, work + 0x10, ch_bits, out_bits);
    int in_bytes = (ch_bits >> 3) << 1;
    int out_bytes = (out_bits >> 3) << 1;
    if (res >= 0) {
        *(u32 *)(work + 4) = in_bytes;
        *(u32 *)(work + 8) = out_bytes;
        if (ch_bits == 16 && (samples & 7) == 0)
            res = sceMeCore_driver_635397BB(0x123, work + 0x280, samples, freq);
    }
    if ((u32)(res + 8) < 8)
        return g_vaudio_err_table[-1 - res];
    return res;
}

int sub_0ef0(u8 *work, int samples, int a2, int a3, int a4, u32 effect_param)
{
    int res = sceMeCore_driver_635397BB(0x121, work + 0x10, samples, a2, a3, a4, effect_param);
    *(u32 *)work = samples;
    if ((u32)(res + 8) < 8)
        return g_vaudio_err_table[-1 - res];
    return res;
}

int sub_0f70(u32 *work, void *in_buf, void *out_buf, int effect_type)
{
    if ((0xD3 >> ((u32)in_buf >> 29)) & 1)
        sceKernelDcacheWritebackRange(in_buf, work[0] * work[1]);
    if ((0xD3 >> ((u32)out_buf >> 29)) & 1)
        sceKernelDcacheInvalidateRangeForUser(out_buf, work[0] * work[2]);

    int res = sceMeCore_driver_FA398D71(0x122, work, in_buf, out_buf, effect_type);
    if ((u32)(res + 8) < 8)
        return g_vaudio_err_table[-1 - res];
    return res;
}
