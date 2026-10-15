/*
 * The boot loaders' Pervasive driver: SceLowio's reset, gate and clock
 * helpers on the registers' physical addresses, without its lock (the boot
 * loaders run on one core with interrupts off). Each reset and gate change
 * returns whether the bits were set before. The secure loader (SKBL,
 * PERVASIVE_SKBL) also starts the secure devices (pervasive_init_kbl.c sets
 * its register pointers); NSKBL keeps only what its SD and UART drivers use. The
 * functions are in register order.
 *
 * Internal names are unknown; they follow SceLowio's exports and SceDriverTzs
 * where the code is the same.
 */
#include <stddef.h>
#include <stdint.h>
#include <sce/pervasive_regs.h>
#include <sce/pervasive_pixel.h>

#define PERVASIVE_ERROR		((int)0x803f0000)

#define PERVASIVE_MISC		((volatile ScePervasiveMiscRegisters *)PERVASIVE_MISC_BASE)
#define PERVASIVE_RESET		((ScePervasiveResetRegisters *)PERVASIVE_RESET_BASE)
#define PERVASIVE_GATE		((ScePervasiveGateRegisters *)PERVASIVE_GATE_BASE)
#define PERVASIVE_BASECLK	((volatile ScePervasiveBaseClkRegisters *)PERVASIVE_BASECLK_BASE)

static inline void PervasiveDmb(void)
{
	__asm__ volatile("dmb sy");
}

static inline void PervasiveDsb(void)
{
	__asm__ volatile("dsb sy" : : : "memory");
}


/* ===== reset ===== */

__attribute__((noinline)) int ResetSet(volatile uint32_t *reg, uint32_t mask,
	int enable)
{
	uint32_t previous = *reg;
	int ret = (previous & mask) != 0;

	if (!enable) {
		*reg = previous & ~mask;
		PervasiveDmb();
		(void)*reg;
		PervasiveDsb();
	} else {
		*reg = previous | mask;
		PervasiveDmb();
	}
	return ret;
}

__attribute__((noinline)) int ResetDisableDevice(volatile uint32_t *reg)
{
	return ResetSet(reg, 1, 0);
}

__attribute__((noinline)) int ResetEnableDevice(volatile uint32_t *reg)
{
	return ResetSet(reg, 1, 1);
}

#ifdef PERVASIVE_SKBL
/* The flag word, when given, is cleared once the device is out of reset and
 * set before it goes back in. */
__attribute__((noinline)) int ResetDisableWithFlag(volatile uint32_t *reg,
	volatile uint32_t *flag)
{
	uint32_t previous = *reg;
	int ret = previous & 1;

	*reg = previous & ~1;
	PervasiveDmb();
	(void)*reg;
	PervasiveDsb();
	if (flag != NULL && *flag != 0) {
		*flag = 0;
		(void)*flag;
		PervasiveDsb();
	}
	return ret;
}

__attribute__((noinline)) int ResetDisableDeviceWithFlag(volatile uint32_t *reg,
	volatile uint32_t *flag)
{
	return ResetDisableWithFlag(reg, flag);
}

__attribute__((noinline)) int ResetEnableWithFlag(volatile uint32_t *reg,
	volatile uint32_t *flag)
{
	uint32_t previous = *reg;
	int ret = previous & 1;

	if (flag != NULL) {
		*flag = 1;
		(void)*flag;
		PervasiveDsb();
	}
	*reg = previous | 1;
	PervasiveDmb();
	return ret;
}

__attribute__((noinline)) int ResetEnableDeviceWithFlag(volatile uint32_t *reg,
	volatile uint32_t *flag)
{
	return ResetEnableWithFlag(reg, flag);
}

int ArmDebuggerResetDisable(void)
{
	return ResetDisableDevice(&PERVASIVE_RESET->armDebugger);
}

__attribute__((noinline)) int SonyRegbusResetDisable(void)
{
	return ResetDisableDevice(&PERVASIVE_RESET->sonyRegbus);
}

__attribute__((noinline)) int CompatRamResetEnable(void)
{
	return ResetEnableDeviceWithFlag(&PERVASIVE_RESET->compatRam, &PERVASIVE_MISC->compatRamResetHold);
}

__attribute__((noinline)) int CompatRamResetDisable(void)
{
	return ResetDisableDeviceWithFlag(&PERVASIVE_RESET->compatRam, &PERVASIVE_MISC->compatRamResetHold);
}

int ScePervasiveForDriver_EDCA3961(int port)
{
	return ResetDisableDevice(&PERVASIVE_RESET->sdio[port]);
}

int DebugPaResetDisable(void)
{
	return ResetDisableDevice(&PERVASIVE_RESET->debugPa);
}

