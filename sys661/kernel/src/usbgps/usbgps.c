#include <common_imp.h>
#include <sysmem_sysevent.h>

SCE_MODULE_INFO("sceUSBGps_Driver", SCE_MODULE_KERNEL | SCE_MODULE_ATTR_CANT_STOP | SCE_MODULE_ATTR_EXCLUSIVE_LOAD, 1, 7);
SCE_SDK_VERSION(0x06060110);

extern s32 sceKermit_driver_5280B410(s32 cmd, void *cb);
extern s32 sceUsb_driver_55494514(void);
extern s32 sceUsb_driver_6C68EE25(s32 a0, s32 a1);
extern s32 sceUsb_driver_79545577(void);
extern s32 sceUsb_driver_7C5791D3(void);
extern s32 sceUsb_driver_8A2D38D7(void);
extern s32 sceUsb_driver_9009AD6B(void);
extern s32 sceUsb_driver_AB8ADCC8(void *a0, void *a1);
extern s32 sceUsb_driver_C99E1766(s32 a0, s32 a1);
extern s32 sceUsb_driver_CB30E794(void);
extern s32 sceUsb_driver_D9EE03ED(void);

const char g_libGpsStr[] = "Lib-PSP usbgps      ....";

static s32 __attribute__((noipa)) usbgps_sysevent_handler(s32 ev_id, char *ev_name, void *param, s32 *result);
static void __attribute__((noipa)) usbgps_kermit_cb(void);

static SceSysEventHandler g_sysEventHandler = {
    .size = sizeof(SceSysEventHandler),
    .name = "SceUsbGps",
    .typeMask = SCE_RESUME_EVENTS,
    .handler = usbgps_sysevent_handler,
};

static s32 g_state;
static u8 g_initFlag;
static s32 g_usbState;

s32 module_start(SceSize args, void *argp)
{
    g_initFlag = 0;
    g_usbState = 0;
    if ((sceUsb_driver_9009AD6B() & 0xF) == 2)
        sceUsb_driver_55494514();
    g_state = 0;
    if (sceUsb_driver_C99E1766(4, 0) < 0)
        return 1;
    if (sceKernelRegisterSysEventHandler(&g_sysEventHandler) != 0)
        return 1;
    return sceKermit_driver_5280B410(12, usbgps_kermit_cb) != 0;
}

s32 sceUsbGps_driver_9F267D34(void)
{
    s32 k1 = pspShiftK1();
    s32 res;
    if (sceUsb_driver_D9EE03ED() == 0) {
        res = 0x80243908;
    } else {
        res = sceUsb_driver_8A2D38D7();
        if (res == 0)
            g_state = 1;
    }
    pspSetK1(k1);
    return res;
}
s32 sceUsbGps_9F267D34(void) __attribute__((alias("sceUsbGps_driver_9F267D34")));

s32 sceUsbGps_driver_A8ED0BC2(u8 flag)
{
    s32 k1 = pspShiftK1();
    s32 res = 0;
    if (sceUsb_driver_D9EE03ED() == 0) {
        res = 0x80243908;
    } else if (g_state == 0) {
        res = 0x80244002;
    } else {
        g_initFlag = flag;
    }
    pspSetK1(k1);
    return res;
}
s32 sceUsbGps_A8ED0BC2(u8 flag) __attribute__((alias("sceUsbGps_driver_A8ED0BC2")));

s32 sceUsbGps_driver_5881C826(u8 *out_flag)
{
    s32 k1 = pspShiftK1();
    s32 res = 0;
    if (sceUsb_driver_D9EE03ED() == 0) {
        res = 0x80243908;
    } else if (g_state == 0) {
        res = 0x80244002;
    } else if (out_flag == NULL) {
        res = 0x80244007;
    } else if (!pspK1StaBufOk(out_flag, 1)) {
        res = 0x80244006;
    } else {
        *out_flag = g_initFlag;
    }
    pspSetK1(k1);
    return res;
}
s32 sceUsbGps_5881C826(u8 *out_flag) __attribute__((alias("sceUsbGps_driver_5881C826")));

s32 sceUsbGps_driver_7C16AC3A(s32 *out_state)
{
    s32 k1 = pspShiftK1();
    s32 res = 0;
    if (out_state == NULL) {
        res = 0x80244007;
    } else if (!pspK1StaBufOk(out_state, 4)) {
        res = 0x80244006;
    } else if (g_state == 0) {
        res = 0x80244002;
    } else if ((sceUsb_driver_9009AD6B() & 0xF00) != 0x200) {
        *out_state = 0;
        g_state = 1;
        if ((sceUsb_driver_9009AD6B() & 0xF) == 2)
            sceUsb_driver_55494514();
    } else {
        s32 st = g_state;
        if (st == 2) {
            *out_state = 0;
            g_state = 3;
        } else if (st < 3) {
            if (st != 1) {
                res = 0x80244010;
            } else {
                *out_state = 0;
                s32 usb_st = g_usbState;
                if (usb_st == 0) {
                    usb_st = sceUsb_driver_CB30E794();
                    g_usbState = usb_st;
                }
                if (usb_st == st) {
                    g_state = 2;
                    sceUsb_driver_7C5791D3();
                }
            }
        } else if (st == 3) {
            *out_state = 2;
            g_state = 4;
        } else if (st == 4) {
            s32 usb_st = g_usbState;
            if (usb_st == 0) {
                usb_st = sceUsb_driver_CB30E794();
                g_usbState = usb_st;
            }
            if (usb_st == 2) {
                g_state = 1;
                sceUsb_driver_55494514();
            }
            *out_state = 3;
        } else {
            res = 0x80244010;
        }
    }
    pspSetK1(k1);
    return res;
}
s32 sceUsbGps_7C16AC3A(s32 *out_state) __attribute__((alias("sceUsbGps_driver_7C16AC3A")));

