/* SceLowio internal hardware mappings. Private helper names are inferred. */
#pragma once

#include <stdint.h>
#include <sysmem/cpu.h>
#include <lowio/pervasive_regs.h>
#include <lowio/uart_regs.h>
#include <lowio/mailbox_regs.h>
#include <lowio/pervasive.h>
#include <lowio/dsi.h>
#include <lowio/pwm.h>

extern ScePervasiveMiscRegisters *s_misc;
extern ScePervasiveVidRegisters *s_video;
extern ScePervasiveMailboxRegisters *s_mailbox;
extern ScePervasive2Registers *s_pervasive2;
extern ScePervasiveResetRegisters *s_reset;
extern ScePervasiveGateRegisters *s_gate;
extern volatile SceUartClkgenRegisters *s_uartClock;
extern ScePervasiveBaseClkRegisters *s_baseClock;
extern SceKernelSpinlock s_pervasiveLock;

/*
 * Sony's DMB orders only the device accesses around it; its DSB is also a
 * compiler barrier (values held in registers are reloaded after it).
 */
static inline void LowioDmb(void)
{
	__asm__ volatile("dmb sy");
}

static inline void LowioDsb(void)
{
	__asm__ volatile("dsb sy" : : : "memory");
}

/*
 * The boot loaders build pervasive.c and pervasive_clock.c too (LOWIO_KBL;
 * LOWIO_SKBL and LOWIO_NSKBL for what only one of them keeps). They run
 * alone, before any mapping: the register blocks are at their physical
 * addresses and nothing takes the lock.
 */
#if LOWIO_KBL
#define MISC_REGS	PERVASIVE_MISC
#define RESET_REGS	PERVASIVE_RESET
#define GATE_REGS	PERVASIVE_GATE
#define BASECLK_REGS	PERVASIVE_BASECLK
#else
#define MISC_REGS	s_misc
#define RESET_REGS	s_reset
#define GATE_REGS	s_gate
#define BASECLK_REGS	s_baseClock
#endif

static inline uint32_t PervasiveLock(void)
{
#if LOWIO_KBL
	return 0;
#else
	return sceKernelSpinlockLowLockCpuSuspendIntr(&s_pervasiveLock);
#endif
}

static inline void PervasiveUnlock(uint32_t state)
{
#if LOWIO_KBL
	(void)state;
#else
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_pervasiveLock, state);
#endif
}

/* pervasive.c, pervasive_clock.c: store a Misc word; returns whether it was set before. */
static inline __attribute__((always_inline)) uint32_t MiscReplace(
	volatile uint32_t *reg, uint32_t value)
{
	uint32_t previous = *reg;

	*reg = value;
	previous = previous != 0;
	LowioDmb();
	return previous;
}

/* smc.S: the monitor gets r2 and r3 as the caller left them */
uint32_t Smc110(uint32_t bus, uint32_t state);
int Smc111(volatile uint32_t *reg, int value);
int Smc113(volatile uint32_t *reg, int value);
uint32_t Smc114(uint32_t clock, uint32_t level);

/* module_start's stages, in its order */
int LowioPervasiveInit(SceSize arglen, const void *argp);
int LowioGpioInit(SceSize arglen, const void *argp);
int LowioPwmInit(SceSize arglen, const void *argp);
int LowioI2cInit(SceSize arglen, const void *argp);
int LowioGrabInit(SceSize arglen, const void *argp);
int LowioCdramInit(SceSize arglen, const void *argp);
int LowioDsiInit(SceSize arglen, const void *argp);
int LowioIftuInit(SceSize arglen, const void *argp);
int LowioCsiInit(SceSize arglen, const void *argp);

/* pervasive_pixel.c */
int LowioPixelClockInit(void);

/* pervasive_registers.c */
int PervasiveMiscInit(void);

#if LOWIO_SKBL
/* pervasive.c's steps of SKBL's power-on sequences (pervasive_clock.c) */
int CompatRamResetEnable(void);
int CompatRamResetDisable(void);
int CompatRamGateEnable(void);
int CompatRamGateDisable(void);
int CompatRamIsolationDisable(void);
int CompatRamBusIdle(void);
int SonyRegbusResetDisable(void);
int SonyRegbusGateEnable(void);
int SonyRegbusIsolationDisable(void);
int SonyRegbusBusIdle(void);
#endif
