/*
 * SceLowio's Pervasive driver, first file: the ScePervasive register mappings
 * and the power-event initialization, the register getters, then the reset
 * and gate controls, in the original's order. Its init calls the clock
 * file's LowioPixelClockInit and PervasiveMiscInit (4 bytes) out of line, and
 * that file calls this one's reset and gate wrappers out of line: at Sony's
 * -O3 that means two files, split where the gate controls end. No record
 * names either file; the names are ours.
 */
#include "lowio_internal.h"
#include <stddef.h>
#include <string.h>
#include <sysmem/sysmem.h>
#include <threadmgr/fast_mutex_object.h>
#include <sysmem/dipsw.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysevent.h>
#include <lowio/pervasive.h>

void LowioIdleEntry(void);

/*
 * PervasiveInit's state, one object: the mapping lock and UIDs, the TAS
 * blocks' mappings and the hardware-info registers it caches (the final four
 * names unknown). The boot loaders keep the last two.
 */
static struct {
#if !LOWIO_KBL
	SceKernelFastMutex mutex;
	SceUID uids[16];
#endif
	uint8_t *tas[8];
	uint32_t hardwareInfo[5];
} s_pervasive;

#if !LOWIO_KBL
static const struct {
	const char *name;
	uint32_t paddr;
	uint32_t size;
	void *base;
} s_pervasiveDevices[16] = {
	{ "ScePervasiveMisc", PERVASIVE_MISC_BASE, PERVASIVE_PAGE_SIZE, &s_misc },
	{ "ScePervasiveReset", PERVASIVE_RESET_BASE, PERVASIVE_PAGE_SIZE, &s_reset },
	{ "ScePervasiveGate", PERVASIVE_GATE_BASE, PERVASIVE_PAGE_SIZE, &s_gate },
	{ "ScePervasiveBaseClk", PERVASIVE_BASECLK_BASE, PERVASIVE_PAGE_SIZE, &s_baseClock },
	{ "ScePervasiveVid", PERVASIVE_VID_BASE, PERVASIVE_PAGE_SIZE, &s_video },
	{ "ScePervasiveMailbox", PERVASIVE_MAILBOX_BASE, PERVASIVE_PAGE_SIZE, &s_mailbox },
	{ "ScePervasiveTas0", PERVASIVE_TAS_BASE(0), PERVASIVE_PAGE_SIZE, &s_pervasive.tas[0] },
	{ "ScePervasiveTas1", PERVASIVE_TAS_BASE(1), PERVASIVE_PAGE_SIZE, &s_pervasive.tas[1] },
	{ "ScePervasiveTas2", PERVASIVE_TAS_BASE(2), PERVASIVE_PAGE_SIZE, &s_pervasive.tas[2] },
	{ "ScePervasiveTas3", PERVASIVE_TAS_BASE(3), PERVASIVE_PAGE_SIZE, &s_pervasive.tas[3] },
	{ "ScePervasiveTas4", PERVASIVE_TAS_BASE(4), PERVASIVE_PAGE_SIZE, &s_pervasive.tas[4] },
	{ "ScePervasiveTas5", PERVASIVE_TAS_BASE(5), PERVASIVE_PAGE_SIZE, &s_pervasive.tas[5] },
	{ "ScePervasiveTas6", PERVASIVE_TAS_BASE(6), PERVASIVE_PAGE_SIZE, &s_pervasive.tas[6] },
	{ "ScePervasiveTas7", PERVASIVE_TAS_BASE(7), PERVASIVE_PAGE_SIZE, &s_pervasive.tas[7] },
	{ "ScePervasive2", PERVASIVE2_BASE, PERVASIVE_PAGE_SIZE, &s_pervasive2 },
	{ "SceUartclkReg", UART_CLKGEN_BASE, PERVASIVE_PAGE_SIZE, &s_uartClock },
};
#endif

static inline __attribute__((always_inline)) void PervasiveClockInit(void)
{
	unsigned mode;

	LowioPixelClockInit();
	PervasiveMiscInit();
#if LOWIO_KBL
	mode = 3;
#else
	mode = sceKernelCheckDipsw(DIPSW_GPU_OVERCLOCK) == 0 ? 3 : 5;
#endif
	ScePervasiveForDriver_6AF0196F(mode, mode);
}

#if !LOWIO_KBL
static int PervasiveSysEvent(int resume, int event, void *args, void *opt)
{
	(void)args;
	(void)opt;
	if (resume == 1 && event == 0x10000)
		PervasiveClockInit();
	return 0;
}
#endif

