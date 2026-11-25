#include <common_imp.h>

typedef struct SircsData {
    u8 type;
    u8 cmd;
    u16 dev;
} SircsData;

typedef struct SircsContext {
    int use_gpio;       /* 0x00 (0x2854) */
    int power_locked;   /* 0x04 (0x2858) */
    u32 tx_total;       /* 0x08 (0x285c) */
    u32 tx_sent;        /* 0x0c (0x2860) */
    u32 *tx_buf;        /* 0x10 (0x2864) */
    u32 rx_total;       /* 0x14 (0x2868) */
    u32 rx_rcvd;        /* 0x18 (0x286c) */
    u32 *rx_buf;        /* 0x1c (0x2870) */
    SceUID sema_id;     /* 0x20 (0x2874) */
    SceUID evf_id;      /* 0x24 (0x2878) */
    int busy;           /* 0x28 (0x287c) */
} SircsContext;

typedef struct IrdaContext {
    int use_gpio;       /* 0x00 (0x2800) */
    int open_count;     /* 0x04 (0x2804) */
    int sircs_lock;     /* 0x08 (0x2808) */
    int (*sircs_cb)(void); /* 0x0c (0x280c) */
    int baudrate;       /* 0x10 (0x2810) */
    u32 alarm_usec;     /* 0x14 (0x2814) */
    int mode;           /* 0x18 (0x2818) */
    int power_locked;   /* 0x1c (0x281c) */
    int tx_count;       /* 0x20 (0x2820) */
    int tx_rd;          /* 0x24 (0x2824) */
    int tx_wr;          /* 0x28 (0x2828) */
    int rx_count;       /* 0x2c (0x282c) */
    int rx_rd;          /* 0x30 (0x2830) */
    int rx_wr;          /* 0x34 (0x2834) */
    SceUID fpl_id;      /* 0x38 (0x2838) */
    int tx_buf_size;    /* 0x3c (0x283c) */
    int rx_buf_size;    /* 0x40 (0x2840) */
    u8 *tx_buf;         /* 0x44 (0x2844) */
    u8 *rx_buf;         /* 0x48 (0x2848) */
    SceUID evf_id;      /* 0x4c (0x284c) */
    SceUID alarm_id;    /* 0x50 (0x2850) */
} IrdaContext;

extern int sceKernelCpuSuspendIntr(void);
extern int sceKernelCpuResumeIntr(int intr);
extern int sceKernelEnableIntr(int intr);
extern int sceKernelDisableIntr(int intr);
extern int sceKernelRegisterIntrHandler(int intr, int no, void *handler, void *common, void *opt);
extern int sceKernelReleaseIntrHandler(int intr);
extern int sceKernelIsIntrContext(void);
extern int sceIoAddDrv(void *drv);
extern int sceIoDelDrv(const char *name);
extern int sceKernelSetEventFlag(SceUID evf, u32 bits);
extern int sceKernelClearEventFlag(SceUID evf, u32 bits);
extern int sceKernelWaitEventFlag(SceUID evf, u32 bits, int wait, u32 *outBits, SceUInt *timeout);
extern SceUID sceKernelCreateEventFlag(const char *name, int attr, int bits, void *opt);
extern int sceKernelDeleteEventFlag(SceUID evf);
extern SceUID sceKernelCreateSema(const char *name, int attr, int initVal, int maxVal, void *opt);
extern int sceKernelDeleteSema(SceUID sema);
extern int sceKernelSignalSema(SceUID sema, int signal);
extern int sceKernelWaitSema(SceUID sema, int signal, SceUInt *timeout);
extern u32 sceKernelGetSystemTimeLow(void);
extern int sceKernelSysClock2USec(void *clock, u32 *sec, u32 *usec);
extern int sceKernelDelayThread(SceUInt delay);
extern SceUID sceKernelCreateFpl(const char *name, int mpid, int attr, int blocksize, int blocks, void *opt);
extern int sceKernelTryAllocateFpl(SceUID uid, void **data);
extern int sceKernelDeleteFpl(SceUID uid);
extern SceUID sceKernelSetAlarm(SceUInt clock, void *handler, void *common);
extern int sceKernelCancelAlarm(SceUID alarmid);
extern int sceGpioPortClear(u32 mask);
extern int sceGpioPortSet(u32 mask);
extern int sceGpioSetPortMode(int port, int mode);
extern int sceKernelPowerLock(int lockType);
extern int sceKernelPowerUnlock(int lockType);
extern int sceKernelRegisterSysEventHandler(void *ev);
extern int sceKernelUnregisterSysEventHandler(void *ev);
extern int sceSysregSircsClkDisable(void);
extern int sceSysregSircsIoDisable(void);
extern int sceSysregSircsIoEnable(void);
extern int sceSysregSircsClkEnable(void);
extern int sceSysregUartClkEnable(int uart);
extern int sceSysregUartIoEnable(int uart);
extern int sceSysregUartIoDisable(int uart);
extern int sceSysregGetTachyonVersion(void);

static IrdaContext g_irda;
static SircsContext g_sircs;

static int sircsIntrHandler(int intr, void *arg);
static void sircsPowerLock(void);
static void sircsPowerUnlock(void);
static int sircsIsBusy(void);
static u32 sircsEncodePulses(SircsData *sd, u32 *out);
static int sircsDecodePulses(SircsData *sd, u32 *in, u32 count);

