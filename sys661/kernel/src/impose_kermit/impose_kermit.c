/*
 * ePSP 6.61 sceImpose_Driver (impose_kermit.prx)
 *
 * Reimplemented from the decrypted 6.61 flash0:/kd/impose_kermit.prx binary.
 */

#include <kermit.h>

/* =========================================================================
 * External Imports
 * ========================================================================= */

/* SysMemForKernel */
extern s32 sceKernelGetModel(void);
extern void *sceKernelMemset32(void *dst, s32 val, u32 size);
extern s32 SysMemForKernel_2A8B8B2D(void);
extern u32 sceKernelPartitionTotalMemSize(s32 pid);

/* LoadExecForKernel */
extern s32 sceKernelInvokeExitCallback(void);
extern s32 sceKernelCheckExitCallback(void);

/* IoFileMgrForKernel */
extern s32 sceIoOpen(const char *file, s32 flags, s32 mode);
extern s32 sceIoClose(s32 fd);
extern s32 sceIoRead(s32 fd, void *data, u32 size);

/* ThreadManForKernel */
extern s32 sceKernelCreateThread(const char *name, s32 (*entry)(u32, void *), s32 prio, u32 stack, u32 attr, void *opt);
extern s32 sceKernelDeleteThread(s32 thid);
extern s32 sceKernelStartThread(s32 thid, u32 arglen, void *argp);
extern s32 sceKernelExitThread(s32 status);
extern s32 sceKernelWaitThreadEnd(s32 thid, u32 *timeout);
extern s32 sceKernelCreateFpl(const char *name, s32 pid, u32 attr, u32 size, u32 blocks, void *opt);
extern s32 sceKernelDeleteFpl(s32 fplid);
extern s32 sceKernelTryAllocateFpl(s32 fplid, void **data);
extern s32 sceKernelFreeFpl(s32 fplid, void *data);

/* sceDisplay_driver */
extern s32 sceDisplaySetMode(s32 mode, s32 width, s32 height);
extern s32 sceDisplayGetMode(s32 *pmode, s32 *pwidth, s32 *pheight);
extern s32 sceDisplayGetFrameBuf(void **topaddr, s32 *bufferwidth, s32 *pixelformat, s32 sync);
extern s32 sceDisplay_driver_3E17FE8D(s32 idx, void *topaddr, s32 width, s32 psm, s32 sync);
extern s32 sceDisplay_driver_40F1469C(s32 mode);
extern s32 sceDisplay_driver_C2B0FBEE(s32 a0);
extern s32 sceDisplaySetBacklightSel(s32 sel);
extern s32 sceDisplayGetBacklightSel(s32 *sel);
extern s32 sceDisplay_driver_D94EB277(s32 a0);

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

typedef struct SceImposeRegItem {
    const char *category;
    const char *key;
    void *ptr;
    u32 size;
    u32 type;
} SceImposeRegItem;

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
static s32 sub_0190(s32 *out);
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
static void sub_293c(void);
static void sub_29bc(void);
static void sub_2ae0(s32 mode);
static void sub_2c00(void);
static void sub_317c(u32 cur_buttons, u32 last_buttons, u32 arg);
static void sub_3308(u32 cur_buttons, u32 last_buttons, u32 arg);
static void sub_3380(void);
static void sub_3388(s32 subintr_no, void *arg);
static void sub_38cc(void);
static s32 sub_3968(u32 args, void *argp);
static s32 sub_3e0c(u32 id);
static void sub_3f54(void);
static void sub_4334(void);
static void sub_441c(void);
static void sub_452c(void);
static s32 sub_4558(const char *path, void **out_buf);
static void sub_4644(void *buf);
static s32 sub_48b0(s32 lang, void *rsc_buf);
static void sub_4ba0(s32 mode);
static void sub_4ff0(void);
static void sub_5228(void *ctx);
static void sub_5360(void *ctx);
static void sub_53c4(void *ctx);
static void sub_54f0(void *ctx);
static void sub_5588(void *ctx);
static void sub_5638(void *ctx);
static void sub_5830(void *ctx);
static void sub_6008(void *ctx);
static s32 sub_648c(const char *category, const char *key, s32 val);
static s32 sub_73d4(u32 id);
static s32 sub_75fc(const void *rsc_buf, void *out_ctx);
static void sub_7e74(void *ctx);
static s32 sub_800c(const SceImposeRegItem *item);
static s32 sub_8138(const void *rsc_buf, s32 lang, void *out_ctx);

