#include "lowio_int.h"

/* =========================================================================
 * Part 1: sceSysreg_driver (178 exported functions)
 * ========================================================================= */

static volatile float g_pllBaseFreq = 37.0f;
static const u32 g_pllFreqTableBits[16] = {
	0x43268000U, 0x43A68000U, 0x43DE0000U, 0x435E0000U,
	0x433E4925U, 0x43BE4925U, 0x43853333U, 0x44053333U,
	0x43140000U, 0x43140000U, 0x43140000U, 0x43140000U,
	0x43140000U, 0x43140000U, 0x43140000U, 0x43140000U
};
static union {
	u32 bits;
	float hz;
} g_pllFreq;
static u32 g_tachyonVer = 0xFFFFFFFFU;
static u8 g_usbIoRefMask;
static u32 g_sysregSubIntrMask;
static u32 g_sysregSubIntrRot;

static inline __attribute__((always_inline)) s32 sysregModifyBits(u32 addr, u32 mask, s32 enable)
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

static inline __attribute__((always_inline)) s32 sysregResetCtrl(u32 mask, s32 enable)
{
	return sysregModifyBits(0xBC10004C, mask, enable);
}

static inline __attribute__((always_inline)) s32 sysregBusClockCtrl(u32 mask, s32 enable)
{
	return sysregModifyBits(0xBC100050, mask, enable);
}

static inline __attribute__((always_inline)) s32 sysregClockGroupACtrl(u32 mask, s32 enable)
{
	return sysregModifyBits(0xBC100054, mask, enable);
}

static inline __attribute__((always_inline)) s32 sysregClockGroupBCtrl(u32 mask, s32 enable)
{
	return sysregModifyBits(0xBC100058, mask, enable);
}

static inline __attribute__((always_inline)) s32 sysregIoCtrl(u32 mask, s32 enable)
{
	return sysregModifyBits(0xBC100078, mask, enable);
}

static inline __attribute__((always_inline)) s32 sysregGpioIoCtrl(u32 mask, s32 enable)
{
	return sysregModifyBits(0xBC10007C, mask, enable);
}

static inline __attribute__((always_inline)) s32 sysregPowerCtrl(u32 mask, s32 enable)
{
	return sysregModifyBits(0xBC100074, mask, enable);
}

/* Reset controls (0xBC10004C) */
s32 sceSysregTopResetEnable(void) { return sysregResetCtrl(0x0001, 1); }
s32 sceSysregScResetEnable(void) { return sysregResetCtrl(0x0002, 1); }
s32 sceSysregMeResetEnable(void) { return sysregResetCtrl(0x0004, 1); }
s32 sceSysregMeResetDisable(void) { return sysregResetCtrl(0x0004, 0); }
s32 sceSysregAwResetEnable(void) { return sysregResetCtrl(0x0008, 1); }
s32 sceSysregAwResetDisable(void) { return sysregResetCtrl(0x0008, 0); }
s32 sceSysregVmeResetEnable(void) { return sysregResetCtrl(0x0010, 1); }
s32 sceSysregVmeResetDisable(void) { return sysregResetCtrl(0x0010, 0); }
s32 sceSysregAvcResetEnable(void) { return sysregResetCtrl(0x0020, 1); }
s32 sceSysregAvcResetDisable(void) { return sysregResetCtrl(0x0020, 0); }
s32 sceSysregUsbResetEnable(void) { return sysregResetCtrl(0x0040, 1); }
s32 sceSysregUsbResetDisable(void) { return sysregResetCtrl(0x0040, 0); }
s32 sceSysregAtaResetEnable(void) { return sysregResetCtrl(0x0080, 1); }
s32 sceSysregAtaResetDisable(void) { return sysregResetCtrl(0x0080, 0); }
s32 sceSysregMsifResetEnable(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregResetCtrl(0x0100U << unit, 1);
}
s32 sceSysregMsifResetDisable(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregResetCtrl(0x0100U << unit, 0);
}
s32 sceSysregKirkResetEnable(void) { return sysregResetCtrl(0x0400, 1); }
s32 sceSysregKirkResetDisable(void) { return sysregResetCtrl(0x0400, 0); }
s32 sceSysregAtahddResetEnable(void) { return sysregResetCtrl(0x1000, 1); }
s32 sceSysregAtahddResetDisable(void) { return sysregResetCtrl(0x1000, 0); }
s32 sceSysregUsbhostResetEnable(void) { return sysregResetCtrl(0x2000, 1); }
s32 sceSysregUsbhostResetDisable(void) { return sysregResetCtrl(0x2000, 0); }
s32 sceSysreg_driver_C6C75585(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregResetCtrl(0x4000U << unit, 1);
}
s32 sceSysreg_driver_0995F8F6(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregResetCtrl(0x4000U << unit, 0);
}
s32 sceSysreg_driver_72887197(void) { return sysregResetCtrl(0x10000, 1); }
s32 sceSysreg_driver_32E02FDF(void) { return sysregResetCtrl(0x10000, 0); }
s32 sceSysreg_driver_73B3E52D(void) { return (s32)((REG32(0xBC10004C) >> 16) & 1U); }

