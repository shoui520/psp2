#include <common_imp.h>

extern int sceUsb_driver_1159202A(void);
extern int sceUsb_driver_18DB54DC(void);
extern int sceUsb_driver_292740A2(int cmd);
extern int sceUsb_driver_32627D8F(void *buf, u32 size);
extern int sceUsb_driver_3B578B06(void);
extern int sceUsb_driver_469C73EB(void);
extern int sceUsb_driver_63E55EBE(void);
extern int sceUsb_driver_6C68EE25(int id, void *cb);
extern int sceUsb_driver_8839766F(int cmd, int val);
extern int sceUsb_driver_8C079564(void *param);
extern int sceUsb_driver_96A32FBC(void);
extern int sceUsb_driver_A119F065(void);
extern int sceUsb_driver_A229F2CF(void);
extern int sceUsb_driver_B3E9ABCE(void);
extern int sceUsb_driver_BDA21CF7(void *param);
extern int sceUsb_driver_BFCDFDD1(void);
extern int sceUsb_driver_C99E1766(int id, void *cb);
extern int sceUsb_driver_CCA3CDF7(void *param);
extern int sceUsb_driver_D1F264A2(void);
extern int sceUsb_driver_D27EDB31(int arg);
extern int sceUsb_driver_D9EE03ED(void);
extern int sceUsb_driver_E09431A0(void);
extern int sceUsb_driver_ED8C8695(void);
extern int sceUsb_driver_F183B4EF(int sw);

extern int sceUsbAccGetAuthStat(void);

extern int sceKernelWaitThreadEnd(int thid, u32 *timeout);
extern int sceKernelTerminateDeleteThread(int thid);
extern int sceKernelGetThreadExitStatus(int thid);
extern int sceKernelCreateThread(const char *name, void *entry, int initPriority, int stackSize, u32 attr, void *option);
extern int sceKernelGetThreadmanIdType(int uid);
extern int sceKernelTerminateThread(int thid);
extern int sceKernelNotifyCallback(int cbid, int arg2);
extern int sceKernelStartThread(int thid, u32 arglen, void *argp);

extern int sceKernelCpuSuspendIntr(void);
extern void sceKernelCpuResumeIntr(int state);

extern int sceKermit_driver_5280B410(u32 id, void *cb);
extern void *memcpy(void *dst, const void *src, u32 n);

static const char *g_cam_thread_names[2] = {
    "SceUsbCamVideoFrame",
    "SceUsbCamStillFrame"
};

static int g_cam_thids[2];     /* 0x2070, 0x2074 */
static int g_saturation;       /* 0x2078 */
static int g_brightness;       /* 0x207c */
static int g_contrast;         /* 0x2080 */
static int g_sharpness;        /* 0x2084 */
static int g_zoom;             /* 0x2088 */
static int g_val_208c;         /* 0x208c */
static int g_reverse_mode;     /* 0x2090 */
static int g_ev_level;         /* 0x2094 */
static int g_effect_mode;      /* 0x2098 */
static int g_lens_dir;         /* 0x209c */
static int g_cam_type;         /* 0x20a0 */
static int g_cb_id;            /* 0x20a4 */
static int g_mic_thid;         /* 0x20a8 */

static int sub_1014(u32 args, void *argp);
static void sub_1084(void);
static int sub_10c8(int idx);
static int sub_11a4(int idx);
static int sub_125c(int cmd, int val, int *store);
static int sub_12c8(int *out, int val);
static int sub_17e8(void);
static int sub_184c(void);
static int sub_1888(u32 args, void *argp);

int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    sub_17e8();
    if (sceUsb_driver_C99E1766(1, NULL) < 0)
        return 1;
    for (int i = 0; i < 2; i++) {
        int thid = sceKernelCreateThread(g_cam_thread_names[i], sub_1014, 0x10, 0xf000, 0x00100001, NULL);
        g_cam_thids[i] = thid;
        if (thid < 0)
            return 1;
    }
    g_lens_dir = (sceUsb_driver_E09431A0() == 0) ? 1 : 0;
    if (sceKermit_driver_5280B410(0x0b, sub_1084) != 0)
        return 1;

    g_cb_id = -1;
    g_saturation = 0x7d;
    g_brightness = 0x80;
    g_contrast = 0x40;
    g_zoom = 0x0a;
    g_ev_level = 8;
    g_sharpness = 0;
    g_val_208c = 0;
    g_reverse_mode = 0;
    g_effect_mode = 0;
    g_cam_type = sceUsb_driver_B3E9ABCE();
    return 0;
}

