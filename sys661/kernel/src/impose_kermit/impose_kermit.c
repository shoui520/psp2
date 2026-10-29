/*
 * ePSP 6.61 sceImpose_Driver (impose_kermit.prx)
 *
 * Reimplemented from the decrypted 6.61 flash0:/kd/impose_kermit.prx binary.
 */

#include <kermit.h>

/* =========================================================================
 * External Imports
 * ========================================================================= */

/* InitForKernel */
extern s32 sceKernelBootFrom(void);
extern s32 sceKernelApplicationType(void);
extern s32 sceKernelSetInitCallback(s32 (*cb)(void *, s32, void *), s32 flag, void *arg);
extern void *sceKernelInitDiscImage(void);

/* LoadExecForKernel */
extern s32 sceKernelInvokeExitCallback(void);
extern s32 sceKernelCheckExitCallback(void);

/* IoFileMgrForKernel */
extern s32 sceIoOpen(const char *file, s32 flags, s32 mode);
extern s32 sceIoClose(s32 fd);
extern s32 sceIoRead(s32 fd, void *data, u32 size);
extern s64 sceIoLseek(s32 fd, s64 offset, s32 whence);
extern s32 sceIoDevctl(const char *dev, u32 cmd, void *indata, s32 inlen, void *outdata, s32 outlen);

/* SystemCtrlForKernel */
extern s32 sceKernelIsDVDMode(void);

/* SysMemForKernel */
extern s32 sceKernelGetModel(void);
extern void *sceKernelMemset32(void *dst, s32 val, u32 size);
extern s32 SysMemForKernel_2A8B8B2D(void *arg);
extern u32 sceKernelPartitionTotalMemSize(s32 pid);

/* ThreadManForKernel */
extern s32 sceKernelCreateThread(const char *name, s32 (*entry)(u32, void *), s32 prio, u32 stack, u32 attr, void *opt);
extern s32 sceKernelDeleteThread(s32 thid);
extern s32 sceKernelStartThread(s32 thid, u32 arglen, void *argp);
extern s32 sceKernelExitThread(s32 status);
extern s32 sceKernelWaitThreadEnd(s32 thid, u32 *timeout);
extern u32 sceKernelGetSystemTimeLow(void);
extern s32 sceKernelCreateFpl(const char *name, s32 pid, u32 attr, u32 size, u32 blocks, void *opt);
extern s32 sceKernelDeleteFpl(s32 fplid);
extern s32 sceKernelTryAllocateFpl(s32 fplid, void **data);
extern s32 sceKernelFreeFpl(s32 fplid, void *data);

/* sceDisplay_driver */
extern s32 sceDisplayEnable(void);
extern s32 sceDisplayDisable(void);
extern s32 sceDisplaySetHoldMode(s32 mode);
extern s32 sceDisplaySetMode(s32 mode, s32 width, s32 height);
extern s32 sceDisplayGetMode(s32 *pmode, s32 *pwidth, s32 *pheight);
extern s32 sceDisplayGetFrameBuf(void **topaddr, s32 *bufferwidth, s32 *pixelformat, s32 sync);
extern s32 sceDisplay_driver_3E17FE8D(s32 idx, void *topaddr, s32 width, s32 psm, s32 sync);
extern s32 sceDisplay_driver_40F1469C(s32 mode);
extern s32 sceDisplay_driver_7DF044BA(s32 a0);
extern s32 sceDisplay_driver_996881D2(void);
extern s32 sceDisplay_driver_9B18DDDD(s32 a0);
extern s32 sceDisplay_driver_C2B0FBEE(s32 a0);
extern s32 sceDisplay_driver_D94EB277(s32 a0);
extern s32 sceDisplay_driver_F455917F(s32 a0);
extern s32 sceDisplaySetBacklightSel(s32 sel);
extern s32 sceDisplayGetBacklightSel(s32 *sel);

/* sceCtrl_driver */
typedef struct SceCtrlData {
    u32 TimeStamp;
    u32 Buttons;
    u8 Lx;
    u8 Ly;
    u8 Rsrv[6];
} SceCtrlData;

extern s32 sceCtrlPeekBufferPositive(SceCtrlData *pad_data, s32 count);
extern s32 sceCtrlGetButtonIntercept(u32 buttons);
extern s32 sceCtrlSetButtonIntercept(u32 buttons);
extern s32 sceCtrlSetSpecialButtonCallback(u32 idx, u32 mask, void (*cb)(u32, u32, u32), u32 arg);

/* scePower_driver */
extern s32 scePower_driver_31AEA94C(void);
extern s32 scePower_driver_7A9EA6DE(void);
extern s32 scePower_driver_872F4ECE(void);
extern s32 scePower_driver_8C873AA7(void);
extern s32 scePower_driver_FA651CE1(void);

/* sceAudio_driver */
extern s32 sceAudio_driver_5182B550(u32 mask);

/* sceHprm_driver */
extern s32 sceHprm_driver_0B83352B(s32 a0, s32 *out, s32 a2, s32 a3);
extern s32 sceHprmPeekCurrentKey(u32 *key);
extern s32 sceHprm_driver_1C3525DD(void);
extern s32 sceHprm_driver_370DDC12(void);
extern s32 sceHprm_driver_9BDE174B(void);
extern s32 sceHprm_driver_AD158331(void);
extern s32 sceHprm_driver_EFCFD0C5(void);
extern s32 sceHprm_driver_FA4A25A7(void);

/* sceKermitPeripheral_driver */
extern s32 sceKermitPeripheral_driver_76BE2E28(s32 a0, s32 a1);

/* sceLcdc_driver */
extern s32 sceLcdc_driver_451FE1A1(s32 a0);

/* sceRtc_driver */
extern s32 sceRtc_driver_9012B140(void *a0);
extern s32 sceRtc_driver_DFF30673(s32 a0, s32 a1, s32 a2, s32 a3);

/* sceReg_driver */
typedef struct RegParam {
    u32 regtype;
    char name[256];
    u32 namelen;
    u32 unk2;
    u32 unk3;
} RegParam;

extern s32 sceRegOpenRegistry(RegParam *reg, s32 mode, s32 *h);
extern s32 sceRegCloseRegistry(s32 h);
extern s32 sceRegFlushRegistry(s32 h);
extern s32 sceRegOpenCategory(s32 h, const char *name, s32 mode, s32 *hd);
extern s32 sceRegCloseCategory(s32 hd);
extern s32 sceRegFlushCategory(s32 hd);
extern s32 sceRegGetKeyInfo(s32 hd, const char *name, s32 *hk, u32 *type, u32 *size);
extern s32 sceRegGetKeyValue(s32 hd, s32 hk, void *buf, u32 size);
extern s32 sceRegSetKeyValue(s32 hd, const char *name, const void *buf, u32 size);

/* SysclibForKernel */
extern void *memcpy(void *dst, const void *src, u32 len);
extern void *memset(void *dst, s32 c, u32 len);
extern char *strncpy(char *dst, const char *src, u32 n);

/* =========================================================================
 * Types & Globals
 * ========================================================================= */

typedef struct SceImposeTvOutMode {
    s32 mode0;
    s32 mode1;
    s32 width;
    s32 height;
} SceImposeTvOutMode;

static const s32 g_battery_thresholds[3] = { 60, 30, 12 };

static const SceImposeTvOutMode g_tvout_modes_ntsc[3] = {
    { 0x00, 0x2D2, 480, 272 },
    { 0x00, 0x3D2, 480, 272 },
    { 0xE0, 0x2E2, 480, 272 },
};

static const SceImposeTvOutMode g_tvout_modes_pal[2] = {
    { 0x00, 0x6D1, 480, 272 },
    { 0x00, 0x7D1, 480, 272 },
};

static const s32 g_brightness_levels[5] = { 24, 40, 72, 100, 0 };

static u8 g_impose_state[0x36C] __attribute__((aligned(16)));
static void *g_impose = g_impose_state;
static const SceImposeTvOutMode *g_tvout_mode = NULL;

#define IMP(off)     (*(s32 *)((u8 *)g_impose + (off)))
#define IMPU(off)    (*(u32 *)((u8 *)g_impose + (off)))
#define IMP_PTR(off) ((void *)((u8 *)g_impose + (off)))

/* Forward declarations */
static void sub_0000(s32 a0, s32 a1);
static s32 sub_009c(u32 bits);
static s32 sub_00c4(s32 val);
static s32 sub_0110(u32 idx);
static s32 sub_0138(s32 val);
static s32 sub_0190(s32 *out0, s32 *out1);
static void sub_0210(s32 mode);
static void sub_0384(u32 p0, u32 p1, u32 p2);
static s32 sub_0520(void);
static void sub_055c(s32 val);
static void sub_056c(s32 val);
static void sub_0590(u32 mask);
static void sub_07f8(void);
static void sub_0898(void);
static s32 sub_160c(s32 a0);
static s32 sub_169c(void);
static void sub_16d8(void);
static s32 sub_173c(s32 flag, s32 mode, s32 w, s32 h);
static void sub_187c(void);
static void sub_191c(s32 idx);
static int impose_sysevent_handler(int ev_id, char *ev_name, void *param, int *result);
static s32 sub_27f8(void);
static s32 sub_2878(void);
static s32 sub_293c(void);
static s32 sub_29bc(void);
static void sub_2ae0(s32 mode);
static void sub_2c00(void);
static void sub_317c(u32 cur_buttons, u32 last_buttons, u32 arg);
static void sub_3308(u32 cur_buttons, u32 last_buttons, u32 arg);
static void sub_3380(s32 a0, s32 a1);
static void sub_3388(s32 subintr_no, void *arg);
static void sub_38cc(void);
static s32 sub_3968(u32 args, void *argp);
static s32 sub_3e0c(u32 id);
static void sub_3f54(void);
static void sub_4334(void);
static void sub_441c(void);
static void sub_452c(void);
static s32 sub_4558(void);
static s32 sub_4644(s32 lang);
static s32 sub_48b0(void);
static void sub_49f4(u32 a0, u32 a1, u32 a2, s32 a3);
static void sub_4ba0(s32 mode);
static void sub_4ff0(void);
static void sub_520c(void);
static void sub_5228(void);
static void sub_5360(void);
static void sub_53c4(void);
static void sub_54f0(s32 x, s32 y, s32 w, s32 h, const void *src);
static void sub_5588(s32 x, s32 y, s32 w, s32 h, const void *src);
static void sub_5638(s32 y);
static s32 sub_5830(void);
static s32 sub_5ddc(void);
static s32 sub_6008(void);
static s32 sub_648c(const char *category, const char *key, s32 val);
static void sub_658c(s32 set, u32 mask);
static s32 sub_660c(void *arg0, s32 arg1, void *arg2);
static s32 sub_6638(s32 mode);
static s32 sub_66d8(s32 mode);
static s32 sub_6788(void);
static s32 sub_6870(void);
static void sub_6908(u32 trig, u32 cur_buttons, u32 prev_buttons);
static void sub_7018(void);
static s32 sub_70e0(s32 a0, s32 a1);
static void sub_7290(s32 a0, s32 a1);
static void sub_7388(s32 a0);
static s32 sub_73d4(u32 id);
static s32 sub_75fc(const void *time_ptr, s32 x, s32 y, s32 align);
static void sub_7e74(void);
static s32 sub_800c(const char *category, const char *key, s32 *out);
static void sub_8138(void);

s32 sceImpose_driver_BB12F974(s32 a0);
s32 sceImpose_driver_0F067E16(void);
s32 sceImpose_driver_E9B0E008(s32 *out);
s32 sceImpose_driver_D067235E(void);
s32 sceImpose_driver_AABEE36A(void);
s32 sceImpose_133636DD(s32 id);
s32 sceImpose_FCD44963(s32 id, s32 val);

