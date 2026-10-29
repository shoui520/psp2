#include <common_imp.h>

SCE_MODULE_INFO("sceMpegbase_Driver", SCE_MODULE_KERNEL | SCE_MODULE_ATTR_EXCLUSIVE_LOAD | SCE_MODULE_ATTR_EXCLUSIVE_START, 1, 3);

extern int sceDmacplus_driver_282CA0D7(void *lli);
extern int sceDmacplus_driver_47D400CB(int a0, int a1);
extern int sceDmacplus_driver_5FCF43BD(int a0, int a1);
extern int sceDmacplus_driver_6945F1D3(void *lli);
extern int sceDmacplus_driver_9C492B9B(void *rgb, void *unk, void *yuv);
extern int sceDmacplus_driver_B269EAC9(int a0, int a1, void *coeff);
extern int sceDmacplus_driver_BF0DB45E(int a0, int a1);
extern int sceDmacplus_driver_D126494B(void *rgb, void *unk, void *yuv);
extern int sceDmacplus_driver_F652F09D(int x, int y, int w, int h);
extern int sceKernelDeleteSema(SceUID semaid);
extern int sceKernelSignalSema(SceUID semaid, int signal);
extern int sceKernelWaitSema(SceUID semaid, int signal, SceUInt *timeout);
extern SceUID sceKernelCreateSema(const char *name, SceUInt attr, int initVal, int maxVal, void *option);
extern void sceKernelDcacheWritebackInvalidateRange(const void *p, unsigned int size);
extern void sceKernelDcacheWritebackRange(const void *p, unsigned int size);
extern void sceKernelDcacheWritebackAll(void);
extern void *sceKernelGetGameInfo(void);
extern int sceMeCore_driver_9EB2943B(void);
extern int sceMeCore_driver_var_AB354CA1;

typedef struct {
    u32 src;
    u32 dst;
    u32 next;
    u32 ctrl;
} SceMpegLLI;

typedef struct {
    SceMpegLLI lli[100];      // 0x000..0x640
    u8 pad_640[16];           // 0x640..0x650
    u8 csc_coeff[24];         // 0x650..0x668
    int csc_width;            // 0x668
    int csc_mode;             // 0x66C
    SceUID sema_id;           // 0x670
    int is_uljs00006;         // 0x674
} SceMpegBaseCtx;

static SceMpegBaseCtx g_mpegbase __attribute__((aligned(64)));

static const u32 g_default_csc_coeff[6] = {
    0x00000095, 0x009500CC, 0x039803CE, 0x01020095, 0x00100000, 0x00000001
};

int sub_09e8(int mode, int width, void *coeff);
int sub_0a5c(u32 *dst_yuv, u32 *src_yuv, u32 *offsets, int flags);
int sub_0b6c(void *rgb, void *unk, void *yuv);
int sub_0bf8(void **p_rgb, void **p_unk, u32 *range, u32 *yuv, u32 *offsets, u32 *out_aligned);
int sub_0d70(void *rgb, void *unk, u32 *range, u32 *yuv);
SceMpegLLI *sub_14d8(SceMpegLLI *lli, u32 src, u32 dst, u32 size);
int sub_1558(SceMpegLLI *end_lli);

int module_start(SceSize args, void *argp)
{
    g_mpegbase.sema_id = sceKernelCreateSema("SceMpegBaseLLI", 0, 1, 1, NULL);
    g_mpegbase.csc_width = 0;
    g_mpegbase.csc_mode = 0;
    char *info = (char *)sceKernelGetGameInfo();
    int is_match = 0;
    if (info != NULL)
        is_match = (strcmp(info + 0x44, "ULJS00006") == 0) ? 1 : 0;
    g_mpegbase.is_uljs00006 = is_match;
    return 0;
}

int module_stop(SceSize args, void *argp)
{
    sceKernelDeleteSema(g_mpegbase.sema_id);
    return 0;
}

int sceMpegbase_119D5DDA(void)
{
    return sceMeCore_driver_9EB2943B();
}