static int irdaIoIoctl(void *arg, int cmd, void *indata, int inlen, void *outdata, int outlen);
static int irdaIoDevctl(void *arg, const char *devname, int cmd, void *indata, int inlen, void *outdata, int outlen);
static int irdaSetBaudrate(int baud);
static int irdaGetRxCount(u32 *out);
static int irdaWaitRxReady(void);
static int irdaFlushRx(void);
static int irdaWaitTxDone(void);
static int irdaGetTxFree(u32 *out);
static int irdaFlushTx(void);
static int irdaHwEnable(void);
static int irdaHwDisable(void);
static int irdaAllocResources(void);
static void irdaFreeResources(void);
static int irdaStartTxAlarm(IrdaContext *ctx);
static void irdaPowerLock(void);
static void irdaPowerUnlock(void);
static void irdaEnableTxHardware(void);
static void irdaDisableTxHardware(void);
static int irdaIsLocked(void);
static void irdaSetSircsBusyCallback(int (*cb)(void));
static int irdaAddDriver(void);
static int irdaDelDriver(void);
static int irdaIoInit(void *arg);
static int irdaIoExit(void *arg);
static int irdaIoOpen(void *arg, char *file, int flags, int mode);
static int irdaIoClose(void *arg);
static int irdaIoRead(void *arg, char *data, int len);
static int irdaIoWrite(void *arg, const char *data, int len);
static int irdaSysEventHandler(int ev_id, char *ev_name, void *param, int *result);
static int irdaOpen(void);
static int irdaClose(void);
static int irdaRead(u8 *buf, u32 len);
static int irdaWrite(const u8 *buf, u32 len);
static int irdaIntrHandler(int intr, IrdaContext *ctx);
static SceUInt irdaTxAlarmHandler(IrdaContext *ctx);
static int irdaDrainRxFifo(IrdaContext *ctx);

static const void * const g_irda_funcs[22] = {
    irdaIoInit,
    irdaIoExit,
    irdaIoOpen,
    irdaIoClose,
    irdaIoRead,
    irdaIoWrite,
    0,
    irdaIoIoctl,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    irdaIoDevctl,
    0
};

static const struct {
    const char *name;
    u32 dev_type;
    u32 version;
    const char *desc;
    const void *funcs;
} g_irda_drv = {
    "irda",
    1,
    0,
    0,
    g_irda_funcs
};

static struct {
    int size;
    const char *name;
    int type_mask;
    int (*handler)(int ev_id, char *ev_name, void *param, int *result);
    int gpVal;
    int busy;
    void *next;
    int reserved[9];
} g_sysevent = {
    0x40,
    "SceIrda",
    0x00ffff00,
    irdaSysEventHandler,
    0, 0, 0, { 0 }
};

int sceSircs_driver_62411801(void)
{
    int ret;

    g_sircs.use_gpio = 0;
    g_sircs.power_locked = 0;
    g_sircs.tx_total = 0;
    g_sircs.tx_sent = 0;
    g_sircs.tx_buf = 0;
    g_sircs.rx_total = 0;
    g_sircs.rx_rcvd = 0;
    g_sircs.rx_buf = 0;
    g_sircs.sema_id = 0;
    g_sircs.evf_id = 0;
    g_sircs.busy = 0;

    if (sceSysregGetTachyonVersion() <= 0x4fffff) {
        g_sircs.use_gpio = 1;
        sceGpioSetPortMode(2, 0);
    }

    ret = sceKernelRegisterIntrHandler(14, 2, sircsIntrHandler, 0, 0);
    if (ret < 0)
        return ret;

    ret = sceKernelCreateSema("SceSIRCS", 1, 1, 1, 0);
    if (ret < 0) {
        sceKernelReleaseIntrHandler(14);
        return ret;
    }
    g_sircs.sema_id = ret;

    ret = sceKernelCreateEventFlag("SceSIRCS", 1, 0, 0);
    if (ret < 0) {
        sceKernelDeleteSema(g_sircs.sema_id);
        sceKernelReleaseIntrHandler(14);
        return ret;
    }

    g_sircs.evf_id = ret;
    g_sircs.busy = 0;
    g_sircs.power_locked = 0;

    ret = irdaAddDriver();
    if (ret < 0) {
        sceKernelDeleteEventFlag(g_sircs.evf_id);
        sceKernelDeleteSema(g_sircs.sema_id);
        sceKernelReleaseIntrHandler(14);
        return ret;
    }

    irdaSetSircsBusyCallback(sircsIsBusy);
    return 0;
}