/* Bus clock controls (0xBC100050) */
s32 sceSysregMeBusClockEnable(void) { return sysregBusClockCtrl(0x0001, 1); }
s32 sceSysregMeBusClockDisable(void) { return sysregBusClockCtrl(0x0001, 0); }
s32 sceSysregAwRegABusClockEnable(void) { return sysregBusClockCtrl(0x0002, 1); }
s32 sceSysregAwRegABusClockDisable(void) { return sysregBusClockCtrl(0x0002, 0); }
s32 sceSysregAwRegBBusClockEnable(void) { return sysregBusClockCtrl(0x0004, 1); }
s32 sceSysregAwRegBBusClockDisable(void) { return sysregBusClockCtrl(0x0004, 0); }
s32 sceSysregAwEdramBusClockEnable(void) { return sysregBusClockCtrl(0x0008, 1); }
s32 sceSysregAwEdramBusClockDisable(void) { return sysregBusClockCtrl(0x0008, 0); }
s32 sysregDmacplusBusClockEnable(void) { return sysregBusClockCtrl(0x0010, 1); }
s32 sysregDmacplusBusClockDisable(void) { return sysregBusClockCtrl(0x0010, 0); }
static s32 __attribute__((used, noinline)) sysregDmacBusClockEnable(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregBusClockCtrl(0x0020U << unit, 1);
}
static s32 __attribute__((used, noinline)) sysregDmacBusClockDisable(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregBusClockCtrl(0x0020U << unit, 0);
}
s32 sceSysregKirkBusClockEnable(void) { return sysregBusClockCtrl(0x0080, 1); }
s32 sceSysregKirkBusClockDisable(void) { return sysregBusClockCtrl(0x0080, 0); }
s32 sceSysregAtaBusClockEnable(void) { return sysregBusClockCtrl(0x0100, 1); }
s32 sceSysregAtaBusClockDisable(void) { return sysregBusClockCtrl(0x0100, 0); }
s32 sceSysregUsbBusClockEnable(void) { return sysregBusClockCtrl(0x0200, 1); }
s32 sceSysregUsbBusClockDisable(void) { return sysregBusClockCtrl(0x0200, 0); }
s32 sceSysregMsifBusClockEnable(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregBusClockCtrl(0x0400U << unit, 1);
}
s32 sceSysregMsifBusClockDisable(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregBusClockCtrl(0x0400U << unit, 0);
}
s32 sceSysregEmcddrBusClockEnable(void) { return sysregBusClockCtrl(0x1000, 1); }
s32 sceSysregEmcddrBusClockDisable(void) { return sysregBusClockCtrl(0x1000, 0); }
s32 sceSysregEmcsmBusClockEnable(void) { return sysregBusClockCtrl(0x2000, 1); }
s32 sceSysregEmcsmBusClockDisable(void) { return sysregBusClockCtrl(0x2000, 0); }
s32 sceSysregApbBusClockEnable(void) { return sysregBusClockCtrl(0x4000, 1); }
s32 sceSysregApbBusClockDisable(void) { return sysregBusClockCtrl(0x4000, 0); }
s32 sceSysregAudioBusClockEnable(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregBusClockCtrl(0x8000U << unit, 1);
}
s32 sceSysregAudioBusClockDisable(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregBusClockCtrl(0x8000U << unit, 0);
}
s32 sceSysregAtahddBusClockEnable(void) { return sysregBusClockCtrl(0x20000, 1); }
s32 sceSysregAtahddBusClockDisable(void) { return sysregBusClockCtrl(0x20000, 0); }
s32 sceSysregUsbhostBusClockEnable(void) { return sysregBusClockCtrl(0x40000, 1); }
s32 sceSysregUsbhostBusClockDisable(void) { return sysregBusClockCtrl(0x40000, 0); }
s32 sceSysreg_driver_3BB0B2C8(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregBusClockCtrl(0x80000U << unit, 1);
}
s32 sceSysreg_driver_76E57DC6(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregBusClockCtrl(0x80000U << unit, 0);
}

/* Clock register 0xBC100054 controls */
s32 sceSysregAtaClkEnable(void) { return sysregClockGroupACtrl(0x0001, 1); }
s32 sceSysregAtaClkDisable(void) { return sysregClockGroupACtrl(0x0001, 0); }
s32 sceSysregAtahddClkEnable(void) { return sysregClockGroupACtrl(0x0002, 1); }
s32 sceSysregAtahddClkDisable(void) { return sysregClockGroupACtrl(0x0002, 0); }
s32 sceSysregUsbhostClkEnable(void) { return sysregClockGroupACtrl(0x10000, 1); }
s32 sceSysregUsbhostClkDisable(void) { return sysregClockGroupACtrl(0x10000, 0); }
s32 sceSysregMsifClkEnable(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregClockGroupACtrl(0x0100U << unit, 1);
}
s32 sceSysregMsifClkDisable(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregClockGroupACtrl(0x0100U << unit, 0);
}
s32 sceSysreg_driver_A7C82BDD(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregClockGroupACtrl(0x1000000U << unit, 1);
}
s32 sceSysreg_driver_FF0E07B1(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregClockGroupACtrl(0x1000000U << unit, 0);
}

