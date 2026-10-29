#include <kermit.h>

/* =========================================================================
 * External kernel imports (matching scetables.S)
 * ========================================================================= */
extern int sceKernelDmaRegisterDdrFlush(void *func);
extern int sceKernelCallSubIntrHandler(int intr_code, int sub_intr_code, void *arg1, void *arg2);
extern void sceKernelCpuResumeIntrWithSync(int state);
extern int sceKernelPollEventFlag(SceUID evid, u32 bits, u32 wait, u32 *out_bits);
extern void sceKernelDcacheWritebackAll(void);
extern void *memset(void *dst, int c, u32 n);
extern void *memcpy(void *dst, const void *src, u32 n);

#define REG32(addr) (*(volatile u32 *)(addr))

/* =========================================================================
 * Part 1: sceSysreg_driver (178 exported functions)
 * ========================================================================= */

static float g_pllBaseFreq = 37.0f;
static float g_pllFreqTable[16] = {
	166.5f, 333.0f, 444.0f, 222.0f, 190.28572f, 380.57144f, 266.4f, 532.8f,
	148.0f, 148.0f, 148.0f, 148.0f, 148.0f,     148.0f,     148.0f, 148.0f
};
static float g_pllFreq;
static u32 g_tachyonVer = 0xFFFFFFFF;
static u8 g_usbIoRefMask;
static u32 g_sysregSubIntrMask;
static u32 g_sysregSubIntrRot;

static s32 sysreg_mod_reg(u32 addr, u32 mask, s32 enable)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u32 val = REG32(addr);
	u32 next = val & ~mask;
	if (enable != 0)
		next = val | mask;
	REG32(addr) = next;
	sceKernelCpuResumeIntr(intr);
	return (val & mask) != 0;
}

static s32 sysreg_reset_ctrl(u32 mask, s32 enable)
{
	return sysreg_mod_reg(0xBC10004C, mask, enable);
}

static s32 sysreg_busclk_ctrl(u32 mask, s32 enable)
{
	return sysreg_mod_reg(0xBC100050, mask, enable);
}

static s32 sysreg_clk1_ctrl(u32 mask, s32 enable)
{
	return sysreg_mod_reg(0xBC100054, mask, enable);
}

static s32 sysreg_clk2_ctrl(u32 mask, s32 enable)
{
	return sysreg_mod_reg(0xBC100058, mask, enable);
}

static s32 sysreg_io_ctrl(u32 mask, s32 enable)
{
	return sysreg_mod_reg(0xBC100078, mask, enable);
}

static s32 sysreg_gpio_io_ctrl(u32 mask, s32 enable)
{
	return sysreg_mod_reg(0xBC10007C, mask, enable);
}

static s32 sysreg_unk74_ctrl(u32 mask, s32 enable)
{
	return sysreg_mod_reg(0xBC100074, mask, enable);
}

/* Reset controls (0xBC10004C) */
s32 sceSysregTopResetEnable(void) { return sysreg_reset_ctrl(0x0001, 1); }
s32 sceSysregScResetEnable(void) { return sysreg_reset_ctrl(0x0002, 1); }
s32 sceSysregMeResetEnable(void) { return sysreg_reset_ctrl(0x0004, 1); }
s32 sceSysregMeResetDisable(void) { return sysreg_reset_ctrl(0x0004, 0); }
s32 sceSysregAwResetEnable(void) { return sysreg_reset_ctrl(0x0008, 1); }
s32 sceSysregAwResetDisable(void) { return sysreg_reset_ctrl(0x0008, 0); }
s32 sceSysregVmeResetEnable(void) { return sysreg_reset_ctrl(0x0010, 1); }
s32 sceSysregVmeResetDisable(void) { return sysreg_reset_ctrl(0x0010, 0); }
s32 sceSysregAvcResetEnable(void) { return sysreg_reset_ctrl(0x0020, 1); }
s32 sceSysregAvcResetDisable(void) { return sysreg_reset_ctrl(0x0020, 0); }
s32 sceSysregUsbResetEnable(void) { return sysreg_reset_ctrl(0x0040, 1); }
s32 sceSysregUsbResetDisable(void) { return sysreg_reset_ctrl(0x0040, 0); }
s32 sceSysregAtaResetEnable(void) { return sysreg_reset_ctrl(0x0080, 1); }
s32 sceSysregAtaResetDisable(void) { return sysreg_reset_ctrl(0x0080, 0); }
s32 sceSysregMsifResetEnable(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_reset_ctrl(0x0100 << no, 1);
}
s32 sceSysregMsifResetDisable(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_reset_ctrl(0x0100 << no, 0);
}
s32 sceSysregKirkResetEnable(void) { return sysreg_reset_ctrl(0x0400, 1); }
s32 sceSysregKirkResetDisable(void) { return sysreg_reset_ctrl(0x0400, 0); }
s32 sceSysregAtahddResetEnable(void) { return sysreg_reset_ctrl(0x1000, 1); }
s32 sceSysregAtahddResetDisable(void) { return sysreg_reset_ctrl(0x1000, 0); }
s32 sceSysregUsbhostResetEnable(void) { return sysreg_reset_ctrl(0x2000, 1); }
s32 sceSysregUsbhostResetDisable(void) { return sysreg_reset_ctrl(0x2000, 0); }
s32 sceSysreg_driver_C6C75585(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_reset_ctrl(0x4000 << no, 1);
}
s32 sceSysreg_driver_0995F8F6(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_reset_ctrl(0x4000 << no, 0);
}
s32 sceSysreg_driver_72887197(void) { return sysreg_reset_ctrl(0x10000, 1); }
s32 sceSysreg_driver_32E02FDF(void) { return sysreg_reset_ctrl(0x10000, 0); }
s32 sceSysreg_driver_73B3E52D(void) { return (REG32(0xBC10004C) >> 16) & 1; }

/* Bus clock controls (0xBC100050) */
s32 sceSysregMeBusClockEnable(void) { return sysreg_busclk_ctrl(0x0001, 1); }
s32 sceSysregMeBusClockDisable(void) { return sysreg_busclk_ctrl(0x0001, 0); }
s32 sceSysregAwRegABusClockEnable(void) { return sysreg_busclk_ctrl(0x0002, 1); }
s32 sceSysregAwRegABusClockDisable(void) { return sysreg_busclk_ctrl(0x0002, 0); }
s32 sceSysregAwRegBBusClockEnable(void) { return sysreg_busclk_ctrl(0x0004, 1); }
s32 sceSysregAwRegBBusClockDisable(void) { return sysreg_busclk_ctrl(0x0004, 0); }
s32 sceSysregAwEdramBusClockEnable(void) { return sysreg_busclk_ctrl(0x0008, 1); }
s32 sceSysregAwEdramBusClockDisable(void) { return sysreg_busclk_ctrl(0x0008, 0); }
static s32 sub_0280(void) { return sysreg_busclk_ctrl(0x0010, 1); }
static s32 sub_028c(void) { return sysreg_busclk_ctrl(0x0010, 0); }
s32 sceSysregKirkBusClockEnable(void) { return sysreg_busclk_ctrl(0x0080, 1); }
s32 sceSysregKirkBusClockDisable(void) { return sysreg_busclk_ctrl(0x0080, 0); }
s32 sceSysregAtaBusClockEnable(void) { return sysreg_busclk_ctrl(0x0100, 1); }
s32 sceSysregAtaBusClockDisable(void) { return sysreg_busclk_ctrl(0x0100, 0); }
s32 sceSysregUsbBusClockEnable(void) { return sysreg_busclk_ctrl(0x0200, 1); }
s32 sceSysregUsbBusClockDisable(void) { return sysreg_busclk_ctrl(0x0200, 0); }
s32 sceSysregMsifBusClockEnable(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_busclk_ctrl(0x0400 << no, 1);
}
s32 sceSysregMsifBusClockDisable(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_busclk_ctrl(0x0400 << no, 0);
}
s32 sceSysregEmcddrBusClockEnable(void) { return sysreg_busclk_ctrl(0x1000, 1); }
s32 sceSysregEmcddrBusClockDisable(void) { return sysreg_busclk_ctrl(0x1000, 0); }
s32 sceSysregEmcsmBusClockEnable(void) { return sysreg_busclk_ctrl(0x2000, 1); }
s32 sceSysregEmcsmBusClockDisable(void) { return sysreg_busclk_ctrl(0x2000, 0); }
s32 sceSysregApbBusClockEnable(void) { return sysreg_busclk_ctrl(0x4000, 1); }
s32 sceSysregApbBusClockDisable(void) { return sysreg_busclk_ctrl(0x4000, 0); }
s32 sceSysregAudioBusClockEnable(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_busclk_ctrl(0x8000 << no, 1);
}
s32 sceSysregAudioBusClockDisable(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_busclk_ctrl(0x8000 << no, 0);
}
s32 sceSysregAtahddBusClockEnable(void) { return sysreg_busclk_ctrl(0x20000, 1); }
s32 sceSysregAtahddBusClockDisable(void) { return sysreg_busclk_ctrl(0x20000, 0); }
s32 sceSysregUsbhostBusClockEnable(void) { return sysreg_busclk_ctrl(0x40000, 1); }
s32 sceSysregUsbhostBusClockDisable(void) { return sysreg_busclk_ctrl(0x40000, 0); }
s32 sceSysreg_driver_3BB0B2C8(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_busclk_ctrl(0x80000 << no, 1);
}
s32 sceSysreg_driver_76E57DC6(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_busclk_ctrl(0x80000 << no, 0);
}

/* Clock1 controls (0xBC100054) */
s32 sceSysregAtaClkEnable(void) { return sysreg_clk1_ctrl(0x0001, 1); }
s32 sceSysregAtaClkDisable(void) { return sysreg_clk1_ctrl(0x0001, 0); }
s32 sceSysregAtahddClkEnable(void) { return sysreg_clk1_ctrl(0x0002, 1); }
s32 sceSysregAtahddClkDisable(void) { return sysreg_clk1_ctrl(0x0002, 0); }
s32 sceSysregUsbhostClkEnable(void) { return sysreg_clk1_ctrl(0x10000, 1); }
s32 sceSysregUsbhostClkDisable(void) { return sysreg_clk1_ctrl(0x10000, 0); }
s32 sceSysregMsifClkEnable(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_clk1_ctrl(0x0100 << no, 1);
}
s32 sceSysregMsifClkDisable(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_clk1_ctrl(0x0100 << no, 0);
}
s32 sceSysreg_driver_A7C82BDD(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_clk1_ctrl(0x1000000 << no, 1);
}
s32 sceSysreg_driver_FF0E07B1(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_clk1_ctrl(0x1000000 << no, 0);
}