int ScePervasiveForDriver_FF4B3CA6(int port)
{
	return ResetEnableDevice(&PERVASIVE_RESET->dmac[port]);
}

int ScePervasiveForDriver_AC475961(int port)
{
	return ResetDisableDevice(&PERVASIVE_RESET->dmac[port]);
}
#else
int ScePervasiveForDriver_3D8A8712(int port)
{
	return ResetEnableDevice(&PERVASIVE_RESET->sdif[port]);
}

int ScePervasiveForDriver_D326E15F(int port)
{
	return ResetDisableDevice(&PERVASIVE_RESET->sdif[port]);
}
#endif

int kscePervasiveUartResetEnable(int port)
{
	return ResetEnableDevice(&PERVASIVE_RESET->uart[port]);
}

int kscePervasiveUartResetDisable(int port)
{
	return ResetDisableDevice(&PERVASIVE_RESET->uart[port]);
}

#ifdef PERVASIVE_SKBL
int DebugBusResetDisable(void)
{
	return ResetDisableDevice(&PERVASIVE_RESET->debugBus);
}

int BootDeviceResetDisable(void)
{
	return ResetDisableDeviceWithFlag(&PERVASIVE_RESET->bootDevice, &PERVASIVE_MISC->bootDeviceResetHold);
}

int Lpddr2ResetDisable(int port)
{
	return ResetDisableDevice(&PERVASIVE_RESET->lpddr2[port]);
}

int TimerResetDisable(void)
{
	return ResetDisableDevice(&PERVASIVE_RESET->timer);
}

int DebugDeviceResetDisable(void)
{
	return ResetDisableDevice(&PERVASIVE_RESET->debugDevice);
}

/* ===== isolation and bus idle (ScePervasiveMisc) ===== */

__attribute__((noinline)) int CompatRamIsolationDisable(void)
{
	uint32_t previous = PERVASIVE_MISC->compatRamIsolation;

	PERVASIVE_MISC->compatRamIsolation = 0;
	PervasiveDmb();
	return previous != 0;
}

__attribute__((noinline)) int SonyRegbusIsolationDisable(void)
{
	uint32_t previous = PERVASIVE_MISC->sonyRegbusIsolation;

	PERVASIVE_MISC->sonyRegbusIsolation = 0;
	PervasiveDmb();
	return previous != 0;
}

int Lpddr2IsolationDisable(int port)
{
	uint32_t previous = PERVASIVE_MISC->lpddr2Isolation[port];

	PERVASIVE_MISC->lpddr2Isolation[port] = 0;
	PervasiveDmb();
	return previous != 0;
}

__attribute__((noinline)) int CompatRamBusIdle(void)
{
	volatile uint32_t *request = &PERVASIVE_MISC->compatRamBusIdleRequest;

	*request = 1;
	while (*request & 1)
		;
	while ((PERVASIVE_MISC->busIdleStatus & 8) == 0)
		;
	PERVASIVE_MISC->busIdleStatus = 8;
	while (PERVASIVE_MISC->busIdleStatus & 8)
		;
	return 0;
}

__attribute__((noinline)) int SonyRegbusBusIdle(void)
{
	volatile uint32_t *request = &PERVASIVE_MISC->sonyRegbusBusIdleRequest;

	*request = 1;
	while (*request & 1)
		;
	while ((PERVASIVE_MISC->busIdleStatus & 0x10) == 0)
		;
	PERVASIVE_MISC->busIdleStatus = 0x10;
	while (PERVASIVE_MISC->busIdleStatus & 0x10)
		;
	return 0;
}
#endif

/* ===== gate ===== */

__attribute__((noinline)) int GateDevice(volatile uint32_t *reg, int enable)
{
	uint32_t previous = *reg;
	int ret = previous & 1;

	if (enable)
		previous |= 1;
	else
		previous &= ~1;
	*reg = previous;
	PervasiveDmb();
	(void)*reg;
	PervasiveDsb();
	return ret;
}

#ifdef PERVASIVE_SKBL
__attribute__((noinline)) int GateDisableDevice(volatile uint32_t *reg)
{
	return GateDevice(reg, 0);
}
#endif

__attribute__((noinline)) int GateEnableDevice(volatile uint32_t *reg)
{
	return GateDevice(reg, 1);
}

#ifdef PERVASIVE_SKBL
/* a device with two clock bits (Debug PA) */
__attribute__((noinline)) int GateDeviceDual(volatile uint32_t *reg, int enable)
{
	uint32_t previous = *reg;
	int ret = (previous & 3) != 0;

	if (enable)
		previous |= 3;
	else
		previous &= ~3;
	*reg = previous;
	PervasiveDmb();
	(void)*reg;
	PervasiveDsb();
	return ret;
}