int sceUsbCamSetupVideo(void *param, void *workarea, int wasize)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
        goto out;
    }
    if (sceUsbAccGetAuthStat() < 0) {
        ret = 0x80243902;
        goto out;
    }
    if (!param) {
        ret = 0x80243907;
        goto out;
    }
    if ((s32)((((u32)param + 0x34) | (u32)param) & pspGetK1()) < 0) {
        ret = 0x80243904;
        goto out;
    }
    if ((s32)(pspGetK1() & (((u32)workarea + (u32)wasize) | (u32)workarea | (u32)wasize)) < 0) {
        ret = 0x80243904;
        goto out;
    }
    if (wasize & 0x3f) {
        ret = 0x80243903;
        goto out;
    }

    int *p = (int *)param;
    g_effect_mode = p[8];
    g_saturation = p[4];
    g_brightness = p[5];
    g_contrast = p[6];
    g_sharpness = p[7];
    g_zoom = p[12];
    g_ev_level = p[11];
    ret = sceUsb_driver_BDA21CF7(param);

out:
    pspSetK1(k1);
    return ret;
}

int sceUsbCamReadVideoFrame(void *buf, u32 size)
{
    u32 k1 = pspShiftK1();
    u32 args[3];
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
        goto out;
    }
    if (!buf) {
        ret = 0x80243907;
        goto out;
    }
    if ((s32)(pspGetK1() & (((u32)buf + size) | (u32)buf | size)) < 0) {
        ret = 0x80243904;
        goto out;
    }
    if (size > 0x8000) {
        ret = 0x80243903;
        goto out;
    }
    if (g_cam_type != 0) {
        ret = 0x80243902;
        goto out;
    }

    args[0] = (u32)buf;
    args[1] = size;
    args[2] = (u32)sceUsb_driver_18DB54DC;
    ret = sceKernelStartThread(g_cam_thids[0], 0x0c, args);
    if (ret < 0)
        ret = 0x80243909;

out:
    pspSetK1(k1);
    return ret;
}

int sceUsbCamReadVideoFrameBlocking(void *buf, u32 size)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
        goto out;
    }
    if (!buf) {
        ret = 0x80243907;
        goto out;
    }
    if ((s32)(pspGetK1() & (((u32)buf + size) | (u32)buf | size)) < 0) {
        ret = 0x80243904;
        goto out;
    }
    if (size > 0x8000) {
        ret = 0x80243903;
        goto out;
    }
    if (g_cam_type != 0) {
        ret = 0x80243902;
        goto out;
    }

    ret = sceUsb_driver_18DB54DC();
    if (ret >= 0)
        memcpy(buf, (void *)0xA8300000, size);

out:
    pspSetK1(k1);
    return ret;
}

int sceUsbCamSetupStill(void *param)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
        goto out;
    }
    if (!param) {
        ret = 0x80243907;
        goto out;
    }
    if ((s32)((((u32)param + 0x18) | (u32)param) & pspGetK1()) < 0) {
        ret = 0x80243904;
        goto out;
    }
    if ((u32)(*(int *)param - 0x14) >= 5) {
        ret = 0x80243907;
        goto out;
    }
    ret = sceUsb_driver_8C079564(param);

out:
    pspSetK1(k1);
    return ret;
}

int sceUsbCamStillInput(void *buf, u32 size)
{
    u32 k1 = pspShiftK1();
    u32 args[3];
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
        goto out;
    }
    if (!buf) {
        ret = 0x80243907;
        goto out;
    }
    if (size < 0x40) {
        ret = 0x80243903;
        goto out;
    }
    if ((s32)(pspGetK1() & (((u32)buf + size) | (u32)buf | size)) < 0) {
        ret = 0x80243904;
        goto out;
    }
    if (size > 0x80000) {
        ret = 0x80243903;
        goto out;
    }
    if (g_cam_type != 0) {
        ret = 0x80243902;
        goto out;
    }

    args[0] = (u32)buf;
    args[1] = size;
    args[2] = (u32)sceUsb_driver_D1F264A2;
    ret = sceKernelStartThread(g_cam_thids[1], 0x0c, args);
    if (ret < 0)
        ret = 0x80243909;