#if LOWIO_KBL
int PervasiveInit(void)
#else
static __attribute__((noinline)) int PervasiveInit(void)
#endif
{
	unsigned i;
#if LOWIO_KBL
	/* the boot loaders run on physical addresses: each block is at its own */
	s_misc = (ScePervasiveMiscRegisters *)PERVASIVE_MISC_BASE;
	s_reset = (ScePervasiveResetRegisters *)PERVASIVE_RESET_BASE;
	s_gate = (ScePervasiveGateRegisters *)PERVASIVE_GATE_BASE;
	s_baseClock = (ScePervasiveBaseClkRegisters *)PERVASIVE_BASECLK_BASE;
	s_video = (ScePervasiveVidRegisters *)PERVASIVE_VID_BASE;
	s_mailbox = (ScePervasiveMailboxRegisters *)PERVASIVE_MAILBOX_BASE;
	for (i = 0; i < 8; i++)
		s_pervasive.tas[i] = (uint8_t *)PERVASIVE_TAS_BASE(i);
	s_pervasive2 = (ScePervasive2Registers *)PERVASIVE2_BASE;
	s_uartClock = (volatile SceUartClkgenRegisters *)UART_CLKGEN_BASE;
#else
	SceKernelAllocMemBlockKernelOpt opt;
	int ret;

	ret = sceKernelInitializeFastMutex(&s_pervasive.mutex, "ScePervasive",
		2, NULL);
	if (ret < 0)
		return ret;
	for (i = 0; i < 16; i++) {
		memset(&opt, 0, sizeof(opt));
		opt.size = 0x58;
		opt.attr = 2;
		opt.paddr = s_pervasiveDevices[i].paddr;
		ret = sceKernelAllocMemBlockForKernel(s_pervasiveDevices[i].name,
			i == 5 ? 0x20100206 : 0x20100806,
			s_pervasiveDevices[i].size, &opt);
		if (ret < 0) {
			unsigned j;

			/* Sony frees the whole UID array, including untouched entries. */
			for (j = 0; j < 16; j++)
				sceKernelFreeMemBlock(s_pervasive.uids[j]);
			sceKernelFinalizeFastMutex(&s_pervasive.mutex);
			return ret;
		}
		s_pervasive.uids[i] = ret;
		sceKernelGetMemBlockBase(ret, (void **)s_pervasiveDevices[i].base);
	}
#endif
	s_pervasive.hardwareInfo[0] = MISC_REGS->revision;
	s_pervasive.hardwareInfo[1] = MISC_REGS->cacheStatus;
	for (i = 0; i < 3; i++)
		s_pervasive.hardwareInfo[i + 2] = MISC_REGS->hardwareInfo[i];
	PervasiveClockInit();
#if !LOWIO_KBL
	sceKernelRegisterSysEventHandler("ScePervasive", PervasiveSysEvent, NULL);
	SceThreadmgrForDriver_B4A05763(LowioIdleEntry);
#endif
	return 0;
}

#if !LOWIO_KBL
int LowioPervasiveInit(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	if (PervasiveInit() < 0)
		return 2;
	return 0;
}

ScePervasiveMiscRegisters *ScePervasiveForDriver_81A155F1(void)
{
	return MISC_REGS;
}

ScePervasiveVidRegisters *ScePervasiveForDriver_47708C03(void)
{
	return s_video;
}

uint32_t ScePervasiveForDriver_04D145C7(void)
{
	return (uintptr_t)s_mailbox;
}

uint32_t ScePervasiveForDriver_270BE98A(unsigned index)
{
	return index < 8 ? (uintptr_t)s_pervasive.tas[index] : 0;
}

uint32_t ScePervasiveForDriver_2A563E35(void)
{
	return (uintptr_t)s_pervasive2;
}

#endif

#if !LOWIO_KBL || LOWIO_NSKBL	/* the boot loader's sdif reads the revision */
uint32_t ScePervasiveForDriver_714EEFB7(void)
{
	return s_pervasive.hardwareInfo[0];
}
#endif

#if !LOWIO_KBL
uint32_t ScePervasiveForDriver_41C3C299(void)
{
	return s_pervasive.hardwareInfo[1];
}

uint32_t ScePervasiveForDriver_FB0EF8C7(void)
{
	return s_pervasive.hardwareInfo[2];
}

uint32_t ScePervasiveForDriver_443E2A3F(void)
{
	return s_pervasive.hardwareInfo[3];
}

uint32_t ScePervasiveForDriver_F58EF3A5(void)
{
	return s_pervasive.hardwareInfo[4];
}