/* Clock register 0xBC100058 controls */
s32 sceSysregSpiClkEnable(s32 unit)
{
	if (unit < 0 || unit >= 6)
		return (s32)0x80000102;
	return sysregClockGroupBCtrl(0x0001U << unit, 1);
}
s32 sceSysregSpiClkDisable(s32 unit)
{
	if (unit < 0 || unit >= 6)
		return (s32)0x80000102;
	return sysregClockGroupBCtrl(0x0001U << unit, 0);
}
s32 sceSysregUartClkEnable(s32 unit)
{
	if (unit < 0 || unit >= 6)
		return (s32)0x80000102;
	return sysregClockGroupBCtrl(0x0040U << unit, 1);
}
s32 sceSysregUartClkDisable(s32 unit)
{
	if (unit < 0 || unit >= 6)
		return (s32)0x80000102;
	return sysregClockGroupBCtrl(0x0040U << unit, 0);
}
s32 sceSysregApbTimerClkEnable(s32 unit)
{
	if (unit < 0 || unit >= 4)
		return (s32)0x80000102;
	return sysregClockGroupBCtrl(0x1000U << unit, 1);
}
s32 sceSysregApbTimerClkDisable(s32 unit)
{
	if (unit < 0 || unit >= 4)
		return (s32)0x80000102;
	return sysregClockGroupBCtrl(0x1000U << unit, 0);
}
s32 sceSysregAudioClkEnable(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregClockGroupBCtrl(0x10000U << unit, 1);
}
s32 sceSysregAudioClkDisable(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregClockGroupBCtrl(0x10000U << unit, 0);
}
s32 sysregLcdcClkEnable(void) { return sysregClockGroupBCtrl(0x40000, 1); }
s32 sysregLcdcClkDisable(void) { return sysregClockGroupBCtrl(0x40000, 0); }
static s32 __attribute__((used, noinline)) sysregPwmClkEnable(void) { return sysregClockGroupBCtrl(0x80000, 1); }
static s32 __attribute__((used, noinline)) sysregPwmClkDisable(void) { return sysregClockGroupBCtrl(0x80000, 0); }
static s32 __attribute__((used, noinline)) sysregKeyClkEnable(void) { return sysregClockGroupBCtrl(0x100000, 1); }
static s32 __attribute__((used, noinline)) sysregKeyClkDisable(void) { return sysregClockGroupBCtrl(0x100000, 0); }
static s32 __attribute__((used, noinline)) sysregIicClkEnable(void) { return sysregClockGroupBCtrl(0x200000, 1); }
static s32 __attribute__((used, noinline)) sysregIicClkDisable(void) { return sysregClockGroupBCtrl(0x200000, 0); }
s32 sceSysregSircsClkEnable(void) { return sysregClockGroupBCtrl(0x400000, 1); }
s32 sceSysregSircsClkDisable(void) { return sysregClockGroupBCtrl(0x400000, 0); }
s32 sceSysregGpioClkEnable(void) { return sysregClockGroupBCtrl(0x800000, 1); }
s32 sceSysregGpioClkDisable(void) { return sysregClockGroupBCtrl(0x800000, 0); }
s32 sceSysregAudioClkoutClkEnable(void) { return sysregClockGroupBCtrl(0x1000000, 1); }
s32 sceSysregAudioClkoutClkDisable(void) { return sysregClockGroupBCtrl(0x1000000, 0); }
s32 sceSysreg_driver_F9C93DD4(void) { return sysregClockGroupBCtrl(0x2000000, 1); }
s32 sceSysreg_driver_B6296512(void) { return sysregClockGroupBCtrl(0x2000000, 0); }

/* Special USB/PLL clock controls (0xBC100054) */
static inline __attribute__((always_inline)) s32 sysregSetUsbClockGroup(s32 op_bits)
{
	volatile s32 uninit_ret;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 val = REG32(0xBC100054);
	u32 hi_rev = (REG32(0xBC100040) >> 31) & 1U;
	u32 group_lo = hi_rev ? 0x10U : 0x90U;
	u32 group_hi = hi_rev ? 0x04010010U : 0x10080U;
	u32 mask = ((op_bits & 2) != 0) ? group_hi : group_lo;
	u32 next = val & ~mask;
	if ((op_bits & 1) != 0)
		next = val | mask;
	REG32(0xBC100054) = next;
	sceKernelCpuResumeIntr(intr);
	return uninit_ret;
}
s32 sceSysreg_driver_FC0131A7(void) { return sysregSetUsbClockGroup(1); }
s32 sceSysreg_driver_C9585F8E(void) { return sysregSetUsbClockGroup(0); }
s32 sceSysreg_driver_68AE6434(void) { return sysregSetUsbClockGroup(3); }
s32 sceSysreg_driver_14EB1393(void) { return sysregSetUsbClockGroup(2); }

static inline __attribute__((always_inline)) s32 sysregSetUsbClockMask(s32 mask, s32 enable)
{
	(void)enable;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 val = REG32(0xBC100054);
	u32 prev = (val >> 4) & 0xFU;
	u32 next_nib = (prev | (u32)mask) & 0xFU;
	REG32(0xBC100054) = (val & ~0xF0U) | (next_nib << 4);
	sceKernelCpuResumeIntr(intr);
	return (s32)prev;
}
s32 sceSysregUsbClkEnable(s32 mask)
{
	return sysregSetUsbClockMask(mask, 1);
}
s32 sceSysregUsbClkDisable(s32 mask)
{
	return sysregSetUsbClockMask(mask, 0);
}

