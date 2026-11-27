#include <common_imp.h>

#define SCE_USBCAM_ERROR_NOT_AVAILABLE     ((int)0x80243902)
#define SCE_USBCAM_ERROR_INVALID_SIZE      ((int)0x80243903)
#define SCE_USBCAM_ERROR_PRIV_REQUIRED     ((int)0x80243904)
#define SCE_USBCAM_ERROR_INVALID_ID        ((int)0x80243905)
#define SCE_USBCAM_ERROR_INVALID_VALUE     ((int)0x80243907)
#define SCE_USBCAM_ERROR_NOT_STARTED       ((int)0x80243908)
#define SCE_USBCAM_ERROR_INPUT_BUSY        ((int)0x80243909)
#define SCE_USBCAM_ERROR_BUFFER_SIZE       ((int)0x8024390A)
#define SCE_USBCAM_ERROR_INVALID_FREQUENCY ((int)0x8024390B)
#define SCE_USBCAM_ERROR_NOT_START_READ    ((int)0x8024390C)
#define SCE_USBCAM_ERROR_NOT_READ_DONE     ((int)0x8024390E)
#define SCE_USBCAM_ERROR_NO_REG_CB         ((int)0x8024390F)
#define SCE_USBCAM_ERROR_ALREADY_REG_CB    ((int)0x80243910)

#define SCE_USBCAM_FRAME_BUFFER_ADDR       ((void *)0xA8300000)

typedef struct SceUsbCamVideoParam {
    int size;
    int resolution;
    int frame_rate;
    int white_balance;
    int saturation;
    int brightness;
    int contrast;
    int sharpness;
    int effect_mode;
    u32 target_file_size;
    int sensitivity;
    int evlevel;
    int zoom;
} SceUsbCamVideoParam;

typedef struct SceUsbCamStillParam {
    int size;
    int resolution;
    u32 target_size;
    int flip_mirror;
    int self_timer;
    int comp_qs;
} SceUsbCamStillParam;

typedef struct SceUsbCamMicParam {
    int size;
    int alc;
    int gain;
    int noiz;
    int freq;
} SceUsbCamMicParam;

typedef struct SceUsbCamFrameArgs {
    void *buf;
    u32 size;
    int (*readFunc)(void);
} SceUsbCamFrameArgs;

typedef struct SceUsbCamMicArgs {
    void *buf;
    u32 size;
} SceUsbCamMicArgs;

extern int sceUsb_driver_1159202A(void);
extern int sceUsb_driver_18DB54DC(void);
extern int sceUsb_driver_292740A2(int cmd);
extern int sceUsb_driver_32627D8F(void *buf, u32 size);
extern int sceUsb_driver_3B578B06(void);
extern int sceUsb_driver_469C73EB(void);
extern int sceUsb_driver_63E55EBE(void);
extern int sceUsb_driver_6C68EE25(int id, void *cb);
extern int sceUsb_driver_8839766F(int cmd, int val);
extern int sceUsb_driver_8C079564(SceUsbCamStillParam *param);
extern int sceUsb_driver_96A32FBC(void);
extern int sceUsb_driver_A119F065(void);
extern int sceUsb_driver_A229F2CF(void);
extern int sceUsb_driver_B3E9ABCE(void);
extern int sceUsb_driver_BDA21CF7(SceUsbCamVideoParam *param);
extern int sceUsb_driver_BFCDFDD1(void);
extern int sceUsb_driver_C99E1766(int id, void *cb);
extern int sceUsb_driver_CCA3CDF7(SceUsbCamMicParam *param);
extern int sceUsb_driver_D1F264A2(void);
extern int sceUsb_driver_D27EDB31(int gain);
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
extern int sceKernelNotifyCallback(int cbid, int notifyArg);
extern int sceKernelStartThread(int thid, u32 arglen, void *argp);

extern int sceKernelCpuSuspendIntr(void);
extern void sceKernelCpuResumeIntr(int state);

extern int sceKermit_driver_5280B410(u32 id, void *cb);
extern void *memcpy(void *dst, const void *src, u32 n);