int sceMpegbase_31007FF8(void)
{
    return sceMeCore_driver_var_AB354CA1;
}

int sceMpegBaseCscAvc(void *rgb, void *unk, int width, u32 *yuv)
{
    int k1 = pspShiftK1();
    u32 k1_mask = pspGetK1();

    if (((u32)yuv | ((u32)yuv + 0x30)) & k1_mask) {
        pspSetK1(k1);
        return 0x80000023;
    }

    u32 h = yuv[0];
    u32 w = yuv[1];
    u32 stride = width ? (u32)width : (u32)g_mpegbase.csc_width;
    u32 blocks = (h * w) << 8;
    u32 y_size = blocks >> 2;
    u32 c_size = blocks >> 3;
    u32 rgb_size = (stride * w) << 2;

    if ((((u32)yuv | ((u32)yuv + 0x30)) & k1_mask) ||
        ((yuv[4] | (yuv[4] + y_size)) & k1_mask) ||
        ((yuv[6] | (yuv[6] + y_size)) & k1_mask) ||
        ((yuv[8] | (yuv[8] + c_size)) & k1_mask) ||
        ((yuv[10] | (yuv[10] + c_size)) & k1_mask) ||
        ((yuv[5] | (yuv[5] + y_size)) & k1_mask) ||
        ((yuv[7] | (yuv[7] + y_size)) & k1_mask) ||
        ((yuv[9] | (yuv[9] + c_size)) & k1_mask) ||
        ((yuv[11] | (yuv[11] + c_size)) & k1_mask) ||
        (((u32)rgb | ((u32)rgb + rgb_size) | rgb_size) & k1_mask) ||
        (unk != NULL && (((u32)unk | ((u32)unk + rgb_size) | rgb_size) & k1_mask))) {
        pspSetK1(k1);
        return 0x80000023;
    }

    int ret = sceDmacplus_driver_F652F09D(0, 0, yuv[1], yuv[0]);
    if (ret != 0) {
        Kprintf("Failed to set csc range : 0x%08x \n", ret);
        pspSetK1(k1);
        return ret;
    }

    if (width != 0) {
        ret = sub_09e8(g_mpegbase.csc_mode, width, g_mpegbase.csc_coeff);
        if (ret != 0) {
            pspSetK1(k1);
            return ret;
        }
    }

    do {
        ret = sceDmacplus_driver_D126494B(rgb, unk, yuv);
        if (ret != 0 && ret != (int)0x80000021) {
            pspSetK1(k1);
            return ret;
        }
        int sync_ret = sceDmacplus_driver_BF0DB45E(0, 0);
        if (ret == 0) {
            ret = sync_ret;
            break;
        }
    } while (ret != 0);

    pspSetK1(k1);
    return ret;
}

int sceMpegBaseCscVme(void *rgb, void *unk, int width, u32 *yuv)
{
    int k1 = pspShiftK1();
    u32 k1_mask = pspGetK1();

    if (((u32)yuv | ((u32)yuv + 0x28)) & k1_mask) {
        pspSetK1(k1);
        return 0x80000023;
    }

    u32 h = yuv[0];
    u32 w = yuv[1];
    u32 stride = width ? (u32)width : (u32)g_mpegbase.csc_width;
    u32 blocks = (h * w) << 8;
    u32 y_size = blocks >> 1;
    u32 c_size = blocks >> 3;
    u32 rgb_size = (stride * w) << 2;

    if (((yuv[4] | (yuv[4] + y_size)) & k1_mask) ||
        ((yuv[5] | (yuv[5] + y_size)) & k1_mask) ||
        ((yuv[6] | (yuv[6] + c_size)) & k1_mask) ||
        ((yuv[7] | (yuv[7] + c_size)) & k1_mask) ||
        ((yuv[8] | (yuv[8] + c_size)) & k1_mask) ||
        ((yuv[9] | (yuv[9] + c_size)) & k1_mask) ||
        (((u32)rgb | ((u32)rgb + rgb_size) | rgb_size) & k1_mask) ||
        (unk != NULL && (((u32)unk | ((u32)unk + rgb_size) | rgb_size) & k1_mask))) {
        pspSetK1(k1);
        return 0x80000023;
    }

    if (width != 0) {
        int ret = sub_09e8(g_mpegbase.csc_mode, width, g_mpegbase.csc_coeff);
        if (ret != 0) {
            pspSetK1(k1);
            return ret;
        }
    }

    int ret;
    do {
        ret = sceDmacplus_driver_9C492B9B(rgb, unk, yuv);
        if (ret != 0 && ret != (int)0x80000021) {
            pspSetK1(k1);
            return ret;
        }
        int sync_ret = sceDmacplus_driver_BF0DB45E(0, 0);
        if (ret == 0) {
            ret = sync_ret;
            break;
        }
    } while (ret != 0);

    pspSetK1(k1);
    return ret;
}