/* IO controls (0xBC100078, 0xBC10007C, 0xBC100074) */
s32 sceSysregEmcsmIoEnable(void) { return sysregIoCtrl(0x0002, 1); }
s32 sceSysregEmcsmIoDisable(void) { return sysregIoCtrl(0x0002, 0); }
s32 sceSysregUsbIoEnable(void) { return sysregIoCtrl(0x0004, 1); }
s32 sceSysregUsbIoDisable(void) { return sysregIoCtrl(0x0004, 0); }
s32 sceSysregAtaIoEnable(void) { return sysregIoCtrl(0x0008, 1); }
s32 sceSysregAtaIoDisable(void) { return sysregIoCtrl(0x0008, 0); }
s32 sceSysregMsifIoEnable(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregIoCtrl(0x0010U << unit, 1);
}
s32 sceSysregMsifIoDisable(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregIoCtrl(0x0010U << unit, 0);
}
s32 sysregLcdcIoEnable(void) { return sysregIoCtrl(0x0040, 1); }
s32 sysregLcdcIoDisable(void) { return sysregIoCtrl(0x0040, 0); }
s32 sceSysregAudioIoEnable(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregIoCtrl(0x0080U << unit, 1);
}
s32 sceSysregAudioIoDisable(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregIoCtrl(0x0080U << unit, 0);
}
static s32 __attribute__((used, noinline)) sysregIicIoEnable(void) { return sysregIoCtrl(0x0200, 1); }
static s32 __attribute__((used, noinline)) sysregIicIoDisable(void) { return sysregIoCtrl(0x0200, 0); }
s32 sceSysregSircsIoEnable(void) { return sysregIoCtrl(0x0400, 1); }
s32 sceSysregSircsIoDisable(void) { return sysregIoCtrl(0x0400, 0); }
s32 sceSysregAudioClkoutIoEnable(void) { return sysregIoCtrl(0x0800, 1); }
s32 sceSysregAudioClkoutIoDisable(void) { return sysregIoCtrl(0x0800, 0); }
static s32 __attribute__((used, noinline)) sysregKeyIoEnable(void) { return (s32)0x80000004; }
static s32 __attribute__((used, noinline)) sysregKeyIoDisable(void) { return (s32)0x80000004; }
static s32 __attribute__((used, noinline)) sysregPwmIoEnable(void) { return sysregIoCtrl(0x2000, 1); }
static s32 __attribute__((used, noinline)) sysregPwmIoDisable(void) { return sysregIoCtrl(0x2000, 0); }
s32 sceSysregAtahddIoEnable(void) { return sysregIoCtrl(0x4000, 1); }
s32 sceSysregAtahddIoDisable(void) { return sysregIoCtrl(0x4000, 0); }
s32 sceSysregUartIoEnable(s32 unit)
{
	if (unit < 0 || unit >= 6)
		return (s32)0x80000102;
	return sysregIoCtrl(0x00010000U << unit, 1);
}
s32 sceSysregUartIoDisable(s32 unit)
{
	if (unit < 0 || unit >= 6)
		return (s32)0x80000102;
	return sysregIoCtrl(0x00010000U << unit, 0);
}
s32 sceSysregSpiIoEnable(s32 unit)
{
	if (unit < 0 || unit >= 6)
		return (s32)0x80000102;
	return sysregIoCtrl(0x01000000U << unit, 1);
}
s32 sceSysregSpiIoDisable(s32 unit)
{
	if (unit < 0 || unit >= 6)
		return (s32)0x80000102;
	return sysregIoCtrl(0x01000000U << unit, 0);
}
s32 sceSysreg_driver_19F4C92D(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregIoCtrl(0x00400000U << unit, 1);
}
s32 sceSysreg_driver_DA5B5ED9(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregIoCtrl(0x00400000U << unit, 0);
}
s32 sceSysreg_driver_0607A4C4(void) { return sysregIoCtrl(0x40000000U, 1); }
s32 sceSysreg_driver_803E5F37(void) { return sysregIoCtrl(0x40000000U, 0); }
static s32 __attribute__((noinline)) sysregUsbAuxIoEnable(void) { return sysregIoCtrl(0x8000U, 1); }
static s32 __attribute__((noinline)) sysregUsbAuxIoDisable(void) { return sysregIoCtrl(0x8000U, 0); }
static s32 __attribute__((noinline)) sysregUsbExtIoEnable(void) { return sysregIoCtrl(0x80000000U, 1); }
static s32 __attribute__((noinline)) sysregUsbExtIoDisable(void) { return sysregIoCtrl(0x80000000U, 0); }

s32 sceSysregGpioIoEnable(s32 port)
{
	if (port < 0 || port >= 32)
		return (s32)0x80000102;
	return sysregGpioIoCtrl(1U << port, 1);
}
s32 sceSysregGpioIoDisable(s32 port)
{
	if (port < 0 || port >= 32)
		return (s32)0x80000102;
	return sysregGpioIoCtrl(1U << port, 0);
}

s32 sceSysreg_driver_33EE43F0(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregPowerCtrl(1U << (unit << 2), 0);
}
s32 sceSysreg_driver_6DA9347D(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregPowerCtrl(1U << (unit << 2), 1);
}
s32 sceSysreg_driver_9E2F8FD5(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregPowerCtrl(2U << (unit << 2), 0);
}
s32 sceSysreg_driver_4C0BED71(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregPowerCtrl(2U << (unit << 2), 1);
}
s32 sceSysreg_driver_1561BCD2(void) { return sysregPowerCtrl(0x0100U, 1); }
s32 sceSysreg_driver_9275DD37(void) { return sysregPowerCtrl(0x0100U, 0); }
s32 sceSysreg_driver_84E0F197(s32 unit, s32 enable)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return sysregPowerCtrl(0x10000U << (unit << 1), enable);
}