/* Clock2 controls (0xBC100058) */
s32 sceSysregSpiClkEnable(s32 no)
{
	if ((u32)no >= 6) return 0x80000102;
	return sysreg_clk2_ctrl(0x0001 << no, 1);
}
s32 sceSysregSpiClkDisable(s32 no)
{
	if ((u32)no >= 6) return 0x80000102;
	return sysreg_clk2_ctrl(0x0001 << no, 0);
}
s32 sceSysregUartClkEnable(s32 no)
{
	if ((u32)no >= 6) return 0x80000102;
	return sysreg_clk2_ctrl(0x0040 << no, 1);
}
s32 sceSysregUartClkDisable(s32 no)
{
	if ((u32)no >= 6) return 0x80000102;
	return sysreg_clk2_ctrl(0x0040 << no, 0);
}
s32 sceSysregApbTimerClkEnable(s32 no)
{
	if ((u32)no >= 4) return 0x80000102;
	return sysreg_clk2_ctrl(0x1000 << no, 1);
}
s32 sceSysregApbTimerClkDisable(s32 no)
{
	if ((u32)no >= 4) return 0x80000102;
	return sysreg_clk2_ctrl(0x1000 << no, 0);
}
s32 sceSysregAudioClkEnable(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_clk2_ctrl(0x10000 << no, 1);
}
s32 sceSysregAudioClkDisable(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_clk2_ctrl(0x10000 << no, 0);
}
static s32 sub_0768(void) { return sysreg_clk2_ctrl(0x80000, 1); }
static s32 sub_0774(void) { return sysreg_clk2_ctrl(0x80000, 0); }
s32 sceSysregSircsClkEnable(void) { return sysreg_clk2_ctrl(0x400000, 1); }
s32 sceSysregSircsClkDisable(void) { return sysreg_clk2_ctrl(0x400000, 0); }
s32 sceSysregGpioClkEnable(void) { return sysreg_clk2_ctrl(0x800000, 1); }
s32 sceSysregGpioClkDisable(void) { return sysreg_clk2_ctrl(0x800000, 0); }
s32 sceSysregAudioClkoutClkEnable(void) { return sysreg_clk2_ctrl(0x1000000, 1); }
s32 sceSysregAudioClkoutClkDisable(void) { return sysreg_clk2_ctrl(0x1000000, 0); }
s32 sceSysreg_driver_F9C93DD4(void) { return sysreg_clk2_ctrl(0x2000000, 1); }
s32 sceSysreg_driver_B6296512(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u32 val = REG32(0xBC100058);
	u32 prev = (val >> 25) & 1;
	REG32(0xBC100058) = val & ~0x2000000U;
	sceKernelCpuResumeIntr(intr);
	return prev;
}

/* Special Clock1 controls (0xBC100054) */
s32 sceSysreg_driver_FC0131A7(void) { return sysreg_clk1_ctrl(0x01000000, 1); }
s32 sceSysreg_driver_C9585F8E(void) { return sysreg_clk1_ctrl(0x01000000, 0); }
s32 sceSysreg_driver_68AE6434(void) { return sysreg_clk1_ctrl(0x02000000, 1); }
s32 sceSysreg_driver_14EB1393(void) { return sysreg_clk1_ctrl(0x02000000, 0); }
s32 sceSysregUsbClkEnable(s32 mask)
{
	if ((u32)(mask - 1) >= 15) return 0x80000107;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 val = REG32(0xBC100054);
	REG32(0xBC100054) = val | ((mask & 0xF) << 4);
	sceKernelCpuResumeIntr(intr);
	return (val >> 4) & 0xF;
}
s32 sceSysregUsbClkDisable(s32 mask)
{
	if ((u32)(mask - 1) >= 15) return 0x80000107;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 val = REG32(0xBC100054);
	REG32(0xBC100054) = val & ~((mask & 0xF) << 4);
	sceKernelCpuResumeIntr(intr);
	return (val >> 4) & 0xF;
}

/* IO controls (0xBC100078, 0xBC10007C, 0xBC100074) */
s32 sceSysregEmcsmIoEnable(void) { return sysreg_io_ctrl(0x0002, 1); }
s32 sceSysregEmcsmIoDisable(void) { return sysreg_io_ctrl(0x0002, 0); }
s32 sceSysregUsbIoEnable(void) { return sysreg_io_ctrl(0x0004, 1); }
s32 sceSysregUsbIoDisable(void) { return sysreg_io_ctrl(0x0004, 0); }
s32 sceSysregAtaIoEnable(void) { return sysreg_io_ctrl(0x0008, 1); }
s32 sceSysregAtaIoDisable(void) { return sysreg_io_ctrl(0x0008, 0); }
s32 sceSysregMsifIoEnable(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_io_ctrl(0x0010 << no, 1);
}
s32 sceSysregMsifIoDisable(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_io_ctrl(0x0010 << no, 0);
}
static s32 sub_0a80(void);
static s32 sub_0a8c(void);
s32 sceSysregAudioIoEnable(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_io_ctrl(0x0080 << no, 1);
}
s32 sceSysregAudioIoDisable(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_io_ctrl(0x0080 << no, 0);
}
s32 sceSysregSircsIoEnable(void) { return sysreg_io_ctrl(0x0400, 1); }
s32 sceSysregSircsIoDisable(void) { return sysreg_io_ctrl(0x0400, 0); }
s32 sceSysregAudioClkoutIoEnable(void) { return sysreg_io_ctrl(0x0800, 1); }
s32 sceSysregAudioClkoutIoDisable(void) { return sysreg_io_ctrl(0x0800, 0); }
s32 sceSysregAtahddIoEnable(void) { return sysreg_io_ctrl(0x4000, 1); }
s32 sceSysregAtahddIoDisable(void) { return sysreg_io_ctrl(0x4000, 0); }
s32 sceSysregUartIoEnable(s32 no)
{
	if ((u32)no >= 6) return 0x80000102;
	return sysreg_io_ctrl(0x00010000 << no, 1);
}
s32 sceSysregUartIoDisable(s32 no)
{
	if ((u32)no >= 6) return 0x80000102;
	return sysreg_io_ctrl(0x00010000 << no, 0);
}
s32 sceSysregSpiIoEnable(s32 no)
{
	if ((u32)no >= 6) return 0x80000102;
	return sysreg_io_ctrl(0x01000000 << no, 1);
}
s32 sceSysregSpiIoDisable(s32 no)
{
	if ((u32)no >= 6) return 0x80000102;
	return sysreg_io_ctrl(0x01000000 << no, 0);
}
s32 sceSysreg_driver_19F4C92D(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_io_ctrl(0x00400000 << no, 1);
}
s32 sceSysreg_driver_DA5B5ED9(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_io_ctrl(0x00400000 << no, 0);
}
static s32 sub_0ca8(void) { return sysreg_io_ctrl(0x08000000, 1); }
static s32 sub_0cb4(void) { return sysreg_io_ctrl(0x08000000, 0); }
static s32 sub_0cc0(void) { return sysreg_io_ctrl(0x10000000, 1); }
static s32 sub_0ccc(void) { return sysreg_io_ctrl(0x10000000, 0); }
s32 sceSysreg_driver_0607A4C4(void) { return sysreg_io_ctrl(0x40000000, 1); }
s32 sceSysreg_driver_803E5F37(void) { return sysreg_io_ctrl(0x40000000, 0); }

s32 sceSysregGpioIoEnable(s32 port)
{
	if ((u32)port >= 32) return 0x80000102;
	return sysreg_gpio_io_ctrl(1U << port, 1);
}
s32 sceSysregGpioIoDisable(s32 port)
{
	if ((u32)port >= 32) return 0x80000102;
	return sysreg_gpio_io_ctrl(1U << port, 0);
}

s32 sceSysreg_driver_33EE43F0(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_unk74_ctrl(1U << (no << 2), 0);
}
s32 sceSysreg_driver_6DA9347D(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_unk74_ctrl(1U << (no << 2), 1);
}
s32 sceSysreg_driver_9E2F8FD5(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_unk74_ctrl(2U << (no << 2), 0);
}
s32 sceSysreg_driver_4C0BED71(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_unk74_ctrl(2U << (no << 2), 1);
}
s32 sceSysreg_driver_1561BCD2(void) { return sysreg_unk74_ctrl(0x0100, 1); }
s32 sceSysreg_driver_9275DD37(void) { return sysreg_unk74_ctrl(0x0100, 0); }
s32 sceSysreg_driver_84E0F197(s32 no, s32 enable)
{
	if ((u32)no >= 2) return 0x80000102;
	return sysreg_unk74_ctrl(0x10000U << (no << 1), enable);
}

/* USB / USBhost / MSIF status & interrupts */
s32 sceSysregUsbGetConnectStatus(void)
{
	return REG32(0xBC100070) & 1;
}
s32 sceSysregUsbQueryIntr(void)
{
	return (REG32(0xBC100070) >> 1) & 0xF;
}
s32 sceSysregUsbAcquireIntr(s32 mask)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u32 v = (REG32(0xBC100070) >> 1) & 0xF;
	REG32(0xBC100070) = (mask & v) << 1;
	sceKernelCpuResumeIntrWithSync(intr);
	return v;
}
s32 sceSysregUsbhostQueryIntr(void)
{
	return (REG32(0xBC100070) >> 5) & 3;
}
s32 sceSysregUsbhostAcquireIntr(s32 mask)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u32 v = (REG32(0xBC100070) >> 5) & 3;
	REG32(0xBC100070) = (mask & v) << 5;
	sceKernelCpuResumeIntrWithSync(intr);
	return v;
}
s32 sceSysregMsifGetConnectStatus(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return (REG32(0xBC100070) >> ((no << 2) + 8)) & 1;
}
s32 sceSysregMsifQueryConnectIntr(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return (REG32(0xBC100070) >> ((no << 2) + 9)) & 7;
}
s32 sceSysregMsifAcquireConnectIntr(s32 no, s32 mask)
{
	if ((u32)no >= 2) return 0x80000102;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = (no << 2) + 9;
	u32 v = (REG32(0xBC100070) >> shift) & 7;
	REG32(0xBC100070) = (mask & v) << shift;
	sceKernelCpuResumeIntrWithSync(intr);
	return v;
}

