/*
 * ePSP 6.61 sceUSB_Acc_Driver (usbacc.prx)
 */

#include <kermit.h>

extern s32 sceUsb_driver_C99E1766(s32 a0, s32 a1);
extern s32 sceUsb_driver_6C68EE25(s32 a0, s32 a1);

s32 module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    return (sceUsb_driver_C99E1766(0, 0) < 0) ? 1 : 0;
}

s32 module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    return (sceUsb_driver_6C68EE25(0, 0) < 0) ? 1 : 0;
}

s32 sceUsbAccGetAuthStat(void)
{
    return 0;
}

s32 sceUsbAccGetInfo(void *info)
{
    (void)info;
    return 0;
}