/* USB / USBhost / MSIF status & interrupts */
s32 sceSysregUsbGetConnectStatus(void)
{
	return (s32)(REG32(0xBC100080) & 1U);
}
s32 sceSysregUsbQueryIntr(void)
{
	return (s32)((REG32(0xBC100080) >> 1) & 0xFU);
}
s32 sceSysregUsbAcquireIntr(s32 mask)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u32 status = (REG32(0xBC100080) >> 1) & 0xFU;
	REG32(0xBC100080) = (((u32)mask & status) & 0xFU) << 1;
	sceKernelCpuResumeIntrWithSync(intr);
	return (s32)status;
}
s32 sceSysregUsbhostQueryIntr(void)
{
	return (s32)(REG32(0xBC1000B0) & 7U);
}
s32 sceSysregUsbhostAcquireIntr(s32 mask)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u32 status = REG32(0xBC1000B0) & 7U;
	REG32(0xBC1000B0) = ((u32)mask & status) & 7U;
	sceKernelCpuResumeIntrWithSync(intr);
	return (s32)status;
}
s32 sceSysregMsifGetConnectStatus(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return (s32)((REG32(0xBC100080) >> ((unit << 3) + 8)) & 1U);
}
s32 sceSysregMsifQueryConnectIntr(s32 unit)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	return (s32)((REG32(0xBC100080) >> ((unit << 3) + 9)) & 7U);
}
s32 sceSysregMsifAcquireConnectIntr(s32 unit, s32 mask)
{
	if (unit < 0 || unit >= 2)
		return (s32)0x80000102;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = (u32)((unit << 3) + 9);
	u32 status = (REG32(0xBC100080) >> shift) & 7U;
	REG32(0xBC100080) = ((u32)mask & status) << shift;
	sceKernelCpuResumeIntrWithSync(intr);
	return (s32)status;
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
	u32 id = cpuid + 1U;
	__asm__ volatile("sync");
	REG32(0xBC100048) = id;
	return ((REG32(0xBC100048) & 3U) == id) ? 0 : -1;
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
	s32 prev = (s32)(val & 1U);
	REG32(0xBC10003C) = (enable != 0) ? (val | 1U) : (val & ~1U);
	sceKernelCpuResumeIntrWithSync(intr);
	return prev;
}
s32 sceSysregDoTimerEvent(s32 unit, s32 op)
{
	if ((u32)unit >= 2U)
		return (s32)0x80000102;
	if ((u32)op >= 4U)
		return (s32)0x80000107;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 val = REG32(0xBC10003C);
	u32 bit = 1U << unit;
	if (op == 1) {
		REG32(0xBC10003C) = val | bit;
	} else if (op == 0) {
		REG32(0xBC10003C) = val & ~bit;
	} else if (op == 2) {
		REG32(0xBC10003C) = val | bit;
		REG32(0xBC10003C) = val & ~bit;
	} else if (op == 3) {
		REG32(0xBC10003C) = val & ~bit;
		REG32(0xBC10003C) = val | bit;
	}
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}
u32 sceSysregGetTachyonVersion(void)
{
	if (g_tachyonVer != 0xFFFFFFFFU)
		return g_tachyonVer;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 raw = REG32(0xBC100040) & 0xFF000000U;
	u32 ver = (raw != 0) ? (raw >> 8) : 0x00100000U;
	g_tachyonVer = ver;
	sceKernelCpuResumeIntr(intr);
	return ver;
}
s32 sceSysregSetAwEdramSize(s32 enable)
{
	if ((s32)sceSysregGetTachyonVersion() <= 0x004FFFFF)
		return 0;
	if (sceSysregGetTachyonVersion() == 0)
		return 0;
	if ((sceSysregGetTachyonVersion() & 0xFFF00000U) == 0x00F00000U)
		return 0;
	s32 intr = sceKernelCpuSuspendIntr();
	__asm__ volatile("sync");
	u32 val = REG32(0xBC100040);
	s32 prev = (s32)((val >> 8) & 1U);
	REG32(0xBC100040) = (enable != 0) ? (val | 0x100U) : (val & ~0x100U);
	sceKernelCpuResumeIntrWithSync(intr);
	return prev;
}
static s32 __attribute__((noinline)) sysregUsbIoRefInc(s32 bit)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u8 mask = g_usbIoRefMask;
	s32 prev = (mask >> bit) & 1;
	if (mask == 0) {
		sceSysregUsbIoEnable();
		mask = g_usbIoRefMask;
	}
	g_usbIoRefMask = (u8)(mask | (1U << bit));
	sceKernelCpuResumeIntr(intr);
	return prev;
}
static s32 __attribute__((noinline)) sysregUsbIoRefDec(s32 bit)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u8 mask = g_usbIoRefMask;
	s32 prev = (mask >> bit) & 1;
	u8 next = (u8)(mask & ~(1U << bit));
	g_usbIoRefMask = next;
	if (next == 0)
		sceSysregUsbIoDisable();
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysreg_driver_15786501(void)
{
	s32 ver = (s32)sceSysregGetTachyonVersion();
	if (ver <= 0x004FFFFF)
		return (s32)0x80000004;
	if (ver > 0x007FFFFF)
		return sysregUsbExtIoEnable();
	return sysregUsbIoRefInc(1);
}
s32 sceSysreg_driver_80944C8A(void)
{
	s32 ver = (s32)sceSysregGetTachyonVersion();
	if (ver <= 0x004FFFFF)
		return (s32)0x80000004;
	if (ver > 0x007FFFFF)
		return sysregUsbExtIoDisable();
	return sysregUsbIoRefDec(1);
}
s32 sceSysreg_driver_4E5C86AA(void)
{
	s32 intr = sceKernelCpuSuspendIntr();
	u32 prev = REG32(0xBC10003C);
	REG32(0xBC10003C) = prev ^ 1U;
	__asm__ volatile("sync");
	REG32(0xBC10003C) = prev;
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}
s32 sceSysregSetMasterPriv(u32 mask, s32 enable)
{
	__asm__ volatile("sync");
	u32 val = REG32(0xBC000044);
	REG32(0xBC000044) = (enable != 0) ? (val | mask) : (val & ~mask);
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
	if ((s32)sceSysregGetTachyonVersion() > 0x007FFFFF)
		return sysregUsbAuxIoEnable();
	return sysregUsbIoRefInc(0);
}
s32 sceSysreg_driver_87B61303(void)
{
	if ((s32)sceSysregGetTachyonVersion() > 0x007FFFFF)
		return sysregUsbAuxIoDisable();
	return sysregUsbIoRefDec(0);
}