/* Inter-processor & HW semaphore */
s32 sceSysregInterruptToOther(void)
{
	__asm__ volatile("sync");
	REG32(0xBC100044) = 1;
	__asm__ volatile("sync");
	return 0;
}
s32 sceSysregSemaTryLock(void)
{
	u32 cpuid;
	__asm__ volatile("mfc0 %0, $22" : "=r"(cpuid));
	u32 id = cpuid + 1;
	__asm__ volatile("sync");
	REG32(0xBC100048) = id;
	return ((REG32(0xBC100048) & 3) == id) ? 0 : -1;
}
s32 sceSysregSemaUnlock(void)
{
	__asm__ volatile("sync");
	REG32(0xBC100048) = 0;
	__asm__ volatile("sync");
	return 0;
}
u32 sceSysreg_driver_1E62714E(volatile u32 *addr)
{
	s32 intr = sceKernelCpuSuspendIntr();
	while (sceSysregSemaTryLock() < 0)
		;
	u32 val = *addr;
	sceSysregSemaUnlock();
	sceKernelCpuResumeIntr(intr);
	return val;
}
u32 sceSysreg_driver_554E97F7(volatile u32 *addr, u32 and_mask, u32 or_mask)
{
	s32 intr = sceKernelCpuSuspendIntr();
	while (sceSysregSemaTryLock() < 0)
		;
	u32 val = *addr;
	*addr = (val & and_mask) | or_mask;
	sceSysregSemaUnlock();
	sceKernelCpuResumeIntr(intr);
	return val;
}

/* Timer event, EDRAM, Tachyon, MasterPriv, Fuse */
s32 sceSysreg_driver_4841B2D2(s32 enable)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u32 val = REG32(0xBC10003C);
	s32 prev = val & 1;
	REG32(0xBC10003C) = (enable != 0) ? (val | 1) : (val & ~1U);
	sceKernelCpuResumeIntrWithSync(intr);
	return prev;
}
s32 sceSysregDoTimerEvent(s32 no, s32 mode)
{
	if ((u32)no >= 2) return 0x80000102;
	if ((u32)mode >= 4) return 0x80000107;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 val = REG32(0xBC10003C);
	u32 bit = 1U << no;
	if (mode == 0) {
		REG32(0xBC10003C) = val & ~bit;
	} else if (mode == 1) {
		REG32(0xBC10003C) = val | bit;
	} else if (mode == 2) {
		REG32(0xBC10003C) = val | bit;
		REG32(0xBC10003C) = val & ~bit;
	} else if (mode == 3) {
		REG32(0xBC10003C) = val & ~bit;
		REG32(0xBC10003C) = val | bit;
	}
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}
u32 sceSysregGetTachyonVersion(void)
{
	if (g_tachyonVer != 0xFFFFFFFF)
		return g_tachyonVer;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 raw = REG32(0xBC100040) & 0xFF000000;
	u32 ver = (raw != 0) ? (raw >> 8) : 0x00100000;
	g_tachyonVer = ver;
	sceKernelCpuResumeIntr(intr);
	return ver;
}
s32 sceSysregSetAwEdramSize(s32 enable)
{
	u32 ver = sceSysregGetTachyonVersion();
	if (ver <= 0x004FFFFF || ver == 0 || (ver & 0xFFF00000) == 0x00F00000)
		return 0;
	s32 intr = sceKernelCpuSuspendIntr();
	__asm__ volatile("sync");
	u32 val = REG32(0xBC100040);
	s32 prev = (val >> 8) & 1;
	REG32(0xBC100040) = (enable != 0) ? (val | 0x100) : (val & ~0x100U);
	sceKernelCpuResumeIntrWithSync(intr);
	return prev;
}
static s32 sub_1b70(s32 bit)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u8 mask = g_usbIoRefMask;
	s32 prev = (mask >> bit) & 1;
	if (mask == 0) {
		sceSysregUsbIoEnable();
		mask = g_usbIoRefMask;
	}
	g_usbIoRefMask = mask | (1U << bit);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
static s32 sub_1bf4(s32 bit)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u8 mask = g_usbIoRefMask;
	s32 prev = (mask >> bit) & 1;
	u8 next = mask & ~(1U << bit);
	g_usbIoRefMask = next;
	if (next == 0)
		sceSysregUsbIoDisable();
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysreg_driver_15786501(void)
{
	u32 ver = sceSysregGetTachyonVersion();
	if (ver <= 0x004FFFFF)
		return 0x80000004;
	if (ver > 0x007FFFFF)
		return sub_0cc0();
	return sub_1b70(1);
}
s32 sceSysreg_driver_80944C8A(void)
{
	u32 ver = sceSysregGetTachyonVersion();
	if (ver <= 0x004FFFFF)
		return 0x80000004;
	if (ver > 0x007FFFFF)
		return sub_0ccc();
	return sub_1bf4(1);
}
s32 sceSysreg_driver_4E5C86AA(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u32 v = REG32(0xBC10003C);
	REG32(0xBC10003C) = v ^ 1;
	__asm__ volatile("sync");
	REG32(0xBC10003C) = v;
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}
s32 sceSysregSetMasterPriv(u32 mask, s32 enable)
{
	__asm__ volatile("sync");
	u32 v = REG32(0xBC000044);
	REG32(0xBC000044) = (enable != 0) ? (v | mask) : (v & ~mask);
	__asm__ volatile("sync");
	return 0;
}
u64 sceSysregGetFuseId(void)
{
	u32 lo = REG32(0xBC100090);
	u32 hi = REG32(0xBC100094);
	return ((u64)hi << 32) | lo;
}
u32 sceSysregGetFuseConfig(void)
{
	return REG32(0xBC100098);
}
s32 sceSysreg_driver_6F3B6D7D(void)
{
	if (sceSysregGetTachyonVersion() > 0x007FFFFF)
		return sub_0ca8();
	return sub_1b70(0);
}
s32 sceSysreg_driver_87B61303(void)
{
	if (sceSysregGetTachyonVersion() > 0x007FFFFF)
		return sub_0cb4();
	return sub_1bf4(0);
}