static int sircsIntrHandler(int intr, void *arg)
{
    volatile u32 *sircs_regs = (volatile u32 *)0xbe340000;

    (void)intr;
    (void)arg;

    if (*(volatile u32 *)0xbe340000 & 0x200) {
        if (g_sircs.tx_sent < g_sircs.tx_total) {
            if ((*(volatile u32 *)0xbe340010 & 0x20) == 0 && g_sircs.tx_sent < g_sircs.tx_total) {
                do {
                    u32 val = *g_sircs.tx_buf;
                    g_sircs.tx_buf++;
                    sircs_regs[5] = val;
                    g_sircs.tx_sent++;
                } while ((*(volatile u32 *)0xbe340010 & 0x20) == 0 && g_sircs.tx_sent < g_sircs.tx_total);
            }
            if (g_sircs.tx_sent >= g_sircs.tx_total) {
                *(volatile u32 *)0xbe340000 |= 0x800;
            }
        } else {
            *(volatile u32 *)0xbe340000 &= ~0x1a00;
            sceKernelSetEventFlag(g_sircs.evf_id, 1);
        }
    } else if (*(volatile u32 *)0xbe340000 & 0x100) {
        if (*(volatile u32 *)0xbe340010 & 1) {
            *(volatile u32 *)0xbe340000 &= ~0x6500;
            sceKernelSetEventFlag(g_sircs.evf_id, 0x200);
        } else if (*(volatile u32 *)0xbe340010 & 2) {
            *(volatile u32 *)0xbe340000 &= ~0x6500;
            sircs_regs[6] = 1;
            sceKernelSetEventFlag(g_sircs.evf_id, 0x400);
        }
        while (g_sircs.rx_rcvd < g_sircs.rx_total && (*(volatile u32 *)0xbe340010 & 0x10) == 0) {
            u32 val = *(volatile u32 *)0xbe340014;
            if (g_sircs.rx_rcvd == 0) {
                if (val & 0x10000) {
                    int diff = (int)(val & 0xffff) - 0x60;
                    if (diff < 0)
                        diff = 0x60 - (int)(val & 0xffff);
                    if (diff < 5) {
                        *g_sircs.rx_buf++ = val;
                        g_sircs.rx_rcvd++;
                    }
                }
            } else {
                if (((g_sircs.rx_rcvd & 1) != 0 && (val & 0x10000) != 0) ||
                    ((g_sircs.rx_rcvd & 1) == 0 && (val & 0x10000) == 0)) {
                    *(volatile u32 *)0xbe340000 &= ~0x6500;
                    sceKernelSetEventFlag(g_sircs.evf_id, 0x400);
                } else {
                    *g_sircs.rx_buf++ = val;
                    g_sircs.rx_rcvd++;
                }
            }
        }
        if (g_sircs.rx_rcvd >= g_sircs.rx_total) {
            *(volatile u32 *)0xbe340000 &= ~0x6500;
            sceKernelSetEventFlag(g_sircs.evf_id, 0x100);
        }
    }
    return -1;
}

int sceSircsSend(SircsData *sd, int count)
{
    u32 pulse_buf[44];
    u64 sysclock;
    u32 outbits;
    u32 sec, usec;
    int intr, ret;
    u32 k1;
    volatile u32 *sircs_regs = (volatile u32 *)0xbe340000;

    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");

    if (sceKernelIsIntrContext()) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0x80020064;
    }

    if ((s32)(((u32)sd | ((u32)sd + 4)) & (k1 << 11)) < 0) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0x800200d3;
    }

    if (sd->type != 12 && sd->type != 15 && sd->type != 20) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0x80290200;
    }

    intr = sceKernelCpuSuspendIntr();
    if (irdaIsLocked() != 0) {
        sceKernelCpuResumeIntr(intr);
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0x80290100;
    }
    g_sircs.busy = 1;
    sceKernelCpuResumeIntr(intr);

    ret = sceKernelWaitSema(g_sircs.sema_id, 1, 0);
    if (ret < 0) {
        g_sircs.busy = 0;
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return ret;
    }

    sircsPowerLock();
    sceSysregSircsClkEnable();
    sceSysregSircsIoEnable();

    if (g_sircs.use_gpio != 0)
        sceGpioPortClear(4);

    sircsEncodePulses(sd, pulse_buf);

    *(volatile u32 *)0xbe340000 = 0xa0;
    sircs_regs[1] = 0x14;
    sircs_regs[3] = 7;
    sceKernelEnableIntr(14);

    if (count > 0) {
        int rem = count;
        do {
            g_sircs.tx_buf = pulse_buf;
            g_sircs.tx_total = (sd->type << 1) + 2;
            g_sircs.tx_sent = 0;
            sceKernelClearEventFlag(g_sircs.evf_id, ~0U);

            if ((*(volatile u32 *)0xbe340010 & 0x20) == 0) {
                u32 *p = g_sircs.tx_buf;
                do {
                    sircs_regs[5] = *p++;
                    g_sircs.tx_sent++;
                } while ((*(volatile u32 *)0xbe340010 & 0x20) == 0);
                g_sircs.tx_buf = p;
            }

            u32 startTime = sceKernelGetSystemTimeLow();
            *(volatile u32 *)0xbe340000 |= 0x1200;
            sceKernelWaitEventFlag(g_sircs.evf_id, 1, 0x11, &outbits, 0);
            u32 dt = sceKernelGetSystemTimeLow() - startTime;
            sysclock = dt;
            sceKernelSysClock2USec(&sysclock, &sec, &usec);
            if (usec <= 0xafc7)
                sceKernelDelayThread(0xafc8 - usec);
            rem--;
        } while (rem != 0);
    }

    sceKernelDisableIntr(14);
    if (g_sircs.use_gpio != 0)
        sceGpioPortSet(4);

    sceSysregSircsClkDisable();
    sceSysregSircsIoDisable();
    g_sircs.busy = 0;
    sircsPowerUnlock();
    sceKernelSignalSema(g_sircs.sema_id, 1);

    __asm__ volatile ("move $k1, %0" : : "r"(k1));
    return 0;
}