/* PLL & Clock selects */
float sceSysregPllGetBaseFrequency(void)
{
	return g_pllBaseFreq;
}
s32 sceSysregPllUpdateFrequency(void)
{
	g_pllFreq.bits = g_pllFreqTableBits[REG32(0xBC100068) & 0xFU];
	return 0;
}
float sceSysregPllGetFrequency(void)
{
	return g_pllFreq.hz;
}
s32 sceSysregPllGetOutSelect(void)
{
	return (s32)(REG32(0xBC100068) & 0xFU);
}
s32 sceSysregPllSetOutSelect(s32 sel)
{
	if (sel < 0 || (sel & 7) >= 6)
		return (s32)0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 val = REG32(0xBC100068);
	s32 prev = (s32)(val & 0xFU);
	REG32(0xBC100068) = (val & ~0xFU) | (u32)sel | 0x80U;
	g_pllFreq.bits = g_pllFreqTableBits[REG32(0xBC100068) & 0xFU];
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregMsifClkSelect(s32 unit, s32 clk)
{
	if ((u32)unit >= 2U)
		return (s32)0x80000102;
	if ((u32)clk >= 3U)
		return (s32)0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = (u32)unit << 1;
	u32 val = REG32(0xBC10005C);
	s32 prev = (s32)((val >> shift) & 3U);
	REG32(0xBC10005C) = (val & ~(3U << shift)) | ((u32)clk << shift);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregMsifDelaySelect(s32 unit, s32 delay)
{
	if ((u32)unit >= 2U)
		return (s32)0x80000102;
	if ((u32)delay >= 8U)
		return (s32)0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = (u32)unit << 2;
	u32 val = REG32(0xBC000050);
	s32 prev = (s32)((val >> shift) & 7U);
	REG32(0xBC000050) = (val & ~(7U << shift)) | ((u32)delay << shift);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysreg_driver_C2F3061F(s32 unit, s32 sel)
{
	if ((u32)unit >= 2U)
		return (s32)0x80000102;
	if ((u32)sel >= 4U)
		return (s32)0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = ((u32)unit << 1) + 10U;
	u32 reg = REG32(0xBC10005C);
	s32 prev = (s32)((reg >> shift) & 3U);
	REG32(0xBC10005C) = (reg & ~(3U << shift)) | ((u32)sel << shift);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregApbTimerClkSelect(s32 unit, s32 clk)
{
	if ((u32)unit >= 4U)
		return (s32)0x80000102;
	if ((u32)clk >= 8U)
		return (s32)0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = (u32)unit << 2;
	u32 reg = REG32(0xBC100060);
	s32 prev = (s32)((reg >> shift) & 7U);
	REG32(0xBC100060) = (reg & ~(7U << shift)) | ((u32)clk << shift);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregAudioClkSelect(s32 unit, s32 clk)
{
	if ((u32)unit >= 2U)
		return (s32)0x80000102;
	if ((u32)clk >= 2U)
		return (s32)0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = (u32)unit + 16U;
	u32 reg = REG32(0xBC100060);
	s32 prev = (s32)((reg >> shift) & 1U);
	REG32(0xBC100060) = (reg & ~(1U << shift)) | ((u32)clk << shift);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregSpiClkSelect(s32 unit, s32 clk)
{
	if ((u32)unit >= 6U)
		return (s32)0x80000102;
	if ((u32)clk >= 8U)
		return (s32)0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = (u32)unit << 2;
	u32 reg = REG32(0xBC100064);
	s32 prev = (s32)((reg >> shift) & 7U);
	REG32(0xBC100064) = (reg & ~(7U << shift)) | ((u32)clk << shift);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysreg_driver_9746F3B2(s32 clk_idx, s32 clk_div)
{
	if ((u32)(clk_div - 1) >= 3U)
		return (s32)0x80000102;
	if (clk_div == 3)
		clk_div = 0;
	if ((s32)sceSysregGetTachyonVersion() > 0x007FFFFF) {
		if ((u32)clk_idx >= 3U)
			return (s32)0x80000102;
		if (clk_idx == 2) {
			if (clk_div != 1)
				return (s32)0x80000102;
			clk_idx = 0;
			clk_div = 3;
		}
	} else {
		if ((u32)clk_idx >= 2U)
			return (s32)0x80000102;
	}
	s32 intr = sceKernelCpuSuspendIntr();
	u32 reg = REG32(0xBC100060);
	s32 prev = (s32)((reg >> 20) & 7U);
	if (prev == 3)
		prev = 1;
	if (prev == 0)
		prev = 3;
	if ((sceSysregGetTachyonVersion() & 0xFFE00000U) == 0x00800000U) {
		if (clk_div == 3 && (REG32(0xBC100100) & 1U) != 0)
			REG32(0xBC100100) &= ~1U;
	}
	REG32(0xBC100060) = (reg & ~(7U << 20)) | (((u32)clk_idx << 22) | ((u32)clk_div << 20));
	if ((sceSysregGetTachyonVersion() & 0xFFE00000U) == 0x00800000U) {
		if (clk_div != 3 && (REG32(0xBC1000E8) & 0xAAAU) == 0 &&
		    (REG32(0xBC100058) & 0x2000000U) == 0 && (REG32(0xBC100100) & 1U) == 0)
			REG32(0xBC100100) |= 1U;
	}
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysreg_driver_434E8AF1(s32 sel)
{
	if ((u32)(sel + 1) >= 3U)
		return (s32)0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 reg = REG32(0xBC100060);
	s32 prev = (s32)((reg >> 24) & 1U);
	if (sel == 1 && (REG32(0xBC100100) & 1U) != 0)
		REG32(0xBC100100) &= ~1U;
	if (sel >= 0)
		REG32(0xBC100060) = (reg & ~(1U << 24)) | ((u32)sel << 24);
	if (sel != 1 && (REG32(0xBC1000E8) & 0xAAAU) == 0 &&
	    (REG32(0xBC100060) & 0x300000U) != 0x300000U && (REG32(0xBC100100) & 1U) == 0)
		REG32(0xBC100100) |= 1U;
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysreg_driver_6596EBC3(s32 unit, s32 sel)
{
	if ((u32)unit >= 6U)
		return (s32)0x80000102;
	if ((u32)sel >= 3U)
		return (s32)0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 shift = (u32)unit << 1;
	u32 reg = REG32(0xBC1000E8);
	s32 prev = (s32)((reg >> shift) & 3U);
	if (sel == 2 && (REG32(0xBC100100) & 1U) != 0)
		REG32(0xBC100100) &= ~1U;
	u32 next = (reg & ~(3U << shift)) | ((u32)sel << shift);
	REG32(0xBC1000E8) = next;
	if ((next & 0xAAAU) == 0 && (REG32(0xBC100060) & 0x300000U) != 0x300000U &&
	    (REG32(0xBC100060) & 0x1000000U) == 0 && (REG32(0xBC100100) & 1U) == 0)
		REG32(0xBC100100) |= 1U;
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregAtaClkSelect(s32 clk)
{
	if ((u32)clk >= 3U)
		return (s32)0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 reg = REG32(0xBC10005C);
	s32 prev = (s32)((reg >> 4) & 3U);
	REG32(0xBC10005C) = (reg & ~(3U << 4)) | ((u32)clk << 4);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregAtahddClkSelect(s32 clk)
{
	if ((u32)clk >= 3U)
		return (s32)0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 reg = REG32(0xBC10005C);
	s32 prev = (s32)((reg >> 8) & 3U);
	REG32(0xBC10005C) = (reg & ~(3U << 8)) | ((u32)clk << 8);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysregAudioClkoutClkSelect(s32 clk)
{
	if ((u32)clk >= 2U)
		return (s32)0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 reg = REG32(0xBC100060);
	s32 prev = (s32)((reg >> 18) & 1U);
	REG32(0xBC100060) = (reg & ~(1U << 18)) | ((u32)clk << 18);
	sceKernelCpuResumeIntr(intr);
	return prev;
}

/* Sysreg sub-interrupts & CPU/Bus clock ratio */
s32 sceSysregRequestIntr(s32 cpu, s32 intr_no)
{
	if ((u32)intr_no >= 32U || (u32)cpu >= 2U)
		return (s32)0x80000102;
	sceSysreg_driver_554E97F7((volatile u32 *)(0xBFC00400 + ((u32)cpu << 2)), 0, 1U << intr_no);
	u32 my_cpu;
	__asm__ volatile("mfc0 %0, $22" : "=r"(my_cpu));
	if ((u32)cpu != my_cpu)
		sceSysregInterruptToOther();
	return 0;
}
static s32 sysregIpiIntrHandler(s32 intr_code, void *common, void *frame)
{
	(void)common;
	u32 my_cpu;
	__asm__ volatile("mfc0 %0, $22" : "=r"(my_cpu));
	volatile u32 *reg = (volatile u32 *)(0xBFC00400 + (my_cpu << 2));
	u32 pending = sceSysreg_driver_1E62714E(reg) & g_sysregSubIntrMask;
	if (pending == 0)
		return -1;
	u32 rot = g_sysregSubIntrRot;
	u32 rval = (pending >> rot) | (pending << (32U - rot));
	u32 lz = (u32)__builtin_clz(rval);
	u32 subintr = (rot - lz + 31U) & 31U;
	sceSysreg_driver_554E97F7(reg, ~(1U << subintr), 0);
	g_sysregSubIntrRot = (g_sysregSubIntrRot + 1U) & 31U;
	sceKernelCallSubIntrHandler(intr_code, (s32)subintr, (void *)subintr, frame);
	return -1;
}
static s32 sysregSubIntrEnable(s32 intr_code, s32 subintr)
{
	(void)intr_code;
	g_sysregSubIntrMask |= (1U << subintr);
	return 0;
}
static s32 sysregSubIntrDisable(s32 intr_code, s32 subintr)
{
	(void)intr_code;
	g_sysregSubIntrMask &= ~(1U << subintr);
	return 0;
}
static s32 sysregSubIntrSuspend(s32 intr_code, s32 subintr, u32 *prev)
{
	(void)intr_code;
	if (prev != NULL)
		*prev = (g_sysregSubIntrMask >> subintr) & 1U;
	g_sysregSubIntrMask &= ~(1U << subintr);
	return 0;
}
static s32 sysregSubIntrResume(s32 intr_code, s32 subintr, u32 prev)
{
	(void)intr_code;
	g_sysregSubIntrMask |= (prev << subintr);
	return 0;
}
static s32 sysregSubIntrIsOccurred(s32 intr_code, s32 subintr)
{
	(void)intr_code;
	u32 my_cpu;
	__asm__ volatile("mfc0 %0, $22" : "=r"(my_cpu));
	volatile u32 *reg = (volatile u32 *)(0xBFC00400 + (my_cpu << 2));
	return (s32)((sceSysreg_driver_1E62714E(reg) >> subintr) & 1U);
}
static SceLowioIntrCb g_sysregSubIntrCb = {
	sizeof(SceLowioIntrCb),
	NULL, NULL, NULL, NULL,
	sysregSubIntrDisable,
	sysregSubIntrEnable,
	sysregSubIntrDisable,
	sysregSubIntrSuspend,
	sysregSubIntrResume,
	sysregSubIntrIsOccurred
};
static SceLowioIntrOpt g_sysregIntrOpt = {
	sizeof(SceLowioIntrOpt),
	32,
	&g_sysregSubIntrCb
};

s32 sceSysregIntrInit(void)
{
	sceKernelRegisterIntrHandler(31, 2, sysregIpiIntrHandler, NULL, &g_sysregIntrOpt);
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
	if ((u32)intr_no >= 32U)
		return (s32)0x80000102;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 mask = g_sysregSubIntrMask;
	s32 prev = (s32)((mask >> intr_no) & 1U);
	g_sysregSubIntrMask = mask | (1U << intr_no);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysreg_driver_BD7B035B(s32 intr_no)
{
	if ((u32)intr_no >= 32U)
		return (s32)0x80000102;
	s32 intr = sceKernelCpuSuspendIntr();
	u32 mask = g_sysregSubIntrMask;
	s32 prev = (s32)((mask >> intr_no) & 1U);
	g_sysregSubIntrMask = mask & ~(1U << intr_no);
	sceKernelCpuResumeIntr(intr);
	return prev;
}
s32 sceSysreg_driver_20B1D0A9(s32 cpu, s32 intr_no)
{
	if ((u32)intr_no >= 32U || (u32)cpu >= 2U)
		return (s32)0x80000102;
	u32 old = sceSysreg_driver_554E97F7((volatile u32 *)(0xBFC00400 + ((u32)cpu << 2)), ~(1U << intr_no), 0);
	return (s32)((old >> intr_no) & 1U);
}

s32 sceSysreg_driver_3F6F2CC7(s32 num, s32 den)
{
	if (den < num || num >= 512 || den == 0)
		return (s32)0x800001FE;
	REG32(0xBC200000) = (((u32)num & 0x1FFU) << 16) | ((u32)den & 0x1FFU);
	__asm__ volatile("sync");
	return 0;
}
s32 sceSysreg_driver_2458B6AC(s32 *num, s32 *den)
{
	if (num != NULL)
		*num = (s32)((REG32(0xBC200000) >> 16) & 0x1FFU);
	if (den != NULL)
		*den = (s32)(REG32(0xBC200000) & 0x1FFU);
	return 0;
}
s32 sceSysreg_driver_63E1EE9C(s32 num, s32 den)
{
	if (den < num || num >= 512 || den == 0)
		return (s32)0x800001FE;
	REG32(0xBC200004) = (((u32)num & 0x1FFU) << 16) | ((u32)den & 0x1FFU);
	__asm__ volatile("sync");
	return 0;
}
s32 sceSysreg_driver_7CF05E81(s32 *num, s32 *den)
{
	if (num != NULL)
		*num = (s32)((REG32(0xBC200004) >> 16) & 0x1FFU);
	if (den != NULL)
		*den = (s32)(REG32(0xBC200004) & 0x1FFU);
	return 0;
}
s32 sceSysreg_driver_B413B041(float freq)
{
	float pll = sceSysregPllGetFrequency();
	s32 ratio = (s32)((freq * 511.0f) / pll);
	if (ratio < 1)
		ratio = 1;
	if (ratio > 511)
		ratio = 511;
	REG32(0xBC200000) = (((u32)ratio & 0x1FFU) << 16) | 0x1FFU;
	__asm__ volatile("sync");
	return 0;
}
float sceSysreg_driver_73EBC752(void)
{
	float pll = sceSysregPllGetFrequency();
	u32 reg = REG32(0xBC200000);
	s32 num = (s32)((reg >> 16) & 0x1FFU);
	s32 den = (s32)(reg & 0x1FFU);
	return (pll * (float)num) / (float)den;
}
s32 sceSysreg_driver_85B74FDA(float freq)
{
	float pll = sceSysregPllGetFrequency();
	s32 ratio = (s32)((freq * 1022.0f) / pll);
	if (ratio < 1)
		ratio = 1;
	if (ratio > 511)
		ratio = 511;
	REG32(0xBC200004) = (((u32)ratio & 0x1FFU) << 16) | 0x1FFU;
	__asm__ volatile("sync");
	return 0;
}
float sceSysreg_driver_96D74557(void)
{
	float pll = sceSysregPllGetFrequency();
	u32 reg = REG32(0xBC200004);
	s32 num = (s32)((reg >> 16) & 0x1FFU);
	s32 den = (s32)((reg & 0x1FFU) << 1);
	return (pll * (float)num) / (float)den;
}

static int sysregSysEventHandler(int ev_id, char *ev_name, void *ev_param, int *result)
{
	(void)ev_id;
	(void)ev_name;
	(void)ev_param;
	(void)result;
	return 0;
}
static SceSysEventHandler g_sysregEvent = {
	sizeof(SceSysEventHandler),
	"SceSysreg",
	0x00FFFF00,
	sysregSysEventHandler,
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