/* PLL & Clock selects */
float sceSysregPllGetBaseFrequency(void)
{
	return g_pllBaseFreq;
}
s32 sceSysregPllUpdateFrequency(void)
{
	g_pllFreq = g_pllFreqTable[REG32(0xBC100068) & 0xF];
	return 0;
}
float sceSysregPllGetFrequency(void)
{
	return g_pllFreq;
}
s32 sceSysregPllGetOutSelect(void)
{
	return REG32(0xBC100068) & 0xF;
}
s32 sceSysregPllSetOutSelect(s32 sel)
{
	if (sel < 0 || (sel & 7) >= 6)
		return 0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 val = REG32(0xBC100068);
	s32 prev = val & 0xF;
	REG32(0xBC100068) = (val & ~0xFU) | sel | 0x80;
	g_pllFreq = g_pllFreqTable[REG32(0xBC100068) & 0xF];
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregMsifClkSelect(s32 no, s32 clk)
{
	if ((u32)no >= 2) return 0x80000102;
	if ((u32)clk >= 3) return 0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = no << 1;
	u32 val = REG32(0xBC10005C);
	s32 prev = (val >> shift) & 3;
	REG32(0xBC10005C) = (val & ~(3U << shift)) | ((clk & 3) << shift);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregMsifDelaySelect(s32 no, s32 delay)
{
	if ((u32)no >= 2) return 0x80000102;
	if ((u32)delay >= 8) return 0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = no << 2;
	u32 val = REG32(0xBC000050);
	s32 prev = (val >> shift) & 7;
	REG32(0xBC000050) = (val & ~(7U << shift)) | ((delay & 7) << shift);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysreg_driver_C2F3061F(s32 no, s32 val)
{
	if ((u32)no >= 2) return 0x80000102;
	if ((u32)val >= 4) return 0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = (no << 1) + 10;
	u32 reg = REG32(0xBC10005C);
	s32 prev = (reg >> shift) & 3;
	REG32(0xBC10005C) = (reg & ~(3U << shift)) | ((val & 3) << shift);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregApbTimerClkSelect(s32 no, s32 clk)
{
	if ((u32)no >= 4) return 0x80000102;
	if ((u32)clk >= 8) return 0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = no << 2;
	u32 reg = REG32(0xBC100060);
	s32 prev = (reg >> shift) & 7;
	REG32(0xBC100060) = (reg & ~(7U << shift)) | ((clk & 7) << shift);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregAudioClkSelect(s32 no, s32 clk)
{
	if ((u32)no >= 2) return 0x80000102;
	if ((u32)clk >= 2) return 0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = no + 16;
	u32 reg = REG32(0xBC100060);
	s32 prev = (reg >> shift) & 1;
	REG32(0xBC100060) = (reg & ~(1U << shift)) | ((clk & 1) << shift);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregSpiClkSelect(s32 no, s32 clk)
{
	if ((u32)no >= 6) return 0x80000102;
	if ((u32)clk >= 8) return 0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = no << 2;
	u32 reg = REG32(0xBC100064);
	s32 prev = (reg >> shift) & 7;
	REG32(0xBC100064) = (reg & ~(7U << shift)) | ((clk & 7) << shift);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysreg_driver_9746F3B2(s32 idx, s32 div)
{
	if ((u32)(div - 1) >= 3) return 0x80000102;
	if (div == 3)
		div = 0;
	u32 ver = sceSysregGetTachyonVersion();
	if (ver > 0x007FFFFF) {
		if ((u32)idx >= 3) return 0x80000102;
		if (idx == 2) {
			if (div != 1) return 0x80000102;
			idx = 0;
			div = 3;
		}
	} else {
		if ((u32)idx >= 2) return 0x80000102;
	}
	s32 intr = sceKernelCpuSuspendIntr();
	u32 reg = REG32(0xBC100060);
	s32 prev = (reg >> 20) & 7;
	if (prev == 3)
		prev = 1;
	else if (prev == 0)
		prev = 3;
	REG32(0xBC100060) = (reg & ~(7U << 20)) | ((idx << 22) | (div << 20));
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysreg_driver_434E8AF1(s32 sel)
{
	if ((u32)(sel + 1) >= 3) return 0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 reg = REG32(0xBC100060);
	s32 prev = (reg >> 24) & 1;
	if (sel >= 0)
		REG32(0xBC100060) = (reg & ~(1U << 24)) | ((sel & 1) << 24);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysreg_driver_6596EBC3(s32 no, s32 sel)
{
	if ((u32)no >= 6) return 0x80000102;
	if ((u32)sel >= 3) return 0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = no << 1;
	u32 reg = REG32(0xBC1000E8);
	s32 prev = (reg >> shift) & 3;
	REG32(0xBC1000E8) = (reg & ~(3U << shift)) | ((sel & 3) << shift);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregAtaClkSelect(s32 clk)
{
	if ((u32)clk >= 3) return 0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 reg = REG32(0xBC10005C);
	s32 prev = (reg >> 4) & 3;
	REG32(0xBC10005C) = (reg & ~(3U << 4)) | ((clk & 3) << 4);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregAtahddClkSelect(s32 clk)
{
	if ((u32)clk >= 3) return 0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 reg = REG32(0xBC10005C);
	s32 prev = (reg >> 8) & 3;
	REG32(0xBC10005C) = (reg & ~(3U << 8)) | ((clk & 3) << 8);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregAudioClkoutClkSelect(s32 clk)
{
	if ((u32)clk >= 2) return 0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 reg = REG32(0xBC100060);
	s32 prev = (reg >> 18) & 1;
	REG32(0xBC100060) = (reg & ~(1U << 18)) | ((clk & 1) << 18);
	sceKernelCpuResumeIntr(intr);
	return prev;
}

/* Sysreg sub-interrupts & CPU/Bus clock ratio */
s32 sceSysregRequestIntr(s32 cpu, s32 intr_no)
{
	if ((u32)cpu >= 2 || (u32)intr_no >= 32)
		return 0x80000102;
	sceSysreg_driver_554E97F7((volatile u32 *)(0xBFC00400 + (cpu << 2)), 0, 1U << intr_no);
	u32 my_cpu;
	__asm__ volatile("mfc0 %0, $22" : "=r"(my_cpu));
	if ((u32)cpu != my_cpu)
		sceSysregInterruptToOther();
	return 0;
}
static s32 sub_28a8(s32 intr_code, void *arg, void *frame)
{
	(void)arg;
	u32 my_cpu;
	__asm__ volatile("mfc0 %0, $22" : "=r"(my_cpu));
	volatile u32 *reg = (volatile u32 *)(0xBFC00400 + (my_cpu << 2));
	u32 pending = sceSysreg_driver_1E62714E(reg) & g_sysregSubIntrMask;
	if (pending == 0)
		return -1;
	u32 rot = g_sysregSubIntrRot;
	u32 rval = (pending >> rot) | (pending << (32 - rot));
	u32 lz = __builtin_clz(rval);
	u32 sub_intr = (rot - lz + 31) & 31;
	sceSysreg_driver_554E97F7(reg, ~(1U << sub_intr), 0);
	g_sysregSubIntrRot = (g_sysregSubIntrRot + 1) & 31;
	sceKernelCallSubIntrHandler(intr_code, sub_intr, (void *)sub_intr, frame);
	return -1;
}
static s32 sub_297c(s32 intr_code, s32 sub_intr)
{
	(void)intr_code;
	g_sysregSubIntrMask |= (1U << sub_intr);
	return 0;
}
static s32 sub_299c(s32 intr_code, s32 sub_intr)
{
	(void)intr_code;
	g_sysregSubIntrMask &= ~(1U << sub_intr);
	return 0;
}
static s32 sub_29c0(s32 intr_code, s32 sub_intr, u32 *prev)
{
	(void)intr_code;
	if (prev != NULL)
		*prev = (g_sysregSubIntrMask >> sub_intr) & 1;
	g_sysregSubIntrMask &= ~(1U << sub_intr);
	return 0;
}
static s32 sub_29f8(s32 intr_code, s32 sub_intr, u32 prev)
{
	(void)intr_code;
	g_sysregSubIntrMask |= (prev << sub_intr);
	return 0;
}
static s32 sub_2a14(s32 intr_code, s32 sub_intr)
{
	(void)intr_code;
	u32 my_cpu;
	__asm__ volatile("mfc0 %0, $22" : "=r"(my_cpu));
	volatile u32 *reg = (volatile u32 *)(0xBFC00400 + (my_cpu << 2));
	return (sceSysreg_driver_1E62714E(reg) >> sub_intr) & 1;
}
static u32 g_sysregSubIntrCb[7] = {
	0, (u32)sub_299c, (u32)sub_297c, (u32)sub_299c,
	(u32)sub_29c0, (u32)sub_29f8, (u32)sub_2a14
};
static u32 g_sysregIntrOpt[3] = { 0x0C, 0x20, (u32)g_sysregSubIntrCb };

s32 sceSysregIntrInit(void)
{
	sceKernelRegisterIntrHandler(31, 2, sub_28a8, NULL, g_sysregIntrOpt);
	sceKernelEnableIntr(31);
	return 0;
}
s32 sceSysregIntrEnd(void)
{
	sceKernelReleaseIntrHandler(31);
	return 0;
}
s32 sceSysreg_driver_BE1FF8BD(s32 intr_no)
{
	if ((u32)intr_no >= 32) return 0x80000102;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 mask = g_sysregSubIntrMask;
	s32 prev = (mask >> intr_no) & 1;
	g_sysregSubIntrMask = mask | (1U << intr_no);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysreg_driver_BD7B035B(s32 intr_no)
{
	if ((u32)intr_no >= 32) return 0x80000102;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 mask = g_sysregSubIntrMask;
	s32 prev = (mask >> intr_no) & 1;
	g_sysregSubIntrMask = mask & ~(1U << intr_no);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysreg_driver_20B1D0A9(s32 cpu, s32 intr_no)
{
	if ((u32)cpu >= 2 || (u32)intr_no >= 32)
		return 0x80000102;
	u32 old = sceSysreg_driver_554E97F7((volatile u32 *)(0xBFC00400 + (cpu << 2)), ~(1U << intr_no), 0);
	return (old >> intr_no) & 1;
}

s32 sceSysreg_driver_3F6F2CC7(s32 num, s32 den)
{
	if (num >= 512 || den < num || den == 0)
		return 0x800001FE;
	REG32(0xBC200000) = ((num & 0x1FF) << 16) | (den & 0x1FF);
	__asm__ volatile("sync");
	return 0;
}
s32 sceSysreg_driver_2458B6AC(s32 *num, s32 *den)
{
	if (num != NULL)
		*num = (REG32(0xBC200000) >> 16) & 0x1FF;
	if (den != NULL)
		*den = REG32(0xBC200000) & 0x1FF;
	return 0;
}
s32 sceSysreg_driver_63E1EE9C(s32 num, s32 den)
{
	if (num >= 512 || den < num || den == 0)
		return 0x800001FE;
	REG32(0xBC200004) = ((num & 0x1FF) << 16) | (den & 0x1FF);
	__asm__ volatile("sync");
	return 0;
}
s32 sceSysreg_driver_7CF05E81(s32 *num, s32 *den)
{
	if (num != NULL)
		*num = (REG32(0xBC200004) >> 16) & 0x1FF;
	if (den != NULL)
		*den = REG32(0xBC200004) & 0x1FF;
	return 0;
}
s32 sceSysreg_driver_B413B041(float freq)
{
	float pll = sceSysregPllGetFrequency();
	s32 ratio = (s32)((freq * 511.0f) / pll);
	if (ratio < 1) ratio = 1;
	if (ratio > 511) ratio = 511;
	REG32(0xBC200000) = ((ratio & 0x1FF) << 16) | 0x1FF;
	__asm__ volatile("sync");
	return 0;
}
float sceSysreg_driver_73EBC752(void)
{
	float pll = sceSysregPllGetFrequency();
	u32 reg = REG32(0xBC200000);
	u32 num = (reg >> 16) & 0x1FF;
	u32 den = reg & 0x1FF;
	return (pll * (float)num) / (float)den;
}
s32 sceSysreg_driver_85B74FDA(float freq)
{
	float pll = sceSysregPllGetFrequency();
	s32 ratio = (s32)((freq * 1022.0f) / pll);
	if (ratio < 1) ratio = 1;
	if (ratio > 511) ratio = 511;
	REG32(0xBC200004) = ((ratio & 0x1FF) << 16) | 0x1FF;
	__asm__ volatile("sync");
	return 0;
}
float sceSysreg_driver_96D74557(void)
{
	float pll = sceSysregPllGetFrequency();
	u32 reg = REG32(0xBC200004);
	u32 num = (reg >> 16) & 0x1FF;
	u32 den = (reg & 0x1FF) << 1;
	return (pll * (float)num) / (float)den;
}

static int sysreg_event_handler(int ev_id, char *ev_name, void *param, int *result)
{
	(void)ev_id;
	(void)ev_name;
	(void)param;
	(void)result;
	return 0;
}
static SceSysEventHandler g_sysregEvent = {
	sizeof(SceSysEventHandler),
	"SceSysreg",
	0x00FFFF00,
	sysreg_event_handler,
	0, 0, NULL,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};
static s32 sub_16c8(SceSize args)
{
	(void)args;
	sceKernelUnregisterSysEventHandler(&g_sysregEvent);
	sceSysregPllUpdateFrequency();
	sceKernelRegisterSysEventHandler(&g_sysregEvent);
	return 0;
}
static s32 sub_1708(SceSize args)
{
	(void)args;
	return 0;
}

/* =========================================================================
 * Part 2: sceDdr_driver (14 exported functions)
 * ========================================================================= */

static s32 g_ddrFreqIdx = -1;
static u32 g_ddrSavedParam[16] = {
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1
};
static const u32 g_ddrCfgTable[8][2] = {
	{ 1, 0 }, { 9, 0 }, { 0x11, 0 }, { 0x19, 0 },
	{ 0, 0 }, { 0, 0 }, { 0, 0 },    { 0, 0 }
};

s32 sceDdrFlush(s32 flag)
{
	__asm__ volatile("sync");
	u32 m = flag & 0xF;
	REG32(0xBD000004) = m;
	while ((REG32(0xBD000004) & m) != 0)
		;
	return 0;
}
s32 sceDdr_driver_E0A39D3E(s32 val)
{
	REG32(0xBD00002C) = (val >= 1) ? ((u32)val | 0x80000000U) : 0;
	__asm__ volatile("sync");
	return 0;
}
s32 sceDdr_driver_00E36648(void)
{
	s32 v = (s32)REG32(0xBD00002C);
	return (v < 0) ? (v & 0x7FFFFFFF) : 0;
}
s32 sceDdr_driver_95B9A692(void)
{
	s32 count = 1;
	s32 i = 64;
	while (i > 0) {
		REG32(0xBD000040) = 0x2B;
		(void)REG32(0xBD000040);
		if ((REG32(0xBD000040) & 0x100) != 0)
			break;
		i--;
		if (i != 0)
			count++;
	}
	for (i = count + 1; i > 0; i--) {
		REG32(0xBD000040) = 0x2A;
		(void)REG32(0xBD000040);
	}
	REG32(0xBD000040) = 0x20;
	__asm__ volatile("sync");
	return count;
}
void sceDdr_driver_77CD1FB3(s32 a0, s32 a1, s32 a2)
{
	while (((REG32(0xBD000020) >> 16) & 1) != 0)
		;
	REG32(0xBD000024) = ((a0 & 1) << 10) | (a1 << 16) | (a2 & 0x7BFF);
	__asm__ volatile("sync");
	REG32(0xBD000020) = a0 | 0x8000;
	__asm__ volatile("sync");
	while (((REG32(0xBD000020) >> 16) & 1) != 0)
		;
}
void sceDdr_driver_3D50DEC9(s32 idx)
{
	u32 v0 = g_ddrCfgTable[idx & 7][0];
	u32 v1 = g_ddrCfgTable[idx & 7][1];
	REG32(0xBD000030) = (REG32(0xBD000030) & ~0x31U) | v0;
	REG32(0xBD000038) = v1;
	__asm__ volatile("sync");
}
s32 sceDdr_driver_2AA39A80(void)
{
	u32 v = REG32(0xBD000030);
	return ((v & 1) | ((v >> 3) & 6)) - 1;
}
void sceDdr_driver_397756C0(s32 a0, s32 a1)
{
	sceDdr_driver_77CD1FB3(0x25, 0, 0);
	sceDdr_driver_77CD1FB3(0x22, 0, 0);
	sceDdr_driver_77CD1FB3(0x22, 0, 0);
	sceDdr_driver_77CD1FB3(0x20, 0, (a0 << 4) | 1);
	sceDdr_driver_77CD1FB3(0x20, 2, a1 << 5);
}
void sceDdr_driver_0BAAE4C5(s32 a0, s32 a1)
{
	(void)a0;
	sceDdr_driver_77CD1FB3(0x20, 2, a1 << 5);
}
void sceDdr_driver_6DE74D80(s32 idx, s32 cas)
{
	static const u32 ddr_timing[8][2] = {
		{ 0x11110000, 0x0006000F },
		{ 0x11110000, 0x0007000F },
		{ 0x11110000, 0x0007000F },
		{ 0x11110400, 0x00070000 },
		{ 0x11110400, 0x00070008 },
		{ 0x11110400, 0x0007000A },
		{ 0x00000000, 0x00000000 },
		{ 0x00000000, 0x00000000 }
	};
	u32 v0 = ddr_timing[idx & 7][0] | ((cas - 3) << 12);
	u32 v1 = ddr_timing[idx & 7][1];
	REG32(0xBD000030) = (REG32(0xBD000030) & 0x31) | v0;
	REG32(0xBD000044) = v1 & 0xFFFF;
	REG32(0xBD000034) = v1 >> 16;
	__asm__ volatile("sync");
}
void sceDdr_driver_9F882141(s32 a0, s32 a1, s32 a2, s32 a3)
{
	sceDdr_driver_3D50DEC9(a2);
	sceDdr_driver_6DE74D80(a0, a1);
	sceDdr_driver_397756C0(a1, (a3 >= 0) ? a3 : 1);
}
s32 sceDdr_driver_4F30BFE8(s32 count)
{
	s32 i;
	for (i = 64; i > 0; i--) {
		REG32(0xBD000040) = 0x2B;
		(void)REG32(0xBD000040);
	}
	for (i = count + 1; i > 0; i--) {
		REG32(0xBD000040) = 0x2A;
		(void)REG32(0xBD000040);
	}
	return 0;
}
s32 sceDdr_driver_F1005384(s32 a0, s32 a1)
{
	REG32(0xBD000040) = a1 | 0x10;
	(void)REG32(0xBD000040);
	REG32(0xBD000040) = a0 | 0x10;
	(void)REG32(0xBD000040);
	REG32(0xBD000040) = 0x20;
	__asm__ volatile("sync");
	return 0;
}
s32 sceDdr_driver_CF07B9E2(s32 idx)
{
	if ((u32)(idx | 8) >= 14)
		return 0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	if (g_ddrFreqIdx >= 0)
		g_ddrSavedParam[g_ddrFreqIdx] = sceDdr_driver_95B9A692();
	g_ddrFreqIdx = idx;
	sceKernelDcacheWritebackAll();
	u32 mpriv = REG32(0xBC000044);
	REG32(0xBC000044) = mpriv & 0xC000FFFFU;
	s32 flush_flag = ((REG32(0xBC10004C) & 4) != 0) ? 5 : 0xF;
	sceDdrFlush(flush_flag);
	sceSysregPllUpdateFrequency();
	REG32(0xBC000044) = mpriv;
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}
static s32 sub_7230(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	sceKernelDmaRegisterDdrFlush(sceDdrFlush);
	return 0;
}
static s32 sub_727c(SceSize args)
{
	(void)args;
	sceKernelDmaRegisterDdrFlush(NULL);
	return 0;
}

/* =========================================================================
 * Part 3: sceDmac, sceDmacplus_driver & sceKermitMemory_driver
 * ========================================================================= */

u32 sceDmacplus_driver_var_374689B0 = 1;
u32 sceDmacplus_driver_var_9D3DDDF0 = 0;
u32 sceDmacplus_driver_var_AA2E7E54 = 0;
u32 sceDmacplus_driver_var_C10B947A = 0;

static u32 g_lcdcDmacEnabled;
static u32 g_lcdcDmacFrameBuf;
static u32 g_lcdcDmacFrameWidth;
static u32 g_lcdcDmacDisplayWidth;
static u32 g_lcdcDmacPixelFormat;
static s32 g_sc128SemaId;
static s32 g_sc128EvFlagId;
static s32 g_kermitDmacMutexId;

static s32 dmacplus_kermit_cmd56a(u32 enabled, u32 fb, u32 width, u32 disp_width)
{
	u8 buf[176];
	SceKermitRequest *req_cached = (SceKermitRequest *)(((u32)buf + 63) & ~63U);
	sceKernelDcacheInvalidateRange(req_cached, 64);
	SceKermitRequest *req = (SceKermitRequest *)KERMIT_UNCACHED(req_cached);
	u64 resp = 0;
	req->cmd = 0x56A;
	req->args[0] = enabled;
	req->args[1] = fb;
	req->args[2] = width;
	req->args[3] = disp_width;
	s32 res = sceKermit_driver_36666181(req, KERMIT_MODE_LOWIO, 0x56A, 4, 0, &resp);
	return (res != 0) ? res : (s32)resp;
}

s32 sceDmacplus_driver_BF0DB45E(s32 a0, s32 a1)
{
	(void)a0;
	(void)a1;
	return 0;
}

s32 sceDmacplus_driver_E9B746F9(void)
{
	g_lcdcDmacEnabled = 0;
	return 0;
}

s32 sceDmacplus_driver_F652F09D(void)
{
	g_lcdcDmacEnabled = 0;
	return dmacplus_kermit_cmd56a(0, g_lcdcDmacFrameBuf, g_lcdcDmacFrameWidth, g_lcdcDmacDisplayWidth);
}

s32 sceDmacplus_driver_33CD36DD(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u32 prev = g_lcdcDmacEnabled;
	g_lcdcDmacEnabled = 1;
	dmacplus_kermit_cmd56a(1, g_lcdcDmacFrameBuf, g_lcdcDmacFrameWidth, g_lcdcDmacDisplayWidth);
	sceKernelCpuResumeIntr(intr);
	return (prev != 0) ? 0x80000020 : 0;
}

s32 sceDmacplus_driver_88ACB6F1(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u32 prev = g_lcdcDmacEnabled;
	g_lcdcDmacEnabled = 0;
	dmacplus_kermit_cmd56a(0, g_lcdcDmacFrameBuf, g_lcdcDmacFrameWidth, g_lcdcDmacDisplayWidth);
	sceKernelCpuResumeIntr(intr);
	return (prev == 0) ? 0x80000021 : 0;
}

static s32 sub_0a80(void)
{
	return sceDmacplus_driver_33CD36DD();
}

static s32 sub_0a8c(void)
{
	return sceDmacplus_driver_88ACB6F1();
}

s32 sceDmacplus_driver_D126494B(u32 fb)
{
	s32 intr = sceKernelCpuSuspendIntr();
	g_lcdcDmacFrameBuf = fb;
	dmacplus_kermit_cmd56a(g_lcdcDmacEnabled, fb, g_lcdcDmacFrameWidth, g_lcdcDmacDisplayWidth);
	sceKernelCpuResumeIntr(intr);
	return 0;
}

s32 sceDmacplus_driver_E9B27A24_5FCF43BD(u32 disp_width, u32 width, u32 pformat);
s32 sceDmacplus_driver_5FCF43BD(u32 disp_width, u32 width, u32 pformat)
{
	if (width == 0 || disp_width > width || pformat >= 4)
		return 0x80000103;
	s32 intr = sceKernelCpuSuspendIntr();
	g_lcdcDmacDisplayWidth = disp_width;
	g_lcdcDmacFrameWidth = width;
	g_lcdcDmacPixelFormat = pformat;
	dmacplus_kermit_cmd56a(g_lcdcDmacEnabled, g_lcdcDmacFrameBuf, width, disp_width);
	sceKernelCpuResumeIntr(intr);
	return 0;
}

s32 sceDmacplus_driver_9C492B9B(u32 *disp_width, u32 *width, u32 *pformat)
{
	if (disp_width != NULL) *disp_width = g_lcdcDmacDisplayWidth;
	if (width != NULL) *width = g_lcdcDmacFrameWidth;
	if (pformat != NULL) *pformat = g_lcdcDmacPixelFormat;
	return 0;
}

s32 sceDmacplus_driver_CCCF6069(u32 a0)
{
	(void)a0;
	return 0;
}

s32 sceDmacplus_driver_ED849260(void *a0)
{
	(void)a0;
	return 0;
}

s32 sceDmacplus_driver_9EBF7D3E(void *a0)
{
	(void)a0;
	return 0;
}

s32 sceDmacplus_driver_47D400CB(void *a0)
{
	(void)a0;
	return 0;
}

s32 sceDmacplus_driver_DBD8526F(void *a0)
{
	(void)a0;
	return 0;
}

s32 sceDmacplus_driver_282CA0D7(void *a0)
{
	(void)a0;
	return 0;
}

s32 sceDmacplus_driver_A3AA8D00(void *a0)
{
	(void)a0;
	return 0;
}

s32 sceDmacplus_driver_B269EAC9(void *a0)
{
	(void)a0;
	return 0;
}

s32 sceDmacplus_driver_6945F1D3(void *a0)
{
	(void)a0;
	return 0;
}

s32 sceDmacplus_driver_B0EC610D(void *lli)
{
	(void)lli;
	return 0;
}

s32 sceDmacplus_driver_2C5FC1E3(void *dst, const void *src, u32 size)
{
	if (size == 0)
		return 0x80000104;
	if (dst == NULL || src == NULL)
		return 0x80000103;
	memcpy(dst, src, size);
	return 0;
}

s32 sceDmacplus_driver_D57D5BC2(void *dst, const void *src, u32 size)
{
	return sceDmacplus_driver_2C5FC1E3(dst, src, size);
}

/* Push PSP memory into shared Kermit mirror before host read */
int sceKermitMemory_driver_80E1240A(const void *addr, u32 size)
{
	u32 uaddr = (u32)addr;
	u32 paddr = uaddr & 0x1FFFFFFF;
	u32 base_off;
	if (paddr >= 0x04000000 && paddr <= 0x0BFFFFFF) {
		u32 masked = uaddr & ~((3U << 21) | (0x3FU << 26));
		base_off = 0x01E00000 + masked;
	} else if ((paddr - 0x00010000) < 0x4000) {
		base_off = 0x01D00000 + (uaddr & 0x03FFFFFF);
	} else {
		return 0;
	}
	u32 kaddr = (0xAA000000 + base_off) & ~(3U << 29);
	const u8 *src = (const u8 *)addr;
	u8 *dst = (u8 *)kaddr;
	if (size < 0x3000) {
		sceKernelMemcpy(dst, src, size);
		sceKernelDcacheWritebackInvalidateRange(dst, size);
		return 0;
	}
	u32 misalign = kaddr & 0x3F;
	if (misalign != 0) {
		u32 head = 0x40 - misalign;
		sceKernelMemcpy(dst, src, head);
		sceKernelDcacheWritebackInvalidateRange(dst, head);
		dst += head;
		src += head;
		size -= head;
	}
	u32 body = size & ~0x3FU;
	u32 tail = size - body;
	sceKernelDcacheWritebackRange(src, body);
	sceDmacplus_driver_2C5FC1E3(dst, src, body);
	if (tail != 0) {
		sceKernelMemcpy(dst + body, src + body, tail);
		sceKernelDcacheWritebackInvalidateRange(dst + body, tail);
	}
	return 0;
}

/* Pull shared Kermit mirror back into PSP memory after host write */
int sceKermitMemory_driver_90B662D0(void *addr, u32 size)
{
	u32 uaddr = (u32)addr;
	u32 paddr = uaddr & 0x1FFFFFFF;
	u32 base_off;
	if (paddr >= 0x04000000 && paddr <= 0x0BFFFFFF) {
		u32 masked = uaddr & ~((3U << 21) | (0x3FU << 26));
		base_off = 0x01E00000 + masked;
	} else if ((paddr - 0x00010000) < 0x4000) {
		base_off = 0x01D00000 + (uaddr & 0x03FFFFFF);
	} else {
		return 0;
	}
	u32 kaddr = (0xAA000000 + base_off) & ~(3U << 29);
	sceKernelDcacheInvalidateRange((const void *)kaddr, size);
	const u8 *src = (const u8 *)kaddr;
	u8 *dst = (u8 *)addr;
	if (size < 0x3000) {
		sceKernelMemcpy(dst, src, size);
		sceKernelDcacheWritebackRange(dst, size);
		return 0;
	}
	u32 misalign = kaddr & 0x3F;
	if (misalign != 0) {
		u32 head = 0x40 - misalign;
		sceKernelMemcpy(dst, src, head);
		sceKernelDcacheWritebackRange(dst, head);
		src += head;
		dst += head;
		size -= head;
	}
	u32 body = size & ~0x3FU;
	u32 tail = size - body;
	sceDmacplus_driver_2C5FC1E3(dst, src, body);
	sceKernelDcacheInvalidateRange(dst, body);
	if (tail != 0) {
		sceKernelMemcpy(dst + body, src + body, tail);
		sceKernelDcacheWritebackRange(dst + body, tail);
	}
	return 0;
}

/* Set Kermit request argument and optionally push buffer */
void sceKermitMemory_driver_AAF047AC(SceKermitRequest *req, int arg_idx, void *addr, u32 size, int copy_flag)
{
	if ((copy_flag & 1) != 0)
		sceKermitMemory_driver_80E1240A(addr, size);
	if (arg_idx < 14)
		*(u32 *)((u8 *)req + 0x10 + (arg_idx << 3)) = (u32)addr;
}

static s32 sub_2f98(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	sub_0280();
	g_sc128SemaId = sceKernelCreateSema("SceDmacplusSc128", 1, 1, 1, NULL);
	g_sc128EvFlagId = sceKernelCreateEventFlag("SceDmacplusSc128", 0x201, 0, NULL);
	sceKernelMemset((void *)0xABD38000, 0, 0x8000);
	g_kermitDmacMutexId = sceKernelCreateMutex("SceKermitDmacplus", 0x100, 0, NULL);
	return 0;
}

static s32 sub_3024(SceSize args)
{
	(void)args;
	if (g_sc128SemaId > 0)
		sceKernelDeleteSema(g_sc128SemaId);
	if (g_sc128EvFlagId > 0)
		sceKernelDeleteEventFlag(g_sc128EvFlagId);
	sub_028c();
	return 0;
}

/* =========================================================================
 * Part 4: sceLcdc_driver (20 exported functions)
 * ========================================================================= */

typedef struct {
	u16 fPrch;
	u16 bPrch;
	u16 sync;
	u16 res;
} SceLcdcModeline;

typedef struct {
	s32 id;
	float sync;
	u8 clk[2];
	u8 syncDiff;
	u8 flags;
	SceLcdcModeline x;
	SceLcdcModeline y;
} SceLcdcDispMode;

typedef struct {
	s32 id;
	s16 xres, v1_6, v1_8, yres, v1_12, v1_14;
} SceLcdcUnkElem2;

typedef struct {
	s32 v2_0;
	s32 syncDiff;
	s32 v2_8;
	s32 unused1;
	s32 xbPrch, xsync, xfPrch, xres;
	s32 ybPrch, ysync, yfPrch, yres;
	s32 hpc;
	s32 vpc;
	s32 unused3[2];
	s32 shiftY, shiftX;
	s32 realX, realY;
	s32 v2_80;
	s32 unused4[7];
	s32 v2_112;
} SceLcdcHwController;

typedef struct {
	s32 v3_0;
	s32 mobo;
	s32 v3_8;
	s32 scale;
	s32 dispId;
	s32 scaledXres;
	s32 yres;
	s32 v3_28;
	SceLcdcHwController *hwCtl;
	SceLcdcDispMode disp;
	s32 v3_64;
	s32 shiftX, shiftY;
	s32 realX, realY;
	s32 tabId;
	s32 v3_88;
	s32 hwData[7];
} SceLcdc;

static SceLcdc g_lcdc;

static SceLcdcDispMode g_lcdcDispModeList[13] = {
	{  0,  9000000.0f, { 0, 3 }, 0, 0, {  2, 41,  2, 480 }, { 10, 2,  2, 272 } },
	{  1, 27000000.0f, { 0, 1 }, 1, 0, { 16, 61, 61, 720 }, {  9, 6, 30, 480 } },
	{  2, 25175000.0f, { 1, 1 }, 1, 0, { 32, 96, 32, 640 }, { 10, 2, 33, 480 } },
	{  3, 27000000.0f, { 0, 1 }, 1, 0, { 61, 96, 61, 640 }, { 10, 2, 33, 480 } },
	{  4, 13500000.0f, { 0, 2 }, 1, 0, { 16, 61, 61, 720 }, {  5, 3, 14, 240 } },
	{  5, 13500000.0f, { 0, 2 }, 1, 0, { 16, 61, 61, 720 }, {  6, 3, 14, 240 } },
	{  6, 13500000.0f, { 0, 2 }, 1, 0, { 16, 61, 61, 720 }, {  3, 3, 14, 505 } },
	{ 11,  9000000.0f, { 0, 3 }, 0, 0, {  3, 41,  2, 480 }, { 10, 2,  2, 272 } },
	{ 12, 27000000.0f, { 0, 1 }, 1, 0, { 16, 61, 61, 720 }, { 10, 6, 30, 480 } },
	{ 14,  9000000.0f, { 0, 2 }, 0, 0, { 10, 51,  4, 320 }, {  5, 5,  5, 180 } },
	{ 15, 18000000.0f, { 1, 1 }, 0, 0, {  2, 41,  2, 480 }, { 10, 2,  2, 272 } },
	{ 16, 18000000.0f, { 1, 1 }, 0, 0, {  3, 41,  2, 480 }, { 10, 2,  2, 272 } },
	{ 17, 13500000.0f, { 2, 4 }, 1, 7, { 16, 61, 61, 720 }, { 10, 6, 30, 479 } }
};

static SceLcdcUnkElem2 g_lcdcTab[8] = {
	{ 1, 0x02BA, 0x0AFC, 0x0580, 0x0168, 0x0C18, 0x060C },
	{ 2, 0x0298, 0x0B90, 0x05C8, 0x0156, 0x0CB8, 0x065C },
	{ 3, 0x0274, 0x0C34, 0x061C, 0x0144, 0x0D70, 0x06B8 },
	{ 4, 0x0251, 0x0CEC, 0x0678, 0x0132, 0x0E38, 0x071C },
	{ 5, 0x02BA, 0x0AFC, 0x0580, 0x01E0, 0x0910, 0x0488 },
	{ 6, 0x0298, 0x0B90, 0x05C8, 0x01C8, 0x098C, 0x04C4 },
	{ 7, 0x0274, 0x0C34, 0x061C, 0x01B0, 0x0A14, 0x0508 },
	{ 8, 0x0251, 0x0CEC, 0x0678, 0x0198, 0x0AAC, 0x0554 }
};

static int lcdc_event_handler(int ev_id, char *ev_name, void *param, int *result);
s32 sceLcdc_driver_5DF2F52A(void);

static SceSysEventHandler g_lcdcEvent = {
	sizeof(SceSysEventHandler),
	"SceLcdc",
	0x00FFFF00,
	lcdc_event_handler,
	0, 0, NULL,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static s32 sub_64d8(void)
{
	sceSysregApbTimerClkDisable(0);
	sceSysregApbTimerClkSelect(0, 4);
	sceSysregApbTimerClkEnable(0);
	while ((REG32(0xBE700020) & 4) != 0)
		;
	REG32(0xBE700000) = 0;
	while ((REG32(0xBE700020) & 4) != 0)
		;
	while ((REG32(0xBE700020) & 2) != 0)
		;
	REG32(0xBE700004) = 0;
	while ((REG32(0xBE700020) & 8) != 0)
		;
	REG32(0xBE70000C) = -1;
	while ((REG32(0xBE700020) & 0x10) != 0)
		;
	REG32(0xBE700010) = 0;
	while ((REG32(0xBE700020) & 0x20) != 0)
		;
	REG32(0xBE700014) = 0;
	while ((REG32(0xBE700020) & 0x3A) != 0)
		;
	REG32(0xBE700000) = 1;
	while ((REG32(0xBE700020) & 4) != 0)
		;
	sceSysregApbTimerClkDisable(0);
	sceSysregApbTimerClkSelect(0, 6);
	sceSysregApbTimerClkEnable(0);
	REG32(0xBE700024) = 1;
	return 0;
}

static s32 sub_5db4(void)
{
	memset(&g_lcdc, 0, sizeof(g_lcdc));
	g_lcdc.v3_0 = 1;
	sub_0768();
	sub_0a80();
	g_lcdc.scale = 3;
	g_lcdc.mobo = 0x00600000;
	g_lcdc.v3_8 = 1;
	g_lcdcDispModeList[8].clk[1] = 1;
	g_lcdcDispModeList[3].id = -1;
	g_lcdcDispModeList[0].clk[1] = 1;
	g_lcdcDispModeList[1].clk[0] = 1;
	g_lcdcDispModeList[1].clk[1] = 1;
	g_lcdcDispModeList[2].id = -1;
	g_lcdcDispModeList[4].clk[0] = 1;
	g_lcdcDispModeList[4].clk[1] = 1;
	g_lcdcDispModeList[5].clk[0] = 1;
	g_lcdcDispModeList[5].clk[1] = 1;
	g_lcdcDispModeList[6].clk[0] = 1;
	g_lcdcDispModeList[6].clk[1] = 1;
	g_lcdcDispModeList[7].clk[1] = 1;
	g_lcdcDispModeList[8].clk[0] = 1;
	REG32(0xBE140070) = 1;
	g_lcdc.hwCtl = (SceLcdcHwController *)0xBE140000;
	sceLcdc_driver_5DF2F52A();
	sceKernelRegisterSysEventHandler(&g_lcdcEvent);
	return 0;
}

static s32 sub_5e8c(void)
{
	sub_0774();
	sceSysreg_driver_9746F3B2(0, g_lcdcDispModeList[0].clk[1]);
	sub_0768();
	if (g_lcdc.mobo > 0x7FFFFF) {
		REG32(0xBE140000) = 0;
		REG32(0xBE140200) = 1;
	}
	g_lcdc.hwCtl->syncDiff = g_lcdc.disp.syncDiff;
	if (g_lcdc.mobo > 0x7FFFFF) {
		if ((REG32(0xBE1401A0) & 1) == 0 && (g_lcdc.disp.flags & 1) != 0)
			sub_64d8();
		REG32(0xBE1401A0) = g_lcdc.disp.flags & 0xFE;
		s32 i;
		for (i = 0; i < 7; i++)
			REG32(0xBE140180 + i * 4) = g_lcdc.hwData[i];
	}
	g_lcdc.hwCtl->v2_8 = g_lcdc.v3_28;
	g_lcdc.hwCtl->xbPrch = g_lcdc.disp.x.bPrch;
	g_lcdc.hwCtl->xsync = g_lcdc.disp.x.sync;
	g_lcdc.hwCtl->xfPrch = g_lcdc.disp.x.fPrch;
	g_lcdc.hwCtl->xres = g_lcdc.disp.x.res;
	g_lcdc.hwCtl->ybPrch = g_lcdc.disp.y.bPrch;
	g_lcdc.hwCtl->ysync = g_lcdc.disp.y.sync;
	g_lcdc.hwCtl->yfPrch = g_lcdc.disp.y.fPrch;
	g_lcdc.hwCtl->yres = g_lcdc.disp.y.res;
	g_lcdc.hwCtl->shiftY = g_lcdc.shiftY;
	g_lcdc.hwCtl->shiftX = g_lcdc.shiftX;
	g_lcdc.hwCtl->realX = g_lcdc.realX;
	g_lcdc.hwCtl->realY = g_lcdc.realY;
	if (g_lcdc.mobo > 0x7FFFFF)
		REG32(0xBE1401A0) = g_lcdc.disp.flags;
	sceKermit_driver_D69C50BB();
	if (g_lcdc.v3_8 != 0)
		g_lcdc.hwCtl->v2_112 = 1;
	sub_0774();
	if (g_lcdc.mobo > 0x7FFFFF) {
		sceSysreg_driver_9746F3B2(g_lcdc.disp.clk[0], 1);
		REG32(0xBE1401B0) = g_lcdc.disp.clk[1] - 1;
	} else {
		sceSysreg_driver_9746F3B2(g_lcdc.disp.clk[0], g_lcdc.disp.clk[1]);
	}
	sub_0768();
	if (g_lcdc.v3_0 != 0)
		sub_0a80();
	return 0;
}

s32 sceLcdc_driver_5DF2F52A(void)
{
	g_lcdc.disp.syncDiff = g_lcdc.hwCtl->syncDiff & 0xFF;
	if (g_lcdc.mobo > 0x7FFFFF) {
		g_lcdc.disp.flags = REG32(0xBE1401A0) & 0xFF;
		s32 i;
		for (i = 0; i < 7; i++)
			g_lcdc.hwData[i] = REG32(0xBE140180 + i * 4);
	}
	g_lcdc.v3_28 = g_lcdc.hwCtl->v2_8;
	s32 v28 = g_lcdc.hwCtl->xres;
	s32 v32 = g_lcdc.hwCtl->ybPrch;
	s32 v36 = g_lcdc.hwCtl->ysync;
	s32 v40 = g_lcdc.hwCtl->yfPrch;
	s32 v44 = g_lcdc.hwCtl->yres;
	g_lcdc.disp.x.bPrch = g_lcdc.hwCtl->xbPrch & 0xFFFF;
	g_lcdc.shiftY = g_lcdc.hwCtl->shiftY;
	g_lcdc.disp.x.sync = g_lcdc.hwCtl->xsync & 0xFFFF;
	g_lcdc.shiftX = g_lcdc.hwCtl->shiftX;
	g_lcdc.disp.x.fPrch = g_lcdc.hwCtl->xfPrch & 0xFFFF;
	s32 v72 = g_lcdc.hwCtl->realX;
	g_lcdc.realX = v72;
	s32 v76 = g_lcdc.hwCtl->realY;
	g_lcdc.disp.x.res = v28 & 0xFFFF;
	g_lcdc.disp.y.bPrch = v32 & 0xFFFF;
	g_lcdc.disp.y.sync = v36 & 0xFFFF;
	g_lcdc.disp.y.fPrch = v40 & 0xFFFF;
	g_lcdc.disp.y.res = v44 & 0xFFFF;
	g_lcdc.disp.sync = 9000000.0f;
	g_lcdc.realY = v76;
	if (g_lcdc.disp.syncDiff != 0) {
		if (g_lcdc.hwData[0] == 1) {
			g_lcdc.scaledXres = g_lcdc.hwData[1];
			g_lcdc.yres = g_lcdc.hwData[2];
		} else {
			g_lcdc.yres = v76;
			g_lcdc.scaledXres = v72;
		}
	} else {
		g_lcdc.yres = v44 & 0xFFFF;
		g_lcdc.scaledXres = v28 & 0xFFFF;
	}
	u32 hw = REG32(0xBC100060);
	u8 hwBit = (hw >> 22) & 1;
	g_lcdc.disp.clk[0] = hwBit;
	switch (hw & 0x300000) {
	case 0x100000:
		g_lcdc.disp.clk[1] = 1;
		break;
	case 0x200000:
		g_lcdc.disp.clk[1] = 2;
		break;
	case 0x300000:
		g_lcdc.disp.clk[1] = 1;
		g_lcdc.disp.clk[0] = 2;
		break;
	case 0:
		g_lcdc.disp.clk[1] = 3;
		break;
	}
	if (g_lcdc.mobo > 0x7FFFFF)
		g_lcdc.disp.clk[1] *= (REG32(0xBE1401B0) + 1);
	s32 xLine = g_lcdc.disp.x.fPrch + g_lcdc.disp.x.bPrch + g_lcdc.disp.x.sync + g_lcdc.disp.x.res;
	s32 zoom = g_lcdc.scale;
	s32 yLine = g_lcdc.disp.y.fPrch + g_lcdc.disp.y.bPrch + g_lcdc.disp.y.sync + g_lcdc.disp.y.res;
	u8 param = g_lcdc.disp.flags & 7;
	s32 i;
	for (i = 0; i < 13; i++) {
		SceLcdcDispMode *cur = &g_lcdcDispModeList[i];
		if (xLine == (cur->x.fPrch + cur->x.bPrch + cur->x.sync + cur->x.res) * zoom) {
			if (yLine == cur->y.fPrch + cur->y.bPrch + cur->y.sync + cur->y.res &&
			    cur->y.bPrch == hwBit && param == cur->flags) {
				g_lcdc.disp.sync = cur->sync;
				g_lcdc.dispId = cur->id;
				if (cur->id != 0 && g_lcdc.disp.syncDiff == 0)
					g_lcdc.shiftY = (g_lcdc.disp.x.sync / zoom) - cur->x.sync;
				break;
			}
		}
	}
	g_lcdc.v3_88 = 0;
	g_lcdc.tabId = 0;
	if (g_lcdc.hwData[0] == 1) {
		for (i = 0; i < 8; i++) {
			if (g_lcdc.hwData[6] == g_lcdcTab[i].v1_12) {
				g_lcdc.tabId = g_lcdcTab[i].id;
				break;
			}
		}
		if (g_lcdc.tabId == 0)
			g_lcdc.v3_88 = 2;
	} else if ((g_lcdc.disp.flags & 0x20) != 0) {
		g_lcdc.v3_88 = 1;
	}
	return 0;
}

static int lcdc_event_handler(int ev_id, char *ev_name, void *param, int *result)
{
	(void)ev_name;
	(void)result;
	if (ev_id == 0x4004) {
		if (((s32 *)param)[1] != 2) {
			sub_0a8c();
			sub_0774();
		}
	} else if (ev_id == 0x10004) {
		if (((u32 *)((s32 *)param)[1])[1] == 0x80000000)
			sceLcdc_driver_5DF2F52A();
		else
			sub_5e8c();
	}
	return 0;
}

s32 sceLcdc_driver_BAF165CE(s32 id, s32 xres, s32 yres, s32 arg3)
{
	if ((u32)arg3 >= 4)
		return 0x80000108;
	s32 i;
	SceLcdcDispMode *dispMode = NULL;
	for (i = 0; i < 13; i++) {
		if (g_lcdcDispModeList[i].id == id) {
			dispMode = &g_lcdcDispModeList[i];
			break;
		}
	}
	if (dispMode == NULL)
		return 0x80000107;
	if (xres >= 480) {
		if (dispMode->x.res < xres)
			return 0x80000104;
	} else if (dispMode->x.res >= 480) {
		return 0x80000104;
	}
	if ((xres & 3) != 0)
		return 0x80000104;
	if (yres >= 272) {
		if (dispMode->y.res < yres)
			return 0x80000104;
	} else if (dispMode->y.res >= 272) {
		return 0x80000104;
	}
	return 0;
}

s32 sceLcdc_driver_E9DBD35F(s32 id, s32 xres, s32 yres, s32 arg3)
{
	s32 ret = sceLcdc_driver_BAF165CE(id, xres, yres, arg3);
	if (ret < 0)
		return ret;
	s32 i;
	SceLcdcDispMode *disp = NULL;
	for (i = 0; i < 13; i++) {
		if (g_lcdcDispModeList[i].id == id) {
			disp = &g_lcdcDispModeList[i];
			break;
		}
	}
	s32 oldIntr = sceKernelCpuSuspendIntr();
	g_lcdc.scaledXres = xres * g_lcdc.scale;
	g_lcdc.yres = yres;
	g_lcdc.v3_28 = arg3;
	g_lcdc.dispId = id;
	if (*(u16 *)g_lcdc.disp.clk != *(u16 *)disp->clk) {
		sub_0774();
		if (g_lcdc.mobo > 0x7FFFFF) {
			sceSysreg_driver_9746F3B2(disp->clk[0], 1);
			REG32(0xBE1401B0) = disp->clk[1] - 1;
		} else {
			sceSysreg_driver_9746F3B2(disp->clk[0], disp->clk[1]);
		}
		sub_0768();
	}
	g_lcdc.disp = *disp;
	if (g_lcdc.scale != 1) {
		g_lcdc.disp.x.bPrch *= g_lcdc.scale;
		g_lcdc.disp.x.res *= g_lcdc.scale;
		g_lcdc.disp.x.sync *= g_lcdc.scale;
		g_lcdc.disp.x.fPrch *= g_lcdc.scale;
	}
	if (g_lcdc.disp.syncDiff == 1 && g_lcdc.disp.x.res != g_lcdc.scaledXres)
		g_lcdc.disp.syncDiff = 0;
	if (g_lcdc.v3_88 == 1 && (g_lcdc.disp.flags & 1) != 0)
		g_lcdc.disp.flags |= 0x20;
	g_lcdc.hwData[0] = 0;
	if (g_lcdc.disp.syncDiff == 0) {
		g_lcdc.shiftY = 0;
		g_lcdc.shiftX = 0;
		s32 corr = (g_lcdc.dispId != 0) ? (g_lcdc.v3_64 * g_lcdc.scale) : 0;
		g_lcdc.disp.x.fPrch -= corr;
		g_lcdc.disp.x.sync += corr;
		if (g_lcdc.yres < g_lcdc.disp.y.res) {
			u16 ny = (u16)g_lcdc.yres;
			g_lcdc.disp.y.fPrch += g_lcdc.disp.y.res - ny;
			g_lcdc.disp.y.res = ny;
		}
	}
	g_lcdc.hwCtl->syncDiff = g_lcdc.disp.syncDiff;
	g_lcdc.hwCtl->v2_8 = g_lcdc.v3_28;
	g_lcdc.hwCtl->xbPrch = g_lcdc.disp.x.bPrch;
	g_lcdc.hwCtl->xsync = g_lcdc.disp.x.sync;
	g_lcdc.hwCtl->xfPrch = g_lcdc.disp.x.fPrch;
	g_lcdc.hwCtl->xres = g_lcdc.disp.x.res;
	g_lcdc.hwCtl->ybPrch = g_lcdc.disp.y.bPrch;
	g_lcdc.hwCtl->ysync = g_lcdc.disp.y.sync;
	g_lcdc.hwCtl->yfPrch = g_lcdc.disp.y.fPrch;
	g_lcdc.hwCtl->yres = g_lcdc.disp.y.res;
	g_lcdc.hwCtl->shiftY = g_lcdc.shiftY;
	g_lcdc.hwCtl->shiftX = g_lcdc.shiftX;
	g_lcdc.hwCtl->realX = g_lcdc.realX;
	g_lcdc.hwCtl->realY = g_lcdc.realY;
	if (id == 14) {
		g_lcdc.v3_8 = 0;
		g_lcdc.hwCtl->v2_112 = 0;
	}
	sceKernelCpuResumeIntr(oldIntr);
	return 0;
}

s32 sceLcdc_driver_88C3D167(s32 *arg0, s32 *arg1, s32 *arg2, s32 *arg3)
{
	if (arg0 != NULL) *arg0 = g_lcdc.dispId;
	if (arg1 != NULL) *arg1 = g_lcdc.scaledXres / g_lcdc.scale;
	if (arg2 != NULL) *arg2 = g_lcdc.yres;
	if (arg3 != NULL) *arg3 = g_lcdc.v3_28;
	return 0;
}

s32 sceLcdc_driver_C004BBE0(void)
{
	s32 oldIntr = sceKernelCpuSuspendIntr();
	s32 old = g_lcdc.hwCtl->v2_0;
	g_lcdc.hwCtl->v2_0 = old & ~1;
	if (g_lcdc.mobo > 0x7FFFFF) {
		u32 first = sceKernelGetSystemTimeLow();
		while (sceKernelGetSystemTimeLow() - first < 64)
			;
	}
	g_lcdc.hwCtl->v2_0 = old;
	sceKernelCpuResumeIntr(oldIntr);
	return 0;
}

s32 sceLcdc_driver_2D80BB2E(s32 *arg0, s32 *arg1, s32 *arg2, s32 *arg3, s32 *arg4)
{
	if (arg0 != NULL) *arg0 = g_lcdc.disp.y.fPrch;
	if (arg1 != NULL) *arg1 = g_lcdc.disp.y.bPrch;
	if (arg2 != NULL) *arg2 = g_lcdc.disp.y.sync;
	if (arg3 != NULL) *arg3 = g_lcdc.disp.y.fPrch + g_lcdc.disp.y.bPrch + g_lcdc.disp.y.sync;
	if (arg4 != NULL) *arg4 = g_lcdc.disp.y.fPrch + g_lcdc.disp.y.bPrch + g_lcdc.disp.y.sync + g_lcdc.disp.y.res;
	return 0;
}

s32 sceLcdc_driver_525E7EB1(void)
{
	s32 oldIntr = sceKernelCpuSuspendIntr();
	s32 i = 1;
	s32 ret;
	do {
		if (i >= 101) {
			ret = -1;
			break;
		}
		i++;
		ret = g_lcdc.hwCtl->hpc;
	} while (ret != g_lcdc.hwCtl->hpc);
	sceKernelCpuResumeIntr(oldIntr);
	return ret;
}

s32 sceLcdc_driver_35F45B50(void)
{
	s32 oldIntr = sceKernelCpuSuspendIntr();
	s32 i = 1;
	s32 ret;
	do {
		if (i >= 4) {
			ret = -1;
			break;
		}
		i++;
		ret = g_lcdc.hwCtl->vpc;
	} while (ret != g_lcdc.hwCtl->vpc);
	sceKernelCpuResumeIntr(oldIntr);
	return ret;
}

static s32 sub_6f7c(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	sub_5db4();
	return 0;
}

static s32 sub_6f9c(SceSize args)
{
	(void)args;
	sceKernelUnregisterSysEventHandler(&g_lcdcEvent);
	return 0;
}

s32 sceLcdc_driver_0BFF31C5(void)
{
	sub_0a80();
	g_lcdc.v3_0 = 1;
	return 0;
}

s32 sceLcdc_driver_3EDBFCB9(void)
{
	sub_0a8c();
	g_lcdc.v3_0 = 0;
	return 0;
}

s32 sceLcdc_driver_451FE1A1(u32 arg)
{
	if (arg >= 9)
		return 0x80000107;
	g_lcdc.tabId = arg;
	return 0;
}

s32 sceLcdc_driver_A3DCEF64(u32 arg)
{
	if (arg >= 3)
		return 0x80000107;
	g_lcdc.v3_88 = arg;
	return 0;
}

s32 sceLcdc_driver_94F83F74(s32 arg0, s32 arg1)
{
	(void)arg0;
	s32 oldIntr = sceKernelCpuSuspendIntr();
	s32 diff = arg1 - g_lcdc.v3_64;
	g_lcdc.v3_64 = arg1;
	if (g_lcdc.dispId != 0 && g_lcdc.disp.syncDiff == 0) {
		s32 m = diff * g_lcdc.scale;
		s32 d1 = g_lcdc.disp.x.fPrch - m;
		s32 d2 = g_lcdc.disp.x.sync + m;
		g_lcdc.hwCtl->xfPrch = d1 & 0xFFFF;
		g_lcdc.disp.x.sync = d2;
		g_lcdc.hwCtl->xres = g_lcdc.disp.x.res;
		g_lcdc.disp.x.fPrch = d1;
	}
	sceKernelCpuResumeIntr(oldIntr);
	return 0;
}

float sceLcdc_driver_1F01BD08(void)
{
	return g_lcdc.disp.sync * (float)g_lcdc.scale;
}

s32 sceLcdc_driver_BB839185(void)
{
	return g_lcdc.scale;
}

float sceLcdc_driver_56307DBC(void)
{
	return g_lcdc.disp.sync;
}

float sceLcdc_driver_C943B9A2(void)
{
	float numPix = (float)(g_lcdc.disp.x.fPrch + g_lcdc.disp.x.bPrch + g_lcdc.disp.x.sync + g_lcdc.disp.x.res);
	return (g_lcdc.disp.sync * (float)g_lcdc.scale) / numPix;
}

float sceLcdc_driver_9D4FB8B1(void)
{
	s32 x = g_lcdc.disp.x.fPrch + g_lcdc.disp.x.bPrch + g_lcdc.disp.x.sync + g_lcdc.disp.x.res;
	s32 y = g_lcdc.disp.y.fPrch + g_lcdc.disp.y.bPrch + g_lcdc.disp.y.sync + g_lcdc.disp.y.res;
	if ((g_lcdc.disp.flags & 5) == 5)
		x >>= 1;
	return (g_lcdc.disp.sync * (float)g_lcdc.scale) / (float)(x * y);
}

s32 sceLcdc_driver_75C4DA9A(void)
{
	s32 v = g_lcdc.hwCtl->v2_80;
	if (v == 1)
		g_lcdc.hwCtl->v2_80 = v;
	return v;
}

s32 sceLcdc_driver_90AA786C(void)
{
	u32 v;
	do {
		v = REG32(0xBE700004);
	} while (v != REG32(0xBE700004));
	return (v ^ 1) & 1;
}

/* =========================================================================
 * Module entry points
 * ========================================================================= */

s32 module_start(SceSize args, void *argp)
{
	sub_16c8(args);
	sub_2f98(args, argp);
	sub_6f7c(args, argp);
	sub_7230(args, argp);
	return 0;
}

s32 module_reboot_before(SceSize args)
{
	sub_727c(args);
	sub_6f9c(args);
	sub_3024(args);
	sub_1708(args);
	return 0;
}