int sceSircs_driver_83381633(SircsData *sd, SceUInt timeout) __attribute__((alias("sceSircs_83381633")));
int sceSircs_83381633(SircsData *sd, SceUInt timeout)
{
    u32 pulse_buf[44];
    u32 outbits;
    int intr, ret;
    u32 k1;
    volatile u32 *sircs_regs = (volatile u32 *)0xbe340000;

    __asm__ volatile ("move %0, $k1" : "=r"(k1));
    __asm__ volatile ("sll $k1, $k1, 11");

    if (sceKernelIsIntrContext()) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0x80020064;
    }

    if ((s32)(((u32)sd | ((u32)sd + 4)) & (k1 << 11)) < 0) {
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0x800200d3;
    }

    intr = sceKernelCpuSuspendIntr();
    if (irdaIsLocked() != 0) {
        sceKernelCpuResumeIntr(intr);
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return 0x80290100;
    }
    g_sircs.busy = 1;
    sceKernelCpuResumeIntr(intr);

    ret = sceKernelWaitSema(g_sircs.sema_id, 1, 0);
    if (ret < 0) {
        g_sircs.busy = 0;
        __asm__ volatile ("move $k1, %0" : : "r"(k1));
        return ret;
    }

    sircsPowerLock();
    sceSysregSircsClkEnable();
    sceSysregSircsIoEnable();

    if (g_sircs.use_gpio != 0)
        sceGpioPortClear(4);

    *(volatile u32 *)0xbe340000 = 0x5c;
    sircs_regs[7] = 1;
    sircs_regs[2] = 0x14;

    while ((*(volatile u32 *)0xbe340010 & 0x10) == 0) {
        (void)*(volatile u32 *)0xbe340014;
    }
    sircs_regs[6] = 1;

    sceKernelEnableIntr(14);
    g_sircs.rx_total = 43;
    g_sircs.rx_rcvd = 0;
    g_sircs.rx_buf = pulse_buf;
    sceKernelClearEventFlag(g_sircs.evf_id, ~0U);

    *(volatile u32 *)0xbe340000 |= 0x6500;
    ret = sceKernelWaitEventFlag(g_sircs.evf_id, 0x700, 0x11, &outbits, timeout ? &timeout : 0);
    sceKernelDisableIntr(14);
    *(volatile u32 *)0xbe340000 &= ~0x6500;
    sircs_regs[7] = 0;

    if (g_sircs.use_gpio != 0)
        sceGpioPortSet(4);

    sceSysregSircsClkDisable();
    sceSysregSircsIoDisable();

    if (ret == (int)0x800201a8) {
        ret = (g_sircs.rx_rcvd != 0) ? 0x80290300 : 0x80290302;
    } else if (ret == 0) {
        if (outbits & 0x200) {
            ret = 0x80290300;
        } else if (outbits & 0x400) {
            ret = 0x80290301;
        } else {
            ret = sircsDecodePulses(sd, pulse_buf, g_sircs.rx_rcvd);
        }
    }

    g_sircs.busy = 0;
    sircsPowerUnlock();
    sceKernelSignalSema(g_sircs.sema_id, 1);

    __asm__ volatile ("move $k1, %0" : : "r"(k1));
    return ret;
}

static void sircsPowerLock(void)
{
    if (g_sircs.power_locked == 0) {
        sceKernelPowerLock(0);
        g_sircs.power_locked = 1;
    }
}

static void sircsPowerUnlock(void)
{
    if (g_sircs.power_locked != 0) {
        sceKernelPowerUnlock(0);
        g_sircs.power_locked = 0;
    }
}

static int sircsIsBusy(void)
{
    return g_sircs.busy;
}

int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    return sceSircs_driver_62411801() < 0 ? 1 : 0;
}

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    irdaDelDriver();
    if (g_sircs.use_gpio != 0)
        sceGpioPortSet(4);
    sceSysregSircsClkDisable();
    sceSysregSircsIoDisable();
    sceKernelReleaseIntrHandler(14);
    sceKernelDeleteSema(g_sircs.sema_id);
    sceKernelDeleteEventFlag(g_sircs.evf_id);
    irdaDelDriver();
    sircsPowerUnlock();
    return 0;
}

int module_reboot_before(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    if (g_sircs.use_gpio != 0)
        sceGpioPortSet(4);
    sceSysregSircsClkDisable();
    sceSysregSircsIoDisable();
    sceKernelReleaseIntrHandler(14);
    sceKernelDeleteSema(g_sircs.sema_id);
    sceKernelDeleteEventFlag(g_sircs.evf_id);
    irdaDelDriver();
    sircsPowerUnlock();
    return 0;
}

int sceSircs_driver_19155A2F(void)
{
    if (g_sircs.use_gpio != 0)
        sceGpioPortSet(4);
    sceSysregSircsClkDisable();
    sceSysregSircsIoDisable();
    sceKernelReleaseIntrHandler(14);
    sceKernelDeleteSema(g_sircs.sema_id);
    sceKernelDeleteEventFlag(g_sircs.evf_id);
    irdaDelDriver();
    sircsPowerUnlock();
    return 0;
}