uint32_t ScePervasiveForDriver_84EAB5BC(void)
{
	return Smc110(0, 1);
}

uint32_t ScePervasiveForDriver_E8656ADF(void)
{
	return Smc110(0, 0);
}

uint32_t ScePervasiveForDriver_D58E3A30(void)
{
	return Smc110(1, 1);
}

uint32_t ScePervasiveForDriver_E0711D42(void)
{
	return Smc110(1, 0);
}

uint32_t ScePervasiveForDriver_B2F232E5(uint32_t mode)
{
	return Smc110(2, (mode == 0 ? 1 : 2));
}

uint32_t ScePervasiveForDriver_A7855D3A(void)
{
	return Smc110(2, 3);
}

uint32_t ScePervasiveForDriver_4712740D(void)
{
	return Smc110(2, 0);
}

uint32_t ScePervasiveForDriver_A1D2E440(uint32_t value)
{
	MISC_REGS->audioMode = value;
	return 0;
}
#endif

/*
 * ScePervasiveReset: devices held in reset while their bit is set.
 * Each helper returns whether the bits were set before the change.
 */

/*
 * Set or clear a device's reset bits; returns whether they were set before.
 * Taking a device out of reset waits for the write to land.
 */
static __attribute__((noinline)) int ResetSet(volatile uint32_t *reg,
	uint32_t mask, int enable)
{
	uint32_t state, previous;

	state = PervasiveLock();
	previous = *reg;
	if (!enable) {
		*reg = previous & ~mask;
		LowioDmb();
		(void)*reg;
		LowioDsb();
	} else {
		*reg = previous | mask;
		LowioDmb();
	}
	PervasiveUnlock(state);
	return (previous & mask) != 0;
}

static __attribute__((noinline)) int ResetDisableDevice(volatile uint32_t *reg)
{
	return ResetSet(reg, 1, 0);
}

#if !LOWIO_KBL
static __attribute__((noinline)) int ResetDisableMask(volatile uint32_t *reg,
	uint32_t mask)
{
	return ResetSet(reg, mask, 0);
}
#endif

static __attribute__((noinline)) int ResetEnableDevice(volatile uint32_t *reg)
{
	return ResetSet(reg, 1, 1);
}

#if !LOWIO_KBL
static __attribute__((noinline)) int ResetEnableMask(volatile uint32_t *reg,
	uint32_t mask)
{
	return ResetSet(reg, mask, 1);
}
#endif

#if LOWIO_SKBL
/*
 * The secure loader's devices with a reset hold flag in ScePervasiveMisc:
 * cleared once the device is out of reset, set before it goes back in.
 */
static __attribute__((noinline)) int ResetDisableWithFlag(volatile uint32_t *reg,
	volatile uint32_t *flag)
{
	uint32_t previous = *reg;
	int ret = previous & 1;

	*reg = previous & ~1;
	LowioDmb();
	(void)*reg;
	LowioDsb();
	if (flag != NULL && *flag != 0) {
		*flag = 0;
		(void)*flag;
		LowioDsb();
	}
	return ret;
}

static __attribute__((noinline)) int ResetDisableDeviceWithFlag(volatile uint32_t *reg,
	volatile uint32_t *flag)
{
	return ResetDisableWithFlag(reg, flag);
}

static __attribute__((noinline)) int ResetEnableWithFlag(volatile uint32_t *reg,
	volatile uint32_t *flag)
{
	uint32_t previous = *reg;
	int ret = previous & 1;

	if (flag != NULL) {
		*flag = 1;
		(void)*flag;
		LowioDsb();
	}
	*reg = previous | 1;
	LowioDmb();
	return ret;
}

static __attribute__((noinline)) int ResetEnableDeviceWithFlag(volatile uint32_t *reg,
	volatile uint32_t *flag)
{
	return ResetEnableWithFlag(reg, flag);
}

int ArmDebuggerResetDisable(void)
{
	return ResetDisableDevice(&RESET_REGS->armDebugger);
}

__attribute__((noinline)) int SonyRegbusResetDisable(void)
{
	return ResetDisableDevice(&RESET_REGS->sonyRegbus);
}

__attribute__((noinline)) int CompatRamResetEnable(void)
{
	return ResetEnableDeviceWithFlag(&RESET_REGS->compatRam, &MISC_REGS->compatRamResetHold);
}

__attribute__((noinline)) int CompatRamResetDisable(void)
{
	return ResetDisableDeviceWithFlag(&RESET_REGS->compatRam, &MISC_REGS->compatRamResetHold);
}
#endif