s32 sceUsbGps_driver_934EC2B2(void *gps_data, void *sat_data)
{
    s32 k1 = pspShiftK1();
    s32 res;
    if (sceUsb_driver_D9EE03ED() == 0) {
        res = 0x80243908;
        goto out;
    }
    if ((sceUsb_driver_9009AD6B() & 0xF00) != 0x200) {
        if (g_state != 0)
            g_state = 1;
        if ((sceUsb_driver_9009AD6B() & 0xF) == 2)
            sceUsb_driver_55494514();
    }
    s32 st = g_state;
    if (st == 4) {
        s32 usb_st = g_usbState;
        if (usb_st == 0) {
            usb_st = sceUsb_driver_CB30E794();
            g_usbState = usb_st;
        }
        if (usb_st != 1) {
            g_state = 1;
            sceUsb_driver_55494514();
            res = 0x80244004;
        } else if ((gps_data != NULL && !pspK1StaBufOk(gps_data, 0x30)) ||
                   (sat_data != NULL && !pspK1StaBufOk(sat_data, 0x104))) {
            res = 0x80244006;
        } else {
            res = sceUsb_driver_AB8ADCC8(gps_data, sat_data);
        }
    } else if (st == 1) {
        res = 0x80244004;
    } else if (st < 2) {
        res = (st == 0) ? 0x80244002 : 0x80244010;
    } else if (st == 2) {
        res = 0x80243007;
    } else if (st == 3) {
        res = 0x80244005;
    } else {
        res = 0x80244010;
    }
out:
    pspSetK1(k1);
    return res;
}
s32 sceUsbGps_934EC2B2(void *gps_data, void *sat_data) __attribute__((alias("sceUsbGps_driver_934EC2B2")));

s32 module_stop(SceSize args, void *argp)
{
    sceKermit_driver_5280B410(12, NULL);
    if (sceUsb_driver_6C68EE25(4, 0) < 0)
        return 1;
    sceKernelUnregisterSysEventHandler(&g_sysEventHandler);
    return 0;
}

s32 sceUsbGps_driver_6EED4811(void)
{
    s32 k1 = pspShiftK1();
    s32 res = sceUsb_driver_79545577();
    if ((sceUsb_driver_9009AD6B() & 0xF) == 2)
        sceUsb_driver_55494514();
    pspSetK1(k1);
    g_state = 0;
    return res;
}
s32 sceUsbGps_6EED4811(void) __attribute__((alias("sceUsbGps_driver_6EED4811")));

s32 sceUsbGps_63D1F89D(void)
{
    return 0;
}

s32 sceUsbGps_driver_9D8F99E8(s32 a0, s32 a1)
{
    s32 k1 = pspShiftK1();
    s32 ok = sceUsb_driver_D9EE03ED();
    pspSetK1(k1);
    return (ok != 0) ? 0 : 0x80243908;
}
s32 sceUsbGps_9D8F99E8(s32 a0, s32 a1) __attribute__((alias("sceUsbGps_driver_9D8F99E8")));

s32 sceUsbGps_driver_31F95CDE(void)
{
    s32 k1 = pspShiftK1();
    s32 ok = sceUsb_driver_D9EE03ED();
    pspSetK1(k1);
    return (ok != 0) ? 0 : 0x80243908;
}
s32 sceUsbGps_31F95CDE(void) __attribute__((alias("sceUsbGps_driver_31F95CDE")));

s32 sceUsbGps_driver_A259CD67(void)
{
    return 0;
}
s32 sceUsbGps_A259CD67(void) __attribute__((alias("sceUsbGps_driver_A259CD67")));

s32 sceUsbGps_driver_268F95CA(void)
{
    return 0;
}
s32 sceUsbGps_268F95CA(void) __attribute__((alias("sceUsbGps_driver_268F95CA")));

s32 sceUsbGps_driver_54D26AA4(void)
{
    return 0;
}
s32 sceUsbGps_54D26AA4(void) __attribute__((alias("sceUsbGps_driver_54D26AA4")));

s32 sceUsbGps_driver_69E4AAA8(void)
{
    return 0;
}
s32 sceUsbGps_69E4AAA8(void) __attribute__((alias("sceUsbGps_driver_69E4AAA8")));

static s32 __attribute__((noipa)) usbgps_sysevent_handler(s32 ev_id, char *ev_name, void *param, s32 *result)
{
    if (ev_id == 0x00400000)
        g_usbState = sceUsb_driver_CB30E794();
    return 0;
}

static void __attribute__((noipa)) usbgps_kermit_cb(void)
{
    g_usbState = 0;
}