static u32 sircsEncodePulses(SircsData *sd, u32 *out)
{
    u32 total_time = 0xbb8;
    u8 cmd = sd->cmd;
    u16 dev = sd->dev;
    int mask;

    *out++ = 0x10060;
    *out++ = 0x18;

    for (mask = 1; mask < 0x80; mask <<= 1) {
        if (cmd & mask) {
            *out++ = 0x10030;
            total_time += 0x4b0;
        } else {
            *out++ = 0x10018;
            total_time += 0x258;
        }
        *out++ = 0x18;
        total_time += 0x258;
    }

    for (mask = 1; mask < (1 << (sd->type - 7)); mask <<= 1) {
        if (dev & mask) {
            *out++ = 0x10030;
            total_time += 0x4b0;
        } else {
            *out++ = 0x10018;
            total_time += 0x258;
        }
        *out++ = 0x18;
        total_time += 0x258;
    }

    return total_time;
}

static int sircsDecodePulses(SircsData *sd, u32 *in, u32 count)
{
    u8 cmd = 0;
    u16 dev = 0;
    int i, diff;
    u32 lowPulse, highPulse;

    (void)count;
    lowPulse = *in;
    if ((lowPulse & 0x10000) == 0)
        return 0x80290310;
    diff = (int)(lowPulse & 0xffff) - 0x60;
    if (diff < 0)
        diff = 0x60 - (int)(lowPulse & 0xffff);
    if (diff >= 5)
        return 0x80290310;
    in++;

    for (i = 0; i < 7; i++, in += 2) {
        lowPulse = in[0];
        if (lowPulse & 0x10000)
            return 0x80290311;
        diff = (int)(lowPulse & 0xffff) - 0x18;
        if (diff < 0)
            diff = 0x18 - (int)(lowPulse & 0xffff);
        if (diff >= 5)
            return 0x80290311;

        highPulse = in[1];
        if ((highPulse & 0x10000) == 0)
            return 0x80290311;
        diff = (int)(highPulse & 0xffff) - 0x30;
        if (diff < 0)
            diff = 0x30 - (int)(highPulse & 0xffff);
        if (diff < 5) {
            cmd |= (1 << i);
        } else {
            diff = (int)(highPulse & 0xffff) - 0x18;
            if (diff < 0)
                diff = 0x18 - (int)(highPulse & 0xffff);
            if (diff >= 5)
                return 0x80290311;
        }
    }

    for (i = 0; i < 16; i++, in += 2) {
        lowPulse = in[0];
        highPulse = in[1];
        if (lowPulse & 0x10000)
            break;
        diff = (int)(lowPulse & 0xffff) - 0x18;
        if (diff < 0)
            diff = 0x18 - (int)(lowPulse & 0xffff);
        if (diff >= 5)
            break;

        if ((highPulse & 0x10000) == 0)
            return 0x80290312;
        diff = (int)(highPulse & 0xffff) - 0x30;
        if (diff < 0)
            diff = 0x30 - (int)(highPulse & 0xffff);
        if (diff < 5) {
            dev |= (1 << i);
        } else {
            diff = (int)(highPulse & 0xffff) - 0x18;
            if (diff < 0)
                diff = 0x18 - (int)(highPulse & 0xffff);
            if (diff >= 5)
                return 0x80290312;
        }
    }

    if (i == 16)
        return 0x80290301;

    if ((highPulse & 0x10000) == 0)
        return 0x80290313;
    diff = (int)(highPulse & 0xffff) - 0x60;
    if (diff < 0)
        diff = 0x60 - (int)(highPulse & 0xffff);
    if (diff >= 5)
        return 0x80290313;

    sd->type = i + 7;
    sd->cmd = cmd;
    sd->dev = dev;
    return 0;
}

static int irdaIoIoctl(void *arg, int cmd, void *indata, int inlen, void *outdata, int outlen)
{
    (void)arg;

    switch (cmd) {
    case 0x05000004:
    case 0x05000008:
        return irdaFlushRx();
    case 0x05000007:
        return irdaFlushTx();
    case 0x05000009:
        return 0;
    case 0x05001003:
        return irdaWaitRxReady();
    case 0x05001006:
        return irdaWaitTxDone();
    case 0x05010001:
        if (indata == 0 || inlen != 4)
            return 0x80020324;
        return irdaSetBaudrate(*(int *)indata);
    case 0x05020002:
        if (outdata == 0 || outlen != 4)
            return 0x80020324;
        return irdaGetRxCount((u32 *)outdata);
    case 0x05020005:
        if (outdata == 0 || outlen != 4)
            return 0x80020324;
        return irdaGetTxFree((u32 *)outdata);
    default:
        return 0x80020325;
    }
}