int sceMpegBaseCscAvcRange(void *rgb, void *unk, u32 *range, int width, u32 *yuv)
{
    int k1 = pspShiftK1();
    u32 k1_mask = pspGetK1();

    if ((((u32)range | ((u32)range + 0x10)) & k1_mask) ||
        (((u32)yuv | ((u32)yuv + 0x30)) & k1_mask)) {
        pspSetK1(k1);
        return 0x80000023;
    }

    u32 h = yuv[0];
    u32 w = yuv[1];
    u32 stride = width ? (u32)width : (u32)g_mpegbase.csc_width;
    u32 blocks = (h * w) << 8;
    u32 y_size = blocks >> 2;
    u32 c_size = blocks >> 3;
    u32 rgb_size = (stride * w) << 2;

    if ((((u32)yuv | ((u32)yuv + 0x30)) & k1_mask) ||
        ((yuv[4] | (yuv[4] + y_size)) & k1_mask) ||
        ((yuv[6] | (yuv[6] + y_size)) & k1_mask) ||
        ((yuv[8] | (yuv[8] + c_size)) & k1_mask) ||
        ((yuv[10] | (yuv[10] + c_size)) & k1_mask) ||
        ((yuv[5] | (yuv[5] + y_size)) & k1_mask) ||
        ((yuv[7] | (yuv[7] + y_size)) & k1_mask) ||
        ((yuv[9] | (yuv[9] + c_size)) & k1_mask) ||
        ((yuv[11] | (yuv[11] + c_size)) & k1_mask) ||
        (((u32)rgb | ((u32)rgb + rgb_size) | rgb_size) & k1_mask) ||
        (unk != NULL && (((u32)unk | ((u32)unk + rgb_size) | rgb_size) & k1_mask))) {
        pspSetK1(k1);
        return 0x80000023;
    }

    if (range[0] + range[2] > w || range[1] + range[3] > h) {
        pspSetK1(k1);
        return 0x800001FE;
    }

    int ret = sceDmacplus_driver_F652F09D(range[0], range[1], range[2], range[3]);
    if (ret != 0) {
        Kprintf("Failed to set csc range : 0x%08x \n", ret);
        pspSetK1(k1);
        return ret;
    }

    if (width != 0) {
        ret = sub_09e8(g_mpegbase.csc_mode, width, g_mpegbase.csc_coeff);
        if (ret != 0) {
            pspSetK1(k1);
            return ret;
        }
    }

    if (yuv[0] != range[3]) {
        ret = sub_0d70(rgb, unk, range, yuv);
        pspSetK1(k1);
        return ret;
    }

    if (yuv[1] == range[2]) {
        ret = sub_0b6c(rgb, unk, yuv);
        pspSetK1(k1);
        return ret;
    }

    void *cur_rgb = rgb;
    void *cur_unk = unk;
    u32 offsets[4];
    u32 aligned_w = 0;
    ret = sub_0bf8(&cur_rgb, &cur_unk, range, yuv, offsets, &aligned_w);
    if (ret != 0) {
        pspSetK1(k1);
        return ret;
    }

    u32 tmp_yuv[12];
    tmp_yuv[0] = range[3];
    tmp_yuv[1] = range[2] - aligned_w;
    tmp_yuv[2] = yuv[2];
    tmp_yuv[3] = yuv[3];
    sub_0a5c(tmp_yuv, yuv, offsets, 0);
    ret = sub_0b6c(cur_rgb, cur_unk, tmp_yuv);
    pspSetK1(k1);
    return ret;
}

