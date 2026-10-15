/*
 * The boot loaders' Pervasive driver: SceLowio's reset, gate and clock
 * helpers on the registers' physical addresses, without its lock (the boot
 * loaders run on one core with interrupts off). Each reset and gate change
 * returns whether the bits were set before. The secure loader (SKBL,
 * PERVASIVE_SKBL) also starts the secure devices; NSKBL keeps only what its
 * SD and UART drivers use. The functions are in register order.
 *
 * Internal names are unknown; they follow SceLowio's exports and SceDriverTzs
 * where the code is the same.
 */
#include <stddef.h>
#include <stdint.h>
#include <sce/pervasive_regs.h>
#include <sce/pervasive_pixel.h>

#define PERVASIVE_ERROR		((int)0x803f0000)

#define s_misc		((volatile ScePervasiveMiscRegisters *)PERVASIVE_MISC_BASE)
#define s_reset		((ScePervasiveResetRegisters *)PERVASIVE_RESET_BASE)
#define s_gate		((ScePervasiveGateRegisters *)PERVASIVE_GATE_BASE)
#define s_baseClock	((volatile ScePervasiveBaseClkRegisters *)PERVASIVE_BASECLK_BASE)

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
	return ResetDisableDevice(&s_reset->armDebugger);
}

__attribute__((noinline)) int SonyRegbusResetDisable(void)
{
	return ResetDisableDevice(&s_reset->sonyRegbus);
}

__attribute__((noinline)) int CompatRamResetEnable(void)
{
	return ResetEnableDeviceWithFlag(&s_reset->compatRam, &s_misc->compatRamResetHold);
}

__attribute__((noinline)) int CompatRamResetDisable(void)
{
	return ResetDisableDeviceWithFlag(&s_reset->compatRam, &s_misc->compatRamResetHold);
}

int ScePervasiveForDriver_EDCA3961(int port)
{
	return ResetDisableDevice(&s_reset->sdio[port]);
}

int DebugPaResetDisable(void)
{
	return ResetDisableDevice(&s_reset->debugPa);
}

int ScePervasiveForDriver_FF4B3CA6(int port)
{
	return ResetEnableDevice(&s_reset->dmac[port]);
}

int ScePervasiveForDriver_AC475961(int port)
{
	return ResetDisableDevice(&s_reset->dmac[port]);
}
#else
int ScePervasiveForDriver_3D8A8712(int port)
{
	return ResetEnableDevice(&s_reset->sdif[port]);
}

int ScePervasiveForDriver_D326E15F(int port)
{
	return ResetDisableDevice(&s_reset->sdif[port]);
}
#endif

int kscePervasiveUartResetEnable(int port)
{
	return ResetEnableDevice(&s_reset->uart[port]);
}

int kscePervasiveUartResetDisable(int port)
{
	return ResetDisableDevice(&s_reset->uart[port]);
}

#ifdef PERVASIVE_SKBL
int DebugBusResetDisable(void)
{
	return ResetDisableDevice(&s_reset->debugBus);
}

int BootDeviceResetDisable(void)
{
	return ResetDisableDeviceWithFlag(&s_reset->bootDevice, &s_misc->bootDeviceResetHold);
}

int Lpddr2ResetDisable(int port)
{
	return ResetDisableDevice(&s_reset->lpddr2[port]);
}

int TimerResetDisable(void)
{
	return ResetDisableDevice(&s_reset->timer);
}

int DebugDeviceResetDisable(void)
{
	return ResetDisableDevice(&s_reset->debugDevice);
}

/* ===== isolation and bus idle (ScePervasiveMisc) ===== */

__attribute__((noinline)) int CompatRamIsolationDisable(void)
{
	uint32_t previous = s_misc->compatRamIsolation;

	s_misc->compatRamIsolation = 0;
	PervasiveDmb();
	return previous != 0;
}

__attribute__((noinline)) int SonyRegbusIsolationDisable(void)
{
	uint32_t previous = s_misc->sonyRegbusIsolation;

	s_misc->sonyRegbusIsolation = 0;
	PervasiveDmb();
	return previous != 0;
}