#if !LOWIO_KBL
int ScePervasiveForDriver_3E79D3D3(void)
{
	return ResetEnableDevice(&RESET_REGS->gpu);
}

int ScePervasiveForDriver_8A85E36B(void)
{
	return ResetDisableDevice(&RESET_REGS->gpu);
}

int ScePervasiveForDriver_377126CD(void)
{
	return Smc111(&RESET_REGS->allegrex, 1);
}

int ScePervasiveForDriver_6E11EB97(void)
{
	return Smc111(&RESET_REGS->allegrex, 0);
}

int ScePervasiveForDriver_7B0F388B(void)
{
	return Smc111(&RESET_REGS->sonyRegbus, 1);
}

int ScePervasiveForDriver_4CCD40E6(void)
{
	return Smc111(&RESET_REGS->sonyRegbus, 0);
}

int ScePervasiveForDriver_7C285361(void)
{
	return Smc111(&RESET_REGS->compatRam, 1);
}

int ScePervasiveForDriver_E40BED0F(void)
{
	return Smc111(&RESET_REGS->compatRam, 0);
}

int ScePervasiveForDriver_28731EC5(void)
{
	return ResetEnableDevice(&RESET_REGS->venezia);
}

int ScePervasiveForDriver_A7E64C6F(void)
{
	return ResetDisableDevice(&RESET_REGS->venezia);
}

int ScePervasiveForDriver_31C0A98B(void)
{
	return ResetEnableDevice(&RESET_REGS->vip);
}

int ScePervasiveForDriver_E2D8F6C3(void)
{
	return ResetDisableDevice(&RESET_REGS->vip);
}

int ScePervasiveForDriver_F345FB53(int port)
{
	volatile uint32_t *sdio = RESET_REGS->sdio;

	return Smc111(&sdio[port], 1);
}
#endif

#if !LOWIO_NSKBL
int ScePervasiveForDriver_EDCA3961(int port)
{
	volatile uint32_t *sdio = RESET_REGS->sdio;

#if LOWIO_SKBL
	return ResetDisableDevice(&sdio[port]);
#else
	return Smc111(&sdio[port], 0);
#endif
}
#endif

#if LOWIO_SKBL
int DebugPaResetDisable(void)
{
	return ResetDisableDevice(&RESET_REGS->debugPa);
}
#endif

#if !LOWIO_NSKBL
int ScePervasiveForDriver_FF4B3CA6(int port)
{
	volatile uint32_t *dmac = RESET_REGS->dmac;

	return ResetEnableDevice(&dmac[port]);
}

int ScePervasiveForDriver_AC475961(int port)
{
	volatile uint32_t *dmac = RESET_REGS->dmac;

	return ResetDisableDevice(&dmac[port]);
}
#endif

#if !LOWIO_KBL
int ScePervasiveForDriver_66BF2885(int port)
{
	volatile uint32_t *csi = RESET_REGS->csi;

	return ResetEnableDevice(&csi[port]);
}

int ScePervasiveForDriver_D3B59C33(int port)
{
	volatile uint32_t *csi = RESET_REGS->csi;

	return ResetDisableDevice(&csi[port]);
}

int ScePervasiveForDriver_E3FC1C8D(int port, unsigned int mask)
{
	volatile uint32_t *dsi = RESET_REGS->dsi;

	return ResetEnableMask(&dsi[port], mask);
}

int ScePervasiveForDriver_FFB43AC2(int port, unsigned int mask)
{
	volatile uint32_t *dsi = RESET_REGS->dsi;

	return ResetDisableMask(&dsi[port], mask);
}

int ScePervasiveForDriver_B68254AD(void)
{
	return ResetEnableDevice(&RESET_REGS->iftu);
}

int ScePervasiveForDriver_E92E28FF(void)
{
	return ResetDisableDevice(&RESET_REGS->iftu);
}

int ScePervasiveForDriver_7AE2F8E8(void)
{
	return ResetEnableDevice(&RESET_REGS->unidentifiedDevice);
}

int ScePervasiveForDriver_17109C28(void)
{
	return ResetDisableDevice(&RESET_REGS->unidentifiedDevice);
}

int ScePervasiveForDriver_4AF7A01E(int port, unsigned int mask)
{
	volatile uint32_t *usb = RESET_REGS->usb;

	return ResetEnableMask(&usb[port], mask);
}

int ScePervasiveForDriver_13CC07C9(int port, unsigned int mask)
{
	volatile uint32_t *usb = RESET_REGS->usb;

	return ResetDisableMask(&usb[port], mask);
}
#endif