static int irdaIoDevctl(void *arg, const char *devname, int cmd, void *indata, int inlen, void *outdata, int outlen)
{
    (void)arg;
    (void)devname;

    switch (cmd) {
    case 0x05110001:
        if (g_irda.open_count != 0)
            return 0x802a0007;
        if (indata == 0 || inlen != 4)
            return 0x80020324;
        g_irda.mode = *(int *)indata;
        return 0;
    case 0x05110003:
        if (g_irda.open_count != 0)
            return 0x802a0007;
        if (indata == 0 || inlen != 4)
            return 0x80020324;
        g_irda.tx_buf_size = *(int *)indata;
        return 0;
    case 0x05110005:
        if (g_irda.open_count != 0)
            return 0x802a0007;
        if (indata == 0 || inlen != 4)
            return 0x80020324;
        g_irda.rx_buf_size = *(int *)indata;
        return 0;
    case 0x05120002:
        if (outdata == 0 || outlen != 4)
            return 0x80020324;
        *(int *)outdata = g_irda.mode;
        return 0;
    case 0x05120004:
        if (outdata == 0 || outlen != 4)
            return 0x80020324;
        *(int *)outdata = g_irda.tx_buf_size;
        return 0;
    case 0x05120006:
        if (outdata == 0 || outlen != 4)
            return 0x80020324;
        *(int *)outdata = g_irda.rx_buf_size;
        return 0;
    default:
        return 0x80020325;
    }
}