out:
    pspSetK1(k1);
    return ret;
}

int sceUsbCamStillInputBlocking(void *buf, u32 size)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
        goto out;
    }
    if (!buf) {
        ret = 0x80243907;
        goto out;
    }
    if (size < 0x40) {
        ret = 0x80243903;
        goto out;
    }
    if ((s32)(pspGetK1() & (((u32)buf + size) | (u32)buf | size)) < 0) {
        ret = 0x80243904;
        goto out;
    }
    if (size > 0x80000) {
        ret = 0x80243903;
        goto out;
    }

    ret = sceUsb_driver_63E55EBE();
    if (ret >= 0) {
        ret = sceUsb_driver_D1F264A2();
        if (ret >= 0)
            memcpy(buf, (void *)0xA8300000, size);
        sceUsb_driver_ED8C8695();
    }

out:
    pspSetK1(k1);
    return ret;
}

int sceUsbCamStillCancelInput(void)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
        goto out;
    }
    ret = sceUsb_driver_BFCDFDD1();
    if (ret >= 0) {
        ret = sceKernelTerminateThread(g_cam_thids[1]);
        if (ret == (int)0x800201a2)
            ret = 0x80243908;
    }

out:
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetReverseMode(int *out)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
    } else if ((s32)((((u32)out + 4) | (u32)out) & pspGetK1()) < 0) {
        ret = 0x80243904;
    } else {
        *out = sceUsb_driver_292740A2(0x17c);
        ret = 0;
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCam_D293A100(int cbid)
{
    u32 k1 = pspShiftK1();
    int ret = 0;

    if (sceKernelGetThreadmanIdType(cbid) != 8) {
        ret = 0x80243905;
    } else {
        int intr = sceKernelCpuSuspendIntr();
        if (g_cb_id > 0) {
            ret = 0x80243910;
            g_cb_id = -1;
        } else {
            g_cb_id = cbid;
        }
        sceKernelCpuResumeIntr(intr);
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_D293A100(int cbid) __attribute__((alias("sceUsbCam_D293A100")));

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    sceKermit_driver_5280B410(0x0b, NULL);
    if (sceUsb_driver_6C68EE25(1, NULL) < 0)
        return 1;
    sub_184c();
    for (int i = 0; i < 2; i++)
        sceKernelTerminateDeleteThread(g_cam_thids[i]);
    return 0;
}

int sceUsbCamSetupVideoEx(void *param, void *workarea, int wasize)
{
    (void)param;
    (void)workarea;
    (void)wasize;
    return 0;
}

int sceUsbCamStartVideo(void)
{
    u32 k1 = pspShiftK1();
    int ret = (sceUsb_driver_D9EE03ED() != 0) ? sceUsb_driver_63E55EBE() : (int)0x80243908;
    pspSetK1(k1);
    return ret;
}

int sceUsbCamStopVideo(void)
{
    u32 k1 = pspShiftK1();
    int ret = sceUsb_driver_ED8C8695();
    pspSetK1(k1);
    return ret;
}

int sceUsbCamWaitReadVideoFrameEnd(void)
{
    return sub_10c8(0);
}

int sceUsbCamPollReadVideoFrameEnd(void)
{
    return sub_11a4(0);
}

int sceUsbCamGetReadVideoFrameSize(void)
{
    u32 k1 = pspShiftK1();
    int ret = (sceUsb_driver_D9EE03ED() != 0) ? sceUsb_driver_A119F065() : (int)0x80243908;
    pspSetK1(k1);
    return ret;
}

int sceUsbCamSetupStillEx(void *param)
{
    (void)param;
    return 0;
}

int sceUsbCamStillWaitInputEnd(void)
{
    return sub_10c8(1);
}

int sceUsbCamStillPollInputEnd(void)
{
    return sub_11a4(1);
}

int sceUsbCamStillGetInputLength(void)
{
    u32 k1 = pspShiftK1();
    int ret = (sceUsb_driver_D9EE03ED() != 0) ? sceUsb_driver_3B578B06() : (int)0x80243908;
    pspSetK1(k1);
    return ret;
}

static int sub_125c(int cmd, int val, int *store);
static int sub_12c8(int *out, int val);

extern int sceUsb_driver_var_3EA085BF;
extern int sceUsb_driver_var_A942D291;

__attribute__((noinline))
static int sub_125c(int cmd, int val, int *store)
{
    if (sceUsb_driver_D9EE03ED() == 0)
        return 0x80243908;
    int ret = sceUsb_driver_8839766F(cmd, val);
    if (ret == 0)
        *store = val;
    return ret;
}

__attribute__((noinline))
static int sub_12c8(int *out, int val)
{
    if (sceUsb_driver_D9EE03ED() == 0)
        return 0x80243908;
    if ((s32)((((u32)out + 4) | (u32)out) & pspGetK1()) < 0)
        return 0x80243904;
    *out = val;
    return 0;
}

int sceUsbCamSetSaturation(int val)
{
    if (sceUsb_driver_var_3EA085BF == 2)
        val += 0x0b;
    u32 k1 = pspShiftK1();
    int ret = sub_125c(0x15a, val, &g_saturation);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetSaturation(int *out)
{
    u32 k1 = pspShiftK1();
    int ret = sub_12c8(out, g_saturation);
    if (ret == 0 && sceUsb_driver_var_3EA085BF == 2)
        *out -= 0x0b;
    pspSetK1(k1);
    return ret;
}

int sceUsbCamSetBrightness(int val)
{
    int mode = sceUsb_driver_var_3EA085BF;
    int adj = val;
    if (mode == 2)
        adj = val + 0x0c;
    else if (mode == 1)
        adj = ((val - 0x80) / 2) + 0x80;
    u32 k1 = pspShiftK1();
    int ret = sub_125c(0x178, adj, &g_brightness);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetBrightness(int *out)
{
    u32 k1 = pspShiftK1();
    int ret = sub_12c8(out, g_brightness);
    if (ret == 0) {
        int mode = sceUsb_driver_var_3EA085BF;
        if (mode == 1)
            *out = (*out << 1) - 0x80;
        else if (mode == 2)
            *out -= 0x0c;
    }
    pspSetK1(k1);
    return ret;
}

int sceUsbCamSetContrast(int val)
{
    u32 k1 = pspShiftK1();
    int ret = sub_125c(0x161, val, &g_contrast);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetContrast(int *out)
{
    u32 k1 = pspShiftK1();
    int ret = sub_12c8(out, g_contrast);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamSetSharpness(int val)
{
    u32 k1 = pspShiftK1();
    int ret = sub_125c(0x179, val, &g_sharpness);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetSharpness(int *out)
{
    u32 k1 = pspShiftK1();
    int ret = sub_12c8(out, g_sharpness);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamSetZoom(int val)
{
    u32 k1 = pspShiftK1();
    int ret = sub_125c(0x167, val, &g_zoom);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetZoom(int *out)
{
    u32 k1 = pspShiftK1();
    int ret = sub_12c8(out, g_zoom);
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_6784E6A8(int val)
{
    u32 k1 = pspShiftK1();
    int ret = sub_125c(0x15e, val, &g_val_208c);
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_6784E6A8(int val) __attribute__((alias("sceUsbCam_6784E6A8")));

int sceUsbCam_AA7D94BA(int *out)
{
    u32 k1 = pspShiftK1();
    int ret = sub_12c8(out, g_val_208c);
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_AA7D94BA(int *out) __attribute__((alias("sceUsbCam_AA7D94BA")));

int sceUsbCamSetReverseMode(int val)
{
    u32 k1 = pspShiftK1();
    int ret = sub_125c(0x16c, val, &g_reverse_mode);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamSetEvLevel(int val)
{
    u32 k1 = pspShiftK1();
    int ret = sub_125c(0x162, val, &g_ev_level);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetEvLevel(int *out)
{
    u32 k1 = pspShiftK1();
    int ret = sub_12c8(out, g_ev_level);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamSetImageEffectMode(int val)
{
    u32 k1 = pspShiftK1();
    int ret = sub_125c(0x170, val, &g_effect_mode);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetImageEffectMode(int *out)
{
    u32 k1 = pspShiftK1();
    int ret = sub_12c8(out, g_effect_mode);
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_00631D06(void)
{
    return 1;
}

int sceUsbCam_driver_00631D06(void) __attribute__((alias("sceUsbCam_00631D06")));

int sceUsbCam_D865997B(void)
{
    return 0;
}

int sceUsbCam_driver_D865997B(void) __attribute__((alias("sceUsbCam_D865997B")));

int sceUsbCamAutoImageReverseSW(int sw)
{
    u32 k1 = pspShiftK1();
    int ret = (sceUsb_driver_D9EE03ED() != 0) ? sceUsb_driver_F183B4EF(sw) : (int)0x80243908;
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetAutoImageReverseState(void)
{
    u32 k1 = pspShiftK1();
    int ret = (sceUsb_driver_D9EE03ED() != 0) ? sceUsb_driver_A229F2CF() : (int)0x80243908;
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetLensDirection(void)
{
    u32 k1 = pspShiftK1();
    int ret = g_lens_dir;
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_41EE8797(void)
{
    u32 k1 = pspShiftK1();
    int intr = sceKernelCpuSuspendIntr();
    int ret = g_cb_id;
    if (ret < 0)
        ret = 0x8024390f;
    else
        g_cb_id = -1;
    sceKernelCpuResumeIntr(intr);
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_41EE8797(void) __attribute__((alias("sceUsbCam_41EE8797")));

__attribute__((noinline))
static int sub_1014(u32 args, void *argp)
{
    if (args != 0x0c)
        return 0x80000104;
    u32 *p = (u32 *)argp;
    void *buf = (void *)p[0];
    u32 size = p[1];
    int (*fn)(void) = (int (*)(void))p[2];
    int ret = fn();
    if (ret >= 0)
        memcpy(buf, (void *)0xA8300000, size);
    return ret;
}

static void sub_1084(void)
{
    g_lens_dir = (g_lens_dir == 0) ? 1 : 0;
    if (g_cb_id > 0)
        sceKernelNotifyCallback(g_cb_id, g_lens_dir);
}

__attribute__((noinline))
static int sub_10c8(int idx)
{
    u32 k1 = pspShiftK1();
    u32 timeout = 0x2dc6c0;
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
        goto out;
    }
    if (g_cam_type != 0) {
        ret = 0x80243902;
        goto out;
    }

    ret = sceKernelWaitThreadEnd(g_cam_thids[idx], &timeout);
    if (ret < 0) {
        if (ret == (int)0x800201a7 || ret == (int)0x800201a2 || ret == (int)0x800201ac)
            ret = 0x8024390c;
        else if (ret == (int)0x800201a8)
            ret = 0x80243902;
    }

out:
    pspSetK1(k1);
    return ret;
}

__attribute__((noinline))
static int sub_11a4(int idx)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
        goto out;
    }
    if (g_cam_type != 0) {
        ret = 0x80243902;
        goto out;
    }

    ret = sceKernelGetThreadExitStatus(g_cam_thids[idx]);
    if (ret < 0) {
        if (ret == (int)0x800201a2)
            ret = 0x8024390c;
        else if (ret == (int)0x800201a4)
            ret = 0x8024390e;
        else
            ret = 0x80243902;
    }

out:
    pspSetK1(k1);
    return ret;
}

int sceUsbCamSetupMic(void *param, void *workarea, int wasize)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
        goto out;
    }
    if ((u32)wasize < 0x108) {
        ret = 0x8024390a;
        goto out;
    }
    if (!param || (s32)((((u32)param + 0x14) | (u32)param) & pspGetK1()) < 0) {
        ret = 0x80243904;
        goto out;
    }
    if (!workarea || (s32)(pspGetK1() & (((u32)workarea + (u32)wasize) | (u32)workarea | (u32)wasize)) < 0) {
        ret = 0x80243904;
        goto out;
    }

    int freq = *((int *)param + 4);
    if (freq != 44100 && freq != 22050 && freq != 11025) {
        ret = 0x8024390b;
        goto out;
    }
    ret = sceUsb_driver_CCA3CDF7(param);

out:
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_3DC0088E(void *buf, u32 size)
{
    u32 k1 = pspShiftK1();
    u32 args[2];
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
        goto out;
    }
    if (!buf) {
        ret = 0x80243907;
        goto out;
    }
    if ((s32)(pspGetK1() & (((u32)buf + size) | (u32)buf | size)) < 0) {
        ret = 0x80243904;
        goto out;
    }
    if (sceUsb_driver_B3E9ABCE() < 0) {
        ret = 0x80243902;
        goto out;
    }

    args[0] = (u32)buf;
    args[1] = size;
    ret = sceKernelStartThread(g_mic_thid, 8, args);
    if (ret < 0)
        ret = 0x80243909;

out:
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_3DC0088E(void *buf, u32 size) __attribute__((alias("sceUsbCam_3DC0088E")));

int sceUsbCam_36636925(void *buf, u32 size)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
    } else if (!buf) {
        ret = 0x80243907;
    } else if ((s32)(pspGetK1() & (((u32)buf + size) | (u32)buf | size)) < 0) {
        ret = 0x80243904;
    } else {
        ret = sceUsb_driver_32627D8F(buf, size);
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_36636925(void *buf, u32 size) __attribute__((alias("sceUsbCam_36636925")));

int sceUsbCam_B048A67D(void)
{
    u32 k1 = pspShiftK1();
    u32 timeout = 0x2dc6c0;
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
        goto out;
    }
    if (sceUsb_driver_B3E9ABCE() < 0) {
        ret = 0x80243902;
        goto out;
    }

    ret = sceKernelWaitThreadEnd(g_mic_thid, &timeout);
    if (ret < 0) {
        if (ret == (int)0x800201a7 || ret == (int)0x800201a2 || ret == (int)0x800201ac)
            ret = 0x8024390c;
        else if (ret == (int)0x800201a8)
            ret = 0x80243902;
    }

out:
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_B048A67D(void) __attribute__((alias("sceUsbCam_B048A67D")));

int sceUsbCam_F8847F60(void)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
        goto out;
    }
    if (sceUsb_driver_B3E9ABCE() < 0) {
        ret = 0x80243902;
        goto out;
    }

    ret = sceKernelGetThreadExitStatus(g_mic_thid);
    if (ret < 0) {
        if (ret == (int)0x800201a2)
            ret = 0x8024390c;
        else if (ret == (int)0x800201a4)
            ret = 0x8024390e;
    }

out:
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_F8847F60(void) __attribute__((alias("sceUsbCam_F8847F60")));

int sceUsbCam_2E930264(void)
{
    return 0;
}

int sceUsbCam_driver_2E930264(void) __attribute__((alias("sceUsbCam_2E930264")));

int sceUsbCam_82A64030(void)
{
    u32 k1 = pspShiftK1();
    int ret = (sceUsb_driver_D9EE03ED() != 0) ? sceUsb_driver_1159202A() : (int)0x80243908;
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_82A64030(void) __attribute__((alias("sceUsbCam_82A64030")));

int sceUsbCam_5145868A(void)
{
    u32 k1 = pspShiftK1();
    int ret = sceUsb_driver_469C73EB();
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_5145868A(void) __attribute__((alias("sceUsbCam_5145868A")));

int sceUsbCam_5778B452(void)
{
    u32 k1 = pspShiftK1();
    int ret = (sceUsb_driver_D9EE03ED() != 0) ? sceUsb_driver_96A32FBC() : (int)0x80243908;
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_5778B452(void) __attribute__((alias("sceUsbCam_5778B452")));

int sceUsbCam_08AEE98A(int arg)
{
    u32 k1 = pspShiftK1();
    int ret = (sceUsb_driver_D9EE03ED() != 0) ? sceUsb_driver_D27EDB31(arg) : (int)0x80243908;
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_08AEE98A(int arg) __attribute__((alias("sceUsbCam_08AEE98A")));

static int sub_17e8(void)
{
    if (sceUsb_driver_C99E1766(2, NULL) < 0)
        return 1;
    int thid = sceKernelCreateThread("SceUsbCamMic", sub_1888, 0x10, 0xf000, 0x00100001, NULL);
    g_mic_thid = thid;
    return (thid < 0) ? 1 : 0;
}

static int sub_184c(void)
{
    if (sceUsb_driver_6C68EE25(2, NULL) < 0)
        return 1;
    sceKernelTerminateDeleteThread(g_mic_thid);
    return 0;
}

static int sub_1888(u32 args, void *argp)
{
    if (args != 8)
        return 0x80000104;
    u32 *p = (u32 *)argp;
    return sceUsb_driver_32627D8F((void *)p[0], p[1]);
}