#if !LOWIO_SKBL
int ScePervasiveForDriver_3D8A8712(int port)
{
	volatile uint32_t *sdif = RESET_REGS->sdif;

	return ResetEnableDevice(&sdif[port]);
}

int ScePervasiveForDriver_D326E15F(int port)
{
	volatile uint32_t *sdif = RESET_REGS->sdif;

	return ResetDisableDevice(&sdif[port]);
}
#endif

#if !LOWIO_KBL
int ScePervasiveForDriver_A3569FF1(void)
{
	return ResetEnableDevice(&RESET_REGS->msif);
}

int ScePervasiveForDriver_CB0F15CD(void)
{
	return ResetDisableDevice(&RESET_REGS->msif);
}

int ScePervasiveForDriver_146108D4(int port)
{
	volatile uint32_t *i2s = RESET_REGS->i2s;

	return ResetEnableDevice(&i2s[port]);
}

int ScePervasiveForDriver_EEE340ED(int port)
{
	volatile uint32_t *i2s = RESET_REGS->i2s;

	return ResetDisableDevice(&i2s[port]);
}

int ScePervasiveForDriver_821AE6C7(int port)
{
	volatile uint32_t *srcMix = RESET_REGS->srcMix;

	return ResetEnableDevice(&srcMix[port]);
}

int ScePervasiveForDriver_5BAFA0DC(int port)
{
	volatile uint32_t *srcMix = RESET_REGS->srcMix;

	return ResetDisableDevice(&srcMix[port]);
}

int ScePervasiveForDriver_B8AF1249(void)
{
	return ResetEnableDevice(&RESET_REGS->spdif);
}

int ScePervasiveForDriver_6A2FB697(void)
{
	return ResetDisableDevice(&RESET_REGS->spdif);
}

int ScePervasiveForDriver_731A097D(void)
{
	return ResetEnableDevice(&RESET_REGS->gpio);
}

int ScePervasiveForDriver_78C34032(void)
{
	return ResetDisableDevice(&RESET_REGS->gpio);
}

int ScePervasiveForDriver_7B16F900(int port)
{
	volatile uint32_t *spi = RESET_REGS->spi;

	return ResetEnableDevice(&spi[port]);
}

int ScePervasiveForDriver_E4B145AE(int port)
{
	volatile uint32_t *spi = RESET_REGS->spi;

	return ResetDisableDevice(&spi[port]);
}

int ScePervasiveForDriver_41F9E1C3(int port)
{
	volatile uint32_t *i2c = RESET_REGS->i2c;

	return ResetEnableDevice(&i2c[port]);
}

int ScePervasiveForDriver_86BEA9A4(int port)
{
	volatile uint32_t *i2c = RESET_REGS->i2c;

	return ResetDisableDevice(&i2c[port]);
}
#endif

int ScePervasiveForDriver_788B6C61(int port)
{
	volatile uint32_t *uart = RESET_REGS->uart;

	return ResetEnableDevice(&uart[port]);
}

int ScePervasiveForDriver_A7CE7DCC(int port)
{
	volatile uint32_t *uart = RESET_REGS->uart;

	return ResetDisableDevice(&uart[port]);
}

#if LOWIO_SKBL
int DebugBusResetDisable(void)
{
	return ResetDisableDevice(&RESET_REGS->debugBus);
}

int BootDeviceResetDisable(void)
{
	return ResetDisableDeviceWithFlag(&RESET_REGS->bootDevice, &MISC_REGS->bootDeviceResetHold);
}

int Lpddr2ResetDisable(int port)
{
	return ResetDisableDevice(&RESET_REGS->lpddr2[port]);
}

int TimerResetDisable(void)
{
	return ResetDisableDevice(&RESET_REGS->timer);
}

int DebugDeviceResetDisable(void)
{
	return ResetDisableDevice(&RESET_REGS->debugDevice);
}

/* isolation and bus idle (ScePervasiveMisc) */
__attribute__((noinline)) int CompatRamIsolationDisable(void)
{
	uint32_t previous = MISC_REGS->compatRamIsolation;

	MISC_REGS->compatRamIsolation = 0;
	LowioDmb();
	return previous != 0;
}

__attribute__((noinline)) int SonyRegbusIsolationDisable(void)
{
	uint32_t previous = MISC_REGS->sonyRegbusIsolation;

	MISC_REGS->sonyRegbusIsolation = 0;
	LowioDmb();
	return previous != 0;
}