static int irdaSetBaudrate(int baud)
{
    volatile u32 *uart_regs = (volatile u32 *)0xbe540000;
    int intr;

    if (g_irda.open_count == 0)
        return 0x80020323;

    if (baud != 9600 && baud != 14400 && baud != 19200 && baud != 38400 && baud != 57600 && baud != 115200)
        return 0x802a0002;

    intr = sceKernelCpuSuspendIntr();
    u32 cr = *(volatile u32 *)0xbe540030;
    cr &= ~1U;
    uart_regs[12] = cr;
    u32 lcr = *(volatile u32 *)0xbe54002c;

    int div16 = baud << 4;
    uart_regs[9] = 0x16e3600 / div16;
    if (g_irda.mode == 0) {
        int fbrd = 0x5b8d8000 / div16;
        uart_regs[10] = fbrd % 64;
    } else {
        uart_regs[10] = 0;
    }

    uart_regs[11] = lcr;
    uart_regs[8] = 0x0d;
    cr = *(volatile u32 *)0xbe540030;
    cr |= 1;
    uart_regs[12] = cr;

    g_irda.baudrate = baud;
    g_irda.alarm_usec = (0xf4240U / baud) << 7;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static int irdaGetRxCount(u32 *out)
{
    int intr;

    if (g_irda.open_count == 0)
        return 0x80020323;

    intr = sceKernelCpuSuspendIntr();
    *out = g_irda.rx_count;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static int irdaWaitRxReady(void)
{
    u32 outbits;
    int ret = sceKernelWaitEventFlag(g_irda.evf_id, 0x10, 1, &outbits, 0);
    return ret < 0 ? ret : 0;
}

static int irdaFlushRx(void)
{
    int intr;

    if (g_irda.open_count == 0)
        return 0x80020323;

    intr = sceKernelCpuSuspendIntr();
    g_irda.rx_rd = 0;
    g_irda.rx_wr = 0;
    g_irda.rx_count = 0;
    while ((*(volatile u32 *)0xbe540018 & 0x10) == 0) {
        (void)*(volatile u32 *)0xbe540000;
    }
    *(volatile u32 *)0xbe540004 = 0xffff;
    sceKernelClearEventFlag(g_irda.evf_id, ~0x10U);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static int irdaWaitTxDone(void)
{
    u32 outbits;
    int ret = sceKernelWaitEventFlag(g_irda.evf_id, 0x20, 0x20, &outbits, 0);
    irdaPowerUnlock();
    return ret < 0 ? ret : 0;
}

static int irdaGetTxFree(u32 *out)
{
    int intr;

    if (g_irda.open_count == 0)
        return 0x80020323;

    intr = sceKernelCpuSuspendIntr();
    *out = g_irda.tx_buf_size - g_irda.tx_count;
    sceKernelCpuResumeIntr(intr);
    return 0;
}

static int irdaFlushTx(void)
{
    int intr;

    if (g_irda.open_count == 0)
        return 0x80020323;

    intr = sceKernelCpuSuspendIntr();
    g_irda.tx_rd = 0;
    g_irda.tx_wr = 0;
    g_irda.tx_count = 0;
    while (*(volatile u32 *)0xbe540018 & 8) {
    }
    sceKernelClearEventFlag(g_irda.evf_id, ~0x20U);
    sceKernelCpuResumeIntr(intr);
    irdaPowerUnlock();
    return 0;
}

static int irdaHwEnable(void)
{
    volatile u32 *uart_regs = (volatile u32 *)0xbe540000;

    if (sceSysregGetTachyonVersion() <= 0x4fffff)
        sceGpioPortClear(4);

    sceSysregUartIoEnable(5);
    sceSysregUartClkEnable(5);
    uart_regs[12] = 0x300;

    int div16 = g_irda.baudrate << 4;
    uart_regs[9] = 0x16e3600 / div16;
    if (g_irda.mode == 0) {
        int fbrd = 0x5b8d8000 / div16;
        uart_regs[10] = fbrd % 64;
    } else {
        uart_regs[10] = 0;
    }

    uart_regs[8] = 0x0d;
    uart_regs[11] = 0x70;
    uart_regs[7] = 0x280;
    uart_regs[13] = 0;
    uart_regs[14] = 0x50;
    uart_regs[1] = 0xffff;
    uart_regs[12] = (g_irda.mode != 0) ? 0x307 : 0x303;
    return 0;
}

static int irdaHwDisable(void)
{
    sceSysregUartIoDisable(5);
    sceSysregUartClkEnable(5);
    if (sceSysregGetTachyonVersion() <= 0x4fffff)
        sceGpioPortSet(4);
    return 0;
}

static int irdaAllocResources(void)
{
    int ret;

    ret = sceKernelCreateFpl("SceIrdaDriver", 1, 1, g_irda.rx_buf_size, 2, 0);
    g_irda.fpl_id = ret;
    if (ret < 0)
        return 0x802a0006;

    ret = sceKernelTryAllocateFpl(g_irda.fpl_id, (void **)&g_irda.rx_buf);
    if (ret < 0)
        return 0x802a0006;

    ret = sceKernelTryAllocateFpl(g_irda.fpl_id, (void **)&g_irda.tx_buf);
    if (ret < 0)
        return 0x802a0006;

    sceKernelRegisterIntrHandler(37, 2, irdaIntrHandler, &g_irda, 0);
    sceKernelEnableIntr(37);
    ret = sceKernelCreateEventFlag("SceIrdaDriver", 0x201, 0, 0);
    g_irda.evf_id = ret;
    return ret < 0 ? 0x802a0006 : 0;
}

static void irdaFreeResources(void)
{
    sceKernelDisableIntr(37);
    sceKernelReleaseIntrHandler(37);
    if (g_irda.alarm_id > 0) {
        sceKernelCancelAlarm(g_irda.alarm_id);
        g_irda.alarm_id = 0;
    }
    if (g_irda.evf_id > 0) {
        sceKernelDeleteEventFlag(g_irda.evf_id);
        g_irda.evf_id = 0;
    }
    if (g_irda.fpl_id > 0) {
        sceKernelDeleteFpl(g_irda.fpl_id);
        g_irda.fpl_id = 0;
    }
}

static int irdaStartTxAlarm(IrdaContext *ctx)
{
    if (ctx->alarm_id == 0) {
        irdaEnableTxHardware();
        SceUID id = sceKernelSetAlarm(100, irdaTxAlarmHandler, ctx);
        ctx->alarm_id = id;
        if (id < 0)
            ctx->alarm_id = 0;
    }
    return 0;
}

static void irdaPowerLock(void)
{
    if (g_irda.power_locked == 0) {
        sceKernelPowerLock(0);
        g_irda.power_locked = 1;
    }
}

static void irdaPowerUnlock(void)
{
    if (g_irda.power_locked != 0) {
        sceKernelPowerUnlock(0);
        g_irda.power_locked = 0;
    }
}

static void irdaEnableTxHardware(void)
{
    volatile u32 cr = *(volatile u32 *)0xbe540030;
    if ((cr & 0x20) == 0) {
        cr &= ~1U;
        *(volatile u32 *)0xbe540030 = cr;
        cr |= 0x21;
        *(volatile u32 *)0xbe540030 = cr;
    }
}

static void irdaDisableTxHardware(void)
{
    volatile u32 cr = *(volatile u32 *)0xbe540030;
    if (cr & 0x20) {
        cr &= ~1U;
        *(volatile u32 *)0xbe540030 = cr;
        cr &= ~0x20U;
        cr |= 1;
        *(volatile u32 *)0xbe540030 = cr;
    }
}

static int irdaIsLocked(void)
{
    return g_irda.sircs_lock;
}

static void irdaSetSircsBusyCallback(int (*cb)(void))
{
    g_irda.sircs_cb = cb;
}

static int irdaAddDriver(void)
{
    return sceIoAddDrv((void *)&g_irda_drv);
}

static int irdaDelDriver(void)
{
    sceIoDelDrv(g_irda_drv.name);
    return 0;
}

static int irdaIoInit(void *arg)
{
    (void)arg;
    if (sceSysregGetTachyonVersion() <= 0x4fffff) {
        g_irda.use_gpio = 1;
        sceGpioSetPortMode(2, 0);
    }
    g_irda.mode = 1;
    g_irda.tx_buf_size = 0x400;
    g_irda.rx_buf_size = 0x400;
    sceKernelRegisterSysEventHandler(&g_sysevent);
    return 0;
}

static int irdaIoExit(void *arg)
{
    (void)arg;
    irdaClose();
    sceKernelUnregisterSysEventHandler(&g_sysevent);
    return 0;
}

static int irdaIoOpen(void *arg, char *file, int flags, int mode)
{
    (void)arg;
    (void)file;
    (void)flags;
    (void)mode;
    return irdaOpen();
}

static int irdaIoClose(void *arg)
{
    (void)arg;
    return irdaClose();
}

static int irdaIoRead(void *arg, char *data, int len)
{
    (void)arg;
    return irdaRead((u8 *)data, len);
}

static int irdaIoWrite(void *arg, const char *data, int len)
{
    (void)arg;
    return irdaWrite((const u8 *)data, len);
}

static int irdaSysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
    (void)ev_name;
    (void)param;
    (void)result;
    if (ev_id == 0x400c) {
        if (g_irda.open_count != 0)
            irdaHwDisable();
    } else if (ev_id == 0x1000c) {
        if (g_irda.open_count != 0)
            irdaHwEnable();
    }
    return 0;
}

static int irdaOpen(void)
{
    if (g_irda.open_count > 0)
        return 0x802a0004;

    irdaPowerLock();
    if (g_irda.open_count == 0) {
        g_irda.baudrate = 9600;
        g_irda.alarm_usec = 0x3400;
        int intr = sceKernelCpuSuspendIntr();
        if (g_irda.sircs_cb != 0 && g_irda.sircs_cb() != 0) {
            sceKernelCpuResumeIntr(intr);
            irdaPowerUnlock();
            return 0x802a0007;
        }
        g_irda.sircs_lock = 1;
        sceKernelCpuResumeIntr(intr);
        irdaHwEnable();
        if (irdaAllocResources() < 0) {
            irdaHwDisable();
            irdaFreeResources();
            g_irda.sircs_lock = 0;
            irdaPowerUnlock();
            return 0x802a0006;
        }
    }
    g_irda.open_count++;
    irdaPowerUnlock();
    return 0;
}

static int irdaClose(void)
{
    irdaPowerLock();
    if (g_irda.open_count == 0) {
        irdaPowerUnlock();
        return 0x802a0003;
    }
    g_irda.open_count--;
    if (g_irda.open_count <= 0) {
        irdaHwDisable();
        irdaFreeResources();
        g_irda.sircs_lock = 0;
        g_irda.open_count = 0;
    }
    irdaPowerUnlock();
    return 0;
}

static int irdaRead(u8 *buf, u32 len)
{
    u32 read_bytes = 0;
    int intr;

    if (g_irda.open_count == 0)
        return 0x80020323;
    if (buf == 0)
        return 0x80020324;

    intr = sceKernelCpuSuspendIntr();
    if (g_irda.rx_count != 0 && len != 0) {
        do {
            buf[read_bytes++] = g_irda.rx_buf[g_irda.rx_rd];
            g_irda.rx_rd = (g_irda.rx_rd + 1) % g_irda.rx_buf_size;
            g_irda.rx_count--;
            if (g_irda.rx_count == 0)
                break;
        } while (read_bytes < len);
    }
    sceKernelClearEventFlag(g_irda.evf_id, ~0x10U);
    sceKernelCpuResumeIntr(intr);
    return read_bytes;
}

static int irdaWrite(const u8 *buf, u32 len)
{
    u32 written = 0;
    int intr;

    if (g_irda.open_count == 0)
        return 0x80020323;
    if (buf == 0)
        return 0x80020324;

    irdaPowerLock();
    intr = sceKernelCpuSuspendIntr();
    sceKernelClearEventFlag(g_irda.evf_id, ~0x20U);
    if (g_irda.tx_count < g_irda.tx_buf_size && len != 0) {
        do {
            g_irda.tx_buf[g_irda.tx_wr] = *buf++;
            written++;
            g_irda.tx_wr = (g_irda.tx_wr + 1) % g_irda.tx_buf_size;
            g_irda.tx_count++;
        } while (g_irda.tx_count < g_irda.tx_buf_size && written < len);
    }
    irdaStartTxAlarm(&g_irda);
    sceKernelCpuResumeIntr(intr);
    return written;
}

static int irdaIntrHandler(int intr, IrdaContext *ctx)
{
    volatile u32 *uart_regs = (volatile u32 *)0xbe540000;
    (void)intr;
    u32 mis = uart_regs[16];
    uart_regs[17] = mis;
    irdaDrainRxFifo(ctx);
    return -1;
}

static SceUInt irdaTxAlarmHandler(IrdaContext *ctx)
{
    SceUInt next = ctx->alarm_usec;

    if ((*(volatile u32 *)0xbe540018 & 0x20) != 0)
        return next;

    while ((*(volatile u32 *)0xbe540018 & 0x20) == 0 && ctx->tx_count != 0) {
        *(volatile u32 *)0xbe540000 = ctx->tx_buf[ctx->tx_rd];
        ctx->tx_rd = (ctx->tx_rd + 1) % ctx->tx_buf_size;
        ctx->tx_count--;
    }

    if (ctx->tx_count == 0 && (*(volatile u32 *)0xbe540018 & 8) == 0) {
        irdaDisableTxHardware();
        sceKernelSetEventFlag(ctx->evf_id, 0x20);
        ctx->alarm_id = 0;
        next = 0;
    }
    return next;
}

static int irdaDrainRxFifo(IrdaContext *ctx)
{
    int rcvd = 0;

    if ((*(volatile u32 *)0xbe540018 & 0x10) != 0)
        return 0;

    if (ctx->rx_count >= ctx->rx_buf_size)
        return 0;

    do {
        u32 dr = *(volatile u32 *)0xbe540000 & 0xffff;
        if (dr & 0xf00) {
            *(volatile u32 *)0xbe540004 = *(volatile u32 *)0xbe540004;
        } else {
            ctx->rx_buf[ctx->rx_wr] = (u8)dr;
            rcvd = 1;
            ctx->rx_wr = (ctx->rx_wr + 1) % ctx->rx_buf_size;
            ctx->rx_count++;
        }
    } while ((*(volatile u32 *)0xbe540018 & 0x10) == 0 && ctx->rx_count < ctx->rx_buf_size);

    if (rcvd != 0)
        sceKernelSetEventFlag(ctx->evf_id, 0x10);
    return 0;
}
