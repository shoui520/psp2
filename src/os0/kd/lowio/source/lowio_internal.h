/* SceLowio internal hardware mappings. Private helper names are inferred. */
#pragma once

#include <stdint.h>
#include <psp2kern/kernel/cpu.h>
#include <sce/pervasive_regs.h>

extern ScePervasiveMiscRegisters *s_misc;
extern uint8_t *s_video, *s_mailbox;
extern ScePervasive2Registers *s_pervasive2;
extern ScePervasiveResetRegisters *s_reset;
extern ScePervasiveGateRegisters *s_gate;
extern volatile uint32_t *s_uartClock;		/* SceUartclkReg: one baud-rate word per port */
extern ScePervasiveBaseClkRegisters *s_baseClock;
extern uint8_t *s_tas[8];
extern SceKernelSpinlock s_pervasiveLock;

static inline void LowioDmb(void)
{
	__asm__ volatile("dmb sy");
}

static inline void LowioDsb(void)
{
	__asm__ volatile("dsb sy");
}

uint32_t Smc110(uint32_t, uint32_t, uint32_t, uint32_t);
int Smc111(volatile uint32_t *reg, int value);
int Smc113(volatile uint32_t *reg, int value);
uint32_t Smc114(uint32_t, uint32_t, uint32_t, uint32_t);

int ScePervasiveForDriver_4AF7A01E(int port, unsigned int mask);

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
