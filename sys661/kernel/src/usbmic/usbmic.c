#include <common_imp.h>
#include <sysmem_kernel.h>
#include <threadman_kernel.h>

extern s32 sceUsb_driver_D9EE03ED(void);
extern s32 sceUsb_driver_C99E1766(s32 id, void *arg);
extern s32 sceUsb_driver_6C68EE25(s32 id, void *arg);

s32 sceUsbMicInputInitEx(void *param)
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

s32 sceUsbMicInputBlocking(s32 maxSamples, s32 frequency, void *buf)
{
    s32 oldK1 = pspShiftK1();
    s32 ret;

    if (sceUsb_driver_D9EE03ED() == 0) {
        ret = 0x80243908;
    } else if (!pspK1DynBufOk(buf, maxSamples * 2)) {
        ret = 0x80243804;
    } else if ((maxSamples % 64) != 0) {
        ret = 0x80243806;
    } else {
        switch (frequency) {
        case 11025:
        case 22050:
        case 44100:
            if (buf != NULL) {
                sceKernelDelayThread(1000);
                sceKernelMemset(buf, 0, maxSamples);
            }
            ret = 0;
            break;
        default:
            ret = 0x8024380A;
            break;
        }
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

s32 sceUsbMicInputInit(void)
{
    s32 oldK1 = pspShiftK1();
    sceKernelDelayThread(1000);
    pspSetK1(oldK1);
    return 0;
}

s32 sceUsbMicInput(void)
{
    s32 oldK1 = pspShiftK1();
    sceKernelDelayThread(1000);
    pspSetK1(oldK1);
    return 0;
}

s32 sceUsbMicGetInputLength(void)
{
    s32 oldK1 = pspShiftK1();
    sceKernelDelayThread(1000);
    pspSetK1(oldK1);
    return 0;
}

s32 sceUsbMicWaitInputEnd(void)
{
    s32 oldK1 = pspShiftK1();
    pspSetK1(oldK1);
    return 0;
}

s32 sceUsbMicPollInputEnd(void)
{
    s32 oldK1 = pspShiftK1();
    pspSetK1(oldK1);
    return 0;
}
