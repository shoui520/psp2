#include <common_imp.h>
#include <sysmem_kernel.h>
#include <threadman_kernel.h>

extern s32 sceUsb_driver_D9EE03ED(void);
extern s32 sceUsb_driver_C99E1766(s32 id, void *arg);
extern s32 sceUsb_driver_6C68EE25(s32 id, void *arg);

s32 sceUsbMic_45310F07(void *param)
{
    s32 oldK1 = pspShiftK1();
    s32 ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
    } else if (param != NULL && pspK1StaBufOk(param, 0x18)) {
        sceKernelDelayThread(1000);
        ret = 0;
    } else {
        ret = 0x80243804;
    }

    pspSetK1(oldK1);
    return ret;
}

s32 sceUsbMic_2E6DCDCD(s32 maxSamples, s32 frequency, void *buf)
{
    s32 oldK1 = pspShiftK1();
    s32 ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
    } else if (!pspK1DynBufOk(buf, maxSamples * 2)) {
        ret = 0x80243804;
    } else if ((maxSamples % 64) != 0) {
        ret = 0x80243806;
    } else if (frequency != 22050 && frequency != 11025 && frequency != 44100) {
        ret = 0x8024380A;
    } else {
        if (buf != NULL) {
            sceKernelDelayThread(1000);
            sceKernelMemset(buf, 0, maxSamples);
        }
        ret = 0;
    }

    pspSetK1(oldK1);
    return ret;
}

s32 module_start(SceSize args, void *argp)
{
    if (sceUsb_driver_C99E1766(3, NULL) < 0)
        return 1;
    sceKernelDelayThread(1000);
    return 0;
}

s32 module_stop(SceSize args, void *argp)
{
    if (sceUsb_driver_6C68EE25(3, NULL) < 0)
        return 1;
    sceKernelDelayThread(1000);
    return 0;
}

s32 sceUsbMic_B8E536EB(void)
{
    s32 oldK1 = pspShiftK1();
    sceKernelDelayThread(1000);
    pspSetK1(oldK1);
    return 0;
}

s32 sceUsbMic_5F7F368D(void)
{
    s32 oldK1 = pspShiftK1();
    sceKernelDelayThread(1000);
    pspSetK1(oldK1);
    return 0;
}

s32 sceUsbMic_63400E20(void)
{
    s32 oldK1 = pspShiftK1();
    sceKernelDelayThread(1000);
    pspSetK1(oldK1);
    return 0;
}

s32 sceUsbMic_F899001C(void)
{
    s32 oldK1 = pspShiftK1();
    pspSetK1(oldK1);
    return 0;
}

s32 sceUsbMic_06128E42(void)
{
    s32 oldK1 = pspShiftK1();
    pspSetK1(oldK1);
    return 0;
}