int sceMpegbase_0530BE4E(int mode)
{
    g_mpegbase.csc_mode = mode;
    return sub_09e8(mode, g_mpegbase.csc_width, g_mpegbase.csc_coeff);
}

int sceMpegbase_AC9E717E(int width, const void *coeff)
{
    int k1 = pspShiftK1();
    u32 k1_mask = pspGetK1();

    if (((u32)coeff | ((u32)coeff + 0x16)) & k1_mask) {
        pspSetK1(k1);
        return 0x80000023;
    }

    g_mpegbase.csc_mode = 0;
    g_mpegbase.csc_width = width;
    const u8 *src = (const u8 *)coeff;
    u8 *dst = g_mpegbase.csc_coeff;
    if ((((u32)src | (u32)dst) & 3) == 0) {
        const u32 *s32 = (const u32 *)src;
        u32 *d32 = (u32 *)dst;
        d32[0] = s32[0];
        d32[1] = s32[1];
        d32[2] = s32[2];
        d32[3] = s32[3];
        d32[4] = s32[4];
        *(u16 *)(d32 + 5) = *(const u16 *)(s32 + 5);
    } else {
        typedef struct __attribute__((packed)) { u32 v; } unaligned_u32;
        const unaligned_u32 *su = (const unaligned_u32 *)src;
        unaligned_u32 *du = (unaligned_u32 *)dst;
        du[0].v = su[0].v;
        du[1].v = su[1].v;
        du[2].v = su[2].v;
        du[3].v = su[3].v;
        du[4].v = su[4].v;
        dst[20] = src[20];
        dst[21] = src[21];
    }

    int ret = sub_09e8(0, width, g_mpegbase.csc_coeff);
    pspSetK1(k1);
    return ret;
}

int sceMpegBaseCscInit(int width)
{
    int k1 = pspShiftK1();
    int ret = sceMpegbase_AC9E717E(width, g_default_csc_coeff);
    pspSetK1(k1);
    return ret;
}

int sub_09e8(int mode, int width, void *coeff)
{
    int ret;
    for (;;) {
        ret = sceDmacplus_driver_B269EAC9(mode, width, coeff);
        if (ret != (int)0x80000021)
            return ret;
        sceDmacplus_driver_BF0DB45E(0, 0);
    }
}

int sub_0a5c(u32 *dst_yuv, u32 *src_yuv, u32 *offsets, int flags)
{
    if (flags & 2) {
        dst_yuv[4]  = src_yuv[5]  + offsets[1];
        dst_yuv[6]  = src_yuv[7]  + offsets[1];
        dst_yuv[8]  = src_yuv[9]  + offsets[3];
        dst_yuv[10] = src_yuv[11] + offsets[3];
    } else {
        dst_yuv[4]  = src_yuv[4]  + offsets[0];
        dst_yuv[6]  = src_yuv[6]  + offsets[0];
        dst_yuv[8]  = src_yuv[8]  + offsets[2];
        dst_yuv[10] = src_yuv[10] + offsets[2];
    }

    if (flags & 1) {
        dst_yuv[5]  = src_yuv[4]  + offsets[0];
        dst_yuv[7]  = src_yuv[6]  + offsets[0];
        dst_yuv[9]  = src_yuv[8]  + offsets[2];
        dst_yuv[11] = src_yuv[10] + offsets[2];
    } else {
        dst_yuv[5]  = src_yuv[5]  + offsets[1];
        dst_yuv[7]  = src_yuv[7]  + offsets[1];
        dst_yuv[9]  = src_yuv[9]  + offsets[3];
        dst_yuv[11] = src_yuv[11] + offsets[3];
    }
    return 0;
}