static SceSysEventHandler g_sysevent_handler = {
    sizeof(SceSysEventHandler),
    (char *)"SceImpose",
    0x00FFFF00,
    impose_sysevent_handler,
    0,
    0,
    NULL,
    { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

/* =========================================================================
 * Internal Helper Implementations
 * ========================================================================= */

static void sub_0000(s32 a0, s32 a1)
{
    if (a0 != 0) {
        if ((IMPU(0xE8) & 1) == 0) {
            IMPU(0xE8) |= 1;
            sub_009c(0x1000);
        }
        if (a1 != 0) {
            sub_2ae0(0);
        } else {
            sub_2c00();
        }
    } else {
        if ((IMPU(0xE8) & 1) != 0) {
            IMPU(0xE8) &= ~1u;
            sub_2c00();
            sub_009c(0x2000);
        }
    }
    IMPU(0xBC) |= 0x20000000u;
}

static s32 sub_009c(u32 bits)
{
    return sceKernelSetEventFlag(IMP(0x08), bits);
}

static s32 sub_00c4(s32 val)
{
    if ((u32)(val - 73) < 28u) {
        return 3;
    }
    if ((u32)(val - 41) < 32u) {
        return 2;
    }
    if ((u32)(val - 25) < 16u) {
        return 1;
    }
    if ((u32)(val - 1) < 24u) {
        return 0;
    }
    return 4;
}

static s32 sub_0110(u32 idx)
{
    if (idx < 5u) {
        return g_brightness_levels[idx];
    }
    return -1;
}

static s32 sub_0138(s32 val)
{
    s32 sel;

    if ((u32)(val - 73) < 28u) {
        sel = 4;
    } else if ((u32)(val - 41) < 32u) {
        sel = 3;
    } else if ((u32)(val - 25) < 16u) {
        sel = 2;
    } else if ((u32)(val - 1) < 24u) {
        sel = 1;
    } else {
        sel = 0;
    }
    sceDisplaySetBacklightSel(sel);
    return 0;
}

static s32 sub_0190(s32 *out0, s32 *out1)
{
    s32 sel = 0;
    s32 val;

    sceDisplayGetBacklightSel(&sel);
    if (sel == 1) {
        val = 24;
    } else if (sel == 2) {
        val = 40;
    } else if (sel == 3) {
        val = 72;
    } else if (sel == 4) {
        val = 100;
    } else {
        val = 0;
    }
    if (out0 != NULL) {
        *out0 = val;
    }
    if (out1 != NULL) {
        *out1 = sel;
    }
    return 0;
}

static void sub_0210(s32 mode)
{
    if (mode == 1) {
        s32 idx = sub_00c4(IMP(0x3C));
        if (idx == 4) {
            idx = sub_00c4(IMP(0xD8));
        } else {
            s32 max_idx = (IMP(0xFC) != 0) ? 3 : 2;
            idx++;
            if (idx > max_idx) {
                idx = 0;
            }
        }
        IMP(0xB4) = sub_0110((u32)idx);
        IMP(0xD4) = IMP(0xB4);
        IMP(0x3C) = IMP(0xB4);
        IMP(0x180) = 0;
        IMPU(0xE8) &= ~4u;
    } else if (mode < 1) {
        if (mode != 0) {
            return;
        }
        if ((IMPU(0xE8) & 4) == 0) {
            IMP(0xD8) = IMP(0x3C);
            IMP(0xB4) = sub_0110(4);
            IMP(0x3C) = IMP(0xB4);
            IMPU(0xE8) |= 4u;
        } else {
            IMP(0xB4) = IMP(0xD8);
            IMP(0x3C) = IMP(0xD8);
            IMPU(0xE8) &= ~4u;
        }
    } else if (mode == 2) {
        if ((IMPU(0xE8) & 4) == 0) {
            IMP(0xD8) = IMP(0x3C);
        }
        IMP(0xB4) = sub_0110(4);
        IMP(0x3C) = IMP(0xB4);
        IMPU(0xE8) |= 4u;
    } else if (mode == 3) {
        if ((IMPU(0xE8) & 4) != 0) {
            IMP(0xB4) = IMP(0xD8);
            IMP(0x3C) = IMP(0xD8);
            IMPU(0xE8) &= ~4u;
        }
    } else {
        return;
    }

    sub_0138(IMP(0x3C));
    IMPU(0xBC) |= 2u;
}

static void sub_0384(u32 p0, u32 p1, u32 p2)
{
    u32 confirm_btn = (IMP(0x24) == 0) ? 0x2000u : 0x4000u;
    u32 cancel_btn  = (IMP(0x24) == 0) ? 0x4000u : 0x2000u;
    u32 trig = p0 & ~p1;

    (void)p2;
    if ((trig & 0x80u) != 0) {
        if (IMP(0x14C) == 1) {
            IMP(0x14C) = 0;
            IMPU(0xC8) |= 0x20u;
        }
    } else if ((trig & 0x20u) != 0) {
        if (IMP(0x14C) == 0) {
            IMP(0x14C) = 1;
            IMPU(0xC8) |= 0x20u;
        }
    } else if ((trig & cancel_btn) != 0) {
        sceImpose_driver_BB12F974(1);
    } else if ((trig & confirm_btn) != 0) {
        if (IMP(0x14C) == 0) {
            if (sceKernelCheckExitCallback() != 0) {
                IMPU(0xA4) |= 0x80000000u;
                sub_009c(0x10);
            }
            sceImpose_driver_BB12F974(1);
        } else {
            sceImpose_driver_BB12F974(1);
        }
    }
}

static s32 sub_0520(void)
{
    s32 val = 0;
    s32 res = sceHprm_driver_0B83352B(0, &val, 0, 0);
    if (res < 0) {
        return -1;
    }
    return val;
}

static void sub_055c(s32 val)
{
    IMP(0x130) = val;
}

static void sub_056c(s32 val)
{
    sub_055c(val);
    sub_009c(0x8000);
}

static void sub_0590(u32 mask)
{
    u8 *fb = (u8 *)IMPU(0x1B0);
    if (fb == NULL) {
        return;
    }

    if ((mask & 0x01u) != 0) {
        sceKernelMemset32(fb, (s32)0x9CE79CE7u, 0x11000);
    }
    if ((mask & 0x02u) != 0) {
        sceKernelMemset32(fb + 0x11000, (s32)0x9CE79CE7u, 0x20000);
    }
    if ((mask & 0x04u) != 0) {
        sceKernelMemset32(fb + 0x31000, (s32)0x9CE79CE7u, 0x8000);
    }
    if ((mask & 0x08u) != 0) {
        sceKernelMemset32(fb + 0x39000, (s32)0x9CE79CE7u, 0xA000);
    }
    if ((mask & 0x20u) != 0) {
        sceKernelMemset32(fb + 0x1D000, (s32)0x9CE79CE7u, 0x9000);
    }
}

static void sub_07f8(void)
{
    u32 a4 = IMPU(0xA4);
    s32 idx;

    if ((a4 & 0x100u) != 0) {
        idx = 0;
    } else if ((a4 & 0x200u) != 0) {
        idx = 1;
    } else if ((a4 & 0x400u) != 0) {
        idx = 2;
    } else if ((a4 & 0x800u) != 0) {
        idx = 3;
    } else {
        return;
    }

    void (*fn)(void) = (void (*)(void))IMPU(0x1B4 + idx * 4);
    if (fn != NULL) {
        fn();
    }
}

static void sub_0898(void)
{
    sub_3e0c(3);
    sub_3e0c(4);
    sub_3e0c(6);
    sub_3e0c(7);
    if (IMP(0x110) != 0) {
        sub_3e0c(16);
        sub_3e0c(17);
        sub_3e0c(20);
    }
}

static s32 sub_160c(s32 a0)
{
    u32 a4 = IMPU(0xA4);

    if ((a4 & 0xF00u) != 0) {
        if ((a4 & 0x100u) != 0) {
            return sceImpose_driver_BB12F974(1);
        }
        return -1;
    }
    if (IMP(0x134) == 0) {
        return -1;
    }
    return sceImpose_driver_BB12F974((a0 != 0) ? 0x50102 : 0x50002);
}

static s32 sub_169c(void)
{
    if (IMP(0x10C) == 0x300) {
        return 1;
    }
    if ((IMPU(0xA4) & 4u) != 0) {
        return (IMP(0x10C) != 0x400) ? 1 : 0;
    }
    return 0;
}

static void sub_16d8(void)
{
    if (IMP(0x9C) == 1 || IMP(0x9C) == 2) {
        sceDisplay_driver_D94EB277(IMP(0x328));
    }
    sceDisplaySetMode(IMP(0x32C), IMP(0x330), IMP(0x334));
}

static s32 sub_173c(s32 flag, s32 mode, s32 w, s32 h)
{
    s32 intr = sceKernelCpuSuspendIntr();

    if (g_tvout_mode == NULL) {
        sub_191c(0);
    }
    IMP(0x328) = g_tvout_mode->mode0;
    if (mode != 0) {
        IMP(0x32C) = mode;
        IMP(0x330) = w;
        IMP(0x334) = h;
    } else {
        IMP(0x32C) = g_tvout_mode->mode1;
        IMP(0x330) = g_tvout_mode->width;
        IMP(0x334) = g_tvout_mode->height;
    }
    IMP(0x31C) = 1;
    sceKernelCpuResumeIntr(intr);

    sub_0210(2);
    sub_3380(2, 0);
    sub_055c(1);
    if (flag != 0) {
        sub_009c(0x800);
    } else {
        sub_16d8();
    }
    return 0;
}

static void sub_187c(void)
{
    if (IMP(0x31C) != 0) {
        IMP(0x31C) = 0;
        IMP(0x328) = 0;
        IMP(0x32C) = 0;
        IMP(0x330) = 480;
        IMP(0x334) = 272;
        sub_16d8();
        sub_0210(3);
        sub_056c(0);
    }
}

static void sub_191c(s32 idx)
{
    if (IMP(0x6C) == 0) {
        if ((u32)idx >= 3u) {
            idx = 0;
        }
        g_tvout_mode = &g_tvout_modes_ntsc[idx];
    } else {
        if ((u32)idx >= 2u) {
            idx = 0;
        }
        g_tvout_mode = &g_tvout_modes_pal[idx];
    }
}

static int impose_sysevent_handler(int ev_id, char *ev_name, void *param, int *result)
{
    (void)ev_name;
    (void)param;
    (void)result;

    if (ev_id == 0x200) {
        if ((IMPU(0xA4) & 0xF00u) != 0) {
            sceImpose_driver_BB12F974(1);
        }
    } else if (ev_id == 0x400) {
        if (IMP(0x16C) == 0) {
            return -1;
        }
    } else if (ev_id == 0x402) {
        sceKernelDisableSubIntr(0x1E, 0x14);
        sub_0898();
    } else if (ev_id == 0x400E) {
        sub_187c();
    } else if (ev_id == 0x1000E) {
        IMP(0xEC) = scePower_driver_31AEA94C();
        IMPU(0xE8) &= ~5u;
        IMP(0xF8) = 0;
        IMP(0xFC) = 0;
        IMP(0x100) = 0;
        IMP(0x104) = 0;
        IMP(0x108) = 0;
        IMP(0x130) = 0;
        IMP(0x180) = 0;
        IMP(0x188) = 0;
        sub_0190((s32 *)IMP_PTR(0x3C), NULL);
        IMP(0xB4) = IMP(0x3C);
        IMP(0xD4) = IMP(0x3C);
        sceKernelEnableSubIntr(0x1E, 0x14);
    } else if (ev_id == 0x100000) {
        sub_009c(0x80000000u);
    }
    return 0;
}

static s32 sub_27f8(void)
{
    if (IMP(0x2B8) == 0) {
        return 0;
    }
    if (sceKernelFreeFpl(IMP(0x2B8), (void *)IMPU(0x2B4)) < 0) {
        return (s32)0x8001000C;
    }
    if (sceKernelDeleteFpl(IMP(0x2B8)) < 0) {
        return (s32)0x8001000C;
    }
    IMP(0x2B8) = 0;
    IMP(0x2B4) = 0;
    return 0;
}

static s32 sub_2878(void)
{
    if (IMP(0x310) == 0) {
        return 0;
    }
    if (IMP(0x20) != IMP(0x314)) {
        s32 sz = sub_4558();
        if (sz < 0) {
            return sz;
        }
        s32 res = sub_4644(IMP(0x314));
        if (res < 0) {
            return res;
        }
        s32 words = sz / 4;
        if (words > 0) {
            u32 *p = (u32 *)IMPU(0x14);
            while (words-- > 0) {
                *p++ = 0x9CE79CE7u;
            }
        }
        sceKernelDcacheWritebackInvalidateRange((void *)IMPU(0x14), (u32)sz);
    }
    IMP(0x310) = 0;
    IMP(0x24) = IMP(0x318);
    IMP(0x20) = IMP(0x314);
    return 0;
}

static s32 sub_293c(void)
{
    if (IMP(0x2B0) == 0) {
        return 0;
    }
    if (sceKernelFreeFpl(IMP(0x2AC), (void *)IMPU(0x2B0)) < 0) {
        return (s32)0x8001000C;
    }
    if (sceKernelDeleteFpl(IMP(0x2AC)) < 0) {
        return (s32)0x8001000C;
    }
    IMP(0x2B8) = 0;
    IMP(0x2AC) = 0;
    IMP(0x2B0) = 0;
    IMP(0x2B4) = 0;
    return 0;
}

static s32 sub_29bc(void)
{
    s32 fpl = sceKernelCreateFpl("SceImpose_IconResource", IMP(0x04), 0x4100u, 0xB400u, 1, NULL);
    if (fpl < 0) {
        return (s32)0x8001000C;
    }
    IMP(0x2B8) = 0xB400;
    IMP(0x2AC) = fpl;
    sceKernelTryAllocateFpl(fpl, (void **)IMP_PTR(0x2B0));
    IMP(0x2B4) = IMP(0x2B0);

    fpl = sceKernelCreateFpl("SceImpose_Resource", IMP(0x04), 0x4100u, 0x6800u, 1, NULL);
    if (fpl < 0) {
        return (s32)0x8001000C;
    }
    IMP(0x1D0) = 0x6800;
    IMP(0x1C4) = fpl;
    sceKernelTryAllocateFpl(fpl, (void **)IMP_PTR(0x1C8));
    IMP(0x1CC) = IMP(0x1C8);

    s32 sz = sub_4558();
    if (sz < 0) {
        return sz;
    }
    s32 res = sub_48b0();
    if (res < 0) {
        return res;
    }
    res = sub_4644(IMP(0x20));
    if (res < 0) {
        return res;
    }
    sceKernelDcacheWritebackInvalidateRange((void *)IMPU(0x14), (u32)sz);
    return 0;
}

static void sub_2ae0(s32 mode)
{
    u32 mask = (mode == 0) ? 0x002DF3F9u : 0x000DF3F9u;
    IMP(0x178) = sceCtrlGetButtonIntercept(0xFFFFFFFFu);
    sceCtrlSetButtonIntercept(mask);
    IMPU(0xA4) |= 0x40000000u;
}

static void sub_2c00(void)
{
    if ((IMPU(0xA4) & 0x40000000u) != 0) {
        sceCtrlSetButtonIntercept((u32)IMP(0x178));
        IMPU(0xA4) &= ~0x40000000u;
    }
}

static void sub_317c(u32 cur_buttons, u32 last_buttons, u32 arg)
{
    (void)arg;
    if (IMP(0xA4) == 0) {
        return;
    }
    if ((last_buttons & 0x01000000u) != 0 && (cur_buttons & 0x01000000u) == 0) {
        if ((IMPU(0x74) & 2u) == 0 && IMP(0x11C) == 0) {
            sub_160c(1);
        }
    }
    if ((cur_buttons & 0x00020000u) != 0) {
        return;
    }
    if ((cur_buttons & 0x00010000u) != 0) {
        if ((IMPU(0x88) & 1u) != 0) {
            return;
        }
        if ((IMPU(0x74) & 1u) == 0 && (IMPU(0xAC) & 0x00010000u) == 0) {
            if ((IMPU(0xA4) & 0x100u) == 0 || IMP(0xC8) >= 4) {
                sub_160c((IMPU(0xA4) & 0x100u) ? 0 : 1);
            } else {
                return;
            }
        }
    }
    if ((cur_buttons & 0x00020000u) != 0) {
        return;
    }
    if ((cur_buttons & 0x00400000u) != 0 && (IMPU(0xAC) & 0x00400000u) == 0) {
        IMP(0x174) = 0;
        IMPU(0xC4) |= 0x00400000u;
    }
    if (IMP(0x7C) != 0 || IMP(0xEC) != 0) {
        return;
    }
    if ((cur_buttons & 0x00800000u) != 0 && (IMPU(0xAC) & 0x00800000u) == 0) {
        IMP(0x178) = 0;
        IMPU(0xC4) |= 0x00800000u;
    }
}

static void sub_3308(u32 cur_buttons, u32 last_buttons, u32 arg)
{
    (void)last_buttons;
    (void)arg;

    if (IMP(0xA4) == 0 || (cur_buttons & 0x00020000u) != 0) {
        return;
    }
    if (IMP(0x7C) != 0 || IMP(0xEC) != 0) {
        return;
    }
    if ((cur_buttons & 0x00100000u) != 0) {
        IMPU(0xC4) |= 0x00100000u;
    }
    if ((cur_buttons & 0x00200000u) != 0) {
        IMP(0xCC) = 0;
        IMPU(0xC4) |= 0x00200000u;
    }
    IMP(0x170) = 0;
}

static void sub_3380(s32 a0, s32 a1)
{
    (void)a0;
    (void)a1;
}

static void sub_3388(s32 subintr_no, void *arg)
{
    SceCtrlData pad;
    u32 trig = 0;
    u32 cur_buttons = 0;
    u32 prev_buttons = 0;
    u32 sys_time;

    (void)subintr_no;
    (void)arg;

    if (IMP(0xA4) == 0) {
        return;
    }

    IMP(0x160) = (IMP(0x160) + 1) & 0x7FFFFFFF;
    IMP(0x164) = (IMP(0x164) + 1) & 0x7FFFFFFF;
    sys_time = sceKernelGetSystemTimeLow();

    if (IMP(0x88) != 0) {
        sceCtrlPeekBufferPositive(&pad, 1);
        if ((IMPU(0x88) & 1u) != 0) {
            IMP(0x8C) = ((pad.Buttons & 0x00010000u) != 0) ? 1 : 0;
        }
    }

    if (IMP(0xC4) == 0 && IMP(0xF8) == 0 && (IMPU(0xA4) & 2u) == 0) {
        IMP(0xAC) = 0;
    } else if (IMP(0xE4) == 0) {
        IMP(0x170) = (IMP(0x170) + 1) & 0x7FFFFFFF;
        IMP(0x174) = (IMP(0x174) + 1) & 0x7FFFFFFF;
        IMP(0x178) = (IMP(0x178) + 1) & 0x7FFFFFFF;
        sceCtrlPeekBufferPositive(&pad, 1);
        prev_buttons = IMPU(0xAC);
        cur_buttons = pad.Buttons;
        IMPU(0xAC) = cur_buttons;
        trig = cur_buttons & (cur_buttons ^ prev_buttons);
        sub_6908(trig, cur_buttons, prev_buttons);
    } else {
        IMP(0xAC) = 0;
    }

    if (sceHprm_driver_EFCFD0C5() != 0) {
        u32 prev_hprm = IMPU(0xB0);
        sceHprmPeekCurrentKey((u32 *)IMP_PTR(0xB0));
        u32 hprm_trig = IMPU(0xB0) & (IMPU(0xB0) ^ prev_hprm);
        if ((hprm_trig & 0x10u) != 0) {
            IMP(0x170) = 0;
            IMPU(0xC4) |= 0x00100000u;
        }
        if ((hprm_trig & 0x20u) != 0) {
            IMP(0x170) = 0;
            IMP(0xCC) = 0;
            IMPU(0xC4) |= 0x00200000u;
        }
    }

    if (IMP(0x134) == 0) {
        sub_5228();
    }
    if ((IMPU(0x114) & 0x00010000u) != 0) {
        sub_5360();
    }

    sub_53c4();

    if ((IMP(0x160) & 3) == 0 && IMP(0xE4) == 0) {
        s32 prev_60 = IMP(0x60);
        IMP(0x60) = scePower_driver_8C873AA7();
        if (IMP(0x60) != prev_60) {
            IMPU(0xB8) &= ~1u;
        }
        if (IMP(0x60) != 0) {
            if (scePower_driver_FA651CE1() != 0) {
                if (IMP(0x44) == 0) {
                    IMPU(0xB8) &= ~0x80u;
                }
                IMP(0x44) = 1;
            } else {
                s32 prev_40 = IMP(0x40);
                s32 pwr = scePower_driver_31AEA94C();
                if (pwr >= 0) {
                    IMP(0x40) = pwr;
                }
                if (IMP(0x40) != prev_40) {
                    IMP(0x44) = 0;
                    IMPU(0xB8) &= ~0x80u;
                }
            }
        }
        IMP(0x58) = scePower_driver_872F4ECE();
        s32 prev_5c = IMP(0x5C);
        IMP(0x5C) = scePower_driver_7A9EA6DE();
        if (IMP(0x5C) != prev_5c) {
            IMPU(0xB8) &= ~0x80u;
        }
    }

    if ((IMP(0x164) % 24) == 0) {
        IMP(0x15C)--;
        if (IMP(0x15C) < 0) {
            IMP(0x15C) = 3;
        }
    }

    if (IMP(0xE8) == 2) {
        if ((u32)(sys_time - IMPU(0x180)) > 0x98967Fu) {
            sub_009c(0x2000);
            IMP(0xE8) = 1;
        }
    }

    if (IMP(0xDC) != 0) {
        s32 cur_val = 0;
        s32 cur_sel = 0;
        IMP(0xDC) = 0;
        s32 target = sub_0110((u32)IMP(0xB4));
        sub_0190(&cur_val, &cur_sel);
        if (IMP(0xD4) != 0) {
            IMP(0xD8) = 1;
        } else {
            if (IMP(0xDC) < target || cur_val < target) {
                sub_0138(target);
            }
        }
    }

    if (IMP(0xEC) != 0) {
        IMP(0x16C)++;
        if (IMP(0x16C) > IMP(0xF0)) {
            sub_009c(0);
            IMP(0x16C) = 0;
            IMP(0xEC) = 0;
        }
    }

    if (IMP(0x188) != 0) {
        sub_07f8();
        if (IMP(0x160) >= 0x78 && (IMPU(0xA4) & 4u) == 0) {
            if ((IMPU(0xA4) & 0x00040000u) == 0) {
                sceImpose_driver_BB12F974(1);
            } else if ((IMPU(0xA4) & 0x00020000u) != 0) {
                IMPU(0xA4) &= ~0x00020000u;
                sub_0590(0x10);
            }
        }
        if ((IMPU(0xA4) & 0x100u) != 0) {
            sub_0384(trig, IMPU(0xAC), prev_buttons);
        }
        u32 bit17 = (IMPU(0xAC) >> 17) & 1u;
        if ((u32)IMP(0x30) != bit17) {
            IMP(0x30) = (s32)bit17;
            IMPU(0xB8) &= ~1u;
        }
    }
}

static void sub_38cc(void)
{
    if ((IMPU(0xA4) & 1u) != 0) {
        if (IMP(0x188) != 0) {
            sceDisplay_driver_3E17FE8D(0, NULL, 512, 1, 1);
            IMP(0xA8) = 0;
            IMP(0x188) = 0;
            IMP(0xB8) = 0;
            IMP(0xC8) = 0;
            IMP(0x160) = 0;
            IMP(0x168) = 0;
            IMP(0xAC) = 0;
            if (IMP(0x348) == 2) {
                IMP(0xA8) = 1;
            } else {
                IMP(0xF4) = 1;
            }
        }
        sub_2c00();
    }
}

static s32 sub_3968(u32 args, void *argp)
{
    u32 pattern = 0;

    (void)args;
    (void)argp;

    while (1) {
        if (sceKernelWaitEventFlag(IMP(0x08), 0xC000FFFFu, 0x21u, &pattern, NULL) < 0) {
            break;
        }
        s32 s3_val = (IMP(0x124) == 0x10) ? IMP(0x130) : -1;
        s32 s2_val = ((pattern & 0x40000000u) == 0) ? 1 : 0;

        if ((pattern & 0x100u) != 0) {
            s32 intr = sceKernelCpuSuspendIntr();
            sub_2ae0(1);
            sceKernelCpuResumeIntr(intr);
        }
        if ((pattern & 0x200u) != 0) {
            IMP(0x104) = 1;
            sub_2878();
            IMP(0x104) = 0;
        }
        if ((pattern & 1u) != 0) {
            sub_7018();
        }
        if ((pattern & 2u) != 0) {
            s32 intr = sceKernelCpuSuspendIntr();
            sub_38cc();
            sceKernelCpuResumeIntr(intr);
        }
        if ((pattern & 0x800u) != 0) {
            if (IMP(0x31C) == 1 && (IMPU(0x110) & 0x01000000u) != 0 && IMP(0x9C) == 0) {
                pattern |= 0x4000u;
            } else {
                sub_16d8();
            }
        }
        if ((pattern & 0x8000u) != 0) {
            if (IMP(0x130) == 1) {
                if (IMP(0x31C) == 1) {
                    sub_6870();
                }
                pattern |= 0x20u;
            } else if (IMP(0x130) == 2) {
                if (sceImpose_driver_E9B0E008(NULL) == 0 && IMP(0x31C) == 0) {
                    if (sub_6788() < 0) {
                        sub_055c(1);
                    }
                }
                pattern |= 0x20u;
            } else {
                pattern |= 0x20u;
            }
            if (IMP(0x124) == 0x10) {
                pattern |= 0x4000u;
            }
            if (IMP(0x9C) != 0) {
                s2_val = 0;
            }
        }
        if ((pattern & 0x4000u) != 0) {
            if (sub_70e0(s3_val, s2_val) != 0) {
                pattern |= 4u;
            }
            if (IMP(0x124) == 0x10) {
                pattern |= 0x20u;
            }
        }
        if ((pattern & 0x20u) != 0) {
            sub_7290(s3_val, s2_val);
        }
        if ((pattern & 0x44u) != 0) {
            sub_7388(s3_val);
            if ((pattern & 4u) != 0) {
                IMPU(0xB8) &= ~6u;
                if (IMP(0xC0) == 0) {
                    IMPU(0xBC) |= 1u;
                }
            }
            if ((pattern & 0x40u) != 0) {
                IMPU(0xB8) &= ~1u;
                if (IMP(0xC0) == 0 && (IMPU(0x88) & 0x10u) == 0) {
                    IMPU(0xBC) |= 4u;
                }
            }
        }
        if ((pattern & 0x400u) != 0) {
            if (IMP(0x7C) == 0 || IMP(0x7C) == 1) {
                sceKermitPeripheral_driver_76BE2E28(-1, -1);
            }
        }
        if ((pattern & 0x1000u) != 0) {
            sceDisplayEnable();
        }
        if ((pattern & 0x2000u) != 0) {
            sceDisplayDisable();
        }
        if ((pattern & 8u) != 0) {
            sub_0898();
            IMP(0xE0) = 0;
        }
        if ((pattern & 0x80u) != 0) {
            if (IMP(0x14) != 0) {
                if (sceKernelFreeFpl(IMP(0x10), (void *)IMPU(0x14)) < 0) {
                    return (s32)0x8001000C;
                }
                if (sceKernelDeleteFpl(IMP(0x10)) < 0) {
                    return (s32)0x8001000C;
                }
                if (sub_293c() < 0) {
                    return (s32)0x8001000C;
                }
                if (sub_27f8() < 0) {
                    return (s32)0x8001000C;
                }
                IMP(0x1B0) = 0;
                IMP(0x10) = 0;
                IMP(0x14) = 0;
            }
        }
        if ((pattern & 0x10u) != 0) {
            if (sceKernelInvokeExitCallback() >= 0) {
                IMPU(0xA4) |= 0x80000000u;
            }
        }
        if ((pattern & 0x80000000u) != 0) {
            sceKernelExitThread(0);
            return 0;
        }
        IMP(0xC0) = 0;
    }
    return 0;
}

static s32 sub_3e0c(u32 id)
{
    const char *cat = NULL;
    const char *key = NULL;
    s32 val = -1;

    switch (id) {
    case 3:
        cat = "/CONFIG/SYSTEM/SOUND";
        key = "main_volume";
        val = IMP(0x28);
        break;
    case 4:
        cat = "/CONFIG/SYSTEM/SOUND";
        key = "mute";
        val = IMP(0x2C);
        break;
    case 6:
        cat = "/CONFIG/SYSTEM/SOUND";
        key = "equalizer_mode";
        val = IMP(0x38);
        break;
    case 7:
        cat = "/CONFIG/SYSTEM";
        key = "backlight_brightness";
        val = IMP(0xB4);
        break;
    case 16:
        cat = "/CONFIG/DISPLAY";
        key = "aspect_ratio";
        val = IMP(0x94);
        break;
    case 17:
        cat = "/CONFIG/DISPLAY";
        key = "scan_mode";
        val = IMP(0x98);
        break;
    case 20:
        cat = "/CONFIG/DISPLAY";
        key = "pi_blending_mode";
        val = IMP(0x138);
        break;
    default:
        return 0;
    }
    if (val == -1) {
        return 0;
    }
    return (sub_648c(cat, key, val) != 0) ? -1 : 0;
}

static void sub_3f54(void)
{
    if (IMP(0xA4) < 0) {
        if (IMP(0xB8) >= 0) {
            sub_0590(0xFF);
            if (IMP(0x1D0) != 0) {
                sub_5588(IMP(0x22C), IMP(0x230), IMP(0x234), IMP(0x238), (const void *)IMPU(0x1E0));
                IMPU(0xB8) |= 0x80000000u;
                sub_009c(0x10);
            }
        }
        return;
    }
    s32 r = sub_5ddc();
    if ((IMPU(0xB8) & 0x10u) == 0) {
        if (sub_6008() == 0) {
            return;
        }
    }
    if (r != 0) {
        return;
    }
    if ((IMPU(0xA4) & 0x00010000u) != 0) {
        r = sub_5830();
        if (r != 0) {
            return;
        }
    }
    if (IMP(0x1D0) == 0) {
        return;
    }
    if ((IMPU(0xB8) & 8u) == 0) {
        sub_5588(IMP(0x1FC), IMP(0x200) + IMP(0x158), IMP(0x204), IMP(0x208), (const void *)IMPU(0x1D4));
        sub_5588(IMP(0x2A4), IMP(0x2A8), 16, 16, (const void *)IMPU(0x2C8));
        sub_5588(IMP(0x29C), IMP(0x2A0), 16, 16, (const void *)IMPU(0x2C4));
        if (IMP(0x24) != 0) {
            sub_5588(IMP(0x21C), IMP(0x220), IMP(0x214), IMP(0x218), (const void *)IMPU(0x1D8));
            sub_5588(IMP(0x20C), IMP(0x210), IMP(0x224), IMP(0x228), (const void *)IMPU(0x1DC));
        } else {
            sub_5588(IMP(0x21C), IMP(0x220), IMP(0x224), IMP(0x228), (const void *)IMPU(0x1DC));
            sub_5588(IMP(0x20C), IMP(0x210), IMP(0x214), IMP(0x218), (const void *)IMPU(0x1D8));
        }
        IMPU(0xB8) |= 8u;
        return;
    }
    u32 sp0 = IMPU(0x1E4);
    u32 sp4 = IMPU(0x1F4);
    if (IMP(0xF4) != 0) {
        if (IMP(0xA8) != 0) {
            sp0 = IMPU(0x1E8);
            sp4 = IMPU(0x1F0);
        }
    }
    if (IMPU(0x150) != sp0) {
        IMPU(0x150) = sp0;
        IMPU(0xB8) &= ~0x100u;
        IMP(0x148) = 0;
    }
    if (IMPU(0x154) != sp4) {
        IMPU(0x154) = sp4;
        IMPU(0xB8) &= ~0x200u;
        IMP(0x14C) = 0;
    }
    if ((IMPU(0xB8) & 0x100u) == 0) {
        sub_5588(IMP(0x23C), IMP(0x240) + IMP(0x158), IMP(0x244), IMP(0x248), (const void *)sp0);
        IMPU(0xB8) |= 0x100u;
    }
    if ((IMPU(0xB8) & 0x200u) == 0) {
        sub_5588(IMP(0x26C), IMP(0x270) + IMP(0x158), IMP(0x274), IMP(0x278), (const void *)sp4);
        IMPU(0xB8) |= 0x200u;
    }
    sub_5638(0xB9);
}

static void sub_4334(void)
{
    if ((IMP(0x164) % 20) < 12) {
        sub_54f0(0xE2, 0x7B, 0x1C, 0x19, (const void *)IMPU(0x2C0));
    } else {
        u8 *dst = (u8 *)IMPU(0x1B0) + ((IMP(0x18) * 0x7B + 0xE2) << 1);
        s32 i;
        for (i = 0; i < 25; i++) {
            sceKernelMemset32(dst, (s32)0x9CE79CE7u, 0x38);
            dst += (IMP(0x18) >> 1) << 2;
        }
    }
}

static void sub_441c(void)
{
    if (IMP(0xB8) >= 0) {
        u8 *dst = (u8 *)IMPU(0x1B0) + ((IMP(0x18) * 0x7C + 0xCA) << 1);
        const u32 *src = (const u32 *)IMPU(0x2E0);
        s32 y, x;
        for (y = 0; y < 32; y++) {
            u32 *d = (u32 *)dst;
            for (x = 0; x < 21; x++) {
                d[0] = src[0];
                d[1] = src[1];
                d += 2;
                src += 2;
            }
            dst = (u8 *)d + (((IMP(0x18) - 0x54) / 4) << 3);
        }
        IMPU(0xB8) |= 0x80000000u;
    }
    if (IMP(0x168) < 0x47) {
        IMP(0x168)++;
        return;
    }
    sceImpose_driver_BB12F974(1);
    if (IMP(0x31C) == 2 && g_tvout_mode != NULL) {
        sub_173c(0, g_tvout_mode->mode1, g_tvout_mode->width, g_tvout_mode->height);
    }
}

static void sub_452c(void)
{
    IMP(0x348) = 3;
    sub_5638(0x80);
}

static s32 sub_4558(void)
{
    if (IMP(0x14) == 0) {
        return -1;
    }
    s32 fd = sceIoOpen("flash0:/kd/resource/impose.rsc", 1, 0);
    if (fd < 0) {
        return (s32)0x80010002;
    }
    s32 size = (s32)sceIoLseek(fd, 0, 2);
    if (size < 0 || sceIoLseek(fd, 0, 0) < 0 || sceIoRead(fd, (void *)IMPU(0x14), (u32)size) < 0) {
        sceIoClose(fd);
        return (s32)0x80010002;
    }
    sceIoClose(fd);
    return size;
}

static s32 sub_4644(s32 lang)
{
    if (IMP(0x14) == 0) {
        return -1;
    }
    s32 intr = sceKernelCpuSuspendIntr();
    (void)lang;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static s32 sub_48b0(void)
{
    u8 *rsc = (u8 *)IMPU(0x14);
    if (rsc == NULL) {
        return -1;
    }
    s32 count = *(s32 *)(rsc + 8);
    s32 intr = sceKernelCpuSuspendIntr();
    if (count > 0 && IMPU(0x2B4) != 0) {
        u8 *dst = (u8 *)IMPU(0x2B4);
        s32 i;
        for (i = 0; i < count && i < 16; i++) {
            s32 idx = *(s32 *)rsc + *(s32 *)(rsc + 4) + i;
            u32 off = *(u32 *)(rsc + (idx << 2) + 0x0C);
            u8 *src = rsc + off;
            u32 len = *(u32 *)src;
            memcpy(dst, src + 4, len);
            IMPU(0x2BC + (i << 2)) = (u32)dst;
            dst += len;
        }
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static void sub_49f4(u32 a0, u32 a1, u32 a2, s32 a3)
{
    u32 sp[2];

    IMP(0x368) = 0;
    IMP(0x340) = 0;
    IMP(0x344) = 0;
    IMP(0x348) = 0;
    IMP(0x354) = 0;
    IMP(0x358) = 0;
    IMP(0x35C) = 0;
    IMP(0x360) = 0;
    IMP(0x364) = 0;
    u32 prev_4c = IMPU(0x34C);
    u32 prev_50 = IMPU(0x350);
    IMPU(0x34C) = 0;
    IMPU(0x350) = 0;
    s32 prev_158 = IMP(0x158);
    IMP(0x158) = 0;

    sp[0] = a0;
    sp[1] = a1;
    SysMemForKernel_2A8B8B2D(sp);

    if ((a0 | a1) != 0) {
        u64 total = ((u64)a1 << 32) | a0;
        u64 sub = (u64)(u32)a2 * 1000000ULL + ((u64)(u32)a3 * 1000000ULL << 32);
        u64 diff = total - sub;
        IMPU(0x34C) = a0;
        IMPU(0x350) = a1;
        IMPU(0x354) = (u32)diff;
        IMPU(0x358) = (u32)(diff >> 32);
        IMP(0x344) = 1;
        IMP(0x158) = -16;
        IMP(0x340) = 1;
    }

    if (prev_4c != a0 || prev_50 != a1) {
        if (IMP(0x14) != 0) {
            s32 cur_158 = IMP(0x158);
            IMP(0x158) = prev_158;
            sub_0590(0x22);
            IMP(0x158) = cur_158;
            if ((IMPU(0xA4) & 0x100u) != 0) {
                IMP(0x164) = 0;
                IMP(0x150) = 0;
                IMP(0x154) = 0;
                IMP(0x148) = 0;
                IMP(0x14C) = 0;
                IMPU(0xB8) &= ~0x718u;
            }
        }
        IMPU(0xBC) |= 0x4000u;
    }
}

static void sub_4ba0(s32 mode)
{
    s32 v;

    if (mode == -1) {
        v = sub_73d4(0);  if (v != -1) IMP(0x18) = v;
        v = sub_73d4(1);  if (v != -1) IMP(0x1C) = v;
        v = sub_73d4(2);  if (v != -1) IMP(0x20) = v;
        v = sub_73d4(14); if (v != -1) IMP(0x24) = v;
        v = sub_73d4(15); if (v != -1) IMP(0x78) = v;
        v = sub_73d4(19); if (v != -1) IMP(0xA0) = v;
    }
    v = sub_73d4(3);  if (v != -1) IMP(0x28) = v;
    v = sub_73d4(4);  if (v != -1) IMP(0x2C) = v;
    v = sub_73d4(5);  if (v != -1) IMP(0x34) = v;
    v = sub_73d4(6);  if (v != -1) IMP(0x38) = v;
    v = sub_73d4(8);  if (v != -1) IMP(0x48) = v;
    v = sub_73d4(9);  if (v != -1) IMP(0x4C) = v;
    v = sub_73d4(12); if (v != -1) IMP(0x64) = v;
    v = sub_73d4(13); if (v != -1) IMP(0x68) = v;

    if (IMP(0x110) != 0) {
        v = sub_73d4(16); if (v != -1) IMP(0x94) = v;
        v = sub_73d4(17); if (v != -1) IMP(0x98) = v;
        v = sub_73d4(18); if (v != -1) IMP(0x6C) = v;
        IMP(0x138) = 1;
        v = sub_73d4(20); if (v != -1) IMP(0x138) = v;
        if ((IMPU(0x110) & 4u) != 0) {
            sceDisplay_driver_7DF044BA(IMP(0x138));
        }
    }

    if (IMP(0x10C) != 0x100) {
        v = sub_73d4(21);
        s32 arg = 2;
        if (v == 1) {
            arg = 1;
        } else if (v == 2) {
            arg = 0;
        }
        sceDisplay_driver_9B18DDDD(arg);
    }

    v = sub_73d4(22);
    sceDisplay_driver_F455917F((v == 1) ? 1 : 0);

    v = sub_73d4(7);
    s32 s2_flag = 0;
    s32 saved_b4 = 0;
    if ((u32)v < 4u) {
        IMP(0xB4) = v;
        s32 cur = sub_00c4(IMP(0xDC));
        if (cur < v) {
            saved_b4 = v;
            s2_flag = 1;
            IMP(0xB4) = cur;
        }
    } else {
        if (IMP(0x108) == 10 || IMP(0x58) == 0) {
            IMP(0x3C) = 72;
            IMP(0xB4) = 2;
        } else {
            IMP(0x3C) = 100;
            IMP(0xB4) = 3;
        }
    }
    IMP(0xD4) = 1;
    sub_0210(0);
    if (s2_flag != 0) {
        IMP(0xB4) = saved_b4;
    }
    if (IMP(0x110) != 0 && IMP(0x328) != 0 && IMP(0x328) != 0xE0) {
        sub_0210(3);
    }

    s32 v11 = sub_73d4(11);
    s32 v10 = sub_73d4(10);
    if (v10 == -1) v10 = 0;
    if (v11 == -1) v11 = 0;
    sceRtc_driver_DFF30673(IMP(0x4C), IMP(0x48), v11, v10 * 60);
    sceRtc_driver_9012B140(IMP_PTR(0x194));

    if (mode == -1) {
        if ((u32)IMP(0x20) >= 12u) {
            IMP(0x20) = 0;
        }
        IMP(0x314) = IMP(0x20);
        IMP(0x318) = IMP(0x24);
        IMP(0x100) = IMP(0x20);
    } else if (mode == 2) {
        v = sub_73d4(2);
        if ((u32)v >= 12u) {
            v = 0;
        }
        if (IMP(0x100) == IMP(0x20) && IMP(0x100) != v) {
            IMP(0x314) = v;
            IMP(0x100) = v;
        }
        if (sub_169c() != 0) {
            IMP(0x20) = v;
        } else {
            IMP(0x310) = 1;
            IMP(0x20) = -1;
        }
    }
}

static void sub_4ff0(void)
{
    if (IMP(0x110) == 0) {
        return;
    }
    if (IMP(0x10C) == 0x100) {
        sceLcdc_driver_451FE1A1(0);
    } else if ((IMPU(0x110) & 0x100u) != 0) {
        if (IMP(0x94) == 0) {
            sceLcdc_driver_451FE1A1(7);
        } else if (IMP(0x94) == 1) {
            sceLcdc_driver_451FE1A1(3);
        }
    }

    if (IMP(0x328) == 0 || IMP(0x328) == 0xE0) {
        IMP(0x31C) = 0;
        sub_055c(1);
        if (IMP(0x134) != 2) {
            sub_009c(0x4020);
        }
        return;
    }

    IMP(0x31C) = 1;
    sub_055c(2);
    if (IMP(0x134) != 2) {
        sub_009c(0x4020);
    }
    if (IMP(0x134) == 2) {
        return;
    }
    IMP(0x9C) = 0;
    if (IMP(0x10C) == 0x300) {
        return;
    }
    s32 target_10c = (IMP(0x10C) < 0x301) ? 0x200 : 0x400;
    if (IMP(0x10C) != target_10c) {
        return;
    }
    if (g_tvout_mode != NULL) {
        sub_173c(1, g_tvout_mode->mode0, g_tvout_mode->width, g_tvout_mode->height);
    }
}

static void sub_520c(void)
{
    sub_009c(0x10);
}

static void sub_5228(void)
{
    s32 changed = 0;
    s32 v = sub_0520();
    if (v != 0 && IMP(0x124) != v) {
        IMP(0x124) = v;
        if (v == 0x10) {
            sub_056c((IMP(0x31C) == 1) ? 2 : 1);
        } else {
            IMP(0x12C) = 0;
            IMP(0x128) = 0;
        }
    }
    if (IMP(0x124) != 0x10) {
        return;
    }
    v = sceHprm_driver_FA4A25A7();
    if (IMP(0x128) != v) {
        IMP(0x128) = v;
        if (IMP(0x130) == 2 && v == 0 && IMP(0x12C) == 0) {
            sub_056c(1);
        }
        if (IMP(0x12C) == 0) {
            changed = 1;
        }
    }
    v = sceHprm_driver_EFCFD0C5();
    if (IMP(0x12C) != v) {
        IMP(0x12C) = v;
        if (IMP(0x130) == 2 && v == 0) {
            sub_056c(1);
        }
        changed = 1;
    }
    if (changed != 0) {
        sub_009c(0x20);
    }
}

static void sub_5360(void)
{
    s32 v = sceHprm_driver_AD158331();
    if (v >= 0 && IMP(0x118) != v) {
        u32 mask = (v == 2) ? 2u : ((v == 1) ? 1u : 0u);
        sceAudio_driver_5182B550(mask);
        IMP(0x118) = v;
    }
}

static void sub_53c4(void)
{
    if (IMP(0x340) == 0) {
        return;
    }
    if (IMP(0x344) == 1) {
        if (IMPU(0x350) == 0 && IMPU(0x34C) == 0) {
            IMPU(0xB8) &= ~0x400u;
            IMPU(0xBC) |= 0x2000u;
            IMP(0x348) = 2;
        } else if (IMPU(0x358) == 0 && IMPU(0x354) == 0) {
            IMPU(0xB8) &= ~0x400u;
            IMPU(0xBC) |= 0x2000u;
            IMP(0x348) = 1;
        }
    } else if (IMP(0x344) == 3) {
        u32 now = sceKernelGetSystemTimeLow();
        if ((s32)(now - IMPU(0x184)) > 0x4C4B40) {
            IMPU(0xA4) |= 0x80000000u;
            sub_009c(0x10);
            IMP(0x344) = 4;
        }
    }
}

static void sub_54f0(s32 x, s32 y, s32 w, s32 h, const void *src)
{
    if (h <= 0 || IMPU(0x1B0) == 0 || src == NULL) {
        return;
    }
    u8 *dst = (u8 *)IMPU(0x1B0) + ((IMP(0x18) * y + x) << 1);
    const u32 *s = (const u32 *)src;
    s32 row, col;
    s32 cols = w / 4;
    for (row = 0; row < h; row++) {
        u32 *d = (u32 *)dst;
        for (col = 0; col < cols; col++) {
            d[0] = s[0];
            d[1] = s[1];
            d += 2;
            s += 2;
        }
        dst = (u8 *)d + (((IMP(0x18) - w) / 4) << 3);
    }
}

static void sub_5588(s32 x, s32 y, s32 w, s32 h, const void *src)
{
    sub_54f0(x, y, w, h, src);
}

static void sub_5638(s32 y)
{
    if (IMP(0x340) == 0 || (IMPU(0xB8) & 0x400u) != 0) {
        return;
    }
    IMPU(0xB8) |= 0x400u;
    sub_75fc(IMP_PTR(0x35C), 16, y + 2, 2);
}

static s32 sub_5830(void)
{
    if (IMP(0x338) == 1) {
        if ((IMPU(0xB8) & 0x20u) == 0) {
            sub_5588(0x1C, 0xDF, 0x20, 0x1C, (const void *)(IMPU(0x304) + 0xE00));
            IMPU(0xB8) |= 0x20u;
        }
        return 0;
    }
    if ((IMPU(0xB8) & 0x20u) == 0) {
        u32 icon = IMPU(0x304) + ((IMP(0x2C) != 0) ? 0x700u : 0u);
        sub_5588(0x1C, 0xDF, 0x20, 0x1C, (const void *)icon);
        IMPU(0xB8) |= 0x20u;
    }
    return 0;
}

static s32 sub_5ddc(void)
{
    s32 ret = 0;
    if ((IMPU(0xB8) & 1u) == 0) {
        sub_0590(1);
        IMP(0xD0) = sub_75fc(IMP_PTR(0x194), (IMP(0x60) != 0) ? 0x1C2 : 0x1E0, 5, 0);
        if (IMP(0x30) != 0) {
            IMP(0xD0) -= 0x1D;
            sub_5588(IMP(0xD0), 7, 0x18, 0x0C, (const void *)IMPU(0x2D0));
        }
        IMPU(0xB8) |= 1u;
        if (IMP(0x60) != 0) {
            IMPU(0xB8) &= ~0x80u;
        }
        ret = 1;
    }
    if (IMP(0x60) != 0) {
        sub_7e74();
    }
    return ret;
}

static s32 sub_6008(void)
{
    if ((u32)IMP(0xC8) < 5u) {
        IMP(0xC8)++;
        return 0;
    }
    if ((IMPU(0xB8) & 0x10u) == 0) {
        sub_5588(0x50, IMP(0x158) + 0x60, 0x148, 9, (const void *)IMPU(0x2DC));
        sub_5588(0x50, IMP(0x158) + 0xAF, 0x148, 9, (const void *)IMPU(0x2DC));
        IMPU(0xB8) |= 0x10u;
    }
    return 1;
}

static s32 sub_648c(const char *category, const char *key, s32 val)
{
    RegParam reg;
    s32 h = 0;
    s32 hd = 0;

    memset(&reg, 0, 0x110);
    reg.regtype = 1;
    strncpy(reg.name, "/system", 0xFF);
    reg.namelen = 7;
    reg.unk2 = 1;
    reg.unk3 = 1;

    if (sceRegOpenRegistry(&reg, 2, &h) != 0) {
        return -1;
    }
    if (sceRegOpenCategory(h, category, 2, &hd) != 0) {
        sceRegCloseRegistry(h);
        return -1;
    }
    if (sceRegSetKeyValue(hd, key, &val, 4) != 0) {
        sceRegCloseCategory(hd);
        sceRegCloseRegistry(h);
        return -1;
    }
    sceRegFlushCategory(hd);
    sceRegCloseCategory(hd);
    sceRegFlushRegistry(h);
    sceRegCloseRegistry(h);
    return 0;
}

static void sub_658c(s32 set, u32 mask)
{
    s32 intr = sceKernelCpuSuspendIntr();
    if (set == 0) {
        IMPU(0x74) &= ~mask;
    } else {
        IMPU(0x74) |= mask;
    }
    sceKernelCpuResumeIntr(intr);
}

static s32 sub_660c(void *arg0, s32 arg1, void *arg2)
{
    (void)arg0;
    (void)arg1;
    (void)arg2;
    sub_4ff0();
    IMP(0x134) = 0;
    return 0;
}

static s32 sub_6638(s32 mode)
{
    if ((u32)mode >= 4u) {
        return (s32)0x802C01FE;
    }
    s32 prev = IMP(0x54);
    if (prev != mode) {
        s32 intr = sceKernelCpuSuspendIntr();
        prev = IMP(0x54);
        IMP(0x84) = 1;
        IMP(0x54) = mode;
        sub_009c(0x20);
        sceKernelCpuResumeIntr(intr);
    }
    return prev;
}

static s32 sub_66d8(s32 mode)
{
    s32 s3_val;
    if (mode == 0) {
        s3_val = (IMP(0x2C) != 0) ? 0 : IMP(0x28);
    } else if (mode == 1) {
        s3_val = 30;
    } else {
        return (s32)0x802C01FE;
    }
    s32 intr = sceKernelCpuSuspendIntr();
    IMP(0x80) = s3_val;
    IMP(0x7C) = mode;
    IMP(0xC4) = 0;
    sub_009c(0x400);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static s32 sub_6788(void)
{
    IMPU(0xC4) &= ~0x00400000u;
    if (IMP(0x110) == 0 || IMP(0x320) == 0) {
        return -1;
    }
    if (IMP(0x324) == 1) {
        return -1;
    }
    if (sceImpose_driver_E9B0E008(NULL) < 0) {
        return -1;
    }
    IMP(0x31C) = 2;
    IMPU(0xBC) |= 0x200u;
    if (IMP(0xFC) != 0) {
        sub_2c00();
        sub_0210(0);
        sub_3380(2, 1);
        IMP(0xF8) = 0;
        IMPU(0xBC) |= 0x20000000u;
    }
    IMP(0xFC) = 0;
    if (sub_169c() == 0) {
        sceImpose_driver_BB12F974(0x40402);
    }
    return 0;
}

static s32 sub_6870(void)
{
    IMPU(0xC4) &= ~0x00400000u;
    if (IMP(0x110) == 0) {
        return -1;
    }
    if (IMP(0x320) == 1) {
        if (sub_169c() == 0 && g_tvout_mode != NULL) {
            sub_173c(0, g_tvout_mode->mode0, g_tvout_mode->width, g_tvout_mode->height);
        }
        IMPU(0xBC) |= 0x200u;
    }
    return 0;
}

static void sub_6908(u32 trig, u32 cur_buttons, u32 prev_buttons)
{
    (void)trig;
    (void)prev_buttons;

    if (IMP(0xF8) != 0) {
        if ((cur_buttons & 0x00F1FFFFu) != 0) {
            if (IMP(0xF8) == 3) {
                sub_0210(0);
                sub_3380(2, 1);
                IMP(0xF8) = 1;
                IMPU(0xBC) |= 0x20000000u;
            }
        } else if (IMP(0xF8) == 1) {
            sub_2c00();
            IMP(0xF8) = 0;
        } else if (IMP(0xF8) == 2) {
            IMP(0xF8) = 3;
        }
    }

    if ((IMPU(0xB0) & 0x10u) != 0) {
        cur_buttons |= 0x00100000u;
    }
    if ((IMPU(0xB0) & 0x20u) != 0) {
        cur_buttons |= 0x00200000u;
    }

    if ((IMPU(0xC4) & 0x00200000u) != 0) {
        if (IMP(0x28) > 0) {
            IMP(0x28)--;
        }
        IMPU(0xC4) &= ~0x00200000u;
        sub_009c(4);
    }
    if ((IMPU(0xC4) & 0x00100000u) != 0) {
        s32 max_vol = (IMP(0x34) == 0) ? 30 : 20;
        if (IMP(0x28) < max_vol) {
            IMP(0x28)++;
        }
        IMPU(0xC4) &= ~0x00100000u;
        sub_009c(4);
    }
    if ((IMPU(0xC4) & 0x00800000u) != 0) {
        IMPU(0xC4) &= ~0x00800000u;
        sub_009c(4);
    }
    if ((IMPU(0xC4) & 0x00400000u) != 0) {
        IMPU(0xC4) &= ~0x00400000u;
        if (IMP(0xF8) == 0 && IMP(0x134) == 0) {
            sub_0210(1);
        }
    }
}

static void sub_7018(void)
{
    if ((IMPU(0xA4) & 1u) != 0 || IMP(0x188) != 0) {
        return;
    }
    sub_0590(0xFF);
    s32 intr = sceKernelCpuSuspendIntr();
    sceDisplay_driver_3E17FE8D(0, (void *)IMPU(0x1B0), 512, 1, 1);
    IMP(0xA8) = 0;
    IMP(0x188) = 1;
    IMP(0xB8) = 0;
    IMP(0xC8) = 0;
    IMP(0x160) = 0;
    IMP(0x168) = 0;
    IMP(0xAC) = 0;
    if (IMP(0x348) == 2) {
        IMP(0xA8) = 1;
    } else {
        IMP(0xF4) = 1;
    }
    sceKernelCpuResumeIntr(intr);
}

static s32 sub_70e0(s32 a0, s32 a1)
{
    s32 ret = 0;
    (void)a0;
    (void)a1;

    if (IMP(0x9C) > 0 && IMP(0x9C) < 3) {
        IMP(0x338) = 1;
        IMPU(0xBC) |= 0x400u;
        if (IMP(0x2C) != 0) {
            IMP(0x2C) = 0;
            IMPU(0xBC) |= 0x408u;
            ret = 1;
        }
    } else {
        IMP(0x338) = IMP(0x78);
        IMPU(0xBC) |= 0x400u;
        if ((IMPU(0x110) & 0x02000000u) == 0) {
            if (IMP(0x31C) == 1) {
                if (sub_169c() == 0 && g_tvout_mode != NULL) {
                    sub_173c(1, g_tvout_mode->mode0, g_tvout_mode->width, g_tvout_mode->height);
                }
                IMPU(0xBC) |= 0x200u;
            } else if (IMP(0x31C) == 2) {
                IMP(0x31C) = 0;
            }
        }
        sub_055c(1);
    }
    IMPU(0xB8) &= ~0x27u;
    return ret;
}

static void sub_7290(s32 a0, s32 a1)
{
    (void)a0;
    IMP(0x128) = sceHprm_driver_FA4A25A7();
    IMP(0x12C) = sceHprm_driver_EFCFD0C5();
    IMP(0x140) = sceHprm_driver_370DDC12();
    if ((IMPU(0x114) & 2u) != 0) {
        IMP(0x144) = sceHprm_driver_1C3525DD();
    }
    if (IMP(0x90) != 0 && IMP(0x338) != 1 && IMP(0xC0) == 0 && IMP(0x84) == 0 &&
        a1 != 0 && (IMPU(0x88) & 0x10u) == 0 && IMP(0x38) != 0) {
        IMPU(0xBC) |= 4u;
    }
    sub_8138();
    IMP(0x84) = 0;
}

static void sub_7388(s32 a0)
{
    (void)a0;
    IMP(0x128) = sceHprm_driver_FA4A25A7();
    IMP(0x12C) = sceHprm_driver_EFCFD0C5();
    sub_8138();
}

static s32 sub_73d4(u32 id)
{
    s32 val = 0;
    const char *cat = NULL;
    const char *key = NULL;

    switch (id) {
    case 0:  return 512;
    case 1:  return 272;
    case 2:  cat = "/CONFIG/SYSTEM/XMB";          key = "language"; break;
    case 3:  cat = "/CONFIG/SYSTEM/SOUND";        key = "main_volume"; break;
    case 4:  cat = "/CONFIG/SYSTEM/SOUND";        key = "mute"; break;
    case 5:  cat = "/CONFIG/SYSTEM/SOUND";        key = "avls"; break;
    case 6:  cat = "/CONFIG/SYSTEM/SOUND";        key = "equalizer_mode"; break;
    case 7:  cat = "/CONFIG/SYSTEM";              key = "backlight_brightness"; break;
    case 8:  cat = "/CONFIG/DATE";                key = "time_format"; break;
    case 9:  cat = "/CONFIG/DATE";                key = "date_format"; break;
    case 10: cat = "/CONFIG/DATE";                key = "summer_time"; break;
    case 11: cat = "/CONFIG/DATE";                key = "time_zone_offset"; break;
    case 12: cat = "/CONFIG/SYSTEM/POWER_SAVING"; key = "suspend_interval"; break;
    case 13: cat = "/CONFIG/SYSTEM/POWER_SAVING"; key = "backlight_off_interval"; break;
    case 14: cat = "/CONFIG/SYSTEM/XMB";          key = "button_assign"; break;
    case 15: cat = "/SYSPROFILE";                 key = "sound_reduction"; break;
    case 16: cat = "/CONFIG/DISPLAY";             key = "aspect_ratio"; break;
    case 17: cat = "/CONFIG/DISPLAY";             key = "scan_mode"; break;
    case 18: cat = "/CONFIG/DISPLAY";             key = "screensaver_start_time"; break;
    case 19: cat = "/CONFIG/SYSTEM/CHARACTER_SET"; key = "oem"; break;
    case 20: cat = "/CONFIG/DISPLAY";             key = "pi_blending_mode"; break;
    case 21: cat = "/CONFIG/DISPLAY";             key = "color_space_mode"; break;
    case 22: cat = "/CONFIG/SYSTEM/POWER_SAVING"; key = "active_backlight_mode"; break;
    default: return -1;
    }

    if (sub_800c(cat, key, &val) != 0) {
        return -1;
    }
    return val;
}

static s32 sub_75fc(const void *time_ptr, s32 x, s32 y, s32 align)
{
    (void)time_ptr;
    (void)y;
    (void)align;
    return x;
}

static void sub_7e74(void)
{
    if ((IMPU(0xB8) & 0x80u) != 0) {
        return;
    }
    s32 lvl = 3;
    if (IMP(0x44) == 0) {
        s32 pct = IMP(0x40);
        s32 i;
        lvl = 0;
        for (i = 0; i < 3; i++) {
            if (pct >= g_battery_thresholds[i]) {
                break;
            }
            lvl++;
        }
    }
    IMPU(0xB8) |= 0x80u;
    (void)lvl;
}

static s32 sub_800c(const char *category, const char *key, s32 *out)
{
    RegParam reg;
    s32 h = 0;
    s32 hd = 0;
    s32 hk = 0;
    u32 type = 0;
    u32 size = 0;
    s32 val = 0;

    memset(&reg, 0, 0x110);
    reg.regtype = 1;
    strncpy(reg.name, "/system", 0xFF);
    reg.namelen = 7;
    reg.unk2 = 1;
    reg.unk3 = 1;

    if (sceRegOpenRegistry(&reg, 2, &h) != 0) {
        return -1;
    }
    if (sceRegOpenCategory(h, category, 2, &hd) != 0) {
        sceRegCloseRegistry(h);
        return -1;
    }
    if (sceRegGetKeyInfo(hd, key, &hk, &type, &size) != 0 ||
        sceRegGetKeyValue(hd, hk, &val, 4) != 0) {
        sceRegCloseCategory(hd);
        sceRegCloseRegistry(h);
        return -1;
    }
    sceRegFlushCategory(hd);
    sceRegCloseCategory(hd);
    sceRegFlushRegistry(h);
    sceRegCloseRegistry(h);
    *out = val;
    return 0;
}

static void sub_8138(void)
{
    s32 m = IMP(0x54);
    s32 a0 = 0;
    s32 a1 = 0;

    if (m == 0) {
        a0 = 0;
        a1 = 0;
    } else if (m == 1) {
        a0 = 1;
        a1 = 1;
    } else if (m == 2) {
        a0 = 0;
        a1 = 1;
    } else if (m == 3) {
        a0 = 1;
        a1 = 0;
    } else {
        return;
    }
    sceKermitPeripheral_driver_76BE2E28(a0, a1);
}

/* =========================================================================
 * Exported Functions: sceImpose & sceImpose_driver
 * ========================================================================= */

s32 sceImposeGetHomePopup(void)
{
    s32 intr = sceKernelCpuSuspendIntr();
    s32 res = (IMP(0x74) ^ 1) & 1;
    sceKernelCpuResumeIntr(intr);
    return res;
}

s32 sceImpose_driver_E9A42056(void) __attribute__((alias("sceImposeGetHomePopup")));

s32 sceImposeSetHomePopup(s32 val)
{
    sub_658c(val ^ 1, 1);
    return 0;
}

s32 sceImposeGetUMDPopup(void)
{
    s32 intr = sceKernelCpuSuspendIntr();
    s32 res = ((IMP(0x74) ^ 2) >> 1) & 1;
    sceKernelCpuResumeIntr(intr);
    return res;
}

s32 sceImpose_driver_EF4D1BC5(void) __attribute__((alias("sceImposeGetUMDPopup")));

s32 sceImposeSetUMDPopup(s32 val)
{
    sub_658c(val ^ 1, 2);
    return 0;
}

s32 sceImposeGetLanguageMode(s32 *lang, s32 *button)
{
    if (lang == NULL || button == NULL) {
        return (s32)0x80000103;
    }
    s32 intr = sceKernelCpuSuspendIntr();
    *lang = IMP(0x314);
    *button = IMP(0x318);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

s32 sceImpose_driver_24FD7BCF(s32 *lang, s32 *button) __attribute__((alias("sceImposeGetLanguageMode")));

s32 sceImposeSetLanguageMode(s32 lang, s32 button)
{
    if ((u32)lang >= 12u || (u32)button >= 2u) {
        return (s32)0x80000107;
    }
    s32 intr = sceKernelCpuSuspendIntr();
    IMP(0x314) = lang;
    IMP(0x318) = button;
    if (IMP(0x104) == 0 && (IMP(0x20) != lang || IMP(0x24) != button)) {
        IMP(0x100) = lang;
        IMP(0x24) = button;
        sub_009c(0x200);
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

s32 sceImposeGetBacklightOffTime(void)
{
    s32 intr = sceKernelCpuSuspendIntr();
    s32 res = sceImpose_133636DD((s32)0x80000002);
    sceKernelCpuResumeIntr(intr);
    return res;
}

s32 sceImpose_driver_E1A76241(void) __attribute__((alias("sceImposeGetBacklightOffTime")));

s32 sceImposeSetBacklightOffTime(s32 val)
{
    if (val < 30) {
        return (s32)0x800001FE;
    }
    s32 intr = sceKernelCpuSuspendIntr();
    s32 res = sceImpose_FCD44963((s32)0x80000002, val);
    sceKernelCpuResumeIntr(intr);
    return res;
}

s32 sceImpose_driver_37F3F1D2(s32 val) __attribute__((alias("sceImposeSetBacklightOffTime")));

s32 sceImpose_133636DD(s32 id)
{
    s32 intr = sceKernelCpuSuspendIntr();
    s32 res = (s32)0x80000107;

    switch ((u32)id) {
    case 1:          res = IMP(0x28); break;
    case 2:          res = IMP(0x2C); break;
    case 4:          res = IMP(0x34); break;
    case 8:          res = IMP(0x38); break;
    case 0x10:       res = IMP(0x90); break;
    case 0x20:       res = IMP(0xB0); break;
    case 0x40:       res = IMP(0xB4); break;
    case 0x80:       res = IMP(0x94); break;
    case 0x100:      res = IMP(0x98); break;
    case 0x200:      res = IMP(0x9C); break;
    case 0x400:      res = IMP(0xA0); break;
    case 0x2000:     res = IMP(0x368); break;
    case 0x20000000: res = IMP(0xE8) & 1; break;
    case 0x80000001: res = IMP(0x64); break;
    case 0x80000002: res = IMP(0x68); break;
    case 0x80000003: res = IMP(0x48); break;
    case 0x80000004: res = IMP(0x4C); break;
    case 0x80000005: res = IMP(0x50); break;
    case 0x80000006: res = IMP(0x6C); break;
    case 0x80000007: res = IMP(0x70); break;
    case 0x80000008: res = IMP(0x78); break;
    case 0x80000009: res = IMP(0x134); break;
    case 0x8000000A: res = IMP(0x138); break;
    case 0x8000000B: res = IMP(0x13C); break;
    default: break;
    }

    sceKernelCpuResumeIntr(intr);
    return res;
}

s32 sceImpose_driver_DC3BECFF(s32 id) __attribute__((alias("sceImpose_133636DD")));

s32 sceImpose_FCD44963(s32 id, s32 val)
{
    s32 intr = sceKernelCpuSuspendIntr();
    s32 res = 0;

    switch ((u32)id) {
    case 1:
        if ((u32)val < 31u) {
            IMP(0x28) = val;
            sub_009c(4);
        } else {
            res = (s32)0x80000108;
        }
        break;
    case 2:
        if ((u32)val < 2u) {
            IMP(0x2C) = val;
            sub_009c(4);
        } else {
            res = (s32)0x80000108;
        }
        break;
    case 4:
        if ((u32)val < 2u) {
            IMP(0x34) = val;
            sub_009c(0x40);
        } else {
            res = (s32)0x80000108;
        }
        break;
    case 8:
        if ((u32)val < 6u) {
            IMP(0x38) = val;
            sub_009c(0x40);
        } else {
            res = (s32)0x80000108;
        }
        break;
    case 0x10:
        if ((u32)val < 2u) {
            IMP(0x90) = val;
            sub_009c(0x40);
        } else {
            res = (s32)0x80000108;
        }
        break;
    case 0x40:
        if ((u32)val < 4u) {
            IMP(0xB4) = val;
            IMP(0xD4) = 1;
            sub_0210(0);
        } else {
            res = (s32)0x80000108;
        }
        break;
    case 0x80:
        if ((u32)val < 2u) {
            IMP(0x94) = val;
        } else {
            res = (s32)0x80000108;
        }
        break;
    case 0x100:
        if ((u32)val < 2u) {
            IMP(0x98) = val;
        } else {
            res = (s32)0x80000108;
        }
        break;
    case 0x200:
        if ((u32)val < 3u) {
            IMP(0x9C) = val;
            sub_009c(0x4000);
        } else {
            res = (s32)0x80000108;
        }
        break;
    case 0x2000:
        IMP(0x368) = val;
        break;
    case 0x80000001:
        IMP(0x64) = val;
        break;
    case 0x80000002:
        IMP(0x68) = val;
        break;
    case 0x80000003:
        IMP(0x48) = val;
        break;
    case 0x80000004:
        IMP(0x4C) = val;
        break;
    case 0x80000005:
        IMP(0x50) = val;
        break;
    case 0x80000006:
        IMP(0x6C) = val;
        sub_191c(IMP(0x98));
        break;
    case 0x80000007:
        IMP(0x70) = val;
        break;
    case 0x80000008:
        IMP(0x78) = val;
        break;
    case 0x80000009:
        IMP(0x134) = val;
        break;
    case 0x8000000A:
        IMP(0x138) = val;
        if ((IMPU(0x110) & 4u) != 0) {
            sceDisplay_driver_7DF044BA(val);
        }
        break;
    case 0x8000000B:
        IMP(0x13C) = val;
        break;
    default:
        res = (s32)0x80000107;
        break;
    }

    sceKernelCpuResumeIntr(intr);
    return res;
}

s32 sceImpose_driver_3C318569(s32 id, s32 val) __attribute__((alias("sceImpose_FCD44963")));

s32 sceImpose_8C943191(s32 *is_charging, s32 *icon_status)
{
    s32 intr = sceKernelCpuSuspendIntr();
    if (is_charging != NULL) {
        *is_charging = IMP(0x5C);
    }
    if (icon_status != NULL) {
        s32 lvl = 3;
        if (IMP(0x44) == 0) {
            s32 pct = IMP(0x40);
            s32 i;
            lvl = 0;
            for (i = 0; i < 3; i++) {
                if (pct >= g_battery_thresholds[i]) {
                    break;
                }
                lvl++;
            }
        }
        *icon_status = lvl;
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

s32 sceImpose_driver_FF2D7FD2(s32 *is_charging, s32 *icon_status) __attribute__((alias("sceImpose_8C943191")));

s32 sceImpose_381BD9E7(s32 a0)
{
    s32 intr = sceKernelCpuSuspendIntr();
    s32 res = (s32)0x80000021;
    if ((IMPU(0x74) & 1u) == 0) {
        res = sub_160c(a0);
    }
    sceKernelCpuResumeIntr(intr);
    return res;
}

s32 sceImpose_9DBCE0C4(s32 val)
{
    s32 intr = sceKernelCpuSuspendIntr();
    s32 prev = IMP(0x88);
    IMP(0x88) = val;
    sceKernelCpuResumeIntr(intr);
    return prev;
}

s32 sceImpose_driver_BCF1D254(s32 val) __attribute__((alias("sceImpose_9DBCE0C4")));

s32 sceImpose_7084E72C(void)
{
    return IMP(0x88);
}

s32 sceImpose_9BA61B49(void)
{
    return IMP(0x8C);
}

s32 sceImpose_A9884B00(s32 a0)
{
    if (IMP(0x110) == 0) {
        return (s32)0x80000004;
    }
    sub_191c(a0);
    if (g_tvout_mode == NULL) {
        return (s32)0x80000104;
    }
    s32 mode = ((IMPU(0x110) & 0x200u) != 0) ? g_tvout_mode->mode1 : g_tvout_mode->mode0;
    return sub_173c(1, mode, g_tvout_mode->width, g_tvout_mode->height);
}

s32 sceImpose_BB3F5DEC(void)
{
    if ((IMPU(0x110) & 0x00010000u) != 0 && IMP(0x10C) != 0x200) {
        return 0;
    }
    return (s32)0x80000004;
}

s32 sceImpose_FF1A2F07(void)
{
    if ((IMPU(0x110) & 0x00020000u) != 0 && IMP(0x10C) != 0x200) {
        return (IMP(0x31C) != 0) ? 0 : -1;
    }
    return (s32)0x80000004;
}

s32 sceImpose_driver_CE16DF6D(void) __attribute__((alias("sceImpose_FF1A2F07")));

/* =========================================================================
 * Exported Functions: sceImpose_driver
 * ========================================================================= */

s32 sceImpose_driver_0F067E16(void)
{
    s32 v = IMP(0xBC);
    IMP(0xBC) = 0;
    return v;
}

s32 sceImpose_driver_133DB99A(void)
{
    return 0;
}

s32 sceImpose_driver_139500A7(void)
{
    return 0;
}

s32 sceImpose_driver_28B97035(s32 a0, s32 a1, s32 a2)
{
    if ((IMPU(0x110) & 0x200u) == 0) {
        return (s32)0x80000004;
    }
    if (a0 != 0) {
        sub_173c(1, a0, a1, a2);
        return 0;
    }
    IMP(0x9C) = 0;
    sub_187c();
    if (IMP(0x124) != 0x10) {
        sub_009c(0x40004020u);
    }
    return 0;
}

s32 sceImpose_driver_686B040E(void)
{
    if (IMP(0xE4) != 0) {
        if ((IMPU(0xA4) & 4u) == 0 && (IMPU(0xA4) & 0x200u) == 0) {
            sceImpose_driver_BB12F974(0x40202);
        }
        IMP(0x17C) = (s32)sceKernelGetSystemTimeLow();
    }
    return 0;
}

s32 sceImpose_driver_9E921836(void)
{
    IMP(0xC0) = 1;
    IMP(0xE4) = 0;
    IMP(0xBC) = 0;
    sceDisplayEnable();
    IMP(0xE8) = 0;
    IMP(0xC4) = 0;
    IMP(0xCC) = 0;
    if (IMP(0xD4) != 0) {
        sub_2c00();
        sub_0210(0);
        sub_3380(2, 1);
        IMP(0xF8) = 0;
        IMPU(0xBC) |= 0x20000000u;
    }
    sub_055c(1);
    IMP(0x70) = 0;
    sub_3380(8, 0);
    sceKernelEnableSubIntr(0x1E, 0x14);
    return 0;
}

s32 sceImpose_driver_AABEE36A(void)
{
    if (IMP(0xA4) >= 0) {
        IMP(0xE0) = 1;
        sub_009c(8);
        while (IMP(0xE0) != 0) {
            sceKernelDelayThread(100000);
        }
    }
    sceKernelUnregisterSysEventHandler(&g_sysevent_handler);
    sceKernelSetEventFlag(IMP(0x08), 0x80000000u);
    sceKernelWaitThreadEnd(IMP(0x0C), NULL);
    sceKernelDeleteThread(IMP(0x0C));
    sceKernelDeleteEventFlag(IMP(0x08));
    sceKernelReleaseSubIntrHandler(0x1E, 0x14);
    return 0;
}

s32 sceImpose_driver_AF5DC020(s32 a0, void *a1)
{
    (void)a0;
    if (a1 == NULL) {
        return (s32)0x80000103;
    }
    return (s32)0x80000107;
}

s32 sceImpose_driver_B3EAE8F0(void)
{
    return IMP(0xA4);
}

s32 sceImpose_driver_B497314D(s32 id, u32 *out)
{
    if (out == NULL) {
        return (s32)0x80000103;
    }
    if (id != 0x4000) {
        return (s32)0x80000107;
    }
    s32 intr = sceKernelCpuSuspendIntr();
    out[0] = IMPU(0x34C);
    out[1] = IMPU(0x350);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

s32 sceImpose_driver_BB12F974(s32 a0)
{
    s32 ev = 0;

    if (a0 != 0 && (a0 & 1) == 0) {
        if ((a0 & 4) != 0) {
            ev = IMP(0xA4) & 2;
            if (IMP(0x14) != 0) {
                ev |= 0x80;
            }
            sub_3380(1, 1);
            if (IMP(0x110) != 0 && IMP(0x328) != 0) {
                sub_3380(4, 0);
            }
            sub_3380(2, 1);
            IMP(0x90) = 1;
        } else if ((a0 & 2) != 0) {
            if (sub_169c() != 0) {
                return (s32)0x80000105;
            }
            if ((IMPU(0xA4) < 2u)) {
                ev = ((a0 & 0x200) != 0) ? 0x101 : 1;
            } else {
                if ((a0 & 0x200) != 0 || (a0 & 0x800) != 0) {
                    sub_0590(0xFF);
                    ev = 0x100;
                } else if ((a0 & 0x400) != 0) {
                    sub_0590(0xFF);
                } else if ((IMPU(0xA4) & 0x00040000u) != 0) {
                    return (s32)0x80000105;
                }
            }
            if (IMP(0x310) != 0) {
                ev |= 0x200;
            }
            IMP(0x154) = 0;
            IMP(0x164) = 0;
            IMP(0x150) = 0;
        }
    } else {
        if ((IMP(0xA4) & 2) != 0) {
            ev = 2;
        }
    }

    IMP(0xA4) = a0;
    if (ev != 0) {
        sub_009c((u32)ev);
    }
    return 0;
}

s32 sceImpose_driver_CBF27B10(void)
{
    return 0;
}

s32 sceImpose_driver_D067235E(void)
{
    void *fb = NULL;
    s32 devctl_out = 0;

    memset(g_impose_state, 0, sizeof(g_impose_state));
    g_impose = g_impose_state;

    IMP(0x108) = sceKernelGetModel();
    IMP(0x10C) = sceKernelApplicationType();
    if (sceKernelPartitionTotalMemSize(8) != 0) {
        IMPU(0x110) |= 0x100u;
    }
    if (sceDisplay_driver_996881D2() >= 0) {
        IMPU(0x110) |= 4u;
    }
    sceDisplayGetMode((s32 *)IMP_PTR(0x32C), (s32 *)IMP_PTR(0x330), (s32 *)IMP_PTR(0x334));
    if (sceHprm_driver_9BDE174B() >= 0) {
        IMPU(0x114) |= 2u;
    }
    IMP(0x60) = scePower_driver_8C873AA7();
    IMP(0x40) = scePower_driver_31AEA94C();
    IMP(0x58) = scePower_driver_872F4ECE();
    IMP(0x5C) = scePower_driver_7A9EA6DE();

    sub_4ba0(-1);

    IMP(0x140) = sceHprm_driver_370DDC12();
    if ((IMPU(0x114) & 2u) != 0) {
        IMP(0x144) = sceHprm_driver_1C3525DD();
    }
    IMP(0x128) = sceHprm_driver_FA4A25A7();
    IMP(0x12C) = sceHprm_driver_EFCFD0C5();
    IMP(0x124) = sub_0520();
    IMP(0x120) = sceKernelBootFrom();

    IMP(0x0C) = sceKernelCreateThread("SceImpose", sub_3968, 0x14, 0x800, 0x00100000u, NULL);
    if (IMP(0x0C) < 0) {
        return IMP(0x0C);
    }

    IMP(0x10) = sceKernelCreateFpl("SceImpose_FrameBuffer", 1, 0, 0x44000, 1, NULL);
    if (IMP(0x10) >= 0) {
        sceKernelTryAllocateFpl(IMP(0x10), &fb);
        IMPU(0x14) = (u32)fb;
        IMPU(0x1B0) = (u32)fb;
    }

    sub_29bc();

    IMP(0x08) = sceKernelCreateEventFlag("SceImpose", 1, 0, NULL);
    if (IMP(0x08) < 0) {
        return IMP(0x08);
    }

    IMPU(0x1B4) = (u32)sub_3f54;
    IMPU(0x1B8) = (u32)sub_4334;
    IMPU(0x1BC) = (u32)sub_441c;
    IMPU(0x1C0) = (u32)sub_452c;

    sceCtrlSetSpecialButtonCallback(2, 0x01C30000u, sub_317c, 0);
    sceCtrlSetSpecialButtonCallback(3, 0x00320000u, sub_3308, 0);

    sceKernelStartThread(IMP(0x0C), 0, NULL);
    sub_3380(8, 0);
    sceKernelRegisterSysEventHandler(&g_sysevent_handler);

    if (IMPU(0x1B0) != 0) {
        sceKernelMemset32((void *)IMPU(0x1B0), (s32)0x9CE79CE7u, 0x44000);
    }

    sceKernelRegisterSubIntrHandler(0x1E, 0x14, sub_3388, NULL);
    sceKernelEnableSubIntr(0x1E, 0x14);
    sceKermit_driver_5280B410(9, sub_520c);

    if (sceIoDevctl("fatms0:", 0x02425856u, NULL, 0, &devctl_out, 4) >= 0 && devctl_out != 0) {
        IMPU(0x114) |= 1u;
    }
    if (sceKernelInitDiscImage() != NULL || sceKernelIsDVDMode() != 0) {
        IMP(0x11C) = 1;
    }

    sub_49f4(0, 0, 0, 0);
    sub_009c(0x4000);
    sceKernelSetInitCallback(sub_660c, 3, NULL);
    return 0;
}

s32 sceImpose_driver_D3C10F9B(void)
{
    return 0;
}

s32 sceImpose_driver_D7B9C4C8(void)
{
    return 0;
}

s32 sceImpose_driver_E9B0E008(s32 *out)
{
    s32 intr = sceKernelCpuSuspendIntr();
    if ((IMPU(0x110) & 0x02000000u) == 0) {
        sceKernelCpuResumeIntr(intr);
        return (s32)0x80000004;
    }
    if (out != NULL) {
        *out = 0;
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* =========================================================================
 * Exported Functions: sceAudioRouting
 * ========================================================================= */

s32 sceAudioRoutingSetMode(s32 mode)
{
    return sub_6638(mode);
}

s32 sceAudioRoutingGetMode(void)
{
    return IMP(0x54);
}

s32 sceAudioRoutingSetVolumeMode(s32 mode)
{
    return sub_66d8(mode);
}

s32 sceAudioRoutingGetVolumeMode(void)
{
    return IMP(0x7C);
}

/* =========================================================================
 * Module Lifecycle
 * ========================================================================= */

s32 module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    return (sceImpose_driver_D067235E() < 0) ? 1 : 0;
}

s32 module_reboot_phase(s32 phase, void *argp)
{
    (void)argp;
    if (phase == 2) {
        if (IMP(0xA4) < 0) {
            sceDisplaySetHoldMode(0);
        } else if ((IMPU(0xA4) & 2u) != 0) {
            sceImpose_driver_BB12F974(1);
        }
        sceImpose_driver_AABEE36A();
    }
    return 0;
}

s32 module_reboot_before(void *arg0)
{
    if (IMP(0x340) != 0 && IMP(0x344) == 4 && arg0 != NULL) {
        u32 p = *(u32 *)((u8 *)arg0 + 0x14);
        if (p != 0) {
            *(s32 *)(p + 0x10) = (s32)0x80020152;
        }
    }
    return 0;
}