int Lpddr2IsolationDisable(int port)
{
	uint32_t previous = s_misc->lpddr2Isolation[port];

	s_misc->lpddr2Isolation[port] = 0;
	PervasiveDmb();
	return previous != 0;
}

__attribute__((noinline)) int CompatRamBusIdle(void)
{
	volatile uint32_t *request = &s_misc->compatRamBusIdleRequest;

	*request = 1;
	while (*request & 1)
		;
	while ((s_misc->busIdleStatus & 8) == 0)
		;
	s_misc->busIdleStatus = 8;
	while (s_misc->busIdleStatus & 8)
		;
	return 0;
}

__attribute__((noinline)) int SonyRegbusBusIdle(void)
{
	volatile uint32_t *request = &s_misc->sonyRegbusBusIdleRequest;

	*request = 1;
	while (*request & 1)
		;
	while ((s_misc->busIdleStatus & 0x10) == 0)
		;
	s_misc->busIdleStatus = 0x10;
	while (s_misc->busIdleStatus & 0x10)
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
	return GateEnable(&s_gate->arm, (mask & 0xf00000) ^ 0x300000);
}

int ArmDebuggerGateEnable(void)
{
	return GateEnableDevice(&s_gate->armDebugger);
}

__attribute__((noinline)) int SonyRegbusGateEnable(void)
{
	return GateEnableDevice(&s_gate->sonyRegbus);
}

__attribute__((noinline)) int CompatRamGateEnable(void)
{
	return GateEnableDevice(&s_gate->compatRam);
}

__attribute__((noinline)) int CompatRamGateDisable(void)
{
	return GateDisableDevice(&s_gate->compatRam);
}

int ScePervasiveForDriver_D34E74C5(int port)
{
	return GateEnableDevice(&s_gate->sdio[port]);
}

int DebugPaGateEnable(void)
{
	return GateDeviceDual(&s_gate->debugPa, 1);
}

int ScePervasiveForDriver_A10583FF(int port)
{
	return GateEnableDevice(&s_gate->dmac[port]);
}
#else
int ScePervasiveForDriver_3CF7A012(int port)
{
	return GateEnableDevice(&s_gate->sdif[port]);
}
#endif

int kscePervasiveGpioClockEnable(void)
{
	return GateEnableDevice(&s_gate->gpio);
}

int kscePervasiveUartClockEnable(int port)
{
	return GateEnableDevice(&s_gate->uart[port]);
}

#ifdef PERVASIVE_SKBL
int DebugBusGateEnable(void)
{
	return GateEnableDevice(&s_gate->debugBus);
}

int BootDeviceGateEnable(void)
{
	return GateEnableDevice(&s_gate->bootDevice);
}

int Lpddr2GateEnable(int port)
{
	return GateEnableDevice(&s_gate->lpddr2[port]);
}

int TimerGateEnable(void)
{
	return GateEnableDevice(&s_gate->timer);
}

int DebugDeviceGateEnable(void)
{
	return GateEnableDevice(&s_gate->debugDevice);
}
#endif

/* ===== clocks ===== */

int ScePervasiveForDriver_6AF0196F(unsigned int core, unsigned int mp)
{
	if (core > 7 || mp > 7)
		return PERVASIVE_ERROR;
	s_baseClock->gpuClock = core << 16 | mp;
	PervasiveDmb();
	return 0;
}

static PixelClockContext s_pixelClock[2];

int LowioPixelClockInit(void)
{
	s_pixelClock[0].setting = NULL;
	s_pixelClock[1].setting = NULL;
	s_pixelClock[0].registers = &s_baseClock->dsi0PixelClock;
	s_pixelClock[0].pllConfig = s_pixelClock[0].registers->pllConfig;
	s_pixelClock[1].registers = &s_baseClock->dsi1PixelClock;
	s_pixelClock[1].pllConfig = s_pixelClock[1].registers->pllConfig;
	return 0;
}

#ifdef PERVASIVE_SKBL
/* ===== the secure devices' power-on ===== */

__attribute__((noinline)) int CompatRamPowerOn(void)
{
	s_baseClock->compatClock = 0;
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
	s_baseClock->allegrexBusClockEnable = 0;
	PervasiveDmb();
	SonyRegbusGateEnable();
	SonyRegbusResetDisable();
	return 0;
}
#endif