int Lpddr2IsolationDisable(int port)
{
	uint32_t previous = MISC_REGS->lpddr2Isolation[port];

	MISC_REGS->lpddr2Isolation[port] = 0;
	LowioDmb();
	return previous != 0;
}

__attribute__((noinline)) int CompatRamBusIdle(void)
{
	volatile uint32_t *request = &MISC_REGS->compatRamBusIdleRequest;

	*request = 1;
	while (*request & 1)
		;
	while ((MISC_REGS->busIdleStatus & 8) == 0)
		;
	MISC_REGS->busIdleStatus = 8;
	while (MISC_REGS->busIdleStatus & 8)
		;
	return 0;
}

__attribute__((noinline)) int SonyRegbusBusIdle(void)
{
	volatile uint32_t *request = &MISC_REGS->sonyRegbusBusIdleRequest;

	*request = 1;
	while (*request & 1)
		;
	while ((MISC_REGS->busIdleStatus & 0x10) == 0)
		;
	MISC_REGS->busIdleStatus = 0x10;
	while (MISC_REGS->busIdleStatus & 0x10)
		;
	return 0;
}
#endif

#if !LOWIO_KBL
int ScePervasiveForDriver_EBE9C84E(void)
{
	return Smc111(&RESET_REGS->vipRegisterAccess, 1);
}

int ScePervasiveForDriver_8CF567AD(void)
{
	return Smc111(&RESET_REGS->vipRegisterAccess, 0);
}

static inline __attribute__((always_inline)) uint32_t MiscReplaceLocked(
	volatile uint32_t *reg, uint32_t value)
{
	uint32_t intr, previous;

	intr = PervasiveLock();
	previous = MiscReplace(reg, value);
	PervasiveUnlock(intr);
	return previous;
}

uint32_t ScePervasiveForDriver_F1908D3C(unsigned head)
{
	volatile ScePervasiveMiscRegisters *misc = MISC_REGS;

	if (head != 0)
		return MiscReplaceLocked(&misc->dsi1Control, 1);
	return MiscReplaceLocked(&misc->dsi0Control, 1);
}

uint32_t ScePervasiveForDriver_350BF570(unsigned head)
{
	volatile ScePervasiveMiscRegisters *misc = MISC_REGS;

	if (head != 0)
		return MiscReplaceLocked(&misc->dsi1Control, 0);
	return MiscReplaceLocked(&misc->dsi0Control, 0);
}

uint32_t ScePervasiveForDriver_0C1A7FFA(void)
{
	volatile ScePervasiveMiscRegisters *misc = MISC_REGS;

	return MiscReplaceLocked(&misc->csiControl, 1);
}

uint32_t ScePervasiveForDriver_5B4EF352(void)
{
	volatile ScePervasiveMiscRegisters *misc = MISC_REGS;

	return MiscReplaceLocked(&misc->csiControl, 0);
}
#endif

/*
 * ScePervasiveGate: a device's clock runs while its bit is set.
 * Each helper returns whether the bits were set before the change.
 */

/*
 * Start or stop a device's clocks; returns whether they ran before. Both wait
 * for the write to land.
 */
static __attribute__((noinline)) int GateSet(volatile uint32_t *reg,
	uint32_t mask, int enable)
{
	uint32_t state, previous;

	state = PervasiveLock();
	previous = *reg;
	if (enable)
		*reg = previous | mask;
	else
		*reg = previous & ~mask;
	LowioDmb();
	(void)*reg;
	LowioDsb();
	PervasiveUnlock(state);
	return (previous & mask) != 0;
}

static __attribute__((noinline)) int GateEnableDevice(volatile uint32_t *reg)
{
	return GateSet(reg, 1, 1);
}

#if !LOWIO_NSKBL
static __attribute__((noinline)) int GateDisableDevice(volatile uint32_t *reg)
{
	return GateSet(reg, 1, 0);
}
#endif

#if !LOWIO_KBL
int ScePervasiveForDriver_39E51AE2(unsigned int mask)
{
	return GateSet(&GATE_REGS->gpu, mask & 0xf000f, 1);
}

int ScePervasiveForDriver_CA0ACFC5(unsigned int mask)
{
	return GateSet(&GATE_REGS->gpu, mask & 0xf000f, 0);
}

int ScePervasiveForDriver_8EE3AEDF(void)
{
	return Smc113(&GATE_REGS->allegrex, 1);
}

int ScePervasiveForDriver_3BF2A9B5(void)
{
	return Smc113(&GATE_REGS->allegrex, 0);
}