static const char *g_cam_thread_names[2] = {
    "SceUsbCamVideoFrame",
    "SceUsbCamStillFrame"
};

static int g_cam_thids[2];
static int g_saturation;
static int g_brightness;
static int g_contrast;
static int g_sharpness;
static int g_zoom;
static int g_anti_flicker;
static int g_reverse_mode;
static int g_ev_level;
static int g_effect_mode;
static int g_lens_dir;
static int g_cam_type;
static int g_cb_id;
static int g_mic_thid;

static int usbCamFrameThread(u32 args, void *argp);
static void usbCamLensEventHandler(void);
static int usbCamWaitFrameThread(int idx);
static int usbCamPollFrameThread(int idx);
static int usbCamSetParam(int cmd, int val, int *store);
static int usbCamGetParam(int *out, int val);
static int usbCamInitMic(void);
static int usbCamTermMic(void);
static int usbCamMicThread(u32 args, void *argp);

int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    usbCamInitMic();
    if (sceUsb_driver_C99E1766(1, NULL) < 0)
        return 1;
    for (int i = 0; i < 2; i++) {
        int thid = sceKernelCreateThread(g_cam_thread_names[i], usbCamFrameThread, 0x10, 0xf000, 0x00100001, NULL);
        g_cam_thids[i] = thid;
        if (thid < 0)
            return 1;
    }
    g_lens_dir = (sceUsb_driver_E09431A0() == 0) ? 1 : 0;
    if (sceKermit_driver_5280B410(0x0b, usbCamLensEventHandler) != 0)
        return 1;

    g_cb_id = -1;
    g_saturation = 0x7d;
    g_brightness = 0x80;
    g_contrast = 0x40;
    g_zoom = 0x0a;
    g_ev_level = 8;
    g_sharpness = 0;
    g_anti_flicker = 0;
    g_reverse_mode = 0;
    g_effect_mode = 0;
    g_cam_type = sceUsb_driver_B3E9ABCE();
    return 0;
}

