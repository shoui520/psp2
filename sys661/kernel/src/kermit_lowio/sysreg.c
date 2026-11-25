#include "lowio_int.h"

/* =========================================================================
 * Part 1: sceSysreg_driver (178 exported functions)
 * ========================================================================= */

static volatile float g_pllBaseFreq = 37.0f;
static float g_pllFreqTable[16] = {
	166.5f, 333.0f, 444.0f, 222.0f, 190.28572f, 380.57144f, 266.4f, 532.8f,
	148.0f, 148.0f, 148.0f, 148.0f, 148.0f,     148.0f,     148.0f, 148.0f
};
static float g_pllFreq;
static u32 g_tachyonVer = 0xFFFFFFFF;
static u8 g_usbIoRefMask;
static u32 g_sysregSubIntrMask;
static u32 g_sysregSubIntrRot;

static inline __attribute__((always_inline)) s32 sysreg_mod_reg(u32 addr, u32 mask, s32 enable)
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

__attribute__((noipa)) static s32 sysreg_reset_ctrl(u32 mask, s32 enable)
{
	return sysreg_mod_reg(0xBC10004C, mask, enable);
}

__attribute__((noipa)) static s32 sysreg_busclk_ctrl(u32 mask, s32 enable)
{
	return sysreg_mod_reg(0xBC100050, mask, enable);
}

__attribute__((noipa)) static s32 sysreg_clk1_ctrl(u32 mask, s32 enable)
{
	return sysreg_mod_reg(0xBC100054, mask, enable);
}

__attribute__((noipa)) static s32 sysreg_clk2_ctrl(u32 mask, s32 enable)
{
	return sysreg_mod_reg(0xBC100058, mask, enable);
}

__attribute__((noipa)) static s32 sysreg_io_ctrl(u32 mask, s32 enable)
{
	return sysreg_mod_reg(0xBC100078, mask, enable);
}

__attribute__((noipa)) static s32 sysreg_gpio_io_ctrl(u32 mask, s32 enable)
{
	return sysreg_mod_reg(0xBC10007C, mask, enable);
}

__attribute__((noipa)) static s32 sysreg_unk74_ctrl(u32 mask, s32 enable)
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
s32 sysregDmacplusBusClockEnable(void) { return sysreg_busclk_ctrl(0x0010, 1); }
s32 sysregDmacplusBusClockDisable(void) { return sysreg_busclk_ctrl(0x0010, 0); }
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
s32 sysregLcdcClkEnable(void) { return sysreg_clk2_ctrl(0x40000, 1); }
s32 sysregLcdcClkDisable(void) { return sysreg_clk2_ctrl(0x40000, 0); }
s32 sceSysregSircsClkEnable(void) { return sysreg_clk2_ctrl(0x400000, 1); }
s32 sceSysregSircsClkDisable(void) { return sysreg_clk2_ctrl(0x400000, 0); }
s32 sceSysregGpioClkEnable(void) { return sysreg_clk2_ctrl(0x800000, 1); }
s32 sceSysregGpioClkDisable(void) { return sysreg_clk2_ctrl(0x800000, 0); }
s32 sceSysregAudioClkoutClkEnable(void) { return sysreg_clk2_ctrl(0x1000000, 1); }
s32 sceSysregAudioClkoutClkDisable(void) { return sysreg_clk2_ctrl(0x1000000, 0); }
s32 sceSysreg_driver_F9C93DD4(void) { return sysreg_clk2_ctrl(0x2000000, 1); }
s32 sceSysreg_driver_B6296512(void) { return sysreg_clk2_ctrl(0x2000000, 0); }