int sub_0b6c(void *rgb, void *unk, void *yuv)
{
    int ret;
    for (;;) {
        ret = sceDmacplus_driver_D126494B(rgb, unk, yuv);
        if (ret != 0 && ret != (int)0x80000021)
            return ret;
        sceDmacplus_driver_BF0DB45E(0, 0);
        if (ret == 0)
            return 0;
    }
}

int sub_0bf8(void **p_rgb, void **p_unk, u32 *range, u32 *yuv, u32 *offsets, u32 *out_aligned)
{
    u32 rx = range[0];
    u32 h = yuv[0];
    u32 half_rx = rx >> 1;
    u32 half_rx_ceil = (rx + 1) >> 1;

    if ((rx & 1) == 0) {
        u32 col_y = (((h << 4) >> 1) * half_rx) << 4;
        u32 col_c = col_y >> 1;
        offsets[0] = col_y;
        offsets[1] = col_y;
        offsets[2] = col_c;
        offsets[3] = col_c;
        *out_aligned = 0;
        return 0;
    }

    u32 h8 = (h << 4) >> 1;
    u32 col_y0 = (h8 * half_rx_ceil) << 4;
    offsets[0] = col_y0;
    offsets[2] = col_y0 >> 1;

    u32 col_y1 = (h8 * (range[0] >> 1)) << 4;
    u32 step_y = h8 << 4;
    offsets[1] = col_y1;
    offsets[3] = col_y1 >> 1;

    u32 tmp_yuv[12];
    tmp_yuv[0] = range[3];
    tmp_yuv[1] = 1;
    tmp_yuv[2] = yuv[2];
    tmp_yuv[3] = yuv[3];
    sub_0a5c(tmp_yuv, yuv, offsets, 2);

    int ret = sub_0b6c(*p_rgb, *p_unk, tmp_yuv);
    if (ret == 0) {
        *p_rgb = (u8 *)(*p_rgb) + 0x40;
        if (*p_unk != NULL)
            *p_unk = (u8 *)(*p_unk) + 0x40;
        offsets[1] += step_y;
        offsets[3] += (step_y >> 1);
    }
    *out_aligned = 1;
    return ret;
}

int sub_0d70(void *rgb, void *unk, u32 *range, u32 *yuv)
{
    void *cur_rgb = rgb;
    void *cur_unk = unk;
    u32 offsets[4];
    u32 aligned_w = 0;

    int ret = sub_0bf8(&cur_rgb, &cur_unk, range, yuv, offsets, &aligned_w);
    if (ret != 0)
        return ret;

    u32 rem_w = range[2] - aligned_w;
    u32 pairs = rem_w >> 1;
    u32 tmp_yuv[12];
    tmp_yuv[0] = range[3];
    tmp_yuv[1] = 2;
    tmp_yuv[2] = yuv[2];
    tmp_yuv[3] = yuv[3];

    if (pairs > 0) {
        u32 step_y = yuv[0] << 7;
        u32 step_c = step_y >> 1;
        for (u32 i = 0; i < pairs; i++) {
            sub_0a5c(tmp_yuv, yuv, offsets, 0);
            ret = sub_0b6c(cur_rgb, cur_unk, tmp_yuv);
            if (ret != 0)
                return ret;
            cur_rgb = (u8 *)cur_rgb + 0x80;
            if (cur_unk != NULL)
                cur_unk = (u8 *)cur_unk + 0x80;
            offsets[0] += step_y;
            offsets[1] += step_y;
            offsets[2] += step_c;
            offsets[3] += step_c;
        }
    }

    if (rem_w & 1) {
        tmp_yuv[1] = 1;
        sub_0a5c(tmp_yuv, yuv, offsets, 0);
        ret = sub_0b6c(cur_rgb, cur_unk, tmp_yuv);
    }
    return ret;
}