int sceUsbCamSetupVideo(SceUsbCamVideoParam *param, void *workarea, int wasize)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = SCE_USBCAM_ERROR_NOT_STARTED;
    } else if (sceUsbAccGetAuthStat() < 0) {
        ret = SCE_USBCAM_ERROR_NOT_AVAILABLE;
    } else if (!param) {
        ret = SCE_USBCAM_ERROR_INVALID_VALUE;
    } else if ((s32)((((u32)param + sizeof(SceUsbCamVideoParam)) | (u32)param) & pspGetK1()) < 0) {
        ret = SCE_USBCAM_ERROR_PRIV_REQUIRED;
    } else if ((s32)(pspGetK1() & (((u32)workarea + (u32)wasize) | (u32)workarea | (u32)wasize)) < 0) {
        ret = SCE_USBCAM_ERROR_PRIV_REQUIRED;
    } else if (wasize & 0x3f) {
        ret = SCE_USBCAM_ERROR_INVALID_SIZE;
    } else {
        g_effect_mode = param->effect_mode;
        g_saturation = param->saturation;
        g_brightness = param->brightness;
        g_contrast = param->contrast;
        g_sharpness = param->sharpness;
        g_zoom = param->zoom;
        g_ev_level = param->evlevel;
        ret = sceUsb_driver_BDA21CF7(param);
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCamReadVideoFrame(void *buf, u32 size)
{
    u32 k1 = pspShiftK1();
    SceUsbCamFrameArgs frameArgs;
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = SCE_USBCAM_ERROR_NOT_STARTED;
    } else if (!buf) {
        ret = SCE_USBCAM_ERROR_INVALID_VALUE;
    } else if ((s32)(pspGetK1() & (((u32)buf + size) | (u32)buf | size)) < 0) {
        ret = SCE_USBCAM_ERROR_PRIV_REQUIRED;
    } else if (size > 0x8000) {
        ret = SCE_USBCAM_ERROR_INVALID_SIZE;
    } else if (g_cam_type != 0) {
        ret = SCE_USBCAM_ERROR_NOT_AVAILABLE;
    } else {
        frameArgs.buf = buf;
        frameArgs.size = size;
        frameArgs.readFunc = sceUsb_driver_18DB54DC;
        ret = sceKernelStartThread(g_cam_thids[0], sizeof(frameArgs), &frameArgs);
        if (ret < 0)
            ret = SCE_USBCAM_ERROR_INPUT_BUSY;
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCamReadVideoFrameBlocking(void *buf, u32 size)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = SCE_USBCAM_ERROR_NOT_STARTED;
    } else if (!buf) {
        ret = SCE_USBCAM_ERROR_INVALID_VALUE;
    } else if ((s32)(pspGetK1() & (((u32)buf + size) | (u32)buf | size)) < 0) {
        ret = SCE_USBCAM_ERROR_PRIV_REQUIRED;
    } else if (size > 0x8000) {
        ret = SCE_USBCAM_ERROR_INVALID_SIZE;
    } else if (g_cam_type != 0) {
        ret = SCE_USBCAM_ERROR_NOT_AVAILABLE;
    } else {
        ret = sceUsb_driver_18DB54DC();
        if (ret >= 0)
            memcpy(buf, SCE_USBCAM_FRAME_BUFFER_ADDR, size);
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCamSetupStill(SceUsbCamStillParam *param)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = SCE_USBCAM_ERROR_NOT_STARTED;
    } else if (!param) {
        ret = SCE_USBCAM_ERROR_INVALID_VALUE;
    } else if ((s32)((((u32)param + sizeof(SceUsbCamStillParam)) | (u32)param) & pspGetK1()) < 0) {
        ret = SCE_USBCAM_ERROR_PRIV_REQUIRED;
    } else if ((u32)(param->size - 0x14) >= 5) {
        ret = SCE_USBCAM_ERROR_INVALID_VALUE;
    } else {
        ret = sceUsb_driver_8C079564(param);
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCamStillInput(void *buf, u32 size)
{
    u32 k1 = pspShiftK1();
    SceUsbCamFrameArgs frameArgs;
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = SCE_USBCAM_ERROR_NOT_STARTED;
    } else if (!buf) {
        ret = SCE_USBCAM_ERROR_INVALID_VALUE;
    } else if (size < 0x40) {
        ret = SCE_USBCAM_ERROR_INVALID_SIZE;
    } else if ((s32)(pspGetK1() & (((u32)buf + size) | (u32)buf | size)) < 0) {
        ret = SCE_USBCAM_ERROR_PRIV_REQUIRED;
    } else if (size > 0x80000) {
        ret = SCE_USBCAM_ERROR_INVALID_SIZE;
    } else if (g_cam_type != 0) {
        ret = SCE_USBCAM_ERROR_NOT_AVAILABLE;
    } else {
        frameArgs.buf = buf;
        frameArgs.size = size;
        frameArgs.readFunc = sceUsb_driver_D1F264A2;
        ret = sceKernelStartThread(g_cam_thids[1], sizeof(frameArgs), &frameArgs);
        if (ret < 0)
            ret = SCE_USBCAM_ERROR_INPUT_BUSY;
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCamStillInputBlocking(void *buf, u32 size)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = SCE_USBCAM_ERROR_NOT_STARTED;
    } else if (!buf) {
        ret = SCE_USBCAM_ERROR_INVALID_VALUE;
    } else if (size < 0x40) {
        ret = SCE_USBCAM_ERROR_INVALID_SIZE;
    } else if ((s32)(pspGetK1() & (((u32)buf + size) | (u32)buf | size)) < 0) {
        ret = SCE_USBCAM_ERROR_PRIV_REQUIRED;
    } else if (size > 0x80000) {
        ret = SCE_USBCAM_ERROR_INVALID_SIZE;
    } else {
        ret = sceUsb_driver_63E55EBE();
        if (ret >= 0) {
            ret = sceUsb_driver_D1F264A2();
            if (ret >= 0)
                memcpy(buf, SCE_USBCAM_FRAME_BUFFER_ADDR, size);
            sceUsb_driver_ED8C8695();
        }
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCamStillCancelInput(void)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = SCE_USBCAM_ERROR_NOT_STARTED;
    } else {
        ret = sceUsb_driver_BFCDFDD1();
        if (ret >= 0) {
            ret = sceKernelTerminateThread(g_cam_thids[1]);
            if (ret == (int)0x800201a2)
                ret = SCE_USBCAM_ERROR_NOT_STARTED;
        }
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetReverseMode(int *out)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = SCE_USBCAM_ERROR_NOT_STARTED;
    } else if ((s32)((((u32)out + 4) | (u32)out) & pspGetK1()) < 0) {
        ret = SCE_USBCAM_ERROR_PRIV_REQUIRED;
    } else {
        *out = sceUsb_driver_292740A2(0x17c);
        ret = 0;
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCamRegisterLensRotationCallback(int cbid)
{
    u32 k1 = pspShiftK1();
    int ret = 0;

    if (sceKernelGetThreadmanIdType(cbid) != 8) {
        ret = SCE_USBCAM_ERROR_INVALID_ID;
    } else {
        int intr = sceKernelCpuSuspendIntr();
        if (g_cb_id > 0) {
            ret = SCE_USBCAM_ERROR_ALREADY_REG_CB;
            g_cb_id = -1;
        } else {
            g_cb_id = cbid;
        }
        sceKernelCpuResumeIntr(intr);
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_D293A100(int cbid) __attribute__((alias("sceUsbCamRegisterLensRotationCallback")));

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    sceKermit_driver_5280B410(0x0b, NULL);
    if (sceUsb_driver_6C68EE25(1, NULL) < 0)
        return 1;
    usbCamTermMic();
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
    int ret = (sceUsb_driver_D9EE03ED() != 0) ? sceUsb_driver_63E55EBE() : SCE_USBCAM_ERROR_NOT_STARTED;
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
    return usbCamWaitFrameThread(0);
}

int sceUsbCamPollReadVideoFrameEnd(void)
{
    return usbCamPollFrameThread(0);
}

int sceUsbCamGetReadVideoFrameSize(void)
{
    u32 k1 = pspShiftK1();
    int ret = (sceUsb_driver_D9EE03ED() != 0) ? sceUsb_driver_A119F065() : SCE_USBCAM_ERROR_NOT_STARTED;
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
    return usbCamWaitFrameThread(1);
}

int sceUsbCamStillPollInputEnd(void)
{
    return usbCamPollFrameThread(1);
}

int sceUsbCamStillGetInputLength(void)
{
    u32 k1 = pspShiftK1();
    int ret = (sceUsb_driver_D9EE03ED() != 0) ? sceUsb_driver_3B578B06() : SCE_USBCAM_ERROR_NOT_STARTED;
    pspSetK1(k1);
    return ret;
}

extern int sceUsb_driver_var_3EA085BF;
extern int sceUsb_driver_var_A942D291;

__attribute__((noinline))
static int usbCamSetParam(int cmd, int val, int *store)
{
    if (sceUsb_driver_D9EE03ED() == 0)
        return SCE_USBCAM_ERROR_NOT_STARTED;
    int ret = sceUsb_driver_8839766F(cmd, val);
    if (ret == 0)
        *store = val;
    return ret;
}

__attribute__((noinline))
static int usbCamGetParam(int *out, int val)
{
    if (sceUsb_driver_D9EE03ED() == 0)
        return SCE_USBCAM_ERROR_NOT_STARTED;
    if ((s32)((((u32)out + 4) | (u32)out) & pspGetK1()) < 0)
        return SCE_USBCAM_ERROR_PRIV_REQUIRED;
    *out = val;
    return 0;
}

int sceUsbCamSetSaturation(int val)
{
    if (sceUsb_driver_var_3EA085BF == 2)
        val += 0x0b;
    u32 k1 = pspShiftK1();
    int ret = usbCamSetParam(0x15a, val, &g_saturation);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetSaturation(int *out)
{
    u32 k1 = pspShiftK1();
    int ret = usbCamGetParam(out, g_saturation);
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
    int ret = usbCamSetParam(0x178, adj, &g_brightness);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetBrightness(int *out)
{
    u32 k1 = pspShiftK1();
    int ret = usbCamGetParam(out, g_brightness);
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
    int ret = usbCamSetParam(0x161, val, &g_contrast);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetContrast(int *out)
{
    u32 k1 = pspShiftK1();
    int ret = usbCamGetParam(out, g_contrast);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamSetSharpness(int val)
{
    u32 k1 = pspShiftK1();
    int ret = usbCamSetParam(0x179, val, &g_sharpness);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetSharpness(int *out)
{
    u32 k1 = pspShiftK1();
    int ret = usbCamGetParam(out, g_sharpness);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamSetZoom(int val)
{
    u32 k1 = pspShiftK1();
    int ret = usbCamSetParam(0x167, val, &g_zoom);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetZoom(int *out)
{
    u32 k1 = pspShiftK1();
    int ret = usbCamGetParam(out, g_zoom);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamSetAntiFlicker(int val)
{
    u32 k1 = pspShiftK1();
    int ret = usbCamSetParam(0x15e, val, &g_anti_flicker);
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_6784E6A8(int val) __attribute__((alias("sceUsbCamSetAntiFlicker")));

int sceUsbCamGetAntiFlicker(int *out)
{
    u32 k1 = pspShiftK1();
    int ret = usbCamGetParam(out, g_anti_flicker);
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_AA7D94BA(int *out) __attribute__((alias("sceUsbCamGetAntiFlicker")));

int sceUsbCamSetReverseMode(int val)
{
    u32 k1 = pspShiftK1();
    int ret = usbCamSetParam(0x16c, val, &g_reverse_mode);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamSetEvLevel(int val)
{
    u32 k1 = pspShiftK1();
    int ret = usbCamSetParam(0x162, val, &g_ev_level);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetEvLevel(int *out)
{
    u32 k1 = pspShiftK1();
    int ret = usbCamGetParam(out, g_ev_level);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamSetImageEffectMode(int val)
{
    u32 k1 = pspShiftK1();
    int ret = usbCamSetParam(0x170, val, &g_effect_mode);
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetImageEffectMode(int *out)
{
    u32 k1 = pspShiftK1();
    int ret = usbCamGetParam(out, g_effect_mode);
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
    int ret = (sceUsb_driver_D9EE03ED() != 0) ? sceUsb_driver_F183B4EF(sw) : SCE_USBCAM_ERROR_NOT_STARTED;
    pspSetK1(k1);
    return ret;
}

int sceUsbCamGetAutoImageReverseState(void)
{
    u32 k1 = pspShiftK1();
    int ret = (sceUsb_driver_D9EE03ED() != 0) ? sceUsb_driver_A229F2CF() : SCE_USBCAM_ERROR_NOT_STARTED;
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

int sceUsbCamUnregisterLensRotationCallback(void)
{
    u32 k1 = pspShiftK1();
    int intr = sceKernelCpuSuspendIntr();
    int ret = g_cb_id;
    if (ret < 0)
        ret = SCE_USBCAM_ERROR_NO_REG_CB;
    else
        g_cb_id = -1;
    sceKernelCpuResumeIntr(intr);
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_41EE8797(void) __attribute__((alias("sceUsbCamUnregisterLensRotationCallback")));

__attribute__((noinline))
static int usbCamFrameThread(u32 args, void *argp)
{
    if (args != sizeof(SceUsbCamFrameArgs))
        return 0x80000104;
    SceUsbCamFrameArgs *frameArgs = (SceUsbCamFrameArgs *)argp;
    void *buf = frameArgs->buf;
    u32 size = frameArgs->size;
    int (*readFunc)(void) = frameArgs->readFunc;
    int ret = readFunc();
    if (ret >= 0)
        memcpy(buf, SCE_USBCAM_FRAME_BUFFER_ADDR, size);
    return ret;
}

static void usbCamLensEventHandler(void)
{
    g_lens_dir = (g_lens_dir == 0) ? 1 : 0;
    if (g_cb_id > 0)
        sceKernelNotifyCallback(g_cb_id, g_lens_dir);
}

__attribute__((noinline))
static int usbCamWaitFrameThread(int idx)
{
    u32 k1 = pspShiftK1();
    u32 timeout = 0x2dc6c0;
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = SCE_USBCAM_ERROR_NOT_STARTED;
    } else if (g_cam_type != 0) {
        ret = SCE_USBCAM_ERROR_NOT_AVAILABLE;
    } else {
        ret = sceKernelWaitThreadEnd(g_cam_thids[idx], &timeout);
        if (ret < 0) {
            switch (ret) {
            case (int)0x800201a2:
            case (int)0x800201a7:
            case (int)0x800201ac:
                ret = SCE_USBCAM_ERROR_NOT_START_READ;
                break;
            case (int)0x800201a8:
                ret = SCE_USBCAM_ERROR_NOT_AVAILABLE;
                break;
            default:
                break;
            }
        }
    }

    pspSetK1(k1);
    return ret;
}

__attribute__((noinline))
static int usbCamPollFrameThread(int idx)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = SCE_USBCAM_ERROR_NOT_STARTED;
    } else if (g_cam_type != 0) {
        ret = SCE_USBCAM_ERROR_NOT_AVAILABLE;
    } else {
        ret = sceKernelGetThreadExitStatus(g_cam_thids[idx]);
        if (ret < 0) {
            if (ret == (int)0x800201a2)
                ret = SCE_USBCAM_ERROR_NOT_START_READ;
            else if (ret == (int)0x800201a4)
                ret = SCE_USBCAM_ERROR_NOT_READ_DONE;
            else
                ret = SCE_USBCAM_ERROR_NOT_AVAILABLE;
        }
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCamSetupMic(SceUsbCamMicParam *param, void *workarea, int wasize)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = SCE_USBCAM_ERROR_NOT_STARTED;
    } else if ((u32)wasize < 0x108) {
        ret = SCE_USBCAM_ERROR_BUFFER_SIZE;
    } else if (!param || (s32)((((u32)param + sizeof(SceUsbCamMicParam)) | (u32)param) & pspGetK1()) < 0) {
        ret = SCE_USBCAM_ERROR_PRIV_REQUIRED;
    } else if (!workarea || (s32)(pspGetK1() & (((u32)workarea + (u32)wasize) | (u32)workarea | (u32)wasize)) < 0) {
        ret = SCE_USBCAM_ERROR_PRIV_REQUIRED;
    } else if (param->freq != 44100 && param->freq != 22050 && param->freq != 11025) {
        ret = SCE_USBCAM_ERROR_INVALID_FREQUENCY;
    } else {
        ret = sceUsb_driver_CCA3CDF7(param);
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCamReadMic(void *buf, u32 size)
{
    u32 k1 = pspShiftK1();
    SceUsbCamMicArgs micArgs;
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = SCE_USBCAM_ERROR_NOT_STARTED;
    } else if (!buf) {
        ret = SCE_USBCAM_ERROR_INVALID_VALUE;
    } else if ((s32)(pspGetK1() & (((u32)buf + size) | (u32)buf | size)) < 0) {
        ret = SCE_USBCAM_ERROR_PRIV_REQUIRED;
    } else if (sceUsb_driver_B3E9ABCE() < 0) {
        ret = SCE_USBCAM_ERROR_NOT_AVAILABLE;
    } else {
        micArgs.buf = buf;
        micArgs.size = size;
        ret = sceKernelStartThread(g_mic_thid, sizeof(micArgs), &micArgs);
        if (ret < 0)
            ret = SCE_USBCAM_ERROR_INPUT_BUSY;
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_3DC0088E(void *buf, u32 size) __attribute__((alias("sceUsbCamReadMic")));

int sceUsbCamReadMicBlocking(void *buf, u32 size)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = SCE_USBCAM_ERROR_NOT_STARTED;
    } else if (!buf) {
        ret = SCE_USBCAM_ERROR_INVALID_VALUE;
    } else if ((s32)(pspGetK1() & (((u32)buf + size) | (u32)buf | size)) < 0) {
        ret = SCE_USBCAM_ERROR_PRIV_REQUIRED;
    } else {
        ret = sceUsb_driver_32627D8F(buf, size);
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_36636925(void *buf, u32 size) __attribute__((alias("sceUsbCamReadMicBlocking")));

int sceUsbCamWaitReadMicEnd(void)
{
    u32 k1 = pspShiftK1();
    u32 timeout = 0x2dc6c0;
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = SCE_USBCAM_ERROR_NOT_STARTED;
    } else if (sceUsb_driver_B3E9ABCE() < 0) {
        ret = SCE_USBCAM_ERROR_NOT_AVAILABLE;
    } else {
        ret = sceKernelWaitThreadEnd(g_mic_thid, &timeout);
        if (ret < 0) {
            switch (ret) {
            case (int)0x800201a2:
            case (int)0x800201a7:
            case (int)0x800201ac:
                ret = SCE_USBCAM_ERROR_NOT_START_READ;
                break;
            case (int)0x800201a8:
                ret = SCE_USBCAM_ERROR_NOT_AVAILABLE;
                break;
            default:
                break;
            }
        }
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_B048A67D(void) __attribute__((alias("sceUsbCamWaitReadMicEnd")));

int sceUsbCamPollReadMicEnd(void)
{
    u32 k1 = pspShiftK1();
    int ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = SCE_USBCAM_ERROR_NOT_STARTED;
    } else if (sceUsb_driver_B3E9ABCE() < 0) {
        ret = SCE_USBCAM_ERROR_NOT_AVAILABLE;
    } else {
        ret = sceKernelGetThreadExitStatus(g_mic_thid);
        if (ret < 0) {
            if (ret == (int)0x800201a2)
                ret = SCE_USBCAM_ERROR_NOT_START_READ;
            else if (ret == (int)0x800201a4)
                ret = SCE_USBCAM_ERROR_NOT_READ_DONE;
        }
    }

    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_F8847F60(void) __attribute__((alias("sceUsbCamPollReadMicEnd")));

int sceUsbCam_2E930264(void)
{
    return 0;
}

int sceUsbCam_driver_2E930264(void) __attribute__((alias("sceUsbCam_2E930264")));

int sceUsbCamStartMic(void)
{
    u32 k1 = pspShiftK1();
    int ret = (sceUsb_driver_D9EE03ED() != 0) ? sceUsb_driver_1159202A() : SCE_USBCAM_ERROR_NOT_STARTED;
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_82A64030(void) __attribute__((alias("sceUsbCamStartMic")));

int sceUsbCamStopMic(void)
{
    u32 k1 = pspShiftK1();
    int ret = sceUsb_driver_469C73EB();
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_5145868A(void) __attribute__((alias("sceUsbCamStopMic")));

int sceUsbCamGetMicDataLength(void)
{
    u32 k1 = pspShiftK1();
    int ret = (sceUsb_driver_D9EE03ED() != 0) ? sceUsb_driver_96A32FBC() : SCE_USBCAM_ERROR_NOT_STARTED;
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_5778B452(void) __attribute__((alias("sceUsbCamGetMicDataLength")));

int sceUsbCamSetMicGain(int gain)
{
    u32 k1 = pspShiftK1();
    int ret = (sceUsb_driver_D9EE03ED() != 0) ? sceUsb_driver_D27EDB31(gain) : SCE_USBCAM_ERROR_NOT_STARTED;
    pspSetK1(k1);
    return ret;
}

int sceUsbCam_driver_08AEE98A(int gain) __attribute__((alias("sceUsbCamSetMicGain")));

static int usbCamInitMic(void)
{
    if (sceUsb_driver_C99E1766(2, NULL) < 0)
        return 1;
    int thid = sceKernelCreateThread("SceUsbCamMic", usbCamMicThread, 0x10, 0xf000, 0x00100001, NULL);
    g_mic_thid = thid;
    return (thid < 0) ? 1 : 0;
}

static int usbCamTermMic(void)
{
    if (sceUsb_driver_6C68EE25(2, NULL) < 0)
        return 1;
    sceKernelTerminateDeleteThread(g_mic_thid);
    return 0;
}

static int usbCamMicThread(u32 args, void *argp)
{
    if (args != sizeof(SceUsbCamMicArgs))
        return 0x80000104;
    SceUsbCamMicArgs *micArgs = (SceUsbCamMicArgs *)argp;
    return sceUsb_driver_32627D8F(micArgs->buf, micArgs->size);
}