/* Special Clock1 controls (0xBC100054) */
__attribute__((noipa)) static s32 sysregSetClockMode54(s32 a0)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u32 val = REG32(0xBC100054);
	u32 bit31 = (REG32(0xBC100040) >> 31) & 1;
	u32 m0 = bit31 ? 0x10U : 0x90U;
	u32 m1 = bit31 ? 0x04010010U : 0x10080U;
	u32 mask = ((a0 & 2) != 0) ? m1 : m0;
	u32 next = val & ~mask;
	if ((a0 & 1) != 0)
		next = val | mask;
	REG32(0xBC100054) = next;
	sceKernelCpuResumeIntr(intr);
	return 0;
}
s32 sceSysreg_driver_FC0131A7(void) { return sysregSetClockMode54(1); }
s32 sceSysreg_driver_C9585F8E(void) { return sysregSetClockMode54(0); }
s32 sceSysreg_driver_68AE6434(void) { return sysregSetClockMode54(3); }
s32 sceSysreg_driver_14EB1393(void) { return sysregSetClockMode54(2); }
__attribute__((noipa)) static s32 sysregSetClockMask54(s32 mask, s32 enable)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u32 val = REG32(0xBC100054);
	u32 prev = (val >> 4) & 0xF;
	u32 m = mask & 0xF;
	u32 next_nib = enable ? (prev | m) : (prev & ~m);
	REG32(0xBC100054) = (val & ~0xF0U) | ((next_nib & 0xF) << 4);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregUsbClkEnable(s32 mask)
{
	return sysregSetClockMask54(mask, 1);
}
s32 sceSysregUsbClkDisable(s32 mask)
{
	return sysregSetClockMask54(mask, 0);
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
s32 sysregLcdcIoEnable(void) { return sysreg_io_ctrl(0x0040, 1); }
s32 sysregLcdcIoDisable(void) { return sysreg_io_ctrl(0x0040, 0); }
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
static s32 sysregIoBit15Enable(void) { return sysreg_io_ctrl(0x8000, 1); }
static s32 sysregIoBit15Disable(void) { return sysreg_io_ctrl(0x8000, 0); }
static s32 sysregIoBit31Enable(void) { return sysreg_io_ctrl(0x80000000U, 1); }
static s32 sysregIoBit31Disable(void) { return sysreg_io_ctrl(0x80000000U, 0); }
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
	return REG32(0xBC100080) & 1;
}
s32 sceSysregUsbQueryIntr(void)
{
	return (REG32(0xBC100080) >> 1) & 0xF;
}
s32 sceSysregUsbAcquireIntr(s32 mask)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u32 v = (REG32(0xBC100080) >> 1) & 0xF;
	REG32(0xBC100080) = ((mask & v) & 0xF) << 1;
	sceKernelCpuResumeIntrWithSync(intr);
	return v;
}
s32 sceSysregUsbhostQueryIntr(void)
{
	return REG32(0xBC1000B0) & 7;
}
s32 sceSysregUsbhostAcquireIntr(s32 mask)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u32 v = REG32(0xBC1000B0) & 7;
	REG32(0xBC1000B0) = (mask & v) & 7;
	sceKernelCpuResumeIntrWithSync(intr);
	return v;
}
s32 sceSysregMsifGetConnectStatus(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return (REG32(0xBC100080) >> ((no << 3) + 8)) & 1;
}
s32 sceSysregMsifQueryConnectIntr(s32 no)
{
	if ((u32)no >= 2) return 0x80000102;
	return (REG32(0xBC100080) >> ((no << 3) + 9)) & 7;
}
s32 sceSysregMsifAcquireConnectIntr(s32 no, s32 mask)
{
	if ((u32)no >= 2) return 0x80000102;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = (no << 3) + 9;
	u32 v = (REG32(0xBC100080) >> shift) & 7;
	REG32(0xBC100080) = (mask & v) << shift;
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
static s32 sysregUsbIoRefInc(s32 bit)
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
static s32 sysregUsbIoRefDec(s32 bit)
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
		return sysregIoBit31Enable();
	return sysregUsbIoRefInc(1);
}
s32 sceSysreg_driver_80944C8A(void)
{
	u32 ver = sceSysregGetTachyonVersion();
	if (ver <= 0x004FFFFF)
		return 0x80000004;
	if (ver > 0x007FFFFF)
		return sysregIoBit31Disable();
	return sysregUsbIoRefDec(1);
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
		return sysregIoBit15Enable();
	return sysregUsbIoRefInc(0);
}
s32 sceSysreg_driver_87B61303(void)
{
	if (sceSysregGetTachyonVersion() > 0x007FFFFF)
		return sysregIoBit15Disable();
	return sysregUsbIoRefDec(0);
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
	if ((sceSysregGetTachyonVersion() & 0xFFE00000) == 0x00800000) {
		if (div == 3 && (REG32(0xBC100100) & 1) != 0)
			REG32(0xBC100100) &= ~1U;
	}
	REG32(0xBC100060) = (reg & ~(7U << 20)) | ((idx << 22) | (div << 20));
	if ((sceSysregGetTachyonVersion() & 0xFFE00000) == 0x00800000) {
		if (div != 3 && (REG32(0xBC1000E8) & 0xAAA) == 0 &&
		    (REG32(0xBC100058) & 0x2000000) == 0 && (REG32(0xBC100100) & 1) == 0)
			REG32(0xBC100100) |= 1;
	}
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysreg_driver_434E8AF1(s32 sel)
{
	if ((u32)(sel + 1) >= 3) return 0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 reg = REG32(0xBC100060);
	s32 prev = (reg >> 24) & 1;
	if (sel == 1 && (REG32(0xBC100100) & 1) != 0)
		REG32(0xBC100100) &= ~1U;
	if (sel >= 0)
		REG32(0xBC100060) = (reg & ~(1U << 24)) | ((sel & 1) << 24);
	if (sel != 1 && (REG32(0xBC1000E8) & 0xAAA) == 0 &&
	    (REG32(0xBC100060) & 0x300000) != 0x300000 && (REG32(0xBC100100) & 1) == 0)
		REG32(0xBC100100) |= 1;
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
	if (sel == 2 && (REG32(0xBC100100) & 1) != 0)
		REG32(0xBC100100) &= ~1U;
	u32 next = (reg & ~(3U << shift)) | ((sel & 3) << shift);
	REG32(0xBC1000E8) = next;
	if ((next & 0xAAA) == 0 && (REG32(0xBC100060) & 0x300000) != 0x300000 &&
	    (REG32(0xBC100060) & 0x1000000) == 0 && (REG32(0xBC100100) & 1) == 0)
		REG32(0xBC100100) |= 1;
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
static s32 sysregIpiIntrHandler(s32 intr_code, void *arg, void *frame)
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
static s32 sysregSubIntrEnable(s32 intr_code, s32 sub_intr)
{
	(void)intr_code;
	g_sysregSubIntrMask |= (1U << sub_intr);
	return 0;
}
static s32 sysregSubIntrDisable(s32 intr_code, s32 sub_intr)
{
	(void)intr_code;
	g_sysregSubIntrMask &= ~(1U << sub_intr);
	return 0;
}
static s32 sysregSubIntrSuspend(s32 intr_code, s32 sub_intr, u32 *prev)
{
	(void)intr_code;
	if (prev != NULL)
		*prev = (g_sysregSubIntrMask >> sub_intr) & 1;
	g_sysregSubIntrMask &= ~(1U << sub_intr);
	return 0;
}
static s32 sysregSubIntrResume(s32 intr_code, s32 sub_intr, u32 prev)
{
	(void)intr_code;
	g_sysregSubIntrMask |= (prev << sub_intr);
	return 0;
}
static s32 sysregSubIntrClear(s32 intr_code, s32 sub_intr)
{
	(void)intr_code;
	u32 my_cpu;
	__asm__ volatile("mfc0 %0, $22" : "=r"(my_cpu));
	volatile u32 *reg = (volatile u32 *)(0xBFC00400 + (my_cpu << 2));
	return (sceSysreg_driver_1E62714E(reg) >> sub_intr) & 1;
}
static u32 g_sysregSubIntrCb[7] = {
	0, (u32)sysregSubIntrDisable, (u32)sysregSubIntrEnable, (u32)sysregSubIntrDisable,
	(u32)sysregSubIntrSuspend, (u32)sysregSubIntrResume, (u32)sysregSubIntrClear
};
static u32 g_sysregIntrOpt[3] = { 0x0C, 0x20, (u32)g_sysregSubIntrCb };

s32 sceSysregIntrInit(void)
{
	sceKernelRegisterIntrHandler(31, 2, sysregIpiIntrHandler, NULL, g_sysregIntrOpt);
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
s32 sysregModuleStart(SceSize args)
{
	(void)args;
	sceKernelUnregisterSysEventHandler(&g_sysregEvent);
	sceSysregPllUpdateFrequency();
	sceKernelRegisterSysEventHandler(&g_sysregEvent);
	return 0;
}
s32 sysregModuleStop(SceSize args)
{
	(void)args;
	return 0;
}