int sceMpegBasePESpacketCopy(u32 *pkt)
{
    int k1 = pspShiftK1();
    u32 k1_mask = pspGetK1();

    if ((u32)pkt & k1_mask) {
        pspSetK1(k1);
        return 0x80000023;
    }

    int ret = sceKernelWaitSema(g_mpegbase.sema_id, 1, NULL);
    if (ret < 0) {
        pspSetK1(k1);
        return ret;
    }

    pspCache(0x18, &g_mpegbase);
    u32 addr_hi = (k1_mask >> 31) << 30;
    SceMpegLLI *lli = g_mpegbase.lli;
    SceMpegLLI *lli_end = &g_mpegbase.lli[100];

    for (;;) {
        u32 src = pkt[0];
        u32 dst = pkt[1];
        u32 next_pkt = pkt[2];
        u32 len = pkt[3] & 0xFFF;

        lli->src = addr_hi | (src & 0x1FFFFFFF);
        lli->dst = addr_hi | (dst & 0x1FFFFFFF);

        if (len == 0)
            break;

        if (next_pkt == 0) {
            lli->next = 0;
            lli->ctrl = len | 0x8A909000U;
            if (g_mpegbase.is_uljs00006 == 0) {
                u32 wb_size = (u32)((u8 *)lli + 16 - (u8 *)&g_mpegbase);
                sceKernelDcacheWritebackRange(&g_mpegbase, wb_size);
            } else {
                sceKernelDcacheWritebackAll();
            }

            do {
                ret = sceDmacplus_driver_282CA0D7(&g_mpegbase);
                if (ret != 0 && ret != (int)0x80000021)
                    break;
                int sync_ret = sceDmacplus_driver_47D400CB(0, 0);
                if (ret == 0) {
                    ret = sync_ret;
                    break;
                }
            } while (ret != 0);

            sceKernelSignalSema(g_mpegbase.sema_id, 1);
            pspSetK1(k1);
            return ret;
        }

        SceMpegLLI *next_lli = lli + 1;
        lli->next = (u32)next_lli & 0x1FFFFFFF;
        lli->ctrl = len | 0x0A909000U;
        pkt = (u32 *)next_pkt;
        lli = next_lli;
        if (lli == lli_end)
            break;
    }

    sceKernelSignalSema(g_mpegbase.sema_id, 1);
    pspSetK1(k1);
    return 0x80000104;
}

int sceMpegBaseYCrCbCopy(u32 *dst, u32 *src, int flags)
{
    int k1 = pspShiftK1();
    u32 k1_mask = pspGetK1();

    if ((((u32)src | ((u32)src + 0x30)) & k1_mask) ||
        (((u32)dst | ((u32)dst + 0x30)) & k1_mask)) {
        pspSetK1(k1);
        return 0x80000023;
    }

    u32 base_y = src[4];
    if (flags & 1) {
        u32 dst_y = dst[4];
        dst[5] = dst_y + (src[5] - base_y);
        dst[8] = dst_y + (src[8] - base_y);
        dst[9] = dst_y + (src[9] - base_y);
    }

    if (flags & 2) {
        u32 dst_y = dst[4];
        dst[6]  = dst_y + (src[6]  - base_y);
        dst[7]  = dst_y + (src[7]  - base_y);
        dst[10] = dst_y + (src[10] - base_y);
        dst[11] = dst_y + (src[11] - base_y);
    }

    u32 y_size = src[6] - base_y;
    u32 c_size = y_size >> 1;
    if (y_size > 0xBFF40) {
        pspSetK1(k1);
        return 0x80000104;
    }

    int ret = sceKernelWaitSema(g_mpegbase.sema_id, 1, NULL);
    if (ret < 0) {
        pspSetK1(k1);
        return ret;
    }

    pspCache(0x18, &g_mpegbase);
    SceMpegLLI *lli = g_mpegbase.lli;

    if (flags & 1) {
        lli = sub_14d8(lli, dst[4], src[4], y_size);
        lli = sub_14d8(lli, dst[5], src[5], y_size);
        lli = sub_14d8(lli, dst[8], src[8], c_size);
        lli = sub_14d8(lli, dst[9], src[9], c_size);
    }
    if (flags & 2) {
        lli = sub_14d8(lli, dst[6],  src[6],  y_size);
        lli = sub_14d8(lli, dst[7],  src[7],  y_size);
        lli = sub_14d8(lli, dst[10], src[10], c_size);
        lli = sub_14d8(lli, dst[11], src[11], c_size);
    }

    if (lli != g_mpegbase.lli)
        ret = sub_1558(lli);

    sceKernelSignalSema(g_mpegbase.sema_id, 1);
    pspSetK1(k1);
    return ret;
}