int ScePervasiveForDriver_7F4AB4AA(void)
{
	return Smc113(&GATE_REGS->sonyRegbus, 1);
}

int ScePervasiveForDriver_0EBBE8DE(void)
{
	return Smc113(&GATE_REGS->sonyRegbus, 0);
}

int ScePervasiveForDriver_B2EE45C9(void)
{
	return Smc113(&GATE_REGS->compatRam, 1);
}

int ScePervasiveForDriver_39979C55(void)
{
	return Smc113(&GATE_REGS->compatRam, 0);
}

int ScePervasiveForDriver_FB01A2DD(void)
{
	return GateSet(&GATE_REGS->venezia, 1, 1);
}

int ScePervasiveForDriver_2EEBE9AE(void)
{
	return GateSet(&GATE_REGS->venezia, 1, 0);
}

int ScePervasiveForDriver_B9904983(void)
{
	return GateSet(&GATE_REGS->venezia, 2, 1);
}

int ScePervasiveForDriver_FBE5AA2F(void)
{
	return GateSet(&GATE_REGS->venezia, 2, 0);
}

int ScePervasiveForDriver_B1CFA18F(void)
{
	return GateEnableDevice(&GATE_REGS->vip);
}

int ScePervasiveForDriver_03E1FAA6(void)
{
	return GateDisableDevice(&GATE_REGS->vip);
}
#endif

#if LOWIO_SKBL
/* the cores' clocks: bits 20-23, two of them inverted */
int ArmGateEnable(uint32_t mask)
{
	return GateSet(&GATE_REGS->arm, (mask & 0xf00000) ^ 0x300000, 1);
}

int ArmDebuggerGateEnable(void)
{
	return GateEnableDevice(&GATE_REGS->armDebugger);
}

__attribute__((noinline)) int SonyRegbusGateEnable(void)
{
	return GateEnableDevice(&GATE_REGS->sonyRegbus);
}

__attribute__((noinline)) int CompatRamGateEnable(void)
{
	return GateEnableDevice(&GATE_REGS->compatRam);
}

__attribute__((noinline)) int CompatRamGateDisable(void)
{
	return GateDisableDevice(&GATE_REGS->compatRam);
}
#endif

#if !LOWIO_NSKBL
int ScePervasiveForDriver_D34E74C5(int port)
{
	volatile uint32_t *sdio = GATE_REGS->sdio;

#if LOWIO_SKBL
	return GateEnableDevice(&sdio[port]);
#else
	return Smc113(&sdio[port], 1);
#endif
}
#endif

#if LOWIO_SKBL
/* Debug PA: two clock bits */
int DebugPaGateEnable(void)
{
	return GateSet(&GATE_REGS->debugPa, 3, 1);
}
#endif

#if !LOWIO_KBL
int ScePervasiveForDriver_701D5355(int port)
{
	volatile uint32_t *sdio = GATE_REGS->sdio;

	return Smc113(&sdio[port], 0);
}
#endif

#if !LOWIO_NSKBL
int ScePervasiveForDriver_A10583FF(int port)
{
	volatile uint32_t *dmac = GATE_REGS->dmac;

#if LOWIO_SKBL
	return GateEnableDevice(&dmac[port]);
#else
	return Smc113(&dmac[port], 1);
#endif
}
#endif

#if !LOWIO_KBL
int ScePervasiveForDriver_15237AF8(int port)
{
	volatile uint32_t *dmac = GATE_REGS->dmac;

	return Smc113(&dmac[port], 0);
}

int ScePervasiveForDriver_1068C067(int port)
{
	volatile uint32_t *csi = GATE_REGS->csi;

	return GateEnableDevice(&csi[port]);
}

int ScePervasiveForDriver_4B223AF5(int port)
{
	volatile uint32_t *csi = GATE_REGS->csi;

	return GateDisableDevice(&csi[port]);
}

int ScePervasiveForDriver_BC42C72F(int port, unsigned int mask)
{
	volatile uint32_t *dsi = GATE_REGS->dsi;

	return GateSet(&dsi[port], mask, 1);
}

int ScePervasiveForDriver_25AE181E(int port, unsigned int mask)
{
	volatile uint32_t *dsi = GATE_REGS->dsi;

	return GateSet(&dsi[port], mask, 0);
}

int ScePervasiveForDriver_07F2A738(void)
{
	return GateEnableDevice(&GATE_REGS->iftu);
}

int ScePervasiveForDriver_5AFE0AF0(void)
{
	return GateDisableDevice(&GATE_REGS->iftu);
}