__attribute__((noinline)) int GateEnable(volatile uint32_t *reg, uint32_t mask)
{
	uint32_t previous = *reg;

	*reg = previous | mask;
	PervasiveDmb();
	(void)*reg;
	PervasiveDsb();
	return (previous & mask) != 0;
}

/* the cores' clocks: bits 20-23, two of them inverted */
int ArmGateEnable(uint32_t mask)
{
	return GateEnable(&PERVASIVE_GATE->arm, (mask & 0xf00000) ^ 0x300000);
}

int ArmDebuggerGateEnable(void)
{
	return GateEnableDevice(&PERVASIVE_GATE->armDebugger);
}

__attribute__((noinline)) int SonyRegbusGateEnable(void)
{
	return GateEnableDevice(&PERVASIVE_GATE->sonyRegbus);
}

__attribute__((noinline)) int CompatRamGateEnable(void)
{
	return GateEnableDevice(&PERVASIVE_GATE->compatRam);
}

__attribute__((noinline)) int CompatRamGateDisable(void)
{
	return GateDisableDevice(&PERVASIVE_GATE->compatRam);
}

int ScePervasiveForDriver_D34E74C5(int port)
{
	return GateEnableDevice(&PERVASIVE_GATE->sdio[port]);
}

int DebugPaGateEnable(void)
{
	return GateDeviceDual(&PERVASIVE_GATE->debugPa, 1);
}

int ScePervasiveForDriver_A10583FF(int port)
{
	return GateEnableDevice(&PERVASIVE_GATE->dmac[port]);
}
#else
int ScePervasiveForDriver_3CF7A012(int port)
{
	return GateEnableDevice(&PERVASIVE_GATE->sdif[port]);
}
#endif

int kscePervasiveGpioClockEnable(void)
{
	return GateEnableDevice(&PERVASIVE_GATE->gpio);
}

int kscePervasiveUartClockEnable(int port)
{
	return GateEnableDevice(&PERVASIVE_GATE->uart[port]);
}

#ifdef PERVASIVE_SKBL
int DebugBusGateEnable(void)
{
	return GateEnableDevice(&PERVASIVE_GATE->debugBus);
}

int BootDeviceGateEnable(void)
{
	return GateEnableDevice(&PERVASIVE_GATE->bootDevice);
}

int Lpddr2GateEnable(int port)
{
	return GateEnableDevice(&PERVASIVE_GATE->lpddr2[port]);
}

int TimerGateEnable(void)
{
	return GateEnableDevice(&PERVASIVE_GATE->timer);
}

int DebugDeviceGateEnable(void)
{
	return GateEnableDevice(&PERVASIVE_GATE->debugDevice);
}
#endif

/* ===== clocks ===== */

int ScePervasiveForDriver_6AF0196F(unsigned int core, unsigned int mp)
{
	if (core > 7 || mp > 7)
		return PERVASIVE_ERROR;
	PERVASIVE_BASECLK->gpuClock = core << 16 | mp;
	PervasiveDmb();
	return 0;
}

static PixelClockContext s_pixelClock[2];

int LowioPixelClockInit(void)
{
	s_pixelClock[0].setting = NULL;
	s_pixelClock[1].setting = NULL;
	s_pixelClock[0].registers = &PERVASIVE_BASECLK->dsi0PixelClock;
	s_pixelClock[0].pllConfig = s_pixelClock[0].registers->pllConfig;
	s_pixelClock[1].registers = &PERVASIVE_BASECLK->dsi1PixelClock;
	s_pixelClock[1].pllConfig = s_pixelClock[1].registers->pllConfig;
	return 0;
}

#ifdef PERVASIVE_SKBL
/* ===== the secure devices' power-on ===== */

__attribute__((noinline)) int CompatRamPowerOn(void)
{
	PERVASIVE_BASECLK->compatClock = 0;
	PervasiveDmb();
	CompatRamResetEnable();
	CompatRamGateEnable();
	CompatRamIsolationDisable();
	CompatRamBusIdle();
	CompatRamGateDisable();
	CompatRamResetDisable();
	CompatRamGateEnable();
	return 0;
}

__attribute__((noinline)) int SonyRegbusPowerOn(void)
{
	SonyRegbusIsolationDisable();
	SonyRegbusBusIdle();
	PERVASIVE_BASECLK->allegrexBusClockEnable = 0;
	PervasiveDmb();
	SonyRegbusGateEnable();
	SonyRegbusResetDisable();
	return 0;
}

int PervasiveMiscInit(void)
{
	return 0;
}
#endif