int sceMpegBaseYCrCbCopyVme(u32 *dst, u32 *src, int flags)
{
    int k1 = pspShiftK1();
    u32 k1_mask = pspGetK1();

    if ((((u32)src | ((u32)src + 0x28)) & k1_mask) ||
        (((u32)dst | ((u32)dst + 0x28)) & k1_mask)) {
        pspSetK1(k1);
        return 0x80000023;
    }

    u32 w_info = src[1];
    u32 h_info = src[0];
    u32 extra = ((w_info & 0x7F) == 0x50) ? 3 : 2;
    u32 total = (((w_info >> 4) + extra) * h_info) << 4;
    u32 y_size = total >> 1;
    u32 c_size = total >> 3;

    if (y_size > 0xBFF40) {
        pspSetK1(k1);
        return 0x80000104;
    }

    int ret = sceKernelWaitSema(g_mpegbase.sema_id, 1, NULL);
    if (ret < 0) {
        pspSetK1(k1);
        return ret;
    }

    pspCache(0x18, &g_mpegbase);
    SceMpegLLI *lli = g_mpegbase.lli;

    if (flags & 1) {
        lli = sub_14d8(lli, dst[4], src[4], y_size);
        lli = sub_14d8(lli, dst[6], src[6], c_size);
        lli = sub_14d8(lli, dst[7], src[7], c_size);
    }
    if (flags & 2) {
        lli = sub_14d8(lli, dst[5], src[5], y_size);
        lli = sub_14d8(lli, dst[8], src[8], c_size);
        lli = sub_14d8(lli, dst[9], src[9], c_size);
    }

    if (lli != g_mpegbase.lli)
        ret = sub_1558(lli);

    sceKernelSignalSema(g_mpegbase.sema_id, 1);
    pspSetK1(k1);
    return ret;
}

SceMpegLLI *sub_14d8(SceMpegLLI *lli, u32 dst, u32 src, u32 size)
{
    if (size == 0)
        return lli;

    u32 k1_mask;
    __asm__ volatile("move %0, $k1" : "=r"(k1_mask));
    u32 addr_hi = (k1_mask >> 31) << 30;

    while (size != 0) {
        u32 chunk = (size < 0xFFF0) ? size : 0xFFF0;
        SceMpegLLI *next_lli = lli + 1;
        lli->src = addr_hi | (src & 0x1FFFFFFF);
        lli->dst = addr_hi | (dst & 0x1FFFFFFF);
        lli->next = (u32)next_lli & 0x1FFFFFFF;
        lli->ctrl = ((s32)chunk >> 4) | 0x01909000U;
        size -= chunk;
        dst += chunk;
        src += chunk;
        lli = next_lli;
    }
    return lli;
}

int sub_1558(SceMpegLLI *end_lli)
{
    end_lli[-1].next = 0;
    end_lli[-1].ctrl |= 0x80000000U;
    sceKernelDcacheWritebackInvalidateRange(&g_mpegbase, (u32)((u8 *)end_lli - (u8 *)&g_mpegbase));

    int ret;
    do {
        ret = sceDmacplus_driver_6945F1D3(&g_mpegbase);
        if (ret != 0 && ret != (int)0x80000021)
            return ret;
        sceDmacplus_driver_5FCF43BD(0, 0);
    } while (ret != 0);

    return 0;
}