int ScePervasiveForDriver_C0C842FE(void)
{
	return GateEnableDevice(&GATE_REGS->unidentifiedDevice);
}

int ScePervasiveForDriver_9BB7B932(void)
{
	return GateDisableDevice(&GATE_REGS->unidentifiedDevice);
}

int ScePervasiveForDriver_A2EFD7AF(int port, unsigned int mask)
{
	volatile uint32_t *usb = GATE_REGS->usb;

	return GateSet(&usb[port], mask, 1);
}

int ScePervasiveForDriver_AD1E81EB(int port, unsigned int mask)
{
	volatile uint32_t *usb = GATE_REGS->usb;

	return GateSet(&usb[port], mask, 0);
}
#endif

#if !LOWIO_SKBL
int ScePervasiveForDriver_3CF7A012(int port)
{
	volatile uint32_t *sdif = GATE_REGS->sdif;

	return GateEnableDevice(&sdif[port]);
}
#endif

#if !LOWIO_KBL
int ScePervasiveForDriver_4E43299D(int port)
{
	volatile uint32_t *sdif = GATE_REGS->sdif;

	return GateDisableDevice(&sdif[port]);
}

int ScePervasiveForDriver_7704C013(void)
{
	return GateEnableDevice(&GATE_REGS->msif);
}

int ScePervasiveForDriver_2A9778CD(void)
{
	return GateDisableDevice(&GATE_REGS->msif);
}

int ScePervasiveForDriver_4235EB7D(int port)
{
	volatile uint32_t *i2s = GATE_REGS->i2s;

	return GateEnableDevice(&i2s[port]);
}

int ScePervasiveForDriver_A3CF9844(int port)
{
	volatile uint32_t *i2s = GATE_REGS->i2s;

	return GateDisableDevice(&i2s[port]);
}

int ScePervasiveForDriver_47F2C35C(int port)
{
	volatile uint32_t *srcMix = GATE_REGS->srcMix;

	return GateEnableDevice(&srcMix[port]);
}

int ScePervasiveForDriver_F861FEE4(int port)
{
	volatile uint32_t *srcMix = GATE_REGS->srcMix;

	return GateDisableDevice(&srcMix[port]);
}

int ScePervasiveForDriver_6B5DEDA2(void)
{
	return GateEnableDevice(&GATE_REGS->spdif);
}

int ScePervasiveForDriver_51981FEE(void)
{
	return GateDisableDevice(&GATE_REGS->spdif);
}
#endif

int ScePervasiveForDriver_2F195C97(void)
{
	return GateEnableDevice(&GATE_REGS->gpio);
}

#if !LOWIO_KBL
int ScePervasiveForDriver_EB176898(void)
{
	return GateDisableDevice(&GATE_REGS->gpio);
}

int ScePervasiveForDriver_DFD96BFC(int port)
{
	volatile uint32_t *spi = GATE_REGS->spi;

	return GateEnableDevice(&spi[port]);
}

int ScePervasiveForDriver_A85BF98A(int port)
{
	volatile uint32_t *spi = GATE_REGS->spi;

	return GateDisableDevice(&spi[port]);
}

int ScePervasiveForDriver_C37CFDFC(int port)
{
	volatile uint32_t *i2c = GATE_REGS->i2c;

	return GateEnableDevice(&i2c[port]);
}

int ScePervasiveForDriver_3F3E1AEA(int port)
{
	volatile uint32_t *i2c = GATE_REGS->i2c;

	return GateDisableDevice(&i2c[port]);
}
#endif

int ScePervasiveForDriver_EFD084D8(int port)
{
	volatile uint32_t *uart = GATE_REGS->uart;

	return GateEnableDevice(&uart[port]);
}

#if LOWIO_SKBL
int DebugBusGateEnable(void)
{
	return GateEnableDevice(&GATE_REGS->debugBus);
}

int BootDeviceGateEnable(void)
{
	return GateEnableDevice(&GATE_REGS->bootDevice);
}

int Lpddr2GateEnable(int port)
{
	return GateEnableDevice(&GATE_REGS->lpddr2[port]);
}

int TimerGateEnable(void)
{
	return GateEnableDevice(&GATE_REGS->timer);
}

int DebugDeviceGateEnable(void)
{
	return GateEnableDevice(&GATE_REGS->debugDevice);
}
#endif

#if !LOWIO_KBL
int ScePervasiveForDriver_18DD8043(int port)
{
	volatile uint32_t *uart = GATE_REGS->uart;

	return GateDisableDevice(&uart[port]);
}
#endif