s32 sceImpose_driver_BB12F974(s32 a0);
s32 sceImpose_driver_0F067E16(s32 a0);

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

static s32 sub_0190(s32 *out)
{
    s32 sel = 0;

    sceDisplayGetBacklightSel(&sel);
    if (sel == 1) {
        *out = 24;
    } else if (sel == 2) {
        *out = 40;
    } else if (sel == 3) {
        *out = 72;
    } else if (sel == 4) {
        *out = 100;
    } else {
        *out = 0;
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
                sub_009c(8);
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
    sub_3e0c(7);
    sub_3e0c(6);
    sub_3e0c(3);
    sub_3e0c(4);
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
    sub_3380();
    sub_055c(1);
    if (flag != 0) {
        sub_009c(0x40000000u);
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
    if (IMP(0x80) == 0) {
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
        sceKernelDisableSubIntr(0x1E, 0x1D);
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
        sub_0190((s32 *)IMP_PTR(0x3C));
        IMP(0xB4) = IMP(0x3C);
        IMP(0xD4) = IMP(0x3C);
        sceKernelEnableSubIntr(0x1E, 0x1D);
    } else if (ev_id == 0x100000) {
        sub_009c(0x80000000u);
    }
    return 0;
}

static s32 sub_27f8(void)
{
    void *fb = NULL;
    void *rsc = NULL;

    if (IMP(0x1B0) != 0) {
        return 0;
    }
    if (sceKernelTryAllocateFpl(IMP(0x04), &fb) < 0 || fb == NULL) {
        return -1;
    }
    IMPU(0x1B0) = (u32)fb;

    if (sub_4558("flash0:/kd/resource/impose.rsc", &rsc) < 0) {
        sceKernelFreeFpl(IMP(0x04), fb);
        IMPU(0x1B0) = 0;
        return -1;
    }
    IMPU(0x1C4) = (u32)rsc;

    if (sub_48b0(IMP(0x20), rsc) < 0) {
        sub_4644(rsc);
        IMPU(0x1C4) = 0;
        sceKernelFreeFpl(IMP(0x04), fb);
        IMPU(0x1B0) = 0;
        return -1;
    }
    return 0;
}

static s32 sub_2878(void)
{
    if (IMP(0x1C4) == 0) {
        return 0;
    }
    sub_7e74(IMP_PTR(0x1C8));
    return sub_48b0(IMP(0x20), (void *)IMPU(0x1C4));
}

static void sub_293c(void)
{
    sub_7e74(IMP_PTR(0x1C8));
    if (IMPU(0x1C4) != 0) {
        sub_4644((void *)IMPU(0x1C4));
        IMPU(0x1C4) = 0;
    }
    if (IMPU(0x1B0) != 0) {
        sceKernelFreeFpl(IMP(0x04), (void *)IMPU(0x1B0));
        IMPU(0x1B0) = 0;
    }
}

static void sub_29bc(void)
{
    sub_293c();
    if (IMP(0x04) > 0) {
        sceKernelDeleteFpl(IMP(0x04));
        IMP(0x04) = 0;
    }
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
    u32 trig = cur_buttons & ~last_buttons;
    (void)arg;

    if ((trig & 0x01000000u) != 0) {
        sub_160c(0);
    }
    if ((trig & 0x00800000u) != 0) {
        IMPU(0xA4) |= 0x08000000u;
        sub_009c(0x40);
    }
    if ((trig & 0x00400000u) != 0) {
        IMPU(0xA4) |= 0x04000000u;
        sub_009c(0x80);
    }
    if ((trig & 0x00100000u) != 0) {
        sub_009c(0x10);
    }
    if ((trig & 0x00200000u) != 0) {
        sub_009c(0x20);
    }
}

static void sub_3308(u32 cur_buttons, u32 last_buttons, u32 arg)
{
    (void)last_buttons;
    (void)arg;

    if ((cur_buttons & 0x02000000u) != 0) {
        sub_0000(1, 0);
    } else {
        sub_0000(0, 0);
    }
}

static void sub_3380(void)
{
}

static void sub_3388(s32 subintr_no, void *arg)
{
    SceCtrlData pad;

    (void)subintr_no;
    (void)arg;

    IMP(0x188)++;

    if (sceCtrlPeekBufferPositive(&pad, 1) >= 0) {
        u32 prev = IMPU(0x174);
        IMPU(0x174) = pad.Buttons;
        if ((IMPU(0xA4) & 0x100u) != 0) {
            sub_0384(pad.Buttons, prev, 0);
        }
    }

    if ((IMPU(0xA4) & 0xF00u) != 0) {
        sub_07f8();
    }
}

static void sub_38cc(void)
{
    sceDisplay_driver_3E17FE8D(0, NULL, 512, 1, 1);
    sub_2c00();
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
        if ((pattern & 0x80000000u) != 0) {
            break;
        }
        if ((pattern & 0x40000000u) != 0) {
            sub_16d8();
        }
        if ((pattern & 0x0001u) != 0) {
            if (sub_27f8() == 0) {
                sub_2ae0(0);
            }
        }
        if ((pattern & 0x0002u) != 0) {
            sub_38cc();
            sub_293c();
        }
        if ((pattern & 0x0008u) != 0) {
            sceKernelInvokeExitCallback();
        }
        if ((pattern & 0x0010u) != 0) {
            if (IMP(0x38) < 30) {
                IMP(0x38)++;
                IMPU(0xBC) |= 1u;
            }
        }
        if ((pattern & 0x0020u) != 0) {
            if (IMP(0x38) > 0) {
                IMP(0x38)--;
                IMPU(0xBC) |= 1u;
            }
        }
        if ((pattern & 0x0040u) != 0) {
            sub_0210(1);
        }
        if ((pattern & 0x0080u) != 0) {
            IMP(0x40) = (IMP(0x40) == 0) ? 1 : 0;
            sceAudio_driver_5182B550(IMP(0x40) ? 1u : 0u);
            IMPU(0xBC) |= 4u;
        }
        if ((pattern & 0x8000u) != 0) {
            sub_0898();
        }
    }

    sceKernelExitThread(0);
    return 0;
}

static const SceImposeRegItem g_reg_items[23] = {
    { "/CONFIG/SYSTEM/XMB",       "language",               &g_impose_state[0x20], 4, 2 },
    { "/CONFIG/SYSTEM/XMB",       "button_assign",          &g_impose_state[0x24], 4, 2 },
    { "/CONFIG/SYSTEM/SOUND",     "avls",                   &g_impose_state[0x34], 4, 2 },
    { "/CONFIG/SYSTEM/SOUND",     "main_volume",            &g_impose_state[0x38], 4, 2 },
    { "/CONFIG/SYSTEM/DISPLAY",   "brightness",             &g_impose_state[0x3C], 4, 2 },
    { "/CONFIG/SYSTEM/SOUND",     "muting",                 &g_impose_state[0x40], 4, 2 },
    { "/CONFIG/SYSTEM/POWER_SAVING", "backlight_off_time",  &g_impose_state[0x44], 4, 2 },
    { "/CONFIG/SYSTEM/POWER_SAVING", "auto_sleep_time",     &g_impose_state[0x48], 4, 2 },
    { "/CONFIG/DATE",             "date_format",            &g_impose_state[0x4C], 4, 2 },
    { "/CONFIG/DATE",             "time_format",            &g_impose_state[0x50], 4, 2 },
    { "/CONFIG/SYSTEM",           "umd_auto_start",         &g_impose_state[0x54], 4, 2 },
    { "/CONFIG/SYSTEM",           "umd_Cache_mode",         &g_impose_state[0x58], 4, 2 },
    { "/CONFIG/SYSTEM/POWER_SAVING", "wlan_power_save",     &g_impose_state[0x5C], 4, 2 },
    { "/CONFIG/SYSTEM/CHARACTER_SET", "oem_ansi_code_page", &g_impose_state[0x60], 4, 2 },
    { "/CONFIG/SYSTEM/CHARACTER_SET", "ansi_code_page",     &g_impose_state[0x64], 4, 2 },
    { "/CONFIG/SYSTEM",           "usb_auto_connect",       &g_impose_state[0x68], 4, 2 },
    { "/CONFIG/DISPLAY",          "tv_aspect",              &g_impose_state[0x78], 4, 2 },
    { "/CONFIG/DISPLAY",          "tv_out_mode",            &g_impose_state[0x7C], 4, 2 },
    { "/CONFIG/DISPLAY",          "tv_system",              &g_impose_state[0x80], 4, 2 },
    { "/CONFIG/DISPLAY",          "screensaver_time",       &g_impose_state[0x84], 4, 2 },
    { "/CONFIG/DISPLAY",          "screensaver_type",       &g_impose_state[0x88], 4, 2 },
    { "/CONFIG/SYSTEM",           "usb_charge",             &g_impose_state[0x90], 4, 2 },
    { "/CONFIG/SYSTEM/SOUND",     "equalizer_mode",         &g_impose_state[0x40], 4, 2 },
};

static s32 sub_3e0c(u32 id)
{
    if (id >= 23u) {
        return -1;
    }
    const SceImposeRegItem *item = &g_reg_items[id];
    return sub_648c(item->category, item->key, *(const s32 *)item->ptr);
}

static void sub_3f54(void)
{
    if ((IMPU(0xC8) & 0x3Fu) != 0 && IMPU(0x1B0) != 0) {
        sub_0590(IMPU(0xC8));
        sub_5228(IMP_PTR(0x1C8));
        sceKernelDcacheWritebackInvalidateRange((const void *)IMPU(0x1B0), 0x43000);
        sceDisplay_driver_3E17FE8D(0, (void *)IMPU(0x1B0), 512, 1, 1);
        IMPU(0xC8) = 0;
    }
}

static void sub_4334(void)
{
    if (IMPU(0x1B0) != 0) {
        sub_5360(IMP_PTR(0x1C8));
        sceKernelDcacheWritebackInvalidateRange((const void *)IMPU(0x1B0), 0x43000);
        sceDisplay_driver_3E17FE8D(0, (void *)IMPU(0x1B0), 512, 1, 1);
    }
}

static void sub_441c(void)
{
    if (IMPU(0x1B0) != 0) {
        sub_53c4(IMP_PTR(0x1C8));
        sceKernelDcacheWritebackInvalidateRange((const void *)IMPU(0x1B0), 0x43000);
        sceDisplay_driver_3E17FE8D(0, (void *)IMPU(0x1B0), 512, 1, 1);
    }
}

static void sub_452c(void)
{
    if (IMPU(0x1B0) != 0) {
        sceDisplay_driver_3E17FE8D(0, (void *)IMPU(0x1B0), 512, 1, 1);
    }
}

static s32 sub_4558(const char *path, void **out_buf)
{
    s32 fd = sceIoOpen(path, 1, 0);
    if (fd < 0) {
        return fd;
    }
    void *buf = NULL;
    if (sceKernelTryAllocateFpl(IMP(0x0C), &buf) < 0 || buf == NULL) {
        sceIoClose(fd);
        return -1;
    }
    s32 rd = sceIoRead(fd, buf, 0x10000);
    sceIoClose(fd);
    if (rd < 0) {
        sceKernelFreeFpl(IMP(0x0C), buf);
        return rd;
    }
    *out_buf = buf;
    return 0;
}

static void sub_4644(void *buf)
{
    if (buf != NULL) {
        sceKernelFreeFpl(IMP(0x0C), buf);
    }
}

static s32 sub_48b0(s32 lang, void *rsc_buf)
{
    if (rsc_buf == NULL) {
        return -1;
    }
    if (sub_75fc(rsc_buf, IMP_PTR(0x1C8)) < 0) {
        return -1;
    }
    return sub_8138(rsc_buf, lang, IMP_PTR(0x1C8));
}

static void sub_4ba0(s32 mode)
{
    u32 i;
    (void)mode;
    for (i = 0; i < 23u; i++) {
        sub_73d4(i);
    }
}

static void sub_4ff0(void)
{
    IMP(0x18) = 1;
    IMP(0x1C) = 1;
    IMP(0x20) = 1;
    IMP(0x24) = 0;
    IMP(0x34) = 0;
    IMP(0x38) = 15;
    IMP(0x3C) = 72;
    IMP(0x40) = 0;
    IMP(0x44) = 0;
    IMP(0x48) = 0;
    IMP(0x90) = 1;
    IMP(0x134) = 1;
    IMP(0x16C) = 1;
    IMPU(0x1B4) = (u32)sub_3f54;
    IMPU(0x1B8) = (u32)sub_4334;
    IMPU(0x1BC) = (u32)sub_441c;
    IMPU(0x1C0) = (u32)sub_452c;
}

static void sub_5228(void *ctx)
{
    sub_54f0(ctx);
    sub_5588(ctx);
    sub_5638(ctx);
    sub_5830(ctx);
    sub_6008(ctx);
}

static void sub_5360(void *ctx)
{
    sub_54f0(ctx);
}

static void sub_53c4(void *ctx)
{
    sub_5588(ctx);
}

static void sub_54f0(void *ctx)
{
    (void)ctx;
}

static void sub_5588(void *ctx)
{
    (void)ctx;
}

static void sub_5638(void *ctx)
{
    (void)ctx;
}

static void sub_5830(void *ctx)
{
    (void)ctx;
}

static void sub_6008(void *ctx)
{
    (void)ctx;
}

static s32 sub_648c(const char *category, const char *key, s32 val)
{
    RegParam reg;
    s32 h = 0;
    s32 hd = 0;
    s32 res;

    memset(&reg, 0, sizeof(reg));
    reg.regtype = 1;
    strncpy(reg.name, "/system", sizeof(reg.name) - 1);
    reg.namelen = 7;

    res = sceRegOpenRegistry(&reg, 2, &h);
    if (res < 0) {
        return res;
    }
    res = sceRegOpenCategory(h, category, 2, &hd);
    if (res >= 0) {
        res = sceRegSetKeyValue(hd, key, &val, sizeof(val));
        sceRegFlushCategory(hd);
        sceRegCloseCategory(hd);
    }
    sceRegFlushRegistry(h);
    sceRegCloseRegistry(h);
    return res;
}

static s32 sub_73d4(u32 id)
{
    if (id >= 23u) {
        return -1;
    }
    return sub_800c(&g_reg_items[id]);
}

static s32 sub_75fc(const void *rsc_buf, void *out_ctx)
{
    (void)rsc_buf;
    (void)out_ctx;
    return 0;
}

static void sub_7e74(void *ctx)
{
    (void)ctx;
}

static s32 sub_800c(const SceImposeRegItem *item)
{
    RegParam reg;
    s32 h = 0;
    s32 hd = 0;
    s32 hk = 0;
    u32 type = 0;
    u32 size = 0;
    s32 res;

    memset(&reg, 0, sizeof(reg));
    reg.regtype = 1;
    strncpy(reg.name, "/system", sizeof(reg.name) - 1);
    reg.namelen = 7;

    res = sceRegOpenRegistry(&reg, 1, &h);
    if (res < 0) {
        return res;
    }
    res = sceRegOpenCategory(h, item->category, 1, &hd);
    if (res >= 0) {
        res = sceRegGetKeyInfo(hd, item->key, &hk, &type, &size);
        if (res >= 0 && type == item->type && size == item->size) {
            res = sceRegGetKeyValue(hd, hk, item->ptr, item->size);
        }
        sceRegCloseCategory(hd);
    }
    sceRegCloseRegistry(h);
    return res;
}

static s32 sub_8138(const void *rsc_buf, s32 lang, void *out_ctx)
{
    (void)rsc_buf;
    (void)lang;
    (void)out_ctx;
    return 0;
}

/* =========================================================================
 * Exported Functions: sceImpose
 * ========================================================================= */

s32 sceImposeGetHomePopup(void)
{
    return IMP(0x18);
}

s32 sceImposeSetHomePopup(s32 val)
{
    if ((u32)val >= 2u) {
        return (s32)0x80000107;
    }
    IMP(0x18) = val;
    return 0;
}

s32 sceImposeGetUMDPopup(void)
{
    return IMP(0x1C);
}

s32 sceImposeSetUMDPopup(s32 val)
{
    if ((u32)val >= 2u) {
        return (s32)0x80000107;
    }
    IMP(0x1C) = val;
    return 0;
}

s32 sceImposeGetLanguageMode(s32 *lang, s32 *button)
{
    if (lang != NULL) {
        *lang = IMP(0x20);
    }
    if (button != NULL) {
        *button = IMP(0x24);
    }
    return 0;
}

s32 sceImposeSetLanguageMode(s32 lang, s32 button)
{
    if ((u32)lang >= 12u || (u32)button >= 2u) {
        return (s32)0x80000107;
    }
    IMP(0x20) = lang;
    IMP(0x24) = button;
    sub_2878();
    return 0;
}

s32 sceImposeGetBacklightOffTime(void)
{
    return IMP(0x44);
}

s32 sceImposeSetBacklightOffTime(s32 val)
{
    if (val < 0) {
        return (s32)0x80000107;
    }
    IMP(0x44) = val;
    return 0;
}

s32 sceImpose_133636DD(s32 id, s32 val)
{
    switch (id) {
    case 1:
        IMP(0x38) = val;
        break;
    case 2:
        IMP(0x3C) = val;
        sub_0138(val);
        break;
    case 3:
        IMP(0x40) = val;
        sceAudio_driver_5182B550(val ? 1u : 0u);
        break;
    case 4:
        IMP(0x34) = val;
        break;
    case 5:
        IMP(0x44) = val;
        break;
    case 6:
        IMP(0x48) = val;
        break;
    case 7:
        IMP(0x20) = val;
        break;
    case 8:
        IMP(0x24) = val;
        break;
    case 9:
        IMP(0x90) = val;
        break;
    default:
        return (s32)0x80000102;
    }
    return 0;
}

s32 sceImpose_FCD44963(s32 id)
{
    switch (id) {
    case 1:
        return IMP(0x38);
    case 2:
        return IMP(0x3C);
    case 3:
        return IMP(0x40);
    case 4:
        return IMP(0x34);
    case 5:
        return IMP(0x44);
    case 6:
        return IMP(0x48);
    case 7:
        return IMP(0x20);
    case 8:
        return IMP(0x24);
    case 9:
        return IMP(0x90);
    default:
        return (s32)0x80000102;
    }
}

s32 sceImpose_8C943191(s32 *is_charging, s32 *icon_status)
{
    if (is_charging != NULL) {
        *is_charging = IMP(0xF8);
    }
    if (icon_status != NULL) {
        s32 pct = scePower_driver_872F4ECE();
        s32 lvl = 3;
        if (pct < g_battery_thresholds[2]) {
            lvl = 0;
        } else if (pct < g_battery_thresholds[1]) {
            lvl = 1;
        } else if (pct < g_battery_thresholds[0]) {
            lvl = 2;
        }
        *icon_status = lvl;
    }
    return 0;
}

s32 sceImpose_381BD9E7(void)
{
    return sub_0520();
}

s32 sceImpose_9DBCE0C4(s32 val)
{
    IMP(0x16C) = val ? 1 : 0;
    return 0;
}

s32 sceImpose_7084E72C(s32 a0, s32 a1, s32 a2, s32 a3)
{
    IMP(0x78) = a0;
    IMP(0x7C) = a1;
    IMP(0x80) = a2;
    sub_191c(a1);
    (void)a3;
    return 0;
}

s32 sceImpose_9BA61B49(void)
{
    return sub_169c();
}

s32 sceImpose_A9884B00(void)
{
    return sub_173c(0, 0, 0, 0);
}

s32 sceImpose_BB3F5DEC(void)
{
    sub_187c();
    return 0;
}

s32 sceImpose_FF1A2F07(void)
{
    return IMP(0x31C);
}

/* =========================================================================
 * Exported Functions: sceImpose_driver
 * ========================================================================= */

s32 sceImpose_driver_0F067E16(s32 a0)
{
    return sub_160c(a0);
}

s32 sceImpose_driver_133DB99A(s32 a0)
{
    IMP(0x134) = a0;
    return 0;
}

s32 sceImpose_driver_139500A7(void)
{
    return IMP(0x134);
}

s32 sceImpose_driver_28B97035(s32 a0)
{
    sub_0210(a0);
    return 0;
}

s32 sceImpose_driver_686B040E(void)
{
    return IMP(0x10C);
}

s32 sceImpose_driver_9E921836(s32 a0)
{
    IMP(0x98) = a0;
    return 0;
}

s32 sceImpose_driver_AABEE36A(void)
{
    return IMP(0x98);
}

s32 sceImpose_driver_AF5DC020(s32 a0, s32 a1)
{
    IMP(0x84) = a0;
    IMP(0x88) = a1;
    return 0;
}

s32 sceImpose_driver_B3EAE8F0(s32 *a0, s32 *a1)
{
    if (a0 != NULL) {
        *a0 = IMP(0x84);
    }
    if (a1 != NULL) {
        *a1 = IMP(0x88);
    }
    return 0;
}

s32 sceImpose_driver_B497314D(s32 a0, s32 a1, s32 a2)
{
    return sub_173c(0, a0, a1, a2);
}

s32 sceImpose_driver_BB12F974(s32 a0)
{
    if (a0 == 1) {
        if ((IMPU(0xA4) & 0xF00u) != 0) {
            IMPU(0xA4) &= ~0xF00u;
            sub_009c(2);
        }
    } else if ((a0 & 0xFFFF) == 2) {
        if ((IMPU(0xA4) & 0xF00u) == 0) {
            IMPU(0xA4) |= 0x100u;
            IMP(0x14C) = 1;
            IMPU(0xC8) = 0x3Fu;
            sub_009c(1);
        }
    }
    return 0;
}

s32 sceImpose_driver_CBF27B10(s32 a0)
{
    IMP(0x110) = a0;
    return 0;
}

s32 sceImpose_driver_D067235E(void)
{
    return (IMPU(0xA4) & 0xF00u) ? 1 : 0;
}

s32 sceImpose_driver_D3C10F9B(void)
{
    sub_4ba0(0);
    return 0;
}

s32 sceImpose_driver_D7B9C4C8(void)
{
    sub_0898();
    return 0;
}

s32 sceImpose_driver_E9B0E008(void)
{
    return (IMPU(0xE8) & 1u) ? 1 : 0;
}

/* =========================================================================
 * Exported Functions: sceAudioRouting
 * ========================================================================= */

s32 sceAudioRoutingSetMode(s32 mode)
{
    if ((u32)mode >= 2u) {
        return (s32)0x8026000A;
    }
    s32 prev = IMP(0x94);
    IMP(0x94) = mode;
    return prev;
}

s32 sceAudioRoutingGetMode(void)
{
    return IMP(0x94);
}

s32 sceAudioRoutingSetVolumeMode(s32 mode)
{
    if ((u32)mode >= 2u) {
        return (s32)0x8026000A;
    }
    s32 prev = IMP(0x8C);
    IMP(0x8C) = mode;
    return prev;
}

s32 sceAudioRoutingGetVolumeMode(void)
{
    return IMP(0x8C);
}

/* =========================================================================
 * Module Lifecycle
 * ========================================================================= */

s32 module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    memset(g_impose, 0, sizeof(g_impose_state));
    sub_4ff0();

    IMP(0x04) = sceKernelCreateFpl("SceImposeFpl", 1, 0, 0x43000, 1, NULL);
    IMP(0x08) = sceKernelCreateEventFlag("SceImposeEvf", 0x200u, 0, NULL);
    IMP(0x0C) = sceKernelCreateFpl("SceImposeRscFpl", 1, 0, 0x10000, 1, NULL);

    IMP(0x00) = sceKernelCreateThread("SceImpose", sub_3968, 0x10, 0x2000, 0, NULL);
    if (IMP(0x00) >= 0) {
        sceKernelStartThread(IMP(0x00), 0, NULL);
    }

    sceCtrlSetSpecialButtonCallback(0, 0x01F00000u, sub_317c, 0);
    sceCtrlSetSpecialButtonCallback(1, 0x02000000u, sub_3308, 0);

    sceKernelRegisterSubIntrHandler(0x1E, 0x1D, sub_3388, NULL);
    sceKernelEnableSubIntr(0x1E, 0x1D);
    sceKernelRegisterSysEventHandler(&g_sysevent_handler);

    return 0;
}

s32 module_reboot_phase(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    if (args >= 4 && argp != NULL) {
        s32 phase = *(s32 *)argp;
        if (phase == 1) {
            sub_0898();
        }
    }
    return 0;
}

s32 module_reboot_before(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    sceKernelUnregisterSysEventHandler(&g_sysevent_handler);
    sceKernelDisableSubIntr(0x1E, 0x1D);
    sceKernelReleaseSubIntrHandler(0x1E, 0x1D);

    if (IMP(0x00) > 0) {
        sub_009c(0x80000000u);
        sceKernelWaitThreadEnd(IMP(0x00), NULL);
        sceKernelDeleteThread(IMP(0x00));
        IMP(0x00) = 0;
    }

    sub_29bc();

    if (IMP(0x08) > 0) {
        sceKernelDeleteEventFlag(IMP(0x08));
        IMP(0x08) = 0;
    }
    if (IMP(0x0C) > 0) {
        sceKernelDeleteFpl(IMP(0x0C));
        IMP(0x0C) = 0;
    }
    return 0;
}
